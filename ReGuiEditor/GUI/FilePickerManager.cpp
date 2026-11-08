#include "FilePickerManager.hpp"

#include "FilePickerRequest.hpp"

#include "Views/FilePickerView.hpp"
#include "ViewsManager.hpp"

void FilePickerManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing FilePickerManager...");

	ViewsManager::getInstance().registerView<FilePickerView>("File Picker");
}

bool FilePickerManager::isActive() const {
	return this->isActive_;
}

bool FilePickerManager::sendRequest(const FilePickerRequest& request) {
	if (this->isActive_) {
		CONSOLE_WARNING("FilePickerManager: A file picker request is already active. Ignoring new request.");
		return false;
	}

	this->currentRequest_ = request;
	this->isActive_ = true;
	return true;
}