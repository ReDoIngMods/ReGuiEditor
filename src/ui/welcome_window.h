#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace regui::ui {

// Startup screen shown before any game asset is touched: "Open File..." plus
// a Recent Files list.
class WelcomeWindow {
public:
    // Fires once with the chosen path. Doesn't close/hide itself - the caller does.
    WelcomeWindow(GtkApplication* app, std::function<void(std::string)> onFileChosen);

    GtkWidget* window() const { return mWindow; }

private:
    GtkWidget* mWindow = nullptr;
    std::function<void(std::string)> mOnFileChosen;

    GtkWidget* buildRecentsList();

    static void onOpenClicked(GtkButton* button, gpointer userData);
    static void onOpenFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData);
    static void onRecentRowActivated(GtkListBox* box, GtkListBoxRow* row, gpointer userData);
};

} // namespace regui::ui
