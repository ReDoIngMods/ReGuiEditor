#include "ResolutionManager.hpp"

#include "DataManager.hpp"

#include <Application.hpp>

#include <Filesystem/FileReader.hpp>

#include <UTILS/Strings.hpp>

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_ResourceManager.h>

#include <JSON/JSON.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

bool ResolutionManager::initialize() {
	auto jsonContent = Filesystem::readFileJson("$GAME_DATA/Gui/guiResolutions.json");
	if (!jsonContent) {
		CONSOLE_ERROR("Failed to read or parse Resolutions.json file.");
		return false;
	}

	if (!jsonContent->isObject()) {
		CONSOLE_ERROR("Invalid format in Resolutions.json: root is not an object.");
		return false;
	}

	const JSONObject& jsonData = jsonContent->asObject();

	const auto resolutionListIterator = jsonData.find("resolutionList");
	if (resolutionListIterator == jsonData.end() || !resolutionListIterator->second.isArray()) {
		CONSOLE_ERROR("Invalid format in Resolutions.json: 'resolutionList' is not an array.");
		return false;
	}

	const JSONArray& resolutionList = resolutionListIterator->second.asArray();

	this->resolutions_.clear();
	this->resolutions_.reserve(resolutionList.size());

	for (const JSONValue& resolutionEntry : resolutionList) {
		if (!resolutionEntry.isObject()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resolution entry in Resolutions.json: not an object.");
			continue;
		}

		const JSONObject& resolutionObject = resolutionEntry.asObject();

		int width = resolutionObject.contains("w") ? resolutionObject.at("w").getInteger().value_or(0) : 0;
		int height = resolutionObject.contains("h") ? resolutionObject.at("h").getInteger().value_or(0) : 0;
		std::string folder = resolutionObject.contains("folder") ? resolutionObject.at("folder").getString().value_or("") : "";
		if (width == 0 || height == 0 || folder.empty()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resolution entry in Resolutions.json: missing or invalid fields.");
			continue;
		}

		this->resolutions_.emplace_back(width, height, folder);
	}

	const auto resourceListIterator = jsonData.find("resources");
	if (resourceListIterator == jsonData.end() || !resourceListIterator->second.isArray()) {
		CONSOLE_ERROR("Invalid format in Resolutions.json: 'resources' is not an array.");
		return false;
	}

	const JSONArray& resourceList = resourceListIterator->second.asArray();

	this->resources_.clear();
	this->resources_.reserve(resourceList.size());

	for (const JSONValue& resourceEntry : resourceList) {
		if (!resourceEntry.isString()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Skipping invalid resource entry in Resolutions.json: not a string.");
			continue;
		}
	
		this->resources_.push_back(resourceEntry.asString());
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

    if (this->resolution_ == bestResolution)
        return true;

    DataManager* dataManager = dynamic_cast<DataManager*>(MyGUI::DataManager::getInstancePtr());
    if (!dataManager) {
        CONSOLE_ERROR("Failed to get DataManager instance.");
        return false;
    }

    MyGUI::ResourceManager& resourceManager = MyGUI::ResourceManager::getInstance();
    for (const auto& resource : this->loadedResources)
        resourceManager.removeResource(resource);
    
    this->loadedResources.clear();

    if (this->resolution_)
        dataManager->removeSearchDirectory(this->resolution_->folder.generic_string());

    this->resolution_ = bestResolution;
    const std::string newFolderStr = this->resolution_->folder.generic_string();
    dataManager->addSearchDirectory(newFolderStr, true);

    std::unordered_set<std::string> oldResourceNames;
    {
        MyGUI::ResourceManager::EnumeratorPtr enumerator = resourceManager.getEnumerator();
        while (enumerator.next()) {
            oldResourceNames.insert(enumerator.current().first);
        }
    }

    this->loadedResources.reserve(this->resources_.size());

    for (const auto& resourcePath : this->resources_) {
        const std::string fullPath = (this->resolution_->folder / resourcePath).generic_string();

        Application::getInstance().splashscreen.setStatus(L"Loading resource: " + UTILS::toWideString(resourcePath));

        if (!resourceManager.load(fullPath)) {
            CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to load resource: %s", fullPath.c_str());
            continue;
        }

        CONSOLE_LOG(LEVEL_RENDER, COLOR_BLUE, "Loaded resource: %s", fullPath.c_str());
    }

    {
        MyGUI::ResourceManager::EnumeratorPtr enumerator = resourceManager.getEnumerator();
        while (enumerator.next()) {
            const auto& [name, resource] = enumerator.current();

            if (oldResourceNames.find(name) == oldResourceNames.end()) {
                this->loadedResources.push_back(resource);
            }
        }
    }

    return true;
}