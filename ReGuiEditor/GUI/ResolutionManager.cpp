#include "ResolutionManager.hpp"

#include "../Filesystem/Filesystem.hpp"

#include "MyGui/MyGuiDataManager.hpp"

#include <MyGui/MyGUI_ResourceManager.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <MyGui/MyGUI_DataManager.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

bool ResolutionManager::initialize() {
	std::string jsonContent;
	if (!FS::readFileDM("$GAME_DATA/Gui/guiResolutions.json", jsonContent)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read Resolutions.json file.");
		return false;
	}

	nlohmann::json jsonData;
	try {
		jsonData = nlohmann::json::parse(jsonContent);
	} catch (const nlohmann::json::parse_error& error) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to parse Resolutions.json file: %s", error.what());
		return false;
	}

	nlohmann::json resolutionList = jsonData["resolutionList"];
	if (!resolutionList.is_array()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid format in Resolutions.json: 'resolutionList' is not an array.");
		return false;
	}

	for (const auto& resolutionEntry : resolutionList) {
		if (!resolutionEntry.is_object()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resolution entry in Resolutions.json: not an object.");
			continue;
		}

		uint32_t width = resolutionEntry.value("w", 0);
		uint32_t height = resolutionEntry.value("h", 0);
		std::string folder = resolutionEntry.value("folder", "");
		if (width == 0 || height == 0 || folder.empty()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resolution entry in Resolutions.json: missing or invalid fields.");
			continue;
		}

		this->resolutions_.push_back({ width, height, folder });
	}

	nlohmann::json resourceList = jsonData["resources"];
	if (!resourceList.is_array()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid format in Resolutions.json: 'resources' is not an array.");
		return false;
	}

	for (const auto& resourceEntry : resourceList) {
		if (!resourceEntry.is_string()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resource entry in Resolutions.json: not a string.");
			continue;
		}

		this->resources_.push_back(resourceEntry.get<std::string>());
	}

	return true;
}

bool ResolutionManager::setResolution(uint32_t width, uint32_t height) {
	const Resolution* bestResolution = nullptr;
	for (const auto& resolution : this->resolutions_) {
		if (resolution.width <= width && resolution.height <= height) {
			if (!bestResolution || resolution.width > bestResolution->width || resolution.height > bestResolution->height)
				bestResolution = &resolution;
		}
	}

	if (bestResolution == nullptr) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "No suitable resolution found for: %ux%u", width, height);
		return false;
	}

	if (this->resolution_) {
		if (bestResolution->width == this->resolution_->width && bestResolution->height == this->resolution_->height)
			return true;
	}

	MyGuiDataManager* dataManager = dynamic_cast<MyGuiDataManager*>(MyGUI::DataManager::getInstancePtr());
	if (!dataManager) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get MyGuiDataManager instance.");
		return false;
	}

	MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
	for (const auto& resource : this->loadedResources)
		resourceManager.removeResource(resource);

	this->loadedResources.clear();

	if (this->resolution_)
		dataManager->removeSearchDirectory(this->resolution_->folder.generic_string());

	this->resolution_ = bestResolution;
	dataManager->addSearchDirectory(this->resolution_->folder.generic_string(), true);

	std::vector<std::string> oldResourceNames;
	{
		MyGUI::ResourceManager::EnumeratorPtr enumerator = resourceManager.getEnumerator();
		while (enumerator.next())
			oldResourceNames.push_back(enumerator.current().first);
	}

	for (const auto& resourcePath : this->resources_) {
		const std::string fullPath = (this->resolution_->folder / resourcePath).generic_string();
		if(!resourceManager.load(fullPath)) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to load resource: %s", fullPath.c_str());
			continue;
		}

		CONSOLE_LOG(LEVEL_RENDER, COLOR_BLUE, "Loaded resource: %s", fullPath.c_str());
	}

	MyGUI::ResourceManager::EnumeratorPtr enumerator = resourceManager.getEnumerator();
	while (enumerator.next()) {
		const std::string& resourceName = enumerator.current().first;
		if (std::find(oldResourceNames.begin(), oldResourceNames.end(), resourceName) == oldResourceNames.end()) {
			const auto& [name, resource] = enumerator.current();

			this->loadedResources.push_back(resource);
		}
	}
	
	return true;
}
