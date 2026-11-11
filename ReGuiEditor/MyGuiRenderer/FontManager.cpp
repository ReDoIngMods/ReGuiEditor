#include "FontManager.hpp"

#include "FreeTypeFont.hpp"

#include "ResolutionManager.hpp"

#include "RenderPlatform/RPRenderManager.hpp"

#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_IResource.h>
#include <MyGui/MyGUI_ResourceManager.h>

#include <freetype/freetype.h>

#include <cmath>
#include <string>
#include <vector>

void FontManager::clearFonts() {
	std::vector<MyGUI::IResourcePtr> fontsToRemove;

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	MyGUI::ResourceManager::EnumeratorPtr enumerator = resourceManager.getEnumerator();
	while (enumerator.next()) {
		const auto& [name, resource] = enumerator.current();
		if (resource->getClassTypeName() == "FreeTypeFont")
			fontsToRemove.push_back(resource);
	}

	for (const auto& resourceName : fontsToRemove)
		resourceManager.removeResource(resourceName);
}

bool FontManager::initialize() {
	if (FT_Init_FreeType(&this->ftLibrary_) != 0) {
		CONSOLE_ERROR("FontManager: Failed to initialize FreeType library.");
		return false;
	}

	int major, minor, patch;
	FT_Library_Version(this->ftLibrary_, &major, &minor, &patch);

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GREEN, "FontManager: FreeType library initialized (version %d.%d.%d).", major, minor, patch);

	MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();
	factoryManager.registerFactory<FreeTypeFont>("Resource", "ResourceTrueTypeFont");

	RPRenderManager& renderManager = RPRenderManager::getInstance();
	renderManager.registerShader("FontShader", "$RGE_DATA/Shaders/gui_mygui_font.hlsl", "$RGE_DATA/Shaders/gui_mygui_font.hlsl");

	return true;
}

void FontManager::cleanup() {
	this->clearFonts();

	if (this->ftLibrary_) {
		FT_Done_FreeType(this->ftLibrary_);
		this->ftLibrary_ = nullptr;
	}
}

bool FontManager::loadFontXML(const std::string& xmlFilePath) {
	if (this->lastFontXMLPath_ == xmlFilePath)
		return true;
	
	this->clearFonts();
	this->lastFontXMLPath_ = xmlFilePath;

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	if (!resourceManager.load(xmlFilePath)) {
		CONSOLE_ERROR("Failed to load MyGUI font XML file: %s", xmlFilePath.c_str());
		return false;
	}

	return true;
}

float FontManager::getScaleFactor() {
	ResolutionManager& resolutionManager = ResolutionManager::getInstance();
	return pow(resolutionManager.getResolutionWidth() / 720.0f, 0.75f);
}