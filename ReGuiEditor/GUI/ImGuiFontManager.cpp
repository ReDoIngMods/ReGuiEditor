#include "ImGuiFontManager.hpp"

#include <Filesystem/DirectoryManager.hpp>

#include <ImGui/imgui.h>

#include <filesystem>
#include <string>

bool ImGuiFontManager::addFont(const std::string& fontPath, float fontSize) {	
	std::filesystem::path resolvedPath;
	if (!DirectoryManager::getInstance().resolveDirectory(fontPath, resolvedPath)) {
		CONSOLE_ERROR("Failed to resolve font path: %s", fontPath.c_str());
		return false;
	}
	
	ImGuiIO& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontFromFileTTF(resolvedPath.string().c_str(), fontSize);
	if (!font) {
		CONSOLE_ERROR("Failed to load font: %s", fontPath.c_str());
		return false;
	}

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Loaded font: %s", fontPath.c_str());

	this->fonts_[resolvedPath.stem().string()] = font;
	return true;
}

bool ImGuiFontManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ImGuiFontManager");

	constexpr float defaultFontSize = 16.0f;

	if (!this->addFont("$RGE_DATA/Fonts/Geist-Medium.ttf", defaultFontSize)) return false;
	if (!this->addFont("$RGE_DATA/Fonts/Futura.otf", defaultFontSize)) return false;
	if (!this->addFont("$RGE_DATA/Fonts/GeistMono-Medium.ttf", defaultFontSize)) return false;

	return true;
}

ImFont* ImGuiFontManager::getFont(const std::string& fontName) {
	auto iterator = this->fonts_.find(fontName);
	if (iterator != this->fonts_.end())
		return iterator->second;

	CONSOLE_WARNING("Font not found: %s", fontName.c_str());
	return nullptr;
}