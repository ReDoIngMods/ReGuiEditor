#pragma once
#include <ImGui/imgui.h>

class WindowDecoration {
	bool isBuiltDockspace_ = false;

	void buildDockspace(ImGuiID dockspaceId, ImVec2 size);
public:
	void render();
};