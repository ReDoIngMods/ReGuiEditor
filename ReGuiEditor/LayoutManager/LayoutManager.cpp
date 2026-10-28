#include "LayoutManager.hpp"

#include "LayoutManagerView.hpp"

#include <GUI/ViewsManager.hpp>

#include <string>

void LayoutManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing LayoutManager...");

	ViewsManager& viewsManager = ViewsManager::getInstance();
	viewsManager.registerView<LayoutManagerView>("Layout Manager");
}

bool LayoutManager::loadLayout(const std::string& layoutName) {
    return false;
}
