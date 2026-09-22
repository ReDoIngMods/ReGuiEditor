#include "FontManager.hpp"

#include "../FilesystemManager/FilesystemManager.hpp"

#include "Lucide/IconsLucide.h"

#include "ImGui/imgui.h"

#include <cmath>
#include <filesystem>
#include <string>

inline static void applyLucideIcons(float fontSize) {
	std::filesystem::path resolvedPath;
	if (!FilesystemManager::getInstance().resolveDirectory("$RGE_DATA/Fonts/Lucide.ttf", resolvedPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resolve Lucide font path");
		return;
	}
	
	static const ImWchar iconRanges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };

	const float baseIconFontSize = fontSize * 2.f / 3.f;
	const float iconFontSize = std::round(baseIconFontSize * 1.5f);

	const float offsetX =  (baseIconFontSize - iconFontSize) * 0.5f;
	const float offsetY = -(baseIconFontSize - iconFontSize) * 0.5f;

	ImFontConfig iconFontConfig;
	iconFontConfig.MergeMode = true;
	iconFontConfig.PixelSnapH = true;
	iconFontConfig.GlyphMinAdvanceX = iconFontSize;
	iconFontConfig.FontDataOwnedByAtlas = false;
	iconFontConfig.GlyphOffset = { offsetX, offsetY };

	ImGui::GetIO().Fonts->AddFontFromFileTTF(resolvedPath.string().c_str(), iconFontSize, &iconFontConfig, iconRanges);
}

bool FontManager::addFont(const std::string& fontPath, float fontSize) {
	FilesystemManager& filesystemManager = FilesystemManager::getInstance();
	
	std::filesystem::path resolvedPath;
	if (!filesystemManager.resolveDirectory(fontPath, resolvedPath)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resolve font path: %s", fontPath.c_str());
		return false;
	}
	
	ImGuiIO& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontFromFileTTF(resolvedPath.string().c_str(), fontSize);
	if (!font) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to load font: %s", fontPath.c_str());
		return false;
	}

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Loaded font: %s", fontPath.c_str());

	this->fonts_[resolvedPath.stem().string()] = font;
	return true;
}

bool FontManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing FontManager");

	constexpr float defaultFontSize = 16.0f;

	if (!this->addFont("$RGE_DATA/Fonts/Geist-Medium.ttf", defaultFontSize)) return false;

	applyLucideIcons(defaultFontSize);

	if (!this->addFont("$RGE_DATA/Fonts/Futura.otf", defaultFontSize)) return false;
	if (!this->addFont("$RGE_DATA/Fonts/IBM-VGA-8x16.ttf", defaultFontSize)) return false;

	return true;
}

ImFont* FontManager::getFont(const std::string& fontName) {
	auto iterator = this->fonts_.find(fontName);
	if (iterator != this->fonts_.end())
		return iterator->second;

	CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Font not found: %s", fontName.c_str());
	return nullptr;
}