#pragma once
#include <UTILS/Singleton.hpp>

#include "ImGuiFontManager.hpp"
#include "ImageManager.hpp"
#include "NotificationManager.hpp"
#include "ViewsManager.hpp"

#include "FilePickerManager.hpp"

#include "DebugTextHandler.hpp"
#include "FileIconHandler.hpp"

class UIManager : public UTILS::Singleton<UIManager> {
	void applyImGuiTheme();
public:
	UIManager() = default;
	~UIManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	ImGuiFontManager* fontManager = nullptr;
	ImageManager* imageManager = nullptr;
	NotificationManager* notificationManager = nullptr;
	ViewsManager* viewsManager = nullptr;

	FilePickerManager* filePickerManager = nullptr;

	DebugTextHandler debugTextHandler;
	FileIconHandler fileIconHandler;
};