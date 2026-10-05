#include "UGCManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

inline static bool getItemInstallPath(PublishedFileId_t fileId, std::filesystem::path& itemPath) {
	char buffer[512];
	if (!SteamUGC()->GetItemInstallInfo(fileId, nullptr, buffer, sizeof(buffer), nullptr))
		return false;

	itemPath = buffer;
	return true;
}

bool UGCManager::getLocalIdFromMod(const std::filesystem::path& modPath, std::string& localId) const {
	std::vector<char> data;
	if (!FilesystemManager::getInstance().readFileRaw(modPath / "description.json", data)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read description.json from %s", modPath.string().c_str());
		return false;
	}

	try {
		localId = nlohmann::json::parse(data, nullptr, true, true).value("localId", "");
	} catch (const nlohmann::json::exception& error) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to parse description.json from %s. Error: %s", modPath.string().c_str(), error.what());
		return false;
	}

	if (localId.empty()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "description.json from %s does not contain a valid localId.", modPath.string().c_str());
		return false;
	}

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

	if (fileId != 0)
		this->fileIdToLocalId_.emplace(fileId, localId);

	this->mods_.emplace(localId, UGCMod{ localId, modPath, fileId });
	return true;
}

void UGCManager::registerWorkshopItem(PublishedFileId_t fileId) {
	std::filesystem::path itemPath;
	if (!getItemInstallPath(fileId, itemPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the installation path for Steam Workshop item: %llu", fileId);
		return;
	}

	this->registerMod(itemPath, fileId);
}

bool UGCManager::loadLocalMods() {
	std::vector<std::string> modDirectories;
	if (!FilesystemManager::getInstance().listDirectories("$RGE_USER_DATA/Mods", modDirectories, FilesystemListFormat::FullPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to list directories in the local mods folder.");
		return false;
	}

	for (const std::string& modDirectory : modDirectories)
		this->registerMod(modDirectory);

	return true;
}

void UGCManager::loadUGCMods() {
	std::vector<PublishedFileId_t> subscribedItems(SteamUGC()->GetNumSubscribedItems());
	subscribedItems.resize(SteamUGC()->GetSubscribedItems(subscribedItems.data(), static_cast<uint32_t>(subscribedItems.size())));

	for (const PublishedFileId_t fileId : subscribedItems)
		this->registerWorkshopItem(fileId);
}

void UGCManager::onItemInstalled(ItemInstalled_t* pCallback) {
	if (pCallback == nullptr) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Received null callback for Steam Workshop item installation.");
		return;
	}

	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item installed: %llu", pCallback->m_nPublishedFileId);
	this->registerWorkshopItem(pCallback->m_nPublishedFileId);
}

void UGCManager::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pCallback) {
	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item unsubscribed: %llu", pCallback->m_nPublishedFileId);

	const auto fileIdIterator = this->fileIdToLocalId_.find(pCallback->m_nPublishedFileId);
	if (fileIdIterator == this->fileIdToLocalId_.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu is not in the local ID map.", pCallback->m_nPublishedFileId);
		return;
	}

	const std::string& localId = fileIdIterator->second;
	if (!FilesystemManager::getInstance().removeDirectory("$CONTENT_" + localId))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu with localId: %s is not in the directories map.", pCallback->m_nPublishedFileId, localId.c_str());

	this->mods_.erase(localId);
	this->fileIdToLocalId_.erase(fileIdIterator);
}

bool UGCManager::initialize() {
	if (!this->loadLocalMods()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load local mods.");
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