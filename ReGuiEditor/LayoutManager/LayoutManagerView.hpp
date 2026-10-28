#pragma once
#include <GUI/View.hpp>

class LayoutManagerView : public View {
	void openLayout(bool force);
public:
	void renderMenu() override;
};