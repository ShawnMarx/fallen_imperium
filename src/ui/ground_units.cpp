// Faction ground units (0.44.0) - see ground_units.h. Our own drawings, after the owner's notes
// (docs/owner/ground_overhaul/groundcombat.txt) and the pictures he sent for inspiration.
//
// Coordinates: an infantry Pose's (x, y) is where the game's soldier has always been drawn - his
// middle; the ground under him is y + 0.44 s. A mech's or structure's (x, y) is its feet. `lift`
// moves the body up from its shadow.
#include "ui/ground_units.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "gfx/canvas.h"
#include "ui/icons.h"
#include "ui/painted.h"

namespace gunits {

namespace {

constexpr float kPi = 3.14159265f;
bool g_factionUnits = true;

uint32_t lighter(uint32_t c, float k) { return mix(c, rgb(255, 255, 255), int(255.f * std::max(0.f, std::min(1.f, k)))); }
uint32_t darker(uint32_t c, float k) { return mix(c, rgb(0, 0, 0), int(255.f * std::max(0.f, std::min(1.f, k)))); }
uint32_t tint(uint32_t natural, uint32_t team, float k) { return mix(natural, team, int(255.f * k)); }
// Retrowave: louder colours (the chunky pass then turns them into neon pixels)
uint32_t pop(uint32_t c, int style) {
    if (style != 1) return c;
    const int r = int((c >> 16) & 255), g = int((c >> 8) & 255), b = int(c & 255);
    const int m = (r + g + b) / 3;
    auto ch = [&](int v) { return std::max(0, std::min(255, m + (v - m) * 3 / 2 + 12)); };
    return rgb(ch(r), ch(g), ch(b));
}
int al(int a, int k) { return a * k / 255; }
float secs(uint32_t now) { return float(now % 3600000u) / 1000.f; }

void ell(Canvas& c, float x, float y, float rx, float ry, uint32_t col, int a) {
    float xs[14], ys[14];
    for (int i = 0; i < 14; ++i) {
        const float t = 2.f * kPi * float(i) / 14.f;
        xs[i] = x + std::cos(t) * rx;
        ys[i] = y + std::sin(t) * ry;
    }
    c.fillPolygon(xs, ys, 14, col, a);
}
// half an ellipse: the top (dome) half when up, the bottom half otherwise
void dome(Canvas& c, float x, float y, float rx, float ry, uint32_t col, int a, bool up = true) {
    float xs[10], ys[10];
    for (int i = 0; i < 9; ++i) {
        const float t = kPi * float(i) / 8.f;
        xs[i] = x - std::cos(t) * rx;
        ys[i] = y + (up ? -1.f : 1.f) * std::sin(t) * ry;
    }
    c.fillPolygon(xs, ys, 9, col, a);
}
void limb(Canvas& c, float x0, float y0, float x1, float y1, float w, uint32_t col, int a) { c.line(x0, y0, x1, y1, col, std::max(1.f, w), a); }
void quad(Canvas& c, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint32_t col, int a) {
    float xs[4] = {x0, x1, x2, x3}, ys[4] = {y0, y1, y2, y3};
    c.fillPolygon(xs, ys, 4, col, a);
}
void tri(Canvas& c, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col, int a) {
    float xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
    c.fillPolygon(xs, ys, 3, col, a);
}
// the shadow on the ground: smaller and fainter the higher the unit is
void shadowAt(Canvas& c, float x, float gy, float rx, float ry, float lift, float s, int a) {
    const float k = std::max(0.35f, 1.f - lift / std::max(1.f, s * 3.f));
    ell(c, x, gy, rx * k, ry * k, rgb(0, 0, 0), al(a, int(70.f * k)));
}

// ------------------------------------------------------------------ the game's own soldier, in parts
void manLegs(Canvas& c, const Pose& p, float y, uint32_t dark) {
    const float s = p.s, st = std::sin(p.walk) * s * 0.1f;
    limb(c, p.x - s * 0.1f, y + s * 0.06f, p.x - s * 0.13f + st, y + s * 0.44f, s * 0.13f, dark, p.a);
    limb(c, p.x + s * 0.1f, y + s * 0.06f, p.x + s * 0.13f - st, y + s * 0.44f, s * 0.13f, dark, p.a);
}
void manTorso(Canvas& c, const Pose& p, float y, uint32_t col, uint32_t dark) {
    const float s = p.s;
    quad(c, p.x - s * 0.22f, y - s * 0.2f, p.x + s * 0.22f, y - s * 0.2f, p.x + s * 0.17f, y + s * 0.12f, p.x - s * 0.17f, y + s * 0.12f, col, p.a);
    c.fillRect(int(p.x - s * 0.18f), int(y - s * 0.02f), int(s * 0.36f), std::max(1, int(s * 0.07f)), dark, p.a);  // belt
}
void manHead(Canvas& c, const Pose& p, float y, uint32_t col) {
    const float s = p.s;
    c.fillCircle(p.x, y - s * 0.33f, s * 0.17f, lighter(col, 0.2f), p.a);  // helmet
    if (p.dir > 0) c.fillRect(int(p.x - s * 0.09f), int(y - s * 0.33f), int(s * 0.18f) + 1, std::max(1, int(s * 0.06f)), rgb(140, 230, 255), p.a);
}
void manRifle(Canvas& c, const Pose& p, float y, uint32_t gun = rgb(40, 40, 44)) {
    const float s = p.s;
    const float gx0 = p.x + s * 0.16f, gy0 = y - s * 0.08f;
    const float gx1 = p.x + s * 0.26f, gy1 = p.dir > 0 ? y + s * 0.34f : y - s * 0.58f;
    limb(c, gx0, gy0, gx1, gy1, s * 0.1f, gun, p.a);
    if (p.firing) c.fillCircle(gx1, gy1, s * 0.16f, rgb(255, 230, 140), p.a);
}
// Voidfall (0.51.1, owner): the painted soldier - the same man, lit like the tiles: a warm key light from up and left,
// the void's violet light on his edges from behind (up and right), violet shadows, a glowing visor and rifle cell.
const uint32_t kVoidShade = painted::kShade, kVoidRim = painted::kRim, kVoidWarm = painted::kWarm;
void paintedMan(Canvas& c, const Pose& p) {
    const float s = p.s, x = p.x, y = p.y, hy = y - s * 0.33f, hr = s * 0.17f;
    const uint32_t col = p.col;
    const uint32_t shade = mix(darker(col, 0.3f), kVoidShade, 110);
    const uint32_t deep = mix(darker(col, 0.55f), kVoidShade, 130);
    const uint32_t lit = mix(lighter(col, 0.3f), kVoidWarm, 80);
    const float st = std::sin(p.walk) * s * 0.1f;
    const float rx = s * 0.026f, ry = -s * 0.022f;  // the rim: where the void's light catches his edges
    // a soft violet pool under his feet
    ell(c, x + s * 0.05f, y + s * 0.45f + p.lift, s * 0.34f, s * 0.1f, rgb(14, 4, 28), al(p.a, 55));
    ell(c, x + s * 0.05f, y + s * 0.45f + p.lift, s * 0.22f, s * 0.065f, rgb(14, 4, 28), al(p.a, 90));
    auto legs = [&](float ox, float oy, uint32_t cl, uint32_t cr, int a) {
        limb(c, x - s * 0.1f + ox, y + s * 0.06f + oy, x - s * 0.13f + st + ox, y + s * 0.44f + oy, s * 0.13f, cl, a);
        limb(c, x + s * 0.1f + ox, y + s * 0.06f + oy, x + s * 0.13f - st + ox, y + s * 0.44f + oy, s * 0.13f, cr, a);
    };
    auto torso = [&](float ox, float oy, uint32_t cl, int a) {
        quad(c, x - s * 0.22f + ox, y - s * 0.2f + oy, x + s * 0.22f + ox, y - s * 0.2f + oy, x + s * 0.17f + ox, y + s * 0.12f + oy,
             x - s * 0.17f + ox, y + s * 0.12f + oy, cl, a);
    };
    // 1. the rim, behind him (it shows only along his upper right edges)
    legs(rx, ry, kVoidRim, kVoidRim, al(p.a, 170));
    torso(rx, ry, kVoidRim, al(p.a, 190));
    c.fillCircle(x + rx, hy + ry, hr, kVoidRim, al(p.a, 210));
    // 2. legs: the near one in the light, the far one in shadow
    legs(0, 0, mix(col, shade, 170), deep, p.a);
    // 3. the body: lit on the left, shadow on the right, a warm catch on the shoulder
    torso(0, 0, col, p.a);
    quad(c, x + s * 0.03f, y - s * 0.2f, x + s * 0.22f, y - s * 0.2f, x + s * 0.17f, y + s * 0.12f, x - s * 0.01f, y + s * 0.12f, shade, al(p.a, 185));
    quad(c, x - s * 0.03f, y - s * 0.2f, x + s * 0.03f, y - s * 0.2f, x - s * 0.01f, y + s * 0.12f, x - s * 0.06f, y + s * 0.12f, mix(col, shade, 128), al(p.a, 170));
    quad(c, x - s * 0.22f, y - s * 0.2f, x - s * 0.05f, y - s * 0.2f, x - s * 0.08f, y - s * 0.12f, x - s * 0.205f, y - s * 0.12f, lit, al(p.a, 210));
    quad(c, x - s * 0.18f, y + s * 0.06f, x + s * 0.18f, y + s * 0.06f, x + s * 0.17f, y + s * 0.12f, x - s * 0.17f, y + s * 0.12f, deep, al(p.a, 110));
    c.fillRect(int(x - s * 0.18f), int(y - s * 0.02f), int(s * 0.36f), std::max(1, int(s * 0.07f)), deep, p.a);  // belt
    c.fillCircle(x - s * 0.07f, y + s * 0.015f, s * 0.03f, kVoidWarm, al(p.a, 200));                                // its buckle
    // 4. the helmet: shaded round, a warm glint
    c.fillCircle(x, hy, hr, shade, p.a);
    c.fillCircle(x - hr * 0.15f, hy - hr * 0.15f, hr * 0.8f, mix(lighter(col, 0.2f), kVoidWarm, 40), p.a);
    c.fillCircle(x - hr * 0.42f, hy - hr * 0.45f, hr * 0.24f, mix(lighter(col, 0.6f), kVoidWarm, 140), al(p.a, 190));
    if (p.dir > 0) {  // the visor glows in the dusk
        glow(c, x, hy + s * 0.03f, s * 0.1f, rgb(110, 215, 255), al(p.a, 140), 2);
        c.fillRect(int(x - s * 0.1f), int(hy), int(s * 0.2f) + 1, std::max(1, int(s * 0.065f)), rgb(150, 235, 255), p.a);
        c.fillRect(int(x - s * 0.06f), int(hy + s * 0.012f), int(s * 0.12f) + 1, std::max(1, int(s * 0.03f)), rgb(235, 252, 255), al(p.a, 220));
    } else {  // from behind: a small light on the side of the helmet
        glow(c, x + hr * 0.62f, hy - hr * 0.1f, s * 0.055f, rgb(110, 215, 255), al(p.a, 170), 2);
        c.fillCircle(x + hr * 0.62f, hy - hr * 0.1f, std::max(0.6f, s * 0.025f), rgb(220, 248, 255), p.a);
    }
    // 5. the rifle: gunmetal with a lit edge and a glowing cell
    const float gx0 = x + s * 0.16f, gy0 = y - s * 0.08f;
    const float gx1 = x + s * 0.26f, gy1 = p.dir > 0 ? y + s * 0.34f : y - s * 0.58f;
    limb(c, gx0 + s * 0.03f, gy0, gx1 + s * 0.03f, gy1, s * 0.045f, kVoidRim, al(p.a, 110));
    limb(c, gx0, gy0, gx1, gy1, s * 0.1f, rgb(36, 32, 48), p.a);
    limb(c, gx0 - s * 0.03f, gy0, gx1 - s * 0.03f, gy1, s * 0.035f, rgb(120, 108, 140), al(p.a, 170));
    const float cx0 = gx0 + (gx1 - gx0) * 0.3f, cy0 = gy0 + (gy1 - gy0) * 0.3f;
    const float cx1 = gx0 + (gx1 - gx0) * 0.55f, cy1 = gy0 + (gy1 - gy0) * 0.55f;
    glow(c, (cx0 + cx1) * 0.5f, (cy0 + cy1) * 0.5f, s * 0.06f, rgb(200, 120, 255), al(p.a, 120), 2);
    limb(c, cx0, cy0, cx1, cy1, s * 0.04f, rgb(236, 190, 255), p.a);
    if (p.firing) {  // a painted flash, not a flat disc
        glow(c, gx1, gy1, s * 0.15f, rgb(255, 180, 100), al(p.a, 230), 2);
        c.fillCircle(gx1, gy1, s * 0.07f, rgb(255, 250, 232), p.a);
    }
}
bool g_voidPaint = std::getenv("TI4_VOIDCLASSIC") == nullptr;  // (off: Voidfall as it was - test pictures and benches only)
const float kMechEyeX = 0.f, kMechEyeY = 0.86f, kMechCoreX = 0.f, kMechCoreY = 0.62f;  // on the picture (x from its middle)

void defaultMan(Canvas& c, const Pose& p) {
    if (p.style == 2 && g_voidPaint) {
        paintedMan(c, p);
        return;
    }
    const uint32_t dark = darker(p.col, 0.45f);
    c.fillCircle(p.x + p.s * 0.12f, p.y + p.s * 0.44f + p.lift, p.s * 0.26f, rgb(0, 0, 0), al(p.a, 70));
    manLegs(c, p, p.y, dark);
    manTorso(c, p, p.y, p.col, dark);
    manHead(c, p, p.y, p.col);
    manRifle(c, p, p.y);
}
void seams(Canvas& c, const Pose& p, uint32_t sc) {  // Creuss / Mahact: glowing seams on the default soldier
    const float s = p.s, x = p.x, y = p.y;
    const float w = std::max(0.8f, s * 0.05f);
    glow(c, x, y - s * 0.06f, s * 0.2f, sc, al(p.a, 120), p.style);
    limb(c, x, y - s * 0.19f, x, y + s * 0.1f, w, sc, p.a);
    limb(c, x - s * 0.18f, y + s * 0.01f, x + s * 0.18f, y + s * 0.01f, w, lighter(sc, 0.3f), p.a);
    limb(c, x - s * 0.2f, y - s * 0.17f, x - s * 0.08f, y - s * 0.05f, w, sc, al(p.a, 200));
    limb(c, x + s * 0.2f, y - s * 0.17f, x + s * 0.08f, y - s * 0.05f, w, sc, al(p.a, 200));
    c.ring(x, y - s * 0.33f, s * 0.17f, w, sc, al(p.a, 220));
    c.fillRect(int(x - s * 0.09f), int(y - s * 0.34f), int(s * 0.18f) + 1, std::max(1, int(s * 0.07f)), lighter(sc, 0.5f), p.a);
}

// ------------------------------------------------------------------ infantry, faction by faction
void arborecInf(Canvas& c, const Pose& p) {  // small humanoid plants
    const float s = p.s, x = p.x, y = p.y, t = secs(p.now) + p.phase;
    const uint32_t stem = pop(tint(rgb(64, 122, 46), p.col, 0.28f), p.style), leaf = lighter(stem, 0.18f);
    const uint32_t pod = pop(tint(rgb(150, 172, 74), p.col, 0.42f), p.style);
    c.fillCircle(x + s * 0.1f, y + s * 0.44f + p.lift, s * 0.26f, rgb(0, 0, 0), al(p.a, 60));
    const float st = std::sin(p.walk) * s * 0.08f, sway = std::sin(t * 2.1f) * s * 0.05f;
    limb(c, x - s * 0.05f, y + s * 0.08f, x - s * 0.15f + st, y + s * 0.44f, s * 0.1f, darker(stem, 0.35f), p.a);  // root legs
    limb(c, x + s * 0.05f, y + s * 0.08f, x + s * 0.15f - st, y + s * 0.44f, s * 0.1f, darker(stem, 0.35f), p.a);
    limb(c, x - s * 0.15f + st, y + s * 0.44f, x - s * 0.25f + st, y + s * 0.46f, s * 0.06f, darker(stem, 0.4f), p.a);
    limb(c, x + s * 0.15f - st, y + s * 0.44f, x + s * 0.25f - st, y + s * 0.46f, s * 0.06f, darker(stem, 0.4f), p.a);
    quad(c, x - s * 0.13f, y - s * 0.18f, x + s * 0.13f, y - s * 0.18f, x + s * 0.07f, y + s * 0.12f, x - s * 0.07f, y + s * 0.12f, stem, p.a);
    // leafy arms
    quad(c, x - s * 0.1f, y - s * 0.14f, x - s * 0.3f, y - s * 0.2f + sway, x - s * 0.42f, y - s * 0.02f + sway, x - s * 0.16f, y - s * 0.04f, leaf, p.a);
    quad(c, x + s * 0.1f, y - s * 0.14f, x + s * 0.3f, y - s * 0.2f - sway, x + s * 0.42f, y - s * 0.02f - sway, x + s * 0.16f, y - s * 0.04f, leaf, p.a);
    // the head: a seed pod with curling feelers
    const float hy = y - s * 0.34f;
    const uint32_t curl = pop(tint(rgb(130, 96, 60), p.col, 0.2f), p.style);
    for (int k = -1; k <= 1; ++k) {
        const float fx = x + float(k) * s * 0.09f, w = std::sin(t * 2.6f + float(k)) * s * 0.06f;
        limb(c, fx, hy - s * 0.1f, fx + float(k) * s * 0.12f + w, hy - s * 0.3f, s * 0.06f, curl, p.a);
        limb(c, fx + float(k) * s * 0.12f + w, hy - s * 0.3f, fx + float(k) * s * 0.2f - w, hy - s * 0.26f, s * 0.05f, curl, p.a);
    }
    c.fillCircle(x, hy, s * 0.17f, pod, p.a);
    c.fillCircle(x - s * 0.05f, hy - s * 0.05f, s * 0.07f, lighter(pod, 0.3f), al(p.a, 200));
    c.fillCircle(x, hy + p.dir * s * 0.06f, s * 0.06f, rgb(70, 24, 30), al(p.a, 220));  // the mouth that spits the spores
    if (p.firing) glow(c, x, hy + p.dir * s * 0.12f, s * 0.2f, rgb(160, 240, 90), p.a, p.style);
}

void muaatInf(Canvas& c, const Pose& p) {  // brown metal eggs on four mechanical legs
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t metal = rgb(70, 60, 54), egg = pop(tint(rgb(128, 88, 54), p.col, 0.3f), p.style);
    c.fillCircle(x + s * 0.06f, y + s * 0.44f + p.lift, s * 0.32f, rgb(0, 0, 0), al(p.a, 60));
    const float lift = std::sin(p.walk) * s * 0.06f;
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) {  // front and back leg each side
            const float hx = x + float(side) * s * 0.12f, hy = y - s * (k ? 0.02f : 0.1f);
            const float kx = x + float(side) * s * (k ? 0.36f : 0.32f), ky = y - s * (k ? 0.12f : 0.24f) - (k == (side > 0) ? lift : -lift);
            const float fx = x + float(side) * s * (k ? 0.4f : 0.3f), fy = y + s * (k ? 0.44f : 0.36f);
            limb(c, hx, hy, kx, ky, s * 0.08f, metal, p.a);
            limb(c, kx, ky, fx, fy, s * 0.06f, lighter(metal, 0.15f), p.a);
        }
    ell(c, x, y - s * 0.12f, s * 0.2f, s * 0.27f, egg, p.a);
    ell(c, x - s * 0.07f, y - s * 0.22f, s * 0.07f, s * 0.1f, lighter(egg, 0.3f), al(p.a, 200));
    c.fillRect(int(x - s * 0.19f), int(y - s * 0.07f), int(s * 0.38f), std::max(1, int(s * 0.05f)), darker(egg, 0.4f), p.a);
    const float ey = y - s * 0.12f + p.dir * s * 0.08f;
    glow(c, x, ey, s * 0.13f, rgb(255, 140, 40), al(p.a, 180), p.style);
    c.fillCircle(x, ey, s * 0.06f, p.firing ? rgb(255, 240, 180) : rgb(255, 160, 50), p.a);
}

void naaluInf(Canvas& c, const Pose& p) {  // the default soldier above the waist, a snake below
    const float s = p.s, x = p.x, y = p.y, t = secs(p.now) + p.phase;
    const uint32_t tail = pop(tint(rgb(118, 150, 88), p.col, 0.45f), p.style), belly = lighter(tail, 0.3f);
    c.fillCircle(x + s * 0.12f, y + s * 0.44f + p.lift, s * 0.3f, rgb(0, 0, 0), al(p.a, 60));
    const float w = std::sin(t * 3.f + p.walk * 0.3f) * s * 0.06f;
    const float xs[5] = {x, x + s * 0.12f + w, x - s * 0.02f - w, x + s * 0.2f + w, x + s * 0.34f};
    const float ys[5] = {y + s * 0.08f, y + s * 0.22f, y + s * 0.34f, y + s * 0.42f, y + s * 0.38f};
    const float wd[5] = {0.24f, 0.2f, 0.17f, 0.12f, 0.06f};
    for (int i = 0; i < 4; ++i) {
        limb(c, xs[i], ys[i], xs[i + 1], ys[i + 1], s * wd[i], tail, p.a);
        limb(c, xs[i], ys[i], xs[i + 1], ys[i + 1], s * wd[i] * 0.35f, belly, al(p.a, 200));
    }
    const uint32_t dark = darker(p.col, 0.45f);
    manTorso(c, p, y, p.col, dark);
    manHead(c, p, y, p.col);
    manRifle(c, p, y);
}

void nekroInf(Canvas& c, const Pose& p) {  // humanoid robots with one red eye
    const float s = p.s, x = p.x, y = p.y, t = secs(p.now) + p.phase;
    const uint32_t frame = rgb(112, 112, 120), body = pop(tint(rgb(52, 52, 58), p.col, 0.32f), p.style);
    c.fillCircle(x + s * 0.12f, y + s * 0.44f + p.lift, s * 0.26f, rgb(0, 0, 0), al(p.a, 70));
    const float st = std::sin(p.walk) * s * 0.1f;
    limb(c, x - s * 0.09f, y + s * 0.08f, x - s * 0.12f + st, y + s * 0.44f, s * 0.08f, frame, p.a);
    limb(c, x + s * 0.09f, y + s * 0.08f, x + s * 0.12f - st, y + s * 0.44f, s * 0.08f, frame, p.a);
    c.fillRect(int(x - s * 0.17f), int(y - s * 0.2f), int(s * 0.34f), int(s * 0.3f) + 1, body, p.a);
    c.fillRect(int(x - s * 0.12f), int(y - s * 0.14f), int(s * 0.24f), std::max(1, int(s * 0.05f)), frame, p.a);
    limb(c, x - s * 0.17f, y - s * 0.16f, x - s * 0.24f, y + s * 0.06f, s * 0.07f, frame, p.a);
    const float gy1 = p.dir > 0 ? y + s * 0.2f : y - s * 0.3f;
    limb(c, x + s * 0.17f, y - s * 0.12f, x + s * 0.24f, gy1, s * 0.09f, rgb(36, 36, 40), p.a);  // pulse gun
    const float hy = y - s * 0.36f;
    c.fillCircle(x, hy, s * 0.17f, rgb(22, 22, 26), p.a);
    c.ring(x, hy, s * 0.16f, std::max(0.8f, s * 0.04f), rgb(176, 176, 186), p.a);
    const float pulse = 0.6f + 0.4f * std::sin(t * 4.f);
    glow(c, x, hy, s * 0.16f, rgb(255, 30, 30), al(p.a, int(150 * pulse)), p.style);
    c.fillCircle(x, hy, s * 0.08f, rgb(220, 20, 24), p.a);
    c.fillCircle(x - s * 0.02f, hy - s * 0.02f, s * 0.03f, rgb(255, 190, 160), p.a);
    if (p.firing) c.fillCircle(x + s * 0.24f, gy1, s * 0.13f, rgb(255, 90, 80), p.a);
}

void sardakkInf(Canvas& c, const Pose& p) {  // a war bug seen from above, its head toward the enemy
    const float s = p.s * 1.1f, x = p.x, d = float(p.dir);
    const float y = p.y + s * 0.1f;  // (low to the ground)
    const uint32_t shell = pop(tint(rgb(74, 50, 40), p.col, 0.62f), p.style), stripe = darker(shell, 0.45f);
    const uint32_t leg = pop(tint(rgb(56, 44, 38), p.col, 0.2f), p.style), bone = pop(rgb(214, 196, 150), p.style);
    ell(c, x + s * 0.06f, y + s * 0.3f + p.lift, s * 0.38f, s * 0.13f, rgb(0, 0, 0), al(p.a, 70));
    const float sc = std::sin(p.walk * 1.3f) * s * 0.05f;
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; ++k) {  // spindly legs: up to the knee, down to the ground
            const float fy = (float(k) - 1.f) * s * 0.13f;
            const float hx = x + float(side) * s * 0.09f, hy = y + d * fy;
            const float kx = x + float(side) * s * 0.28f, ky = y + d * fy * 1.3f - s * 0.13f + (k % 2 ? sc : -sc);
            const float fx = x + float(side) * s * 0.38f, fyy = y + d * fy * 1.6f + s * 0.14f;
            limb(c, hx, hy, kx, ky, s * 0.06f, leg, p.a);
            limb(c, kx, ky, fx, fyy, s * 0.04f, leg, p.a);
        }
    // the tail-end abdomen, banded (the trooper bug's), then the thorax and a long head
    ell(c, x, y - d * s * 0.17f, s * 0.17f, s * 0.21f, shell, p.a);
    for (int k = 0; k < 3; ++k)
        limb(c, x - s * 0.14f, y - d * s * (0.08f + 0.08f * float(k)), x + s * 0.14f, y - d * s * (0.08f + 0.08f * float(k)), s * 0.035f, stripe, p.a);
    ell(c, x - s * 0.05f, y - d * s * 0.2f - s * 0.04f, s * 0.05f, s * 0.08f, lighter(shell, 0.3f), al(p.a, 170));
    ell(c, x, y + d * s * 0.05f, s * 0.13f, s * 0.11f, darker(shell, 0.1f), p.a);  // thorax
    const float hy = y + d * s * 0.21f;
    ell(c, x, hy, s * 0.09f, s * 0.1f, darker(shell, 0.25f), p.a);                  // head
    c.fillCircle(x - s * 0.045f, hy + d * s * 0.03f, std::max(0.8f, s * 0.025f), rgb(250, 210, 80), p.a);
    c.fillCircle(x + s * 0.045f, hy + d * s * 0.03f, std::max(0.8f, s * 0.025f), rgb(250, 210, 80), p.a);
    limb(c, x - s * 0.05f, hy + d * s * 0.05f, x - s * 0.015f, hy + d * s * 0.15f, s * 0.035f, bone, p.a);  // mandibles
    limb(c, x + s * 0.05f, hy + d * s * 0.05f, x + s * 0.015f, hy + d * s * 0.15f, s * 0.035f, bone, p.a);
    // the raised scything forelimbs (the hormagaunt's)
    const float up = std::sin(secs(p.now) * 3.f + p.phase) * s * 0.05f;
    limb(c, x - s * 0.1f, y + d * s * 0.07f, x - s * 0.24f, y - s * 0.3f + up, s * 0.055f, leg, p.a);
    limb(c, x - s * 0.24f, y - s * 0.3f + up, x - s * 0.15f, y + d * s * 0.2f - s * 0.18f, s * 0.04f, bone, p.a);
    limb(c, x + s * 0.1f, y + d * s * 0.07f, x + s * 0.24f, y - s * 0.3f - up, s * 0.055f, leg, p.a);
    limb(c, x + s * 0.24f, y - s * 0.3f - up, x + s * 0.15f, y + d * s * 0.2f - s * 0.18f, s * 0.04f, bone, p.a);
    if (p.firing) glow(c, x, hy + d * s * 0.14f, s * 0.12f, rgb(230, 230, 160), p.a, p.style);
}

void cabalInf(Canvas& c, const Pose& p) {  // eldritch horrors, three kinds mixed in the squad
    const float s = p.s, x = p.x, y = p.y, t = secs(p.now) + p.phase;
    const uint32_t flesh = pop(tint(rgb(124, 82, 64), p.col, 0.35f), p.style), dark = darker(flesh, 0.4f);
    const int kind = p.idx % 3;
    c.fillCircle(x + s * 0.1f, y + s * 0.44f + p.lift, s * 0.27f, rgb(0, 0, 0), al(p.a, 65));
    if (kind == 0) {  // tall and spindly, arms to the ground
        const float st = std::sin(p.walk) * s * 0.1f;
        limb(c, x - s * 0.05f, y + s * 0.02f, x - s * 0.15f + st, y + s * 0.44f, s * 0.07f, dark, p.a);
        limb(c, x + s * 0.05f, y + s * 0.02f, x + s * 0.15f - st, y + s * 0.44f, s * 0.07f, dark, p.a);
        quad(c, x - s * 0.08f, y - s * 0.34f, x + s * 0.08f, y - s * 0.34f, x + s * 0.05f, y + s * 0.04f, x - s * 0.05f, y + s * 0.04f, flesh, p.a);
        const float sw = std::sin(t * 2.f) * s * 0.06f;
        limb(c, x - s * 0.08f, y - s * 0.3f, x - s * 0.3f + sw, y + s * 0.1f, s * 0.05f, flesh, p.a);
        limb(c, x - s * 0.3f + sw, y + s * 0.1f, x - s * 0.36f + sw, y + s * 0.4f, s * 0.04f, dark, p.a);
        limb(c, x + s * 0.08f, y - s * 0.3f, x + s * 0.3f - sw, y + s * 0.1f, s * 0.05f, flesh, p.a);
        limb(c, x + s * 0.3f - sw, y + s * 0.1f, x + s * 0.36f - sw, y + s * 0.4f, s * 0.04f, dark, p.a);
        c.fillCircle(x, y - s * 0.44f, s * 0.1f, dark, p.a);  // a head without a face
        c.fillCircle(x, y - s * 0.42f, s * 0.04f, rgb(150, 230, 90), al(p.a, 200));
    } else if (kind == 1) {  // a blob on tentacles, eyes all over
        for (int k = 0; k < 4; ++k) {
            const float bx = x + (float(k) - 1.5f) * s * 0.12f, w = std::sin(t * 2.5f + float(k)) * s * 0.07f;
            limb(c, bx, y + s * 0.08f, bx + w, y + s * 0.3f, s * 0.07f, dark, p.a);
            limb(c, bx + w, y + s * 0.3f, bx - w * 0.5f + float(k % 2 ? 1 : -1) * s * 0.08f, y + s * 0.44f, s * 0.05f, dark, p.a);
        }
        ell(c, x, y - s * 0.1f, s * 0.24f, s * 0.22f, flesh, p.a);
        ell(c, x - s * 0.05f, y - s * 0.18f, s * 0.1f, s * 0.07f, lighter(flesh, 0.2f), al(p.a, 180));
        static const float ex[5] = {-0.12f, 0.06f, 0.13f, -0.03f, 0.1f}, ey[5] = {-0.16f, -0.22f, -0.08f, -0.03f, 0.02f};
        for (int k = 0; k < 5; ++k) c.fillCircle(x + ex[k] * s, y + ey[k] * s, s * 0.035f, rgb(230, 220, 90), p.a);
        ell(c, x, y + p.dir * s * 0.04f, s * 0.08f, s * 0.04f, rgb(60, 14, 20), p.a);
    } else {  // a low crawler with a glowing maw
        const float d = float(p.dir);
        for (int k = 0; k < 4; ++k)
            for (int side = -1; side <= 1; side += 2) {
                const float ly = y + s * 0.18f + (float(k) - 1.5f) * s * 0.07f * d;
                const float w = std::sin(p.walk + float(k)) * s * 0.04f;
                limb(c, x + float(side) * s * 0.12f, ly, x + float(side) * s * 0.32f, ly - s * 0.1f + w, s * 0.05f, dark, p.a);
                limb(c, x + float(side) * s * 0.32f, ly - s * 0.1f + w, x + float(side) * s * 0.38f, ly + s * 0.16f, s * 0.04f, dark, p.a);
            }
        ell(c, x, y + s * 0.18f, s * 0.18f, s * 0.15f, flesh, p.a);
        const float my = y + s * 0.18f + d * s * 0.1f;
        glow(c, x, my, s * 0.12f, rgb(110, 230, 80), al(p.a, 170), p.style);
        ell(c, x, my, s * 0.07f, s * 0.05f, rgb(40, 10, 16), p.a);
    }
}

void empyreanInf(Canvas& c, const Pose& p) {  // the default soldier with bat wings
    const float s = p.s, x = p.x, y = p.y;
    const float f = std::sin(secs(p.now) * 6.f + p.phase) * s * 0.08f;
    const uint32_t mem = pop(tint(rgb(74, 42, 92), p.col, 0.35f), p.style), bone = darker(mem, 0.4f);
    for (int side = -1; side <= 1; side += 2) {
        const float sd = float(side);
        float xs[6] = {x + sd * s * 0.12f, x + sd * s * 0.44f, x + sd * s * 0.56f, x + sd * s * 0.44f, x + sd * s * 0.32f, x + sd * s * 0.2f};
        float ys[6] = {y - s * 0.18f, y - s * 0.46f + f, y - s * 0.12f + f * 0.5f, y - s * 0.04f, y - s * 0.08f, y + s * 0.0f};
        c.fillPolygon(xs, ys, 6, mem, al(p.a, 235));
        limb(c, xs[0], ys[0], xs[1], ys[1], s * 0.04f, bone, p.a);
        limb(c, xs[0], ys[0], xs[3], ys[3], s * 0.03f, bone, p.a);
    }
    defaultMan(c, p);
}

void axisInf(Canvas& c, const Pose& p) {  // the default soldier with an ant's body behind him
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t chit = pop(tint(rgb(64, 42, 32), p.col, 0.4f), p.style);
    for (int side = -1; side <= 1; side += 2)  // two more legs
        limb(c, x + float(side) * s * 0.08f, y + s * 0.12f, x + float(side) * s * 0.32f, y + s * 0.42f, s * 0.05f, darker(chit, 0.3f), p.a);
    limb(c, x - s * 0.1f, y + s * 0.08f, x - s * 0.24f, y + s * 0.14f, s * 0.06f, darker(chit, 0.2f), p.a);  // the waist
    ell(c, x - s * 0.36f, y + s * 0.2f, s * 0.19f, s * 0.14f, chit, p.a);                                    // the ant's body behind him
    limb(c, x - s * 0.5f, y + s * 0.16f, x - s * 0.22f, y + s * 0.16f, s * 0.035f, lighter(chit, 0.25f), p.a);
    limb(c, x - s * 0.48f, y + s * 0.25f, x - s * 0.24f, y + s * 0.25f, s * 0.035f, lighter(chit, 0.25f), p.a);
    c.fillCircle(x - s * 0.42f, y + s * 0.13f, s * 0.05f, lighter(chit, 0.35f), al(p.a, 200));
    defaultMan(c, p);
}

void mykoInf(Canvas& c, const Pose& p) {  // the default soldier under a grey mushroom cap
    defaultMan(c, p);
    const float s = p.s, x = p.x, y = p.y - s * 0.42f;
    ell(c, x, y + s * 0.05f, s * 0.29f, s * 0.05f, rgb(112, 100, 94), p.a);
    dome(c, x, y + s * 0.04f, s * 0.3f, s * 0.17f, pop(rgb(172, 166, 160), p.style), p.a);
    c.fillCircle(x - s * 0.1f, y - s * 0.05f, s * 0.04f, rgb(230, 226, 220), p.a);
    c.fillCircle(x + s * 0.08f, y - s * 0.09f, s * 0.035f, rgb(230, 226, 220), p.a);
    c.fillCircle(x + s * 0.16f, y - s * 0.02f, s * 0.03f, rgb(230, 226, 220), p.a);
}

void droid(Canvas& c, const Pose& p) {  // Roh'Dhna battle droid: thin limbs, a long narrow head
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t tan = pop(tint(rgb(194, 172, 130), p.col, 0.35f), p.style), dk = darker(tan, 0.35f);
    c.fillCircle(x + s * 0.1f, y + s * 0.44f + p.lift, s * 0.22f, rgb(0, 0, 0), al(p.a, 60));
    const float st = std::sin(p.walk) * s * 0.1f;
    limb(c, x - s * 0.06f, y + s * 0.06f, x - s * 0.1f + st, y + s * 0.44f, s * 0.05f, tan, p.a);
    limb(c, x + s * 0.06f, y + s * 0.06f, x + s * 0.1f - st, y + s * 0.44f, s * 0.05f, tan, p.a);
    quad(c, x - s * 0.13f, y - s * 0.2f, x + s * 0.13f, y - s * 0.2f, x + s * 0.07f, y + s * 0.08f, x - s * 0.07f, y + s * 0.08f, tan, p.a);
    c.fillRect(int(x - s * 0.08f), int(y - s * 0.14f), int(s * 0.16f), int(s * 0.1f) + 1, dk, p.a);
    limb(c, x - s * 0.13f, y - s * 0.17f, x - s * 0.18f, y + s * 0.06f, s * 0.04f, tan, p.a);
    const float gy1 = p.dir > 0 ? y + s * 0.26f : y - s * 0.46f;
    limb(c, x + s * 0.13f, y - s * 0.12f, x + s * 0.22f, gy1, s * 0.07f, rgb(50, 50, 54), p.a);
    limb(c, x, y - s * 0.2f, x, y - s * 0.28f, s * 0.04f, tan, p.a);
    quad(c, x - s * 0.05f, y - s * 0.46f, x + s * 0.05f, y - s * 0.46f, x + s * 0.035f, y - s * 0.26f, x - s * 0.035f, y - s * 0.26f, tan, p.a);
    c.fillCircle(x, y - s * 0.42f, s * 0.03f, dk, p.a);
    if (p.firing) c.fillCircle(x + s * 0.22f, gy1, s * 0.12f, rgb(255, 120, 110), p.a);
}

void florzenInf(Canvas& c, const Pose& p) {  // floating jellyfish
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = s * (0.18f + 0.04f * std::sin(t * 2.f));
    const float gy = p.y + p.lift + s * 0.44f, by = p.y - s * 0.3f - h;
    shadowAt(c, x, gy, s * 0.24f, s * 0.08f, h, s, p.a);
    const uint32_t bell = pop(tint(rgb(120, 168, 255), p.col, 0.35f), p.style), ten = pop(tint(rgb(160, 110, 220), p.col, 0.3f), p.style);
    for (int k = 0; k < 5; ++k) {  // the tentacles, drifting
        const float tx = x + (float(k) - 2.f) * s * 0.08f;
        const float w1 = std::sin(t * 2.4f + float(k) * 1.3f) * s * 0.06f, w2 = std::sin(t * 2.4f + float(k) * 1.3f + 1.2f) * s * 0.08f;
        limb(c, tx, by, tx + w1, by + s * 0.24f, s * 0.04f, ten, al(p.a, 200));
        limb(c, tx + w1, by + s * 0.24f, tx + w2, by + s * 0.46f, s * 0.03f, ten, al(p.a, 160));
    }
    glow(c, x, by - s * 0.06f, s * 0.3f, rgb(150, 130, 255), al(p.a, 110), p.style);
    dome(c, x, by, s * 0.23f, s * 0.2f, bell, al(p.a, 190));
    ell(c, x, by, s * 0.23f, s * 0.04f, lighter(bell, 0.3f), al(p.a, 200));
    c.fillCircle(x, by - s * 0.08f, s * 0.07f, p.firing ? rgb(255, 250, 220) : rgb(255, 176, 96), p.a);
    c.fillCircle(x - s * 0.08f, by - s * 0.13f, s * 0.04f, lighter(bell, 0.6f), al(p.a, 200));
}

void khraskInf(Canvas& c, const Pose& p) {  // rock golems with grass growing on their backs
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t stone = pop(tint(rgb(98, 92, 84), p.col, 0.3f), p.style), dk = darker(stone, 0.35f);
    c.fillCircle(x + s * 0.1f, y + s * 0.44f + p.lift, s * 0.3f, rgb(0, 0, 0), al(p.a, 70));
    const float st = std::sin(p.walk) * s * 0.06f;
    c.fillRect(int(x - s * 0.18f + st), int(y + s * 0.12f), int(s * 0.13f) + 1, int(s * 0.32f), dk, p.a);  // stumpy legs
    c.fillRect(int(x + s * 0.05f - st), int(y + s * 0.12f), int(s * 0.13f) + 1, int(s * 0.32f), dk, p.a);
    float xs[6] = {x - s * 0.28f, x - s * 0.18f, x + s * 0.18f, x + s * 0.28f, x + s * 0.18f, x - s * 0.18f};
    float ys[6] = {y - s * 0.12f, y - s * 0.3f, y - s * 0.3f, y - s * 0.12f, y + s * 0.16f, y + s * 0.16f};
    c.fillPolygon(xs, ys, 6, stone, p.a);
    limb(c, x - s * 0.1f, y - s * 0.22f, x - s * 0.02f, y - s * 0.04f, s * 0.03f, dk, p.a);  // cracks
    limb(c, x + s * 0.12f, y - s * 0.1f, x + s * 0.04f, y + s * 0.06f, s * 0.03f, dk, p.a);
    limb(c, x - s * 0.26f, y - s * 0.12f, x - s * 0.32f, y + s * 0.16f, s * 0.1f, stone, p.a);  // arms
    limb(c, x + s * 0.26f, y - s * 0.12f, x + s * 0.32f, y + s * 0.16f, s * 0.1f, stone, p.a);
    c.fillCircle(x, y - s * 0.38f, s * 0.1f, stone, p.a);
    c.fillCircle(x - s * 0.04f, y - s * 0.38f, s * 0.02f, rgb(150, 255, 170), p.a);
    c.fillCircle(x + s * 0.04f, y - s * 0.38f, s * 0.02f, rgb(150, 255, 170), p.a);
    const uint32_t grass = pop(rgb(96, 160, 60), p.style);
    for (int k = 0; k < 6; ++k) {  // grass on the shoulders
        const float gx = x + (float(k) - 2.5f) * s * 0.09f, sw = std::sin(secs(p.now) * 3.f + float(k)) * s * 0.03f;
        limb(c, gx, y - s * 0.28f, gx + sw + (k % 2 ? 0.03f : -0.03f) * s, y - s * 0.4f, s * 0.035f, grass, p.a);
    }
    if (p.firing) glow(c, x + s * 0.32f, y + s * 0.16f, s * 0.14f, rgb(160, 255, 180), p.a, p.style);
}

// a nebula: overlapping clouds of colour turning round a bright heart
void nebula(Canvas& c, float x, float y, float r, uint32_t team, int a, uint32_t now, float phase, int style, bool heart = true) {
    static const uint32_t pal[5] = {rgb(92, 82, 255), rgb(204, 72, 222), rgb(255, 140, 92), rgb(70, 200, 255), rgb(240, 90, 150)};
    const float t = secs(now) * 0.6f + phase;
    if (style == 2) c.fillCircle(x, y, r * 1.5f, rgb(120, 80, 220), al(a, 26));
    for (int k = 0; k < 5; ++k) {
        const float ang = t + float(k) * 2.f * kPi / 5.f, rr = r * (0.36f + 0.1f * std::sin(t * 1.7f + float(k)));
        const uint32_t col = pop(tint(pal[(k + int(phase * 3.f)) % 5], team, 0.28f), style);
        c.fillCircle(x + std::cos(ang) * rr, y + std::sin(ang) * rr * 0.75f, r * (0.55f + 0.08f * float(k % 2)), col, al(a, style == 1 ? 120 : 70));
    }
    if (heart) {
        glow(c, x, y, r * 0.5f, rgb(255, 250, 230), al(a, 190), style);
        c.fillCircle(x, y, std::max(1.f, r * 0.14f), rgb(255, 255, 255), a);
    }
}

void mortheusInf(Canvas& c, const Pose& p) {
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = s * (0.12f + 0.04f * std::sin(t * 1.5f));
    shadowAt(c, x, p.y + p.lift + s * 0.44f, s * 0.3f, s * 0.09f, h, s, p.a);
    nebula(c, x, p.y - s * 0.2f - h, s * 0.42f, p.col, p.a, p.now, p.phase, p.style);
    if (p.firing) glow(c, x, p.y - s * 0.2f - h + p.dir * s * 0.2f, s * 0.2f, rgb(255, 250, 200), p.a, p.style);
}

void rock(Canvas& c, float x, float y, float r, uint32_t team, int a, float phase, int style, uint32_t now, float glowK) {
    const uint32_t stone = pop(tint(rgb(72, 64, 62), team, 0.32f), style);
    c.fillCircle(x, y, r, stone, a);
    c.fillCircle(x - r * 0.3f, y - r * 0.32f, r * 0.42f, lighter(stone, 0.18f), al(a, 200));
    c.fillCircle(x + r * 0.35f, y + r * 0.2f, r * 0.22f, darker(stone, 0.35f), a);  // craters
    c.fillCircle(x - r * 0.25f, y + r * 0.42f, r * 0.15f, darker(stone, 0.35f), a);
    c.fillCircle(x + r * 0.05f, y - r * 0.5f, r * 0.12f, darker(stone, 0.3f), a);
    const float pulse = 0.6f + 0.4f * std::sin(secs(now) * 2.5f + phase);
    const uint32_t lava = rgb(255, 76, 44);
    const int la = al(a, int(230.f * pulse * glowK));
    if (la > 4) {  // glowing cracks
        const float w = std::max(0.8f, r * 0.08f);
        limb(c, x - r * 0.6f, y - r * 0.05f, x - r * 0.1f, y + r * 0.12f, w, lava, la);
        limb(c, x - r * 0.1f, y + r * 0.12f, x + r * 0.2f, y - r * 0.25f, w, lava, la);
        limb(c, x + r * 0.2f, y - r * 0.25f, x + r * 0.62f, y - r * 0.1f, w, lava, la);
        limb(c, x - r * 0.1f, y + r * 0.12f, x - r * 0.02f, y + r * 0.6f, w, lava, la);
        glow(c, x - r * 0.1f, y + r * 0.12f, r * 0.35f, lava, la / 2, style);
    }
}

void zelianInf(Canvas& c, const Pose& p) {  // floating rocks
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = s * (0.2f + 0.05f * std::sin(t * 1.8f));
    shadowAt(c, x, p.y + p.lift + s * 0.44f, s * 0.24f, s * 0.08f, h, s, p.a);
    rock(c, x, p.y - s * 0.12f - h, s * 0.27f, p.col, p.a, p.phase, p.style, p.now, p.firing ? 1.4f : 1.f);
}

void cheiranInf(Canvas& c, const Pose& p) {  // crabs seen from above, claws toward the enemy
    const float s = p.s, x = p.x, d = float(p.dir), y = p.y + s * 0.14f;
    const uint32_t shell = pop(tint(rgb(70, 92, 112), p.col, 0.4f), p.style), dk = darker(shell, 0.4f);
    ell(c, x + s * 0.06f, y + s * 0.28f + p.lift, s * 0.38f, s * 0.12f, rgb(0, 0, 0), al(p.a, 60));
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; ++k) {
            const float ly = y - d * (float(k) - 0.5f) * s * 0.09f, w = std::sin(p.walk + float(k) * 1.7f) * s * 0.04f;
            limb(c, x + float(side) * s * 0.2f, ly, x + float(side) * s * 0.36f, ly - s * 0.1f + w, s * 0.05f, dk, p.a);
            limb(c, x + float(side) * s * 0.36f, ly - s * 0.1f + w, x + float(side) * s * 0.44f, ly + s * 0.12f, s * 0.04f, dk, p.a);
        }
    for (int side = -1; side <= 1; side += 2) {  // the claws
        const float sd = float(side), cx = x + sd * s * 0.22f, cy = y + d * s * 0.3f;
        limb(c, x + sd * s * 0.14f, y + d * s * 0.08f, cx, cy, s * 0.07f, shell, p.a);
        tri(c, cx - s * 0.07f, cy, cx + s * 0.07f, cy, cx + sd * s * 0.02f, cy + d * s * 0.16f, lighter(shell, 0.15f), p.a);
        tri(c, cx - s * 0.02f, cy, cx + sd * s * 0.1f, cy + d * s * 0.02f, cx + sd * s * 0.08f, cy + d * s * 0.13f, dk, p.a);
    }
    ell(c, x, y, s * 0.24f, s * 0.17f, shell, p.a);
    ell(c, x - s * 0.06f, y - s * 0.05f, s * 0.1f, s * 0.06f, lighter(shell, 0.2f), al(p.a, 200));
    limb(c, x - s * 0.18f, y + d * s * 0.04f, x + s * 0.18f, y + d * s * 0.04f, s * 0.03f, dk, p.a);
    const float ey = y + d * s * 0.12f;
    c.fillCircle(x - s * 0.07f, ey, s * 0.035f, rgb(190, 110, 255), p.a);
    c.fillCircle(x + s * 0.07f, ey, s * 0.035f, rgb(190, 110, 255), p.a);
    for (int k = -1; k <= 1; ++k)
        limb(c, x + float(k) * s * 0.04f, ey + d * s * 0.03f, x + float(k) * s * 0.06f, ey + d * s * 0.11f, s * 0.03f, rgb(150, 110, 120), p.a);
    if (p.firing) glow(c, x, ey + d * s * 0.12f, s * 0.13f, rgb(170, 240, 90), p.a, p.style);
}

// a fish seen from above, its head toward the enemy (dir); r = half its length
void fish(Canvas& c, float x, float y, float r, int dir, uint32_t body, int a, float t, bool open, bool angler, int style) {
    const float d = float(dir);
    const float wag = std::sin(t * 5.f) * r * 0.18f;
    // tail fin
    tri(c, x + wag * 0.3f, y - d * r * 0.8f, x - r * 0.35f + wag, y - d * r * 1.15f, x + r * 0.35f + wag, y - d * r * 1.15f, darker(body, 0.2f), a);
    // body: a teardrop, wide at the head
    float xs[9], ys[9];
    const float hw = angler ? r * 0.62f : r * 0.42f;
    xs[0] = x;               ys[0] = y + d * r * 0.95f;
    xs[1] = x + hw * 0.8f;   ys[1] = y + d * r * 0.62f;
    xs[2] = x + hw;          ys[2] = y + d * r * 0.15f;
    xs[3] = x + hw * 0.6f;   ys[3] = y - d * r * 0.45f;
    xs[4] = x + wag * 0.2f + r * 0.08f; ys[4] = y - d * r * 0.85f;
    xs[5] = x + wag * 0.2f - r * 0.08f; ys[5] = y - d * r * 0.85f;
    xs[6] = x - hw * 0.6f;   ys[6] = y - d * r * 0.45f;
    xs[7] = x - hw;          ys[7] = y + d * r * 0.15f;
    xs[8] = x - hw * 0.8f;   ys[8] = y + d * r * 0.62f;
    c.fillPolygon(xs, ys, 9, body, a);
    ell(c, x, y + d * r * 0.05f, hw * 0.35f, r * 0.45f, lighter(body, 0.18f), al(a, 170));
    for (int k = 0; k < 3; ++k)  // spines along the back
        limb(c, x, y - d * r * (0.1f + 0.22f * float(k)), x + (k % 2 ? 1.f : -1.f) * r * 0.16f, y - d * r * (0.22f + 0.22f * float(k)),
             std::max(0.8f, r * 0.05f), darker(body, 0.3f), a);
    for (int side = -1; side <= 1; side += 2)  // side fins
        tri(c, x + float(side) * hw * 0.85f, y + d * r * 0.1f, x + float(side) * hw * 1.5f, y - d * r * 0.1f + wag * 0.3f, x + float(side) * hw * 0.8f,
            y - d * r * 0.25f, darker(body, 0.15f), a);
    // the mouth: open wide, fangs
    const float my = y + d * r * 0.78f, mw = hw * (open ? 0.75f : 0.55f);
    ell(c, x, my, mw, r * (open ? 0.2f : 0.1f), rgb(40, 10, 14), a);
    for (int k = -2; k <= 2; ++k)
        tri(c, x + float(k) * mw * 0.35f - r * 0.03f, my - d * r * 0.08f, x + float(k) * mw * 0.35f + r * 0.03f, my - d * r * 0.08f,
            x + float(k) * mw * 0.35f, my + d * r * 0.08f, rgb(236, 232, 210), a);
    if (open) glow(c, x, my, mw * 0.8f, rgb(255, 170, 80), al(a, 200), style);
    c.fillCircle(x - hw * 0.55f, y + d * r * 0.45f, std::max(1.f, r * 0.07f), rgb(255, 220, 90), a);
    c.fillCircle(x + hw * 0.55f, y + d * r * 0.45f, std::max(1.f, r * 0.07f), rgb(255, 220, 90), a);
    if (angler) {  // the lure: a stalk curving forward, a glowing bulb
        const float lx = x + r * 0.15f, ly = y + d * r * 1.35f;
        limb(c, x, y + d * r * 0.4f, x + r * 0.3f, y + d * r * 0.95f, std::max(1.f, r * 0.05f), darker(body, 0.2f), a);
        limb(c, x + r * 0.3f, y + d * r * 0.95f, lx, ly, std::max(1.f, r * 0.04f), darker(body, 0.2f), a);
        const float pl = 0.6f + 0.4f * std::sin(t * 3.f);
        glow(c, lx, ly, r * 0.3f, rgb(200, 255, 140), al(a, int(200.f * pl)), style);
        c.fillCircle(lx, ly, std::max(1.f, r * 0.08f), rgb(240, 255, 210), a);
    }
}

void ghotiInf(Canvas& c, const Pose& p) {  // big fish monsters, swimming in the air
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = s * (0.1f + 0.04f * std::sin(t * 2.2f));
    ell(c, x + s * 0.06f, p.y + s * 0.4f + p.lift, s * 0.24f, s * 0.12f, rgb(0, 0, 0), al(p.a, 55));
    fish(c, x, p.y + s * 0.02f - h, s * 0.36f, p.dir, pop(tint(rgb(44, 112, 124), p.col, 0.38f), p.style), p.a, t, p.firing, false, p.style);
}

// ------------------------------------------------------------------ the fallen
void fallenMan(Canvas& c, const Pose& p, float pool, uint32_t blood, bool bright) {
    const float s = p.s, x = p.x, y = p.y, k = std::min(1.f, pool), seed = p.phase + float(p.idx);
    c.fillCircle(x + s * 0.1f, y + s * 0.34f, s * (0.12f + 0.2f * k), blood, al(p.a, 170));
    c.fillCircle(x + s * (0.1f + 0.28f * std::sin(seed)), y + s * (0.36f + 0.1f * std::cos(seed)), s * (0.05f + 0.1f * k), lighter(blood, 0.08f), al(p.a, 150));
    c.fillCircle(x - s * (0.2f + 0.1f * std::cos(seed * 1.7f)), y + s * 0.4f, s * 0.06f * k, darker(blood, 0.2f), al(p.a, 160));
    if (bright) glow(c, x + s * 0.1f, y + s * 0.34f, s * (0.2f + 0.2f * k), blood, al(p.a, 90), p.style);
    c.fillRect(int(x - s * 0.3f), int(y + s * 0.18f), int(s * 0.55f), std::max(2, int(s * 0.22f)), darker(p.col, 0.35f), p.a);
    c.fillCircle(x + s * 0.32f, y + s * 0.28f, s * 0.15f, darker(p.col, 0.2f), p.a);
}
void pool(Canvas& c, const Pose& p, float k, uint32_t blood, float r = 1.f) {
    const float s = p.s * r;
    c.fillCircle(p.x + s * 0.06f, p.y + p.s * 0.34f, s * (0.12f + 0.22f * k), blood, al(p.a, 170));
    c.fillCircle(p.x - s * 0.16f, p.y + p.s * 0.4f, s * (0.04f + 0.08f * k), darker(blood, 0.15f), al(p.a, 150));
}

}  // namespace

// ====================================================================== the look of each faction
Look lookOf(const char* key) {
    if (!key) return Default;
    static const struct {
        const char* key;
        Look look;
    } kMap[] = {{"arborec", Arborec},   {"ghost", Creuss},     {"muaat", Muaat},     {"naalu", Naalu},     {"nekro", Nekro},
                {"sardakk", Sardakk},   {"cabal", Cabal},      {"empyrean", Empyrean}, {"mahact", Mahact}, {"crimson", Mahact},
                {"titans", Titans},     {"axis", Axis},        {"florzen", Florzen}, {"khrask", Khrask},   {"mortheus", Mortheus},
                {"mykomentori", Myko},  {"rohdhna", Rohdhna},  {"zelian", Zelian},   {"cheiran", Cheiran}, {"ghoti", Ghoti},
                {"kyro", Kyro}};
    for (const auto& m : kMap)
        if (!std::strcmp(m.key, key)) return m.look;
    return Default;
}

const char* lookName(Look l) {
    static const char* kNames[kLooks] = {"Default", "Arborec", "Creuss", "Muaat", "Naalu", "Nekro", "Sardakk N'orr", "Vuil'raith",
                                         "Empyrean", "Mahact / Crimson", "Titans of Ul", "Shipwrights of Axis", "Florzen",
                                         "L'tokk Khrask", "Glimmer of Mortheus", "Myko-Mentori", "Roh'Dhna", "Zelian Purifier",
                                         "Cheiran Hordes", "Ghoti Wayfarers", "Kyro Sodality"};
    return l < kLooks ? kNames[l] : "?";
}

void setFactionUnits(bool on) { g_factionUnits = on; }
bool factionUnits() { return g_factionUnits; }

bool phases(Look l) { return l == Creuss || l == Mahact; }
int phaseAlpha(Look l, uint32_t now, float phase) {
    if (!phases(l)) return 255;
    // (owner: "units phase in and out every 1 second") - a dip of 0.38 s once a second, each unit at its own moment
    const uint32_t t = (now + uint32_t(phase * 159.f)) % 1000u;
    if (t >= 380) return 255;
    return 255 - int(190.f * std::sin(kPi * float(t) / 380.f));
}
bool onePerPiece(Look l) { return l == Mortheus; }
float infantryScale(Look l) {
    switch (l) {
        case Mortheus: return 1.7f;
        case Ghoti: return 1.15f;
        case Khrask: return 1.1f;
        case Cheiran: return 1.05f;
        default: return 1.f;
    }
}
float hover(Look l, int) {
    switch (l) {
        case Florzen: return 0.18f;
        case Zelian: return 0.2f;
        case Mortheus: return 0.12f;
        case Ghoti: return 0.1f;
        default: return 0.f;
    }
}

Weapon infantryWeapon(Look l, int idx) {
    switch (l) {
        case Arborec: return WSpore;
        case Muaat: return WOrangePlasma;
        case Nekro: return WRedPulse;
        case Sardakk: return WSpine;
        case Cabal: return idx % 2 ? WSlimeShot : WGreenFlame;
        case Titans: return WPurpleLaser;
        case Florzen: return WLightning;
        case Khrask: return WCrystal;
        case Mortheus: return WStar;
        case Zelian: return WRock;
        case Cheiran: return WSlimeStream;
        case Ghoti: return WGas;
        case Rohdhna: return idx % 2 ? WDroidLaser : WDefault;
        default: return WDefault;
    }
}
Weapon mechWeapon(Look l) {
    switch (l) {
        case Arborec: return WSeed;
        case Muaat: return WFlame2;
        case Nekro: return WRedGatling;
        case Sardakk: return WBlueBall;
        case Titans: return WPurpleBeam;
        case Florzen: return WHyperBeam;
        case Khrask: return WBigCrystal;
        case Mortheus: return WDarkStar;
        case Zelian: return WBigRock;
        case Cheiran: return WBigSlime;
        case Ghoti: return WBigGas;
        default: return WDefault;
    }
}

void muzzle(Look l, bool isMech, int dir, int idx, float& dx, float& dy) {
    const float d = float(dir);
    if (isMech) {
        switch (l) {
            case Arborec: dx = 0.f, dy = -0.64f + d * 0.06f; return;
            case Muaat: dx = 0.26f, dy = -0.44f; return;
            case Nekro: dx = 0.f, dy = -0.62f + d * 0.05f; return;
            case Sardakk: dx = 0.f, dy = -0.25f + d * 0.3f; return;
            case Titans: dx = 0.3f, dy = -0.5f; return;
            case Florzen: dx = 0.f, dy = -0.7f; return;
            case Khrask: dx = 0.28f, dy = -0.55f; return;
            case Mortheus: dx = 0.f, dy = -0.55f; return;
            case Zelian: dx = 0.f, dy = -0.6f; return;
            case Cheiran: dx = 0.23f, dy = -0.3f + d * 0.31f; return;
            case Ghoti: dx = 0.f, dy = -0.35f + d * 0.35f; return;
            case Rohdhna: dx = 0.28f, dy = -0.5f; return;
            default: dx = 0.17f, dy = -0.35f; return;
        }
    }
    switch (l) {
        case Arborec: dx = 0.f, dy = -0.34f + d * 0.12f; return;
        case Muaat: dx = 0.f, dy = -0.12f + d * 0.1f; return;
        case Nekro: dx = 0.24f, dy = d > 0 ? 0.2f : -0.3f; return;
        case Sardakk: dx = 0.f, dy = 0.12f + d * 0.3f; return;
        case Cabal: dx = 0.f, dy = idx % 3 == 2 ? 0.18f + d * 0.12f : -0.25f; return;
        case Florzen: dx = 0.f, dy = -0.66f; return;
        case Khrask: dx = 0.32f, dy = 0.14f; return;
        case Mortheus: dx = 0.f, dy = -0.36f; return;
        case Zelian: dx = 0.f, dy = -0.44f; return;
        case Cheiran: dx = 0.f, dy = 0.26f + d * 0.22f; return;
        case Ghoti: dx = 0.f, dy = -0.08f + d * 0.3f; return;
        default: dx = 0.26f, dy = d > 0 ? 0.34f : -0.58f; return;
    }
}

uint32_t bloodOf(Look l) {
    switch (l) {
        case Creuss: return rgb(40, 190, 255);
        case Mahact: return rgb(255, 44, 44);
        case Arborec: return rgb(92, 170, 52);
        case Muaat: return rgb(255, 128, 30);
        case Nekro: return rgb(22, 22, 26);
        case Sardakk: return rgb(170, 190, 60);
        case Cabal: return rgb(84, 168, 62);
        case Florzen: return rgb(150, 92, 222);
        case Cheiran: return rgb(124, 200, 80);
        case Ghoti: return rgb(40, 118, 128);
        case Zelian: return rgb(255, 80, 40);
        case Khrask: return rgb(72, 68, 62);
        default: return rgb(110, 6, 8);
    }
}

void glow(Canvas& c, float x, float y, float r, uint32_t col, int a, int style) {
    if (a <= 0 || r <= 0.3f) return;
    if (style == 1) {  // Retrowave: hard blocks of light, no soft edge (the chunky pass pixelates them)
        c.fillCircle(x, y, r * 0.7f, col, al(a, 150));
        c.fillCircle(x, y, r * 0.35f, lighter(col, 0.5f), al(a, 220));
    } else if (style == 2) {  // Voidfall: a painted bloom, wider and softer
        c.fillCircle(x, y, r * 1.7f, col, al(a, 34));
        c.fillCircle(x, y, r, col, al(a, 80));
        c.fillCircle(x, y, r * 0.45f, lighter(col, 0.45f), al(a, 170));
    } else {
        c.fillCircle(x, y, r, col, al(a, 70));
        c.fillCircle(x, y, r * 0.5f, lighter(col, 0.35f), al(a, 150));
    }
}

void infantry(Canvas& c, Look l, const Pose& p) {
    switch (l) {
        case Arborec: arborecInf(c, p); return;
        case Creuss:
        case Mahact:
            defaultMan(c, p);
            seams(c, p, l == Creuss ? rgb(70, 200, 255) : rgb(255, 66, 56));
            return;
        case Muaat: muaatInf(c, p); return;
        case Naalu: naaluInf(c, p); return;
        case Nekro: nekroInf(c, p); return;
        case Sardakk: sardakkInf(c, p); return;
        case Cabal: cabalInf(c, p); return;
        case Empyrean: empyreanInf(c, p); return;
        case Axis: axisInf(c, p); return;
        case Florzen: florzenInf(c, p); return;
        case Khrask: khraskInf(c, p); return;
        case Mortheus: mortheusInf(c, p); return;
        case Myko: mykoInf(c, p); return;
        case Rohdhna:
            if (p.idx % 2) droid(c, p);
            else defaultMan(c, p);
            return;
        case Zelian: zelianInf(c, p); return;
        case Cheiran: cheiranInf(c, p); return;
        case Ghoti: ghotiInf(c, p); return;
        default: defaultMan(c, p); return;  // Default, Titans, Kyro
    }
}

void fallen(Canvas& c, Look l, const Pose& p, float poolK) {
    const float s = p.s, x = p.x, y = p.y, k = std::min(1.f, poolK);
    switch (l) {
        case Mortheus: {  // the nebula comes apart and is gone
            const float f = std::min(1.f, poolK / 1.7f);
            if (f >= 1.f) return;
            nebula(c, x, y - s * 0.2f, s * 0.42f * (1.f + 1.2f * f), p.col, int(float(p.a) * (1.f - f)), p.now, p.phase, p.style, f < 0.3f);
            return;
        }
        case Creuss:
        case Mahact: fallenMan(c, p, poolK, bloodOf(l), true); return;
        case Arborec: {  // a wilted plant in its sap
            pool(c, p, k, bloodOf(l));
            const uint32_t stem = darker(tint(rgb(64, 122, 46), p.col, 0.28f), 0.3f);
            limb(c, x - s * 0.28f, y + s * 0.3f, x + s * 0.18f, y + s * 0.36f, s * 0.12f, stem, p.a);
            c.fillCircle(x + s * 0.26f, y + s * 0.34f, s * 0.14f, darker(tint(rgb(150, 172, 74), p.col, 0.42f), 0.35f), p.a);
            return;
        }
        case Muaat: {  // a cracked egg leaking fire
            pool(c, p, k, bloodOf(l));
            if (k < 1.f) glow(c, x, y + s * 0.32f, s * 0.3f, rgb(255, 120, 30), al(p.a, int(180.f * (1.f - k * 0.7f))), p.style);
            const uint32_t egg = darker(tint(rgb(128, 88, 54), p.col, 0.3f), 0.35f);
            ell(c, x - s * 0.06f, y + s * 0.3f, s * 0.24f, s * 0.14f, egg, p.a);
            tri(c, x + s * 0.1f, y + s * 0.2f, x + s * 0.28f, y + s * 0.26f, x + s * 0.16f, y + s * 0.38f, darker(egg, 0.3f), p.a);
            limb(c, x - s * 0.3f, y + s * 0.3f, x - s * 0.42f, y + s * 0.2f, s * 0.05f, rgb(60, 52, 48), p.a);
            limb(c, x + s * 0.18f, y + s * 0.36f, x + s * 0.34f, y + s * 0.44f, s * 0.05f, rgb(60, 52, 48), p.a);
            return;
        }
        case Nekro:
        case Rohdhna: {
            if (l == Rohdhna && p.idx % 2 == 0) break;
            pool(c, p, k, rgb(24, 22, 24), 0.9f);  // oil
            const uint32_t metal = l == Nekro ? rgb(92, 92, 100) : darker(tint(rgb(194, 172, 130), p.col, 0.35f), 0.2f);
            c.fillRect(int(x - s * 0.26f), int(y + s * 0.2f), int(s * 0.32f), std::max(2, int(s * 0.18f)), darker(metal, 0.2f), p.a);
            limb(c, x + s * 0.06f, y + s * 0.3f, x + s * 0.34f, y + s * 0.4f, s * 0.06f, metal, p.a);
            c.fillCircle(x + s * 0.3f, y + s * 0.24f, s * 0.12f, l == Nekro ? rgb(30, 30, 34) : metal, p.a);
            if (l == Nekro && k < 1.f) c.fillCircle(x + s * 0.3f, y + s * 0.24f, s * 0.05f, rgb(150, 20, 20), al(p.a, int(255.f * (1.f - k))));
            return;
        }
        case Sardakk: {  // on its back, legs in the air
            pool(c, p, k, bloodOf(l));
            const uint32_t shell = darker(tint(rgb(70, 48, 40), p.col, 0.5f), 0.25f);
            ell(c, x, y + s * 0.3f, s * 0.22f, s * 0.12f, shell, p.a);
            for (int i = 0; i < 4; ++i)
                limb(c, x - s * 0.15f + float(i) * s * 0.1f, y + s * 0.26f, x - s * 0.2f + float(i) * s * 0.14f, y + s * 0.08f, s * 0.04f,
                     rgb(190, 170, 130), p.a);
            return;
        }
        case Cabal: {
            pool(c, p, k, bloodOf(l), 1.1f);
            ell(c, x - s * 0.02f, y + s * 0.3f, s * 0.22f, s * 0.12f, darker(tint(rgb(124, 82, 64), p.col, 0.35f), 0.35f), p.a);
            return;
        }
        case Florzen: {  // a collapsed bell
            pool(c, p, k, bloodOf(l), 1.1f);
            const uint32_t bell = darker(tint(rgb(120, 168, 255), p.col, 0.35f), 0.3f);
            ell(c, x, y + s * 0.32f, s * 0.24f, s * 0.08f, bell, al(p.a, 200));
            for (int i = 0; i < 4; ++i) limb(c, x - s * 0.15f + float(i) * s * 0.1f, y + s * 0.34f, x - s * 0.2f + float(i) * s * 0.12f, y + s * 0.46f, s * 0.03f, bell, p.a);
            return;
        }
        case Khrask: {  // a heap of stones and torn grass
            const uint32_t stone = darker(tint(rgb(98, 92, 84), p.col, 0.3f), 0.2f);
            static const float rx[5] = {-0.22f, -0.05f, 0.12f, 0.26f, 0.02f}, ry[5] = {0.32f, 0.36f, 0.3f, 0.38f, 0.24f}, rr[5] = {0.1f, 0.12f, 0.09f, 0.07f, 0.08f};
            for (int i = 0; i < 5; ++i) c.fillCircle(x + rx[i] * s, y + ry[i] * s, rr[i] * s, i % 2 ? stone : darker(stone, 0.2f), p.a);
            limb(c, x - s * 0.1f, y + s * 0.24f, x - s * 0.04f, y + s * 0.14f, s * 0.03f, rgb(96, 160, 60), p.a);
            return;
        }
        case Zelian: {  // two halves of a rock, the glow going out
            const uint32_t stone = darker(tint(rgb(72, 64, 62), p.col, 0.32f), 0.15f);
            c.fillCircle(x - s * 0.13f, y + s * 0.32f, s * 0.15f, stone, p.a);
            c.fillCircle(x + s * 0.15f, y + s * 0.34f, s * 0.13f, darker(stone, 0.15f), p.a);
            if (k < 1.f) glow(c, x, y + s * 0.33f, s * 0.12f, rgb(255, 80, 40), al(p.a, int(220.f * (1.f - k))), p.style);
            return;
        }
        case Cheiran: {
            pool(c, p, k, bloodOf(l), 1.1f);
            const uint32_t shell = darker(tint(rgb(70, 92, 112), p.col, 0.4f), 0.25f);
            ell(c, x, y + s * 0.3f, s * 0.24f, s * 0.13f, shell, p.a);
            limb(c, x + s * 0.2f, y + s * 0.3f, x + s * 0.38f, y + s * 0.18f, s * 0.06f, shell, p.a);
            return;
        }
        case Ghoti: {
            pool(c, p, k, bloodOf(l), 1.2f);
            const uint32_t body = darker(tint(rgb(44, 112, 124), p.col, 0.38f), 0.3f);
            ell(c, x, y + s * 0.32f, s * 0.34f, s * 0.11f, body, p.a);
            tri(c, x - s * 0.3f, y + s * 0.32f, x - s * 0.48f, y + s * 0.22f, x - s * 0.48f, y + s * 0.42f, darker(body, 0.2f), p.a);
            return;
        }
        default: break;
    }
    fallenMan(c, p, poolK, bloodOf(l), false);
    if (l == Naalu) limb(c, x + s * 0.3f, y + s * 0.34f, x + s * 0.55f, y + s * 0.42f, s * 0.12f, darker(tint(rgb(118, 150, 88), p.col, 0.45f), 0.3f), p.a);
    if (l == Empyrean) tri(c, x - s * 0.2f, y + s * 0.2f, x - s * 0.55f, y + s * 0.1f, x - s * 0.45f, y + s * 0.34f, darker(tint(rgb(74, 42, 92), p.col, 0.35f), 0.3f), p.a);
    if (l == Myko) dome(c, x + s * 0.5f, y + s * 0.4f, s * 0.2f, s * 0.12f, rgb(150, 146, 140), p.a);
    if (l == Axis) ell(c, x - s * 0.4f, y + s * 0.3f, s * 0.14f, s * 0.1f, darker(tint(rgb(64, 42, 32), p.col, 0.4f), 0.2f), p.a);
}

// ====================================================================== mechs
namespace {

void arborecMech(Canvas& c, const Pose& p, bool wreck) {  // a plant behemoth with a flytrap's mouth
    const float s = p.s, x = p.x, y = p.y, t = secs(p.now) + p.phase;
    const float dk = wreck ? 0.5f : 0.f;
    const uint32_t trunk = darker(pop(tint(rgb(96, 104, 52), p.col, 0.25f), p.style), dk);
    const uint32_t vine = darker(pop(tint(rgb(150, 140, 62), p.col, 0.25f), p.style), dk);
    const uint32_t pod = darker(pop(tint(rgb(88, 150, 60), p.col, 0.4f), p.style), dk);
    ell(c, x, y + p.lift, s * 0.46f, s * 0.12f, rgb(0, 0, 0), al(p.a, 80));
    for (int k = 0; k < 6; ++k) {  // roots
        const float rx = x + (float(k) - 2.5f) * s * 0.16f;
        limb(c, x + (float(k) - 2.5f) * s * 0.04f, y - s * 0.1f, rx, y + s * 0.02f, s * 0.06f, darker(trunk, 0.3f), p.a);
    }
    quad(c, x - s * 0.17f, y - s * 0.04f, x + s * 0.17f, y - s * 0.04f, x + s * 0.11f, y - s * 0.52f, x - s * 0.11f, y - s * 0.52f, trunk, p.a);
    limb(c, x - s * 0.05f, y - s * 0.1f, x - s * 0.02f, y - s * 0.45f, s * 0.03f, darker(trunk, 0.3f), p.a);
    if (!wreck)
        for (int k = 0; k < 4; ++k) {  // tentacle vines, curling
            const float side = k < 2 ? -1.f : 1.f, out = (k % 2 ? 0.36f : 0.24f);
            float px = x + side * s * 0.1f, py = y - s * 0.56f;
            for (int j = 1; j <= 4; ++j) {
                const float f = float(j) / 4.f, w = std::sin(t * 1.8f + float(k) * 1.3f + f * 3.f) * s * 0.07f;
                const float nx = x + side * s * (0.1f + out * f) + w, ny = y - s * (0.56f + 0.36f * f) + std::cos(f * 3.f + t) * s * 0.03f;
                limb(c, px, py, nx, ny, s * (0.07f - 0.012f * float(j)), vine, p.a);
                px = nx;
                py = ny;
            }
            c.fillCircle(px, py, s * 0.025f, lighter(vine, 0.3f), p.a);
        }
    else
        for (int k = 0; k < 4; ++k) limb(c, x + (float(k) - 1.5f) * s * 0.1f, y - s * 0.5f, x + (float(k) - 1.5f) * s * 0.22f, y - s * 0.12f, s * 0.04f, vine, p.a);
    const float hy = y - s * (wreck ? 0.5f : 0.7f), d = float(p.dir);
    c.fillCircle(x, hy, s * 0.2f, pod, p.a);
    c.fillCircle(x - s * 0.06f, hy - s * 0.07f, s * 0.08f, lighter(pod, 0.25f), al(p.a, 200));
    const float my = hy + d * s * 0.05f;
    ell(c, x, my, s * 0.14f, s * 0.075f, rgb(110, 30, 50), p.a);  // lips
    ell(c, x, my, s * 0.11f, s * 0.05f, rgb(40, 8, 16), p.a);
    for (int k = -2; k <= 2; ++k) {
        tri(c, x + float(k) * s * 0.04f - s * 0.012f, my - s * 0.045f, x + float(k) * s * 0.04f + s * 0.012f, my - s * 0.045f, x + float(k) * s * 0.04f,
            my - s * 0.005f, rgb(240, 236, 220), p.a);
        tri(c, x + float(k) * s * 0.04f - s * 0.012f, my + s * 0.045f, x + float(k) * s * 0.04f + s * 0.012f, my + s * 0.045f, x + float(k) * s * 0.04f,
            my + s * 0.005f, rgb(240, 236, 220), p.a);
    }
    if (p.firing && !wreck) glow(c, x, my, s * 0.12f, rgb(255, 150, 40), p.a, p.style);
}

void muaatMech(Canvas& c, const Pose& p, bool wreck) {  // the giant egg, two flamethrowers
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t metal = darker(rgb(70, 60, 54), wreck ? 0.4f : 0.f), egg = darker(pop(tint(rgb(128, 88, 54), p.col, 0.3f), p.style), wreck ? 0.5f : 0.f);
    ell(c, x, y + p.lift, s * 0.46f, s * 0.12f, rgb(0, 0, 0), al(p.a, 80));
    const float lift = wreck ? 0.f : std::sin(p.walk) * s * 0.04f;
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) {
            const float hx = x + float(side) * s * 0.14f, hy = y - s * (k ? 0.3f : 0.4f);
            const float kx = x + float(side) * s * (k ? 0.42f : 0.36f), ky = y - s * (k ? 0.5f : 0.62f) - (k == (side > 0) ? lift : -lift);
            const float fx = x + float(side) * s * (k ? 0.46f : 0.34f), fy = y - s * (k ? 0.0f : 0.06f);
            limb(c, hx, hy, kx, ky, s * 0.07f, metal, p.a);
            limb(c, kx, ky, fx, fy, s * 0.05f, lighter(metal, 0.15f), p.a);
            c.fillCircle(kx, ky, s * 0.04f, darker(metal, 0.2f), p.a);
        }
    const float ey = y - s * 0.46f;
    ell(c, x, ey, s * 0.22f, s * 0.3f, egg, p.a);
    ell(c, x - s * 0.08f, ey - s * 0.12f, s * 0.07f, s * 0.11f, lighter(egg, 0.3f), al(p.a, 190));
    c.fillRect(int(x - s * 0.21f), int(ey + s * 0.02f), int(s * 0.42f), std::max(1, int(s * 0.04f)), darker(egg, 0.4f), p.a);
    for (int side = -1; side <= 1; side += 2) {  // the flamethrowers
        const float nx = x + float(side) * s * 0.24f;
        limb(c, x + float(side) * s * 0.15f, ey + s * 0.04f, nx, ey + s * 0.02f + float(p.dir) * s * 0.06f, s * 0.07f, metal, p.a);
        if (!wreck) glow(c, nx, ey + s * 0.02f + float(p.dir) * s * 0.06f, s * (p.firing ? 0.1f : 0.05f), rgb(255, 150, 50), p.a, p.style);
    }
    if (!wreck) {
        for (int k = 0; k < 3; ++k) c.fillRect(int(x - s * 0.1f + float(k) * s * 0.07f), int(ey - s * 0.08f), std::max(1, int(s * 0.03f)), int(s * 0.12f), rgb(255, 150, 50), p.a);
        glow(c, x, ey - s * 0.02f, s * 0.14f, rgb(255, 130, 40), al(p.a, 160), p.style);
    }
}

void nekroMech(Canvas& c, const Pose& p, bool wreck) {  // a giant drone with one pulsing red eye
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = wreck ? 0.f : s * (0.26f + 0.04f * std::sin(t * 1.6f));
    const float cy = p.y - s * 0.36f - h;
    shadowAt(c, x, p.y + p.lift, s * 0.34f, s * 0.1f, h, s, p.a);
    const uint32_t metal = darker(pop(tint(rgb(36, 36, 40), p.col, 0.3f), p.style), wreck ? 0.3f : 0.f);
    const float spin = wreck ? 0.6f : t * 0.4f;
    for (int k = 0; k < 6; ++k) {  // claw arms
        const float a = spin + float(k) * kPi / 3.f, flex = wreck ? 0.f : std::sin(t * 2.f + float(k)) * 0.15f;
        const float jx = x + std::cos(a) * s * 0.36f, jy = cy + std::sin(a) * s * 0.3f;
        const float tx = x + std::cos(a + 0.5f + flex) * s * 0.52f, ty = cy + std::sin(a + 0.5f + flex) * s * 0.44f;
        limb(c, x + std::cos(a) * s * 0.2f, cy + std::sin(a) * s * 0.17f, jx, jy, s * 0.05f, metal, p.a);
        limb(c, jx, jy, tx, ty, s * 0.035f, lighter(metal, 0.2f), p.a);
        if (!wreck) c.fillCircle(jx, jy, s * 0.02f, rgb(255, 50, 40), p.a);
    }
    for (int k = 0; k < 10; ++k) {  // spikes
        const float a = float(k) * kPi / 5.f + spin * 0.3f;
        limb(c, x + std::cos(a) * s * 0.22f, cy + std::sin(a) * s * 0.22f, x + std::cos(a) * s * 0.3f, cy + std::sin(a) * s * 0.3f, s * 0.03f, metal, p.a);
    }
    c.fillCircle(x, cy, s * 0.24f, metal, p.a);
    c.fillCircle(x - s * 0.07f, cy - s * 0.08f, s * 0.08f, lighter(metal, 0.18f), al(p.a, 180));
    const float ey = cy + float(p.dir) * s * 0.04f;
    c.ring(x, ey, s * 0.13f, std::max(1.f, s * 0.025f), rgb(176, 176, 186), p.a);
    if (!wreck) {
        const float pl = 0.55f + 0.45f * std::sin(t * 3.4f);
        glow(c, x, ey, s * (0.22f + 0.06f * pl), rgb(255, 30, 30), al(p.a, int(200.f * pl)), p.style);
        c.fillCircle(x, ey, s * 0.1f, rgb(224, 20, 24), p.a);
        c.fillCircle(x - s * 0.03f, ey - s * 0.03f, s * 0.035f, rgb(255, 200, 170), p.a);
    } else {
        c.fillCircle(x, ey, s * 0.1f, rgb(50, 10, 10), p.a);
    }
}

void sardakkMech(Canvas& c, const Pose& p, bool wreck) {  // a giant beetle
    const float s = p.s, x = p.x, d = float(p.dir), y = p.y - s * 0.22f;
    const uint32_t shell = darker(pop(tint(rgb(30, 30, 34), p.col, 0.4f), p.style), wreck ? 0.3f : 0.f), leg = rgb(26, 26, 28);
    ell(c, x, p.y - s * 0.02f + p.lift, s * 0.46f, s * 0.14f, rgb(0, 0, 0), al(p.a, 80));
    const float sc = wreck ? 0.f : std::sin(p.walk * 1.2f) * s * 0.04f;
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; ++k) {
            const float fy = (float(k) - 1.f) * s * 0.18f;
            const float hx = x + float(side) * s * 0.22f, hy = y + d * fy;
            const float kx = x + float(side) * s * 0.42f, ky = y + d * fy * 1.2f - s * 0.12f + (k % 2 ? sc : -sc);
            const float fx = x + float(side) * s * 0.5f, fyy = p.y - s * 0.02f + d * fy * 0.6f;
            limb(c, hx, hy, kx, ky, s * 0.06f, leg, p.a);
            limb(c, kx, ky, fx, fyy, s * 0.045f, leg, p.a);
        }
    ell(c, x, y - d * s * 0.06f, s * 0.3f, s * 0.26f, shell, p.a);  // the wing cases
    limb(c, x, y - d * s * 0.3f, x, y + d * s * 0.16f, s * 0.02f, darker(shell, 0.5f), p.a);
    ell(c, x - s * 0.12f, y - d * s * 0.1f - s * 0.04f, s * 0.08f, s * 0.15f, lighter(shell, 0.22f), al(p.a, 170));
    ell(c, x + s * 0.12f, y - d * s * 0.1f - s * 0.04f, s * 0.06f, s * 0.12f, lighter(shell, 0.12f), al(p.a, 150));
    ell(c, x, y + d * s * 0.2f, s * 0.2f, s * 0.1f, lighter(shell, 0.08f), p.a);  // pronotum
    const float hy = y + d * s * 0.3f;
    ell(c, x, hy, s * 0.11f, s * 0.07f, darker(shell, 0.2f), p.a);
    limb(c, x - s * 0.06f, hy, x - s * 0.1f, hy + d * s * 0.1f, s * 0.03f, leg, p.a);
    limb(c, x + s * 0.06f, hy, x + s * 0.1f, hy + d * s * 0.1f, s * 0.03f, leg, p.a);
    if (!wreck) {
        c.fillCircle(x - s * 0.06f, hy + d * s * 0.02f, s * 0.025f, rgb(90, 255, 120), p.a);
        c.fillCircle(x + s * 0.06f, hy + d * s * 0.02f, s * 0.025f, rgb(90, 255, 120), p.a);
        if (p.firing) glow(c, x, hy + d * s * 0.08f, s * 0.12f, rgb(90, 170, 255), p.a, p.style);
    }
}

// a humanoid robot (Titans; Roh'Dhna with rock plates)
void robotMech(Canvas& c, const Pose& p, bool wreck, bool rocky, float scale) {
    const float s = p.s * scale, x = p.x, y = p.y;
    const uint32_t metal = darker(pop(tint(rocky ? rgb(96, 86, 72) : rgb(80, 84, 96), p.col, 0.35f), p.style), wreck ? 0.45f : 0.f);
    const uint32_t dk = darker(metal, 0.35f), lit = rocky ? rgb(150, 255, 170) : rgb(226, 96, 255);
    ell(c, x, y + p.lift, s * 0.34f, s * 0.1f, rgb(0, 0, 0), al(p.a, 80));
    const float st = wreck ? 0.f : std::sin(p.walk) * s * 0.03f;
    quad(c, x - s * 0.17f, y - s * 0.42f, x - s * 0.05f, y - s * 0.42f, x - s * 0.06f + st, y, x - s * 0.18f + st, y, dk, p.a);  // legs
    quad(c, x + s * 0.05f, y - s * 0.42f, x + s * 0.17f, y - s * 0.42f, x + s * 0.18f - st, y, x + s * 0.06f - st, y, dk, p.a);
    c.fillRect(int(x - s * 0.2f + st), int(y - s * 0.05f), int(s * 0.15f), std::max(1, int(s * 0.05f)), metal, p.a);
    c.fillRect(int(x + s * 0.05f - st), int(y - s * 0.05f), int(s * 0.15f), std::max(1, int(s * 0.05f)), metal, p.a);
    c.fillCircle(x - s * 0.11f, y - s * 0.22f, s * 0.05f, metal, p.a);  // knees
    c.fillCircle(x + s * 0.11f, y - s * 0.22f, s * 0.05f, metal, p.a);
    quad(c, x - s * 0.22f, y - s * 0.82f, x + s * 0.22f, y - s * 0.82f, x + s * 0.13f, y - s * 0.42f, x - s * 0.13f, y - s * 0.42f, metal, p.a);
    limb(c, x - s * 0.12f, y - s * 0.62f, x + s * 0.12f, y - s * 0.62f, s * 0.02f, dk, p.a);
    if (!wreck) glow(c, x, y - s * 0.7f, s * 0.08f, lit, p.a, p.style);
    c.fillCircle(x, y - s * 0.7f, s * 0.035f, wreck ? dk : lighter(lit, 0.4f), p.a);
    // shoulders (rock / crystal plates for the Roh'Dhna)
    for (int side = -1; side <= 1; side += 2) {
        const float sx = x + float(side) * s * 0.25f, sy = y - s * 0.78f;
        c.fillCircle(sx, sy, s * 0.1f, lighter(metal, 0.1f), p.a);
        if (rocky) {
            tri(c, sx - s * 0.06f, sy - s * 0.04f, sx + s * 0.02f, sy - s * 0.04f, sx - s * 0.02f + float(side) * s * 0.06f, sy - s * 0.2f, pop(rgb(150, 214, 240), p.style), p.a);
            tri(c, sx, sy - s * 0.02f, sx + float(side) * s * 0.08f, sy, sx + float(side) * s * 0.1f, sy - s * 0.14f, pop(rgb(120, 190, 220), p.style), p.a);
        }
    }
    limb(c, x - s * 0.28f, y - s * 0.74f, x - s * 0.32f, y - s * 0.44f, s * 0.08f, metal, p.a);  // the left arm
    c.fillCircle(x - s * 0.32f, y - s * 0.42f, s * 0.05f, dk, p.a);
    limb(c, x + s * 0.28f, y - s * 0.74f, x + s * 0.32f, y - s * 0.5f, s * 0.1f, metal, p.a);  // the arm cannon / fist
    if (!wreck) glow(c, x + s * 0.32f, y - s * 0.5f, s * (p.firing ? 0.12f : 0.08f), lit, p.a, p.style);
    c.fillCircle(x + s * 0.32f, y - s * 0.5f, s * 0.05f, wreck ? dk : lighter(lit, 0.3f), p.a);
    c.fillCircle(x, y - s * 0.9f, s * 0.08f, metal, p.a);  // head
    c.fillRect(int(x - s * 0.06f), int(y - s * 0.91f), int(s * 0.12f) + 1, std::max(1, int(s * 0.025f)), wreck ? dk : lit, p.a);
}

void florzenMech(Canvas& c, const Pose& p, bool wreck) {  // a mechanical jellyfish
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = wreck ? 0.f : s * (0.2f + 0.04f * std::sin(t * 1.7f));
    const float by = p.y - s * 0.62f - h + (wreck ? s * 0.4f : 0.f);
    shadowAt(c, x, p.y + p.lift, s * 0.34f, s * 0.1f, h, s, p.a);
    const uint32_t metal = darker(pop(tint(rgb(112, 120, 150), p.col, 0.35f), p.style), wreck ? 0.4f : 0.f);
    for (int k = 0; k < 6; ++k) {  // jointed metal tentacles
        float px = x + (float(k) - 2.5f) * s * 0.07f, py = by;
        for (int j = 1; j <= 4; ++j) {
            const float w = wreck ? 0.f : std::sin(t * 2.f + float(k) * 1.1f + float(j) * 0.8f) * s * 0.04f;
            const float nx = px + w + (float(k) - 2.5f) * s * 0.02f, ny = py + (wreck ? s * 0.05f : s * 0.13f);
            limb(c, px, py, nx, ny, s * 0.03f, metal, p.a);
            c.fillCircle(nx, ny, s * 0.018f, lighter(metal, 0.3f), p.a);
            px = nx;
            py = ny;
        }
    }
    if (!wreck) glow(c, x, by - s * 0.1f, s * 0.36f, rgb(150, 130, 255), al(p.a, 110), p.style);
    dome(c, x, by, s * 0.3f, s * 0.26f, metal, p.a);
    for (int k = -2; k <= 2; ++k) limb(c, x + float(k) * s * 0.06f, by, x + float(k) * s * 0.11f, by - s * 0.23f + float(k * k) * s * 0.02f, s * 0.015f, darker(metal, 0.4f), p.a);
    c.fillRect(int(x - s * 0.3f), int(by - s * 0.02f), int(s * 0.6f), std::max(1, int(s * 0.04f)), lighter(metal, 0.2f), p.a);
    if (!wreck) {
        glow(c, x, by - s * 0.1f, s * 0.12f, rgb(255, 236, 150), p.a, p.style);
        c.fillCircle(x, by - s * 0.1f, s * 0.055f, rgb(255, 255, 230), p.a);
    }
}

void khraskMech(Canvas& c, const Pose& p, bool wreck) {  // a crystal golem, brown and ice blue
    const float s = p.s, x = p.x, y = p.y;
    const uint32_t stone = darker(pop(tint(rgb(110, 84, 60), p.col, 0.3f), p.style), wreck ? 0.45f : 0.f), dk = darker(stone, 0.35f);
    const uint32_t ice = darker(pop(rgb(150, 212, 240), p.style), wreck ? 0.4f : 0.f), ice2 = lighter(ice, 0.4f);
    ell(c, x, y + p.lift, s * 0.4f, s * 0.11f, rgb(0, 0, 0), al(p.a, 80));
    const float st = wreck ? 0.f : std::sin(p.walk) * s * 0.03f;
    c.fillRect(int(x - s * 0.2f + st), int(y - s * 0.3f), int(s * 0.14f), int(s * 0.3f), dk, p.a);
    c.fillRect(int(x + s * 0.06f - st), int(y - s * 0.3f), int(s * 0.14f), int(s * 0.3f), dk, p.a);
    float xs[6] = {x - s * 0.32f, x - s * 0.22f, x + s * 0.22f, x + s * 0.32f, x + s * 0.2f, x - s * 0.2f};
    float ys[6] = {y - s * 0.62f, y - s * 0.8f, y - s * 0.8f, y - s * 0.62f, y - s * 0.28f, y - s * 0.28f};
    c.fillPolygon(xs, ys, 6, stone, p.a);
    for (int k = 0; k < 4; ++k) {  // crystal spikes on the shoulders and back
        const float cx = x + (float(k) - 1.5f) * s * 0.17f, cy = y - s * 0.78f, hh = s * (k == 1 || k == 2 ? 0.26f : 0.18f);
        tri(c, cx - s * 0.05f, cy, cx + s * 0.05f, cy, cx + (float(k) - 1.5f) * s * 0.03f, cy - hh, ice, p.a);
        limb(c, cx, cy, cx + (float(k) - 1.5f) * s * 0.03f, cy - hh, s * 0.012f, ice2, p.a);
    }
    limb(c, x - s * 0.3f, y - s * 0.66f, x - s * 0.38f, y - s * 0.3f, s * 0.12f, stone, p.a);
    limb(c, x + s * 0.3f, y - s * 0.66f, x + s * 0.38f, y - s * 0.34f, s * 0.12f, stone, p.a);
    tri(c, x + s * 0.32f, y - s * 0.34f, x + s * 0.44f, y - s * 0.34f, x + s * 0.38f, y - s * 0.22f, ice, p.a);
    c.fillCircle(x, y - s * 0.86f, s * 0.08f, stone, p.a);
    if (!wreck) {
        c.fillCircle(x - s * 0.03f, y - s * 0.86f, s * 0.018f, rgb(160, 255, 180), p.a);
        c.fillCircle(x + s * 0.03f, y - s * 0.86f, s * 0.018f, rgb(160, 255, 180), p.a);
        if (p.firing) glow(c, x + s * 0.38f, y - s * 0.3f, s * 0.12f, rgb(160, 255, 180), p.a, p.style);
    }
}

void cheiranMech(Canvas& c, const Pose& p, bool wreck) {  // a giant crab
    Pose q = p;
    q.s = p.s * 1.05f;
    q.y = p.y - q.s * 0.44f;
    if (wreck) q.col = darker(p.col, 0.45f);
    q.firing = false;
    cheiranInf(c, q);
    if (p.firing && !wreck) {
        const float d = float(p.dir);
        glow(c, p.x - p.s * 0.23f, p.y - p.s * 0.3f + d * p.s * 0.31f, p.s * 0.1f, rgb(170, 240, 90), p.a, p.style);
        glow(c, p.x + p.s * 0.23f, p.y - p.s * 0.3f + d * p.s * 0.31f, p.s * 0.1f, rgb(170, 240, 90), p.a, p.style);
    }
}

void ghotiMech(Canvas& c, const Pose& p, bool wreck) {  // a giant angler fish
    const float s = p.s, x = p.x, t = secs(p.now) + p.phase;
    const float h = wreck ? 0.f : s * (0.12f + 0.03f * std::sin(t * 1.6f));
    ell(c, x, p.y + p.lift, s * 0.36f, s * 0.12f, rgb(0, 0, 0), al(p.a, 70));
    const uint32_t body = darker(pop(tint(rgb(36, 86, 104), p.col, 0.35f), p.style), wreck ? 0.45f : 0.f);
    fish(c, x, p.y - s * 0.3f - h, s * 0.36f, p.dir, body, p.a, wreck ? 0.f : t * 0.7f, p.firing && !wreck, !wreck, p.style);
}

void zelianMech(Canvas& c, const Pose& p, bool wreck) {
    const float s = p.s, t = secs(p.now) + p.phase;
    const float h = wreck ? 0.f : s * (0.16f + 0.04f * std::sin(t * 1.4f));
    shadowAt(c, p.x, p.y + p.lift, s * 0.3f, s * 0.1f, h, s, p.a);
    rock(c, p.x, p.y - s * 0.34f - h, s * 0.32f, wreck ? darker(p.col, 0.4f) : p.col, p.a, p.phase, p.style, p.now, wreck ? 0.f : (p.firing ? 1.4f : 1.f));
}

void mortheusMech(Canvas& c, const Pose& p, bool wreck) {
    const float s = p.s, t = secs(p.now) + p.phase;
    if (wreck) return;  // (a nebula leaves nothing behind - the battle fades it out)
    const float h = s * (0.12f + 0.04f * std::sin(t));
    shadowAt(c, p.x, p.y + p.lift, s * 0.36f, s * 0.1f, h, s, p.a);
    const float cy = p.y - s * 0.42f - h;
    nebula(c, p.x, cy, s * 0.42f, p.col, p.a, p.now, p.phase, p.style);
    for (int arm = 0; arm < 2; ++arm)  // and a swirl turning in it
        for (int k = 0; k < 7; ++k) {
            const float a = t * 1.1f + float(arm) * kPi + float(k) * 0.45f, r = s * (0.08f + 0.05f * float(k));
            c.fillCircle(p.x + std::cos(a) * r, cy + std::sin(a) * r * 0.7f, std::max(1.f, s * (0.045f - 0.004f * float(k))), rgb(255, 240, 220), al(p.a, 220 - k * 24));
        }
}

}  // namespace

bool voidPaint() { return g_voidPaint; }
void setVoidPaint(bool on) { g_voidPaint = on; }
bool paintedByHand(Look l, int idx) {
    switch (l) {
        case Default: case Titans: case Kyro: case Creuss: case Mahact: return true;
        case Rohdhna: return idx % 2 == 0;
        default: return false;
    }
}
int paintIdx(Look l, int idx) { return l == Cabal ? idx % 3 : l == Rohdhna ? idx % 2 : 0; }
// Voidfall (0.51.1): the default mech painted: a soft violet pool under it, the picture re-lit, its visor glowing.
void paintedMech(Canvas& c, const Pose& p, float footY, float sz, uint32_t mc, bool mirror, bool wreck) {
    const float x = p.x;
    ell(c, x + sz * 0.04f, p.y + p.lift + sz * 0.02f, sz * 0.42f, sz * 0.1f, rgb(14, 4, 28), al(p.a, 60));
    ell(c, x + sz * 0.04f, p.y + p.lift + sz * 0.02f, sz * 0.28f, sz * 0.065f, rgb(14, 4, 28), al(p.a, 90));
    float w = 0, h = 0;
    if (!icons::drawUnitArtOnPainted(c, "unit/mech", x, footY, int(sz * 0.9f), int(sz), mc, mirror, p.a, &w, &h)) return;
    if (wreck) return;
    const float ex = x + (mirror ? -1.f : 1.f) * w * kMechEyeX, ey = footY - h * kMechEyeY;
    glow(c, ex, ey, sz * 0.055f, rgb(110, 215, 255), al(p.a, 170), 2);
    c.fillRect(int(ex - w * 0.05f), int(ey - sz * 0.006f), std::max(1, int(w * 0.1f)), std::max(1, int(sz * 0.018f)), rgb(200, 245, 255), p.a);
    const float cx2 = x + (mirror ? -1.f : 1.f) * w * kMechCoreX, cy2 = footY - h * kMechCoreY;  // the chest core
    glow(c, cx2, cy2, sz * 0.045f, rgb(200, 120, 255), al(p.a, 140), 2);
    c.fillCircle(cx2, cy2, std::max(0.7f, sz * 0.018f), rgb(240, 210, 255), p.a);
}

bool hasMech(Look l) {
    switch (l) {
        case Arborec: case Muaat: case Nekro: case Sardakk: case Titans: case Rohdhna: case Florzen: case Khrask: case Mortheus:
        case Zelian: case Cheiran: case Ghoti: return true;
        default: return false;
    }
}
bool hasStructure(Look l, bool pds) { return (l == Titans && pds) || l == Zelian || l == Mortheus; }
bool hasFighter(Look l) { return l == Naalu; }
bool infantryTimed(Look l) {
    switch (l) {
        case Arborec: case Naalu: case Nekro: case Sardakk: case Cabal: case Empyrean: case Florzen: case Khrask: case Mortheus:
        case Zelian: case Ghoti: return true;
        default: return false;
    }
}

bool mech(Canvas& c, Look l, const Pose& p, bool wreck) {
    switch (l) {
        case Arborec: arborecMech(c, p, wreck); return true;
        case Muaat: muaatMech(c, p, wreck); return true;
        case Nekro: nekroMech(c, p, wreck); return true;
        case Sardakk: sardakkMech(c, p, wreck); return true;
        case Titans: robotMech(c, p, wreck, false, 1.f); return true;
        case Rohdhna: robotMech(c, p, wreck, true, 1.f); return true;
        case Florzen: florzenMech(c, p, wreck); return true;
        case Khrask: khraskMech(c, p, wreck); return true;
        case Mortheus: mortheusMech(c, p, wreck); return true;
        case Zelian: zelianMech(c, p, wreck); return true;
        case Cheiran: cheiranMech(c, p, wreck); return true;
        case Ghoti: ghotiMech(c, p, wreck); return true;
        default: return false;
    }
}

void mechOverlay(Canvas& c, Look l, const Pose& p, bool wreck) {
    const float s = p.s, x = p.x, y = p.y;
    if (l == Creuss || l == Mahact) {  // glowing seams across the default mech
        if (wreck) return;
        const uint32_t sc = l == Creuss ? rgb(70, 200, 255) : rgb(255, 66, 56);
        const float w = std::max(1.f, s * 0.025f);
        glow(c, x, y - s * 0.55f, s * 0.14f, sc, al(p.a, 140), p.style);
        limb(c, x, y - s * 0.78f, x, y - s * 0.35f, w, sc, p.a);
        limb(c, x - s * 0.22f, y - s * 0.6f, x + s * 0.22f, y - s * 0.6f, w, sc, al(p.a, 220));
        limb(c, x - s * 0.18f, y - s * 0.3f, x - s * 0.15f, y - s * 0.05f, w, sc, al(p.a, 200));
        limb(c, x + s * 0.18f, y - s * 0.3f, x + s * 0.15f, y - s * 0.05f, w, sc, al(p.a, 200));
        c.fillCircle(x, y - s * 0.55f, s * 0.04f, lighter(sc, 0.5f), p.a);
    } else if (l == Cabal) {  // bloody organic growths (a necromorph's)
        const float t = secs(p.now) + p.phase, pl = 1.f + 0.06f * std::sin(t * 2.2f);
        const uint32_t flesh = wreck ? rgb(70, 20, 22) : rgb(150, 40, 46), hi = rgb(214, 96, 96);
        static const float gx[6] = {-0.18f, 0.14f, 0.2f, -0.08f, 0.02f, -0.22f}, gy[6] = {-0.7f, -0.62f, -0.3f, -0.42f, -0.82f, -0.2f};
        static const float gr[6] = {0.09f, 0.07f, 0.06f, 0.08f, 0.05f, 0.05f};
        for (int k = 0; k < 6; ++k) {
            c.fillCircle(x + gx[k] * s, y + gy[k] * s, gr[k] * s * pl, flesh, p.a);
            c.fillCircle(x + gx[k] * s - gr[k] * s * 0.3f, y + gy[k] * s - gr[k] * s * 0.3f, gr[k] * s * 0.4f, hi, al(p.a, 170));
        }
        for (int k = 0; k < 3; ++k) {  // hanging tendrils
            const float tx = x + (float(k) - 1.f) * s * 0.16f, sw = std::sin(t * 1.5f + float(k)) * s * 0.03f;
            limb(c, tx, y - s * 0.38f, tx + sw, y - s * 0.12f, std::max(1.f, s * 0.025f), flesh, p.a);
        }
    }
}

bool fighter(Canvas& c, Look l, const Pose& p) {
    if (l != Naalu) return false;
    // the crystal fighter: a long faceted crystal, its point toward the enemy
    const float s = p.s, x = p.x, y = p.y, d = float(p.dir), t = secs(p.now) + p.phase;
    const uint32_t cr = pop(tint(rgb(150, 230, 255), p.col, 0.35f), p.style);
    glow(c, x, y, s * 0.55f, rgb(140, 220, 255), al(p.a, 120), p.style);
    float xs[6] = {x, x + s * 0.16f, x + s * 0.12f, x, x - s * 0.12f, x - s * 0.16f};
    float ys[6] = {y + d * s * 0.55f, y + d * s * 0.05f, y - d * s * 0.35f, y - d * s * 0.45f, y - d * s * 0.35f, y + d * s * 0.05f};
    c.fillPolygon(xs, ys, 6, cr, al(p.a, 215));
    tri(c, xs[0], ys[0], xs[1], ys[1], x, y, lighter(cr, 0.4f), al(p.a, 200));
    tri(c, xs[0], ys[0], xs[5], ys[5], x, y, darker(cr, 0.15f), al(p.a, 200));
    for (int k = 0; k < 6; ++k) limb(c, xs[k], ys[k], xs[(k + 1) % 6], ys[(k + 1) % 6], std::max(0.8f, s * 0.02f), lighter(cr, 0.6f), al(p.a, 220));
    const float tw = 0.5f + 0.5f * std::sin(t * 6.f);
    c.fillCircle(x + s * 0.05f, y - d * s * 0.1f, std::max(1.f, s * 0.04f), rgb(255, 255, 255), al(p.a, int(150 + 100 * tw)));
    return true;
}

float structureScale(Look l, bool pds) {
    if (l == Titans && pds) return 1.5f;
    if (l == Zelian) return 1.f;
    return 1.f;
}

bool structure(Canvas& c, Look l, bool pds, const Pose& p, bool ruined) {
    switch (l) {
        case Titans: {
            if (!pds) return false;
            // (owner) a humanoid robot much bigger than the mech, with rocket pods on its shoulders
            Pose q = p;
            robotMech(c, q, ruined, false, 1.f);
            const float s = p.s, x = p.x, y = p.y;
            const uint32_t metal = darker(pop(tint(rgb(80, 84, 96), p.col, 0.35f), p.style), ruined ? 0.45f : 0.f);
            for (int side = -1; side <= 1; side += 2) {
                const float bx = x + float(side) * s * 0.3f, by = y - s * 0.94f;
                c.fillRect(int(bx - s * 0.09f), int(by - s * 0.07f), int(s * 0.18f), int(s * 0.12f), metal, p.a);
                for (int k = 0; k < 3; ++k) c.fillCircle(bx - s * 0.05f + float(k) * s * 0.05f, by - s * 0.01f, std::max(1.f, s * 0.018f), rgb(24, 24, 28), p.a);
            }
            return true;
        }
        case Zelian: {  // even bigger rocks - they do nothing
            const float s = p.s;
            ell(c, p.x, p.y + p.lift, s * 0.42f, s * 0.12f, rgb(0, 0, 0), al(p.a, 80));
            rock(c, p.x, p.y - s * 0.34f, s * 0.36f, ruined ? darker(p.col, 0.4f) : p.col, p.a, float(pds ? 1 : 2), p.style, p.now, ruined ? 0.f : 0.6f);
            return true;
        }
        case Mortheus: {  // a great, dim nebula
            if (ruined) return true;
            const float s = p.s;
            nebula(c, p.x, p.y - s * 0.4f, s * 0.5f, p.col, al(p.a, 200), p.now, float(pds ? 0.7f : 2.1f), p.style, true);
            return true;
        }
        default: return false;
    }
}

}  // namespace gunits
