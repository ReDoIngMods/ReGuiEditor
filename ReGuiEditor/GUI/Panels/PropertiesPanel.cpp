#include "PropertiesPanel.hpp"

#include <ImGui/imgui.h>

void PropertiesPanel::render() {
	ImGui::Begin("Properties");
	ImGui::Text("Properties");
	ImGui::End();
}
