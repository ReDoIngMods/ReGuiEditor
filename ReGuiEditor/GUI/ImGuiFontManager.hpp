#pragma once
#include <UTILS/Singleton.hpp>

#include <string>
#include <unordered_map>

#include <ImGui/imgui.h>

class ImGuiFontManager : public UTILS::Singleton<ImGuiFontManager> {
	std::unordered_map<std::string, ImFont*> fonts_;

	bool addFont(const std::string& fontPath, float fontSize);
public:
	ImGuiFontManager() = default;
	ImGuiFontManager(const ImGuiFontManager&) = delete;
	ImGuiFontManager& operator=(const ImGuiFontManager&) = delete;
	ImGuiFontManager(ImGuiFontManager&&) = delete;
	ImGuiFontManager& operator=(ImGuiFontManager&&) = delete;

	bool initialize();

	ImFont* getFont(const std::string& fontName);
};