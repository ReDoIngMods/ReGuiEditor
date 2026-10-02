#pragma once
#include <UTILS/Singleton.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

struct UGCMod {
	std::string localId;
	std::filesystem::path path;
	PublishedFileId_t fileId = 0;

	[[nodiscard]] bool isWorkshop() const { return this->fileId != 0; }
};

class UGCManager : public UTILS::Singleton<UGCManager> {
	std::unordered_map<std::string, UGCMod> mods_;
	std::unordered_map<PublishedFileId_t, std::string> fileIdToLocalId_;
	bool callbacksRegistered_ = false;

	bool loadLocalMods();
	void loadUGCMods();

	bool getLocalIdFromMod(const std::filesystem::path& modPath, std::string& localId) const;
	bool registerMod(const std::filesystem::path& modPath, PublishedFileId_t fileId = 0);
	void registerWorkshopItem(PublishedFileId_t fileId);

	STEAM_CALLBACK_MANUAL(UGCManager, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
	STEAM_CALLBACK_MANUAL(UGCManager, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, itemUnsubscribedCallback_);
public:
	UGCManager() = default;
	~UGCManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	[[nodiscard]] std::optional<UGCMod> getModByLocalId(const std::string& localId) const;
	[[nodiscard]] std::optional<UGCMod> getModByFileId(PublishedFileId_t fileId) const;
};