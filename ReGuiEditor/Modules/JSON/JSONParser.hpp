#pragma once
#include "JSONObject.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace ReGuiEditor {
	class JSONParser {
		std::string_view input_;
		std::size_t position_;

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
	public:
		explicit JSONParser(std::string_view input) : input_(input), position_(0) {}

		bool parse(JSONValue& value);

		static bool parseJson(std::string_view input, JSONValue& value);
		static bool parseJson(const char* data, std::size_t size, JSONValue& value);
	};
}