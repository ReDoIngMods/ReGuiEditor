#pragma once
#include "FontCache.hpp"

#include <Modules/Misc/Singleton.hpp>

#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <optional>
#include <string>
#include <vector>

class FontManager : public ReGuiEditor::Singleton<FontManager> {
public:
	// One font <Resource> element plus the result of its (threaded) cache read.
	struct Job {
		MyGUI::xml::ElementPtr element = nullptr;
		std::string fontName;
		std::string cacheFilePath;

		std::optional<FontCache::Data> cacheData;
		std::string cacheError;
	};
private:
	std::vector<MyGUI::ResourceTrueTypeFont*> loadedFonts_;
	std::string lastFontXMLPath_;

	void loadFont(Job& job, const std::string& xmlFilePath);
public:
	bool loadFontXML(const std::string& xmlFilePath);
	void clearFonts();
};
