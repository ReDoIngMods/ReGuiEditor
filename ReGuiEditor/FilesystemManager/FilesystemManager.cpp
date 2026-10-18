#include "FilesystemManager.hpp"

#include <UTILS/EnvVars.hpp>
#include <UTILS/Strings.hpp>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <steam/isteamapps.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

bool FilesystemManager::splitKey(const std::string& path, std::string& key, std::string& rest) const {
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

bool FilesystemManager::resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const {
	const auto iterator = this->directories_.find(key);
	if (iterator == directories_.end()) {
		CONSOLE_ERROR("Failed to resolve path: %s/%s", key.data(), rest.data());
		return false;
	}

	resolvedPath = iterator->second / rest;
	resolvedPath.make_preferred();

	return true;
}

bool FilesystemManager::getAppInstallPath(std::filesystem::path& installPath) {
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

bool FilesystemManager::getUserProfilePath(std::filesystem::path& userProfilePath) {
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

bool FilesystemManager::initialize() {
	std::filesystem::path installPath;
	if (!this->getAppInstallPath(installPath)) {
		CONSOLE_ERROR("Failed to get the game installation path.");
		return false;
	}

	std::filesystem::path userProfilePath;
	if (!this->getUserProfilePath(userProfilePath)) {
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

void FilesystemManager::cleanup() {
	this->directories_.clear();
}

bool FilesystemManager::readFile(const std::string& path, std::vector<uint8_t>& outputData) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(path, resolvedPath)) {
		CONSOLE_ERROR("Failed to resolve path: %s", path.data());
		return false;
	}

	if (!std::filesystem::exists(resolvedPath)) {
		CONSOLE_ERROR("File does not exist: %s", resolvedPath.string().c_str());
		return false;
	}

	HANDLE fileHandle = CreateFileA(resolvedPath.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_ERROR("Failed to open file for reading: %s", resolvedPath.string().c_str());
		return false;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize)) {
		CONSOLE_ERROR("Failed to get file size: %s", resolvedPath.string().c_str());
	
		CloseHandle(fileHandle);
		return false;
	}

	HANDLE mappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!mappingHandle) {
		CONSOLE_ERROR("Failed to create file mapping: %s", resolvedPath.string().c_str());

		CloseHandle(fileHandle);
		return false;
	}

	LPVOID mappedView = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_ERROR("Failed to map view of file: %s", resolvedPath.string().c_str());
		
		CloseHandle(mappingHandle);
		CloseHandle(fileHandle);
		return false;
	}

	outputData.resize(static_cast<size_t>(fileSize.QuadPart));
	outputData.assign(static_cast<uint8_t*>(mappedView), static_cast<uint8_t*>(mappedView) + fileSize.QuadPart);

	UnmapViewOfFile(mappedView);

	CloseHandle(mappingHandle);
	CloseHandle(fileHandle);

	return true;
}

bool FilesystemManager::writeFile(const std::string& path, const std::vector<uint8_t>& data) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(path, resolvedPath)) {
		CONSOLE_ERROR("Failed to resolve path: %s", path.data());
		return false;
	}

	if (!std::filesystem::exists(resolvedPath.parent_path())) {
		if (!std::filesystem::create_directories(resolvedPath.parent_path())) {
			CONSOLE_ERROR("Failed to create directories for path: %s", resolvedPath.parent_path().string().c_str());
			return false;
		}
	}

	HANDLE fileHandle = CreateFileA(resolvedPath.string().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_ERROR("Failed to open file for writing: %s", resolvedPath.string().c_str());
		return false;
	}

	LARGE_INTEGER newSize;
	ZeroMemory(&newSize, sizeof(newSize));
	newSize.QuadPart = static_cast<LONGLONG>(data.size());

	if (!SetFilePointerEx(fileHandle, newSize, nullptr, FILE_BEGIN) || !SetEndOfFile(fileHandle)) {
		CONSOLE_ERROR("Failed to set file size: %s", resolvedPath.string().c_str());

		CloseHandle(fileHandle);
		return false;
	}

	LARGE_INTEGER zeroOffset;
	ZeroMemory(&zeroOffset, sizeof(zeroOffset));
	if (!SetFilePointerEx(fileHandle, zeroOffset, nullptr, FILE_BEGIN)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to rewind file (Win32 error %lu): %s", GetLastError(), resolvedPath.string().c_str());
		CloseHandle(fileHandle);
		return false;
	}

	HANDLE mappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
	if (!mappingHandle) {
		CONSOLE_ERROR("Failed to create file mapping: %s", resolvedPath.string().c_str());
	
		CloseHandle(fileHandle);
		return false;
	}

	LPVOID mappedView = MapViewOfFile(mappingHandle, FILE_MAP_WRITE, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_ERROR("Failed to map view of file: %s", resolvedPath.string().c_str());

		CloseHandle(mappingHandle);
		CloseHandle(fileHandle);
		return false;
	}

	memcpy(mappedView, data.data(), data.size());

	UnmapViewOfFile(mappedView);

	CloseHandle(mappingHandle);
	CloseHandle(fileHandle);
	return true;
}