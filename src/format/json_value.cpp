#include "json_value.h"

namespace regui::format {

JsonValue JsonValue::makeBool(bool value) {
    JsonValue v;
    v.type = JsonType::Bool;
    v.boolValue = value;
    return v;
}

JsonValue JsonValue::makeNumber(double value) {
    JsonValue v;
    v.type = JsonType::Number;
    v.numberValue = value;
    return v;
}

JsonValue JsonValue::makeString(std::string value) {
    JsonValue v;
    v.type = JsonType::String;
    v.stringValue = std::move(value);
    return v;
}

JsonValue JsonValue::makeArray() {
    JsonValue v;
    v.type = JsonType::Array;
    return v;
}

JsonValue JsonValue::makeObject() {
    JsonValue v;
    v.type = JsonType::Object;
    return v;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type != JsonType::Object) {
        return nullptr;
    }
    for (const auto& [k, v] : objectFields) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

JsonValue* JsonValue::find(const std::string& key) {
    return const_cast<JsonValue*>(static_cast<const JsonValue*>(this)->find(key));
}

void JsonValue::set(const std::string& key, JsonValue value) {
    if (type != JsonType::Object) {
        type = JsonType::Object;
        objectFields.clear();
    }
    for (auto& [k, v] : objectFields) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    objectFields.emplace_back(key, std::move(value));
}

const std::string* JsonValue::getString(const std::string& key) const {
    const JsonValue* v = find(key);
    return (v && v->isString()) ? &v->stringValue : nullptr;
}

bool JsonValue::operator==(const JsonValue& other) const {
    if (type != other.type || leadingComments != other.leadingComments ||
        trailingComments != other.trailingComments) {
        return false;
    }
    switch (type) {
        case JsonType::Null:
            return true;
        case JsonType::Bool:
            return boolValue == other.boolValue;
        case JsonType::Number:
            return numberValue == other.numberValue;
        case JsonType::String:
            return stringValue == other.stringValue;
        case JsonType::Array:
            return arrayItems == other.arrayItems;
        case JsonType::Object:
            return objectFields == other.objectFields;
    }
    return false;
}

} // namespace regui::format
