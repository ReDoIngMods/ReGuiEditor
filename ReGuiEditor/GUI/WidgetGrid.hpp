#pragma once
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <UTILS/Strings.hpp>

#include <string>

// WidgetGrid's are if not the best way of creating layouts for ImGui. Allows you to split a region into sub regions.

struct WidgetGrid {
private:
	ImRect rect_;

	enum Axis {
		AxisX = 0,
		AxisY = 1
	};

	WidgetGrid cut(Axis axis, bool fromMax, float scale, float offset, float paddingScale, float paddingOffset) {
		const float extent = this->rect_.Max[axis] - this->rect_.Min[axis];
		const float size = ImClamp(extent * scale + offset, 0.0f, extent);
		const float gap = ImClamp(extent * paddingScale + paddingOffset, 0.0f, extent - size);

		ImRect part = this->rect_;
		if (fromMax) {
			part.Min[axis] = this->rect_.Max[axis] - size;
			this->rect_.Max[axis] = part.Min[axis] - gap;
		} else {
			part.Max[axis] = this->rect_.Min[axis] + size;
			this->rect_.Min[axis] = part.Max[axis] + gap;
		}

		return WidgetGrid(part);
	}
public:
	WidgetGrid() = default;
	WidgetGrid(const ImRect& rectangle) : rect_(rectangle) {}
	WidgetGrid(const ImVec2& min, const ImVec2& max) : rect_(min, max) {}
	WidgetGrid(float x1, float y1, float x2, float y2) : rect_(x1, y1, x2, y2) {}

	static WidgetGrid fromCurrentWindow(bool applyWindowPadding = true) {
		ImVec2 min = ImGui::GetWindowPos();
		ImVec2 max = min + ImGui::GetWindowSize();

		if (applyWindowPadding) {
			const ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;
			min += windowPadding;
			max -= windowPadding;
		}

		return WidgetGrid(min, max);
	}

	static WidgetGrid fromCurrentWindowContentRegion() {
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 max = min + ImGui::GetContentRegionAvail();

		return WidgetGrid(min, max);
	}

	static WidgetGrid fromItemRect() {
		return WidgetGrid(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	}

	ImRect getRectangle() const { return this->rect_; }
	ImVec2 getCenter() const { return this->rect_.GetCenter(); }
	float getWidth() const { return this->rect_.GetWidth(); }
	float getHeight() const { return this->rect_.GetHeight(); }

	ImVec2 getPosition() const { return this->rect_.Min; }
	ImVec2 getSize() const { return this->rect_.GetSize(); }

	ImVec2 getMin() const { return this->rect_.Min; }
	ImVec2 getMax() const { return this->rect_.Max; }

	ImVec2 getCenterForElement(const ImVec2& elementSize) const {
		const ImVec2 center = this->getCenter();
		return ImVec2(center.x - (elementSize.x * 0.5f), center.y - (elementSize.y * 0.5f));
	}

	void drawDebug(const char* text) const {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(this->rect_.Min, this->rect_.Max, IM_COL32(255, 0, 0, 255));

		const std::string debugText = UTILS::formatString("%s (%d, %d)", text, static_cast<int>(this->getWidth()), static_cast<int>(this->getHeight()));

		ImVec2 textSize = ImGui::CalcTextSize(debugText.data());
		drawList->AddText(this->getCenterForElement(textSize), IM_COL32_WHITE, debugText.data());
	}

	// Well i would love to make my own UDim and UDim2... but its kinda overkill for JUST this so lets not do that.

	WidgetGrid splitLeft  (float scale, float offset = 0.0f, float paddingScale = 0.f, float paddingOffset = 0.f) { return this->cut(AxisX, false, scale, offset, paddingScale, paddingOffset); }
	WidgetGrid splitTop   (float scale, float offset = 0.0f, float paddingScale = 0.f, float paddingOffset = 0.f) { return this->cut(AxisY, false, scale, offset, paddingScale, paddingOffset); }
	WidgetGrid splitRight (float scale, float offset = 0.0f, float paddingScale = 0.f, float paddingOffset = 0.f) { return this->cut(AxisX, true,  scale, offset, paddingScale, paddingOffset); }
	WidgetGrid splitBottom(float scale, float offset = 0.0f, float paddingScale = 0.f, float paddingOffset = 0.f) { return this->cut(AxisY, true,  scale, offset, paddingScale, paddingOffset); }

	WidgetGrid splitLeftPixels  (float pixels, float paddingPixels = 0.f) { return this->cut(AxisX, false, 0.f, pixels, 0.f, paddingPixels); }
	WidgetGrid splitTopPixels   (float pixels, float paddingPixels = 0.f) { return this->cut(AxisY, false, 0.f, pixels, 0.f, paddingPixels); }
	WidgetGrid splitRightPixels (float pixels, float paddingPixels = 0.f) { return this->cut(AxisX, true,  0.f, pixels, 0.f, paddingPixels); }
	WidgetGrid splitBottomPixels(float pixels, float paddingPixels = 0.f) { return this->cut(AxisY, true,  0.f, pixels, 0.f, paddingPixels); }

	WidgetGrid splitLeftScale  (float scale, float paddingScale = 0.f) { return this->cut(AxisX, false, scale, 0.f, paddingScale, 0.f); }
	WidgetGrid splitTopScale   (float scale, float paddingScale = 0.f) { return this->cut(AxisY, false, scale, 0.f, paddingScale, 0.f); }
	WidgetGrid splitRightScale (float scale, float paddingScale = 0.f) { return this->cut(AxisX, true,  scale, 0.f, paddingScale, 0.f); }
	WidgetGrid splitBottomScale(float scale, float paddingScale = 0.f) { return this->cut(AxisY, true,  scale, 0.f, paddingScale, 0.f); }

	WidgetGrid padCopy(const ImVec2& padding) {
		WidgetGrid copy = *this;
		copy.rect_.Min += padding;
		copy.rect_.Max -= padding;

		return copy;
	}

	WidgetGrid& pad(const ImVec2& padding) {
		this->rect_.Min += padding;
		this->rect_.Max -= padding;
		return *this;
	}

	WidgetGrid padYCopy(float padding) {
		WidgetGrid copy = *this;
		copy.rect_.Min.y += padding;
		copy.rect_.Max.y -= padding;
		return copy;
	}

	WidgetGrid& padY(float padding) {
		this->rect_.Min.y += padding;
		this->rect_.Max.y -= padding;
		return *this;
	}

	WidgetGrid padXCopy(float padding) {
		WidgetGrid copy = *this;
		copy.rect_.Min.x += padding;
		copy.rect_.Max.x -= padding;
		return copy;
	}

	WidgetGrid& padX(float padding) {
		this->rect_.Min.x += padding;
		this->rect_.Max.x -= padding;
		return *this;
	}
};