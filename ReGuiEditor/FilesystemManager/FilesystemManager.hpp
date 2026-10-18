#pragma once
#include <UTILS/Singleton.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class FilesystemManager : public UTILS::Singleton<FilesystemManager> {
	std::unordered_map<std::string, std::filesystem::path> directories_;

	bool splitKey(const std::string& path, std::string& key, std::string& rest) const;

	bool resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const;

	bool getAppInstallPath(std::filesystem::path& installPath);
	bool getUserProfilePath(std::filesystem::path& userProfilePath);
public:
	FilesystemManager() = default;
	~FilesystemManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	bool addDirectory(const std::string& name, const std::filesystem::path& path) {
		return this->directories_.try_emplace(name, std::filesystem::absolute(path)).second;
	}

	bool removeDirectory(const std::string& name) {
		return this->directories_.erase(name) != 0;
	}

	[[nodiscard]] bool resolveDirectory(const std::string& path, std::filesystem::path& resolvedPath) const {
		if (path.empty())
			return false;

		// Dependency paths may already be resolved native paths. Do not interpret
		// a drive letter (for example, "D:") as a virtual filesystem key.
		const std::filesystem::path inputPath(path);
		if (inputPath.is_absolute()) {
			resolvedPath = inputPath.lexically_normal();
			return true;
		}

		std::string key, rest;
		if (!this->splitKey(path, key, rest))
			return false;

		return this->resolveKey(key, rest, resolvedPath);
	}

	[[nodiscard]] bool resolveContentData(std::string& path, const std::string& localId) const {
		if (path.empty())
			return false;
		
		std::string key, rest;
		if (!this->splitKey(path, key, rest))
			return false;
		
		if (key != "$CONTENT_DATA")
			return true;

		path = "$CONTENT_" + localId + "/" + rest;
		return true;
	}

	[[nodiscard]] bool readFile(const std::string& path, std::vector<uint8_t>& outputData) const;
	[[nodiscard]] bool readFile(const std::string& path, std::string& outputString) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		outputString.assign(data.begin(), data.end());
		return true;
	}

	[[nodiscard]] bool readFile(const std::string& path, std::vector<char>& outputData) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		outputData.assign(data.begin(), data.end());
		return true;
	}

	[[nodiscard]] bool readFile(const std::string& path, UTILS::ByteStreamReader& reader) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		reader.setData(data.data(), data.size());
		return true;
	}

	[[nodiscard]] bool writeFile(const std::string& path, const std::vector<uint8_t>& data) const;
	[[nodiscard]] bool writeFile(const std::string& path, const std::string& data) const {
		std::vector<uint8_t> byteData(data.begin(), data.end());
		return this->writeFile(path, byteData);
	}

	[[nodiscard]] bool writeFile(const std::string& path, const std::vector<char>& data) const {
		std::vector<uint8_t> byteData(data.begin(), data.end());
		return this->writeFile(path, byteData);
	}

	[[nodiscard]] bool writeFile(const std::string& path, const UTILS::ByteStreamWriter& writer) const {
		return this->writeFile(path, writer.toVector());
	}

	[[nodiscard]] bool exists(const std::string& path) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path, resolvedPath))
			return false;

		return std::filesystem::exists(resolvedPath);
	}

	[[nodiscard]] bool getFileWriteTime(const std::string& path, time_t& writeTime) const {
		std::filesystem::path resolvedPath;
		if (!this->resolveDirectory(path, resolvedPath))
			return false;

		if (!std::filesystem::exists(resolvedPath))
			return false;

		writeTime = std::filesystem::last_write_time(resolvedPath).time_since_epoch().count();
		return true;
	}
};