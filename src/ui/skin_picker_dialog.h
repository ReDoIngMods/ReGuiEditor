#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "canvas/mygui_canvas.h"

namespace regui::ui {

// A modal "browse skins" popup, grouped by skin XML file, with a live
// preview composited from each SkinInfo's own pieces (not a real MyGUI
// render - MyGUI's singletons rule out a second live instance).
class SkinPickerDialog {
public:
    // `current` preselects a row. `onChosen` fires once, only on "Confirm & Apply".
    static void show(
        GtkWindow* parent, const std::vector<canvas::SkinInfo>& skins, const std::string& current,
        std::function<void(std::string)> onChosen);

private:
    // Heap-allocated alongside the GtkWindow, freed on "destroy". Defined in
    // the .cpp (needs <windows.h>, which this header shouldn't leak).
    struct State;

    static GtkWidget* buildSkinList(State* state, const std::vector<canvas::SkinInfo>& skins, const std::string& current);
    static void selectSkin(State* state, const canvas::SkinInfo* skin);

    static void onRowSelected(GtkListBox* box, GtkListBoxRow* row, gpointer userData);
    static void onPreviewSizeChanged(GtkSpinButton* spin, gpointer userData);
    static void onDrawPreview(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer userData);
    static void onCancelClicked(GtkButton* button, gpointer userData);
    static void onApplyClicked(GtkButton* button, gpointer userData);
    static void onDialogDestroy(GtkWidget* dialog, gpointer userData);
};

} // namespace regui::ui
