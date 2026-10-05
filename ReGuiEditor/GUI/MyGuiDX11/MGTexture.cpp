#include "MGTexture.hpp"

#include "MGRTTexture.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <rend/Common/Alignment.hpp>
#include <rend/Common/Format.hpp>

#include <rend/ResourceManager.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
#include <UTILS/Console.hpp>
#include <UTILS/Strings.hpp>

#include <FreeImage/FreeImage.h>

#include <LZ4/lz4.h>

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

constexpr uint32_t BYTES_PER_PIXEL = 4;
constexpr uint32_t MAX_CACHED_TEXTURE_SIZE = 8192;

struct TextureCacheHeader {
	char magic[4] = { 'M', 'G', 'T', 'C' }; // MyGui Texture Cache

	uint32_t version = 4;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	int64_t sourceWriteTime = 0;
};

std::string MyGUICustom::DirectX11Texture::getCacheFilepath(const std::string& _filename) {
	constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
	constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;

	uint64_t hash = FNV_OFFSET;
	for (const char character : _filename) {
		hash ^= static_cast<uint64_t>(static_cast<uint8_t>(character));
		hash *= FNV_PRIME;
	}

	const std::string filename = std::filesystem::path(_filename).stem().string();
	return UTILS::formatString("$RGE_CACHE_DATA/Textures/%016llx_%s.mgtc", hash, filename.c_str());
}

bool MyGUICustom::DirectX11Texture::createBacking(int _width, int _height, rend::Texture2DFlags _flags) {
	this->destroy();

	const int textureWidth = rend::alignPower2(_width);
	const int textureHeight = rend::alignPower2(_height);

	this->texture_ = rend::ResourceManager::getInstance().createTexture2D(static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), rend::Format::R8G8B8A8_UNORM, _flags);
	if (!this->texture_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture: Failed to create %dx%d texture '%s'.", textureWidth, textureHeight, this->name_.c_str());
		return false;
	}

	this->texture_->setDebugName("MyGUI::Texture \"" + this->name_ + "\"");

	this->width_ = _width;
	this->height_ = _height;
	this->textureWidth_ = textureWidth;
	this->textureHeight_ = textureHeight;

	return true;
}

bool MyGUICustom::DirectX11Texture::upload(const uint8_t* _pixels, size_t _size) {
	if (!this->texture_ || !_pixels)
		return false;

	return this->texture_->writeDataRegion(_pixels, _size, 0, 0, static_cast<uint32_t>(this->width_), static_cast<uint32_t>(this->height_), static_cast<uint32_t>(this->width_) * BYTES_PER_PIXEL);
}

bool MyGUICustom::DirectX11Texture::loadCache(const std::string& _filename) {
	const std::string sourceName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (sourceName.empty())
		return false;

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	time_t sourceWriteTime = 0;
	if (!filesystemManager.getFileWriteTimeRaw(sourceName, sourceWriteTime)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Failed to get write time for source file: %s", _filename.c_str());
		return false;
	}

	const std::string cacheName = getCacheFilepath(_filename);

	std::string buffer;
	if (!filesystemManager.readFile(cacheName, buffer)) {
		CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "DirectX11Texture::loadCache: Cache file not found or failed to read: %s", cacheName.c_str());
		return false;
	}

	if (buffer.size() < sizeof(TextureCacheHeader)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Cache file is too small: %s", _filename.c_str());
		return false;
	}

	UTILS::ByteStreamReader reader(buffer.data(), buffer.size());

	const TextureCacheHeader header = reader.read<TextureCacheHeader>();
	if (memcmp(header.magic, "MGTC", 4) != 0 || header.version != TextureCacheHeader{}.version) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Invalid cache file format: %s", _filename.c_str());
		return false;
	}

	if (header.sourceWriteTime != sourceWriteTime) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Cache file is outdated: %s", _filename.c_str());
		return false;
	}

	if (header.width == 0 || header.height == 0 || header.textureWidth < header.width || header.textureHeight < header.height) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Invalid texture dimensions in cache file: %s", _filename.c_str());
		return false;
	}

	if (reader.getRemaining() < 2 * sizeof(uint64_t)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Cache file is truncated: %s", _filename.c_str());
		return false;
	}

	const uint64_t uncompressedDataSize = reader.read<uint64_t>();
	const uint64_t compressedDataSize = reader.read<uint64_t>();

	const uint64_t expectedDataSize = static_cast<uint64_t>(header.width) * header.height * BYTES_PER_PIXEL;
	if (uncompressedDataSize != expectedDataSize) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Uncompressed data size does not match expected size: %s", _filename.c_str());
		return false;
	}

	if (compressedDataSize == 0 || compressedDataSize > static_cast<uint64_t>(INT_MAX) || compressedDataSize > reader.getRemaining()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Invalid compressed data size in cache file: %s", _filename.c_str());
		return false;
	}

	std::vector<char> compressedData(static_cast<size_t>(compressedDataSize));
	reader.readBytes(compressedData.data(), compressedData.size());

	std::vector<uint8_t> pixelData(static_cast<size_t>(uncompressedDataSize));

	const int decompressedSize = LZ4_decompress_safe(compressedData.data(), reinterpret_cast<char*>(pixelData.data()), static_cast<int>(compressedDataSize), static_cast<int>(uncompressedDataSize));
	if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != pixelData.size()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Failed to decompress cached texture data: %s", _filename.c_str());
		return false;
	}

	if (!this->createBacking(static_cast<int>(header.width), static_cast<int>(header.height), rend::Texture2DFlags::ShaderResource))
		return false;

	if (static_cast<uint32_t>(this->textureWidth_) != header.textureWidth || static_cast<uint32_t>(this->textureHeight_) != header.textureHeight) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Texture dimensions in cache file do not match expected size: %s", _filename.c_str());

		this->destroy();
		return false;
	}

	if (!this->upload(pixelData.data(), pixelData.size())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadCache: Failed to upload cached texture: %s", _filename.c_str());

		this->destroy();
		return false;
	}

	return true;
}

bool MyGUICustom::DirectX11Texture::storeCache(const std::string& _filename, const uint8_t* _pixels) {
	if (!this->texture_ || !_pixels)
		return false;

	const std::string sourceName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (sourceName.empty())
		return false;

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	time_t sourceWriteTime = 0;
	if (!filesystemManager.getFileWriteTimeRaw(sourceName, sourceWriteTime)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Failed to get write time for source file: %s", _filename.c_str());
		return false;
	}

	const uint32_t width = static_cast<uint32_t>(this->width_);
	const uint32_t height = static_cast<uint32_t>(this->height_);

	if (width > MAX_CACHED_TEXTURE_SIZE || height > MAX_CACHED_TEXTURE_SIZE) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture dimensions exceed maximum allowed size (%ux%u): %s", MAX_CACHED_TEXTURE_SIZE, MAX_CACHED_TEXTURE_SIZE, _filename.c_str());
		return false;
	}

	const size_t dataSize = static_cast<size_t>(width) * height * BYTES_PER_PIXEL;
	if (dataSize > static_cast<size_t>(INT_MAX)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture is too large for LZ4: %s", _filename.c_str());
		return false;
	}

	const int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
	std::vector<char> compressedData(static_cast<size_t>(maxCompressedSize));

	const int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(_pixels), compressedData.data(), static_cast<int>(dataSize), maxCompressedSize);
	if (compressedSize <= 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to compress texture data: %s", _filename.c_str());
		return false;
	}

	TextureCacheHeader header;
	header.width = width;
	header.height = height;
	header.textureWidth = static_cast<uint32_t>(this->textureWidth_);
	header.textureHeight = static_cast<uint32_t>(this->textureHeight_);
	header.sourceWriteTime = sourceWriteTime;

	UTILS::ByteStreamWriter writer;
	writer.write(header);
	writer.write<uint64_t>(static_cast<uint64_t>(dataSize));
	writer.write<uint64_t>(static_cast<uint64_t>(compressedSize));
	writer.writeBytes(compressedData.data(), static_cast<size_t>(compressedSize));

	const std::string cacheName = getCacheFilepath(_filename);
	if (!filesystemManager.writeFile(cacheName, writer.toString())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to write cache file: %s", cacheName.c_str());
		return false;
	}

	return true;
}

void MyGUICustom::DirectX11Texture::createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) {
	if (_format != MyGUI::PixelFormat::R8G8B8A8 || _width <= 0 || _height <= 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::createManual: Unsupported format or invalid size (%dx%d) for '%s'.", _width, _height, this->name_.c_str());
		return;
	}

	rend::Texture2DFlags flags = rend::Texture2DFlags::ShaderResource;
	if (_usage == MyGUI::TextureUsage::RenderTarget)
		flags = flags | rend::Texture2DFlags::RenderTarget;

	if (this->createBacking(_width, _height, flags))
		this->usage_ = _usage;
}

void MyGUICustom::DirectX11Texture::loadFromFile(const std::string& _filename) {
	if (this->loadCache(_filename))
		return;

	CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "Cache miss! Loading texture & generating cache from: %s", _filename.c_str());

	const std::string fullName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (fullName.empty()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to get full path for file: %s", _filename.c_str());
		return;
	}

	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(fullName.c_str(), 0);
	if (fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(fullName.c_str());

	if (fif == FIF_UNKNOWN) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Unknown image format for file: %s", _filename.c_str());
		return;
	}

	FIBITMAP* bitmap = FreeImage_Load(fif, fullName.c_str());
	if (!bitmap) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to load image file: %s", _filename.c_str());
		return;
	}

	FIBITMAP* convertedBitmap = FreeImage_ConvertTo32Bits(bitmap);
	FreeImage_Unload(bitmap);
	bitmap = convertedBitmap;

	if (!bitmap) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to convert image to 32-bit: %s", _filename.c_str());
		return;
	}

	if (!FreeImage_FlipVertical(bitmap)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to flip image vertically. File: %s", _filename.c_str());

		FreeImage_Unload(bitmap);
		return;
	}

	const int srcWidth = static_cast<int>(FreeImage_GetWidth(bitmap));
	const int srcHeight = static_cast<int>(FreeImage_GetHeight(bitmap));
	const size_t srcPitch = FreeImage_GetPitch(bitmap);
	const uint8_t* srcBits = FreeImage_GetBits(bitmap);

	if (!this->createBacking(srcWidth, srcHeight, rend::Texture2DFlags::ShaderResource)) {
		FreeImage_Unload(bitmap);
		return;
	}

	const size_t dstPitch = static_cast<size_t>(this->width_) * BYTES_PER_PIXEL;
	std::vector<uint8_t> pixels(dstPitch * static_cast<size_t>(this->height_));

	for (int row = 0; row < srcHeight; ++row) {
		const uint8_t* source = srcBits + static_cast<size_t>(row) * srcPitch;
		uint8_t* destination = pixels.data() + static_cast<size_t>(row) * dstPitch;

		for (int column = 0; column < srcWidth; ++column) {
			destination[0] = source[2];
			destination[1] = source[1];
			destination[2] = source[0];
			destination[3] = source[3];

			source += BYTES_PER_PIXEL;
			destination += BYTES_PER_PIXEL;
		}
	}

	FreeImage_Unload(bitmap);

	if (!this->upload(pixels.data(), pixels.size())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to upload texture: %s", _filename.c_str());

		this->destroy();
		return;
	}

	(void)this->storeCache(_filename, pixels.data());
}

void MyGUICustom::DirectX11Texture::saveToFile(const std::string& _filename) {
	CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::saveToFile: Saving textures is not supported (requested '%s').", _filename.c_str());
}

void MyGUICustom::DirectX11Texture::destroy() {
	this->renderTarget_.reset();
	this->texture_.reset();

	this->pixels_.clear();
	this->pixels_.shrink_to_fit();

	this->width_ = 0;
	this->height_ = 0;
	this->textureWidth_ = 0;
	this->textureHeight_ = 0;
	this->isLocked_ = false;
	this->usage_ = MyGUI::TextureUsage::Default;
}

void* MyGUICustom::DirectX11Texture::lock(MyGUI::TextureUsage _access) {
	if (this->isLocked_ || !this->texture_)
		return nullptr;

	if (_access != MyGUI::TextureUsage::Write) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::lock: Only write access is supported for locking.");
		return nullptr;
	}

	this->isLocked_ = true;
	this->pixels_.assign(static_cast<size_t>(this->width_) * this->height_ * BYTES_PER_PIXEL, 0);

	return this->pixels_.data();
}

void MyGUICustom::DirectX11Texture::unlock() {
	if (!this->isLocked_)
		return;

	if (!this->upload(this->pixels_.data(), this->pixels_.size()))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::unlock: Failed to upload texture '%s'.", this->name_.c_str());

	this->pixels_.clear();
	this->pixels_.shrink_to_fit();

	this->isLocked_ = false;
}

MyGUI::IRenderTarget* MyGUICustom::DirectX11Texture::getRenderTarget() {
	if (!this->texture_ || !(this->texture_->getFlags() & rend::Texture2DFlags::RenderTarget))
		return nullptr;

	if (!this->renderTarget_)
		this->renderTarget_ = std::make_unique<MyGUICustom::DirectX11RTTexture>(this);

	return this->renderTarget_.get();
}