#pragma once

#include <gtk/gtk.h>

#include <functional>

#include "model/undo_stack.h"

namespace regui::ui {

// A non-modal "Action History" window (Edit > History) listing every
// undo/redo-able action, most recent first, each with an optional expandable
// detail line and a "Jump Here" button. Stays alive for MainWindow's
// lifetime once first opened - closing it just hides it, so its UndoStack
// subscription (kept live so the list refreshes as the user keeps editing)
// never has to be torn down.
class HistoryWindow {
public:
    // `onJump(isRedoTab, displayIndex)` fires when "Jump Here" is clicked on
    // a row - displayIndex is that row's most-recent-first position within
    // its own tab (0 = the top row). The caller decides what that means for
    // the underlying UndoStack (see MainWindow::jumpToHistoryEntry()).
    HistoryWindow(GtkWindow* parent, model::UndoStack* undoStack, std::function<void(bool, size_t)> onJump);

    // Shows the window, raising it if already open.
    void present();

private:
    void rebuild();
    void populateList(GtkWidget* box, const std::vector<model::UndoStack::Entry>& entries, bool isRedoTab);

    static gboolean onCloseRequest(GtkWindow* window, gpointer userData);
    static void onJumpButtonClicked(GtkButton* button, gpointer userData);

    GtkWidget* mWindow = nullptr;
    GtkWidget* mUndoBox = nullptr;
    GtkWidget* mRedoBox = nullptr;
    model::UndoStack* mUndoStack = nullptr;
    std::function<void(bool, size_t)> mOnJump;
};

} // namespace regui::ui
