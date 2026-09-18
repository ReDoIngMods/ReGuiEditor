#pragma once
#include <UTILS/Singleton.hpp>

#include "ResolutionManager.hpp"
#include "ShaderManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "MyGui/MyGuiDataManager.hpp"
#include "MyGui/MyGuiFontManager.hpp"
#include "MyGui/MyGuiLogListener.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/BlurrySubSkinRenderer.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include <GUI/MyGuiDX11/RenderManager.hpp>

#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_LogManager.h>
#include <MyGui/MyGUI_LogSource.h>

#include <d3d11.h>

#include <cstdint>

class GuiSystemManager : public UTILS::Singleton<GuiSystemManager> {
	MyGUI::LogManager* logManager_ = nullptr;
	MyGUI::LogSource* myGuiLogSource_ = nullptr;
	MyGuiLogListener myGuiLogListener_;

	MyGUI::Gui* gui_ = nullptr;
	MyGUICustom::DirectX11RenderManager* renderManager_ = nullptr;
	
	MyGuiDataManager* dataManager_ = nullptr;
	MyGuiFontManager* myGuiFontManager_ = nullptr;

	ResolutionManager* resolutionManager_ = nullptr;
	ShaderManager* shaderManager_ = nullptr;

	GuiBlurManager* guiBlurManager_ = nullptr;
	BlurrySubSkinRegistry* blurrySubSkinRegistry_ = nullptr;
	BlurrySubSkinRenderer* blurrySubSkinRenderer_ = nullptr;

	uint32_t renderTargetWidth_ = 0;
	uint32_t renderTargetHeight_ = 0;

	BackgroundImageHelper* backgroundImageHelper_ = nullptr;

	bool createRenderTarget(uint32_t width, uint32_t height);
	void cleanupRenderTarget();
public:
	ID3D11Texture2D* renderTexture = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;
	ID3D11ShaderResourceView* renderShaderResourceView = nullptr;

	GuiSystemManager() = default;
	~GuiSystemManager() { this->cleanup(); }

	GuiSystemManager(const GuiSystemManager&) = delete;
	GuiSystemManager& operator=(const GuiSystemManager&) = delete;
	GuiSystemManager(GuiSystemManager&&) = delete;
	GuiSystemManager& operator=(GuiSystemManager&&) = delete;

	bool initialize();
	void cleanup();

	void render();

	bool changeResolution(uint32_t width, uint32_t height);

	uint32_t getRenderTargetWidth() const { return this->renderTargetWidth_; }
	uint32_t getRenderTargetHeight() const { return this->renderTargetHeight_; }
	GuiBlurManager* getGuiBlurManager() const { return this->guiBlurManager_; }
	BackgroundImageHelper* getBackgroundImageHelper() const { return this->backgroundImageHelper_; }
};