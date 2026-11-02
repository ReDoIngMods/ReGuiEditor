#pragma once
#include <MyGui/MyGUI_FontData.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace FontCache {
	struct Glyph {
		MyGUI::Char codePoint = 0;
		uint32_t glyphIndex = 0;
		MyGUI::GlyphInfo info{};
	};

	// Fully parsed and decompressed cache file. Contains no MyGUI/GPU objects, so it can be produced on any thread.
	struct Data {
		uint32_t width = 0;
		uint32_t height = 0;

		std::string resourceName;
		std::string source;
		float size = 0.0f;
		uint32_t resolution = 0;
		bool antialias = false;
		float spaceWidth = 0.0f;
		float tabWidth = 0.0f;
		int offsetHeight = 0;
		uint32_t substituteCodePoint = 0;
		int defaultHeight = 0;

		std::vector<Glyph> glyphs;
		std::vector<char> pixels;
	};

	[[nodiscard]] uint64_t hashElement(MyGUI::xml::ElementPtr element);
	[[nodiscard]] std::string getFilepath(const std::string& fontName, uint64_t hash);

	// Thread-safe. Returns nullopt on a miss; outputErrorMsg is empty for a plain miss and holds a reason when the file was unusable.
	[[nodiscard]] std::optional<Data> read(const std::string& filePath, std::string& outputErrorMsg);

	// Main thread only. Builds the MyGUI resource + GPU texture and registers it.
	[[nodiscard]] MyGUI::ResourceTrueTypeFont* create(Data&& data, const std::string& fontName);

	bool load(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource);
	bool save(const std::string& filePath, const std::string& fontName, MyGUI::ResourceTrueTypeFont* fontResource);
}
