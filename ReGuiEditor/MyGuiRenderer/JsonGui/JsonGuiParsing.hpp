#pragma once
#include "JsonGuiTypes.hpp"

#include <JSON/JSON.hpp>

#include <string>
#include <string_view>

namespace JsonGui {
	[[nodiscard]] bool isReservedKey(std::string_view key);
	[[nodiscard]] bool isHandledKey(std::string_view key);

	[[nodiscard]] bool isEventKey(std::string_view key);
	[[nodiscard]] bool toEventType(std::string_view key, GuiEventType& type);

	[[nodiscard]] bool toPropertyValue(const JSONValue& value, std::string& output);

	[[nodiscard]] const JSONValue& getValue(const JSONValue& node, std::string_view key);
	[[nodiscard]] std::string getString(const JSONValue& node, std::string_view key, std::string_view fallback = {});
}
