#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_map>

class FileIconHandler {
	std::unordered_map<std::string, std::string> extensionToIconMap_;
	std::string folderIconPath_;
	std::string defaultFileIconPath_;

	static std::string normalizeExtension(std::string extension) {
		if (!extension.empty() && extension.front() == '.')
			extension.erase(0, 1);

		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return extension;
	}
public:
	bool initialize();

	const std::string& getIconForExtension(const std::string& extension) const {
		auto iterator = this->extensionToIconMap_.find(normalizeExtension(extension));
		if (iterator != this->extensionToIconMap_.end())
			return iterator->second;

		return this->defaultFileIconPath_;
	}

	const std::string& getIconForFile(const std::filesystem::path& filePath) const {
		return this->getIconForExtension(filePath.extension().string());
	}

	const std::string& getIconForFolder() const {
		return this->folderIconPath_;
	}
};