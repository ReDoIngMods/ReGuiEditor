#include "MyGuiDataManager.hpp"

#include "MyGuiDataStream.hpp"

#include <DirectoryManager/DirectoryManager.hpp>

#include <MyGui/MyGUI_IDataStream.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

inline static bool matchWildcard(const std::string& pattern, const std::string& str){
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

inline static void addEntry(std::vector<std::string>& result, const std::filesystem::directory_entry& entry, const std::string& pattern, bool outputAsFullPath) {
	if (entry.is_directory())
		return;

	const std::string name = entry.path().filename().string();
	if (!pattern.empty() && !matchWildcard(pattern, name))
		return;

	result.push_back(outputAsFullPath ? entry.path().string() : name);
}

inline static void scanFolder(std::vector<std::string>& result, const std::string& folder, bool scanRecursive, const std::string& mask, bool outputAsFullPath) {
	const std::string pattern = (mask == "*.*") ? "" : mask;
	const auto options = std::filesystem::directory_options::skip_permission_denied;

	std::error_code errorCode;

	if (scanRecursive) {
		std::filesystem::recursive_directory_iterator iterator(folder, std::filesystem::directory_options::skip_permission_denied, errorCode);
		if (errorCode)
			return;

		for (const auto& entry : iterator)
			addEntry(result, entry, pattern, outputAsFullPath);
	} else {
		std::filesystem::directory_iterator iterator(folder, std::filesystem::directory_options::skip_permission_denied, errorCode);
		if (errorCode)
			return;

		for (const auto& entry : iterator)
			addEntry(result, entry, pattern, outputAsFullPath);
	}
}

MyGUI::IDataStream* MyGuiDataManager::getData(const std::string& _name) {
	std::string filepath = this->getDataPath(_name);
	if (filepath.empty())
		return nullptr;

	return new MyGuiDataStream(filepath);
}

bool MyGuiDataManager::isDataExist(const std::string& _name) {
	return !this->getDataPath(_name).empty();
}

const std::vector<std::string>& MyGuiDataManager::getDataListNames(const std::string& _pattern) {
	static std::vector<std::string> result;
	result.clear();
	
	for (const auto& searchDir : this->searchDirectories_) {
		std::filesystem::path resolvedPath;
		if (!DirectoryManager::getInstance().resolveDirectory(searchDir.path, resolvedPath))
			continue;
	
		scanFolder(result, resolvedPath.string(), searchDir.recursive, _pattern, false);
	}
	
	return result;
}

const std::string& MyGuiDataManager::getDataPath(const std::string& _name) {
	static std::string path;

	std::vector<std::string> result;
	result.clear();
	
	DirectoryManager& directoryManager = DirectoryManager::getInstance();
	if (_name.starts_with("$")) {
		std::filesystem::path resolvedPath;
		if (!directoryManager.resolveDirectory(_name, resolvedPath))
			return path;

		path = resolvedPath.string();
		return path;
	}

	for (const auto& searchDir : this->searchDirectories_) {
		std::filesystem::path resolvedPath;
		if (!DirectoryManager::getInstance().resolveDirectory(searchDir.path, resolvedPath))
			continue;

		scanFolder(result, resolvedPath.string(), searchDir.recursive, _name, true);
	}

	path = result.size() == 1 ? result[0] : "";
	return path;
}