#include "RenderPanel.hpp"

#include "../GuiSystemManager.hpp"

#include <ImGui/imgui.h>

#include <algorithm>
#include <cmath>

void RenderPanel::drawGrid() const {
	const ImVec2 cursorPosition = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	constexpr float BASE_SIZE = 16.0f;

	constexpr ImU32 BACKGROUND_COLOR = IM_COL32(15, 17, 20, 255);
	constexpr ImU32 CHECKER_COLOR = IM_COL32(20, 22, 26, 255);

	const float cellSize = BASE_SIZE * this->zoom_;

	if (cellSize <= 0.0f || windowSize.x <= 0.0f || windowSize.y <= 0.0f)
		return;

	const float panX = this->pan_.x * this->zoom_;
	const float panY = this->pan_.y * this->zoom_;

	const ImVec2 max = {
		cursorPosition.x + windowSize.x,
		cursorPosition.y + windowSize.y
	};

	drawList->AddRectFilled(cursorPosition, max, BACKGROUND_COLOR);

	const int startX = static_cast<int>(std::floor(panX / cellSize)) - 1;
	const int endX = static_cast<int>(std::floor((panX + windowSize.x) / cellSize)) + 1;
	const int startY = static_cast<int>(std::floor(panY / cellSize)) - 1;
	const int endY = static_cast<int>(std::floor((panY + windowSize.y) / cellSize)) + 1;

	for (int y = startY; y <= endY; ++y) {
		const float top = cursorPosition.y + y * cellSize - panY;
		const float bottom = top + cellSize;

		if (bottom <= cursorPosition.y || top >= max.y)
			continue;

		for (int x = startX; x <= endX; ++x) {
			if (((x ^ y) & 1) == 0)
				continue;

			const float left = cursorPosition.x + x * cellSize - panX;
			const float right = left + cellSize;

			if (right <= cursorPosition.x || left >= max.x)
				continue;

			const ImVec2 topLeft = { std::max(left, cursorPosition.x), std::max(top, cursorPosition.y) };
			const ImVec2 bottomRight = { std::min(right, max.x), std::min(bottom, max.y) };

			drawList->AddRectFilled(topLeft, bottomRight, CHECKER_COLOR);
		}
	}
}

void RenderPanel::render() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
	ImGui::Begin("Render View");

	GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	ImVec2 renderTargetSize = {
		static_cast<float>(guiSystemManager.getRenderTargetWidth()),
		static_cast<float>(guiSystemManager.getRenderTargetHeight())
	};

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();

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
	ImGui::End();

	ImGui::PopStyleVar();
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