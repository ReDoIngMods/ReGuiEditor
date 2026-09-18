#pragma once
#include <UTILS/Singleton.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

class DirectoryManager : public UTILS::Singleton<DirectoryManager> {
	std::unordered_map<std::string, std::filesystem::path> directories_;
	std::unordered_map<PublishedFileId_t, std::string> fileIdToLocalId_;
	
	bool callbacksRegistered_ = false;

	bool resolveKey(std::string_view key, std::string_view rest, std::filesystem::path& resolvedPath) const {
		auto iterator = directories_.find(std::string(key));
		if (iterator == directories_.end()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Directory key not found: \"%.*s\"", static_cast<int>(key.size()), key.data());
			return false;
		}

		resolvedPath = iterator->second / rest;
		resolvedPath.make_preferred();

		return true;
	}

	bool loadLocalMods();
	bool loadUGCMods();

	bool getLocalIdFromMod(const std::filesystem::path& modPath, std::string& uuid);

	STEAM_CALLBACK_MANUAL(DirectoryManager, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
	STEAM_CALLBACK_MANUAL(DirectoryManager, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, remoteStoragePublishedFileUnsubscribedCallback_);
public:
	DirectoryManager() = default;
	~DirectoryManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	void addDirectory(const std::string& name, const std::filesystem::path& path) {
		this->directories_.try_emplace(name, std::filesystem::absolute(path));
	}

	[[nodiscard]] bool resolveDirectory(std::string_view path, std::filesystem::path& resolvedPath) const {
		if (path.empty())
			return false;

		const size_t slash = path.find('/');

		const std::string_view key = path.substr(0, slash);
		const std::string_view rest = (slash == std::string_view::npos) ? std::string_view{} : path.substr(slash + 1);

		return this->resolveKey(key, rest, resolvedPath);
	}
};