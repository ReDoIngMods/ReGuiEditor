#pragma once
#include <UTILS/Singleton.hpp>

#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <string>
#include <vector>

class FontManager : public UTILS::Singleton<FontManager> {
	std::vector<MyGUI::ResourceTrueTypeFont*> loadedFonts_;
	std::string lastFontXMLPath_;

	void loadFont(MyGUI::xml::ElementPtr element, const std::string& xmlFilePath);
public:
	bool loadFontXML(const std::string& xmlFilePath);
	void clearFonts();
};
