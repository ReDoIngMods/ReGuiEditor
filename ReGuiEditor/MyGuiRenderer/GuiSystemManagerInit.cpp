#include "GuiSystemManager.hpp"

#include "../ConfigManager/ConfigManager.hpp"

#include "BackgroundImageHelper.hpp"

#include "BlurrySubSkin/BlurrySubSkinRegistry.hpp"
#include "BlurrySubSkin/BlurrySubSkinRenderer.hpp"
#include "BlurrySubSkin/GuiBlurManager.hpp"

#include "DataManager.hpp"
#include "FontManager.hpp"
#include "JsonGuiInstance.hpp"

#include "LanguageManager.hpp"
#include "ResolutionManager.hpp"

#include "RenderPlatform/RPRenderManager.hpp"

#include <UTILS/Profiler.hpp>

#include <MyGui/MyGUI_Exception.h>
#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_LogManager.h>
#include <MyGui/MyGUI_LogSource.h>
#include <MyGui/MyGUI_ResourceManager.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>

#include <array>
#include <cstdint>
#include <string>

constexpr int DEFAULT_CANVAS_WIDTH = 1920;
constexpr int DEFAULT_CANVAS_HEIGHT = 1080;

constexpr auto SEARCH_DIRECTORIES = std::to_array<const char*>({
	"$GAME_DATA/Gui",
	"$GAME_DATA/Gui/Editor",
	"$GAME_DATA/Gui/AssetIcons",
	"$GAME_DATA/Gui/Fonts",
	"$GAME_DATA/Gui/MyGUI_Media",
	"$GAME_DATA/Gui/Beacon",
	"$GAME_DATA/Gui/EnemyMarkers",
	"$GAME_DATA/Gui/Handbook",
	"$GAME_DATA/Gui/LostItem",
	"$GAME_DATA/Gui/QuestMarker",
	"$GAME_DATA/Gui/QuestPins",
	"$GAME_DATA/Gui/Waypoint"
});

constexpr auto RECURSIVE_SEARCH_DIRECTORIES = std::to_array<const char*>({
	"$CHALLENGE_DATA/Gui",
	"$SURVIVAL_DATA/Gui"
});

constexpr auto RESOURCE_FILES = std::to_array<const char*>({
	"$GAME_DATA/Gui/ScrapMekSkin.xml",
	"$GAME_DATA/Gui/ScrapMekTemplate.xml",
	"$GAME_DATA/Gui/IconMap.xml",
	"$GAME_DATA/Gui/ToolIconMap.xml",
	"$GAME_DATA/Gui/Rotations.xml",

	"$GAME_DATA/Gui/MyGUI_Media/MyGUI_CommonSkins.xml",
	"$GAME_DATA/Gui/MyGUI_Media/MyGUI_BlackOrangeTheme.xml",

	"$GAME_DATA/Gui/MyGUI_Media/MyGUI_Layers.xml",
	"$GAME_DATA/Gui/GuiSettings.xml",
	"$GAME_DATA/Gui/EditorSkin.xml",
	"$GAME_DATA/Gui/EditorTemplates.xml",
	"$GAME_DATA/Gui/PolygonalSkin.xml",

	"IconMapChallenge.xml",
	"IconMapSurvival.xml"
});

void GuiSystemManager::initializeLogging() {
	PROFILE_SCOPE("GuiSystemManager::initialize - Logging");

	this->logManager_ = new MyGUI::LogManager;
	this->logManager_->setSTDOutputEnabled(false);

	this->myGuiLogSource_ = new MyGUI::LogSource;
	this->myGuiLogSource_->addLogListener(&this->myGuiLogListener_);
	this->myGuiLogSource_->open();

	this->logManager_->addLogSource(this->myGuiLogSource_);
}

void GuiSystemManager::initializeDataManager() {
	PROFILE_SCOPE("GuiSystemManager::initialize - DataManager");

	this->dataManager_ = new DataManager;

	for (const char* directory : SEARCH_DIRECTORIES)
		this->dataManager_->addSearchDirectory(directory);

	for (const char* directory : RECURSIVE_SEARCH_DIRECTORIES)
		this->dataManager_->addSearchDirectory(directory, true);
}

bool GuiSystemManager::initializeRenderManager() {
	PROFILE_SCOPE("GuiSystemManager::initialize - RenderManager");

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
	return true;
}

bool GuiSystemManager::initializeRenderTarget(uint32_t& canvasWidth, uint32_t& canvasHeight) {
	PROFILE_SCOPE("GuiSystemManager::initialize - CreateRenderTarget");

	ConfigManager& configManager = ConfigManager::getInstance();
	canvasWidth = static_cast<uint32_t>(configManager.getConfigVariableWithDefault("mygui.canvas.width", DEFAULT_CANVAS_WIDTH));
	canvasHeight = static_cast<uint32_t>(configManager.getConfigVariableWithDefault("mygui.canvas.height", DEFAULT_CANVAS_HEIGHT));

	if (!this->createRenderTarget(canvasWidth, canvasHeight)) {
		CONSOLE_ERROR("Failed to create render target for MyGUI.");
		return false;
	}

	return true;
}

bool GuiSystemManager::initializeGui() {
	PROFILE_SCOPE("GuiSystemManager::initialize - Gui");

	this->gui_ = new MyGUI::Gui;
	try {
		this->gui_->initialise("");
	} catch (const MyGUI::Exception& exception) {
		CONSOLE_ERROR("Failed to initialize MyGUI Gui. Error: %s", exception.getFullDescription().c_str());
		return false;
	}

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "Initialized MyGUI Gui.");

	{
		PROFILE_SCOPE("GuiSystemManager::initialize - Register TTF factory");

		MyGUI::FactoryManager::getInstance().registerFactory<MyGUI::ResourceTrueTypeFont>("Resource", "ResourceTrueTypeFont");
	}

	JsonGuiInstance::registerWidgetFactories();
	JsonGuiInstance::registerOverlayShader();

	return true;
}

bool GuiSystemManager::loadResourceFiles() {
	PROFILE_SCOPE("GuiSystemManager::initialize - Load additional config files");

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Loading additional config files files...");

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	for (const char* resourceFile : RESOURCE_FILES) {
		try {
			if (!resourceManager.load(resourceFile)) {
				CONSOLE_ERROR("Failed to load MyGUI resource file: %s", resourceFile);
				return false;
			}
		} catch (const MyGUI::Exception& exception) {
			CONSOLE_ERROR("Failed to load MyGUI resource file: %s. Exception message: %s", resourceFile, exception.getFullDescription().c_str());
			return false;
		}

		CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "Loaded MyGUI resource file: %s", resourceFile);
	}

	return true;
}

bool GuiSystemManager::initializeFonts() {
	PROFILE_SCOPE("GuiSystemManager::initialize - FontManager");

	this->myGuiFontManager_ = FontManager::createInstance();
	if (!this->myGuiFontManager_->loadFontXML("$GAME_DATA/Gui/Language/English/Fonts.xml")) {
		CONSOLE_ERROR("Failed to load MyGUI fonts from XML.");
		return false;
	}

	return true;
}

bool GuiSystemManager::initializeLanguages() {
	PROFILE_SCOPE("GuiSystemManager::initialize - LanguageManager");

	this->languageManager_ = LanguageManager::createInstance();
	if (!this->languageManager_->initialize()) {
		CONSOLE_ERROR("Failed to initialize LanguageManager.");
		return false;
	}

	return true;
}

bool GuiSystemManager::initializeResolution(uint32_t canvasWidth, uint32_t canvasHeight) {
	{
		PROFILE_SCOPE("GuiSystemManager::initialize - ResolutionManager");

		this->resolutionManager_ = ResolutionManager::createInstance();
		if (!this->resolutionManager_->initialize()) {
			CONSOLE_ERROR("Failed to initialize ResolutionManager.");
			return false;
		}
	}

	{
		PROFILE_SCOPE("GuiSystemManager::initialize - Set Resolution");

		if (!this->resolutionManager_->setResolution(canvasWidth, canvasHeight)) {
			CONSOLE_ERROR("Failed to set resolution for MyGUI.");
			return false;
		}
	}

	return true;
}

bool GuiSystemManager::initializeBlurring(uint32_t canvasWidth, uint32_t canvasHeight) {
	PROFILE_SCOPE("GuiSystemManager::initialize - Blurring System");

	this->guiBlurManager_ = GuiBlurManager::createInstance();
	if (!this->guiBlurManager_->initialize(canvasWidth, canvasHeight)) {
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

	return true;
}

bool GuiSystemManager::initializeBackground() {
	PROFILE_SCOPE("GuiSystemManager::initialize - BackgroundImageHelper");

	this->backgroundImageHelper_ = new BackgroundImageHelper;
	if (!this->backgroundImageHelper_->initialize()) {
		CONSOLE_ERROR("Failed to initialize BackgroundImageHelper.");
		return false;
	}

	this->backgroundImageHelper_->setBackgroundTexture("gui_loadingscreen_creative.png");
	return true;
}

bool GuiSystemManager::initialize() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing GuiSystemManager...");
	PROFILE_FUNCTION();

	this->initializeLogging();
	this->initializeDataManager();

	if (!this->initializeRenderManager())
		return false;

	uint32_t canvasWidth = 0;
	uint32_t canvasHeight = 0;
	if (!this->initializeRenderTarget(canvasWidth, canvasHeight)) return false;
	if (!this->initializeGui()) return false;
	if (!this->initializeBlurring(canvasWidth, canvasHeight)) return false;
	if (!this->loadResourceFiles()) return false;
	if (!this->initializeFonts()) return false;
	if (!this->initializeLanguages()) return false;
	if (!this->initializeResolution(canvasWidth, canvasHeight)) return false;

	return this->initializeBackground();
}
