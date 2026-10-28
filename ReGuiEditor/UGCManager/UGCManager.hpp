#pragma once
#include "UGCMod.hpp"

#include <UTILS/Singleton.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class UGCManager : public UTILS::Singleton<UGCManager> {
	std::vector<std::shared_ptr<UGCMod>> mods_;

	std::vector<std::shared_ptr<UGCMod>> workshopMods_;
	std::vector<std::shared_ptr<UGCMod>> localMods_;

	std::unordered_map<PublishedFileId_t, std::string> fileIdToLocalId_;
	bool callbacksRegistered_ = false;

	bool loadLocalMods();
	void loadUGCMods();

	bool registerMod(const std::filesystem::path& modPath, bool isLocal);
	void registerWorkshopItem(PublishedFileId_t fileId);

	STEAM_CALLBACK_MANUAL(UGCManager, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
	STEAM_CALLBACK_MANUAL(UGCManager, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, itemUnsubscribedCallback_);
public:
	UGCManager() = default;
	~UGCManager() { this->cleanup(); }

	[[nodiscard]] bool initialize();
	void cleanup();

	[[nodiscard]] std::shared_ptr<UGCMod> getModByLocalId(const std::string& localId) const;
	[[nodiscard]] std::shared_ptr<UGCMod> getModByFileId(PublishedFileId_t fileId) const;

	[[nodiscard]] const std::vector<std::shared_ptr<UGCMod>>& getAllMods() const { return this->mods_; }
	[[nodiscard]] const std::vector<std::shared_ptr<UGCMod>>& getWorkshopMods() const { return this->workshopMods_; }
	[[nodiscard]] const std::vector<std::shared_ptr<UGCMod>>& getLocalMods() const { return this->localMods_; }
};