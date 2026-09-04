#pragma once

// Shown in the About dialog (File menu).
namespace regui {

constexpr const char* kAppName = "ReGui Editor";
constexpr const char* kAppVersion = "0.1.1";
constexpr const char* kAppDescription =
    "The official editor for creating and modifying sm.regui layouts.";

// Null-terminated.
constexpr const char* kAppCreators[] = {
    "UchStalker",
    "The ReDoIng Mods Team",
    nullptr,
};

constexpr const char* kLicenseUrl = "https://github.com/ReDoIngMods/ReGuiEditor?tab=MPL-2.0-1-ov-file";

} // namespace regui
