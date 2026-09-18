#include "RenderManager.hpp"

#include "../ShaderManager.hpp"

#include "Texture.hpp"
#include "VertexBuffer.hpp"

#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_Timer.h>
#include <MyGui/MyGUI_VertexData.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <Windows.h>

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

#include <string>

#define SAFE_RELEASE(ptr) do { if (ptr) { (ptr)->Release(); (ptr) = nullptr; } } while(0)

struct alignas(16) ConstantBufferData {
	float uvScale[2] = { 1.0f, 1.0f };
};

bool MyGUICustom::DirectX11RenderManager::initialise(ID3D11Device* device) {
	this->device = device;
	this->device->AddRef();

	this->device->GetImmediateContext(&this->context);

	this->vertexFormat = MyGUI::VertexColourType::ColourABGR;

	memset(&this->info, 0, sizeof(this->info));
	this->setViewSize(1920, 1080);

	std::string vertexShaderBytecode;
	std::string vertexShaderBytecodeTextured;

	ShaderManager& shaderManager = ShaderManager::getInstance();
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain", this->shaderPlain.vertexShader, &vertexShaderBytecode)) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain", this->shaderPlain.pixelShader)) return false;

	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain", { "TEXTURED" }, this->shaderTextured.vertexShader, &vertexShaderBytecodeTextured)) return false;
	if (!shaderManager.getShader("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain", { "TEXTURED" }, this->shaderTextured.pixelShader)) return false;

	D3D11_INPUT_ELEMENT_DESC inputElementDescPlain[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	HRESULT result = this->device->CreateInputLayout(inputElementDescPlain, ARRAYSIZE(inputElementDescPlain), vertexShaderBytecode.data(), vertexShaderBytecode.size(), &this->shaderPlain.inputLayout);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create input layout for plain shader. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC inputElementDescTextured[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	result = this->device->CreateInputLayout(inputElementDescTextured, ARRAYSIZE(inputElementDescTextured), vertexShaderBytecodeTextured.data(), vertexShaderBytecodeTextured.size(), &this->shaderTextured.inputLayout);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create input layout for textured shader. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.BorderColor[0] = 0.0f;
	samplerDesc.BorderColor[1] = 0.0f;
	samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 0.0f;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.MaxLOD = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MipLODBias = 0.0f;

	result = this->device->CreateSamplerState(&samplerDesc, &this->samplerState);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create sampler state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0f;

	result = this->device->CreateBlendState(&blendDesc, &this->blendState);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create blend state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(depthDesc));
	depthDesc.DepthEnable = false;
	depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.StencilEnable = false;
	depthDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthDesc.FrontFace = depthDesc.BackFace;

	result = this->device->CreateDepthStencilState(&depthDesc, &this->depthStencilState);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create depth stencil state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(rasterDesc));
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	
	result = this->device->CreateRasterizerState(&rasterDesc, &this->rasterizerState);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create rasterizer state. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_BUFFER_DESC constantBufferDesc;
	ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = this->device->CreateBuffer(&constantBufferDesc, nullptr, &this->constantBuffer);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create constant buffer. HRESULT: 0x%X", result);
		return false;
	}

	return true;
}

void MyGUICustom::DirectX11RenderManager::shutdown() {
	this->destroyAllResources();

	SAFE_RELEASE(this->constantBuffer);
	SAFE_RELEASE(this->shaderPlain.inputLayout);
	SAFE_RELEASE(this->shaderPlain.pixelShader);
	SAFE_RELEASE(this->shaderPlain.vertexShader);
	SAFE_RELEASE(this->shaderTextured.inputLayout);
	SAFE_RELEASE(this->shaderTextured.pixelShader);
	SAFE_RELEASE(this->shaderTextured.vertexShader);
	SAFE_RELEASE(this->samplerState);
	SAFE_RELEASE(this->blendState);
	SAFE_RELEASE(this->depthStencilState);
	SAFE_RELEASE(this->rasterizerState);

	if (this->context) {
		this->context->Release();
		this->context = nullptr;
	}

	if (this->device) {
		this->device->Release();
		this->device = nullptr;
	}
}

MyGUI::ITexture* MyGUICustom::DirectX11RenderManager::createTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::createTexture: Texture with name '%s' already exists. Returning existing texture.", _name.c_str());
		return iterator->second;
	}

	MyGUICustom::DirectX11Texture* texture = new MyGUICustom::DirectX11Texture(_name);
	this->textures[_name] = texture;

	return texture;
}

void MyGUICustom::DirectX11RenderManager::destroyTexture(MyGUI::ITexture* _texture) {
	if (!_texture) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::destroyTexture: Attempted to destroy a null texture.");
		return;
	}

	auto iterator = this->textures.find(_texture->getName());
	if (iterator == this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::destroyTexture: Texture with name '%s' not found in manager.", _texture->getName().c_str());
		return;
	}

	this->textures.erase(iterator);
	delete _texture;
}

MyGUI::ITexture* MyGUICustom::DirectX11RenderManager::getTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end())
		return iterator->second;
	
	return nullptr;
}

void MyGUICustom::DirectX11RenderManager::begin() {
	this->context->OMSetBlendState(this->blendState, nullptr, 0xFFFFFFFF);
	this->context->RSSetState(this->rasterizerState);
	this->context->OMSetDepthStencilState(this->depthStencilState, 0);
	this->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void MyGUICustom::DirectX11RenderManager::end() { }

void MyGUICustom::DirectX11RenderManager::doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	MyGUICustom::DirectX11VertexBuffer* buffer = dynamic_cast<MyGUICustom::DirectX11VertexBuffer*>(_buffer);
	if (!buffer) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::doRender: Invalid vertex buffer provided.");
		return;
	}

	MyGUICustom::DirectX11Texture* texture = dynamic_cast<MyGUICustom::DirectX11Texture*>(_texture);
	if (!texture || !texture->shaderResourceView) {
		this->context->PSSetShader(this->shaderPlain.pixelShader, nullptr, 0);
		this->context->VSSetShader(this->shaderPlain.vertexShader, nullptr, 0);
		this->context->IASetInputLayout(this->shaderPlain.inputLayout);
	} else {
		this->context->PSSetShader(this->shaderTextured.pixelShader, nullptr, 0);
		this->context->VSSetShader(this->shaderTextured.vertexShader, nullptr, 0);
		this->context->IASetInputLayout(this->shaderTextured.inputLayout);

		this->context->PSSetShaderResources(0, 1, &texture->shaderResourceView);
		this->context->PSSetSamplers(0, 1, &this->samplerState);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		if (SUCCEEDED(this->context->Map(this->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
			ConstantBufferData* data = reinterpret_cast<ConstantBufferData*>(mappedResource.pData);
			data->uvScale[0] = static_cast<float>(texture->getWidth()) / static_cast<float>(texture->getRealWidth());
			data->uvScale[1] = static_cast<float>(texture->getHeight()) / static_cast<float>(texture->getRealHeight());

			this->context->Unmap(this->constantBuffer, 0);
		};

		this->context->PSSetConstantBuffers(0, 1, &this->constantBuffer);
	}

	UINT stride = sizeof(MyGUI::Vertex);
	UINT offset = 0;
	this->context->IASetVertexBuffers(0, 1, &buffer->buffer, &stride, &offset);
	this->context->Draw(static_cast<UINT>(_count), 0);
}

void MyGUICustom::DirectX11RenderManager::destroyAllResources() {
	for (auto& pair : this->textures)
		delete pair.second;

	this->textures.clear();
}

void MyGUICustom::DirectX11RenderManager::drawOneFrame() {
	MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
	if (!gui) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::drawOneFrame: Failed to get GUI instance.");
		return;
	}

	uint32_t currentTime = this->timer_.getMilliseconds();
	uint32_t deltaTime = currentTime - this->lastTime_;

	this->onFrameEvent(static_cast<float>(deltaTime) / 1000.0f);
	this->lastTime_ = currentTime;

	this->begin();
	this->onRenderToTarget(this, this->needsUpdate_);
	this->end();

	this->needsUpdate_ = false;
}

void MyGUICustom::DirectX11RenderManager::setViewSize(int _width, int _height) {
	_width = std::max(1, _width);
	_height = std::max(1, _height);

	this->viewSize.set(_width, _height);

	this->info.maximumDepth = 0.0f;
	this->info.hOffset = 0.0f;
	this->info.vOffset = 0.0f;
	this->info.aspectCoef = static_cast<float>(_height) / static_cast<float>(_width);
	this->info.pixScaleX = 1.0f / static_cast<float>(_width);
	this->info.pixScaleY = 1.0f / static_cast<float>(_height);

	this->onResizeView(this->viewSize);

	this->needsUpdate_ = true;
}