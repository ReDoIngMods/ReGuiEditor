#pragma once

#include <string>

#include "json_value.h"

namespace regui::format {

// Serializes a document back to text. Formatting is normalized, not
// preserved byte-for-byte - only data, comments, and metadata round-trip.
std::string writeDocument(const JsonValue& root);

} // namespace regui::format
