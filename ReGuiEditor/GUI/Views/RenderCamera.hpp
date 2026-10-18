#pragma once
#include <ImGui/imgui.h>

#include <algorithm>

class RenderCamera {
public:
	static constexpr float MIN_ZOOM = 0.1f;
	static constexpr float MAX_ZOOM = 1000.0f;

	ImVec2 pan = { 0.0f, 0.0f };
	float zoom = 1.0f;

	// Avoid manually setting the zoom variable, use this or otherwise
	// your not getting clamping

	inline void setZoom(float zoom) {
		this->zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
	}

	inline void zoomBy(float factor) {
		this->setZoom(this->zoom * factor);
	}
	
	inline void resetZoom() {
		this->zoom = 1.0f;
	}

	inline void resetPan() {
		this->pan = { 0.0f, 0.0f };
	}

	inline void zoomAt(float factor, const ImVec2& anchor) {
		const ImVec2 contentUnderAnchor = {
			this->pan.x + anchor.x / this->zoom,
			this->pan.y + anchor.y / this->zoom
		};

		this->zoomBy(factor);

		this->pan.x = contentUnderAnchor.x - anchor.x / this->zoom;
		this->pan.y = contentUnderAnchor.y - anchor.y / this->zoom;
	}

	inline void panBy(const ImVec2& viewDelta) {
		this->pan.x -= viewDelta.x / this->zoom;
		this->pan.y -= viewDelta.y / this->zoom;
	}

	inline void fit(const ImVec2& contentSize, const ImVec2& viewSize) {
		if (contentSize.x <= 0.0f || contentSize.y <= 0.0f)
			return;

		this->setZoom(std::min(viewSize.x / contentSize.x, viewSize.y / contentSize.y));

		this->pan.x = (contentSize.x - viewSize.x / this->zoom) * 0.5f;
		this->pan.y = (contentSize.y - viewSize.y / this->zoom) * 0.5f;
	}

	inline ImVec2 contentToView(const ImVec2& point) const {
		return {
			(point.x - this->pan.x) * this->zoom,
			(point.y - this->pan.y) * this->zoom
		};
	}
};
