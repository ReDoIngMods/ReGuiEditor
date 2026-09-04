#pragma once

#include <optional>
#include <string>

namespace regui::ui::game_assets {

// Stored/validated path to Scrap Mechanic's "Data/Gui" folder (not the game
// root) - this is what the editor used to require a manual local copy of
// under assets/scrapmechanic-gui. Returns nullopt if nothing is stored, or
// the stored path no longer exists/validates.
std::optional<std::string> loadStoredGuiPath();

// Persists `guiPath` (the "Data/Gui" folder itself) for future launches.
void storeGuiPath(const std::string& guiPath);

// True if `guiPath` looks like Scrap Mechanic's actual Gui folder (i.e. has
// a GuiConfig.xml directly inside it).
bool isValidGuiPath(const std::string& guiPath);

// If `gameRoot` is a valid Scrap Mechanic install (`gameRoot/Data/Gui`
// passes isValidGuiPath), returns that Gui path; else nullopt.
std::optional<std::string> guiPathFromGameRoot(const std::string& gameRoot);

// Searches Steam's library folders (via the registry + libraryfolders.vdf)
// and a handful of common install locations across every drive letter.
// Windows-only; always nullopt elsewhere.
std::optional<std::string> searchKnownLocations();

} // namespace regui::ui::game_assets
