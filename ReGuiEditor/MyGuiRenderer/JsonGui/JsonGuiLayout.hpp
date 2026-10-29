#pragma once
#include <MyGui/MyGUI_Align.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Widget.h>

#include <string_view>
#include <vector>

namespace JsonGui {
	enum class LayoutRule {
		Absolute,
		Vertical,
		Horizontal,
		HorizontalRight,
		Center,
		Grid
	};

	struct FlowItem {
		MyGUI::Widget* widget;
		MyGUI::IntPoint offset;
	};

	[[nodiscard]] LayoutRule toLayoutRule(std::string_view value);
	[[nodiscard]] MyGUI::Align toAlign(std::string_view horizontal, std::string_view vertical);

	void applyAnchor(MyGUI::Widget* widget, std::string_view anchor, const MyGUI::IntSize& parentSize, const MyGUI::IntPoint& offset);
	void applyLayout(MyGUI::Widget* parent, LayoutRule rule, const std::vector<FlowItem>& items, const MyGUI::IntSize& gridItemSize);
}
