#include "ui/backdrops.h"

#include <algorithm>
#include <cmath>

#include "gfx/canvas.h"
#include "gfx/image.h"
#include "ui/icons.h"
#include "ui/voidfall.h"

namespace backdrops {
namespace {

constexpr float kPi = 3.14159265f;

uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
float frand(uint32_t seed) { return float(hash32(seed) & 0xffffu) / 65535.f; }

// Widescreen (0.46.0): the grid also runs on to the sides of a wider screen (a screen laid out
// for 640 and drawn centred): its first and last column, in the view's grid
int gridL(const Canvas& c) { return -(c.viewX() + 1) / 2; }
int gridR(const Canvas& c) { return (c.fullWidth() - c.viewX()) / 2; }
// While it lives, everything may draw across the whole canvas (not just the centred view).
struct WholeClip {
    Canvas& c;
    Rect old;
    explicit WholeClip(Canvas& cc) : c(cc), old(cc.clip()) { c.setClip(Rect{-c.viewX(), 0, c.fullWidth(), c.height()}); }
    ~WholeClip() { c.setClip(old); }
};

// one chunky pixel of the 320x240 grid (2x2 on the 640x480 screen)
inline void P(Canvas& c, int gx, int gy, uint32_t col, int a = 255) {
    if (gx < gridL(c) || gy < 0 || gx >= gridR(c) || gy >= 240 || a <= 0) return;
    c.fillRect(gx * 2, gy * 2, 2, 2, col, std::min(255, a));
}
// a filled disc of chunky pixels
void disc(Canvas& c, float gx, float gy, float r, uint32_t col, int a = 255) {
    const int x0 = int(std::floor(gx - r)), x1 = int(std::ceil(gx + r));
    const int y0 = int(std::floor(gy - r)), y1 = int(std::ceil(gy + r));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float dx = float(x) + 0.5f - gx, dy = float(y) + 0.5f - gy;
            if (dx * dx + dy * dy <= r * r) P(c, x, y, col, a);
        }
}
float smooth(float t) {
    t = std::max(0.f, std::min(1.f, t));
    return t * t * (3.f - 2.f * t);
}

// pixel stars drifting left, in three layers
// (widescreen, 0.46.0: across the whole width - as many more stars as the screen is wider)
void pixelStars(Canvas& c, uint32_t now, int count, float speedScale, int alphaScale) {
    static const float kSpeed[3] = {4.f, 9.f, 19.f};  // grid pixels per second
    const int gw = std::max(320, gridR(c) - gridL(c)), g0 = gw > 320 ? gridL(c) : 0;
    const int n = count * gw / 320;
    for (int layer = 0; layer < 3; ++layer)
        for (int i = 0; i < n; ++i) {
            const uint32_t h = hash32(uint32_t(i * 7919 + layer * 104729 + 17));
            const int gy = int(h % 236u);
            // 0.32.0 (owner: "the rolling stars aren't showing"): the drift wraps by itself - it used to go
            // negative after a minute or two and every star fell off the left edge
            const double dd = double(now) * 0.001 * double(kSpeed[layer] * speedScale);
            const double drift = dd - std::floor(dd / double(gw)) * double(gw);  // (no fmod: glibc 2.38 versioned it - Steam Deck build)
            int gx = int(std::floor(double((h >> 9) % uint32_t(gw)) - drift));
            gx = ((gx % gw) + gw) % gw + g0;
            const uint32_t pick = (h >> 20) % 9u;
            const uint32_t col = pick == 0 ? rgb(255, 90, 210) : pick == 1 ? rgb(90, 220, 255) : pick == 2 ? rgb(255, 210, 150)
                                                                                                 : rgb(225, 228, 245);
            const float tw = 0.75f + 0.25f * float(std::sin(double(now) * 0.004 + double(h & 63u)));
            const int a = int(float(60 + layer * 70 + int((h >> 26) % 40u)) * tw) * alphaScale / 255;
            P(c, gx, gy, col, a);
            if (layer == 2 && (h & 7u) == 0) {  // a few bright ones get a little cross
                P(c, gx - 1, gy, col, a / 3);
                P(c, gx + 1, gy, col, a / 3);
                P(c, gx, gy - 1, col, a / 3);
                P(c, gx, gy + 1, col, a / 3);
            }
        }
}

// ---------------------------------------------------------------- the tear
float tearX(int gy) {  // the crack's centre line, jagged
    const int k = gy / 3;
    const float jag = (frand(uint32_t(k) * 31u + 5u) - 0.5f) * 3.f;
    const float jag2 = (frand(uint32_t(k + 1) * 31u + 5u) - 0.5f) * 3.f;
    const float f = float(gy % 3) / 3.f;
    return 27.f + 4.f * std::sin(float(gy) * 0.045f) + jag * (1.f - f) + jag2 * f;
}
float tearW(int gy) {  // half width: widest in the middle, closing at both ends
    const float u = float(gy - 24) / 192.f;
    if (u <= 0.f || u >= 1.f) return 0.f;
    return 0.6f + 7.f * float(std::pow(double(std::sin(kPi * u)), 0.8)) + 1.2f * std::sin(float(gy) * 0.31f);
}

void drawTear(Canvas& c, uint32_t now) {
    const float t = float(now % 3600000u) * 0.001f;  // (wrapped: a float can't hold days of milliseconds)
    const float pulse = 0.5f + 0.5f * std::sin(t * 1.7f);
    for (int gy = 18; gy < 222; ++gy) {
        const float cx = tearX(gy), w = tearW(gy);
        if (w <= 0.f) continue;
        // the glow around it
        for (int dx = -int(w) - 9; dx <= int(w) + 9; ++dx) {
            const float d = std::fabs(float(dx)) - w;
            if (d <= 0.f) continue;
            const int a = int((70.f + 40.f * pulse) * std::max(0.f, 1.f - d / 9.f));
            P(c, int(cx) + dx, gy, rgb(150, 10, 26), a);
        }
        for (int dx = -int(w); dx <= int(w); ++dx) {
            const float d = std::fabs(float(dx));
            const uint32_t h = hash32(uint32_t(gy * 977 + dx * 131));
            if (d > w - 1.2f) {  // the raw red edge, hot in places
                const bool hot = ((h >> 4) % 13u) == 0 && std::sin(t * 6.f + float(h & 31u)) > 0.3f;
                P(c, int(cx) + dx, gy, hot ? rgb(255, 150, 110) : rgb(225, 30, 48));
            } else {  // inside: the dark beyond, with veins of red that breathe
                const bool vein = ((h >> 7) % 9u) == 0;
                const float v = 0.5f + 0.5f * std::sin(t * 2.3f + float(gy) * 0.21f + float(dx));
                P(c, int(cx) + dx, gy, vein ? mix(rgb(40, 0, 10), rgb(140, 8, 30), int(v * 255)) : rgb(16, 0, 6));
            }
        }
    }
    // it bleeds: drops run down from the lower edges and fade
    for (int i = 0; i < 18; ++i) {
        const uint32_t h = hash32(uint32_t(i) * 2654435761u + 99u);
        const int gy0 = 110 + int(h % 95u);
        const float side = (h >> 8) & 1u ? 1.f : -1.f;
        const int gx = int(tearX(gy0) + side * (tearW(gy0) + 0.2f));
        const float period = 2.6f + float((h >> 12) % 100u) * 0.03f;
        const float ph = std::fmod(t / period + float((h >> 20) % 100u) * 0.01f, 1.f);
        const int len = 18 + int((h >> 3) % 22u);
        const int gy = gy0 + int(ph * float(len));
        const int a = int(255.f * (1.f - ph));
        P(c, gx, gy, rgb(200, 16, 36), a);
        P(c, gx, gy - 1, rgb(150, 10, 28), a * 2 / 3);
        P(c, gx, gy - 2, rgb(110, 6, 20), a / 3);
    }
    // red dust drifting out of it
    for (int i = 0; i < 26; ++i) {
        const uint32_t h = hash32(uint32_t(i) * 40503u + 7u);
        const int gy = 30 + int(h % 180u);
        const float ph = std::fmod(t * (0.10f + float((h >> 10) % 50u) * 0.002f) + float((h >> 16) % 100u) * 0.01f, 1.f);
        const int gx = int(tearX(gy) + tearW(gy) + 2.f + ph * 46.f);
        P(c, gx, gy - int(ph * 6.f), rgb(220, 40, 60), int(170.f * (1.f - ph)));
    }
}

// ---------------------------------------------------------------- the tentacle
void drawTentacle(Canvas& c, uint32_t now) {
    const uint32_t kCycle = 11000;
    const float ct = float(now % kCycle);
    float ext;
    if (ct < 2600.f) ext = smooth(ct / 2600.f);
    else if (ct < 6500.f) ext = 1.f;
    else if (ct < 8600.f) ext = 1.f - smooth((ct - 6500.f) / 2100.f);
    else ext = 0.f;
    if (ext <= 0.02f) return;
    const float t = float(now % 3600000u) * 0.001f;
    const int N = 30;
    const float seg = 2.4f;
    float px[N], py[N], pa[N];
    const int baseY = 158;
    float x = tearX(baseY) - 1.f, y = float(baseY);
    float a = -1.0f;  // it rises out of the tear, then curls back over itself (staying left of the menu)
    // as it pulls back the curl tightens; reaching out it unrolls
    const float tight = (1.f - ext) * 0.10f;
    for (int i = 0; i < N; ++i) {
        const float u = float(i) / float(N);
        a += -0.02f - (0.26f + tight) * u * u + 0.07f * std::sin(t * 1.5f + float(i) * 0.3f) * (0.3f + u);
        px[i] = x;
        py[i] = y;
        pa[i] = a;
        x += std::cos(a) * seg;
        y += std::sin(a) * seg;
    }
    const int vis = std::max(2, int(ext * float(N)));
    const uint32_t body = rgb(58, 104, 40), dark = rgb(30, 58, 22), light = rgb(118, 168, 72), sucker = rgb(176, 196, 116);
    for (int i = vis - 1; i >= 0; --i) {
        const float u = float(i) / float(N);
        const float r = 6.6f * float(std::pow(double(1.f - u), 0.85)) + 0.9f;
        // the end in reach tapers further while it is still coming out
        const float tipFade = i > vis - 4 ? 0.55f + 0.15f * float(vis - i) : 1.f;
        const float rr = r * tipFade;
        const int void_ = i < 6 ? 255 - i * 40 : 0;  // the part still in the rift is lost in its dark
        auto dim = [&](uint32_t col) { return mix(col, rgb(12, 0, 6), void_); };
        disc(c, px[i], py[i], rr + 0.8f, dim(dark));
        disc(c, px[i], py[i], rr, dim(body));
        const float nx = -std::sin(pa[i]), ny = std::cos(pa[i]);  // the lower side
        disc(c, px[i] - nx * rr * 0.35f, py[i] - ny * rr * 0.35f, rr * 0.45f, dim(light), 200);
        if (i % 2 == 0 && rr > 1.6f) {  // suckers along the inner curve
            P(c, int(px[i] + nx * rr * 0.72f), int(py[i] + ny * rr * 0.72f), dim(sucker));
            P(c, int(px[i] + nx * rr * 0.72f) + 1, int(py[i] + ny * rr * 0.72f), dim(rgb(120, 150, 80)), 200);
        }
    }
    // a little slime catching the light
    if (vis > 6) {
        const int i = vis - 3;
        P(c, int(px[i]), int(py[i]) - 1, rgb(200, 240, 160), 180);
    }
}

// ---------------------------------------------------------------- 0.46.2 (owner): more rifts, a war sun, the void dragon
// A smaller rift anywhere: centre column cx, from row top for len rows, at most wmax wide each side.
void drawRift(Canvas& c, uint32_t now, float cx0, int top, int len, float wmax, uint32_t seed) {
    const float t = float(now % 3600000u) * 0.001f;
    const float pulse = 0.5f + 0.5f * std::sin(t * 1.4f + float(seed % 7u));
    for (int gy = top; gy < top + len; ++gy) {
        const float u = float(gy - top) / float(len);
        const int k = gy / 3;
        const float f = float(gy % 3) / 3.f;
        const float jag = (frand(uint32_t(k) * 37u + seed) - 0.5f) * 2.4f * (1.f - f) + (frand(uint32_t(k + 1) * 37u + seed) - 0.5f) * 2.4f * f;
        const float cx = cx0 + 3.f * std::sin(float(gy) * 0.05f + float(seed % 13u)) + jag;
        const float w = 0.4f + wmax * float(std::pow(double(std::sin(kPi * u)), 0.8)) + 0.7f * std::sin(float(gy) * 0.33f + float(seed));
        if (w <= 0.f) continue;
        for (int dx = -int(w) - 6; dx <= int(w) + 6; ++dx) {  // the glow
            const float d = std::fabs(float(dx)) - w;
            if (d <= 0.f) continue;
            P(c, int(cx) + dx, gy, rgb(150, 10, 26), int((55.f + 35.f * pulse) * std::max(0.f, 1.f - d / 6.f)));
        }
        for (int dx = -int(w); dx <= int(w); ++dx) {
            const uint32_t h = hash32(uint32_t(gy * 977 + dx * 131) + seed);
            if (std::fabs(float(dx)) > w - 1.2f) {
                const bool hot = ((h >> 4) % 11u) == 0 && std::sin(t * 6.f + float(h & 31u)) > 0.3f;
                P(c, int(cx) + dx, gy, hot ? rgb(255, 150, 110) : rgb(215, 28, 46));
            } else {
                const float v = 0.5f + 0.5f * std::sin(t * 2.1f + float(gy) * 0.23f + float(dx));
                P(c, int(cx) + dx, gy, ((h >> 7) % 9u) == 0 ? mix(rgb(40, 0, 10), rgb(130, 8, 28), int(v * 255)) : rgb(14, 0, 6));
            }
        }
    }
}

// The void dragon's cycle: 0 hidden .. 1 risen (it rises from below the screen, lingers, sinks back)
float dragonUp(uint32_t now) {
    const float ct = float((now + 5200u) % 21000u);
    if (ct < 3800.f) return smooth(ct / 3800.f);
    if (ct < 11800.f) return 1.f;
    if (ct < 15600.f) return 1.f - smooth((ct - 11800.f) / 3800.f);
    return 0.f;
}

// Where things stand (in grid pixels): the war sun far off at the right, the dragon below it
struct Battle {
    float wx, wy, hx, hy, up;
};
Battle battleAt(const Canvas& c, uint32_t now) {
    Battle b;
    const float r = float(gridR(c));
    const float t = float(now % 3600000u) * 0.001f;
    b.wx = r - 32.f;
    b.wy = 50.f + 1.5f * std::sin(t * 0.5f);
    b.up = dragonUp(now);
    b.hx = (242.f + r) / 2.f + 6.f + 3.f * std::sin(t * 0.7f);  // (the middle of the room right of the New Game panel)
    b.hy = 290.f - b.up * 136.f + 2.5f * std::sin(t * 1.3f);  // (154 when risen; well below the screen when hidden)
    return b;
}

// a distant war sun: a dark sphere with a red-lit trench and a glowing core
void drawWarSun(Canvas& c, uint32_t now, float wx, float wy, bool firing) {
    const float t = float(now % 3600000u) * 0.001f;
    const float R = 13.f;
    disc(c, wx, wy, R + 3.f, rgb(60, 20, 70), 40);  // a faint halo against the haze
    disc(c, wx + 1.f, wy + 1.f, R, rgb(26, 26, 38));
    disc(c, wx, wy, R - 0.6f, rgb(58, 58, 74));
    disc(c, wx - 2.f, wy - 2.f, R - 3.5f, rgb(82, 82, 100));
    disc(c, wx - 4.f, wy - 4.f, R - 7.5f, rgb(130, 130, 150));
    for (int dx = -int(R) + 1; dx <= int(R) - 1; ++dx) {  // the trench round its middle
        P(c, int(wx) + dx, int(wy), rgb(20, 18, 28));
        const float ph = std::sin(t * 3.f - float(dx) * 0.7f);
        if ((dx + 40) % 3 == 0) P(c, int(wx) + dx, int(wy), rgb(255, 70, 40), int(120.f + 120.f * ph));
    }
    for (int i = 0; i < 9; ++i) {  // panel seams
        const uint32_t h = hash32(uint32_t(i) * 7919u + 3u);
        const float a = float(h % 628u) * 0.01f, rr = R * (0.3f + float((h >> 10) % 60u) * 0.01f);
        P(c, int(wx + std::cos(a) * rr), int(wy + std::sin(a) * rr * 0.8f), rgb(36, 36, 50));
    }
    // the core (the "eye" that fires): brighter while it shoots
    const float glow = firing ? 0.7f + 0.3f * std::sin(t * 18.f) : 0.35f + 0.15f * std::sin(t * 2.f);
    disc(c, wx - 4.f, wy + 3.5f, 3.2f, rgb(120, 20, 10));
    disc(c, wx - 4.f, wy + 3.5f, 2.2f, mix(rgb(200, 60, 20), rgb(255, 220, 140), int(glow * 255)));
    disc(c, wx - 4.f, wy + 3.5f, 6.f, rgb(255, 120, 40), int(glow * 60.f));
    P(c, int(wx) + 7, int(wy - R) + 1, rgb(160, 160, 180));  // an antenna
    P(c, int(wx) + 7, int(wy - R), rgb(255, 80, 60), int(160.f + 90.f * std::sin(t * 4.f)));
}

// the Vuil'raith void dragon: a serpent of the void with a long fanged skull, a cluster of green eyes and green
// tentacles writhing round its neck; (hx, hy) is the back of its skull, (dx, dy) where the snout points
void drawVoidDragon(Canvas& c, uint32_t now, float hx, float hy, float dx, float dy, float up) {
    if (up <= 0.01f) return;
    const float t = float(now % 3600000u) * 0.001f;
    const float dl = std::sqrt(dx * dx + dy * dy);
    dx /= dl;
    dy /= dl;
    const float nx = -dy, ny = dx;  // across the head (nx > 0: its lower side)
    // the void it drags along
    for (int i = 0; i < 4; ++i) disc(c, hx - 4.f, hy + 16.f + float(i) * 10.f, 34.f - float(i) * 5.f, rgb(34, 6, 50), 40);
    // the neck: up out of the bottom in a slow S
    const float bx = hx - 18.f, by = 262.f;
    float px[40], py[40], pr[40];
    const int NN = 40;
    for (int i = 0; i < NN; ++i) {
        const float u = float(i) / float(NN - 1);
        const float sway = 7.f * std::sin(u * 3.1f + t * 0.9f) * (1.f - u);
        px[i] = bx + (hx - bx) * u + sway;
        py[i] = by + (hy + 4.f - by) * u;
        pr[i] = 13.f - 3.f * u;
    }
    // tentacles: from the neck, curling out on both sides, waving
    auto tentacle = [&](int k, float sx, float sy, float side, float len) {
        float x = sx, y = sy, a = -kPi / 2.f + side * 0.9f;
        const int N = int(len);
        for (int i = 0; i < N; ++i) {
            const float u = float(i) / float(N);
            a += side * (-0.04f + 0.13f * u) + 0.13f * std::sin(t * (1.4f + 0.19f * float(k)) + float(i) * 0.33f + float(k) * 1.7f);
            x += std::cos(a) * 2.4f;
            y += std::sin(a) * 2.4f;
            const float r = 3.8f * (1.f - u) + 0.7f;
            disc(c, x, y, r + 0.8f, rgb(12, 34, 14));
            disc(c, x, y, r, mix(rgb(34, 110, 40), rgb(110, 220, 80), int(u * 220.f)));
            if (i % 3 == 1 && r > 1.4f) P(c, int(x + std::cos(a + side * 1.57f) * r * 0.7f), int(y + std::sin(a + side * 1.57f) * r * 0.7f), rgb(190, 255, 150), 210);
        }
        disc(c, x, y, 1.6f, rgb(190, 255, 160), 230);  // a glowing tip
        disc(c, x, y, 3.5f, rgb(120, 255, 120), 40);
    };
    const float reach = 0.45f + 0.55f * up;
    for (int k = 0; k < 4; ++k) {  // behind the neck
        const int i = 6 + k * 6;
        tentacle(k, px[i] + (k % 2 ? 6.f : -6.f), py[i], k % 2 ? 1.f : -1.f, (26.f + float(k % 3) * 6.f) * reach);
    }
    for (int i = 0; i < NN; ++i) {  // the neck itself: dark plates, lit along the left, green spots
        disc(c, px[i], py[i], pr[i] + 0.8f, rgb(60, 30, 90));
        disc(c, px[i] + 0.8f, py[i], pr[i], rgb(26, 8, 38));
        if (i % 4 == 0) {
            for (int k = -int(pr[i]) + 2; k <= int(pr[i]) - 2; ++k) P(c, int(px[i]) + k, int(py[i]), rgb(44, 16, 62));
            P(c, int(px[i] - pr[i] * 0.5f), int(py[i]) + 1, rgb(110, 255, 120), int(140.f + 100.f * std::sin(t * 3.f + float(i))));
        }
    }
    // the skull: the cranium, a crest of horns, then the jaws reaching for the war sun
    const float open = 0.18f + 0.22f * (0.5f + 0.5f * std::sin(t * 1.5f));
    const float ca = std::cos(open), sa = std::sin(open);
    const float jx = dx * ca - dy * sa, jy = dx * sa + dy * ca;  // the lower jaw's direction (turned down by `open`)
    for (int i = 0; i < 7; ++i) {  // horns sweeping back
        const float a = std::atan2(dy, dx) + kPi + (float(i) - 3.f) * 0.32f;
        const float L = 10.f + float(i % 3) * 3.f;
        for (int j = 0; j < int(L); ++j) {
            const float r = 1.6f * (1.f - float(j) / L) + 0.4f;
            disc(c, hx + std::cos(a) * (9.f + float(j)), hy + std::sin(a) * (9.f + float(j)) - float(j) * 0.4f, r,
                 j > int(L) - 4 ? rgb(150, 90, 190) : rgb(54, 22, 76));
        }
    }
    for (int i = 0; i < 9; ++i) {  // the lower jaw
        const float r = 6.5f - float(i) * 0.55f;
        disc(c, hx + jx * float(i) * 2.6f + nx * 3.f, hy + jy * float(i) * 2.6f + ny * 3.f, r + 0.7f, rgb(60, 30, 90));
        disc(c, hx + jx * float(i) * 2.6f + nx * 3.f, hy + jy * float(i) * 2.6f + ny * 3.f, r, rgb(30, 10, 42));
    }
    for (int i = 2; i < 10; ++i) {  // the maw: a green void glowing between the jaws
        const float mx = (dx + jx) * 0.5f, my = (dy + jy) * 0.5f;
        disc(c, hx + mx * float(i) * 2.5f + nx * 1.5f, hy + my * float(i) * 2.5f + ny * 1.5f, 3.f - float(i) * 0.2f,
             mix(rgb(30, 90, 40), rgb(170, 255, 140), int(128.f + 127.f * std::sin(t * 5.f + float(i)))));
    }
    disc(c, hx, hy, 12.f, rgb(70, 36, 104));  // the cranium
    disc(c, hx + 0.8f, hy + 0.8f, 11.f, rgb(28, 8, 40));
    for (int i = 0; i < 10; ++i) {  // the upper jaw / snout
        const float r = 8.f - float(i) * 0.62f;
        disc(c, hx + dx * float(i) * 2.7f - nx * 1.f, hy + dy * float(i) * 2.7f - ny * 1.f, r + 0.7f, rgb(70, 36, 104));
        disc(c, hx + dx * float(i) * 2.7f - nx * 0.5f, hy + dy * float(i) * 2.7f - ny * 0.5f, r, rgb(28, 8, 40));
    }
    for (int i = 3; i < 10; ++i) {  // fangs, down from the snout and up from the jaw
        const float ux = hx + dx * float(i) * 2.7f + nx * (7.2f - float(i) * 0.6f), uy = hy + dy * float(i) * 2.7f + ny * (7.2f - float(i) * 0.6f);
        P(c, int(ux), int(uy), rgb(235, 235, 215));
        if (i % 2) P(c, int(ux + nx), int(uy + ny), rgb(200, 200, 180));
        const float lx = hx + jx * float(i) * 2.6f + nx * (3.f - 5.f + float(i) * 0.5f), ly = hy + jy * float(i) * 2.6f + ny * (3.f - 5.f + float(i) * 0.5f);
        if (i % 2 == 0) P(c, int(lx), int(ly), rgb(225, 225, 205));
    }
    // eyes: a cluster of green ones along the skull, blinking out of step
    for (int i = 0; i < 6; ++i) {
        const float along = 1.f + float(i % 3) * 4.5f, across = -4.5f + float(i / 3) * 3.5f;
        const float ex = hx + dx * along + nx * across, ey = hy + dy * along + ny * across;
        if (std::fmod(t * (0.5f + 0.11f * float(i)) + float(i) * 0.41f, 3.f) < 0.14f) continue;
        disc(c, ex, ey, 3.f, rgb(90, 255, 110), 50);
        disc(c, ex, ey, 1.1f, rgb(170, 255, 140));
        P(c, int(ex), int(ey), rgb(240, 255, 230));
    }
    for (int k = 4; k < 8; ++k) {  // tentacles in front, round the head
        const int i = 22 + (k - 4) * 4;
        tentacle(k, px[i] + (k % 2 ? 9.f : -9.f), py[i] - 2.f, k % 2 ? 1.f : -1.f, (18.f + float(k % 3) * 5.f) * reach);
    }
}

// the war sun's missiles: a volley while the dragon is up, each with a smoke trail, bursting on it
void drawMissiles(Canvas& c, uint32_t now, float sx, float sy, float tx0, float ty0) {
    const uint32_t kEvery = 520, kFlight = 1400;
    for (int m = 0; m < 4; ++m) {
        const uint32_t launch = (now / kEvery - uint32_t(m)) * kEvery;
        if (launch > now || dragonUp(launch) < 0.98f || dragonUp(launch + kFlight) < 0.9f) continue;  // (only while it is up)
        const uint32_t age = now - launch;
        const uint32_t h = hash32(launch / kEvery + 17u);
        const float tx = tx0 + float(h % 13u) - 6.f, ty = ty0 + float((h >> 5) % 11u) - 5.f;
        const float bend = -18.f - float((h >> 9) % 22u), side = (h >> 14) & 1u ? 14.f : -10.f;
        auto at = [&](float u, float& x, float& y) {  // up and over, then down on it
            const float cx = (sx + tx) / 2.f + side, cy = std::min(sy, ty) + bend;
            x = (1 - u) * (1 - u) * sx + 2 * (1 - u) * u * cx + u * u * tx;
            y = (1 - u) * (1 - u) * sy + 2 * (1 - u) * u * cy + u * u * ty;
        };
        if (age < 160) disc(c, sx, sy, 2.5f, rgb(255, 230, 160), 220 - int(age));  // the launch flash
        if (age < kFlight) {
            const float u = float(age) / float(kFlight);
            for (int k = 14; k >= 0; --k) {  // smoke, then fire, then the missile
                const float uu = u - float(k) * 0.022f;
                if (uu < 0.f) continue;
                float x, y;
                at(uu, x, y);
                if (k > 5) disc(c, x, y, 0.9f + float(k) * 0.05f, rgb(120, 100, 130), 150 - k * 8);
                else if (k > 0) P(c, int(x), int(y), k < 3 ? rgb(255, 200, 90) : rgb(255, 120, 40), 230 - k * 20);
                else disc(c, x, y, 1.1f, rgb(255, 255, 230));
            }
        } else if (age < kFlight + 600) {  // the burst
            const float e = float(age - kFlight) / 600.f;
            const float rr = 2.f + e * 8.f;
            for (int i = 0; i < 16; ++i) {
                const float a = float(i) * kPi / 8.f + float(h % 7u);
                P(c, int(tx + std::cos(a) * rr), int(ty + std::sin(a) * rr), e < 0.5f ? rgb(255, 235, 150) : rgb(255, 110, 40), int(255.f * (1.f - e)));
            }
            if (e < 0.45f) disc(c, tx, ty, 3.5f * (1.f - e), rgb(255, 250, 220), int(255.f * (1.f - e / 0.45f)));
            disc(c, tx, ty - e * 6.f, 2.f + e * 4.f, rgb(90, 80, 100), int(120.f * (1.f - e)));
        }
    }
}

// ---------------------------------------------------------------- the cruiser
const char* kCruiser[] = {
    "            mmm             ",
    "         mmh###m            ",
    "   eem#######hhhhhm         ",
    "eeee##c#c#c#c##########mm   ",
    "eeee######################hh>",
    "eeee###################mm   ",
    "   eem#######mm             ",
};
constexpr int kCruiserRows = 7, kCruiserCols = 29;

}  // namespace

void drawPixelCruiser(Canvas& c, float x, float y, int scale, uint32_t now, int alpha) {
    // (x, y) = the bow tip; the sprite is drawn to its left
    const int ox = int(x) - (kCruiserCols - 1) * scale, oy = int(y) - 4 * scale;
    for (int r = 0; r < kCruiserRows; ++r)
        for (int k = 0; kCruiser[r][k]; ++k) {
            const char ch = kCruiser[r][k];
            if (ch == ' ') continue;
            uint32_t col;
            switch (ch) {
                case '#': col = rgb(48, 40, 88); break;
                case 'h': col = rgb(104, 92, 168); break;
                case 'm': col = rgb(255, 70, 200); break;
                case 'c': col = rgb(100, 236, 255); break;
                case 'e': col = rgb(128, 118, 150); break;
                default: col = rgb(255, 210, 240); break;
            }
            c.fillRect(ox + k * scale, oy + r * scale, scale, scale, col, alpha);
        }
    // thrusters: flickering flames behind the engines
    for (int r = 3; r <= 5; ++r) {
        const uint32_t h = hash32(uint32_t(now / 70u) * 13u + uint32_t(r));
        const int len = 3 + int(h % 5u) - (r == 4 ? 0 : 1);
        for (int k = 1; k <= len; ++k) {
            const float f = float(k) / float(len + 1);
            const uint32_t col = f < 0.3f ? rgb(255, 240, 190) : f < 0.65f ? rgb(255, 140, 70) : rgb(255, 60, 170);
            c.fillRect(ox - k * scale, oy + r * scale, scale, scale, col, int(float(alpha) * (1.f - f * 0.8f)));
        }
    }
}

void drawRetroNewGame(Canvas& c, uint32_t now) {
    WholeClip whole(c);  // (widescreen: the backdrop runs on to the screen's edges)
    c.verticalGradient(0, 0, 640, 480, rgb(8, 4, 20), rgb(20, 6, 30));
    pixelStars(c, now, 70, 0.6f, 255);
    // a faint synthwave haze low on the horizon
    for (int gy = 200; gy < 240; ++gy) {
        const int a = (gy - 200) * 2;
        for (int gx = std::min(0, gridL(c)); gx < std::max(320, gridR(c)); gx += 1) P(c, gx, gy, rgb(90, 16, 80), a / 3);
    }
    drawTear(c, now);
    // 0.46.2 (owner): more rifts - one at the right edge, more in a wider screen's sides
    drawRift(c, now, float(gridR(c)) - 12.f, 96, 96, 4.5f, 77u);
    if (gridL(c) < -20) drawRift(c, now, float(gridL(c)) + 26.f, 40, 150, 6.f, 913u);
    if (gridR(c) > 340) drawRift(c, now, float(gridR(c)) - 58.f, 8, 64, 3.f, 4421u);
    drawTentacle(c, now);
    // ...and far off at the right, a war sun firing on a Vuil'raith void dragon that rises from below and sinks back
    {
        const Battle b = battleAt(c, now);
        const bool room = gridR(c) - 242 >= 100;  // (the dragon only where a wider screen leaves room beside the panel)
        if (room) drawVoidDragon(c, now, b.hx, b.hy, b.wx - b.hx, b.wy - b.hy, b.up);
        if (room && b.up > 0.6f) drawMissiles(c, now, b.wx - 6.f, b.wy + 7.f, b.hx + 6.f, b.hy - 4.f);
        drawWarSun(c, now, b.wx, b.wy, room && b.up > 0.6f);
    }
    // the cruiser, far off, crossing the top of the screen now and then
    const uint32_t kPeriod = 24000, kCross = 17000;
    const uint32_t ph = now % kPeriod;
    if (ph < kCross) {
        const float u = float(ph) / float(kCross);
        const float l = float(gridL(c) * 2), r = float(gridR(c) * 2);
        const float x = l - 60.f + u * (r - l + 120.f);
        const float y = 26.f + 2.f * float(std::sin(double(now) * 0.0013));
        drawPixelCruiser(c, x, y, 2, now, 235);
    }
}

bool drawVoidfallNewGame(Canvas& c, uint32_t now) {
    static Image pic;
    static int gen = -1;
    if (pic.w != c.pw() || pic.h != c.ph() || gen != Canvas::scaleGeneration()) {
        gen = Canvas::scaleGeneration();
        // (widescreen, 0.46.0: the painting covers the wider screen - enlarged and its top and bottom trimmed, not stretched)
        const bool ok = c.fullWidth() > 640 ? voidfall::pictureCover("newgame", c.pw(), c.ph(), pic)
                                            : voidfall::picture("newgame", c.pw(), c.ph(), pic);
        if (!ok) return false;
    }
    if (pic.empty()) return false;
    WholeClip whole(c);
    c.pxBlit(pic, 0, 0);
    // embers drifting up and to the left from the tendrils
    const float t = float(now % 3600000u) / 1000.f;
    for (int i = 0; i < 46; ++i) {
        const uint32_t h = hash32(uint32_t(i) * 2246822519u + 911u);
        const float sp = 8.f + float(h % 23u);
        const float life = 640.f / sp;
        const float ph = float((h >> 8) % 1000u) / 1000.f * life;
        const float u = std::fmod(t + ph, life) / life;  // (float fmodf: an old symbol)
        const float x0 = 380.f + float((h >> 4) % 300u), y0 = 40.f + float((h >> 12) % 260u);
        const float x = x0 - u * 260.f, y = y0 - u * 120.f + 6.f * std::sin(t * 1.3f + float(i));
        const float fl = 0.55f + 0.45f * std::sin(t * 5.f + float(i) * 1.7f);
        const int a = int(200.f * fl * (1.f - u) * std::min(1.f, u * 8.f));
        c.fillCircle(x, y, 1.0f + 0.8f * float((h >> 20) & 1u), (h >> 21) & 1u ? rgb(255, 170, 90) : rgb(255, 110, 150), a);
    }
    voidfall::tendrils(c, Rect{0, 0, 640, 480}, now, 9311u, 5, 0.9f, 215);
    // a ship crossing from the lower left now and then, its engines lit
    const uint32_t kPeriod = 26000, kCross = 15000;
    const uint32_t ph = now % kPeriod;
    if (ph < kCross) {
        const float u = float(ph) / float(kCross);
        const float l = float(gridL(c) * 2), r = float(gridR(c) * 2);
        const float x = l - 80.f + u * (r - l + 180.f), y = 330.f - u * 210.f + 4.f * std::sin(t * 1.1f);
        voidfall::flame(c, x - 34.f, y + 3.f, -1.f, 70.f, 6.f, now, 5u, 235);
        voidfall::flame(c, x - 34.f, y - 5.f, -1.f, 50.f, 4.f, now, 9u, 200);
        if (!icons::drawUnitArt(c, "unit/dreadnought", x, y, 84, 40, rgb(70, 92, 96), false, 245)) c.fillCircle(x, y, 12.f, rgb(70, 92, 96));
        c.fillCircle(x + 6.f, y - 6.f, 18.f, rgb(255, 150, 80), 18);  // lit from the void's glow
    }
    return true;
}

void drawCredits(Canvas& c, uint32_t now) {
    WholeClip whole(c);  // (widescreen: the starscape and the ships run on to the screen's edges)
    c.fillRect(0, 0, 640, 480, rgb(2, 2, 8), 252);  // darker than before (owner)
    pixelStars(c, now, 90, 2.2f, 255);
    const float l = float(gridL(c) * 2), r = float(gridR(c) * 2);
    // ships in hyperspace beaming across, left to right
    for (int i = 0; i < 7; ++i) {
        const uint32_t h = hash32(uint32_t(i) * 2246822519u + 3u);
        const uint32_t period = 3000u + (h % 3000u);
        const uint32_t ph = (now + (h >> 8) % period) % period;
        const uint32_t beam = 1500u;
        if (ph >= beam) continue;
        const float u = float(ph) / float(beam);
        const float hx = l - 80.f + u * (r - l + 180.f);
        const float hy = 30.f + float((h >> 12) % 400u);
        const uint32_t core = (h >> 3) & 1u ? rgb(90, 230, 255) : rgb(255, 90, 220);
        // the trail: chunky bands that thin and fade behind the ship
        const int trail = 220;
        const int sc = (h >> 14) % 3u == 0 ? 2 : 1;  // some pass closer
        for (int k = 0; k < trail * sc; k += 2) {
            const float f = float(k) / float(trail * sc);
            const int a = int(235.f * (1.f - f) * (1.f - f));
            const int x = int(hx) - 30 * sc - k;
            if (x < int(l) - 2 || x > int(r) + 2) continue;
            c.fillRect(x, int(hy) - 2 * sc, 2, 3 * sc, core, a);
            if (k < 140 * sc) c.fillRect(x, int(hy) - 2 * sc - 1, 2, 1, rgb(255, 255, 255), a / 2);
            if (k < 90 * sc) c.fillRect(x, int(hy) + sc, 2, 1, rgb(255, 255, 255), a / 3);
        }
        drawPixelCruiser(c, hx, hy, sc, now, 255);
        // a flash where it drops out of hyperspace
        if (u > 0.92f) c.fillCircle(hx, hy, 6.f * (u - 0.92f) / 0.08f + 2.f, rgb(255, 255, 255), 120);
    }
}

}  // namespace backdrops
