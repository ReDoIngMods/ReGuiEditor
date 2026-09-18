#include "RenderManager.hpp"
#include "Texture.hpp"
#include <FreeImage/FreeImage.h>
#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <Windows.h>
#include <cstdint>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>
#include <string>
#include <string.h>
#include <vector>

inline static UINT power2Round(UINT value) {
	UINT result = 1;
	while (result < value)
		result <<= 1;
	
	return result;
}

void MyGUICustom::DirectX11Texture::createManual(int _width, int _height, MyGUI::TextureUsage _usage, MyGUI::PixelFormat _format) {
	this->destroy();

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = this->width_ = power2Round(_width);
	desc.Height = this->height_ = power2Round(_height);
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
	std::string fullName = MyGUI::DataManager::getInstance().getDataPath(_filename);
	if (fullName.empty()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to get full path for file: %s", _filename.c_str());
		return;
	}

	this->destroy();

	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(fullName.c_str(), 0);
	if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(fullName.c_str());
	if (fif == FIF_UNKNOWN) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Unknown image format for file: %s", _filename.c_str());
		return;
	}

	FIBITMAP* bitmap = FreeImage_Load(fif, fullName.c_str());
	if (!bitmap) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to load image file: %s", _filename.c_str());
		return;
	}

	{ // Convert to 32-bit RGBA if not already
		FIBITMAP* newBitmap = FreeImage_ConvertTo32Bits(bitmap);
		FreeImage_Unload(bitmap);
		bitmap = newBitmap;

		if (!bitmap) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to convert image to 32-bit RGBA: %s", _filename.c_str());
			return;
		}
	}

	if (!FreeImage_FlipVertical(bitmap)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11Texture::loadFromFile: Failed to flip image vertically: %s", _filename.c_str());
		
		FreeImage_Unload(bitmap);
		return;
	}

	this->realWidth_ = FreeImage_GetWidth(bitmap);
	this->realHeight_ = FreeImage_GetHeight(bitmap);

	this->width_ = power2Round(this->realWidth_);
	this->height_ = power2Round(this->realHeight_);

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
	desc.Usage = D3D11_USAGE_IMMUTABLE;
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

	this->width_ = 0;
	this->height_ = 0;
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
	
	context->UpdateSubresource(this->texture, 0, 0, this->writeData_, this->width_ * 4, 0);
	
	delete[] this->writeData_;
	this->writeData_ = nullptr;

	this->isLocked_ = false;
}