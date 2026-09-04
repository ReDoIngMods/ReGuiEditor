#include "ui/game_assets.h"

#include <glib.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace regui::ui::game_assets {

namespace {

// Same settings directory as recent_files.cpp/layout_settings.cpp.
std::string storePath() {
    std::string dir = std::string(g_get_user_config_dir()) + "/reguieditor";
    g_mkdir_with_parents(dir.c_str(), 0755);
    return dir + "/game-gui-path.txt";
}

#ifdef _WIN32
std::optional<std::string> readRegistryString(HKEY root, const char* subKey, const char* valueName) {
    HKEY key;
    if (RegOpenKeyExA(root, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    char buffer[2048] = {};
    DWORD size = sizeof(buffer) - 1;
    DWORD type = 0;
    LONG result = RegQueryValueExA(key, valueName, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return std::nullopt;
    }
    return std::string(buffer);
}

// Crude line scan for `"path"		"D:\\SteamLibrary"` entries - a full VDF/
// KeyValues parser is overkill for pulling out one field.
std::vector<std::string> parseLibraryFoldersVdf(const std::string& vdfPath) {
    std::vector<std::string> paths;
    std::ifstream in(vdfPath);
    std::string line;
    while (std::getline(in, line)) {
        size_t keyPos = line.find("\"path\"");
        if (keyPos == std::string::npos) {
            continue;
        }
        size_t valueStart = line.find('"', keyPos + 6);
        if (valueStart == std::string::npos) {
            continue;
        }
        size_t valueEnd = line.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) {
            continue;
        }
        std::string raw = line.substr(valueStart + 1, valueEnd - valueStart - 1);
        std::string unescaped;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') {
                unescaped += '\\';
                ++i;
            } else {
                unescaped += raw[i];
            }
        }
        paths.push_back(std::move(unescaped));
    }
    return paths;
}

std::vector<char> availableDriveLetters() {
    std::vector<char> drives;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (mask & (1u << i)) {
            drives.push_back(static_cast<char>('A' + i));
        }
    }
    return drives;
}

// Relative to a drive root - covers the default Steam location plus the
// handful of spots people commonly point a second Steam library at.
constexpr const char* kCommonRelativeGameRoots[] = {
    "Program Files (x86)\\Steam\\steamapps\\common\\Scrap Mechanic",
    "Program Files\\Steam\\steamapps\\common\\Scrap Mechanic",
    "SteamLibrary\\steamapps\\common\\Scrap Mechanic",
    "Steam\\steamapps\\common\\Scrap Mechanic",
    "Games\\Steam\\steamapps\\common\\Scrap Mechanic",
    "Games\\Scrap Mechanic",
    "Scrap Mechanic",
};
#endif

} // namespace

bool isValidGuiPath(const std::string& guiPath) {
    std::error_code ec;
    return std::filesystem::exists(guiPath + "/GuiConfig.xml", ec);
}

std::optional<std::string> guiPathFromGameRoot(const std::string& gameRoot) {
    std::string guiPath = gameRoot + "/Data/Gui";
    if (isValidGuiPath(guiPath)) {
        return guiPath;
    }
    return std::nullopt;
}

std::optional<std::string> loadStoredGuiPath() {
    std::ifstream in(storePath());
    std::string line;
    if (std::getline(in, line) && !line.empty() && isValidGuiPath(line)) {
        std::cout << "Loaded stored GUI path: " << line << "\n";
        return line;
    }
    std::cout << "No valid stored GUI path found.\n";
    return std::nullopt;
}

void storeGuiPath(const std::string& guiPath) {
    std::ofstream out(storePath(), std::ios::trunc);
    out << guiPath << "\n";

    std::cout << "Stored assets path: " << guiPath << "\n";
}

std::optional<std::string> searchKnownLocations() {
#ifdef _WIN32
    std::vector<std::string> steamRoots;
    if (auto steamPath = readRegistryString(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath")) {
        steamRoots.push_back(*steamPath);
    }
    if (auto installPath =
            readRegistryString(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath")) {
        steamRoots.push_back(*installPath);
    }
    if (auto installPath = readRegistryString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath")) {
        steamRoots.push_back(*installPath);
    }

    std::vector<std::string> libraries = steamRoots;
    for (const std::string& steamRoot : steamRoots) {
        for (std::string& extra : parseLibraryFoldersVdf(steamRoot + "/steamapps/libraryfolders.vdf")) {
            libraries.push_back(std::move(extra));
        }
    }

    for (const std::string& library : libraries) {
        if (auto guiPath = guiPathFromGameRoot(library + "/steamapps/common/Scrap Mechanic")) {
            return guiPath;
        }
    }

    // Fallback: a handful of common install spots on every drive, for
    // portable/non-registry Steam installs (or a non-Steam copy of the game).
    for (char drive : availableDriveLetters()) {
        for (const char* relative : kCommonRelativeGameRoots) {
            if (auto guiPath = guiPathFromGameRoot(std::string(1, drive) + ":\\" + relative)) {
                return guiPath;
            }
        }
    }
#endif
    return std::nullopt;
}

} // namespace regui::ui::game_assets
