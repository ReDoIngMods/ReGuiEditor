#include "RenderPanel.hpp"

#include "../FontManager.hpp"
#include "../GuiSystemManager.hpp"
#include "../NotificationManager.hpp"

#include <rend/FullscreenPass.hpp>

#include <rend/Common/Viewport.hpp>

#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <UTILS/Console.hpp>

#include <algorithm>
#include <cstring>

constexpr float GRID_BASE_SIZE = 45.0f;

struct alignas(16) GridConstants {
	float origin[2] = {};
	float pan[2] = {};
	float cellSize = 1.0f;
	float padding[3] = {};
	float backgroundColor[4] = { 15.0f / 255.0f, 17.0f / 255.0f, 20.0f / 255.0f, 1.0f };
	float checkerColor[4] = { 20.0f / 255.0f, 22.0f / 255.0f, 26.0f / 255.0f, 1.0f };
};

struct GridCallbackData {
	RenderPanel* panel = nullptr;
	ImVec2 windowPosition = { 0.0f, 0.0f };
	ImVec2 pan = { 0.0f, 0.0f };
	float zoom = 1.0f;
};


void RenderPanel::drawDebugRenderInfos() const {
	const GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	const ImVec2 renderTargetSize = {
		static_cast<float>(guiSystemManager.getRenderTargetWidth()),
		static_cast<float>(guiSystemManager.getRenderTargetHeight())
	};

	ImGui::SetCursorPos({ 0, ImGui::GetFrameHeight() });

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
	ImGui::BeginChild("DebugRenderView", { 0.0f, 0.0f }, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);

	FontManager& fontManager = FontManager::getInstance();
	ImFont* font = fontManager.getFont("IBM-VGA-8x16");
	if (font)
		ImGui::PushFont(font, 0.f);

	if (this->debugRenderInfos_.empty()) {
		ImGui::Text("No debug render infos.");
	} else {
		for (const auto& [name, debugRenderInfo] : this->debugRenderInfos_) {
			if (!debugRenderInfo.enabled)
				continue;

			ImGui::Text("========== %-16s ==========", name.c_str());
			
			debugRenderInfo.renderFunction();
		}
	}
	
	ImGui::PopFont();

	ImGui::EndChild();
	ImGui::PopStyleVar();
}

bool RenderPanel::initialize() {
	this->addDebugRenderInfo("RenderPanel Info", [this]() {
		GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();

		ImGui::Text("Zoom: %.2f", this->zoom_);
		ImGui::Text("Pan: (%.2f, %.2f)", this->pan_.x, this->pan_.y);
		ImGui::Text("Render Target Size: (%.2f, %.2f)", static_cast<float>(guiSystemManager.getRenderTargetWidth()), static_cast<float>(guiSystemManager.getRenderTargetHeight()));
	});

	rend::FullscreenPassDesc gridPassDesc;
	gridPassDesc.pixelShader = "$RGE_DATA/Shaders/gui_grid.hlsl";
	gridPassDesc.constantBufferSize = sizeof(GridConstants);
	gridPassDesc.debugName = "RenderPanel::Grid";

	if (!this->gridPass_.init(gridPassDesc))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "RenderPanel::initialize: Failed to create the grid pass, the grid will not be drawn.");

	return true;
}

void RenderPanel::cleanup() {
	this->gridPass_.cleanup();
}

void RenderPanel::gridDrawCallback(const ImDrawList*, const ImDrawCmd* drawCmd) {
	GridCallbackData data;
	memcpy(&data, drawCmd->UserCallbackData, sizeof(GridCallbackData));

	const RGE_ImGui_ImplDX11_FrameInfo* frameInfo = RGE_ImGui_ImplDX11_GetFrameInfo();
	if (!data.panel || !frameInfo || !frameInfo->renderView)
		return;

	rend::Viewport viewport;
	if (!RGE_ImGui_ImplDX11_GetCallbackViewport(drawCmd, viewport))
		return;

	const ImVec2 scale = frameInfo->framebufferScale;

	GridConstants constants;
	constants.origin[0] = (data.windowPosition.x - frameInfo->displayPos.x) * scale.x;
	constants.origin[1] = (data.windowPosition.y - frameInfo->displayPos.y) * scale.y;
	constants.pan[0] = data.pan.x * data.zoom * scale.x;
	constants.pan[1] = data.pan.y * data.zoom * scale.y;
	constants.cellSize = GRID_BASE_SIZE * data.zoom * scale.x;

	rend::FullscreenDrawParams params;
	params.target = frameInfo->renderView;
	params.viewport = viewport;
	params.constants = &constants;

	data.panel->gridPass_.draw(params);
}

void RenderPanel::drawGrid() {
	if (!this->gridPass_.isValid())
		return;

	const ImVec2 windowSize = ImGui::GetWindowSize();
	if (this->zoom_ <= 0.0f || windowSize.x <= 0.0f || windowSize.y <= 0.0f)
		return;

	GridCallbackData data;
	data.panel = this;
	data.windowPosition = ImGui::GetWindowPos();
	data.pan = this->pan_;
	data.zoom = this->zoom_;

	ImGui::GetWindowDrawList()->AddCallback(&RenderPanel::gridDrawCallback, &data, sizeof(data));
}

void RenderPanel::render() {
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;

	ImGui::SetNextWindowClass(&windowClass);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
	ImGui::Begin("Render View");

	const GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	const ImVec2 renderTargetSize = {
		static_cast<float>(guiSystemManager.getRenderTargetWidth()),
		static_cast<float>(guiSystemManager.getRenderTargetHeight())
	};

	const ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild("RenderView", viewportSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImGuiIO& io = ImGui::GetIO();

	const bool hovered = ImGui::IsWindowHovered();
	const ImVec2 winPos = ImGui::GetWindowPos();
	const ImVec2 mouseInView = { io.MousePos.x - winPos.x, io.MousePos.y - winPos.y };

	if (hovered) {
		float wheel = io.MouseWheel;
		if (wheel != 0.0f) {
			const ImVec2 imagePosUnderMouse = {
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
		const ImVec2 delta = io.MouseDelta;
		this->pan_.x -= delta.x / this->zoom_;
		this->pan_.y -= delta.y / this->zoom_;
	}

	this->drawGrid();

	const ImVec2 imageSize = {
		renderTargetSize.x * this->zoom_,
		renderTargetSize.y * this->zoom_
	};

	const ImVec2 imagePositionStart = {
		winPos.x - this->pan_.x * this->zoom_,
		winPos.y - this->pan_.y * this->zoom_
	};

	const ImVec2 imagePositionEnd = {
		imagePositionStart.x + imageSize.x,
		imagePositionStart.y + imageSize.y
	};

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddImage(reinterpret_cast<ImTextureID>(guiSystemManager.getRenderShaderResourceView()), imagePositionStart, imagePositionEnd, { 0, 0 }, { 1, 1 });
	drawList->AddRect(imagePositionStart, imagePositionEnd, IM_COL32(255, 255, 255, 255), 0.0f, 1.0f);

	ImGui::EndChild();
	
	this->drawDebugRenderInfos();

	NotificationManager::getInstance().render();

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