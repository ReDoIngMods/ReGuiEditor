#include "BackgroundImageHelper.hpp"

#include "../Application.hpp"

#include "ShaderManager.hpp"

#include <GUI/MyGuiDX11/Texture.hpp>

#include <MyGui/MyGUI_RenderManager.h>

#include <Windows.h>

#include <d3d11.h>
#include <d3dcommon.h>

#include <string>

#define SAFE_RELEASE(ptr) do { if (ptr) { (ptr)->Release(); (ptr) = nullptr; } } while(0)

bool BackgroundImageHelper::initialize() {
	ShaderManager& shaderManager = ShaderManager::getInstance();
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_fullscreen_triangle.hlsl", "vsMain", this->vertexShader_)) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_background.hlsl", "psMain", this->pixelShader_)) return false;
	
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	ID3D11Device* device = Application::getInstance().d3d11Device.getDevice();
	HRESULT result = device->CreateSamplerState(&samplerDesc, &this->samplerState_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BackgroundImageHelper::initialize: Failed to create sampler state. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

void BackgroundImageHelper::cleanup() {
	SAFE_RELEASE(this->samplerState_);
	SAFE_RELEASE(this->pixelShader_);
	SAFE_RELEASE(this->vertexShader_);
}

void BackgroundImageHelper::setBackgroundTexture(const std::string& texturePath) {
	if (this->backgroundTexture_)
		this->backgroundTexture_ = nullptr;

	if (texturePath.empty())
		return;

	this->backgroundTexture_ = dynamic_cast<MyGUICustom::DirectX11Texture*>(MyGUI::RenderManager::getInstance().getTexture(texturePath));
	if (!this->backgroundTexture_) {
		this->backgroundTexture_ = dynamic_cast<MyGUICustom::DirectX11Texture*>(MyGUI::RenderManager::getInstance().createTexture(texturePath));
		if (!this->backgroundTexture_) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "BackgroundImageHelper::setBackgroundTexture: Failed to load texture '%s'.", texturePath.c_str());
			return;
		}

		this->backgroundTexture_->loadFromFile(texturePath);
	}
}

void BackgroundImageHelper::render() {
	if (!this->backgroundTexture_)
		return;

	ID3D11DeviceContext* context = Application::getInstance().d3d11Device.getContext();
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->VSSetShader(this->vertexShader_, nullptr, 0);
	context->PSSetShader(this->pixelShader_, nullptr, 0);

	ID3D11ShaderResourceView* resources[] = { this->backgroundTexture_->shaderResourceView };
	context->PSSetSamplers(0, 1, &this->samplerState_);
	context->PSSetShaderResources(0, 1, resources);

	context->Draw(3, 0);

	ID3D11ShaderResourceView* nullResources[] = { nullptr };
	context->PSSetShaderResources(0, 1, nullResources);
}