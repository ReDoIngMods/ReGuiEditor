#pragma once
#include "JSONObject.hpp"

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>

class JSONWriter {
	bool pretty_ = false;
	int indent_ = 4;

	void newline(std::ostream& stream, int depth) const {
		if (!pretty_)
			return;

		stream << '\n';
		for (int indent = 0; indent < depth * this->indent_; ++indent)
			stream << ' ';
	}

	void writeString(std::ostream& stream, const std::string& value) const {
		static const char hex[] = "0123456789abcdef";

		stream << '"';

		for (uint8_t character : value) {
			switch (character) {
			case '"':  stream << "\\\""; break;
			case '\\': stream << "\\\\"; break;
			case '\b': stream << "\\b";  break;
			case '\f': stream << "\\f";  break;
			case '\n': stream << "\\n";  break;
			case '\r': stream << "\\r";  break;
			case '\t': stream << "\\t";  break;
			default:
				if (character < 0x20)
					stream << "\\u00" << hex[character >> 4] << hex[character & 0x0F];
				else
					stream << static_cast<char>(character);
			}
		}

		stream << '"';
	}

	void writeValue(std::ostream& stream, const JSONValue& value, int depth) const {
		switch (value.getType()) {
		case JSONType::Null:
			stream << "null";
			break;
		case JSONType::Boolean:
			stream << (value.get<bool>() ? "true" : "false");
			break;
		case JSONType::Number:
			stream << value.get<double>();
			break;
		case JSONType::String:
			this->writeString(stream, value.get<std::string>());
			break;
		case JSONType::Array: {
			const auto& array = value.get<JSONArray>();
			stream << '[';

			for (std::size_t index = 0; index < array.size(); ++index) {
				if (index > 0)
					stream << ',';

				this->newline(stream, depth + 1);
				this->writeValue(stream, array[index], depth + 1);
			}

			if (!array.empty())
				this->newline(stream, depth);

			stream << ']';
			break;
		}
		case JSONType::Object: {
			const auto& object = value.get<JSONObject>();
			stream << '{';

			std::size_t index = 0;
			for (const auto& entry : object) {
				if (index++ > 0)
					stream << ',';

				this->newline(stream, depth + 1);
				this->writeString(stream, entry.first);

				stream << (this->pretty_ ? ": " : ":");

				this->writeValue(stream, entry.second, depth + 1);
			}

			if (!object.empty())
				this->newline(stream, depth);

			stream << '}';
			break;
		}
		}
	}
public:
	explicit JSONWriter(bool pretty = false, int indent = 4) : pretty_(pretty), indent_(indent) {}

	bool isPretty() const { return this->pretty_; }
	int getIndent() const { return this->indent_; }

	std::string write(const JSONValue& value) const {
		std::ostringstream stream;
		this->writeValue(stream, value, 0);
		return stream.str();
	}

	static std::string stringify(const JSONValue& value, bool pretty = false, int indent = 4) {
		return JSONWriter(pretty, indent).write(value);
	}
};