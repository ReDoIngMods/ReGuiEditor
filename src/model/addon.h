#pragma once

#include <optional>
#include <string>
#include <vector>

#include "format/json_value.h"

namespace regui::model {

struct NamedEntry {
    std::string name;
    format::JsonValue value;
    std::optional<format::Metadata> metadata;
};

// A single widget type definition from a .readdon file. Reads sections on
// demand rather than parsing into a fixed struct, since the schema is still
// evolving - a missing/renamed section just reads as empty.
class WidgetTypeDef {
public:
    WidgetTypeDef(std::string name, format::JsonValue raw) : mName(std::move(name)), mRaw(std::move(raw)) {}

    const std::string& name() const { return mName; }
    const format::JsonValue& raw() const { return mRaw; }

    // An object-valued section (e.g. "available-properties") as named entries, in order.
    std::vector<NamedEntry> section(const std::string& key) const;

    // An array-valued section of plain strings (e.g. "available-callbacks").
    std::vector<std::string> stringArraySection(const std::string& key) const;

    // Raw base64 image string from "icon" (nominally 128x128), or empty.
    std::string icon() const;

private:
    std::string mName;
    format::JsonValue mRaw;
};

// An index of widget types loaded from one or more .readdon files.
class ReAddonRegistry {
public:
    // A later file's widget name overrides an earlier one's.
    void loadFile(const std::string& path);
    void loadText(const std::string& text);

    const WidgetTypeDef* find(const std::string& typeName) const;
    const std::vector<WidgetTypeDef>& widgetTypes() const { return mTypes; }

private:
    std::vector<WidgetTypeDef> mTypes;
};

} // namespace regui::model
