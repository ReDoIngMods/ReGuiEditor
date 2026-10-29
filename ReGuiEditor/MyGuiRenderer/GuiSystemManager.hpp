#pragma once
#include <UTILS/Singleton.hpp>

#include "LanguageManager.hpp"
#include "ResolutionManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "DataManager.hpp"
#include "FontManager.hpp"
#include "LogListener.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/BlurrySubSkinRenderer.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include "RenderPlatform/RPRenderManager.hpp"

#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/RenderView.hpp>
#include <rend/Resources/ShaderResourceView.hpp>

#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_LogManager.h>
#include <MyGui/MyGUI_LogSource.h>

#include <cstdint>
#include <memory>
#include <string>

class GuiSystemManager : public UTILS::Singleton<GuiSystemManager> {
	MyGUI::LogManager* logManager_ = nullptr;
	MyGUI::LogSource* myGuiLogSource_ = nullptr;
	LogListener myGuiLogListener_;

	MyGUI::Gui* gui_ = nullptr;
	RPRenderManager* renderManager_ = nullptr;
	
	DataManager* dataManager_ = nullptr;
	std::shared_ptr<FontManager> myGuiFontManager_ = nullptr;
	std::shared_ptr<LanguageManager> languageManager_ = nullptr;

	std::shared_ptr<ResolutionManager> resolutionManager_ = nullptr;

	std::shared_ptr<GuiBlurManager> guiBlurManager_ = nullptr;
	std::shared_ptr<BlurrySubSkinRegistry> blurrySubSkinRegistry_ = nullptr;
	std::shared_ptr<BlurrySubSkinRenderer> blurrySubSkinRenderer_ = nullptr;

	uint32_t renderTargetWidth_ = 0;
	uint32_t renderTargetHeight_ = 0;

	rend::RenderViewHandle renderView_;

	BackgroundImageHelper* backgroundImageHelper_ = nullptr;

	bool createRenderTarget(uint32_t width, uint32_t height);
	void cleanupRenderTarget();

	void onLanguageChanged(const std::string& language);

	void initializeLogging();
	void initializeDataManager();

	[[nodiscard]] bool initializeRenderManager();
	[[nodiscard]] bool initializeRenderTarget(uint32_t& canvasWidth, uint32_t& canvasHeight);
	[[nodiscard]] bool initializeGui();
	[[nodiscard]] bool loadResourceFiles();
	[[nodiscard]] bool initializeFonts();
	[[nodiscard]] bool initializeLanguages();
	[[nodiscard]] bool initializeResolution(uint32_t canvasWidth, uint32_t canvasHeight);
	[[nodiscard]] bool initializeBlurring(uint32_t canvasWidth, uint32_t canvasHeight);
	[[nodiscard]] bool initializeBackground();
public:
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

	const rend::ShaderResourceView* getRenderShaderResourceView() const { return this->renderView_.get(); }
	BackgroundImageHelper* getBackgroundImageHelper() const { return this->backgroundImageHelper_; }
};