#include "Renderer.hpp"

#include "D3D11Device.hpp"

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "../Application.hpp"

#include <Windows.h>

bool rend::Renderer::createRTV() {
	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	this->backBufferTexture_ = resourceManager.createTexture2DFromBackBuffer();
	if (!this->backBufferTexture_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create back buffer texture!");
		return false;
	}

	this->backBufferTexture_->setDebugName("BackBufferTexture");

	this->backBufferRenderView_ = resourceManager.createRenderView(this->backBufferTexture_);
	if (!this->backBufferRenderView_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create back buffer render view!");

		this->backBufferTexture_->Release();
		this->backBufferTexture_ = nullptr;

		return false;
	}

	this->backBufferRenderView_->setDebugName("BackBufferRenderView");

	return true;
}

void rend::Renderer::releaseRTV() {
	this->backBufferRenderView_.reset();
	this->backBufferTexture_.reset();
}

void rend::Renderer::render() {
	Application::getInstance().render();
}

bool rend::Renderer::resizeBefore(UINT width, UINT height) {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "resizeBefore(%u, %u)", width, height);

	this->releaseRTV();
	return true;
}

bool rend::Renderer::resizeAfter(UINT width, UINT height) {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "resizeAfter(%u, %u)", width, height);

	if (!this->createRTV()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create render target view after resize!");
		return false;
	}

	Application::getInstance().onResize(width, height);
	return true;
}

bool rend::Renderer::initRenderObjects() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, __FUNCTION__);

	if (!this->createRTV()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create render target view!");
		return false;
	}

	this->shaderManager_ = rend::ShaderManager::createInstance();

	return true;
}

void rend::Renderer::shutdownRenderObjects() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, __FUNCTION__);

	if (this->shaderManager_) {
		this->shaderManager_->destroyInstance();
		this->shaderManager_ = nullptr;
	}

	this->releaseRTV();
}

rend::Renderer::~Renderer() {
	if (this->d3d11Device_) {
		this->d3d11Device_->destroyInstance();
		this->d3d11Device_ = nullptr;
	}

	if (this->resourceManager_) {
		this->resourceManager_->destroyInstance();
		this->resourceManager_ = nullptr;
	}
}

bool rend::Renderer::init(HWND window) {
	this->resourceManager_ = rend::ResourceManager::createInstance();

	this->d3d11Device_ = rend::D3D11Device::createInstance();
	this->d3d11Device_->setRenderCallback([this] { this->render(); });
	this->d3d11Device_->setResizeBeforeCallback([this](UINT width, UINT height) { return this->resizeBefore(width, height); });
	this->d3d11Device_->setResizeAfterCallback([this](UINT width, UINT height) { return this->resizeAfter(width, height); });
	this->d3d11Device_->setInitCallback([this] { return this->initRenderObjects(); });
	this->d3d11Device_->setShutdownCallback([this] { this->shutdownRenderObjects(); });

	if (!this->d3d11Device_->init(window)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize D3D11Device!");
		return false;
	}

	return true;
}

bool rend::Renderer::resize(UINT width, UINT height) {
	return this->d3d11Device_ && this->d3d11Device_->resize(width, height);
}

bool rend::Renderer::isOccluded() const {
	return this->d3d11Device_ && this->d3d11Device_->isOccluded();
}