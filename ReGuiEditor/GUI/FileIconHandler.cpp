#include "FileIconHandler.hpp"
#include <string>

#include <Filesystem/FileReader.hpp>

#include <JSON/JSON.hpp>

bool FileIconHandler::initialize() {
	auto jsonContent = Filesystem::readFileJson("$RGE_DATA/Icons/IconTranslation.json");
	if (!jsonContent) {
		CONSOLE_ERROR("Failed to read or parse file icons configuration.");
		return false;
	}

	if (!jsonContent->isObject()) {
		CONSOLE_ERROR("File icons configuration is not a valid JSON object.");
		return false;
	}

	const JSONObject& jsonObject = jsonContent->asObject();

	const auto folderIconIterator = jsonObject.find("defaultFolderIcon");
	if (folderIconIterator == jsonObject.end() || !folderIconIterator->second.isString()) {
		CONSOLE_ERROR("Missing 'defaultFolderIcon' in file icons configuration or it is not a valid string.");
		return false;
	}

	const auto defaultFileIconIterator = jsonObject.find("defaultFileIcon");
	if (defaultFileIconIterator == jsonObject.end() || !defaultFileIconIterator->second.isString()) {
		CONSOLE_ERROR("Missing 'defaultFileIcon' in file icons configuration or it is not a valid string.");
		return false;
	}

	this->folderIconPath_ = folderIconIterator->second.asString();
	this->defaultFileIconPath_ = defaultFileIconIterator->second.asString();

	const auto entriesIterator = jsonObject.find("entries");
	if (entriesIterator == jsonObject.end() || !entriesIterator->second.isArray()) {
		CONSOLE_ERROR("Missing 'entries' in file icons configuration or it is not a valid array.");
		return false;
	}

	for (const JSONValue& entryValue : entriesIterator->second.asArray()) {
		if (!entryValue.isObject()) {
			CONSOLE_ERROR("Invalid entry in file icons configuration.");
			continue;
		}

		const JSONObject& entryObject = entryValue.asObject();

		const auto iconIterator = entryObject.find("icon");
		if (iconIterator == entryObject.end() || !iconIterator->second.isString()) {
			CONSOLE_ERROR("Missing 'icon' in an entry of file icons configuration or it is not a valid string.");
			continue;
		}

		const auto extensionsIterator = entryObject.find("extensions");
		if (extensionsIterator == entryObject.end() || !extensionsIterator->second.isArray()) {
			CONSOLE_ERROR("Missing 'extensions' in an entry of file icons configuration or it is not a valid array.");
			continue;
		}

		for (const JSONValue& extensionValue : extensionsIterator->second.asArray()) {
			if (!extensionValue.isString()) {
				CONSOLE_ERROR("Invalid extension in an entry of file icons configuration.");
				continue;
			}

			this->extensionToIconMap_[normalizeExtension(extensionValue.asString())] = iconIterator->second.asString();
		}
	}

	return true;
}
