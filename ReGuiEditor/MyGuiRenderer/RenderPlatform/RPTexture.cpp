#include "RPTexture.hpp"

#include "RPRTTexture.hpp"

#include <Filesystem/FileInfo.hpp>
#include <Filesystem/FileReader.hpp>
#include <Filesystem/FileWriter.hpp>

#include <rend/Common/Format.hpp>

#include <rend/ResourceManager.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
#include <UTILS/Hashing.hpp>
#include <UTILS/Strings.hpp>

#include <FreeImage/FreeImage.h>

#include <LZ4/lz4.h>

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <Windows.h>

#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr uint32_t BYTES_PER_PIXEL = 4;
constexpr uint32_t MAX_CACHED_TEXTURE_SIZE = 8192;

struct TextureCacheHeader {
    char magic[4] = { 'M', 'G', 'T', 'C' }; // MyGui Texture Cache

    uint32_t version = 5;
    uint32_t width = 0;
    uint32_t height = 0;
    int64_t sourceWriteTime = 0;
};

std::string RPTexture::getCacheFilepath(const std::string& _filename) const {
	const size_t hash = UTILS::hashValue(_filename);
	const std::string filename = std::filesystem::path(_filename).stem().string();

	return UTILS::formatString("$RGE_CACHE_DATA/Textures/%016llx_%dx%d_%s.mgtc", hash, this->width_, this->height_, filename.c_str());
}

bool RPTexture::createBacking(int _width, int _height, rend::Texture2DFlags _flags, rend::Format _format) {
    this->destroy();

    this->texture_ = rend::ResourceManager::getInstance().createTexture2D(static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), _format, _flags);
    if (!this->texture_) {
        CONSOLE_ERROR("RPTexture: Failed to create %dx%d texture '%s'.", _width, _height, this->name_.c_str());
        return false;
    }

    this->texture_->setDebugName("MyGUI::Texture \"" + this->name_ + "\"");

    this->width_ = _width;
    this->height_ = _height;

    return true;
}

bool RPTexture::upload(const uint8_t* _pixels, size_t _size) {
    if (!this->texture_ || !_pixels)
        return false;

    uint32_t bytesPerPixel = (this->format_ == MyGUI::PixelFormat::L8) ? 1 : BYTES_PER_PIXEL;
    return this->texture_->writeDataRegion(_pixels, _size, 0, 0, static_cast<uint32_t>(this->width_), static_cast<uint32_t>(this->height_), static_cast<uint32_t>(this->width_) * bytesPerPixel);
}

bool RPTexture::loadCache(const std::string& _filename) {
    const std::string sourceName = MyGUI::DataManager::getInstance().getDataPath(_filename);
    if (sourceName.empty())
        return false;

    const std::string cacheName = this->getCacheFilepath(_filename);
    if (!Filesystem::exists(cacheName))
        return false;

    time_t sourceWriteTime = 0;
    if (!Filesystem::getFileWriteTime(sourceName, sourceWriteTime)) {
        CONSOLE_WARNING("Failed to get write time for source file: %s", _filename.c_str());
        return false;
    }

	auto buffer = Filesystem::readFileBinary(cacheName);
	if (!buffer) {
		CONSOLE_WARNING("Failed to read cache file: %s", _filename.c_str());
		return false;
	}

    if (buffer->size() < sizeof(TextureCacheHeader)) {
        CONSOLE_WARNING("Cache file is too small: %s", _filename.c_str());
        return false;
    }

    UTILS::ByteStreamReader reader(buffer->data(), buffer->size());

    const TextureCacheHeader header = reader.read<TextureCacheHeader>();
    if (memcmp(header.magic, "MGTC", 4) != 0 || header.version != TextureCacheHeader{}.version) {
        CONSOLE_WARNING("Invalid cache file format: %s", _filename.c_str());
        return false;
    }

    if (header.sourceWriteTime != sourceWriteTime) {
        CONSOLE_WARNING("Cache file is outdated: %s", _filename.c_str());
        return false;
    }

    if (header.width == 0 || header.height == 0) {
        CONSOLE_WARNING("Invalid texture dimensions in cache file: %s", _filename.c_str());
        return false;
    }

    if (reader.getRemaining() < 2 * sizeof(uint64_t)) {
        CONSOLE_WARNING("Cache file is truncated: %s", _filename.c_str());
        return false;
    }

    const uint64_t uncompressedDataSize = reader.read<uint64_t>();
    const uint64_t compressedDataSize = reader.read<uint64_t>();

    const uint64_t expectedDataSize = static_cast<uint64_t>(header.width) * header.height * BYTES_PER_PIXEL;
    if (uncompressedDataSize != expectedDataSize) {
        CONSOLE_WARNING("Uncompressed data size does not match expected size: %s", _filename.c_str());
        return false;
    }

    if (compressedDataSize == 0 || compressedDataSize > static_cast<uint64_t>(INT_MAX) || compressedDataSize > reader.getRemaining()) {
        CONSOLE_WARNING("Invalid compressed data size in cache file: %s", _filename.c_str());
        return false;
    }

	std::vector<uint8_t> pixelData(static_cast<size_t>(uncompressedDataSize));

	const size_t compressedDataOffset = buffer->size() - reader.getRemaining();
	const char* compressedData = reinterpret_cast<const char*>(buffer->data() + compressedDataOffset);
	const int decompressedSize = LZ4_decompress_safe(compressedData, reinterpret_cast<char*>(pixelData.data()), static_cast<int>(compressedDataSize), static_cast<int>(uncompressedDataSize));

    if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != pixelData.size()) {
        CONSOLE_WARNING("Failed to decompress cached texture data: %s", _filename.c_str());
        return false;
    }

    if (!this->createBacking(static_cast<int>(header.width), static_cast<int>(header.height), rend::Texture2DFlags::ShaderResource, rend::Format::R8G8B8A8_UNORM))
        return false;

    if (!this->upload(pixelData.data(), pixelData.size())) {
        CONSOLE_ERROR("RPTexture::loadCache: Failed to upload cached texture: %s", _filename.c_str());

        this->destroy();
        return false;
    }

    return true;
}

bool RPTexture::storeCache(const std::string& _filename, const uint8_t* _pixels) {
    if (!this->texture_ || !_pixels)
        return false;

    const std::string sourceName = MyGUI::DataManager::getInstance().getDataPath(_filename);
    if (sourceName.empty())
        return false;

    time_t sourceWriteTime = 0;
    if (!Filesystem::getFileWriteTime(sourceName, sourceWriteTime)) {
        CONSOLE_WARNING("Failed to get write time for source file: %s", _filename.c_str());
        return false;
    }

    const uint32_t width = static_cast<uint32_t>(this->width_);
    const uint32_t height = static_cast<uint32_t>(this->height_);

    if (width > MAX_CACHED_TEXTURE_SIZE || height > MAX_CACHED_TEXTURE_SIZE) {
        CONSOLE_WARNING("Texture dimensions exceed maximum allowed size (%ux%u): %s", MAX_CACHED_TEXTURE_SIZE, MAX_CACHED_TEXTURE_SIZE, _filename.c_str());
        return false;
    }

    const size_t dataSize = static_cast<size_t>(width) * height * BYTES_PER_PIXEL;
    if (dataSize > static_cast<size_t>(INT_MAX)) {
        CONSOLE_WARNING("Texture is too large for LZ4: %s", _filename.c_str());
        return false;
    }

    const int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
    std::vector<char> compressedData(static_cast<size_t>(maxCompressedSize));

    const int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(_pixels), compressedData.data(), static_cast<int>(dataSize), maxCompressedSize);
    if (compressedSize <= 0) {
        CONSOLE_ERROR("RPTexture::storeCache: Failed to compress texture data: %s", _filename.c_str());
        return false;
    }

    TextureCacheHeader header;
    header.width = width;
    header.height = height;
    header.sourceWriteTime = sourceWriteTime;

    UTILS::ByteStreamWriter writer;
    writer.write(header);
    writer.write<uint64_t>(static_cast<uint64_t>(dataSize));
    writer.write<uint64_t>(static_cast<uint64_t>(compressedSize));
    writer.writeBytes(compressedData.data(), static_cast<size_t>(compressedSize));

    const std::string cacheName = this->getCacheFilepath(_filename);
    if (!Filesystem::writeFile(cacheName, writer.toString())) {
        CONSOLE_ERROR("RPTexture::storeCache: Failed to write cache file: %s", cacheName.c_str());
        return false;
    }

    return true;
}

void RPTexture::createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) {
    const bool isRenderTarget = _usage.isValue(MyGUI::TextureUsage::RenderTarget);
    const bool isSupportedFormat = _format == MyGUI::PixelFormat::R8G8B8A8 || (_format == MyGUI::PixelFormat::L8 && !isRenderTarget);
    if (!isSupportedFormat || _width <= 0 || _height <= 0) {
        CONSOLE_ERROR("RPTexture::createManual: Unsupported format or invalid size (%dx%d) for '%s'.", _width, _height, this->name_.c_str());
        return;
    }

    rend::Texture2DFlags flags = rend::Texture2DFlags::ShaderResource;
    if (isRenderTarget)
        flags = flags | rend::Texture2DFlags::RenderTarget;

    rend::Format rendFormat = (_format == MyGUI::PixelFormat::L8) ? rend::Format::R8_UNORM : rend::Format::R8G8B8A8_UNORM;
    if (this->createBacking(_width, _height, flags, rendFormat)) {
        this->usage_ = _usage;
        this->format_ = _format;
    }
}

void RPTexture::loadFromFile(const std::string& _filename) {
    if (this->loadCache(_filename))
        return;

    CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "CACHE MIS! fileName=%s", _filename.c_str());

    const std::string filepath = MyGUI::DataManager::getInstance().getDataPath(_filename);
    if (filepath.empty()) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Failed to get full path for file: %s", _filename.c_str());
        return;
    }

	auto fileData = Filesystem::readFileBinary(filepath);
	if (!fileData) {
		CONSOLE_ERROR("RPTexture::loadFromFile: Failed to read file: %s", _filename.c_str());
		return;
	}

    FIMEMORY* fiMemory = FreeImage_OpenMemory(fileData->data(), static_cast<DWORD>(fileData->size()));
    if (!fiMemory) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Failed to open memory for file: %s", _filename.c_str());
        return;
    }

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(fiMemory, 0);
    if (fif == FIF_UNKNOWN) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Unknown image format for file: %s", _filename.c_str());

        FreeImage_CloseMemory(fiMemory);
        return;
    }

    FIBITMAP* bitmap = FreeImage_LoadFromMemory(fif, fiMemory);
    FreeImage_CloseMemory(fiMemory);
	fileData->clear();

    if (bitmap && (FreeImage_GetImageType(bitmap) != FIT_BITMAP || FreeImage_GetBPP(bitmap) != 32)) {
        FIBITMAP* convertedBitmap = FreeImage_ConvertTo32Bits(bitmap);
        FreeImage_Unload(bitmap);
        bitmap = convertedBitmap;
    }

    if (!bitmap) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Failed to load or convert image to 32-bit: %s", _filename.c_str());
        return;
    }

    if (!FreeImage_FlipVertical(bitmap)) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Failed to flip image vertically. File: %s", _filename.c_str());

        FreeImage_Unload(bitmap);
        return;
    }

    const int srcWidth = static_cast<int>(FreeImage_GetWidth(bitmap));
    const int srcHeight = static_cast<int>(FreeImage_GetHeight(bitmap));

    uint8_t* bits = FreeImage_GetBits(bitmap);
    const size_t byteCount = static_cast<size_t>(srcWidth) * srcHeight * BYTES_PER_PIXEL;
    if (!this->createBacking(srcWidth, srcHeight, rend::Texture2DFlags::ShaderResource, rend::Format::R8G8B8A8_UNORM)) {
        FreeImage_Unload(bitmap);
        return;
    }

    for (size_t index = 0; index < byteCount; index += BYTES_PER_PIXEL) // BGRA -> RGBA in place
        std::swap(bits[index], bits[index + 2]);

    if (!this->upload(bits, byteCount)) {
        CONSOLE_ERROR("RPTexture::loadFromFile: Failed to upload texture: %s", _filename.c_str());

        FreeImage_Unload(bitmap);
        this->destroy();
        return;
    }

    (void)this->storeCache(_filename, bits);
    FreeImage_Unload(bitmap);
}

void RPTexture::saveToFile(const std::string& _filename) {
    CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RPTexture::saveToFile: Saving textures is not supported (requested '%s').", _filename.c_str());
}

void RPTexture::destroy() {
    this->renderTarget_.reset();
    this->texture_.reset();

    this->pixels_.clear();
    this->pixels_.shrink_to_fit();

    this->width_ = 0;
    this->height_ = 0;
    this->isLocked_ = false;
    this->usage_ = MyGUI::TextureUsage::Default;
    this->format_ = MyGUI::PixelFormat::R8G8B8A8;
}

void* RPTexture::lock(MyGUI::TextureUsage _access) {
    if (this->isLocked_ || !this->texture_)
        return nullptr;

    if (!_access.isValue(MyGUI::TextureUsage::Write)) {
        CONSOLE_ERROR("RPTexture::lock: Only write access is supported for locking.");
        return nullptr;
    }

    this->isLocked_ = true;
    this->pixels_.assign(static_cast<size_t>(this->width_) * this->height_ * this->getNumElemBytes(), 0);

    return this->pixels_.data();
}

void RPTexture::unlock() {
    if (!this->isLocked_)
        return;

    if (!this->upload(this->pixels_.data(), this->pixels_.size()))
        CONSOLE_ERROR("RPTexture::unlock: Failed to upload texture '%s'.", this->name_.c_str());

    this->pixels_.clear();
    this->pixels_.shrink_to_fit();

    this->isLocked_ = false;
}

MyGUI::IRenderTarget* RPTexture::getRenderTarget() {
    if (!this->texture_ || !(this->texture_->getFlags() & rend::Texture2DFlags::RenderTarget))
        return nullptr;

    if (!this->renderTarget_)
        this->renderTarget_ = std::make_unique<RPRTTexture>(this);

    return this->renderTarget_.get();
}