#pragma once
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <vector>

namespace TitlebarHitTest {
    inline std::vector<ImRect> gRectangles;

    inline void beginFrame() { gRectangles.clear(); }
    inline void addRect(ImVec2 min, ImVec2 max) { gRectangles.push_back({ min, max }); }

    inline bool isOverWidget(ImVec2 screenPoint) {
        for (const ImRect& rectangle : gRectangles)
            if (rectangle.Contains(screenPoint))
                return true;

        return false;
    }
}