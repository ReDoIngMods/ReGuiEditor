#include "GuiBlurSkinTexture.hpp"

#include <GUI/MyGuiDX11/Texture.hpp>

#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>

#include <d3d11.h>

#include <string>

bool GuiBlurSkinTexture::initialize(const std::string& name, int width, int height) {
	this->name_ = name;

	MyGUI::ITexture* texture = MyGUI::RenderManager::getInstance().createTexture(name);
	if (!texture) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiBlurSkinTexture::initialize: Failed to create MyGUI texture '%s'.", name.c_str());
		return false;
	}

	this->texture_ = static_cast<MyGUICustom::DirectX11Texture*>(texture);
	this->texture_->createManual(width, height, MyGUI::TextureUsage::Default, MyGUI::PixelFormat::R8G8B8A8);

	return true;
}

void GuiBlurSkinTexture::cleanup() {
	if (this->texture_) {
		MyGUI::RenderManager::getInstance().destroyTexture(this->texture_);
		this->texture_ = nullptr;
	}
}

void GuiBlurSkinTexture::update(ID3D11Texture2D* texture, ID3D11ShaderResourceView* shaderResourceView) {
	if (!this->texture_)
		return;

	if (this->texture_->texture) this->texture_->texture->Release();
	if (this->texture_->shaderResourceView) this->texture_->shaderResourceView->Release();

	texture->AddRef();
	shaderResourceView->AddRef();

	this->texture_->texture = texture;
	this->texture_->shaderResourceView = shaderResourceView;
}
