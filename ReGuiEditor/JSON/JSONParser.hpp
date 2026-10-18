#pragma once
#include "JSONObject.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

class JSONParser {
	std::string_view input_;
	std::size_t position_;

	void skipWhitespace() noexcept {
		while (this->position_ < this->input_.size()) {
			const char character = this->input_[this->position_];
			if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
				++this->position_;
				continue;
			}

			if (character == '/' && this->skipComment())
				continue;

			break;
		}
	}

	bool skipComment() noexcept {
		const std::string_view remaining = this->input_.substr(this->position_);

		if (remaining.substr(0, 2) == "//") {
			const std::size_t end = remaining.find('\n');
			this->position_ = end == std::string_view::npos ? this->input_.size() : this->position_ + end;

			return true;
		}

		if (remaining.substr(0, 2) == "/*") {
			const std::size_t end = remaining.find("*/", 2);
			if (end == std::string_view::npos)
				return false;

			this->position_ += end + 2;
			return true;
		}

		return false;
	}

	bool consumeCharacter(const char character) noexcept {
		if (this->position_ < this->input_.size() && this->input_[this->position_] == character) {
			++this->position_;
			return true;
		}

		return false;
	}

	bool expectCharacter(const char character) {
		if (!this->consumeCharacter(character)) {
			CONSOLE_ERROR("Expected character '%c' at position %zu", character, this->position_);
			return false;
		}

		return true;
	}

	bool matchText(std::string_view text) noexcept {
		if (this->input_.substr(this->position_, text.size()) != text)
			return false;

		this->position_ += text.size();
		return true;
	}

	std::size_t skipDigits() noexcept {
		const std::size_t start = this->position_;
		while (this->position_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->position_])))
			++this->position_;

		return this->position_ - start;
	}

	static void appendUTF8Character(std::string& output, uint32_t codePoint) {
		if (codePoint <= 0x7F) {
			output += static_cast<char>(codePoint);
		}
		else if (codePoint <= 0x7FF) {
			output += static_cast<char>(0xC0 | (codePoint >> 6));
			output += static_cast<char>(0x80 | (codePoint & 0x3F));
		}
		else if (codePoint <= 0xFFFF) {
			output += static_cast<char>(0xE0 | (codePoint >> 12));
			output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
			output += static_cast<char>(0x80 | (codePoint & 0x3F));
		}
		else {
			output += static_cast<char>(0xF0 | (codePoint >> 18));
			output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
			output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
			output += static_cast<char>(0x80 | (codePoint & 0x3F));
		}
	}

	bool parseHex4(uint32_t& codePoint) {
		codePoint = 0;
		for (int index = 0; index < 4; ++index) {
			if (this->position_ >= this->input_.size()) {
				CONSOLE_ERROR("Unexpected end of input while parsing Unicode escape at position %zu", this->position_);
				return false;
			}

			const char character = this->input_[this->position_++];
			codePoint <<= 4;
			if (character >= '0' && character <= '9')
				codePoint |= character - '0';
			else if (character >= 'a' && character <= 'f')
				codePoint |= character - 'a' + 10;
			else if (character >= 'A' && character <= 'F')
				codePoint |= character - 'A' + 10;
			else {
				CONSOLE_ERROR("Invalid Unicode escape character '%c' at position %zu", character, this->position_ - 1);
				return false;
			}
		}

		return true;
	}

	bool parseStringRaw(std::string& result) {
		if (!this->expectCharacter('"'))
			return false;

		while (this->position_ < this->input_.size()) {
			const char character = this->input_[this->position_++];
			if (character == '"')
				return true;

			if (character == '\\') {
				if (this->position_ >= this->input_.size()) {
					CONSOLE_ERROR("Unexpected end of input while parsing escape sequence at position %zu", this->position_);
					return false;
				}

				const char escaped = this->input_[this->position_++];
				switch (escaped) {
				case '"': result += '"'; break;
				case '\\': result += '\\'; break;
				case '/': result += '/'; break;
				case 'b': result += '\b'; break;
				case 'f': result += '\f'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				case 'u': {
					uint32_t codePoint;
					if (!this->parseHex4(codePoint))
						return false;

					if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
						if (this->input_.substr(this->position_, 2) != "\\u") {
							CONSOLE_ERROR("Missing low surrogate for high surrogate at position %zu", this->position_);
							return false;
						}

						this->position_ += 2;
						uint32_t lowSurrogate;
						if (!this->parseHex4(lowSurrogate))
							return false;

						if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF) {
							CONSOLE_ERROR("Invalid low surrogate at position %zu", this->position_ - 4);
							return false;
						}

						codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
					}
					else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
						CONSOLE_ERROR("Unexpected low surrogate without preceding high surrogate at position %zu", this->position_ - 4);
						return false;
					}

					appendUTF8Character(result, codePoint);
					break;
				}
				default:
					CONSOLE_ERROR("Invalid escape sequence '\\%c' at position %zu", escaped, this->position_ - 1);
					return false;
				}
			}
			else if (static_cast<unsigned char>(character) < 0x20) {
				CONSOLE_ERROR("Unescaped control character in string at position %zu", this->position_ - 1);
				return false;
			}
			else {
				result += character;
			}
		}

		CONSOLE_ERROR("Unexpected end of input while parsing string at position %zu", this->position_);
		return false;
	}

	bool parseString(JSONValue& value) {
		std::string result;
		if (!this->parseStringRaw(result))
			return false;

		value = std::move(result);
		return true;
	}

	bool parseNumber(JSONValue& value) {
		const std::size_t start = this->position_;

		this->consumeCharacter('-');

		if (this->consumeCharacter('0')) {
			if (this->position_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->position_]))) {
				CONSOLE_ERROR("Invalid number with leading zero at position %zu", this->position_);
				return false;
			}
		}
		else if (this->skipDigits() == 0) {
			CONSOLE_ERROR("Invalid number at position %zu", this->position_);
			return false;
		}

		if (this->consumeCharacter('.') && this->skipDigits() == 0) {
			CONSOLE_ERROR("Missing fractional digits after '.' at position %zu", this->position_);
			return false;
		}

		if (this->consumeCharacter('e') || this->consumeCharacter('E')) {
			if (!this->consumeCharacter('+'))
				this->consumeCharacter('-');

			if (this->skipDigits() == 0) {
				CONSOLE_ERROR("Missing exponent digits after 'e' or 'E' at position %zu", this->position_);
				return false;
			}
		}

		const char* first = this->input_.data() + start;
		const char* last = this->input_.data() + this->position_;

		double number = 0.0;
		const auto [end, error] = std::from_chars(first, last, number);
		if (error != std::errc() || end != last) {
			CONSOLE_ERROR("Failed to parse number at position %zu", start);
			return false;
		}

		value = number;
		return true;
	}

	bool parseArray(JSONValue& value) {
		if (!this->expectCharacter('['))
			return false;

		JSONArray array;
		this->skipWhitespace();

		if (this->consumeCharacter(']')) {
			value = std::move(array);
			return true;
		}

		while (true) {
			JSONValue element;
			if (!this->parseValue(element))
				return false;

			array.push_back(std::move(element));
			this->skipWhitespace();

			if (this->consumeCharacter(']')) {
				value = std::move(array);
				return true;
			}

			if (!this->expectCharacter(','))
				return false;
		}
	}

	bool parseObject(JSONValue& value) {
		if (!this->expectCharacter('{'))
			return false;

		JSONObject object;
		this->skipWhitespace();

		if (this->consumeCharacter('}')) {
			value = std::move(object);
			return true;
		}

		while (true) {
			this->skipWhitespace();

			std::string key;
			if (!this->parseStringRaw(key))
				return false;

			this->skipWhitespace();
			if (!this->expectCharacter(':'))
				return false;

			JSONValue element;
			if (!this->parseValue(element))
				return false;

			object[key] = std::move(element);
			this->skipWhitespace();

			if (this->consumeCharacter('}')) {
				value = std::move(object);
				return true;
			}

			if (!this->expectCharacter(','))
				return false;
		}
	}

	bool parseValue(JSONValue& value) {
		this->skipWhitespace();

		if (this->position_ >= this->input_.size()) {
			CONSOLE_ERROR("Unexpected end of input at position %zu", this->position_);
			return false;
		}

		const char character = this->input_[this->position_];
		switch (character) {
		case 'n':
			if (this->matchText("null")) {
				value = nullptr;
				return true;
			}

			break;
		case 't':
			if (this->matchText("true")) {
				value = true;
				return true;
			}

			break;
		case 'f':
			if (this->matchText("false")) {
				value = false;
				return true;
			}

			break;
		case '"':
			return this->parseString(value);
		case '[':
			return this->parseArray(value);
		case '{':
			return this->parseObject(value);
		default:
			if (character == '-' || (character >= '0' && character <= '9'))
				return this->parseNumber(value);

			break;
		}

		CONSOLE_ERROR("Unexpected character '%c' at position %zu", character, this->position_);
		return false;
	}
public:
	explicit JSONParser(std::string_view input) : input_(input), position_(0) {}

	bool parse(JSONValue& value) {
		JSONValue result;
		if (!this->parseValue(result))
			return false;

		this->skipWhitespace();
		if (this->position_ != this->input_.size()) {
			CONSOLE_ERROR("Unexpected trailing characters at position %zu", this->position_);
			return false;
		}

		value = std::move(result);
		return true;
	}

	static bool parseJson(std::string_view input, JSONValue& value) {
		JSONParser parser(input);
		return parser.parse(value);
	}

	static bool parseJson(const char* data, std::size_t size, JSONValue& value) {
		std::string_view input(data, size);

		JSONParser parser(input);
		return parser.parse(value);
	}
};