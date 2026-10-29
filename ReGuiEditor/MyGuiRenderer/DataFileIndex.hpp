#pragma once
#include <string>
#include <vector>

#include <UTILS/Singleton.hpp>

class DataFileIndex : public UTILS::Singleton<DataFileIndex> {
	struct Entry {
		std::string name;
		std::string relativePath;
	};

	struct Directory {
		std::string configuredPath;
		std::vector<Entry> files;
	};

	std::vector<Directory> directories_;
	bool initialized_ = false;
public:
	struct SearchFolder {
		std::string configuredPath;
		std::string resolvedPath;

		bool recursive = false;
	};

	void update(const std::vector<SearchFolder>& folders, bool forceRebuild);

	[[nodiscard]] bool findFirst(const std::string& pattern, std::string& path) const;

	void collectNames(const std::string& pattern, std::vector<std::string>& names) const;
};
