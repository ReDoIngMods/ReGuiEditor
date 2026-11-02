#include "TitlebarHitTest.hpp"
#include "Window.hpp"

#include <Windows.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <ImGui/imgui.h>

#pragma comment(lib, "dwmapi.lib")

constexpr int BORDER_THICKNESS = 8;
constexpr const wchar_t* CLASS_NAME = L"REGUI_EDITOR_WINDOW_CLASS";

LRESULT CALLBACK ReGuiEditor::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCCREATE) {
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);

		ReGuiEditor::Window* window = static_cast<ReGuiEditor::Window*>(createStruct->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}

	ReGuiEditor::Window* self = reinterpret_cast<ReGuiEditor::Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (!self)
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

		POINT screenPoint;
		ZeroMemory(&screenPoint, sizeof(POINT));
		screenPoint.x = GET_X_LPARAM(lParam);
		screenPoint.y = GET_Y_LPARAM(lParam);

		POINT cursorPosition = screenPoint;
		ScreenToClient(hwnd, &cursorPosition);

		RECT clientRectangle;
		GetClientRect(hwnd, &clientRectangle);

		const bool isOnLeftBorder   = cursorPosition.x <  BORDER_THICKNESS;
		const bool isOnTopBorder    = cursorPosition.y <  BORDER_THICKNESS;
		const bool isOnRightBorder  = cursorPosition.x >= clientRectangle.right - BORDER_THICKNESS;
		const bool isOnBottomBorder = cursorPosition.y >= clientRectangle.bottom - BORDER_THICKNESS;

		if (isOnLeftBorder  && isOnTopBorder   ) return HTTOPLEFT;
		if (isOnRightBorder && isOnTopBorder   ) return HTTOPRIGHT;
		if (isOnLeftBorder  && isOnBottomBorder) return HTBOTTOMLEFT;
		if (isOnRightBorder && isOnBottomBorder) return HTBOTTOMRIGHT;

		if (isOnLeftBorder  ) return HTLEFT;
		if (isOnRightBorder ) return HTRIGHT;
		if (isOnTopBorder   ) return HTTOP;
		if (isOnBottomBorder) return HTBOTTOM;

		if (!ImGui::GetCurrentContext()) 
			return HTCLIENT;

		if (cursorPosition.y >= ImGui::GetFrameHeightWithSpacing())
			return HTCLIENT;

		const ImVec2 screenPosition = {
			static_cast<float>(screenPoint.x),
			static_cast<float>(screenPoint.y)
		};	

		if (self->titlebarHitTest_.isOverWidget(screenPosition))
			return HTCLIENT;

		return HTCAPTION;
	}
	case WM_CLOSE:
		if (self->onCloseRequest_) {
			if (self->onCloseRequest_()) {
				CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE: Close request approved by application");
			} else {
				CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE: Close request denied by application");
				return 0;
			}
		} else {
			CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"WM_CLOSE");
		}

		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED || LOWORD(lParam) == 0 || HIWORD(lParam) == 0)
			return 0;

		if (self->onResize_)
			self->onResize_(LOWORD(lParam), HIWORD(lParam));

		return 0;
	case WM_DPICHANGED: {
		const RECT* newRect = reinterpret_cast<const RECT*>(lParam);

		const int width  = newRect->right  - newRect->left;
		const int height = newRect->bottom - newRect->top;

		SetWindowPos(hwnd, nullptr, newRect->left, newRect->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		break;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_NCDESTROY:
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		break;
	}

	if (self->windowProc_)
		return self->windowProc_(hwnd, message, wParam, lParam);

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ReGuiEditor::Window::create(const wchar_t* title, int width, int height) {
	WNDCLASSW wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = ReGuiEditor::WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;
	wc.hIcon = LoadIconW(wc.hInstance, IDI_APPLICATION);

	if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		CONSOLE_ERROR(L"Failed to register window class");
		return false;
	}

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	int windowX = (screenWidth - width) / 2;
	int windowY = (screenHeight - height) / 2;

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GREEN, L"Creating window at (%d, %d) with size (%d, %d)", windowX, windowY, width, height);

	this->hwnd_ = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, windowX, windowY, width, height, NULL, NULL, wc.hInstance, this);
	if (!this->hwnd_) {
		CONSOLE_ERROR(L"Failed to create window");
		return false;
	}

	SetWindowPos(this->hwnd_, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_DONOTROUND;
	DwmSetWindowAttribute(this->hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

	UpdateWindow(this->hwnd_);
	return true;
}

void ReGuiEditor::Window::cleanup() {
	if (this->hwnd_) {
		DestroyWindow(this->hwnd_);
		this->hwnd_ = nullptr;
	}
}

void ReGuiEditor::Window::update() {
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);

		if (message.message == WM_QUIT)
			this->wantsExit_ = true;
	}
}

void ReGuiEditor::Window::setShowState(ReGuiEditor::WindowShowState state) const {
	switch (state) {
	case ReGuiEditor::WindowShowState::Show:     ShowWindowAsync(this->hwnd_, SW_SHOW); break;
	case ReGuiEditor::WindowShowState::Hide:     ShowWindowAsync(this->hwnd_, SW_HIDE); break;
	case ReGuiEditor::WindowShowState::Minimize: ShowWindowAsync(this->hwnd_, SW_MINIMIZE); break;
	case ReGuiEditor::WindowShowState::Maximize: ShowWindowAsync(this->hwnd_, SW_MAXIMIZE); break;
	case ReGuiEditor::WindowShowState::Restore:  ShowWindowAsync(this->hwnd_, SW_RESTORE); break;
	default:
		CONSOLE_WARNING(L"Unknown WindowShowState: %d", static_cast<int>(state));
		break;
	}
}

bool ReGuiEditor::Window::isMaximized() const {
	WINDOWPLACEMENT placement;
	ZeroMemory(&placement, sizeof(WINDOWPLACEMENT));
	placement.length = sizeof(WINDOWPLACEMENT);

	GetWindowPlacement(this->hwnd_, &placement);
	
	return placement.showCmd == SW_SHOWMAXIMIZED;
}

bool ReGuiEditor::Window::isMinimized() const {
	WINDOWPLACEMENT placement;
	ZeroMemory(&placement, sizeof(WINDOWPLACEMENT));
	placement.length = sizeof(WINDOWPLACEMENT);

	GetWindowPlacement(this->hwnd_, &placement);

	return placement.showCmd == SW_SHOWMINIMIZED;
}