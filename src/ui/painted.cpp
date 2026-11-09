// Voidfall's painted ground units (0.51.1) - see painted.h.
#include "ui/painted.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

#include "gfx/canvas.h"

namespace painted {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
}  // namespace

const Lit* relight(const Canvas& c, uint64_t key, float left, float top, float w, float h,
                   const std::function<void(Canvas&, float, float)>& draw, uint32_t now, const Light& L) {
    if (w <= 0.f || h <= 0.f) return nullptr;
    static std::unordered_map<uint64_t, Lit> cache;
    key = (key ^ uint64_t(c.scale() * 1000.f) * 0x9E3779B97F4A7C15ull) * 1099511628211ull;
    auto it = cache.find(key);
    if (it != cache.end()) {
        it->second.used = now;
        return it->second.img.empty() ? nullptr : &it->second;
    }
    if (cache.size() > 2000) {  // forget pictures not drawn for a while
        for (auto e = cache.begin(); e != cache.end();) {
            if (int32_t(now - e->second.used) > 1500) e = cache.erase(e);
            else ++e;
        }
        if (cache.size() > 4000) cache.clear();
    }
    const float s = c.scale();
    // the canvas: the box with a unit of room round it, the anchor on a whole real pixel
    const int wu = int(std::ceil(w)) + 2, hu = int(std::ceil(h)) + 2;
    static std::unique_ptr<Canvas> off;
    if (!off || off->width() != wu || off->height() != hu || std::fabs(off->scale() - s) > 1e-4f) off = std::make_unique<Canvas>(wu, hu, s);
    Canvas& o = *off;
    const float apx = std::ceil((1.f - left) * s), apy = std::ceil((1.f - top) * s);  // the anchor, real pixels
    const float ax = apx / s, ay = apy / s;
    const int pw = o.pw(), ph = o.ph();
    std::vector<uint32_t> blackPx(size_t(pw) * size_t(ph));
    o.resetClip();
    o.clear(0x000000);
    draw(o, ax, ay);
    std::copy(o.pixels(), o.pixels() + blackPx.size(), blackPx.begin());
    o.clear(0xFFFFFF);
    draw(o, ax, ay);
    const uint32_t* whitePx = o.pixels();
    // alpha from the two backgrounds, colour from the black one (unpremultiplied)
    std::vector<float> A(size_t(pw) * size_t(ph), 0.f);
    std::vector<uint32_t> C(size_t(pw) * size_t(ph), 0);
    int vx0 = pw, vy0 = ph, vx1 = -1, vy1 = -1;  // everything visible
    int tx0 = pw, ty0 = ph, tx1 = -1, ty1 = -1;  // the body (solid)
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x) {
            const size_t i = size_t(y) * size_t(pw) + size_t(x);
            const uint32_t b = blackPx[i], wh = whitePx[i];
            const int br = int((b >> 16) & 255), bg = int((b >> 8) & 255), bb = int(b & 255);
            const float diff = float(int((wh >> 16) & 255) - br + int((wh >> 8) & 255) - bg + int(wh & 255) - bb) / (3.f * 255.f);
            const float a = clamp01(1.f - diff);
            if (a < 0.02f) continue;
            A[i] = a;
            auto un = [&](int v) { return std::min(255, int(float(v) / a + 0.5f)); };
            C[i] = rgb(un(br), un(bg), un(bb));
            vx0 = std::min(vx0, x), vx1 = std::max(vx1, x), vy0 = std::min(vy0, y), vy1 = std::max(vy1, y);
            if (a >= 0.5f) tx0 = std::min(tx0, x), tx1 = std::max(tx1, x), ty0 = std::min(ty0, y), ty1 = std::max(ty1, y);
        }
    Lit& out = cache[key];
    out.used = now;
    if (vx1 < 0) return nullptr;  // (remembered as nothing)
    if (tx1 < 0) tx0 = vx0, tx1 = vx1, ty0 = vy0, ty1 = vy1;
    // the body's mask for the edges: only the fairly solid parts (not shadows or the soft edge of a glow)
    auto S = [&](int x, int y) -> float {
        if (x < 0 || y < 0 || x >= pw || y >= ph) return 0.f;
        return clamp01((A[size_t(y) * size_t(pw) + size_t(x)] - 0.35f) / 0.4f);
    };
    const int d = std::max(1, int(std::lround(float(std::max(tx1 - tx0, ty1 - ty0)) / 40.f)));
    const float tw = float(std::max(1, tx1 - tx0)), th = float(std::max(1, ty1 - ty0));
    Image& im = out.img;
    im.w = vx1 - vx0 + 1;
    im.h = vy1 - vy0 + 1;
    im.px.assign(size_t(im.w) * size_t(im.h), 0u);
    const bool plain = !L.violet && !L.shade && !L.lift && !L.rim && !L.key;  // (only kept: drawn as it is)
    for (int y = vy0; y <= vy1; ++y)
        for (int x = vx0; x <= vx1; ++x) {
            const size_t i = size_t(y) * size_t(pw) + size_t(x);
            const float a = A[i];
            if (a <= 0.f) continue;
            uint32_t col = C[i];
            if (plain) {
                const uint32_t A8 = uint32_t(std::lround(a * 255.f));
                im.px[size_t(y - vy0) * size_t(im.w) + size_t(x - vx0)] = (A8 << 24) | ((((col >> 16) & 255) * A8 / 255) << 16) |
                                                                         ((((col >> 8) & 255) * A8 / 255) << 8) | ((col & 255) * A8 / 255);
                continue;
            }
            const int r = int((col >> 16) & 255), g = int((col >> 8) & 255), b = int(col & 255);
            const int lum = (r * 77 + g * 150 + b * 29) >> 8;
            const int mx = std::max(r, std::max(g, b));
            const float keep = 1.f - 0.85f * clamp01(float(mx - 195) / 60.f);  // glowing parts keep their light
            const float t = clamp01(float(y - ty0) / th), u = clamp01(float(x - tx0) / tw);
            col = mix(col, kShade, int(float((255 - lum) * L.violet / 255) * keep));
            const float sh = clamp01((0.62f * t + 0.38f * u - 0.34f) / 0.6f);
            col = mix(col, kDeep, int(sh * float(L.shade) * keep));
            const float lift = clamp01((0.5f - (0.6f * t + 0.4f * u)) / 0.5f);
            col = mix(col, kWarmHi, int(lift * float(L.lift) * keep));
            const float body = S(x, y);
            if (body > 0.f) {  // only the outer edges: a narrow gap in the body has the body again 3 steps on
                // (a thin part - a leg, a tentacle - gets only half: it would be all edge)
                const float key = body * (1.f - std::max(S(x - d, y - d), S(x - 3 * d, y - 3 * d))) * (0.5f + 0.5f * S(x + d, y + d));
                const float rim = body * (1.f - std::max(S(x + d, y - d), S(x + 3 * d, y - 3 * d))) * (0.5f + 0.5f * S(x - d, y + d));
                col = mix(col, kWarm, int(key * float(L.key) * keep));
                col = mix(col, kRim, int(rim * float(L.rim)));
            }
            const uint32_t A8 = uint32_t(std::lround(a * 255.f));
            im.px[size_t(y - vy0) * size_t(im.w) + size_t(x - vx0)] = (A8 << 24) | ((((col >> 16) & 255) * A8 / 255) << 16) |
                                                                     ((((col >> 8) & 255) * A8 / 255) << 8) | ((col & 255) * A8 / 255);
        }
    out.ax = int(apx) - vx0;
    out.ay = int(apy) - vy0;
    return &out;
}

void drawLit(Canvas& c, const Lit& l, float x, float y, int alpha) {
    if (l.img.empty() || alpha <= 0) return;
    c.pxBlit(l.img, int(std::lround(c.fX(x))) - l.ax, int(std::lround(c.fY(y))) - l.ay, alpha);
}

}  // namespace painted
