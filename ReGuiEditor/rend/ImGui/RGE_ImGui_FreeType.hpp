#pragma once

// This is basicly a copy of the original ImGui FreeType implementation but heavly cleaned up.

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

enum RGE_ImGui_FreeType_LoaderFlags {
	RGE_ImGui_FreeType_LoaderFlags_None = 0,
    RGE_ImGui_FreeType_LoaderFlags_NoHinting  = 1 << 0,
    RGE_ImGui_FreeType_LoaderFlags_NoAutoHint = 1 << 1,
    RGE_ImGui_FreeType_LoaderFlags_ForceAutoHint = 1 << 2,
    RGE_ImGui_FreeType_LoaderFlags_LightHinting = 1 << 3,
    RGE_ImGui_FreeType_LoaderFlags_MonoHinting = 1 << 4,
    RGE_ImGui_FreeType_LoaderFlags_Bold = 1 << 5,
    RGE_ImGui_FreeType_LoaderFlags_Oblique = 1 << 6,
    RGE_ImGui_FreeType_LoaderFlags_Monochrome = 1 << 7,
    RGE_ImGui_FreeType_LoaderFlags_LoadColor = 1 << 8,
    RGE_ImGui_FreeType_LoaderFlags_Bitmap = 1 << 9,
};

IMGUI_API const ImFontLoader* RGE_ImGui_FreeType_GetFontLoader();