// Button test: lights up each button on a drawing of the device and lists the
// raw key codes, so we can confirm the button mapping on real hardware.
#include <algorithm>
#include <cstdio>

#include "app/app.h"
#include "gfx/font.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr uint32_t kHoldToExitMs = 1500;

class ButtonTestScene : public Scene {
public:
    bool wantsAllButtons() const override { return true; }
    bool animating() const override { return exiting_; }

    void update(App& app, const Input& in, uint32_t now) override {
        for (int i = 0; i < kBtnCount; ++i) {
            if (in.pressed(Btn(i))) seen_[i] = true;
            held_[i] = in.held(Btn(i));
        }
        uint32_t menu = in.heldFor(Btn::Menu, now);
        uint32_t combo = std::min(in.heldFor(Btn::Start, now), in.heldFor(Btn::Select, now));
        uint32_t h = std::max(menu, combo);
        exiting_ = h > 0;
        progress_ = float(h) / float(kHoldToExitMs);
        if (h >= kHoldToExitMs) app.pop();
    }

    void draw(App& app, Canvas& c, uint32_t) override {
        drawSpaceBackground(c, false);
        drawDevice(c);
        drawSidePanel(app, c);
        // exit hint + progress
        const int y = 452;
        c.fillRect(0, y - 4, 640, 32, rgb(5, 8, 16), 230);
        c.hline(0, 639, y - 4, theme::border);
        drawText(c, FONT_SM, 12, y + 2, keyText("Hold MENU (or START + SELECT) to go back"), theme::dim);
        if (exiting_) {
            c.fillRoundRect(440, y + 4, 180, 12, 6, rgb(40, 48, 70));
            c.fillRoundRect(440, y + 4, int(180 * std::min(1.f, progress_)), 12, 6, theme::gold);
        }
    }

private:
    uint32_t col(Btn b) const {
        int i = int(b);
        if (held_[i]) return theme::gold;
        return seen_[i] ? rgb(70, 110, 90) : rgb(58, 66, 92);
    }
    uint32_t txt(Btn b) const { return held_[int(b)] ? theme::black : theme::text; }

    void pill(Canvas& c, int x, int y, int w, int h, Btn b, const char* label) {
        c.fillRoundRect(x, y, w, h, h / 2, col(b));
        drawTextAligned(c, FONT_SMB, x, y + (h - FONT_SMB.lineHeight) / 2, w, Align::Center, keyLabel(label), txt(b));
    }
    void round(Canvas& c, int cx, int cy, int r, Btn b, const char* label) {
        c.fillCircle(cx + 0.5f, cy + 0.5f, float(r), col(b));
        const std::string key = keyLabel(label);
        drawTextAligned(c, key.size() > 2 ? FONT_XS : FONT_MDB, cx - r, cy - (key.size() > 2 ? FONT_XS : FONT_MDB).lineHeight / 2,
                        2 * r + 1, Align::Center, key, txt(b));
    }

    void drawDevice(Canvas& c) {
        // shoulders
        pill(c, 46, 30, 64, 22, Btn::L2, "L2");
        pill(c, 116, 38, 64, 22, Btn::L1, "L1");
        pill(c, 240, 38, 64, 22, Btn::R1, "R1");
        pill(c, 310, 30, 64, 22, Btn::R2, "R2");
        // body
        c.fillRoundRect(36, 56, 348, 380, 30, rgb(30, 36, 54));
        c.frameRoundRect(36, 56, 348, 380, 30, theme::border, 2);
        // screen
        c.fillRoundRect(76, 80, 268, 160, 8, rgb(10, 12, 20));
        c.frameRoundRect(76, 80, 268, 160, 8, rgb(60, 70, 96), 1);
        drawTextAligned(c, FONT_SM, 76, 150, 268, Align::Center,
                        keyboardKeys() ? "Keys on the game's buttons" : "Miyoo Mini Plus", theme::faint);
        // side buttons
        pill(c, 384, 92, 44, 20, Btn::Power, "PWR");
        pill(c, 384, 150, 44, 20, Btn::VolUp, "V+");
        pill(c, 384, 176, 44, 20, Btn::VolDown, "V-");
        // d-pad
        const int dx = 122, dy = 318, arm = 24, len = 34;
        c.fillRoundRect(dx - arm / 2, dy - arm / 2, arm, arm, 3, rgb(44, 50, 70));
        pill(c, dx - arm / 2, dy - arm / 2 - len, arm, len, Btn::Up, "");
        pill(c, dx - arm / 2, dy + arm / 2, arm, len, Btn::Down, "");
        pill(c, dx - arm / 2 - len, dy - arm / 2, len, arm, Btn::Left, "");
        pill(c, dx + arm / 2, dy - arm / 2, len, arm, Btn::Right, "");
        drawTextAligned(c, FONT_XS, dx - 40, dy + 56, 80, Align::Center, keyLabel("D-PAD"), theme::faint);
        // face buttons (X top, A right, B bottom, Y left)
        const int fx = 298, fy = 318, sp = 34, r = 16;
        round(c, fx, fy - sp, r, Btn::X, "X");
        round(c, fx + sp, fy, r, Btn::A, "A");
        round(c, fx, fy + sp, r, Btn::B, "B");
        round(c, fx - sp, fy, r, Btn::Y, "Y");
        // start / select / menu
        pill(c, 150, 400, 60, 18, Btn::Select, "SELECT");
        pill(c, 216, 400, 60, 18, Btn::Start, "START");
        pill(c, 178, 266, 64, 20, Btn::Menu, "MENU");
    }

    void drawSidePanel(App& app, Canvas& c) {
        const Rect p{440, 10, 192, 430};
        drawPanel(c, p, theme::panel, theme::border, 10);
        int x = p.x + 10, y = p.y + 8;
        int seen = 0;
        for (int i = 0; i < kBtnCount; ++i) seen += seen_[i] ? 1 : 0;
        drawText(c, FONT_LG, x, y, "Button test", theme::gold);
        y += 26;
        char buf[64];
        std::snprintf(buf, sizeof buf, "Detected %d of %d", seen, kBtnCount);
        drawText(c, FONT_SM, x, y, buf, seen == kBtnCount ? theme::green : theme::dim);
        y += 20;
        // checklist, 3 columns
        for (int i = 0; i < kBtnCount; ++i) {
            int cx = x + (i % 3) * 58, cy = y + (i / 3) * 18;
            uint32_t cc = seen_[i] ? theme::green : theme::faint;
            drawText(c, FONT_XS, cx, cy, std::string(seen_[i] ? "\xE2\x9C\x93 " : "\xC2\xB7 ") + btnName(Btn(i)), cc);
        }
        y += 6 * 18 + 8;
        c.hline(x, p.x + p.w - 10, y, theme::border);
        y += 6;
        drawText(c, FONT_SMB, x, y, "Last key events", theme::text);
        y += 18;
        const auto& log = app.keyLog();
        int shown = 0;
        for (auto it = log.rbegin(); it != log.rend() && shown < 9; ++it, ++shown) {
            std::string name = it->btn == Btn::None ? "unknown" : btnName(it->btn);
            std::string line = name + (it->down ? " down" : " up");
            drawText(c, FONT_XS, x, y, line, it->btn == Btn::None ? theme::red : theme::text);
            drawText(c, FONT_XS, x + 78, y, fitText(FONT_XS, app.platform().rawKeyName(it->raw), 100), theme::dim);
            y += 15;
        }
        if (log.empty()) drawText(c, FONT_XS, x, y, "Press any button\xE2\x80\xA6", theme::dim);
    }

    bool seen_[kBtnCount] = {};
    bool held_[kBtnCount] = {};
    bool exiting_ = false;
    float progress_ = 0.f;
};

}  // namespace

std::unique_ptr<Scene> makeButtonTestScene() { return std::unique_ptr<Scene>(new ButtonTestScene()); }
