#pragma once
#include "Window/Window.hpp"

#include "Filesystem/DirectoryManager.hpp"
#include "Filesystem/FilesystemManager.hpp"
#include "GUI/UIManager.hpp"
#include "LayoutManager/LayoutManager.hpp"
#include "MyGuiRenderer/GuiSystemManager.hpp"

#include "Services/ServiceManager.hpp"

#include <Modules/Application/ApplicationBase.hpp>

#include <rend/Renderer.hpp>
#include <rend/Resources/SwapChain.hpp>

#include <Windows.h>

#include <memory>

namespace ReGuiEditor {
	class MainApp : public ReGuiEditor::ApplicationBase {
		bool initializeRenderer();
		bool initializeUI();
		bool initializeMyGui();
		bool initializeServices();
	public:
		bool isInitialized = false;
		bool isWindowShown = false;

		ReGuiEditor::Window window;
		
		std::shared_ptr<rend::Renderer> renderer = nullptr;
		rend::SwapChainHandle swapChain;
		
		std::shared_ptr<ReGuiEditor::ServiceManager> serviceManager = nullptr;

		std::shared_ptr<ReGuiEditor::DirectoryManager> directoryManager = nullptr;
		std::shared_ptr<ReGuiEditor::FilesystemManager> filesystemManager = nullptr;
		
		std::shared_ptr<GuiSystemManager> guiSystemManager = nullptr;
		std::shared_ptr<LayoutManager> layoutManager = nullptr;
		std::shared_ptr<UIManager> uiManager = nullptr;
		std::shared_ptr<LayoutManager> viewsManager = nullptr;

		bool initialize() override;

		void update() override;
		void tick() override;
		void render() override;

		LRESULT windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

		bool onCloseRequest() override;
		void onWindowResize(int width, int height) override;
	};

	template <>
	MainApp& ApplicationBase::getInstance<MainApp>();

	inline MainApp& getApp() { return ApplicationBase::getInstance<MainApp>(); }
}