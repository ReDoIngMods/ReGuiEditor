#include "FontCache.hpp" // VCIC-Excluded

#include "RenderPlatform/RPTexture.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
#include <UTILS/Hashing.hpp>
#include <UTILS/Strings.hpp>

#include <rend/ResourceManager.hpp>
#include <rend/Resources/Texture2D.hpp>

#include <LZ4/lz4.h>

#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_FontData.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_ResourceManager.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <freetype/fttypes.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

constexpr uint32_t CACHE_VERSION = 2;
constexpr uint64_t HASH_SEED = 14695981039346656037ULL;

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

	UTILS::hashCombine(hash, element->getName());

	MyGUI::xml::VectorAttributes attributes = element->getAttributes();
	for (const auto& attribute : attributes) {
		UTILS::hashCombine(hash, attribute.first);
		UTILS::hashCombine(hash, attribute.second);
	}

	UTILS::hashCombine(hash, element->getContent());

	MyGUI::xml::ElementEnumerator childElements = element->getElementEnumerator();
	while (childElements.next())
		hashElementRecursive(childElements.current(), hash);
}

uint64_t FontCache::hashElement(MyGUI::xml::ElementPtr element) {
	if (!element)
		return 0;

	uint64_t hash = HASH_SEED;
	hashElementRecursive(element, hash);

	return hash;
}

std::string FontCache::getFilepath(const std::string& fontName, uint64_t hash) {
	return UTILS::formatString("$RGE_CACHE_DATA/Fonts/%s_%016llx.mgfc", fontName.c_str(), hash);
}

bool FontCache::load(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource) {
	const FilesystemManager& filesystemManager = FilesystemManager::getInstance();
	if (!filesystemManager.exists(filePath))
		return false;

	UTILS::ByteStreamReader reader;
	if (!filesystemManager.readFile(filePath, reader)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to read cache file: %s", filePath.c_str());
		return false;
	}

	FontCacheHeader header = reader.read<FontCacheHeader>();
	if (!header.isValid()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Invalid cache header in file: %s", filePath.c_str());
		return false;
	}

	const std::string fontName = element->findAttribute("name");
	if (fontName.empty())
		return false;

	MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();
	MyGUI::ResourceTrueTypeFont* fontResource = factoryManager.createObject("Resource", "ResourceTrueTypeFont")->castType<MyGUI::ResourceTrueTypeFont>();
	if (!fontResource) {
		CONSOLE_ERROR("Failed to create ResourceTrueTypeFont object for cached font: %s", fontName.c_str());
		return false;
	}

	fontResource->mResourceName = reader.readString();
	fontResource->mSource = reader.readString();
	fontResource->mSize = reader.read<float>();
	fontResource->mResolution = reader.read<uint32_t>();
	fontResource->mAntialias = reader.read<bool>();
	fontResource->mSpaceWidth = reader.read<float>();
	fontResource->mTabWidth = reader.read<float>();
	fontResource->mOffsetHeight = reader.read<int>();
	fontResource->mSubstituteCodePoint = reader.read<uint32_t>();
	fontResource->mDefaultHeight = reader.read<int>();

	const uint32_t totalCodeRanges = reader.read<uint32_t>();

	auto charMapHint = fontResource->mCharMap.end();
	auto glyphMapHint = fontResource->mGlyphMap.end();

	for (uint32_t index = 0; index < totalCodeRanges; ++index) {
		const MyGUI::Char first = reader.read<MyGUI::Char>();
		const MyGUI::Char second = reader.read<MyGUI::Char>();

		for (uint64_t value = first; value <= second; ++value) {
			const MyGUI::Char codePoint = static_cast<MyGUI::Char>(value);
			const FontCacheGlyphInfo glyphCacheInfo = reader.read<FontCacheGlyphInfo>();

			MyGUI::GlyphInfo glyphInfo{};
			glyphInfo.codePoint = codePoint;
			glyphInfo.width = glyphCacheInfo.width;
			glyphInfo.height = glyphCacheInfo.height;
			glyphInfo.advance = glyphCacheInfo.advance;
			glyphInfo.bearingX = glyphCacheInfo.bearingX;
			glyphInfo.bearingY = glyphCacheInfo.bearingY;
			glyphInfo.uvRect.left = glyphCacheInfo.uvRect[0];
			glyphInfo.uvRect.top = glyphCacheInfo.uvRect[1];
			glyphInfo.uvRect.right = glyphCacheInfo.uvRect[2];
			glyphInfo.uvRect.bottom = glyphCacheInfo.uvRect[3];

			charMapHint = fontResource->mCharMap.emplace_hint(charMapHint, codePoint, glyphCacheInfo.glyphIndex);
			glyphMapHint = fontResource->mGlyphMap.emplace_hint(glyphMapHint, codePoint, glyphInfo);
			MyGUI::GlyphInfo& storedGlyphInfo = glyphMapHint->second;

			if (fontResource->mSubstituteCodePoint == codePoint)
				fontResource->mSubstituteGlyphInfo = &storedGlyphInfo;
		}
	}

	RPTexture* texture = dynamic_cast<RPTexture*>(MyGUI::RenderManager::getInstance().createTexture("FontTexture_" + fontName));
	texture->createManual(static_cast<int>(header.width), static_cast<int>(header.height), MyGUI::TextureUsage::Static, MyGUI::PixelFormat::R8G8B8A8);

	const size_t uncompressedDataSize = reader.read<size_t>();
	const size_t compressedDataSize = reader.read<size_t>();

	std::vector<char> compressedData(compressedDataSize);
	reader.readBytes(compressedData.data(), compressedDataSize);

	std::vector<char> uncompressedData(uncompressedDataSize);

	int decompressedSize = LZ4_decompress_safe(compressedData.data(), uncompressedData.data(), static_cast<int>(compressedDataSize), static_cast<int>(uncompressedDataSize));
	if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != uncompressedDataSize) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to decompress font data for font: %s", fontName.c_str());
		texture->destroy();
		return false;
	}

	void* textureData = texture->lock(MyGUI::TextureUsage::Write);
	if (!textureData) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to lock texture for font: %s", fontName.c_str());
		texture->destroy();
		return false;
	}

	memcpy(textureData, uncompressedData.data(), uncompressedDataSize);
	texture->unlock();

	fontResource->mTexture = texture;

	MyGUI::ResourceManager::getInstance().addResource(fontResource);

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

	UTILS::ByteStreamWriter writer;

	FontCacheHeader header;
	header.width = width;
	header.height = height;
	writer.write(header);

	writer.writeString(fontResource->mResourceName);
	writer.writeString(fontResource->mSource);
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

	writer.write<size_t>(dataSize);

	const int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
	std::vector<char> compressedData(static_cast<size_t>(maxCompressedSize));

	const int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(pixelData.data()), compressedData.data(), static_cast<int>(dataSize), maxCompressedSize);
	if (compressedSize <= 0) {
		CONSOLE_ERROR("FontCache::save: Failed to compress font data: %s", fontName.c_str());
		return false;
	}

	writer.write<size_t>(static_cast<size_t>(compressedSize));
	writer.writeBytes(compressedData.data(), static_cast<size_t>(compressedSize));

	if (!FilesystemManager::getInstance().writeFile(filePath, writer)) {
		CONSOLE_ERROR("FontCache::save: Failed to write cache file: %s", filePath.c_str());
		return false;
	}

	return true;
}
