#include "ui/properties_panel.h"

#include "ui/skin_picker_dialog.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace regui::ui {

namespace {

enum class FieldKind { Integer, Float, Bool, Widget, String };

// Field kind from metadata's declared `type:`, falling back to the JSON value's own type.
FieldKind resolveFieldKind(const std::string& declaredTypeRaw, const format::JsonValue& value) {
    std::string t = declaredTypeRaw;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (t == "integer" || t == "int") {
        return FieldKind::Integer;
    }
    if (t == "float" || t == "number" || t == "double") {
        return FieldKind::Float;
    }
    if (t == "bool" || t == "boolean") {
        return FieldKind::Bool;
    }
    if (t == "widget") {
        return FieldKind::Widget;
    }
    if (t == "string") {
        return FieldKind::String;
    }

    if (value.isBool()) {
        return FieldKind::Bool;
    }
    if (value.isNumber()) {
        return FieldKind::Float;
    }
    return FieldKind::String;
}

std::string declaredParent(const model::NamedEntry& entry) {
    if (entry.metadata) {
        if (const std::string* p = entry.metadata->get("parent")) {
            return *p;
        }
    }
    return {};
}

// Orders properties depth-first by metadata's `parent:`, so related
// sub-properties nest under it. An unmatched parent still shows at top level.
std::vector<std::pair<const model::NamedEntry*, int>> orderPropertiesTree(
    const std::vector<model::NamedEntry>& properties) {
    std::vector<std::pair<const model::NamedEntry*, int>> ordered;
    std::vector<bool> visited(properties.size(), false);

    std::function<void(const std::string&, int)> appendChildrenOf = [&](const std::string& parentName, int depth) {
        for (size_t i = 0; i < properties.size(); ++i) {
            if (visited[i] || declaredParent(properties[i]) != parentName) {
                continue;
            }
            visited[i] = true;
            ordered.emplace_back(&properties[i], depth);
            appendChildrenOf(properties[i].name, depth + 1);
        }
    };
    appendChildrenOf(std::string(), 0);

    for (size_t i = 0; i < properties.size(); ++i) {
        if (!visited[i]) {
            ordered.emplace_back(&properties[i], 0);
        }
    }
    return ordered;
}

} // namespace

PropertiesPanel::PropertiesPanel(model::ReAddonRegistry* registry, model::UndoStack* undoStack)
    : mRegistry(registry), mUndoStack(undoStack) {
    mScrolled = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(mScrolled, TRUE);
    gtk_widget_set_vexpand(mScrolled, TRUE);
    gtk_widget_set_size_request(mScrolled, 280, -1); // floor, independent of the paned's divider position
    rebuildFields();
}

void PropertiesPanel::setNode(model::WidgetNode* node) {
    mNode = node;
    rebuildFields();
}

void PropertiesPanel::refresh() { rebuildFields(); }

void PropertiesPanel::updateLiveValues(model::WidgetNode* node) {
    if (!node || node != mNode || !mXPixelSpin) {
        return;
    }
    // Guards the same handlers rebuildFields() does - these are programmatic, not user, edits.
    mSuppressLive = true;
    model::Anchor anchor = mNode->anchor();
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mAnchorXSpin), anchor.x);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mAnchorYSpin), anchor.y);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mXPixelSpin), mNode->x().pixel);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mYPixelSpin), mNode->y().pixel);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mWidthPixelSpin), mNode->width().pixel);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mHeightPixelSpin), mNode->height().pixel);
    mSuppressLive = false;
}

void PropertiesPanel::rebuildFields() {
    mSuppressLive = true;

    // Repopulated by addBuiltInFields() if mNode is set, else left null so updateLiveValues() no-ops.
    mAnchorXSpin = mAnchorYSpin = nullptr;
    mXPixelSpin = mYPixelSpin = nullptr;
    mWidthPixelSpin = mHeightPixelSpin = nullptr;

    // gtk_scrolled_window_set_child() unparents the old child itself - don't do it again here.
    mGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(mGrid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(mGrid), 8);
    gtk_widget_set_margin_start(mGrid, 8);
    gtk_widget_set_margin_end(mGrid, 8);
    gtk_widget_set_margin_top(mGrid, 8);
    gtk_widget_set_margin_bottom(mGrid, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mScrolled), mGrid);

    if (!mNode) {
        GtkWidget* label = gtk_label_new("No widget selected");
        gtk_widget_add_css_class(label, "dim-label");
        gtk_grid_attach(GTK_GRID(mGrid), label, 0, 0, 2, 1);
        mSuppressLive = false;
        return;
    }

    int row = addBuiltInFields(GTK_GRID(mGrid), 0);

    const model::WidgetTypeDef* typeDef = mRegistry ? mRegistry->find(mNode->type) : nullptr;
    if (typeDef) {
        addDynamicFields(GTK_GRID(mGrid), row, *typeDef);
    } else {
        std::string text = "No .readdon definition loaded for type '" + mNode->type + "'";
        GtkWidget* note = gtk_label_new(text.c_str());
        gtk_label_set_wrap(GTK_LABEL(note), TRUE);
        gtk_widget_add_css_class(note, "dim-label");
        gtk_grid_attach(GTK_GRID(mGrid), note, 0, row, 3, 1);
    }

    mSuppressLive = false;
}

int PropertiesPanel::addBuiltInFields(GtkGrid* grid, int row) {
    GtkWidget* nameEntry = addEntryRow(grid, row++, "Name", "", mNode->name);
    g_signal_connect(nameEntry, "changed", G_CALLBACK(onNameChanged), this);
    trackFocus(nameEntry);

    addLabelRow(grid, row++, "Type", mNode->type.empty() ? "(none)" : mNode->type);

    addSkinPickerRow(grid, row++, mNode->skin());

    model::Anchor anchor = mNode->anchor();
    GtkWidget *anchorX, *anchorY;
    addAnchorRow(grid, row++, anchor.x, anchor.y, &anchorX, &anchorY);
    g_signal_connect(anchorX, "value-changed", G_CALLBACK(onAnchorXChanged), this);
    g_signal_connect(anchorY, "value-changed", G_CALLBACK(onAnchorYChanged), this);
    trackFocus(anchorX);
    trackFocus(anchorY);
    mAnchorXSpin = anchorX;
    mAnchorYSpin = anchorY;

    GtkWidget *xPixel, *xScale;
    model::LayoutAxis x = mNode->x();
    addAxisRow(grid, row++, "X", x.pixel, x.scale, &xPixel, &xScale);
    g_signal_connect(xPixel, "value-changed", G_CALLBACK(onXPixelChanged), this);
    g_signal_connect(xScale, "value-changed", G_CALLBACK(onXScaleChanged), this);
    trackFocus(xPixel);
    trackFocus(xScale);
    mXPixelSpin = xPixel;

    GtkWidget *yPixel, *yScale;
    model::LayoutAxis y = mNode->y();
    addAxisRow(grid, row++, "Y", y.pixel, y.scale, &yPixel, &yScale);
    g_signal_connect(yPixel, "value-changed", G_CALLBACK(onYPixelChanged), this);
    g_signal_connect(yScale, "value-changed", G_CALLBACK(onYScaleChanged), this);
    trackFocus(yPixel);
    trackFocus(yScale);
    mYPixelSpin = yPixel;

    GtkWidget *wPixel, *wScale;
    model::LayoutAxis w = mNode->width();
    addAxisRow(grid, row++, "Width", w.pixel, w.scale, &wPixel, &wScale);
    g_signal_connect(wPixel, "value-changed", G_CALLBACK(onWidthPixelChanged), this);
    g_signal_connect(wScale, "value-changed", G_CALLBACK(onWidthScaleChanged), this);
    trackFocus(wPixel);
    trackFocus(wScale);
    mWidthPixelSpin = wPixel;

    GtkWidget *hPixel, *hScale;
    model::LayoutAxis h = mNode->height();
    addAxisRow(grid, row++, "Height", h.pixel, h.scale, &hPixel, &hScale);
    g_signal_connect(hPixel, "value-changed", G_CALLBACK(onHeightPixelChanged), this);
    g_signal_connect(hScale, "value-changed", G_CALLBACK(onHeightScaleChanged), this);
    trackFocus(hPixel);
    trackFocus(hScale);
    mHeightPixelSpin = hPixel;

    return row;
}

int PropertiesPanel::addDynamicFields(GtkGrid* grid, int row, const model::WidgetTypeDef& typeDef) {
    std::vector<model::NamedEntry> properties = typeDef.section("available-properties");
    if (!properties.empty()) {
        GtkWidget* header = gtk_label_new("Properties");
        gtk_widget_add_css_class(header, "heading");
        gtk_label_set_xalign(GTK_LABEL(header), 0.0f);
        gtk_grid_attach(grid, header, 0, row++, 3, 1);

        for (const auto& [entryPtr, indent] : orderPropertiesTree(properties)) {
            const model::NamedEntry& entry = *entryPtr;
            std::string label = entry.name;
            std::string tooltip;
            std::string declaredType;
            if (entry.metadata) {
                if (const std::string* l = entry.metadata->get("label")) {
                    label = *l;
                }
                if (const std::string* d = entry.metadata->get("description")) {
                    tooltip = *d;
                }
                if (const std::string* t = entry.metadata->get("type")) {
                    declaredType = *t;
                }
            }

            const format::JsonValue* current = mNode->properties.find(entry.name);
            const format::JsonValue& value = current ? *current : entry.value;
            FieldKind kind = resolveFieldKind(declaredType, value);

            GtkWidget* field = nullptr;
            switch (kind) {
                case FieldKind::Bool: {
                    GtkWidget* labelWidget = gtk_label_new(label.c_str());
                    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
                    if (indent > 0) {
                        gtk_widget_set_margin_start(labelWidget, indent * 16);
                    }
                    if (!tooltip.empty()) {
                        gtk_widget_set_tooltip_text(labelWidget, tooltip.c_str());
                    }
                    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);
                    field = gtk_check_button_new();
                    gtk_check_button_set_active(GTK_CHECK_BUTTON(field), value.isBool() && value.boolValue);
                    gtk_grid_attach(grid, field, 1, row, 2, 1);
                    g_signal_connect(field, "toggled", G_CALLBACK(onDynamicBoolChanged), this);
                    break;
                }
                case FieldKind::Integer: {
                    double v = value.isNumber() ? value.numberValue : 0.0;
                    field = addSpinRow(grid, row, label, tooltip, v, -1000000, 1000000, 1, 0, indent);
                    g_signal_connect(field, "value-changed", G_CALLBACK(onDynamicNumberChanged), this);
                    break;
                }
                case FieldKind::Float: {
                    double v = value.isNumber() ? value.numberValue : 0.0;
                    field = addSpinRow(grid, row, label, tooltip, v, -1000000, 1000000, 0.01, 4, indent);
                    g_signal_connect(field, "value-changed", G_CALLBACK(onDynamicNumberChanged), this);
                    break;
                }
                case FieldKind::Widget: {
                    std::string current_ = value.isString() ? value.stringValue : std::string();
                    field = addWidgetPickerRow(grid, row, label, tooltip, current_, indent);
                    g_signal_connect(field, "notify::selected", G_CALLBACK(onDynamicWidgetRefChanged), this);
                    break;
                }
                case FieldKind::String:
                default: {
                    std::string text = value.isString() ? value.stringValue : std::string();
                    field = addEntryRow(grid, row, label, tooltip, text, indent);
                    g_signal_connect(field, "changed", G_CALLBACK(onDynamicTextChanged), this);
                    break;
                }
            }
            g_object_set_data_full(G_OBJECT(field), "regui-prop-name", g_strdup(entry.name.c_str()), g_free);
            if (!tooltip.empty()) {
                gtk_widget_set_tooltip_text(field, tooltip.c_str());
            }
            trackFocus(field);
            ++row;
        }
    }

    // required-callbacks (has metadata/defaults) merged with
    // available-callbacks (plain names), de-duplicated by name.
    std::vector<model::NamedEntry> callbacks = typeDef.section("required-callbacks");
    std::vector<std::string> seen;
    seen.reserve(callbacks.size());
    for (const model::NamedEntry& e : callbacks) {
        seen.push_back(e.name);
    }
    for (const std::string& name : typeDef.stringArraySection("available-callbacks")) {
        if (std::find(seen.begin(), seen.end(), name) == seen.end()) {
            model::NamedEntry entry;
            entry.name = name;
            entry.value = format::JsonValue::makeString("");
            callbacks.push_back(entry);
            seen.push_back(name);
        }
    }

    if (!callbacks.empty()) {
        GtkWidget* header = gtk_label_new("Callbacks");
        gtk_widget_add_css_class(header, "heading");
        gtk_label_set_xalign(GTK_LABEL(header), 0.0f);
        gtk_grid_attach(grid, header, 0, row++, 3, 1);

        for (const model::NamedEntry& entry : callbacks) {
            std::string label = entry.name;
            std::string tooltip;
            if (entry.metadata) {
                if (const std::string* l = entry.metadata->get("label")) {
                    label = *l;
                }
                if (const std::string* d = entry.metadata->get("description")) {
                    tooltip = *d;
                }
            }
            std::string current;
            if (const format::JsonValue* cb = mNode->callbacksJson()) {
                if (const format::JsonValue* v = cb->find(entry.name)) {
                    current = v->stringValue;
                }
            }
            GtkWidget* field = addEntryRow(grid, row, label, tooltip, current);
            g_object_set_data_full(G_OBJECT(field), "regui-callback-name", g_strdup(entry.name.c_str()), g_free);
            g_signal_connect(field, "changed", G_CALLBACK(onDynamicCallbackChanged), this);
            trackFocus(field);
            ++row;
        }
    }

    return row;
}

GtkWidget* PropertiesPanel::addLabelRow(GtkGrid* grid, int row, const std::string& label, const std::string& text) {
    GtkWidget* labelWidget = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* valueWidget = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(valueWidget), 0.0f);
    gtk_widget_add_css_class(valueWidget, "dim-label");
    gtk_grid_attach(grid, valueWidget, 1, row, 2, 1);
    return valueWidget;
}

GtkWidget* PropertiesPanel::addEntryRow(
    GtkGrid* grid, int row, const std::string& label, const std::string& tooltip, const std::string& value,
    int indent) {
    GtkWidget* labelWidget = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    if (indent > 0) {
        gtk_widget_set_margin_start(labelWidget, indent * 16);
    }
    if (!tooltip.empty()) {
        gtk_widget_set_tooltip_text(labelWidget, tooltip.c_str());
    }
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), value.c_str());
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(grid, entry, 1, row, 2, 1);
    return entry;
}

GtkWidget* PropertiesPanel::makeSpin(double value, double min, double max, double step, int digits) {
    GtkWidget* spin = gtk_spin_button_new_with_range(min, max, step);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), digits);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
    gtk_widget_set_hexpand(spin, TRUE);
    return spin;
}

GtkWidget* PropertiesPanel::addSpinRow(
    GtkGrid* grid, int row, const std::string& label, const std::string& tooltip, double value, double min,
    double max, double step, int digits, int indent) {
    GtkWidget* labelWidget = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    if (indent > 0) {
        gtk_widget_set_margin_start(labelWidget, indent * 16);
    }
    if (!tooltip.empty()) {
        gtk_widget_set_tooltip_text(labelWidget, tooltip.c_str());
    }
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* spin = makeSpin(value, min, max, step, digits);
    gtk_grid_attach(grid, spin, 1, row, 2, 1);
    return spin;
}

void PropertiesPanel::addAxisRow(
    GtkGrid* grid, int row, const std::string& label, double pixel, double scale, GtkWidget** outPixel,
    GtkWidget** outScale) {
    GtkWidget* labelWidget = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* pixelSpin = makeSpin(pixel, -100000, 100000, 1, 1);
    gtk_widget_set_tooltip_text(pixelSpin, "pixel offset");
    gtk_grid_attach(grid, pixelSpin, 1, row, 1, 1);

    GtkWidget* scaleSpin = makeSpin(scale, -10, 10, 0.01, 4);
    gtk_widget_set_tooltip_text(scaleSpin, "scale (fraction of parent)");
    gtk_grid_attach(grid, scaleSpin, 2, row, 1, 1);

    *outPixel = pixelSpin;
    *outScale = scaleSpin;
}

void PropertiesPanel::addAnchorRow(GtkGrid* grid, int row, double x, double y, GtkWidget** outX, GtkWidget** outY) {
    GtkWidget* labelWidget = gtk_label_new("Anchor");
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* xSpin = makeSpin(x, -2, 2, 0.01, 3);
    gtk_widget_set_tooltip_text(xSpin, "anchor X (fraction of the widget's own size)");
    gtk_grid_attach(grid, xSpin, 1, row, 1, 1);

    GtkWidget* ySpin = makeSpin(y, -2, 2, 0.01, 3);
    gtk_widget_set_tooltip_text(ySpin, "anchor Y (fraction of the widget's own size)");
    gtk_grid_attach(grid, ySpin, 2, row, 1, 1);

    *outX = xSpin;
    *outY = ySpin;
}

void PropertiesPanel::collectWidgetNames(std::vector<model::WidgetNode>& nodes, std::vector<std::string>& out) const {
    for (model::WidgetNode& node : nodes) {
        if (!node.name.empty()) {
            out.push_back(node.name);
        }
        collectWidgetNames(node.children, out);
    }
}

GtkWidget* PropertiesPanel::addWidgetPickerRow(
    GtkGrid* grid, int row, const std::string& label, const std::string& tooltip, const std::string& current,
    int indent) {
    GtkWidget* labelWidget = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    if (indent > 0) {
        gtk_widget_set_margin_start(labelWidget, indent * 16);
    }
    if (!tooltip.empty()) {
        gtk_widget_set_tooltip_text(labelWidget, tooltip.c_str());
    }
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    std::vector<std::string> names = {"(none)"};
    if (mRoots) {
        collectWidgetNames(*mRoots, names);
    }
    std::vector<const char*> cNames;
    cNames.reserve(names.size() + 1);
    for (const std::string& name : names) {
        cNames.push_back(name.c_str());
    }
    cNames.push_back(nullptr);

    GtkWidget* dropdown = gtk_drop_down_new_from_strings(cNames.data());
    guint selected = 0; // "(none)"
    for (guint i = 0; i < names.size(); ++i) {
        if (names[i] == current) {
            selected = i;
            break;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), selected);
    gtk_widget_set_hexpand(dropdown, TRUE);
    if (!tooltip.empty()) {
        gtk_widget_set_tooltip_text(dropdown, tooltip.c_str());
    }
    gtk_grid_attach(grid, dropdown, 1, row, 2, 1);
    return dropdown;
}

GtkWidget* PropertiesPanel::addSkinPickerRow(GtkGrid* grid, int row, const std::string& current) {
    GtkWidget* labelWidget = gtk_label_new("Skin");
    gtk_label_set_xalign(GTK_LABEL(labelWidget), 0.0f);
    gtk_grid_attach(grid, labelWidget, 0, row, 1, 1);

    GtkWidget* button = gtk_button_new_with_label(current.empty() ? "(none)" : current.c_str());
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_tooltip_text(button, "Widget skin - click to browse every skin with a live preview");
    gtk_grid_attach(grid, button, 1, row, 2, 1);
    g_signal_connect(button, "clicked", G_CALLBACK(onSkinButtonClicked), this);
    return button;
}

void PropertiesPanel::trackFocus(GtkWidget* field) {
    GtkEventController* focus = gtk_event_controller_focus_new();
    g_signal_connect(focus, "enter", G_CALLBACK(onFocusEnter), this);
    g_signal_connect(focus, "leave", G_CALLBACK(onFocusLeave), this);
    gtk_widget_add_controller(field, focus);
}

void PropertiesPanel::onFocusEnter(GtkEventControllerFocus*, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mNode && !self->mEditing) {
        self->mEditSnapshot = {self->mNode->name, self->mNode->properties};
        self->mEditing = true;
    }
}

void PropertiesPanel::onFocusLeave(GtkEventControllerFocus*, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (!self->mEditing || !self->mNode) {
        self->mEditing = false;
        return;
    }
    self->mEditing = false;

    bool changed =
        self->mNode->name != self->mEditSnapshot.name || self->mNode->properties != self->mEditSnapshot.properties;
    if (!changed) {
        return;
    }

    if (self->mUndoStack) {
        model::WidgetNode* node = self->mNode;
        Snapshot before = self->mEditSnapshot;
        Snapshot after = {node->name, node->properties};
        PropertiesPanel* panel = self;
        std::string description = before.name != after.name ? "Widget Renamed" : "Widget Properties Changed";
        std::string detail = before.name != after.name ? "Renamed '" + before.name + "' to '" + after.name + "'" : "";
        self->mUndoStack->push(
            [node, before, panel]() {
                node->name = before.name;
                node->properties = before.properties;
                panel->refresh();
                if (panel->mOnCommit) {
                    panel->mOnCommit();
                }
            },
            [node, after, panel]() {
                node->name = after.name;
                node->properties = after.properties;
                panel->refresh();
                if (panel->mOnCommit) {
                    panel->mOnCommit();
                }
            },
            description, detail);
    }

    if (self->mOnCommit) {
        self->mOnCommit();
    }
}

void PropertiesPanel::onNameChanged(GtkEditable* editable, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* text = gtk_editable_get_text(editable);
    self->mNode->name = text ? text : "";
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onSkinButtonClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (!self->mNode) {
        return;
    }
    model::WidgetNode* node = self->mNode;
    std::string before = node->skin();
    static const std::vector<canvas::SkinInfo> kEmptySkins;
    const std::vector<canvas::SkinInfo>& skins = self->mAvailableSkins ? *self->mAvailableSkins : kEmptySkins;

    // One-shot modal edit, fires once on "Confirm & Apply" - build the undo pair directly, no focus-snapshot dance.
    SkinPickerDialog::show(self->mParentWindow, skins, before, [self, node, before](std::string chosen) {
        if (chosen == before) {
            return; // picked the same skin back - nothing to commit
        }
        node->setSkin(chosen);
        if (self->mUndoStack) {
            PropertiesPanel* panel = self;
            std::string nodeName = node->name;
            self->mUndoStack->push(
                [node, before, panel]() {
                    node->setSkin(before);
                    panel->refresh();
                    if (panel->mOnCommit) {
                        panel->mOnCommit();
                    }
                },
                [node, chosen, panel]() {
                    node->setSkin(chosen);
                    panel->refresh();
                    if (panel->mOnCommit) {
                        panel->mOnCommit();
                    }
                },
                "Widget Skin Changed", "Changed '" + nodeName + "' skin from '" + before + "' to '" + chosen + "'");
        }
        self->refresh();
        if (self->mOnLiveChange) {
            self->mOnLiveChange();
        }
        if (self->mOnCommit) {
            self->mOnCommit();
        }
    });
}

void PropertiesPanel::onAnchorXChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::Anchor a = self->mNode->anchor();
    a.x = gtk_spin_button_get_value(spin);
    self->mNode->setAnchor(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onAnchorYChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::Anchor a = self->mNode->anchor();
    a.y = gtk_spin_button_get_value(spin);
    self->mNode->setAnchor(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onXPixelChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->x();
    a.pixel = gtk_spin_button_get_value(spin);
    self->mNode->setX(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onXScaleChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->x();
    a.scale = gtk_spin_button_get_value(spin);
    self->mNode->setX(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onYPixelChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->y();
    a.pixel = gtk_spin_button_get_value(spin);
    self->mNode->setY(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onYScaleChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->y();
    a.scale = gtk_spin_button_get_value(spin);
    self->mNode->setY(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onWidthPixelChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->width();
    a.pixel = gtk_spin_button_get_value(spin);
    self->mNode->setWidth(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onWidthScaleChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->width();
    a.scale = gtk_spin_button_get_value(spin);
    self->mNode->setWidth(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onHeightPixelChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->height();
    a.pixel = gtk_spin_button_get_value(spin);
    self->mNode->setHeight(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onHeightScaleChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    model::LayoutAxis a = self->mNode->height();
    a.scale = gtk_spin_button_get_value(spin);
    self->mNode->setHeight(a);
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onDynamicTextChanged(GtkEditable* editable, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(editable), "regui-prop-name"));
    if (!name) {
        return;
    }
    const char* text = gtk_editable_get_text(editable);
    self->mNode->properties.set(name, format::JsonValue::makeString(text ? text : ""));
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onDynamicNumberChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(spin), "regui-prop-name"));
    if (!name) {
        return;
    }
    self->mNode->properties.set(name, format::JsonValue::makeNumber(gtk_spin_button_get_value(spin)));
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onDynamicBoolChanged(GtkCheckButton* check, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(check), "regui-prop-name"));
    if (!name) {
        return;
    }
    self->mNode->properties.set(name, format::JsonValue::makeBool(gtk_check_button_get_active(check)));
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onDynamicCallbackChanged(GtkEditable* editable, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(editable), "regui-callback-name"));
    if (!name) {
        return;
    }
    const char* text = gtk_editable_get_text(editable);
    self->mNode->setCallback(name, text ? text : "");
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

void PropertiesPanel::onDynamicWidgetRefChanged(GtkDropDown* dropdown, GParamSpec*, gpointer userData) {
    auto* self = static_cast<PropertiesPanel*>(userData);
    if (self->mSuppressLive || !self->mNode) {
        return;
    }
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(dropdown), "regui-prop-name"));
    if (!name) {
        return;
    }
    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected == GTK_INVALID_LIST_POSITION) {
        return;
    }
    auto* model = GTK_STRING_LIST(gtk_drop_down_get_model(dropdown));
    const char* text = gtk_string_list_get_string(model, selected);
    // Index 0 is the "(none)" placeholder - clears the reference instead of storing "(none)".
    self->mNode->properties.set(
        name, format::JsonValue::makeString(selected == 0 ? std::string() : std::string(text ? text : "")));
    if (self->mOnLiveChange) {
        self->mOnLiveChange();
    }
}

} // namespace regui::ui
