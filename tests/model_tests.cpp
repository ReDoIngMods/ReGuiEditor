#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "model/addon.h"
#include "model/layout.h"
#include "model/undo_stack.h"

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

// Jumping (undoTo/redoTo) is how the History window's "Jump Here" button
// works - this pins down that every action it steps over lands on the
// *other* stack, in the right order, rather than just vanishing.
void testUndoStackJump() {
    UndoStack stack;
    std::vector<std::string> applied; // mirrors "current state" as a stack of applied action ids

    auto pushAction = [&](const std::string& id) {
        applied.push_back(id); // matches real call sites, which apply the change before push()ing it
        stack.push(
            [&applied, id]() {
                check(!applied.empty() && applied.back() == id, "undo '" + id + "' pops the expected top state");
                applied.pop_back();
            },
            [&applied, id]() { applied.push_back(id); }, "Action " + id);
    };

    for (const std::string& id : {"P1", "P2", "P3", "P4", "P5"}) {
        pushAction(id);
    }
    check((applied == std::vector<std::string>{"P1", "P2", "P3", "P4", "P5"}), "all 5 actions applied initially");

    // Jump from P5 back to right after P3 - undoes P5 then P4, in that order.
    bool changed = stack.undoTo(2);
    check(changed, "undoTo(2) reports a change");
    check((applied == std::vector<std::string>{"P1", "P2", "P3"}), "undoTo(2) leaves state at P3");
    check(stack.undoEntries().size() == 3, "3 entries remain on the undo stack");
    check(stack.redoEntries().size() == 2, "both skipped-over actions land on the redo stack");
    check(stack.redoEntries()[0].description == "Action P5", "P5 (undone first) sits at redoEntries()[0]");
    check(stack.redoEntries()[1].description == "Action P4", "P4 (undone last) is redoEntries()[1] == back()");

    // A plain undo() (unrelated to jumping) should behave the same way: append to the redo stack.
    stack.undo();
    check(stack.redoEntries().size() == 3, "plain undo() also appends to the redo stack");
    check(stack.redoEntries().back().description == "Action P3", "most recently undone action is redoEntries().back()");

    // Jump forward again, one step short of everything - the rest must stay on the redo stack, not disappear.
    changed = stack.redoTo(2);
    check(changed, "redoTo(2) reports a change");
    check((applied == std::vector<std::string>{"P1", "P2", "P3", "P4"}), "redoTo(2) reapplies P3 then P4");
    check(stack.redoEntries().size() == 1, "the one remaining redo entry (P5) is kept, not dropped");
    check(stack.redoEntries()[0].description == "Action P5", "remaining redo entry is P5");

    // Overshoot on purpose - redoTo() must clamp at the end instead of crashing.
    changed = stack.redoTo(5);
    check(changed, "redoTo() past the end still reports a change");
    check((applied == std::vector<std::string>{"P1", "P2", "P3", "P4", "P5"}), "redoTo() clamps and replays the rest");
    check(stack.redoEntries().empty(), "redo stack is empty once everything is replayed");

    // A genuinely new action after a jump should still clear the redo stack - jumping isn't special-cased there.
    stack.undoTo(2);
    check(stack.redoEntries().size() == 2, "sanity: 2 entries pending redo before the new action");
    pushAction("P6");
    check(stack.redoEntries().empty(), "a new push() still clears the redo stack, same as before jumping existed");
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
        testUndoStackJump();
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
