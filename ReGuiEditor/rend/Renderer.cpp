#include "Renderer.hpp"

#include "D3D11Device.hpp"

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "../Application.hpp"

#include <Windows.h>

void rend::Renderer::render() {
	Application::getInstance().render();
}

bool rend::Renderer::initRenderObjects() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, __FUNCTION__);

	this->shaderManager_ = rend::ShaderManager::createInstance();

	return true;
}

void rend::Renderer::shutdownRenderObjects() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, __FUNCTION__);

	if (this->shaderManager_) {
		this->shaderManager_->destroyInstance();
		this->shaderManager_ = nullptr;
	}
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
	this->d3d11Device_->setInitCallback([this] { return this->initRenderObjects(); });
	this->d3d11Device_->setShutdownCallback([this] { this->shutdownRenderObjects(); });

	if (!this->d3d11Device_->init()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize D3D11Device!");
		return false;
	}

	return true;
}