#include "FontManager.hpp"

#include "../Window/TitlebarHitTest.hpp"

#include "../Application.hpp"

#include "WindowDecoration.hpp"

#include "Lucide/IconsLucide.h"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <algorithm>
#include <cfloat>

inline static void drawTitlebar(const char* titleBar) {
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
        const char* glyphPointer = titleBar;
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

    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, titleBar);
    float fontOffset = (frameHeight - glyphHeight) / 2.f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 textPos = { cursor.x, cursor.y + fontOffset };

    ImGui::GetWindowDrawList()->AddText(font, fontSize, textPos, 0xFF00A0FF, titleBar);

    // Not registered as a widget rect -> falls through to native
    // HTCAPTION, so the label area drags/double-click-maximizes too.
    ImGui::Dummy({ textSize.x, frameHeight });
}

void WindowDecoration::buildDockspace(ImGuiID dockspaceID, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, size);

    constexpr float LeftWidth = 0.15f;
    constexpr float CenterWidth = 0.50f;
    constexpr float RightWidth = 0.15f;

	ImGuiID left = 0;
	ImGuiID center = 0;
	ImGuiID right = 0;

	left = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, LeftWidth, nullptr, &center);
	right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, RightWidth / (CenterWidth + RightWidth), nullptr, &center);

    ImGui::DockBuilderDockWindow("Explorer", left);
    ImGui::DockBuilderDockWindow("Render View", center);
    ImGui::DockBuilderDockWindow("Properties", right);

    ImGui::DockBuilderFinish(dockspaceID);
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
        drawTitlebar("ReGuiEditor");

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

		// Maximize, Minimize, Close buttons
		const float buttonSize = ImGui::GetFrameHeight();
		const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float buttonArrayWidth = (buttonSize + buttonSpacing) * 3 - buttonSpacing;

		const ImVec2 buttonPos = { mainViewport->Size.x - buttonArrayWidth, 0.f };

		ImGui::SetCursorPos(buttonPos);
        
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

		Application& app = Application::getInstance();

        if (ImGui::Button(ICON_LC_MINUS, { buttonSize, buttonSize }))
            app.window.minimize();
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        ImGui::SameLine();

        if (app.window.isMaximized()) {
            if (ImGui::Button(ICON_LC_COPY, { buttonSize, buttonSize }))
                app.window.restore();
        } else {
            if (ImGui::Button(ICON_LC_SQUARE, { buttonSize, buttonSize }))
                app.window.maximize();
        }
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        ImGui::SameLine();

        if (ImGui::Button(ICON_LC_X, { buttonSize, buttonSize }))
            app.isRunning = false;
		TitlebarHitTest::addRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

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