#pragma once
#include "TitlebarHitTest.hpp"

#include <Windows.h>

#include <functional>
#include <utility>

namespace ReGuiEditor {
	enum class WindowShowState {
		Show,
		Hide,

		Minimize,
		Maximize,
		Restore,
	};

	class Window {
		HWND hwnd_ = nullptr;
		std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> windowProc_;
		std::function<void(int, int)> onResize_;
		std::function<bool()> onCloseRequest_;

		bool wantsExit_ = false;

		ReGuiEditor::TitlebarHitTest titlebarHitTest_;

		friend LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	public:
		Window() = default;
		~Window() { this->cleanup(); }

		bool create(const wchar_t* title, int width, int height);
		void cleanup();

		void update();

		void setShowState(ReGuiEditor::WindowShowState state) const;

		bool isMaximized() const;
		bool isMinimized() const;

		void setWindowProc(std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> windowProc) { this->windowProc_ = std::move(windowProc); }
		void setOnCloseRequest(std::function<bool()> onCloseRequest) { this->onCloseRequest_ = std::move(onCloseRequest); }
		void setOnResize(std::function<void(int, int)> onResize) { this->onResize_ = std::move(onResize); }

		HWND getHwnd() const { return this->hwnd_; }
		bool wantsExit() const { return this->wantsExit_; }

		ReGuiEditor::TitlebarHitTest& getTitlebarHitTest() { return this->titlebarHitTest_; }
		const ReGuiEditor::TitlebarHitTest& getTitlebarHitTest() const { return this->titlebarHitTest_; }
	};
}