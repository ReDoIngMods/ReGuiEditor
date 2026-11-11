#pragma once
#include "JSONObject.hpp"

#include <sstream>
#include <string>

class JSONWriter {
	bool prettyPrint_ = false;
	int indent_ = 4;

	void newline(std::ostringstream& stream, int depth) const;
	void writeString(std::ostringstream& stream, const std::string& value) const;
	void writeValue(std::ostringstream& stream, const JSONValue& value, int depth) const;

	JSONWriter(bool prettyPrint, int indent) : prettyPrint_(prettyPrint), indent_(indent) {}
public:
	[[nodiscard]] static std::string writeJson(const JSONValue& value, bool prettyPrint = false, int indent = 4);
};