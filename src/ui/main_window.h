#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "canvas/mygui_canvas.h"
#include "model/addon.h"
#include "model/layout.h"
#include "model/undo_stack.h"
#include "ui/hierarchy_panel.h"
#include "ui/properties_panel.h"

namespace regui::ui {

// The application's main window: header bar (File/Edit menus plus canvas
// display controls), a hierarchy tree on the left, the MyGUI canvas in the
// center, and the properties panel on the right.
class MainWindow {
public:
    // `initialPath`, if non-empty, is loaded immediately.
    explicit MainWindow(GtkApplication* app, const std::string& initialPath = {});

    GtkWidget* window() const { return mWindow; }

    // Forces the canvas to realize now, running its one-time asset load
    // synchronously with `onProgress` fired throughout - call once, before
    // window() is presented, so main.cpp can drive a LoadingWindow.
    void loadAssets(std::function<void(const canvas::LoadProgress&)> onProgress);

private:
    GtkWidget* mWindow = nullptr;
    GtkWidget* mOuterPaned = nullptr;

    std::unique_ptr<canvas::MyGuiCanvas> mCanvas;
    std::unique_ptr<model::ReLayoutDocument> mDocument;
    std::unique_ptr<HierarchyPanel> mHierarchyPanel;
    std::unique_ptr<PropertiesPanel> mPropertiesPanel;
    model::ReAddonRegistry mRegistry;
    model::UndoStack mUndoStack;
    std::string mCurrentPath;
    std::optional<model::WidgetNode> mClipboard; // Ctrl+C / right-click "Copy" target; pasted by Ctrl+V

    bool mDirty = false; // unsaved changes since the last load/save; reflected in the titlebar
    // Runs once a pending Save As actually completes; used to defer closing
    // the window until the close-confirmation dialog's "Save" choice really lands on disk.
    std::function<void()> mPendingSaveAsCallback;

    // Refreshes the titlebar to reflect mCurrentPath/mDirty.
    void updateTitle();
    // Marks the document unsaved and updates the titlebar, if not already dirty.
    void markDirty();
    // A blocking "OK"-only alert (e.g. a save failure) - title is not a printf format string.
    void showErrorAlert(const std::string& title, const std::string& detail);

    GtkWidget* buildHeaderBar();
    void rebuildHierarchyPanel();
    // Reparents `dragged` under `newParent` (nullptr = root), refreshing
    // dependent views and pushing an undo/redo pair.
    void reparentNode(model::WidgetNode* dragged, model::WidgetNode* newParent);
    void loadDocument(const std::string& path);
    void loadAddonsFromDirectory(const std::string& directory);
    void undo();
    void redo();

    // Saves to mCurrentPath if known, else falls back to promptSaveAs().
    // `onSaved`, if given, runs only once the save actually succeeds.
    void save(std::function<void()> onSaved = {});
    // "Save As..." dialog (File menu, and save()'s fallback). `onSaved` as above.
    void promptSaveAs(std::function<void()> onSaved = {});
    // "Open..." dialog (File menu).
    void promptOpen();
    // "Open Addon..." dialog (File menu).
    void promptOpenAddon();
    // "About" dialog (File menu); see src/app_info.h.
    void showAboutDialog();
    // Deep-copies `node` into mClipboard (Ctrl+C / right-click "Copy").
    void copyNode(model::WidgetNode* node);
    // Inserts a copy of mClipboard as a new sibling of the selection (Ctrl+V), with undo/redo.
    void pasteClipboard();
    // Removes `node`, refreshing dependent views and pushing an undo/redo pair.
    void deleteNode(model::WidgetNode* node);

    static void onOpenFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData);
    static void onSaveAsFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData);
    static void onOpenAddonFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData);
    static void onResolutionChanged(GtkDropDown* dropdown, GParamSpec* pspec, gpointer userData);
    static void onGridSpacingChanged(GtkSpinButton* spin, gpointer userData);
    static void onGridEnabledToggled(GtkCheckButton* button, gpointer userData);
    // Fired on the OS-level window-close request (X button, Alt+F4, ...);
    // shows the unsaved-changes prompt and blocks the close until it resolves.
    static gboolean onCloseRequest(GtkWindow* window, gpointer userData);
    static void onCloseConfirmChoice(GObject* source, GAsyncResult* result, gpointer userData);
    static gboolean onKeyPressed(
        GtkEventControllerKey* controller,
        guint keyval,
        guint keycode,
        GdkModifierType state,
        gpointer userData);
};

} // namespace regui::ui
