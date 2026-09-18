#include "ExplorerPanel.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

void ExplorerPanel::render() {
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;

	ImGui::SetNextWindowClass(&windowClass);

	ImGui::Begin("Explorer");
	ImGui::Text("Explorer");
	ImGui::End();
}
