#pragma once
#include <UTILS/Singleton.hpp>

#include "ImGuiFontManager.hpp"
#include "ImageManager.hpp"
#include "NotificationManager.hpp"
#include "ViewsManager.hpp"

#include "DebugTextHandler.hpp"

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

	DebugTextHandler debugTextHandler;
};