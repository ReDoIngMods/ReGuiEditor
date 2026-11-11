#include "FreeTypeFont.hpp"

#include "FontManager.hpp"

#include <Application.hpp>

#include <UTILS/Strings.hpp>

#include <MyGui/MyGUI_Bitwise.h>
#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_FontData.h>
#include <MyGui/MyGUI_IDataStream.h>
#include <MyGui/MyGUI_IFont.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_StringUtility.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Version.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <freetype/freetype.h>
#include <freetype/ftimage.h>
#include <freetype/fttypes.h>
#include <freetype/tttables.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

template<typename T>
inline static void setMaxValue(T& value, const T& newValue) {
	if (newValue > value)
		value = newValue;
}

static constexpr std::pair<const MyGUI::Char, const uint8_t> characterMaskData[] = {
	std::make_pair(MyGUI::FontCodeType::Selected,     static_cast<uint8_t>('\x88')),
	std::make_pair(MyGUI::FontCodeType::SelectedBack, static_cast<uint8_t>('\x60')),
	std::make_pair(MyGUI::FontCodeType::Cursor,       static_cast<uint8_t>('\xFF')),
	std::make_pair(MyGUI::FontCodeType::Tab,          static_cast<uint8_t>('\x00'))
};

const std::map<const MyGUI::Char, const uint8_t> charMask(characterMaskData, characterMaskData + sizeof(characterMaskData) / sizeof(*characterMaskData));

constexpr uint8_t charMaskBlack = static_cast<const uint8_t>('\x00');
constexpr uint8_t charMaskWhite = static_cast<const uint8_t>('\xFF');

static constexpr int   glyphSpacing = 1;
static constexpr float tabWidth = 8.0f;
static constexpr float selectedWidth = 1.0f;
static constexpr float cursorWidth = 2.0f;

static constexpr MyGUI::Char specialCodePoints[] = {
	static_cast<MyGUI::Char>(MyGUI::FontCodeType::Tab),
	static_cast<MyGUI::Char>(MyGUI::FontCodeType::Selected),
	static_cast<MyGUI::Char>(MyGUI::FontCodeType::SelectedBack),
	static_cast<MyGUI::Char>(MyGUI::FontCodeType::Cursor),
	static_cast<MyGUI::Char>(MyGUI::FontCodeType::NotDefined)
};

FT_Face FreeTypeFont::loadFontFace(const FT_Library& ftLibrary, uint8_t*& outputFontBuffer) {
	outputFontBuffer = nullptr;

	MyGUI::DataManager& dataManager = MyGUI::DataManager::getInstance();
	MyGUI::IDataStream* dataStream = dataManager.getData(this->properties_.source);
	if (!dataStream) {
		CONSOLE_ERROR("FreeTypeFont: Failed to open font source: %s", this->properties_.source.c_str());
		return nullptr;
	}

	const size_t fontBufferSize = static_cast<size_t>(dataStream->size());
	if (fontBufferSize == 0) {
		CONSOLE_ERROR("FreeTypeFont: Font source is empty: %s", this->properties_.source.c_str());
		dataManager.freeData(dataStream);
		return nullptr;
	}

	std::unique_ptr<uint8_t[]> fontBuffer(new uint8_t[fontBufferSize]);

	dataStream->read(fontBuffer.get(), fontBufferSize);
	dataManager.freeData(dataStream);
	dataStream = nullptr;

	FT_Face ftFace = nullptr;
	if (FT_New_Memory_Face(ftLibrary, fontBuffer.get(), static_cast<FT_Long>(fontBufferSize), 0, &ftFace) != 0) {
		CONSOLE_ERROR("FreeTypeFont: Failed to create font face from source: %s", this->properties_.source.c_str());
		return nullptr;
	}

	if (!(ftFace->face_flags & FT_FACE_FLAG_SCALABLE)) {
		CONSOLE_ERROR("FreeTypeFont: Font face is not scalable: %s", this->properties_.source.c_str());
		
		FT_Done_Face(ftFace);
		return nullptr;
	}

	const FT_F26Dot6 ftSize = static_cast<FT_F26Dot6>(this->properties_.size * FontManager::getScaleFactor() * 64.0f);
	if (FT_Set_Char_Size(ftFace, ftSize, 0, 96, 96) != 0) {
		CONSOLE_ERROR("FreeTypeFont: Failed to set character size for font face: %s", this->properties_.source.c_str());
		
		FT_Done_Face(ftFace);
		return nullptr;
	}

	if (this->characterMap_.empty())
		this->addCodePointRnage(0, 0xFFFF);

	outputFontBuffer = fontBuffer.release();
	return ftFace;
}

void FreeTypeFont::autoWrapGlyphPosition(int glyphWidth, int textureWidth, int lineHeight, int& outputTextureX, int& outputTextureY) {
	if (glyphWidth > 0 && outputTextureX + glyphSpacing + glyphWidth > textureWidth) {
		outputTextureX = 0;
		outputTextureY += lineHeight + glyphSpacing;
	}
}

MyGUI::GlyphInfo FreeTypeFont::createFaceGlyphInfo(MyGUI::Char codepoint, int fontAscent, FT_GlyphSlot glyph) const {
	float width = 0.0f;
	float height = 0.0f;
	float bearingX = 0.0f;
	float bearingY = 0.0f;

	const FT_Bitmap& bitmap = glyph->bitmap;
	if (bitmap.buffer != nullptr && bitmap.width > 0 && bitmap.rows > 0 && bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
		width = static_cast<float>(bitmap.width);
		height = static_cast<float>(bitmap.rows);
		bearingX = static_cast<float>(glyph->bitmap_left);
		bearingY = static_cast<float>(glyph->bitmap_top);
	}

	const float advance = (glyph->advance.x / 64.0f) - bearingX + this->properties_.letterSpacing;

	return MyGUI::GlyphInfo(codepoint, width, height, advance, bearingX, std::floor(static_cast<float>(fontAscent) - bearingY));
}

int FreeTypeFont::createGlyph(FT_UInt glyphIndex, const MyGUI::GlyphInfo& glyphInfo, GlyphHeightMap& glyphHeightMap) {
	const int width = static_cast<int>(std::ceil(glyphInfo.width));
	const int height = static_cast<int>(std::ceil(glyphInfo.height));

	MyGUI::GlyphInfo& glyph = this->glyphMap_.insert_or_assign(glyphInfo.codePoint, glyphInfo).first->second;
	glyphHeightMap[height].insert(std::make_pair(glyphIndex, &glyph));

	return (width > 0) ? glyphSpacing + width : 0;
}

int FreeTypeFont::createFaceGlyph(FT_UInt glyphIndex, MyGUI::Char codepoint, int fontascent, const FT_Face& ftFace, GlyphHeightMap& glyphHeightMap) {
	if (this->glyphMap_.find(codepoint) != this->glyphMap_.end())
		return 0;

	if (FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER) != 0) {
		CONSOLE_WARNING("FreeTypeFont: Failed to load glyph for code point %u in %s", codepoint, this->properties_.source.c_str());
		return 0;
	}

	return this->createGlyph(glyphIndex, this->createFaceGlyphInfo(codepoint, fontascent, ftFace->glyph), glyphHeightMap);
}

void FreeTypeFont::renderGlyphs(const GlyphHeightMap& glyphHeightMap, const FT_Face& ftFace, uint8_t* textureBuffer, int textureWidth, int textureHeight) {
	int textureX = 0;
	int textureY = 0;

	std::vector<uint8_t> packedBitmap;

	for (const auto& [lineHeight, glyphList] : glyphHeightMap) {
		for (const auto& [glyphIndex, glyphInfoPtr] : glyphList) {
			MyGUI::GlyphInfo& info = *glyphInfoPtr;

			const auto mask = charMask.find(info.codePoint);
			if (mask != charMask.end()) {
				this->renderGlyph(info, mask->second, lineHeight, textureBuffer, textureWidth, textureHeight, textureX, textureY);

				if (info.codePoint == static_cast<MyGUI::Char>(MyGUI::FontCodeType::Selected) || info.codePoint == static_cast<MyGUI::Char>(MyGUI::FontCodeType::SelectedBack)) {
					info.width = 0.0f;
					info.uvRect.right = info.uvRect.left;
				}

				continue;
			}

			const int glyphWidth = static_cast<int>(std::ceil(info.width));
			const int glyphHeight = static_cast<int>(std::ceil(info.height));

			packedBitmap.clear();

			if (glyphWidth > 0 && glyphHeight > 0 && FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER) == 0) {
				const FT_Bitmap& bitmap = ftFace->glyph->bitmap;
				if (bitmap.buffer != nullptr && bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
					packedBitmap.assign(static_cast<size_t>(glyphWidth) * glyphHeight, charMaskBlack);

					const int copyWidth = std::min(glyphWidth, static_cast<int>(bitmap.width));
					const int copyRows = std::min(glyphHeight, static_cast<int>(bitmap.rows));

					for (int row = 0; row < copyRows; ++row) {
						const uint8_t* source = bitmap.pitch >= 0
							? bitmap.buffer + static_cast<size_t>(row) * bitmap.pitch
							: bitmap.buffer + static_cast<size_t>(bitmap.rows - 1 - row) * (-bitmap.pitch);

						memcpy(packedBitmap.data() + static_cast<size_t>(row) * glyphWidth, source, static_cast<size_t>(copyWidth));
					}
				}
			}

			this->renderGlyph(info, charMaskWhite, lineHeight, textureBuffer, textureWidth, textureHeight, textureX, textureY, packedBitmap.empty() ? nullptr : packedBitmap.data());
		}
	}
}

void FreeTypeFont::renderGlyph(MyGUI::GlyphInfo& glyphInfo, uint8_t alpha, int lineHeight, uint8_t* textureBuffer, int textureWidth, int textureHeight, int& outputTextureX, int& outputTextureY, uint8_t* glyphBuffer) {
	const int width = static_cast<int>(std::ceil(glyphInfo.width));
	const int height = static_cast<int>(std::ceil(glyphInfo.height));

	this->autoWrapGlyphPosition(width, textureWidth, lineHeight, outputTextureX, outputTextureY);

	if (width > 0 && outputTextureX + width <= textureWidth && outputTextureY + height <= textureHeight) {
		for (int row = 0; row < height; ++row) {
			uint8_t* destination = textureBuffer + static_cast<size_t>(outputTextureY + row) * textureWidth + outputTextureX;

			if (glyphBuffer)
				std::memcpy(destination, glyphBuffer + static_cast<size_t>(row) * width, static_cast<size_t>(width));
			else
				std::memset(destination, alpha, static_cast<size_t>(width));
		}
	}

	glyphInfo.uvRect.left = static_cast<float>(outputTextureX) / textureWidth;
	glyphInfo.uvRect.top = static_cast<float>(outputTextureY) / textureHeight;
	glyphInfo.uvRect.right = static_cast<float>(outputTextureX + glyphInfo.width) / textureWidth;
	glyphInfo.uvRect.bottom = static_cast<float>(outputTextureY + glyphInfo.height) / textureHeight;

	outputTextureX += glyphSpacing + width;
}

void FreeTypeFont::initialise() {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_BLUE, "Loading Font: %s", this->mResourceName.c_str());
	Application::getInstance().splashscreen.setStatus(UTILS::formatString(L"Loading font: %hs", this->mResourceName.c_str()));

	MyGUI::RenderManager& renderManager = MyGUI::RenderManager::getInstance();
	if (!renderManager.isFormatSupported(MyGUI::PixelFormat::L8, MyGUI::TextureUsage::Static | MyGUI::TextureUsage::Write)) {
		CONSOLE_ERROR("FreeTypeFont: L8 texture format is not supported.");
		return;
	}

	FontManager& fontManager = FontManager::getInstance();
	FT_Library& ftLibrary = fontManager.getFTLibrary();

	uint8_t* rawFontBuffer = nullptr;
	FT_Face ftFace = this->loadFontFace(ftLibrary, rawFontBuffer);
	if (!ftFace) {
		CONSOLE_ERROR("FreeTypeFont: Failed to load font face from source: %s", this->properties_.source.c_str());
		return;
	}

	std::unique_ptr<uint8_t[]> fontBuffer(rawFontBuffer);
	std::unique_ptr<FT_FaceRec, decltype(&FT_Done_Face)> faceGuard(ftFace, &FT_Done_Face);

	this->glyphMap_.clear();

	for (const MyGUI::Char special : specialCodePoints)
		this->characterMap_.erase(special);

	int fontAscender = static_cast<int>(ftFace->size->metrics.ascender >> 6);
	int fontDescender = static_cast<int>(-ftFace->size->metrics.descender >> 6);

	TT_OS2* os2Table = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(ftFace, FT_SFNT_OS2));
	if (os2Table && ftFace->units_per_EM != 0) {
		const int ppem = ftFace->size->metrics.y_ppem;
		const int unitsPerEM = ftFace->units_per_EM;

		setMaxValue<int>(fontAscender, os2Table->usWinAscent * ppem / unitsPerEM);
		setMaxValue<int>(fontDescender, os2Table->usWinDescent * ppem / unitsPerEM);

		setMaxValue<int>(fontAscender, os2Table->sTypoAscender * ppem / unitsPerEM);
		setMaxValue<int>(fontDescender, -os2Table->sTypoDescender * ppem / unitsPerEM);
	}

	this->properties_.defaultHeight = fontAscender + fontDescender;

	GlyphHeightMap glyphHeightMap;
	int textureWidth = 0;

	this->addCodePoint(' ');

	for (auto iterator = this->characterMap_.begin(); iterator != this->characterMap_.end();) {
		const MyGUI::Char codepoint = iterator->first;
		const FT_UInt glyphIndex = FT_Get_Char_Index(ftFace, codepoint);

		if (glyphIndex == 0) {
			iterator = this->characterMap_.erase(iterator);
			continue;
		}

		iterator->second = glyphIndex;
		textureWidth += this->createFaceGlyph(glyphIndex, codepoint, fontAscender, ftFace, glyphHeightMap);
		++iterator;
	}

	FT_UInt nextGlyphIndex = static_cast<FT_UInt>(ftFace->num_glyphs);

	const float height = static_cast<float>(this->properties_.defaultHeight);
	textureWidth += this->createGlyph(nextGlyphIndex++, MyGUI::GlyphInfo(static_cast<MyGUI::Char>(MyGUI::FontCodeType::Tab), 0.0f, 0.0f, tabWidth, 0.0f, 0.0f), glyphHeightMap);
	textureWidth += this->createGlyph(nextGlyphIndex++, MyGUI::GlyphInfo(static_cast<MyGUI::Char>(MyGUI::FontCodeType::Selected), selectedWidth, height, 0.0f, 0.0f, 0.0f), glyphHeightMap);
	textureWidth += this->createGlyph(nextGlyphIndex++, MyGUI::GlyphInfo(static_cast<MyGUI::Char>(MyGUI::FontCodeType::SelectedBack), selectedWidth, height, 0.0f, 0.0f, 0.0f), glyphHeightMap);
	textureWidth += this->createGlyph(nextGlyphIndex++, MyGUI::GlyphInfo(static_cast<MyGUI::Char>(MyGUI::FontCodeType::Cursor), cursorWidth, height, 0.0f, 0.0f, 0.0f), glyphHeightMap);

	textureWidth += this->createFaceGlyph(0, static_cast<MyGUI::Char>(MyGUI::FontCodeType::NotDefined), fontAscender, ftFace, glyphHeightMap);

	double averageGlyphHeight = 0.0;
	for (const auto& [glyphHeight, glyphList] : glyphHeightMap)
		averageGlyphHeight += static_cast<double>(glyphHeight) * static_cast<double>(glyphList.size());

	averageGlyphHeight /= static_cast<double>(this->glyphMap_.size());

	textureWidth = MyGUI::Bitwise::firstPO2From(textureWidth);
	int textureHeight = MyGUI::Bitwise::firstPO2From(static_cast<int>(std::ceil(averageGlyphHeight)) + glyphSpacing);
	while (textureWidth > textureHeight) {
		textureWidth /= 2;
		textureHeight *= 2;
	}

	do {
		if (textureHeight > textureWidth * 2)
			textureWidth *= 2;

		int textureX = 0;
		int textureY = 0;

		for (const auto& [lineHeight, glyphList] : glyphHeightMap) {
			for (const auto& [glyphIndex, glyphInfo] : glyphList) {
				const int glyphWidth = static_cast<int>(std::ceil(glyphInfo->width));
				const int glyphHeight = static_cast<int>(std::ceil(glyphInfo->height));

				this->autoWrapGlyphPosition(glyphWidth, textureWidth, glyphHeight, textureX, textureY);

				if (glyphWidth > 0)
					textureX += glyphSpacing + glyphWidth;

				if (textureY + glyphHeight > textureHeight)
					textureHeight = MyGUI::Bitwise::firstPO2From(textureY + glyphHeight);
			}
		}
	} while (textureHeight > textureWidth * 2);

	if (this->textureFont_) {
		renderManager.destroyTexture(this->textureFont_);
		this->textureFont_ = nullptr;
	}

	this->textureFont_ = renderManager.createTexture(MyGUI::utility::toString(reinterpret_cast<size_t>(this), "_TrueTypeFont"));
	this->textureFont_->setShader("FontShader");
	this->textureFont_->createManual(textureWidth, textureHeight, MyGUI::TextureUsage::Static | MyGUI::TextureUsage::Write, MyGUI::PixelFormat::L8);
	this->textureFont_->setInvalidateListener(this);

	uint8_t* textureBuffer = static_cast<uint8_t*>(this->textureFont_->lock(MyGUI::TextureUsage::Write));
	if (!textureBuffer) {
		CONSOLE_ERROR("FreeTypeFont: Failed to lock texture for writing.");
		return;
	}

	memset(textureBuffer, charMaskBlack, static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight));

	this->renderGlyphs(glyphHeightMap, ftFace, textureBuffer, textureWidth, textureHeight);

	this->textureFont_->unlock();

	const auto fallback = this->glyphMap_.find(static_cast<MyGUI::Char>(MyGUI::FontCodeType::NotDefined));
	if (fallback != this->glyphMap_.end())
		this->fallbackGlyphInfo_ = fallback->second;
}

FreeTypeFont::~FreeTypeFont() {
	if (this->textureFont_) {
		MyGUI::RenderManager& renderManager = MyGUI::RenderManager::getInstance();
		renderManager.destroyTexture(this->textureFont_);

		this->textureFont_ = nullptr;
	}
}

void FreeTypeFont::deserialization(MyGUI::xml::ElementPtr node, MyGUI::Version version) {
	MyGUI::IFont::deserialization(node, version);

	const std::string resourceName = node->findAttribute("name");

	MyGUI::xml::ElementEnumerator child = node->getElementEnumerator();
	while (child.next()) {
		if (child->getName() == "Property") {
			const std::string key = child->findAttribute("key");
			const std::string value = child->findAttribute("value");

			if (key == "Source") {
				this->properties_.source = value;
			} else if (key == "Size") {
				this->properties_.size = MyGUI::utility::parseFloat(value);
			} else if (key == "LetterSpacing") {
				this->properties_.letterSpacing = MyGUI::utility::parseFloat(value);
			} else {
				CONSOLE_WARNING("FreeTypeFont: Unknown property key: %s in %s", key.c_str(), resourceName.c_str());
			}
		}
	}

	this->initialise();
}

const MyGUI::GlyphInfo* FreeTypeFont::getGlyphInfo(MyGUI::Char character) const {
	const auto iterator = this->glyphMap_.find(character);
	if (iterator != this->glyphMap_.end())
		return &iterator->second;

	return &this->fallbackGlyphInfo_;
}

MyGUI::ITexture* FreeTypeFont::getTextureFont() const {
	return this->textureFont_;
}

int FreeTypeFont::getDefaultHeight() const {
	return this->properties_.defaultHeight;
}

void FreeTypeFont::textureInvalidate(MyGUI::ITexture* _texture) {
	this->glyphMap_.clear();
	this->initialise();
}

void FreeTypeFont::addCodePoint(MyGUI::Char codepoint) {
	this->characterMap_[codepoint] = 0;
}

void FreeTypeFont::removeCodePoint(MyGUI::Char codepoint) {
	this->characterMap_.erase(codepoint);
}

void FreeTypeFont::clearCodePoints() {
	this->characterMap_.clear();
}

void FreeTypeFont::addCodePointRnage(MyGUI::Char start, MyGUI::Char end) {
	if (start > end) {
		CONSOLE_WARNING("FreeTypeFont: Invalid code point range: start (%u) is greater than end (%u).", start, end);
		return;
	}

	auto hint = this->characterMap_.lower_bound(start);
	for (MyGUI::Char codepoint = start;; ++codepoint) {
		hint = this->characterMap_.insert_or_assign(hint, codepoint, 0);
		++hint;

		if (codepoint == end)
			break;
	}
}

void FreeTypeFont::removeCodePointRange(MyGUI::Char start, MyGUI::Char end) {
	if (start > end) {
		CONSOLE_WARNING("FreeTypeFont: Invalid code point range: start (%u) is greater than end (%u).", start, end);
		return;
	}

	this->characterMap_.erase(this->characterMap_.lower_bound(start), this->characterMap_.upper_bound(end));
}