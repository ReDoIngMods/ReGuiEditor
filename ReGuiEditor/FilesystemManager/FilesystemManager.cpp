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
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resolve path: %s/%s", key.data(), rest.data());
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
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the game installation path from Steam.");
		return false;
	}

	std::vector<char> installPathBuffer(static_cast<size_t>(installPathLength) + 1, '\0');

	const uint32_t writtenLength = SteamApps()->GetAppInstallDir(appId, installPathBuffer.data(), static_cast<uint32_t>(installPathBuffer.size()));
	if (writtenLength == 0) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the game installation path from Steam.");
		return false;
	}

	installPath = installPathBuffer.data();
	return true;
}

bool FilesystemManager::getUserProfilePath(std::filesystem::path& userProfilePath) {
	std::string appDataPath;
	if (!UTILS::readEnvVar("APPDATA", appDataPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read APPDATA environment variable.");
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
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the game installation path.");
		return false;
	}

	std::filesystem::path userProfilePath;
	if (!this->getUserProfilePath(userProfilePath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get the user profile path.");
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

bool FilesystemManager::readFileRaw(const std::filesystem::path& path, std::vector<char>& outputContent) const {
	const HANDLE fileHandle = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize)) {
		CloseHandle(fileHandle);
		return false;
	}

	if (fileSize.QuadPart == 0) {
		outputContent.clear();

		CloseHandle(fileHandle);
		return true;
	}

	const HANDLE fileMappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (fileMappingHandle == nullptr) {
		CloseHandle(fileHandle);
		return false;
	}

	const LPVOID mappedView = MapViewOfFile(fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
	if (mappedView == nullptr) {
		CloseHandle(fileMappingHandle);
		CloseHandle(fileHandle);

		return false;
	}

	outputContent.resize(static_cast<size_t>(fileSize.QuadPart));
	memcpy(outputContent.data(), mappedView, static_cast<size_t>(fileSize.QuadPart));

	UnmapViewOfFile(mappedView);
	CloseHandle(fileMappingHandle);
	CloseHandle(fileHandle);

	return true;
}

bool FilesystemManager::writeFileRaw(const std::filesystem::path& path, const std::vector<char>& content) const {
	if (!this->createDirectoriesRaw(path.parent_path()))
		return false;

	const HANDLE fileHandle = CreateFileA(path.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER fileSize{};
	fileSize.QuadPart = static_cast<LONGLONG>(content.size());
	if (!SetFilePointerEx(fileHandle, fileSize, nullptr, FILE_BEGIN)) {
		CloseHandle(fileHandle);
		return false;
	}

	if (!SetEndOfFile(fileHandle)) {
		CloseHandle(fileHandle);
		return false;
	}

	if (content.empty()) {
		CloseHandle(fileHandle);
		return true;
	}

	const HANDLE fileMappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
	if (fileMappingHandle == nullptr) {
		CloseHandle(fileHandle);
		return false;
	}

	const LPVOID mappedView = MapViewOfFile(fileMappingHandle, FILE_MAP_WRITE, 0, 0, 0);
	if (mappedView == nullptr) {
		CloseHandle(fileMappingHandle);
		CloseHandle(fileHandle);

		return false;
	}

	memcpy(mappedView, content.data(), content.size());

	UnmapViewOfFile(mappedView);
	CloseHandle(fileMappingHandle);
	CloseHandle(fileHandle);

	return true;
}

bool FilesystemManager::listItems(const std::string& directory, std::vector<std::string>& outputItems, FilesystemListFormat format, bool listFiles) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(directory, resolvedPath))
		return false;

	WIN32_FIND_DATAA findData;
	const HANDLE findHandle = FindFirstFileA((resolvedPath / "*").string().c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
		return false;

	do {
		const bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if (isDirectory == listFiles)
			continue;

		if (!listFiles && (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0))
			continue;

		std::string itemPath;
		switch (format) {
		case FilesystemListFormat::FullPath:
			itemPath = (resolvedPath / findData.cFileName).string();
			break;
		case FilesystemListFormat::FSPath:
			itemPath = directory + "/" + findData.cFileName;
			break;
		case FilesystemListFormat::Name:
		default:
			itemPath = findData.cFileName;
			break;
		}

		outputItems.push_back(std::move(itemPath));
	} while (FindNextFileA(findHandle, &findData));

	FindClose(findHandle);
	return true;
}

bool FilesystemManager::fileExists(const std::string& path) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(path, resolvedPath))
		return false;

	const DWORD attributes = GetFileAttributesA(resolvedPath.string().c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return false;

	return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool FilesystemManager::directoryExists(const std::string& path) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(path, resolvedPath))
		return false;

	const DWORD attributes = GetFileAttributesA(resolvedPath.string().c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return false;

	return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FilesystemManager::doesExist(const std::string& path) const {
	std::filesystem::path resolvedPath;
	if (!this->resolveDirectory(path, resolvedPath))
		return false;

	return GetFileAttributesA(resolvedPath.string().c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool FilesystemManager::getFileWriteTimeRaw(const std::filesystem::path& path, time_t& writeTime) const {
	const HANDLE fileHandle = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE)
		return false;

	FILETIME lastWriteTime;

	const BOOL success = GetFileTime(fileHandle, nullptr, nullptr, &lastWriteTime);
	CloseHandle(fileHandle);

	if (!success)
		return false;

	const uint64_t fileTime64 = (static_cast<uint64_t>(lastWriteTime.dwHighDateTime) << 32) | lastWriteTime.dwLowDateTime;
	writeTime = static_cast<time_t>(fileTime64 / 10000000ULL - 11644473600ULL);

	return true;
}

bool FilesystemManager::createDirectoriesRaw(const std::filesystem::path& path) const {
	if (path.empty())
		return false;

	const std::wstring pathStr = path.wstring();

	DWORD attributes = GetFileAttributesW(pathStr.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	const auto parent = path.parent_path();
	if (!parent.empty() && parent != path) {
		if (!this->createDirectoriesRaw(parent))
			return false;
	}

	if (CreateDirectoryW(pathStr.c_str(), nullptr))
		return true;

	const DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS) {
		attributes = GetFileAttributesW(pathStr.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	return false;
}