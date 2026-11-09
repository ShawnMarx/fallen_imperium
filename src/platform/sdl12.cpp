// SDL 1.2 backend. SDL is not linked at build time: the device's own
// libSDL-1.2.so.0 is opened at runtime, so the program has no hard dependency
// on it and can fall back to fbdev when it's missing.
#include <dlfcn.h>

#include <cstdio>
#include <string>

#include <SDL/SDL.h>

#include "core/log.h"
#include "platform/pixelconv.h"
#include "platform/platform.h"

namespace {

struct SdlApi {
    int (*Init)(Uint32);
    void (*Quit)();
    SDL_Surface* (*SetVideoMode)(int, int, int, Uint32);
    int (*Flip)(SDL_Surface*);
    int (*PollEvent)(SDL_Event*);
    int (*ShowCursor)(int);
    int (*LockSurface)(SDL_Surface*);
    void (*UnlockSurface)(SDL_Surface*);
    char* (*GetError)();
    int (*EnableKeyRepeat)(int, int);
};

Btn mapSdlKey(int sym) {
    switch (sym) {
        case SDLK_UP: return Btn::Up;
        case SDLK_DOWN: return Btn::Down;
        case SDLK_LEFT: return Btn::Left;
        case SDLK_RIGHT: return Btn::Right;
        case SDLK_SPACE: return Btn::A;
        case SDLK_LCTRL: return Btn::B;
        case SDLK_LSHIFT: return Btn::X;
        case SDLK_LALT: return Btn::Y;
        case SDLK_e: return Btn::L1;
        case SDLK_t: return Btn::R1;
        case SDLK_TAB: return Btn::L2;
        case SDLK_BACKSPACE: return Btn::R2;
        case SDLK_RCTRL: return Btn::Select;
        case SDLK_RETURN: return Btn::Start;
        case SDLK_ESCAPE: return Btn::Menu;
        case SDLK_POWER: return Btn::Power;
        default: return Btn::None;
    }
}

class SdlPlatform : public Platform {
public:
    const char* name() const override { return "sdl"; }

    bool init() override {
        static const char* candidates[] = {
            "libSDL-1.2.so.0",
            "/mnt/SDCARD/.tmp_update/lib/libSDL-1.2.so.0",
            "/mnt/SDCARD/miyoo/lib/libSDL-1.2.so.0",
            "/customer/lib/libSDL-1.2.so.0",
            "/config/lib/libSDL-1.2.so.0",
            "./lib/libSDL-1.2.so.0",
        };
        for (const char* c : candidates) {
            lib_ = dlopen(c, RTLD_NOW | RTLD_GLOBAL);
            if (lib_) {
                libPath_ = c;
                break;
            }
            LOG("sdl: dlopen %s failed: %s", c, dlerror());
        }
        if (!lib_) return false;
        LOG("sdl: loaded %s", libPath_.c_str());
        bool ok = true;
        auto load = [&](const char* sym) -> void* {
            void* p = dlsym(lib_, sym);
            if (!p) {
                LOG("sdl: missing symbol %s", sym);
                ok = false;
            }
            return p;
        };
        api_.Init = reinterpret_cast<int (*)(Uint32)>(load("SDL_Init"));
        api_.Quit = reinterpret_cast<void (*)()>(load("SDL_Quit"));
        api_.SetVideoMode = reinterpret_cast<SDL_Surface* (*)(int, int, int, Uint32)>(load("SDL_SetVideoMode"));
        api_.Flip = reinterpret_cast<int (*)(SDL_Surface*)>(load("SDL_Flip"));
        api_.PollEvent = reinterpret_cast<int (*)(SDL_Event*)>(load("SDL_PollEvent"));
        api_.ShowCursor = reinterpret_cast<int (*)(int)>(load("SDL_ShowCursor"));
        api_.LockSurface = reinterpret_cast<int (*)(SDL_Surface*)>(load("SDL_LockSurface"));
        api_.UnlockSurface = reinterpret_cast<void (*)(SDL_Surface*)>(load("SDL_UnlockSurface"));
        api_.GetError = reinterpret_cast<char* (*)()>(load("SDL_GetError"));
        api_.EnableKeyRepeat = reinterpret_cast<int (*)(int, int)>(load("SDL_EnableKeyRepeat"));
        if (!ok) return false;

        if (api_.Init(SDL_INIT_VIDEO) != 0) {
            LOG("sdl: SDL_Init failed: %s", api_.GetError());
            return false;
        }
        inited_ = true;
        api_.ShowCursor(SDL_DISABLE);
        api_.EnableKeyRepeat(0, 0);
        screen_ = api_.SetVideoMode(640, 480, 32, SDL_HWSURFACE);
        if (!screen_) {
            LOG("sdl: SDL_SetVideoMode failed: %s", api_.GetError());
            return false;
        }
        const SDL_PixelFormat* pf = screen_->format;
        fmt_.bpp = pf->BitsPerPixel;
        fmt_.rShift = pf->Rshift;
        fmt_.gShift = pf->Gshift;
        fmt_.bShift = pf->Bshift;
        fmt_.rBits = 8 - pf->Rloss;
        fmt_.gBits = 8 - pf->Gloss;
        fmt_.bBits = 8 - pf->Bloss;
        LOG("sdl: screen %dx%d bpp=%d pitch=%d R%d G%d B%d flags=0x%x", screen_->w, screen_->h, fmt_.bpp,
            screen_->pitch, fmt_.rShift, fmt_.gShift, fmt_.bShift, screen_->flags);
        if (fmt_.bpp != 32 && fmt_.bpp != 16) {
            LOG("sdl: unsupported bpp");
            return false;
        }
        return true;
    }

    void shutdown() override {
        if (inited_) api_.Quit();
        inited_ = false;
        // The library stays loaded: unloading SDL while its threads may still
        // be finishing has caused crashes on some handhelds.
    }

    void poll(std::vector<KeyEvent>& out, bool& quit) override {
        SDL_Event e;
        while (api_.PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                int sym = int(e.key.keysym.sym);
                out.push_back({mapSdlKey(sym), e.type == SDL_KEYDOWN, sym});
            }
        }
    }

    void present(const Canvas& c, bool rotate) override {
        if (api_.LockSurface(screen_) != 0) return;
        convertFrame(c, static_cast<uint8_t*>(screen_->pixels), screen_->pitch, screen_->w, screen_->h, fmt_,
                     rotate);
        api_.UnlockSurface(screen_);
        api_.Flip(screen_);
    }

    bool defaultRotate() const override { return false; }

    std::string rawKeyName(int raw) const override { return "SDLK " + std::to_string(raw); }

    std::string describe() const override {
        char buf[512];
        std::snprintf(buf, sizeof buf,
                      "Backend: SDL 1.2 (%s)\n"
                      "Screen: %dx%d, %d bits per pixel\n"
                      "Colour layout: R%d G%d B%d (offsets)",
                      libPath_.c_str(), screen_ ? screen_->w : 0, screen_ ? screen_->h : 0, fmt_.bpp, fmt_.rShift,
                      fmt_.gShift, fmt_.bShift);
        return buf;
    }

    uint32_t ticks() override { return monotonicMs(); }

    void waitFrame(uint32_t start, bool idle) override {
        const uint32_t budget = idle ? 33 : 16;
        uint32_t spent = monotonicMs() - start;
        if (spent < budget) sleepMs(budget - spent);
    }

private:
    void* lib_ = nullptr;
    std::string libPath_;
    SdlApi api_{};
    bool inited_ = false;
    SDL_Surface* screen_ = nullptr;
    PixelFormatDesc fmt_;
};

}  // namespace

std::unique_ptr<Platform> makeSdlPlatform() { return std::unique_ptr<Platform>(new SdlPlatform()); }
