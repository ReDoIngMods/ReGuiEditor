#include "FilesystemManager.hpp"

#include "DirectoryManager.hpp"

#include <Windows.h>
#pragma comment(lib, "shell32.lib")

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

inline static std::optional<time_t> getFileWriteTime(const std::filesystem::path& path) {
	std::error_code errorCode;

	auto ftime = std::filesystem::last_write_time(path, errorCode);
	if (errorCode) {
		CONSOLE_ERROR("Failed to get last write time for file: %s, error: %s", path.string().c_str(), errorCode.message().c_str());
		return std::nullopt;
	}

	return ftime.time_since_epoch().count() / 10000000 - 11644473600; // Convert to time_t
}

inline static void updateEntryData(FilesystemListEntry& entry, const std::filesystem::path& path) {
	auto writeTimeOpt = getFileWriteTime(path);
	if (!writeTimeOpt) {
		entry.writeTime = 0;
		entry.formattedWriteTime = "N/A";
	} else {
		entry.writeTime = writeTimeOpt.value();

		std::tm* tm = std::localtime(&entry.writeTime);

		char buffer[64];
		if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm))
			entry.formattedWriteTime = buffer;
		else
			entry.formattedWriteTime = "N/A";
	}

	SHFILEINFOA shFileInfo;
	ZeroMemory(&shFileInfo, sizeof(shFileInfo));

	if (SHGetFileInfoA(path.string().c_str(), FILE_ATTRIBUTE_NORMAL, &shFileInfo, sizeof(shFileInfo), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES) == 0)
		entry.fileTypename = path.extension().string();
	else
		entry.fileTypename = shFileInfo.szTypeName;

	if (std::filesystem::is_regular_file(path)) {
		std::error_code errorCode;
		entry.fileSize = std::filesystem::file_size(path, errorCode);

		if (errorCode) {
			CONSOLE_ERROR("Failed to get file size for: %s, error: %s", path.string().c_str(), errorCode.message().c_str());
			entry.fileSize = 0;
		}
	} else {
		entry.fileSize = 0;
	}
}

bool FilesystemManager::readFile(const std::string& path, std::vector<uint8_t>& outputData) const {
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
		return false;

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
	if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
		return false;

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

bool FilesystemManager::exists(const std::string& path) const {
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
		return false;
	
	return std::filesystem::exists(resolvedPath);
}

bool FilesystemManager::getFileWriteTime(const std::string& path, time_t& writeTime) const {
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
		return false;
	
	std::error_code errorCode;
	std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(resolvedPath, errorCode);
	if (errorCode) {
		CONSOLE_ERROR("Failed to get file write time for path: %s. Error: %s", resolvedPath.string().c_str(), errorCode.message().c_str());
		return false;
	}

	writeTime = fileTime.time_since_epoch().count() / 10000000 - 11644473600;
	return true;
}

std::optional<std::vector<FilesystemListEntry>> FilesystemManager::listDirectory(const std::string& directory, bool includeFiles, bool includeDirectories) const {
	if (!includeFiles && !includeDirectories)
		return {};
	
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(directory, resolvedPath))
		return std::nullopt;

	if (!std::filesystem::exists(resolvedPath) || !std::filesystem::is_directory(resolvedPath)) {
		CONSOLE_ERROR("Directory does not exist or is not a directory: %s", resolvedPath.string().c_str());
		return std::nullopt;
	}

	std::vector<FilesystemListEntry> entries;
	for (const auto& entry : std::filesystem::directory_iterator(resolvedPath)) {
		if ((!includeFiles && !entry.is_directory()) || (!includeDirectories && entry.is_directory()))
			continue;

		FilesystemListEntry listEntry;
		listEntry.name = entry.path().filename().string();
		listEntry.isDirectory = entry.is_directory();
		updateEntryData(listEntry, entry.path());

		entries.push_back(std::move(listEntry));
	}

	std::sort(entries.begin(), entries.end(), [](const FilesystemListEntry& a, const FilesystemListEntry& b) -> bool {
		if (a.isDirectory != b.isDirectory)
			return a.isDirectory;

		const int comparison = _stricmp(a.name.c_str(), b.name.c_str());
		if (comparison != 0)
			return comparison < 0;

		return a.name < b.name;
	});

	return entries;
}