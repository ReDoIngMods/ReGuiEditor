#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "model/addon.h"
#include "model/layout.h"

using namespace regui::model;

namespace {

int gFailures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++gFailures;
    }
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

void testLoadRelayout(const std::string& examplesDir) {
    ReLayoutDocument doc = ReLayoutDocument::load(examplesDir + "/example.relayout");
    check(doc.version == 3, "document version");
    check(doc.name == "MyReLayout", "document metadata.name");
    check(doc.roots.size() == 1, "one root widget");
    if (doc.roots.empty()) {
        return;
    }

    const WidgetNode& widget = doc.roots[0];
    check(widget.name == "MyWidget", "widget name");
    check(widget.type == "ReGui.Widget", "widget type");
    check(widget.skin() == "<skin name>", "widget skin");

    Anchor a = widget.anchor();
    check(near(a.x, 0.5) && near(a.y, 0.5), "widget anchor (0.5, 0.5)");

    LayoutAxis w = widget.width();
    check(near(w.pixel, 0) && near(w.scale, 0.5), "widget width {0px, 0.5}");

    const regui::format::JsonValue* callbacks = widget.callbacksJson();
    check(callbacks != nullptr && callbacks->find("onClick") != nullptr &&
              callbacks->find("onClick")->stringValue == "cl_onWidgetClick",
          "widget callbacks.onClick");
}

void testResolveRect() {
    WidgetNode widget;
    widget.setAnchor({0.5, 0.5});
    widget.setX({0, 0});
    widget.setY({0, 0});
    widget.setWidth({0, 0.5});
    widget.setHeight({0, 0.5});

    Rect parent{0, 0, 1000, 800};
    Rect resolved = widget.resolveRect(parent);

    check(near(resolved.width, 500) && near(resolved.height, 400), "resolved size scales from parent");
    check(near(resolved.x, -250) && near(resolved.y, -200), "resolved position accounts for anchor fraction");
}

void testEditAndRoundTrip(const std::string& examplesDir, const std::string& scratchDir) {
    ReLayoutDocument doc = ReLayoutDocument::load(examplesDir + "/example.relayout");
    WidgetNode& widget = doc.roots[0];
    widget.setSkin("NewSkin");
    widget.setAnchor({0.0, 1.0});

    std::string path = scratchDir + "/edited.relayout";
    doc.save(path);

    ReLayoutDocument reloaded = ReLayoutDocument::load(path);
    check(reloaded.roots.size() == 1, "reloaded document still has one widget");
    if (reloaded.roots.empty()) {
        return;
    }
    const WidgetNode& reloadedWidget = reloaded.roots[0];
    check(reloadedWidget.skin() == "NewSkin", "edited skin persisted");
    Anchor a = reloadedWidget.anchor();
    check(near(a.x, 0.0) && near(a.y, 1.0), "edited anchor persisted");
    // Untouched data should survive the edit/save/reload round trip.
    check(reloadedWidget.name == "MyWidget", "unedited widget name survives edit round trip");
    const regui::format::JsonValue* callbacks = reloadedWidget.callbacksJson();
    check(callbacks != nullptr && callbacks->find("onHover") != nullptr &&
              callbacks->find("onHover")->stringValue == "cl_onWidgetHover",
          "untouched callbacks survive edit round trip");
}

void testAddonRegistry(const std::string& examplesDir) {
    ReAddonRegistry registry;
    registry.loadFile(examplesDir + "/example.readdon");

    const WidgetTypeDef* def = registry.find("MyCustomWidget");
    check(def != nullptr, "registry finds MyCustomWidget");
    if (!def) {
        return;
    }

    auto properties = def->section("available-properties");
    check(properties.size() == 2, "available-properties has 2 entries");
    if (properties.size() == 2) {
        check(properties[0].name == "MyProperty", "first property name preserves declaration order");
        check(properties[0].value.stringValue == "<default value>", "first property default value");
        check(properties[0].metadata.has_value() && properties[0].metadata->get("label") != nullptr,
              "first property carries metadata");
    }

    auto callbacks = def->stringArraySection("available-callbacks");
    check((callbacks == std::vector<std::string>{"onSomething", "onSomething2"}), "available-callbacks contents");

    check(registry.find("NoSuchWidget") == nullptr, "unknown type name returns nullptr");
}

// moveWidgetNode has to resolve nodes to index-paths rather than trusting
// raw pointers across the mutation, since erasing a sibling shifts every
// later element in that same vector to a new address. This specifically
// exercises that shift-adjustment path: A is root index 0, C is root
// index 2 (after A), so erasing A must correctly retarget "become a child
// of C" at C's new index (1), not its original one.
void testMoveWidgetNode() {
    std::vector<WidgetNode> roots(3);
    roots[0].name = "A";
    roots[1].name = "B";
    roots[1].children.resize(2);
    roots[1].children[0].name = "B1";
    roots[1].children[1].name = "B2";
    roots[2].name = "C";

    WidgetNode* a = &roots[0];
    WidgetNode* b = &roots[1];
    WidgetNode* c = &roots[2];
    WidgetNode* b1 = &roots[1].children[0];
    WidgetNode* b2 = &roots[1].children[1];

    // Reject: dropping a node onto itself.
    WidgetNode* moved = nullptr;
    check(!moveWidgetNode(roots, c, c, &moved), "moving a node onto itself is rejected");
    check(roots.size() == 3, "rejected self-drop leaves the tree unchanged");

    // Reject: dropping a node onto its own descendant.
    check(!moveWidgetNode(roots, b, b1, &moved), "moving a node onto its own descendant is rejected");
    check(b->children.size() == 2, "rejected cycle leaves the tree unchanged");

    // Move a root (A, index 0) to become a child of a later root (C, index
    // 2) - the shift-adjustment case described above.
    check(moveWidgetNode(roots, a, c, &moved), "move root A under root C");
    check(roots.size() == 2, "A is no longer a root");
    check(roots[0].name == "B" && roots[1].name == "C", "B and C shift down after A is removed");
    check(roots[1].children.size() == 1 && roots[1].children[0].name == "A", "C gained A as a child");
    check(moved != nullptr && moved->name == "A", "moveWidgetNode returns the node's new location");

    // Move a nested child (B2) to root level.
    check(moveWidgetNode(roots, b2, nullptr, &moved), "move B2 to root level");
    check(roots[0].children.size() == 1 && roots[0].children[0].name == "B1", "B keeps only B1");
    check(roots.size() == 3 && roots.back().name == "B2", "B2 appended as a new root");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: model_tests <examples-dir> [scratch-dir]\n";
        return 2;
    }
    std::string examplesDir = argv[1];
    std::string scratchDir = argc > 2 ? argv[2] : ".";

    try {
        testLoadRelayout(examplesDir);
        testResolveRect();
        testEditAndRoundTrip(examplesDir, scratchDir);
        testAddonRegistry(examplesDir);
        testMoveWidgetNode();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: unexpected exception: " << e.what() << "\n";
        ++gFailures;
    }

    if (gFailures == 0) {
        std::cout << "All model tests passed.\n";
        return 0;
    }
    std::cerr << gFailures << " test(s) failed.\n";
    return 1;
}
