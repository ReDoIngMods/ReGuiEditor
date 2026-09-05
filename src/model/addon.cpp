#include "addon.h"

#include <algorithm>
#include <array>
#include <cctype>
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

// Directory portion of `path` (both slash styles), or "" if it's bare (i.e. relative to cwd).
std::string directoryOf(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : path.substr(0, pos);
}

// Whether `path` names a media file "icon" is allowed to point at, by extension.
bool hasIconFileExtension(const std::string& path) {
    static constexpr std::array<const char*, 4> kExtensions = {".png", ".jpg", ".jpeg", ".ico"};
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    for (const char* ext : kExtensions) {
        size_t extLen = std::char_traits<char>::length(ext);
        if (lower.size() >= extLen && lower.compare(lower.size() - extLen, extLen, ext) == 0) {
            return true;
        }
    }
    return false;
}

std::string base64Encode(const std::string& data) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        auto b0 = static_cast<unsigned char>(data[i]);
        auto b1 = static_cast<unsigned char>(data[i + 1]);
        auto b2 = static_cast<unsigned char>(data[i + 2]);
        out += kAlphabet[b0 >> 2];
        out += kAlphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += kAlphabet[((b1 & 0x0F) << 2) | (b2 >> 6)];
        out += kAlphabet[b2 & 0x3F];
    }
    size_t remaining = data.size() - i;
    if (remaining == 1) {
        auto b0 = static_cast<unsigned char>(data[i]);
        out += kAlphabet[b0 >> 2];
        out += kAlphabet[(b0 & 0x03) << 4];
        out += "==";
    } else if (remaining == 2) {
        auto b0 = static_cast<unsigned char>(data[i]);
        auto b1 = static_cast<unsigned char>(data[i + 1]);
        out += kAlphabet[b0 >> 2];
        out += kAlphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += kAlphabet[(b1 & 0x0F) << 2];
        out += "=";
    }
    return out;
}

// Reads `relativePath` (resolved against `baseDir`) and base64-encodes it, or ""
// if it can't be read - callers treat that the same as "no icon set".
std::string readIconFileAsBase64(const std::string& baseDir, const std::string& relativePath) {
    std::string fullPath = baseDir.empty() ? relativePath : baseDir + "/" + relativePath;
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return base64Encode(buffer.str());
}

} // namespace

std::vector<NamedEntry> WidgetTypeDef::section(const std::string& key) const {
    std::vector<NamedEntry> result;
    const format::JsonValue* section = mRaw.find(key);
    if (!section || !section->isObject()) {
        return result;
    }
    for (const auto& [name, entryValue] : section->objectFields) {
        NamedEntry entry;
        entry.name = name;
        if (entryValue.isObject()) {
            const format::JsonValue* defaultValue = entryValue.find("default");
            entry.value = defaultValue ? *defaultValue : format::JsonValue::makeNull();
            entry.metadata = entryValue;
        } else {
            entry.value = entryValue;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

std::string WidgetTypeDef::icon() const {
    const std::string* value = mRaw.getString("icon");
    if (!value) {
        return std::string();
    }
    if (hasIconFileExtension(*value)) {
        return readIconFileAsBase64(mBaseDir, *value);
    }
    return *value;
}

std::string WidgetTypeDef::label() const {
    const std::string* value = mRaw.getString("label");
    return value ? *value : mName;
}

std::string WidgetTypeDef::description() const {
    const std::string* value = mRaw.getString("description");
    return value ? *value : std::string();
}

void ReAddonRegistry::loadFile(const std::string& path) { loadText(readFile(path), directoryOf(path)); }

void ReAddonRegistry::loadText(const std::string& text, const std::string& baseDir) {
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
            *it = WidgetTypeDef(name, widget, baseDir);
        } else {
            mTypes.emplace_back(name, widget, baseDir);
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
