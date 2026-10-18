#include "FontManager.hpp"

#include "../Window/TitlebarHitTest.hpp"

#include "../Application.hpp"

#include "ImageManager.hpp"
#include "WindowDecoration.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

void WindowDecoration::buildDockspace(ImGuiID dockspaceID, ImVec2 size) {
	ImGui::DockBuilderRemoveNode(dockspaceID);

	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);

	ImGui::DockBuilderSetNodeSize(dockspaceID, size);

	ImGuiID right = 0;
	ImGuiID explorer = 0;
	ImGuiID properties = 0;

	ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Right, 0.20f, &right, &dockspaceID);
	explorer = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.50f, &explorer, &properties);

	ImGui::DockBuilderDockWindow("Render View", dockspaceID);
	ImGui::DockBuilderDockWindow("Explorer", explorer);
	ImGui::DockBuilderDockWindow("Properties", properties);

	ImGui::DockBuilderFinish(dockspaceID);
}

void WindowDecoration::drawProgamName(const char* name) {
	ImGuiStyle& style = ImGui::GetStyle();
	ImFont* font = FontManager::getInstance().getFont("Futura");
	if (!font) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Font 'Futura' not found. Using default font.");
		font = ImGui::GetFont();
	}

	float fontSize = ImGui::GetFontSize();
	ImFontBaked* baked = font->GetFontBaked(fontSize);

	float minY = FLT_MAX;
	float maxY = -FLT_MAX;

	{
		const char* glyphPointer = name;
		while (*glyphPointer) {
			ImFontGlyph* glyph = baked->FindGlyph(*glyphPointer);
			if (glyph) {
				minY = std::min(minY, glyph->Y0);
				maxY = std::max(maxY, glyph->Y1);
			}

			++glyphPointer;
		}
	}

	float glyphHeight = maxY - minY;
	float frameHeight = ImGui::GetFrameHeight();

	ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, name);
	float fontOffset = (frameHeight - glyphHeight) / 2.f;

	ImVec2 cursor = ImGui::GetCursorScreenPos();
	ImVec2 textPos = { cursor.x, cursor.y + fontOffset };

	ImGui::GetWindowDrawList()->AddText(font, fontSize, textPos, 0xFF00A0FF, name);

	ImGui::Dummy({ textSize.x, frameHeight });
}

void WindowDecoration::drawWindowButtons() {
	const float buttonSize = std::round(ImGui::GetFrameHeight() / 2.f) * 2.f;
	const float iconSize = std::round(buttonSize * 0.6f);
	const float pad = (buttonSize - iconSize) * 0.5f;
	const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
	const float buttonArrayWidth = (buttonSize + buttonSpacing) * 3 - buttonSpacing;

	ImGui::SetCursorPos({ ImGui::GetWindowWidth() - buttonArrayWidth, 0.f });

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad, pad));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

	Application& app = Application::getInstance();
	ImageManager& imageManager = ImageManager::getInstance();

	const ImTextureID minimizeIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-minimize.svg", 64, 64, 0xFFAAAAAA);
	const ImTextureID maximizeIcon = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-maximize.svg", 64, 64, 0xFFAAAAAA);
	const ImTextureID restoreIcon  = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-restore.svg",  64, 64, 0xFFAAAAAA);
	const ImTextureID closeIcon    = imageManager.findOrLoadImGuiTextureSVG("$RGE_DATA/Icons/chrome-close.svg",    64, 64, 0xFFAAAAAA);

	const ImVec2 iconExtent = { iconSize, iconSize };

	if (ImGui::ImageButton("#minimizeButton", minimizeIcon, iconExtent))
		app.window.minimize();
	TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	ImGui::SameLine();

	if (app.window.isMaximized()) {
		if (ImGui::ImageButton("#restoreButton", restoreIcon, iconExtent))
			app.window.restore();
	}
	else {
		if (ImGui::ImageButton("#maximizeButton", maximizeIcon, iconExtent))
			app.window.maximize();
	}
	TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	ImGui::SameLine();

	if (ImGui::ImageButton("#closeButton", closeIcon, iconExtent))
		app.isRunning = false;
	TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

void WindowDecoration::render() {
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

	if (ImGui::BeginMenuBar()) {
		this->drawProgamName("ReGuiEditor");

		bool fileOpen = ImGui::BeginMenu("File");
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		if (fileOpen) {
			ImGui::MenuItem("New");
			ImGui::MenuItem("Open");
			ImGui::MenuItem("Save");
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}

		bool editOpen = ImGui::BeginMenu("Edit");
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		if (editOpen) {
			ImGui::MenuItem("Undo");
			ImGui::MenuItem("Redo");
			ImGui::Separator();
			ImGui::MenuItem("Cut");
			ImGui::MenuItem("Copy");
			ImGui::MenuItem("Paste");
			ImGui::EndMenu();
		}

		bool viewOpen = ImGui::BeginMenu("View");
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		if (viewOpen) {
			Application& app = Application::getInstance();
			app.explorerPanel.renderMenu();
			app.propertiesPanel.renderMenu();
			app.renderPanel.renderMenu();

			ImGui::EndMenu();
		}

		bool helpOpen = ImGui::BeginMenu("Help");
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		if (helpOpen) {
			ImGui::MenuItem("About");
			ImGui::EndMenu();
		}

		this->drawWindowButtons();

		ImGui::EndMenuBar();
	}

	ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
	ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();

	ImGui::DockSpace(dockspaceId, dockspaceSize, ImGuiDockNodeFlags_None);

	if (!this->isBuiltDockspace_) {
		this->buildDockspace(dockspaceId, dockspaceSize);
		this->isBuiltDockspace_ = true;
	}

	ImGui::End();
}