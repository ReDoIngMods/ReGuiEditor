#pragma once
#include <UGCManager/UGCMod.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace ReGuiEditor {
	struct FilePickerResult {
		std::shared_ptr<UGCMod> selectedMod = nullptr;
		std::filesystem::path selectedPath = "";

		[[nodiscard]] std::string getFullPath() const;
		[[nodiscard]] bool isValid() const;
	};
}