#include "GuiSystemManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include "FontManager.hpp"

#include "ResolutionManager.hpp"

#include "RenderPlatform/RPRenderManager.hpp"

#include <rend/Common/Format.hpp>
#include <rend/D3D11Device.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Resources/Texture2D.hpp>

#include <Modules/Misc/Profiler.hpp>
#include <Modules/Misc/Strings.hpp>

#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_LanguageManager.h>

#include <DirectXColors.h>
#include <d3d11.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

bool GuiSystemManager::createRenderTarget(uint32_t width, uint32_t height) {
	this->cleanupRenderTarget();

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	rend::Texture2DHandle texture = resourceManager.createTexture2D(width, height, rend::Format::R8G8B8A8_UNORM, rend::Texture2DFlags::ShaderResource | rend::Texture2DFlags::RenderTarget);
	if (!texture) {
		CONSOLE_ERROR("GuiSystemManager::createRenderTarget: Failed to create render target texture.");
		return false;
	}
	texture->setDebugName("GuiSystemManager::RenderTarget (Texture)");

	this->renderView_ = resourceManager.createRenderView(texture);
	if (!this->renderView_) {
		CONSOLE_ERROR("GuiSystemManager::createRenderTarget: Failed to create render view.");
		return false;
	}
	this->renderView_->setDebugName("GuiSystemManager::RenderTarget (RenderView)");

	this->renderTargetWidth_ = width;
	this->renderTargetHeight_ = height;

	this->renderManager_->setRenderView(this->renderView_);
	this->renderManager_->setViewSize(static_cast<int>(width), static_cast<int>(height));

	return true;
}

void GuiSystemManager::cleanupRenderTarget() {
	this->renderView_.reset();

	this->renderTargetWidth_ = 0;
	this->renderTargetHeight_ = 0;
}

void GuiSystemManager::onLanguageChanged(const std::string& language) {
	this->myGuiFontManager_->loadFontXML("$GAME_DATA/Gui/Language/" + language + "/Fonts.xml");

	MyGUI::LanguageManager* languageManager = MyGUI::LanguageManager::getInstancePtr();
	languageManager->clearUserTags();

	static constexpr std::array<std::string_view, 3> INTERFACE_TAG_FILES = {
		"InterfaceTags.txt",
		"LogItemInterfaceTags.txt",
		"QuestInterfaceTags.txt"
	};

	for (const std::string_view& tagFile : INTERFACE_TAG_FILES) {
		const std::string tagFilePath = ReGuiEditor::formatString("$GAME_DATA/Gui/Language/%s/%s", language.c_str(), tagFile.data());
		if (!languageManager->loadUserTags(tagFilePath))
			CONSOLE_WARNING("Failed to load user tags from file: %s", tagFilePath.c_str());
	}
}

void GuiSystemManager::cleanup() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Cleaning up GuiSystemManager...");

	if (this->gui_) {
		this->gui_->shutdown();
		delete this->gui_;
		this->gui_ = nullptr;
	}

	if (this->backgroundImageHelper_) {
		delete this->backgroundImageHelper_;
		this->backgroundImageHelper_ = nullptr;
	}

	this->cleanupRenderTarget();

	if (this->renderManager_) {
		this->renderManager_->shutdown();
		delete this->renderManager_;
		this->renderManager_ = nullptr;
	}

	if (this->dataManager_) {
		delete this->dataManager_;
		this->dataManager_ = nullptr;
	}

	if (this->logManager_) {
		delete this->logManager_;
		this->logManager_ = nullptr;
	}

	if (this->myGuiLogSource_) {
		delete this->myGuiLogSource_;
		this->myGuiLogSource_ = nullptr;
	}
}

bool GuiSystemManager::changeResolution(uint32_t width, uint32_t height) {
	PROFILE_FUNCTION();

	if (width == this->renderTargetWidth_ && height == this->renderTargetHeight_)
		return true;

	this->cleanupRenderTarget();

	if (!this->createRenderTarget(width, height)) {
		CONSOLE_ERROR("Failed to change resolution.");
		return false;
	}

	if (!this->resolutionManager_->setResolution(width, height)) {
		CONSOLE_ERROR("Failed to set resolution.");
		return false;
	}

	if (!this->guiBlurManager_->resize(width, height)) {
		CONSOLE_ERROR("Failed to resize GuiBlurManager.");
		return false;
	}

	return true;
}

void GuiSystemManager::render() {
	PROFILE_FUNCTION();

	if (!this->renderView_) {
		CONSOLE_ERROR("GuiSystemManager::render: Render view is not created.");
		return;
	}

	rend::D3D11Device::getInstance().getDeviceContext1()->ClearRenderTargetView(this->renderView_->getRenderTargetView(), DirectX::Colors::Aquamarine);

	{
		PROFILE_SCOPE("GuiSystemManager::background");
		this->backgroundImageHelper_->render(this->renderView_, static_cast<float>(this->renderTargetWidth_), static_cast<float>(this->renderTargetHeight_));
	}

	{
		PROFILE_SCOPE("GuiSystemManager::blur");
		this->guiBlurManager_->captureAndBlur(this->renderView_);
		this->blurrySubSkinRegistry_->updateAllInstances(this->guiBlurManager_->getResultShaderResourceView(), static_cast<int>(this->renderTargetWidth_), static_cast<int>(this->renderTargetHeight_));
	}

	{
		PROFILE_SCOPE("GuiSystemManager::drawOneFrame");
		this->renderManager_->drawOneFrame();
	}
}
