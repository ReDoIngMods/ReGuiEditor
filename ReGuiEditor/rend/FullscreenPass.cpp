#include "FullscreenPass.hpp"

#include "ResourceManager.hpp"

#include "Common/PrimitiveTopology.hpp"

#include "Resources/Buffers/ConstantBuffer.hpp"

#include "Resources/PipelineContext.hpp"

#include "Resources/Shaders/PixelShader.hpp"
#include "Resources/Shaders/VertexShader.hpp"

#include <cstdint>
#include <string>
#include <vector>

bool rend::FullscreenPass::init(const rend::FullscreenPassDesc& desc) {
	this->cleanup();

	const std::string name = desc.debugName.empty() ? "FullscreenPass" : desc.debugName;

	if (desc.pixelShader.empty()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): No pixel shader specified.", name.c_str());
		return false;
	}

	if (desc.constantBufferSize % 16 != 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): constantBufferSize (%u) must be a multiple of 16.", name.c_str(), desc.constantBufferSize);
		return false;
	}

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	this->vertexShader_ = resourceManager.createShader<rend::VertexShader>(desc.vertexShader, desc.vertexEntrypoint);
	if (!this->vertexShader_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): Failed to create vertex shader '%s'.", name.c_str(), desc.vertexShader.c_str());

		this->cleanup();
		return false;
	}

	this->pixelShader_ = resourceManager.createShader<rend::PixelShader>(desc.pixelShader, desc.pixelEntrypoint, desc.macros);
	if (!this->pixelShader_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): Failed to create pixel shader '%s'.", name.c_str(), desc.pixelShader.c_str());

		this->cleanup();
		return false;
	}

	if (desc.constantBufferSize > 0) {
		this->constantBuffer_ = resourceManager.createConstantBuffer(desc.constantBufferSize);
		if (!this->constantBuffer_) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): Failed to create constant buffer.", name.c_str());

			this->cleanup();
			return false;
		}

		this->constantBuffer_->setDebugName(name + "::ConstantBuffer");
	}

	this->pipelineContext_ = resourceManager.createPipelineContext();
	if (!this->pipelineContext_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::init(%s): Failed to create pipeline context.", name.c_str());

		this->cleanup();
		return false;
	}

	this->vertexShader_->setDebugName(name + "::VertexShader");
	this->pixelShader_->setDebugName(name + "::PixelShader");
	this->pipelineContext_->setDebugName(name + "::PipelineContext");

	this->constantBufferSize_ = desc.constantBufferSize;
	return true;
}

void rend::FullscreenPass::cleanup() {
	this->pipelineContext_.reset();
	this->constantBuffer_.reset();
	this->pixelShader_.reset();
	this->vertexShader_.reset();

	this->constantBufferSize_ = 0;
}

bool rend::FullscreenPass::draw(const rend::FullscreenDrawParams& params) {
	if (!this->isValid() || !params.target)
		return false;

	// A fully clipped region is perfectly normal (e.g. a window that's scrolled out of view), not an error.
	if (params.viewport.width <= 0.0f || params.viewport.height <= 0.0f)
		return true;

	if (params.constants) {
		if (!this->constantBuffer_) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::draw: Constants were provided but the pass was created with constantBufferSize = 0.");
			return false;
		}

		if (!this->constantBuffer_->writeRawBuffer(params.constants, this->constantBufferSize_)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::draw: Failed to update constant buffer.");
			return false;
		}
	}

	if (!this->pipelineContext_->begin())
		return false;

	this->pipelineContext_->setRenderViews(params.target);
	this->pipelineContext_->setViewports(params.viewport);

	// The triangle is generated from SV_VertexID, so there's no input layout or vertex buffer.
	this->pipelineContext_->setInputLayout(nullptr);
	this->pipelineContext_->setPrimitiveTopology(rend::PrimitiveTopology::TriangleList);

	if (params.blendState)
		this->pipelineContext_->setBlendState(params.blendState);

	this->pipelineContext_->setVertexShader(this->vertexShader_);
	this->pipelineContext_->setPixelShader(this->pixelShader_);

	std::vector<const rend::ConstantBuffer*> constantBuffers;
	if (this->constantBuffer_)
		constantBuffers.push_back(this->constantBuffer_.get());

	constantBuffers.insert(constantBuffers.end(), params.extraConstantBuffers.begin(), params.extraConstantBuffers.end());

	if (!constantBuffers.empty())
		this->pipelineContext_->setConstantBuffers(rend::ShaderStage::Pixel, 0, static_cast<uint32_t>(constantBuffers.size()), constantBuffers.data());

	if (!params.samplerStates.empty())
		this->pipelineContext_->setSamplerStates(rend::ShaderStage::Pixel, 0, static_cast<uint32_t>(params.samplerStates.size()), params.samplerStates.data());

	if (!params.shaderResourceViews.empty())
		this->pipelineContext_->setShaderResourceViews(rend::ShaderStage::Pixel, 0, static_cast<uint32_t>(params.shaderResourceViews.size()), params.shaderResourceViews.data());

	this->pipelineContext_->draw(3, 0);

	// The saved context state keeps whatever is bound to it alive after end(). Unbind the target (it may be the swap
	// chain back buffer, which ResizeBuffers refuses to resize while anything still references it) and the inputs.
	this->pipelineContext_->clearRenderViews();

	if (!params.shaderResourceViews.empty())
		this->pipelineContext_->clearShaderResourceViews<rend::ShaderStage::Pixel>();

	if (!this->pipelineContext_->end()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "FullscreenPass::draw: Failed to end pipeline context.");
		return false;
	}

	return true;
}