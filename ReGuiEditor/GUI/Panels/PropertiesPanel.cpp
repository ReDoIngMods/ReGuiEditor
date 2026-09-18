#include "PropertiesPanel.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

void PropertiesPanel::render() {
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;

	ImGui::SetNextWindowClass(&windowClass);

	ImGui::Begin("Properties");
	ImGui::Text("Properties");
	ImGui::End();
}
