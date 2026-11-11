#include "JsonGuiInstance.hpp"

#include "../Filesystem/FileReader.hpp"

#include "GuiSystemManager.hpp"

#include "JsonGui/JsonGuiLayout.hpp"
#include "JsonGui/JsonGuiOverlay.hpp"
#include "JsonGui/JsonGuiParsing.hpp"

#include <JSON/JSONObject.hpp>

#include <MyGui/MyGUI_Align.h>
#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Widget.h>

#include <cmath>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

constexpr int MAX_BUILD_DEPTH = 128;

using EventBinding = std::pair<std::string_view, const JSONValue*>;

[[nodiscard]] static MyGUI::Widget* createWidget(MyGUI::Widget* parent, const std::string& layer, const std::string& type, const std::string& skin, const MyGUI::IntCoord& coord, const std::string& name) {
	try {
		if (parent)
			return parent->createWidgetT(type, skin, coord, MyGUI::Align::Default, name);

		return MyGUI::Gui::getInstance().createWidgetT(type, skin, coord, MyGUI::Align::Default, layer, name);
	} catch (const std::exception& exception) {
		CONSOLE_ERROR("JsonGui: failed to create widget '%s' (%s): %s", name.c_str(), type.c_str(), exception.what());
		return nullptr;
	}
}

static void applyAlignment(MyGUI::Widget* widget, const JSONValue& node) {
	const JSONValue& horizontalAlign = node.at("HorizontalAlign");
	const JSONValue& verticalAlign = node.at("VerticalAlign");
	if (!horizontalAlign.is<std::string>() && !verticalAlign.is<std::string>())
		return;

	widget->setAlign(JsonGui::toAlign(JsonGui::getString(node, "HorizontalAlign"), JsonGui::getString(node, "VerticalAlign")));
}

static void applyNodeAnchor(MyGUI::Widget* widget, const JSONValue& node, MyGUI::Widget* parent, const MyGUI::IntCoord& coord) {
	const std::string* anchor = node.at("Anchor").tryGet<std::string>();
	if (!anchor)
		return;

	const MyGUI::IntSize parentSize = parent ? parent->getClientCoord().size() : MyGUI::RenderManager::getInstance().getViewSize();
	JsonGui::applyAnchor(widget, *anchor, parentSize, coord.point());
}

static void applyProperties(MyGUI::Widget* widget, const JSONValue& node, const std::string& name, std::vector<EventBinding>& events) {
	std::string propertyValue;
	for (const auto& [key, value] : node.get<JSONObject>()) {
		if (JsonGui::isReservedKey(key) || JsonGui::isHandledKey(key))
			continue;

		if (JsonGui::isEventKey(key)) {
			events.emplace_back(key, &value);
			continue;
		}

		if (!JsonGui::toPropertyValue(value, propertyValue)) {
			if (!value.isNull())
				CONSOLE_WARNING("JsonGui: property '%s' of widget '%s' has an unsupported value type.", key.c_str(), name.c_str());

			continue;
		}

		widget->setProperty(key, propertyValue);
	}
}

bool JsonGuiInstance::loadSubLayout(const std::string& path, int depth, JSONValue& output) {
	if (path.empty()) {
		CONSOLE_WARNING("JsonGui: SubLayout has no 'SubLayoutPath'.");
		return false;
	}

	if (depth >= MAX_BUILD_DEPTH) {
		CONSOLE_ERROR("JsonGui: SubLayout '%s' exceeds the maximum nesting depth.", path.c_str());
		return false;
	}

	auto jsonContent = Filesystem::readFileJson(path);
	if (!jsonContent) {
		CONSOLE_ERROR("JsonGui: failed to read or parse SubLayout '%s'.", path.c_str());
		return false;
	}

	output = std::move(jsonContent.value());
	return true;
}

MyGUI::Widget* JsonGuiInstance::build(const JSONValue& node, MyGUI::Widget* parent, int depth) {
	if (!node.is<JSONObject>()) {
		CONSOLE_ERROR("JsonGui: widget node is not an object.");
		return nullptr;
	}

	const std::string type = JsonGui::getString(node, "Type", "Widget");
	const std::string name = JsonGui::getString(node, "Name");

	JSONValue subLayout;
	const bool hasSubLayout = type == "SubLayout" && loadSubLayout(JsonGui::getString(node, "SubLayoutPath"), depth, subLayout);

	const std::string skin = hasSubLayout ? "PanelEmpty" : JsonGui::getString(node, "Skin");
	const JSONValue& sizeSource = hasSubLayout ? subLayout : node;

	MyGUI::IntCoord coord(
		node.at("x").getOrDefault(0),
		node.at("y").getOrDefault(0),

		sizeSource.at("width").getOrDefault(0),
		sizeSource.at("height").getOrDefault(0)
	);

	GuiSystemManager& guiSystemManager = GuiSystemManager::getInstance();
	float screenWidth = static_cast<float>(guiSystemManager.getRenderTargetWidth());
	float screenHeight = static_cast<float>(guiSystemManager.getRenderTargetHeight());

	float scaleX = screenWidth / 1280;
	float scaleY = screenHeight / 720;

	coord.width = static_cast<int>(std::ceil(coord.width * scaleX));
	coord.height = static_cast<int>(std::ceil(coord.height * scaleY));
	coord.left = static_cast<int>(std::ceil(coord.left * scaleX));
	coord.top = static_cast<int>(std::ceil(coord.top * scaleY));

	MyGUI::Widget* widget = createWidget(parent, this->settings_.layer, type, skin, coord, name);
	if (!widget)
		return nullptr;

	if (JsonGui::isBlankSkin(skin))
		JsonGui::addBlankSkinOverlay(widget, skin);

	applyAlignment(widget, node);
	applyNodeAnchor(widget, node, parent, coord);

	std::vector<EventBinding> events;
	applyProperties(widget, node, name, events);

	for (const auto& [key, value] : events)
		this->bindEvent(widget, name, key, *value, depth == 0);

	std::vector<const JSONValue*> childNodes;
	if (const JSONArray* childs = node.at("Childs").tryGet<JSONArray>()) {
		for (const JSONValue& child : *childs)
			childNodes.push_back(&child);
	}

	if (hasSubLayout)
		childNodes.push_back(&subLayout);

	std::vector<JsonGui::FlowItem> flow;
	for (const JSONValue* child : childNodes) {
		MyGUI::Widget* built = this->build(*child, widget, depth + 1);
		if (!built || child->at("Anchor").is<std::string>())
			continue;

		flow.emplace_back(built, built->getPosition());
	}

	const JSONValue& gridItemSize = node.at("GridItemSize");
	const MyGUI::IntSize gridSize(
		static_cast<int>(std::ceil(gridItemSize.at("width").getOrDefault(0) * scaleX)),
		static_cast<int>(std::ceil(gridItemSize.at("height").getOrDefault(0) * scaleY))
	);

	JsonGui::applyLayout(widget, JsonGui::toLayoutRule(JsonGui::getString(node, "LayoutRule")), flow, gridSize);

	return widget;
}

void JsonGuiInstance::destroyRoot() {
	this->widgetEvents_.clear();
	if (this->root_ && MyGUI::Gui::getInstancePtr())
		MyGUI::Gui::getInstance().destroyWidget(this->root_);

	this->root_ = nullptr;
}

bool JsonGuiInstance::render(const JSONValue& document) {
	this->destroyRoot();

	this->closeHandler_.clear();
	this->rootName_.clear();

	this->root_ = this->build(document, nullptr, 0);
	if (!this->root_) {
		this->active_ = false;
		return false;
	}

	this->root_->setVisible(!this->hidden_);
	this->active_ = true;

	return true;
}

void JsonGuiInstance::close() {
	this->destroyRoot();
	this->active_ = false;
}

void JsonGuiInstance::setHidden(bool hidden) {
	this->hidden_ = hidden;

	if (this->root_)
		this->root_->setVisible(!hidden);
}

MyGUI::Widget* JsonGuiInstance::findWidget(const std::string& name) const {
	return this->root_ ? this->root_->findWidget(name) : nullptr;
}
