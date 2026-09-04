#include "ui/skin_picker_dialog.h"

#include "ui/center_window.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

#include <algorithm>
#include <cmath>

namespace regui::ui {

struct SkinPickerDialog::State {
    std::function<void(std::string)> onChosen;
    GtkWidget* dialog = nullptr;
    GtkWidget* previewArea = nullptr;
    GtkWidget* previewSizeSpin = nullptr;
    GtkWidget* nameLabel = nullptr;
    const canvas::SkinInfo* selected = nullptr;
    GdkPixbuf* previewPixbuf = nullptr; // decoded texture atlas for `selected`; reloaded on selection change
#ifdef GDK_WINDOWING_WIN32
    HWND parentHwnd = nullptr; // disabled for the popup's lifetime - see show()/onDialogDestroy()
#endif
};

namespace {

// Header label above the first row of each category (rows are pre-sorted).
void headerFunc(GtkListBoxRow* row, GtkListBoxRow* before, gpointer) {
    auto* skin = static_cast<canvas::SkinInfo*>(g_object_get_data(G_OBJECT(row), "regui-skin"));
    auto* prevSkin = before ? static_cast<canvas::SkinInfo*>(g_object_get_data(G_OBJECT(before), "regui-skin")) : nullptr;
    if (skin && (!prevSkin || prevSkin->category != skin->category)) {
        GtkWidget* header = gtk_label_new(skin->category.c_str());
        gtk_widget_add_css_class(header, "heading");
        gtk_widget_add_css_class(header, "dim-label");
        gtk_label_set_xalign(GTK_LABEL(header), 0.0f);
        gtk_widget_set_margin_top(header, before ? 10 : 2);
        gtk_widget_set_margin_start(header, 4);
        gtk_list_box_row_set_header(row, header);
    } else {
        gtk_list_box_row_set_header(row, nullptr);
    }
}

GtkWidget* makeRowLabel(const std::string& text) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 6);
    gtk_widget_set_margin_end(label, 6);
    gtk_widget_set_margin_top(label, 4);
    gtk_widget_set_margin_bottom(label, 4);
    return label;
}

} // namespace

void SkinPickerDialog::show(
    GtkWindow* parent, const std::vector<canvas::SkinInfo>& skins, const std::string& current,
    std::function<void(std::string)> onChosen) {
    auto* state = new State();
    state->onChosen = std::move(onChosen);

    GtkWidget* dialog = gtk_window_new();
    state->dialog = dialog;
    gtk_window_set_title(GTK_WINDOW(dialog), "Choose Skin");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 760, 520);
    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    }
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(rootBox, 8);
    gtk_widget_set_margin_end(rootBox, 8);
    gtk_widget_set_margin_top(rootBox, 8);
    gtk_widget_set_margin_bottom(rootBox, 8);
    gtk_window_set_child(GTK_WINDOW(dialog), rootBox);

    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(rootBox), paned);

    // Built before buildSkinList(), whose preselected row synchronously
    // fires selectSkin() and reads nameLabel/previewArea.
    GtkWidget* rightBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(rightBox, 260, -1);
    gtk_widget_set_margin_start(rightBox, 14); // clearance from the paned divider/skin list

    GtkWidget* nameLabel = gtk_label_new("(none)");
    gtk_label_set_xalign(GTK_LABEL(nameLabel), 0.0f);
    gtk_widget_add_css_class(nameLabel, "heading");
    gtk_box_append(GTK_BOX(rightBox), nameLabel);
    state->nameLabel = nameLabel;

    GtkWidget* sizeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* sizeLabel = gtk_label_new("Preview size");
    gtk_box_append(GTK_BOX(sizeRow), sizeLabel);
    GtkWidget* sizeSpin = gtk_spin_button_new_with_range(16, 512, 8);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sizeSpin), 256);
    gtk_widget_set_hexpand(sizeSpin, TRUE);
    gtk_widget_set_tooltip_text(sizeSpin, "Size (px) of the preview widget's larger dimension");
    g_signal_connect(sizeSpin, "value-changed", G_CALLBACK(onPreviewSizeChanged), state);
    gtk_box_append(GTK_BOX(sizeRow), sizeSpin);
    gtk_box_append(GTK_BOX(rightBox), sizeRow);
    state->previewSizeSpin = sizeSpin;

    // onDrawPreview() clamps/centers to whatever GTK allocates this area on each draw.
    GtkWidget* previewFrame = gtk_frame_new(nullptr);
    gtk_widget_set_hexpand(previewFrame, TRUE);
    gtk_widget_set_vexpand(previewFrame, TRUE);

    GtkWidget* previewArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(previewArea, TRUE);
    gtk_widget_set_vexpand(previewArea, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(previewArea), onDrawPreview, state, nullptr);
    gtk_frame_set_child(GTK_FRAME(previewFrame), previewArea);
    gtk_box_append(GTK_BOX(rightBox), previewFrame);
    state->previewArea = previewArea;

    GtkWidget* listContainer = buildSkinList(state, skins, current);
    gtk_paned_set_start_child(GTK_PANED(paned), listContainer);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    gtk_paned_set_end_child(GTK_PANED(paned), rightBox);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 460);

    GtkWidget* buttonRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttonRow, GTK_ALIGN_END);
    GtkWidget* cancelButton = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancelButton, "clicked", G_CALLBACK(onCancelClicked), state);
    gtk_box_append(GTK_BOX(buttonRow), cancelButton);
    GtkWidget* applyButton = gtk_button_new_with_label("Confirm & Apply");
    gtk_widget_add_css_class(applyButton, "suggested-action");
    g_signal_connect(applyButton, "clicked", G_CALLBACK(onApplyClicked), state);
    gtk_box_append(GTK_BOX(buttonRow), applyButton);
    gtk_box_append(GTK_BOX(rootBox), buttonRow);

    g_signal_connect(dialog, "destroy", G_CALLBACK(onDialogDestroy), state);

#ifdef GDK_WINDOWING_WIN32
    // GTK4's modal/transient-for handling doesn't reliably block input to
    // the parent on Win32, so disable the owner HWND directly like a native
    // modal dialog would; re-enabled in onDialogDestroy().
    if (parent) {
        if (GdkSurface* parentSurface = gtk_native_get_surface(GTK_NATIVE(parent))) {
            if (GDK_IS_WIN32_SURFACE(parentSurface)) {
                state->parentHwnd = gdk_win32_surface_get_handle(parentSurface);
                EnableWindow(state->parentHwnd, FALSE);
            }
        }
    }
#endif

    gtk_window_present(GTK_WINDOW(dialog));
    centerWindowOnScreen(GTK_WINDOW(dialog));
}

GtkWidget* SkinPickerDialog::buildSkinList(
    State* state, const std::vector<canvas::SkinInfo>& skins, const std::string& current) {
    std::vector<const canvas::SkinInfo*> sorted;
    sorted.reserve(skins.size());
    for (const canvas::SkinInfo& skin : skins) {
        sorted.push_back(&skin);
    }
    std::sort(sorted.begin(), sorted.end(), [](const canvas::SkinInfo* a, const canvas::SkinInfo* b) {
        return a->category != b->category ? a->category < b->category : a->name < b->name;
    });

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);

    GtkWidget* listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_BROWSE);
    gtk_list_box_set_header_func(GTK_LIST_BOX(listBox), headerFunc, nullptr, nullptr);

    GtkWidget* noneRow = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(noneRow), makeRowLabel("(none)"));
    gtk_list_box_append(GTK_LIST_BOX(listBox), noneRow);

    // Falls back to "(none)" if `current` doesn't match any loaded skin.
    GtkListBoxRow* selectRow = GTK_LIST_BOX_ROW(noneRow);
    for (const canvas::SkinInfo* skin : sorted) {
        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), makeRowLabel(skin->name));
        g_object_set_data(G_OBJECT(row), "regui-skin", const_cast<canvas::SkinInfo*>(skin));
        gtk_list_box_append(GTK_LIST_BOX(listBox), row);
        if (!current.empty() && skin->name == current) {
            selectRow = GTK_LIST_BOX_ROW(row);
        }
    }

    g_signal_connect(listBox, "row-selected", G_CALLBACK(onRowSelected), state);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), listBox);

    if (selectRow) {
        gtk_list_box_select_row(GTK_LIST_BOX(listBox), selectRow);
        gtk_widget_grab_focus(GTK_WIDGET(selectRow));
    }
    return scroller;
}

void SkinPickerDialog::selectSkin(State* state, const canvas::SkinInfo* skin) {
    state->selected = skin;
    if (state->previewPixbuf) {
        g_object_unref(state->previewPixbuf);
        state->previewPixbuf = nullptr;
    }
    if (skin && !skin->texturePath.empty()) {
        GError* error = nullptr;
        state->previewPixbuf = gdk_pixbuf_new_from_file(skin->texturePath.c_str(), &error);
        if (error) {
            g_error_free(error); // leaves previewPixbuf null - onDrawPreview shows the "(no preview)" fallback
        }
    }
    gtk_label_set_text(GTK_LABEL(state->nameLabel), skin ? skin->name.c_str() : "(none)");
    gtk_widget_queue_draw(state->previewArea);
}

void SkinPickerDialog::onRowSelected(GtkListBox*, GtkListBoxRow* row, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    const auto* skin = row ? static_cast<canvas::SkinInfo*>(g_object_get_data(G_OBJECT(row), "regui-skin")) : nullptr;
    selectSkin(state, skin);
}

void SkinPickerDialog::onPreviewSizeChanged(GtkSpinButton*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    gtk_widget_queue_draw(state->previewArea);
}

void SkinPickerDialog::onDrawPreview(GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer userData) {
    auto* state = static_cast<State*>(userData);

    // Checkerboard backdrop so an alpha-channel skin's silhouette reads clearly.
    constexpr int kCell = 8;
    for (int y = 0; y < height; y += kCell) {
        for (int x = 0; x < width; x += kCell) {
            bool dark = ((x / kCell) + (y / kCell)) % 2 == 0;
            double shade = dark ? 0.35 : 0.45;
            cairo_set_source_rgb(cr, shade, shade, shade);
            cairo_rectangle(cr, x, y, kCell, kCell);
            cairo_fill(cr);
        }
    }

    const canvas::SkinInfo* skin = state->selected;
    if (!skin) {
        return;
    }
    if (skin->pieces.empty() || !state->previewPixbuf) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, 6, height / 2.0);
        cairo_show_text(cr, "No texture preview for this skin");
        return;
    }

    // Requested preview box from the spinner and the skin's aspect ratio, clamped to the allocated area.
    double baseW = skin->baseWidth > 0 ? skin->baseWidth : width;
    double baseH = skin->baseHeight > 0 ? skin->baseHeight : height;
    double aspect = baseW / baseH;
    double requestedSize = gtk_spin_button_get_value(GTK_SPIN_BUTTON(state->previewSizeSpin));
    double boxW = aspect >= 1.0 ? requestedSize : requestedSize * aspect;
    double boxH = aspect >= 1.0 ? requestedSize / aspect : requestedSize;
    constexpr double kPad = 8.0;
    double shrink = std::min({1.0, (width - kPad) / boxW, (height - kPad) / boxH});
    if (shrink > 0) {
        boxW *= shrink;
        boxH *= shrink;
    }

    // Reproduces MyGUI's 9-slice: each piece stays pinned to its aligned edge(s), Stretch fills the margin.
    cairo_save(cr);
    cairo_translate(cr, (width - boxW) / 2.0, (height - boxH) / 2.0); // center the (possibly shrunk) box

    for (const canvas::SkinPiece& piece : skin->pieces) {
        if (piece.textureRect.width <= 0 || piece.textureRect.height <= 0) {
            continue;
        }
        double leftMargin = piece.place.x;
        double topMargin = piece.place.y;
        double rightMargin = baseW - (piece.place.x + piece.place.width);
        double bottomMargin = baseH - (piece.place.y + piece.place.height);

        double px, pw;
        switch (piece.hAlign) {
            case canvas::SkinAlign::End:
                pw = piece.place.width;
                px = boxW - rightMargin - pw;
                break;
            case canvas::SkinAlign::Stretch:
                px = leftMargin;
                pw = std::max(0.0, boxW - leftMargin - rightMargin);
                break;
            case canvas::SkinAlign::Begin:
            default:
                px = leftMargin;
                pw = piece.place.width;
                break;
        }
        double py, ph;
        switch (piece.vAlign) {
            case canvas::SkinAlign::End:
                ph = piece.place.height;
                py = boxH - bottomMargin - ph;
                break;
            case canvas::SkinAlign::Stretch:
                py = topMargin;
                ph = std::max(0.0, boxH - topMargin - bottomMargin);
                break;
            case canvas::SkinAlign::Begin:
            default:
                py = topMargin;
                ph = piece.place.height;
                break;
        }
        if (pw <= 0 || ph <= 0) {
            continue;
        }

        cairo_save(cr);
        cairo_translate(cr, px, py);
        cairo_scale(cr, pw / piece.textureRect.width, ph / piece.textureRect.height);
        gdk_cairo_set_source_pixbuf(cr, state->previewPixbuf, -piece.textureRect.x, -piece.textureRect.y);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST); // pixel-art atlas - no smoothing blur
        cairo_rectangle(cr, 0, 0, piece.textureRect.width, piece.textureRect.height);
        cairo_clip(cr);
        cairo_paint(cr);
        cairo_restore(cr);
    }
    cairo_restore(cr); // matches the translate() before the loop
}

void SkinPickerDialog::onCancelClicked(GtkButton*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

void SkinPickerDialog::onApplyClicked(GtkButton*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    std::string chosen = state->selected ? state->selected->name : std::string();
    if (state->onChosen) {
        state->onChosen(chosen);
    }
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

void SkinPickerDialog::onDialogDestroy(GtkWidget*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
#ifdef GDK_WINDOWING_WIN32
    if (state->parentHwnd) {
        EnableWindow(state->parentHwnd, TRUE);
        SetActiveWindow(state->parentHwnd); // native modal-close behaviour: focus returns to the owner
    }
#endif
    if (state->previewPixbuf) {
        g_object_unref(state->previewPixbuf);
    }
    delete state;
}

} // namespace regui::ui
