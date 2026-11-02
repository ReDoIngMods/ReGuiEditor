#include "FilePickerResult.hpp"

#include <filesystem>
#include <string>

std::string ReGuiEditor::FilePickerResult::getFullPath() const {
	if (!this->isValid())
		return "";

	std::filesystem::path output = this->selectedMod->contentPath / this->selectedPath;
	output.make_preferred();

	return output.string();
}

bool ReGuiEditor::FilePickerResult::isValid() const {
	return this->selectedMod != nullptr && !this->selectedPath.empty();
}