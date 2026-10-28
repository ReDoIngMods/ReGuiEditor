#include "LuaScriptWatcher.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <string>
#include <vector>

void LuaScriptWatcher::watchFile(const std::string& path) {
	time_t writeTime;
	if (!FilesystemManager::getInstance().getFileWriteTime(path, writeTime))
		writeTime = 0;

	this->files_[path] = writeTime;
}

std::vector<std::string> LuaScriptWatcher::collectChangedFiles() {
	std::vector<std::string> changedFiles;

	for (auto& [path, knownWriteTime] : this->files_) {
		time_t writeTime;
		if (!FilesystemManager::getInstance().getFileWriteTime(path, writeTime) || writeTime == knownWriteTime)
			continue;

		knownWriteTime = writeTime;
		changedFiles.push_back(path);
	}

	return changedFiles;
}