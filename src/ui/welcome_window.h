#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace regui::ui {

// Startup screen shown before any game asset is touched: "New ReLayout" /
// "Open ReLayout..." plus a Recent Files list.
class WelcomeWindow {
public:
    // `onFileChosen` fires once with the chosen path ("Open ReLayout..." or a recent file);
    // `onNewRequested` fires once, with nothing to load, for "New ReLayout". Neither
    // closes/hides the window itself - the caller does.
    WelcomeWindow(
        GtkApplication* app, std::function<void(std::string)> onFileChosen,
        std::function<void()> onNewRequested);

    GtkWidget* window() const { return mWindow; }

private:
    GtkWidget* mWindow = nullptr;
    std::function<void(std::string)> mOnFileChosen;
    std::function<void()> mOnNewRequested;

    GtkWidget* buildRecentsList();

    static void onNewClicked(GtkButton* button, gpointer userData);
    static void onOpenClicked(GtkButton* button, gpointer userData);
    static void onOpenFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData);
    static void onRecentRowActivated(GtkListBox* box, GtkListBoxRow* row, gpointer userData);
};

} // namespace regui::ui
