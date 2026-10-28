#pragma once
#include <UTILS/Singleton.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

class DirectoryManager : public UTILS::Singleton<DirectoryManager> {
	std::unordered_map<std::string, std::filesystem::path> directories_;

	bool splitKey(const std::string& path, std::string& key, std::string& rest) const;

	bool resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const;
public:
	bool initialize();

	bool addDirectory(const std::string& name, const std::filesystem::path& path) {
		return this->directories_.try_emplace(name, std::filesystem::absolute(path)).second;
	}

	bool removeDirectory(const std::string& name) {
		return this->directories_.erase(name) != 0;
	}

	[[nodiscard]] bool resolveDirectory(const std::string& path, std::filesystem::path& resolvedPath) const {
		if (path.empty())
			return false;

		const std::filesystem::path inputPath(path);
		if (inputPath.is_absolute())
			return false;

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
};