#include "DirectoryManager.hpp"

#include "../Filesystem/Filesystem.hpp"

#include <UTILS/EnvVars.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <steam/isteamapps.h>
#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

bool DirectoryManager::loadLocalMods() {
	uint64_t steamId = SteamUser()->GetSteamID().ConvertToUint64();

	std::string appDataPath;
	if (!UTILS::readEnvVar("APPDATA", appDataPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read APPDATA environment variable.");
		return false;
	}

	const std::string userFolderName = "User_" + std::to_string(steamId);
	const std::filesystem::path modsPath = std::filesystem::path(appDataPath) / "Axolot Games" / "Scrap Mechanic" / "User" / userFolderName / "Mods";

	if (!FS::directoryExists(modsPath)) {
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_WHITE, "Local mods folder does not exist, skipping: %s", modsPath.string().c_str());
		return true;
	}

	std::error_code iterationError;
	for (const auto& entry : std::filesystem::directory_iterator(modsPath, iterationError)) {
		if (!entry.is_directory())
			continue;

		const std::filesystem::path modPath = entry.path();

		std::string localId;
		if (!this->getLocalIdFromMod(modPath, localId)) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get localId for local mod: %s", modPath.string().c_str());
			continue;
		}

		const std::string key = "$CONTENT_" + localId;

		auto [iterator, inserted] = this->directories_.emplace(key, modPath);
		if (!inserted) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Local mod: %s was already loaded with localId: %s", modPath.string().c_str(), localId.c_str());
			continue;
		}
	}

	if (iterationError) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to iterate local mods folder: %s. Error: %s", modsPath.string().c_str(), iterationError.message().c_str());
		return false;
	}

	return true;
}

bool DirectoryManager::loadUGCMods() {
	uint32_t numSubscribedItems = SteamUGC()->GetNumSubscribedItems();
	std::vector<PublishedFileId_t> subscribedItems(numSubscribedItems);
	SteamUGC()->GetSubscribedItems(subscribedItems.data(), numSubscribedItems);

	for (const PublishedFileId_t& fileId : subscribedItems) {
		char filePath[512];
		if (!SteamUGC()->GetItemInstallInfo(fileId, nullptr, filePath, sizeof(filePath), nullptr)) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get the installation path for Steam Workshop item: %llu", fileId);
			continue;
		}

		const std::filesystem::path itemPath(filePath);

		std::string localId;
		if (!this->getLocalIdFromMod(itemPath, localId)) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get localId for Steam Workshop item: %llu", fileId);
			continue;
		}

		const std::string key = "$CONTENT_" + localId;

		auto [iterator, inserted] = this->directories_.emplace(key, itemPath);
		if (!inserted) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu was already loaded with localId: %s", fileId, localId.c_str());
			continue;
		}

		this->fileIdToLocalId_[fileId] = localId;
	}

	return true;
}

bool DirectoryManager::getLocalIdFromMod(const std::filesystem::path& modPath, std::string& uuid) {
	const std::filesystem::path descriptionJsonPath = modPath / "description.json";

	std::string descriptionJsonData;
	if (!FS::readFile(descriptionJsonPath, descriptionJsonData)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read description.json from %s", modPath.string().c_str());
		return false;
	}

	nlohmann::json descriptionJson;
	try {
		descriptionJson = nlohmann::json::parse(descriptionJsonData, nullptr, true, true);
	} catch (const nlohmann::json::parse_error& e) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to parse description.json from %s. Error: %s", modPath.string().c_str(), e.what());
		return false;
	}

	nlohmann::json::json_pointer localIdPointer("/localId");
	if (!descriptionJson.contains(localIdPointer) || !descriptionJson[localIdPointer].is_string()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "description.json for Steam Workshop item: %s does not contain a valid localId.", modPath.string().c_str());
		return false;
	}

	uuid = descriptionJson[localIdPointer].get<std::string>();
	return true;
}

void DirectoryManager::onItemInstalled(ItemInstalled_t* pCallback) {
	if (pCallback == nullptr) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Received null callback for Steam Workshop item installation.");
		return;
	}

	if (pCallback->m_unAppID != SteamUtils()->GetAppID())
		return;

	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item installed: %llu", pCallback->m_nPublishedFileId);

	char filePath[512];
	if (!SteamUGC()->GetItemInstallInfo(pCallback->m_nPublishedFileId, nullptr, filePath, sizeof(filePath), nullptr)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the installation path for Steam Workshop item: %llu", pCallback->m_nPublishedFileId);
		return;
	}

	const std::filesystem::path itemPath(filePath);

	std::string localId;
	if (!this->getLocalIdFromMod(itemPath, localId)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get localId for installed Steam Workshop item: %llu", pCallback->m_nPublishedFileId);
		return;
	}

	const std::string key = "$CONTENT_" + localId;

	auto [iterator, inserted] = this->directories_.emplace(key, itemPath);
	if (!inserted) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu was already installed with localId: %s", pCallback->m_nPublishedFileId, localId.c_str());
		return;
	}

	this->fileIdToLocalId_[pCallback->m_nPublishedFileId] = localId;
}

void DirectoryManager::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pCallback) {
	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item unsubscribed: %llu", pCallback->m_nPublishedFileId);

	auto iterator = this->fileIdToLocalId_.find(pCallback->m_nPublishedFileId);
	if (iterator == this->fileIdToLocalId_.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu is not in the local ID map.", pCallback->m_nPublishedFileId);
		return;
	}

	const std::string localId = iterator->second;
	this->fileIdToLocalId_.erase(iterator);

	const std::string key = "$CONTENT_" + localId;

	auto directoryIterator = this->directories_.find(key);
	if (directoryIterator != this->directories_.end())
		this->directories_.erase(directoryIterator);
	else
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Steam Workshop item: %llu with localId: %s is not in the directories map.", pCallback->m_nPublishedFileId, localId.c_str());
}

bool DirectoryManager::initialize() {
	const AppId_t appId = SteamUtils()->GetAppID();

	uint32_t installPathLength = SteamApps()->GetAppInstallDir(appId, nullptr, 0);
	if (installPathLength == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the game installation path from Steam.");
		return false;
	}

	std::vector<char> installPathBuffer(static_cast<size_t>(installPathLength) + 1, '\0');
	const uint32_t writtenLength = SteamApps()->GetAppInstallDir(appId, installPathBuffer.data(), static_cast<uint32_t>(installPathBuffer.size()));
	if (writtenLength == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the game installation path from Steam.");
		return false;
	}

	const std::filesystem::path installPath(installPathBuffer.data());

	this->addDirectory("$CHALLENGE_DATA", installPath / "ChallengeData");
	this->addDirectory("$GAME_DATA", installPath / "Data");
	this->addDirectory("$PARTICLE_EDITOR_DATA", installPath / "ParticleEditorData");
	this->addDirectory("$SURVIVAL_DATA", installPath / "Survival");

	this->addDirectory("$RGE_DATA", "../Data");
	this->addDirectory("$RGE_CACHE_DATA", "../Cache");

	if (!this->loadLocalMods()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load local mods.");
		return false;
	}

	if (!this->loadUGCMods()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load UGC mods.");
		return false;
	}

	this->itemInstalledCallback_.Register(this, &DirectoryManager::onItemInstalled);
	this->remoteStoragePublishedFileUnsubscribedCallback_.Register(this, &DirectoryManager::onItemUnsubscribed);
	this->callbacksRegistered_ = true;

	return true;
}

void DirectoryManager::cleanup() {
	if (this->callbacksRegistered_) {
		this->itemInstalledCallback_.Unregister();
		this->remoteStoragePublishedFileUnsubscribedCallback_.Unregister();

		this->callbacksRegistered_ = false;
	}

	this->directories_.clear();
	this->fileIdToLocalId_.clear();
}