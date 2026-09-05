#pragma once

#include <optional>
#include <string>
#include <vector>

#include "format/json_value.h"

namespace regui::model {

struct NamedEntry {
    std::string name;
    format::JsonValue value;
    // The full definition object (e.g. `{"default": ..., "label": ..., "description": ...}`),
    // if the section entry was written that way rather than as a bare default value.
    std::optional<format::JsonValue> metadata;

    // A definition with a "label" but no "default" and no "properties" isn't an editable
    // property at all - it's a pure grouping header for other properties to nest under via "parent".
    bool isSection() const { return metadata.has_value() && !metadata->find("default") && !isGroup(); }

    // `"type": "group"`: a compound property with no "default" of its own, instead listing
    // anonymous sub-property definitions under "properties" - see WidgetTypeDef::section().
    bool isGroup() const {
        const format::JsonValue* properties = metadata ? metadata->find("properties") : nullptr;
        return properties && properties->isArray();
    }
};

// A single widget type definition from a .readdon file. Reads sections on
// demand rather than parsing into a fixed struct, since the schema is still
// evolving - a missing/renamed section just reads as empty.
class WidgetTypeDef {
public:
    // `baseDir` is the .readdon file's own directory, used to resolve an "icon"
    // written as a relative media file path rather than inline base64 data.
    WidgetTypeDef(std::string name, format::JsonValue raw, std::string baseDir = {})
        : mName(std::move(name)), mRaw(std::move(raw)), mBaseDir(std::move(baseDir)) {}

    const std::string& name() const { return mName; }
    const format::JsonValue& raw() const { return mRaw; }

    // User-facing name for pickers (e.g. the "New Widget" menu) - falls back to name() if unset.
    std::string label() const;
    // Hover-tooltip text for pickers, or empty if unset.
    std::string description() const;

    // An object-valued section (e.g. "available-properties") as named entries, in order.
    // Each entry's value may be a bare default value, or a definition object of the form
    // `{"default": ..., "label": ..., "description": ..., ...}` carrying extra metadata.
    // `"type": "group"` is a special case: no "default" of its own, instead a "properties"
    // array of anonymous sub-definitions - see NamedEntry::isGroup().
    std::vector<NamedEntry> section(const std::string& key) const;

    // Base64 image data (nominally 128x128) for "icon", or empty if unset/unreadable.
    // "icon" may be written either as inline base64 data, or as a relative path (from
    // this widget's .readdon file) to a .png/.jpg/.jpeg/.ico file - in the latter case
    // the file is read and base64-encoded here, so callers never need to tell the two apart.
    std::string icon() const;

private:
    std::string mName;
    format::JsonValue mRaw;
    std::string mBaseDir;
};

// An index of widget types loaded from one or more .readdon files.
class ReAddonRegistry {
public:
    // A later file's widget name overrides an earlier one's.
    void loadFile(const std::string& path);
    // `baseDir`, if given, resolves any "icon" written as a relative media file path.
    void loadText(const std::string& text, const std::string& baseDir = {});

    const WidgetTypeDef* find(const std::string& typeName) const;
    const std::vector<WidgetTypeDef>& widgetTypes() const { return mTypes; }

private:
    std::vector<WidgetTypeDef> mTypes;
};

} // namespace regui::model
