#pragma once

#include <string>
#include <vector>

#include "format/json_value.h"

namespace regui::model {

struct LayoutAxis {
    double pixel = 0;
    double scale = 0;
};

struct Anchor {
    double x = 0;
    double y = 0;
};

struct Rect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

// A widget instance in a ReLayout tree. Wraps a generic `properties` bag so
// unrecognized properties round-trip untouched, plus typed accessors.
class WidgetNode {
public:
    std::string name;
    std::string type;
    format::JsonValue properties = format::JsonValue::makeObject();
    std::vector<WidgetNode> children;

    std::vector<format::Comment> leadingComments;
    std::vector<format::Comment> trailingComments;

    Anchor anchor() const;
    void setAnchor(const Anchor& value);

    LayoutAxis x() const;
    void setX(const LayoutAxis& value);
    LayoutAxis y() const;
    void setY(const LayoutAxis& value);
    LayoutAxis width() const;
    void setWidth(const LayoutAxis& value);
    LayoutAxis height() const;
    void setHeight(const LayoutAxis& value);

    std::string skin() const;
    void setSkin(std::string value);

    // event name -> handler function name; nullptr if none set.
    const format::JsonValue* callbacksJson() const;
    void setCallback(const std::string& event, std::string handlerName);

    // Resolves this widget's absolute pixel rect from its parent's, Roblox
    // UDim2/AnchorPoint-style: size = parentSize * scale + pixel.
    Rect resolveRect(const Rect& parent) const;

    static WidgetNode fromJson(const format::JsonValue& widgetObject);
    format::JsonValue toJson() const;
};

// A parsed .relayout document: metadata plus a forest of WidgetNode roots.
class ReLayoutDocument {
public:
    static ReLayoutDocument parse(const std::string& text);
    static ReLayoutDocument load(const std::string& path);

    std::string serialize() const;
    void save(const std::string& path) const;

    int version = 3;
    std::string name;
    std::string author;
    std::vector<WidgetNode> roots;

private:
    format::JsonValue mRoot = format::JsonValue::makeObject();
};

// Moves `nodeToMove` to become the last child of `newParent` (nullptr =
// root). Returns false if not found, or if the move would nest a node under
// its own descendant. Resolves both nodes to index paths first since
// erasing from a vector invalidates sibling pointers, `newParent` included.
bool moveWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* nodeToMove, WidgetNode* newParent, WidgetNode** outMoved);

// nullptr if `node` is a root or isn't found.
WidgetNode* findParentOf(std::vector<WidgetNode>& roots, WidgetNode* node);

// Removes `node` from its parent (or `roots`). If `outRemoved` is non-null,
// the detached subtree is moved into it - the only way to get it back.
bool removeWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* node, WidgetNode* outRemoved);

// Inserts `node` as a child of `parent` (nullptr = root) at `index`
// (clamped), e.g. to restore on undo or drop a pasted copy. Returns the
// inserted node's stable pointer, valid until the container's next mutation.
WidgetNode* insertWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* parent, size_t index, WidgetNode node);

} // namespace regui::model
