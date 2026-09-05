#include "ui/welcome_window.h"

#include "ui/accent_style.h"
#include "ui/recent_files.h"

#include <filesystem>

namespace regui::ui {

namespace {

std::string baseName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

} // namespace

WelcomeWindow::WelcomeWindow(
    GtkApplication* app, std::function<void(std::string)> onFileChosen, std::function<void()> onNewRequested)
    : mOnFileChosen(std::move(onFileChosen)), mOnNewRequested(std::move(onNewRequested)) {
    mWindow = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(mWindow), "Welcome to ReGui Editor");
    gtk_window_set_default_size(GTK_WINDOW(mWindow), 460, 520);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(root, 28);
    gtk_widget_set_margin_end(root, 28);
    gtk_widget_set_margin_top(root, 28);
    gtk_widget_set_margin_bottom(root, 20);
    gtk_window_set_child(GTK_WINDOW(mWindow), root);

    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>ReGui Editor</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(root), title);

    GtkWidget* subtitle = gtk_label_new("The layout editor for sm.regui");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);
    gtk_widget_add_css_class(subtitle, "dim-label");
    gtk_widget_set_margin_bottom(subtitle, 12);
    gtk_box_append(GTK_BOX(root), subtitle);

    GtkWidget* actionsRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(actionsRow), TRUE);

    GtkWidget* newButton = gtk_button_new_with_label("New ReLayout");
    gtk_widget_add_css_class(newButton, "suggested-action");
    gtk_widget_add_css_class(newButton, "pill");
    applyAccentButtonStyle(newButton);
    g_signal_connect(newButton, "clicked", G_CALLBACK(onNewClicked), this);
    gtk_box_append(GTK_BOX(actionsRow), newButton);

    GtkWidget* openButton = gtk_button_new_with_label("Open ReLayout\xE2\x80\xA6");
    gtk_widget_add_css_class(openButton, "suggested-action");
    gtk_widget_add_css_class(openButton, "pill");
    applyAccentButtonStyle(openButton);
    g_signal_connect(openButton, "clicked", G_CALLBACK(onOpenClicked), this);
    gtk_box_append(GTK_BOX(actionsRow), openButton);

    gtk_box_append(GTK_BOX(root), actionsRow);

    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget* recentsLabel = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(recentsLabel), "<b>Recent Files</b>");
    gtk_label_set_xalign(GTK_LABEL(recentsLabel), 0.0f);
    gtk_widget_set_margin_top(recentsLabel, 8);
    gtk_box_append(GTK_BOX(root), recentsLabel);

    GtkWidget* recentsList = buildRecentsList();
    gtk_widget_set_vexpand(recentsList, TRUE);
    gtk_box_append(GTK_BOX(root), recentsList);
}

GtkWidget* WelcomeWindow::buildRecentsList() {
    std::vector<std::string> recents = recent_files::load();
    if (recents.empty()) {
        GtkWidget* placeholder = gtk_label_new("No recent files yet");
        gtk_widget_add_css_class(placeholder, "dim-label");
        gtk_widget_set_valign(placeholder, GTK_ALIGN_START);
        gtk_widget_set_margin_top(placeholder, 8);
        return placeholder;
    }

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(listBox, "boxed-list");

    for (const std::string& path : recents) {
        GtkWidget* row = gtk_list_box_row_new();

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_widget_set_margin_top(box, 6);
        gtk_widget_set_margin_bottom(box, 6);

        GtkWidget* nameLabel = gtk_label_new(baseName(path).c_str());
        gtk_label_set_xalign(GTK_LABEL(nameLabel), 0.0f);
        gtk_box_append(GTK_BOX(box), nameLabel);

        GtkWidget* pathLabel = gtk_label_new(path.c_str());
        gtk_label_set_xalign(GTK_LABEL(pathLabel), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(pathLabel), PANGO_ELLIPSIZE_MIDDLE);
        gtk_widget_add_css_class(pathLabel, "dim-label");
        gtk_widget_add_css_class(pathLabel, "caption");
        gtk_box_append(GTK_BOX(box), pathLabel);

        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "regui-path", g_strdup(path.c_str()), g_free);
        gtk_list_box_append(GTK_LIST_BOX(listBox), row);
    }

    g_signal_connect(listBox, "row-activated", G_CALLBACK(onRecentRowActivated), this);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), listBox);
    return scroller;
}

void WelcomeWindow::onNewClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<WelcomeWindow*>(userData);
    if (self->mOnNewRequested) {
        self->mOnNewRequested();
    }
}

void WelcomeWindow::onOpenClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<WelcomeWindow*>(userData);

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open ReLayout");

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "ReLayout files");
    gtk_file_filter_add_pattern(filter, "*.relayout");
    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    gtk_file_dialog_open(dialog, GTK_WINDOW(self->mWindow), nullptr, &WelcomeWindow::onOpenFileDialogReady, self);
    g_object_unref(dialog);
}

void WelcomeWindow::onOpenFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* self = static_cast<WelcomeWindow*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        if (error) {
            g_error_free(error); // includes user-cancelled, not necessarily a real failure
        }
        return;
    }
    if (char* path = g_file_get_path(file)) {
        if (self->mOnFileChosen) {
            self->mOnFileChosen(path);
        }
        g_free(path);
    }
    g_object_unref(file);
}

void WelcomeWindow::onRecentRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer userData) {
    auto* self = static_cast<WelcomeWindow*>(userData);
    const char* path = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "regui-path"));
    if (path && self->mOnFileChosen) {
        self->mOnFileChosen(path);
    }
}

} // namespace regui::ui
