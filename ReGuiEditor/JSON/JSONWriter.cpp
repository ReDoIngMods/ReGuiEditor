#include "JSONWriter.hpp"

#include "JSONObject.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

void JSONWriter::newline(std::ostringstream& stream, int depth) const {
	if (!this->prettyPrint_)
		return;

	stream << '\n';
	for (int indent = 0; indent < depth * this->indent_; ++indent)
		stream << ' ';
}

void JSONWriter::writeString(std::ostringstream& stream, const std::string& value) const {
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

void JSONWriter::writeValue(std::ostringstream& stream, const JSONValue& value, int depth) const {
	if (depth > 128) {
		stream << "null";
		return;
	}

	switch (value.getType()) {
	case JSONType::Null:
		stream << "null";
		break;
	case JSONType::Boolean:
		stream << (value.get<bool>() ? "true" : "false");
		break;
	case JSONType::Number: {
		double number = value.get<double>();
		if (std::isnan(number) || std::isinf(number))
			stream << "null";
		else
			stream << number;

		break;
	}
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

			stream << (this->prettyPrint_ ? ": " : ":");

			this->writeValue(stream, entry.second, depth + 1);
		}

		if (!object.empty())
			this->newline(stream, depth);

		stream << '}';
		break;
	}
	}
}

std::string JSONWriter::writeJson(const JSONValue& value, bool prettyPrint, int indent) {
	std::ostringstream stream;
	
	JSONWriter writer(prettyPrint, indent);
	writer.writeValue(stream, value, 0);

	return stream.str();
}