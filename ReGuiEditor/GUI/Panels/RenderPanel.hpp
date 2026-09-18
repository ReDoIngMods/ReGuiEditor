#pragma once
#include "Panel.hpp"

#include <ImGui/imgui.h>
#include <algorithm>

class RenderPanel : public Panel {
	float zoom_ = 1.0f;
	ImVec2 pan_ = { 0.0f, 0.0f };

	void drawGrid() const;
public:
	void render() override;

	void renderMenu() override;

	float getMinZoom() const { return 0.1f; }
	float getMaxZoom() const { return 1000.0f; }
	float getZoom() const { return this->zoom_; }
	void setZoom(float zoom) { this->zoom_ = std::clamp(zoom, this->getMinZoom(), this->getMaxZoom()); }
	void resetZoom() { this->zoom_ = 1.0f; }

	ImVec2 getPan() const { return this->pan_; }
	void setPan(const ImVec2& pan) { this->pan_ = pan; }
	void resetPan() { this->pan_ = { 0.0f, 0.0f }; }
};
