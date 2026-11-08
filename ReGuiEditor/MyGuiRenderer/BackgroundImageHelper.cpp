#include "BackgroundImageHelper.hpp"

#include "RenderPlatform/RPTexture.hpp"

#include <MyGui/MyGUI_RenderManager.h>

#include <rend/ResourceManager.hpp>

#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/Viewport.hpp>

#include <rend/FullscreenPass.hpp>

#include <rend/Resources/RenderView.hpp>

#include <string>

struct alignas(16) ConstantBufferData {
	float screenResolution[2];
	float imageResolution[2];
};

bool BackgroundImageHelper::initialize() {
	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	rend::FullscreenPassDesc passDesc;
	passDesc.pixelShader = "$RGE_DATA/Shaders/gui_background.hlsl";
	passDesc.constantBufferSize = sizeof(ConstantBufferData);
	passDesc.debugName = "BackgroundImage";

	if (!this->fullscreenPass_.init(passDesc)) {
		CONSOLE_ERROR("BackgroundImageHelper::initialize: Failed to create fullscreen pass.");
		return false;
	}

	this->samplerState_ = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Wrap);
	if (!this->samplerState_) {
		CONSOLE_ERROR("BackgroundImageHelper::initialize: Failed to create sampler state.");
		return false;
	}
	this->samplerState_->setDebugName("BackgroundImage::SamplerState");

	return true;
}

void BackgroundImageHelper::cleanup() {
	this->samplerState_.reset();
	this->fullscreenPass_.cleanup();
}

void BackgroundImageHelper::setBackgroundTexture(const std::string& texturePath) {
	if (this->backgroundTexture_)
		this->backgroundTexture_ = nullptr;

	if (texturePath.empty())
		return;

	this->backgroundTexture_ = dynamic_cast<RPTexture*>(MyGUI::RenderManager::getInstance().getTexture(texturePath));
	if (!this->backgroundTexture_) {
		this->backgroundTexture_ = dynamic_cast<RPTexture*>(MyGUI::RenderManager::getInstance().createTexture(texturePath));
		if (!this->backgroundTexture_) {
			CONSOLE_ERROR("BackgroundImageHelper::setBackgroundTexture: Failed to load texture '%s'.", texturePath.c_str());
			return;
		}

		this->backgroundTexture_->loadFromFile(texturePath);
	}
}

void BackgroundImageHelper::render(rend::RenderView* renderView, float width, float height) {
	if (!this->backgroundTexture_ || !this->fullscreenPass_.isValid())
		return;

	ConstantBufferData cbData = {};
	cbData.screenResolution[0] = width;
	cbData.screenResolution[1] = height;
	cbData.imageResolution[0] = static_cast<float>(this->backgroundTexture_->getWidth());
	cbData.imageResolution[1] = static_cast<float>(this->backgroundTexture_->getHeight());

	rend::FullscreenDrawParams params;
	params.target = renderView;
	params.viewport = rend::Viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
	params.constants = &cbData;
	params.shaderResourceViews = { this->backgroundTexture_->getTexture().get() };
	params.samplerStates = { this->samplerState_.get() };

	this->fullscreenPass_.draw(params);
}
