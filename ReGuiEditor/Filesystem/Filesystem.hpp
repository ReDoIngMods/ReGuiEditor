#pragma once
#include <DirectoryManager/DirectoryManager.hpp>

#include <filesystem>
#include <string>

namespace FS {
	bool createDirectories(const std::filesystem::path& path);

	bool readFile(const std::filesystem::path& path, std::string& outContent);
	bool writeFile(const std::filesystem::path& path, const std::string& content);

	bool fileExists(const std::filesystem::path& path);
	bool directoryExists(const std::filesystem::path& path);
	bool fileOrDirectoryExists(const std::filesystem::path& path);

	inline bool readFileDM(const std::string& path, std::string& outContent) {
		std::filesystem::path resolvedPath;
		if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
			return false;

		return FS::readFile(resolvedPath, outContent);
	}

	inline bool writeFileDM(const std::string& path, const std::string& content) {
		std::filesystem::path resolvedPath;
		if (!DirectoryManager::getInstance().resolveDirectory(path, resolvedPath))
			return false;
	
		return FS::writeFile(resolvedPath, content);
	}
}