#pragma once
#include <UTILS/Singleton.hpp>

#include "ConfigManager/ConfigManager.hpp"
#include "FilesystemManager/FilesystemManager.hpp"
#include "LuaManager/LuaManager.hpp"
#include "UGCManager/UGCManager.hpp"

#include "Window/Splashscreen.hpp"
#include "Window/Window.hpp"

#include "MyGuiRenderer/GuiSystemManager.hpp"

#include "GUI/UIManager.hpp"

#include <rend/Renderer.hpp>
#include <rend/Resources/SwapChain.hpp>

#include <Windows.h>

class Application : public UTILS::Singleton<Application> {
public:
	Application() = default;
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	bool isRunning = true;
	bool isInitialized = false;
	bool isWindowShown = false;

	Window window;
	Splashscreen splashscreen;

	rend::Renderer* renderer = nullptr;
	rend::SwapChainHandle swapChain;

	ConfigManager* configManager = nullptr;
	FilesystemManager* filesystemManager = nullptr;
	UGCManager* ugcManager = nullptr;
	GuiSystemManager* guiSystemManager = nullptr;
	LuaManager* luaManager = nullptr;
	UIManager* uiManager = nullptr;

	LARGE_INTEGER frequency{};
	LARGE_INTEGER counter{};
	double deltaTime = 0.0;
	double fixedUpdateTimeAccumulator = 0.0;

	bool initialize();

	void update();
	void fixedUpdate();
	void render(bool vsyncEnabled = true);

	LRESULT windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	bool onCloseRequest();
	void onWindowResize(int width, int height);
	void onResize(int width, int height);
};