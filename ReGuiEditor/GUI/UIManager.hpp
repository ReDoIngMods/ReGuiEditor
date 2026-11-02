#pragma once
#include <Modules/Misc/Singleton.hpp>

#include "ImGuiFontManager.hpp"
#include "ImageManager.hpp"
#include "NotificationManager.hpp"
#include "ViewsManager.hpp"

#include "FilePickerManager.hpp"

#include "DebugTextHandler.hpp"
#include "FileIconHandler.hpp"

#include <memory>

class UIManager : public ReGuiEditor::Singleton<UIManager> {
	void applyImGuiTheme();
public:
	UIManager() = default;
	~UIManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	std::shared_ptr<ImGuiFontManager> fontManager = nullptr;
	std::shared_ptr<ImageManager> imageManager = nullptr;
	std::shared_ptr<NotificationManager> notificationManager = nullptr;
	std::shared_ptr<ReGuiEditor::ViewsManager> viewsManager = nullptr;

	std::shared_ptr<ReGuiEditor::FilePickerManager> filePickerManager = nullptr;

	DebugTextHandler debugTextHandler;
	FileIconHandler fileIconHandler;
};