#include "DirectoryManager.hpp"

#include <UTILS/EnvVars.hpp>
#include <UTILS/Strings.hpp>
#include <UTILS/Uuid.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

bool DirectoryManager::splitKey(const std::wstring& path, std::wstring& key, std::wstring& rest) const {
	const size_t slash = path.find_first_of(L"/\\");
	if (slash == std::wstring::npos) {
		key = path;
		rest = L"";
		return true;
	}

	key = path.substr(0, slash);
	rest = path.substr(slash + 1);
	return true;
}

bool DirectoryManager::resolveKey(const std::wstring& key, const std::wstring& rest, std::filesystem::path& resolvedPath) const {
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

	this->addDirectory(L"$CHALLENGE_DATA", installPath / "ChallengeData");
	this->addDirectory(L"$GAME_DATA", installPath / "Data");
	this->addDirectory(L"$PARTICLE_EDITOR_DATA", installPath / "ParticleEditorData");
	this->addDirectory(L"$SURVIVAL_DATA", installPath / "Survival");
	this->addDirectory(L"$SHADER_DATA", "../Data/Shaders");

	this->addDirectory(L"$RGE_DATA", "../Data");
	this->addDirectory(L"$RGE_CACHE_DATA", "../Cache");
	this->addDirectory(L"$RGE_USER_DATA", userProfilePath);

	return true;
}

std::optional<std::filesystem::path> DirectoryManager::resolveDirectory(const std::filesystem::path& path) const {
	std::wstring key, rest;
	if (!this->splitKey(path.wstring(), key, rest))
		return std::nullopt;

	std::filesystem::path resolvedPath;
	if (!this->resolveKey(key, rest, resolvedPath))
		return std::nullopt;

	return resolvedPath;
}

std::optional<std::filesystem::path> DirectoryManager::resolveDirectory(const std::filesystem::path& path, const UTILS::UUID localId) const {
	std::filesystem::path resolvedPath = path;
	this->resolveContentData(resolvedPath, localId);

	return this->resolveDirectory(resolvedPath);
}

void DirectoryManager::resolveContentData(std::filesystem::path& path, const UTILS::UUID localId) const {
	constexpr const wchar_t* CONTENT_DATA_PREFIX = L"$CONTENT_DATA";
	constexpr const size_t CONTENT_DATA_PREFIX_LENGTH = std::char_traits<wchar_t>::length(CONTENT_DATA_PREFIX);

	const std::wstring pathStr = path.wstring();
	if (pathStr.compare(0, CONTENT_DATA_PREFIX_LENGTH, CONTENT_DATA_PREFIX) == 0) {
		const std::wstring localIdStr = localId.toWStr();
		const std::wstring newPrefix = L"$CONTENT_" + localIdStr;

		path = newPrefix + pathStr.substr(CONTENT_DATA_PREFIX_LENGTH);
	}
}