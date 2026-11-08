#pragma once
#include "UGCMod.hpp"

#include <UTILS/Singleton.hpp>
#include <UTILS/Uuid.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

class UGCManager : public UTILS::Singleton<UGCManager> {
	std::vector<std::shared_ptr<UgcMod>> mods_;
	std::shared_mutex modsMutex_;

	void loadUgcModsFromPath(std::filesystem::path directoryPath, bool isLocal);

	void loadWorkshopMods();

	bool steamCallbacksRegistered_ = false;
	STEAM_CALLBACK_MANUAL(UGCManager, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
	STEAM_CALLBACK_MANUAL(UGCManager, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, itemUnsubscribedCallback_);
public:
	UGCManager() = default;
	~UGCManager() { this->cleanup(); }

	[[nodiscard]] bool initialize();
	void cleanup();

	std::shared_ptr<UgcMod> addMod(const std::filesystem::path& modPath, bool isLocal);

	const std::vector<std::shared_ptr<UgcMod>>& getMods();

	const std::shared_ptr<UgcMod> getModByLocalId(const UTILS::UUID& localId);
	const std::shared_ptr<UgcMod> getModByFileId(uint64_t fileId);
	const std::shared_ptr<UgcMod> getModByContentPath(const std::string& contentPath);
};