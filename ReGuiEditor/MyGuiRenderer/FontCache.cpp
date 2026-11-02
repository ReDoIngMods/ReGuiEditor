#include "FontCache.hpp" // VCIC-Excluded

#include "RenderPlatform/RPTexture.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <Modules/Bytestream/BytestreamReader.hpp>
#include <Modules/Bytestream/BytestreamWriter.hpp>
#include <Modules/Math/Hashing.hpp>
#include <Modules/Misc/Strings.hpp>

#include <rend/ResourceManager.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_FontData.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_ResourceManager.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <freetype/fttypes.h>

#include <MyGui/MyGUI_IObject.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

constexpr uint32_t CACHE_VERSION = 3;

struct FontCacheHeader {
	char magic[4] = { 'M', 'G', 'F', 'C' }; // MyGui Font Cache

	uint32_t version = CACHE_VERSION;
	uint32_t width = 0;
	uint32_t height = 0;

	bool isValid() const {
		return memcmp(this->magic, "MGFC", 4) == 0 && this->version == CACHE_VERSION;
	}
};

struct FontCacheGlyphInfo {
	float width = 0.0f;
	float height = 0.0f;
	float advance = 0.0f;
	float bearingX = 0.0f;
	float bearingY = 0.0f;
	float uvRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	FT_UInt glyphIndex = 0;
};

static void hashElementRecursive(MyGUI::xml::ElementPtr element, uint64_t& hash) {
	if (!element)
		return;

	ReGuiEditor::hashCombine(hash, element->getName());

	MyGUI::xml::VectorAttributes attributes = element->getAttributes();
	for (const auto& attribute : attributes) {
		ReGuiEditor::hashCombine(hash, attribute.first);
		ReGuiEditor::hashCombine(hash, attribute.second);
	}

	ReGuiEditor::hashCombine(hash, element->getContent());

	MyGUI::xml::ElementEnumerator childElements = element->getElementEnumerator();
	while (childElements.next())
		hashElementRecursive(childElements.current(), hash);
}

uint64_t FontCache::hashElement(MyGUI::xml::ElementPtr element) {
	if (!element)
		return 0;

	uint64_t hash = ReGuiEditor::HASH_SEED;
	hashElementRecursive(element, hash);

	return hash;
}

std::string FontCache::getFilepath(const std::string& fontName, uint64_t hash) {
	return ReGuiEditor::formatString("$RGE_CACHE_DATA/Fonts/%s_%016llx.mgfc", fontName.c_str(), hash);
}

std::optional<FontCache::Data> FontCache::read(const std::string& filePath, std::string& outputErrorMsg) {
	outputErrorMsg.clear();

	const ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();
	if (!filesystemManager.exists(filePath))
		return std::nullopt;

	ReGuiEditor::BytestreamReader reader;
	if (!filesystemManager.readFile(filePath, reader)) {
		outputErrorMsg = "Failed to read cache file: " + filePath;
		return std::nullopt;
	}

	const FontCacheHeader header = reader.read<FontCacheHeader>();
	if (!header.isValid()) {
		outputErrorMsg = "Invalid cache header in file: " + filePath;
		return std::nullopt;
	}

	Data data;
	data.width = header.width;
	data.height = header.height;

	data.resourceName = reader.read<std::string>();
	data.source = reader.read<std::string>();
	data.size = reader.read<float>();
	data.resolution = reader.read<uint32_t>();
	data.antialias = reader.read<bool>();
	data.spaceWidth = reader.read<float>();
	data.tabWidth = reader.read<float>();
	data.offsetHeight = reader.read<int>();
	data.substituteCodePoint = reader.read<uint32_t>();
	data.defaultHeight = reader.read<int>();

	const uint32_t totalCodeRanges = reader.read<uint32_t>();
	for (uint32_t index = 0; index < totalCodeRanges; ++index) {
		const MyGUI::Char first = reader.read<MyGUI::Char>();
		const MyGUI::Char second = reader.read<MyGUI::Char>();

		const uint64_t glyphCount = static_cast<uint64_t>(second) - first + 1;
		if (second < first || glyphCount * sizeof(FontCacheGlyphInfo) > reader.remaining()) {
			outputErrorMsg = "Corrupt glyph range in cache file: " + filePath;
			return std::nullopt;
		}

		data.glyphs.reserve(data.glyphs.size() + static_cast<size_t>(glyphCount));
		for (uint64_t value = first; value <= second; ++value) {
			const FontCacheGlyphInfo cacheInfo = reader.read<FontCacheGlyphInfo>();

			Glyph& glyph = data.glyphs.emplace_back();
			glyph.codePoint = static_cast<MyGUI::Char>(value);
			glyph.glyphIndex = cacheInfo.glyphIndex;
			glyph.info.codePoint = glyph.codePoint;
			glyph.info.width = cacheInfo.width;
			glyph.info.height = cacheInfo.height;
			glyph.info.advance = cacheInfo.advance;
			glyph.info.bearingX = cacheInfo.bearingX;
			glyph.info.bearingY = cacheInfo.bearingY;
			glyph.info.uvRect.left = cacheInfo.uvRect[0];
			glyph.info.uvRect.top = cacheInfo.uvRect[1];
			glyph.info.uvRect.right = cacheInfo.uvRect[2];
			glyph.info.uvRect.bottom = cacheInfo.uvRect[3];
		}
	}

	if (!reader.readCompressableData(data.pixels)) {
		outputErrorMsg = "Failed to read compressed font data from: " + filePath;
		return std::nullopt;
	}

	const uint64_t expectedSize = static_cast<uint64_t>(data.width) * data.height * 4;
	if (data.width == 0 || data.height == 0 || data.pixels.size() != expectedSize) {
		outputErrorMsg = "Font texture size mismatch in cache file: " + filePath;
		return std::nullopt;
	}

	return data;
}

MyGUI::ResourceTrueTypeFont* FontCache::create(Data&& data, const std::string& fontName) {
	MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();

	MyGUI::IObject* fontObject = factoryManager.createObject("Resource", "ResourceTrueTypeFont");
	std::unique_ptr<MyGUI::ResourceTrueTypeFont> fontResource(fontObject ? fontObject->castType<MyGUI::ResourceTrueTypeFont>() : nullptr);
	if (!fontResource) {
		CONSOLE_ERROR("Failed to create ResourceTrueTypeFont object for cached font: %s", fontName.c_str());
		delete fontObject;
		return nullptr;
	}

	fontResource->mResourceName = std::move(data.resourceName);
	fontResource->mSource = std::move(data.source);
	fontResource->mSize = data.size;
	fontResource->mResolution = data.resolution;
	fontResource->mAntialias = data.antialias;
	fontResource->mSpaceWidth = data.spaceWidth;
	fontResource->mTabWidth = data.tabWidth;
	fontResource->mOffsetHeight = data.offsetHeight;
	fontResource->mSubstituteCodePoint = data.substituteCodePoint;
	fontResource->mDefaultHeight = data.defaultHeight;

	auto charMapHint = fontResource->mCharMap.end();
	auto glyphMapHint = fontResource->mGlyphMap.end();

	for (const Glyph& glyph : data.glyphs) {
		charMapHint = fontResource->mCharMap.emplace_hint(charMapHint, glyph.codePoint, glyph.glyphIndex);
		glyphMapHint = fontResource->mGlyphMap.emplace_hint(glyphMapHint, glyph.codePoint, glyph.info);

		if (fontResource->mSubstituteCodePoint == glyph.codePoint)
			fontResource->mSubstituteGlyphInfo = &glyphMapHint->second;
	}

	RPTexture* texture = dynamic_cast<RPTexture*>(MyGUI::RenderManager::getInstance().createTexture("FontTexture_" + fontName));
	if (!texture)
		return nullptr;

	texture->createManual(static_cast<int>(data.width), static_cast<int>(data.height), MyGUI::TextureUsage::Static, MyGUI::PixelFormat::R8G8B8A8);

	void* textureData = texture->lock(MyGUI::TextureUsage::Write);
	if (!textureData) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to lock texture for font: %s", fontName.c_str());
		texture->destroy();
		return nullptr;
	}

	memcpy(textureData, data.pixels.data(), data.pixels.size());
	texture->unlock();

	fontResource->mTexture = texture;

	MyGUI::ResourceTrueTypeFont* releasedResource = fontResource.release();
	MyGUI::ResourceManager::getInstance().addResource(releasedResource);

	return releasedResource;
}

bool FontCache::load(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource) {
	const std::string fontName = element->findAttribute("name");
	if (fontName.empty())
		return false;

	std::string error;
	std::optional<Data> data = read(filePath, error);
	if (!data) {
		if (!error.empty())
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "%s", error.c_str());

		return false;
	}

	MyGUI::ResourceTrueTypeFont* fontResource = create(std::move(*data), fontName);
	if (!fontResource)
		return false;

	if (outputFontResource)
		*outputFontResource = fontResource;

	return true;
}

bool FontCache::save(const std::string& filePath, const std::string& fontName, MyGUI::ResourceTrueTypeFont* fontResource) {
	if (!fontResource)
		return false;

	RPTexture* texture = dynamic_cast<RPTexture*>(fontResource->getTextureFont());
	if (!texture) {
		CONSOLE_ERROR("FontCache::save: Failed to retrieve texture font for: %s", fontName.c_str());
		return false;
	}

	const rend::Texture2DHandle fontTexture = texture->getTexture();
	if (!fontTexture) {
		CONSOLE_ERROR("FontCache::save: Texture font is not initialized for: %s", fontName.c_str());
		return false;
	}

	const uint32_t width = fontTexture->getWidth();
	const uint32_t height = fontTexture->getHeight();
	if (width == 0 || height == 0) {
		CONSOLE_ERROR("FontCache::save: Invalid texture dimensions for: %s", fontName.c_str());
		return false;
	}

	rend::Texture2DHandle stagingTexture = rend::ResourceManager::getInstance().createTexture2D(width, height, fontTexture->getFormat(), rend::Texture2DFlags::StagingRead);
	if (!stagingTexture || !fontTexture->copyResource(stagingTexture)) {
		CONSOLE_ERROR("FontCache::save: Failed to create staging texture for: %s", fontName.c_str());
		return false;
	}
	stagingTexture->setDebugName("MyGUI::Font \"" + fontName + "\" (Staging)");

	const size_t dataSize = fontTexture->getTotalSize();
	std::vector<uint8_t> pixelData(dataSize);
	if (!stagingTexture->readData(pixelData.data(), static_cast<uint32_t>(pixelData.size()))) {
		CONSOLE_ERROR("FontCache::save: Failed to read back texture data for: %s", fontName.c_str());
		return false;
	}

	ReGuiEditor::BytestreamWriter writer;

	FontCacheHeader header;
	header.width = width;
	header.height = height;
	writer.write(header);

	writer.write<std::string>(fontResource->mResourceName);
	writer.write<std::string>(fontResource->mSource);
	writer.write<float>(fontResource->mSize);
	writer.write<uint32_t>(fontResource->mResolution);
	writer.write<bool>(fontResource->mAntialias);
	writer.write<float>(fontResource->mSpaceWidth);
	writer.write<float>(fontResource->mTabWidth);
	writer.write<int>(fontResource->mOffsetHeight);
	writer.write<uint32_t>(fontResource->mSubstituteCodePoint);
	writer.write<int>(fontResource->mDefaultHeight);

	std::vector<std::pair<MyGUI::Char, MyGUI::Char>> codeRanges = fontResource->getCodePointRanges();
	writer.write<uint32_t>(static_cast<uint32_t>(codeRanges.size()));

	for (const auto& range : codeRanges) {
		writer.write<uint32_t>(range.first);
		writer.write<uint32_t>(range.second);

		for (uint64_t value = range.first; value <= range.second; ++value) {
			const MyGUI::Char codePoint = static_cast<MyGUI::Char>(value);

			const MyGUI::GlyphInfo* glyphInfo = fontResource->getGlyphInfo(codePoint);
			if (!glyphInfo) {
				CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to retrieve glyph info for code point: %u in font: %s", codePoint, fontName.c_str());

				writer.write(FontCacheGlyphInfo{});
				continue;
			}

			FontCacheGlyphInfo glyphCacheInfo;
			glyphCacheInfo.width = glyphInfo->width;
			glyphCacheInfo.height = glyphInfo->height;
			glyphCacheInfo.advance = glyphInfo->advance;
			glyphCacheInfo.bearingX = glyphInfo->bearingX;
			glyphCacheInfo.bearingY = glyphInfo->bearingY;
			glyphCacheInfo.uvRect[0] = glyphInfo->uvRect.left;
			glyphCacheInfo.uvRect[1] = glyphInfo->uvRect.top;
			glyphCacheInfo.uvRect[2] = glyphInfo->uvRect.right;
			glyphCacheInfo.uvRect[3] = glyphInfo->uvRect.bottom;
			glyphCacheInfo.glyphIndex = fontResource->mCharMap[codePoint];

			writer.write(glyphCacheInfo);
		}
	}

	writer.writeCompressableData(pixelData);

	if (!ReGuiEditor::FilesystemManager::getInstance().writeFile(filePath, writer)) {
		CONSOLE_ERROR("FontCache::save: Failed to write cache file: %s", filePath.c_str());
		return false;
	}

	return true;
}
