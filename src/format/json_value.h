#pragma once

#include <string>
#include <utility>
#include <vector>

namespace regui::format {

// A single // or /* */ comment, without its delimiters.
struct Comment {
    std::string text;
    bool isLineComment = false;
    // Source line, used only during parsing to place the comment relative to values.
    size_t line = 0;

    bool operator==(const Comment& other) const {
        return text == other.text && isLineComment == other.isLineComment;
    }
};

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

// DOM node for the ReLayout/ReAddon JSON-with-comments dialect. Object
// fields stay in an ordered vector so writing doesn't reshuffle keys;
// comments are preserved verbatim for round-tripping.
struct JsonValue {
    JsonType type = JsonType::Null;

    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayItems;
    std::vector<std::pair<std::string, JsonValue>> objectFields;

    std::vector<Comment> leadingComments;
    std::vector<Comment> trailingComments;

    static JsonValue makeNull() { return JsonValue{}; }
    static JsonValue makeBool(bool value);
    static JsonValue makeNumber(double value);
    static JsonValue makeString(std::string value);
    static JsonValue makeArray();
    static JsonValue makeObject();

    bool isNull() const { return type == JsonType::Null; }
    bool isObject() const { return type == JsonType::Object; }
    bool isArray() const { return type == JsonType::Array; }
    bool isString() const { return type == JsonType::String; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isBool() const { return type == JsonType::Bool; }

    // nullptr if `type` isn't Object or the key is absent.
    const JsonValue* find(const std::string& key) const;
    JsonValue* find(const std::string& key);
    void set(const std::string& key, JsonValue value);

    // The field's string value if `key` is present and a string, else nullptr.
    const std::string* getString(const std::string& key) const;

    // Used by tests to verify parse->write->parse round-trips.
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }
};

} // namespace regui::format
