#include "FilePickerResult.hpp"

#include <filesystem>
#include <string>

std::string FilePickerResult::getFullPath() const {
	if (!this->isValid())
		return "";

	std::filesystem::path output = std::filesystem::path(this->contentPath) / this->selectedPath;
	output.make_preferred();

	return output.string();
}

bool FilePickerResult::isValid() const {
	return !this->contentPath.empty() && !this->selectedPath.empty();
}