#pragma once
#include <UTILS/Singleton.hpp>

#include "ConfigManager/ConfigManager.hpp"
#include "LuaManager/LuaManager.hpp"
#include "UGCManager/UGCManager.hpp"

#include "Filesystem/DirectoryManager.hpp"
#include "Filesystem/FilesystemManager.hpp"

#include "LayoutManager/LayoutManager.hpp"

#include "Window/Splashscreen.hpp"
#include "Window/Window.hpp"

#include "MyGuiRenderer/GuiSystemManager.hpp"

#include "GUI/UIManager.hpp"

#include <rend/Renderer.hpp>
#include <rend/Resources/SwapChain.hpp>

#include <Windows.h>

#include <memory>

class Application : public UTILS::Singleton<Application> {
public:
	bool isRunning = true;
	bool isInitialized = false;
	bool isWindowShown = false;

	Window window;
	Splashscreen splashscreen;

	std::shared_ptr<rend::Renderer> renderer = nullptr;
	rend::SwapChainHandle swapChain;

	std::shared_ptr<ConfigManager> configManager = nullptr;
	std::shared_ptr<DirectoryManager> directoryManager = nullptr;
	std::shared_ptr<FilesystemManager> filesystemManager = nullptr;
	std::shared_ptr<UGCManager> ugcManager = nullptr;
	
	std::shared_ptr<GuiSystemManager> guiSystemManager = nullptr;
	std::shared_ptr<LayoutManager> layoutManager = nullptr;
	std::shared_ptr<UIManager> uiManager = nullptr;

	std::shared_ptr<LuaManager> luaManager = nullptr;

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