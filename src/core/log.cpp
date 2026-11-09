#include "core/log.h"

#include "platform/platform.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>

namespace {
FILE* g_file = nullptr;
char g_crumb[256] = "start-up";
}

namespace logx {

void open(const char* path) {
    // 0.30.0: the last run's log is kept beside it (ti4.prev.log) - after a crash, the game is restarted to
    // make the bug report, and that new run used to overwrite the log that showed the crash
    {
        std::string prev = path;
        const size_t dot = prev.rfind(".log");
        prev = dot == std::string::npos ? prev + ".prev" : prev.substr(0, dot) + ".prev.log";
        if (std::FILE* f = std::fopen(path, "rb")) {
            std::fclose(f);
            std::remove(prev.c_str());
            std::rename(path, prev.c_str());
        }
    }
    g_file = std::fopen(path, "w");
}

void write(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    timespec ts{};
    monotonicNow(&ts);
    char stamp[32];
    std::snprintf(stamp, sizeof stamp, "[%6ld.%03ld] ", (long)ts.tv_sec % 1000000, ts.tv_nsec / 1000000);
    std::fprintf(stderr, "%s%s\n", stamp, buf);
    if (g_file) {
        std::fprintf(g_file, "%s%s\n", stamp, buf);
        std::fflush(g_file);  // keep the log intact even if the app crashes
    }
}

void crumb(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(g_crumb, sizeof g_crumb, fmt, ap);
    va_end(ap);
}

const char* lastCrumb() { return g_crumb; }

int fd() { return g_file ? fileno(g_file) : -1; }

void close() {
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}

}  // namespace logx
