#pragma once
#include "../View.hpp"

#include <ImGui/imgui.h>

class DecorationView : public View {
	bool isBuiltDockspace_ = false;

	void buildDockspace(ImGuiID dockspaceId, ImVec2 size);

	void drawProgamName(const char* name);

	bool drawWindowButton(const char* id, ImTextureID icon, ImVec2 size);
	void drawWindowButtons();

	bool beginMenu(const char* label, bool enabled = true);

	void renderMenubar();
public:
	void render() override;
};