#include "DataManager.hpp"

#include "DataFileIndex.hpp"
#include "DataStream.hpp"

#include <Filesystem/DirectoryManager.hpp>

#include <MyGui/MyGUI_IDataStream.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

std::vector<DataFileIndex::SearchFolder> DataManager::resolveSearchFolders() const {
	DirectoryManager& directoryManager = DirectoryManager::getInstance();

	std::vector<DataFileIndex::SearchFolder> folders;
	folders.reserve(this->searchDirectories_.size());

	for (const SearchDirectory& searchDirectory : this->searchDirectories_) {
		DataFileIndex::SearchFolder folder;
		folder.configuredPath = searchDirectory.path;
		folder.recursive = searchDirectory.recursive;

		std::filesystem::path resolvedPath;
		if (directoryManager.resolveDirectory(searchDirectory.path, resolvedPath))
			folder.resolvedPath = resolvedPath.string();

		folders.push_back(std::move(folder));
	}

	return folders;
}

bool DataManager::executeLookup(bool forceRebuild, const std::string& pattern, std::string& path) const {
	DataFileIndex& index = DataFileIndex::getInstance();
	index.update(this->resolveSearchFolders(), forceRebuild);

	return index.findFirst(pattern, path);
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

	DataFileIndex& index = DataFileIndex::getInstance();
	index.update(this->resolveSearchFolders(), false);

	index.collectNames(_pattern == "*.*" ? std::string() : _pattern, result);
	return result;
}

const std::string& DataManager::getDataPath(const std::string& _name) const {
	static std::string path;
	path.clear();

	if (_name.starts_with("$")) {
		path = _name;
		return path;
	}

	const std::string pattern = (_name == "*.*") ? "" : _name;
	if (!this->executeLookup(false, pattern, path)) {
		if (!this->executeLookup(true, pattern, path))
			path.clear();
	}

	return path;
}
