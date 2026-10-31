#include "UIManager.hpp"

#include <Application.hpp>

#include "ImGuiFontManager.hpp"
#include "ImageManager.hpp"
#include "NotificationManager.hpp"
#include "ViewsManager.hpp"

#include "FilePickerManager.hpp"

#include "Views/DecorationView.hpp"
#include "Views/ExplorerView.hpp"
#include "Views/PropertiesView.hpp"
#include "Views/UIRenderView.hpp"

#include <Modules/Misc/Profiler.hpp>

#include <rend/ImGui/RGE_ImGui_FreeType.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_Win32.hpp>

#include <ImGui/imgui.h>

void UIManager::applyImGuiTheme() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	constexpr auto gray = [](unsigned v, float a = 1.0f) {
		const float f = v / 255.0f;
		return ImVec4{ f, f, f, a };
	};

	constexpr auto orange = [](float a = 1.0f) { return ImVec4{ 0.961f, 0.647f, 0.141f, a }; }; // #F5A524

	const ImVec4 clear = gray(0, 0.0f);
	const ImVec4 inset = gray(0x0F);
	const ImVec4 bgDeep = gray(0x14);
	const ImVec4 panel = gray(0x1A);
	const ImVec4 popup = gray(0x22);
	const ImVec4 line = gray(0x2B);

	const ImVec4 button = gray(0x26);
	const ImVec4 buttonHover = gray(0x30);
	const ImVec4 buttonActive = gray(0x3A);

	const ImVec4 text = gray(0xED);
	const ImVec4 textDisabled = gray(0x6B);
	const ImVec4 accent = orange();
	const ImVec4 accentSoft = orange(0.30f);

	colors[ImGuiCol_Text] = text;
	colors[ImGuiCol_TextDisabled] = textDisabled;
	colors[ImGuiCol_WindowBg] = panel;
	colors[ImGuiCol_ChildBg] = clear;
	colors[ImGuiCol_PopupBg] = popup;
	colors[ImGuiCol_Border] = line;
	colors[ImGuiCol_BorderShadow] = clear;

	colors[ImGuiCol_FrameBg] = inset;
	colors[ImGuiCol_FrameBgHovered] = gray(0x16);
	colors[ImGuiCol_FrameBgActive] = gray(0x16);

	colors[ImGuiCol_TitleBg] = bgDeep;
	colors[ImGuiCol_TitleBgActive] = bgDeep;
	colors[ImGuiCol_TitleBgCollapsed] = bgDeep;
	colors[ImGuiCol_MenuBarBg] = bgDeep;

	colors[ImGuiCol_ScrollbarBg] = clear;
	colors[ImGuiCol_ScrollbarGrab] = gray(0x3A);
	colors[ImGuiCol_ScrollbarGrabHovered] = gray(0x4C);
	colors[ImGuiCol_ScrollbarGrabActive] = gray(0x5E);

	colors[ImGuiCol_CheckMark] = accent;
	colors[ImGuiCol_CheckboxSelectedBg] = inset;
	colors[ImGuiCol_SliderGrab] = gray(0x70);
	colors[ImGuiCol_SliderGrabActive] = accent;

	colors[ImGuiCol_Button] = button;
	colors[ImGuiCol_ButtonHovered] = buttonHover;
	colors[ImGuiCol_ButtonActive] = buttonActive;

	// White overlays on neutral gray stay neutral
	colors[ImGuiCol_Header] = gray(0xFF, 0.10f);
	colors[ImGuiCol_HeaderHovered] = gray(0xFF, 0.06f);
	colors[ImGuiCol_HeaderActive] = gray(0xFF, 0.14f);

	colors[ImGuiCol_Separator] = line;
	colors[ImGuiCol_SeparatorHovered] = accent;
	colors[ImGuiCol_SeparatorActive] = accent;
	colors[ImGuiCol_ResizeGrip] = clear;
	colors[ImGuiCol_ResizeGripHovered] = accent;
	colors[ImGuiCol_ResizeGripActive] = accent;
	colors[ImGuiCol_InputTextCursor] = text;

	colors[ImGuiCol_Tab] = bgDeep;
	colors[ImGuiCol_TabHovered] = gray(0x22);
	colors[ImGuiCol_TabSelected] = panel;
	colors[ImGuiCol_TabSelectedOverline] = accent;
	colors[ImGuiCol_TabDimmed] = bgDeep;
	colors[ImGuiCol_TabDimmedSelected] = panel;
	colors[ImGuiCol_TabDimmedSelectedOverline] = gray(0x48);

	colors[ImGuiCol_DockingPreview] = accentSoft;
	colors[ImGuiCol_DockingEmptyBg] = bgDeep;

	colors[ImGuiCol_PlotLines] = gray(0x92);
	colors[ImGuiCol_PlotLinesHovered] = accent;
	colors[ImGuiCol_PlotHistogram] = gray(0x92);
	colors[ImGuiCol_PlotHistogramHovered] = accent;

	colors[ImGuiCol_TableHeaderBg] = bgDeep;
	colors[ImGuiCol_TableBorderStrong] = line;
	colors[ImGuiCol_TableBorderLight] = gray(0x24);
	colors[ImGuiCol_TableRowBg] = clear;
	colors[ImGuiCol_TableRowBgAlt] = gray(0xFF, 0.025f);

	colors[ImGuiCol_TextLink] = accent;
	colors[ImGuiCol_TextSelectedBg] = accentSoft;
	colors[ImGuiCol_TreeLines] = gray(0x3A);
	colors[ImGuiCol_DragDropTarget] = accent;
	colors[ImGuiCol_DragDropTargetBg] = orange(0.15f);
	colors[ImGuiCol_UnsavedMarker] = accent;
	colors[ImGuiCol_NavCursor] = accent;
	colors[ImGuiCol_NavWindowingHighlight] = gray(0xFF, 0.60f);
	colors[ImGuiCol_NavWindowingDimBg] = gray(0, 0.50f);
	colors[ImGuiCol_ModalWindowDimBg] = gray(0, 0.55f);

	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.PopupRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;

	style.WindowBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBarBorderSize = 1.0f;
	style.TabBarOverlineSize = 2.0f;

	// Looser than before
	style.WindowPadding = { 10, 10 };
	style.FramePadding = { 8, 6 };
	style.ItemSpacing = { 8, 7 };
	style.ItemInnerSpacing = { 6, 6 };
	style.IndentSpacing = 16.0f;
	style.ScrollbarSize = 12.0f;
	style.GrabMinSize = 10.0f;
	style.WindowMenuButtonPosition = ImGuiDir_None;
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
	this->viewsManager->registerView<UIRenderView>("Render");

	this->filePickerManager = FilePickerManager::createInstance();
	this->filePickerManager->initialize();

	if(!this->fileIconHandler.initialize()) {
		CONSOLE_ERROR("Failed to initialize FileIconHandler.");
		return false;
	}

	return true;
}

void UIManager::cleanup() {
	if (ImGui::GetCurrentContext()) {
		RGE_ImGui_ImplDX11_Shutdown();
		RGE_ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
	}
}
