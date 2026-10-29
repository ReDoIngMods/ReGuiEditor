#include "RPRTTexture.hpp"

#include "RPRenderManager.hpp"
#include "RPTexture.hpp"

#include <rend/D3D11Device.hpp>
#include <rend/ResourceManager.hpp>

#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>

RPRTTexture::RPRTTexture(RPTexture* texture) : texture_(texture) {
	const float width = static_cast<float>(texture->getWidth());
	const float height = static_cast<float>(texture->getHeight());

	this->renderTargetInfo_.maximumDepth = 0.0f;
	this->renderTargetInfo_.hOffset = 0.0f;
	this->renderTargetInfo_.vOffset = 0.0f;
	this->renderTargetInfo_.aspectCoef = height / width;
	this->renderTargetInfo_.pixScaleX = 1.0f / width;
	this->renderTargetInfo_.pixScaleY = 1.0f / height;

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	this->renderView_ = resourceManager.createRenderView(texture->getTexture());
	if (this->renderView_)
		this->renderView_->setDebugName("MyGUI::RTTexture \"" + texture->getName() + "\" (RenderView)");
	else
		CONSOLE_ERROR("RPRTTexture: Failed to create render view for texture '%s'.", texture->getName().c_str());

	this->pipelineContext_ = resourceManager.createPipelineContext();
	if (this->pipelineContext_)
		this->pipelineContext_->setDebugName("MyGUI::RTTexture \"" + texture->getName() + "\" (PipelineContext)");
	else
		CONSOLE_ERROR("RPRTTexture: Failed to create pipeline context for texture '%s'.", texture->getName().c_str());
}

void RPRTTexture::begin() {
	if (!this->renderView_ || !this->pipelineContext_)
		return;

	if (!this->pipelineContext_->begin()) {
		CONSOLE_ERROR("RPRTTexture::begin: Failed to begin pipeline context.");
		return;
	}

	this->isActive_ = true;

	constexpr float transparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	rend::D3D11Device::getInstance().getDeviceContext1()->ClearRenderTargetView(this->renderView_->getRenderTargetView(), transparent);

	RPRenderManager::getInstance().setupContext(*this->pipelineContext_, this->renderView_, static_cast<float>(this->texture_->getWidth()), static_cast<float>(this->texture_->getHeight()));
}

void RPRTTexture::end() {
	if (!this->isActive_)
		return;

	this->isActive_ = false;

	if (!this->pipelineContext_->end())
		CONSOLE_ERROR("RPRTTexture::end: Failed to end pipeline context.");
}

void RPRTTexture::doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	if (this->isActive_)
		RPRenderManager::getInstance().drawBatch(*this->pipelineContext_, _buffer, _texture, _count);
}