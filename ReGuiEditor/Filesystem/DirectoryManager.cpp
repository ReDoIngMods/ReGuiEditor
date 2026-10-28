#include "DirectoryManager.hpp"

#include <UTILS/EnvVars.hpp>
#include <UTILS/Strings.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <steam/isteamapps.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

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

inline static bool getUserProfilePath(std::filesystem::path& userProfilePath) {
	std::string appDataPath;
	if (!UTILS::readEnvVar("APPDATA", appDataPath)) {
		CONSOLE_ERROR("Failed to read APPDATA environment variable.");
		return false;
	}

	const uint64_t steamId = SteamUser()->GetSteamID().ConvertToUint64();
	userProfilePath = UTILS::formatString("%s\\Axolot Games\\Scrap Mechanic\\User\\User_%llu", appDataPath.c_str(), steamId);
	userProfilePath.make_preferred();

	return true;
}

bool DirectoryManager::splitKey(const std::string& path, std::string& key, std::string& rest) const {
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

bool DirectoryManager::resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const {
	const auto iterator = this->directories_.find(key);
	if (iterator == directories_.end()) {
		CONSOLE_ERROR("Failed to resolve path: %s/%s", key.data(), rest.data());
		return false;
	}

	resolvedPath = iterator->second / rest;
	resolvedPath.make_preferred();

	return true;
}

bool DirectoryManager::initialize() {
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