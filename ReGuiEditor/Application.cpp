#include "Application.hpp"

#include <Services/ConfigurationService/ConfigurationService.hpp>
#include <Services/LuaService/LuaService.hpp>

#include <Services/Service.hpp>
#include <Services/ServiceManager.hpp>

#include "UGCManager/UGCManager.hpp"

#include "Filesystem/DirectoryManager.hpp"
#include "Filesystem/FilesystemManager.hpp"

#include "MyGuiRenderer/GuiSystemManager.hpp"

#include "GUI/UIManager.hpp"

#include "LayoutManager/LayoutManager.hpp"

#include "Window/Splashscreen.hpp"

#include <Modules/Application/ApplicationBase.hpp>
#include <Modules/Misc/Profiler.hpp>

#include <rend/Renderer.hpp>
#include <rend/ResourceManager.hpp>

#include <rend/Common/Color.hpp>

#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>
#include <rend/ImGui/RGE_ImGui_Impl_Win32.hpp>

#include <ImGui/imgui.h>

#include <Windows.h>

#include "Window/Window.hpp"
#include <filesystem>
#include <functional>

bool ReGuiEditor::MainApp::initializeRenderer() {
	SET_SPLASH_STATUS("Initializing renderer...");

	this->renderer = rend::Renderer::createInstance();
	if (!this->renderer->init())
		return false;

	this->swapChain = rend::ResourceManager::getInstance().createSwapChain(this->window.getHwnd());
	if (!this->swapChain)
		return false;

	return true;
}

bool ReGuiEditor::MainApp::initializeUI() {
	SET_SPLASH_STATUS("Initializing UI interface...");

	this->uiManager = UIManager::createInstance();
	if (!this->uiManager->initialize())
		return false;

	this->layoutManager = LayoutManager::createInstance();
	this->layoutManager->initialize();

	return true;
}

bool ReGuiEditor::MainApp::initializeUGC() {
	SET_SPLASH_STATUS("Loading workshop content...");

	this->ugcManager = UGCManager::createInstance();
	if (!this->ugcManager->initialize())
		return false;

	return true;
}

bool ReGuiEditor::MainApp::initializeMyGui() {
	SET_SPLASH_STATUS("Initializing MyGUI...");

	this->guiSystemManager = GuiSystemManager::createInstance();
	if (!this->guiSystemManager->initialize())
		return false;

	return true;
}

bool ReGuiEditor::MainApp::initializeServices() {
	SET_SPLASH_STATUS("Initializing services...");

	this->serviceManager = ServiceManager::createInstance();
	this->serviceManager->registerService<ConfigurationService>();
	this->serviceManager->registerService<LuaService>();
	if (!this->serviceManager->initializeServices(ReGuiEditor::ServiceInitStage::PreInit))
		return false;
	
	return true;

}

bool ReGuiEditor::MainApp::initialize() {
	PROFILE_FUNCTION();
	
	this->window.setOnCloseRequest(std::bind(&ReGuiEditor::MainApp::onCloseRequest, this));
	this->window.setOnResize(std::bind(&ReGuiEditor::MainApp::onWindowResize, this, std::placeholders::_1, std::placeholders::_2));
	this->window.setWindowProc(std::bind(&ReGuiEditor::MainApp::windowProc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	if (!this->window.create(L"ReGui Layout Editor", 1920, 1080))
		return false;

	this->directoryManager = ReGuiEditor::DirectoryManager::createInstance();
	if (!this->directoryManager->initialize())
		return false;

	this->filesystemManager = ReGuiEditor::FilesystemManager::createInstance();

	std::filesystem::path splashImagePath;
	if (this->directoryManager->resolveDirectory("$RGE_DATA/Images/splash.png", splashImagePath))
		ReGuiEditor::gSplashscreen.show(splashImagePath);

	if (!this->initializeRenderer()) return false;
	if (!this->initializeServices()) return false;
	if (!this->initializeUGC()) return false;
	if (!this->initializeUI()) return false;
	if (!this->initializeMyGui()) return false;

	if (!this->serviceManager->initializeServices(ReGuiEditor::ServiceInitStage::PostInit))
		return false;

	SET_SPLASH_STATUS("Starting...");

	this->isInitialized = true;
	return true;
}

void ReGuiEditor::MainApp::update() {
	PROFILE_FUNCTION();

	if (!this->isInitialized)
		return;

	if (this->window.wantsExit()) {
		this->isRunning_ = false;
		return;
	}

	this->window.update();

	this->serviceManager->callUpdate();
	this->uiManager->viewsManager->update(this->deltaTime_);
}

void ReGuiEditor::MainApp::tick() {
	PROFILE_FUNCTION();

	this->serviceManager->callTick();
	this->uiManager->viewsManager->tick();
}

void ReGuiEditor::MainApp::render() {
	PROFILE_FUNCTION();

	if(!this->swapChain->clearRenderTarget(rend::Color::darkGray()))
		CONSOLE_ERROR(L"Failed to clear render target");

	{
		PROFILE_SCOPE("Application::guiSystemRender");
		this->guiSystemManager->render();
	}

	RGE_ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	this->serviceManager->callRender();
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

		if(!this->swapChain->present(true))
			CONSOLE_ERROR(L"Failed to main present swap chain");
	}

	if (!this->isWindowShown) {
		this->window.setShowState(ReGuiEditor::WindowShowState::Show);
		ReGuiEditor::gSplashscreen.close();

		this->isWindowShown = true;
	}
}

LRESULT ReGuiEditor::MainApp::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (RGE_ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
		return true;

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ReGuiEditor::MainApp::onCloseRequest() {
	return true;
}

void ReGuiEditor::MainApp::onWindowResize(int width, int height) {
	if (!this->isInitialized || !this->swapChain || width <= 0 || height <= 0)
		return;

	if (!this->swapChain->resize(static_cast<UINT>(width), static_cast<UINT>(height))) {
		CONSOLE_ERROR(L"Failed to resize renderer to %dx%d", width, height);
		return;
	}

	this->render();
}

REGUIEDITOR_CREATE_APPLICATION_DEFINITION(ReGuiEditor::MainApp)