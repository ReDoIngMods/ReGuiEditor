#include "FilePickerManager.hpp"

#include "FilePickerRequest.hpp"

#include "Views/FilePickerView.hpp"
#include "ViewsManager.hpp"

void ReGuiEditor::FilePickerManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing FilePickerManager...");

	ReGuiEditor::ViewsManager::getInstance().registerView<FilePickerView>("File Picker");
}

bool ReGuiEditor::FilePickerManager::isActive() const {
	return this->isActive_;
}

bool ReGuiEditor::FilePickerManager::sendRequest(const ReGuiEditor::FilePickerRequest& request) {
	if (this->isActive_) {
		CONSOLE_WARNING("FilePickerManager: A file picker request is already active. Ignoring new request.");
		return false;
	}

	this->currentRequest_ = request;
	this->isActive_ = true;
	return true;
}