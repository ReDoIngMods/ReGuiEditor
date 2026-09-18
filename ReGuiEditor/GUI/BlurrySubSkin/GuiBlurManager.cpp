#include "GuiBlurManager.hpp"

#include "../../Application.hpp"
#include "../ShaderManager.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include "../Utilities/D3D11RenderTarget.hpp"

#define SAFE_RELEASE(ptr) do { if (ptr) { (ptr)->Release(); (ptr) = nullptr; } } while(0)


void GuiBlurManager::cleanupRenderTargets(ID3D11Texture2D*& texture, ID3D11RenderTargetView*& targetView, ID3D11ShaderResourceView*& shaderResourceView) {
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(targetView);
	SAFE_RELEASE(texture);
}

bool GuiBlurManager::createRenderObjects() {
	ShaderManager& shaderManager = ShaderManager::getInstance();
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_fullscreen_triangle.hlsl", "vsMain"          , this->shaderVertex_              )) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/post_blur.hlsl"              , "psMainHorizontal", this->shaderPixelBlurHorizontal_ )) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/post_blur.hlsl"              , "psMainVertical"  , this->shaderPixelBlurVertical_   )) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/post_blur.hlsl"              , "psMainCopy"      , this->shaderPixelCopy_           )) return false;

	Application& application = Application::getInstance();
	ID3D11Device* device = application.d3d11Device.getDevice();

	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	HRESULT result = device->CreateSamplerState(&samplerDesc, &this->samplerState_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderObjects: Failed to create sampler state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.ByteWidth = sizeof(ConstantBuffer);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = device->CreateBuffer(&bufferDesc, nullptr, &this->constantBuffer_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderObjects: Failed to create constant buffer. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

void GuiBlurManager::cleanupRenderObjects() {
	SAFE_RELEASE(this->constantBuffer_);
	SAFE_RELEASE(this->samplerState_);

	SAFE_RELEASE(this->shaderPixelCopy_);
	SAFE_RELEASE(this->shaderPixelBlurVertical_);
	SAFE_RELEASE(this->shaderPixelBlurHorizontal_);
	SAFE_RELEASE(this->shaderVertex_);
}

bool GuiBlurManager::initialize(uint32_t fullWidth, uint32_t fullHeight, uint32_t downsample) {
	this->downsample_ = downsample ? downsample : 1;
	this->fullWidth_ = fullWidth;
	this->fullHeight_ = fullHeight;
	this->lowWidth_ = std::max<uint32_t>(1, fullWidth / this->downsample_);
	this->lowHeight_ = std::max<uint32_t>(1, fullHeight / this->downsample_);

	if (!this->createRenderObjects()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::initialize: Failed to create render objects.");
		return false;
	}

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->captureTexture, this->captureTargetView, this->captureShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::initialize: Failed to create capture render target.");
		return false;
	}

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->pingTexture, this->pingTargetView, this->pingShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::initialize: Failed to create ping render target.");
		return false;
	}

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->pongTexture, this->pongTargetView, this->pongShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::initialize: Failed to create pong render target.");
		return false;
	}

	return true;
}

void GuiBlurManager::cleanup() {
	this->cleanupRenderTargets(this->pongTexture, this->pongTargetView, this->pongShaderResourceView);
	this->cleanupRenderTargets(this->pingTexture, this->pingTargetView, this->pingShaderResourceView);
	this->cleanupRenderTargets(this->captureTexture, this->captureTargetView, this->captureShaderResourceView);

	this->cleanupRenderObjects();

	this->lowWidth_ = 0;
	this->lowHeight_ = 0;
	this->fullWidth_ = 0;
	this->fullHeight_ = 0;
}

bool GuiBlurManager::resize(uint32_t fullWidth, uint32_t fullHeight) {
	this->cleanupRenderTargets(this->pongTexture, this->pongTargetView, this->pongShaderResourceView);
	this->cleanupRenderTargets(this->pingTexture, this->pingTargetView, this->pingShaderResourceView);
	this->cleanupRenderTargets(this->captureTexture, this->captureTargetView, this->captureShaderResourceView);

	this->fullWidth_ = fullWidth;
	this->fullHeight_ = fullHeight;
	this->lowWidth_ = std::max<uint32_t>(1, fullWidth / this->downsample_);
	this->lowHeight_ = std::max<uint32_t>(1, fullHeight / this->downsample_);

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->captureTexture, this->captureTargetView, this->captureShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::resize: Failed to recreate capture render target.");
		return false;
	}

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->pingTexture, this->pingTargetView, this->pingShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::resize: Failed to recreate ping render target.");
		return false;
	}

	if (!RenderTargetUtils::createColorfulRTV(this->lowWidth_, this->lowHeight_, DXGI_FORMAT_R8G8B8A8_UNORM, this->pongTexture, this->pongTargetView, this->pongShaderResourceView)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::resize: Failed to recreate pong render target.");
		return false;
	}

	return true;
}

void GuiBlurManager::blurPass(ID3D11ShaderResourceView* sourceView, ID3D11RenderTargetView* destinationView, ID3D11PixelShader* pixelShader, float texelSizeX, float texelSizeY) {
	ID3D11DeviceContext* context = Application::getInstance().d3d11Device.getContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT result = context->Map(this->constantBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (SUCCEEDED(result)) {
		ConstantBuffer* data = static_cast<ConstantBuffer*>(mapped.pData);
		data->texelSizeX = texelSizeX;
		data->texelSizeY = texelSizeY;
		context->Unmap(this->constantBuffer_, 0);
	}

	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(viewport));
	viewport.Width = static_cast<FLOAT>(this->lowWidth_);
	viewport.Height = static_cast<FLOAT>(this->lowHeight_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	context->RSSetViewports(1, &viewport);
	context->OMSetRenderTargets(1, &destinationView, nullptr);

	context->VSSetShader(this->shaderVertex_, nullptr, 0);

	context->PSSetShader(pixelShader, nullptr, 0);
	context->PSSetShaderResources(0, 1, &sourceView);
	context->PSSetSamplers(0, 1, &this->samplerState_);
	context->PSSetConstantBuffers(0, 1, &this->constantBuffer_);

	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->Draw(3, 0);

	ID3D11ShaderResourceView* nullView = nullptr;
	context->PSSetShaderResources(0, 1, &nullView);
}

void GuiBlurManager::captureAndBlur(ID3D11ShaderResourceView* sourceView) {
	this->blurPass(sourceView, this->captureTargetView, this->shaderPixelCopy_, 0.0f, 0.0f);

	float texelSizeX = 1.0f / static_cast<float>(this->lowWidth_);
	float texelSizeY = 1.0f / static_cast<float>(this->lowHeight_);

	this->blurPass(this->captureShaderResourceView, this->pingTargetView, this->shaderPixelBlurHorizontal_, texelSizeX, 0.0f);
	this->blurPass(this->pingShaderResourceView, this->pongTargetView, this->shaderPixelBlurVertical_, 0.0f, texelSizeY);
}
