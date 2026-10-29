#pragma once
#include <MyGui/MyGUI_Widget.h>

#include <string_view>

namespace JsonGui {
	[[nodiscard]] bool isBlankSkin(std::string_view skin);

	void addBlankSkinOverlay(MyGUI::Widget* widget, std::string_view skin);

	void registerOverlayShader();
}
