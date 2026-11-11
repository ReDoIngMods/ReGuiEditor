#include "FileInfo.hpp" // VCIC-Excluded

#include "DirectoryManager.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

bool Filesystem::exists(const std::filesystem::path& path) {
	auto resolvedPath = DirectoryManager::resolveDirectoryStatic(path);
	if (!resolvedPath)
		return false;

	std::error_code errorCode;
	return std::filesystem::exists(resolvedPath.value(), errorCode) && !errorCode;
}

bool Filesystem::getFileWriteTime(const std::filesystem::path& path, time_t& writeTime) {
	auto resolvedPath = DirectoryManager::resolveDirectoryStatic(path);
	if (!resolvedPath)
		return false;

	std::error_code errorCode;
	std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(resolvedPath.value(), errorCode);
	if (errorCode) {
		CONSOLE_ERROR("Failed to get file write time for path: %s. Error: %s", path.c_str(), errorCode.message().c_str());
		return false;
	}

	const auto fileTimeUtc = std::chrono::file_clock::to_utc(fileTime);
	const auto systemTime = std::chrono::utc_clock::to_sys(fileTimeUtc);

	writeTime = std::chrono::system_clock::to_time_t(systemTime);
	return true;
}
