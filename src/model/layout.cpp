#include "layout.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "format/parser.h"
#include "format/writer.h"

namespace regui::model {

namespace {

format::JsonValue& getOrCreateChild(format::JsonValue& parent, const std::string& key) {
    if (auto* existing = parent.find(key)) {
        return *existing;
    }
    parent.set(key, format::JsonValue::makeObject());
    return *parent.find(key);
}

LayoutAxis readAxis(const format::JsonValue* obj) {
    LayoutAxis axis;
    if (!obj) {
        return axis;
    }
    if (const auto* pixel = obj->find("pixel")) {
        axis.pixel = pixel->numberValue;
    }
    if (const auto* scale = obj->find("scale")) {
        axis.scale = scale->numberValue;
    }
    return axis;
}

format::JsonValue writeAxis(const LayoutAxis& axis) {
    auto value = format::JsonValue::makeObject();
    value.set("pixel", format::JsonValue::makeNumber(axis.pixel));
    value.set("scale", format::JsonValue::makeNumber(axis.scale));
    return value;
}

const format::JsonValue* findLayout(const format::JsonValue& properties) {
    return properties.find("layout");
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open '" + path + "'");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not write '" + path + "'");
    }
    out << text;
}

} // namespace

Anchor WidgetNode::anchor() const {
    Anchor result;
    if (const auto* layout = findLayout(properties)) {
        if (const auto* anchorValue = layout->find("anchor")) {
            if (const auto* x = anchorValue->find("x")) {
                result.x = x->numberValue;
            }
            if (const auto* y = anchorValue->find("y")) {
                result.y = y->numberValue;
            }
        }
    }
    return result;
}

void WidgetNode::setAnchor(const Anchor& value) {
    auto& layout = getOrCreateChild(properties, "layout");
    auto anchorValue = format::JsonValue::makeObject();
    anchorValue.set("x", format::JsonValue::makeNumber(value.x));
    anchorValue.set("y", format::JsonValue::makeNumber(value.y));
    layout.set("anchor", std::move(anchorValue));
}

LayoutAxis WidgetNode::x() const { return readAxis(findLayout(properties) ? findLayout(properties)->find("x") : nullptr); }
void WidgetNode::setX(const LayoutAxis& value) { getOrCreateChild(properties, "layout").set("x", writeAxis(value)); }

LayoutAxis WidgetNode::y() const { return readAxis(findLayout(properties) ? findLayout(properties)->find("y") : nullptr); }
void WidgetNode::setY(const LayoutAxis& value) { getOrCreateChild(properties, "layout").set("y", writeAxis(value)); }

LayoutAxis WidgetNode::width() const {
    return readAxis(findLayout(properties) ? findLayout(properties)->find("width") : nullptr);
}
void WidgetNode::setWidth(const LayoutAxis& value) { getOrCreateChild(properties, "layout").set("width", writeAxis(value)); }

LayoutAxis WidgetNode::height() const {
    return readAxis(findLayout(properties) ? findLayout(properties)->find("height") : nullptr);
}
void WidgetNode::setHeight(const LayoutAxis& value) {
    getOrCreateChild(properties, "layout").set("height", writeAxis(value));
}

std::string WidgetNode::skin() const {
    if (const auto* value = properties.find("skin")) {
        return value->stringValue;
    }
    return {};
}

void WidgetNode::setSkin(std::string value) { properties.set("skin", format::JsonValue::makeString(std::move(value))); }

double WidgetNode::alpha() const {
    if (const auto* value = properties.find("alpha")) {
        return value->numberValue;
    }
    return 1.0;
}

void WidgetNode::setAlpha(double value) { properties.set("alpha", format::JsonValue::makeNumber(value)); }

const format::JsonValue* WidgetNode::callbacksJson() const { return properties.find("callbacks"); }

void WidgetNode::setCallback(const std::string& event, std::string handlerName) {
    auto& callbacks = getOrCreateChild(properties, "callbacks");
    callbacks.set(event, format::JsonValue::makeString(std::move(handlerName)));
}

Rect WidgetNode::resolveRect(const Rect& parent) const {
    LayoutAxis w = width();
    LayoutAxis h = height();
    LayoutAxis xAxis = x();
    LayoutAxis yAxis = y();
    Anchor a = anchor();

    Rect result;
    result.width = parent.width * w.scale + w.pixel;
    result.height = parent.height * h.scale + h.pixel;
    double anchorAbsX = parent.x + parent.width * xAxis.scale + xAxis.pixel;
    double anchorAbsY = parent.y + parent.height * yAxis.scale + yAxis.pixel;
    result.x = anchorAbsX - a.x * result.width;
    result.y = anchorAbsY - a.y * result.height;
    return result;
}

WidgetNode WidgetNode::fromJson(const format::JsonValue& widgetObject) {
    WidgetNode node;
    if (const auto* n = widgetObject.find("name")) {
        node.name = n->stringValue;
    }
    if (const auto* t = widgetObject.find("type")) {
        node.type = t->stringValue;
    }
    if (const auto* p = widgetObject.find("properties")) {
        node.properties = *p;
    }
    if (const auto* c = widgetObject.find("children")) {
        for (const auto& child : c->arrayItems) {
            node.children.push_back(fromJson(child));
        }
    }
    node.leadingComments = widgetObject.leadingComments;
    node.trailingComments = widgetObject.trailingComments;
    return node;
}

format::JsonValue WidgetNode::toJson() const {
    auto obj = format::JsonValue::makeObject();
    obj.set("name", format::JsonValue::makeString(name));
    obj.set("type", format::JsonValue::makeString(type));
    obj.set("properties", properties);

    auto childrenArray = format::JsonValue::makeArray();
    for (const auto& child : children) {
        childrenArray.arrayItems.push_back(child.toJson());
    }
    obj.set("children", std::move(childrenArray));

    obj.leadingComments = leadingComments;
    obj.trailingComments = trailingComments;
    return obj;
}

ReLayoutDocument ReLayoutDocument::parse(const std::string& text) {
    ReLayoutDocument doc;
    doc.mRoot = format::parseDocument(text);

    if (const auto* v = doc.mRoot.find("version")) {
        doc.version = static_cast<int>(v->numberValue);
    }
    if (const auto* m = doc.mRoot.find("metadata")) {
        if (const auto* n = m->find("name")) {
            doc.name = n->stringValue;
        }
        if (const auto* a = m->find("author")) {
            doc.author = a->stringValue;
        }
    }
    if (const auto* data = doc.mRoot.find("data")) {
        for (const auto& widget : data->arrayItems) {
            doc.roots.push_back(WidgetNode::fromJson(widget));
        }
    }
    return doc;
}

ReLayoutDocument ReLayoutDocument::load(const std::string& path) { return parse(readFile(path)); }

std::string ReLayoutDocument::serialize() const {
    format::JsonValue root = mRoot;
    root.set("version", format::JsonValue::makeNumber(version));

    format::JsonValue metadata = format::JsonValue::makeObject();
    if (const auto* existing = root.find("metadata")) {
        metadata = *existing;
    }
    metadata.set("name", format::JsonValue::makeString(name));
    metadata.set("author", format::JsonValue::makeString(author));
    root.set("metadata", std::move(metadata));

    auto data = format::JsonValue::makeArray();
    for (const auto& widget : roots) {
        data.arrayItems.push_back(widget.toJson());
    }
    root.set("data", std::move(data));

    return format::writeDocument(root);
}

void ReLayoutDocument::save(const std::string& path) const { writeFile(path, serialize()); }

namespace {

// Root-relative child-index path to `target`, or false if not found.
bool findPath(std::vector<WidgetNode>& nodes, WidgetNode* target, std::vector<size_t>& outPath) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (&nodes[i] == target) {
            outPath = {i};
            return true;
        }
        std::vector<size_t> childPath;
        if (findPath(nodes[i].children, target, childPath)) {
            outPath = {i};
            outPath.insert(outPath.end(), childPath.begin(), childPath.end());
            return true;
        }
    }
    return false;
}

WidgetNode* resolvePath(std::vector<WidgetNode>& roots, const std::vector<size_t>& path) {
    if (path.empty() || path[0] >= roots.size()) {
        return nullptr;
    }
    WidgetNode* node = &roots[path[0]];
    for (size_t depth = 1; depth < path.size(); ++depth) {
        if (path[depth] >= node->children.size()) {
            return nullptr;
        }
        node = &node->children[path[depth]];
    }
    return node;
}

} // namespace

bool moveWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* nodeToMove, WidgetNode* newParent, WidgetNode** outMoved) {
    std::vector<size_t> movePath;
    if (!nodeToMove || !findPath(roots, nodeToMove, movePath)) {
        return false;
    }

    std::vector<size_t> parentPath; // empty means root level
    if (newParent) {
        if (newParent == nodeToMove || !findPath(roots, newParent, parentPath)) {
            return false;
        }
        // Reject dropping onto itself or a descendant.
        if (parentPath.size() > movePath.size() &&
            std::equal(movePath.begin(), movePath.end(), parentPath.begin())) {
            return false;
        }
    }

    std::vector<WidgetNode>* sourceContainer =
        movePath.size() == 1
            ? &roots
            : &resolvePath(roots, std::vector<size_t>(movePath.begin(), movePath.end() - 1))->children;
    size_t sourceIndex = movePath.back();

    WidgetNode extracted = std::move((*sourceContainer)[sourceIndex]);
    sourceContainer->erase(sourceContainer->begin() + sourceIndex);

    // A later sibling's index just shifted down by one from the erase above.
    if (!parentPath.empty() && parentPath.size() == movePath.size() &&
        std::equal(parentPath.begin(), parentPath.end() - 1, movePath.begin(), movePath.end() - 1) &&
        parentPath.back() > sourceIndex) {
        parentPath.back() -= 1;
    }

    std::vector<WidgetNode>* destContainer =
        parentPath.empty() ? &roots : &resolvePath(roots, parentPath)->children;
    destContainer->push_back(std::move(extracted));
    if (outMoved) {
        *outMoved = &destContainer->back();
    }
    return true;
}

WidgetNode* findParentOf(std::vector<WidgetNode>& roots, WidgetNode* node) {
    std::vector<size_t> path;
    if (!findPath(roots, node, path) || path.size() == 1) {
        return nullptr;
    }
    return resolvePath(roots, std::vector<size_t>(path.begin(), path.end() - 1));
}

bool removeWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* node, WidgetNode* outRemoved) {
    std::vector<size_t> path;
    if (!node || !findPath(roots, node, path)) {
        return false;
    }
    std::vector<WidgetNode>* container =
        path.size() == 1 ? &roots : &resolvePath(roots, std::vector<size_t>(path.begin(), path.end() - 1))->children;
    size_t index = path.back();
    if (outRemoved) {
        *outRemoved = std::move((*container)[index]);
    }
    container->erase(container->begin() + index);
    return true;
}

WidgetNode* insertWidgetNode(std::vector<WidgetNode>& roots, WidgetNode* parent, size_t index, WidgetNode node) {
    std::vector<WidgetNode>& container = parent ? parent->children : roots;
    index = std::min(index, container.size());
    auto it = container.insert(container.begin() + index, std::move(node));
    return &*it;
}

} // namespace regui::model
