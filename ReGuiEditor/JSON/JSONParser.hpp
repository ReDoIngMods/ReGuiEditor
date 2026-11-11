#pragma once
#include "JSONObject.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class JSONParser {
	std::string_view input_;
	std::size_t position_ = 0;

	void skipWhitespace() noexcept;
	bool skipComment() noexcept;

	bool consumeCharacter(const char character) noexcept;
	bool expectCharacter(const char character);

	bool matchText(std::string_view text) noexcept;

	std::size_t skipDigits() noexcept;

	bool parseHex4(uint32_t& codePoint);
	bool parseStringRaw(std::string& result);

	bool parseString(JSONValue& value);
	bool parseNumber(JSONValue& value);
	bool parseArray(JSONValue& value);
	bool parseObject(JSONValue& value);

	bool parseValue(JSONValue& value);

	JSONParser(std::string_view input) : input_(input) {}
public:
	[[nodiscard]] static std::optional<JSONValue> parseJson(std::string_view input);
};