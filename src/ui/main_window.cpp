#include "ui/main_window.h"

#include "app_info.h"
#include "ui/layout_settings.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace regui::ui {

namespace {

struct ResolutionPreset {
    const char* label;
    double width;
    double height;
};

// Common target screens to preview a layout against, via the canvas's reference resolution.
constexpr ResolutionPreset kResolutionPresets[] = {
    {"1280 x 720 (720p)", 1280, 720},
    {"1920 x 1080 (1080p)", 1920, 1080},
    {"2560 x 1440 (1440p / 2K)", 2560, 1440},
    {"3840 x 2160 (4K)", 3840, 2160},
    {"1920 x 1200 (16:10)", 1920, 1200},
    {"1600 x 1200 (4:3)", 1600, 1200},
    {"3440 x 1440 (21:9 Ultrawide)", 3440, 1440},
    {"5120 x 1440 (32:9 Super Ultrawide)", 5120, 1440},
};
constexpr int kDefaultResolutionIndex = 1; // 1920x1080

// Mirrors MyGuiCanvas's own default - buildHeaderBar() runs before mCanvas exists.
constexpr double kDefaultGridSpacing = 10.0;

// The window opens maximized, so the dock-slot defaults need the real screen
// size up front. Falls back to a plausible 1280x800 desktop.
GdkRectangle primaryMonitorGeometry() {
    GdkRectangle geometry{0, 0, 1280, 800};
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        return geometry;
    }
    GListModel* monitors = gdk_display_get_monitors(display);
    if (!monitors || g_list_model_get_n_items(monitors) == 0) {
        return geometry;
    }
    GdkMonitor* monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
    gdk_monitor_get_geometry(monitor, &geometry);
    g_object_unref(monitor);
    return geometry;
}

// How close to an edge (in px) a drag has to be to dock there; anything
// farther from every edge than this counts as the "float it" center zone.
constexpr double kDockEdgeBand = 100.0;

DockZone classifyDockZone(double x, double y, double width, double height) {
    double distances[] = {x, width - x, y, height - y}; // left, right, top, bottom
    int nearest = 0;
    for (int i = 1; i < 4; ++i) {
        if (distances[i] < distances[nearest]) {
            nearest = i;
        }
    }
    if (distances[nearest] > kDockEdgeBand) {
        return DockZone::Center;
    }
    switch (nearest) {
        case 0:
            return DockZone::Left;
        case 1:
            return DockZone::Right;
        case 2:
            return DockZone::Top;
        default:
            return DockZone::Bottom;
    }
}

DockEdge dockZoneToEdge(DockZone zone) {
    switch (zone) {
        case DockZone::Left:
            return DockEdge::Left;
        case DockZone::Right:
            return DockEdge::Right;
        case DockZone::Top:
            return DockEdge::Top;
        default:
            return DockEdge::Bottom;
    }
}

// A translucent highlight rect for one dock-zone indicator, sized/aligned to
// hug the edge it represents (or centered, for "float here").
GtkWidget* buildDockIndicator(GtkAlign halign, GtkAlign valign, int width, int height) {
    GtkWidget* indicator = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(indicator, "regui-dock-indicator");
    gtk_widget_set_halign(indicator, halign);
    gtk_widget_set_valign(indicator, valign);
    gtk_widget_set_size_request(indicator, width, height);
    gtk_widget_set_can_target(indicator, FALSE); // purely decorative - never steals the drop
    gtk_widget_set_visible(indicator, FALSE); // shown for the duration of a drag, not at rest
    gtk_widget_set_margin_start(indicator, 12);
    gtk_widget_set_margin_end(indicator, 12);
    gtk_widget_set_margin_top(indicator, 12);
    gtk_widget_set_margin_bottom(indicator, 12);
    return indicator;
}

void ensureDockCssLoaded() {
    static bool loaded = false;
    if (loaded) {
        return;
    }
    loaded = true;
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        provider,
        // All 5 zones show this outline for the whole drag, so the user can
        // see every option at once; only the currently-hovered one also gets
        // .regui-dock-indicator-active's fill.
        ".regui-dock-indicator {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  border: 2px dashed rgba(255, 255, 255, 0.4);"
        "  border-radius: 8px;"
        "}"
        ".regui-dock-indicator.regui-dock-indicator-active {"
        "  background-color: rgba(53, 132, 228, 0.35);"
        "  border: 2px solid rgba(53, 132, 228, 0.9);"
        "}"
        // The app always forces a dark theme (see main.cpp's ensureDarkTheme()), so a light,
        // low-alpha line reads as a subtle separator rather than guessing at a theme color.
        ".regui-dock-header { border-bottom: 1px solid rgba(255, 255, 255, 0.12); }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// A right-click popup for the canvas, same idea as HierarchyPanel's own row/empty-space menus.
struct CanvasMenuItem {
    std::function<void()> action;
    GtkWidget* popover;
};

void onCanvasMenuItemClicked(GtkButton*, gpointer userData) {
    auto* item = static_cast<CanvasMenuItem*>(userData);
    if (item->action) {
        item->action();
    }
    gtk_popover_popdown(GTK_POPOVER(item->popover));
}

void onCanvasMenuPopoverClosed(GtkPopover* popover, gpointer) { gtk_widget_unparent(GTK_WIDGET(popover)); }

// A flat, left-aligned button for a popover menu row (GTK centers plain button labels by default).
GtkWidget* newMenuButton(const std::string& label) {
    GtkWidget* button = gtk_button_new_with_label(label.c_str());
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(button, TRUE);
    if (GtkWidget* buttonLabel = gtk_button_get_child(GTK_BUTTON(button))) {
        gtk_label_set_xalign(GTK_LABEL(buttonLabel), 0.0f);
    }
    return button;
}

void showCanvasContextMenu(
    GtkWidget* anchor, double x, double y, std::vector<std::pair<std::string, std::function<void()>>> entries) {
    GtkWidget* popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE); // flat context menu, not a speech bubble
    gtk_widget_add_css_class(popover, "menu");
    gtk_widget_set_parent(popover, anchor);
    GdkRectangle rect{static_cast<int>(x), static_cast<int>(y), 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_signal_connect(popover, "closed", G_CALLBACK(onCanvasMenuPopoverClosed), nullptr);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    for (auto& [label, action] : entries) {
        GtkWidget* button = newMenuButton(label);
        gtk_box_append(GTK_BOX(box), button);
        auto* ctx = new CanvasMenuItem{std::move(action), popover};
        g_object_set_data_full(
            G_OBJECT(button), "regui-menu-ctx", ctx, [](gpointer p) { delete static_cast<CanvasMenuItem*>(p); });
        g_signal_connect(button, "clicked", G_CALLBACK(onCanvasMenuItemClicked), ctx);
    }

    gtk_popover_popup(GTK_POPOVER(popover));
}

struct HeaderMenuEntry {
    std::string label;
    std::function<void()> action;
    const char* tooltip = nullptr;
};

// A "File"/"Edit"-style header dropdown: a persistent popover instead of one built per right-click.
GtkWidget* buildHeaderMenu(const char* label, std::vector<HeaderMenuEntry> entries) {
    GtkWidget* menuButton = gtk_menu_button_new();
    gtk_menu_button_set_label(GTK_MENU_BUTTON(menuButton), label);

    GtkWidget* popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE); // flat dropdown, not a speech bubble
    gtk_widget_add_css_class(popover, "menu");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    for (HeaderMenuEntry& entry : entries) {
        GtkWidget* button = newMenuButton(entry.label);
        if (entry.tooltip) {
            gtk_widget_set_tooltip_text(button, entry.tooltip);
        }
        gtk_box_append(GTK_BOX(box), button);
        auto* ctx = new CanvasMenuItem{std::move(entry.action), popover};
        g_object_set_data_full(
            G_OBJECT(button), "regui-menu-ctx", ctx, [](gpointer p) { delete static_cast<CanvasMenuItem*>(p); });
        g_signal_connect(button, "clicked", G_CALLBACK(onCanvasMenuItemClicked), ctx);
    }

    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menuButton), popover);
    return menuButton;
}

} // namespace

MainWindow::MainWindow(GtkApplication* app, std::string guiAssetsPath, const std::string& initialPath) {
    ensureDockCssLoaded();
    layout_settings::LayoutSettings settings = layout_settings::load();

    mPropertiesPanel = std::make_unique<PropertiesPanel>(&mRegistry, &mUndoStack);

    mWindow = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(mWindow), "ReGui Editor");
    g_signal_connect(mWindow, "close-request", G_CALLBACK(onCloseRequest), this);
    gtk_window_set_default_size(GTK_WINDOW(mWindow), settings.windowWidth, settings.windowHeight);
    GtkWidget* headerBar = buildHeaderBar();
    gtk_window_set_titlebar(GTK_WINDOW(mWindow), headerBar);
    mPropertiesPanel->setParentWindow(GTK_WINDOW(mWindow)); // for the skin field's picker popup

    mHierarchyDetach = std::make_unique<DetachablePanel>(GTK_WINDOW(mWindow), "Hierarchy");
    mHierarchyDetach->enableDragSource();
    mHierarchyDetach->setNextFloatGeometry(
        settings.hierarchyFloatX, settings.hierarchyFloatY, settings.hierarchyFloatWidth, settings.hierarchyFloatHeight);
    mHierarchyDetach->setOnFloatCloseRequested([this]() { setPanelEdge(mHierarchyDetach.get(), mHierarchyEdge); });

    mPropertiesDetach = std::make_unique<DetachablePanel>(GTK_WINDOW(mWindow), "Properties");
    mPropertiesDetach->enableDragSource();
    mPropertiesDetach->setContent(mPropertiesPanel->widget());
    mPropertiesDetach->setNextFloatGeometry(
        settings.propertiesFloatX, settings.propertiesFloatY, settings.propertiesFloatWidth,
        settings.propertiesFloatHeight);
    mPropertiesDetach->setOnFloatCloseRequested([this]() { setPanelEdge(mPropertiesDetach.get(), mPropertiesEdge); });

    mHierarchyEdge = settings.hierarchyEdge;
    mPropertiesEdge = settings.propertiesEdge;
    if (mHierarchyEdge == mPropertiesEdge) {
        // A hand-edited (or otherwise corrupt) settings file asked for both
        // panels at the same edge - fall back to the original default split
        // rather than silently orphaning one of them.
        mHierarchyEdge = DockEdge::Left;
        mPropertiesEdge = DockEdge::Right;
    }

    mCanvas = std::make_unique<canvas::MyGuiCanvas>(std::move(guiAssetsPath));
    mCanvas->setUndoStack(&mUndoStack);
    // Live reference - starts empty, fills in once the canvas loads the skin XML.
    mPropertiesPanel->setAvailableSkins(&mCanvas->availableSkins());
    mCanvas->setOnSelectionChanged([this](model::WidgetNode* node) {
        if (mHierarchyPanel) {
            mHierarchyPanel->selectNode(node);
        }
        mPropertiesPanel->setNode(node);
    });
    mCanvas->setOnDragEnd([this]() { mPropertiesPanel->setNode(mCanvas->selectedNode()); });
    // Cheap in-place update, not a full grid rebuild - fires every mouse-move tick while dragging.
    mCanvas->setOnDragUpdate([this]() {
        markDirty();
        mPropertiesPanel->updateLiveValues(mCanvas->selectedNode());
    });
    mCanvas->setOnContextMenuRequested([this](model::WidgetNode* node, double x, double y) {
        if (node) {
            // Right-click also selects, so the menu target matches the rest of the editor.
            mCanvas->setSelection(node);
            mHierarchyPanel->selectNode(node);
            mPropertiesPanel->setNode(node);
            showCanvasContextMenu(
                mCanvas->widget(), x, y,
                {
                    {"Delete", [this, node]() { deleteNode(node); }},
                    {"Copy", [this, node]() { copyNode(node); }},
                });
        } else {
            // "New" needs a widget-type picker that doesn't exist yet, so it's left unwired.
            showCanvasContextMenu(
                mCanvas->widget(), x, y,
                {
                    {"New\xE2\x80\xA6", []() {}},
                    {"Insert\xE2\x80\xA6", [this]() { pasteClipboard(); }},
                });
        }
    });
    mCanvas->setOnZoomChanged([this](double zoom) { updateZoomLabel(zoom); });

    mPropertiesPanel->setOnLiveChange([this]() {
        markDirty();
        mCanvas->refresh();
    });
    mPropertiesPanel->setOnCommit([this]() {
        // Doesn't touch mPropertiesPanel itself - this runs from inside its
        // own focus-leave handling, so rebuilding mid-signal would crash.
        rebuildHierarchyPanel();
        mCanvas->refresh();
    });

    GtkWidget* dockArea = buildDockArea(settings);
    gtk_window_set_child(GTK_WINDOW(mWindow), dockArea);
    if (settings.windowMaximized) {
        gtk_window_maximize(GTK_WINDOW(mWindow));
    }

    loadAddonsFromDirectory(REGUIEDITOR_ADDONS_ROOT);
    rebuildHierarchyPanel(); // starts empty (no document loaded yet)
    // Both panels' wrappers only exist once mHierarchyDetach/mPropertiesDetach
    // are constructed and rebuildHierarchyPanel() has populated content -
    // now place them at their (possibly restored) edges.
    setSlotChild(mHierarchyEdge, mHierarchyDetach->widget());
    setSlotChild(mPropertiesEdge, mPropertiesDetach->widget());

    GtkEventController* keyController = gtk_event_controller_key_new();
    g_signal_connect(keyController, "key-pressed", G_CALLBACK(onKeyPressed), this);
    gtk_widget_add_controller(mWindow, keyController);

    mPendingHierarchyFloating = settings.hierarchyFloating;
    mPendingPropertiesFloating = settings.propertiesFloating;
    if (mPendingHierarchyFloating || mPendingPropertiesFloating) {
        // Deferred to first map so a restored floating panel never appears
        // before the main window itself does (this constructor runs before
        // the caller presents mWindow at all).
        mFirstMapHandlerId = g_signal_connect(mWindow, "map", G_CALLBACK(onFirstMap), this);
    }

    if (!initialPath.empty()) {
        loadDocument(initialPath);
    }
}

GtkWidget* MainWindow::buildDockArea(const layout_settings::LayoutSettings& settings) {
    constexpr int kDefaultLeftWidth = 240;
    constexpr int kDefaultRightWidth = 495;
    constexpr int kDefaultTopHeight = 200;
    constexpr int kDefaultBottomHeight = 200;

    GdkRectangle screen = primaryMonitorGeometry();
    int leftWidth = settings.leftSlotWidth >= 0 ? settings.leftSlotWidth : kDefaultLeftWidth;
    int rightWidth = settings.rightSlotWidth >= 0 ? settings.rightSlotWidth : kDefaultRightWidth;
    int topHeight = settings.topSlotHeight >= 0 ? settings.topSlotHeight : kDefaultTopHeight;
    int bottomHeight = settings.bottomSlotHeight >= 0 ? settings.bottomSlotHeight : kDefaultBottomHeight;
    int canvasWidth = std::max(400, screen.width - leftWidth - rightWidth);

    mDockMidHorizontalInner = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_resize_start_child(GTK_PANED(mDockMidHorizontalInner), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(mDockMidHorizontalInner), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(mDockMidHorizontalInner), TRUE); // the canvas soaks up extra width
    gtk_paned_set_end_child(GTK_PANED(mDockMidHorizontalInner), mCanvas->widget());
    gtk_paned_set_position(GTK_PANED(mDockMidHorizontalInner), leftWidth);

    mDockMidHorizontalOuter = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_resize_start_child(GTK_PANED(mDockMidHorizontalOuter), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(mDockMidHorizontalOuter), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(mDockMidHorizontalOuter), FALSE);
    gtk_paned_set_start_child(GTK_PANED(mDockMidHorizontalOuter), mDockMidHorizontalInner);
    gtk_paned_set_position(GTK_PANED(mDockMidHorizontalOuter), leftWidth + canvasWidth);

    mDockMidVertical = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_resize_start_child(GTK_PANED(mDockMidVertical), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(mDockMidVertical), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(mDockMidVertical), TRUE);
    gtk_paned_set_end_child(GTK_PANED(mDockMidVertical), mDockMidHorizontalOuter);
    gtk_paned_set_position(GTK_PANED(mDockMidVertical), topHeight);

    mDockOuterVertical = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_resize_start_child(GTK_PANED(mDockOuterVertical), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(mDockOuterVertical), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(mDockOuterVertical), FALSE);
    gtk_paned_set_start_child(GTK_PANED(mDockOuterVertical), mDockMidVertical);
    gtk_paned_set_position(GTK_PANED(mDockOuterVertical), std::max(400, screen.height - bottomHeight));

    mDockOverlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(mDockOverlay), mDockOuterVertical);

    mDockIndicatorLeft = buildDockIndicator(GTK_ALIGN_START, GTK_ALIGN_FILL, 140, -1);
    mDockIndicatorRight = buildDockIndicator(GTK_ALIGN_END, GTK_ALIGN_FILL, 140, -1);
    mDockIndicatorTop = buildDockIndicator(GTK_ALIGN_FILL, GTK_ALIGN_START, -1, 100);
    mDockIndicatorBottom = buildDockIndicator(GTK_ALIGN_FILL, GTK_ALIGN_END, -1, 100);
    mDockIndicatorCenter = buildDockIndicator(GTK_ALIGN_CENTER, GTK_ALIGN_CENTER, 200, 150);
    for (GtkWidget* indicator :
         {mDockIndicatorLeft, mDockIndicatorRight, mDockIndicatorTop, mDockIndicatorBottom, mDockIndicatorCenter}) {
        gtk_overlay_add_overlay(GTK_OVERLAY(mDockOverlay), indicator);
    }

    GtkDropTarget* dropTarget = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_MOVE);
    g_signal_connect(dropTarget, "enter", G_CALLBACK(onDockDragEnter), this);
    g_signal_connect(dropTarget, "motion", G_CALLBACK(onDockDragMotion), this);
    g_signal_connect(dropTarget, "leave", G_CALLBACK(onDockDragLeave), this);
    g_signal_connect(dropTarget, "drop", G_CALLBACK(onDockDrop), this);
    gtk_widget_add_controller(mDockOverlay, GTK_EVENT_CONTROLLER(dropTarget));

    return mDockOverlay;
}

void MainWindow::loadAssets(std::function<void(const canvas::LoadProgress&)> onProgress) {
    mCanvas->setOnLoadProgress(std::move(onProgress));
    gtk_widget_realize(mCanvas->widget());
}

void MainWindow::updateTitle() {
    std::string title = "ReGui Editor";
    if (!mCurrentPath.empty()) {
        title += " \xE2\x80\x94 " + mCurrentPath;
    }
    if (mDirty) {
        title += " *";
    }
    gtk_window_set_title(GTK_WINDOW(mWindow), title.c_str());
}

void MainWindow::markDirty() {
    if (!mDirty) {
        mDirty = true;
        updateTitle();
    }
}

void MainWindow::showErrorAlert(const std::string& title, const std::string& detail) {
    GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", title.c_str());
    gtk_alert_dialog_set_detail(dialog, detail.c_str());
    const char* buttons[] = {"OK", nullptr};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_modal(dialog, TRUE);
    gtk_alert_dialog_show(dialog, GTK_WINDOW(mWindow));
    g_object_unref(dialog);
}

GtkWidget* MainWindow::buildHeaderBar() {
    GtkWidget* headerBar = gtk_header_bar_new();

    GtkWidget* fileMenu = buildHeaderMenu(
        "File", {
                    {"Open\xE2\x80\xA6", [this]() { promptOpen(); }, nullptr},
                    {"Save", [this]() { save(); }, "Save (Ctrl+S)"},
                    {"Save As\xE2\x80\xA6", [this]() { promptSaveAs(); }, nullptr},
                    {"Open Addon\xE2\x80\xA6", [this]() { promptOpenAddon(); }, "Load a .readdon widget-type definition"},
                    {"About\xE2\x80\xA6", [this]() { showAboutDialog(); }, nullptr},
                });
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerBar), fileMenu);

    GtkWidget* editMenu = buildHeaderMenu(
        "Edit", {
                    {"Undo", [this]() { undo(); }, "Undo (Ctrl+Z)"},
                    {"Redo", [this]() { redo(); }, "Redo (Ctrl+Y)"},
                    {"History\xE2\x80\xA6", [this]() { showHistoryWindow(); }, "Browse recent actions"},
                });
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerBar), editMenu);

    // Zoom controls, packed first so they land at the very right edge of the header bar.
    GtkWidget* zoomBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget* zoomOutButton = gtk_button_new_with_label("\xE2\x88\x92"); // U+2212 MINUS SIGN
    gtk_widget_add_css_class(zoomOutButton, "flat");
    gtk_widget_set_tooltip_text(zoomOutButton, "Zoom out (Ctrl+Scroll)");
    g_signal_connect(zoomOutButton, "clicked", G_CALLBACK(onZoomOutClicked), this);
    gtk_box_append(GTK_BOX(zoomBox), zoomOutButton);

    mZoomLabel = gtk_button_new_with_label("100%");
    gtk_widget_add_css_class(mZoomLabel, "flat");
    gtk_widget_set_tooltip_text(mZoomLabel, "Reset zoom and pan");
    g_signal_connect(mZoomLabel, "clicked", G_CALLBACK(onZoomResetClicked), this);
    gtk_box_append(GTK_BOX(zoomBox), mZoomLabel);

    GtkWidget* zoomInButton = gtk_button_new_with_label("+");
    gtk_widget_add_css_class(zoomInButton, "flat");
    gtk_widget_set_tooltip_text(zoomInButton, "Zoom in (Ctrl+Scroll)");
    g_signal_connect(zoomInButton, "clicked", G_CALLBACK(onZoomInClicked), this);
    gtk_box_append(GTK_BOX(zoomBox), zoomInButton);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar), zoomBox);

    std::vector<const char*> resolutionLabels;
    for (const ResolutionPreset& preset : kResolutionPresets) {
        resolutionLabels.push_back(preset.label);
    }
    resolutionLabels.push_back(nullptr);
    GtkWidget* resolutionDropdown = gtk_drop_down_new_from_strings(resolutionLabels.data());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(resolutionDropdown), kDefaultResolutionIndex);
    gtk_widget_set_tooltip_text(resolutionDropdown, "Canvas reference resolution (simulated target screen)");
    g_signal_connect(resolutionDropdown, "notify::selected", G_CALLBACK(onResolutionChanged), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar), resolutionDropdown);

    GtkWidget* gridSpacingSpin = gtk_spin_button_new_with_range(2, 500, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gridSpacingSpin), kDefaultGridSpacing);
    gtk_widget_set_tooltip_text(gridSpacingSpin, "Grid spacing / snap increment (reference units)");
    g_signal_connect(gridSpacingSpin, "value-changed", G_CALLBACK(onGridSpacingChanged), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar), gridSpacingSpin);

    // pack_end inserts to the left of what's already packed, so this lands left of gridSpacingSpin.
    GtkWidget* gridEnabledCheck = gtk_check_button_new_with_label("Grid");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gridEnabledCheck), TRUE);
    gtk_widget_set_tooltip_text(
        gridEnabledCheck, "Show grid dots and snap to the grid (aligning to other widgets still works either way)");
    g_signal_connect(gridEnabledCheck, "toggled", G_CALLBACK(onGridEnabledToggled), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar), gridEnabledCheck);

    return headerBar;
}

void MainWindow::rebuildHierarchyPanel() {
    static std::vector<model::WidgetNode> emptyRoots;
    std::vector<model::WidgetNode>& roots = mDocument ? mDocument->roots : emptyRoots;

    // Preserved across a mid-session refresh; a fresh load already cleared selection via setRoots().
    model::WidgetNode* previousSelection = mCanvas->selectedNode();

    mHierarchyPanel = std::make_unique<HierarchyPanel>(roots, mRegistry);
    mHierarchyPanel->setOnSelectionChanged([this](model::WidgetNode* node) {
        mCanvas->setSelection(node);
        mPropertiesPanel->setNode(node);
    });
    mHierarchyPanel->setOnReparent(
        [this](model::WidgetNode* dragged, model::WidgetNode* newParent) { reparentNode(dragged, newParent); });
    mHierarchyPanel->setOnDeleteRequested([this](model::WidgetNode* node) { deleteNode(node); });
    mHierarchyPanel->setOnCopyRequested([this](model::WidgetNode* node) { copyNode(node); });
    mHierarchyPanel->setOnInsertRequested([this]() { pasteClipboard(); });
    // Swaps content only - whichever dock slot (or floating window)
    // mHierarchyDetach->widget() itself currently lives in is untouched.
    mHierarchyDetach->setContent(mHierarchyPanel->widget());

    model::WidgetNode* toSelect = previousSelection ? previousSelection : (roots.empty() ? nullptr : &roots[0]);
    mCanvas->setSelection(toSelect);
    mHierarchyPanel->selectNode(toSelect);
}

void MainWindow::reparentNode(model::WidgetNode* dragged, model::WidgetNode* newParent) {
    if (!mDocument || !dragged) {
        return;
    }
    model::WidgetNode* oldParent = model::findParentOf(mDocument->roots, dragged);
    if (oldParent == newParent) {
        return; // dropped back onto its current parent - nothing to do
    }

    // Captured before the move - `dragged`/`newParent` are dangling or reused afterward.
    std::string draggedName = dragged->name;
    std::string newParentName = newParent ? newParent->name : "Root";

    // `dragged` is dangling once moveWidgetNode() returns - use its output pointer instead.
    // Selection is cleared first since the move can invalidate it too.
    mCanvas->setSelection(nullptr);

    model::WidgetNode* movedNode = nullptr;
    if (!model::moveWidgetNode(mDocument->roots, dragged, newParent, &movedNode)) {
        return;
    }

    mCanvas->setRoots(&mDocument->roots);
    mPropertiesPanel->setRoots(&mDocument->roots);
    rebuildHierarchyPanel();
    mCanvas->setSelection(movedNode);
    mHierarchyPanel->selectNode(movedNode);
    mPropertiesPanel->setNode(movedNode);
    markDirty();

    // `current` is a shared indirection cell the two closures hand off to
    // each other, since each moveWidgetNode() call invalidates the last pointer.
    auto current = std::make_shared<model::WidgetNode*>(movedNode);
    mUndoStack.push(
        [this, current, oldParent]() {
            model::WidgetNode* restored = nullptr;
            if (!model::moveWidgetNode(mDocument->roots, *current, oldParent, &restored)) {
                return;
            }
            *current = restored;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mCanvas->setSelection(restored);
            mHierarchyPanel->selectNode(restored);
            mPropertiesPanel->setNode(restored);
            markDirty();
        },
        [this, current, newParent]() {
            model::WidgetNode* redone = nullptr;
            if (!model::moveWidgetNode(mDocument->roots, *current, newParent, &redone)) {
                return;
            }
            *current = redone;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mCanvas->setSelection(redone);
            mHierarchyPanel->selectNode(redone);
            mPropertiesPanel->setNode(redone);
            markDirty();
        },
        "Widget Reparented", "Moved '" + draggedName + "' under '" + newParentName + "'");
}

void MainWindow::undo() {
    bool canUndo = mUndoStack.canUndo();
    mUndoStack.undo();
    if (canUndo) {
        markDirty();
    }
    if (mHierarchyPanel) {
        mHierarchyPanel->selectNode(mCanvas->selectedNode());
    }
    mPropertiesPanel->setNode(mCanvas->selectedNode());
}

void MainWindow::redo() {
    bool canRedo = mUndoStack.canRedo();
    mUndoStack.redo();
    if (canRedo) {
        markDirty();
    }
    if (mHierarchyPanel) {
        mHierarchyPanel->selectNode(mCanvas->selectedNode());
    }
    mPropertiesPanel->setNode(mCanvas->selectedNode());
}

void MainWindow::showHistoryWindow() {
    if (!mHistoryWindow) {
        mHistoryWindow = std::make_unique<HistoryWindow>(
            GTK_WINDOW(mWindow), &mUndoStack, [this](bool isRedoTab, size_t displayIndex) {
                jumpToHistoryEntry(isRedoTab, displayIndex);
            });
    }
    mHistoryWindow->present();
}

void MainWindow::jumpToHistoryEntry(bool isRedoTab, size_t displayIndex) {
    bool changed = isRedoTab ? mUndoStack.redoTo(displayIndex + 1) : mUndoStack.undoTo(displayIndex);
    if (changed) {
        markDirty();
    }
    if (mHierarchyPanel) {
        mHierarchyPanel->selectNode(mCanvas->selectedNode());
    }
    mPropertiesPanel->setNode(mCanvas->selectedNode());
}

void MainWindow::save(std::function<void()> onSaved) {
    if (!mDocument) {
        return;
    }
    if (mCurrentPath.empty()) {
        promptSaveAs(std::move(onSaved));
        return;
    }
    try {
        mDocument->save(mCurrentPath);
        mDirty = false;
        updateTitle();
        if (onSaved) {
            onSaved();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save '" << mCurrentPath << "': " << e.what() << "\n";
        showErrorAlert("Unable to save layout", e.what());
    }
}

void MainWindow::copyNode(model::WidgetNode* node) {
    if (!node) {
        return;
    }
    mClipboard = *node; // deep copy - independent of the tree from here on
}

void MainWindow::pasteClipboard() {
    if (!mClipboard || !mDocument) {
        return;
    }
    model::WidgetNode* selected = mCanvas->selectedNode();
    model::WidgetNode* parent = selected ? model::findParentOf(mDocument->roots, selected) : nullptr;
    std::vector<model::WidgetNode>& siblings = parent ? parent->children : mDocument->roots;

    model::WidgetNode* inserted = model::insertWidgetNode(mDocument->roots, parent, siblings.size(), *mClipboard);
    mCanvas->setRoots(&mDocument->roots);
    mPropertiesPanel->setRoots(&mDocument->roots);
    rebuildHierarchyPanel();
    mCanvas->setSelection(inserted);
    mHierarchyPanel->selectNode(inserted);
    mPropertiesPanel->setNode(inserted);
    markDirty();

    // `toPaste` is a stable copy reused on every redo; `current` tracks the
    // live node's pointer (or nullptr once undone), same pattern as reparentNode().
    auto toPaste = std::make_shared<model::WidgetNode>(*mClipboard);
    auto current = std::make_shared<model::WidgetNode*>(inserted);
    mUndoStack.push(
        [this, current]() {
            if (!*current) {
                return;
            }
            model::removeWidgetNode(mDocument->roots, *current, nullptr);
            *current = nullptr;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mCanvas->setSelection(nullptr);
            mHierarchyPanel->selectNode(nullptr);
            mPropertiesPanel->setNode(nullptr);
            markDirty();
        },
        [this, current, toPaste, parent]() {
            std::vector<model::WidgetNode>& redoSiblings = parent ? parent->children : mDocument->roots;
            model::WidgetNode* redone = model::insertWidgetNode(mDocument->roots, parent, redoSiblings.size(), *toPaste);
            *current = redone;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mCanvas->setSelection(redone);
            mHierarchyPanel->selectNode(redone);
            mPropertiesPanel->setNode(redone);
            markDirty();
        },
        "Widget Created");
}

void MainWindow::deleteNode(model::WidgetNode* node) {
    if (!node || !mDocument) {
        return;
    }
    model::WidgetNode* parent = model::findParentOf(mDocument->roots, node);
    std::vector<model::WidgetNode>& siblings = parent ? parent->children : mDocument->roots;
    size_t index = 0;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (&siblings[i] == node) {
            index = i;
            break;
        }
    }

    // Cleared before the erase, same reasoning as reparentNode().
    mCanvas->setSelection(nullptr);

    model::WidgetNode removed;
    if (!model::removeWidgetNode(mDocument->roots, node, &removed)) {
        return;
    }
    mCanvas->setRoots(&mDocument->roots);
    mPropertiesPanel->setRoots(&mDocument->roots);
    rebuildHierarchyPanel();
    mPropertiesPanel->setNode(mCanvas->selectedNode());
    markDirty();

    auto removedCopy = std::make_shared<model::WidgetNode>(std::move(removed));
    auto current = std::make_shared<model::WidgetNode*>(nullptr);
    std::string removedName = removedCopy->name;
    mUndoStack.push(
        [this, removedCopy, current, parent, index]() {
            model::WidgetNode* restored = model::insertWidgetNode(mDocument->roots, parent, index, *removedCopy);
            *current = restored;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mCanvas->setSelection(restored);
            mHierarchyPanel->selectNode(restored);
            mPropertiesPanel->setNode(restored);
            markDirty();
        },
        [this, current]() {
            if (!*current) {
                return;
            }
            mCanvas->setSelection(nullptr);
            model::removeWidgetNode(mDocument->roots, *current, nullptr);
            *current = nullptr;
            mCanvas->setRoots(&mDocument->roots);
            mPropertiesPanel->setRoots(&mDocument->roots);
            rebuildHierarchyPanel();
            mPropertiesPanel->setNode(mCanvas->selectedNode());
            markDirty();
        },
        "Widget Deleted", "Deleted '" + removedName + "'");
}

void MainWindow::loadDocument(const std::string& path) {
    try {
        mDocument = std::make_unique<model::ReLayoutDocument>(model::ReLayoutDocument::load(path));
        mCurrentPath = path;
        mDirty = false;
        mUndoStack.clear();
        // setRoots() clears selection, so it must run before rebuildHierarchyPanel() re-syncs row 0.
        mCanvas->setRoots(&mDocument->roots);
        mPropertiesPanel->setRoots(&mDocument->roots);
        rebuildHierarchyPanel();
        updateTitle();
    } catch (const std::exception& e) {
        std::cerr << "Failed to load '" << path << "': " << e.what() << "\n";
    }
}

void MainWindow::loadAddonsFromDirectory(const std::string& directory) {
    std::error_code errorCode;
    if (!std::filesystem::is_directory(directory, errorCode)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".readdon") {
            continue;
        }
        try {
            mRegistry.loadFile(entry.path().string());
        } catch (const std::exception& e) {
            std::cerr << "Failed to load addon '" << entry.path().string() << "': " << e.what() << "\n";
        }
    }
}

void MainWindow::promptOpen() {
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

    gtk_file_dialog_open(dialog, GTK_WINDOW(mWindow), nullptr, &MainWindow::onOpenFileDialogReady, this);
    g_object_unref(dialog);
}

void MainWindow::onOpenFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        if (error) {
            g_error_free(error); // includes user-cancelled, not necessarily a real failure
        }
        return;
    }
    if (char* path = g_file_get_path(file)) {
        self->loadDocument(path);
        g_free(path);
    }
    g_object_unref(file);
}

void MainWindow::promptSaveAs(std::function<void()> onSaved) {
    if (!mDocument) {
        return;
    }

    mPendingSaveAsCallback = std::move(onSaved);

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save ReLayout As");
    if (!mCurrentPath.empty()) {
        // set_initial_name() wants just the filename, not the full path - directory goes through set_initial_folder().
        std::filesystem::path currentPath(mCurrentPath);
        gtk_file_dialog_set_initial_name(dialog, currentPath.filename().string().c_str());
        std::string parentDir = currentPath.parent_path().string();
        if (!parentDir.empty()) {
            GFile* folder = g_file_new_for_path(parentDir.c_str());
            gtk_file_dialog_set_initial_folder(dialog, folder);
            g_object_unref(folder);
        }
    }

    gtk_file_dialog_save(dialog, GTK_WINDOW(mWindow), nullptr, &MainWindow::onSaveAsFileDialogReady, this);
    g_object_unref(dialog);
}

void MainWindow::onSaveAsFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        self->mPendingSaveAsCallback = {};
        if (error) {
            g_error_free(error);
        }
        return;
    }
    if (char* path = g_file_get_path(file)) {
        try {
            self->mDocument->save(path);
            self->mCurrentPath = path;
            self->mDirty = false;
            self->updateTitle();
            if (self->mPendingSaveAsCallback) {
                auto callback = std::move(self->mPendingSaveAsCallback);
                callback();
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to save '" << path << "': " << e.what() << "\n";
            self->mPendingSaveAsCallback = {};
            self->showErrorAlert("Unable to save layout", e.what());
        }
        g_free(path);
    }
    g_object_unref(file);
}

void MainWindow::promptOpenAddon() {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Addon");

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "ReAddon files");
    gtk_file_filter_add_pattern(filter, "*.readdon");
    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_object_unref(filter);

    gtk_file_dialog_open(dialog, GTK_WINDOW(mWindow), nullptr, &MainWindow::onOpenAddonFileDialogReady, this);
    g_object_unref(dialog);
}

void MainWindow::showAboutDialog() {
    // A plain window rather than GtkAboutDialog, which brings more chrome (Credits/License/System, animation) than needed.
    GtkWidget* dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "About");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(mWindow));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_widget_set_size_request(dialog, 420, -1);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 28);
    gtk_widget_set_margin_bottom(box, 28);
    gtk_widget_set_margin_start(box, 36);
    gtk_widget_set_margin_end(box, 36);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget* nameLabel = gtk_label_new(regui::kAppName);
    gtk_widget_add_css_class(nameLabel, "title-1");
    gtk_box_append(GTK_BOX(box), nameLabel);

    std::string versionText = std::string("Version ") + regui::kAppVersion;
    GtkWidget* versionLabel = gtk_label_new(versionText.c_str());
    gtk_widget_add_css_class(versionLabel, "dim-label");
    gtk_box_append(GTK_BOX(box), versionLabel);

    GtkWidget* descriptionLabel = gtk_label_new(regui::kAppDescription);
    gtk_label_set_wrap(GTK_LABEL(descriptionLabel), TRUE);
    gtk_label_set_justify(GTK_LABEL(descriptionLabel), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_top(descriptionLabel, 12);
    gtk_box_append(GTK_BOX(box), descriptionLabel);

    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(separator, 4);
    gtk_widget_set_margin_bottom(separator, 4);
    gtk_box_append(GTK_BOX(box), separator);

    std::string creatorsText;
    for (const char* const* creator = regui::kAppCreators; *creator; ++creator) {
        if (!creatorsText.empty()) {
            creatorsText += "\n";
        }
        creatorsText += *creator;
    }
    if (!creatorsText.empty()) {
        GtkWidget* creatorsHeading = gtk_label_new("Created by");
        gtk_widget_add_css_class(creatorsHeading, "dim-label");
        gtk_widget_add_css_class(creatorsHeading, "caption-heading");
        gtk_box_append(GTK_BOX(box), creatorsHeading);

        GtkWidget* creatorsLabel = gtk_label_new(creatorsText.c_str());
        gtk_box_append(GTK_BOX(box), creatorsLabel);
    }

    // GtkLabel turns a Pango <a href="..."> span into a clickable link on its own.
    std::string licenseMarkup = std::string("<a href=\"") + regui::kLicenseUrl + "\">License</a>";
    GtkWidget* licenseLabel = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(licenseLabel), licenseMarkup.c_str());
    gtk_box_append(GTK_BOX(box), licenseLabel);

    // Center every label; the separator is excluded so it spans the full width.
    for (GtkWidget* child = gtk_widget_get_first_child(box); child; child = gtk_widget_get_next_sibling(child)) {
        if (!GTK_IS_SEPARATOR(child)) {
            gtk_widget_set_halign(child, GTK_ALIGN_CENTER);
        }
    }

    gtk_window_present(GTK_WINDOW(dialog));
}

void MainWindow::onOpenAddonFileDialogReady(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        if (error) {
            g_error_free(error);
        }
        return;
    }
    if (char* path = g_file_get_path(file)) {
        try {
            self->mRegistry.loadFile(path);
            // The new type definition might match the currently selected widget.
            self->mPropertiesPanel->setNode(self->mCanvas->selectedNode());
        } catch (const std::exception& e) {
            std::cerr << "Failed to load addon '" << path << "': " << e.what() << "\n";
        }
        g_free(path);
    }
    g_object_unref(file);
}

void MainWindow::onResolutionChanged(GtkDropDown* dropdown, GParamSpec*, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected >= std::size(kResolutionPresets)) {
        return;
    }
    const ResolutionPreset& preset = kResolutionPresets[selected];
    self->mCanvas->setReferenceResolution(preset.width, preset.height);
}

void MainWindow::onGridSpacingChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    self->mCanvas->setGridSpacing(gtk_spin_button_get_value(spin));
}

void MainWindow::onGridEnabledToggled(GtkCheckButton* button, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    self->mCanvas->setGridEnabled(gtk_check_button_get_active(button));
}

void MainWindow::onZoomInClicked(GtkButton*, gpointer userData) {
    static_cast<MainWindow*>(userData)->mCanvas->zoomIn();
}

void MainWindow::onZoomOutClicked(GtkButton*, gpointer userData) {
    static_cast<MainWindow*>(userData)->mCanvas->zoomOut();
}

void MainWindow::onZoomResetClicked(GtkButton*, gpointer userData) {
    static_cast<MainWindow*>(userData)->mCanvas->resetView();
}

void MainWindow::updateZoomLabel(double zoom) {
    gtk_button_set_label(GTK_BUTTON(mZoomLabel), (std::to_string(static_cast<int>(std::lround(zoom * 100))) + "%").c_str());
}

gboolean MainWindow::onCloseRequest(GtkWindow*, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    // Captured here, synchronously, while every floating window is still
    // guaranteed alive - not on mWindow's own "destroy" signal, since a
    // floating panel's window is torn down via destroy-with-parent as part
    // of that same teardown, in an order this code shouldn't have to guess at.
    self->saveLayoutSettings();

    if (!self->mDirty) {
        return FALSE;
    }

    GtkAlertDialog* dialog = gtk_alert_dialog_new("Unsaved changes");
    gtk_alert_dialog_set_detail(dialog, "Save the layout before closing ReGui Editor?");
    const char* buttons[] = {"Save", "Discard", "Cancel", nullptr};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 2);
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_set_modal(dialog, TRUE);
    gtk_alert_dialog_choose(dialog, GTK_WINDOW(self->mWindow), nullptr, &MainWindow::onCloseConfirmChoice, self);
    g_object_unref(dialog);
    return TRUE;
}

void MainWindow::onCloseConfirmChoice(GObject* source, GAsyncResult* result, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    GError* error = nullptr;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &error);
    if (error) {
        g_error_free(error);
        return;
    }
    if (choice == 0) {
        self->save([self]() { gtk_window_destroy(GTK_WINDOW(self->mWindow)); });
    } else if (choice == 1) {
        gtk_window_destroy(GTK_WINDOW(self->mWindow));
    }
}

void MainWindow::onFirstMap(GtkWidget* window, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    if (self->mPendingHierarchyFloating) {
        self->setPanelFloating(self->mHierarchyDetach.get());
    }
    if (self->mPendingPropertiesFloating) {
        self->setPanelFloating(self->mPropertiesDetach.get());
    }
    g_signal_handler_disconnect(window, self->mFirstMapHandlerId);
    self->mFirstMapHandlerId = 0;
}

void MainWindow::saveLayoutSettings() {
    layout_settings::LayoutSettings settings;

    settings.windowMaximized = gtk_window_is_maximized(GTK_WINDOW(mWindow));
    if (!settings.windowMaximized) {
        settings.windowWidth = gtk_widget_get_width(mWindow);
        settings.windowHeight = gtk_widget_get_height(mWindow);
    }

    settings.leftSlotWidth = gtk_paned_get_position(GTK_PANED(mDockMidHorizontalInner));
    settings.topSlotHeight = gtk_paned_get_position(GTK_PANED(mDockMidVertical));
    int outerWidth = gtk_widget_get_width(mDockMidHorizontalOuter);
    int outerHeight = gtk_widget_get_height(mDockOuterVertical);
    settings.rightSlotWidth = outerWidth > 0 ? outerWidth - gtk_paned_get_position(GTK_PANED(mDockMidHorizontalOuter)) : -1;
    settings.bottomSlotHeight = outerHeight > 0 ? outerHeight - gtk_paned_get_position(GTK_PANED(mDockOuterVertical)) : -1;

    settings.hierarchyEdge = mHierarchyEdge;
    settings.hierarchyFloating = mHierarchyDetach->isFloating();
    mHierarchyDetach->currentFloatGeometry(
        settings.hierarchyFloatX, settings.hierarchyFloatY, settings.hierarchyFloatWidth, settings.hierarchyFloatHeight);

    settings.propertiesEdge = mPropertiesEdge;
    settings.propertiesFloating = mPropertiesDetach->isFloating();
    mPropertiesDetach->currentFloatGeometry(
        settings.propertiesFloatX, settings.propertiesFloatY, settings.propertiesFloatWidth,
        settings.propertiesFloatHeight);

    layout_settings::save(settings);
}

void MainWindow::setSlotChild(DockEdge edge, GtkWidget* widget) {
    switch (edge) {
        case DockEdge::Left:
            gtk_paned_set_start_child(GTK_PANED(mDockMidHorizontalInner), widget);
            break;
        case DockEdge::Right:
            gtk_paned_set_end_child(GTK_PANED(mDockMidHorizontalOuter), widget);
            break;
        case DockEdge::Top:
            gtk_paned_set_start_child(GTK_PANED(mDockMidVertical), widget);
            break;
        case DockEdge::Bottom:
            gtk_paned_set_end_child(GTK_PANED(mDockOuterVertical), widget);
            break;
    }
}

namespace {
bool edgeIsWidthBased(DockEdge edge) {
    return edge == DockEdge::Left || edge == DockEdge::Right;
}
} // namespace

int MainWindow::slotSize(DockEdge edge) {
    switch (edge) {
        case DockEdge::Left:
            return gtk_paned_get_position(GTK_PANED(mDockMidHorizontalInner));
        case DockEdge::Right: {
            int total = gtk_widget_get_width(mDockMidHorizontalOuter);
            return total > 0 ? total - gtk_paned_get_position(GTK_PANED(mDockMidHorizontalOuter)) : -1;
        }
        case DockEdge::Top:
            return gtk_paned_get_position(GTK_PANED(mDockMidVertical));
        case DockEdge::Bottom: {
            int total = gtk_widget_get_height(mDockOuterVertical);
            return total > 0 ? total - gtk_paned_get_position(GTK_PANED(mDockOuterVertical)) : -1;
        }
    }
    return -1;
}

void MainWindow::setSlotSize(DockEdge edge, int size) {
    switch (edge) {
        case DockEdge::Left:
            gtk_paned_set_position(GTK_PANED(mDockMidHorizontalInner), size);
            break;
        case DockEdge::Right: {
            int total = gtk_widget_get_width(mDockMidHorizontalOuter);
            if (total > 0) {
                gtk_paned_set_position(GTK_PANED(mDockMidHorizontalOuter), total - size);
            }
            break;
        }
        case DockEdge::Top:
            gtk_paned_set_position(GTK_PANED(mDockMidVertical), size);
            break;
        case DockEdge::Bottom: {
            int total = gtk_widget_get_height(mDockOuterVertical);
            if (total > 0) {
                gtk_paned_set_position(GTK_PANED(mDockOuterVertical), total - size);
            }
            break;
        }
    }
}

int MainWindow::panelSizeIfApplicable(DetachablePanel* panel, bool wantWidth) {
    if (panel->isFloating()) {
        int x, y, w, h;
        panel->currentFloatGeometry(x, y, w, h);
        int size = wantWidth ? w : h;
        return size > 0 ? size : -1;
    }
    DockEdge edge = edgeFieldFor(panel);
    if (edgeIsWidthBased(edge) != wantWidth) {
        return -1; // e.g. currently docked Top (a height) but asked for its width - no such thing
    }
    return slotSize(edge);
}

DockEdge& MainWindow::edgeFieldFor(DetachablePanel* panel) {
    return panel == mHierarchyDetach.get() ? mHierarchyEdge : mPropertiesEdge;
}

DetachablePanel* MainWindow::otherPanel(DetachablePanel* panel) {
    return panel == mHierarchyDetach.get() ? mPropertiesDetach.get() : mHierarchyDetach.get();
}

void MainWindow::setPanelEdge(DetachablePanel* panel, DockEdge edge) {
    DetachablePanel* other = otherPanel(panel);
    DockEdge& panelEdgeField = edgeFieldFor(panel);
    DockEdge& otherEdgeField = edgeFieldFor(other);

    bool collision = !other->isFloating() && otherEdgeField == edge;
    bool panelWasFloating = panel->isFloating();
    DockEdge panelOldEdge = panelEdgeField;
    // Read before `panel` is extracted below, while its old slot/float
    // geometry is still current - the size it should carry to the new edge,
    // if the two edges share an axis (moving Left<->Right keeps the width,
    // Top<->Bottom keeps the height; Left->Top etc. has nothing to carry).
    int carriedSize = panelSizeIfApplicable(panel, edgeIsWidthBased(edge));

    // 1. Extract `panel` from wherever it currently is.
    if (panelWasFloating) {
        panel->setFloating(false); // leaves panel->widget() unparented
    } else {
        setSlotChild(panelOldEdge, nullptr);
    }

    // 2. If the target edge is occupied by `other`, extract it too before overwriting that slot.
    if (collision) {
        setSlotChild(edge, nullptr);
    }

    // 3. Place `panel` at the new edge, carrying its size over if applicable.
    setSlotChild(edge, panel->widget());
    panelEdgeField = edge;
    if (carriedSize > 0) {
        setSlotSize(edge, carriedSize);
    }

    // 4. Give `other` panel's old spot, if it got displaced.
    if (collision) {
        if (panelWasFloating) {
            other->setFloating(true); // otherEdgeField is untouched - it's still where `other` should redock later
        } else {
            setSlotChild(panelOldEdge, other->widget());
            otherEdgeField = panelOldEdge;
        }
    }

    saveLayoutSettings();
}

void MainWindow::setPanelFloating(DetachablePanel* panel) {
    if (panel->isFloating()) {
        return; // e.g. dropped on the center zone while already floating
    }
    // Carry its current on-screen size into the floating window so it
    // doesn't visibly resize just from being undocked.
    int dockedWidth = gtk_widget_get_width(panel->widget());
    int dockedHeight = gtk_widget_get_height(panel->widget());
    if (dockedWidth > 0 && dockedHeight > 0) {
        int x, y, oldWidth, oldHeight;
        panel->currentFloatGeometry(x, y, oldWidth, oldHeight);
        panel->setNextFloatGeometry(x, y, dockedWidth, dockedHeight);
    }
    setSlotChild(edgeFieldFor(panel), nullptr);
    panel->setFloating(true);
    saveLayoutSettings();
}

namespace {
void setIndicatorActive(GtkWidget* indicator, bool active) {
    if (active) {
        gtk_widget_add_css_class(indicator, "regui-dock-indicator-active");
    } else {
        gtk_widget_remove_css_class(indicator, "regui-dock-indicator-active");
    }
}
} // namespace

// All 5 zones are shown for the duration of the drag so every option is
// visible at once; only `zone` also gets the blue "this is where it'll land" fill.
void MainWindow::showDockIndicator(DockZone zone) {
    for (GtkWidget* indicator :
         {mDockIndicatorLeft, mDockIndicatorRight, mDockIndicatorTop, mDockIndicatorBottom, mDockIndicatorCenter}) {
        gtk_widget_set_visible(indicator, TRUE);
    }
    setIndicatorActive(mDockIndicatorLeft, zone == DockZone::Left);
    setIndicatorActive(mDockIndicatorRight, zone == DockZone::Right);
    setIndicatorActive(mDockIndicatorTop, zone == DockZone::Top);
    setIndicatorActive(mDockIndicatorBottom, zone == DockZone::Bottom);
    setIndicatorActive(mDockIndicatorCenter, zone == DockZone::Center);
}

void MainWindow::hideDockIndicators() {
    for (GtkWidget* indicator :
         {mDockIndicatorLeft, mDockIndicatorRight, mDockIndicatorTop, mDockIndicatorBottom, mDockIndicatorCenter}) {
        gtk_widget_set_visible(indicator, FALSE);
        setIndicatorActive(indicator, false);
    }
}

void MainWindow::updateDockIndicatorForPosition(double x, double y) {
    double width = gtk_widget_get_width(mDockOverlay);
    double height = gtk_widget_get_height(mDockOverlay);
    showDockIndicator(classifyDockZone(x, y, width, height));
}

GdkDragAction MainWindow::onDockDragEnter(GtkDropTarget*, double x, double y, gpointer userData) {
    static_cast<MainWindow*>(userData)->updateDockIndicatorForPosition(x, y);
    return GDK_ACTION_MOVE;
}

GdkDragAction MainWindow::onDockDragMotion(GtkDropTarget*, double x, double y, gpointer userData) {
    static_cast<MainWindow*>(userData)->updateDockIndicatorForPosition(x, y);
    return GDK_ACTION_MOVE;
}

void MainWindow::onDockDragLeave(GtkDropTarget*, gpointer userData) {
    static_cast<MainWindow*>(userData)->hideDockIndicators();
}

gboolean MainWindow::onDockDrop(GtkDropTarget* target, const GValue*, double x, double y, gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    self->hideDockIndicators();

    GdkDrop* drop = gtk_drop_target_get_current_drop(target);
    GdkDrag* drag = drop ? gdk_drop_get_drag(drop) : nullptr;
    if (!drag) {
        return FALSE; // not a same-process drag we recognize
    }
    auto* panel = static_cast<DetachablePanel*>(g_object_get_data(G_OBJECT(drag), "regui-dock-panel-id"));
    if (panel != self->mHierarchyDetach.get() && panel != self->mPropertiesDetach.get()) {
        return FALSE;
    }

    double width = gtk_widget_get_width(self->mDockOverlay);
    double height = gtk_widget_get_height(self->mDockOverlay);
    DockZone zone = classifyDockZone(x, y, width, height);
    if (zone == DockZone::Center) {
        self->setPanelFloating(panel);
    } else {
        self->setPanelEdge(panel, dockZoneToEdge(zone));
    }
    return TRUE;
}

gboolean MainWindow::onKeyPressed(
    GtkEventControllerKey*,
    guint keyval,
    guint /*keycode*/,
    GdkModifierType state,
    gpointer userData) {
    auto* self = static_cast<MainWindow*>(userData);
    bool ctrl = (state & GDK_CONTROL_MASK) != 0;
    bool shift = (state & GDK_SHIFT_MASK) != 0;

    if (ctrl && !shift && keyval == GDK_KEY_z) {
        self->undo();
        return TRUE;
    }
    if (ctrl && ((shift && keyval == GDK_KEY_z) || (!shift && keyval == GDK_KEY_y))) {
        self->redo();
        return TRUE;
    }
    if (ctrl && !shift && keyval == GDK_KEY_s) {
        self->save();
        return TRUE;
    }
    if (ctrl && !shift && keyval == GDK_KEY_c) {
        self->copyNode(self->mCanvas->selectedNode());
        return TRUE;
    }
    if (ctrl && !shift && keyval == GDK_KEY_v) {
        self->pasteClipboard();
        return TRUE;
    }
    if (!ctrl && (keyval == GDK_KEY_Delete || keyval == GDK_KEY_KP_Delete)) {
        self->deleteNode(self->mCanvas->selectedNode());
        return TRUE;
    }
    return FALSE;
}

} // namespace regui::ui
