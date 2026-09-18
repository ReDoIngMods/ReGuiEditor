#pragma once
#include <UTILS/Singleton.hpp>

#include "ConfigManager/ConfigManager.hpp"
#include "DirectoryManager/DirectoryManager.hpp"

#include "Window/D3D11Device.hpp"
#include "Window/Window.hpp"

#include "GUI/FontManager.hpp"
#include "GUI/GuiSystemManager.hpp"
#include "GUI/WindowDecoration.hpp"

#include "GUI/Panels/ExplorerPanel.hpp"
#include "GUI/Panels/PropertiesPanel.hpp"
#include "GUI/Panels/RenderPanel.hpp"

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

	Window window;
	D3D11Device d3d11Device;

	ConfigManager* configManager = nullptr;
	DirectoryManager* directoryManager = nullptr;
	FontManager* fontManager = nullptr;
	GuiSystemManager* guiSystemManager = nullptr;

	ExplorerPanel explorerPanel;
	PropertiesPanel propertiesPanel;
	RenderPanel renderPanel;	

	WindowDecoration windowDecoration;

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
	void onResize(int width, int height);
};