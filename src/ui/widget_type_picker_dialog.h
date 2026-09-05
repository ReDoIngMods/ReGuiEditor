#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

#include "model/addon.h"

namespace regui::ui {

// A modal "choose a widget type" popup for creating a new widget, listing
// every type loaded into the ReAddonRegistry by its declared icon/label,
// with the description shown as a hover tooltip.
class WidgetTypePickerDialog {
public:
    // `onChosen` fires once, only when a type is actually picked (double-click
    // a row, or select one and click "Create").
    static void show(GtkWindow* parent, const model::ReAddonRegistry& registry, std::function<void(std::string)> onChosen);

private:
    // Heap-allocated alongside the GtkWindow, freed on "destroy".
    struct State;

    static GtkWidget* buildList(State* state, const model::ReAddonRegistry& registry);
    static gboolean filterRow(GtkListBoxRow* row, gpointer userData);
    static void onSearchChanged(GtkSearchEntry* entry, gpointer userData);
    static void onRowActivated(GtkListBox* box, GtkListBoxRow* row, gpointer userData);
    static void onCancelClicked(GtkButton* button, gpointer userData);
    static void onCreateClicked(GtkButton* button, gpointer userData);
    static void onDialogDestroy(GtkWidget* dialog, gpointer userData);
};

} // namespace regui::ui
