#pragma once
#include "JSONObject.hpp"

#include <ostream>
#include <string>

namespace ReGuiEditor {
	class JSONWriter {
		bool pretty_ = false;
		int indent_ = 4;

		void newline(std::ostream& stream, int depth) const;

		void writeString(std::ostream& stream, const std::string& value) const;
		void writeValue(std::ostream& stream, const JSONValue& value, int depth) const;
	public:
		explicit JSONWriter(bool pretty = false, int indent = 4) : pretty_(pretty), indent_(indent) {}

		bool isPretty() const { return this->pretty_; }
		int getIndent() const { return this->indent_; }

		std::string write(const JSONValue& value) const;

		static std::string stringify(const JSONValue& value, bool pretty = false, int indent = 4);
	};
}