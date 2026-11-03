#pragma once
#include <GUI/View.hpp>

#include <filesystem>
#include <string>

namespace ReGuiEditor {
	class LayoutManagerView : public ReGuiEditor::View {
		std::filesystem::path lastOpenedLayoutPath;
		std::string lastOpenedLayoutContentPath;

		void openLayout(bool force);
	public:
		void renderMenu() override;
	};
}