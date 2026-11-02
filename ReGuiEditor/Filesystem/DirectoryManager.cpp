#include "DirectoryManager.hpp"

#include <Modules/Misc/Strings.hpp>

#include <Windows.h>

#include <KnownFolders.h>
#include <ShlObj_core.h>
#pragma comment(lib, "shell32.lib")


#include <steam/isteamapps.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

inline static bool getAppInstallPath(std::filesystem::path& installPath) {
	const AppId_t appId = SteamUtils()->GetAppID();

	const uint32_t installPathLength = SteamApps()->GetAppInstallDir(appId, nullptr, 0);
	if (installPathLength == 0) {
		CONSOLE_ERROR("Failed to get the game installation path from Steam.");
		return false;
	}

	std::vector<char> installPathBuffer(static_cast<size_t>(installPathLength) + 1, '\0');

	const uint32_t writtenLength = SteamApps()->GetAppInstallDir(appId, installPathBuffer.data(), static_cast<uint32_t>(installPathBuffer.size()));
	if (writtenLength == 0) {
		CONSOLE_ERROR("Failed to get the game installation path from Steam.");
		return false;
	}

	installPath = installPathBuffer.data();
	return true;
}

inline static bool getUserProfilePath(std::filesystem::path& output) {
	PWSTR userProfilePath = nullptr;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &userProfilePath);
	if (FAILED(result) || userProfilePath == nullptr) {
		CONSOLE_ERROR("Failed to get the user profile path. HRESULT: 0x%08X", result);
		return false;
	}

	output = std::filesystem::path(userProfilePath) / "Axolot Games" / "Scrap Mechanic" / "User" / ReGuiEditor::formatString("User_%llu", SteamUser()->GetSteamID().ConvertToUint64());
	CoTaskMemFree(userProfilePath);

	return true;
}

bool ReGuiEditor::DirectoryManager::splitKey(const std::string& path, std::string& key, std::string& rest) const {
	const size_t slash = path.find_first_of("/\\");
	if (slash == std::string::npos) {
		key = path;
		rest = "";
		return true;
	}

	key = path.substr(0, slash);
	rest = path.substr(slash + 1);
	return true;
}

bool ReGuiEditor::DirectoryManager::resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const {
	std::shared_lock lock(this->mutex_);

	const auto iterator = this->directories_.find(key);
	if (iterator == directories_.end()) {
		CONSOLE_ERROR("Failed to resolve path: %s/%s", key.data(), rest.data());
		return false;
	}

	resolvedPath = iterator->second / rest;
	resolvedPath.make_preferred();

	return true;
}

bool ReGuiEditor::DirectoryManager::initialize() {
	std::filesystem::path installPath;
	if (!getAppInstallPath(installPath)) {
		CONSOLE_ERROR("Failed to get the game installation path.");
		return false;
	}

	std::filesystem::path userProfilePath;
	if (!getUserProfilePath(userProfilePath)) {
		CONSOLE_ERROR("Failed to get the user profile path.");
		return false;
	}

	this->addDirectory("$CHALLENGE_DATA", installPath / "ChallengeData");
	this->addDirectory("$GAME_DATA", installPath / "Data");
	this->addDirectory("$PARTICLE_EDITOR_DATA", installPath / "ParticleEditorData");
	this->addDirectory("$SURVIVAL_DATA", installPath / "Survival");

	this->addDirectory("$RGE_DATA", "../Data");
	this->addDirectory("$RGE_CACHE_DATA", "../Cache");
	this->addDirectory("$RGE_USER_DATA", userProfilePath);

	return true;
}

bool ReGuiEditor::DirectoryManager::addDirectory(const std::string& name, const std::filesystem::path& path) {
	std::unique_lock lock(this->mutex_);
	return this->directories_.try_emplace(name, std::filesystem::absolute(path)).second;
}

bool ReGuiEditor::DirectoryManager::removeDirectory(const std::string& name) {
	std::unique_lock lock(this->mutex_);
	return this->directories_.erase(name) > 0;
}

bool ReGuiEditor::DirectoryManager::resolveDirectory(const std::string& path, std::filesystem::path& resolvedPath) const {
	std::string key;
	std::string rest;
	if (!this->splitKey(path, key, rest)) {
		CONSOLE_ERROR("Failed to split path: %s", path.data());
		return false;
	}

	if (!this->resolveKey(key, rest, resolvedPath)) {
		CONSOLE_ERROR("Failed to resolve path: %s", path.data());
		return false;
	}

	return true;
}

bool ReGuiEditor::DirectoryManager::resolveContentData(std::string& path, const ReGuiEditor::Uuid& localId) const {
	if (path.empty())
		return false;

	std::string key, rest;
	if (!this->splitKey(path, key, rest))
		return false;
	
	if (key != "$CONTENT_DATA")
		return true;

	path = ReGuiEditor::formatString("$CONTENT_%s/%s", localId.toString().c_str(), rest.c_str());
	return true;
}