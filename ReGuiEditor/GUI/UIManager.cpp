#include "UIManager.hpp"

#include "ImGuiFontManager.hpp"
#include "ImageManager.hpp"
#include "NotificationManager.hpp"
#include "ViewsManager.hpp"

#include "Views/DecorationView.hpp"
#include "Views/ExplorerView.hpp"
#include "Views/PropertiesView.hpp"
#include "Views/UIRenderView.hpp"

#include <rend/ImGui/RGE_ImGui_FreeType.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_Win32.hpp>

#include <Application.hpp>

#include <UTILS/Profiler.hpp>

#include <ImGui/imgui.h>

void UIManager::applyImGuiTheme() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	const ImVec4 background = { 0.039f, 0.039f, 0.043f, 1.00f }; // hsl(240 10% 3.9%)
	const ImVec4 foreground = { 0.980f, 0.980f, 0.980f, 1.00f }; // hsl(0 0% 98%)
	const ImVec4 card = { 0.055f, 0.055f, 0.060f, 1.00f };
	const ImVec4 secondary = { 0.159f, 0.159f, 0.176f, 1.00f }; // hsl(240 3.7% 15.9%)
	const ImVec4 mutedFg = { 0.639f, 0.639f, 0.659f, 1.00f }; // hsl(240 5% 64.9%)
	const ImVec4 accent = { 0.200f, 0.200f, 0.220f, 1.00f };
	const ImVec4 border = { 0.159f, 0.159f, 0.176f, 1.00f };
	const ImVec4 destructive = { 0.450f, 0.100f, 0.100f, 1.00f }; // hsl(0 62.8% 30.6%)
	const ImVec4 ring = foreground;

	colors[ImGuiCol_Text] = foreground;
	colors[ImGuiCol_TextDisabled] = mutedFg;
	colors[ImGuiCol_WindowBg] = background;
	colors[ImGuiCol_ChildBg] = background;
	colors[ImGuiCol_PopupBg] = card;
	colors[ImGuiCol_Border] = border;
	colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	colors[ImGuiCol_FrameBg] = secondary;
	colors[ImGuiCol_FrameBgHovered] = accent;
	colors[ImGuiCol_FrameBgActive] = accent;
	colors[ImGuiCol_TitleBg] = background;
	colors[ImGuiCol_TitleBgActive] = background;
	colors[ImGuiCol_TitleBgCollapsed] = background;
	colors[ImGuiCol_MenuBarBg] = card;
	colors[ImGuiCol_ScrollbarBg] = background;
	colors[ImGuiCol_ScrollbarGrab] = secondary;
	colors[ImGuiCol_ScrollbarGrabHovered] = accent;
	colors[ImGuiCol_ScrollbarGrabActive] = ring;
	colors[ImGuiCol_CheckMark] = foreground;
	colors[ImGuiCol_CheckboxSelectedBg] = secondary;
	colors[ImGuiCol_SliderGrab] = mutedFg;
	colors[ImGuiCol_SliderGrabActive] = foreground;
	colors[ImGuiCol_Button] = secondary;
	colors[ImGuiCol_ButtonHovered] = accent;
	colors[ImGuiCol_ButtonActive] = ring;
	colors[ImGuiCol_Header] = secondary;
	colors[ImGuiCol_HeaderHovered] = accent;
	colors[ImGuiCol_HeaderActive] = accent;
	colors[ImGuiCol_Separator] = border;
	colors[ImGuiCol_SeparatorHovered] = mutedFg;
	colors[ImGuiCol_SeparatorActive] = foreground;
	colors[ImGuiCol_ResizeGrip] = secondary;
	colors[ImGuiCol_ResizeGripHovered] = accent;
	colors[ImGuiCol_ResizeGripActive] = ring;
	colors[ImGuiCol_InputTextCursor] = foreground;
	colors[ImGuiCol_TabHovered] = accent;
	colors[ImGuiCol_Tab] = background;
	colors[ImGuiCol_TabSelected] = secondary;
	colors[ImGuiCol_TabSelectedOverline] = ring;
	colors[ImGuiCol_TabDimmed] = background;
	colors[ImGuiCol_TabDimmedSelected] = secondary;
	colors[ImGuiCol_TabDimmedSelectedOverline] = border;
	colors[ImGuiCol_DockingPreview] = { ring.x, ring.y, ring.z, 0.35f };
	colors[ImGuiCol_DockingEmptyBg] = background;
	colors[ImGuiCol_PlotLines] = mutedFg;
	colors[ImGuiCol_PlotLinesHovered] = foreground;
	colors[ImGuiCol_PlotHistogram] = mutedFg;
	colors[ImGuiCol_PlotHistogramHovered] = foreground;
	colors[ImGuiCol_TableHeaderBg] = card;
	colors[ImGuiCol_TableBorderStrong] = border;
	colors[ImGuiCol_TableBorderLight] = border;
	colors[ImGuiCol_TableRowBg] = background;
	colors[ImGuiCol_TableRowBgAlt] = card;
	colors[ImGuiCol_TextLink] = foreground;
	colors[ImGuiCol_TextSelectedBg] = { foreground.x, foreground.y, foreground.z, 0.25f };
	colors[ImGuiCol_TreeLines] = border;
	colors[ImGuiCol_DragDropTarget] = destructive;
	colors[ImGuiCol_DragDropTargetBg] = { destructive.x, destructive.y, destructive.z, 0.20f };
	colors[ImGuiCol_UnsavedMarker] = mutedFg;
	colors[ImGuiCol_NavCursor] = ring;
	colors[ImGuiCol_NavWindowingHighlight] = { foreground.x, foreground.y, foreground.z, 0.70f };
	colors[ImGuiCol_NavWindowingDimBg] = { background.x, background.y, background.z, 0.60f };
	colors[ImGuiCol_ModalWindowDimBg] = { background.x, background.y, background.z, 0.60f };

	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.WindowPadding = { 10, 10 };
	style.FramePadding = { 8, 5 };
	style.ItemSpacing = { 8, 6 };
}

bool UIManager::initialize() {
	PROFILE_FUNCTION();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.Fonts->SetFontLoader(RGE_ImGui_FreeType_GetFontLoader());

	applyImGuiTheme();

	Application& app = Application::getInstance();
	if (!RGE_ImGui_ImplWin32_Init(app.window.getHwnd())) {
		CONSOLE_ERROR(L"Failed to initialize ImGui Win32 backend");
		return false;
	}

	if (!RGE_ImGui_ImplDX11_Init()) {
		CONSOLE_ERROR(L"Failed to initialize ImGui DX11 backend");
		return false;
	}

	io.Fonts->SetFontLoader(RGE_ImGui_FreeType_GetFontLoader());

	this->debugTextHandler.initialize();

	this->fontManager = ImGuiFontManager::createInstance();
	if (!this->fontManager->initialize()) {
		CONSOLE_ERROR("Failed to initialize ImGuiFontManager.");
		return false;
	}

	this->imageManager = ImageManager::createInstance();
	if (!this->imageManager->initialize()) {
		CONSOLE_ERROR("Failed to initialize ImageManager.");
		return false;
	}

	this->notificationManager = NotificationManager::createInstance();
	if (!this->notificationManager) {
		CONSOLE_ERROR("Failed to create NotificationManager.");
		return false;
	}


	this->viewsManager = ViewsManager::createInstance();
	if (!this->viewsManager) {
		CONSOLE_ERROR("Failed to create ViewsManager.");
		return false;
	}

	this->viewsManager->registerView<DecorationView>("Decoration");
	this->viewsManager->registerView<ExplorerView>("Explorer");
	this->viewsManager->registerView<PropertiesView>("Properties");
	this->viewsManager->registerView<UIRenderView>("Render View");

	return true;
}

void UIManager::cleanup() {
	if (this->viewsManager) {
		this->viewsManager->destroyInstance();
		this->viewsManager = nullptr;
	}

	if (this->notificationManager) {
		this->notificationManager->destroyInstance();
		this->notificationManager = nullptr;
	}

	if (this->imageManager) {
		this->imageManager->destroyInstance();
		this->imageManager = nullptr;
	}

	if (this->fontManager) {
		this->fontManager->destroyInstance();
		this->fontManager = nullptr;
	}

	if (ImGui::GetCurrentContext()) {
		RGE_ImGui_ImplDX11_Shutdown();
		RGE_ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
	}
}
