#pragma once

#include <string>
#include <vector>

namespace regui::ui::recent_files {

// Persisted "recently opened" list, most-recent first. Missing files are dropped silently.
std::vector<std::string> load();

// Moves `path` to the front (inserting if new), caps the list, and saves immediately.
void addAndSave(const std::string& path);

} // namespace regui::ui::recent_files
