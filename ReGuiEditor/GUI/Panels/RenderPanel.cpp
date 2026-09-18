#include "RenderPanel.hpp"

#include "../GuiSystemManager.hpp"

#include <ImGui/imgui.h>

#include <algorithm>
#include <cmath>

void RenderPanel::drawGrid() const {
	ImVec2 childPos = ImGui::GetWindowPos();
	ImVec2 childSize = ImGui::GetWindowSize();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	constexpr float baseCheckerSize = 16.0f;
	constexpr ImU32 colorA = IM_COL32(255, 0, 0, 255);
	constexpr ImU32 colorB = IM_COL32(0, 0, 255, 255);

	float checkerSize = baseCheckerSize * this->zoom_;
	float panPixelX = this->pan_.x * this->zoom_;
	float panPixelY = this->pan_.y * this->zoom_;

	int iStart = static_cast<int>(std::floor(panPixelX / checkerSize)) - 1;
	int iEnd = static_cast<int>(std::floor((panPixelX + childSize.x) / checkerSize)) + 1;
	int jStart = static_cast<int>(std::floor(panPixelY / checkerSize)) - 1;
	int jEnd = static_cast<int>(std::floor((panPixelY + childSize.y) / checkerSize)) + 1;

	for (int j = jStart; j <= jEnd; ++j) {
		for (int i = iStart; i <= iEnd; ++i) {
			float x = i * checkerSize - panPixelX;
			float y = j * checkerSize - panPixelY;

			bool isAlt = ((i + j) % 2 + 2) % 2 != 0;

			ImVec2 rectMin = { childPos.x + std::max(x, 0.0f), childPos.y + std::max(y, 0.0f) };
			ImVec2 rectMax = { childPos.x + std::min(x + checkerSize, childSize.x), childPos.y + std::min(y + checkerSize, childSize.y) };

			if (rectMax.x <= rectMin.x || rectMax.y <= rectMin.y) continue;

			drawList->AddRectFilled(rectMin, rectMax, isAlt ? colorA : colorB);
		}
	}
}

void RenderPanel::render() {
	ImGui::Begin("Render View");

	GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	ImVec2 renderTargetSize = {
		static_cast<float>(guiSystemManager.getRenderTargetWidth()),
		static_cast<float>(guiSystemManager.getRenderTargetHeight())
	};

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
	ImGui::BeginChild("RenderView", viewportSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	this->drawGrid();

	ImGuiIO& io = ImGui::GetIO();

	bool hovered = ImGui::IsWindowHovered();
	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 mouseInView = { io.MousePos.x - winPos.x, io.MousePos.y - winPos.y };

	if (hovered) {
		float wheel = io.MouseWheel;
		if (wheel != 0.0f) {
			ImVec2 imagePosUnderMouse = {
				this->pan_.x + mouseInView.x / this->zoom_,
				this->pan_.y + mouseInView.y / this->zoom_
			};

			this->zoom_ = std::clamp(this->zoom_ * (1.0f + wheel * 0.1f), this->getMinZoom(), this->getMaxZoom());

			this->pan_.x = imagePosUnderMouse.x - mouseInView.x / this->zoom_;
			this->pan_.y = imagePosUnderMouse.y - mouseInView.y / this->zoom_;
		}

		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			this->zoom_ = std::clamp(std::min(viewportSize.x / renderTargetSize.x, viewportSize.y / renderTargetSize.y), this->getMinZoom(), this->getMaxZoom());

			this->pan_.x = (renderTargetSize.x - viewportSize.x / this->zoom_) * 0.5f;
			this->pan_.y = (renderTargetSize.y - viewportSize.y / this->zoom_) * 0.5f;
		}
	}

	if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
		ImVec2 delta = io.MouseDelta;
		this->pan_.x -= delta.x / this->zoom_;
		this->pan_.y -= delta.y / this->zoom_;
	}

	ImVec2 imageSize = {
		renderTargetSize.x * this->zoom_,
		renderTargetSize.y * this->zoom_
	};

	ImGui::SetCursorPos({
		-this->pan_.x * this->zoom_,
		-this->pan_.y * this->zoom_
	});


	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::Image(reinterpret_cast<ImTextureID>(guiSystemManager.renderShaderResourceView), imageSize, { 0, 0 }, { 1, 1 });
	ImGui::PopStyleVar();

	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
}

void RenderPanel::renderMenu() {
	if (ImGui::BeginMenu("Render")) {
		// Zooming
		if (ImGui::MenuItem("Zoom In", "Ctrl + +")) this->setZoom(this->getZoom() * 1.1f);
		if (ImGui::MenuItem("Zoom Out", "Ctrl + -")) this->setZoom(this->getZoom() / 1.1f);

		if (ImGui::MenuItem("Reset Zoom", "Ctrl + 0")) this->resetZoom();

		// Panning
		ImGui::Separator();

		if (ImGui::MenuItem("Reset Pan")) this->resetPan();

		ImGui::EndMenu();
	}
}