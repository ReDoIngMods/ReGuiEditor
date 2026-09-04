#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <unordered_map>
#include <vector>

#include "model/addon.h"
#include "model/layout.h"

namespace regui::ui {

// An Explorer-style tree view of a ReLayout document's WidgetNode forest.
// Holds non-owning pointers into the tree, so the document must outlive the
// panel, and a new document means rebuilding a new panel, not mutating this one.
class HierarchyPanel {
public:
    // `registry` looks up each row's widget-type icon; must outlive the panel.
    HierarchyPanel(std::vector<model::WidgetNode>& roots, const model::ReAddonRegistry& registry);
    ~HierarchyPanel();

    HierarchyPanel(const HierarchyPanel&) = delete;
    HierarchyPanel& operator=(const HierarchyPanel&) = delete;

    // Owned by the GtkScrolledWindow once added to a container; don't free manually.
    GtkWidget* widget() const { return mScrolledWindow; }

    // Fired on user selection change (including to null). Not fired by selectNode().
    void setOnSelectionChanged(std::function<void(model::WidgetNode*)> callback);

    // Selects `node`'s row (nullptr clears) without firing the callback above.
    void selectNode(model::WidgetNode* node);

    // Fired on drag-drop reparent (newParent = nullptr for root level).
    // Mutating the tree and rebuilding dependent views is the caller's job.
    void setOnReparent(std::function<void(model::WidgetNode* dragged, model::WidgetNode* newParent)> callback) {
        mOnReparent = std::move(callback);
    }

    // Right-click-on-a-row menu ("Delete" / "Copy"). Right-click also selects the row first.
    void setOnDeleteRequested(std::function<void(model::WidgetNode*)> callback) {
        mOnDeleteRequested = std::move(callback);
    }
    void setOnCopyRequested(std::function<void(model::WidgetNode*)> callback) { mOnCopyRequested = std::move(callback); }

    // Right-click-on-empty-space menu ("New" / "Insert"). "New" is
    // deliberately left unset by MainWindow for now - it needs a widget-type
    // picker that doesn't exist yet, so it's a no-op beyond closing the menu
    // until a caller wires it up; "Insert" pastes the clipboard.
    void setOnNewRequested(std::function<void()> callback) { mOnNewRequested = std::move(callback); }
    void setOnInsertRequested(std::function<void()> callback) { mOnInsertRequested = std::move(callback); }

private:
    GtkWidget* mScrolledWindow = nullptr;
    GtkSelectionModel* mSelectionModel = nullptr; // owned by the list view
    const model::ReAddonRegistry* mRegistry = nullptr;
    std::unordered_map<std::string, GdkTexture*> mIconCache; // widget type name -> decoded row icon (nullptr = none)
    std::function<void(model::WidgetNode*)> mOnSelectionChanged;
    std::function<void(model::WidgetNode*, model::WidgetNode*)> mOnReparent;
    std::function<void(model::WidgetNode*)> mOnDeleteRequested;
    std::function<void(model::WidgetNode*)> mOnCopyRequested;
    std::function<void()> mOnNewRequested;
    std::function<void()> mOnInsertRequested;
    bool mSuppressCallback = false;

    // Decoded/downscaled row icon for `type`, cached by name - nullptr if none.
    GdkTexture* iconForType(const std::string& type);

    void showObjectMenu(GtkWidget* anchor, double x, double y, model::WidgetNode* node);
    void showEmptySpaceMenu(GtkWidget* anchor, double x, double y);

    static void onSelectionChangedThunk(GtkSelectionModel* model, guint position, guint nItems, gpointer userData);

    // Factory callbacks: members (not free functions) so they can wire up
    // onDragPrepare/onRowDrop/onRowRightClick below on each row.
    static void onFactorySetup(GtkListItemFactory* factory, GtkListItem* listItem, gpointer userData);
    static void onFactoryBind(GtkListItemFactory* factory, GtkListItem* listItem, gpointer userData);

    static GdkContentProvider* onDragPrepare(GtkDragSource* source, double x, double y, gpointer userData);
    static gboolean onRowDrop(GtkDropTarget* target, const GValue* value, double x, double y, gpointer userData);
    static gboolean onRootDrop(GtkDropTarget* target, const GValue* value, double x, double y, gpointer userData);

    static void onRowRightClick(GtkGestureClick* gesture, gint nPress, gdouble x, gdouble y, gpointer userData);
    static void onEmptySpaceRightClick(GtkGestureClick* gesture, gint nPress, gdouble x, gdouble y, gpointer userData);
    static void onRowMenuItemClicked(GtkButton* button, gpointer userData);
    static void onEmptyMenuItemClicked(GtkButton* button, gpointer userData);
};

} // namespace regui::ui
