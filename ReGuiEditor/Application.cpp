#include "Application.hpp"

#include "ConfigManager/ConfigManager.hpp"
#include "LuaManager/LuaManager.hpp"
#include "UGCManager/UGCManager.hpp"

#include "Filesystem/DirectoryManager.hpp"
#include "Filesystem/FilesystemManager.hpp"

#include "MyGuiRenderer/GuiSystemManager.hpp"

#include "GUI/UIManager.hpp"

#include "LayoutManager/LayoutManager.hpp"

#include <rend/Renderer.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Common/Color.hpp>

#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_Win32.hpp>

#include <UTILS/Profiler.hpp>

#include <ImGui/imgui.h>

#include <Windows.h>

#include <filesystem>
#include <functional>

bool Application::initialize() {
	PROFILE_FUNCTION();

	QueryPerformanceFrequency(&this->frequency);
	QueryPerformanceCounter(&this->counter);

	this->window.setOnCloseRequest(std::bind(&Application::onCloseRequest, this));
	this->window.setOnResize(std::bind(&Application::onWindowResize, this, std::placeholders::_1, std::placeholders::_2));
	this->window.setWindowProc(std::bind(&Application::windowProc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	if (!this->window.create(L"ReGui Layout Editor", 1920, 1080))
		return false;

	this->directoryManager = DirectoryManager::createInstance();
	if (!this->directoryManager->initialize())
		return false;

	this->filesystemManager = FilesystemManager::createInstance();

	std::filesystem::path splashImagePath;
	if (this->directoryManager->resolveDirectory("$RGE_DATA/Images/splash.png", splashImagePath))
		this->splashscreen.show(splashImagePath);

	this->splashscreen.setStatus(L"Initializing renderer...");
	this->renderer = rend::Renderer::createInstance();
	if (!this->renderer->init())
		return false;

	this->swapChain = rend::ResourceManager::getInstance().createSwapChain(this->window.getHwnd());
	if (!this->swapChain)
		return false;

	this->splashscreen.setStatus(L"Loading workshop content...");
	this->ugcManager = UGCManager::createInstance();
	if (!this->ugcManager->initialize())
		return false;

	this->configManager = ConfigManager::createInstance();

	this->splashscreen.setStatus(L"Initializing interface...");
	this->uiManager = UIManager::createInstance();
	if (!this->uiManager->initialize())
		return false;

	this->splashscreen.setStatus(L"Loading GUI resources...");
	this->guiSystemManager = GuiSystemManager::createInstance();
	if (!this->guiSystemManager->initialize())
		return false;

	this->splashscreen.setStatus(L"Loading Layout manager...");
	this->layoutManager = LayoutManager::createInstance();
	this->layoutManager->initialize();

	this->splashscreen.setStatus(L"Starting Lua Engine...");
	this->luaManager = LuaManager::createInstance();
	if (!this->luaManager->initialize())
		return false;

	this->splashscreen.setStatus(L"Starting...");

	this->isInitialized = true;
	return true;
}

void Application::update() {
	PROFILE_FUNCTION();

	if (this->window.wantsExit()) {
		this->isRunning = false;
		return;
	}

	LARGE_INTEGER currentCounter;
	QueryPerformanceCounter(&currentCounter);

	this->deltaTime = static_cast<double>(currentCounter.QuadPart - this->counter.QuadPart) / static_cast<double>(this->frequency.QuadPart);
	this->counter = currentCounter;

	this->window.update();

	this->uiManager->viewsManager->update(this->deltaTime);

	this->fixedUpdateTimeAccumulator += this->deltaTime;
	while (this->fixedUpdateTimeAccumulator >= 1.0 / 40.0) {
		this->fixedUpdate();
		this->fixedUpdateTimeAccumulator -= 1.0 / 40.0;
	}
}

void Application::fixedUpdate() {
	PROFILE_FUNCTION();

	this->luaManager->update(static_cast<float>(this->deltaTime));
	this->uiManager->viewsManager->tick();
}

void Application::render(bool vsyncEnabled) {
	PROFILE_FUNCTION();

	{
		PROFILE_SCOPE("Application::luaTick");
		this->luaManager->tick();
	}

	if(!this->swapChain->clearRenderTarget(rend::Color::darkGray()))
		CONSOLE_ERROR(L"Failed to clear render target");

	{
		PROFILE_SCOPE("Application::guiSystemRender");
		this->guiSystemManager->render();
	}

	RGE_ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		PROFILE_SCOPE("Application::viewsRender");
		this->uiManager->viewsManager->render();
	}

	ImGui::Render();
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	{
		PROFILE_SCOPE("Application::imguiDrawData");
		RGE_ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData(), this->swapChain->getRenderTargetView());
	}
	
	{
		PROFILE_SCOPE("Application::present");

		if(!this->swapChain->present(vsyncEnabled))
			CONSOLE_ERROR(L"Failed to main present swap chain");
	}

	// The window stays hidden until the first frame is on screen, so it never opens blank.
	if (!this->isWindowShown) {
		this->window.show();
		this->splashscreen.close();
		this->isWindowShown = true;
	}
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

	if (!this->swapChain->resize(static_cast<UINT>(width), static_cast<UINT>(height))) {
		CONSOLE_ERROR(L"Failed to resize renderer to %dx%d", width, height);
		return;
	}

	this->render(false);
}

void Application::onResize(int width, int height) {
	if (!this->isInitialized)
		return;

	this->render(false);
}