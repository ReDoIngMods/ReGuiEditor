#include "DirectoryListing.hpp"

#include "DirectoryManager.hpp"

#include <Windows.h>
#pragma comment(lib, "shell32.lib")

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

inline static std::optional<time_t> getFileWriteTime(const std::filesystem::path& path) {
	std::error_code ec;

	auto ftime = std::filesystem::last_write_time(path, ec);
	if (ec) {
		CONSOLE_ERROR("Failed to get last write time for file: %s, error: %s", path.string().c_str(), ec.message().c_str());
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
		if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm))
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
		std::error_code ec;
		entry.fileSize = std::filesystem::file_size(path, ec);

		if (ec) {
			CONSOLE_ERROR("Failed to get file size for: %s, error: %s", path.string().c_str(), ec.message().c_str());
			entry.fileSize = 0;
		}
	} else {
		entry.fileSize = 0;
	}
}

std::optional<std::vector<FilesystemListEntry>> Filesystem::listDirectory(const std::string& directory, bool includeFiles, bool includeDirectories) {
	if (!includeFiles && !includeDirectories)
		return {};
	
	auto resolvedPathOpt = DirectoryManager::getInstance().resolveDirectory(directory);
	if (!resolvedPathOpt.has_value())
		return std::nullopt;

	const auto& resolvedPath = resolvedPathOpt.value();
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
