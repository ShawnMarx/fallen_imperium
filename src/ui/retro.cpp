#include "ui/retro.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/log.h"
#include "engine/data.h"
#include "ui/voidfall.h"

namespace retro {

namespace {

constexpr float kPi = 3.14159265358979f;
// The art canvas: corner radius 48 design pixels (96 x 84); the sharp PC art (0.48.0, assets/retro-pc.dat) has kK = 2
// pixels per design pixel (192 x 168). Set by pack() for the pack in use; pieces stay in design pixels (kArtR).
int kW = 96, kH = 84, kK = 1;

const uint8_t kBayer[8][8] = {{0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26}, {12, 44, 4, 36, 14, 46, 6, 38},
                              {60, 28, 52, 20, 62, 30, 54, 22}, {3, 35, 11, 43, 1, 33, 9, 41},   {51, 19, 59, 27, 49, 17, 57, 25},
                              {15, 47, 7, 39, 13, 45, 5, 37},   {63, 31, 55, 23, 61, 29, 53, 21}};
inline float bayer(int x, int y) { return (float(kBayer[y & 7][x & 7]) + 0.5f) / 64.f; }

struct Anim {
    std::string name;
    int w = 0, h = 0, frames = 0, ms = 100;
    std::vector<uint16_t> local;   // local colour -> palette index (local 0 = transparent)
    std::vector<uint32_t> offs;
    std::vector<uint8_t> data;
    int cur = -1;                  // the decoded frame in buf
    std::vector<uint8_t> buf;
    int gcur = -1;                 // ... and in palette indices (gbuf)
    std::vector<uint16_t> gbuf;
    const uint16_t* global(int k) {
        const uint8_t* f = frame(k);
        if (gcur != cur || gbuf.size() != buf.size()) {
            gbuf.resize(buf.size());
            for (size_t i = 0; i < buf.size(); ++i) gbuf[i] = local[f[i]];
            gcur = cur;
        }
        return gbuf.data();
    }

    size_t end(int k) const { return k + 1 < frames ? offs[size_t(k + 1)] : data.size(); }
    const uint8_t* frame(int k) {
        k = ((k % frames) + frames) % frames;
        if (k == cur) return buf.data();
        if (cur < 0 || k < cur) {  // from the key frame
            buf.assign(size_t(w) * size_t(h), 0);
            size_t p = offs[0], e = end(0), o = 0;
            while (p < e && o < buf.size()) {
                const uint8_t c = data[p++];
                if (c < 127) {
                    const size_t n = size_t(c) + 1;
                    for (size_t i = 0; i < n && p < e && o < buf.size(); ++i) buf[o++] = data[p++];
                } else {
                    const size_t n = size_t(c) - 126;
                    const uint8_t v = p < e ? data[p++] : 0;
                    for (size_t i = 0; i < n && o < buf.size(); ++i) buf[o++] = v;
                }
            }
            cur = 0;
        }
        while (cur < k) {
            ++cur;
            size_t p = offs[size_t(cur)], e = end(cur), o = 0;
            auto varint = [&]() {
                size_t v = 0;
                int s = 0;
                while (p < e) {
                    const uint8_t b = data[p++];
                    v |= size_t(b & 0x7F) << s;
                    if (!(b & 0x80)) break;
                    s += 7;
                }
                return v;
            };
            while (p < e) {
                o += varint();
                size_t n = varint();
                for (size_t i = 0; i < n && p < e; ++i, ++o)
                    if (o < buf.size()) buf[o] = data[p++];
                    else ++p;
            }
        }
        return buf.data();
    }
};

struct PlanetTex {
    uint8_t atmo = 255, cloud = 255, rim = 255;
    float ambient = 0.14f, gain = 1.f;
    uint32_t periodMs = 20000;
    int tw = 64, th = 32;
    std::vector<uint8_t> tex;  // ramp | flags << 6, level, extra
};

struct Pack {
    bool tried = false, ok = false;
    int k = 1;                                // pixels per design pixel (from the starfields' width)
    std::vector<uint32_t> pal;                // palette index -> colour
    std::vector<std::vector<uint16_t>> ramps; // ramp -> palette indices
    std::vector<std::string> rampNames;
    std::vector<Anim> anims;
    std::unordered_map<std::string, int> animIndex;
    std::vector<PlanetTex> planets;           // by kPlanets index
    std::vector<bool> hasPlanet;
};

struct Reader {
    const std::vector<uint8_t>& d;
    size_t p = 0;
    bool bad = false;
    explicit Reader(const std::vector<uint8_t>& v) : d(v) {}
    bool need(size_t n) {
        if (p + n > d.size()) bad = true;
        return !bad;
    }
    uint8_t u8() { return need(1) ? d[p++] : 0; }
    uint16_t u16() {
        if (!need(2)) return 0;
        uint16_t v = uint16_t(d[p] | (d[p + 1] << 8));
        p += 2;
        return v;
    }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = uint32_t(d[p]) | (uint32_t(d[p + 1]) << 8) | (uint32_t(d[p + 2]) << 16) | (uint32_t(d[p + 3]) << 24);
        p += 4;
        return v;
    }
    std::string str() {
        const size_t n = u8();
        if (!need(n)) return "";
        std::string s(reinterpret_cast<const char*>(d.data() + p), n);
        p += n;
        return s;
    }
};

bool loadFile(const char* path, Pack& pk) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    if (d.size() < 8 || std::memcmp(d.data(), "RTW1", 4) != 0) return false;
    Reader r(d);
    r.p = 4;
    const int np = r.u16();
    pk.pal.resize(size_t(np));
    for (int i = 0; i < np; ++i) {
        const int rr = r.u8(), gg = r.u8(), bb = r.u8();
        pk.pal[size_t(i)] = rgb(rr, gg, bb);
    }
    const int nr = r.u8();
    for (int i = 0; i < nr; ++i) {
        pk.rampNames.push_back(r.str());
        const int n = r.u8();
        std::vector<uint16_t> ids;
        for (int k = 0; k < n; ++k) ids.push_back(r.u16());
        pk.ramps.push_back(ids);
    }
    const int na = r.u16();
    for (int i = 0; i < na && !r.bad; ++i) {
        Anim a;
        a.name = r.str();
        a.w = r.u16();
        a.h = r.u16();
        a.frames = r.u16();
        a.ms = r.u16();
        const int nl = r.u8();
        a.local.assign(1, 0);
        for (int k = 0; k < nl; ++k) a.local.push_back(r.u16());
        for (int k = 0; k < a.frames; ++k) a.offs.push_back(r.u32());
        const uint32_t len = r.u32();
        if (!r.need(len)) break;
        a.data.assign(d.begin() + long(r.p), d.begin() + long(r.p + len));
        r.p += len;
        if (a.frames <= 0 || a.w <= 0 || a.h <= 0) continue;
        pk.animIndex[a.name] = int(pk.anims.size());
        pk.anims.push_back(std::move(a));
    }
    std::unordered_map<std::string, int> byKey;
    for (int i = 0; i < ti::kPlanetCount; ++i) byKey[ti::kPlanets[i].key] = i;
    pk.planets.assign(size_t(ti::kPlanetCount), PlanetTex());
    pk.hasPlanet.assign(size_t(ti::kPlanetCount), false);
    const int npl = r.u16();
    for (int i = 0; i < npl && !r.bad; ++i) {
        const std::string key = r.str();
        PlanetTex t;
        t.atmo = r.u8();
        t.cloud = r.u8();
        t.rim = r.u8();
        t.ambient = float(r.u8()) / 100.f;
        t.gain = float(r.u8()) / 100.f;
        t.periodMs = uint32_t(r.u16()) * 100u;
        t.tw = r.u8();
        t.th = r.u8();
        const size_t n = size_t(t.tw) * size_t(t.th) * 3u;
        if (!r.need(n)) break;
        t.tex.assign(d.begin() + long(r.p), d.begin() + long(r.p + n));
        r.p += n;
        auto it = byKey.find(key);
        if (it != byKey.end()) {
            pk.planets[size_t(it->second)] = std::move(t);
            pk.hasPlanet[size_t(it->second)] = true;
        }
    }
    return !r.bad && !pk.anims.empty() && !pk.ramps.empty();
}

// which = 0: retro.dat (the 640 x 480 art), 1: retro-pc.dat (0.48.0: twice as many pixels, for the sharp PC picture)
Pack& loadPack(int which) {
    static Pack packs[2];
    Pack& p = packs[which];
    if (!p.tried) {
        p.tried = true;
        const char* const paths[2][3] = {{"retro.dat", "assets/retro.dat", "../assets/retro.dat"},
                                         {"retro-pc.dat", "assets/retro-pc.dat", "../assets/retro-pc.dat"}};
        for (const char* path : paths[which]) {
            Pack q;
            if (loadFile(path, q)) {
                q.tried = q.ok = true;
                for (const Anim& a : q.anims)
                    if (a.name == "star0") q.k = std::max(1, a.w / 96);
                p = std::move(q);
                LOG("retro: %d animations, %d ramps, %dx art from %s", int(p.anims.size()), int(p.ramps.size()), p.k, path);
                break;
            }
        }
        if (!p.ok) LOG("retro: %s not found", which ? "retro-pc.dat" : "retro.dat - only the Simplistic tileset");
        if (p.ok && which == 1) {  // the two packs must list the same pieces in the same order (ids are shared)
            Pack& base = loadPack(0);
            bool same = base.ok && base.anims.size() == p.anims.size() && base.ramps.size() == p.ramps.size() && base.pal == p.pal;
            for (size_t i = 0; same && i < p.anims.size(); ++i) same = base.anims[i].name == p.anims[i].name;
            if (!same) {
                LOG("retro: retro-pc.dat doesn't match retro.dat - not used");
                p.ok = false;
            }
        }
    }
    return p;
}

// The pack for the picture in use: the sharp art when the screen is enlarged 1.5x or more (TI4_RETROK=1 / 2 forces one).
Pack& pack() {
    static int forced = -1;
    if (forced < 0) {
        const char* e = std::getenv("TI4_RETROK");
        forced = e ? std::atoi(e) : 0;
    }
    const bool sharp = forced ? forced >= 2 : Canvas::screenScale() >= 1.5f;
    Pack* p = &loadPack(0);
    if (sharp || !p->ok) {
        Pack& hi = loadPack(1);
        if (hi.ok) p = &hi;
    }
    if (p->ok && p->k != kK) {
        kK = p->k;
        kW = 96 * kK;
        kH = 84 * kK;
    }
    return *p;
}

int g_tileset = 0;  // setTileset (0.32.0: three tilesets)

int animId(const char* name) {
    Pack& p = pack();
    auto it = p.animIndex.find(name);
    return it == p.animIndex.end() ? -1 : it->second;
}

int rampId(const char* name) {
    Pack& p = pack();
    for (size_t i = 0; i < p.rampNames.size(); ++i)
        if (p.rampNames[i] == name) return int(i);
    return -1;
}

// one shaded pixel: level v (0..1) on a ramp, dithered (as gen_tile_art's Cv.shade)
inline uint16_t shadeIdx(const std::vector<uint16_t>& ramp, float v, float b) {
    const int n = int(ramp.size());
    if (n <= 0) return 0;
    v = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    int k = int(std::floor(v * float(n - 1) + (b - 0.5f) + 0.5f));
    k = k < 0 ? 0 : k >= n ? n - 1 : k;
    return ramp[size_t(k)];
}

// ------------------------------------------------------------------ planets on their spheres
const float kLight[3] = {-0.5939f, -0.4981f, 0.5556f};  // gen_tile_art's LIGHT, normalised
constexpr float kTilt = 0.35f;

struct SpherePx {
    int8_t ox, oy;
    uint16_t u;      // texel column at rotation 0, 8.8 fixed
    uint8_t v;       // texel row
    uint8_t lam;     // lambert 0..255
    uint8_t lit;     // min(1, 1.12 lam) ^ 0.85, 0..255 (gen_tile_art's lit() without the ambient)
    uint8_t d2;      // distance from the centre squared 0..255 (1 = the limb)
};

// x ^ 0.85 from square roots (no pow: powf would need a newer glibc than the Miyoo has)
float pow085(float x) {
    if (x <= 0.f) return 0.f;
    const float s1 = std::sqrt(x), s2 = std::sqrt(s1), s3 = std::sqrt(s2), s4 = std::sqrt(s3), s5 = std::sqrt(s4);
    const float s8 = std::sqrt(std::sqrt(std::sqrt(s5)));
    return s1 * s2 * s4 * s5 * s8;  // x^(1/2 + 1/4 + 1/16 + 1/32 + 1/256)
}
struct AtmoPx {
    int8_t ox, oy;
    uint8_t lam;     // light on the rim 0..255
};
struct SphereLut {
    std::vector<SpherePx> px;
    std::vector<AtmoPx> atmo;
};

const SphereLut& sphereLut(float r, int tw, int th) {
    static std::unordered_map<int, SphereLut> cache;
    const int key = (int(r * 4.f + 0.5f) * 256 + tw) * 256 + th;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    SphereLut& L = cache[key];
    r = float(int(r * 4.f + 0.5f)) / 4.f;
    const float ct = std::cos(kTilt), st = std::sin(kTilt);
    const int R = int(std::ceil(r + 2.f));
    const float aw = 1.6f / std::max(1.f, r);
    for (int oy = -R; oy <= R; ++oy)
        for (int ox = -R; ox <= R; ++ox) {
            const float dx = float(ox) / r, dy = float(oy) / r;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= 1.f) {
                const float nz = std::sqrt(std::max(0.f, 1.f - d2));
                const float lam = std::max(0.f, std::min(1.f, dx * kLight[0] + dy * kLight[1] + nz * kLight[2]));
                const float x1 = dx * ct + dy * st, y1 = -dx * st + dy * ct;
                const float lon = std::atan2(x1, nz);                     // -pi..pi
                const float lat = std::asin(std::max(-1.f, std::min(1.f, y1)));
                float u = (lon / (2.f * kPi) + 0.5f) * float(tw);
                int v = int((lat / kPi + 0.5f) * float(th));
                v = std::max(0, std::min(th - 1, v));
                SpherePx p;
                p.ox = int8_t(ox);
                p.oy = int8_t(oy);
                p.u = uint16_t(int(u * 256.f) & 0xFFFF);
                p.v = uint8_t(v);
                p.lam = uint8_t(lam * 255.f);
                p.lit = uint8_t(pow085(std::min(1.f, lam * 1.12f)) * 255.f);
                p.d2 = uint8_t(std::min(255.f, d2 * 255.f));
                L.px.push_back(p);
            } else {
                const float d = std::sqrt(d2);
                if (d > 1.f + aw) continue;
                float lam = (dx * kLight[0] + dy * kLight[1]) / std::max(1e-6f, d) * 0.9f + 0.25f;
                lam = std::max(0.f, std::min(1.f, lam));
                L.atmo.push_back({int8_t(ox), int8_t(oy), uint8_t(lam * 255.f)});
            }
        }
    return L;
}

void drawPlanetArt(uint16_t* buf, int bw, int bh, int x0, int y0, const Piece& pc, uint32_t now) {
    Pack& pk = pack();
    if (pc.id < 0 || pc.id >= int(pk.planets.size()) || !pk.hasPlanet[size_t(pc.id)]) return;
    const PlanetTex& T = pk.planets[size_t(pc.id)];
    const int cx = x0 + int(std::floor(pc.x + float(bw) / 2.f)), cy = y0 + int(std::floor(pc.y + float(bh) / 2.f));
    const SphereLut& L = sphereLut(std::max(2.f, pc.r), T.tw, T.th);
    const uint32_t period = std::max(1000u, T.periodMs);
    const int rot = int(uint64_t(now % period) * uint64_t(T.tw) * 256u / period);
    const int rotC = int(uint64_t(now % (period * 5 / 8)) * uint64_t(T.tw) * 256u / (period * 5 / 8));  // clouds turn faster
    const float t2 = float(now % 6400) / 6400.f;  // the loop of the glow and lights
    const int twMask = T.tw - 1;
    const std::vector<uint16_t>* city = nullptr;
    {
        static int cityR = -2;
        if (cityR == -2) cityR = rampId("city");
        if (cityR >= 0) city = &pk.ramps[size_t(cityR)];
    }
    const float dimK = pc.dim ? 0.55f : 1.f;
    for (const SpherePx& p : L.px) {
        const int x = cx + p.ox, y = cy + p.oy;
        if (x < 0 || y < 0 || x >= bw || y >= bh) continue;
        const int u = ((int(p.u) + rot) >> 8) & twMask;
        const uint8_t* tx = &T.tex[(size_t(p.v) * size_t(T.tw) + size_t(u)) * 3u];
        const int ramp = tx[0] & 63, flags = tx[0] >> 6;
        const float level = float(tx[1]) / 255.f, lam = float(p.lam) / 255.f;
        const float b = bayer(x, y);
        const float lit = (T.ambient + (1.f - T.ambient) * float(p.lit) / 255.f) * T.gain;
        uint16_t out = 0;
        if (T.rim != 255 && p.d2 > 153 && lam > 0.35f) {
            out = shadeIdx(pk.ramps[T.rim], (0.45f + 0.5f * lam) * dimK, b);
        } else if (flags == 1) {  // glow: lava cracks, ice veins - not lit, pulsing
            const float ph = float(tx[2]) / 255.f * 2.f * kPi;
            out = shadeIdx(pk.ramps[size_t(ramp)], (level + 0.27f * std::cos(2.f * kPi * 2.f * t2 + ph)) * dimK, b);
        } else if (flags == 2 && lam < 0.2f && city) {  // city lights on the night side
            const float ph = float(tx[2]) / 255.f * 2.f * kPi;
            out = shadeIdx(*city, (0.5f + 0.3f * std::cos(2.f * kPi * 3.f * t2 + ph)) * (1.f - lam * 2.5f) * dimK, b);
        } else {
            out = shadeIdx(pk.ramps[size_t(ramp)], level * lit * dimK, b);
        }
        if (T.cloud != 255) {
            const int uc = ((int(p.u) + rotC) >> 8) & twMask;
            const float cl = float(T.tex[(size_t(p.v) * size_t(T.tw) + size_t(uc)) * 3u + 2u]) / 255.f;
            // thin, dithered clouds: the surface shows through their edges
            if (cl > 0.f && std::min(1.f, cl * 1.6f) * 0.85f > b) out = shadeIdx(pk.ramps[T.cloud], (0.4f + 0.5f * cl) * lit * dimK, b);
        }
        buf[size_t(y) * size_t(bw) + size_t(x)] = out;
    }
    if (T.atmo != 255)
        for (const AtmoPx& a : L.atmo) {
            const int x = cx + a.ox, y = cy + a.oy;
            if (x < 0 || y < 0 || x >= bw || y >= bh) continue;
            const float lam = float(a.lam) / 255.f, b = bayer(x, y);
            if ((0.25f + 0.75f * lam) * dimK <= b) continue;
            buf[size_t(y) * size_t(bw) + size_t(x)] = shadeIdx(pk.ramps[T.atmo], 0.35f + 0.6f * lam, b);
        }
}

// an animated sprite (wormhole, token) into the art buffer, scaled to radius r
void drawSprite(uint16_t* buf, int bw, int bh, int x0, int y0, int anim, float px, float py, float scale, uint32_t now) {
    Pack& pk = pack();
    if (anim < 0) return;
    Anim& a = pk.anims[size_t(anim)];
    const uint8_t* f = a.frame(int(now / uint32_t(std::max(1, a.ms))));
    const float hw = float(a.w) * scale / 2.f, hh = float(a.h) * scale / 2.f;
    const float cx = px + float(bw) / 2.f + float(x0), cy = py + float(bh) / 2.f + float(y0);
    const int xa = std::max(0, int(std::floor(cx - hw))), xb = std::min(bw, int(std::ceil(cx + hw)));
    const int ya = std::max(0, int(std::floor(cy - hh))), yb = std::min(bh, int(std::ceil(cy + hh)));
    for (int y = ya; y < yb; ++y) {
        const int sy = int((float(y) + 0.5f - (cy - hh)) / scale);
        if (sy < 0 || sy >= a.h) continue;
        for (int x = xa; x < xb; ++x) {
            const int sx = int((float(x) + 0.5f - (cx - hw)) / scale);
            if (sx < 0 || sx >= a.w) continue;
            const uint8_t l = f[size_t(sy) * size_t(a.w) + size_t(sx)];
            if (l) buf[size_t(y) * size_t(bw) + size_t(x)] = a.local[l];
        }
    }
}

// hyperlanes: neon lines from edge to edge, a spark running along each
void drawLanes(uint16_t* buf, uint32_t lanes, uint32_t now) {
    Pack& pk = pack();
    static int rc = -2, rp = -2;
    if (rc == -2) {
        rc = rampId("neonc");
        rp = rampId("neonp");
    }
    if (rc < 0 || rp < 0) return;
    struct Seg {
        float ax, ay, bx, by;
    };
    std::vector<Seg> segs;
    for (int a = 0; a < 6; ++a)
        for (int b = a + 1; b < 6; ++b) {
            if (!((lanes >> (a * 6 + b)) & 1)) continue;
            const float aa = -kPi / 2.f + float(a) * kPi / 3.f, ab = -kPi / 2.f + float(b) * kPi / 3.f, rr = kArtR * 0.866f;
            segs.push_back({rr * std::cos(aa), rr * std::sin(aa), rr * std::cos(ab), rr * std::sin(ab)});
        }
    const float t = float(now % 2400) / 2400.f;
    const float ik = 1.f / float(kK);  // (design pixels per pixel)
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            uint16_t& o = buf[size_t(y) * size_t(kW) + size_t(x)];
            if (!o) continue;
            const float X = (float(x) + 0.5f) * ik - 48.f, Y = (float(y) + 0.5f) * ik - 42.f;
            float best = 99.f, along = 0.f;
            for (const Seg& s : segs) {  // straight from edge to edge, as the Simplistic lanes
                const float wx = s.bx - s.ax, wy = s.by - s.ay;
                const float l2 = wx * wx + wy * wy;
                float u = l2 > 0 ? ((X - s.ax) * wx + (Y - s.ay) * wy) / l2 : 0.f;
                u = std::max(0.f, std::min(1.f, u));
                const float dx = X - (s.ax + u * wx), dy = Y - (s.ay + u * wy);
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d < best) {
                    best = d;
                    along = u;
                }
            }
            const float b = bayer(x, y);
            if (best < 1.1f) {
                float spark = std::fabs(along - t);
                spark = std::min(spark, 1.f - spark);
                o = shadeIdx(pk.ramps[size_t(rc)], spark < 0.04f ? 1.f : 0.82f, b);
            } else if (best < 2.6f) {
                if ((2.6f - best) / 1.5f * 0.85f > b) o = shadeIdx(pk.ramps[size_t(rc)], 0.5f, b);
            } else if (best < 4.2f) {
                if ((4.2f - best) / 1.6f * 0.45f > b) o = shadeIdx(pk.ramps[size_t(rp)], 0.35f, b);
            }
        }
}

constexpr uint32_t kClear = 0xFF000000u;  // a transparent pixel in a scaled picture

struct Scaled {
    int sizeKey = -1, frame = -1, w = 0, h = 0;
    std::vector<uint32_t> px;  // colours, kClear outside the tile
};
struct Comp {
    int frame = -1;
    uint32_t used = 0;
    std::vector<uint16_t> px;
    Scaled scaled[2];          // the tile at the sizes it was drawn at lately
};

// 0.44.3 (owner: the Voidfall paintings of the Fracture and Thunder's Edge showed in Retrowave): the Fracture's new
// design (shattered crimson glass) as Retrowave pixel art - the painting drawn at the art size and moved onto the
// Retrowave palette with the same ordered dither as the baked tiles; Retrowave's own planets go on top. Thunder's
// Edge keeps its Retrowave look of 0.28.1 (owner: "the old design with the storm clouds looks better")
bool paintedSky(const Tile& t) {
    if (t.def < 0 || t.def >= ti::kSystemCount) return false;
    const char* k = ti::kSystems[t.def].key;
    return !std::strncmp(k, "fracture", 8) && k[8] >= '1' && k[8] <= '7' && !k[9];
}

uint64_t tileKey(const Tile& t) {
    uint64_t h = 1469598103934665603ull;
    auto mixIn = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mixIn(uint64_t(t.bg + 7));
    if (paintedSky(t)) {
        mixIn(uint64_t(t.def + 11));
        mixIn(t.sky ? 77u : 78u);
    }
    mixIn(t.flip);
    mixIn(t.lanes);
    for (const Piece& p : t.pieces) {
        mixIn(uint64_t(p.kind) * 131 + uint64_t(p.id + 3));
        mixIn(uint64_t(int(p.x * 8.f) + 4096) * 8192 + uint64_t(int(p.y * 8.f) + 4096));
        mixIn(uint64_t(int(p.r * 8.f)) * 2 + (p.dim ? 1 : 0));
    }
    return h;
}

// Pale, writhing tentacles that come in from the cloud and wrap over the planet's face (art pixels, palette indices).
void cabalTentacles(uint16_t* buf, int bw, int bh, float px, float py, float pr, uint32_t now, float k) {
    Pack& pk = pack();
    static int skin = -2, dark = -2;
    if (skin == -2) {
        skin = rampId("lavender");
        dark = rampId("rift");
    }
    if (skin < 0 || dark < 0) return;
    const std::vector<uint16_t>& S = pk.ramps[size_t(skin)];
    const std::vector<uint16_t>& D = pk.ramps[size_t(dark)];
    const float t = float(now % 120000u) / 1000.f;
    const float cx = px + float(bw) / 2.f, cy = py + float(bh) / 2.f;
    // four arms: where they come in from, which way they sweep, how far round
    static const float kArm[5][3] = {{2.4f, 1.f, 2.4f}, {-0.6f, -1.f, 2.1f}, {0.9f, 1.f, 1.8f}, {3.9f, -1.f, 2.2f}, {5.3f, 1.f, 1.6f}};
    for (int k = 0; k < 5; ++k) {
        const float a0 = kArm[k][0] + 0.15f * std::sin(t * 0.5f + float(k)), dir = kArm[k][1], sweep = kArm[k][2];
        const int n = 34;
        for (int pass = 0; pass < 2; ++pass)  // the dark outline first, then the pale skin over it
            for (int i = 0; i < n; ++i) {
                const float s = float(i) / float(n - 1);
                const float wob = 0.12f * std::sin(t * 1.3f + s * 7.f + float(k) * 1.7f);
                const float th = a0 + dir * (sweep * s + wob);
                const float rad = pr * (1.95f - 1.0f * s + 0.07f * std::sin(t * 0.9f + s * 9.f));
                const float x = cx + std::cos(th) * rad, y = cy + std::sin(th) * rad * 0.92f;
                const float w = ((1.25f - 0.85f * s) + (pass == 0 ? 0.6f : 0.f)) * k;
                const int x0 = int(std::floor(x - w)), x1 = int(std::ceil(x + w)), y0 = int(std::floor(y - w)), y1 = int(std::ceil(y + w));
                for (int yy = y0; yy <= y1; ++yy)
                    for (int xx = x0; xx <= x1; ++xx) {
                        if (xx < 0 || yy < 0 || xx >= bw || yy >= bh) continue;
                        const float dx = float(xx) + 0.5f - x, dy = float(yy) + 0.5f - y;
                        if (dx * dx + dy * dy > w * w) continue;
                        uint16_t& o = buf[size_t(yy) * size_t(bw) + size_t(xx)];
                        if (o == 0) continue;  // outside the tile
                        const float b = bayer(xx, yy);
                        if (pass == 0) o = shadeIdx(D, 0.25f, b);
                        else {
                            const float lit = 0.45f + 0.45f * (-dx * 0.6f - dy * 0.8f) / std::max(0.5f * k, w) + 0.25f * (1.f - s);
                            o = shadeIdx(S, lit, b);
                        }
                    }
            }
    }
}

Comp& compose(const Tile& t, uint32_t now, uint32_t periodMs) {
    static std::unordered_map<uint64_t, Comp> cache;
    static uint32_t sweep = 0;
    Pack& pk = pack();
    const uint64_t key = tileKey(t) * 31u + uint64_t(kK);  // (0.48.0: per pack - the sharp art has more pixels)
    Comp& cp = cache[key];
    cp.used = now;
    // (background frames: now / 100 ms, as baked; 0.48.0: each tile on its own beat, so the map's tiles don't all
    // recompose in the same frame - that made a hitch every 100 ms)
    const int frame = int((now + uint32_t(key % periodMs)) / periodMs);
    if (cp.frame == frame && !cp.px.empty()) return cp;
    cp.frame = frame;
    cp.px.resize(size_t(kW) * size_t(kH));
    Anim* bga = t.bg >= 0 && t.bg < int(pk.anims.size()) ? &pk.anims[size_t(t.bg)] : nullptr;
    if (paintedSky(t) && voidfall::available()) {
        static std::unique_ptr<Canvas> artC;
        if (!artC || artC->width() != kW || artC->height() != kH) artC = std::make_unique<Canvas>(kW, kH, 1.f);
        Canvas& art = *artC;
        static std::vector<uint16_t> lut;  // nearest palette colour per 5-bit colour (as chunky)
        if (lut.empty()) {
            lut.resize(32768);
            for (int i = 0; i < 32768; ++i) {
                const int r = ((i >> 10) & 31) * 255 / 31, g = ((i >> 5) & 31) * 255 / 31, b = (i & 31) * 255 / 31;
                int best = 1, bd = 1 << 30;
                for (size_t k = 1; k < pk.pal.size(); ++k) {
                    const uint32_t q = pk.pal[k];
                    const int dr = r - int((q >> 16) & 255), dg = g - int((q >> 8) & 255), db = b - int(q & 255);
                    const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                    if (d < bd) {
                        bd = d;
                        best = int(k);
                    }
                }
                lut[size_t(i)] = uint16_t(best);
            }
        }
        constexpr uint32_t kOff = 0x00FF00FFu;  // (outside the hex)
        art.clear(kOff);
        Tile sky = t;
        sky.pieces.clear();  // (the planets: Retrowave's own, below)
        voidfall::draw(art, sky, float(kW) / 2.f, float(kH) / 2.f, kArtR * float(kK), now);
        const uint32_t* src = art.pixels();
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                const uint32_t col = src[size_t(y) * size_t(kW) + size_t(x)];
                if ((col & 0xFFFFFFu) == kOff) {
                    cp.px[size_t(y) * size_t(kW) + size_t(x)] = 0;
                    continue;
                }
                const int d = int(kBayer[y & 7][x & 7]) / 5 - 6;
                auto q = [&](int v) { v += d; return v < 0 ? 0 : v > 255 ? 31 : v * 31 / 255; };
                cp.px[size_t(y) * size_t(kW) + size_t(x)] = lut[size_t(q(int((col >> 16) & 255)) << 10 | q(int((col >> 8) & 255)) << 5 | q(int(col & 255)))];
            }
    } else if (bga && bga->w == kW && bga->h == kH) {
        const uint16_t* f = bga->global(int(now / 100u));
        for (int y = 0; y < kH; ++y) {
            const uint16_t* src = f + size_t((t.flip & 2) ? kH - 1 - y : y) * size_t(kW);
            uint16_t* dst = &cp.px[size_t(y) * size_t(kW)];
            if (t.flip & 1) std::reverse_copy(src, src + kW, dst);
            else std::copy(src, src + kW, dst);
        }
    } else {
        std::fill(cp.px.begin(), cp.px.end(), uint16_t(0));
    }
    if (t.lanes) drawLanes(cp.px.data(), t.lanes, now);
    static int wh[2][5] = {{-2}};
    static int ion[2] = {-2}, tear = -2;
    if (wh[0][0] == -2) {
        const char* k[5] = {"alpha", "beta", "gamma", "delta", "epsilon"};
        for (int i = 0; i < 5; ++i) {
            wh[0][i] = animId((std::string("wh_") + k[i] + "_s").c_str());
            wh[1][i] = animId((std::string("wh_") + k[i] + "_l").c_str());
        }
        ion[0] = animId("ion_alpha");
        ion[1] = animId("ion_beta");
        tear = animId("tear");
    }
    // wormholes and tokens first, planets over them (pieces are in design pixels: kK pixels each)
    const float fk = float(kK);
    for (const Piece& p : t.pieces) {
        if (p.kind == Piece::Wormhole && p.id >= 0 && p.id < 5) {
            const bool big = p.r >= 11.5f;
            drawSprite(cp.px.data(), kW, kH, 0, 0, wh[big ? 1 : 0][p.id], p.x * fk, p.y * fk, p.r / (big ? 14.f : 9.f), now);
        } else if (p.kind == Piece::IonStorm && p.id >= 0 && p.id < 2) {
            drawSprite(cp.px.data(), kW, kH, 0, 0, ion[p.id], p.x * fk, p.y * fk, p.r / 19.5f, now);
        } else if (p.kind == Piece::Tear) {
            drawSprite(cp.px.data(), kW, kH, 0, 0, tear, p.x * fk, p.y * fk, p.r / 19.5f, now);
        }
    }
    static int station[2] = {-2, -2};
    if (station[0] == -2) station[0] = animId("station"), station[1] = animId("station_dim");
    for (const Piece& p : t.pieces) {
        if (p.kind != Piece::Planet) continue;
        const bool isStation = p.id >= 0 && p.id < ti::kPlanetCount && (ti::kPlanets[p.id].flags & ti::kPlanetStation);
        if (isStation && station[p.dim ? 1 : 0] >= 0)  // Thunder's Edge (0.34.0): a space station, not a world
            drawSprite(cp.px.data(), kW, kH, 0, 0, station[p.dim ? 1 : 0], p.x * fk, p.y * fk, p.r * 1.05f / 14.f, now);
        else {
            Piece q = p;
            q.x *= fk;
            q.y *= fk;
            q.r *= fk;
            drawPlanetArt(cp.px.data(), kW, kH, 0, 0, q, now);
        }
    }
    // 0.30.0 (owner's picture): the Vuil'raith home - pale tentacles out of the nebula, wrapped round Acheron
    {
        static int cabal = -2;
        if (cabal == -2) cabal = animId("54");
        if (cabal >= 0 && t.bg == cabal)
            for (const Piece& p : t.pieces)
                if (p.kind == Piece::Planet) {
                    cabalTentacles(cp.px.data(), kW, kH, p.x * float(kK), p.y * float(kK), p.r * float(kK), now, float(kK));
                    break;
                }
    }
    // forget tiles not drawn for a while
    if (++sweep % 512 == 0 && cache.size() > 160) {
        for (auto it = cache.begin(); it != cache.end();) {
            if (now - it->second.used > 4000u && &it->second != &cp) it = cache.erase(it);
            else ++it;
        }
    }
    return cp;
}

// Where each screen pixel of a scaled tile samples the art (per size, made once).
struct Samples {
    std::vector<int16_t> x0, x1, y0, y1;  // nearest: x0 / y0; reduced: two samples each way
    std::vector<uint8_t> wx, wy;          // enlarged by an uneven amount: x0's share (0-255) of a pixel on an art edge
};
const Samples& samplesFor(int sizeKey, float s, int w, int h, bool smooth) {
    // 0.43.3 (tester's crash, mouse-wheel zoom in Retrowave): two scales that round to the same key can still make
    // tiles one pixel apart in size - the table is kept per exact width and height too, or the wider tile read
    // past the end of the narrower one's table
    static std::unordered_map<int64_t, Samples> cache;
    const int64_t key = (((int64_t(sizeKey) * 2 + (smooth ? 1 : 0)) * 4 + kK) * 8192 + w) * 8192 + h;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Samples& sm = cache[key];
    auto at = [&](float v, int n) { return int16_t(std::max(0, std::min(n - 1, int(v / s)))); };
    for (int x = 0; x < w; ++x) {
        sm.x0.push_back(s >= 0.99f ? at(float(x) + 0.5f, kW) : at(float(x) + 0.25f, kW));
        sm.x1.push_back(at(float(x) + 0.75f, kW));
    }
    for (int y = 0; y < h; ++y) {
        sm.y0.push_back(s >= 0.99f ? at(float(y) + 0.5f, kH) : at(float(y) + 0.25f, kH));
        sm.y1.push_back(at(float(y) + 0.75f, kH));
    }
    // (0.27.0) the sharp PC picture enlarges the art by uneven amounts (1.59, 2.4...): a screen
    // pixel that straddles two art pixels mixes them by how much of each it covers, so every
    // art pixel looks the same size ("sharp bilinear")
    auto share = [&](int i, int n, std::vector<int16_t>& a, std::vector<int16_t>& b, std::vector<uint8_t>& wt) {
        const float lo = float(i) / s, hi = float(i + 1) / s;
        const int p = std::max(0, std::min(n - 1, int(lo)));
        const float edge = float(p + 1);
        if (hi <= edge || p + 1 >= n) {
            a[size_t(i)] = b[size_t(i)] = int16_t(p);
            wt[size_t(i)] = 255;
        } else {
            a[size_t(i)] = int16_t(p);
            b[size_t(i)] = int16_t(p + 1);
            wt[size_t(i)] = uint8_t(std::max(0.f, std::min(255.f, (edge - lo) * s * 255.f + 0.5f)));
        }
    };
    if (smooth && s > 1.01f && std::fabs(s - std::floor(s + 0.5f)) > 0.01f) {
        sm.wx.resize(size_t(w));
        sm.wy.resize(size_t(h));
        for (int x = 0; x < w; ++x) share(x, kW, sm.x0, sm.x1, sm.wx);
        for (int y = 0; y < h; ++y) share(y, kH, sm.y0, sm.y1, sm.wy);
    }
    return sm;
}

// The composed tile at a screen size, made when the art or the size changes.
const Scaled& scaledArt(Comp& cp, float s, bool smooth) {
    Pack& pk = pack();
    const int sizeKey = int(s * 1000.f + 0.5f) * 2 + (smooth ? 1 : 0);
    Scaled* sc = &cp.scaled[0];
    if (cp.scaled[0].sizeKey != sizeKey) sc = cp.scaled[1].sizeKey == sizeKey ? &cp.scaled[1] : nullptr;
    if (!sc) {  // replace the one made longer ago
        sc = cp.scaled[0].frame <= cp.scaled[1].frame ? &cp.scaled[0] : &cp.scaled[1];
        sc->sizeKey = sizeKey;
        sc->frame = -1;
    }
    if (sc->frame == cp.frame) return *sc;
    sc->frame = cp.frame;
    sc->w = int(std::ceil(float(kW) * s));
    sc->h = int(std::ceil(float(kH) * s));
    sc->px.resize(size_t(sc->w) * size_t(sc->h));
    const Samples& sm = samplesFor(sizeKey / 2, s, sc->w, sc->h, smooth);
    const uint16_t* art = cp.px.data();
    const uint32_t* pal = pk.pal.data();
    if (!sm.wx.empty()) {  // enlarged by an uneven amount: shares at the art pixels' edges
        auto blend = [&](uint32_t a, uint32_t b, int wa) -> uint32_t {  // a's share wa of 255
            if (a == kClear || b == kClear) return wa >= 128 ? a : b;
            if (wa >= 255) return a;
            return mix(b, a, wa);
        };
        for (int y = 0; y < sc->h; ++y) {
            const uint16_t* ra = art + size_t(sm.y0[size_t(y)]) * size_t(kW);
            const uint16_t* rb = art + size_t(sm.y1[size_t(y)]) * size_t(kW);
            const int wy = sm.wy[size_t(y)];
            uint32_t* out = &sc->px[size_t(y) * size_t(sc->w)];
            for (int x = 0; x < sc->w; ++x) {
                const int xa = sm.x0[size_t(x)], xb = sm.x1[size_t(x)], wx = sm.wx[size_t(x)];
                auto colOf = [&](uint16_t i) { return i ? pal[i] : kClear; };
                const uint32_t top = blend(colOf(ra[xa]), colOf(ra[xb]), wx);
                const uint32_t bot = wy >= 255 ? top : blend(colOf(rb[xa]), colOf(rb[xb]), wx);
                out[x] = blend(top, bot, wy);
            }
        }
        return *sc;
    }
    if (s >= 0.99f) {  // enlarged (or 1:1): nearest pixel - the chunky look
        for (int y = 0; y < sc->h; ++y) {
            const uint16_t* row = art + size_t(sm.y0[size_t(y)]) * size_t(kW);
            uint32_t* out = &sc->px[size_t(y) * size_t(sc->w)];
            for (int x = 0; x < sc->w; ++x) {
                const uint16_t i = row[sm.x0[size_t(x)]];
                out[x] = i ? pal[i] : kClear;
            }
        }
        return *sc;
    }
    // reduced: the average of 2 x 2 samples in each screen pixel
    for (int y = 0; y < sc->h; ++y) {
        const uint16_t* ra = art + size_t(sm.y0[size_t(y)]) * size_t(kW);
        const uint16_t* rb = art + size_t(sm.y1[size_t(y)]) * size_t(kW);
        uint32_t* out = &sc->px[size_t(y) * size_t(sc->w)];
        for (int x = 0; x < sc->w; ++x) {
            const int xa = sm.x0[size_t(x)], xb = sm.x1[size_t(x)];
            const uint16_t q[4] = {ra[xa], ra[xb], rb[xa], rb[xb]};
            uint32_t rbS = 0, gS = 0;
            int n = 0;
            for (uint16_t i : q)
                if (i) {
                    const uint32_t col = pal[i];
                    rbS += col & 0xFF00FFu;
                    gS += col & 0x00FF00u;
                    ++n;
                }
            if (n < 2) {
                out[x] = kClear;
                continue;
            }
            // n is 2..4: halve or quarter (3: near enough with a third)
            if (n == 4) out[x] = ((rbS >> 2) & 0xFF00FFu) | ((gS >> 2) & 0x00FF00u);
            else if (n == 2) out[x] = ((rbS >> 1) & 0xFF00FFu) | ((gS >> 1) & 0x00FF00u);
            else out[x] = rgb(int(((rbS >> 16) & 0x3FF) / 3), int(((gS >> 8) & 0x3FF) / 3), int((rbS & 0x3FF) / 3));
        }
    }
    return *sc;
}

}  // namespace

bool available() { return pack().ok; }
bool tilesetAvailable(int t) { return t == kSimplistic || (t == kRetrowave && available()) || (t == kVoidfall && voidfall::available()); }
int tileset() { return tilesetAvailable(g_tileset) ? g_tileset : kSimplistic; }
bool on() { return tileset() == kRetrowave; }
bool hd() { return tileset() == kVoidfall; }
bool art() { return tileset() != kSimplistic; }
void setTileset(int t) { g_tileset = t >= 0 && t < kTilesets ? t : kSimplistic; }
void setOn(bool v) { g_tileset = v ? kRetrowave : kSimplistic; }
const char* tilesetName(int t) { return t == kRetrowave ? "Retrowave" : t == kVoidfall ? "Voidfall" : "Simplistic"; }

void setBackground(Tile& t, int def, uint32_t seed, bool nexusActive) {
    t.bg = -1;
    t.flip = 0;
    t.def = def;
    t.seed = seed;
    t.nexusActive = nexusActive;
    if (!available()) return;
    const char* key = def >= 0 && def < ti::kSystemCount ? ti::kSystems[def].key : "";
    std::string k = key;
    if (k == "82b" || k == "82a" || k == "82") k = nexusActive ? "82b" : "82a";
    // Uncharted Space (0.33.0): its anomaly tiles borrow the matching backgrounds
    if ((k == "d120" || k == "d121") && animId(k.c_str()) >= 0) {  // (0.33.1) their own: rocks with a nebula / a rift
        t.bg = animId(k.c_str());
        return;
    }
    static const char* usMap[][2] = {{"d104", "68"}, {"d119", "42"}, {"d120", "44"}, {"d121", "41"}, {"d122", "41"}, {"d123", "43"}};
    for (const auto& m : usMap)
        if (k == m[0]) k = m[1];
    // Thunder's Edge (0.34.0): the entropic scar has its own background, the other anomalies borrow
    if ((k == "114" || k == "116") && animId("scar") >= 0) {
        t.bg = animId("scar");
        return;
    }
    if (k == "117" && animId("d121") >= 0) {  // asteroids round a gravity rift, as Uncharted Space's
        t.bg = animId("d121");
        return;
    }
    static const char* teMap[][2] = {{"113", "41"}, {"115", "44"}, {"117", "67"}, {"92", "68"}};
    for (const auto& m : teMap)
        if (k == m[0]) k = m[1];
    // 0.43.0 (owner D44): the Muaat's Nova Seed (81) and the Zelian asteroid field (d36) - their own when baked
    if ((k == "81" || k == "d36") && animId(k.c_str()) < 0) k = k == "81" ? "43" : "44";
    static const char* special[] = {"41", "42", "43", "44", "45", "67", "68", "79", "80", "81", "d36", "56", "17", "82a", "82b", "54"};
    for (const char* s : special)
        if (k == s) {
            t.bg = animId(s);
            if (t.bg >= 0) return;
        }
    seed = seed * 2654435761u + 0x9E37u;
    t.bg = animId((std::string("star") + char('0' + int((seed >> 8) % 8u))).c_str());
    t.flip = uint8_t((seed >> 16) & 3u);
}

float snapScale(float size) {
    pack();  // (the art scale in use)
    const float s = size * float(kK) / kArtR;  // pixels of art per design pixel... in whole steps of the art's pixels
    return s >= 1.f ? std::floor(s) / float(kK) : s / float(kK);
}

int artScale() {
    if (!available()) return 1;
    pack();
    return kK;
}

void draw(Canvas& c, const Tile& t, float cx, float cy, float size, uint32_t now, int alpha) {
    if (hd()) {  // 0.32.0: the Voidfall tileset draws the same tiles its own way
        voidfall::draw(c, t, cx, cy, size, now, alpha);
        return;
    }
    if (!available() || size < 2.f) return;
    // small tiles move at half the rate: half the work where the detail is too small to see
    Comp& cp = compose(t, now, size < 40.f ? 200u : 100u);
    // (0.27.0: in real pixels - on the sharp PC picture the art is enlarged further)
    cx = c.fX(cx);
    cy = c.fY(cy);
    const float s = c.fx(size) / (kArtR * float(kK));
    const Scaled& sc = scaledArt(cp, s, !c.unscaled());
    const int x0 = int(std::floor(cx - float(kW) / 2.f * s + 0.5f)), y0 = int(std::floor(cy - float(kH) / 2.f * s + 0.5f));
    const Rect cl = c.pxClip();
    const int xa = std::max(cl.x, x0), xb = std::min(cl.x + cl.w, x0 + sc.w);
    const int ya = std::max(cl.y, y0), yb = std::min(cl.y + cl.h, y0 + sc.h);
    if (xa >= xb || ya >= yb) return;
    uint32_t* px = c.pixels();
    const int W = c.pw();
    for (int y = ya; y < yb; ++y) {
        const uint32_t* in = &sc.px[size_t(y - y0) * size_t(sc.w) + size_t(xa - x0)];
        uint32_t* line = px + size_t(y) * size_t(W);
        if (alpha >= 255) {
            for (int x = xa; x < xb; ++x, ++in)
                if (*in != kClear) line[x] = *in;
        } else {
            for (int x = xa; x < xb; ++x, ++in)
                if (*in != kClear) line[x] = mix(line[x], *in, alpha);
        }
    }
}

void chunkyGrid(const Canvas& c, Rect area, int f, int& ox, int& oy, int& fpx) {
    pack();
    area = c.toPx(area);
    fpx = kK > 1 ? std::max(2, int(std::lround(c.fx(float(f)) / float(kK)))) : std::max(2, c.S(f));
    const Rect cl = c.pxClip();
    ox = std::max(area.x, cl.x);
    oy = std::max(area.y, cl.y);
}

void chunky(Canvas& c, Rect area, int f) {
    if (f < 2 || !available()) return;
    Pack& pk = pack();
    // nearest palette colour for every 5-bit-per-channel colour, made once
    static std::vector<uint16_t> lut;
    if (lut.empty()) {
        lut.resize(32768);
        for (int i = 0; i < 32768; ++i) {
            const int r = ((i >> 10) & 31) * 255 / 31, g = ((i >> 5) & 31) * 255 / 31, b = (i & 31) * 255 / 31;
            int best = 1, bd = 1 << 30;
            for (size_t k = 1; k < pk.pal.size(); ++k) {
                const uint32_t p = pk.pal[k];
                const int dr = r - int((p >> 16) & 255), dg = g - int((p >> 8) & 255), db = b - int(p & 255);
                const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                if (d < bd) {
                    bd = d;
                    best = int(k);
                }
            }
            lut[size_t(i)] = uint16_t(best);
        }
    }
    // (0.27.0: in real pixels; the blocks stay the same size on the screen - 0.48.0: with the sharp art, as fine as its pixels)
    area = c.toPx(area);
    f = kK > 1 ? std::max(2, int(std::lround(c.fx(float(f)) / float(kK)))) : std::max(2, c.S(f));
    const Rect cl = c.pxClip();
    const int x0 = std::max(area.x, cl.x), y0 = std::max(area.y, cl.y);
    const int x1 = std::min(area.x + area.w, cl.x + cl.w), y1 = std::min(area.y + area.h, cl.y + cl.h);
    uint32_t* px = c.pixels();
    const int W = c.pw();
    for (int by = y0; by < y1; by += f)
        for (int bx = x0; bx < x1; bx += f) {
            const int ex = std::min(x1, bx + f), ey = std::min(y1, by + f);
            {  // a block of one colour stays as it is (the tile art under a unit keeps its own pixels)
                const uint32_t first = px[size_t(by) * size_t(W) + size_t(bx)];
                bool same = true;
                for (int y = by; y < ey && same; ++y)
                    for (int x = bx; x < ex; ++x)
                        if (px[size_t(y) * size_t(W) + size_t(x)] != first) {
                            same = false;
                            break;
                        }
                if (same) continue;
            }
            int r = 0, g = 0, b = 0, n = 0;
            for (int y = by; y < ey; ++y)
                for (int x = bx; x < ex; ++x) {
                    const uint32_t col = px[size_t(y) * size_t(W) + size_t(x)];
                    r += int((col >> 16) & 255);
                    g += int((col >> 8) & 255);
                    b += int(col & 255);
                    ++n;
                }
            r /= n;
            g /= n;
            b /= n;
            // ordered dither between palette steps (+-6 levels)
            const int d = int(kBayer[(by / f) & 7][(bx / f) & 7]) / 5 - 6;
            auto q = [&](int v) { v += d; return v < 0 ? 0 : v > 255 ? 31 : v * 31 / 255; };
            const uint32_t col = pk.pal[lut[size_t(q(r) << 10 | q(g) << 5 | q(b))]];
            for (int y = by; y < ey; ++y) std::fill(px + size_t(y) * size_t(W) + size_t(bx), px + size_t(y) * size_t(W) + size_t(ex), col);
        }
}

}  // namespace retro
