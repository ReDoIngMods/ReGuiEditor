#pragma once
#include <UTILS/Singleton.hpp>

#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <cstdint>
#include <string>
#include <vector>

class FontManager : public UTILS::Singleton<FontManager> {
	std::vector<MyGUI::ResourceTrueTypeFont*> loadedFonts_;

	void hashCombineString(uint64_t& hash, const std::string& str);
	void processHashXMLElement(MyGUI::xml::ElementPtr element, uint64_t& hash);
	uint64_t generateHashFromXMLElement(MyGUI::xml::ElementPtr element);

	std::string getCacheFilepath(const std::string& fontName, uint64_t hash);

	bool loadFromCache(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource);
	bool generateCacheFile(const std::string& filePath, const std::string& fontName, MyGUI::ResourceTrueTypeFont* fontResource);
public:
	bool loadFontXML(const std::string& xmlFilePath);
	void clearFonts();
};