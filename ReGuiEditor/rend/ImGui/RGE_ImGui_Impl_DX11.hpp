#pragma once
#include <ImGui/imgui.h>

#include <rend/Common/Viewport.hpp>

#include <rend/Resources/RenderView.hpp>

IMGUI_IMPL_API bool RGE_ImGui_ImplDX11_Init();
IMGUI_IMPL_API void RGE_ImGui_ImplDX11_Shutdown();
IMGUI_IMPL_API void RGE_ImGui_ImplDX11_RenderDrawData(ImDrawData* drawData, rend::RenderView* renderView);
			
IMGUI_IMPL_API bool RGE_ImGui_ImplDX11_CreateDeviceObjects();
IMGUI_IMPL_API void RGE_ImGui_ImplDX11_InvalidateDeviceObjects();

IMGUI_IMPL_API void RGE_ImGui_ImplDX11_UpdateTexture(ImTextureData* textureData);

struct RGE_ImGui_ImplDX11_FrameInfo {
	rend::RenderView* renderView = nullptr;
	ImVec2 displayPos = { 0.0f, 0.0f };
	ImVec2 framebufferScale = { 1.0f, 1.0f };
};

IMGUI_IMPL_API const RGE_ImGui_ImplDX11_FrameInfo* RGE_ImGui_ImplDX11_GetFrameInfo();
IMGUI_IMPL_API bool RGE_ImGui_ImplDX11_GetCallbackViewport(const ImDrawCmd* drawCmd, rend::Viewport& outputViewport);
