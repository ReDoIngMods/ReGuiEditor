#include "addon.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "format/parser.h"

namespace regui::model {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

std::vector<NamedEntry> WidgetTypeDef::section(const std::string& key) const {
    std::vector<NamedEntry> result;
    const format::JsonValue* section = mRaw.find(key);
    if (!section || !section->isObject()) {
        return result;
    }
    for (const auto& [name, value] : section->objectFields) {
        result.push_back(NamedEntry{name, value, value.metadata()});
    }
    return result;
}

std::vector<std::string> WidgetTypeDef::stringArraySection(const std::string& key) const {
    std::vector<std::string> result;
    const format::JsonValue* section = mRaw.find(key);
    if (!section || !section->isArray()) {
        return result;
    }
    for (const auto& item : section->arrayItems) {
        if (item.isString()) {
            result.push_back(item.stringValue);
        }
    }
    return result;
}

std::string WidgetTypeDef::icon() const {
    const format::JsonValue* value = mRaw.find("icon");
    return value && value->isString() ? value->stringValue : std::string();
}

void ReAddonRegistry::loadFile(const std::string& path) { loadText(readFile(path)); }

void ReAddonRegistry::loadText(const std::string& text) {
    format::JsonValue root = format::parseDocument(text);
    const format::JsonValue* widgets = root.find("widgets");
    if (!widgets || !widgets->isArray()) {
        return;
    }
    for (const auto& widget : widgets->arrayItems) {
        const auto* nameField = widget.find("name");
        if (!nameField) {
            continue;
        }
        std::string name = nameField->stringValue;
        auto it = std::find_if(mTypes.begin(), mTypes.end(), [&](const WidgetTypeDef& t) { return t.name() == name; });
        if (it != mTypes.end()) {
            *it = WidgetTypeDef(name, widget);
        } else {
            mTypes.emplace_back(name, widget);
        }
    }
}

const WidgetTypeDef* ReAddonRegistry::find(const std::string& typeName) const {
    for (const auto& t : mTypes) {
        if (t.name() == typeName) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace regui::model
