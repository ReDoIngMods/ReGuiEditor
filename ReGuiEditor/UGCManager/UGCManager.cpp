#include "UGCManager.hpp"

#include <Application.hpp>

#include <FilesystemManager/FilesystemManager.hpp>

#include <JSON/JSONObject.hpp>
#include <JSON/JSONParser.hpp>

#include <UTILS/Strings.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

inline static bool getItemInstallPath(PublishedFileId_t fileId, std::filesystem::path& itemPath) {
	char buffer[1024];
	if (!SteamUGC()->GetItemInstallInfo(fileId, nullptr, buffer, sizeof(buffer), nullptr))
		return false;

	itemPath = buffer;
	return true;
}

inline static bool isOwnApp(AppId_t appId) {
	return appId == SteamUtils()->GetAppID();
}

bool UGCManager::getLocalIdFromMod(const std::filesystem::path& modPath, std::string& localId) const {
	std::ifstream file(modPath / "description.json", std::ios::binary);
	if (!file) {
		CONSOLE_ERROR("Failed to read description.json from %s", modPath.string().c_str());
		return false;
	}

	std::vector<char> data;
	data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	JSONValue json;
	if (!JSONParser::parseJson(data.data(), data.size(), json)) {
		CONSOLE_ERROR("Failed to parse description.json from %s", modPath.string().c_str());
		return false;
	}

	const std::string* localIdPtr = json["localId"].tryGet<std::string>();
	if (!localIdPtr || localIdPtr->empty()) {
		CONSOLE_ERROR("description.json from %s does not contain a valid localId.", modPath.string().c_str());
		return false;
	}

	localId = *localIdPtr;
	return true;
}

bool UGCManager::registerMod(const std::filesystem::path& modPath, PublishedFileId_t fileId) {
	std::string localId;
	if (!this->getLocalIdFromMod(modPath, localId))
		return false;

	if (!FilesystemManager::getInstance().addDirectory("$CONTENT_" + localId, modPath)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Mod: %s was already loaded with localId: %s", modPath.string().c_str(), localId.c_str());
		return false;
	}

	Application::getInstance().splashscreen.setStatus(UTILS::formatString(L"Loading mod: %hs (%s)", localId.c_str(), modPath.string().c_str()));

	if (fileId != 0)
		this->fileIdToLocalId_[fileId] = localId;

	this->mods_.emplace(localId, UGCMod{ localId, modPath, fileId });
	return true;
}

void UGCManager::registerWorkshopItem(PublishedFileId_t fileId) {
	if (this->fileIdToLocalId_.contains(fileId))
		return;
	
	std::filesystem::path itemPath;
	if (!getItemInstallPath(fileId, itemPath)) {
		CONSOLE_ERROR("Failed to get the installation path for Steam Workshop item: %llu", static_cast<unsigned long long>(fileId));
		return;
	}

	this->registerMod(itemPath, fileId);
}

bool UGCManager::loadLocalMods() {
	std::filesystem::path modsRoot;
	if (!FilesystemManager::getInstance().resolveDirectory("$RGE_USER_DATA/Mods", modsRoot)) {
		CONSOLE_ERROR("Failed to resolve the local mods folder.");
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(modsRoot, error))
		return true;

	for (std::filesystem::directory_iterator iterator(modsRoot, error), end; !error && iterator != end; iterator.increment(error)) {
		if (iterator->is_directory(error))
			this->registerMod(iterator->path());
	}

	if (error) {
		CONSOLE_ERROR("Failed to list the local mods folder: %s", error.message().c_str());
		return false;
	}

	return true;
}

void UGCManager::loadUGCMods() {
	std::vector<PublishedFileId_t> subscribedItems(SteamUGC()->GetNumSubscribedItems());
	subscribedItems.resize(SteamUGC()->GetSubscribedItems(subscribedItems.data(), static_cast<uint32_t>(subscribedItems.size())));

	for (const PublishedFileId_t fileId : subscribedItems)
		this->registerWorkshopItem(fileId);
}

void UGCManager::onItemInstalled(ItemInstalled_t* pCallback) {
	if (pCallback == nullptr || !isOwnApp(pCallback->m_unAppID))
		return;

	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item installed: %llu", static_cast<unsigned long long>(pCallback->m_nPublishedFileId));
	this->registerWorkshopItem(pCallback->m_nPublishedFileId);
}

void UGCManager::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pCallback) {
	if (pCallback == nullptr || !isOwnApp(pCallback->m_nAppID))
		return;

	const unsigned long long fileId = pCallback->m_nPublishedFileId;
	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item unsubscribed: %llu", fileId);

	const auto fileIdIterator = this->fileIdToLocalId_.find(pCallback->m_nPublishedFileId);
	if (fileIdIterator == this->fileIdToLocalId_.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu is not in the local ID map.", fileId);
		return;
	}

	const std::string& localId = fileIdIterator->second;
	if (!FilesystemManager::getInstance().removeDirectory("$CONTENT_" + localId))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu with localId: %s is not in the directories map.", fileId, localId.c_str());

	this->mods_.erase(localId);
	this->fileIdToLocalId_.erase(fileIdIterator);
}

bool UGCManager::initialize() {
	if (this->callbacksRegistered_)
		return true;

	if (!this->loadLocalMods()) {
		CONSOLE_ERROR("Failed to load local mods.");
		return false;
	}

	this->loadUGCMods();

	this->itemInstalledCallback_.Register(this, &UGCManager::onItemInstalled);
	this->itemUnsubscribedCallback_.Register(this, &UGCManager::onItemUnsubscribed);
	this->callbacksRegistered_ = true;

	return true;
}

void UGCManager::cleanup() {
	if (this->callbacksRegistered_) {
		this->itemInstalledCallback_.Unregister();
		this->itemUnsubscribedCallback_.Unregister();
		this->callbacksRegistered_ = false;
	}

	FilesystemManager& filesystem = FilesystemManager::getInstance();
	for (const auto& [localId, mod] : this->mods_)
		filesystem.removeDirectory("$CONTENT_" + localId);

	this->fileIdToLocalId_.clear();
	this->mods_.clear();
}

std::optional<UGCMod> UGCManager::getModByLocalId(const std::string& localId) const {
	const auto iterator = this->mods_.find(localId);
	if (iterator == this->mods_.end())
		return std::nullopt;

	return iterator->second;
}

std::optional<UGCMod> UGCManager::getModByFileId(PublishedFileId_t fileId) const {
	const auto iterator = this->fileIdToLocalId_.find(fileId);
	if (iterator == this->fileIdToLocalId_.end())
		return std::nullopt;

	return this->getModByLocalId(iterator->second);
}