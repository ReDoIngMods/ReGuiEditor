#include "TitlebarHitTest.hpp"
#include "Window.hpp"

#include <Windows.h>
#include <dwmapi.h>

#include <ImGui/imgui.h>

#pragma comment(lib, "dwmapi.lib")

constexpr int BORDER_THICKNESS = 8;
constexpr const wchar_t* CLASS_NAME = L"REGUI_EDITOR_WINDOW_CLASS";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCCREATE) {
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);

		Window* window = static_cast<Window*>(createStruct->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}

	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (!window)
		return DefWindowProcW(hwnd, message, wParam, lParam);

	switch (message) {
	case WM_NCCALCSIZE:
		if (!wParam)
			break;

		if (IsZoomed(hwnd)) {
			NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
			HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

			MONITORINFO monitorInfo;
			ZeroMemory(&monitorInfo, sizeof(MONITORINFO));
			monitorInfo.cbSize = sizeof(MONITORINFO);

			GetMonitorInfoW(monitor, &monitorInfo);
			params->rgrc[0] = monitorInfo.rcWork;
		}

		return 0;
	case WM_NCHITTEST: {
		LRESULT isHit = DefWindowProcW(hwnd, message, wParam, lParam);
		if (isHit != HTCLIENT)
			return isHit;

		POINT screenPoint{ LOWORD(lParam), HIWORD(lParam) };

		POINT cursorPosition = screenPoint;
		ScreenToClient(hwnd, &cursorPosition);

		RECT clientRectangle;
		GetClientRect(hwnd, &clientRectangle);

		bool isOnLeftBorder = cursorPosition.x < BORDER_THICKNESS;
		bool isOnRightBorder = cursorPosition.x >= clientRectangle.right - BORDER_THICKNESS;
		bool isOnTopBorder = cursorPosition.y < BORDER_THICKNESS;
		bool isOnBottomBorder = cursorPosition.y >= clientRectangle.bottom - BORDER_THICKNESS;

		if (isOnLeftBorder && isOnTopBorder) return HTTOPLEFT;
		if (isOnRightBorder && isOnTopBorder) return HTTOPRIGHT;
		if (isOnLeftBorder && isOnBottomBorder) return HTBOTTOMLEFT;
		if (isOnRightBorder && isOnBottomBorder) return HTBOTTOMRIGHT;

		if (isOnLeftBorder) return HTLEFT;
		if (isOnRightBorder) return HTRIGHT;
		if (isOnTopBorder) return HTTOP;
		if (isOnBottomBorder) return HTBOTTOM;

		if (cursorPosition.y < ImGui::GetFrameHeightWithSpacing() && !TitlebarHitTest::isOverWidget({ (float)screenPoint.x, (float)screenPoint.y }))
			return HTCAPTION;

		return HTCLIENT;
	}
	case WM_CLOSE:
		if (window->onCloseRequest_) {
			if (window->onCloseRequest_()) {
				CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE: Close request approved by application");
			}
			else {
				CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE: Close request denied by application");
				return 0;
			}
		}
		else {
			CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE");
		}

		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;

		if (window->onResize_)
			window->onResize_(LOWORD(lParam), HIWORD(lParam));

		return 0;
	}

	if (window->windowProc_)
		return window->windowProc_(hwnd, message, wParam, lParam);

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool Window::create(const wchar_t* title, int width, int height) {
	WNDCLASSW wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

	if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to register window class");
		return false;
	}

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	int windowX = (screenWidth - width) / 2;
	int windowY = (screenHeight - height) / 2;

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GREEN, L"Creating window at (%d, %d) with size (%d, %d)", windowX, windowY, width, height);

	this->hwnd_ = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, windowX, windowY, width, height, NULL, NULL, wc.hInstance, this);
	if (!this->hwnd_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to create window");
		return false;
	}

	SetWindowPos(this->hwnd_, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_DONOTROUND;
	DwmSetWindowAttribute(this->hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

	UpdateWindow(this->hwnd_);
	return true;
}

void Window::cleanup() {
	if (this->hwnd_) {
		DestroyWindow(this->hwnd_);
		this->hwnd_ = nullptr;
	}
}

void Window::update() {
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);

		if (message.message == WM_QUIT)
			this->wantsExit_ = true;
	}
}

void Window::maximize() const { ShowWindowAsync(this->hwnd_, SW_MAXIMIZE); }
void Window::minimize() const { ShowWindowAsync(this->hwnd_, SW_MINIMIZE); }

void Window::restore() const { ShowWindowAsync(this->hwnd_, SW_RESTORE); }
void Window::show() const { ShowWindowAsync(this->hwnd_, SW_SHOW); }

bool Window::isMaximized() const {
	WINDOWPLACEMENT placement;
	ZeroMemory(&placement, sizeof(WINDOWPLACEMENT));
	placement.length = sizeof(WINDOWPLACEMENT);

	GetWindowPlacement(this->hwnd_, &placement);

	return placement.showCmd == SW_MAXIMIZE;
}