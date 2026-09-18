#pragma once
#include "Panel.hpp"

#include <ImGui/imgui.h>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>

class RenderPanel : public Panel {
	float zoom_ = 1.0f;
	ImVec2 pan_ = { 0.0f, 0.0f };

	struct DebugRenderInfo {
		bool enabled = false;
		std::function<void()> renderFunction;
	};

	std::unordered_map<std::string, DebugRenderInfo> debugRenderInfos_;

	void drawGrid() const;
	void drawDebugRenderInfos() const;
public:
	bool initialize() override;

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

	void addDebugRenderInfo(const std::string& name, const std::function<void()>& renderFunction) {
		this->debugRenderInfos_[name] = { true, renderFunction };
	}

	void removeDebugRenderInfo(const std::string& name) {
		auto iterator = this->debugRenderInfos_.find(name);
		if (iterator == this->debugRenderInfos_.end()) {
			CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, L"Debug render info with name '%s' does not exist.", name.c_str());
			return;
		}

		this->debugRenderInfos_.erase(iterator);
	}

	const std::unordered_map<std::string, DebugRenderInfo>& getDebugRenderInfos() const {
		return this->debugRenderInfos_;
	}
};
