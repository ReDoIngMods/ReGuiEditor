#include "JsonGuiOverlay.hpp" // VCIC-Excluded

#include <MyGui/MyGUI_Align.h>
#include <MyGui/MyGUI_Colour.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_ImageBox.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Widget.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

constexpr auto BLANK_SKINS = std::to_array<std::string_view>({ "PanelEmpty", "EditBoxEmpty", "ButtonEmptySkin" });

constexpr std::string_view OVERLAY_SHADER = "Dashed";
constexpr std::string_view OVERLAY_TEXTURE = "ReGuiDashedOverlay";
constexpr std::string_view OVERLAY_SHADER_FILE = "$RGE_DATA/Shaders/gui_dashed.hlsl";

constexpr auto GREEN_OVERLAY_SKINS = std::to_array<std::string_view>({
	"PanelEmpty",
});

constexpr auto GREEN_OVERLAY_TYPES = std::to_array<std::string_view>({
	"TextBox",
	"EditBox",
	"EditBoxStretch",
	"EditBoxEmpty",
	"EditBox_LiftInput",
	"WordWrapEmpty",
	"AdvanceEditBox"
});

const MyGUI::Colour GREEN_OVERLAY_COLOUR(0.0f, 1.0f, 0.0f);
const MyGUI::Colour ORANGE_OVERLAY_COLOUR(1.0f, 0.5f, 0.0f);

bool JsonGui::isBlankSkin(std::string_view skin) {
	return std::ranges::find(BLANK_SKINS, skin) != BLANK_SKINS.end();
}

void JsonGui::addBlankSkinOverlay(MyGUI::Widget* widget, std::string_view skin) {
	MyGUI::Widget* part = widget->createWidgetT("ImageBox", "ImageBox", MyGUI::IntCoord(0, 0, widget->getWidth(), widget->getHeight()), MyGUI::Align::Stretch);

	part->setNeedMouseFocus(false);
	part->setNeedKeyFocus(false);
	
	bool useGreenOverlay = std::find(GREEN_OVERLAY_SKINS.begin(), GREEN_OVERLAY_SKINS.end(), skin) != GREEN_OVERLAY_SKINS.end();
	if (!useGreenOverlay)
		useGreenOverlay = std::find(GREEN_OVERLAY_TYPES.begin(), GREEN_OVERLAY_TYPES.end(), widget->getTypeName()) != GREEN_OVERLAY_TYPES.end();
	
	part->setColour(useGreenOverlay ? GREEN_OVERLAY_COLOUR : ORANGE_OVERLAY_COLOUR);
	if (MyGUI::ImageBox* image = part->castType<MyGUI::ImageBox>(false))
		image->setImageTexture(std::string(OVERLAY_TEXTURE));
}

void JsonGui::registerOverlayShader() {
	MyGUI::RenderManager& renderManager = MyGUI::RenderManager::getInstance();

	const std::string shaderName(OVERLAY_SHADER);
	const std::string shaderPath(OVERLAY_SHADER_FILE);
	renderManager.registerShader(shaderName, shaderPath, shaderPath);

	MyGUI::ITexture* texture = renderManager.createTexture(std::string(OVERLAY_TEXTURE));
	if (!texture) {
		CONSOLE_ERROR("JsonGui: failed to create the blank skin overlay texture.");
		return;
	}

	texture->createManual(1, 1, MyGUI::TextureUsage::Default, MyGUI::PixelFormat::R8G8B8A8);
	texture->setShader(shaderName);
}