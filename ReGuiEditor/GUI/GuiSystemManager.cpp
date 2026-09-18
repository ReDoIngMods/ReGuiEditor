#include "GuiSystemManager.hpp"

#include "../Application.hpp"
#include "../ConfigManager/ConfigManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/BlurrySubSkinRenderer.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include "MyGui/MyGuiDataManager.hpp"
#include "MyGui/MyGuiFontManager.hpp"

#include "ResolutionManager.hpp"
#include "ShaderManager.hpp"

#include <GUI/MyGuiDX11/RenderManager.hpp>

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

#include <Windows.h>

#include <array>
#include <cstdint>

#include "NotificationManager.hpp"
#include <DirectXColors.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>
#include <string>

bool GuiSystemManager::createRenderTarget(uint32_t width, uint32_t height) {
	this->cleanupRenderTarget();

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	Application& app = Application::getInstance();
	ID3D11Device* device = app.d3d11Device.getDevice();

	HRESULT result = device->CreateTexture2D(&desc, nullptr, &this->renderTexture);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiSystemManager::createRenderTarget: Failed to create RTV texture. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = desc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	result = device->CreateRenderTargetView(this->renderTexture, &rtvDesc, &this->renderTargetView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiSystemManager::createRenderTarget: Failed to create RTV. HRESULT: 0x%X", result);

		this->renderTexture->Release();
		this->renderTexture = nullptr;

		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	result = device->CreateShaderResourceView(this->renderTexture, &srvDesc, &this->renderShaderResourceView);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiSystemManager::createRenderTarget: Failed to create SRV. HRESULT: 0x%X", result);
		
		this->renderTargetView->Release();
		this->renderTargetView = nullptr;

		this->renderTexture->Release();
		this->renderTexture = nullptr;
		
		return false;
	}

	this->renderTargetWidth_ = width;
	this->renderTargetHeight_ = height;

	this->renderManager_->setViewSize(static_cast<int>(width), static_cast<int>(height));

	return true;
}

void GuiSystemManager::cleanupRenderTarget() {
	if (this->renderShaderResourceView) {
		this->renderShaderResourceView->Release();
		this->renderShaderResourceView = nullptr;
	}

	if (this->renderTargetView) {
		this->renderTargetView->Release();
		this->renderTargetView = nullptr;
	}

	if (this->renderTexture) {
		this->renderTexture->Release();
		this->renderTexture = nullptr;
	}

	this->renderTargetWidth_ = 0;
	this->renderTargetHeight_ = 0;
}

bool GuiSystemManager::initialize() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing GuiSystemManager...");

	// Initialize Shader Manager

	this->shaderManager_ = ShaderManager::createInstance();

	// Initialize Logging

	this->logManager_ = new MyGUI::LogManager;
	this->logManager_->setSTDOutputEnabled(false);

	this->myGuiLogSource_ = new MyGUI::LogSource;
	this->myGuiLogSource_->addLogListener(&this->myGuiLogListener_);
	this->myGuiLogSource_->open();

	this->logManager_->addLogSource(this->myGuiLogSource_);

	// Initialize Data Manager

	this->dataManager_ = new MyGuiDataManager;

	// Initialize Render Manager

	this->renderManager_ = new MyGUICustom::DirectX11RenderManager;
	try {
		if(!this->renderManager_->initialise(Application::getInstance().d3d11Device.getDevice())) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize MyGUI DirectX11RenderManager.");
			return false;
		}
	} catch (const MyGUI::Exception& exception) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize MyGUI DirectX11RenderManager. Exception message: %s", exception.getFullDescription().c_str());
		return false;
	}

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "Initialized MyGUI DirectX11RenderManager.");

	// Create Render Target

	ConfigManager& configManager = ConfigManager::getInstance();
	int canvasWidth = configManager.getConfigVariableWithDefault("mygui.canvas.width", 1920);
	int canvasHeight = configManager.getConfigVariableWithDefault("mygui.canvas.height", 1080);

	if (!this->createRenderTarget(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create render target for MyGUI.");
		return false;
	}

	// Initialize Gui

	this->gui_ = new MyGUI::Gui;
	try {
		this->gui_->initialise("");
	} catch (const MyGUI::Exception& exception) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize MyGUI Gui. Error: %s", exception.getFullDescription().c_str());
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
		if (!resourceManager.load(resourceFile)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load MyGUI resource file: %s", resourceFile.c_str());
			return false;
		}

		CONSOLE_LOG(LEVEL_RENDER, COLOR_BLUE, "Loaded resource: %s", resourceFile.c_str());
	}

	// Initialize Font Manager

	this->myGuiFontManager_ = MyGuiFontManager::createInstance();
	if (!this->myGuiFontManager_->loadFontXML("$GAME_DATA/Gui/Language/English/Fonts.xml")) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load MyGUI fonts from XML.");
		return false;
	}

	// Initialize Resolution Manager

	this->resolutionManager_ = ResolutionManager::createInstance();
	if (!this->resolutionManager_->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize ResolutionManager.");
		return false;
	}

	// Initialize Blurring System

	this->guiBlurManager_ = GuiBlurManager::createInstance();
	if (!this->guiBlurManager_->initialize(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize GuiBlurManager.");
		return false;
	}

	this->blurrySubSkinRenderer_ = BlurrySubSkinRenderer::createInstance();
	if (!this->blurrySubSkinRenderer_->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize BlurrySubSkinRenderer.");
		return false;
	}

	this->blurrySubSkinRegistry_ = BlurrySubSkinRegistry::createInstance();
	if (!this->blurrySubSkinRegistry_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create BlurrySubSkinRegistry.");
		return false;
	}

	// Set Resolution

	if (!this->resolutionManager_->setResolution(static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight))) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to set resolution for MyGUI.");
		return false;
	}

	// Initialize Background Image Helper

	this->backgroundImageHelper_ = new BackgroundImageHelper;
	if (!this->backgroundImageHelper_->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize BackgroundImageHelper.");
		return false;
	}

	this->backgroundImageHelper_->setBackgroundTexture("gui_loadingscreen_creative.png");

	// Notification Manager

	this->notificationManager_ = NotificationManager::createInstance();

	// Debug shit

	MyGUI::Widget* rootWidget = this->gui_->createWidgetReal<MyGUI::Widget>("PanelEmpty", 0.f, 0.f, 1.f, 1.f, MyGUI::Align::Default, "Main");
	MyGUI::LayoutManager::getInstance().loadLayout("$CONTENT_3660881a-a6b8-40e5-a348-27b368a742e9/Gui/Layout/Computer/Computer.layout", "", rootWidget);

	return true;
}

void GuiSystemManager::cleanup() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Cleaning up GuiSystemManager...");

	if (this->notificationManager_) {
		this->notificationManager_->destroyInstance();
		this->notificationManager_ = nullptr;
	}

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

	if (this->shaderManager_) {
		this->shaderManager_->destroyInstance();
		this->shaderManager_ = nullptr;
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
	if (width == this->renderTargetWidth_ && height == this->renderTargetHeight_)
		return true;
	
	this->cleanupRenderTarget();

	if (!this->createRenderTarget(width, height)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to change resolution.");
		return false;
	}

	if (!this->resolutionManager_->setResolution(width, height)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to set resolution.");
		return false;
	}

	if (!this->guiBlurManager_->resize(width, height)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resize GuiBlurManager.");
		return false;
	}

	return true;
}

void GuiSystemManager::render() {
	if (!this->renderTargetView) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "GuiSystemManager::render: Render target view is not created.");
		return;
	}

	ID3D11DeviceContext* context = Application::getInstance().d3d11Device.getContext();
	context->ClearRenderTargetView(this->renderTargetView, DirectX::Colors::Aquamarine);
	context->OMSetRenderTargets(1, &this->renderTargetView, nullptr);

	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(viewport));
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<FLOAT>(this->renderTargetWidth_);
	viewport.Height = static_cast<FLOAT>(this->renderTargetHeight_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	context->RSSetViewports(1, &viewport);

	this->backgroundImageHelper_->render();

	this->guiBlurManager_->captureAndBlur(this->renderShaderResourceView);
	this->blurrySubSkinRegistry_->updateAllInstances(this->guiBlurManager_->getResultShaderResourceView(), this->renderTargetWidth_, this->renderTargetHeight_);

	context->RSSetViewports(1, &viewport);
	context->OMSetRenderTargets(1, &this->renderTargetView, nullptr);
	this->renderManager_->drawOneFrame();
}