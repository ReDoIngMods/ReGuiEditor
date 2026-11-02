#include "FileIconHandler.hpp"
#include <string>

#include <Filesystem/FilesystemManager.hpp>

#include <Modules/JSON/JSONObject.hpp>

bool FileIconHandler::initialize() {
	ReGuiEditor::JSONValue jsonContent;
	if (!ReGuiEditor::FilesystemManager::getInstance().readFile("$RGE_DATA/Icons/IconTranslation.json", jsonContent)) {
		CONSOLE_ERROR("Failed to read file icons configuration.");
		return false;
	}

	const ReGuiEditor::JSONObject* jsonObject = jsonContent.tryGet<ReGuiEditor::JSONObject>();
	if (!jsonObject) {
		CONSOLE_ERROR("File icons configuration is not a valid JSON object.");
		return false;
	}

	const std::string* folderIconValue = jsonObject->at("defaultFolderIcon").tryGet<std::string>();
	if (!folderIconValue) {
		CONSOLE_ERROR("Missing 'defaultFolderIcon' in file icons configuration or it is not a valid string.");
		return false;
	}

	const std::string* defaultFileIconValue = jsonObject->at("defaultFileIcon").tryGet<std::string>();
	if (!defaultFileIconValue) {
		CONSOLE_ERROR("Missing 'defaultFileIcon' in file icons configuration or it is not a valid string.");
		return false;
	}

	this->folderIconPath_ = *folderIconValue;
	this->defaultFileIconPath_ = *defaultFileIconValue;

	const ReGuiEditor::JSONArray* entriesArray = jsonObject->at("entries").tryGet<ReGuiEditor::JSONArray>();
	if (!entriesArray) {
		CONSOLE_ERROR("Missing 'entries' in file icons configuration or it is not a valid array.");
		return false;
	}

	for (const ReGuiEditor::JSONValue& entryValue : *entriesArray) {
		const ReGuiEditor::JSONObject* entryObject = entryValue.tryGet<ReGuiEditor::JSONObject>();
		if (!entryObject) {
			CONSOLE_ERROR("Invalid entry in file icons configuration.");
			continue;
		}

		const std::string* iconPath = entryObject->at("icon").tryGet<std::string>();
		if (!iconPath) {
			CONSOLE_ERROR("Missing 'icon' in an entry of file icons configuration or it is not a valid string.");
			continue;
		}

		const ReGuiEditor::JSONArray* extensionsArray = entryObject->at("extensions").tryGet<ReGuiEditor::JSONArray>();
		if (!extensionsArray) {
			CONSOLE_ERROR("Missing 'extensions' in an entry of file icons configuration or it is not a valid array.");
			continue;
		}

		for (const ReGuiEditor::JSONValue& extensionValue : *extensionsArray) {
			const std::string* extension = extensionValue.tryGet<std::string>();
			if (!extension) {
				CONSOLE_ERROR("Invalid extension in an entry of file icons configuration.");
				continue;
			}

			this->extensionToIconMap_[normalizeExtension(*extension)] = *iconPath;
		}
	}

	return true;
}