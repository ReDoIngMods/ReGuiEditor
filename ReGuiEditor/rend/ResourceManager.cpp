#include "ResourceManager.hpp"

#include "Common/BlendMode.hpp"
#include "Common/BlendOperand.hpp"
#include "Common/ComparisonFunction.hpp"
#include "Common/CullingMode.hpp"
#include "Common/Format.hpp"
#include "Common/RefCounter.hpp"
#include "Common/SamplerAddressMode.hpp"
#include "Common/SamplerFilter.hpp"

#include "Resources/SwapChain.hpp"

#include "Resources/Buffers/ConstantBuffer.hpp"
#include "Resources/Buffers/IndexBuffer.hpp"
#include "Resources/Buffers/VertexBuffer.hpp"

#include "Resources/BlendState.hpp"
#include "Resources/DepthStencilState.hpp"
#include "Resources/PipelineContext.hpp"
#include "Resources/RasterizerState.hpp"
#include "Resources/SamplerState.hpp"

#include "Resources/RenderView.hpp"
#include "Resources/Texture2D.hpp"

#include "Resources/Resource.hpp"

#include "Resources/Shaders/InputLayout.hpp"
#include "Resources/Shaders/PixelShader.hpp"
#include "Resources/Shaders/VertexShader.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct ID3D11Texture2D;

bool rend::ResourceManager::registerResource(rend::Resource* resource) {
	std::lock_guard<std::mutex> lock(this->resourcesMutex_);

	auto iterator = std::find(this->resources_.begin(), this->resources_.end(), resource);
	if (iterator != this->resources_.end())
		return false;

	this->resources_.push_back(resource);
	this->resourceCount_.fetch_add(1, std::memory_order_relaxed);
	return true;
}

bool rend::ResourceManager::unregisterResource(rend::Resource* resource) {
	std::lock_guard<std::mutex> lock(this->resourcesMutex_);

	auto iterator = std::find(this->resources_.begin(), this->resources_.end(), resource);
	if (iterator == this->resources_.end())
		return false;

	this->resources_.erase(iterator);
	this->resourceCount_.fetch_sub(1, std::memory_order_relaxed);
	return true;
}

void rend::ResourceManager::cleanup() {
	std::vector<rend::Resource*> leaked;
	{
		std::lock_guard<std::mutex> lock(this->resourcesMutex_);
		leaked.swap(this->resources_);
	}

	for (rend::Resource* resource : leaked) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "!!! LEAK !!! Resource \"%s\" was not released before shutdown!", resource->getTypeName());
		delete resource;
	}
}

rend::SwapChainHandle rend::ResourceManager::createSwapChain(void* window) {
	rend::SwapChain* swapChain = new rend::SwapChain;
	if (!swapChain->init(window)) {
		swapChain->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(swapChain);
}

rend::Texture2DHandle rend::ResourceManager::createTexture2D(uint32_t width, uint32_t height, rend::Format format, rend::Texture2DFlags flags) {
	rend::Texture2D* texture = new rend::Texture2D;
	if (!texture->init(width, height, format, flags)) {
		texture->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(texture);
}

rend::Texture2DHandle rend::ResourceManager::createTexture2DNative(ID3D11Texture2D* nativeTexture) {
	rend::Texture2D* texture = new rend::Texture2D;
	if (!texture->initFromNative(nativeTexture)) {
		texture->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(texture);
}

rend::RenderViewHandle rend::ResourceManager::createRenderView(rend::Texture2D* texture) {
	rend::RenderView* renderView = new rend::RenderView;
	if (!renderView->init(texture)) {
		renderView->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(renderView);
}

rend::VertexShaderHandle rend::ResourceManager::createVertexShader(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) {
	rend::VertexShader* vertexShader = new rend::VertexShader;
	if (!vertexShader->init(shaderName, entrypoint, macros)) {
		vertexShader->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(vertexShader);
}

rend::PixelShaderHandle rend::ResourceManager::createPixelShader(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) {
	rend::PixelShader* pixelShader = new rend::PixelShader;
	if (!pixelShader->init(shaderName, entrypoint, macros)) {
		pixelShader->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(pixelShader);
}

rend::InputLayoutHandle rend::ResourceManager::createInputLayout(const rend::InputLayoutDesc& inputElementDescs, rend::VertexShader* vertexShader) {
	rend::InputLayout* inputLayout = new rend::InputLayout;
	if (!inputLayout->init(inputElementDescs, vertexShader)) {
		inputLayout->Release();
		return nullptr;
	}
	
	return rend::makeRefCountPtr(inputLayout);
}

rend::IndexBufferHandle rend::ResourceManager::createIndexBuffer(rend::IndexBufferFormat format, uint32_t indexCount, const void* initialData) {
	rend::IndexBuffer* indexBuffer = new rend::IndexBuffer;
	if (!indexBuffer->init(format, indexCount, initialData)) {
		indexBuffer->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(indexBuffer);
}

rend::VertexBufferHandle rend::ResourceManager::createVertexBuffer(uint32_t vertexSize, uint32_t vertexCount, const void* initialData) {
	rend::VertexBuffer* vertexBuffer = new rend::VertexBuffer;
	if (!vertexBuffer->init(vertexSize, vertexCount, initialData)) {
		vertexBuffer->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(vertexBuffer);
}

rend::ConstantBufferHandle rend::ResourceManager::createConstantBuffer(uint32_t bufferSize, const void* initialData) {
	rend::ConstantBuffer* constantBuffer = new rend::ConstantBuffer;
	if (!constantBuffer->init(bufferSize, initialData)) {
		constantBuffer->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(constantBuffer);
}

rend::RasterizerStateHandle rend::ResourceManager::createRasterizerState(const rend::RasterizerDesc& desc) {
	rend::RasterizerState* rasterizerState = new rend::RasterizerState;
	if (!rasterizerState->init(desc)) {
		rasterizerState->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(rasterizerState);
}

rend::RasterizerStateHandle rend::ResourceManager::createDefaultRasterizerState() {
	rend::RasterizerDesc desc;
	desc.cullMode = rend::CullingMode::Back;
	desc.depthClip = true;
	desc.multisample = false;
	desc.antialiasedLines = false;

	return this->createRasterizerState(desc);
}

rend::BlendStateHandle rend::ResourceManager::createBlendState(const rend::BlendDesc& desc) {
	rend::BlendState* blendState = new rend::BlendState;
	if (!blendState->init(desc)) {
		blendState->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(blendState);
}

rend::BlendStateHandle rend::ResourceManager::createDefaultBlendState() {
	rend::BlendDesc desc;
	desc.alphaToCoverage = false;

	for (rend::BlendDesc::RenderTargetBlendDesc& renderTarget : desc.renderTargets) {
		renderTarget.blendEnable = true;
		
		renderTarget.srcBlend = rend::BlendMode::SrcAlpha;
		renderTarget.destBlend = rend::BlendMode::InvSrcAlpha;
		renderTarget.blendOperand = rend::BlendOperand::Add;
		
		renderTarget.srcAlphaBlend = rend::BlendMode::One;
		renderTarget.destAlphaBlend = rend::BlendMode::InvSrcAlpha;
		renderTarget.alphaBlendOperand = rend::BlendOperand::Add;

		renderTarget.renderTargetWriteMask = rend::ColorWriteMask::All;
	}

	return this->createBlendState(desc);
}

rend::SamplerStateHandle rend::ResourceManager::createSamplerState(const rend::SamplerDesc& desc) {
	rend::SamplerState* samplerState = new rend::SamplerState;
	if (!samplerState->init(desc)) {
		samplerState->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(samplerState);
}

rend::SamplerStateHandle rend::ResourceManager::createLinearSamplerState(rend::SamplerAddressMode addressMode) {
	rend::SamplerDesc desc;
	desc.filter = rend::SamplerFilter::Linear;
	desc.addressU = addressMode;
	desc.addressV = addressMode;
	desc.addressW = addressMode;

	return this->createSamplerState(desc);
}

rend::SamplerStateHandle rend::ResourceManager::createPointSamplerState(rend::SamplerAddressMode addressMode) {
	rend::SamplerDesc desc;
	desc.filter = rend::SamplerFilter::Point;
	desc.addressU = addressMode;
	desc.addressV = addressMode;
	desc.addressW = addressMode;

	return this->createSamplerState(desc);
}

rend::SamplerStateHandle rend::ResourceManager::createAnisotropicSamplerState(rend::SamplerAddressMode addressMode, uint32_t maxAnisotropy) {
	rend::SamplerDesc desc;
	desc.filter = rend::SamplerFilter::Anisotropic;
	desc.addressU = addressMode;
	desc.addressV = addressMode;
	desc.addressW = addressMode;
	desc.maxAnisotropy = maxAnisotropy;

	return this->createSamplerState(desc);
}

rend::DepthStencilStateHandle rend::ResourceManager::createDepthStencilState(const rend::DepthStencilDesc& desc) {
	rend::DepthStencilState* depthStencilState = new rend::DepthStencilState();
	if (!depthStencilState->init(desc)) {
		depthStencilState->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(depthStencilState);
}

rend::DepthStencilStateHandle rend::ResourceManager::createDefaultDepthStencilState() {
	rend::DepthStencilDesc desc;
	desc.depthEnable = true;
	desc.depthWriteMask = true;
	desc.depthFunc = rend::ComparisonFunction::Less;
	desc.stencilEnable = false;
	desc.stencilReadMask = 0xFF;
	desc.stencilWriteMask = 0xFF;

	return this->createDepthStencilState(desc);
}

rend::PipelineContextHandle rend::ResourceManager::createPipelineContext(const std::string& name) {
	rend::PipelineContext* pipelineContext = new rend::PipelineContext();
	if (!pipelineContext->init()) {
		pipelineContext->Release();
		return nullptr;
	}

	return rend::makeRefCountPtr(pipelineContext);
}