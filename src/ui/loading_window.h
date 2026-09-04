#pragma once

#include <gtk/gtk.h>

#include "canvas/mygui_canvas.h"

namespace regui::ui {

// Shown between picking a file and the full editor appearing, while
// MainWindow::loadAssets() runs synchronously - shows live progress instead
// of the app looking frozen.
class LoadingWindow {
public:
    explicit LoadingWindow(GtkApplication* app);

    GtkWidget* window() const { return mWindow; }

    // Updates the labels/progress bar and pumps the main loop once so it repaints.
    void update(const canvas::LoadProgress& progress);

private:
    GtkWidget* mWindow = nullptr;
    GtkWidget* mWhatLabel = nullptr;
    GtkWidget* mWhereLabel = nullptr;
    GtkWidget* mWhoLabel = nullptr;
    GtkWidget* mProgressBar = nullptr;
};

} // namespace regui::ui
