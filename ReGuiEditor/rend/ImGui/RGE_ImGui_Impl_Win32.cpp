#include "RGE_ImGui_Impl_Win32.hpp" // VCIC-Excluded

#include <ImGui/imgui.h>

#include <ShellScalingApi.h>
#include <Windows.h>

#include <cfloat>
#include <vector>

#pragma comment(lib, "Shcore.lib")

static constexpr wchar_t RGE_ImGui_ImplWin32_ViewportClassName[] = L"RGE_ImGui_Viewport";

struct RGE_ImGui_ImplWin32_Data {
	HINSTANCE hInstance = nullptr;
	HWND hwnd = nullptr;
	LARGE_INTEGER frequency = {};
	LARGE_INTEGER lastTime = {};

	UINT codePage = 0;
	ImGuiMouseCursor lastMouseCursor = ImGuiMouseCursor_None;

	bool isFocused = false;
	float initialDpiScale = -FLT_MAX;
};

inline static LRESULT CALLBACK RGE_ImGui_ImplWin32_PlatformWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

inline static void UpdateMouseButton(const RAWMOUSE& mouse, USHORT downFlag, USHORT upFlag, int button) {
	ImGuiIO& io = ImGui::GetIO();

	if (mouse.usButtonFlags & downFlag)
		io.AddMouseButtonEvent(button, true);
	else if (mouse.usButtonFlags & upFlag)
		io.AddMouseButtonEvent(button, false);
}

inline static RGE_ImGui_ImplWin32_Data* getBackendWin32Data() {
	return ImGui::GetCurrentContext() ? reinterpret_cast<RGE_ImGui_ImplWin32_Data*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
}

inline static void UpdateCodePage() {
	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	if (!backendData)
		return;

	HKL keyboardLayout = GetKeyboardLayout(0);
	LANGID langId = LOWORD(reinterpret_cast<UINT_PTR>(keyboardLayout));
	LCID localeId = MAKELCID(langId, SORT_DEFAULT);

	WCHAR localeName[LOCALE_NAME_MAX_LENGTH]{};
	if (LCIDToLocaleName(localeId, localeName, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to convert LCID to locale name: %u (Defaulting to ACP)", localeId);

		backendData->codePage = GetACP();
		return;
	}

	UINT codePage = 0;
	if (GetLocaleInfoEx(localeName, LOCALE_RETURN_NUMBER | LOCALE_IDEFAULTANSICODEPAGE, reinterpret_cast<LPWSTR>(&codePage), sizeof(codePage) / sizeof(WCHAR)) == 0) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get code page for keyboard layout: %u (Defaulting to ACP)", localeId);

		backendData->codePage = GetACP();
		return;
	}

	if (!IsValidCodePage(codePage)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Invalid code page: %u (Defaulting to ACP)", codePage);
		backendData->codePage = GetACP();
		return;
	}

	backendData->codePage = codePage;
}

inline static bool UpdateMouseCursor() {
	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	if (!backendData)
		return false;

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
		return false;

	ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
	if (imguiCursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
		SetCursor(nullptr);
		return true;
	}

	LPTSTR win32Cursor = IDC_ARROW;
	switch (imguiCursor) {
	case ImGuiMouseCursor_Arrow: win32Cursor = IDC_ARROW; break;
	case ImGuiMouseCursor_TextInput: win32Cursor = IDC_IBEAM; break;
	case ImGuiMouseCursor_ResizeAll: win32Cursor = IDC_SIZEALL; break;
	case ImGuiMouseCursor_ResizeNS: win32Cursor = IDC_SIZENS; break;
	case ImGuiMouseCursor_ResizeEW: win32Cursor = IDC_SIZEWE; break;
	case ImGuiMouseCursor_ResizeNESW: win32Cursor = IDC_SIZENESW; break;
	case ImGuiMouseCursor_ResizeNWSE: win32Cursor = IDC_SIZENWSE; break;
	case ImGuiMouseCursor_Hand: win32Cursor = IDC_HAND; break;
	case ImGuiMouseCursor_Wait: win32Cursor = IDC_WAIT; break;
	case ImGuiMouseCursor_Progress: win32Cursor = IDC_APPSTARTING; break;
	case ImGuiMouseCursor_NotAllowed: win32Cursor = IDC_NO; break;
	}

	SetCursor(LoadCursor(nullptr, win32Cursor));
	return true;
}

inline static ImGuiKey MapKeyToImGuiKey(WPARAM wParam, LPARAM lParam) {
	if (wParam == VK_RETURN && (HIWORD(lParam) & KF_EXTENDED))
		return ImGuiKey_KeypadEnter;

	const int scancode = static_cast<int>(LOBYTE(HIWORD(lParam)));
	const bool isExtended = (HIWORD(lParam) & KF_EXTENDED) != 0;

	switch (wParam) {
	case VK_TAB: return ImGuiKey_Tab;
	case VK_LEFT: return ImGuiKey_LeftArrow;
	case VK_RIGHT: return ImGuiKey_RightArrow;
	case VK_UP: return ImGuiKey_UpArrow;
	case VK_DOWN: return ImGuiKey_DownArrow;
	case VK_PRIOR: return ImGuiKey_PageUp;
	case VK_NEXT: return ImGuiKey_PageDown;
	case VK_HOME: return ImGuiKey_Home;
	case VK_END: return ImGuiKey_End;
	case VK_INSERT: return ImGuiKey_Insert;
	case VK_DELETE: return ImGuiKey_Delete;
	case VK_BACK: return ImGuiKey_Backspace;
	case VK_SPACE: return ImGuiKey_Space;
	case VK_RETURN: return ImGuiKey_Enter;
	case VK_ESCAPE: return ImGuiKey_Escape;
	case VK_OEM_COMMA: return ImGuiKey_Comma;
	case VK_OEM_PERIOD: return ImGuiKey_Period;
	case VK_CAPITAL: return ImGuiKey_CapsLock;
	case VK_SCROLL: return ImGuiKey_ScrollLock;
	case VK_NUMLOCK: return ImGuiKey_NumLock;
	case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
	case VK_PAUSE: return ImGuiKey_Pause;
	case VK_NUMPAD0: return ImGuiKey_Keypad0;
	case VK_NUMPAD1: return ImGuiKey_Keypad1;
	case VK_NUMPAD2: return ImGuiKey_Keypad2;
	case VK_NUMPAD3: return ImGuiKey_Keypad3;
	case VK_NUMPAD4: return ImGuiKey_Keypad4;
	case VK_NUMPAD5: return ImGuiKey_Keypad5;
	case VK_NUMPAD6: return ImGuiKey_Keypad6;
	case VK_NUMPAD7: return ImGuiKey_Keypad7;
	case VK_NUMPAD8: return ImGuiKey_Keypad8;
	case VK_NUMPAD9: return ImGuiKey_Keypad9;
	case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
	case VK_DIVIDE: return ImGuiKey_KeypadDivide;
	case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
	case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
	case VK_ADD: return ImGuiKey_KeypadAdd;
	case VK_SHIFT: return (scancode == 0x36) ? ImGuiKey_RightShift : ImGuiKey_LeftShift;
	case VK_CONTROL: return isExtended ? ImGuiKey_RightCtrl : ImGuiKey_LeftCtrl;
	case VK_MENU: return isExtended ? ImGuiKey_RightAlt : ImGuiKey_LeftAlt;
	case VK_LWIN: return ImGuiKey_LeftSuper;
	case VK_RWIN: return ImGuiKey_RightSuper;
	case VK_APPS: return ImGuiKey_Menu;
	case '0': return ImGuiKey_0;
	case '1': return ImGuiKey_1;
	case '2': return ImGuiKey_2;
	case '3': return ImGuiKey_3;
	case '4': return ImGuiKey_4;
	case '5': return ImGuiKey_5;
	case '6': return ImGuiKey_6;
	case '7': return ImGuiKey_7;
	case '8': return ImGuiKey_8;
	case '9': return ImGuiKey_9;
	case 'A': return ImGuiKey_A;
	case 'B': return ImGuiKey_B;
	case 'C': return ImGuiKey_C;
	case 'D': return ImGuiKey_D;
	case 'E': return ImGuiKey_E;
	case 'F': return ImGuiKey_F;
	case 'G': return ImGuiKey_G;
	case 'H': return ImGuiKey_H;
	case 'I': return ImGuiKey_I;
	case 'J': return ImGuiKey_J;
	case 'K': return ImGuiKey_K;
	case 'L': return ImGuiKey_L;
	case 'M': return ImGuiKey_M;
	case 'N': return ImGuiKey_N;
	case 'O': return ImGuiKey_O;
	case 'P': return ImGuiKey_P;
	case 'Q': return ImGuiKey_Q;
	case 'R': return ImGuiKey_R;
	case 'S': return ImGuiKey_S;
	case 'T': return ImGuiKey_T;
	case 'U': return ImGuiKey_U;
	case 'V': return ImGuiKey_V;
	case 'W': return ImGuiKey_W;
	case 'X': return ImGuiKey_X;
	case 'Y': return ImGuiKey_Y;
	case 'Z': return ImGuiKey_Z;
	case VK_F1: return ImGuiKey_F1;
	case VK_F2: return ImGuiKey_F2;
	case VK_F3: return ImGuiKey_F3;
	case VK_F4: return ImGuiKey_F4;
	case VK_F5: return ImGuiKey_F5;
	case VK_F6: return ImGuiKey_F6;
	case VK_F7: return ImGuiKey_F7;
	case VK_F8: return ImGuiKey_F8;
	case VK_F9: return ImGuiKey_F9;
	case VK_F10: return ImGuiKey_F10;
	case VK_F11: return ImGuiKey_F11;
	case VK_F12: return ImGuiKey_F12;
	case VK_F13: return ImGuiKey_F13;
	case VK_F14: return ImGuiKey_F14;
	case VK_F15: return ImGuiKey_F15;
	case VK_F16: return ImGuiKey_F16;
	case VK_F17: return ImGuiKey_F17;
	case VK_F18: return ImGuiKey_F18;
	case VK_F19: return ImGuiKey_F19;
	case VK_F20: return ImGuiKey_F20;
	case VK_F21: return ImGuiKey_F21;
	case VK_F22: return ImGuiKey_F22;
	case VK_F23: return ImGuiKey_F23;
	case VK_F24: return ImGuiKey_F24;
	case VK_BROWSER_BACK: return ImGuiKey_AppBack;
	case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
	default: break;
	}

	switch (scancode) {
	case 41: return ImGuiKey_GraveAccent;
	case 12: return ImGuiKey_Minus;
	case 13: return ImGuiKey_Equal;
	case 26: return ImGuiKey_LeftBracket;
	case 27: return ImGuiKey_RightBracket;
	case 86: return ImGuiKey_Oem102;
	case 43: return ImGuiKey_Backslash;
	case 39: return ImGuiKey_Semicolon;
	case 40: return ImGuiKey_Apostrophe;
	case 51: return ImGuiKey_Comma;
	case 52: return ImGuiKey_Period;
	case 53: return ImGuiKey_Slash;
	default: break;
	}

	return ImGuiKey_None;
}

inline static HWND GetViewportHWND(ImGuiViewport* viewport) { return reinterpret_cast<HWND>(viewport->PlatformHandle); }
inline static bool IsMainViewport(ImGuiViewport* viewport) { return viewport == ImGui::GetMainViewport(); }

inline static ImVec2 GetViewportClientSize(HWND hwnd) {
	RECT rect;
	if (!GetClientRect(hwnd, &rect)) {
		CONSOLE_ERROR("Failed to get client rect for ImGui viewport window! Error: %lu", GetLastError());
		return { 0.0f, 0.0f };
	}

	return {
		static_cast<float>(rect.right - rect.left),
		static_cast<float>(rect.bottom - rect.top)
	};
}

inline static ImVec2 GetViewportWindowPos(HWND hwnd) {
	POINT point = { 0, 0 };
	if (!ClientToScreen(hwnd, &point)) {
		CONSOLE_ERROR("Failed to get window position for ImGui viewport window! Error: %lu", GetLastError());
		return { 0.0f, 0.0f };
	}

	return {
		static_cast<float>(point.x),
		static_cast<float>(point.y)
	};
}

inline static void SetViewportWindowPos(HWND hwnd, ImVec2 pos) {
	POINT point = {
		.x = static_cast<LONG>(pos.x),
		.y = static_cast<LONG>(pos.y)
	};

	RECT rect;
	if (!GetWindowRect(hwnd, &rect)) {
		CONSOLE_ERROR("Failed to get window rect for ImGui viewport window! Error: %lu", GetLastError());
		return;
	}

	POINT clientPos = { 0, 0 };
	if (!ClientToScreen(hwnd, &clientPos)) {
		CONSOLE_ERROR("Failed to get client position for ImGui viewport window! Error: %lu", GetLastError());
		return;
	}

	const int offsetX = clientPos.x - rect.left;
	const int offsetY = clientPos.y - rect.top;
	if (!SetWindowPos(hwnd, nullptr, point.x - offsetX, point.y - offsetY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER))
		CONSOLE_ERROR("Failed to set window position for ImGui viewport window! Error: %lu", GetLastError());
}

inline static void SetViewportWindowSize(HWND hwnd, ImVec2 size) {
	RECT rect = {
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(size.x),
		.bottom = static_cast<LONG>(size.y)
	};

	DWORD style = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE));
	DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_EXSTYLE));

	UINT dpi = GetDpiForWindow(hwnd);
	if (dpi == 0)
		dpi = 96;

	if (!AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, dpi))
		CONSOLE_ERROR("Failed to adjust window rect for ImGui viewport window! Error: %lu", GetLastError());

	if (!SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER))
		CONSOLE_ERROR("Failed to set window size for ImGui viewport window! Error: %lu", GetLastError());
}

inline static void UpdateViewportPosition(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd)
		return;

	ImVec2 position = GetViewportWindowPos(hwnd);
	if (viewport->Pos.x != position.x || viewport->Pos.y != position.y)
		viewport->PlatformRequestMove = true;
}

inline static void UpdateViewportSize(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd)
		return;

	ImVec2 size = GetViewportClientSize(hwnd);
	if (viewport->Size.x != size.x || viewport->Size.y != size.y)
		viewport->PlatformRequestResize = true;
}

inline static void RGE_ImGui_ImplWin32_CreateWindow(ImGuiViewport* viewport) {
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exStyle = WS_EX_APPWINDOW;

	if (viewport->Flags & ImGuiViewportFlags_NoDecoration)
		style = WS_POPUP;

	if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
		exStyle = WS_EX_TOOLWINDOW;

	if (viewport->Flags & ImGuiViewportFlags_TopMost)
		exStyle |= WS_EX_TOPMOST;

	HWND parent = nullptr;

	if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
		parent = GetViewportHWND(ImGui::GetMainViewport());

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	HWND hwnd = CreateWindowExW(exStyle, RGE_ImGui_ImplWin32_ViewportClassName, L"RGE_ImGui_ImplWin32_Viewport", style, 0, 0, 1, 1, parent, nullptr, backendData->hInstance, viewport);
	if (!hwnd) {
		CONSOLE_ERROR("Failed to create ImGui viewport window! Error: %lu", GetLastError());
		return;
	}

	viewport->PlatformHandle = hwnd;
	viewport->PlatformHandleRaw = hwnd;

	SetViewportWindowPos(hwnd, viewport->Pos);
	SetViewportWindowSize(hwnd, viewport->Size);

	ShowWindow(hwnd, SW_HIDE);
}

inline static void RGE_ImGui_ImplWin32_DestroyWindow(ImGuiViewport* viewport) {
	if (!viewport->PlatformHandle && !viewport->PlatformHandleRaw)
		return;

	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_DestroyWindow: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	DestroyWindow(hwnd);

	viewport->PlatformHandle = nullptr;
	viewport->PlatformHandleRaw = nullptr;
}

inline static void RGE_ImGui_ImplWin32_ShowWindow(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_ShowWindow: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	ShowWindow(hwnd, SW_SHOW);
}

inline static void RGE_ImGui_ImplWin32_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowPos: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	SetViewportWindowPos(hwnd, pos);
}

inline static ImVec2 RGE_ImGui_ImplWin32_GetWindowPos(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_GetWindowPos: Failed to get viewport window handle. Error: %lu", GetLastError());
		return { 0.0f, 0.0f };
	}

	return GetViewportWindowPos(hwnd);
}

inline static void RGE_ImGui_ImplWin32_SetWindowSize(ImGuiViewport* viewport, ImVec2 size) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowSize: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	SetViewportWindowSize(hwnd, size);
}

inline static ImVec2 RGE_ImGui_ImplWin32_GetWindowSize(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_GetWindowSize: Failed to get viewport window handle. Error: %lu", GetLastError());
		return { 0.0f, 0.0f };
	}

	return GetViewportClientSize(hwnd);
}

inline static void RGE_ImGui_ImplWin32_SetWindowFocus(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowFocus: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	BringWindowToTop(hwnd);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
}

inline static bool RGE_ImGui_ImplWin32_GetWindowFocus(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RGE_ImGui_ImplWin32_GetWindowFocus: Failed to get viewport window handle. Error: %lu", GetLastError());
		return false;
	}

	return GetForegroundWindow() == hwnd;
}

inline static bool RGE_ImGui_ImplWin32_GetWindowMinimized(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_GetWindowMinimized: Failed to get viewport window handle. Error: %lu", GetLastError());
		return false;
	}

	return IsIconic(hwnd) != 0;
}

inline static void RGE_ImGui_ImplWin32_SetWindowTitle(ImGuiViewport* viewport, const char* title) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowTitle: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	int length = MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, nullptr, 0);
	if (length <= 0) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowTitle: Failed to convert title to wide string. Error: %lu", GetLastError());
		return;
	}

	std::vector<wchar_t> wideTitle(static_cast<size_t>(length));
	MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, wideTitle.data(), length);

	if (!SetWindowTextW(hwnd, wideTitle.data()))
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowTitle: Failed to set window title. Error: %lu", GetLastError());
}

inline static void RGE_ImGui_ImplWin32_SetWindowAlpha(ImGuiViewport* viewport, float alpha) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowAlpha: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	alpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

	LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

	if (!SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(alpha * 255.0f), LWA_ALPHA))
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_SetWindowAlpha: Failed to set window alpha. Error: %lu", GetLastError());
}

inline static void RGE_ImGui_ImplWin32_UpdateWindow(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_UpdateWindow: Failed to get viewport window handle. Error: %lu", GetLastError());
		return;
	}

	UpdateViewportPosition(viewport);
	UpdateViewportSize(viewport);

	if (IsIconic(hwnd))
		viewport->Flags |= ImGuiViewportFlags_IsMinimized;
	else
		viewport->Flags &= ~ImGuiViewportFlags_IsMinimized;
}

inline static float RGE_ImGui_ImplWin32_GetWindowDpiScale(ImGuiViewport* viewport) {
	HWND hwnd = GetViewportHWND(viewport);
	if (!hwnd) {
		CONSOLE_ERROR("RGE_ImGui_ImplWin32_GetWindowDpiScale: Failed to get viewport window handle. Error: %lu", GetLastError());
		return 1.0f;
	}

	UINT dpi = GetDpiForWindow(hwnd);
	if (dpi == 0) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RGE_ImGui_ImplWin32_GetWindowDpiScale: Failed to get DPI for window. Error: %lu", GetLastError());
		return 1.0f;
	}

	return static_cast<float>(dpi) / 96.0f;
}

inline static void RGE_ImGui_ImplWin32_OnChangedViewport(ImGuiViewport* viewport) {
	(void)viewport;
}

inline static float RGE_ImGui_ImplWin32_GetMonitorDpiScale(HMONITOR monitor) {
	UINT dpiX = 96;
	UINT dpiY = 96;

	HRESULT result = GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get DPI for monitor! Error: 0x%08X", result);
		return 1.0f;
	}

	return static_cast<float>(dpiX) / 96.0f;
}

inline static BOOL CALLBACK RGE_ImGui_ImplWin32_UpdateMonitorsCallback(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM userData) {
	(void)hdc;
	(void)rect;

	ImGuiPlatformIO* platformIO = reinterpret_cast<ImGuiPlatformIO*>(userData);

	MONITORINFO monitorInfo;
	ZeroMemory(&monitorInfo, sizeof(MONITORINFO));
	monitorInfo.cbSize = sizeof(MONITORINFO);

	if (!GetMonitorInfoW(monitor, &monitorInfo))
		return TRUE;

	ImGuiPlatformMonitor platformMonitor{};
	platformMonitor.MainPos = {
		static_cast<float>(monitorInfo.rcMonitor.left),
		static_cast<float>(monitorInfo.rcMonitor.top)
	};
	platformMonitor.MainSize = {
		static_cast<float>(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left),
		static_cast<float>(monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top)
	};
	platformMonitor.WorkPos = {
		static_cast<float>(monitorInfo.rcWork.left),
		static_cast<float>(monitorInfo.rcWork.top)
	};
	platformMonitor.WorkSize = {
		static_cast<float>(monitorInfo.rcWork.right - monitorInfo.rcWork.left),
		static_cast<float>(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top)
	};

	platformMonitor.DpiScale = RGE_ImGui_ImplWin32_GetMonitorDpiScale(monitor);
	platformMonitor.PlatformHandle = monitor;

	platformIO->Monitors.push_back(platformMonitor);
	return TRUE;
}

inline static void RGE_ImGui_ImplWin32_UpdateMonitors() {
	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

	platformIO.Monitors.resize(0);
	EnumDisplayMonitors(nullptr, nullptr, RGE_ImGui_ImplWin32_UpdateMonitorsCallback, reinterpret_cast<LPARAM>(&platformIO));
}

inline static bool RGE_ImGui_ImplWin32_RegisterViewportClass() {
	WNDCLASSEXW windowClass;
	ZeroMemory(&windowClass, sizeof(WNDCLASSEXW));
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = RGE_ImGui_ImplWin32_PlatformWndProc;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.lpszClassName = RGE_ImGui_ImplWin32_ViewportClassName;

	if (!RegisterClassExW(&windowClass)) {
		DWORD error = GetLastError();
		if (error != ERROR_CLASS_ALREADY_EXISTS) {
			CONSOLE_ERROR("Failed to register ImGui viewport window class! Error: %lu", error);
			return false;
		}
	}

	return true;
}

inline static void RGE_ImGui_ImplWin32_SetupPlatformInterface(HWND hWnd) {
	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	platformIO.Platform_CreateWindow = RGE_ImGui_ImplWin32_CreateWindow;
	platformIO.Platform_DestroyWindow = RGE_ImGui_ImplWin32_DestroyWindow;
	platformIO.Platform_ShowWindow = RGE_ImGui_ImplWin32_ShowWindow;
	platformIO.Platform_SetWindowPos = RGE_ImGui_ImplWin32_SetWindowPos;
	platformIO.Platform_GetWindowPos = RGE_ImGui_ImplWin32_GetWindowPos;
	platformIO.Platform_SetWindowSize = RGE_ImGui_ImplWin32_SetWindowSize;
	platformIO.Platform_GetWindowSize = RGE_ImGui_ImplWin32_GetWindowSize;
	platformIO.Platform_SetWindowFocus = RGE_ImGui_ImplWin32_SetWindowFocus;
	platformIO.Platform_GetWindowFocus = RGE_ImGui_ImplWin32_GetWindowFocus;
	platformIO.Platform_GetWindowMinimized = RGE_ImGui_ImplWin32_GetWindowMinimized;
	platformIO.Platform_SetWindowTitle = RGE_ImGui_ImplWin32_SetWindowTitle;
	platformIO.Platform_SetWindowAlpha = RGE_ImGui_ImplWin32_SetWindowAlpha;
	platformIO.Platform_UpdateWindow = RGE_ImGui_ImplWin32_UpdateWindow;
	platformIO.Platform_GetWindowDpiScale = RGE_ImGui_ImplWin32_GetWindowDpiScale;
	platformIO.Platform_OnChangedViewport = RGE_ImGui_ImplWin32_OnChangedViewport;

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	mainViewport->PlatformHandle = hWnd;
	mainViewport->PlatformHandleRaw = hWnd;

	RGE_ImGui_ImplWin32_UpdateMonitors();

	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
}

inline static void RGE_ImGui_ImplWin32_ShutdownPlatformInterface() {
	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	platformIO.Platform_CreateWindow = nullptr;
	platformIO.Platform_DestroyWindow = nullptr;
	platformIO.Platform_ShowWindow = nullptr;
	platformIO.Platform_SetWindowPos = nullptr;
	platformIO.Platform_GetWindowPos = nullptr;
	platformIO.Platform_SetWindowSize = nullptr;
	platformIO.Platform_GetWindowSize = nullptr;
	platformIO.Platform_SetWindowFocus = nullptr;
	platformIO.Platform_GetWindowFocus = nullptr;
	platformIO.Platform_GetWindowMinimized = nullptr;
	platformIO.Platform_SetWindowTitle = nullptr;
	platformIO.Platform_SetWindowAlpha = nullptr;
	platformIO.Platform_UpdateWindow = nullptr;
	platformIO.Platform_GetWindowDpiScale = nullptr;
	platformIO.Platform_OnChangedViewport = nullptr;

	platformIO.Monitors.clear();

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	mainViewport->PlatformHandle = nullptr;
	mainViewport->PlatformHandleRaw = nullptr;

	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags &= ~ImGuiBackendFlags_PlatformHasViewports;
}

inline static ImGuiViewport* RGE_ImGui_ImplWin32_FindViewportByHWND(HWND hwnd) {
	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	for (int index = 0; index < platformIO.Viewports.Size; ++index) {
		ImGuiViewport* viewport = platformIO.Viewports[index];
		if (viewport->PlatformHandle == hwnd)
			return viewport;
	}

	return nullptr;
}

inline static bool RGE_ImGui_ImplWin32_IsViewportWindow(HWND hwnd) {
	if (!hwnd)
		return false;

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	if (!backendData)
		return false;

	if (hwnd == backendData->hwnd)
		return true;

	return RGE_ImGui_ImplWin32_FindViewportByHWND(hwnd) != nullptr;
}

inline static LRESULT CALLBACK RGE_ImGui_ImplWin32_PlatformWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (!ImGui::GetCurrentContext()) {
		if (RGE_ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return TRUE;

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	ImGuiViewport* viewport = reinterpret_cast<ImGuiViewport*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (!viewport)
		viewport = RGE_ImGui_ImplWin32_FindViewportByHWND(hWnd);

	if (!viewport) {
		if (RGE_ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return TRUE;

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	switch (msg) {
	case WM_NCCREATE: {
		const CREATESTRUCTW* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		if (createStruct)
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));

		break;
	}
	case WM_CLOSE:
		viewport->PlatformRequestClose = true;
		return 0;
	case WM_MOVE:
		viewport->PlatformRequestMove = true;
		break;
	case WM_SIZE:
		viewport->PlatformRequestResize = true;
		if (wParam == SIZE_MINIMIZED)
			viewport->Flags |= ImGuiViewportFlags_IsMinimized;
		else
			viewport->Flags &= ~ImGuiViewportFlags_IsMinimized;

		break;
	case WM_DPICHANGED:
		if (lParam) {
			const RECT* suggestedRect = reinterpret_cast<const RECT*>(lParam);

			int newWidth = suggestedRect->right - suggestedRect->left;
			int newHeight = suggestedRect->bottom - suggestedRect->top;
			if (!SetWindowPos(hWnd, nullptr, suggestedRect->left, suggestedRect->top, newWidth, newHeight, SWP_NOZORDER | SWP_NOACTIVATE))
				CONSOLE_ERROR("Failed to set window position and size on WM_DPICHANGED! Error: %lu", GetLastError());
		}

		return 0;
	case WM_NCDESTROY:
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
		break;
	}

	if (RGE_ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return TRUE;

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool RGE_ImGui_ImplWin32_Init(HWND hWnd) {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing ImGui Win32 backend...");

	IMGUI_CHECKVERSION();

	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
	IM_ASSERT(io.BackendPlatformName == nullptr && "Already initialized a platform backend!");

	RGE_ImGui_ImplWin32_Data* backendData = IM_NEW(RGE_ImGui_ImplWin32_Data)();
	backendData->hwnd = hWnd;
	backendData->hInstance = GetModuleHandleW(nullptr);

	io.BackendPlatformName = "RGE_ImGui_ImplWin32";
	io.BackendPlatformUserData = backendData;

	if (!QueryPerformanceFrequency(&backendData->frequency)) {
		CONSOLE_ERROR("Failed to query performance frequency!");

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	if (!QueryPerformanceCounter(&backendData->lastTime)) {
		CONSOLE_ERROR("Failed to query performance counter!");

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	if (!RGE_ImGui_ImplWin32_RegisterViewportClass()) {
		CONSOLE_ERROR("Failed to register ImGui viewport window class! Error: %lu", GetLastError());

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	RGE_ImGui_ImplWin32_SetupPlatformInterface(hWnd);

	UpdateCodePage();
	return true;
}

void RGE_ImGui_ImplWin32_Shutdown() {
	ImGuiIO& io = ImGui::GetIO();
	if (io.BackendPlatformUserData == nullptr && io.BackendPlatformName == nullptr)
		return;

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Shutting down ImGui Win32 backend...");

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		ImGui::DestroyPlatformWindows();

	RGE_ImGui_ImplWin32_ShutdownPlatformInterface();

	IM_DELETE(backendData);

	io.BackendPlatformName = nullptr;
	io.BackendPlatformUserData = nullptr;
}

void RGE_ImGui_ImplWin32_NewFrame() {
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");
	IM_ASSERT(io.BackendPlatformName != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	IM_ASSERT(backendData->hwnd != nullptr && "HWND is null!");

	HWND foregroundWindow = GetForegroundWindow();
	backendData->isFocused = RGE_ImGui_ImplWin32_IsViewportWindow(foregroundWindow);

	if (backendData->initialDpiScale == -FLT_MAX)
		backendData->initialDpiScale = GetDpiForWindow(backendData->hwnd) / 96.0f;

	RECT rect;
	if (GetClientRect(backendData->hwnd, &rect)) {
		io.DisplaySize.x = static_cast<float>(rect.right - rect.left);
		io.DisplaySize.y = static_cast<float>(rect.bottom - rect.top);
	}

	LARGE_INTEGER currentTime;
	if (QueryPerformanceCounter(&currentTime)) {
		io.DeltaTime = static_cast<float>(currentTime.QuadPart - backendData->lastTime.QuadPart) / static_cast<float>(backendData->frequency.QuadPart);
		backendData->lastTime = currentTime;
	}

	POINT mousePosition;
	if (GetCursorPos(&mousePosition)) {
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			io.AddMousePosEvent(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y));
		else if (backendData->isFocused && ScreenToClient(backendData->hwnd, &mousePosition))
			io.AddMousePosEvent(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y));
	}

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		RGE_ImGui_ImplWin32_UpdateMonitors();

		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

		for (ImGuiViewport* viewport : platformIO.Viewports) {
			if (!viewport || viewport == ImGui::GetMainViewport())
				continue;

			RGE_ImGui_ImplWin32_UpdateWindow(viewport);
		}
	}
}

LRESULT RGE_ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (!ImGui::GetCurrentContext())
		return FALSE;

	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");
	IM_ASSERT(io.BackendPlatformName != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();

	ImGuiViewport* viewport = RGE_ImGui_ImplWin32_FindViewportByHWND(hWnd);
	if (backendData->hwnd != hWnd && viewport == nullptr)
		return FALSE;

	switch (msg) {
	case WM_MOUSEACTIVATE:
		if (viewport->Flags & ImGuiViewportFlags_NoFocusOnClick)
			return MA_NOACTIVATE;
		break;
	case WM_SETFOCUS:
		io.AddFocusEvent(true);
		return FALSE;
	case WM_KILLFOCUS: {
		HWND newFocus = reinterpret_cast<HWND>(wParam);

		if (!RGE_ImGui_ImplWin32_IsViewportWindow(newFocus)) {
			io.AddFocusEvent(false);
			io.ClearInputKeys();

			for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
				io.AddMouseButtonEvent(button, false);
		}

		return FALSE;
	}
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, msg == WM_LBUTTONDOWN);
		return FALSE;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, msg == WM_RBUTTONDOWN);
		return FALSE;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, msg == WM_MBUTTONDOWN);
		return FALSE;
	case WM_MOUSEWHEEL:
		io.AddMouseWheelEvent(0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA));
		return FALSE;
	case WM_MOUSEHWHEEL:
		io.AddMouseWheelEvent(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA), 0.0f);
		return FALSE;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		if (wParam >= 256)
			break;

		const bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		ImGuiKey key = MapKeyToImGuiKey(wParam, lParam);

		if (key != ImGuiKey_None)
			io.AddKeyEvent(key, isDown);

		if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift)
			io.AddKeyEvent(ImGuiMod_Shift, isDown);
		else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl)
			io.AddKeyEvent(ImGuiMod_Ctrl, isDown);
		else if (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt)
			io.AddKeyEvent(ImGuiMod_Alt, isDown);
		else if (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper)
			io.AddKeyEvent(ImGuiMod_Super, isDown);

		return FALSE;
	}
	case WM_CHAR:
		if (IsWindowUnicode(hWnd)) {
			if (wParam > 0 && wParam < 0x10000)
				io.AddInputCharacterUTF16(static_cast<unsigned short>(wParam));
		} else {
			wchar_t character = 0;
			MultiByteToWideChar(backendData->codePage, MB_PRECOMPOSED, reinterpret_cast<LPCCH>(&wParam), 2, &character, 1);
			io.AddInputCharacter(character);
		}

		return FALSE;
	case WM_INPUTLANGCHANGE:
		UpdateCodePage();
		return FALSE;
	case WM_IME_COMPOSITION: {
		LRESULT result = DefWindowProc(hWnd, msg, wParam, lParam);
		return (lParam & GCS_RESULTSTR) ? TRUE : result;
	}
	case WM_IME_CHAR:
		if (IsWindowUnicode(hWnd) == FALSE) {
			if (IsDBCSLeadByte(HIBYTE(wParam)))
				wParam = MAKEWORD(HIBYTE(wParam), LOBYTE(wParam));

			wchar_t character = 0;
			MultiByteToWideChar(backendData->codePage, MB_PRECOMPOSED, reinterpret_cast<LPCCH>(&wParam), 2, &character, 1);

			io.AddInputCharacterUTF16(character);
			return TRUE;
		}

		return FALSE;
	case WM_SETCURSOR:
		return LOWORD(lParam) == HTCLIENT && UpdateMouseCursor();
	}

	return FALSE;
}