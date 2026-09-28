#pragma once
#include <UTILS/Singleton.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

enum class FilesystemListFormat {
	Name,			// filename.txt
	FullPath,		// C:/path/to/filename.txt
	FSPath			// $KEY/path/to/filename.txt
};

class FilesystemManager : public UTILS::Singleton<FilesystemManager> {
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

	STEAM_CALLBACK_MANUAL(FilesystemManager, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
	STEAM_CALLBACK_MANUAL(FilesystemManager, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, remoteStoragePublishedFileUnsubscribedCallback_);

	bool getAppInstallPath(std::filesystem::path& installPath);
	bool getUserProfilePath(std::filesystem::path& userProfilePath);
public:
	FilesystemManager() = default;
	~FilesystemManager() { this->cleanup(); }

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
		const std::string_view rest = (slash == std::string_view::npos) ? "" : path.substr(slash + 1);

		return this->resolveKey(key, rest, resolvedPath);
	}

	[[nodiscard]] bool resolveDirectoryWithContentData(std::string_view path, const std::string& localId, std::filesystem::path& resolvedPath) const {
		if (path.empty())
			return false;
		
		const size_t slash = path.find('/');
		
		const std::string_view key = path.substr(0, slash);
		const std::string_view rest = (slash == std::string_view::npos) ? "" : path.substr(slash + 1);
		
		if (key == "$CONTENT_DATA")
			return this->resolveKey("$CONTENT_" + localId, rest, resolvedPath);

		return this->resolveKey(key, rest, resolvedPath);
	}

	// The 2 functions below do not resolve directory keys, they operate on raw filesystem paths. Please avoid unless
	// you really have to. Use readFile and writeFile for normal file operations.

	[[nodiscard]] bool readFileRaw(const std::filesystem::path& path, std::vector<char>& outputContent) const;
	[[nodiscard]] bool writeFileRaw(const std::filesystem::path& path, const std::vector<char>& content) const;

	[[nodiscard]] bool readFile(const std::string& path, std::string& outputContent) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path, resolvedPath))
			return false;
	
		std::vector<char> buffer;
		if (!this->readFileRaw(resolvedPath, buffer))
			return false;

		outputContent.assign(buffer.begin(), buffer.end());
		return true;
	}

	[[nodiscard]] bool writeFile(const std::string& path, const std::string& content) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path, resolvedPath))
			return false;
	
		const std::vector<char> buffer(content.begin(), content.end());
		return this->writeFileRaw(resolvedPath, buffer);
	}
private:
	[[nodiscard]] bool listItems(const std::string& directory, std::vector<std::string>& outputItems, FilesystemListFormat format, bool listFiles) const;
public:
	[[nodiscard]] bool listFiles(const std::string& directory, std::vector<std::string>& outputFiles, FilesystemListFormat format = FilesystemListFormat::FSPath) const {
		return this->listItems(directory, outputFiles, format, true);
	}

	[[nodiscard]] bool listDirectories(const std::string& directory, std::vector<std::string>& outputDirectories, FilesystemListFormat format = FilesystemListFormat::FSPath) const {
		return this->listItems(directory, outputDirectories, format, false);
	}

	[[nodiscard]] bool fileExists(const std::string& path) const;
	[[nodiscard]] bool directoryExists(const std::string& path) const;
	
	[[nodiscard]] bool doesExist(const std::string& path) const;

	[[nodiscard]] bool getFileWriteTime(const std::string& path, time_t& writeTime) const;

	[[nodiscard]] bool getFileWriteTime(const std::filesystem::path& path, time_t& writeTime) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path.string(), resolvedPath))
			return false;

		return this->getFileWriteTimeRaw(resolvedPath, writeTime);
	}

	// The function below does not resolve directory keys, it operates on raw filesystem paths. Please avoid unless
	// you really have to. Use getFileWriteTime for normal file operations.
	[[nodiscard]] bool getFileWriteTimeRaw(const std::filesystem::path& path, time_t& writeTime) const;

	[[nodiscard]] bool createDirectories(const std::string& path) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path, resolvedPath))
			return false;

		return this->createDirectoriesRaw(resolvedPath);
	}

	// The function below does not resolve directory keys, it operates on raw filesystem paths. Please avoid unless
	// you really have to. Use getFileWriteTime for normal file operations.
	[[nodiscard]] bool createDirectoriesRaw(const std::filesystem::path& path) const;
};