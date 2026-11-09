#include "ui/pixel_ships.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>

#include "gfx/image.h"
#include "ui/icons.h"

using namespace ti;

namespace pxship {

const char* pictureOf(UnitType t) {
    switch (t) {
        case UnitType::Carrier: return "unit/carrier";
        case UnitType::Cruiser: return "unit/cruiser";
        case UnitType::Destroyer: return "unit/destroyer";
        case UnitType::Dreadnought: return "unit/dreadnought";
        case UnitType::Fighter: return "unit/fighter";
        case UnitType::Flagship:
        case UnitType::Flagship2: return "unit/flagship";
        case UnitType::WarSun: return "unit/warsun";
        case UnitType::Mech: return "unit/mech";
        case UnitType::PDS: return "unit/pds";
        case UnitType::SpaceDock: return "unit/spacedock";
        case UnitType::Infantry: return "unit/infantry";
        default: return nullptr;
    }
}

namespace {

uint32_t mixc(uint32_t a, uint32_t b, float t) {
    const int ar = int((a >> 16) & 255), ag = int((a >> 8) & 255), ab = int(a & 255);
    const int br = int((b >> 16) & 255), bg = int((b >> 8) & 255), bb = int(b & 255);
    auto ch = [&](int x, int y) { return int(std::lround(float(x) + float(y - x) * t)); };
    return uint32_t(ch(ar, br) << 16 | ch(ag, bg) << 8 | ch(ab, bb));
}

uint32_t hash32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

// the picture's grey levels (5th and 95th percentile inside it), once per picture
std::pair<float, float> greyRange(const Image& im) {
    std::vector<float> v;
    for (uint32_t p : im.px) {
        const int a = int(p >> 24);
        if (a < 128) continue;
        v.push_back(float(std::min(255, int((p >> 8) & 255) * 255 / a)) / 255.f);
    }
    if (v.size() < 10) return {0.f, 1.f};
    std::sort(v.begin(), v.end());
    return {v[v.size() * 5 / 100], v[v.size() * 95 / 100]};
}

Sprite make(UnitType t, const Image& im, int len, uint32_t col) {
    Sprite s;
    const int W0 = im.w, H0 = im.h;
    const int L = std::max(6, len), H = std::max(3, int(std::lround(float(H0) * float(L) / float(W0))));
    // area-averaged alpha and (premultiplied) grey
    std::vector<float> A(size_t(L) * size_t(H)), G(size_t(L) * size_t(H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x) {
            const float sx0 = float(x) * float(W0) / float(L), sx1 = float(x + 1) * float(W0) / float(L);
            const float sy0 = float(y) * float(H0) / float(H), sy1 = float(y + 1) * float(H0) / float(H);
            float sa = 0, sg = 0, area = 0;
            for (int yy = int(sy0); yy < std::min(H0, int(std::ceil(sy1))); ++yy) {
                const float wy = std::min(sy1, float(yy + 1)) - std::max(sy0, float(yy));
                if (wy <= 0) continue;
                for (int xx = int(sx0); xx < std::min(W0, int(std::ceil(sx1))); ++xx) {
                    const float wx = std::min(sx1, float(xx + 1)) - std::max(sx0, float(xx));
                    if (wx <= 0) continue;
                    const uint32_t p = im.px[size_t(yy) * size_t(W0) + size_t(xx)];
                    sa += float(p >> 24) * wx * wy;
                    sg += float((p >> 8) & 255) * wx * wy;
                    area += wx * wy;
                }
            }
            A[size_t(y) * size_t(L) + size_t(x)] = area > 0 ? sa / area / 255.f : 0.f;
            G[size_t(y) * size_t(L) + size_t(x)] = sa > 0 ? std::min(1.f, sg / sa) : 0.f;
        }
    // the silhouette (1 pixel of room round it for the outline), lone pixels dropped
    const int Wp = L + 2, Hp = H + 2;
    s.w = Wp;
    s.h = Hp;
    std::vector<uint8_t> M(size_t(Wp) * size_t(Hp), 0);
    std::vector<float> Gp(size_t(Wp) * size_t(Hp), 0.f);
    auto at = [&](int x, int y) -> uint8_t { return x < 0 || y < 0 || x >= Wp || y >= Hp ? 0 : M[size_t(y) * size_t(Wp) + size_t(x)]; };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < L; ++x) {
            M[size_t(y + 1) * size_t(Wp) + size_t(x + 1)] = A[size_t(y) * size_t(L) + size_t(x)] > 0.42f ? 1 : 0;
            Gp[size_t(y + 1) * size_t(Wp) + size_t(x + 1)] = G[size_t(y) * size_t(L) + size_t(x)];
        }
    {
        std::vector<uint8_t> keep = M;
        for (int y = 0; y < Hp; ++y)
            for (int x = 0; x < Wp; ++x) {
                if (!M[size_t(y) * size_t(Wp) + size_t(x)]) continue;
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dx || dy) && at(x + dx, y + dy)) ++n;
                if (n < 2) keep[size_t(y) * size_t(Wp) + size_t(x)] = 0;
            }
        M.swap(keep);
    }
    static std::map<const Image*, std::pair<float, float>> ranges;
    auto rit = ranges.find(&im);
    if (rit == ranges.end()) rit = ranges.emplace(&im, greyRange(im)).first;
    const float lo = rit->second.first, hi = std::max(rit->second.first + 0.01f, rit->second.second);
    // six tones of the colour, from near black to near white
    const uint32_t tone[6] = {mixc(col, 0x04050A, 0.84f), mixc(col, 0x0A0C16, 0.62f), mixc(col, 0x343848, 0.42f),
                              mixc(col, 0x969AAA, 0.2f), mixc(col, 0xFFFFFF, 0.3f), mixc(col, 0xFFFFFF, 0.64f)};
    s.px.assign(size_t(Wp) * size_t(Hp), 0);
    // per column: the hull's top and bottom (a cylinder lit from above)
    std::vector<int> top(size_t(Wp), -1), bot(size_t(Wp), -1);
    for (int x = 0; x < Wp; ++x)
        for (int y = 0; y < Hp; ++y)
            if (at(x, y)) {
                if (top[size_t(x)] < 0) top[size_t(x)] = y;
                bot[size_t(x)] = y;
            }
    const bool sphere = t == UnitType::WarSun;
    const bool ground = t == UnitType::Mech || t == UnitType::PDS || t == UnitType::SpaceDock || t == UnitType::Infantry;
    for (int y = 0; y < Hp; ++y)
        for (int x = 0; x < Wp; ++x) {
            if (!at(x, y)) continue;
            const float g = Gp[size_t(y) * size_t(Wp) + size_t(x)];
            float v = std::max(0.f, std::min(1.f, (g - lo) / (hi - lo)));
            v = float(std::pow(double(v), 1.35));
            float rim = 0.f;
            if (!at(x, y - 1)) rim += 0.22f;
            if (!at(x - 1, y)) rim += 0.08f;
            if (!at(x, y + 1)) rim -= 0.2f;
            if (!at(x + 1, y)) rim -= 0.06f;
            float vol;
            if (sphere) {
                const float dx = (float(x) - float(Wp - 1) / 2.f) / (float(Wp) / 2.f), dy = (float(y) - float(Hp - 1) / 2.f) / (float(Hp) / 2.f);
                vol = std::max(-0.35f, std::min(0.35f, -0.3f * dx - 0.35f * dy));
            } else {
                const float u = float(y - top[size_t(x)]) / float(std::max(1, bot[size_t(x)] - top[size_t(x)]));
                vol = 0.3f * (0.42f - u) + 0.1f * float(std::exp(-double((u - 0.22f) / 0.12f) * double((u - 0.22f) / 0.12f)));
            }
            const float lum = std::max(0.f, std::min(1.f, v * 0.62f + 0.07f + rim + vol));
            int lv = int(std::lround(lum * 5.f));
            // panel lines: darker than both neighbours across or down
            auto gat = [&](int xx, int yy) { return Gp[size_t(std::max(0, std::min(Hp - 1, yy))) * size_t(Wp) + size_t(std::max(0, std::min(Wp - 1, xx)))]; };
            const bool line = (g < gat(x - 1, y) - 0.18f && g < gat(x + 1, y) - 0.18f) || (g < gat(x, y - 1) - 0.18f && g < gat(x, y + 1) - 0.18f);
            if (line) lv -= 2;
            lv = std::max(0, std::min(5, lv));
            s.px[size_t(y) * size_t(Wp) + size_t(x)] = 0xFF000000u | tone[lv];
            s.hull.push_back(y * Wp + x);
        }
    // the stripe: the full colour along the spine, a quarter to four fifths of the way along
    if (!sphere && !ground && L >= 12) {
        long sy = 0, n = 0;
        for (int y = 0; y < Hp; ++y)
            for (int x = 0; x < Wp; ++x)
                if (at(x, y)) sy += y, ++n;
        const int spine = n ? int(std::lround(double(sy) / double(n))) : Hp / 2;
        const int rows = Hp >= 14 ? 2 : 1;
        for (int k = 0; k < rows; ++k) {
            const int r = spine - (rows - 1) + k;
            for (int x = Wp / 4; x < Wp * 4 / 5; ++x)
                if (at(x, r) && at(x, r - 1) && at(x, r + 1)) s.px[size_t(r) * size_t(Wp) + size_t(x)] = 0xFF000000u | (k == 0 && rows > 1 ? mixc(col, 0xFFFFFF, 0.18f) : col);
        }
    }
    // windows along the upper hull (the bigger ships)
    if (L >= 36 && !ground) {
        for (int x = 4; x < Wp - 4; x += 4) {
            if (top[size_t(x)] < 0 || bot[size_t(x)] - top[size_t(x)] < 6) continue;
            const int y = top[size_t(x)] + 2;
            if (!at(x, y) || !at(x, y + 1) || (hash32(uint32_t(x) * 2654435761u + uint32_t(len) + uint32_t(t) * 97u) & 1u)) continue;
            s.px[size_t(y) * size_t(Wp) + size_t(x)] = 0xFFFFE8A0u;
            s.lights.push_back(y * Wp + x);
        }
    }
    // the outline: every empty pixel touching the hull
    const uint32_t ink = 0xFF000000u | mixc(col, 0x000000, 0.88f);
    for (int y = 0; y < Hp; ++y)
        for (int x = 0; x < Wp; ++x)
            if (!at(x, y) && (at(x - 1, y) || at(x + 1, y) || at(x, y - 1) || at(x, y + 1))) s.px[size_t(y) * size_t(Wp) + size_t(x)] = ink;
    // the engines: rows whose hull starts at the stern
    if (!sphere && !ground) {
        int minLeft = Wp;
        std::vector<int> left(size_t(Hp), Wp);
        for (int y = 0; y < Hp; ++y)
            for (int x = 0; x < Wp; ++x)
                if (at(x, y)) {
                    left[size_t(y)] = x;
                    minLeft = std::min(minLeft, x);
                    break;
                }
        const int slack = std::max(1, L / 30);
        for (int y = 0; y < Hp;) {
            if (left[size_t(y)] > minLeft + slack) {
                ++y;
                continue;
            }
            int y1 = y;
            while (y1 + 1 < Hp && left[size_t(y1 + 1)] <= minLeft + slack) ++y1;
            s.nozzles.push_back({y, y1, minLeft});
            y = y1 + 1;
        }
    }
    return s;
}

}  // namespace

const Sprite* get(UnitType t, int len, uint32_t col) {
    const char* name = pictureOf(t);
    if (!name) return nullptr;
    const Image* im = icons::image(name);
    if (!im || im->w <= 0 || im->h <= 0) return nullptr;
    static std::map<std::tuple<int, int, uint32_t>, Sprite> cache;
    const auto key = std::make_tuple(int(t), len, col & 0xFFFFFFu);
    auto it = cache.find(key);
    if (it != cache.end()) return &it->second;
    if (cache.size() > 400) cache.clear();  // (sizes and colours are few; a guard)
    return &(cache[key] = make(t, *im, len, col & 0xFFFFFFu));
}

void draw(Canvas& c, const Sprite& s, float cx, float cy, const Grid& g, const Look& k, uint32_t now) {
    if (s.w <= 0 || g.f <= 0 || k.alpha <= 0) return;
    const int f = g.f;
    auto snap = [&](float v, int o) { return o + int(std::floor((v - float(o)) / float(f) + 0.5f)) * f; };
    const int x0 = snap(cx - float(s.w * f) / 2.f, g.x), y0 = snap(cy - float(s.h * f) / 2.f, g.y);
    auto block = [&](int sx, int sy, uint32_t col, int a) {  // sprite pixel (before mirroring)
        const int px = x0 + (k.faceLeft ? s.w - 1 - sx : sx) * f, py = y0 + sy * f;
        for (int yy = 0; yy < f; ++yy) c.pxSpan(px, px + f, py + yy, col, a);
    };
    // which windows are dark this moment (each blinks on its own beat)
    const uint32_t beat = now / 700u;
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x) {
            uint32_t col = s.px[size_t(y) * size_t(s.w) + size_t(x)];
            if (!col) continue;
            col &= 0xFFFFFFu;
            if (k.whiten > 0) col = mixc(col, 0xFFFFFF, float(k.whiten) / 255.f);
            block(x, y, col, k.alpha);
        }
    for (int idx : s.lights) {
        const uint32_t h = hash32(uint32_t(idx) * 31u + k.seed + beat * 7919u);
        if ((h & 7u) == 0) block(idx % s.w, idx / s.w, 0x6A5A30u, k.alpha);  // off for a moment
        else if ((h & 7u) == 1) block(idx % s.w, idx / s.w, 0xFFFFFFu, k.alpha);
    }
    // engine flames: 2-5 pixels behind each nozzle, flickering (white-yellow, orange, red)
    if (k.engines)
        for (size_t n = 0; n < s.nozzles.size(); ++n) {
            const Sprite::Nozzle& z = s.nozzles[n];
            const uint32_t h = hash32(uint32_t(now / 90u) * 131u + k.seed + uint32_t(n) * 977u);
            const int rows = z.y1 - z.y0 + 1;
            const int len = std::max(1, s.w / 24) + int(h % 3u) + rows / 3;  // (in proportion to the ship)
            for (int y = z.y0; y <= z.y1; ++y) {
                const bool edge = rows > 2 && (y == z.y0 || y == z.y1);
                const int l = edge ? len / 2 : len;
                for (int i = 1; i <= l; ++i) {
                    const float u = float(i) / float(std::max(1, l));
                    const uint32_t col = u < 0.34f ? 0xFFF4C8u : u < 0.67f ? 0xFFA040u : 0xD8401Cu;
                    block(z.x - i, y, col, k.alpha * (i == l ? 150 : 235) / 255);
                }
            }
        }
    // a damaged ship burns at the breach: flickering fire pixels and a dark scorch
    if (k.hurt && !s.hull.empty()) {
        const int bx = std::max(1, std::min(s.w - 2, k.hurtX)), by = std::max(1, std::min(s.h - 2, k.hurtY));
        for (int i = 0; i < 9; ++i) {
            const uint32_t h = hash32(uint32_t(i) * 53u + k.seed);
            const int dx = int(h % 5u) - 2, dy = int((h >> 4) % 3u) - 1;
            const int idx = (by + dy) * s.w + (bx + dx);
            if (idx < 0 || idx >= int(s.px.size()) || !s.px[size_t(idx)]) continue;
            const uint32_t fl = hash32(h + now / 110u);
            const uint32_t col = i < 3 ? 0x2A1810u : (fl & 3u) == 0 ? 0xFFF0B0u : (fl & 3u) == 1 ? 0xFF8A30u : (fl & 3u) == 2 ? 0xE04018u : 0x3A2418u;
            block(bx + dx, by + dy, col, k.alpha);
        }
    }
}

const Sprite* fit(const Canvas& c, const Grid& g, UnitType t, float maxW, float maxH, uint32_t col) {
    if (g.f <= 0) return nullptr;
    const char* name = pictureOf(t);
    const Image* im = name ? icons::image(name) : nullptr;
    if (!im || im->w <= 0 || im->h <= 0) return nullptr;
    const float iw = float(im->w), ih = float(im->h);
    const float size = im->w >= im->h ? std::min(maxW, maxH * iw / ih) : std::min(maxH, maxW * ih / iw);
    const float width = im->w >= im->h ? size : size * iw / ih;
    return get(t, std::max(6, int(std::lround(c.fx(width) / float(g.f)))), col);
}

void drawOn(Canvas& c, const Sprite& s, float cx, float footY, const Grid& g, const Look& k, uint32_t now) {
    draw(c, s, cx, footY - float(s.h * g.f) / 2.f + float(g.f), g, k, now);  // (the outline row sits below the feet)
}

const Pixelized* pixelize(const Canvas& c, const Grid& g, uint64_t key, float left, float top, float w, float h,
                          const std::function<void(Canvas&, float, float)>& draw, uint32_t now) {
    if (g.f <= 0 || w <= 0.f || h <= 0.f) return nullptr;
    static std::unordered_map<uint64_t, Pixelized> cache;
    key = key * 1099511628211ull ^ uint64_t(g.f) * 977u ^ uint64_t(c.scale() * 1000.f);
    auto it = cache.find(key);
    if (it != cache.end()) {
        it->second.used = now;
        return &it->second;
    }
    if (cache.size() > 2500)  // forget pictures not drawn for a while
        for (auto e = cache.begin(); e != cache.end();) {
            if (int32_t(now - e->second.used) > 1500) e = cache.erase(e);
            else ++e;
        }
    constexpr int ss = 4;                                  // canvas pixels per grid pixel
    const float gpu = c.scale() / float(g.f);              // grid pixels per game unit
    const int gw = std::max(1, int(std::ceil(w * gpu))), gh = std::max(1, int(std::ceil(h * gpu)));
    const float offScale = float(ss) * gpu;
    // the canvas: whole game units, at least gw * ss x gh * ss pixels
    const int wu = int(std::ceil(float(gw * ss) / offScale)) + 1, hu = int(std::ceil(float(gh * ss) / offScale)) + 1;
    static std::unique_ptr<Canvas> off;
    if (!off || off->width() != wu || off->height() != hu || std::fabs(off->scale() - offScale) > 1e-4f) off = std::make_unique<Canvas>(wu, hu, offScale);
    Canvas& o = *off;
    const int pw = o.pw();
    std::vector<uint32_t> blackPx(size_t(pw) * size_t(o.ph()));
    o.resetClip();
    o.clear(0x000000);
    draw(o, -left, -top);
    std::copy(o.pixels(), o.pixels() + blackPx.size(), blackPx.begin());
    o.clear(0xFFFFFF);
    draw(o, -left, -top);
    const uint32_t* whitePx = o.pixels();
    Pixelized out;
    out.used = now;
    out.ax = int(std::floor(-left * gpu));
    out.ay = int(std::floor(-top * gpu));
    // cut down: alpha from the two backgrounds, colour from the black one
    const int W = gw + 2, H = gh + 2;  // (a pixel of room for the outline)
    std::vector<float> A(size_t(W) * size_t(H), 0.f);
    std::vector<uint32_t> C(size_t(W) * size_t(H), 0);
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            float sa = 0, sr = 0, sg = 0, sb = 0;
            for (int yy = y * ss; yy < y * ss + ss && yy < o.ph(); ++yy)
                for (int xx = x * ss; xx < x * ss + ss && xx < pw; ++xx) {
                    const uint32_t b = blackPx[size_t(yy) * size_t(pw) + size_t(xx)], wh = whitePx[size_t(yy) * size_t(pw) + size_t(xx)];
                    const float br = float((b >> 16) & 255), bg = float((b >> 8) & 255), bb = float(b & 255);
                    const float diff = (float((wh >> 16) & 255) - br + float((wh >> 8) & 255) - bg + float(wh & 255) - bb) / (3.f * 255.f);
                    const float a = std::max(0.f, std::min(1.f, 1.f - diff));
                    sa += a;
                    sr += br;
                    sg += bg;
                    sb += bb;
                }
            const float a = sa / float(ss * ss);
            A[size_t(y + 1) * size_t(W) + size_t(x + 1)] = a;
            if (sa > 0.01f) {
                auto q = [&](float v) {  // unpremultiplied, posterised to 8 levels
                    const int u = std::max(0, std::min(255, int(v / sa)));
                    return std::min(255, (u + 18) / 36 * 36 + (u >= 236 ? 3 : 0));
                };
                C[size_t(y + 1) * size_t(W) + size_t(x + 1)] = uint32_t(q(sr) << 16 | q(sg) << 8 | q(sb));
            }
        }
    Sprite& s = out.s;
    s.w = W;
    s.h = H;
    s.px.assign(size_t(W) * size_t(H), 0);
    std::vector<uint8_t> solid(size_t(W) * size_t(H), 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = size_t(y) * size_t(W) + size_t(x);
            if (A[i] >= 0.5f) {
                solid[i] = 1;
                s.px[i] = 0xFF000000u | C[i];
            } else if (A[i] >= 0.12f) {  // faint (shadows, glows, smoke): a see-through pixel, its alpha in three steps
                const uint32_t a = A[i] < 0.22f ? 80u : A[i] < 0.36f ? 130u : 180u;
                s.px[i] = a << 24 | C[i];
            }
        }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = size_t(y) * size_t(W) + size_t(x);
            if (solid[i]) continue;
            uint32_t nb = 0;
            int n = 0;
            for (const auto& d : {std::make_pair(1, 0), std::make_pair(-1, 0), std::make_pair(0, 1), std::make_pair(0, -1)}) {
                const int xx = x + d.first, yy = y + d.second;
                if (xx < 0 || yy < 0 || xx >= W || yy >= H || !solid[size_t(yy) * size_t(W) + size_t(xx)]) continue;
                nb = C[size_t(yy) * size_t(W) + size_t(xx)];
                ++n;
            }
            if (n) s.px[i] = 0xFF000000u | mixc(nb, 0x000000, 0.8f);
        }
    out.ax += 1;  // (the outline's room)
    out.ay += 1;
    return &(cache[key] = std::move(out));
}

void drawPixelized(Canvas& c, const Pixelized& p, float x, float y, const Grid& g, int alpha) {
    if (alpha <= 0 || g.f <= 0) return;
    const int f = g.f;
    const int x0 = g.x + int(std::floor((x - float(g.x)) / float(f))) * f - p.ax * f;
    const int y0 = g.y + int(std::floor((y - float(g.y)) / float(f))) * f - p.ay * f;
    const Sprite& s = p.s;
    for (int sy = 0; sy < s.h; ++sy) {
        const uint32_t* row = &s.px[size_t(sy) * size_t(s.w)];
        for (int sx = 0; sx < s.w;) {
            if (!row[sx]) {
                ++sx;
                continue;
            }
            int e = sx + 1;
            while (e < s.w && row[e] == row[sx]) ++e;  // a run of one colour: one span per row of the block
            const int a = alpha * int(row[sx] >> 24) / 255;
            for (int yy = 0; yy < f; ++yy) c.pxSpan(x0 + sx * f, x0 + e * f, y0 + sy * f + yy, row[sx] & 0xFFFFFFu, a);
            sx = e;
        }
    }
}

uint32_t hash(uint32_t h) { return hash32(h); }
float dith(int gx, int gy) {
    static const uint8_t m[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return (float(m[(gy & 3) * 4 + (gx & 3)]) + 0.5f) / 16.f;
}
void dot(Canvas& c, const Grid& g, int gx, int gy, uint32_t col, int a) {
    const int px = g.x + gx * g.f, py = g.y + gy * g.f;
    for (int yy = 0; yy < g.f; ++yy) c.pxSpan(px, px + g.f, py + yy, col, a);
}
void fireball(Canvas& c, const Grid& g, int cx, int cy, float rr, float k, uint32_t seed, uint32_t now) {
    const int n = int(std::ceil(rr * 1.25f)) + 1;
    const uint32_t fr = now / 70u;
    for (int dy = -n; dy <= n; ++dy)
        for (int dx = -n; dx <= n; ++dx) {
            const uint32_t h = hash32(uint32_t(dx) * 73856093u ^ uint32_t(dy) * 19349663u ^ (seed + fr * 83492791u));
            float d = std::sqrt(float(dx * dx + dy * dy)) / std::max(0.5f, rr);
            d += (float(h % 1000u) / 1000.f - 0.5f) * 0.38f;
            if (d > 1.12f) continue;
            if (dith(cx + dx, cy + dy) < k * k * 1.15f - 0.05f) continue;  // breaking up
            const float heat = (1.f - d) * (1.25f - k);
            uint32_t col;
            if (heat > 0.72f) col = 0xFFFAE0u;
            else if (heat > 0.5f) col = 0xFFD660u;
            else if (heat > 0.3f) col = 0xFF842Au;
            else if (heat > 0.12f) col = 0xCE3A1Eu;
            else col = k > 0.45f ? 0x423A3Cu : 0x8C281Au;
            dot(c, g, cx + dx, cy + dy, col);
        }
}
void flash(Canvas& c, const Grid& g, int cx, int cy, float r, float k) {
    const float rr = r * (0.6f + 0.8f * std::sqrt(k));
    const float hole = rr * std::min(1.f, k * 1.25f);
    const int n = int(std::ceil(rr)) + 1;
    for (int dy = -n; dy <= n; ++dy)
        for (int dx = -n; dx <= n; ++dx) {
            const float d = std::sqrt(float(dx * dx + dy * dy));
            if (d > rr || d < hole) continue;
            const float u = (d - hole) / std::max(0.5f, rr - hole);
            dot(c, g, cx + dx, cy + dy, k < 0.25f || u > 0.6f ? 0xFFFFFFu : 0xBEDCFFu);
        }
}
void muzzle(Canvas& c, const Grid& g, int cx, int cy, float r, float k) {
    const int arm = std::max(1, int(std::lround(r * (k < 0.5f ? 1.4f : 0.8f))));
    dot(c, g, cx, cy, 0xFFFFF0u);
    for (int i = 1; i <= arm; ++i) {
        const uint32_t col = i == arm ? 0xFFAA46u : 0xFFE28Cu;
        dot(c, g, cx + i, cy, col);
        dot(c, g, cx - i, cy, col);
        if (i < arm) {
            dot(c, g, cx, cy + i, col);
            dot(c, g, cx, cy - i, col);
        }
    }
}
void puff(Canvas& c, const Grid& g, int cx, int cy, float rr, float fade, uint32_t seed, uint32_t col, int a) {
    rr = std::max(0.6f, rr);
    const int n = int(std::ceil(rr));
    for (int dy = -n; dy <= n; ++dy)
        for (int dx = -n; dx <= n; ++dx) {
            const float d = std::sqrt(float(dx * dx + dy * dy)) / rr;
            const float jag = float(hash32(seed + uint32_t(dx) * 31u + uint32_t(dy) * 977u) % 1000u) / 1000.f;
            if (d > 1.f || d + jag * 0.45f > 0.35f + 0.9f * fade) continue;
            dot(c, g, cx + dx, cy + dy, d < 0.4f ? mixc(col, 0xFFFFFF, 0.16f) : d > 0.75f ? mixc(col, 0x000000, 0.2f) : col, a);
        }
}
void spark(Canvas& c, const Grid& g, int x, int y, float vx, float vy, uint32_t tail, float k) {
    if (dith(x, y) < k * k) return;
    dot(c, g, x, y, 0xFFFAD2u);
    const int tx = vx > 0.05f ? -1 : vx < -0.05f ? 1 : 0, ty = vy > 0.05f ? -1 : vy < -0.05f ? 1 : 0;
    if (tx || ty) dot(c, g, x + tx, y + ty, tail);
}

}  // namespace pxship
