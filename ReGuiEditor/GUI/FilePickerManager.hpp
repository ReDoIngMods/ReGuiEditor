#pragma once
#include "Views/FilePickerView.hpp" // VCIC-Excluded
#include "ViewsManager.hpp"

#include <UGCManager/UGCMod.hpp>

#include <UTILS/Singleton.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct FilePickerResult {
	std::shared_ptr<UGCMod> selectedMod = nullptr;
	std::string selectedPath = "";

	std::string getFullPath() const {
		if (!this->isValid())
			return "";

		return this->selectedMod->contentPath + "/" + this->selectedPath;
	}

	bool isValid() const {
		return this->selectedMod != nullptr && !this->selectedPath.empty();
	}
};

struct FilePickerRequest {
	std::string title = "";

	std::shared_ptr<UGCMod> defaultMod = nullptr;
	std::string defaultPath = "";

	std::vector<std::string> allowedExtensions = {};
	bool allowDirectorySelection = false;

	std::function<void(const FilePickerResult&)> onSelected = nullptr;
	std::function<void()> onCancel = nullptr;
};

class FilePickerManager : public UTILS::Singleton<FilePickerManager> {
	FilePickerRequest currentRequest_;
	bool isActive_ = false;

	friend class FilePickerView;
public:
	void initialize() {
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing FilePickerManager...");

		ViewsManager::getInstance().registerView<FilePickerView>("File Picker");
	}

	bool isActive() const {
		return this->isActive_;
	}

	bool sendRequest(const FilePickerRequest& request) {
		if (this->isActive_) {
			CONSOLE_ERROR("FilePickerManager is already active. Cannot send a new request.");
			return false;
		}

		this->currentRequest_ = request;
		this->isActive_ = true;
		return true;
	}
};