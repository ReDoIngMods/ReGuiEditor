#include "ViewsManager.hpp"

#include "View.hpp"

#include <Modules/Misc/Profiler.hpp>

ReGuiEditor::ViewsManager::ViewsManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ViewsManager...");
}

ReGuiEditor::ViewsManager::~ViewsManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Cleaning up ViewsManager...");

	for (auto& viewInstance : this->views_) {
		viewInstance.view->deinitialize();
		delete viewInstance.view;
	}

	this->views_.clear();
}

bool ReGuiEditor::ViewsManager::registerViewInternal(const char* name, ReGuiEditor::View* view) {
	if (!view) {
		CONSOLE_ERROR("Cannot register a null view with name: %s", name);
		return false;
	}

	if (!view->initialize()) {
		CONSOLE_ERROR("Failed to initialize view: %s", name);
		return false;
	}

	this->views_.emplace_back(view, name);
	return true;
}

void ReGuiEditor::ViewsManager::render() {
	for (auto& viewInstance : this->views_) {
		PROFILE_SCOPE("ViewsManager::render: %s", viewInstance.name);

		viewInstance.view->render();
	}
}

void ReGuiEditor::ViewsManager::renderMenu() {
	for (auto& viewInstance : this->views_)
		viewInstance.view->renderMenu();
}

void ReGuiEditor::ViewsManager::tick() {
	for (auto& viewInstance : this->views_)
		viewInstance.view->tick();
}

void ReGuiEditor::ViewsManager::update(double deltaTime) {
	for (auto& viewInstance : this->views_)
		viewInstance.view->update(deltaTime);
}