#include "GuiBlurSkinTexture.hpp"

#include <GUI/MyGuiDX11/MGTexture.hpp>

#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>

#include <rend/ResourceManager.hpp>

#include <UTILS/Console.hpp>

#include <string>

bool GuiBlurSkinTexture::initialize(const std::string& name, int width, int height) {
	this->name_ = name;

	MyGUI::ITexture* texture = MyGUI::RenderManager::getInstance().createTexture(name);
	if (!texture) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurSkinTexture::initialize: Failed to create MyGUI texture '%s'.", name.c_str());
		return false;
	}

	this->texture_ = static_cast<MyGUICustom::DirectX11Texture*>(texture);
	return this->resize(width, height);
}

void GuiBlurSkinTexture::cleanup() {
	this->renderView_.reset();

	if (this->texture_) {
		MyGUI::RenderManager::getInstance().destroyTexture(this->texture_);
		this->texture_ = nullptr;
	}
}

bool GuiBlurSkinTexture::resize(int width, int height) {
	if (!this->texture_)
		return false;

	this->renderView_.reset();
	this->texture_->createManual(width, height, MyGUI::TextureUsage::RenderTarget, MyGUI::PixelFormat::R8G8B8A8);

	if (!this->texture_->getTexture()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurSkinTexture::resize: Failed to create %dx%d texture '%s'.", width, height, this->name_.c_str());
		return false;
	}

	this->renderView_ = rend::ResourceManager::getInstance().createRenderView(this->texture_->getTexture());
	if (!this->renderView_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurSkinTexture::resize: Failed to create render view for '%s'.", this->name_.c_str());
		return false;
	}

	this->renderView_->setDebugName("BlurrySubSkin composite \"" + this->name_ + "\" (RenderView)");

	return true;
}
