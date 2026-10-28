#include "DataManager.hpp"

#include "DataStream.hpp"

#include <Filesystem/DirectoryManager.hpp>

#include <MyGui/MyGUI_IDataStream.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

inline static bool matchWildcard(const std::string& pattern, const std::string& str) {
	size_t patternIndex = 0;
	size_t stringIndex = 0;

	size_t starPatternIndex = std::string::npos;
	size_t starStringIndex = 0;

	while (stringIndex < str.size()) {
		const bool patternMatches = patternIndex < pattern.size() && (pattern[patternIndex] == str[stringIndex] || pattern[patternIndex] == '?');
		if (patternMatches) {
			++patternIndex;
			++stringIndex;
		} else if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
			starPatternIndex = patternIndex++;
			starStringIndex = stringIndex;
		} else if (starPatternIndex != std::string::npos) {
			patternIndex = starPatternIndex + 1;
			stringIndex = ++starStringIndex;
		} else {
			return false;
		}
	}

	while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
		++patternIndex;

	return patternIndex == pattern.size();
}

inline static void addEntry(std::vector<std::string>& result, const std::filesystem::directory_entry& entry, const std::string& pattern, const std::filesystem::path& basePath, bool outputAsFullPath) {
	if (entry.is_directory())
		return;

	const std::string name = entry.path().filename().string();
	if (!pattern.empty() && !matchWildcard(pattern, name))
		return;

	result.push_back(outputAsFullPath ? entry.path().lexically_relative(basePath).string() : name);
}

inline static void scanFolder(std::vector<std::string>& result, const std::string& folder, bool scanRecursive, const std::string& mask, bool outputAsFullPath) {
	const std::string pattern = (mask == "*.*") ? "" : mask;
	const std::filesystem::path basePath(folder);
	const auto options = std::filesystem::directory_options::skip_permission_denied;

	std::error_code errorCode;

	if (scanRecursive) {
		std::filesystem::recursive_directory_iterator iterator(folder, options, errorCode);
		if (errorCode)
			return;

		for (const auto& entry : iterator)
			addEntry(result, entry, pattern, basePath, outputAsFullPath);
	} else {
		std::filesystem::directory_iterator iterator(folder, options, errorCode);
		if (errorCode)
			return;

		for (const auto& entry : iterator)
			addEntry(result, entry, pattern, basePath, outputAsFullPath);
	}
}

MyGUI::IDataStream* DataManager::getData(const std::string& _name) const {
	std::string filepath = this->getDataPath(_name);
	if (filepath.empty())
		return nullptr;

	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(filepath, resolvedPath))
		return nullptr;

	return new DataStream(resolvedPath.string());
}

bool DataManager::isDataExist(const std::string& _name) const {
	return !this->getDataPath(_name).empty();
}

const std::vector<std::string>& DataManager::getDataListNames(const std::string& _pattern) const {
	static std::vector<std::string> result;
	result.clear();

	DirectoryManager& directoryManager = DirectoryManager::getInstance();
	for (const auto& searchDir : this->searchDirectories_) {
		std::filesystem::path resolvedPath;
		if (!directoryManager.resolveDirectory(searchDir.path, resolvedPath))
			continue;

		scanFolder(result, resolvedPath.string(), searchDir.recursive, _pattern, false);
	}

	return result;
}

const std::string& DataManager::getDataPath(const std::string& _name) const {
	static std::string path;
	path.clear();

	DirectoryManager& directoryManager = DirectoryManager::getInstance();
	if (_name.starts_with("$")) {
		path = _name;
		return path;
	}

	std::vector<std::string> result;
	result.clear();

	for (const auto& searchDir : this->searchDirectories_) {
		std::filesystem::path resolvedPath;
		if (!directoryManager.resolveDirectory(searchDir.path, resolvedPath))
			continue;

		scanFolder(result, resolvedPath.string(), searchDir.recursive, _name, true);

		if (result.size() == 1) {
			path = (std::filesystem::path(searchDir.path) / result[0]).string();
			return path;
		}

		result.clear();
	}

	return path;
}