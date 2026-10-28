#include "FontManager.hpp"

#include "RenderPlatform/RPTexture.hpp"

#include <Application.hpp>

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FilesystemManager.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>
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
#include <MyGui/MyGUI_Version.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <freetype/fttypes.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct FontCacheHeader {
	char magic[4] = { 'M', 'G', 'F', 'C' }; // MyGui Font Cache

	uint32_t version = 2;
	uint32_t width = 0;
	uint32_t height = 0;

	bool isValid() const {
		return memcmp(this->magic, "MGFC", 4) == 0 && this->version == 2;
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

void FontManager::hashCombineString(uint64_t& hash, const std::string& str) {
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;
	for (const char character : str) {
		hash ^= static_cast<uint64_t>(static_cast<uint8_t>(character));
		hash *= FNV_PRIME;
	}
}

void FontManager::processHashXMLElement(MyGUI::xml::ElementPtr element, uint64_t& hash) {
	if (!element)
		return;

	this->hashCombineString(hash, element->getName());

	MyGUI::xml::VectorAttributes attributes = element->getAttributes();
	for (const auto& attribute : attributes) {
		this->hashCombineString(hash, attribute.first);
		this->hashCombineString(hash, attribute.second);
	}

	this->hashCombineString(hash, element->getContent());

	MyGUI::xml::ElementEnumerator childElements = element->getElementEnumerator();
	while (childElements.next())
		this->processHashXMLElement(childElements.current(), hash);
}

uint64_t FontManager::generateHashFromXMLElement(MyGUI::xml::ElementPtr element) {
	if (!element)
		return 0;

	uint64_t hash = 14695981039346656037ULL;
	this->processHashXMLElement(element, hash);

	return hash;
}

std::string FontManager::getCacheFilepath(const std::string& fontName, uint64_t hash) {
	return UTILS::formatString("$RGE_CACHE_DATA/Fonts/%s_%016llx.mgfc", fontName.c_str(), hash);
}

bool FontManager::loadFromCache(const std::string& filePath, MyGUI::xml::ElementPtr element, MyGUI::ResourceTrueTypeFont** outputFontResource) {
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
	for (uint32_t index = 0; index < totalCodeRanges; ++index) {
		const MyGUI::Char first = reader.read<MyGUI::Char>();
		const MyGUI::Char second = reader.read<MyGUI::Char>();

		for (uint64_t value = first; value <= second; ++value) {
			const MyGUI::Char codePoint = static_cast<MyGUI::Char>(value);

			MyGUI::GlyphInfo glyphInfo{};
			glyphInfo.codePoint = codePoint;
			glyphInfo.width = reader.read<float>();
			glyphInfo.height = reader.read<float>();
			glyphInfo.advance = reader.read<float>();
			glyphInfo.bearingX = reader.read<float>();
			glyphInfo.bearingY = reader.read<float>();
			glyphInfo.uvRect.left = reader.read<float>();
			glyphInfo.uvRect.top = reader.read<float>();
			glyphInfo.uvRect.right = reader.read<float>();
			glyphInfo.uvRect.bottom = reader.read<float>();

			const FT_UInt glyphIndex = reader.read<FT_UInt>();

			fontResource->mCharMap[codePoint] = glyphIndex;

			// Since MyGUI 3.4 the glyph map is keyed by code point, not glyph index.
			MyGUI::GlyphInfo& storedGlyphInfo = fontResource->mGlyphMap[codePoint];
			storedGlyphInfo = glyphInfo;

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

	char* uncompressedData = new char[uncompressedDataSize];

	int decompressedSize = LZ4_decompress_safe(compressedData.data(), uncompressedData, static_cast<int>(compressedDataSize), static_cast<int>(uncompressedDataSize));
	if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != uncompressedDataSize) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to decompress font data for font: %s", fontName.c_str());

		delete[] uncompressedData;
		texture->destroy();
		return false;
	}

	void* textureData = texture->lock(MyGUI::TextureUsage::Write);
	if (!textureData) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to lock texture for font: %s", fontName.c_str());

		texture->destroy();
		return false;
	}

	memcpy(textureData, uncompressedData, uncompressedDataSize);

	delete[] uncompressedData;

	texture->unlock();

	fontResource->mTexture = texture;

	MyGUI::ResourceManager::getInstance().addResource(fontResource);

	if (outputFontResource)
		*outputFontResource = fontResource;

	return true;
}

bool FontManager::generateCacheFile(const std::string& filePath, const std::string& fontName, MyGUI::ResourceTrueTypeFont* fontResource) {
	if (!fontResource)
		return false;

	RPTexture* texture = dynamic_cast<RPTexture*>(fontResource->getTextureFont());
	if (!texture) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Failed to retrieve texture font for: %s", fontName.c_str());
		return false;
	}

	const rend::Texture2DHandle fontTexture = texture->getTexture();
	if (!fontTexture) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Texture font is not initialized for: %s", fontName.c_str());
		return false;
	}

	const uint32_t width = fontTexture->getWidth();
	const uint32_t height = fontTexture->getHeight();
	if (width == 0 || height == 0) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Invalid texture dimensions for: %s", fontName.c_str());
		return false;
	}

	rend::Texture2DHandle stagingTexture = rend::ResourceManager::getInstance().createTexture2D(width, height, fontTexture->getFormat(), rend::Texture2DFlags::StagingRead);
	if (!stagingTexture || !fontTexture->copyResource(stagingTexture)) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Failed to create staging texture for: %s", fontName.c_str());
		return false;
	}
	stagingTexture->setDebugName("MyGUI::Font \"" + fontName + "\" (Staging)");

	const size_t dataSize = fontTexture->getTotalSize();
	std::vector<uint8_t> pixelData(dataSize);
	if (!stagingTexture->readData(pixelData.data(), static_cast<uint32_t>(pixelData.size()))) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Failed to read back texture data for: %s", fontName.c_str());
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
		CONSOLE_ERROR("FontManager::generateCacheFile: Failed to compress font data: %s", fontName.c_str());
		return false;
	}

	writer.write<size_t>(static_cast<size_t>(compressedSize));
	writer.writeBytes(compressedData.data(), static_cast<size_t>(compressedSize));

	if (!FilesystemManager::getInstance().writeFile(filePath, writer)) {
		CONSOLE_ERROR("FontManager::generateCacheFile: Failed to write cache file: %s", filePath.c_str());
		return false;
	}

	return true;
}

bool FontManager::loadFontXML(const std::string& xmlFilePath) {
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(xmlFilePath, resolvedPath)) {
		CONSOLE_ERROR("FontManager::loadFontXML: Failed to resolve font XML file path: %s", xmlFilePath.c_str());
		return false;
	}

	MyGUI::xml::Document document;
	if (!document.open(resolvedPath.generic_string())) {
		CONSOLE_ERROR("FontManager::loadFontXML: Failed to open font XML file: %s", xmlFilePath.c_str());
		return false;
	}

	MyGUI::xml::ElementPtr root = document.getRoot();
	if (!root) {
		CONSOLE_ERROR("FontManager::loadFontXML: Font XML file has no root element: %s", xmlFilePath.c_str());
		return false;
	}

	if (root->getName() != "MyGUI" || root->findAttribute("type") != "Resource") {
		CONSOLE_ERROR("FontManager::loadFontXML: Font XML file is not a valid MyGUI resource: %s", xmlFilePath.c_str());
		return false;
	}

	MyGUI::xml::ElementEnumerator resourceElement = root->getElementEnumerator();
	while (resourceElement.next()) {
		if (resourceElement->getName() != "Resource")
			continue;

		if (resourceElement->findAttribute("type") != "ResourceTrueTypeFont")
			continue;

		const std::string fontName = resourceElement->findAttribute("name");
		if (fontName.empty()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "FontManager::loadFontXML: Font XML file has a Resource element without a name attribute: %s", xmlFilePath.c_str());
			continue;
		}

		Application::getInstance().splashscreen.setStatus(L"Loading font: " + UTILS::toWideString(fontName));

		MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
		if (resourceManager.isExist(fontName))
			resourceManager.removeByName(fontName);

		const std::string cacheFilePath = this->getCacheFilepath(fontName, this->generateHashFromXMLElement(resourceElement.current()));

		MyGUI::ResourceTrueTypeFont* fontResource = nullptr;
		if (this->loadFromCache(cacheFilePath, resourceElement.current(), &fontResource)) {
			this->loadedFonts_.push_back(fontResource);

			CONSOLE_LOG(LEVEL_RENDER, COLOR_DARK_BLUE, "Loaded font: %s from XML file: %s", fontName.c_str(), xmlFilePath.c_str());
			continue;
		}

		CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "Cache miss! Loading font & generating cache from: %s", fontName.c_str());

		MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();
		fontResource = factoryManager.createObject("Resource", "ResourceTrueTypeFont")->castType<MyGUI::ResourceTrueTypeFont>();
		if (!fontResource) {
			CONSOLE_ERROR("FontManager::loadFontXML: Failed to create ResourceTrueTypeFont object for font: %s", fontName.c_str());
			continue;
		}

		// Hijack properties to fix size. Would've been better if we had our own ResourceTrueTypeFont implementation, but this is easier for now.
		auto elementHijack = resourceElement->getElementEnumerator();

		while (elementHijack.next()) {
			if (elementHijack->getName() == "Property") {
				std::string key;
				elementHijack->findAttribute("key", key);

				if (key == "Size") {
					std::string value;
					elementHijack->findAttribute("value", value);

					if (!value.empty()) {
						float size = std::stof(value);
						size *= 1.33f;

						elementHijack->setAttribute("value", std::to_string(size));
					}

					break;
				}
			}
		}

		static const MyGUI::Version currentVersion(1, 1, 0);
		fontResource->deserialization(resourceElement.current(), currentVersion);

		resourceManager.addResource(fontResource);

		if (!this->generateCacheFile(cacheFilePath, fontName, fontResource))
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to generate cache file for font: %s", fontName.c_str());

		this->loadedFonts_.push_back(fontResource);
		CONSOLE_LOG(LEVEL_RENDER, COLOR_DARK_BLUE, "Loaded font: %s from XML file: %s", fontName.c_str(), xmlFilePath.c_str());
	}

	return true;
}