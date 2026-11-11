#pragma once
#include <UTILS/Singleton.hpp>
#include <UTILS/Uuid.hpp>

#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

class DirectoryManager : public UTILS::Singleton<DirectoryManager> {
	std::unordered_map<std::wstring, std::filesystem::path> directories_;
	mutable std::shared_mutex mutex_;

	bool splitKey(const std::wstring& path, std::wstring& key, std::wstring& rest) const;
	bool resolveKey(const std::wstring& key, const std::wstring& rest, std::filesystem::path& resolvedPath) const;
public:
	bool initialize();

	bool addDirectory(const std::wstring& name, const std::filesystem::path& path) {
		return this->directories_.try_emplace(name, std::filesystem::absolute(path)).second;
	}

	bool removeDirectory(const std::wstring& name) {
		return this->directories_.erase(name) != 0;
	}

	[[nodiscard]] std::optional<std::filesystem::path> resolveDirectory(const std::filesystem::path& path) const;
	[[nodiscard]] std::optional<std::filesystem::path> resolveDirectory(const std::filesystem::path& path, const UTILS::UUID localId) const;

	[[nodiscard]] void resolveContentData(std::filesystem::path& path, const UTILS::UUID localId) const;

	[[nodiscard]] static std::optional<std::filesystem::path> resolveDirectoryStatic(const std::filesystem::path& path) {
		return DirectoryManager::getInstance().resolveDirectory(path);
	}

	[[nodiscard]] static std::optional<std::filesystem::path> resolveDirectoryStatic(const std::filesystem::path& path, const UTILS::UUID localId) {
		return DirectoryManager::getInstance().resolveDirectory(path, localId);
	}

	[[nodiscard]] static void resolveContentDataStatic(std::filesystem::path& path, const UTILS::UUID localId) {
		DirectoryManager::getInstance().resolveContentData(path, localId);
	}
};