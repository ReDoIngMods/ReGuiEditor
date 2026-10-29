#pragma once
#include "DataFileIndex.hpp"

#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_IDataStream.h>
#include <MyGui/MyGUI_Types.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class DataManager : public MyGUI::DataManager {
private:
	struct SearchDirectory {
		std::string path;
		bool recursive;
	};

	std::vector<SearchDirectory> searchDirectories_;

	std::shared_ptr<DataFileIndex> dataFileIndex_ = DataFileIndex::createInstance();

	std::vector<DataFileIndex::SearchFolder> resolveSearchFolders() const;
	bool executeLookup(bool forceRebuild, const std::string& pattern, std::string& path) const;
public:
	MyGUI::IDataStream* getData(const std::string& _name) const override;

	void freeData(MyGUI::IDataStream* _data) override {
		delete _data;
	}

	bool isDataExist(const std::string& _name) const override;

	const MyGUI::VectorString& getDataListNames(const std::string& _pattern) const override;
	const std::string& getDataPath(const std::string& _name) const override;

	void addSearchDirectory(const std::string& path, bool recursive = false) {
		this->searchDirectories_.emplace_back(path, recursive);

		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Added search directory: %s%s", path.c_str(), recursive ? " (recursive)" : "");
	}

	void removeSearchDirectory(const std::string& path) {
		auto iterator = std::remove_if(this->searchDirectories_.begin(), this->searchDirectories_.end(), [&path](const SearchDirectory& dir) -> bool {
			return dir.path == path;
		});

		if (iterator != this->searchDirectories_.end())
			CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Removed search directory: %s", path.c_str());

		this->searchDirectories_.erase(iterator, this->searchDirectories_.end());
	}
};
