#pragma once
#include "../Service.hpp"

#include "UgcMod.hpp"

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace ReGuiEditor {
	class UgcService : public ReGuiEditor::Service {
		std::vector<std::shared_ptr<ReGuiEditor::UgcMod>> mods_;
		std::shared_mutex modsMutex_;

		void loadUgcModsFromPath(std::filesystem::path directoryPath, bool isLocal);

		void loadWorkshopMods();

		bool steamCallbacksRegistered_ = false;
		STEAM_CALLBACK_MANUAL(UgcService, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
		STEAM_CALLBACK_MANUAL(UgcService, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, itemUnsubscribedCallback_);
	public:
		bool initialize() override;
		void cleanup() override;

		std::shared_ptr<ReGuiEditor::UgcMod> addMod(const std::filesystem::path& modPath, bool isLocal);

		const std::vector<std::shared_ptr<ReGuiEditor::UgcMod>>& getMods();

		const std::shared_ptr<ReGuiEditor::UgcMod> getModByLocalId(const ReGuiEditor::Uuid& localId);
		const std::shared_ptr<ReGuiEditor::UgcMod> getModByFileId(uint64_t fileId);
		const std::shared_ptr<ReGuiEditor::UgcMod> getModByContentPath(const std::string& contentPath);

		const char* getServiceName() const override { return "UgcService"; }
	};
}