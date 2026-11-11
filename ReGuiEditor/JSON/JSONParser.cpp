#include "JSON.hpp"

#include <cstdint>
#include <cstdio>
#include <format>
#include <istream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

inline static uint8_t fromHexCharacter(const char character) {
	if (character >= '0' && character <= '9') return static_cast<uint8_t>(character - '0');
	else if (character >= 'a' && character <= 'f') return static_cast<uint8_t>(character - 'a' + 10);
	else if (character >= 'A' && character <= 'F') return static_cast<uint8_t>(character - 'A' + 10);

	return 0;
}

inline static const char toHexCharacter(const uint8_t value) {
	if (value < 10) return static_cast<char>('0' + value);
	if (value < 16) return static_cast<char>('A' + (value - 10));
	return '0';
}

inline static bool isHexCharacter(const char character) {
	return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
}

inline static void appendCodepoint(std::string& str, const uint32_t codepoint) {
	if (codepoint <= 0x7F) {
		str += static_cast<char>(codepoint);
	} else if (codepoint <= 0x7FF) {
		str += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	} else if (codepoint <= 0xFFFF) {
		str += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
		str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	} else if (codepoint <= 0x10FFFF) {
		str += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
		str += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
}

struct JSONParserContext {
	std::istream& stream;

	int line = 1;
	int column = 1;
	int position = 0;

	JSONParserResult parseResult = JSONParserResult::success();
	bool isError = false;

	JSONParserContext(std::istream& stream) noexcept : stream(stream) {}
	JSONParserContext(JSONParserContext&& other) noexcept 
		: stream(other.stream), line(other.line), column(other.column), position(other.position), parseResult(other.parseResult), isError(other.isError) {}

	void setErrorMessage(const std::string& message) {
		if (!this->isError) {
			this->parseResult = JSONParserResult::error(this->line, this->column, this->position, message);
		}
		this->isError = true;
	}

	template<typename... Args>
	void setErrorMessage(const std::string& format, Args&&... args) {
		this->setErrorMessage(std::vformat(format, std::make_format_args(args...)));
	}

	const char peekCharacter() {
		int character = this->stream.peek();
		if (character == EOF)
			return '\0';

		return static_cast<char>(character);
	}

	const char consumeCharacter() {
		int character = this->stream.get();
		if (character == EOF)
			return '\0';

		char charValue = static_cast<char>(character);
		if (charValue == '\n') {
			++this->line;
			this->column = 1;
		}
		else {
			++this->column;
		}

		++this->position;
		return charValue;
	}

	bool expectCharacter(char expectedCharacter) {
		char character = this->consumeCharacter();
		if (character != expectedCharacter) {
			this->setErrorMessage("Expected character '{}', but got '{}'", expectedCharacter, character == '\0' ? "EOF" : std::string(1, character));
			return false;
		}

		return true;
	}

	bool matchString(const std::string& expectedString) {
		for (const char expectedCharacter : expectedString) {
			if (this->consumeCharacter() != expectedCharacter)
				return false;
		}

		return true;
	}

	bool expectString(const std::string& expectedString) {
		if (!this->matchString(expectedString)) {
			this->setErrorMessage("Expected string '{}'", expectedString);
			return false;
		}

		return true;
	}

	bool skipWhitespace() {
		while (true) {
			const char character = this->peekCharacter();
			if (character == '\0')
				break;

			if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
				this->consumeCharacter();
				continue;
			}

			if (character == '/') {
				this->consumeCharacter();
				const char nextCharacter = this->peekCharacter();

				if (nextCharacter == '/') {
					this->consumeCharacter();
					while (true) {
						const char character = this->peekCharacter();
						if (character == '\0' || character == '\n' || character == '\r')
							break;

						this->consumeCharacter();
					}

					continue;
				} else if (nextCharacter == '*') {
					this->consumeCharacter();

					bool closed = false;
					while (this->peekCharacter() != '\0') {
						const char character = this->consumeCharacter();
						if (character == '*') {
							if (this->peekCharacter() == '/') {
								this->consumeCharacter();

								closed = true;
								break;
							}
						}
					}

					if (!closed) {
						this->setErrorMessage("Unterminated multi-line comment");
						return false;
					}

					continue;
				} else {
					this->stream.unget();
					--this->position;
					--this->column;
					break;
				}
			}

			break;
		}

		return !this->isError;
	}

	bool parseNull(JSONValue& output) {
		if (!this->expectString("null"))
			return false;

		output = nullptr;
		return true;
	}

	bool parseBoolean(JSONValue& output) {
		const char character = this->peekCharacter();
		if (character == 't') {
			if (!this->matchString("true")) {
				this->setErrorMessage("Expected 'true', but got '{}'", character);
				return false;
			}

			output = true;
			return true;
		} else if (character == 'f') {
			if (!this->matchString("false")) {
				this->setErrorMessage("Expected 'false', but got '{}'", character);
				return false;
			}

			output = false;
			return true;
		}

		this->setErrorMessage("Expected 'true' or 'false', but got '{}'", character);
		return false;
	}

	bool parseStringInto(std::string& result) {
		if (!this->expectCharacter('"')) return false;

		result.clear();
		while (true) {
			const char character = this->consumeCharacter();
			if (character == '\0') {
				this->setErrorMessage("Unterminated string");
				return false;
			}

			if (character == '"')
				break;

			if (character == '\\') {
				const char escapeCharacter = this->consumeCharacter();
				if (escapeCharacter == '\0') {
					this->setErrorMessage("Unterminated escape sequence in string");
					return false;
				}

				switch (escapeCharacter) {
				case '"':  result += '"';  break;
				case '\\': result += '\\'; break;
				case '/':  result += '/';  break;
				case 'b':  result += '\b'; break;
				case 'f':  result += '\f'; break;
				case 'n':  result += '\n'; break;
				case 'r':  result += '\r'; break;
				case 't':  result += '\t'; break;
				case 'u': {
					std::string hexDigits;
					for (int index = 0; index < 4; ++index) {
						const char hexCharacter = this->consumeCharacter();
						if (!isHexCharacter(hexCharacter)) {
							this->setErrorMessage("Invalid Unicode escape sequence");
							return false;
						}

						hexDigits += hexCharacter;
					}

					uint32_t codePoint = 0;
					for (const char hexCharacter : hexDigits)
						codePoint = (codePoint << 4) | fromHexCharacter(hexCharacter);

					appendCodepoint(result, codePoint);
					break;
				}
				default:
					this->setErrorMessage("Invalid escape character '{}'", escapeCharacter);
					return false;
				}
			} else if (static_cast<unsigned char>(character) < 0x20) {
				this->setErrorMessage("Invalid control character in string");
				return false;
			} else {
				result += character;
			}
		}
		return true;
	}

	bool parseString(JSONValue& output) {
		std::string result;
		if (!this->parseStringInto(result))
			return false;

		output = std::move(result);
		return true;
	}

	bool parseNumber(JSONValue& output) {
		std::string numberStr;
		bool isFloating = false;

		while (true) {
			const char character = this->peekCharacter();

			if ((character >= '0' && character <= '9') || character == '-' || character == '+' || character == '.' || character == 'e' || character == 'E') {
				if (character == '.' || character == 'e' || character == 'E')
					isFloating = true;

				numberStr += character;
				this->consumeCharacter();

				continue;
			}

			break;
		}

		if (numberStr.empty()) {
			this->setErrorMessage("Expected number");
			return false;
		}

		try {
			size_t index = 0;
			if (isFloating) {
				double value = std::stod(numberStr, &index);
				if (index != numberStr.size()) {
					this->setErrorMessage("Invalid number format '{}'", numberStr);
					return false;
				}
				output = value;
			} else {
				long long value = std::stoll(numberStr, &index);
				if (index != numberStr.size()) {
					this->setErrorMessage("Invalid number format '{}'", numberStr);
					return false;
				}

				if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max())
					output = static_cast<int>(value);
				else
					output = static_cast<double>(value);
			}

			return true;
		} catch (...) {
			this->setErrorMessage("Invalid number format '{}'", numberStr);
			return false;
		}
	}

	bool parseArray(JSONValue& output) {
		if (!this->expectCharacter('[')) return false;
		if (!this->skipWhitespace()) return false;

		if (this->peekCharacter() == ']') {
			this->consumeCharacter();

			output = JSONArray{};
			return true;
		}

		JSONArray array;
		while (true) {
			if (!this->skipWhitespace())
				return false;

			JSONValue element;
			if (!this->parse(element)) return false;

			array.push_back(std::move(element));
			if (!this->skipWhitespace()) return false;

			const char character = this->peekCharacter();
			if (character == ']') {
				this->consumeCharacter();
				break;
			} else if (character == ',') {
				this->consumeCharacter();
			} else {
				this->setErrorMessage("Expected ',' or ']' in array, got '{}'", character == '\0' ? "EOF" : std::string(1, character));
				return false;
			}
		}

		output = std::move(array);
		return true;
	}

	bool parseObject(JSONValue& output) {
		if (!this->expectCharacter('{')) return false;
		if (!this->skipWhitespace()) return false;

		if (this->peekCharacter() == '}') {
			this->consumeCharacter();

			output = JSONObject{};
			return true;
		}

		JSONObject object;
		while (true) {
			if (!this->skipWhitespace())
				return false;

			if (this->peekCharacter() != '"') {
				const char ch = this->peekCharacter();
				this->setErrorMessage("Expected string key in object, got '{}'", ch == '\0' ? "EOF" : std::string(1, ch));
				return false;
			}

			std::string key;
			if (!this->parseStringInto(key)) return false;

			if (!this->skipWhitespace()) return false;
			if (!this->expectCharacter(':')) return false;
			if (!this->skipWhitespace()) return false;

			JSONValue value;
			if (!this->parse(value))
				return false;

			object.insert(key, std::move(value));
			if (!this->skipWhitespace())
				return false;

			const char character = this->peekCharacter();
			if (character == '}') {
				this->consumeCharacter();
				break;
			} else if (character == ',') {
				this->consumeCharacter();
			} else {
				this->setErrorMessage("Expected ',' or '}' in object, got '{}'", character == '\0' ? "EOF" : std::string(1, character));
				return false;
			}
		}

		output = std::move(object);
		return true;
	}

	bool parse(JSONValue& output) {
		if (!this->skipWhitespace())
			return false;

		const char character = this->peekCharacter();
		if (character == 'n') return this->parseNull(output);
		if (character == 't') return this->parseBoolean(output);
		if (character == 'f') return this->parseBoolean(output);
		if (character == '"') return this->parseString(output);
		if (character == '[') return this->parseArray(output);
		if (character == '{') return this->parseObject(output);
		if ((character >= '0' && character <= '9') || character == '-') return this->parseNumber(output);

		this->setErrorMessage("Unexpected character '{}' while parsing JSON", character == '\0' ? "EOF" : std::string(1, character));
		return false;
	}
};

JSONParserResult JSONValue::parse(std::istream& stream, JSONValue& output) {
	JSONParserContext parser(stream);
	if (!parser.parse(output))
		return parser.parseResult;

	return JSONParserResult::success();
}

JSONParserResult JSONValue::parse(const std::string& jsonStr, JSONValue& output) {
	std::stringstream stream(jsonStr);
	return JSONValue::parse(stream, output);
}