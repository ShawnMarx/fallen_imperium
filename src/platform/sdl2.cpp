// SDL2 backend for the Linux desktop / Steam Deck build (owner, 2026-10-24:
// a one-off build to play on his Steam Deck). Only compiled with TI4_SDL2.
// SDL2 is opened at runtime (libSDL2-2.0.so.0 - SteamOS has it), so the
// program has no build-time dependency on it.
//
// The picture (0.27.0): Picture = Sharp draws at the screen's resolution (4:3,
// shown pixel for pixel - 1067x800 on the Deck); Original draws 640x480 and
// scales it up, as before.
// Controls:
//   Steam Deck / gamepad: D-pad or left stick, A B X Y, L1 / R1 (bumpers),
//     L2 / R2 (triggers), View = START, Menu = MENU, stick clicks (L3 / R3)
//     = SELECT, right stick = the map camera.
//   Keyboard & mouse (as the Windows version, 0.27.0): WASD moves the map
//     camera (Controls = Controller: WASD is the D-pad), arrows = D-pad, Space
//     A, Backspace B, Esc = the game menu, Tab = view the map, X, V = Y, Q / E =
//     L1 / R1, Z / C = L2 / R2, Enter START, R SELECT, F11 full screen.
//   Mouse / touch screen: point, left click (tap) = A, right drag = camera,
//     right click = B, wheel = smooth zoom.
#ifdef TI4_SDL2
#ifndef __EMSCRIPTEN__
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "app/app.h"
#include "core/log.h"
#include "core/settings.h"
#include "platform/platform.h"

namespace {

#ifdef __EMSCRIPTEN__
constexpr bool kWeb = true;  // F11 is left to the browser
#else
constexpr bool kWeb = false;
#endif

struct Api {
    int (*Init)(Uint32);
    void (*Quit)();
    const char* (*GetError)();
    SDL_bool (*SetHint)(const char*, const char*);
    int (*GetDesktopDisplayMode)(int, SDL_DisplayMode*);
    SDL_Window* (*CreateWindow)(const char*, int, int, int, int, Uint32);
    void (*DestroyWindow)(SDL_Window*);
    int (*SetWindowFullscreen)(SDL_Window*, Uint32);
    Uint32 (*GetWindowFlags)(SDL_Window*);
    SDL_Renderer* (*CreateRenderer)(SDL_Window*, int, Uint32);
    void (*DestroyRenderer)(SDL_Renderer*);
    int (*RenderSetLogicalSize)(SDL_Renderer*, int, int);
    int (*SetRenderDrawColor)(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8);
    int (*RenderClear)(SDL_Renderer*);
    int (*RenderCopy)(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*);
    void (*RenderPresent)(SDL_Renderer*);
    SDL_Texture* (*CreateTexture)(SDL_Renderer*, Uint32, int, int, int);
    void (*DestroyTexture)(SDL_Texture*);
    int (*UpdateTexture)(SDL_Texture*, const SDL_Rect*, const void*, int);
    int (*PollEvent)(SDL_Event*);
    int (*NumJoysticks)();
    SDL_bool (*IsGameController)(int);
    SDL_GameController* (*GameControllerOpen)(int);
    const char* (*GameControllerName)(SDL_GameController*);
    const char* (*GetKeyName)(SDL_Keycode);
    int (*ShowCursor)(int);
    int (*GetRendererOutputSize)(SDL_Renderer*, int*, int*);
    void (*GetWindowSize)(SDL_Window*, int*, int*);
    void (*GetVersion)(SDL_version*);
    void (*SetWindowSize)(SDL_Window*, int, int);
    void (*SetWindowPosition)(SDL_Window*, int, int);
};

// Raw codes in KeyEvent: keyboard = SDL keycode, pad buttons and axes above these.
constexpr int kRawPad = 0x20000, kRawAxis = 0x21000;

// keyboard & mouse: WASD = the camera (returns -10 - direction)
int mapKey(SDL_Keycode k, bool controller) {
    if (!controller) switch (k) {
            case SDLK_w: return -10;
            case SDLK_s: return -11;
            case SDLK_a: return -12;
            case SDLK_d: return -13;
            default: break;
        }
    switch (k) {
        case SDLK_w: case SDLK_UP: return int(Btn::Up);
        case SDLK_s: case SDLK_DOWN: return int(Btn::Down);
        case SDLK_a: case SDLK_LEFT: return int(Btn::Left);
        case SDLK_d: case SDLK_RIGHT: return int(Btn::Right);
        case SDLK_SPACE: return int(Btn::A);
        case SDLK_BACKSPACE: return int(Btn::B);
        case SDLK_x: return int(Btn::X);
        case SDLK_v: return int(Btn::Y);
        case SDLK_q: return int(Btn::L1);
        case SDLK_e: return int(Btn::R1);
        case SDLK_z: return int(Btn::L2);
        case SDLK_c: case SDLK_TAB: return int(Btn::R2);  // Tab: view the map
        case SDLK_RETURN: case SDLK_KP_ENTER: return int(Btn::Start);
        case SDLK_r: return int(Btn::Select);
        case SDLK_ESCAPE: return int(Btn::Menu);  // Esc: the game menu (owner, 0.27.0)
        default: return int(Btn::None);
    }
}

Btn mapPad(int b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_A: return Btn::A;
        case SDL_CONTROLLER_BUTTON_B: return Btn::B;
        case SDL_CONTROLLER_BUTTON_X: return Btn::X;
        case SDL_CONTROLLER_BUTTON_Y: return Btn::Y;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Btn::L1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Btn::R1;
        case SDL_CONTROLLER_BUTTON_BACK: return Btn::Start;    // Deck "View"
        case SDL_CONTROLLER_BUTTON_START: return Btn::Menu;    // Deck "Menu"
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Btn::Select;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return Btn::Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Btn::Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Btn::Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Btn::Right;
        default: return Btn::None;
    }
}

class Sdl2Platform : public Platform {
public:
    const char* name() const override { return "sdl2"; }

    bool init() override {
#ifdef __EMSCRIPTEN__
        // the web build links SDL2 in (Emscripten's port), so no dlopen
        api_.Init = &SDL_Init;
        api_.Quit = &SDL_Quit;
        api_.GetError = &SDL_GetError;
        api_.SetHint = &SDL_SetHint;
        api_.GetDesktopDisplayMode = &SDL_GetDesktopDisplayMode;
        api_.CreateWindow = &SDL_CreateWindow;
        api_.DestroyWindow = &SDL_DestroyWindow;
        api_.SetWindowFullscreen = &SDL_SetWindowFullscreen;
        api_.GetWindowFlags = &SDL_GetWindowFlags;
        api_.CreateRenderer = &SDL_CreateRenderer;
        api_.DestroyRenderer = &SDL_DestroyRenderer;
        api_.RenderSetLogicalSize = &SDL_RenderSetLogicalSize;
        api_.SetRenderDrawColor = &SDL_SetRenderDrawColor;
        api_.RenderClear = &SDL_RenderClear;
        api_.RenderCopy = &SDL_RenderCopy;
        api_.RenderPresent = &SDL_RenderPresent;
        api_.CreateTexture = &SDL_CreateTexture;
        api_.DestroyTexture = &SDL_DestroyTexture;
        api_.UpdateTexture = &SDL_UpdateTexture;
        api_.PollEvent = &SDL_PollEvent;
        api_.NumJoysticks = &SDL_NumJoysticks;
        api_.IsGameController = &SDL_IsGameController;
        api_.GameControllerOpen = &SDL_GameControllerOpen;
        api_.GameControllerName = &SDL_GameControllerName;
        api_.GetKeyName = &SDL_GetKeyName;
        api_.ShowCursor = &SDL_ShowCursor;
        api_.GetRendererOutputSize = &SDL_GetRendererOutputSize;
        api_.GetWindowSize = &SDL_GetWindowSize;
        api_.GetVersion = &SDL_GetVersion;
        api_.SetWindowSize = &SDL_SetWindowSize;
        api_.SetWindowPosition = &SDL_SetWindowPosition;
#else
        static const char* libs[] = {"libSDL2-2.0.so.0", "./lib/libSDL2-2.0.so.0", "libSDL2.so"};
        for (const char* l : libs)
            if ((lib_ = dlopen(l, RTLD_NOW | RTLD_LOCAL)) != nullptr) {
                LOG("sdl2: loaded %s", l);
                break;
            }
        if (!lib_) {
            LOG("sdl2: can't load SDL2 (%s)", dlerror());
            return false;
        }
        bool ok = true;
        auto sym = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(lib_, name));
            if (!fn) {
                LOG("sdl2: missing %s", name);
                ok = false;
            }
        };
        sym(api_.Init, "SDL_Init");
        sym(api_.Quit, "SDL_Quit");
        sym(api_.GetError, "SDL_GetError");
        sym(api_.SetHint, "SDL_SetHint");
        sym(api_.GetDesktopDisplayMode, "SDL_GetDesktopDisplayMode");
        sym(api_.CreateWindow, "SDL_CreateWindow");
        sym(api_.DestroyWindow, "SDL_DestroyWindow");
        sym(api_.SetWindowFullscreen, "SDL_SetWindowFullscreen");
        sym(api_.GetWindowFlags, "SDL_GetWindowFlags");
        sym(api_.CreateRenderer, "SDL_CreateRenderer");
        sym(api_.DestroyRenderer, "SDL_DestroyRenderer");
        sym(api_.RenderSetLogicalSize, "SDL_RenderSetLogicalSize");
        sym(api_.SetRenderDrawColor, "SDL_SetRenderDrawColor");
        sym(api_.RenderClear, "SDL_RenderClear");
        sym(api_.RenderCopy, "SDL_RenderCopy");
        sym(api_.RenderPresent, "SDL_RenderPresent");
        sym(api_.CreateTexture, "SDL_CreateTexture");
        sym(api_.DestroyTexture, "SDL_DestroyTexture");
        sym(api_.UpdateTexture, "SDL_UpdateTexture");
        sym(api_.PollEvent, "SDL_PollEvent");
        sym(api_.NumJoysticks, "SDL_NumJoysticks");
        sym(api_.IsGameController, "SDL_IsGameController");
        sym(api_.GameControllerOpen, "SDL_GameControllerOpen");
        sym(api_.GameControllerName, "SDL_GameControllerName");
        sym(api_.GetKeyName, "SDL_GetKeyName");
        sym(api_.ShowCursor, "SDL_ShowCursor");
        sym(api_.GetRendererOutputSize, "SDL_GetRendererOutputSize");
        sym(api_.GetWindowSize, "SDL_GetWindowSize");
        sym(api_.GetVersion, "SDL_GetVersion");
        sym(api_.SetWindowSize, "SDL_SetWindowSize");
        sym(api_.SetWindowPosition, "SDL_SetWindowPosition");
        if (!ok) return false;
#endif

        if (api_.Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
            LOG("sdl2: SDL_Init failed: %s", api_.GetError());
            return false;
        }
        inited_ = true;
        api_.SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // smooth scaling (not blocky)
        // Full screen on small displays (the Steam Deck is 1280x800); otherwise the
        // biggest whole-number size that fits.
        SDL_DisplayMode mode{};
        int dw = 1280, dh = 800;
        if (api_.GetDesktopDisplayMode(0, &mode) == 0) {
            dw = mode.w;
            dh = mode.h;
        }
        int scale = 1;
        for (int s = 4; s >= 1; --s)
            if (kW * s <= dw - 40 && kH * s <= dh - 80) {
                scale = s;
                break;
            }
        // (0.27.0: full screen to start with - the Options screen / F11 make it a window)
#ifdef __EMSCRIPTEN__
        const bool full = false;  // web: the canvas fills the browser tab (the page's CSS), and follows it
        dw = std::max(kW, dw);
        dh = std::max(kH, dh);
#else
        const bool full = !std::getenv("TI4_WINDOWED");
#endif
        dispW_ = dw;
        dispH_ = dh;
        winW_ = kW * scale;
        winH_ = kH * scale;
        // widescreen (0.46.0): a window the display's shape (the picture follows the window)
        if (dh > 0 && dw * 3 > dh * 4) winW_ = std::min(dw - 40, winH_ * dw / dh);
        std::string title = std::string("Fallen Imperium ") + TI4_VERSION;
        // (full screen: the window is made screen-sized too, for desktops without a window manager)
        win_ = api_.CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, full ? dw : winW_,
                                 full ? dh : winH_, SDL_WINDOW_RESIZABLE | kHighDpi | (full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
        if (!win_) {
            LOG("sdl2: SDL_CreateWindow failed: %s", api_.GetError());
            return false;
        }
        // (0.32.1, owner: tearing) present in step with the screen's refresh
        ren_ = api_.CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        vsync_ = ren_ != nullptr;
        if (!ren_) ren_ = api_.CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED);
        if (!ren_) ren_ = api_.CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
        LOG("sdl2: renderer%s", vsync_ ? " with vsync" : " without vsync");
        if (!ren_) {
            LOG("sdl2: SDL_CreateRenderer failed: %s", api_.GetError());
            return false;
        }
        // (0.27.0: no logical size - present() works out where the picture goes, so a
        // sharp picture lands pixel for pixel and the mouse can be mapped back)
        SDL_version v{};
        api_.GetVersion(&v);
        preciseWheel_ = v.major > 2 || (v.major == 2 && (v.minor > 0 || v.patch >= 18));
#ifdef __EMSCRIPTEN__
        deck_ = false;  // web: a 1280x800 browser window is not a Deck (keyboard & mouse first)
#else
        deck_ = std::getenv("SteamDeck") != nullptr || (dw == 1280 && dh == 800);
#endif
        for (int i = 0; i < api_.NumJoysticks(); ++i) openPad(i);
        LOG("sdl2: display %dx%d, window %dx%d%s", dw, dh, winW_, winH_, full ? " (full screen)" : "");
        return true;
    }

    void shutdown() override {
        if (tex_) api_.DestroyTexture(tex_);
        if (ren_) api_.DestroyRenderer(ren_);
        if (win_) api_.DestroyWindow(win_);
        tex_ = nullptr;
        ren_ = nullptr;
        win_ = nullptr;
        if (inited_) api_.Quit();
        inited_ = false;
        // the library stays loaded (unloading SDL at exit can crash some drivers)
    }

    void poll(std::vector<KeyEvent>& out, bool& quit) override {
        SDL_Event e;
        while (api_.PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: quit = true; break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    if (e.key.repeat) break;  // the game repeats by itself
                    const SDL_Keycode k = e.key.keysym.sym;
                    const bool down = e.type == SDL_KEYDOWN;
                    if (down && (k == SDLK_F11 && !kWeb || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)))) {
                        fullscreenKey();
                        break;
                    }
                    const int m = down ? mapKey(k, controller_) : (keyMapped_.count(k) ? keyMapped_[k] : mapKey(k, controller_));
                    if (down) keyMapped_[k] = m;
                    if (m <= -10) out.push_back({Btn::None, down, kRawPanUp + (-10 - m), 0, 0});  // the camera
                    else press(out, Btn(m), down, int(k));
                    break;
                }
                case SDL_MOUSEMOTION: {
                    float x, y;
                    toGame(e.motion.x, e.motion.y, x, y);
                    out.push_back({Btn::None, false, kRawMouseMove, x, y});
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    const bool down = e.type == SDL_MOUSEBUTTONDOWN;
                    float x, y;
                    toGame(e.button.x, e.button.y, x, y);
                    if (e.button.button == SDL_BUTTON_LEFT) out.push_back({Btn::None, down, kRawMouseLeft, x, y});
                    else if (e.button.button == SDL_BUTTON_RIGHT) out.push_back({Btn::None, down, kRawMouseRight, x, y});
                    break;
                }
                case SDL_MOUSEWHEEL: {
                    float y = preciseWheel_ && e.wheel.preciseY != 0.f ? e.wheel.preciseY : float(e.wheel.y);
                    if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
                    out.push_back({Btn::None, true, kRawWheel, std::max(-3.f, std::min(3.f, y)), 0});
                    break;
                }
                case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
                    press(out, mapPad(e.cbutton.button), e.type == SDL_CONTROLLERBUTTONDOWN, kRawPad + e.cbutton.button);
                    break;
                case SDL_CONTROLLERAXISMOTION: axis(out, e.caxis.axis, e.caxis.value); break;
                default: break;
            }
        }
    }

    void present(const Canvas& c, bool rotate180) override {
        if (!ren_) return;
        const int w = c.pw(), h = c.ph();
        if (!tex_ || w != tw_ || h != th_) {  // the picture's size changed (0.27.0)
            if (tex_) api_.DestroyTexture(tex_);
            tex_ = api_.CreateTexture(ren_, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, w, h);
            if (!tex_) {
                LOG("sdl2: SDL_CreateTexture failed: %s", api_.GetError());
                return;
            }
            tw_ = w;
            th_ = h;
            frame_.assign(size_t(w) * size_t(h), 0);
        }
        const uint32_t* src = c.pixels();
        const size_t n = size_t(w) * size_t(h);
        if (rotate180) {
            for (size_t i = 0; i < n; ++i) frame_[i] = src[n - 1 - i];
            api_.UpdateTexture(tex_, nullptr, frame_.data(), w * 4);
        } else {
            api_.UpdateTexture(tex_, nullptr, src, w * 4);
        }
        // where it goes: pixel for pixel when it was made for this screen, else as big as fits
        // (keeping the picture's shape: 4:3, or wider on a widescreen, 0.46.0)
        int ow = kW, oh = kH;
        api_.GetRendererOutputSize(ren_, &ow, &oh);
        unitsW_ = c.fullWidth();
        int dw, dh;
        if (w > kW && w <= ow && h <= oh) {
            dw = w;
            dh = h;
        } else {
            dw = ow;
            dh = int(int64_t(ow) * h / std::max(1, w));
            if (dh > oh) {
                dh = oh;
                dw = int(int64_t(oh) * w / std::max(1, h));
            }
        }
        dest_ = SDL_Rect{(ow - dw) / 2, (oh - dh) / 2, dw, dh};
        api_.SetRenderDrawColor(ren_, 0, 0, 0, 255);
        api_.RenderClear(ren_);
        api_.RenderCopy(ren_, tex_, nullptr, &dest_);
        api_.RenderPresent(ren_);
    }

    float desiredScale() const override {
        if (!sharp_ || !ren_) return 1.f;
        int ow = kW, oh = kH;
        api_.GetRendererOutputSize(ren_, &ow, &oh);
        if (ow < kW / 2 || oh < kH / 2) return 1.f;
        float s = std::min(float(ow) / float(kW), float(oh) / float(kH));
        return std::max(1.f, std::min(3.f, std::floor(s * 100.f) / 100.f));
    }

    // widescreen (0.46.0): the window's shape at 480 units tall (4:3 and narrower: 640)
    int desiredWidth() const override {
        if (!wide_ || !ren_) return kW;
        int ow = kW, oh = kH;
        api_.GetRendererOutputSize(ren_, &ow, &oh);
        if (ow < kW / 2 || oh < kH / 2) return kW;
        return std::max(kW, std::min(kMaxW, int(float(kH) * float(ow) / float(oh))));
    }

    void configure(Settings& st) override {
        settings_ = &st;
        if (st.get("controls").empty()) st.setInt("controls", deck_ ? 1 : 0);  // the Deck: its own buttons
#ifdef __EMSCRIPTEN__
        st.setInt("fullscreen", 0);  // web: the browser tab is the window (the browser's own F11 for full screen)
#else
        if (st.get("fullscreen").empty()) st.setInt("fullscreen", 1);
#endif  // full screen to start with (0.27.0)
        sharp_ = st.getInt("screen", 1) == 1;
        wide_ = sharp_ && st.getInt("wide", 1) == 1;  // (0.46.0; the Original picture stays 640x480)
        controller_ = st.getInt("controls", 0) == 1;
        setKeyboardKeys(!controller_);
        setDeckKeys(controller_);
        const bool full = st.getInt("fullscreen", 1) == 1;
        if (full != fullscreen()) toggleFullscreen();
        LOG("sdl2: picture %s%s, controls %s", sharp_ ? "sharp" : "original", wide_ ? " widescreen" : " 4:3",
            controller_ ? "controller" : "keyboard & mouse");
    }

    bool defaultRotate() const override { return false; }
    bool screenRescue() const override { return false; }

    std::string rawKeyName(int raw) const override {
        if (raw == kRawWheelUp) return "Mouse wheel up";
        if (raw == kRawWheelDown) return "Mouse wheel down";
        if (raw == kRawMouseLeft) return "Left mouse button / tap";
        if (raw == kRawMouseRight) return "Right mouse button";
        if (raw == kRawWheel) return "Mouse wheel";
        if (raw >= kRawPanUp && raw <= kRawPanRight) return "Camera (WASD / right stick)";
        if (raw >= kRawAxis) return raw - kRawAxis < 4 ? "Left stick" : "Trigger";
        if (raw >= kRawPad) {
            static const char* names[15] = {"Pad A", "Pad B", "Pad X", "Pad Y", "Pad View", "Pad Guide", "Pad Menu", "Left stick click",
                                            "Right stick click", "Pad L1", "Pad R1", "Pad up", "Pad down", "Pad left", "Pad right"};
            const int b = raw - kRawPad;
            return b >= 0 && b < 15 ? names[b] : "Pad button";
        }
        const char* k = api_.GetKeyName ? api_.GetKeyName(SDL_Keycode(raw)) : nullptr;
        return k && *k ? std::string("key ") + k : "key";
    }

    std::string describe() const override {
        char buf[512];
        std::snprintf(buf, sizeof buf,
                      "Backend: SDL2 (Linux / Steam Deck)%s\n"
                      "Pad: A B X Y, L1/R1, L2/R2, View START, Menu MENU, stick click SELECT\n"
                      "Keys: WASD camera, arrows, Space A, Backspace B, Esc menu, Tab map, Q/E, Z/C, Enter START, R SELECT\n"
                      "Pads found: %d",
                      fullscreen() ? " - full screen" : "", pads_);
        return buf;
    }

    uint32_t ticks() override { return monotonicMs(); }

    void waitFrame(uint32_t start, bool idle) override {
#ifdef __EMSCRIPTEN__
        // web: always hand the browser a turn (Asyncify), or the page never draws
        const uint32_t budget = idle ? 33 : 16;
        const uint32_t spent = monotonicMs() - start;
        sleepMs(spent < budget ? budget - spent : 0);
#else
        const uint32_t budget = idle ? 33 : vsync_ ? 8 : 16;  // (vsync: the present already waited)
        const uint32_t spent = monotonicMs() - start;
        if (spent < budget) sleepMs(budget - spent);
#endif
    }

private:
    static constexpr int kW = 640, kH = 480;
#ifdef __EMSCRIPTEN__
    static constexpr Uint32 kHighDpi = SDL_WINDOW_ALLOW_HIGHDPI;  // web: draw at the screen's real pixels
#else
    static constexpr Uint32 kHighDpi = 0;
#endif
    static constexpr int kMaxW = 1120;  // ~21:9; anything wider gets bars at the sides
    bool vsync_ = false;

    bool fullscreen() const { return win_ && (api_.GetWindowFlags(win_) & SDL_WINDOW_FULLSCREEN); }

    // window coordinates -> game units (through the window / pixel ratio and where the picture is)
    void toGame(int wx, int wy, float& x, float& y) const {
        int ww = 1, wh = 1, ow = 1, oh = 1;
        api_.GetWindowSize(win_, &ww, &wh);
        api_.GetRendererOutputSize(ren_, &ow, &oh);
        const float px = float(wx) * float(ow) / float(std::max(1, ww)), py = float(wy) * float(oh) / float(std::max(1, wh));
        x = dest_.w > 0 ? (px - float(dest_.x)) * float(unitsW_) / float(dest_.w) : px;
        y = dest_.h > 0 ? (py - float(dest_.y)) * float(kH) / float(dest_.h) : py;
    }

    void toggleFullscreen() {
        if (!win_) return;
        if (fullscreen()) {
            api_.SetWindowFullscreen(win_, 0);
            api_.SetWindowSize(win_, winW_, winH_);
            api_.SetWindowPosition(win_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        } else {
            // (the window is made screen-sized too, for desktops without a window manager)
            api_.SetWindowPosition(win_, 0, 0);
            api_.SetWindowSize(win_, dispW_, dispH_);
            api_.SetWindowFullscreen(win_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        LOG("sdl2: full screen %s", fullscreen() ? "on" : "off");
    }
    void fullscreenKey() {
        toggleFullscreen();
        if (settings_) {
            settings_->setInt("fullscreen", fullscreen() ? 1 : 0);
            settings_->save();
        }
    }

    void openPad(int index) {
        if (!api_.IsGameController(index)) return;
        if (SDL_GameController* p = api_.GameControllerOpen(index)) {
            ++pads_;
            LOG("sdl2: controller %s", api_.GameControllerName(p) ? api_.GameControllerName(p) : "?");
        }
    }

    // Several inputs can mean the same button (keyboard + pad + stick): it goes
    // down with the first and up with the last.
    void press(std::vector<KeyEvent>& out, Btn b, bool down, int raw) {
        if (b == Btn::None) {
            out.push_back({Btn::None, down, raw});
            return;
        }
        std::vector<int>& src = sources_[int(b)];
        auto it = std::find(src.begin(), src.end(), raw);
        if (down) {
            if (it != src.end()) return;
            src.push_back(raw);
            if (src.size() == 1) out.push_back({b, true, raw});
        } else {
            if (it == src.end()) return;
            src.erase(it);
            if (src.empty()) out.push_back({b, false, raw});
        }
    }

    // Left stick = D-pad (with a dead zone and hysteresis); triggers = L2 / R2.
    void axis(std::vector<KeyEvent>& out, int a, int v) {
        auto set = [&](Btn b, bool on, int raw) { press(out, b, on, raw); };
        const int on = 16000, off = 9000;
        if (a == SDL_CONTROLLER_AXIS_LEFTX || a == SDL_CONTROLLER_AXIS_LEFTY) {
            const bool x = a == SDL_CONTROLLER_AXIS_LEFTX;
            const Btn neg = x ? Btn::Left : Btn::Up, pos = x ? Btn::Right : Btn::Down;
            const int rn = kRawAxis + (x ? 0 : 2), rp = rn + 1;
            bool& n = stick_[x ? 0 : 2];
            bool& p = stick_[x ? 1 : 3];
            if (!n && v < -on) set(neg, n = true, rn);
            else if (n && v > -off) set(neg, n = false, rn);
            if (!p && v > on) set(pos, p = true, rp);
            else if (p && v < off) set(pos, p = false, rp);
        } else if (a == SDL_CONTROLLER_AXIS_RIGHTX || a == SDL_CONTROLLER_AXIS_RIGHTY) {  // the camera (0.27.0)
            const bool x = a == SDL_CONTROLLER_AXIS_RIGHTX;
            const int rn = x ? kRawPanLeft : kRawPanUp, rp = x ? kRawPanRight : kRawPanDown;
            bool& n = rstick_[x ? 0 : 2];
            bool& p = rstick_[x ? 1 : 3];
            if (!n && v < -on) out.push_back({Btn::None, n = true, rn, 0, 0});
            else if (n && v > -off) out.push_back({Btn::None, n = false, rn, 0, 0});
            if (!p && v > on) out.push_back({Btn::None, p = true, rp, 0, 0});
            else if (p && v < off) out.push_back({Btn::None, p = false, rp, 0, 0});
        } else if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            const bool left = a == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
            bool& t = trig_[left ? 0 : 1];
            const int raw = kRawAxis + (left ? 4 : 5);
            if (!t && v > on) set(left ? Btn::L2 : Btn::R2, t = true, raw);
            else if (t && v < off) set(left ? Btn::L2 : Btn::R2, t = false, raw);
        }
    }

    void* lib_ = nullptr;
    Api api_{};
    bool inited_ = false;
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    SDL_Texture* tex_ = nullptr;
    std::vector<uint32_t> frame_;
    std::vector<int> sources_[kBtnCount];
    bool stick_[4] = {}, trig_[2] = {}, rstick_[4] = {};
    int pads_ = 0;
    int tw_ = 0, th_ = 0;
    SDL_Rect dest_{0, 0, kW, kH};
    bool sharp_ = true, controller_ = false, preciseWheel_ = false, deck_ = false, wide_ = true;
    int unitsW_ = kW;  // the presented picture's width in game units (widescreen)
    std::map<SDL_Keycode, int> keyMapped_;
    int dispW_ = 1280, dispH_ = 800, winW_ = kW, winH_ = kH;
    Settings* settings_ = nullptr;
};

}  // namespace

std::unique_ptr<Platform> makeSdl2Platform() { return std::unique_ptr<Platform>(new Sdl2Platform()); }

#endif  // TI4_SDL2
