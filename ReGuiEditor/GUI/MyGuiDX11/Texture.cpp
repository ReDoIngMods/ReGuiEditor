#include "Texture.hpp"

#include "RenderManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
#include <UTILS/Strings.hpp>

#include <FreeImage/FreeImage.h>

#include <LZ4/lz4.h>

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_RenderFormat.h>

#include <Windows.h>

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

struct TextureCacheHeader {
	char magic[4] = { 'M', 'G', 'T', 'C' }; // MyGui Texture Cache

	uint32_t version = 1;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t realWidth = 0;
	uint32_t realHeight = 0;
	int64_t sourceWriteTime = 0;
};

inline static UINT power2Round(UINT value) {
	UINT result = 1;
	while (result < value)
		result <<= 1;

	return result;
}

std::string MyGUICustom::DirectX11Texture::getCacheFilepath(const std::string& _filename) {
	constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;

	uint64_t hash = FNV_OFFSET;
	for (const char character : _filename) {
		hash ^= static_cast<uint64_t>(static_cast<uint8_t>(character));
		hash *= FNV_PRIME;
	}

	const std::string filename = std::filesystem::path(_filename).stem().string();
	return UTILS::formatString("$RGE_CACHE_DATA/Textures/%016llx_%s.mgtc", hash, filename.c_str());
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

	const std::string cacheName = this->getCacheFilepath(_filename);

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

	TextureCacheHeader header = reader.read<TextureCacheHeader>();
	if (memcmp(header.magic, "MGTC", 4) != 0 || header.version != 1) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Invalid cache file format: %s", _filename.c_str());
		return false;
	}

	if (header.sourceWriteTime != sourceWriteTime) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Cache file is outdated: %s", _filename.c_str());
		return false;
	}

	if (header.width == 0 || header.height == 0 || header.realWidth == 0 || header.realHeight == 0 || header.realWidth > header.width || header.realHeight > header.height) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Invalid texture dimensions in cache file: %s", _filename.c_str());
		return false;
	}

	const uint64_t uncompressedDataSize = reader.read<uint64_t>();
	const uint64_t compressedDataSize = reader.read<uint64_t>();

	const uint64_t expectedDataSize = static_cast<uint64_t>(header.width) * static_cast<uint64_t>(header.height) * 4;

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

	int decompressedSize = LZ4_decompress_safe(compressedData.data(), reinterpret_cast<char*>(pixelData.data()), static_cast<int>(compressedDataSize), static_cast<int>(uncompressedDataSize));
	if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != pixelData.size()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::loadCache: Failed to decompress cached texture data: %s", _filename.c_str());
		return false;
	}

	this->destroy();

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11Device* device = renderManager.device;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = header.width;
	desc.Height = header.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = pixelData.data();
	initData.SysMemPitch = header.width * 4;

	HRESULT result = device->CreateTexture2D(&desc, &initData, &this->texture);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadCache: Failed to create texture from cache. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	result = device->CreateShaderResourceView(this->texture, &srvDesc, &this->shaderResourceView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadCache: Failed to create shader resource view from cache. HRESULT: 0x%X", result);

		this->texture->Release();
		this->texture = nullptr;
		return false;
	}

	this->width_ = static_cast<int>(header.width);
	this->height_ = static_cast<int>(header.height);
	this->realWidth_ = static_cast<int>(header.realWidth);
	this->realHeight_ = static_cast<int>(header.realHeight);

	return true;
}

bool MyGUICustom::DirectX11Texture::storeCache(const std::string& _filename) {
	if (!this->texture)
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

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11Device* device = renderManager.device;
	ID3D11DeviceContext* context = renderManager.context;

	D3D11_TEXTURE2D_DESC desc;
	this->texture->GetDesc(&desc);

	if (desc.Width == 0 || desc.Height == 0) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Invalid texture dimensions: %s", _filename.c_str());
		return false;
	}

	if (desc.Width > 8192 || desc.Height > 8192) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture dimensions exceed maximum allowed size (8192x8192): %s", _filename.c_str());
		return false;
	}

	if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Unsupported texture format (only B8G8R8A8_UNORM is supported): %s", _filename.c_str());
		return false;
	}

	if (desc.MipLevels != 1) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture has multiple mip levels, which is not supported for caching: %s", _filename.c_str());
		return false;
	}

	if (desc.ArraySize != 1) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture is an array texture, which is not supported for caching: %s", _filename.c_str());
		return false;
	}

	if (desc.SampleDesc.Count != 1) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture is multisampled, which is not supported for caching: %s", _filename.c_str());
		return false;
	}

	const size_t rowSize = static_cast<size_t>(desc.Width) * 4;
	const size_t dataSize = rowSize * static_cast<size_t>(desc.Height);

	if (dataSize > static_cast<size_t>(INT_MAX)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11Texture::storeCache: Texture is too large for LZ4: %s", _filename.c_str());
		return false;
	}

	D3D11_TEXTURE2D_DESC stagingDesc = desc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	ID3D11Texture2D* stagingTexture = nullptr;
	HRESULT result = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
	if (FAILED(result) || !stagingTexture) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to create staging texture. HRESULT: 0x%X", result);
		return false;
	}

	context->CopyResource(stagingTexture, this->texture);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	result = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to map staging texture for reading. HRESULT: 0x%X", result);

		stagingTexture->Release();
		return false;
	}

	std::vector<uint8_t> pixelData(dataSize);
	const uint8_t* mappedBytes = static_cast<const uint8_t*>(mappedResource.pData);
	for (UINT row = 0; row < desc.Height; ++row)
		memcpy(pixelData.data() + row * rowSize, mappedBytes + row * mappedResource.RowPitch, rowSize);

	context->Unmap(stagingTexture, 0);
	stagingTexture->Release();

	int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
	std::vector<char> compressedData(maxCompressedSize);

	int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(pixelData.data()), compressedData.data(), static_cast<int>(dataSize), maxCompressedSize);
	if (compressedSize <= 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to compress texture data: %s", _filename.c_str());
		return false;
	}

	UTILS::ByteStreamWriter writer;

	TextureCacheHeader header;
	header.width = desc.Width;
	header.height = desc.Height;
	header.realWidth = static_cast<uint32_t>(this->realWidth_);
	header.realHeight = static_cast<uint32_t>(this->realHeight_);
	header.sourceWriteTime = sourceWriteTime;

	writer.write(header);
	writer.write<uint64_t>(static_cast<uint64_t>(dataSize));
	writer.write<uint64_t>(static_cast<uint64_t>(compressedSize));
	writer.writeBytes(compressedData.data(), static_cast<size_t>(compressedSize));

	const std::string cacheName = this->getCacheFilepath(_filename);
	if (!FilesystemManager::getInstance().writeFile(cacheName, writer.toString())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::storeCache: Failed to write cache file: %s", cacheName.c_str());
		return false;
	}

	return true;
}

void MyGUICustom::DirectX11Texture::createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) {
	this->destroy();

	this->realWidth_ = _width;
	this->realHeight_ = _height;
	this->width_ = power2Round(static_cast<UINT>(_width));
	this->height_ = power2Round(static_cast<UINT>(_height));

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = this->width_;
	desc.Height = this->height_;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.Usage = D3D11_USAGE_DEFAULT;

	if (_usage == MyGUI::TextureUsage::RenderTarget)
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	else
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11Device* device = renderManager.device;

	HRESULT result = device->CreateTexture2D(&desc, nullptr, &this->texture);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::createManual: Failed to create texture. HRESULT: 0x%X", result);
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	result = device->CreateShaderResourceView(this->texture, &srvDesc, &this->shaderResourceView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::createManual: Failed to create shader resource view. HRESULT: 0x%X", result);

		this->texture->Release();
		this->texture = nullptr;

		return;
	}
}

void MyGUICustom::DirectX11Texture::loadFromFile(const std::string& _filename) {
	if (this->loadCache(_filename))
		return;

	CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "Cache miss! Loading texture & generating cache from: %s", _filename.c_str());

	std::string fullName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (fullName.empty()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to get full path for file: %s", _filename.c_str());
		return;
	}

	this->destroy();

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

	{
		FIBITMAP* newBitmap = FreeImage_ConvertTo32Bits(bitmap);
		FreeImage_Unload(bitmap);
		bitmap = newBitmap;

		if (!bitmap) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to convert image to 32-bit BGRA: %s", _filename.c_str());
			return;
		}
	}

	if (!FreeImage_FlipVertical(bitmap)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to flip image vertically. File: %s", _filename.c_str());

		FreeImage_Unload(bitmap);
		return;
	}

	this->realWidth_ = static_cast<int>(FreeImage_GetWidth(bitmap));
	this->realHeight_ = static_cast<int>(FreeImage_GetHeight(bitmap));

	this->width_ = power2Round(static_cast<UINT>(this->realWidth_));
	this->height_ = power2Round(static_cast<UINT>(this->realHeight_));

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11Device* device = renderManager.device;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = this->width_;
	desc.Height = this->height_;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	const UINT srcWidth = FreeImage_GetWidth(bitmap);
	const UINT srcHeight = FreeImage_GetHeight(bitmap);
	const UINT srcPitch = FreeImage_GetPitch(bitmap);
	const UINT dstPitch = this->width_ * 4;

	std::vector<uint8_t> padded(static_cast<size_t>(dstPitch) * this->height_, 0);
	const uint8_t* srcBits = FreeImage_GetBits(bitmap);

	for (UINT row = 0; row < srcHeight; ++row)
		memcpy(padded.data() + row * dstPitch, srcBits + row * srcPitch, srcWidth * 4);

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = padded.data();
	initData.SysMemPitch = dstPitch;

	HRESULT result = device->CreateTexture2D(&desc, &initData, &this->texture);
	FreeImage_Unload(bitmap);

	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to create texture. HRESULT: 0x%X", result);
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	result = device->CreateShaderResourceView(this->texture, &srvDesc, &this->shaderResourceView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to create shader resource view. HRESULT: 0x%X", result);

		this->texture->Release();
		this->texture = nullptr;

		return;
	}

	(void)this->storeCache(_filename);
}

void MyGUICustom::DirectX11Texture::destroy() {
	if (this->shaderResourceView) {
		this->shaderResourceView->Release();
		this->shaderResourceView = nullptr;
	}

	if (this->texture) {
		this->texture->Release();
		this->texture = nullptr;
	}

	if (this->writeData_) {
		delete[] this->writeData_;
		this->writeData_ = nullptr;
	}

	this->isLocked_ = false;
	this->width_ = 0;
	this->height_ = 0;
	this->realWidth_ = 0;
	this->realHeight_ = 0;
}

void* MyGUICustom::DirectX11Texture::lock(MyGUI::TextureUsage _access) {
	if (this->isLocked_)
		return nullptr;

	if (_access != MyGUI::TextureUsage::Write) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::lock: Only write access is supported for locking.");
		return nullptr;
	}

	this->isLocked_ = true;
	this->writeData_ = new uint8_t[this->width_ * this->height_ * 4];

	return this->writeData_;
}

void MyGUICustom::DirectX11Texture::unlock() {
	if (!this->isLocked_)
		return;

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11DeviceContext* context = renderManager.context;

	context->UpdateSubresource(this->texture, 0, nullptr, this->writeData_, this->width_ * 4, 0);

	delete[] this->writeData_;
	this->writeData_ = nullptr;

	this->isLocked_ = false;
}