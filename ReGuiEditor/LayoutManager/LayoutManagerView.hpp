#pragma once
#include <GUI/View.hpp>

#include <UGCManager/UGCMod.hpp>

#include <filesystem>
#include <memory>

namespace ReGuiEditor {
	class LayoutManagerView : public ReGuiEditor::View {
		std::filesystem::path lastOpenedLayoutPath;
		std::shared_ptr<struct UGCMod> lastOpenedLayoutMod;

		void openLayout(bool force);
	public:
		void renderMenu() override;
	};
}