#pragma once
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <cstdint>
#include <string>

namespace FontCache {
	[[nodiscard]] uint64_t hashElement(MyGUI::xml::ElementPtr element);
	[[nodiscard]] std::string getFilepath(const std::string& fontName, uint64_t hash);

	bool load(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource);
	bool save(const std::string& filePath, const std::string& fontName, MyGUI::ResourceTrueTypeFont* fontResource);
}
