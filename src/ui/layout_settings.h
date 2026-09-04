#pragma once

namespace regui::ui::layout_settings {

// Matches MainWindow::DockEdge - kept as a plain int here so this header
// doesn't need to depend on main_window.h.
enum class DockEdge { Left, Right, Top, Bottom };

// Persisted window/panel layout, restored on startup so the editor doesn't
// need re-arranging from scratch every launch. A missing/unreadable file
// yields defaults that match the editor's original hardcoded behavior
// (maximized, hierarchy on the left, properties on the right).
struct LayoutSettings {
    int windowWidth = 1280;
    int windowHeight = 800;
    bool windowMaximized = true;

    // -1 means "not set yet" - MainWindow falls back to its own computed default.
    int leftSlotWidth = -1;
    int rightSlotWidth = -1;
    int topSlotHeight = -1;
    int bottomSlotHeight = -1;

    DockEdge hierarchyEdge = DockEdge::Left;
    bool hierarchyFloating = false;
    int hierarchyFloatX = -1;
    int hierarchyFloatY = -1;
    int hierarchyFloatWidth = 320;
    int hierarchyFloatHeight = 600;

    DockEdge propertiesEdge = DockEdge::Right;
    bool propertiesFloating = false;
    int propertiesFloatX = -1;
    int propertiesFloatY = -1;
    int propertiesFloatWidth = 360;
    int propertiesFloatHeight = 700;
};

LayoutSettings load();
void save(const LayoutSettings& settings);

} // namespace regui::ui::layout_settings
