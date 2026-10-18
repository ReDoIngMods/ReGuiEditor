#include "Application.hpp"

#include <rend/Renderer.hpp>

#include <rend/Common/Color.hpp>

#include <rend/ImGui/RGE_ImGui_FreeType.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_Win32.hpp>

#include "ConfigManager/ConfigManager.hpp"
#include "FilesystemManager/FilesystemManager.hpp"
#include "LuaManager/LuaManager.hpp"
#include "UGCManager/UGCManager.hpp"

#include "GUI/FontManager.hpp"
#include "GUI/GuiSystemManager.hpp"
#include "GUI/ImageManager.hpp"
#include "GUI/NotificationManager.hpp"

#include <ImGui/imgui.h>

#include <Windows.h>

#include <functional>

void applyImGuiTheme();

Application::~Application() {
	this->explorerPanel.cleanup();
	this->propertiesPanel.cleanup();
	this->renderPanel.cleanup();

	if (this->luaManager) {
		this->luaManager->destroyInstance();
		this->luaManager = nullptr;
	}

	if (this->guiSystemManager) {
		this->guiSystemManager->destroyInstance();
		this->guiSystemManager = nullptr;
	}

	if (this->imageManager) {
		this->imageManager->destroyInstance();
		this->imageManager = nullptr;
	}

	if (this->fontManager) {
		this->fontManager->destroyInstance();
		this->fontManager = nullptr;
	}

	if (this->configManager) {
		this->configManager->destroyInstance();
		this->configManager = nullptr;
	}

	if (this->ugcManager) {
		this->ugcManager->destroyInstance();
		this->ugcManager = nullptr;
	}

	if (ImGui::GetCurrentContext()) {
		RGE_ImGui_ImplDX11_Shutdown();
		RGE_ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
	}

	this->swapChain.reset();

	if (this->renderer) {
		this->renderer->destroyInstance();
		this->renderer = nullptr;
	}

	if (this->filesystemManager) {
		this->filesystemManager->destroyInstance();
		this->filesystemManager = nullptr;
	}

	this->window.cleanup();
}

bool Application::initialize() {
	QueryPerformanceFrequency(&this->frequency);
	QueryPerformanceCounter(&this->counter);

	this->window.setOnCloseRequest(std::bind(&Application::onCloseRequest, this));
	this->window.setOnResize(std::bind(&Application::onWindowResize, this, std::placeholders::_1, std::placeholders::_2));
	this->window.setWindowProc(std::bind(&Application::windowProc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	if (!this->window.create(L"ReGui Layout Editor", 1920, 1080)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create window");
		return false;
	}

	this->filesystemManager = FilesystemManager::createInstance();
	if (!this->filesystemManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize FilesystemManager");
		return false;
	}

	this->renderer = rend::Renderer::createInstance();
	if (!this->renderer->init(this->window.getHwnd())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize renderer");
		return false;
	}

	this->swapChain = this->renderer->getResourceManager()->createSwapChain(this->window.getHwnd());
	if (!this->swapChain) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create swap chain");
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.Fonts->SetFontLoader(RGE_ImGui_FreeType_GetFontLoader());

	applyImGuiTheme();

	if (!RGE_ImGui_ImplWin32_Init(this->window.getHwnd())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize ImGui Win32 backend");
		return false;
	}

	if (!RGE_ImGui_ImplDX11_Init()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize ImGui DX11 backend");
		return false;
	}

	io.Fonts->SetFontLoader(RGE_ImGui_FreeType_GetFontLoader());

	this->ugcManager = UGCManager::createInstance();
	if (!this->ugcManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize UGCManager");
		return false;
	}

	this->configManager = ConfigManager::createInstance();

	this->fontManager = FontManager::createInstance();
	if (!this->fontManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize FontManager");
		return false;
	}

	this->imageManager = ImageManager::createInstance();
	if (!this->imageManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize ImageManager");
		return false;
	}

	this->guiSystemManager = GuiSystemManager::createInstance();
	if (!this->guiSystemManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize GuiSystemManager");
		return false;
	}

	this->luaManager = LuaManager::createInstance();
	if (!this->luaManager->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize LuaManager");
		return false;
	}

	if (!this->explorerPanel.initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize ExplorerPanel");
		return false;
	}

	if (!this->propertiesPanel.initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize PropertiesPanel");
		return false;
	}

	if (!this->renderPanel.initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize RenderPanel");
		return false;
	}

	this->window.show();
	this->isInitialized = true;
	return true;
}

void Application::update() {
	if (this->window.wantsExit()) {
		this->isRunning = false;
		return;
	}

	LARGE_INTEGER currentCounter;
	QueryPerformanceCounter(&currentCounter);

	this->deltaTime = static_cast<double>(currentCounter.QuadPart - this->counter.QuadPart) / static_cast<double>(this->frequency.QuadPart);
	this->counter = currentCounter;

	this->window.update();

	NotificationManager::getInstance().update(static_cast<float>(this->deltaTime));

	this->fixedUpdateTimeAccumulator += this->deltaTime;
	while (this->fixedUpdateTimeAccumulator >= 1.0 / 40.0) {
		this->fixedUpdate();
		this->fixedUpdateTimeAccumulator -= 1.0 / 40.0;
	}
}

void Application::fixedUpdate() {
	this->luaManager->update(static_cast<float>(this->deltaTime));

	this->explorerPanel.update();
	this->propertiesPanel.update();
	this->renderPanel.update();
}

void Application::render(bool vsyncEnabled) {
	this->luaManager->tick();

	if(!this->swapChain->clearRenderTarget(rend::Color::darkGray()))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to clear render target");

	this->guiSystemManager->render();

	RGE_ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	this->windowDecoration.render();

	this->explorerPanel.render();
	this->propertiesPanel.render();
	this->renderPanel.render();

	ImGui::Render();
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	RGE_ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData(), this->swapChain->getRenderTargetView());
	
	if(!this->swapChain->present(vsyncEnabled))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to main present swap chain");
}

LRESULT Application::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (RGE_ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
		return true;

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool Application::onCloseRequest() {
	return true;
}

void Application::onWindowResize(int width, int height) {
	if (!this->isInitialized || !this->swapChain || width <= 0 || height <= 0)
		return;

	if (!this->swapChain->resize(static_cast<UINT>(width), static_cast<UINT>(height)))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to resize renderer to %dx%d", width, height);
}

void Application::onResize(int width, int height) {
	if (!this->isInitialized)
		return;

	this->render(false);
}

void applyImGuiTheme() {
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