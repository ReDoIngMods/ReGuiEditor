#include "json_value.h"

#include <cctype>

namespace regui::format {

namespace {

std::string trim(const std::string& s) {
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }
    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(begin, end - begin);
}

bool consumePrefix(std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size() || s.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    s.erase(0, prefix.size());
    return true;
}

bool consumeSuffix(std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size() || s.compare(s.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }
    s.erase(s.size() - suffix.size());
    return true;
}

} // namespace

std::optional<Metadata> Metadata::tryParse(const std::string& commentText) {
    std::string body = trim(commentText);
    if (!consumePrefix(body, "metadata:")) {
        return std::nullopt;
    }
    body = trim(body);
    if (!consumeSuffix(body, "metadata;")) {
        return std::nullopt;
    }

    Metadata result;
    size_t start = 0;
    while (start <= body.size()) {
        size_t sep = body.find(';', start);
        std::string entry = trim(body.substr(start, sep == std::string::npos ? std::string::npos : sep - start));
        if (!entry.empty()) {
            size_t colon = entry.find(':');
            if (colon == std::string::npos) {
                return std::nullopt;
            }
            result.set(trim(entry.substr(0, colon)), trim(entry.substr(colon + 1)));
        }
        if (sep == std::string::npos) {
            break;
        }
        start = sep + 1;
    }
    return result;
}

std::string Metadata::format() const {
    std::string out = "metadata:";
    for (const auto& [key, value] : mEntries) {
        out += " " + key + ": " + value + ";";
    }
    out += " metadata;";
    return out;
}

const std::string* Metadata::get(const std::string& key) const {
    for (const auto& [k, v] : mEntries) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

void Metadata::set(const std::string& key, std::string value) {
    for (auto& [k, v] : mEntries) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    mEntries.emplace_back(key, std::move(value));
}

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

std::optional<Metadata> JsonValue::metadata() const {
    for (const auto& comment : trailingComments) {
        if (auto parsed = Metadata::tryParse(comment.text)) {
            return parsed;
        }
    }
    return std::nullopt;
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
