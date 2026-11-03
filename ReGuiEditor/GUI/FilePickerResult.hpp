#pragma once
#include <filesystem>
#include <string>

namespace ReGuiEditor {
	struct FilePickerResult {
		std::string contentPath = "";
		std::filesystem::path selectedPath = "";

		[[nodiscard]] std::string getFullPath() const;
		[[nodiscard]] bool isValid() const;
	};
}