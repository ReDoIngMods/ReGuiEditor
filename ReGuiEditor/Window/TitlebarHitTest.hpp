#pragma once
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <vector>

namespace ReGuiEditor {
	class TitlebarHitTest {
		std::vector<ImRect> rectangles_;
	public:
		void beginFrame() {
			this->rectangles_.clear();
		}

		void addRect(ImVec2 min, ImVec2 max) {
			this->rectangles_.emplace_back(min, max);
		}

		[[nodiscard]] bool isOverWidget(ImVec2 screenPoint) const {
			for (const ImRect& rectangle : this->rectangles_)
				if (rectangle.Contains(screenPoint))
					return true;

			return false;
		}
	};
}