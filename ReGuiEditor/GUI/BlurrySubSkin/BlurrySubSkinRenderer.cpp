#include "BlurrySubSkinRenderer.hpp"

#include "../../Application.hpp"
#include "../ShaderManager.hpp"

#include <MyGui/MyGUI_Types.h>

#include <Windows.h>

#include <d3d11.h>
#include <d3dcommon.h>

#include <cstring>

#define SAFE_RELEASE(ptr) do { if (ptr) { (ptr)->Release(); (ptr) = nullptr; } } while(0)

bool BlurrySubSkinRenderer::initialize() {
	if (this->initialized_) return true;

	ShaderManager& shaderManager = ShaderManager::getInstance();
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_fullscreen_triangle.hlsl", "vsMain", this->vertexShader_)) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_blur_background.hlsl", "psMain", this->pixelShader_)) return false;

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
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkinRenderer::initialize: Failed to create sampler state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.ByteWidth = sizeof(ConstantBufferData);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = device->CreateBuffer(&bufferDesc, nullptr, &this->paramsBuffer_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BlurrySubSkinRenderer::initialize: Failed to create constant buffer. HRESULT: 0x%X", result);
		return false;
	}

	this->initialized_ = true;
	return true;
}

void BlurrySubSkinRenderer::cleanup() {
	SAFE_RELEASE(this->paramsBuffer_);
	SAFE_RELEASE(this->samplerState_);
	SAFE_RELEASE(this->pixelShader_);
	SAFE_RELEASE(this->vertexShader_);

	this->initialized_ = false;
}

void BlurrySubSkinRenderer::composite(ID3D11ShaderResourceView* backdropView, const MyGUI::FloatRect& backdropUV, ID3D11ShaderResourceView* atlasView, const MyGUI::FloatRect& atlasUV, ID3D11RenderTargetView* destinationView, int destinationWidth, int destinationHeight) {
	if (!this->initialized_)
		return;

	ID3D11DeviceContext* context = Application::getInstance().d3d11Device.getContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT result = context->Map(this->paramsBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (SUCCEEDED(result)) {
		ConstantBufferData* data = static_cast<ConstantBufferData*>(mapped.pData);
		memcpy(data->backdropUV, &backdropUV, sizeof(data->backdropUV));
		memcpy(data->atlasUV, &atlasUV, sizeof(data->atlasUV));

		context->Unmap(this->paramsBuffer_, 0);
	}

	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(viewport));
	viewport.Width = static_cast<FLOAT>(destinationWidth);
	viewport.Height = static_cast<FLOAT>(destinationHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	context->RSSetViewports(1, &viewport);
	context->OMSetRenderTargets(1, &destinationView, nullptr);

	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->VSSetShader(this->vertexShader_, nullptr, 0);
	context->PSSetShader(this->pixelShader_, nullptr, 0);

	ID3D11ShaderResourceView* views[2] = { backdropView, atlasView };
	context->PSSetShaderResources(0, 2, views);
	context->PSSetSamplers(0, 1, &this->samplerState_);
	context->PSSetConstantBuffers(0, 1, &this->paramsBuffer_);

	context->Draw(3, 0);

	ID3D11ShaderResourceView* nullViews[2] = { nullptr, nullptr };
	context->PSSetShaderResources(0, 2, nullViews);
}