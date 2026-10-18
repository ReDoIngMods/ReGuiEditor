#include "DecorationView.hpp"

#include <GUI/ImGuiFontManager.hpp>
#include <GUI/ImageManager.hpp>
#include <GUI/ViewsManager.hpp>

#include <Window/TitlebarHitTest.hpp>

#include <Application.hpp>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <cmath>

void DecorationView::buildDockspace(ImGuiID dockspaceId, ImVec2 size) {
	ImGui::DockBuilderRemoveNode(dockspaceId);

	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);

	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID right = 0;
	ImGuiID explorer = 0;
	ImGuiID properties = 0;

	ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.20f, &right, &dockspaceId);
	explorer = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.50f, &explorer, &properties);

	ImGui::DockBuilderDockWindow("Render View", dockspaceId);
	ImGui::DockBuilderDockWindow("Explorer", explorer);
	ImGui::DockBuilderDockWindow("Properties", properties);

	ImGui::DockBuilderFinish(dockspaceId);
}

void DecorationView::drawProgamName(const char* name) {
	ImFont* font = ImGuiFontManager::getInstance().getFont("Futura");
	if (!font) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Font 'Futura' not found. Using default font.");
		font = ImGui::GetFont();
	}

	static const ImColor textColor = ImColor(0xFF, 0xA0, 0x00);

	ImGui::PushFont(font, 0.f);
	ImGui::TextColored(textColor, name);
	ImGui::PopFont();
}

bool DecorationView::drawWindowButton(const char* id, ImTextureID icon, ImVec2 size) {
	const bool click = ImGui::ImageButton(id, icon, size);
	TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	return click;
}

void DecorationView::drawWindowButtons() {
	const float buttonSize = std::round(ImGui::GetFrameHeight() / 2.f) * 2.f;
	const float iconSize = std::round(buttonSize * 0.6f);

	const float padding = (buttonSize - iconSize) * 0.5f;

	const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
	const float buttonArrayWidth = (buttonSize + buttonSpacing) * 3 - buttonSpacing;

	const ImVec2 iconExtent = { iconSize, iconSize };

	ImageManager& imageManager = ImageManager::getInstance();
	const ImTextureID minimizeIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-minimize.svg", 64, 64, 0xFFAAAAAA);
	const ImTextureID maximizeIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-maximize.svg", 64, 64, 0xFFAAAAAA);
	const ImTextureID restoreIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-restore.svg", 64, 64, 0xFFAAAAAA);
	const ImTextureID closeIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-close.svg", 64, 64, 0xFFAAAAAA);

	ImGui::SetCursorPos({ ImGui::GetWindowWidth() - buttonArrayWidth, 0.f });

	ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { padding, padding });
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

	Application& app = Application::getInstance();
	if (this->drawWindowButton("#minimizeButton", minimizeIcon, iconExtent))
		app.window.minimize();

	if (app.window.isMaximized()) {
		if (this->drawWindowButton("#restoreButton", restoreIcon, iconExtent))
			app.window.restore();
	} else {
		if (this->drawWindowButton("#maximizeButton", maximizeIcon, iconExtent))
			app.window.maximize();
	}

	if (this->drawWindowButton("#closeButton", closeIcon, iconExtent))
		app.isRunning = false;

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

bool DecorationView::beginMenu(const char* label, bool enabled) {
	const bool isOpened = ImGui::BeginMenu(label, enabled);
	TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	return isOpened;
}

void DecorationView::renderMenubar() {
	if (!ImGui::BeginMenuBar())
		return;

	this->drawProgamName("ReGuiEditor");

	if (this->beginMenu("File")) {
		ImGui::MenuItem("New", "Ctrl+N");
		ImGui::MenuItem("Open", "Ctrl+O");
		ImGui::MenuItem("Save", "Ctrl+S");
		ImGui::MenuItem("Save As", "Ctrl+Shift+S");
		ImGui::Separator();
		ImGui::MenuItem("Exit", "Alt+F4");
		ImGui::EndMenu();
	}

	if (this->beginMenu("Edit")) {
		ImGui::MenuItem("Undo", "Ctrl+Z");
		ImGui::MenuItem("Redo", "Ctrl+Y");
		ImGui::Separator();
		ImGui::MenuItem("Cut", "Ctrl+X");
		ImGui::MenuItem("Copy", "Ctrl+C");
		ImGui::MenuItem("Paste", "Ctrl+V");
		ImGui::EndMenu();
	}

	if (this->beginMenu("View")) {
		ViewsManager::getInstance().renderMenu();
		ImGui::EndMenu();
	}

	if (this->beginMenu("Help")) {
		ImGui::MenuItem("About");
		ImGui::EndMenu();
	}

	this->drawWindowButtons();

	ImGui::EndMenuBar();
}

void DecorationView::render() {
	TitlebarHitTest::beginFrame();

	ImGuiWindowClass windowClass;
	windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon | ImGuiViewportFlags_NoFocusOnAppearing;

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(mainViewport->Pos);
	ImGui::SetNextWindowSize(mainViewport->Size);
	ImGui::SetNextWindowViewport(mainViewport->ID);
	ImGui::SetNextWindowClass(&windowClass);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	ImGui::Begin("##window_decoration", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

	ImGui::PopStyleVar();

	this->renderMenubar();

	ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
	ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();

	ImGui::DockSpace(dockspaceId, dockspaceSize, ImGuiDockNodeFlags_None);

	if (!this->isBuiltDockspace_) {
		this->buildDockspace(dockspaceId, dockspaceSize);
		this->isBuiltDockspace_ = true;
	}

	ImGui::End();
}