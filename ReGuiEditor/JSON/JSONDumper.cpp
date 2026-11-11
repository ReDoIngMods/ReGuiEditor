#include "JSON.hpp"

#include <cmath>
#include <iomanip>
#include <ios>
#include <iterator>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

struct JSONDumper {
	std::ostream& stream;
	bool prettyPrint;
	const char indentCharacter;
	int indentLevel;

	JSONDumper(std::ostream& stream, bool prettyPrint, const char indentCharacter) : stream(stream), prettyPrint(prettyPrint), indentCharacter(indentCharacter), indentLevel(0) {}

	void increaseIndent() { ++this->indentLevel; }
	void decreaseIndent() { --this->indentLevel; }

	void writeIndent() {
		if (!this->prettyPrint)
			return;

		for (int index = 0; index < this->indentLevel; ++index)
			this->stream << this->indentCharacter;
	}

	void writeNewLine() {
		if (this->prettyPrint)
			this->stream << '\n';
	}

	void dumpString(const std::string& value) {
		this->stream << '"';
		
		for (const char character : value) {
			switch (character) {
			case '"':  this->stream << "\\\""; break;
			case '\\': this->stream << "\\\\"; break;
			case '\b': this->stream <<  "\\b"; break;
			case '\f': this->stream <<  "\\f"; break;
			case '\n': this->stream <<  "\\n"; break;
			case '\r':
				this->stream << "\\r";
				break;
			case '\t':
				this->stream << "\\t";
				break;
			default:
				if (static_cast<unsigned char>(character) < 0x20)
					this->stream << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(character)) << std::dec;
				else
					this->stream << character;

				break;
			}
		}

		this->stream << '"';
	}

	void dumpValue(const JSONValue& value) {
		switch (value.getType()) {
		case JSONType::Null:
			this->stream << "null";
			return;
		case JSONType::Boolean:
			this->stream << (value.asBoolean() ? "true" : "false");
			return;
		case JSONType::Integer:
			this->stream << value.asInteger();
			return;
		case JSONType::FloatingPoint: {
			double doubleValue = value.asDouble();
			if (std::isnan(doubleValue) || std::isinf(doubleValue))
				this->stream << "null";
			else
				this->stream << doubleValue;

			return;
		}
		case JSONType::String:
			this->dumpString(value.asString());
			return;
		case JSONType::Object: {
			const JSONObject& object = value.asObject();

			this->stream << '{';
			this->writeNewLine();

			this->increaseIndent();

			for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
				if (iterator->second.isNull() || iterator->second.isNone())
					continue;

				this->writeIndent();
				this->stream << '"' << iterator->first << "\"" << (this->prettyPrint ? " : " : ":");

				this->dumpValue(iterator->second);

				auto nextIterator = std::next(iterator);
				while (nextIterator != object.end() && (nextIterator->second.isNull() || nextIterator->second.isNone()))
					++nextIterator;

				if (nextIterator != object.end())
					this->stream << ',';

				this->writeNewLine();
			}

			this->decreaseIndent();

			this->writeIndent();
			this->stream << '}';
			return;
		}
		case JSONType::Array: {
			const JSONArray& array = value.asArray();

			this->stream << '[';
			this->writeNewLine();

			this->increaseIndent();

			for (size_t index = 0; index < array.size(); ++index) {
				// Do not skip null or none values, otherwise the items would be shifted.

				this->writeIndent();
				this->dumpValue(array[index]);

				if (index + 1 < array.size())
					this->stream << ',';

				this->writeNewLine();
			}

			this->decreaseIndent();

			this->writeIndent();
			this->stream << ']';
			return;
		}
		default:
			throw std::runtime_error("Invalid JSON type");
		}
	}
};

void JSONValue::dump(std::string& str, bool prettyPrint, const char indentCharacter) const {
	std::ostringstream stream;
	this->dump(stream, prettyPrint, indentCharacter);

	str = stream.str();
}

void JSONValue::dump(std::ostream& stream, bool prettyPrint, const char indentCharacter) const {
	JSONDumper dumper(stream, prettyPrint, indentCharacter);
	dumper.dumpValue(*this);
}

std::string JSONValue::dump(bool prettyPrint, const char indentCharacter) const {
	std::ostringstream stream;
	this->dump(stream, prettyPrint, indentCharacter);

	return stream.str();
}