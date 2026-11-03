#pragma once
#include "FilePickerResult.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ReGuiEditor {
	struct FilePickerRequest {
		std::string title = "";

		std::string defaultContentPath = "";
		std::string defaultPath = "";

		std::vector<std::string> allowedExtensions = {};
		bool allowDirectorySelection = false;

		std::function<void(const FilePickerResult&)> onSelected = nullptr;
		std::function<void()> onCancel = nullptr;
	};
}