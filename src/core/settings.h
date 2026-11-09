// Small key=value settings file (settings.cfg in the app folder).
#pragma once
#include <map>
#include <string>

class Settings {
public:
    void load(const std::string& path);
    bool save() const;
    std::string get(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def) const;
    bool getBool(const std::string& key, bool def) const { return getInt(key, def ? 1 : 0) != 0; }
    void set(const std::string& key, const std::string& value) { kv_[key] = value; }
    void setInt(const std::string& key, int v) { kv_[key] = std::to_string(v); }
    void setBool(const std::string& key, bool v) { setInt(key, v ? 1 : 0); }

private:
    std::string path_;
    std::map<std::string, std::string> kv_;
};
