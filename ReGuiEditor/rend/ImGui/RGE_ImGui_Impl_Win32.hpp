#pragma once
#include <ImGui/imgui.h>

#include <Windows.h>

IMGUI_IMPL_API bool RGE_ImGui_ImplWin32_Init(HWND hWnd);
IMGUI_IMPL_API void RGE_ImGui_ImplWin32_Shutdown();
IMGUI_IMPL_API void RGE_ImGui_ImplWin32_NewFrame();

IMGUI_IMPL_API LRESULT RGE_ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);