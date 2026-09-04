#include "writer.h"

#include <cmath>
#include <sstream>

namespace regui::format {

namespace {

constexpr int kIndentWidth = 4;

std::string indentStr(int level) {
    return std::string(static_cast<size_t>(level) * kIndentWidth, ' ');
}

void writeEscapedString(std::ostream& out, const std::string& s) {
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << c;
                break;
        }
    }
    out << '"';
}

void writeNumber(std::ostream& out, double v) {
    if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 1e15) {
        out << static_cast<long long>(v);
        return;
    }
    std::ostringstream oss;
    oss.precision(15);
    oss << v;
    out << oss.str();
}

void writeComment(std::ostream& out, const Comment& comment) {
    if (comment.isLineComment) {
        out << "//" << comment.text;
    } else {
        out << "/*" << comment.text << "*/";
    }
}

// Emitted after the value's terminator so a line comment can't swallow what follows.
void writeTrailingComments(std::ostream& out, const std::vector<Comment>& comments, int indent) {
    if (comments.empty()) {
        return;
    }
    bool hasLine = false;
    for (const auto& c : comments) {
        if (c.isLineComment) {
            hasLine = true;
            break;
        }
    }
    if (!hasLine) {
        for (const auto& c : comments) {
            out << ' ';
            writeComment(out, c);
        }
        return;
    }
    for (const auto& c : comments) {
        out << '\n' << indentStr(indent);
        writeComment(out, c);
    }
}

void writeLeadingComments(std::ostream& out, const std::vector<Comment>& comments, int indent) {
    for (const auto& c : comments) {
        out << indentStr(indent);
        writeComment(out, c);
        out << '\n';
    }
}

void writeValue(std::ostream& out, const JsonValue& value, int indent);

void writeObject(std::ostream& out, const JsonValue& value, int indent) {
    if (value.objectFields.empty()) {
        out << "{}";
        return;
    }
    out << "{\n";
    for (size_t i = 0; i < value.objectFields.size(); ++i) {
        const auto& [key, field] = value.objectFields[i];
        writeLeadingComments(out, field.leadingComments, indent + 1);
        out << indentStr(indent + 1);
        writeEscapedString(out, key);
        out << ": ";
        writeValue(out, field, indent + 1);
        bool isLast = (i + 1 == value.objectFields.size());
        if (!isLast) {
            out << ",";
        }
        writeTrailingComments(out, field.trailingComments, indent + 1);
        out << "\n";
    }
    out << indentStr(indent) << "}";
}

void writeArray(std::ostream& out, const JsonValue& value, int indent) {
    if (value.arrayItems.empty()) {
        out << "[]";
        return;
    }
    out << "[\n";
    for (size_t i = 0; i < value.arrayItems.size(); ++i) {
        const auto& item = value.arrayItems[i];
        writeLeadingComments(out, item.leadingComments, indent + 1);
        out << indentStr(indent + 1);
        writeValue(out, item, indent + 1);
        bool isLast = (i + 1 == value.arrayItems.size());
        if (!isLast) {
            out << ",";
        }
        writeTrailingComments(out, item.trailingComments, indent + 1);
        out << "\n";
    }
    out << indentStr(indent) << "]";
}

void writeValue(std::ostream& out, const JsonValue& value, int indent) {
    switch (value.type) {
        case JsonType::Null:
            out << "null";
            break;
        case JsonType::Bool:
            out << (value.boolValue ? "true" : "false");
            break;
        case JsonType::Number:
            writeNumber(out, value.numberValue);
            break;
        case JsonType::String:
            writeEscapedString(out, value.stringValue);
            break;
        case JsonType::Array:
            writeArray(out, value, indent);
            break;
        case JsonType::Object:
            writeObject(out, value, indent);
            break;
    }
}

} // namespace

std::string writeDocument(const JsonValue& root) {
    std::ostringstream out;
    writeLeadingComments(out, root.leadingComments, 0);
    writeValue(out, root, 0);
    writeTrailingComments(out, root.trailingComments, 0);
    out << "\n";
    return out.str();
}

} // namespace regui::format
