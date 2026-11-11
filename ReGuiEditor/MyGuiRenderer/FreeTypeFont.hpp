#pragma once
#include <UTILS/Hashing.hpp>

#include <MyGui/MyGUI_FontData.h>
#include <MyGui/MyGUI_IFont.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RTTI.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Version.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <freetype/freetype.h>
#include <freetype/fttypes.h>

#include <cstdint>
#include <map>
#include <string>

// FreeTypeFont is a heavly stripped down & modified version of MyGUI's ResourceTrueTypeFont implementation.

struct FreeTypeFontProperties {
	std::string source = "";
	float size = 12.0f;
	float letterSpacing = 0.0f;
	
	// Properties below are calculated at runtime and cannot be set directly.

	int defaultHeight = 0;
};

namespace std {
	template<>
	struct hash<FreeTypeFontProperties> {
		std::size_t operator()(const FreeTypeFontProperties& properties) const noexcept {
			std::size_t hash = 0;
			UTILS::hashCombine(hash, properties.source);
			UTILS::hashCombine(hash, properties.size);
			UTILS::hashCombine(hash, properties.letterSpacing);

			return hash;
		}
	};
}

class FreeTypeFont : public MyGUI::IFont, public MyGUI::ITextureInvalidateListener {
	MYGUI_RTTI_DERIVED(FreeTypeFont);

	FreeTypeFontProperties properties_;
	MyGUI::ITexture* textureFont_ = nullptr;

	MyGUI::GlyphInfo fallbackGlyphInfo_;

	typedef std::map<MyGUI::Char, MyGUI::GlyphInfo> GlyphMap;
	typedef std::map<MyGUI::Char, size_t> CharacterMap;
	typedef std::map<int, std::map<FT_UInt, MyGUI::GlyphInfo*>> GlyphHeightMap;

	CharacterMap characterMap_;
	GlyphMap glyphMap_;

	FT_Face loadFontFace(const FT_Library& ftLibrary, uint8_t*& outputFontBuffer);
	void autoWrapGlyphPosition(int glyphWidth, int textureWidth, int lineHeight, int& outputTextureX, int& outputTextureY);
	MyGUI::GlyphInfo createFaceGlyphInfo(MyGUI::Char codepoint, int fontAscent, FT_GlyphSlot glyph) const;

	int createGlyph(FT_UInt glyphIndex, const MyGUI::GlyphInfo& glyphInfo, GlyphHeightMap& glyphHeightMap);
	int createFaceGlyph(FT_UInt glyphIndex, MyGUI::Char codepoint, int fontascent, const FT_Face& ftFace, GlyphHeightMap& glyphHeightMap);

	void renderGlyphs(const GlyphHeightMap& glyphHeightMap, const FT_Face& ftFace, uint8_t* textureBuffer, int textureWidth, int textureHeight);
	void renderGlyph(MyGUI::GlyphInfo& glyphInfo, uint8_t alpha, int lineHeight, uint8_t* textureBuffer, int textureWidth, int textureHeight, int& outputTextureX, int& outputTextureY, uint8_t* glyphBuffer = nullptr);

	void initialise();
public:
	~FreeTypeFont() override;

	void deserialization(MyGUI::xml::ElementPtr node, MyGUI::Version version) override;

	const MyGUI::GlyphInfo* getGlyphInfo(MyGUI::Char character) const override;
	MyGUI::ITexture* getTextureFont() const override;
	int getDefaultHeight() const override;

	void textureInvalidate(MyGUI::ITexture* _texture) override;

	const FreeTypeFontProperties& getProperties() const { return this->properties_; }

	void addCodePoint(MyGUI::Char codepoint);
	void removeCodePoint(MyGUI::Char codepoint);
	void clearCodePoints();

	void addCodePointRnage(MyGUI::Char start, MyGUI::Char end);
	void removeCodePointRange(MyGUI::Char start, MyGUI::Char end);
};