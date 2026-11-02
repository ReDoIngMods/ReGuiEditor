#pragma once
#include <Modules/Misc/Singleton.hpp>
#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <filesystem>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ReGuiEditor {
	class DirectoryManager : public ReGuiEditor::Singleton<DirectoryManager> {
		mutable std::shared_mutex mutex_;
		std::unordered_map<std::string, std::filesystem::path> directories_;

		bool splitKey(const std::string& path, std::string& key, std::string& rest) const;

		bool resolveKey(const std::string& key, const std::string& rest, std::filesystem::path& resolvedPath) const;
	public:
		bool initialize();

		bool addDirectory(const std::string& name, const std::filesystem::path& path);
		bool removeDirectory(const std::string& name);

		[[nodiscard]] bool resolveDirectory(const std::string& path, std::filesystem::path& resolvedPath) const;
		[[nodiscard]] bool resolveContentData(std::string& path, const ReGuiEditor::Uuid& localId) const;
	};
}