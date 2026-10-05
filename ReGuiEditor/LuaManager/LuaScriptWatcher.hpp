#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class LuaScriptWatcher {
	std::unordered_map<std::string, time_t> files_;
public:
	void watchFile(const std::string& path);
	void clearFiles() { this->files_.clear(); }

	[[nodiscard]] std::vector<std::string> collectChangedFiles();
};