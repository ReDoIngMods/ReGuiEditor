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
#include "ui/detachable_panel.h"
#include "ui/hierarchy_panel.h"
#include "ui/history_window.h"
#include "ui/layout_settings.h"
#include "ui/properties_panel.h"

namespace regui::ui {

using DockEdge = layout_settings::DockEdge;
// The 4 edges plus the "float it" zone in the middle, used only while
// classifying where a drag is currently hovering.
enum class DockZone { Left, Right, Top, Bottom, Center };

// The application's main window: header bar (File/Edit menus plus canvas
// display controls), a hierarchy tree on the left, the MyGUI canvas in the
// center, and the properties panel on the right.
class MainWindow {
public:
    // `guiAssetsPath` is Scrap Mechanic's own "Data/Gui" folder (resolved by
    // ui::asset_setup before this is ever constructed). `initialPath`, if
    // non-empty, is loaded immediately.
    explicit MainWindow(GtkApplication* app, std::string guiAssetsPath, const std::string& initialPath = {});

    GtkWidget* window() const { return mWindow; }

    // Forces the canvas to realize now, running its one-time asset load
    // synchronously with `onProgress` fired throughout - call once, before
    // window() is presented, so main.cpp can drive a LoadingWindow.
    void loadAssets(std::function<void(const canvas::LoadProgress&)> onProgress);

private:
    GtkWidget* mWindow = nullptr;

    // Dock skeleton: 4 nested GtkPaned wrapping the canvas, each contributing
    // one edge slot. A slot's paned child is nullptr when nothing docks
    // there. See setSlotChild()/DockEdge.
    //   mDockMidHorizontalInner (H): start=left slot,  end=canvas
    //   mDockMidHorizontalOuter (H): start=inner,       end=right slot
    //   mDockMidVertical        (V): start=top slot,    end=outer-H
    //   mDockOuterVertical      (V): start=mid-V,        end=bottom slot
    GtkWidget* mDockMidHorizontalInner = nullptr;
    GtkWidget* mDockMidHorizontalOuter = nullptr;
    GtkWidget* mDockMidVertical = nullptr;
    GtkWidget* mDockOuterVertical = nullptr;
    // Overlays the whole dock skeleton so the 5 drop-zone indicators can be
    // drawn above everything (docked panels and canvas alike) while dragging.
    GtkWidget* mDockOverlay = nullptr;
    GtkWidget* mDockIndicatorLeft = nullptr;
    GtkWidget* mDockIndicatorRight = nullptr;
    GtkWidget* mDockIndicatorTop = nullptr;
    GtkWidget* mDockIndicatorBottom = nullptr;
    GtkWidget* mDockIndicatorCenter = nullptr; // "drop here to float"
    GtkWidget* mZoomLabel = nullptr; // header-bar button showing "<N>%"; click resets zoom
    std::unique_ptr<HistoryWindow> mHistoryWindow; // created lazily on first Edit > History

    std::unique_ptr<canvas::MyGuiCanvas> mCanvas;
    std::unique_ptr<model::ReLayoutDocument> mDocument;
    std::unique_ptr<HierarchyPanel> mHierarchyPanel;
    std::unique_ptr<PropertiesPanel> mPropertiesPanel;
    // Wrap the two side panels so either can be dragged to dock at any edge
    // or dropped in the middle to float in its own always-on-top window;
    // widget() is what actually lives in a dock slot (see mDock* above).
    std::unique_ptr<DetachablePanel> mHierarchyDetach;
    std::unique_ptr<DetachablePanel> mPropertiesDetach;
    // Current (or, while floating, "last/next") dock edge for each panel.
    DockEdge mHierarchyEdge = DockEdge::Left;
    DockEdge mPropertiesEdge = DockEdge::Right;
    gulong mFirstMapHandlerId = 0; // restores saved floating state once the window is actually shown
    // Read once by onFirstMap(), since the LayoutSettings this came from is a constructor-local.
    bool mPendingHierarchyFloating = false;
    bool mPendingPropertiesFloating = false;
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
    GtkWidget* buildDockArea(const layout_settings::LayoutSettings& settings);
    void rebuildHierarchyPanel();
    // Captures current window/paned/panel-float geometry and writes it out - see layout_settings.h.
    void saveLayoutSettings();

    // Docking policy - the only place that decides where a panel actually
    // goes; both the drag-and-drop drop handler and floating-window
    // close-request routing funnel through these.
    void setSlotChild(DockEdge edge, GtkWidget* widget);
    // Edge's current size along its own axis (width for Left/Right, height
    // for Top/Bottom) - what setSlotSize() would need to reproduce it.
    int slotSize(DockEdge edge);
    void setSlotSize(DockEdge edge, int size);
    // `panel`'s current size along `wantWidth`'s axis if that's a size it
    // actually has right now (its current slot's matching axis, or its
    // floating geometry) - -1 if not applicable (e.g. docked on the other axis).
    int panelSizeIfApplicable(DetachablePanel* panel, bool wantWidth);
    DockEdge& edgeFieldFor(DetachablePanel* panel);
    DetachablePanel* otherPanel(DetachablePanel* panel);
    // Docks `panel` at `edge`, swapping with whatever else is already there if occupied.
    void setPanelEdge(DetachablePanel* panel, DockEdge edge);
    // Undocks `panel` into a floating window (no-op if already floating).
    void setPanelFloating(DetachablePanel* panel);
    void showDockIndicator(DockZone zone);
    void hideDockIndicators();
    // Updates mZoomLabel's text to reflect `zoom` (e.g. 1.25 -> "125%").
    void updateZoomLabel(double zoom);
    // Reparents `dragged` under `newParent` (nullptr = root), refreshing
    // dependent views and pushing an undo/redo pair.
    void reparentNode(model::WidgetNode* dragged, model::WidgetNode* newParent);
    void loadDocument(const std::string& path);
    void loadAddonsFromDirectory(const std::string& directory);
    void undo();
    void redo();
    // Edit > History - lazily creates mHistoryWindow, then presents it.
    void showHistoryWindow();
    // HistoryWindow's callback: jumps to the state right after the clicked
    // entry (Undo tab: `displayIndex` entries get undone; Redo tab:
    // `displayIndex + 1` get redone), then syncs selection/dirty state same as undo()/redo().
    void jumpToHistoryEntry(bool isRedoTab, size_t displayIndex);

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
    static void onZoomInClicked(GtkButton* button, gpointer userData);
    static void onZoomOutClicked(GtkButton* button, gpointer userData);
    static void onZoomResetClicked(GtkButton* button, gpointer userData);
    // Fired on the OS-level window-close request (X button, Alt+F4, ...);
    // shows the unsaved-changes prompt and blocks the close until it resolves.
    static gboolean onCloseRequest(GtkWindow* window, gpointer userData);
    static void onCloseConfirmChoice(GObject* source, GAsyncResult* result, gpointer userData);
    // One-shot: applies the saved floating-panel state once the window is actually mapped,
    // so a restored floating panel never appears before the main window itself does.
    static void onFirstMap(GtkWidget* window, gpointer userData);
    // Dock-zone drop target on mDockOverlay - shared logic lives in updateDockIndicatorForPosition().
    static GdkDragAction onDockDragEnter(GtkDropTarget* target, double x, double y, gpointer userData);
    static GdkDragAction onDockDragMotion(GtkDropTarget* target, double x, double y, gpointer userData);
    static void onDockDragLeave(GtkDropTarget* target, gpointer userData);
    static gboolean onDockDrop(GtkDropTarget* target, const GValue* value, double x, double y, gpointer userData);
    void updateDockIndicatorForPosition(double x, double y);
    static gboolean onKeyPressed(
        GtkEventControllerKey* controller,
        guint keyval,
        guint keycode,
        GdkModifierType state,
        gpointer userData);
};

} // namespace regui::ui
