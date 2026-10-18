#include "UIRenderView.hpp"

#include "../../MyGuiRenderer/GuiSystemManager.hpp"

#include "../DebugTextHandler.hpp"

#include "../NotificationManager.hpp"
#include "../UIManager.hpp"

#include <rend/FullscreenPass.hpp>

#include <rend/Common/Viewport.hpp>

#include <rend/ImGui/RGE_ImGui_Impl_DX11.hpp>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <UTILS/Console.hpp>

#include <cstring>

constexpr float GRID_BASE_SIZE = 45.0f;

constexpr float ZOOM_WHEEL_SENSITIVITY = 0.1f;
constexpr float ZOOM_MENU_STEP = 1.1f;

struct alignas(16) GridConstants {
	float origin[2] = {};
	float pan[2] = {};
	float cellSize = 1.0f;
	float padding[3] = {};
	float backgroundColor[4] = { 15.0f / 255.0f, 17.0f / 255.0f, 20.0f / 255.0f, 1.0f };
	float checkerColor[4] = { 20.0f / 255.0f, 22.0f / 255.0f, 26.0f / 255.0f, 1.0f };
};

struct GridCallbackData {
	UIRenderView* view = nullptr;
	ImVec2 windowPosition = { 0.0f, 0.0f };
	ImVec2 pan = { 0.0f, 0.0f };
	float zoom = 1.0f;
};

void UIRenderView::drawGrid() {
	if (!this->gridPass_.isValid())
		return;

	const ImVec2 windowSize = ImGui::GetWindowSize();
	if (windowSize.x <= 0.0f || windowSize.y <= 0.0f)
		return;

	GridCallbackData data;
	data.view = this;
	data.windowPosition = ImGui::GetWindowPos();
	data.pan = this->camera_.pan;
	data.zoom = this->camera_.zoom;

	ImGui::GetWindowDrawList()->AddCallback(&UIRenderView::gridDrawCallback, &data, sizeof(data));
}

void UIRenderView::gridDrawCallback(const ImDrawList* drawList, const ImDrawCmd* drawCmd) {
	GridCallbackData data;
	memcpy(&data, drawCmd->UserCallbackData, sizeof(GridCallbackData));

	const RGE_ImGui_ImplDX11_FrameInfo* frameInfo = RGE_ImGui_ImplDX11_GetFrameInfo();
	if (!data.view || !frameInfo || !frameInfo->renderView)
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

	data.view->gridPass_.draw(params);
}

bool UIRenderView::initialize() {
	this->debugTextId_ = UIManager::getInstance().debugTextHandler.addEntry("RenderPanel Info", [this](DebugTextStream& stream) {
		GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
		stream.print("Zoom            = %.2f", this->camera_.zoom);
		stream.print("Pan             = (%.2f, %.2f)", this->camera_.pan.x, this->camera_.pan.y);
		stream.print("Render View Res = (%u, %u)", guiSystemManager.getRenderTargetWidth(), guiSystemManager.getRenderTargetHeight());
	});

	rend::FullscreenPassDesc gridPassDesc;
	gridPassDesc.pixelShader = "$RGE_DATA/Shaders/gui_grid.hlsl";
	gridPassDesc.constantBufferSize = sizeof(GridConstants);
	gridPassDesc.debugName = "RenderPanel::Grid";

	if (!this->gridPass_.init(gridPassDesc))
		CONSOLE_ERROR("RenderPanel::initialize: Failed to create the grid pass, the grid will not be drawn.");

	return true;
}

void UIRenderView::deinitialize() {
	this->gridPass_.cleanup();

	UIManager::getInstance().debugTextHandler.removeEntry(this->debugTextId_);
}

void UIRenderView::handleInput(const ImVec2& viewportSize, const ImVec2& renderTargetSize) {
	if (!ImGui::IsWindowHovered())
		return;

	const ImGuiIO& io = ImGui::GetIO();

	const ImVec2 windowPosition = ImGui::GetWindowPos();
	const ImVec2 mouseInView = { io.MousePos.x - windowPosition.x, io.MousePos.y - windowPosition.y };

	if (io.MouseWheel != 0.0f)
		this->camera_.zoomAt(1.0f + io.MouseWheel * ZOOM_WHEEL_SENSITIVITY, mouseInView);

	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		this->camera_.fit(renderTargetSize, viewportSize);

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		this->camera_.panBy(io.MouseDelta);
}

void UIRenderView::render() {
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoTabBar;

	ImGui::SetNextWindowClass(&windowClass);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
	ImGui::Begin("Render View", NULL, ImGuiWindowFlags_NoTitleBar);

	const GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	const ImVec2 renderTargetSize = {
		static_cast<float>(guiSystemManager.getRenderTargetWidth()),
		static_cast<float>(guiSystemManager.getRenderTargetHeight())
	};

	const ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild("RenderView", viewportSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	this->handleInput(viewportSize, renderTargetSize);
	this->drawGrid();

	const ImVec2 windowPosition = ImGui::GetWindowPos();
	const ImVec2 viewStart = this->camera_.contentToView({ 0.0f, 0.0f });
	const ImVec2 viewEnd = this->camera_.contentToView(renderTargetSize);

	const ImVec2 imagePositionStart = { windowPosition.x + viewStart.x, windowPosition.y + viewStart.y };
	const ImVec2 imagePositionEnd = { windowPosition.x + viewEnd.x, windowPosition.y + viewEnd.y };

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddImage(reinterpret_cast<ImTextureID>(guiSystemManager.getRenderShaderResourceView()), imagePositionStart, imagePositionEnd, { 0, 0 }, { 1, 1 });
	drawList->AddRect(imagePositionStart, imagePositionEnd, IM_COL32(255, 255, 255, 255), 0.0f, 1.0f);

	UIManager::getInstance().debugTextHandler.renderDebugText();

	ImGui::EndChild();

	NotificationManager::getInstance().render();

	ImGui::End();

	ImGui::PopStyleVar();
}

void UIRenderView::renderMenu() {
	if (ImGui::BeginMenu("Render")) {
		// Zooming
		if (ImGui::MenuItem("Zoom In", "Ctrl + +")) this->camera_.zoomBy(ZOOM_MENU_STEP);
		if (ImGui::MenuItem("Zoom Out", "Ctrl + -")) this->camera_.zoomBy(1.0f / ZOOM_MENU_STEP);

		if (ImGui::MenuItem("Reset Zoom", "Ctrl + 0")) this->camera_.resetZoom();

		// Panning
		ImGui::Separator();

		if (ImGui::MenuItem("Reset Pan")) this->camera_.resetPan();

		ImGui::EndMenu();
	}
}

void UIRenderView::update(double deltaTime) {
	NotificationManager::getInstance().update(static_cast<float>(deltaTime));
}