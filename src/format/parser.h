#pragma once

#include <stdexcept>
#include <string>

#include "json_value.h"

namespace regui::format {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, size_t line, size_t column)
        : std::runtime_error(message + " (line " + std::to_string(line) + ", column " + std::to_string(column) + ")"),
          line(line),
          column(column) {}

    size_t line;
    size_t column;
};

// Parses a ReLayout/ReAddon document: JSON plus `//` and `/* */` comments.
// Throws ParseError on malformed input.
JsonValue parseDocument(const std::string& text);

} // namespace regui::format
