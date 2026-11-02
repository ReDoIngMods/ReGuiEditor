#pragma once
#include "FilePickerResult.hpp"

#include <UGCManager/UGCMod.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ReGuiEditor {
	struct FilePickerRequest {
		std::string title = "";

		std::shared_ptr<UGCMod> defaultMod = nullptr;
		std::string defaultPath = "";

		std::vector<std::string> allowedExtensions = {};
		bool allowDirectorySelection = false;

		std::function<void(const FilePickerResult&)> onSelected = nullptr;
		std::function<void()> onCancel = nullptr;
	};
}