// System info: device details plus a quick drawing benchmark. A photo of this
// screen tells us most of what we need to tune the full game.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "app/app.h"
#include "audio/audio.h"
#include "core/log.h"
#include "gfx/font.h"
#include "engine/engine.h"
#include "ui/map_view.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

#ifndef _WIN32
std::string readField(const char* path, const char* key) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, std::char_traits<char>::length(key), key) == 0) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string v = line.substr(colon + 1);
            size_t a = v.find_first_not_of(" \t");
            return a == std::string::npos ? "" : v.substr(a);
        }
    }
    return "";
}
#endif

class SystemInfoScene : public Scene {
public:
    explicit SystemInfoScene(App& app) {
        lines_.push_back({"Version", TI4_VERSION " (" TI4_STAGE ")"});
        lines_.push_back({"Sound", audio::status()});
        lines_.push_back({"Built", __DATE__ " " __TIME__});
        std::string desc = app.platform().describe();
        size_t start = 0;
        while (start < desc.size()) {
            size_t nl = desc.find('\n', start);
            std::string l = desc.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            size_t colon = l.find(": ");
            if (colon != std::string::npos) lines_.push_back({l.substr(0, colon), l.substr(colon + 2)});
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
#ifdef _WIN32
        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);
        lines_.push_back({"CPU", std::to_string(si.dwNumberOfProcessors) + " logical processors"});
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof ms;
        if (GlobalMemoryStatusEx(&ms))
            lines_.push_back({"Memory", std::to_string(ms.ullTotalPhys >> 20) + " MB total, " +
                                            std::to_string(ms.ullAvailPhys >> 20) + " MB free"});
        char cwd[MAX_PATH] = "?";
        GetCurrentDirectoryA(sizeof cwd, cwd);
        lines_.push_back({"App folder", cwd});
#else
        std::string cpu = readField("/proc/cpuinfo", "Hardware");
        if (cpu.empty()) cpu = readField("/proc/cpuinfo", "model name");
        std::string cores = std::to_string(sysconf(_SC_NPROCESSORS_ONLN));
        lines_.push_back({"CPU", cpu + " (" + cores + " cores)"});
        std::string freq;
        {
            std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
            long khz = 0;
            if (f >> khz) freq = std::to_string(khz / 1000) + " MHz";
        }
        if (!freq.empty()) lines_.push_back({"CPU speed", freq});
        lines_.push_back({"Memory", readField("/proc/meminfo", "MemTotal") + " total, " +
                                        readField("/proc/meminfo", "MemAvailable") + " free"});
        utsname u{};
        if (uname(&u) == 0) lines_.push_back({"Kernel", std::string(u.release) + " " + u.machine});
        char cwd[256] = "?";
        if (getcwd(cwd, sizeof cwd)) lines_.push_back({"App folder", cwd});
#endif
        runBenchmark();
        for (const auto& l : lines_) LOG("sysinfo: %s: %s", l.first.c_str(), l.second.c_str());
    }

    void update(App& app, const Input& in, uint32_t) override {
        if (in.pressed(Btn::B) || in.pressed(Btn::Menu)) {
            app.pop();
            return;
        }
        if (in.pressed(Btn::X) && app.platform().screenRescue()) {
            std::string b = app.settings().get("backend", "auto");
            b = b == "auto" ? "fbdev" : b == "fbdev" ? "sdl" : "auto";
            app.settings().set("backend", b);
            app.settings().save();
            LOG("sysinfo: backend preference now %s", b.c_str());
        }
    }

    void draw(App& app, Canvas& c, uint32_t) override {
        drawSpaceBackground(c, false);
        drawPanel(c, Rect{10, 10, 620, 432}, theme::panel, theme::border, 10);
        drawText(c, FONT_LG, 24, 18, "System info", theme::gold);
        int y = 50;
        for (const auto& l : lines_) {
            drawText(c, FONT_SMB, 24, y, l.first, theme::dim);
            drawText(c, FONT_SM, 150, y, fitText(FONT_SM, l.second, 465), theme::text);
            y += 18;
        }
        y += 6;
        c.hline(24, 616, y, theme::border);
        y += 8;
        drawText(c, FONT_SMB, 24, y, "Speed", theme::gold);
        y += 20;
        char buf[160];
        std::snprintf(buf, sizeof buf, "Drawing the galaxy: %.1f ms (normal zoom), %.1f ms (closest zoom)", benchNormal_,
                      benchClose_);
        drawText(c, FONT_SM, 24, y, buf, theme::text);
        y += 18;
        std::snprintf(buf, sizeof buf, "Sending a frame to the screen: %.1f ms", app.presentMs());
        drawText(c, FONT_SM, 24, y, buf, theme::text);
        y += 26;
        if (app.platform().screenRescue()) {
            std::string pref = app.settings().get("backend", "auto");
            drawText(c, FONT_SM, 24, y, "Screen method for next launch: " + pref + "   (X to change)", theme::dim);
            y += 18;
        }
        drawText(c, FONT_SM, 24, y, "A log is written to ti4.log in the app folder.", theme::dim);
        if (app.platform().screenRescue()) drawHintBar(c, {{"X", "Screen method"}, {"B", "Back"}});
        else drawHintBar(c, {{"B", "Back"}});
    }

private:
    void runBenchmark() {
        // Draw the map of a freshly set-up 6-player game a few times.
        ti::Engine e;
        ti::Setup s;
        s.players = 6;
        s.human = -1;
        s.seed = 7;
        e.start(s);
        Canvas off(640, 480);
        MapView v;
        v.setViewport(Rect{0, 22, 420, 430});
        auto timeIt = [&](int zoom) {
            v.setZoom(zoom);
            v.setCursor(e.game(), std::max(0, e.game().mecatol()));
            const int n = 12;
            uint32_t t0 = monotonicMs();
            for (int i = 0; i < n; ++i) {
                drawSpaceBackground(off, false);
                v.draw(off, e.game(), uint32_t(i * 50));
            }
            return float(monotonicMs() - t0) / float(n);
        };
        benchNormal_ = timeIt(1);
        benchClose_ = timeIt(3);
        LOG("sysinfo: map draw %.2f ms (zoom 1), %.2f ms (zoom 3)", benchNormal_, benchClose_);
    }

    std::vector<std::pair<std::string, std::string>> lines_;
    float benchNormal_ = 0, benchClose_ = 0;
};

}  // namespace

std::unique_ptr<Scene> makeSystemInfoScene(App& app) { return std::unique_ptr<Scene>(new SystemInfoScene(app)); }
