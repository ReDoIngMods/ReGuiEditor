#include "RGE_ImGui_Impl_Win32.hpp" // VCIC-Excluded

#include <ImGui/imgui.h>

#include <Windows.h>
#include <hidusage.h>

#include <cfloat>
#include <vector>

struct RGE_ImGui_ImplWin32_Data {
	HWND hwnd = nullptr;
	LARGE_INTEGER frequency = {};
	LARGE_INTEGER lastTime = {};

	UINT codePage = 0;
	ImGuiMouseCursor lastMouseCursor = ImGuiMouseCursor_None;

	bool isFocused = false;
	float initialDpiScale = -FLT_MAX;
};

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

bool RGE_ImGui_ImplWin32_Init(HWND hWnd) {
	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Initializing ImGui Win32 backend...");

	IMGUI_CHECKVERSION();

	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
	IM_ASSERT(io.BackendPlatformName == nullptr && "Already initialized a platform backend!");

	RGE_ImGui_ImplWin32_Data* backendData = IM_NEW(RGE_ImGui_ImplWin32_Data)();
	backendData->hwnd = hWnd;

	io.BackendPlatformName = "RGE_ImGui_ImplWin32";
	io.BackendPlatformUserData = backendData;

	if (!QueryPerformanceFrequency(&backendData->frequency)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query performance frequency!");

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	if (!QueryPerformanceCounter(&backendData->lastTime)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to query performance counter!");

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	RAWINPUTDEVICE device;
	ZeroMemory(&device, sizeof(RAWINPUTDEVICE));
	device.usUsagePage = HID_USAGE_PAGE_GENERIC;
	device.usUsage = HID_USAGE_GENERIC_MOUSE;
	device.dwFlags = RIDEV_INPUTSINK;
	device.hwndTarget = hWnd;

	if (!RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE))) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to register raw input devices! Error: %lu", GetLastError());

		RGE_ImGui_ImplWin32_Shutdown();
		return false;
	}

	UpdateCodePage();
	return true;
}

void RGE_ImGui_ImplWin32_Shutdown() {
	ImGuiIO& io = ImGui::GetIO();
	if (io.BackendPlatformUserData == nullptr && io.BackendPlatformName == nullptr)
		return;

	CONSOLE_LOG(LEVEL_RENDER, COLOR_GRAY, "Shutting down ImGui Win32 backend...");

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();

	RAWINPUTDEVICE device;
	ZeroMemory(&device, sizeof(RAWINPUTDEVICE));
	device.usUsagePage = HID_USAGE_PAGE_GENERIC;
	device.usUsage = HID_USAGE_GENERIC_MOUSE;
	device.dwFlags = RIDEV_REMOVE;

	if (!RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE)))
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to unregister raw input device! Error: %lu", GetLastError());

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
	backendData->isFocused = GetFocus() == backendData->hwnd;

	if (backendData->initialDpiScale == -FLT_MAX) {
		backendData->initialDpiScale = GetDpiForWindow(backendData->hwnd) / 96.0f;
	}

	float currentDpiScale = GetDpiForWindow(backendData->hwnd) / 96.0f;
	float fixedScale = backendData->initialDpiScale / currentDpiScale;

	//I have no idea if we should apply this or not, cause this has both pro's and cons.
	//ImGui::GetStyle().FontScaleDpi = currentDpiScale;

	RECT rect;
	if (GetClientRect(backendData->hwnd, &rect)) {
		io.DisplaySize.x = static_cast<float>(rect.right - rect.left) * fixedScale;
		io.DisplaySize.y = static_cast<float>(rect.bottom - rect.top) * fixedScale;
	}

	LARGE_INTEGER currentTime;
	if (QueryPerformanceCounter(&currentTime)) {
		io.DeltaTime = static_cast<float>(currentTime.QuadPart - backendData->lastTime.QuadPart) / static_cast<float>(backendData->frequency.QuadPart);
		backendData->lastTime = currentTime;
	}

	POINT mousePos;
	if (backendData->isFocused && GetCursorPos(&mousePos) && ScreenToClient(backendData->hwnd, &mousePos))
		io.AddMousePosEvent(static_cast<float>(mousePos.x) * fixedScale, static_cast<float>(mousePos.y) * fixedScale);
}

LRESULT RGE_ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (!ImGui::GetCurrentContext())
		return FALSE;

	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");
	IM_ASSERT(io.BackendPlatformName != nullptr && "Did you call ImGui::CreateContext() and ImGui_ImplWin32_Init()?");

	RGE_ImGui_ImplWin32_Data* backendData = getBackendWin32Data();
	if (backendData->hwnd != hWnd)
		return FALSE;

	switch (msg) {
	case WM_SETFOCUS:
		io.AddFocusEvent(true);
		return FALSE;
	case WM_KILLFOCUS:
		io.AddFocusEvent(false);
		io.ClearInputKeys();
		io.ClearEventsQueue();
		return FALSE;
	case WM_INPUT: {
		UINT size = 0;
		GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (size == 0)
			break;

		std::vector<BYTE> rawInputData(size);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawInputData.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to get raw input data!");
			break;
		}

		if (!backendData->isFocused)
			return FALSE;

		const RAWINPUT* rawInput = reinterpret_cast<const RAWINPUT*>(rawInputData.data());
		if (rawInput->header.dwType == RIM_TYPEMOUSE) {
			const RAWMOUSE& mouse = rawInput->data.mouse;
			UpdateMouseButton(mouse, RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, ImGuiMouseButton_Left);
			UpdateMouseButton(mouse, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, ImGuiMouseButton_Right);
			UpdateMouseButton(mouse, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, ImGuiMouseButton_Middle);

			if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
				io.AddMouseWheelEvent(0.0f, static_cast<SHORT>(mouse.usButtonData) / static_cast<float>(WHEEL_DELTA));
			if (mouse.usButtonFlags & RI_MOUSE_HWHEEL)
				io.AddMouseWheelEvent(static_cast<SHORT>(mouse.usButtonData) / static_cast<float>(WHEEL_DELTA), 0.0f);
		}

		return FALSE;
	}
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		const bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		if (wParam >= 256)
			break;

		ImGuiKey key = MapKeyToImGuiKey(wParam, lParam);
		if (key != ImGuiKey_None)
			io.AddKeyEvent(key, isDown);

		// ImGui fucking sucks, WHY NOT DO THE MODIFERS IF YOU'RE ALREADY DOING THE FUCKING KEYS, JESUS CHRIST???

		if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) io.AddKeyEvent(ImGuiMod_Shift, isDown);
		else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl) io.AddKeyEvent(ImGuiMod_Ctrl, isDown);
		else if (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt) io.AddKeyEvent(ImGuiMod_Alt, isDown);
		else if (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) io.AddKeyEvent(ImGuiMod_Super, isDown);

		return FALSE;
	}
	case WM_CHAR:
		if (IsWindowUnicode(backendData->hwnd)) {
			if (wParam > 0 && wParam < 0x10000)
				io.AddInputCharacter(static_cast<unsigned short>(wParam));
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
		if (IsWindowUnicode(backendData->hwnd) == FALSE) {
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