#pragma once
#include <UTILS/Singleton.hpp>

#include <string>
#include <unordered_map>

#include <ImGui/imgui.h>

class FontManager : public UTILS::Singleton<FontManager> {
	std::unordered_map<std::string, ImFont*> fonts_;

	bool addFont(const std::string& fontPath, float fontSize);
public:
	FontManager() = default;
	FontManager(const FontManager&) = delete;
	FontManager& operator=(const FontManager&) = delete;
	FontManager(FontManager&&) = delete;
	FontManager& operator=(FontManager&&) = delete;

	bool initialize();

	ImFont* getFont(const std::string& fontName);
};