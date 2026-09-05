#include "ui/widget_type_picker_dialog.h"

#include "ui/center_window.h"

#include <algorithm>
#include <cctype>

namespace regui::ui {

struct WidgetTypePickerDialog::State {
    std::function<void(std::string)> onChosen;
    GtkWidget* dialog = nullptr;
    GtkWidget* listBox = nullptr;
    std::string searchQuery; // lowercased; empty means "show everything"
};

namespace {

constexpr int kIconDisplaySize = 24;

std::string toLower(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Decodes a base64 icon and scales it down to kIconDisplaySize; nullptr if there isn't one.
GdkTexture* decodeIcon(const std::string& base64) {
    if (base64.empty()) {
        return nullptr;
    }
    gsize decodedLen = 0;
    guchar* decoded = g_base64_decode(base64.c_str(), &decodedLen);
    if (!decoded) {
        return nullptr;
    }
    GBytes* bytes = g_bytes_new_take(decoded, decodedLen); // takes ownership of `decoded`, frees via g_free
    GInputStream* stream = g_memory_input_stream_new_from_bytes(bytes);
    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_stream(stream, nullptr, &error);
    g_object_unref(stream);
    g_bytes_unref(bytes);
    if (error) {
        g_error_free(error); // leaves pixbuf null - handled below
    }
    if (!pixbuf) {
        return nullptr;
    }

    GdkPixbuf* scaled = gdk_pixbuf_scale_simple(pixbuf, kIconDisplaySize, kIconDisplaySize, GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);
    if (!scaled) {
        return nullptr;
    }
    GdkTexture* texture = gdk_texture_new_for_pixbuf(scaled);
    g_object_unref(scaled);
    return texture;
}

GtkWidget* makeRow(const model::WidgetTypeDef& type) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(box, 6);
    gtk_widget_set_margin_end(box, 6);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);

    GtkWidget* icon = gtk_image_new();
    gtk_widget_set_size_request(icon, kIconDisplaySize, kIconDisplaySize);
    if (GdkTexture* texture = decodeIcon(type.icon())) {
        gtk_image_set_from_paintable(GTK_IMAGE(icon), GDK_PAINTABLE(texture));
        g_object_unref(texture); // the GtkImage keeps its own reference
    }
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget* label = gtk_label_new(type.label().c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget* row = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    std::string description = type.description();
    if (!description.empty()) {
        gtk_widget_set_tooltip_text(row, description.c_str());
    }
    g_object_set_data_full(
        G_OBJECT(row), "regui-type-name", g_strdup(type.name().c_str()), g_free);
    g_object_set_data_full(
        G_OBJECT(row), "regui-type-label", g_strdup(toLower(type.label()).c_str()), g_free);
    return row;
}

} // namespace

void WidgetTypePickerDialog::show(
    GtkWindow* parent, const model::ReAddonRegistry& registry, std::function<void(std::string)> onChosen) {
    auto* state = new State();
    state->onChosen = std::move(onChosen);

    GtkWidget* dialog = gtk_window_new();
    state->dialog = dialog;
    gtk_window_set_title(GTK_WINDOW(dialog), "New Widget");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 480);
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

    GtkWidget* listContainer = buildList(state, registry);
    gtk_widget_set_vexpand(listContainer, TRUE);
    gtk_box_append(GTK_BOX(rootBox), listContainer);

    GtkWidget* buttonRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttonRow, GTK_ALIGN_END);
    GtkWidget* cancelButton = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancelButton, "clicked", G_CALLBACK(onCancelClicked), state);
    gtk_box_append(GTK_BOX(buttonRow), cancelButton);
    GtkWidget* createButton = gtk_button_new_with_label("Create");
    gtk_widget_add_css_class(createButton, "suggested-action");
    g_signal_connect(createButton, "clicked", G_CALLBACK(onCreateClicked), state);
    gtk_box_append(GTK_BOX(buttonRow), createButton);
    gtk_box_append(GTK_BOX(rootBox), buttonRow);

    g_signal_connect(dialog, "destroy", G_CALLBACK(onDialogDestroy), state);

    gtk_window_present(GTK_WINDOW(dialog));
    centerWindowOnScreen(GTK_WINDOW(dialog));
}

GtkWidget* WidgetTypePickerDialog::buildList(State* state, const model::ReAddonRegistry& registry) {
    std::vector<const model::WidgetTypeDef*> sorted;
    sorted.reserve(registry.widgetTypes().size());
    for (const model::WidgetTypeDef& type : registry.widgetTypes()) {
        sorted.push_back(&type);
    }
    std::sort(sorted.begin(), sorted.end(), [](const model::WidgetTypeDef* a, const model::WidgetTypeDef* b) {
        return a->label() < b->label();
    });

    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget* searchEntry = gtk_search_entry_new();
    gtk_widget_set_tooltip_text(searchEntry, "Filter widget types by name");
    g_signal_connect(searchEntry, "search-changed", G_CALLBACK(onSearchChanged), state);
    gtk_box_append(GTK_BOX(container), searchEntry);

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(container), scroller);

    GtkWidget* listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_BROWSE);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(listBox), filterRow, state, nullptr);
    g_signal_connect(listBox, "row-activated", G_CALLBACK(onRowActivated), state);
    state->listBox = listBox;

    GtkListBoxRow* firstRow = nullptr;
    for (const model::WidgetTypeDef* type : sorted) {
        GtkWidget* row = makeRow(*type);
        gtk_list_box_append(GTK_LIST_BOX(listBox), row);
        if (!firstRow) {
            firstRow = GTK_LIST_BOX_ROW(row);
        }
    }
    if (firstRow) {
        gtk_list_box_select_row(GTK_LIST_BOX(listBox), firstRow);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), listBox);
    gtk_widget_grab_focus(searchEntry);
    return container;
}

gboolean WidgetTypePickerDialog::filterRow(GtkListBoxRow* row, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    if (state->searchQuery.empty()) {
        return TRUE;
    }
    const char* label = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "regui-type-label"));
    return label && std::string(label).find(state->searchQuery) != std::string::npos;
}

void WidgetTypePickerDialog::onSearchChanged(GtkSearchEntry* entry, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    state->searchQuery = toLower(gtk_editable_get_text(GTK_EDITABLE(entry)));
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(state->listBox));
}

void WidgetTypePickerDialog::onRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    const char* name = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "regui-type-name"));
    if (name && state->onChosen) {
        state->onChosen(name);
    }
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

void WidgetTypePickerDialog::onCancelClicked(GtkButton*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

void WidgetTypePickerDialog::onCreateClicked(GtkButton*, gpointer userData) {
    auto* state = static_cast<State*>(userData);
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->listBox));
    const char* name = row ? static_cast<const char*>(g_object_get_data(G_OBJECT(row), "regui-type-name")) : nullptr;
    if (name && state->onChosen) {
        state->onChosen(name);
    }
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

void WidgetTypePickerDialog::onDialogDestroy(GtkWidget*, gpointer userData) {
    delete static_cast<State*>(userData);
}

} // namespace regui::ui
