#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace regui::ui::asset_setup {

// Resolves the Scrap Mechanic "Data/Gui" assets path: uses a previously
// stored path if it's still valid, else searches known Steam/install
// locations, else asks the user to browse to the game's install folder (and
// stores whatever is found/chosen for next time).
//
// Calls `onReady(guiPath)` once resolved - synchronously if already known or
// found, or later if the user has to be prompted. If the user gives up when
// prompted, quits `app` instead of ever calling onReady().
void resolveGuiAssetsPath(GtkApplication* app, std::function<void(std::string)> onReady);

} // namespace regui::ui::asset_setup
