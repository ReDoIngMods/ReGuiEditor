#include "hierarchy_panel.h"

#include "ui/accent_style.h"

#include <string>

namespace regui::ui {

namespace {

// Fixed small size regardless of the source image's own resolution.
constexpr int kIconDisplaySize = 20;

// Wraps a non-owning WidgetNode* as a GObject, since GtkTreeListModel needs one.
G_DECLARE_FINAL_TYPE(ReguiWidgetItem, regui_widget_item, REGUI, WIDGET_ITEM, GObject)

struct _ReguiWidgetItem {
    GObject parent_instance;
    // GLib zero-initializes new instances (no C++ constructor runs), which
    // is a valid null pointer on every platform this project targets.
    model::WidgetNode* node;
};

G_DEFINE_TYPE(ReguiWidgetItem, regui_widget_item, G_TYPE_OBJECT)

void regui_widget_item_class_init(ReguiWidgetItemClass*) {}
void regui_widget_item_init(ReguiWidgetItem*) {}

ReguiWidgetItem* regui_widget_item_new(model::WidgetNode* node) {
    auto* item = REGUI_WIDGET_ITEM(g_object_new(regui_widget_item_get_type(), nullptr));
    item->node = node;
    return item;
}

GListStore* makeChildStore(std::vector<model::WidgetNode>& children) {
    GListStore* store = g_list_store_new(regui_widget_item_get_type());
    for (auto& child : children) {
        ReguiWidgetItem* item = regui_widget_item_new(&child);
        g_list_store_append(store, item);
        g_object_unref(item);
    }
    return store;
}

// GtkTreeListModelCreateModelFunc: returns the GListModel of `item`'s
// children, or nullptr if it's a leaf.
GListModel* createChildModel(gpointer item, gpointer) {
    auto* widgetItem = REGUI_WIDGET_ITEM(item);
    if (widgetItem->node->children.empty()) {
        return nullptr;
    }
    return G_LIST_MODEL(makeChildStore(widgetItem->node->children));
}

// Decodes a base64 icon and scales it down to kIconDisplaySize.
GdkTexture* decodeRowIcon(const std::string& base64) {
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

// Freed automatically when its button is (see g_object_set_data_full below).
enum class RowMenuAction { Delete, Copy };
struct RowMenuContext {
    HierarchyPanel* self;
    GtkWidget* popover;
    model::WidgetNode* node;
    RowMenuAction action;
};

enum class EmptyMenuAction { New, Insert };
struct EmptyMenuContext {
    HierarchyPanel* self;
    GtkWidget* popover;
    EmptyMenuAction action;
};

void closePopoverOnClosed(GtkPopover* popover, gpointer) { gtk_widget_unparent(GTK_WIDGET(popover)); }

} // namespace

void HierarchyPanel::onFactorySetup(GtkListItemFactory*, GtkListItem* listItem, gpointer userData) {
    GtkWidget* expander = gtk_tree_expander_new();

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* icon = gtk_image_new();
    gtk_widget_set_size_request(icon, kIconDisplaySize, kIconDisplaySize);
    gtk_box_append(GTK_BOX(row), icon);
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_append(GTK_BOX(row), label);
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), row);
    g_object_set_data(G_OBJECT(expander), "regui-icon-widget", icon);
    g_object_set_data(G_OBJECT(expander), "regui-label-widget", label);

    gtk_list_item_set_child(listItem, expander);

    // Each row is a drag source and drop target for reparenting. Node
    // pointers are read off "regui-node", kept current by onFactoryBind
    // (rows are recycled as the list scrolls).
    GtkDragSource* dragSource = gtk_drag_source_new();
    gtk_drag_source_set_actions(dragSource, GDK_ACTION_MOVE);
    g_signal_connect(dragSource, "prepare", G_CALLBACK(onDragPrepare), userData);
    gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(dragSource));

    GtkDropTarget* dropTarget = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
    g_signal_connect(dropTarget, "drop", G_CALLBACK(onRowDrop), userData);
    gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(dropTarget));

    // Selects the row and shows the Delete/Copy menu; claims the sequence
    // so it doesn't bubble to the empty-space menu.
    GtkGesture* rightClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rightClick), GDK_BUTTON_SECONDARY);
    g_signal_connect(rightClick, "pressed", G_CALLBACK(onRowRightClick), userData);
    gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(rightClick));
}

void HierarchyPanel::onFactoryBind(GtkListItemFactory*, GtkListItem* listItem, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    auto* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(listItem));
    GtkWidget* expander = gtk_list_item_get_child(listItem);
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);

    auto* item = REGUI_WIDGET_ITEM(gtk_tree_list_row_get_item(row));
    GtkWidget* label = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(expander), "regui-label-widget"));
    GtkWidget* icon = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(expander), "regui-icon-widget"));
    gtk_label_set_text(GTK_LABEL(label), item->node->name.empty() ? "(unnamed)" : item->node->name.c_str());
    GdkTexture* texture = self->iconForType(item->node->type);
    gtk_image_set_from_paintable(GTK_IMAGE(icon), texture ? GDK_PAINTABLE(texture) : nullptr);
    g_object_set_data(G_OBJECT(expander), "regui-node", item->node);
    g_object_unref(item); // gtk_tree_list_row_get_item() returns a new reference
}

GdkTexture* HierarchyPanel::iconForType(const std::string& type) {
    auto it = mIconCache.find(type);
    if (it != mIconCache.end()) {
        return it->second;
    }
    GdkTexture* texture = nullptr;
    if (mRegistry) {
        if (const model::WidgetTypeDef* def = mRegistry->find(type)) {
            texture = decodeRowIcon(def->icon());
        }
    }
    mIconCache.emplace(type, texture);
    return texture;
}

GdkContentProvider* HierarchyPanel::onDragPrepare(GtkDragSource* source, double, double, gpointer) {
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(source));
    auto* node = static_cast<model::WidgetNode*>(g_object_get_data(G_OBJECT(widget), "regui-node"));
    if (!node) {
        return nullptr;
    }
    return gdk_content_provider_new_typed(G_TYPE_POINTER, node);
}

gboolean HierarchyPanel::onRowDrop(GtkDropTarget* target, const GValue* value, double, double, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    if (!self->mOnReparent) {
        return FALSE;
    }
    auto* dragged = static_cast<model::WidgetNode*>(g_value_get_pointer(value));
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
    auto* targetNode = static_cast<model::WidgetNode*>(g_object_get_data(G_OBJECT(widget), "regui-node"));
    if (!dragged || !targetNode) {
        return FALSE;
    }
    self->mOnReparent(dragged, targetNode);
    return TRUE;
}

gboolean HierarchyPanel::onRootDrop(GtkDropTarget*, const GValue* value, double, double, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    if (!self->mOnReparent) {
        return FALSE;
    }
    auto* dragged = static_cast<model::WidgetNode*>(g_value_get_pointer(value));
    if (!dragged) {
        return FALSE;
    }
    self->mOnReparent(dragged, nullptr);
    return TRUE;
}

void HierarchyPanel::onRowRightClick(GtkGestureClick* gesture, gint, gdouble x, gdouble y, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    auto* node = static_cast<model::WidgetNode*>(g_object_get_data(G_OBJECT(widget), "regui-node"));
    if (!node) {
        return;
    }
    // Stops this press from also reaching the empty-space-menu gesture.
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    self->selectNode(node);
    if (self->mOnSelectionChanged) {
        self->mOnSelectionChanged(node);
    }
    self->showObjectMenu(widget, x, y, node);
}

void HierarchyPanel::onEmptySpaceRightClick(GtkGestureClick*, gint, gdouble x, gdouble y, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    self->showEmptySpaceMenu(self->mScrolledWindow, x, y);
}

void HierarchyPanel::onRowMenuItemClicked(GtkButton*, gpointer userData) {
    auto* ctx = static_cast<RowMenuContext*>(userData);
    switch (ctx->action) {
        case RowMenuAction::Delete:
            if (ctx->self->mOnDeleteRequested) {
                ctx->self->mOnDeleteRequested(ctx->node);
            }
            break;
        case RowMenuAction::Copy:
            if (ctx->self->mOnCopyRequested) {
                ctx->self->mOnCopyRequested(ctx->node);
            }
            break;
    }
    gtk_popover_popdown(GTK_POPOVER(ctx->popover));
}

void HierarchyPanel::onEmptyMenuItemClicked(GtkButton*, gpointer userData) {
    auto* ctx = static_cast<EmptyMenuContext*>(userData);
    switch (ctx->action) {
        case EmptyMenuAction::New:
            if (ctx->self->mOnNewRequested) {
                ctx->self->mOnNewRequested();
            }
            break;
        case EmptyMenuAction::Insert:
            if (ctx->self->mOnInsertRequested) {
                ctx->self->mOnInsertRequested();
            }
            break;
    }
    gtk_popover_popdown(GTK_POPOVER(ctx->popover));
}

void HierarchyPanel::showObjectMenu(GtkWidget* anchor, double x, double y, model::WidgetNode* node) {
    GtkWidget* popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE); // flat context menu, not a speech bubble
    gtk_widget_add_css_class(popover, "menu");
    gtk_widget_set_parent(popover, anchor);
    GdkRectangle rect{static_cast<int>(x), static_cast<int>(y), 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_signal_connect(popover, "closed", G_CALLBACK(closePopoverOnClosed), nullptr);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    struct { const char* label; RowMenuAction action; } items[] = {
        {"Delete", RowMenuAction::Delete},
        {"Copy", RowMenuAction::Copy},
    };
    for (const auto& entry : items) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        gtk_widget_add_css_class(button, "flat");
        gtk_box_append(GTK_BOX(box), button);
        auto* ctx = new RowMenuContext{this, popover, node, entry.action};
        g_object_set_data_full(
            G_OBJECT(button), "regui-menu-ctx", ctx, [](gpointer p) { delete static_cast<RowMenuContext*>(p); });
        g_signal_connect(button, "clicked", G_CALLBACK(onRowMenuItemClicked), ctx);
    }

    gtk_popover_popup(GTK_POPOVER(popover));
}

void HierarchyPanel::showEmptySpaceMenu(GtkWidget* anchor, double x, double y) {
    GtkWidget* popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE); // flat context menu, not a speech bubble
    gtk_widget_add_css_class(popover, "menu");
    gtk_widget_set_parent(popover, anchor);
    GdkRectangle rect{static_cast<int>(x), static_cast<int>(y), 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_signal_connect(popover, "closed", G_CALLBACK(closePopoverOnClosed), nullptr);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    struct { const char* label; EmptyMenuAction action; } items[] = {
        {"New\xE2\x80\xA6", EmptyMenuAction::New},
        {"Insert\xE2\x80\xA6", EmptyMenuAction::Insert},
    };
    for (const auto& entry : items) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        gtk_widget_add_css_class(button, "flat");
        gtk_box_append(GTK_BOX(box), button);
        auto* ctx = new EmptyMenuContext{this, popover, entry.action};
        g_object_set_data_full(
            G_OBJECT(button), "regui-menu-ctx", ctx, [](gpointer p) { delete static_cast<EmptyMenuContext*>(p); });
        g_signal_connect(button, "clicked", G_CALLBACK(onEmptyMenuItemClicked), ctx);
    }

    gtk_popover_popup(GTK_POPOVER(popover));
}

HierarchyPanel::HierarchyPanel(std::vector<model::WidgetNode>& roots, const model::ReAddonRegistry& registry)
    : mRegistry(&registry) {
    GListStore* rootStore = makeChildStore(roots);

    GtkTreeListModel* treeModel =
        gtk_tree_list_model_new(G_LIST_MODEL(rootStore), FALSE, TRUE, createChildModel, nullptr, nullptr);

    GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(treeModel));

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(onFactorySetup), this);
    g_signal_connect(factory, "bind", G_CALLBACK(onFactoryBind), this);

    GtkWidget* listView = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
    applyAccentRowSelectionStyle(listView);

    mSelectionModel = GTK_SELECTION_MODEL(selection);
    g_signal_connect(mSelectionModel, "selection-changed", G_CALLBACK(onSelectionChangedThunk), this);

    mScrolledWindow = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mScrolledWindow), listView);

    // Dropping below the rows moves the dragged widget to root level.
    GtkDropTarget* rootDropTarget = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
    g_signal_connect(rootDropTarget, "drop", G_CALLBACK(onRootDrop), this);
    gtk_widget_add_controller(mScrolledWindow, GTK_EVENT_CONTROLLER(rootDropTarget));

    // Row right-clicks claim their own sequence, so this only fires for empty space.
    GtkGesture* emptyRightClick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(emptyRightClick), GDK_BUTTON_SECONDARY);
    g_signal_connect(emptyRightClick, "pressed", G_CALLBACK(onEmptySpaceRightClick), this);
    gtk_widget_add_controller(mScrolledWindow, GTK_EVENT_CONTROLLER(emptyRightClick));
}

HierarchyPanel::~HierarchyPanel() {
    for (auto& [type, texture] : mIconCache) {
        if (texture) {
            g_object_unref(texture);
        }
    }
}

void HierarchyPanel::setOnSelectionChanged(std::function<void(model::WidgetNode*)> callback) {
    mOnSelectionChanged = std::move(callback);
}

void HierarchyPanel::selectNode(model::WidgetNode* node) {
    if (!mSelectionModel) {
        return;
    }
    mSuppressCallback = true;
    if (!node) {
        gtk_selection_model_unselect_all(mSelectionModel);
    } else {
        guint n = g_list_model_get_n_items(G_LIST_MODEL(mSelectionModel));
        for (guint i = 0; i < n; ++i) {
            gpointer rowPtr = g_list_model_get_item(G_LIST_MODEL(mSelectionModel), i);
            auto* row = GTK_TREE_LIST_ROW(rowPtr);
            gpointer itemPtr = gtk_tree_list_row_get_item(row);
            bool matches = REGUI_WIDGET_ITEM(itemPtr)->node == node;
            g_object_unref(itemPtr);
            g_object_unref(rowPtr);
            if (matches) {
                gtk_selection_model_select_item(mSelectionModel, i, TRUE);
                break;
            }
        }
    }
    mSuppressCallback = false;
}

void HierarchyPanel::onSelectionChangedThunk(GtkSelectionModel*, guint, guint, gpointer userData) {
    auto* self = static_cast<HierarchyPanel*>(userData);
    if (self->mSuppressCallback || !self->mOnSelectionChanged) {
        return;
    }
    guint selected = gtk_single_selection_get_selected(GTK_SINGLE_SELECTION(self->mSelectionModel));
    if (selected == GTK_INVALID_LIST_POSITION) {
        self->mOnSelectionChanged(nullptr);
        return;
    }
    gpointer rowPtr = g_list_model_get_item(G_LIST_MODEL(self->mSelectionModel), selected);
    auto* row = GTK_TREE_LIST_ROW(rowPtr);
    gpointer itemPtr = gtk_tree_list_row_get_item(row);
    self->mOnSelectionChanged(REGUI_WIDGET_ITEM(itemPtr)->node);
    g_object_unref(itemPtr);
    g_object_unref(rowPtr);
}

} // namespace regui::ui
