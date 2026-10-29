#include "GuiSystemManager.hpp"

#include "../ConfigManager/ConfigManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/BlurrySubSkinRenderer.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include "DataManager.hpp"
#include "FontManager.hpp"
#include "ResolutionManager.hpp"

#include "RenderPlatform/RPRenderManager.hpp"

#include <rend/Common/Format.hpp>
#include <rend/D3D11Device.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Resources/Texture2D.hpp>

#include <UTILS/Profiler.hpp>

#include <MyGui/MyGUI_Align.h>
#include <MyGui/MyGUI_Exception.h>
#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_LayoutManager.h>
#include <MyGui/MyGUI_LogManager.h>
#include <MyGui/MyGUI_LogSource.h>
#include <MyGui/MyGUI_ResourceManager.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_Widget.h>

#include <DirectXColors.h>
#include <d3d11.h>

#include <array>
#include <cstdint>
#include <string>

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

bool GuiSystemManager::initialize() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing GuiSystemManager...");
	PROFILE_FUNCTION();

	// Initialize Logging

	this->logManager_ = new MyGUI::LogManager;
	this->logManager_->setSTDOutputEnabled(false);

	this->myGuiLogSource_ = new MyGUI::LogSource;
	this->myGuiLogSource_->addLogListener(&this->myGuiLogListener_);
	this->myGuiLogSource_->open();

	this->logManager_->addLogSource(this->myGuiLogSource_);

	// Initialize Data Manager

	this->dataManager_ = new DataManager;

	// Initialize Render Manager

	this->renderManager_ = new RPRenderManager;
	try {
		if (!this->renderManager_->initialise()) {
			CONSOLE_ERROR("Failed to initialize MyGUI RPRenderManager.");
			return false;
		}
	} catch (const MyGUI::Exception& exception) {
		CONSOLE_ERROR("Failed to initialize MyGUI RPRenderManager. Exception message: %s", exception.getFullDescription().c_str());
		return false;
	}

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "Initialized MyGUI RPRenderManager.");

	// Create Render Target

	ConfigManager& configManager = ConfigManager::getInstance();
	int canvasWidth = configManager.getConfigVariableWithDefault("mygui.canvas.width", 1920);
	int canvasHeight = configManager.getConfigVariableWithDefault("mygui.canvas.height", 1080);

	if (!this->createRenderTarget(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_ERROR("Failed to create render target for MyGUI.");
		return false;
	}

	// Initialize Gui

	this->gui_ = new MyGUI::Gui;
	try {
		this->gui_->initialise("");
	} catch (const MyGUI::Exception& exception) {
		CONSOLE_ERROR("Failed to initialize MyGUI Gui. Error: %s", exception.getFullDescription().c_str());
		return false;
	}
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "Initialized MyGUI Gui.");

	// Register ResourceTrueTypeFont factory

	MyGUI::FactoryManager::getInstance().registerFactory<MyGUI::ResourceTrueTypeFont>("Resource", "ResourceTrueTypeFont");

	// Load MyGUI resource files

	static const std::array<std::string, 4> resourceFiles = {
		"$GAME_DATA/Gui/GuiConfig.xml",
		"$SURVIVAL_DATA/Gui/GuiConfigSurvival.xml",
		"$CHALLENGE_DATA/Gui/GuiConfigChallenge.xml",
		//"$CUSTOMIZATION_DATA/Gui/GuiConfigCustomization.xml",

		"$GAME_DATA/Gui/GuiSettings.xml"
	};

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	for (const auto& resourceFile : resourceFiles) {
		try {
			if(!resourceManager.load(resourceFile)) {
				CONSOLE_ERROR("Failed to load MyGUI resource file: %s", resourceFile.c_str());
				return false;
			}
		} catch (const MyGUI::Exception& exception) {
			CONSOLE_ERROR("Failed to load MyGUI resource file: %s. Exception message: %s", resourceFile.c_str(), exception.getFullDescription().c_str());
			return false;
		}

		CONSOLE_LOG(LEVEL_RENDER, COLOR_BLUE, "Loaded resource: %s", resourceFile.c_str());
	}

	// Initialize Font Manager

	this->myGuiFontManager_ = FontManager::createInstance();
	if (!this->myGuiFontManager_->loadFontXML("$GAME_DATA/Gui/Language/English/Fonts.xml")) {
		CONSOLE_ERROR("Failed to load MyGUI fonts from XML.");
		return false;
	}

	// Initialize Resolution Manager

	this->resolutionManager_ = ResolutionManager::createInstance();
	if (!this->resolutionManager_->initialize()) {
		CONSOLE_ERROR("Failed to initialize ResolutionManager.");
		return false;
	}

	// Initialize Blurring System

	this->guiBlurManager_ = GuiBlurManager::createInstance();
	if (!this->guiBlurManager_->initialize(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_ERROR("Failed to initialize GuiBlurManager.");
		return false;
	}

	this->blurrySubSkinRenderer_ = BlurrySubSkinRenderer::createInstance();
	if (!this->blurrySubSkinRenderer_->initialize()) {
		CONSOLE_ERROR("Failed to initialize BlurrySubSkinRenderer.");
		return false;
	}

	this->blurrySubSkinRegistry_ = BlurrySubSkinRegistry::createInstance();
	if (!this->blurrySubSkinRegistry_) {
		CONSOLE_ERROR("Failed to create BlurrySubSkinRegistry.");
		return false;
	}

	// Set Resolution

	if (!this->resolutionManager_->setResolution(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_ERROR("Failed to set resolution for MyGUI.");
		return false;
	}

	// Initialize Background Image Helper

	this->backgroundImageHelper_ = new BackgroundImageHelper;
	if (!this->backgroundImageHelper_->initialize()) {
		CONSOLE_ERROR("Failed to initialize BackgroundImageHelper.");
		return false;
	}

	this->backgroundImageHelper_->setBackgroundTexture("gui_loadingscreen_creative.png");

	return true;
}

void GuiSystemManager::cleanup() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Cleaning up GuiSystemManager...");


	if (this->gui_) {
		this->gui_->shutdown();
		delete this->gui_;
		this->gui_ = nullptr;
	}

	if (this->backgroundImageHelper_) {
		this->backgroundImageHelper_->cleanup();
		delete this->backgroundImageHelper_;
		this->backgroundImageHelper_ = nullptr;
	}

	if (this->blurrySubSkinRegistry_) {
		this->blurrySubSkinRegistry_->destroyInstance();
		this->blurrySubSkinRegistry_ = nullptr;
	}

	if (this->blurrySubSkinRenderer_) {
		this->blurrySubSkinRenderer_->destroyInstance();
		this->blurrySubSkinRenderer_ = nullptr;
	}

	if (this->guiBlurManager_) {
		this->guiBlurManager_->destroyInstance();
		this->guiBlurManager_ = nullptr;
	}

	if (this->resolutionManager_) {
		this->resolutionManager_->destroyInstance();
		this->resolutionManager_ = nullptr;
	}

	if (this->myGuiFontManager_) {
		this->myGuiFontManager_->destroyInstance();
		this->myGuiFontManager_ = nullptr;
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
