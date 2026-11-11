#pragma once
#include <UTILS/Singleton.hpp>

#include <freetype/freetype.h>

#include <string>

class FontManager : public UTILS::Singleton<FontManager> {
	std::string lastFontXMLPath_;

	void clearFonts();

	FT_Library ftLibrary_ = nullptr;
public:
	FontManager() = default;
	~FontManager() = default;

	bool initialize();
	void cleanup();

	bool loadFontXML(const std::string& xmlFilePath);

	FT_Library& getFTLibrary() { return this->ftLibrary_; }

	static float getScaleFactor();
};
