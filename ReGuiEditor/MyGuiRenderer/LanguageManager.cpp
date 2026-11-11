#include "LanguageManager.hpp"

#include <Filesystem/DirectoryListing.hpp>
#include <Filesystem/FileReader.hpp>

#include <JSON/JSON.hpp>

#include <UTILS/Strings.hpp>

#include <MyGui/MyGUI_LanguageManager.h>

#include <string>
#include <utility>
#include <vector>

bool LanguageManager::initialize(const std::string& defaultLanguage) {
	auto jsonData = Filesystem::readFileJson("$GAME_DATA/Gui/Language/languages.json");
	if (!jsonData) {
		CONSOLE_ERROR("Failed to read or parse languages.json");
		return false;
	}

	if (!jsonData->isObject()) {
		CONSOLE_ERROR("Invalid format in languages.json: root is not an object.");
		return false;
	}

	for (const auto& [languageKey, languageValue] : jsonData->asObject()) {
		if (!languageValue.isString()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid language entry in languages.json: value is not a string for key '%s'", languageKey.c_str());
			continue;
		}

		LanguageEntry languageEntry;
		languageEntry.name = languageKey;
		languageEntry.folderName = languageValue.asString();

		const std::string languageFolderPath = UTILS::formatString("$GAME_DATA/Gui/Language/%s", languageEntry.folderName.c_str());

		auto entries = Filesystem::listDirectory(languageFolderPath, true, false);
		if (!entries.has_value()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to list directory for language: %s", languageFolderPath.c_str());
			continue;
		}

		for (const auto& entry : entries.value()) {
			if (entry.name.find("InterfaceTags") == std::string::npos)
				continue;

			if (entry.name.size() < 4 || entry.name.substr(entry.name.size() - 4) != ".txt")
				continue;

			languageEntry.interfaceTagFiles.push_back(UTILS::formatString("%s/%s", languageFolderPath.c_str(), entry.name.c_str()));
		}

		this->availableLanguages_.push_back(std::move(languageEntry));
	}

	this->setCurrentLanguage(defaultLanguage);
	return true;
}

void LanguageManager::setCurrentLanguage(const std::string& language) {
	const LanguageEntry* languageEntry = nullptr;
	for (const auto& entry : this->availableLanguages_) {
		if (entry.name == language || entry.folderName == language) {
			languageEntry = &entry;
			break;
		}
	}

	if (!languageEntry) {
		CONSOLE_ERROR("Language '%s' is not available.", language.c_str());
		return;
	}

	this->currentLanguage_ = languageEntry->name;

	MyGUI::LanguageManager& languageManager = MyGUI::LanguageManager::getInstance();
	languageManager.clearUserTags();

	for (const auto& tagFile : languageEntry->interfaceTagFiles) {
		if (!languageManager.loadUserTags(tagFile))
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to load user tags from file: %s", tagFile.c_str());
	}
}