#include "JsonGuiLayout.hpp"

#include <MyGui/MyGUI_Align.h>
#include <MyGui/MyGUI_ScrollView.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Widget.h>

#include <algorithm>
#include <string_view>
#include <vector>

[[nodiscard]] inline static bool containsWord(std::string_view text, std::string_view word) {
	return text.find(word) != std::string_view::npos;
}

JsonGui::LayoutRule JsonGui::toLayoutRule(std::string_view value) {
	if (value == "Vertical") return LayoutRule::Vertical;
	if (value == "Horizontal") return LayoutRule::Horizontal;
	if (value == "HorizontalRight") return LayoutRule::HorizontalRight;
	if (value == "Center") return LayoutRule::Center;
	if (value == "Grid") return LayoutRule::Grid;

	return LayoutRule::Absolute;
}

MyGUI::Align JsonGui::toAlign(std::string_view horizontal, std::string_view vertical) {
	MyGUI::Align horizontalAlign = MyGUI::Align::Left;
	if      (horizontal == "Centered") horizontalAlign = MyGUI::Align::HCenter;
	else if (horizontal == "Right"   ) horizontalAlign = MyGUI::Align::Right;
	else if (horizontal == "Stretch" ) horizontalAlign = MyGUI::Align::HStretch;

	MyGUI::Align verticalAlign = MyGUI::Align::Top;
	if      (vertical == "Centered") verticalAlign = MyGUI::Align::VCenter;
	else if (vertical == "Bottom"  ) verticalAlign = MyGUI::Align::Bottom;
	else if (vertical == "Stretch" ) verticalAlign = MyGUI::Align::VStretch;

	return horizontalAlign | verticalAlign;
}

void JsonGui::applyAnchor(MyGUI::Widget* widget, std::string_view anchor, const MyGUI::IntSize& parentSize, const MyGUI::IntPoint& offset) {
	const bool left = containsWord(anchor, "Left");
	const bool right = containsWord(anchor, "Right");
	const bool top = containsWord(anchor, "Top");
	const bool bottom = containsWord(anchor, "Bottom");

	const MyGUI::IntSize size = widget->getSize();
	const int baseX = left ? 0 : (right ? parentSize.width - size.width : (parentSize.width - size.width) / 2);
	const int baseY = top ? 0 : (bottom ? parentSize.height - size.height : (parentSize.height - size.height) / 2);

	widget->setPosition(baseX + offset.left, baseY + offset.top);
}

void JsonGui::applyLayout(MyGUI::Widget* parent, LayoutRule rule, const std::vector<FlowItem>& items, const MyGUI::IntSize& gridItemSize) {
	if (rule == LayoutRule::Absolute || items.empty())
		return;

	const MyGUI::IntSize parentSize = parent->getClientCoord().size();
	int cursor = rule == LayoutRule::HorizontalRight ? parentSize.width : 0;

	const int columns = std::max(1, gridItemSize.width > 0 ? parentSize.width / gridItemSize.width : 1);
	int contentHeight = 0;
	int index = 0;

	for (const auto& [widget, offset] : items) {
		const MyGUI::IntSize size = widget->getSize();

		MyGUI::IntPoint position;
		switch (rule) {
		case LayoutRule::Vertical:
			position = { offset.left, cursor + offset.top };
			cursor = position.top + size.height;
			break;
		case LayoutRule::Horizontal:
			position = { cursor + offset.left, offset.top };
			cursor = position.left + size.width;
			break;
		case LayoutRule::HorizontalRight:
			position = { cursor - size.width - offset.left, offset.top };
			cursor = position.left;
			break;
		case LayoutRule::Center:
			position = { (parentSize.width - size.width) / 2 + offset.left, (parentSize.height - size.height) / 2 + offset.top };
			break;
		case LayoutRule::Grid:
			position = { (index % columns) * gridItemSize.width + offset.left, (index / columns) * gridItemSize.height + offset.top };
			break;
		default:
			break;
		}

		widget->setPosition(position);
		contentHeight = std::max(contentHeight, position.top + size.height);
		++index;
	}

	if (MyGUI::ScrollView* scrollView = parent->castType<MyGUI::ScrollView>(false))
		scrollView->setCanvasSize(parentSize.width, contentHeight);
}
