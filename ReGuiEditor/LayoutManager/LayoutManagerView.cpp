#include "LayoutManagerView.hpp"

#include <GUI/FilePickerManager.hpp>

#include <ImGui/imgui.h>

void LayoutManagerView::openLayout(bool force) {
	FilePickerManager& filePickerManager = FilePickerManager::getInstance();
	if (filePickerManager.isActive())
		return;

	FilePickerRequest request;
	request.title = "Open Layout";
	request.allowedExtensions = { ".layout" };
	request.onSelected = [this](const FilePickerResult& result) {
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Selected layout: %s", result.getFullPath().c_str());
	};

	filePickerManager.sendRequest(request);
}

void LayoutManagerView::renderMenu() {
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Open Layout"))
			this->openLayout(false);

		ImGui::EndMenu();
	}
}