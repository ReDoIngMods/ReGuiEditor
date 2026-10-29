#include "Renderer.hpp"

#include "D3D11Device.hpp"

#include "ResourceManager.hpp"
#include "ShaderManager.hpp"

#include "../Application.hpp"

void rend::Renderer::render() {
	Application::getInstance().render();
}

bool rend::Renderer::initRenderObjects() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, __FUNCTION__);

	this->shaderManager_ = rend::ShaderManager::createInstance();

	return true;
}

bool rend::Renderer::init() {
	this->resourceManager_ = rend::ResourceManager::createInstance();

	this->d3d11Device_ = rend::D3D11Device::createInstance();
	this->d3d11Device_->setRenderCallback([this] { this->render(); });
	this->d3d11Device_->setInitCallback([this] { return this->initRenderObjects(); });

	if (!this->d3d11Device_->init()) {
		CONSOLE_ERROR("Failed to initialize D3D11Device!");
		return false;
	}

	return true;
}