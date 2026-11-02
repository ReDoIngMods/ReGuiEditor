#include "FontManager.hpp"

#include "FontCache.hpp"

#include <Window/Splashscreen.hpp>

#include <Filesystem/DirectoryManager.hpp>

#include <Modules/Misc/ParallelFor.hpp>

#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_IObject.h>
#include <MyGui/MyGUI_ResourceManager.h>
#include <MyGui/MyGUI_ResourceTrueTypeFont.h>
#include <MyGui/MyGUI_Version.h>
#include <MyGui/MyGUI_XmlDocument.h>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

constexpr float FONT_SIZE_SCALE = 1.66f;

static void scaleFontSize(MyGUI::xml::ElementPtr element) {
	MyGUI::xml::ElementEnumerator properties = element->getElementEnumerator();
	while (properties.next()) {
		if (properties->getName() != "Property")
			continue;

		std::string key;
		if (!properties->findAttribute("key", key) || key != "Size")
			continue;

		std::string value;
		if (properties->findAttribute("value", value) && !value.empty()) {
			const float size = std::stof(value) * FONT_SIZE_SCALE;
			properties->setAttribute("value", std::to_string(size));
		}

		break;
	}
}

static void readCachesParallel(std::vector<FontManager::Job>& jobs) {
	ReGuiEditor::parallelFor(jobs.size(), [&jobs](size_t index) {
		FontManager::Job& job = jobs[index];
		try {
			job.cacheData = FontCache::read(job.cacheFilePath, job.cacheError);
		} catch (const std::exception& exception) {
			job.cacheData.reset();
			job.cacheError = exception.what();
		}
	});
}

void FontManager::loadFont(Job& job, const std::string& xmlFilePath) {
	MyGUI::xml::ElementPtr element = job.element;
	const std::string& fontName = job.fontName;

	SET_SPLASH_STATUS("Loading font: %s", fontName.c_str());

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	if (resourceManager.isExist(fontName))
		resourceManager.removeByName(fontName);

	if (!job.cacheError.empty())
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "%s", job.cacheError.c_str());

	MyGUI::ResourceTrueTypeFont* fontResource = nullptr;
	if (job.cacheData)
		fontResource = FontCache::create(std::move(*job.cacheData), fontName);

	if (fontResource) {
		this->loadedFonts_.push_back(fontResource);

		CONSOLE_LOG(LEVEL_RENDER, COLOR_DARK_BLUE, "Loaded font: %s from XML file: %s", fontName.c_str(), xmlFilePath.c_str());
		return;
	}

	CONSOLE_LOG(LEVEL_CACHE, COLOR_DARK_GRAY, "Cache miss! Loading font & generating cache from: %s", fontName.c_str());

	MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();
	MyGUI::IObject* fontObject = factoryManager.createObject("Resource", "ResourceTrueTypeFont");
	fontResource = fontObject ? fontObject->castType<MyGUI::ResourceTrueTypeFont>() : nullptr;
	if (!fontResource) {
		CONSOLE_ERROR("FontManager::loadFont: Failed to create ResourceTrueTypeFont object for font: %s", fontName.c_str());
		return;
	}

	scaleFontSize(element);

	static const MyGUI::Version currentVersion(1, 1, 0);
	fontResource->deserialization(element, currentVersion);

	resourceManager.addResource(fontResource);

	if (!FontCache::save(job.cacheFilePath, fontName, fontResource))
		CONSOLE_WARNING("Failed to generate cache file for font: %s", fontName.c_str());

	this->loadedFonts_.push_back(fontResource);
	CONSOLE_LOG(LEVEL_RENDER, COLOR_DARK_BLUE, "Loaded font: %s from XML file: %s", fontName.c_str(), xmlFilePath.c_str());
}

bool FontManager::loadFontXML(const std::string& xmlFilePath) {
	if (this->lastFontXMLPath_ == xmlFilePath)
		return true;

	this->clearFonts();

	std::filesystem::path resolvedPath;
	if (!ReGuiEditor::DirectoryManager::getInstance().resolveDirectory(xmlFilePath, resolvedPath)) {
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

	std::vector<Job> jobs;

	MyGUI::xml::ElementEnumerator resourceElement = root->getElementEnumerator();
	while (resourceElement.next()) {
		if (resourceElement->getName() != "Resource")
			continue;

		if (resourceElement->findAttribute("type") != "ResourceTrueTypeFont")
			continue;

		Job& job = jobs.emplace_back();
		job.element = resourceElement.current();
		job.fontName = job.element->findAttribute("name");

		if (job.fontName.empty()) {
			CONSOLE_WARNING("Font XML file has a Resource element without a name attribute: %s", xmlFilePath.c_str());
			jobs.pop_back();
			continue;
		}

		job.cacheFilePath = FontCache::getFilepath(job.fontName, FontCache::hashElement(job.element));
	}

	readCachesParallel(jobs);

	for (Job& job : jobs)
		this->loadFont(job, xmlFilePath);

	return true;
}

void FontManager::clearFonts() {
	for (MyGUI::ResourceTrueTypeFont* font : this->loadedFonts_)
		MyGUI::ResourceManager::getInstance().removeResource(font);

	this->loadedFonts_.clear();
}
