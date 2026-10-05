#pragma once
#include <Windows.h>

#include <functional>
#include <utility>

class Window {
	HWND hwnd_ = nullptr;
	std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> windowProc_;
	std::function<void(int, int)> onResize_;
	std::function<bool()> onCloseRequest_;

	bool wantsExit_ = false;

	friend LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
public:
	bool create(const wchar_t* title, int width, int height);
	void cleanup();

	void update();

	void maximize() const;
	void minimize() const;
	void restore() const;
	void show() const;

	bool isMaximized() const;

	void setWindowProc(std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> windowProc) { this->windowProc_ = std::move(windowProc); }
	void setOnCloseRequest(std::function<bool()> onCloseRequest) { this->onCloseRequest_ = std::move(onCloseRequest); }
	void setOnResize(std::function<void(int, int)> onResize) { this->onResize_ = std::move(onResize); }

	HWND getHwnd() const { return this->hwnd_; }
	bool wantsExit() const { return this->wantsExit_; }
};