#pragma once
#include <Modules/Misc/Singleton.hpp>

#include <string>
#include <unordered_map>

#include <ImGui/imgui.h>

class ImGuiFontManager : public ReGuiEditor::Singleton<ImGuiFontManager> {
	std::unordered_map<std::string, ImFont*> fonts_;

	bool addFont(const std::string& fontPath, float fontSize);
public:
	bool initialize();

	ImFont* getFont(const std::string& fontName);
};