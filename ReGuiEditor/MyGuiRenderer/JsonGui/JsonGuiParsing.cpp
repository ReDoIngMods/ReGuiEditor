#include "JsonGuiParsing.hpp" // VCIC-Excluded

#include "JsonGuiTypes.hpp"

#include <JSON/JSON.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string>
#include <string_view>

constexpr std::array<std::string_view, 8> RESERVED_KEYS = { "Type", "Skin", "Name", "x", "y", "width", "height", "Childs" };

constexpr auto HANDLED_KEYS = std::to_array<std::string_view>({
	"Anchor",             "LayoutRule",          "GridItemSize",   "HorizontalAlign",   "VerticalAlign",
	"RenderDepth",        "Layer",               "Hotbar",         "ToolTip",           "Popup",
	"Cursor",             "DisableOnFocusSound", "HeightFromText", "BottomAnchored",    "GridScrollOffset",
	"NeedScroll",         "CropEffect",          "CroppingWidget", "Effects",           "ContainerData",
	"BlueprintPreview",   "ItemPreview",         "SubLayoutPath",  "RotatingSkinAngle", "RotatingSkinCenterX",
	"RotatingSkinCenterY"
});

struct EventKey {
	std::string_view key;
	GuiEventType type;
};

constexpr auto EVENT_KEYS = std::to_array<EventKey>({
	{"onClick",      GuiEventType::Click     },
	{"onPressed",    GuiEventType::Pressed   },
	{"onReleased",   GuiEventType::Released  },
	{"onDrag",       GuiEventType::Drag      },
	{"onMouseWheel", GuiEventType::MouseWheel},
	{"onTextEdit",   GuiEventType::TextEdit  },
	{"onTextEnter",  GuiEventType::TextEnter }
});

bool JsonGui::isReservedKey(std::string_view key) { return std::ranges::find(RESERVED_KEYS, key) != RESERVED_KEYS.end(); }
bool JsonGui::isHandledKey(std::string_view key) { return std::ranges::find(HANDLED_KEYS, key) != HANDLED_KEYS.end(); }

bool JsonGui::isEventKey(std::string_view key) {
	return key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
}

bool JsonGui::toEventType(std::string_view key, GuiEventType& type) {
	for (const EventKey& entry : EVENT_KEYS) {
		if (entry.key != key)
			continue;

		type = entry.type;
		return true;
	}

	return false;
}

bool JsonGui::toPropertyValue(const JSONValue& value, std::string& output) {
	switch (value.getType()) {
	case JSONType::String:
		output = value.asString();
		return true;
	case JSONType::Boolean:
		output = value.asBoolean() ? "true" : "false";
		return true;
	case JSONType::Integer:
	case JSONType::FloatingPoint:
		output = std::format("{}", value.getDouble().value());
		return true;
	default:
		return false;
	}
}

const JSONValue& JsonGui::getValue(const JSONValue& node, std::string_view key) {
	static const JSONValue none;
	if (!node.isObject())
		return none;

	const JSONObject& object = node.asObject();
	const auto iterator = object.find(std::string(key));
	return iterator != object.end() ? iterator->second : none;
}

std::string JsonGui::getString(const JSONValue& node, std::string_view key, std::string_view fallback) {
	const JSONValue& value = JsonGui::getValue(node, key);
	return value.isString() ? value.asString() : std::string(fallback);
}
