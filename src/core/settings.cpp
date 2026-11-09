#include "core/settings.h"

#include <cstdlib>
#include <fstream>

#include "core/log.h"

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

void Settings::load(const std::string& path) {
    path_ = path;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    LOG("settings: loaded %zu values from %s", kv_.size(), path.c_str());
}

bool Settings::save() const {
    std::ofstream out(path_, std::ios::binary);
    if (!out) {
        LOG("settings: could not write %s", path_.c_str());
        return false;
    }
    out << "# Twilight Imperium (Miyoo) settings\n";
    for (const auto& kv : kv_) out << kv.first << "=" << kv.second << "\n";
    return true;
}

std::string Settings::get(const std::string& key, const std::string& def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : it->second;
}

int Settings::getInt(const std::string& key, int def) const {
    auto it = kv_.find(key);
    if (it == kv_.end() || it->second.empty()) return def;
    return std::atoi(it->second.c_str());
}
