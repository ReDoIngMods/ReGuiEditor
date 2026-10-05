#include "GuiBlurManager.hpp"

#include <rend/Common/Format.hpp>
#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/Viewport.hpp>

#include <rend/FullscreenPass.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/ShaderResourceView.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

bool GuiBlurManager::createRenderObjects() {
	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	const auto createPass = [](rend::FullscreenPass& pass, const char* entrypoint, const char* debugName) {
		rend::FullscreenPassDesc passDesc;
		passDesc.pixelShader = "$RGE_DATA/Shaders/post_blur.hlsl";
		passDesc.pixelEntrypoint = entrypoint;
		passDesc.constantBufferSize = sizeof(BlurParameters);
		passDesc.debugName = debugName;

		if (!pass.init(passDesc)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderObjects: Failed to create fullscreen pass (%s).", debugName);
			return false;
		}

		return true;
	};

	if (!createPass(this->copyPass_, "psMainCopy", "GuiBlurManager::Copy"))
		return false;

	if (!createPass(this->blurHorizontalPass_, "psMainHorizontal", "GuiBlurManager::BlurHorizontal"))
		return false;

	if (!createPass(this->blurVerticalPass_, "psMainVertical", "GuiBlurManager::BlurVertical"))
		return false;

	this->samplerState_ = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Clamp);
	if (!this->samplerState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderObjects: Failed to create sampler state.");
		return false;
	}

	this->samplerState_->setDebugName("GuiBlurManager::SamplerState");

	return true;
}

rend::RenderViewHandle GuiBlurManager::createRenderTarget(const std::string& name) {
	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	rend::Texture2DHandle texture = resourceManager.createTexture2D(this->lowWidth_, this->lowHeight_, rend::Format::R8G8B8A8_UNORM, rend::Texture2DFlags::ShaderResource | rend::Texture2DFlags::RenderTarget);
	if (!texture) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderTarget: Failed to create texture for '%s'.", name.c_str());
		return nullptr;
	}
	texture->setDebugName("GuiBlurManager " + name + " (Texture)");

	rend::RenderViewHandle renderView = resourceManager.createRenderView(texture);
	if (!renderView) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::createRenderTarget: Failed to create render view for '%s'.", name.c_str());
		return nullptr;
	}
	renderView->setDebugName("GuiBlurManager " + name + " (RenderView)");
	return renderView;
}

bool GuiBlurManager::createRenderTargets() {
	this->captureView_ = this->createRenderTarget("Capture");
	if (!this->captureView_) return false;

	this->pingView_ = this->createRenderTarget("Ping");
	if (!this->pingView_) return false;

	this->pongView_ = this->createRenderTarget("Pong");
	return this->pongView_ != nullptr;
}

void GuiBlurManager::cleanupRenderTargets() {
	this->pongView_.reset();
	this->pingView_.reset();
	this->captureView_.reset();
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

	if (!this->createRenderTargets()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::initialize: Failed to create render targets.");
		return false;
	}

	return true;
}

void GuiBlurManager::cleanup() {
	this->cleanupRenderTargets();

	this->samplerState_.reset();

	this->blurVerticalPass_.cleanup();
	this->blurHorizontalPass_.cleanup();
	this->copyPass_.cleanup();

	this->lowWidth_ = 0;
	this->lowHeight_ = 0;
	this->fullWidth_ = 0;
	this->fullHeight_ = 0;
}

bool GuiBlurManager::resize(uint32_t fullWidth, uint32_t fullHeight) {
	this->cleanupRenderTargets();

	this->fullWidth_ = fullWidth;
	this->fullHeight_ = fullHeight;
	this->lowWidth_ = std::max<uint32_t>(1, fullWidth / this->downsample_);
	this->lowHeight_ = std::max<uint32_t>(1, fullHeight / this->downsample_);

	if (!this->createRenderTargets()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurManager::resize: Failed to recreate render targets.");
		return false;
	}

	return true;
}

void GuiBlurManager::blurPass(const rend::ShaderResourceView* sourceView, rend::RenderView* destinationView, rend::FullscreenPass& pass, float texelSizeX, float texelSizeY) {
	const BlurParameters parameters = { texelSizeX, texelSizeY };

	rend::FullscreenDrawParams params;
	params.target = destinationView;
	params.viewport = rend::Viewport(0.0f, 0.0f, static_cast<float>(this->lowWidth_), static_cast<float>(this->lowHeight_), 0.0f, 1.0f);
	params.constants = &parameters;
	params.shaderResourceViews = { sourceView };
	params.samplerStates = { this->samplerState_.get() };

	// The pass unbinds the source and the target again afterwards, so the target of one pass can be the source of the next.
	pass.draw(params);
}

void GuiBlurManager::captureAndBlur(const rend::ShaderResourceView* sourceView) {
	if (!this->pongView_ || !this->copyPass_.isValid() || !this->blurHorizontalPass_.isValid() || !this->blurVerticalPass_.isValid())
		return;

	const float texelSizeX = 1.0f / static_cast<float>(this->lowWidth_);
	const float texelSizeY = 1.0f / static_cast<float>(this->lowHeight_);

	this->blurPass(sourceView, this->captureView_.get(), this->copyPass_, 0.0f, 0.0f);
	this->blurPass(this->captureView_.get(), this->pingView_.get(), this->blurHorizontalPass_, texelSizeX, 0.0f);
	this->blurPass(this->pingView_.get(), this->pongView_.get(), this->blurVerticalPass_, 0.0f, texelSizeY);
}
