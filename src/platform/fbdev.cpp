// Direct framebuffer + evdev backend (no libraries). Mirrors how OnionOS's
// own utilities draw: the frame is written, turned 180 degrees, into every
// page of /dev/fb0 so it shows no matter which page the display is scanning.
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/log.h"
#include "platform/pixelconv.h"
#include "platform/platform.h"

namespace {

Btn mapLinuxKey(int code) {
    switch (code) {
        case KEY_UP: return Btn::Up;
        case KEY_DOWN: return Btn::Down;
        case KEY_LEFT: return Btn::Left;
        case KEY_RIGHT: return Btn::Right;
        case KEY_SPACE: return Btn::A;
        case KEY_LEFTCTRL: return Btn::B;
        case KEY_LEFTSHIFT: return Btn::X;
        case KEY_LEFTALT: return Btn::Y;
        case KEY_E: return Btn::L1;
        case KEY_T: return Btn::R1;
        case KEY_TAB: return Btn::L2;
        case KEY_BACKSPACE: return Btn::R2;
        case KEY_RIGHTCTRL: return Btn::Select;
        case KEY_ENTER: return Btn::Start;
        case KEY_ESC: return Btn::Menu;
        case KEY_POWER: return Btn::Power;
        case KEY_VOLUMEUP: return Btn::VolUp;
        case KEY_VOLUMEDOWN: return Btn::VolDown;
        default: return Btn::None;
    }
}

const char* linuxKeyName(int code) {
    switch (code) {
        case KEY_UP: return "KEY_UP";
        case KEY_DOWN: return "KEY_DOWN";
        case KEY_LEFT: return "KEY_LEFT";
        case KEY_RIGHT: return "KEY_RIGHT";
        case KEY_SPACE: return "KEY_SPACE";
        case KEY_LEFTCTRL: return "KEY_LEFTCTRL";
        case KEY_LEFTSHIFT: return "KEY_LEFTSHIFT";
        case KEY_LEFTALT: return "KEY_LEFTALT";
        case KEY_E: return "KEY_E";
        case KEY_T: return "KEY_T";
        case KEY_TAB: return "KEY_TAB";
        case KEY_BACKSPACE: return "KEY_BACKSPACE";
        case KEY_RIGHTCTRL: return "KEY_RIGHTCTRL";
        case KEY_ENTER: return "KEY_ENTER";
        case KEY_ESC: return "KEY_ESC";
        case KEY_POWER: return "KEY_POWER";
        case KEY_VOLUMEUP: return "KEY_VOLUMEUP";
        case KEY_VOLUMEDOWN: return "KEY_VOLUMEDOWN";
        default: return nullptr;
    }
}

class FbdevPlatform : public Platform {
public:
    const char* name() const override { return "fbdev"; }

    bool init() override {
        fb_ = open("/dev/fb0", O_RDWR);
        if (fb_ < 0) {
            LOG("fbdev: cannot open /dev/fb0 (%s)", std::strerror(errno));
            return false;
        }
        if (ioctl(fb_, FBIOGET_VSCREENINFO, &var_) < 0 || ioctl(fb_, FBIOGET_FSCREENINFO, &fix_) < 0) {
            LOG("fbdev: screen info ioctl failed (%s)", std::strerror(errno));
            return false;
        }
        LOG("fbdev: %ux%u virtual %ux%u bpp=%u line=%u mem=%u r=%u/%u g=%u/%u b=%u/%u a=%u/%u", var_.xres,
            var_.yres, var_.xres_virtual, var_.yres_virtual, var_.bits_per_pixel, fix_.line_length,
            fix_.smem_len, var_.red.offset, var_.red.length, var_.green.offset, var_.green.length,
            var_.blue.offset, var_.blue.length, var_.transp.offset, var_.transp.length);
        if (var_.bits_per_pixel != 32 && var_.bits_per_pixel != 16) {
            LOG("fbdev: unsupported bpp %u", var_.bits_per_pixel);
            return false;
        }
        fmt_.bpp = int(var_.bits_per_pixel);
        fmt_.rShift = int(var_.red.offset);
        fmt_.gShift = int(var_.green.offset);
        fmt_.bShift = int(var_.blue.offset);
        fmt_.rBits = int(var_.red.length);
        fmt_.gBits = int(var_.green.length);
        fmt_.bBits = int(var_.blue.length);
        pages_ = var_.yres ? int(var_.yres_virtual / var_.yres) : 1;
        if (pages_ < 1) pages_ = 1;
        pageBytes_ = size_t(fix_.line_length) * var_.yres;
        if (pageBytes_ * pages_ > fix_.smem_len) pages_ = int(fix_.smem_len / pageBytes_);
        if (pages_ < 1) {
            LOG("fbdev: framebuffer memory too small");
            return false;
        }
        mem_ = static_cast<uint8_t*>(mmap(nullptr, fix_.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_, 0));
        if (mem_ == MAP_FAILED) {
            mem_ = nullptr;
            LOG("fbdev: mmap failed (%s)", std::strerror(errno));
            return false;
        }
        staging_.resize(pageBytes_);
        openInputs();
        if (inputs_.empty()) {
            LOG("fbdev: no readable /dev/input/event* devices");
            return false;
        }
        return true;
    }

    void shutdown() override {
        if (mem_ && pages_ >= 2 && flipChecked_ && flip_ && front_ != 0) {  // leave the first page showing
            fb_var_screeninfo v = var_;
            v.xoffset = 0;
            v.yoffset = 0;
            ioctl(fb_, FBIOPAN_DISPLAY, &v);
        }
        if (mem_) {
            std::memset(mem_, 0, size_t(pages_) * pageBytes_);
            munmap(mem_, fix_.smem_len);
            mem_ = nullptr;
        }
        if (fb_ >= 0) close(fb_);
        fb_ = -1;
        for (int fd : inputs_) close(fd);
        inputs_.clear();
    }

    void poll(std::vector<KeyEvent>& out, bool& quit) override {
        (void)quit;
        input_event ev[32];
        for (int fd : inputs_) {
            for (;;) {
                ssize_t n = read(fd, ev, sizeof ev);
                if (n <= 0) break;
                int count = int(n / ssize_t(sizeof(input_event)));
                for (int i = 0; i < count; ++i) {
                    if (ev[i].type != EV_KEY) continue;
                    if (ev[i].value == 2) continue;  // kernel auto-repeat; we do our own
                    out.push_back({mapLinuxKey(ev[i].code), ev[i].value != 0, int(ev[i].code)});
                }
            }
        }
    }

    void present(const Canvas& c, bool rotate) override {
        convertFrame(c, staging_.data(), int(fix_.line_length), int(var_.xres), int(var_.yres), fmt_, rotate);
        // (0.32.1, owner: tearing) two pages: draw into the hidden one, then show it (the screen switches
        // at its refresh). Checked on the first flip; if the driver doesn't follow, every page as before.
        if (pages_ >= 2 && flip_) {
            const int back = (front_ + 1) % 2;
            std::memcpy(mem_ + size_t(back) * pageBytes_, staging_.data(), pageBytes_);
            fb_var_screeninfo v = var_;
            v.xoffset = 0;
            v.yoffset = uint32_t(back) * var_.yres;
            bool ok = ioctl(fb_, FBIOPAN_DISPLAY, &v) == 0;
            if (ok && !flipChecked_) {
                fb_var_screeninfo now{};
                ok = ioctl(fb_, FBIOGET_VSCREENINFO, &now) == 0 && now.yoffset == v.yoffset;
                flipChecked_ = true;
                LOG("fbdev: page flipping %s", ok ? "on" : "not followed - copying to every page");
            }
            if (ok) {
                front_ = back;
                return;
            }
            flip_ = false;
            LOG("fbdev: page flip failed (%s) - copying to every page", std::strerror(errno));
        }
        for (int p = 0; p < pages_; ++p) std::memcpy(mem_ + size_t(p) * pageBytes_, staging_.data(), pageBytes_);
    }

    bool defaultRotate() const override { return true; }

    std::string rawKeyName(int raw) const override {
        const char* n = linuxKeyName(raw);
        return n ? std::string(n) + " (" + std::to_string(raw) + ")" : "code " + std::to_string(raw);
    }

    std::string describe() const override {
        char buf[512];
        std::snprintf(buf, sizeof buf,
                      "Backend: direct framebuffer (fbdev)\n"
                      "Screen: %ux%u, %u bits per pixel, %d page(s)\n"
                      "Colour layout: R%u G%u B%u (offsets)\n"
                      "Input devices: %s",
                      var_.xres, var_.yres, var_.bits_per_pixel, pages_, var_.red.offset, var_.green.offset,
                      var_.blue.offset, inputNames_.c_str());
        return buf;
    }

    uint32_t ticks() override { return monotonicMs(); }

    void waitFrame(uint32_t start, bool idle) override {
        const uint32_t budget = idle ? 33 : 16;  // ~30 fps when nothing moves, ~60 when animating
        uint32_t spent = monotonicMs() - start;
        if (spent < budget) sleepMs(budget - spent);
    }

private:
    int front_ = 0;            // the page on the screen (page flipping, 0.32.1)
    bool flip_ = true, flipChecked_ = false;
    void openInputs() {
        DIR* d = opendir("/dev/input");
        if (!d) return;
        while (dirent* e = readdir(d)) {
            if (std::strncmp(e->d_name, "event", 5) != 0) continue;
            std::string path = std::string("/dev/input/") + e->d_name;
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            char nm[128] = "?";
            ioctl(fd, EVIOCGNAME(sizeof nm), nm);
            LOG("fbdev: input %s = \"%s\"", path.c_str(), nm);
            inputs_.push_back(fd);
            if (!inputNames_.empty()) inputNames_ += ", ";
            inputNames_ += std::string(e->d_name) + " \"" + nm + "\"";
        }
        closedir(d);
    }

    int fb_ = -1;
    fb_var_screeninfo var_{};
    fb_fix_screeninfo fix_{};
    PixelFormatDesc fmt_;
    uint8_t* mem_ = nullptr;
    int pages_ = 1;
    size_t pageBytes_ = 0;
    std::vector<uint8_t> staging_;
    std::vector<int> inputs_;
    std::string inputNames_;
};

}  // namespace

std::unique_ptr<Platform> makeFbdevPlatform() { return std::unique_ptr<Platform>(new FbdevPlatform()); }
