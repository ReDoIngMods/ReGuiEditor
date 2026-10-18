#pragma once
#include "RenderCamera.hpp"

#include "../DebugTextHandler.hpp"
#include "../ViewsManager.hpp"

#include <rend/FullscreenPass.hpp>

#include <ImGui/imgui.h>

class UIRenderView : public View {
	RenderCamera camera_;

	rend::FullscreenPass gridPass_;

	DebugTextID debugTextId_ = 0;

	void handleInput(const ImVec2& viewportSize, const ImVec2& renderTargetSize);
	void drawGrid();

	static void gridDrawCallback(const ImDrawList* drawList, const ImDrawCmd* drawCmd);
public:
	bool initialize() override;
	void deinitialize() override;

	void render() override;
	void renderMenu() override;

	void update(double deltaTime) override;

	RenderCamera& getCamera() { return this->camera_; }
	const RenderCamera& getCamera() const { return this->camera_; }
};
