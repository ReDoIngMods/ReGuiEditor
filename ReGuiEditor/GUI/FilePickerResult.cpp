#include "FilePickerResult.hpp"

#include <filesystem>
#include <string>

std::string ReGuiEditor::FilePickerResult::getFullPath() const {
	if (!this->isValid())
		return "";

	std::filesystem::path output = std::filesystem::path(this->contentPath) / this->selectedPath;
	output.make_preferred();

	return output.string();
}

bool ReGuiEditor::FilePickerResult::isValid() const {
	return !this->contentPath.empty() && !this->selectedPath.empty();
}