#include "app/bug_report.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "core/log.h"
#include "game/session.h"

namespace bugreport {

namespace {
std::string g_crashFolder;
}

std::string make(const char* suffix, bool gameLog) {
    char name[96];
    const std::time_t now = std::time(nullptr);
    const std::tm* t = std::localtime(&now);
    if (t) std::snprintf(name, sizeof name, "bug-report-%04d%02d%02d-%02d%02d%02d%s", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                         t->tm_hour, t->tm_min, t->tm_sec, suffix);
    else std::snprintf(name, sizeof name, "bug-report%s", suffix);
#ifdef _WIN32
    _mkdir(name);
#else
    mkdir(name, 0755);
#endif
    int copied = 0;
    for (const char* f : {"ti4.log", "ti4.prev.log", "savegame.ti4", "settings.cfg", "launch_log.txt"}) {
        std::FILE* in = std::fopen(f, "rb");
        if (!in) continue;
        std::FILE* out = std::fopen((std::string(name) + "/" + f).c_str(), "wb");
        if (out) {
            char buf[16384];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, in)) > 0) std::fwrite(buf, 1, n, out);
            std::fclose(out);
            ++copied;
        }
        std::fclose(in);
    }
    // 0.29.0: the saved game's own log, readable (what happened, turn by turn)
    if (gameLog) {
        GameSession gs;
        std::string err;
        if (GameSession::saveExists() && gs.loadSaved(&err)) {
            if (std::FILE* out = std::fopen((std::string(name) + "/game_log.txt").c_str(), "wb")) {
                for (const std::string& line : gs.game().log) {
                    std::fwrite(line.data(), 1, line.size(), out);
                    std::fputc('\n', out);
                }
                std::fclose(out);
                ++copied;
            }
        }
    }
    LOG("bug report: %d files copied into %s", copied, name);
    return copied ? std::string(name) : std::string();
}

bool afterCrash() {
    std::FILE* f = std::fopen("ti4.prev.log", "rb");
    if (!f) return false;
    bool crashed = false;
    char line[1100];
    while (std::fgets(line, sizeof line, f))
        if (std::strstr(line, "] CRASH: ")) crashed = true;
    std::fclose(f);
    if (!crashed) return false;
    LOG("the last run crashed: making a crash report");
    // (no game log here: replaying the save could be what crashed - the save itself is copied)
    g_crashFolder = make("-crash", false);
    return !g_crashFolder.empty();
}

const std::string& crashFolder() { return g_crashFolder; }

}  // namespace bugreport
