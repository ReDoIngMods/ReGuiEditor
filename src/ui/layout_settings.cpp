#include "ui/layout_settings.h"

#include <glib.h>

#include <string>

namespace regui::ui::layout_settings {

namespace {

constexpr const char* kGroup = "Layout";

// Same settings directory as recent_files.cpp, kept as one file per kind of
// state rather than merging into one bigger structured format.
std::string storePath() {
    std::string dir = std::string(g_get_user_config_dir()) + "/reguieditor";
    g_mkdir_with_parents(dir.c_str(), 0755);
    return dir + "/layout.ini";
}

int getIntOr(GKeyFile* keyFile, const char* key, int fallback) {
    GError* error = nullptr;
    int value = g_key_file_get_integer(keyFile, kGroup, key, &error);
    if (error) {
        g_error_free(error);
        return fallback;
    }
    return value;
}

bool getBoolOr(GKeyFile* keyFile, const char* key, bool fallback) {
    GError* error = nullptr;
    gboolean value = g_key_file_get_boolean(keyFile, kGroup, key, &error);
    if (error) {
        g_error_free(error);
        return fallback;
    }
    return value;
}

DockEdge getEdgeOr(GKeyFile* keyFile, const char* key, DockEdge fallback) {
    int raw = getIntOr(keyFile, key, static_cast<int>(fallback));
    if (raw < static_cast<int>(DockEdge::Left) || raw > static_cast<int>(DockEdge::Bottom)) {
        return fallback;
    }
    return static_cast<DockEdge>(raw);
}

} // namespace

LayoutSettings load() {
    LayoutSettings settings;

    GKeyFile* keyFile = g_key_file_new();
    if (!g_key_file_load_from_file(keyFile, storePath().c_str(), G_KEY_FILE_NONE, nullptr)) {
        g_key_file_free(keyFile);
        return settings; // no saved layout yet - defaults match the original hardcoded behavior
    }

    settings.windowWidth = getIntOr(keyFile, "WindowWidth", settings.windowWidth);
    settings.windowHeight = getIntOr(keyFile, "WindowHeight", settings.windowHeight);
    settings.windowMaximized = getBoolOr(keyFile, "WindowMaximized", settings.windowMaximized);

    settings.leftSlotWidth = getIntOr(keyFile, "LeftSlotWidth", settings.leftSlotWidth);
    settings.rightSlotWidth = getIntOr(keyFile, "RightSlotWidth", settings.rightSlotWidth);
    settings.topSlotHeight = getIntOr(keyFile, "TopSlotHeight", settings.topSlotHeight);
    settings.bottomSlotHeight = getIntOr(keyFile, "BottomSlotHeight", settings.bottomSlotHeight);

    settings.hierarchyEdge = getEdgeOr(keyFile, "HierarchyEdge", settings.hierarchyEdge);
    settings.hierarchyFloating = getBoolOr(keyFile, "HierarchyFloating", settings.hierarchyFloating);
    settings.hierarchyFloatX = getIntOr(keyFile, "HierarchyFloatX", settings.hierarchyFloatX);
    settings.hierarchyFloatY = getIntOr(keyFile, "HierarchyFloatY", settings.hierarchyFloatY);
    settings.hierarchyFloatWidth = getIntOr(keyFile, "HierarchyFloatWidth", settings.hierarchyFloatWidth);
    settings.hierarchyFloatHeight = getIntOr(keyFile, "HierarchyFloatHeight", settings.hierarchyFloatHeight);

    settings.propertiesEdge = getEdgeOr(keyFile, "PropertiesEdge", settings.propertiesEdge);
    settings.propertiesFloating = getBoolOr(keyFile, "PropertiesFloating", settings.propertiesFloating);
    settings.propertiesFloatX = getIntOr(keyFile, "PropertiesFloatX", settings.propertiesFloatX);
    settings.propertiesFloatY = getIntOr(keyFile, "PropertiesFloatY", settings.propertiesFloatY);
    settings.propertiesFloatWidth = getIntOr(keyFile, "PropertiesFloatWidth", settings.propertiesFloatWidth);
    settings.propertiesFloatHeight = getIntOr(keyFile, "PropertiesFloatHeight", settings.propertiesFloatHeight);

    g_key_file_free(keyFile);
    return settings;
}

void save(const LayoutSettings& settings) {
    GKeyFile* keyFile = g_key_file_new();

    g_key_file_set_integer(keyFile, kGroup, "WindowWidth", settings.windowWidth);
    g_key_file_set_integer(keyFile, kGroup, "WindowHeight", settings.windowHeight);
    g_key_file_set_boolean(keyFile, kGroup, "WindowMaximized", settings.windowMaximized);

    g_key_file_set_integer(keyFile, kGroup, "LeftSlotWidth", settings.leftSlotWidth);
    g_key_file_set_integer(keyFile, kGroup, "RightSlotWidth", settings.rightSlotWidth);
    g_key_file_set_integer(keyFile, kGroup, "TopSlotHeight", settings.topSlotHeight);
    g_key_file_set_integer(keyFile, kGroup, "BottomSlotHeight", settings.bottomSlotHeight);

    g_key_file_set_integer(keyFile, kGroup, "HierarchyEdge", static_cast<int>(settings.hierarchyEdge));
    g_key_file_set_boolean(keyFile, kGroup, "HierarchyFloating", settings.hierarchyFloating);
    g_key_file_set_integer(keyFile, kGroup, "HierarchyFloatX", settings.hierarchyFloatX);
    g_key_file_set_integer(keyFile, kGroup, "HierarchyFloatY", settings.hierarchyFloatY);
    g_key_file_set_integer(keyFile, kGroup, "HierarchyFloatWidth", settings.hierarchyFloatWidth);
    g_key_file_set_integer(keyFile, kGroup, "HierarchyFloatHeight", settings.hierarchyFloatHeight);

    g_key_file_set_integer(keyFile, kGroup, "PropertiesEdge", static_cast<int>(settings.propertiesEdge));
    g_key_file_set_boolean(keyFile, kGroup, "PropertiesFloating", settings.propertiesFloating);
    g_key_file_set_integer(keyFile, kGroup, "PropertiesFloatX", settings.propertiesFloatX);
    g_key_file_set_integer(keyFile, kGroup, "PropertiesFloatY", settings.propertiesFloatY);
    g_key_file_set_integer(keyFile, kGroup, "PropertiesFloatWidth", settings.propertiesFloatWidth);
    g_key_file_set_integer(keyFile, kGroup, "PropertiesFloatHeight", settings.propertiesFloatHeight);

    g_key_file_save_to_file(keyFile, storePath().c_str(), nullptr);
    g_key_file_free(keyFile);
}

} // namespace regui::ui::layout_settings
