// Windows backend: the picture in a resizable window or full screen,
// keyboard, mouse and Xbox-style controllers in place of the Miyoo's buttons.
//
// The picture (0.27.0, owner: "crisp on computer monitors"): with Picture =
// Sharp the game draws at the window's own resolution (4:3, as close to
// 1920x1080 as fits without stretching - 1440x1080 on a 1080p monitor) and the
// window shows it pixel for pixel; Picture = Original draws 640x480 and
// enlarges it, as before.
//
// Controls (0.27.0, owner: like Hearts of Iron 4 / Total War):
//   Keyboard & mouse: WASD moves the map camera, the mouse points at tiles and
//     rows (no camera jumps), left click / Space confirm, a right-button drag
//     moves the camera, a right click / Backspace goes back, the wheel zooms
//     smoothly, Esc = the game menu, Tab = view the map, arrows = D-pad,
//     X, V = Y, Q / E = L1 / R1, Z = L2, C = R2, Enter = START, R = SELECT.
//   Controller: an Xbox-style pad (XInput): A B X Y, bumpers L1 / R1,
//     triggers L2 / R2, View = START, Menu = MENU, stick clicks = SELECT, left
//     stick / D-pad = D-pad, right stick = the camera. WASD then works as the
//     D-pad; the mouse still works.
//   F11 or Alt+Enter: full screen.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "core/settings.h"
#include "platform/platform.h"

namespace {

const wchar_t* kWindowClass = L"TI4WindowClass";
constexpr int kRawPad = 0x20000;  // controller buttons in KeyEvent::raw

class Win32Platform : public Platform {
public:
    const char* name() const override { return "win32"; }

    bool init() override {
        // Draw at the screen's real resolution instead of being blurred by
        // Windows' display scaling.
        SetProcessDPIAware();
        timeBeginPeriod(1);  // 1 ms sleeps for steady frame timing
        timerSet_ = true;

        HINSTANCE inst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &Win32Platform::wndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        wc.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            LOG("win32: RegisterClassEx failed (%lu)", (unsigned long)GetLastError());
            return false;
        }

        // A window that fills most of the work area, keeping 4:3 - or (widescreen, 0.46.0)
        // the screen's own shape; the picture follows the window either way.
        RECT work{0, 0, 1280, 960};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int workW = work.right - work.left, workH = work.bottom - work.top;
        const DWORD style = WS_OVERLAPPEDWINDOW;
        const int scrW = GetSystemMetrics(SM_CXSCREEN), scrH = GetSystemMetrics(SM_CYSCREEN);
        const float shape = scrH > 0 && scrW * 3 > scrH * 4 ? std::min(float(kMaxW) / float(kH), float(scrW) / float(scrH)) : 4.f / 3.f;
        const int baseW = int(float(kH) * shape + 0.5f);
        int cw = kW, ch = kH;
        for (float s = 4.f; s >= 1.f; s -= 0.25f) {
            RECT r{0, 0, int(baseW * s), int(kH * s)};
            AdjustWindowRect(&r, style, FALSE);
            if (r.right - r.left <= workW && r.bottom - r.top <= workH) {
                cw = int(baseW * s);
                ch = int(kH * s);
                break;
            }
        }
        RECT r{0, 0, cw, ch};
        AdjustWindowRect(&r, style, FALSE);
        const int w = r.right - r.left, h = r.bottom - r.top;
        const int x = work.left + std::max(0, (workW - w) / 2), y = work.top + std::max(0, (workH - h) / 2);

        std::wstring title = L"Fallen Imperium " + widen(TI4_VERSION);
        hwnd_ = CreateWindowExW(0, kWindowClass, title.c_str(), style, x, y, w, h, nullptr, nullptr, inst, this);
        if (!hwnd_) {
            LOG("win32: CreateWindowEx failed (%lu)", (unsigned long)GetLastError());
            return false;
        }
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
        SetForegroundWindow(hwnd_);
        LOG("win32: window %dx%d, work area %dx%d", cw, ch, workW, workH);

        // (0.32.1, owner: tearing with the Voidfall tileset) wait for the desktop compositor before each
        // picture, so it is never caught half copied; it also paces the frames to the screen
        if ((dwmLib_ = LoadLibraryW(L"dwmapi.dll")) != nullptr)
            dwmFlush_ = reinterpret_cast<HRESULT(WINAPI*)()>(reinterpret_cast<void*>(GetProcAddress(dwmLib_, "DwmFlush")));
        // Xbox-style controllers
        for (const wchar_t* dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
            if ((xinputLib_ = LoadLibraryW(dll)) != nullptr) {
                xinputGetState_ = reinterpret_cast<DWORD(WINAPI*)(DWORD, XINPUT_STATE*)>(
                    reinterpret_cast<void*>(GetProcAddress(xinputLib_, "XInputGetState")));
                if (xinputGetState_) break;
            }
        }
        return true;
    }

    void configure(Settings& s) override {
        settings_ = &s;
        sharp_ = s.getInt("screen", 1) == 1;
        wide_ = sharp_ && s.getInt("wide", 1) == 1;  // (0.46.0; the Original picture stays 640x480)
        controller_ = s.getInt("controls", 0) == 1;
        setKeyboardKeys(!controller_);
        setDeckKeys(controller_);  // pad names: View = START, stick click = SELECT
        const bool full = s.getInt("fullscreen", 1) == 1;
        if (full != fullscreen_) toggleFullscreen();
        releaseAll();
        LOG("win32: picture %s%s, controls %s, full screen %d", sharp_ ? "sharp" : "original", wide_ ? " widescreen" : " 4:3",
            controller_ ? "controller" : "keyboard & mouse", int(fullscreen_));
    }

    void shutdown() override {
        if (hwnd_) {
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (timerSet_) timeEndPeriod(1);
        timerSet_ = false;
    }

    void poll(std::vector<KeyEvent>& out, bool& quit) override {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) quitRequested_ = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        pollPad();
        out.insert(out.end(), pending_.begin(), pending_.end());
        pending_.clear();
        if (quitRequested_) quit = true;
    }

    // Sharp: as many pixels per game unit as the window has (4:3 inside it), at most 3.
    float desiredScale() const override {
        if (!sharp_ || !hwnd_) return 1.f;
        RECT cr{0, 0, 0, 0};
        GetClientRect(hwnd_, &cr);
        if (cr.right < kW / 2 || cr.bottom < kH / 2) return lastScale_;
        float s = std::min(float(cr.right) / float(kW), float(cr.bottom) / float(kH));
        s = std::max(1.f, std::min(3.f, std::floor(s * 100.f) / 100.f));
        lastScale_ = s;
        return s;
    }

    // widescreen (0.46.0): the window's shape at 480 units tall (4:3 and narrower: 640)
    int desiredWidth() const override {
        if (!wide_ || !hwnd_) return kW;
        RECT cr{0, 0, 0, 0};
        GetClientRect(hwnd_, &cr);
        if (cr.right < kW / 2 || cr.bottom < kH / 2) return lastWidth_;
        lastWidth_ = std::max(kW, std::min(kMaxW, int(float(kH) * float(cr.right) / float(cr.bottom))));
        return lastWidth_;
    }

    void present(const Canvas& c, bool rotate180) override {
        const int w = c.pw(), h = c.ph();
        unitsW_ = c.fullWidth();
        if (w != fw_ || h != fh_) {
            fw_ = w;
            fh_ = h;
            std::memset(&bmi_, 0, sizeof bmi_);
            bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi_.bmiHeader.biWidth = w;
            bmi_.bmiHeader.biHeight = -h;  // top-down rows, like the canvas
            bmi_.bmiHeader.biPlanes = 1;
            bmi_.bmiHeader.biBitCount = 32;  // the canvas is 0x00RRGGBB, exactly a 32-bit DIB
            bmi_.bmiHeader.biCompression = BI_RGB;
            frame_.assign(size_t(w) * size_t(h), 0);
        }
        const uint32_t* src = c.pixels();
        const size_t n = size_t(w) * size_t(h);
        if (rotate180) {
            for (size_t i = 0; i < n; ++i) frame_[i] = src[n - 1 - i];
        } else {
            std::memcpy(frame_.data(), src, n * 4);
        }
        synced_ = dwmFlush_ && dwmFlush_() >= 0;  // (fails when the compositor is off - then no wait)
        HDC dc = GetDC(hwnd_);
        paint(dc);
        ReleaseDC(hwnd_, dc);
    }

    bool defaultRotate() const override { return false; }
    bool screenRescue() const override { return false; }

    std::string rawKeyName(int raw) const override {
        if ((raw >= 'A' && raw <= 'Z') || (raw >= '0' && raw <= '9')) return std::string("key ") + char(raw);
        if (raw == kRawMouseLeft) return "Left mouse button";
        if (raw == kRawMouseRight) return "Right mouse button";
        if (raw == kRawWheel) return "Mouse wheel";
        if (raw >= kRawPanUp && raw <= kRawPanRight) {
            static const char* n[4] = {"W (camera up)", "S (camera down)", "A (camera left)", "D (camera right)"};
            return n[raw - kRawPanUp];
        }
        if (raw >= kRawPad) return "Controller button " + std::to_string(raw - kRawPad);
        switch (raw) {
            case VK_SPACE: return "Space";
            case VK_BACK: return "Backspace";
            case VK_RETURN: return "Enter";
            case VK_TAB: return "Tab";
            case VK_ESCAPE: return "Esc";
            case VK_UP: return "Up arrow";
            case VK_DOWN: return "Down arrow";
            case VK_LEFT: return "Left arrow";
            case VK_RIGHT: return "Right arrow";
            case VK_SHIFT: return "Shift";
            case VK_CONTROL: return "Ctrl";
            case VK_MENU: return "Alt";
            default: break;
        }
        char buf[32];
        std::snprintf(buf, sizeof buf, "key 0x%02X", raw);
        return buf;
    }

    std::string describe() const override {
        RECT cr{0, 0, 0, 0};
        if (hwnd_) GetClientRect(hwnd_, &cr);
        char buf[640];
        std::snprintf(buf, sizeof buf,
                      "Backend: Windows window (GDI)\n"
                      "Window: %ldx%ld%s, picture %dx%d (%s)\n"
                      "Controls: %s; controller: %s\n"
                      "Keys: WASD camera, arrows move, Space A, Backspace B, Esc menu, Tab map\n"
                      "Mouse: point, left click A, right drag camera, right click B, wheel zoom",
                      long(cr.right), long(cr.bottom), fullscreen_ ? " (full screen)" : "", fw_, fh_, sharp_ ? "sharp" : "original",
                      controller_ ? "controller" : "keyboard & mouse", padConnected_ ? "connected" : "none");
        return buf;
    }

    uint32_t ticks() override { return monotonicMs(); }

    void waitFrame(uint32_t start, bool idle) override {
        const uint32_t budget = idle ? 33 : synced_ ? 8 : 16;  // (synced: the compositor already waited)
        uint32_t spent = monotonicMs() - start;
        if (spent < budget) sleepMs(budget - spent);
    }

private:
    static constexpr int kW = 640, kH = 480;
    static constexpr int kMaxW = 1120;  // ~21:9; anything wider gets bars at the sides

    static std::wstring widen(const char* s) {
        std::wstring w;
        for (; *s; ++s) w += wchar_t(static_cast<unsigned char>(*s));
        return w;
    }

    // The picture, as large as the window allows while keeping its shape (4:3 or wider), centred.
    // A sharp picture made for this window is shown pixel for pixel.
    void paint(HDC dc) {
        RECT cr;
        GetClientRect(hwnd_, &cr);
        const int cw = cr.right, ch = cr.bottom;
        if (cw <= 0 || ch <= 0 || fw_ <= 0) return;
        int w, h;
        if (fw_ <= cw && fh_ <= ch && fw_ > kW) {  // made for this window: 1:1
            w = fw_;
            h = fh_;
        } else {  // as big as fits, keeping the picture's shape
            w = cw;
            h = int(int64_t(cw) * fh_ / fw_);
            if (h > ch) {
                h = ch;
                w = int(int64_t(ch) * fw_ / fh_);
            }
        }
        const int x = (cw - w) / 2, y = (ch - h) / 2;
        destX_ = x;
        destY_ = y;
        destW_ = w;
        destH_ = h;
        HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        if (x > 0) {
            RECT a{0, 0, x, ch}, b{x + w, 0, cw, ch};
            FillRect(dc, &a, black);
            FillRect(dc, &b, black);
        }
        if (y > 0) {
            RECT a{0, 0, cw, y}, b{0, y + h, cw, ch};
            FillRect(dc, &a, black);
            FillRect(dc, &b, black);
        }
        // Whole-number scales stay pixel-sharp; anything else is smoothed.
        const bool exact = w % fw_ == 0 && h % fh_ == 0;
        SetStretchBltMode(dc, exact ? COLORONCOLOR : HALFTONE);
        if (!exact) SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, x, y, w, h, 0, 0, fw_, fh_, frame_.data(), &bmi_, DIB_RGB_COLORS, SRCCOPY);
    }

    void toggleFullscreen() {
        DWORD style = DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        if (!fullscreen_) {
            MONITORINFO mi{};
            mi.cbSize = sizeof mi;
            placement_.length = sizeof placement_;
            if (!GetWindowPlacement(hwnd_, &placement_) ||
                !GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi))
                return;
            SetWindowLongPtrW(hwnd_, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
            SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen_ = true;
        } else {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(hwnd_, &placement_);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen_ = false;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        LOG("win32: full screen %s", fullscreen_ ? "on" : "off");
    }

    // keys -> buttons (or the camera)
    int mapKey(WPARAM vk) const {
        if (!controller_) {
            switch (vk) {  // keyboard & mouse: WASD is the camera
                case 'W': return -10 - 0;
                case 'S': return -10 - 1;
                case 'A': return -10 - 2;
                case 'D': return -10 - 3;
                default: break;
            }
        }
        switch (vk) {
            case 'W': case VK_UP: return int(Btn::Up);
            case 'S': case VK_DOWN: return int(Btn::Down);
            case 'A': case VK_LEFT: return int(Btn::Left);
            case 'D': case VK_RIGHT: return int(Btn::Right);
            case VK_SPACE: return int(Btn::A);
            case VK_BACK: return int(Btn::B);
            case 'X': return int(Btn::X);
            case 'V': return int(Btn::Y);
            case 'Q': return int(Btn::L1);
            case 'E': return int(Btn::R1);
            case 'Z': return int(Btn::L2);
            case 'C': case VK_TAB: return int(Btn::R2);  // Tab: view the map
            case VK_RETURN: return int(Btn::Start);
            case 'R': return int(Btn::Select);
            case VK_ESCAPE: return int(Btn::Menu);       // Esc: the game menu (owner, 0.27.0)
            default: return int(Btn::None);
        }
    }

    // Several keys can mean the same button (W and the up arrow): the button
    // goes down with the first and up with the last.
    void key(WPARAM vk, bool down) {
        if (vk >= 256) return;
        if (keyDown_[vk] == down) return;  // Windows' own key repeat: the game repeats by itself
        keyDown_[vk] = down;
        const int m = down ? mapKey(vk) : keyMapped_[vk];
        if (down) keyMapped_[vk] = m;
        if (m <= -10) {  // the camera
            pending_.push_back({Btn::None, down, kRawPanUp + (-10 - m), 0, 0});
            return;
        }
        Btn b = Btn(m);
        if (b == Btn::None) {
            pending_.push_back({Btn::None, down, int(vk)});
            return;
        }
        button(b, down, int(vk));
    }

    void button(Btn b, bool down, int raw) {
        int& n = btnKeys_[int(b)];
        if (down) {
            if (n++ == 0) pending_.push_back({b, true, raw});
        } else if (n > 0 && --n == 0) {
            pending_.push_back({b, false, raw});
        }
    }

    void releaseAll() {
        for (int vk = 0; vk < 256; ++vk)
            if (keyDown_[vk]) key(WPARAM(vk), false);
    }

    // window pixels -> game units
    void mouseAt(LPARAM lp, float& x, float& y) const {
        const int mx = short(LOWORD(lp)), my = short(HIWORD(lp));
        x = destW_ > 0 ? float(mx - destX_) * float(unitsW_) / float(destW_) : float(mx);
        y = destH_ > 0 ? float(my - destY_) * float(kH) / float(destH_) : float(my);
    }
    void mouseEvent(int raw, bool down, LPARAM lp) {
        float x, y;
        mouseAt(lp, x, y);
        pending_.push_back({Btn::None, down, raw, x, y});
    }

    // An Xbox-style controller: buttons, the D-pad and left stick as the D-pad,
    // the right stick as the camera.
    void pollPad() {
        if (!xinputGetState_) return;
        const uint32_t now = monotonicMs();
        if (!padConnected_ && now - lastPadTry_ < 2000) return;  // look for one every 2 s
        lastPadTry_ = now;
        XINPUT_STATE st{};
        if (xinputGetState_(0, &st) != ERROR_SUCCESS) {
            if (padConnected_) LOG("win32: controller disconnected");
            padConnected_ = false;
            for (int i = 0; i < 20; ++i) setPad(i, false);
            return;
        }
        if (!padConnected_) LOG("win32: controller connected");
        padConnected_ = true;
        const WORD b = st.Gamepad.wButtons;
        const int lx = st.Gamepad.sThumbLX, ly = st.Gamepad.sThumbLY, rx = st.Gamepad.sThumbRX, ry = st.Gamepad.sThumbRY;
        const int dz = 16000;
        setPad(0, (b & XINPUT_GAMEPAD_DPAD_UP) || ly > dz);
        setPad(1, (b & XINPUT_GAMEPAD_DPAD_DOWN) || ly < -dz);
        setPad(2, (b & XINPUT_GAMEPAD_DPAD_LEFT) || lx < -dz);
        setPad(3, (b & XINPUT_GAMEPAD_DPAD_RIGHT) || lx > dz);
        setPad(4, b & XINPUT_GAMEPAD_A);
        setPad(5, b & XINPUT_GAMEPAD_B);
        setPad(6, b & XINPUT_GAMEPAD_X);
        setPad(7, b & XINPUT_GAMEPAD_Y);
        setPad(8, b & XINPUT_GAMEPAD_LEFT_SHOULDER);
        setPad(9, b & XINPUT_GAMEPAD_RIGHT_SHOULDER);
        setPad(10, st.Gamepad.bLeftTrigger > 100);
        setPad(11, st.Gamepad.bRightTrigger > 100);
        setPad(12, b & XINPUT_GAMEPAD_BACK);
        setPad(13, b & XINPUT_GAMEPAD_START);
        setPad(14, b & (XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB));
        setPad(15, ry > dz);
        setPad(16, ry < -dz);
        setPad(17, rx < -dz);
        setPad(18, rx > dz);
    }
    void setPad(int i, bool down) {
        if (padDown_[i] == down) return;
        padDown_[i] = down;
        static const Btn kPadBtn[15] = {Btn::Up, Btn::Down, Btn::Left,  Btn::Right, Btn::A,     Btn::B,     Btn::X,     Btn::Y,
                                        Btn::L1, Btn::R1,   Btn::L2,    Btn::R2,    Btn::Start, Btn::Menu,  Btn::Select};
        if (i < 15) button(kPadBtn[i], down, kRawPad + i);
        else pending_.push_back({Btn::None, down, kRawPanUp + (i - 15), 0, 0});  // the right stick: the camera
    }

    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_CLOSE:
                quitRequested_ = true;
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(hwnd_, &ps);
                paint(dc);
                EndPaint(hwnd_, &ps);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                MINMAXINFO* mm = reinterpret_cast<MINMAXINFO*>(lp);
                RECT r{0, 0, kW / 2, kH / 2};
                AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
                mm->ptMinTrackSize.x = r.right - r.left;
                mm->ptMinTrackSize.y = r.bottom - r.top;
                return 0;
            }
            case WM_KILLFOCUS:
                releaseAll();  // keys let go while another window had focus
                if (mouseL_) mouseButton(kRawMouseLeft, false, lp);
                if (mouseR_) mouseButton(kRawMouseRight, false, lp);
                return 0;
            case WM_MOUSEACTIVATE:
                // the click that brings the window to the front doesn't also confirm
                return GetForegroundWindow() == hwnd_ ? MA_ACTIVATE : MA_ACTIVATEANDEAT;
            case WM_MOUSEMOVE:
                mouseEvent(kRawMouseMove, false, lp);
                return 0;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
                mouseButton(kRawMouseLeft, true, lp);
                return 0;
            case WM_LBUTTONUP:
                mouseButton(kRawMouseLeft, false, lp);
                return 0;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
                mouseButton(kRawMouseRight, true, lp);
                return 0;
            case WM_RBUTTONUP:
                mouseButton(kRawMouseRight, false, lp);
                return 0;
            case WM_CAPTURECHANGED:
                if (reinterpret_cast<HWND>(lp) != hwnd_) {
                    if (mouseL_) mouseButton(kRawMouseLeft, false, 0);
                    if (mouseR_) mouseButton(kRawMouseRight, false, 0);
                }
                return 0;
            case WM_MOUSEWHEEL: {
                // whole notches are 120; touchpads send smaller steps: passed on as fractions (a smooth zoom)
                KeyEvent e{Btn::None, true, kRawWheel, float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA), 0};
                pending_.push_back(e);
                return 0;
            }
            case WM_SYSKEYDOWN:
                if (wp == VK_RETURN) {  // Alt+Enter
                    if (!(lp & (1 << 30))) fullscreenKey();
                    return 0;
                }
                if (wp == VK_F4) break;  // Alt+F4 closes the window as usual
                key(wp, true);
                return 0;
            case WM_KEYDOWN:
                if (wp == VK_F11) {
                    if (!(lp & (1 << 30))) fullscreenKey();
                    return 0;
                }
                key(wp, true);
                return 0;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                key(wp, false);
                return 0;
            case WM_SYSCHAR:
                return 0;  // no beep for Alt+Enter
            default:
                break;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }

    void fullscreenKey() {
        toggleFullscreen();
        if (settings_) {
            settings_->setInt("fullscreen", fullscreen_ ? 1 : 0);
            settings_->save();
        }
    }

    // Mouse buttons: held while down, even when let go outside the window.
    void mouseButton(int raw, bool down, LPARAM lp) {
        bool& held = raw == kRawMouseLeft ? mouseL_ : mouseR_;
        if (held == down) return;
        held = down;
        if (down) SetCapture(hwnd_);
        mouseEvent(raw, down, lp);
        if (!mouseL_ && !mouseR_ && GetCapture() == hwnd_) ReleaseCapture();
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            auto* self = static_cast<Win32Platform*>(cs->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        auto* self = reinterpret_cast<Win32Platform*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        return self->handle(msg, wp, lp);
    }

    HWND hwnd_ = nullptr;
    BITMAPINFO bmi_{};
    std::vector<uint32_t> frame_;
    int fw_ = 0, fh_ = 0;
    int destX_ = 0, destY_ = 0, destW_ = kW, destH_ = kH;
    std::vector<KeyEvent> pending_;
    bool keyDown_[256] = {};
    int keyMapped_[256] = {};
    int btnKeys_[kBtnCount] = {};
    bool mouseL_ = false, mouseR_ = false;
    bool fullscreen_ = false;
    bool quitRequested_ = false;
    bool timerSet_ = false;
    bool sharp_ = true, controller_ = false, wide_ = true;
    mutable float lastScale_ = 1.f;
    mutable int lastWidth_ = kW;
    int unitsW_ = kW;  // the presented picture's width in game units (widescreen)
    Settings* settings_ = nullptr;
    WINDOWPLACEMENT placement_{};
    HMODULE xinputLib_ = nullptr;
    HMODULE dwmLib_ = nullptr;
    HRESULT(WINAPI* dwmFlush_)() = nullptr;
    bool synced_ = false;
    DWORD(WINAPI* xinputGetState_)(DWORD, XINPUT_STATE*) = nullptr;
    bool padConnected_ = false, padDown_[20] = {};
    uint32_t lastPadTry_ = 0;
};

}  // namespace

std::unique_ptr<Platform> makeWin32Platform() { return std::unique_ptr<Platform>(new Win32Platform()); }

#endif  // _WIN32
