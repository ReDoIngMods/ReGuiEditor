#include "RPTexture.hpp"

#include "RPRTTexture.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <rend/Common/Alignment.hpp>
#include <rend/Common/Format.hpp>

#include <rend/ResourceManager.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <Modules/Bytestream/BytestreamReader.hpp>
#include <Modules/Bytestream/BytestreamWriter.hpp>
#include <Modules/Misc/Strings.hpp>

#include <FreeImage/FreeImage.h>

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <Windows.h>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

constexpr uint32_t BYTES_PER_PIXEL = 4;
constexpr uint32_t MAX_CACHED_TEXTURE_SIZE = 8192;

struct TextureCacheHeader {
	char magic[4] = { 'M', 'G', 'T', 'C' };
	uint32_t version = 5;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	int64_t sourceWriteTime = 0;
};

std::string RPTexture::getCacheFilepath(const std::string& _filename) {
	// Dimensions are unknown before the image is decoded, so they live in the header instead of the filename.
	return ReGuiEditor::formatString("$RGE_CACHE_DATA/Textures/%s.mgtc", _filename.c_str());
}

bool RPTexture::createBacking(int _width, int _height, rend::Texture2DFlags _flags) {
	this->destroy();

	const int textureWidth = rend::alignPower2(_width);
	const int textureHeight = rend::alignPower2(_height);

	this->texture_ = rend::ResourceManager::getInstance().createTexture2D(static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight), rend::Format::R8G8B8A8_UNORM, _flags);
	if (!this->texture_) {
		CONSOLE_ERROR("Failed to create %dx%d texture '%s'.", textureWidth, textureHeight, this->name_.c_str());
		return false;
	}

	const std::string debugName = ReGuiEditor::formatString("MyGUI::Texture \"%s\"", this->name_.c_str());
	this->texture_->setDebugName(debugName);

	this->width_ = _width;
	this->height_ = _height;
	this->textureWidth_ = textureWidth;
	this->textureHeight_ = textureHeight;

	return true;
}

bool RPTexture::upload(const uint8_t* _pixels, size_t _size) {
	if (!this->texture_ || !_pixels)
		return false;

	return this->texture_->writeDataRegion(_pixels, _size, 0, 0, static_cast<uint32_t>(this->width_), static_cast<uint32_t>(this->height_), static_cast<uint32_t>(this->width_) * BYTES_PER_PIXEL);
}

bool RPTexture::loadCache(const std::string& _filename) {
	const std::string sourceName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (sourceName.empty())
		return false;

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	const std::string cacheName = getCacheFilepath(_filename);
	if (!filesystemManager.exists(cacheName))
		return false;

	time_t sourceWriteTime = 0;
	if (!filesystemManager.getFileWriteTime(sourceName, sourceWriteTime)) {
		CONSOLE_WARNING("Failed to get write time for source file: %s", _filename.c_str());
		return false;
	}

	std::vector<uint8_t> buffer;
	if (!filesystemManager.readFile(cacheName, buffer)) {
		CONSOLE_WARNING("Cache file not found or failed to read: %s", cacheName.c_str());
		return false;
	}

	if (buffer.size() < sizeof(TextureCacheHeader)) {
		CONSOLE_WARNING("Cache file is too small: %s", _filename.c_str());
		return false;
	}

	ReGuiEditor::BytestreamReader reader(buffer.data(), buffer.size());

	const TextureCacheHeader header = reader.read<TextureCacheHeader>();
	if (memcmp(header.magic, "MGTC", 4) != 0 || header.version != TextureCacheHeader{}.version) {
		CONSOLE_WARNING("Invalid cache file format: %s", _filename.c_str());
		return false;
	}

	if (header.sourceWriteTime != sourceWriteTime) {
		CONSOLE_WARNING("Cache file is outdated: %s", _filename.c_str());
		return false;
	}

	if (header.width == 0 || header.height == 0 || header.textureWidth < header.width || header.textureHeight < header.height) {
		CONSOLE_WARNING("Invalid texture dimensions in cache file: %s", _filename.c_str());
		return false;
	}

	std::vector<uint8_t> pixelData;
	if (!reader.readCompressableData(pixelData)) {
		CONSOLE_WARNING("Failed to read compressed texture data from cache file: %s", _filename.c_str());
		return false;
	}

	if (pixelData.size() != static_cast<size_t>(header.width) * header.height * BYTES_PER_PIXEL) {
		CONSOLE_WARNING("Pixel data size mismatch in cache file: %s", _filename.c_str());
		return false;
	}

	if (!this->createBacking(static_cast<int>(header.width), static_cast<int>(header.height), rend::Texture2DFlags::ShaderResource))
		return false;

	if (static_cast<uint32_t>(this->textureWidth_) != header.textureWidth || static_cast<uint32_t>(this->textureHeight_) != header.textureHeight) {
		CONSOLE_WARNING("Texture dimensions in cache file do not match expected size: %s", _filename.c_str());

		this->destroy();
		return false;
	}

	if (!this->upload(pixelData.data(), pixelData.size())) {
		CONSOLE_ERROR("Failed to upload cached texture: %s", _filename.c_str());

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

	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	time_t sourceWriteTime = 0;
	if (!filesystemManager.getFileWriteTime(sourceName, sourceWriteTime)) {
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

	TextureCacheHeader header;
	header.width = width;
	header.height = height;
	header.textureWidth = static_cast<uint32_t>(this->textureWidth_);
	header.textureHeight = static_cast<uint32_t>(this->textureHeight_);
	header.sourceWriteTime = sourceWriteTime;

	ReGuiEditor::BytestreamWriter writer;
	writer.write<TextureCacheHeader>(header);
	writer.writeCompressableData(_pixels, dataSize);

	const std::string cacheName = getCacheFilepath(_filename);
	if (!filesystemManager.writeFile(cacheName, writer)) {
		CONSOLE_ERROR("Failed to write cache file: %s", cacheName.c_str());
		return false;
	}

	return true;
}

void RPTexture::createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) {
	if (_format != MyGUI::PixelFormat::R8G8B8A8 || _width <= 0 || _height <= 0) {
		CONSOLE_ERROR("Unsupported format or invalid size (%dx%d) for '%s'.", _width, _height, this->name_.c_str());
		return;
	}

	rend::Texture2DFlags flags = rend::Texture2DFlags::ShaderResource;
	if (_usage.isValue(MyGUI::TextureUsage::RenderTarget))
		flags = flags | rend::Texture2DFlags::RenderTarget;

	if (this->createBacking(_width, _height, flags))
		this->usage_ = _usage;
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

	std::vector<uint8_t> fileData;
	if (!FilesystemManager::getInstance().readFile(filepath, fileData)) {
		CONSOLE_ERROR("RPTexture::loadFromFile: Failed to read file: %s", _filename.c_str());
		return;
	}

	FIMEMORY* fiMemory = FreeImage_OpenMemory(fileData.data(), static_cast<DWORD>(fileData.size()));
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

	FIBITMAP* convertedBitmap = FreeImage_ConvertTo32Bits(bitmap);
	FreeImage_Unload(bitmap);
	bitmap = convertedBitmap;

	if (!bitmap) {
		CONSOLE_ERROR("RPTexture::loadFromFile: Failed to convert image to 32-bit: %s", _filename.c_str());
		return;
	}

	if (!FreeImage_FlipVertical(bitmap)) {
		CONSOLE_ERROR("RPTexture::loadFromFile: Failed to flip image vertically. File: %s", _filename.c_str());

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
		CONSOLE_ERROR("RPTexture::loadFromFile: Failed to upload texture: %s", _filename.c_str());

		this->destroy();
		return;
	}

	(void)this->storeCache(_filename, pixels.data());
}

void RPTexture::saveToFile(const std::string& _filename) {
	CONSOLE_WARNING("RPTexture::saveToFile: Saving textures is not supported (requested '%s').", _filename.c_str());
}

void RPTexture::destroy() {
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

void* RPTexture::lock(MyGUI::TextureUsage _access) {
	if (this->isLocked_ || !this->texture_)
		return nullptr;

	if (!_access.isValue(MyGUI::TextureUsage::Write)) {
		CONSOLE_ERROR("RPTexture::lock: Only write access is supported for locking.");
		return nullptr;
	}

	this->isLocked_ = true;
	this->pixels_.assign(static_cast<size_t>(this->width_) * this->height_ * BYTES_PER_PIXEL, 0);

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