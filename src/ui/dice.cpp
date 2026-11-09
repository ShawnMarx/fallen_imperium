#include "ui/dice.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gfx/font.h"
#include "ui/icons.h"

namespace dice {

namespace {
constexpr float kPi = 3.14159265f;

// The silhouette of a ten-sided die seen face on: the kite face in front, a
// facet either side, the lower facets under it (unit height, centred).
const float kOuter[6][2] = {{0, -0.5f}, {0.43f, -0.12f}, {0.40f, 0.18f}, {0, 0.5f}, {-0.40f, 0.18f}, {-0.43f, -0.12f}};
const float kFace[4][2] = {{0, -0.5f}, {0.27f, 0.06f}, {0, 0.32f}, {-0.27f, 0.06f}};
const float kLowR[4][2] = {{0.27f, 0.06f}, {0.40f, 0.18f}, {0, 0.5f}, {0, 0.32f}};
const float kLowL[4][2] = {{-0.27f, 0.06f}, {-0.40f, 0.18f}, {0, 0.5f}, {0, 0.32f}};
const float kSideR[4][2] = {{0, -0.5f}, {0.43f, -0.12f}, {0.40f, 0.18f}, {0.27f, 0.06f}};
const float kSideL[4][2] = {{0, -0.5f}, {-0.43f, -0.12f}, {-0.40f, 0.18f}, {-0.27f, 0.06f}};

struct Xf {
    float cx, cy, size, cs, sn, sq;
    void at(float x, float y, float& ox, float& oy) const {
        x *= sq;
        ox = cx + (x * cs - y * sn) * size;
        oy = cy + (x * sn + y * cs) * size;
    }
};

void face(Canvas& c, const Xf& t, const float (*p)[2], int n, uint32_t col, int a) {
    float xs[8], ys[8];
    for (int i = 0; i < n; ++i) t.at(p[i][0], p[i][1], xs[i], ys[i]);
    c.fillPolygon(xs, ys, n, col, a);
}

void edges(Canvas& c, const Xf& t, const float (*p)[2], int n, uint32_t col, float thick, int a) {
    for (int i = 0; i < n; ++i) {
        float x0, y0, x1, y1;
        t.at(p[i][0], p[i][1], x0, y0);
        t.at(p[(i + 1) % n][0], p[(i + 1) % n][1], x1, y1);
        c.line(x0, y0, x1, y1, col, thick, a);
    }
}
}  // namespace

void drawD10(Canvas& c, float cx, float cy, float size, int value, bool hit, float spin, int faction, int alpha) {
    if (alpha <= 0) return;
    const bool rolling = spin >= 0.f;
    Xf t{cx, cy, size, 1.f, 0.f, 1.f};
    int shown = value <= 0 ? 10 : value;
    if (rolling) {
        // tumbling: it turns, flattens as it shows its edge, bounces and flickers numbers
        const float k = 1.f - spin;  // energy left
        const float rot = std::sin(spin * kPi * 5.f) * 0.9f * k;
        t.cs = std::cos(rot);
        t.sn = std::sin(rot);
        t.sq = 0.55f + 0.45f * std::fabs(std::cos(spin * kPi * 7.f));
        t.cy -= std::fabs(std::sin(spin * kPi * 3.f)) * size * 0.35f * k;
        shown = 1 + int(std::fabs(std::sin(spin * 97.f + cx * 0.37f)) * 10.f) % 10;
    }
    const bool green = hit && !rolling;
    const uint32_t body = green ? rgb(26, 92, 44) : rgb(34, 35, 40);
    const uint32_t front = green ? rgb(48, 156, 72) : rgb(64, 66, 74);
    const uint32_t sideCol = green ? rgb(36, 120, 56) : rgb(48, 50, 57);
    const uint32_t low = green ? rgb(22, 80, 38) : rgb(28, 29, 34);
    const uint32_t edge = green ? rgb(160, 245, 170) : rgb(132, 136, 148);
    // shadow, then the facets
    {
        Xf s = t;
        s.cx += size * 0.06f;
        s.cy += size * 0.08f;
        face(c, s, kOuter, 6, rgb(0, 0, 0), alpha * 110 / 255);
    }
    face(c, t, kOuter, 6, body, alpha);
    face(c, t, kSideR, 4, shade(sideCol, 0.85f), alpha);
    face(c, t, kSideL, 4, sideCol, alpha);
    face(c, t, kLowR, 4, low, alpha);
    face(c, t, kLowL, 4, shade(low, 1.15f), alpha);
    face(c, t, kFace, 4, front, alpha);
    const float thick = std::max(1.f, size / 26.f);
    edges(c, t, kFace, 4, edge, thick, alpha * 200 / 255);
    edges(c, t, kOuter, 6, shade(edge, 0.7f), thick, alpha * 170 / 255);
    if (green) {  // a soft glow round a hit
        c.fillCircle(cx, cy, size * 0.62f, rgb(90, 255, 120), alpha * 40 / 255);
    }
    // the number on the front face
    const FontData& f = size >= 30 ? FONT_MDB : size >= 21 ? FONT_SMB : FONT_XS;
    float nx, ny;
    t.at(0, -0.06f, nx, ny);
    const uint32_t numCol = rolling ? rgb(210, 214, 224) : green ? rgb(255, 255, 255) : rgb(190, 192, 200);
    drawTextAligned(c, f, int(nx) - 20, int(ny) - f.lineHeight / 2, 40, Align::Center, std::to_string(shown), numCol, alpha);
    if (shown == 6 || shown == 9) c.hline(int(nx) - 3, int(nx) + 3, int(ny) + f.lineHeight / 2 - 1, numCol, alpha);  // 6 / 9
    // the roller's emblem in the corner
    if (faction >= 0 && size >= 16) {
        const float es = std::max(9.f, size * 0.42f);
        const float ex = cx + size * 0.34f, ey = cy - size * 0.38f;
        c.fillCircle(ex, ey, es * 0.58f, rgb(12, 12, 16), alpha * 220 / 255);
        c.ring(ex, ey, es * 0.58f, 1.f, edge, alpha * 160 / 255);
        drawFactionIcon(c, faction, int(ex - es / 2.f), int(ey - es / 2.f), int(es), alpha);
    }
}

float Timing::spin(uint32_t now, int i, int n) const {
    const int t = int(now - start) - int(appear);
    if (roll == 0 || n <= 0) return -1.f;
    const float settle = float(roll) * (0.45f + 0.55f * float(i + 1) / float(n));
    if (float(t) >= settle) return -1.f;
    return std::max(0.f, std::min(1.f, float(t) / settle));
}

int Timing::alpha(uint32_t now) const {
    const int t = int(now - start) - int(appear);
    if (t < 0) return 0;
    int a = t < 120 ? t * 255 / 120 : 255;
    if (fadeAt) {
        const int f = int(now - fadeAt);
        if (f >= int(fade)) return 0;
        if (f > 0) a = std::min(a, 255 - f * 255 / int(std::max<uint32_t>(1, fade)));
    }
    return std::max(0, std::min(255, a));
}

bool Timing::settled(uint32_t now, int n) const { return n <= 0 || spin(now, n - 1, n) < 0.f; }

int drawRoll(Canvas& c, float cx, float bottom, float size, int perRow, const std::vector<int8_t>& values, const std::vector<int8_t>& need,
             int faction, const Timing& t, uint32_t now) {
    const int a = t.alpha(now);
    const int n = int(values.size());
    if (a <= 0 || n == 0) return 0;
    perRow = std::max(1, perRow);
    const int rows = (n + perRow - 1) / perRow;
    const float stepX = size * 0.98f, stepY = size * 1.08f;
    for (int i = 0; i < n; ++i) {
        const int row = i / perRow, col = i % perRow;
        const int inRow = std::min(perRow, n - row * perRow);
        const float x = cx + (float(col) - float(inRow - 1) / 2.f) * stepX;
        const float y = bottom - size * 0.5f - float(rows - 1 - row) * stepY;
        const bool hit = i < int(need.size()) && values[size_t(i)] >= need[size_t(i)];
        drawD10(c, x, y, size, values[size_t(i)], hit, t.spin(now, i, n), faction, a);
    }
    return int(float(rows) * stepY);
}

}  // namespace dice
