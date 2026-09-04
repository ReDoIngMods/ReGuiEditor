#include "ui/recent_files.h"

#include <glib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace regui::ui::recent_files {

namespace {

constexpr size_t kMaxEntries = 10;

// One path per line - just an ordered list, no need for the JSON format here.
std::string storePath() {
    std::string dir = std::string(g_get_user_config_dir()) + "/reguieditor";
    g_mkdir_with_parents(dir.c_str(), 0755);
    return dir + "/recent-files.txt";
}

} // namespace

std::vector<std::string> load() {
    std::vector<std::string> result;
    std::ifstream in(storePath());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && std::filesystem::exists(line)) {
            result.push_back(line);
        }
    }
    return result;
}

void addAndSave(const std::string& path) {
    std::vector<std::string> entries = load(); // already existence-filtered
    entries.erase(std::remove(entries.begin(), entries.end(), path), entries.end());
    entries.insert(entries.begin(), path);
    if (entries.size() > kMaxEntries) {
        entries.resize(kMaxEntries);
    }

    std::ofstream out(storePath(), std::ios::trunc);
    for (const std::string& entry : entries) {
        out << entry << "\n";
    }
}

} // namespace regui::ui::recent_files
