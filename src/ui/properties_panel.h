#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "canvas/mygui_canvas.h"
#include "format/json_value.h"
#include "model/addon.h"
#include "model/layout.h"
#include "model/undo_stack.h"

namespace regui::ui {

// A property grid bound to the currently-selected WidgetNode. Only
// type-agnostic fields (name, skin, anchor, position/size, z-order) are
// built-in; everything else is generated at runtime from the matching
// WidgetTypeDef in the ReAddonRegistry.
class PropertiesPanel {
public:
    PropertiesPanel(model::ReAddonRegistry* registry, model::UndoStack* undoStack);

    GtkWidget* widget() const { return mScrolled; }

    // Displays `node`'s properties; nullptr shows an empty/disabled state.
    void setNode(model::WidgetNode* node);

    // Widget forest, for populating a "widget"-type property's picker. Non-owning.
    void setRoots(std::vector<model::WidgetNode>* roots) { mRoots = roots; }

    // Skins for the "Skin" field's picker - typically MyGuiCanvas::availableSkins(). Non-owning.
    void setAvailableSkins(const std::vector<canvas::SkinInfo>* skins) { mAvailableSkins = skins; }

    // Parent for the skin picker popup. Non-owning; must outlive the panel.
    void setParentWindow(GtkWindow* window) { mParentWindow = window; }

    // Rebuilds the fields for the current node.
    void refresh();

    // Cheap alternative to setNode()/refresh() for every mouse-move tick
    // while dragging: pushes values into the existing spin buttons instead
    // of rebuilding the grid. No-op unless `node` is the displayed one.
    void updateLiveValues(model::WidgetNode* node);

    // Fired on every field edit, immediately (live preview).
    void setOnLiveChange(std::function<void()> callback) { mOnLiveChange = std::move(callback); }

    // Fired once an edit commits (field loses focus) and something changed.
    void setOnCommit(std::function<void()> callback) { mOnCommit = std::move(callback); }

private:
    struct Snapshot {
        std::string name;
        format::JsonValue properties;
    };

    GtkWidget* mScrolled = nullptr;
    GtkWidget* mGrid = nullptr; // current field grid, replaced wholesale by rebuildFields()

    model::WidgetNode* mNode = nullptr;
    model::ReAddonRegistry* mRegistry = nullptr;
    model::UndoStack* mUndoStack = nullptr;
    std::vector<model::WidgetNode>* mRoots = nullptr;
    const std::vector<canvas::SkinInfo>* mAvailableSkins = nullptr;
    GtkWindow* mParentWindow = nullptr;
    std::function<void()> mOnLiveChange;
    std::function<void()> mOnCommit;

    bool mEditing = false;      // an edit session (focus-in to focus-out) is active
    bool mSuppressLive = false; // true while rebuildFields() sets initial values
    Snapshot mEditSnapshot;

    // Spin buttons updateLiveValues() can push to; reset by rebuildFields(), (re)populated by addBuiltInFields().
    GtkWidget* mAnchorXSpin = nullptr;
    GtkWidget* mAnchorYSpin = nullptr;
    GtkWidget* mXPixelSpin = nullptr;
    GtkWidget* mYPixelSpin = nullptr;
    GtkWidget* mWidthPixelSpin = nullptr;
    GtkWidget* mHeightPixelSpin = nullptr;

    void rebuildFields();
    int addBuiltInFields(GtkGrid* grid, int row);
    int addDynamicFields(GtkGrid* grid, int row, const model::WidgetTypeDef& typeDef);

    // Row helpers: label in column 0, field(s) spanning the rest.
    GtkWidget* addLabelRow(GtkGrid* grid, int row, const std::string& label, const std::string& text);
    // `indent` nests a row under its declared metadata `parent:` property; 0 for top-level fields.
    GtkWidget* addEntryRow(
        GtkGrid* grid, int row, const std::string& label, const std::string& tooltip, const std::string& value,
        int indent = 0);
    GtkWidget* addSpinRow(
        GtkGrid* grid,
        int row,
        const std::string& label,
        const std::string& tooltip,
        double value,
        double min,
        double max,
        double step,
        int digits,
        int indent = 0);
    void addAxisRow(
        GtkGrid* grid, int row, const std::string& label, double pixel, double scale, GtkWidget** outPixel,
        GtkWidget** outScale);
    // Anchor X/Y are plain fractions (no pixel/scale split): one row, X and Y side by side.
    void addAnchorRow(GtkGrid* grid, int row, double x, double y, GtkWidget** outX, GtkWidget** outY);
    GtkWidget* makeSpin(double value, double min, double max, double step, int digits);
    GtkWidget* addWidgetPickerRow(
        GtkGrid* grid, int row, const std::string& label, const std::string& tooltip, const std::string& current,
        int indent = 0);
    void collectWidgetNames(std::vector<model::WidgetNode>& nodes, std::vector<std::string>& out) const;
    // Button showing the current skin name; opens SkinPickerDialog.
    GtkWidget* addSkinPickerRow(GtkGrid* grid, int row, const std::string& current);
    void trackFocus(GtkWidget* field);

    static void onFocusEnter(GtkEventControllerFocus* controller, gpointer userData);
    static void onFocusLeave(GtkEventControllerFocus* controller, gpointer userData);

    // Built-in field handlers.
    static void onNameChanged(GtkEditable* editable, gpointer userData);
    static void onSkinButtonClicked(GtkButton* button, gpointer userData);
    static void onAnchorXChanged(GtkSpinButton* spin, gpointer userData);
    static void onAnchorYChanged(GtkSpinButton* spin, gpointer userData);
    static void onXPixelChanged(GtkSpinButton* spin, gpointer userData);
    static void onXScaleChanged(GtkSpinButton* spin, gpointer userData);
    static void onYPixelChanged(GtkSpinButton* spin, gpointer userData);
    static void onYScaleChanged(GtkSpinButton* spin, gpointer userData);
    static void onWidthPixelChanged(GtkSpinButton* spin, gpointer userData);
    static void onWidthScaleChanged(GtkSpinButton* spin, gpointer userData);
    static void onHeightPixelChanged(GtkSpinButton* spin, gpointer userData);
    static void onHeightScaleChanged(GtkSpinButton* spin, gpointer userData);

    // Dynamic field handlers: bound property/callback name is stashed on the widget itself.
    static void onDynamicTextChanged(GtkEditable* editable, gpointer userData);
    static void onDynamicNumberChanged(GtkSpinButton* spin, gpointer userData);
    static void onDynamicBoolChanged(GtkCheckButton* check, gpointer userData);
    static void onDynamicCallbackChanged(GtkEditable* editable, gpointer userData);
    static void onDynamicWidgetRefChanged(GtkDropDown* dropdown, GParamSpec* pspec, gpointer userData);
};

} // namespace regui::ui
