#pragma once
#include <GUI/View.hpp>

#include <filesystem>
#include <string>

class LayoutManagerView : public View {
	std::filesystem::path lastOpenedLayoutPath;
	std::string lastOpenedLayoutContentPath;

	void openLayout(bool force);
public:
	void renderMenu() override;
};