#include "ui/voidfall.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/log.h"
#include "engine/data.h"
#include "gfx/font.h"
#include "gfx/image.h"
#include "gfx/picture.h"

namespace voidfall {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kS3 = 0.8660254f;  // sqrt(3) / 2

// ------------------------------------------------------------------ the pack
struct Tex {  // 0xAARRGGBB, premultiplied
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    bool ok() const { return w > 0; }
};
struct Grey {
    int w = 0, h = 0;
    std::vector<uint8_t> v;
    bool ok() const { return w > 0; }
};
enum Motion : uint8_t { Drift, Turn, Slide };
struct Sky {
    std::string name, flow;
    Motion motion = Drift;
    float speed = 1.f;
    uint32_t veil = 0;
    int twinkles = 8;
    // (0.33.1) an optional second moving layer - Uncharted Space's anomalies come in pairs (rocks round a rift)
    std::string flow2;
    Motion motion2 = Drift;
    float speed2 = 0.f;
    // (0.43.1) light running along a grey mask (the Fracture's cracks): out from the middle, or in (dir -1)
    std::string pulseMask;
    uint32_t pulseCol = 0;
    float pulsePeriod = 3.f;
    int pulseDir = 1;
};
struct PlanetRec {
    std::string key, kind;
    float period = 24.f, ambient = 0.16f;
    uint32_t atmo = 0, rim = 0, emis = 0;
    bool hasAtmo = false, hasE = false, hasC = false;
};

struct Pack {
    bool tried = false, ok = false;
    std::vector<uint8_t> blob;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> toc;
    std::unordered_map<std::string, Sky> skies;
    std::vector<PlanetRec> planets;
    std::vector<int> planetOf;  // kPlanets index -> planets
    std::unordered_map<std::string, Tex> rgb;
    std::unordered_map<std::string, Grey> grey;
};

uint32_t parseRgb(const std::string& s) {
    if (s.size() != 6) return 0;
    return uint32_t(std::strtoul(s.c_str(), nullptr, 16));
}

Pack& pack() {
    static Pack p;
    if (p.tried) return p;
    p.tried = true;
#if defined(_WIN32) || defined(TI4_SDL2)
    const bool pcFirst = true;
#else
    const bool pcFirst = std::getenv("TI4_VOIDFALL_PC") != nullptr;  // tests: the sharp PC art
#endif
    const char* names[2] = {pcFirst ? "voidfall-pc.dat" : "voidfall-miyoo.dat", pcFirst ? "voidfall-miyoo.dat" : "voidfall-pc.dat"};
    for (const char* n : names) {
        for (const char* dir : {"", "assets/", "../assets/"}) {
            const std::string path = std::string(dir) + n;
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) continue;
            std::fseek(f, 0, SEEK_END);
            const long len = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (len > 16 && len < 256L * 1024 * 1024) {
                p.blob.resize(size_t(len));
                if (std::fread(p.blob.data(), 1, p.blob.size(), f) != p.blob.size()) p.blob.clear();
            }
            std::fclose(f);
            if (p.blob.size() < 8 || std::memcmp(p.blob.data(), "VFD1", 4) != 0) {
                p.blob.clear();
                continue;
            }
            LOG("voidfall: %s (%u bytes)", path.c_str(), unsigned(p.blob.size()));
            break;
        }
        if (!p.blob.empty()) break;
    }
    if (p.blob.empty()) {
        LOG("voidfall: no voidfall pack - the Voidfall tileset is off");
        return p;
    }
    const uint8_t* b = p.blob.data();
    auto u32 = [&](size_t o) { return uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24; };
    const uint32_t n = u32(4);
    size_t o = 8;
    for (uint32_t i = 0; i < n; ++i) {
        if (o + 2 > p.blob.size()) return p;
        const size_t nl = size_t(b[o]) | size_t(b[o + 1]) << 8;
        o += 2;
        if (o + nl + 8 > p.blob.size()) return p;
        std::string name(reinterpret_cast<const char*>(b + o), nl);
        o += nl;
        const uint32_t off = u32(o), len = u32(o + 4);
        o += 8;
        if (size_t(off) + len > p.blob.size()) return p;
        p.toc[name] = {off, len};
    }
    auto it = p.toc.find("index.txt");
    if (it == p.toc.end()) return p;
    std::istringstream in(std::string(reinterpret_cast<const char*>(b + it->second.first), it->second.second));
    std::string line;
    std::unordered_map<std::string, int> byKey;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string kind;
        ls >> kind;
        if (kind == "sky") {
            Sky s;
            std::string motion, veil;
            ls >> s.name >> s.flow >> motion >> s.speed >> veil >> s.twinkles;
            s.motion = motion == "turn" ? Turn : motion == "slide" ? Slide : Drift;
            s.veil = parseRgb(veil);
            std::string f2, m2;
            float sp2 = 0.f;
            if (ls >> f2 >> m2 >> sp2) {
                s.flow2 = f2;
                s.motion2 = m2 == "turn" ? Turn : m2 == "slide" ? Slide : Drift;
                s.speed2 = sp2;
            }
            p.skies[s.name] = s;
        } else if (kind == "pulse") {  // 0.43.1: pulse SKY MASK RGB PERIOD DIR
            std::string sky, mask, col;
            float period = 3.f;
            int dir = 1;
            ls >> sky >> mask >> col >> period >> dir;
            auto sit = p.skies.find(sky);
            if (sit != p.skies.end()) {
                sit->second.pulseMask = mask;
                sit->second.pulseCol = parseRgb(col);
                sit->second.pulsePeriod = std::max(0.5f, period);
                sit->second.pulseDir = dir < 0 ? -1 : 1;
            }
        } else if (kind == "planet") {
            PlanetRec r;
            std::string atmo, rim, emis;
            int he = 0, hc = 0;
            ls >> r.key >> r.kind >> r.period >> r.ambient >> atmo >> rim >> emis >> he >> hc;
            r.hasAtmo = atmo != "-";
            r.atmo = parseRgb(atmo);
            r.rim = parseRgb(rim);
            r.emis = parseRgb(emis);
            r.hasE = he != 0;
            r.hasC = hc != 0;
            byKey[r.key] = int(p.planets.size());
            p.planets.push_back(r);
        }
    }
    p.planetOf.assign(size_t(ti::kPlanetCount), -1);
    for (int i = 0; i < ti::kPlanetCount; ++i) {
        auto k = byKey.find(ti::kPlanets[i].key);
        if (k != byKey.end()) p.planetOf[size_t(i)] = k->second;
    }
    p.ok = !p.skies.empty();
    return p;
}

const Tex* texRgb(const std::string& name) {
    Pack& p = pack();
    auto it = p.rgb.find(name);
    if (it != p.rgb.end()) return it->second.ok() ? &it->second : nullptr;
    Tex& t = p.rgb[name];
    auto e = p.toc.find(name);
    if (e == p.toc.end()) return nullptr;
    Image img;
    if (!decodePicture(p.blob.data() + e->second.first, e->second.second, img)) {
        LOG("voidfall: can't read %s", name.c_str());
        return nullptr;
    }
    t.w = img.w;
    t.h = img.h;
    t.px = std::move(img.px);
    return &t;
}

const Grey* texGrey(const std::string& name) {
    Pack& p = pack();
    auto it = p.grey.find(name);
    if (it != p.grey.end()) return it->second.ok() ? &it->second : nullptr;
    Grey& g = p.grey[name];
    auto e = p.toc.find(name);
    if (e == p.toc.end()) return nullptr;
    Image img;
    if (!decodePicture(p.blob.data() + e->second.first, e->second.second, img)) return nullptr;
    g.w = img.w;
    g.h = img.h;
    g.v.resize(img.px.size());
    for (size_t i = 0; i < img.px.size(); ++i) g.v[i] = uint8_t((img.px[i] >> 8) & 255);
    return &g;
}

// ------------------------------------------------------------------ pixels
inline uint32_t lerpRGB(uint32_t a, uint32_t b, uint32_t w) {  // w 0..256: b's share
    const uint32_t rb = (((a & 0xFF00FFu) * (256 - w) + (b & 0xFF00FFu) * w) >> 8) & 0xFF00FFu;
    const uint32_t g = (((a & 0x00FF00u) * (256 - w) + (b & 0x00FF00u) * w) >> 8) & 0x00FF00u;
    return rb | g;
}
inline uint32_t lerp4(uint32_t a, uint32_t b, uint32_t w) {
    const uint32_t rb = (((a & 0x00FF00FFu) * (256 - w) + (b & 0x00FF00FFu) * w) >> 8) & 0x00FF00FFu;
    const uint32_t ag = (((a >> 8) & 0x00FF00FFu) * (256 - w) + ((b >> 8) & 0x00FF00FFu) * w) & 0xFF00FF00u;
    return rb | ag;
}
// bilinear, coordinates in 1/256 texel (texel centres at .5 already taken off by the caller)
inline uint32_t sampleClamp(const Tex& t, int fu, int fv) {
    int x = fu >> 8, y = fv >> 8;
    const uint32_t wx = uint32_t(fu & 255), wy = uint32_t(fv & 255);
    const int x0 = std::max(0, std::min(t.w - 1, x)), x1 = std::max(0, std::min(t.w - 1, x + 1));
    const int y0 = std::max(0, std::min(t.h - 1, y)), y1 = std::max(0, std::min(t.h - 1, y + 1));
    const uint32_t* r0 = &t.px[size_t(y0) * size_t(t.w)];
    const uint32_t* r1 = &t.px[size_t(y1) * size_t(t.w)];
    return lerp4(lerp4(r0[x0], r0[x1], wx), lerp4(r1[x0], r1[x1], wx), wy);
}
inline uint32_t sampleWrap(const Tex& t, int fu, int fv) {  // power-of-two sizes
    const int mx = t.w - 1, my = t.h - 1;
    const int x0 = (fu >> 8) & mx, y0 = (fv >> 8) & my, x1 = (x0 + 1) & mx, y1 = (y0 + 1) & my;
    const uint32_t wx = uint32_t(fu & 255), wy = uint32_t(fv & 255);
    const uint32_t* r0 = &t.px[size_t(y0) * size_t(t.w)];
    const uint32_t* r1 = &t.px[size_t(y1) * size_t(t.w)];
    return lerp4(lerp4(r0[x0], r0[x1], wx), lerp4(r1[x0], r1[x1], wx), wy);
}
inline int sampleGreyWrap(const Grey& g, int fu, int fv) {  // 0..255
    const int mx = g.w - 1, my = g.h - 1;
    const int x0 = (fu >> 8) & mx, y0 = (fv >> 8) & my, x1 = (x0 + 1) & mx, y1 = (y0 + 1) & my;
    const int wx = fu & 255, wy = fv & 255;
    const uint8_t* r0 = &g.v[size_t(y0) * size_t(g.w)];
    const uint8_t* r1 = &g.v[size_t(y1) * size_t(g.w)];
    const int a = r0[x0] * (256 - wx) + r0[x1] * wx, b = r1[x0] * (256 - wx) + r1[x1] * wx;
    return (a * (256 - wy) + b * wy) >> 16;
}
inline int sampleGreyClamp(const Grey& g, int fu, int fv) {  // 0..255, edges held (any size)
    const int x0 = std::max(0, std::min(g.w - 1, fu >> 8)), y0 = std::max(0, std::min(g.h - 1, fv >> 8));
    const int x1 = std::min(g.w - 1, x0 + 1), y1 = std::min(g.h - 1, y0 + 1);
    const int wx = fu < 0 ? 0 : fu & 255, wy = fv < 0 ? 0 : fv & 255;
    const uint8_t* r0 = &g.v[size_t(y0) * size_t(g.w)];
    const uint8_t* r1 = &g.v[size_t(y1) * size_t(g.w)];
    const int a = r0[x0] * (256 - wx) + r0[x1] * wx, b = r1[x0] * (256 - wx) + r1[x1] * wx;
    return (a * (256 - wy) + b * wy) >> 16;
}
// u wraps (any width), v clamps: planet maps
inline uint32_t samplePlanet(const Tex& t, int fu, int fv) {
    int x0 = (fu >> 8) % t.w;
    if (x0 < 0) x0 += t.w;
    const int x1 = x0 + 1 >= t.w ? 0 : x0 + 1;
    const int y0 = std::max(0, std::min(t.h - 1, fv >> 8)), y1 = std::min(t.h - 1, y0 + 1);
    const uint32_t wx = uint32_t(fu & 255), wy = uint32_t(fv & 255);
    const uint32_t* r0 = &t.px[size_t(y0) * size_t(t.w)];
    const uint32_t* r1 = &t.px[size_t(y1) * size_t(t.w)];
    return lerpRGB(lerpRGB(r0[x0], r0[x1], wx), lerpRGB(r1[x0], r1[x1], wx), wy);
}
inline int greyNearest(const Grey& g, int fu, int fv) {  // soft maps (clouds, glow): no filtering needed
    int x = ((fu + 128) >> 8) % g.w;
    if (x < 0) x += g.w;
    const int y = std::max(0, std::min(g.h - 1, (fv + 128) >> 8));
    return g.v[size_t(y) * size_t(g.w) + size_t(x)];
}
inline int greyPlanet(const Grey& g, int fu, int fv) {
    int x0 = (fu >> 8) % g.w;
    if (x0 < 0) x0 += g.w;
    const int x1 = x0 + 1 >= g.w ? 0 : x0 + 1;
    const int y0 = std::max(0, std::min(g.h - 1, fv >> 8)), y1 = std::min(g.h - 1, y0 + 1);
    const int wx = fu & 255, wy = fv & 255;
    const uint8_t* r0 = &g.v[size_t(y0) * size_t(g.w)];
    const uint8_t* r1 = &g.v[size_t(y1) * size_t(g.w)];
    const int a = r0[x0] * (256 - wx) + r0[x1] * wx, b = r1[x0] * (256 - wx) + r1[x1] * wx;
    return (a * (256 - wy) + b * wy) >> 16;
}
inline uint32_t addRGB(uint32_t d, int r, int g, int b) {
    r += int((d >> 16) & 255);
    g += int((d >> 8) & 255);
    b += int(d & 255);
    return rgb(r > 255 ? 255 : r < 0 ? 0 : r, g > 255 ? 255 : g < 0 ? 0 : g, b > 255 ? 255 : b < 0 ? 0 : b);
}
inline uint32_t addCol(uint32_t d, uint32_t c, int k) {  // d + c * k / 256
    return addRGB(d, int((c >> 16) & 255) * k >> 8, int((c >> 8) & 255) * k >> 8, int(c & 255) * k >> 8);
}
inline uint32_t scaleRGB(uint32_t c, int k) {  // k / 256
    return rgb(int((c >> 16) & 255) * k >> 8, int((c >> 8) & 255) * k >> 8, int(c & 255) * k >> 8);
}
inline float smoothstep(float a, float b, float x) {
    float t = (x - a) / (b - a);
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    return t * t * (3.f - 2.f * t);
}
inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
inline float rnd01(uint32_t s) { return float(hash32(s) & 0xFFFFFF) / 16777216.f; }
// quick approximations for the per-pixel swirls (plenty for pictures)
inline float fastCos(float a) {
    static float tab[1025];
    static bool made = false;
    if (!made) {
        for (int i = 0; i <= 1024; ++i) tab[i] = std::cos(float(i) / 1024.f * 2.f * 3.14159265f);
        made = true;
    }
    float u = a * (1024.f / (2.f * 3.14159265f));
    u -= 1024.f * std::floor(u / 1024.f);
    const int i = int(u);
    const float f = u - float(i);
    return tab[i & 1023] + (tab[(i & 1023) + 1] - tab[i & 1023]) * f;
}
inline float fastAtan2(float y, float x) {
    const float ax = std::fabs(x), ay = std::fabs(y);
    const float mn = std::min(ax, ay), mx = std::max(ax, ay) + 1e-9f;
    const float a = mn / mx, s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}
inline float fastLog(float x) {  // x > 0
    int e;
    const float m = std::frexp(x, &e);  // x = m 2^e, m in [0.5, 1)
    const float t = (m - 1.f) / (m + 1.f), t2 = t * t;
    return float(e) * 0.693147181f + 2.f * t * (1.f + t2 * (0.333333f + t2 * 0.2f));
}

// colours (the box art's)
constexpr uint32_t kLattice = rgb(185, 160, 255);
constexpr uint32_t kShimmer = rgb(255, 120, 200);
constexpr uint32_t kEdgeHi = rgb(214, 196, 255);
constexpr uint32_t kRule = rgb(255, 90, 170);

// A veil enlarged to a tile's pixels, tileable (period n x n).
struct VeilPlane {
    int n = 0;
    std::vector<uint8_t> v;
};
const VeilPlane* veilPlane(const std::string& name, float R) {
    static std::unordered_map<std::string, VeilPlane> cache;
    const int n = std::max(16, int(2.2f * R + 0.5f));
    const std::string key = name + "@" + std::to_string(n);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second.n ? &it->second : nullptr;
    if (cache.size() > 24) cache.clear();
    VeilPlane& p = cache[key];
    const Grey* g = texGrey(name);
    if (!g) return nullptr;
    p.n = n;
    p.v.resize(size_t(n) * size_t(n));
    const float k = float(g->w) / float(n);  // texels per pixel (the veil is square, a power of two)
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            p.v[size_t(y) * size_t(n) + size_t(x)] =
                uint8_t(sampleGreyWrap(*g, int((float(x) + 0.5f) * k * 256.f - 128.f), int((float(y) + 0.5f) * k * 256.f - 128.f)));
    return &p;
}

// ------------------------------------------------------------------ tiles
struct PlanetPix {
    int32_t u;      // texture u, 1/256 texel (the turn is added)
    uint16_t v;     // texture v, 1/256 texel
    uint8_t cov;    // edge coverage
    uint8_t lam;    // daylight 0..255
    uint8_t rim;    // the magenta rim on the dark side
    uint8_t fres;   // atmosphere on the lit limb
};
struct PlanetDraw {
    int id = -1;    // kPlanets
    const PlanetRec* rec = nullptr;
    float x = 0, y = 0, r = 0;  // in the tile's pixels
    bool dim = false;
    bool station = false;  // Thunder's Edge (0.34.0): painted as a space station (drawStation)
    int bx = 0, by = 0, bw = 0, bh = 0;
    std::vector<PlanetPix> lut;
    // (0.32.1) the planet as last painted, kept until it has turned by a texel (or its glow moved)
    std::vector<uint32_t> img;
    int64_t imgKey = INT64_MIN;
};
struct Star {
    float x, y, size;
    uint32_t period, phase, col;
};

struct Built {
    int W = 0, H = 0;
    float R = 0, cx = 0, cy = 0;  // corner radius and centre in the tile's pixels
    std::vector<uint32_t> base;
    std::vector<uint32_t> layer;    // (0.32.1) base + the moving layer, kept until that has moved a pixel
    int64_t layerKey = INT64_MIN;
    std::vector<int16_t> fullL, fullR;  // per row: where the hex covers whole pixels (copied without blending)
    std::vector<uint8_t> mask, latt;
    std::unique_ptr<Canvas> frame;
    std::vector<PlanetDraw> planets;
    std::vector<Star> stars;
    uint32_t tick = 0xFFFFFFFFu, used = 0;
    const Sky* sky = nullptr;
    bool hyper = false;
    // (0.43.1) the pulse: the mask's pixels in this tile - index, strength, distance from the middle (0..65535 = R)
    struct PulsePx {
        uint32_t i;
        uint8_t m;
        uint16_t rr;
    };
    std::vector<PulsePx> pulse;
};

const char* skyName(const retro::Tile& t) {
    if (t.sky && pack().skies.count(t.sky)) return t.sky;  // 0.43.1: a named sky (Thunder's Edge's storm)
    if (t.lanes) return "hyper";
    if (t.def < 0 || t.def >= ti::kSystemCount) return nullptr;
    const ti::SystemDef& d = ti::kSystems[t.def];
    if (d.back == ti::Back::Hyperlane) return "hyper";
    std::string k = d.key;
    if (k == "82" || k == "82a" || k == "82b") return t.nexusActive ? "82b" : "82a";
    // Uncharted Space (0.33.0): its anomaly tiles borrow the matching skies
    static const char* usMap[][2] = {{"d104", "68"}, {"d119", "42"}, {"d122", "41"}, {"d123", "80"}};
    if ((k == "d120" || k == "d121") && pack().skies.count(k)) return k == "d120" ? "d120" : "d121";  // (0.33.1) their own skies
    if (k == "d120") return "45";  // (an older pack)
    if (k == "d121") return "67";
    for (const auto& m : usMap)
        if (k == m[0]) return m[1];
    // Thunder's Edge (0.34.0): its anomalies borrow the matching skies; the entropic scar has its own
    static const char* teMap[][2] = {{"113", "41"}, {"115", "44"}, {"117", "67"}, {"112", "18"}, {"92", "68"}};
    if ((k == "114" || k == "116") && pack().skies.count("scar")) return "scar";
    if (k == "117" && pack().skies.count("d121")) return "d121";
    for (const auto& m : teMap)
        if (k == m[0]) return m[1];
    if (k == "d36") return pack().skies.count("d36") ? "d36" : "44";  // 0.43.0: the Zelian asteroid field
    // 0.43.1 (owner): the Fracture's seven systems have their own - cracked space
    if (k.size() == 9 && k.compare(0, 8, "fracture") == 0 && k[8] >= '1' && k[8] <= '7') {
        static const char* frac[7] = {"frac1", "frac2", "frac3", "frac4", "frac5", "frac6", "frac7"};
        if (pack().skies.count(frac[k[8] - '1'])) return frac[k[8] - '1'];
    }
    static const char* special[] = {"41", "42", "43", "44", "45", "67", "68", "79", "80", "81", "56", "17", "54", "18"};
    for (const char* s : special)
        if (k == s) return s;
    static const char* stars[] = {"star0", "star1", "star2", "star3", "star4", "star5", "star6", "star7"};
    return stars[hash32(t.seed * 2654435761u + 0x9E37u) % 8u];
}

uint64_t tileKey(const retro::Tile& t, int rq) {
    uint64_t h = 1469598103934665603ull;
    auto mixIn = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mixIn(uint64_t(uint32_t(t.def)));
    mixIn(t.seed);
    mixIn(t.nexusActive ? 1 : 0);
    mixIn(t.lanes);
    if (t.sky)
        for (const char* q = t.sky; *q; ++q) mixIn(uint64_t(uint8_t(*q)) + 0x51);
    for (const retro::Piece& p : t.pieces) {
        mixIn(uint64_t(p.kind) << 32 | uint32_t(p.id));
        mixIn(uint64_t(int(p.x * 64.f)) << 32 ^ uint64_t(uint32_t(int(p.y * 64.f))));
        mixIn(uint64_t(int(p.r * 64.f)) << 1 | (p.dim ? 1 : 0));
    }
    mixIn(uint64_t(uint32_t(rq)));
    return h;
}

// Signed distance to a flat-top hex's edge (negative inside), corner radius R.
inline float hexDist(float dx, float dy, float R) {
    const float ax = std::fabs(dx), ay = std::fabs(dy);
    return std::max(ay - kS3 * R, kS3 * ax + 0.5f * ay - kS3 * R);
}

void buildPlanet(PlanetDraw& pd, const Tex& alb, int W, int H) {
    const float r = pd.r;
    pd.bx = std::max(0, int(std::floor(pd.x - r - 1.f)));
    pd.by = std::max(0, int(std::floor(pd.y - r - 1.f)));
    pd.bw = std::min(W, int(std::ceil(pd.x + r + 1.f))) - pd.bx;
    pd.bh = std::min(H, int(std::ceil(pd.y + r + 1.f))) - pd.by;
    if (pd.bw <= 0 || pd.bh <= 0) return;
    pd.lut.assign(size_t(pd.bw) * size_t(pd.bh), PlanetPix{0, 0, 0, 0, 0, 0});
    // light from the upper left, a little towards us; the axis tilted back
    float Lx = -0.62f, Ly = -0.5f, Lz = 0.6f;
    const float ln = std::sqrt(Lx * Lx + Ly * Ly + Lz * Lz);
    Lx /= ln, Ly /= ln, Lz /= ln;
    const float tilt = 0.32f, ct = std::cos(tilt), st = std::sin(tilt);
    const float tw = float(alb.w), th = float(alb.h);
    for (int y = 0; y < pd.bh; ++y)
        for (int x = 0; x < pd.bw; ++x) {
            const float dx = (float(pd.bx + x) + 0.5f - pd.x) / r, dy = (float(pd.by + y) + 0.5f - pd.y) / r;
            const float d2 = dx * dx + dy * dy, d = std::sqrt(d2);
            const float cov = std::max(0.f, std::min(1.f, (1.f - d) * r + 0.5f));
            if (cov <= 0.f) continue;
            const float nx = std::min(1.f, dx), ny = dy, nz = std::sqrt(std::max(0.f, 1.f - std::min(1.f, d2)));
            // rotate into the planet's frame (tilted axis)
            const float py = ny * ct - nz * st, pz = ny * st + nz * ct;
            const float lon = std::atan2(nx, pz), lat = std::asin(std::max(-1.f, std::min(1.f, py)));
            PlanetPix& q = pd.lut[size_t(y) * size_t(pd.bw) + size_t(x)];
            q.u = int32_t((lon / (2.f * kPi) + 0.5f) * tw * 256.f - 128.f);
            q.v = uint16_t(std::max(0.f, (lat / kPi + 0.5f) * th * 256.f - 128.f));
            q.cov = uint8_t(cov * 255.f + 0.5f);
            const float lam = std::max(0.f, nx * Lx + ny * Ly + nz * Lz);
            q.lam = uint8_t(std::min(255.f, lam * 255.f));
            const float away = std::max(0.f, -(nx * Lx + ny * Ly) / std::sqrt(Lx * Lx + Ly * Ly));
            const float edge = 1.f - nz;
            q.rim = uint8_t(std::min(255.f, away * edge * edge * 255.f * 1.15f));
            q.fres = uint8_t(std::min(255.f, edge * edge * edge * std::min(1.f, lam * 1.6f + 0.15f) * 255.f));
        }
}

void buildBase(Built& b, const retro::Tile& t, float R, Pack& pk) {
    b.R = R;
    b.W = int(std::ceil(2.f * R)) + 2;
    b.H = int(std::ceil(2.f * kS3 * R)) + 2;
    b.cx = float(b.W) * 0.5f;
    b.cy = float(b.H) * 0.5f;
    const size_t n = size_t(b.W) * size_t(b.H);
    b.base.assign(n, 0);
    b.mask.assign(n, 0);
    b.latt.assign(n, 0);
    b.frame.reset(new Canvas(b.W, b.H, 1.f));
    const char* sn = skyName(t);
    auto sit = sn ? pk.skies.find(sn) : pk.skies.end();
    b.sky = sit != pk.skies.end() ? &sit->second : nullptr;
    b.hyper = sn && !std::strcmp(sn, "hyper");
    const Tex* sky = b.sky ? texRgb(b.sky->name) : nullptr;
    // the sky fills the hex's box: (-R, -sqrt3/2 R) .. (R, sqrt3/2 R); starfields are mirrored by the seed
    const uint32_t hs = hash32(t.seed * 7919u + 17u);
    const bool fx = b.sky && b.sky->name.compare(0, 4, "star") == 0 && (hs & 1u);
    const bool fy = b.sky && b.sky->name.compare(0, 4, "star") == 0 && (hs & 2u);
    const float tk = sky ? float(sky->w) * 0.5f / R : 0.f;  // texels per tile pixel
    const float lh = kS3 * R / 3.f;                           // the lattice: three strips from the middle to an edge
    const float lw = std::max(0.55f, R / 220.f);              // its lines' half width
    const float band = std::max(2.f, R * 0.06f);              // the darker band inside the edge
    const float rule = R * 0.045f;                            // the inner rule
    for (int y = 0; y < b.H; ++y) {
        const float dy = float(y) + 0.5f - b.cy;
        for (int x = 0; x < b.W; ++x) {
            const float dx = float(x) + 0.5f - b.cx;
            const float d = hexDist(dx, dy, R);
            const float cov = 0.5f - d;
            if (cov <= 0.f) continue;
            const size_t i = size_t(y) * size_t(b.W) + size_t(x);
            b.mask[i] = uint8_t(std::min(1.f, cov) * 255.f + 0.5f);
            uint32_t col = rgb(14, 8, 34);
            if (sky) {
                const float sx = fx ? -dx : dx, sy = fy ? -dy : dy;
                const int fu = int((sx * tk + float(sky->w) * 0.5f - 0.5f) * 256.f);
                const int fv = int((sy * tk + float(sky->h) * 0.5f - 0.5f) * 256.f);
                col = sampleClamp(*sky, fu, fv) & 0xFFFFFFu;
            }
            col = scaleRGB(col, 212);  // a little darker than the painting: the pieces go on top
            // the triangle lattice
            float la = 0.f;
            const float proj[3] = {dy, kS3 * dx + 0.5f * dy, kS3 * dx - 0.5f * dy};
            for (float pj : proj) {
                const float q = pj / lh;
                const float dist = std::fabs(q - std::floor(q + 0.5f)) * lh;
                la = std::max(la, 1.f - dist / lw);
            }
            if (la > 0.f) {
                b.latt[i] = uint8_t(la * 255.f);
                col = mix(col, kLattice, int(la * 30.f));
            }
            // the printed tile's edge: darker inside, a fine pink rule, a pale outer line
            const float e = -d;
            const float k = smoothstep(0.f, band, e);
            col = scaleRGB(col, int(140.f + 116.f * k));
            const float rl = 1.f - std::fabs(e - rule) / std::max(0.6f, R / 200.f);
            if (rl > 0.f) col = mix(col, kRule, int(rl * 80.f));
            if (e < 1.8f) col = mix(col, kEdgeHi, int((1.f - e / 1.8f) * 170.f));
            b.base[i] = col;
        }
    }
    // (0.43.1) the pulse mask's pixels (the Fracture's cracks), sampled like the sky
    b.pulse.clear();
    if (b.sky && !b.sky->pulseMask.empty())
        if (const Grey* pm = texGrey(b.sky->pulseMask)) {
            const float pk = float(pm->w) * 0.5f / R;
            for (int y = 0; y < b.H; ++y) {
                const float dy = float(y) + 0.5f - b.cy;
                for (int x = 0; x < b.W; ++x) {
                    const size_t i = size_t(y) * size_t(b.W) + size_t(x);
                    if (!b.mask[i]) continue;
                    const float dx = float(x) + 0.5f - b.cx;
                    const int fu = int((dx * pk + float(pm->w) * 0.5f - 0.5f) * 256.f);
                    const int fv = int((dy * pk + float(pm->h) * 0.5f - 0.5f) * 256.f);
                    const int m = int(sampleGreyClamp(*pm, fu, fv));
                    if (m < 10) continue;
                    const float rr = std::min(1.f, std::hypot(dx, dy) / R);
                    b.pulse.push_back({uint32_t(i), uint8_t(m), uint16_t(rr * 65535.f)});
                }
            }
        }
    // per row: the run of whole pixels (copied straight, no blending)
    b.fullL.assign(size_t(b.H), 0);
    b.fullR.assign(size_t(b.H), 0);
    for (int y = 0; y < b.H; ++y) {
        const uint8_t* m = &b.mask[size_t(y) * size_t(b.W)];
        int l = 0, r = b.W;
        while (l < b.W && m[l] < 255) ++l;
        while (r > l && m[r - 1] < 255) --r;
        b.fullL[size_t(y)] = int16_t(l);
        b.fullR[size_t(y)] = int16_t(r);
    }
    // pieces in the tile's pixels
    const float k = R / retro::kArtR;
    b.planets.clear();
    for (const retro::Piece& p : t.pieces) {
        if (p.kind != retro::Piece::Planet || p.id < 0 || p.id >= ti::kPlanetCount) continue;
        PlanetDraw pd;
        pd.id = p.id;
        const int ri = pk.planetOf[size_t(p.id)];
        pd.rec = ri >= 0 ? &pk.planets[size_t(ri)] : nullptr;
        pd.x = b.cx + p.x * k;
        pd.y = b.cy + p.y * k;
        pd.r = std::max(1.5f, p.r * k);
        pd.dim = p.dim;
        pd.station = (ti::kPlanets[p.id].flags & ti::kPlanetStation) != 0;
        const Tex* alb = pd.rec && !pd.station ? texRgb("p_" + pd.rec->key) : nullptr;
        if (alb) buildPlanet(pd, *alb, b.W, b.H);
        // its atmosphere's glow, into the sky
        if (pd.rec && pd.rec->hasAtmo && !pd.station) {
            const uint32_t ac = pd.rec->atmo;
            const float r = pd.r, reach = r * 1.7f;
            const int x0 = std::max(0, int(pd.x - reach)), x1 = std::min(b.W, int(pd.x + reach) + 1);
            const int y0 = std::max(0, int(pd.y - reach)), y1 = std::min(b.H, int(pd.y + reach) + 1);
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    const size_t i = size_t(y) * size_t(b.W) + size_t(x);
                    if (!b.mask[i]) continue;
                    const float dd = std::hypot(float(x) + 0.5f - pd.x, float(y) + 0.5f - pd.y) / r;
                    if (dd < 0.92f || dd > 1.7f) continue;
                    const float g1 = 1.f / (1.f + ((dd - 1.f) / 0.1f) * ((dd - 1.f) / 0.1f));
                    const float g2 = smoothstep(1.7f, 1.f, dd) * 0.35f;
                    b.base[i] = addCol(b.base[i], ac, int((g1 * 0.55f + g2) * 150.f));
                }
        }
        b.planets.push_back(std::move(pd));
    }
    if (!b.sky && !t.lanes) {  // nothing printed: only the pieces show
        for (size_t i = 0; i < n; ++i) {
            if (!b.mask[i]) continue;
            const float x = float(i % size_t(b.W)) + 0.5f, y = float(i / size_t(b.W)) + 0.5f;
            float keep = 0.f;
            for (const PlanetDraw& pd : b.planets) {
                const float dd = std::hypot(x - pd.x, y - pd.y) / pd.r;
                keep = std::max(keep, smoothstep(1.75f, 1.05f, dd));
            }
            b.mask[i] = uint8_t(float(b.mask[i]) * keep);
        }
        return;
    }
    // stars that twinkle
    b.stars.clear();
    const int ns = b.sky ? b.sky->twinkles : 6;
    for (int s = 0; s < ns; ++s) {
        const uint32_t h = t.seed * 131u + uint32_t(s) * 977u + 5u;
        Star st;
        st.x = b.cx + (rnd01(h) * 2.f - 1.f) * R * 0.85f;
        st.y = b.cy + (rnd01(h + 1) * 2.f - 1.f) * R * 0.72f;
        if (hexDist(st.x - b.cx, st.y - b.cy, R) > -R * 0.08f) continue;
        bool onPlanet = false;
        for (const PlanetDraw& pd : b.planets)
            if (std::hypot(st.x - pd.x, st.y - pd.y) < pd.r * 1.3f) onPlanet = true;
        if (onPlanet) continue;
        st.size = std::max(0.7f, R / 130.f) * (0.7f + rnd01(h + 2) * 0.8f);
        st.period = 1800u + (hash32(h + 3) % 3400u);
        st.phase = hash32(h + 4) % st.period;
        st.col = rnd01(h + 5) < 0.35f ? rgb(255, 214, 180) : rnd01(h + 6) < 0.5f ? rgb(255, 170, 230) : rgb(214, 210, 255);
        b.stars.push_back(st);
    }
    // hyperlanes: glowing paths painted into the base
    if (t.lanes) {
        Canvas tmp(b.W, b.H, 1.f);
        std::memcpy(tmp.pixels(), b.base.data(), n * 4);
        for (int a = 0; a < 6; ++a)
            for (int c2 = a + 1; c2 < 6; ++c2) {
                if (!((t.lanes >> (a * 6 + c2)) & 1u)) continue;
                const float a0 = -kPi / 2.f + float(a) * kPi / 3.f, a1 = -kPi / 2.f + float(c2) * kPi / 3.f;
                const float x0 = b.cx + std::cos(a0) * kS3 * R, y0 = b.cy + std::sin(a0) * kS3 * R;
                const float x1 = b.cx + std::cos(a1) * kS3 * R, y1 = b.cy + std::sin(a1) * kS3 * R;
                float px = x0, py = y0;
                for (int s = 1; s <= 24; ++s) {
                    const float u = float(s) / 24.f, w0 = (1 - u) * (1 - u), w1 = 2 * (1 - u) * u, w2 = u * u;
                    const float qx = w0 * x0 + w1 * b.cx + w2 * x1, qy = w0 * y0 + w1 * b.cy + w2 * y1;
                    tmp.line(px, py, qx, qy, rgb(255, 70, 170), std::max(3.f, R * 0.16f), 40);
                    tmp.line(px, py, qx, qy, rgb(255, 110, 200), std::max(1.5f, R * 0.07f), 110);
                    tmp.line(px, py, qx, qy, rgb(255, 220, 245), std::max(1.f, R * 0.018f), 220);
                    px = qx;
                    py = qy;
                }
            }
        std::memcpy(b.base.data(), tmp.pixels(), n * 4);
    }
}

// ---- the moving parts
// Where the moving layer is now, in whole pixels (or turn steps): the layer is only redrawn when this changes.
int64_t flowKeyOf(const Built& b, const Sky& s, uint32_t now) {
    const float t = float(now % 3600000u) / 1000.f;
    if (s.motion == Drift) {
        const int n = std::max(16, int(2.2f * b.R + 0.5f));
        const float sp = s.speed * float(n) / 64.f * 0.5f;
        return int64_t(std::floor(sp * t)) * 7919 + int64_t(std::floor(sp * 0.6f * t));
    }
    if (s.motion == Turn) return int64_t(std::floor(s.speed * 0.25f * t * b.R * 0.9f));  // a pixel at the rim
    const Tex* fl = texRgb(s.flow);
    if (!fl) return 0;
    const float tk = float(fl->w) / (2.4f * b.R);
    return int64_t(std::floor(s.speed * float(fl->w) / 90.f * t / tk));  // slide: a pixel at a time
}
// the second layer as a Sky of its own
Sky secondLayer(const Sky& s) {
    Sky s2 = s;
    s2.flow = s.flow2;
    s2.motion = s.motion2;
    s2.speed = s.speed2;
    return s2;
}
int64_t flowKey(const Built& b, uint32_t now) {
    if (!b.sky) return 0;
    int64_t k = flowKeyOf(b, *b.sky, now);
    if (!b.sky->flow2.empty()) k = k * 1000003 + flowKeyOf(b, secondLayer(*b.sky), now);
    if (!b.pulse.empty()) k = k * 1000033 + int64_t(now / 40u);  // (0.43.1) the pulse: 25 steps a second
    return k;
}

void drawFlowOf(Built& b, const Sky& s, uint32_t now, uint32_t* px) {
    const float t = float(now % 3600000u) / 1000.f;
    if (s.motion == Drift) {
        // the veil, made once at this size as a tileable picture in the tile's own pixels (one period =
        // a little more than the tile); each step it is only shifted - two copies going different ways
        const VeilPlane* vp = veilPlane(s.flow, b.R);
        if (!vp) return;
        const float sp = s.speed * float(vp->n) / 64.f * 0.5f;  // pixels a second
        const float ang = float(hash32(uint32_t(b.W) * 31u + (s.name.empty() ? 0u : uint32_t(s.name[s.name.size() - 1]))) % 628u) / 100.f;
        const int n = vp->n;
        auto wrapI = [n](float v) { int i = int(std::floor(v)) % n; return i < 0 ? i + n : i; };
        const int ox = wrapI(std::cos(ang) * sp * t), oy = wrapI(std::sin(ang) * sp * t);
        const int ox2 = wrapI(-std::cos(ang + 0.9f) * sp * 0.6f * t + float(n) * 0.37f);
        const int oy2 = wrapI(-std::sin(ang + 0.9f) * sp * 0.6f * t + float(n) * 0.61f);
        // the tint, as a table: veil level -> colour to add
        uint32_t lut[256];
        const uint32_t vc = s.veil;
        for (int a = 0; a < 256; ++a) lut[a] = scaleRGB(vc, a * 62 >> 8);
        const uint8_t* V = vp->v.data();
        for (int y = 0; y < b.H; ++y) {
            const size_t row = size_t(y) * size_t(b.W);
            const uint8_t* r1 = V + size_t((y + oy) % n) * size_t(n);
            const uint8_t* r2 = V + size_t((y + oy2) % n) * size_t(n);
            int x1 = ox, x2 = ox2;
            for (int x = 0; x < b.W; ++x) {
                const int a = (int(r1[x1]) * 3 + int(r2[x2])) >> 2;
                if (++x1 == n) x1 = 0;
                if (++x2 == n) x2 = 0;
                if (a < 6 || !b.mask[row + size_t(x)]) continue;
                const uint32_t add = lut[a], d = px[row + size_t(x)];
                // per-channel saturating add (7 bits a channel, the lowest bit kept from either)
                const uint32_t sum = (d & 0xFEFEFEu) + (add & 0xFEFEFEu);
                const uint32_t over = (sum >> 8) & 0x010101u;
                px[row + size_t(x)] = (sum | (over * 0xFEu)) & 0xFEFEFEu;
            }
        }
        return;
    }
    const Tex* fl = texRgb(s.flow);
    if (!fl) return;
    if (s.motion == Turn) {
        const float w = s.speed * 0.25f * t;  // radians
        const float c = std::cos(w), sn = std::sin(w);
        const float tk = float(fl->w) * 0.5f / b.R;
        const float half = float(fl->w) * 0.5f - 0.5f;
        for (int y = 0; y < b.H; ++y) {
            const float dy = float(y) + 0.5f - b.cy;
            const float dx0 = 0.5f - b.cx;
            // u = (c dx - s dy) tk + half, v = (s dx + c dy) tk + half: linear along the row
            int fu = int(((c * dx0 - sn * dy) * tk + half) * 256.f), fv = int(((sn * dx0 + c * dy) * tk + half) * 256.f);
            const int du = int(c * tk * 256.f), dv = int(sn * tk * 256.f);
            const size_t row = size_t(y) * size_t(b.W);
            for (int x = 0; x < b.W; ++x, fu += du, fv += dv) {
                if (!b.mask[row + size_t(x)]) continue;
                if (fu < 0 || fv < 0 || (fu >> 8) >= fl->w - 1 || (fv >> 8) >= fl->h - 1) continue;
                const uint32_t q = sampleClamp(*fl, fu, fv);
                if (!(q & 0xF0F0F0u)) continue;
                px[row + size_t(x)] = addRGB(px[row + size_t(x)], int((q >> 16) & 255), int((q >> 8) & 255), int(q & 255));
            }
        }
        return;
    }
    // Slide: a tileable picture with alpha moving across (asteroids)
    const float tk = float(fl->w) / (2.4f * b.R);
    const float sp = s.speed * float(fl->w) / 90.f;
    const int ou = int(sp * t * 256.f), ov = int(sp * 0.35f * t * 256.f);
    const int du = int(tk * 256.f);
    for (int y = 0; y < b.H; ++y) {
        const int fv = int(float(y) * tk * 256.f) + ov;
        int fu = ou + 97 * 256;
        const size_t row = size_t(y) * size_t(b.W);
        for (int x = 0; x < b.W; ++x, fu += du) {
            if (!b.mask[row + size_t(x)]) continue;
            const uint32_t q = sampleWrap(*fl, fu, fv);
            const uint32_t a = q >> 24;
            if (a < 4) continue;
            // premultiplied over
            const uint32_t d = px[row + size_t(x)];
            px[row + size_t(x)] = addRGB(scaleRGB(d, int(256 - a - (a >> 7))), int((q >> 16) & 255), int((q >> 8) & 255), int(q & 255));
        }
    }
}

void drawFlow(Built& b, uint32_t now, uint32_t* px) {
    if (!b.sky) return;
    drawFlowOf(b, *b.sky, now, px);
    if (!b.sky->flow2.empty()) drawFlowOf(b, secondLayer(*b.sky), now, px);
    // (0.43.1) the pulse: bright waves running along the mask, out from the middle (or in to it)
    if (!b.pulse.empty()) {
        const Sky& s = *b.sky;
        const float t = float(now % 3600000u) / 1000.f;
        const float base = 2.f * kPi * t / s.pulsePeriod;
        const float waves = 2.f * kPi * 1.6f * float(s.pulseDir) / 65535.f;
        const int cr = int((s.pulseCol >> 16) & 255), cg = int((s.pulseCol >> 8) & 255), cb = int(s.pulseCol & 255);
        // the wave's shape as a table: phase (256 steps a turn) -> strength 0..256
        int shape[256];
        for (int k = 0; k < 256; ++k) {
            const float sn = std::sin(float(k) / 256.f * 2.f * kPi);
            const float w = sn > 0.f ? sn * sn * sn * sn * sn * sn : 0.f;
            shape[k] = int((0.16f + 0.84f * w) * 256.f);
        }
        for (const Built::PulsePx& q : b.pulse) {
            const float ph = base - waves * float(q.rr);
            const int k = int(ph / (2.f * kPi) * 256.f) & 255;
            const int a = (int(q.m) * shape[k]) >> 8;  // 0..255
            px[q.i] = addRGB(px[q.i], cr * a >> 8, cg * a >> 8, cb * a >> 8);
        }
    }
}

void drawShimmer(Built& b, uint32_t now, Canvas& f) {
    // a pink light sweeping across the lattice every few seconds
    const uint32_t per = 7000u + uint32_t(b.W % 7) * 450u;
    const float ph = float((now + uint32_t(b.H) * 37u) % per) / float(per);
    const float span = 2.6f * b.R;
    const float pos = -1.3f * b.R + ph * span * 1.6f;  // part of the time it is off the tile
    const float wdt = b.R * 0.28f;
    uint32_t* px = f.pixels();
    for (int y = 0; y < b.H; ++y) {
        const float dy = float(y) + 0.5f - b.cy;
        const size_t row = size_t(y) * size_t(b.W);
        // along (cos 30, sin 30)
        const float base = 0.5f * dy;
        const float lo = (pos - wdt - base) / kS3 + b.cx, hi = (pos + wdt - base) / kS3 + b.cx;
        const int x0 = std::max(0, int(lo)), x1 = std::min(b.W, int(hi) + 1);
        for (int x = x0; x < x1; ++x) {
            const uint8_t l = b.latt[row + size_t(x)];
            if (!l) continue;
            const float dx = float(x) + 0.5f - b.cx;
            const float pj = kS3 * dx + base;
            const float k = 1.f - std::fabs(pj - pos) / wdt;
            if (k <= 0.f) continue;
            px[row + size_t(x)] = addCol(px[row + size_t(x)], kShimmer, int(k * k * float(l) * 0.75f));
        }
    }
}

void drawStars(Built& b, uint32_t now, Canvas& f) {
    for (const Star& s : b.stars) {
        const float ph = float((now + s.phase) % s.period) / float(s.period);
        float k = 0.5f + 0.5f * std::cos(ph * 2.f * kPi);
        k = k * k * k * k;
        if (k < 0.04f) continue;
        const float len = s.size * (1.5f + 3.f * k);
        f.fillCircle(s.x, s.y, s.size * 1.4f, s.col, int(70.f * k));
        f.line(s.x - len, s.y, s.x + len, s.y, s.col, std::max(0.6f, s.size * 0.5f), int(150.f * k));
        f.line(s.x, s.y - len, s.x, s.y + len, s.col, std::max(0.6f, s.size * 0.5f), int(150.f * k));
        f.fillCircle(s.x, s.y, std::max(0.6f, s.size * 0.6f), rgb(255, 255, 255), int(230.f * k));
    }
}

void drawPlanet(PlanetDraw& pd, uint32_t now, Canvas& f, int W) {
    if (!pd.rec || pd.lut.empty()) return;
    const PlanetRec& r = *pd.rec;
    const Tex* alb = texRgb("p_" + r.key);
    if (!alb) return;
    const Grey* em = r.hasE ? texGrey("e_" + r.key) : nullptr;
    const Grey* cl = r.hasC ? texGrey("c_" + r.key) : nullptr;
    const uint32_t per = uint32_t(std::max(4.f, r.period) * 1000.f);
    const int turn = int(int64_t(now % per) * int64_t(alb->w) * 256 / int64_t(per));
    const int turnC = int(int64_t(now % (per * 4 / 5)) * int64_t(cl ? cl->w : 1) * 256 / int64_t(per * 4 / 5));
    const float pulse = 0.78f + 0.22f * std::sin(float(now % 100000u) * 0.0021f + float(pd.id));
    const int amb = int(r.ambient * 256.f);
    const uint32_t ec = r.emis, rc = r.rim, ac = r.hasAtmo ? r.atmo : rgb(190, 170, 255);
    const int er = int((ec >> 16) & 255), eg = int((ec >> 8) & 255), eb = int(ec & 255);
    uint32_t* px = f.pixels();
    const float ew = em ? float(em->w) / float(alb->w) : 1.f;
    // repaint only when the surface has turned by a texel, the clouds moved or the glow changed a step
    const int64_t key = (int64_t(turn >> 8) << 32) ^ (int64_t(turnC >> 8) << 12) ^ int64_t(em ? int(pulse * 24.f) : 0);
    const size_t n = size_t(pd.bw) * size_t(pd.bh);
    if (pd.img.size() != n || key != pd.imgKey) {
        pd.img.resize(n);
        pd.imgKey = key;
    } else {
        for (int y = 0; y < pd.bh; ++y) {
            const PlanetPix* q = &pd.lut[size_t(y) * size_t(pd.bw)];
            const uint32_t* in = &pd.img[size_t(y) * size_t(pd.bw)];
            uint32_t* out = px + size_t(pd.by + y) * size_t(W) + size_t(pd.bx);
            for (int x = 0; x < pd.bw; ++x)
                if (q[x].cov) out[x] = q[x].cov >= 255 ? in[x] : mix(out[x], in[x], q[x].cov);
        }
        return;
    }
    for (int y = 0; y < pd.bh; ++y) {
        const PlanetPix* q = &pd.lut[size_t(y) * size_t(pd.bw)];
        uint32_t* out = px + size_t(pd.by + y) * size_t(W) + size_t(pd.bx);
        uint32_t* keep = &pd.img[size_t(y) * size_t(pd.bw)];
        for (int x = 0; x < pd.bw; ++x, ++q) {
            if (!q->cov) continue;
            const uint32_t a = samplePlanet(*alb, q->u + turn, q->v);
            // day light: warm, ambient violet at night
            const int lam = q->lam;
            const int lit = amb + ((256 - amb) * lam >> 8);
            int R = int((a >> 16) & 255) * lit >> 8, G = int((a >> 8) & 255) * lit >> 8, B = int(a & 255) * lit >> 8;
            R = R * 268 >> 8;  // warm key
            B += (255 - lam) * 18 >> 8;  // violet night
            R += (255 - lam) * 6 >> 8;
            if (cl) {  // clouds, turning a little faster
                const int cu = int(float(q->u) * float(cl->w) / float(alb->w)) + turnC;
                const int cv = int(float(q->v) * float(cl->h) / float(alb->h));
                const int c = greyNearest(*cl, cu, cv) * 220 >> 8;
                const int cb = 30 + (lam * 225 >> 8);
                R += (cb - R) * c >> 8;
                G += (cb - G) * c >> 8;
                B += ((cb + 10 > 255 ? 255 : cb + 10) - B) * c >> 8;
            }
            if (em) {  // night lights / glowing cracks: brighter away from the sun
                const int e = greyNearest(*em, int(float(q->u + turn) * ew), int(float(q->v) * ew));
                if (e > 8) {
                    const int night = 255 - std::min(255, lam * 2);
                    const int k = int(float(e * (80 + night * 176 / 255) >> 8) * pulse);
                    R += er * k >> 8;
                    G += eg * k >> 8;
                    B += eb * k >> 8;
                }
            }
            // the magenta rim on the far side, the atmosphere on the lit edge
            R += int((rc >> 16) & 255) * q->rim >> 8;
            G += int((rc >> 8) & 255) * q->rim >> 8;
            B += int(rc & 255) * q->rim >> 8;
            R += int((ac >> 16) & 255) * q->fres * 3 >> 10;
            G += int((ac >> 8) & 255) * q->fres * 3 >> 10;
            B += int(ac & 255) * q->fres * 3 >> 10;
            uint32_t col = rgb(R > 255 ? 255 : R, G > 255 ? 255 : G, B > 255 ? 255 : B);
            if (pd.dim) {  // exhausted: grey and dark
                const int l = (R * 3 + G * 6 + B) / 10;
                col = mix(rgb(l, l, l), col, 90);
                col = scaleRGB(col, 140);
            }
            keep[x] = col;
            out[x] = q->cov >= 255 ? col : mix(out[x], col, q->cov);
        }
    }
}

// Thunder's Edge (0.34.0): a space station, painted per pixel - a tilted steel ring turning slowly
// round a lit hub on four spokes, a mast with a blinking light; key light from the upper left, violet
// shadows as the rest of the tileset.
void drawStation(const PlanetDraw& pd, uint32_t now, Canvas& f, const Built& b) {
    const float r = pd.r * 1.08f;
    if (r < 2.f) return;
    const float tilt = 0.42f, ringR = 0.82f, ringW = 0.15f;
    const float spin = float(now % 60000u) / 60000.f * 2.f * kPi;
    const int x0 = std::max(0, int(pd.x - r * 1.05f)), x1 = std::min(b.W, int(pd.x + r * 1.05f) + 1);
    const int y0 = std::max(0, int(pd.y - r * 1.02f)), y1 = std::min(b.H, int(pd.y + r * 0.5f) + 1);
    const float aa = 1.f / r;  // one pixel in station units
    const uint32_t steelHi = pd.dim ? rgb(130, 128, 146) : rgb(226, 230, 248), steelLo = pd.dim ? rgb(34, 28, 48) : rgb(44, 32, 74);
    const uint32_t lamp = pd.dim ? rgb(150, 110, 70) : rgb(255, 196, 110), win = pd.dim ? rgb(80, 110, 130) : rgb(140, 230, 255);
    const bool blink = !pd.dim && (now % 1600u) < 380u;
    uint32_t* px = f.pixels();
    float sx[4], sy[4];
    for (int k = 0; k < 4; ++k) sx[k] = std::cos(spin * 0.5f + float(k) * kPi / 2.f), sy[k] = std::sin(spin * 0.5f + float(k) * kPi / 2.f);
    auto cov = [&](float d) { return d <= -aa ? 1.f : d >= aa ? 0.f : 0.5f - 0.5f * d / aa; };  // d < 0 inside
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const size_t i = size_t(y) * size_t(b.W) + size_t(x);
            if (!b.mask[i]) continue;
            const float u = (float(x) + 0.5f - pd.x) / r, v = (float(y) + 0.5f - pd.y) / r;
            const float rv = v / tilt, rr = std::sqrt(u * u + rv * rv);
            uint32_t col = px[i];
            const float sRing = (rr - ringR) / ringW;
            const float ringC = cov((std::fabs(rr - ringR) - ringW) * tilt);
            const float th = fastAtan2(rv, u);
            float seam = (th - spin) * 12.f / (2.f * kPi);
            seam -= std::floor(seam);
            const float lit = std::max(0.f, std::min(1.f, 0.58f + 0.32f * fastCos(th + 2.36f) - 0.25f * std::fabs(sRing) - (seam < 0.1f ? 0.25f : 0.f)));
            auto ringCol = [&](float shade) {
                uint32_t c = mix(steelLo, steelHi, int(lit * shade * 255.f));
                if (std::fabs(seam - 0.5f) < 0.07f && std::fabs(sRing) < 0.45f) c = lamp;
                return c;
            };
            auto spokeC = [&](bool front) {
                float best = 0.f;
                for (int k = 0; k < 4; ++k) {
                    if ((sy[k] > 0.f) != front) continue;
                    const float t2 = std::max(0.f, std::min(ringR, u * sx[k] + rv * sy[k]));
                    const float dx = u - t2 * sx[k], dy = rv - t2 * sy[k];
                    best = std::max(best, cov((std::sqrt(dx * dx + dy * dy) - 0.06f) * tilt));
                }
                return best;
            };
            if (rv < 0.f && ringC > 0.f) col = mix(col, ringCol(0.62f), int(ringC * 255.f));  // the far half
            const float sb = spokeC(false);
            if (sb > 0.f) col = mix(col, mix(steelLo, steelHi, 90), int(sb * 255.f));
            const float mastC = std::min(cov(std::fabs(u) - 0.045f), cov(v > -0.25f ? v + 0.25f : (v < -0.95f ? -0.95f - v : -1.f)));
            if (mastC > 0.f) col = mix(col, mix(steelLo, steelHi, 150), int(mastC * 255.f));
            const float hu = u / 0.34f, hv = (v + 0.04f) / 0.34f, hd = hu * hu + hv * hv;
            const float hubC = cov((std::sqrt(hd) - 1.f) * 0.34f);
            if (hubC > 0.f) {
                const float nz = std::sqrt(std::max(0.f, 1.f - hd));
                const float lam = std::max(0.f, (-0.6f * hu - 0.6f * hv + 0.55f * nz) / 0.98f);
                uint32_t c = mix(steelLo, steelHi, int((0.12f + 0.88f * lam) * 255.f));
                float wa = (fastAtan2(hv, hu) + spin) * 8.f / (2.f * kPi);
                wa -= std::floor(wa);
                if (std::fabs(hv) < 0.15f && wa < 0.5f && nz > 0.3f) c = mix(c, win, 200);
                c = addCol(c, rgb(255, 120, 200), int(std::max(0.f, 1.f - nz * 3.f) * 70.f));  // the magenta rim
                col = mix(col, c, int(hubC * 255.f));
            }
            const float sf = spokeC(true);
            if (sf > 0.f && hubC < 1.f) col = mix(col, mix(steelLo, steelHi, 170), int(sf * (1.f - hubC) * 255.f));
            if (rv >= 0.f && ringC > 0.f) col = mix(col, ringCol(1.f), int(ringC * 255.f));  // the near half
            if (blink) {
                const float bd = std::sqrt(u * u + (v + 0.97f) * (v + 0.97f));
                if (bd < 0.12f) col = addCol(col, rgb(255, 60, 70), int((1.f - bd / 0.12f) * 255.f));
            }
            px[i] = col;
        }
}

void drawPlanetRing(const PlanetDraw& pd, uint32_t now, Canvas& f) {
    // the Voidfall sector tiles' sighting ring: a thin circle, four ticks turning slowly
    if (pd.r < 9.f) return;
    const float rr = pd.r * 1.24f;
    const uint32_t col = pd.dim ? rgb(150, 140, 180) : rgb(214, 196, 255);
    f.ring(pd.x, pd.y, rr, 1.f, col, 70);
    const float a0 = float(now % 60000u) / 60000.f * 2.f * kPi + float(pd.id);
    for (int k = 0; k < 4; ++k) {
        const float a = a0 + float(k) * kPi / 2.f, c = std::cos(a), s = std::sin(a);
        f.line(pd.x + c * (rr - 2.5f), pd.y + s * (rr - 2.5f), pd.x + c * (rr + 3.5f), pd.y + s * (rr + 3.5f), col, 1.2f, 170);
    }
}

// a wormhole: a turning spiral round a dark eye, in its colour
void drawWormhole(Built& b, float wx, float wy, float rv, int kind, uint32_t now, Canvas& f, bool storm) {
    static const uint32_t cols[5][3] = {{rgb(120, 40, 10), rgb(255, 150, 50), rgb(255, 230, 170)},     // alpha
                                        {rgb(10, 80, 40), rgb(70, 230, 140), rgb(200, 255, 220)},      // beta
                                        {rgb(80, 10, 90), rgb(230, 80, 230), rgb(255, 200, 255)},      // gamma
                                        {rgb(20, 40, 120), rgb(100, 170, 255), rgb(220, 240, 255)},    // delta
                                        {rgb(110, 8, 20), rgb(245, 60, 70), rgb(255, 210, 200)}};      // epsilon (Thunder's Edge)
    const uint32_t* cc = cols[std::max(0, std::min(4, kind))];
    const float t = float(now % 1000000u) / 1000.f;
    const float reach = rv * 1.6f;
    // (0.33.1) on a supernova's glare the wormhole needs a dark backing to be seen (Alpha Beta Nova)
    const bool glare = b.sky && (b.sky->name == "43" || b.sky->name == "80" || b.sky->name == "81");
    const int x0 = std::max(0, int(wx - reach)), x1 = std::min(b.W, int(wx + reach) + 1);
    const int y0 = std::max(0, int(wy - reach)), y1 = std::min(b.H, int(wy + reach) + 1);
    uint32_t* px = f.pixels();
    const int arms = storm ? 5 : 3;
    const float spin = storm ? 2.2f : 1.4f;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const size_t i = size_t(y) * size_t(b.W) + size_t(x);
            if (!b.mask[i]) continue;
            const float dx = float(x) + 0.5f - wx, dy = float(y) + 0.5f - wy;
            const float d = std::sqrt(dx * dx + dy * dy) / rv;
            if (d > 1.6f) continue;
            const float ang = fastAtan2(dy, dx);
            const float sp = 0.5f + 0.5f * fastCos(float(arms) * ang + 4.5f * fastLog(d + 0.08f) - t * spin);
            const float armsI = sp * sp * sp * smoothstep(0.18f, 0.45f, d) * smoothstep(1.45f, 0.85f, d);
            const float ring = 1.f / (1.f + ((d - 1.f) / 0.07f) * ((d - 1.f) / 0.07f));
            const float glow = smoothstep(1.6f, 0.2f, d) * 0.3f;
            uint32_t p = px[i];
            if (glare) p = mix(p, rgb(14, 4, 10), int(smoothstep(1.55f, 0.7f, d) * 190.f));
            p = mix(p, rgb(6, 2, 14), int(smoothstep(0.34f, 0.12f, d) * 235.f));  // the eye
            const float k = armsI * 0.95f + ring * 0.75f + glow;
            const uint32_t c = k > 0.7f ? mix(cc[1], cc[2], int((k - 0.7f) * 600.f)) : mix(cc[0], cc[1], int(k / 0.7f * 255.f));
            px[i] = addCol(p, c, int(std::min(1.f, k) * 256.f));
        }
    if (storm) {  // the ion storm crackles
        const uint32_t slot = now / 260u;
        for (int k = 0; k < 2; ++k) {
            const uint32_t h = hash32(slot * 5u + uint32_t(k) + uint32_t(kind) * 13u);
            float a = rnd01(h) * 2.f * kPi;
            float px0 = wx + std::cos(a) * rv * 0.3f, py0 = wy + std::sin(a) * rv * 0.3f;
            for (int s = 0; s < 4; ++s) {
                a += (rnd01(h + uint32_t(s) * 7u) - 0.5f) * 1.6f;
                const float len = rv * 0.32f;
                const float qx = px0 + std::cos(a) * len, qy = py0 + std::sin(a) * len;
                f.line(px0, py0, qx, qy, s & 1 ? rgb(120, 240, 255) : rgb(255, 140, 230), 1.2f, 210);
                px0 = qx;
                py0 = qy;
            }
        }
    }
}

void drawTear(Built& b, float tx, float ty, float r, uint32_t now, Canvas& f) {
    // the Vuil'raith Dimensional Tear: a red wound in space, breathing
    const float br = 1.f + 0.08f * std::sin(float(now % 100000u) * 0.004f);
    f.fillCircle(tx, ty, r * 1.25f * br, rgb(200, 20, 40), 40);
    f.fillCircle(tx, ty, r * 0.85f * br, rgb(255, 60, 60), 50);
    float xs[12], ys[12];
    for (int k = 0; k < 12; ++k) {
        const float a = float(k) / 12.f * 2.f * kPi;
        const float rr = (k % 2 ? 0.28f : 0.36f) * r * br * (std::fabs(std::sin(a)) * 0.6f + 0.4f);
        xs[k] = tx + std::cos(a) * r * 0.95f * br * (0.55f + 0.45f * std::fabs(std::cos(a)));
        ys[k] = ty + std::sin(a) * rr;
    }
    f.fillPolygon(xs, ys, 12, rgb(10, 0, 4), 240);
    f.line(tx - r * 0.9f * br, ty, tx + r * 0.9f * br, ty, rgb(255, 120, 100), 1.2f, 200);
    (void)b;
}

// The Vuil'raith home (owner's picture): pale tentacles out of the storm, wrapped round Acheron - some
// behind it, some across its face - swaying; red lightning in bursts.
void drawTentacle(Canvas& f, const PlanetDraw& p, int k, float t) {
    const float base = float(k) / 7.f * 2.f * kPi + 0.35f + 0.3f * float(k % 2);
    const float dir = k % 2 ? 1.f : -1.f;
    const float sweep = 1.25f + 0.35f * float((k * 7) % 3);
    const float sway = 0.18f * std::sin(t * 0.6f + float(k) * 1.7f);
    const float r0 = p.r * (3.3f + 0.3f * float((k * 5) % 3));  // from beyond the tile's edge
    const int segs = std::max(14, int(p.r * 0.9f));
    // the spine, from out in the cloud curling in round the planet
    std::vector<float> xs(size_t(segs) + 1), ys(size_t(segs) + 1), ws(size_t(segs) + 1);
    for (int s = 0; s <= segs; ++s) {
        const float u = float(s) / float(segs);
        const float curl = u > 0.82f ? (u - 0.82f) * 6.f : 0.f;  // the tip curls
        const float rad = r0 + (p.r * 1.02f - r0) * smoothstep(0.f, 0.7f, u) - curl * p.r * 0.12f +
                          p.r * 0.05f * std::sin(u * 7.f + t * 1.1f + float(k));
        const float a = base + sway * (1.f - u) + dir * (u * sweep + curl * 0.9f);
        xs[size_t(s)] = p.x + std::cos(a) * rad;
        ys[size_t(s)] = p.y + std::sin(a) * rad * 0.94f;
        ws[size_t(s)] = std::max(0.6f, p.r * (0.115f * float(std::pow(double(1.f - u), 0.9)) + 0.01f) * (1.f + 0.18f * std::sin(u * kPi * 1.3f)));
    }
    // a tube: overlapping discs along the spine - outline, flesh, a lit ridge
    auto tube = [&](float grow, uint32_t col, int alpha0, float shift) {
        for (int s = 0; s < segs; ++s) {
            const int alpha = int(float(alpha0) * std::min(1.f, float(s) / (float(segs) * 0.22f) + 0.15f));  // out of the storm
            const float x0 = xs[size_t(s)], y0 = ys[size_t(s)], x1 = xs[size_t(s) + 1], y1 = ys[size_t(s) + 1];
            const float w = ws[size_t(s)] + grow;
            if (w <= 0.3f) continue;
            const float len = std::hypot(x1 - x0, y1 - y0);
            const int n = std::max(1, int(len / std::max(0.7f, w * 0.45f)));
            for (int i = 0; i < n; ++i) {
                const float u = float(i) / float(n);
                f.fillCircle(x0 + (x1 - x0) * u - shift * w, y0 + (y1 - y0) * u - shift * w, w, col, alpha);
            }
        }
    };
    tube(std::max(1.f, ws[0] * 0.18f), rgb(24, 6, 18), 110, 0.f);
    tube(0.f, rgb(118, 118, 160), 175, 0.f);
    tube(-0.4f * ws[0], rgb(178, 180, 222), 130, 0.25f);
    tube(-0.75f * ws[0], rgb(236, 236, 255), 90, 0.4f);
    // rings of muscle and the suckers along the underside
    for (int s = 2; s < segs - 1; s += 2) {
        const float w = ws[size_t(s)];
        if (w < 1.6f) continue;
        const float dx = xs[size_t(s) + 1] - xs[size_t(s) - 1], dy = ys[size_t(s) + 1] - ys[size_t(s) - 1];
        const float l = std::max(0.01f, std::hypot(dx, dy)), nx = -dy / l, ny = dx / l;
        if ((s * 7 + k) % 3) f.line(xs[size_t(s)] - nx * w * 0.8f, ys[size_t(s)] - ny * w * 0.8f, xs[size_t(s)] + nx * w * 0.8f,
                                    ys[size_t(s)] + ny * w * 0.8f, rgb(70, 50, 90), std::max(0.6f, w * 0.12f), 45);
        if (s % 4 == 0) f.fillCircle(xs[size_t(s)] + nx * w * 0.55f, ys[size_t(s)] + ny * w * 0.55f, w * 0.26f, rgb(120, 40, 70), 150);
    }
}

void drawCabal(Built& b, uint32_t now, Canvas& f, bool front) {
    if (b.planets.empty()) return;
    const PlanetDraw& p = b.planets[0];
    const float t = float(now % 1000000u) / 1000.f;
    for (int k = 0; k < 7; ++k)
        if ((k % 3 == 0) == front) drawTentacle(f, p, k, t);
    if (!front) return;
    // red lightning in bursts
    const uint32_t per = 1500u, ph = now % per;
    if (ph < 300u) {
        const uint32_t slot = now / per;
        const int alpha = 255 - int(ph) * 200 / 300;
        for (int k = 0; k < 2; ++k) {
            const uint32_t h = hash32(slot * 3u + uint32_t(k));
            float a = rnd01(h) * 2.f * kPi;
            float x = b.cx + std::cos(a) * b.R * 0.75f, y = b.cy + std::sin(a) * b.R * 0.6f;
            const float to = a + kPi + (rnd01(h + 1) - 0.5f);
            for (int s = 0; s < 7; ++s) {
                const float d = to + (rnd01(h + uint32_t(s) * 11u) - 0.5f) * 1.4f;
                const float len = b.R * 0.13f;
                const float qx = x + std::cos(d) * len, qy = y + std::sin(d) * len;
                f.line(x, y, qx, qy, rgb(255, 60, 50), std::max(2.f, b.R * 0.025f), alpha / 3);
                f.line(x, y, qx, qy, rgb(255, 220, 210), std::max(1.f, b.R * 0.006f), alpha);
                x = qx;
                y = qy;
            }
        }
    }
}

void drawSpecial(Built& b, const retro::Tile& t, uint32_t now, Canvas& f) {
    if (!b.sky) return;
    const std::string& n = b.sky->name;
    const float tt = float(now % 1000000u) / 1000.f;
    if (n == "43" || n == "80" || n == "81") {  // the star breathes
        const float k = 0.5f + 0.5f * std::sin(tt * 1.3f);
        f.fillCircle(b.cx, b.cy, b.R * (0.22f + 0.05f * k), rgb(255, 210, 140), int(40.f + 50.f * k));
        f.fillCircle(b.cx, b.cy, b.R * 0.1f, rgb(255, 250, 230), 200);
    } else if (n == "54") {
        drawCabal(b, now, f, true);
    } else if (n == "hyper" && t.lanes) {  // light running along the lanes
        for (int a = 0; a < 6; ++a)
            for (int c2 = a + 1; c2 < 6; ++c2) {
                if (!((t.lanes >> (a * 6 + c2)) & 1u)) continue;
                const float a0 = -kPi / 2.f + float(a) * kPi / 3.f, a1 = -kPi / 2.f + float(c2) * kPi / 3.f;
                const float x0 = b.cx + std::cos(a0) * kS3 * b.R, y0 = b.cy + std::sin(a0) * kS3 * b.R;
                const float x1 = b.cx + std::cos(a1) * kS3 * b.R, y1 = b.cy + std::sin(a1) * kS3 * b.R;
                for (int k = 0; k < 2; ++k) {
                    float u = std::fmod(tt * 0.35f + float(k) * 0.5f + float(a * 6 + c2) * 0.13f, 1.f);
                    if ((a + c2) % 2) u = 1.f - u;
                    const float w0 = (1 - u) * (1 - u), w1 = 2 * (1 - u) * u, w2 = u * u;
                    const float qx = w0 * x0 + w1 * b.cx + w2 * x1, qy = w0 * y0 + w1 * b.cy + w2 * y1;
                    f.fillCircle(qx, qy, std::max(2.f, b.R * 0.07f), rgb(255, 120, 210), 70);
                    f.fillCircle(qx, qy, std::max(1.f, b.R * 0.028f), rgb(255, 240, 250), 230);
                }
            }
    }
}

std::unordered_map<uint64_t, Built>& cache() {
    static std::unordered_map<uint64_t, Built> c;
    return c;
}

void animate(Built& b, const retro::Tile& t, uint32_t now) {
    Canvas& f = *b.frame;
    const int64_t fk = flowKey(b, now);
    if (b.layer.size() != b.base.size() || fk != b.layerKey) {
        b.layer = b.base;
        drawFlow(b, now, b.layer.data());
        b.layerKey = fk;
    }
    std::memcpy(f.pixels(), b.layer.data(), b.layer.size() * 4);
    drawShimmer(b, now, f);
    drawStars(b, now, f);
    const float k = b.R / retro::kArtR;
    for (const retro::Piece& p : t.pieces) {
        if (p.kind == retro::Piece::Wormhole) drawWormhole(b, b.cx + p.x * k, b.cy + p.y * k, p.r * k, p.id, now, f, false);
        else if (p.kind == retro::Piece::IonStorm) drawWormhole(b, b.cx + p.x * k, b.cy + p.y * k, p.r * k * 0.9f, p.id ? 3 : 2, now, f, true);
        else if (p.kind == retro::Piece::Tear) drawTear(b, b.cx + p.x * k, b.cy + p.y * k, std::max(4.f, p.r * k), now, f);
    }
    if (b.sky && b.sky->name == "54") drawCabal(b, now, f, false);  // the tentacles behind Acheron
    for (PlanetDraw& pd : b.planets) {
        if (pd.station) drawStation(pd, now, f, b);
        else drawPlanet(pd, now, f, b.W);
    }
    for (const PlanetDraw& pd : b.planets) drawPlanetRing(pd, now, f);
    drawSpecial(b, t, now, f);
}

void blit(Canvas& c, const Built& b, int x0, int y0, int alpha) {
    const Rect cl = c.pxClip();
    const int xa = std::max(cl.x, x0), xb = std::min(cl.x + cl.w, x0 + b.W);
    const int ya = std::max(cl.y, y0), yb = std::min(cl.y + cl.h, y0 + b.H);
    if (xa >= xb || ya >= yb) return;
    uint32_t* px = c.pixels();
    const int W = c.pw();
    const uint32_t* fr = b.frame->pixels();
    for (int y = ya; y < yb; ++y) {
        const size_t row = size_t(y - y0) * size_t(b.W);
        uint32_t* line = px + size_t(y) * size_t(W);
        // the whole pixels in one copy, the edges blended
        int fl = x0 + b.fullL[size_t(y - y0)], fr2 = x0 + b.fullR[size_t(y - y0)];
        if (alpha < 255) fl = fr2 = xb;  // (fading: blend everything)
        fl = std::max(fl, xa);
        fr2 = std::min(fr2, xb);
        auto edge = [&](int from, int to) {
            for (int x = from; x < to; ++x) {
                const int m = b.mask[row + size_t(x - x0)];
                if (!m) continue;
                const int a = alpha >= 255 ? m : m * alpha / 255;
                line[x] = a >= 255 ? fr[row + size_t(x - x0)] : mix(line[x], fr[row + size_t(x - x0)], a);
            }
        };
        if (fl < fr2) {
            edge(xa, fl);
            std::memcpy(line + fl, fr + row + size_t(fl - x0), size_t(fr2 - fl) * 4);
            edge(fr2, xb);
        } else {
            edge(xa, xb);
        }
    }
}

// While the map zooms the size changes every frame: show a tile built at another size, scaled, until the
// size holds still for a moment.
void blitScaled(Canvas& c, const Built& b, float pcx, float pcy, float R, int alpha) {
    const float k = b.R / R;
    const int W = int(std::ceil(2.f * R)) + 2, H = int(std::ceil(2.f * kS3 * R)) + 2;
    const int x0 = int(std::floor(pcx - float(W) * 0.5f + 0.5f)), y0 = int(std::floor(pcy - float(H) * 0.5f + 0.5f));
    const Rect cl = c.pxClip();
    const int xa = std::max(cl.x, x0), xb = std::min(cl.x + cl.w, x0 + W);
    const int ya = std::max(cl.y, y0), yb = std::min(cl.y + cl.h, y0 + H);
    uint32_t* px = c.pixels();
    const int CW = c.pw();
    const uint32_t* fr = b.frame->pixels();
    for (int y = ya; y < yb; ++y) {
        const int sy = std::max(0, std::min(b.H - 1, int((float(y - y0) + 0.5f - float(H) * 0.5f) * k + b.cy)));
        uint32_t* line = px + size_t(y) * size_t(CW);
        for (int x = xa; x < xb; ++x) {
            const int sx = std::max(0, std::min(b.W - 1, int((float(x - x0) + 0.5f - float(W) * 0.5f) * k + b.cx)));
            const size_t i = size_t(sy) * size_t(b.W) + size_t(sx);
            const int m = b.mask[i];
            if (!m) continue;
            const int a = alpha >= 255 ? m : m * alpha / 255;
            line[x] = a >= 255 ? fr[i] : mix(line[x], fr[i], a);
        }
    }
}

}  // namespace

bool available() { return pack().ok; }

// 0.43.0 (owner: "tiles with nebulas are difficult to detect in Voidfall"): orange tendrils reaching in from the
// tile's six edges and curling towards the middle, swaying - drawn over the nebula's sky
static void nebulaTendrils(Canvas& c, float cx, float cy, float r, uint32_t now, uint32_t seed, int alpha) {
    const float t = float(now % 3600000u) / 1000.f;
    const bool few = r < 40.f;
    const int perEdge = few ? 1 : 2, steps = few ? 6 : 10;
    for (int e = 0; e < 6; ++e)
        for (int k = 0; k < perEdge; ++k) {
            const uint32_t h = hash32(seed * 2654435761u + uint32_t(e * 7 + k) * 40503u + 11u);
            auto fr = [&](int b) { return float((h >> (b * 5)) & 31u) / 31.f; };
            // a root on the edge (between its corners), pointing inward with a little twist
            const float a0 = 3.14159265f / 3.f * float(e), a1 = a0 + 3.14159265f / 3.f;
            const float along = 0.25f + 0.5f * fr(0);
            float px = cx + r * 0.98f * ((1.f - along) * std::cos(a0) + along * std::cos(a1));
            float py = cy + r * 0.98f * ((1.f - along) * std::sin(a0) + along * std::sin(a1));
            float ang = std::atan2(cy - py, cx - px) + (fr(1) - 0.5f) * 0.9f;
            const float curl = (fr(2) - 0.5f) * 2.4f, len = r * (0.42f + 0.25f * fr(3)), w0 = std::max(1.f, r * (0.035f + 0.025f * fr(4)));
            const float ph = fr(5) * 6.28f, spd = 0.5f + 0.4f * fr(1);
            float lx = px, ly = py;
            for (int s = 1; s <= steps; ++s) {
                const float u = float(s) / float(steps);
                const float a = ang + curl * u * u + 0.25f * u * std::sin(t * spd + ph + u * 3.f);
                px += std::cos(a) * len / float(steps);
                py += std::sin(a) * len / float(steps);
                const float wd = std::max(0.7f, w0 * (1.f - 0.8f * u));
                const uint32_t col = mix(rgb(150, 50, 10), rgb(255, 170, 60), int(u * 255.f));
                const int al = alpha * (230 - int(110.f * u)) / 255;
                c.line(lx, ly, px, py, rgb(255, 120, 30), wd * 2.6f, al / 5);  // the glow
                c.line(lx, ly, px, py, col, wd, al);
                lx = px;
                ly = py;
            }
            const float fl = 0.6f + 0.4f * std::sin(t * 5.f + ph);
            c.fillCircle(lx, ly, std::max(1.f, w0 * 0.9f * fl), rgb(255, 220, 140), int(float(alpha) * 0.8f * fl));
        }
}

static void drawCore(Canvas& c, const retro::Tile& t, float cx, float cy, float size, uint32_t now, int alpha);
void draw(Canvas& c, const retro::Tile& t, float cx, float cy, float size, uint32_t now, int alpha) {
    drawCore(c, t, cx, cy, size, now, alpha);
    if (t.def >= 0 && t.def < ti::kSystemCount && (ti::kSystems[t.def].anomalies & ti::kNebula) && size >= 10.f)
        nebulaTendrils(c, cx, cy, size, now, uint32_t(t.def) * 977u + t.seed, alpha);
}
static void drawCore(Canvas& c, const retro::Tile& t, float cx, float cy, float size, uint32_t now, int alpha) {
    Pack& pk = pack();
    if (!pk.ok || size < 2.f) return;
    const float R = c.fx(size);
    const float pcx = c.fX(cx), pcy = c.fY(cy);
    const int rq = int(R * 4.f + 0.5f);
    const uint64_t key = tileKey(t, rq);
    auto& C = cache();
    static std::unordered_map<uint64_t, uint64_t> lastOf;  // the tile's content -> the size drawn last
    static std::unordered_map<uint64_t, uint32_t> missAt;  // sizes asked for once
    const uint64_t content = tileKey(t, -1);
    auto it = C.find(key);
    if (it == C.end()) {
        // the map is zooming when the same tile was just drawn at another size: show that one, scaled,
        // until a size is asked for twice in a row
        auto seen = missAt.find(key);
        const bool settled = seen != missAt.end() && now - seen->second <= 400u;
        missAt[key] = now;
        if (missAt.size() > 4096) missAt.clear();
        if (!settled) {
            auto lo = lastOf.find(content);
            if (lo != lastOf.end()) {
                auto b2 = C.find(lo->second);
                const float ratio = b2 != C.end() ? b2->second.R / R : 0.f;
                if (b2 != C.end() && now - b2->second.used <= 300u && b2->second.frame && ratio > 0.7f && ratio < 1.45f) {
                    b2->second.used = now;
                    blitScaled(c, b2->second, pcx, pcy, R, alpha);
                    return;
                }
            }
        }
        // (0.32.1) after a zoom every tile wants rebuilding at once - a hitch: a few per frame, the others
        // shown scaled from the size they had until their turn comes
        static uint32_t budgetFrame = 0;
        static size_t budgetPx = 0;
        if (now != budgetFrame) {
            budgetFrame = now;
            budgetPx = 0;
        }
        const size_t px = size_t(2.f * R + 2.f) * size_t(2.f * kS3 * R + 2.f);
        const size_t budget = c.unscaled() ? 40000u : 180000u;  // about two tiles at the closest zoom
        if (budgetPx > 0 && budgetPx + px > budget) {
            auto lo = lastOf.find(content);
            if (lo != lastOf.end()) {
                auto b2 = C.find(lo->second);
                const float ratio = b2 != C.end() ? b2->second.R / R : 0.f;
                if (b2 != C.end() && b2->second.frame && ratio > 0.55f && ratio < 1.8f) {
                    b2->second.used = now;
                    blitScaled(c, b2->second, pcx, pcy, R, alpha);
                    return;
                }
            }
        }
        budgetPx += px;
        Built& nb = C[key];
        buildBase(nb, t, R, pk);
        it = C.find(key);
    }
    lastOf[content] = key;
    if (lastOf.size() > 4096) lastOf.clear();
    Built& b = it->second;
    b.used = now;
    // animation steps: finer for big tiles, coarser for small ones; slower on the Miyoo
    static const int forced = std::getenv("TI4_VFSTEP") ? std::atoi(std::getenv("TI4_VFSTEP")) : 0;  // tests: timings
    const uint32_t step = forced > 0 ? uint32_t(forced) : c.unscaled() ? (b.R < 40.f ? 240u : 150u) : (b.R < 60.f ? 160u : 80u);
    // tiles take their steps at different moments, so no one frame redraws them all
    const uint32_t offset = uint32_t(key % step);
    const uint32_t tick = (now + offset) / step;
    if (tick != b.tick) {
        b.tick = tick;
        animate(b, t, tick * step - offset);
    }
    const int x0 = int(std::floor(pcx - b.cx + 0.5f)), y0 = int(std::floor(pcy - b.cy + 0.5f));
    blit(c, b, x0, y0, alpha);
    // the wormholes' letters, on the screen (sharp text)
    const float k = size / retro::kArtR;
    for (const retro::Piece& p : t.pieces) {
        if (p.kind != retro::Piece::Wormhole || p.id < 0 || p.id > 4) continue;
        const float r = p.r * k;
        if (r < 6.f) continue;
        static const char* sym[5] = {"\xCE\xB1", "\xCE\xB2", "\xCE\xB3", "\xCE\xB4", "\xCE\xB5"};
        static const uint32_t col[5] = {rgb(255, 214, 150), rgb(190, 255, 210), rgb(255, 200, 255), rgb(210, 230, 255), rgb(255, 196, 190)};
        const FontData& fnt = r >= 14.f ? FONT_MDB : FONT_SMB;
        drawTextAligned(c, fnt, int(cx + p.x * k - r), int(cy + p.y * k) - fnt.lineHeight / 2 - 1, int(2 * r), Align::Center, sym[p.id],
                        col[p.id], alpha);
    }
    // forget tiles not drawn for a while
    static uint32_t sweep = 0;
    if (++sweep % 256 == 0 && C.size() > 48) {
        for (auto e = C.begin(); e != C.end();) {
            if (now - e->second.used > 2500u && &e->second != &b) e = C.erase(e);
            else ++e;
        }
    }
}

// ============================================================ the battle look and New Game backdrop (0.33.1)
const char* skyOf(const retro::Tile& t) {
    const char* n = skyName(t);
    return n ? n : "star0";
}

bool picture(const std::string& name, int w, int h, Image& out) {
    out = Image();
    const Tex* t = texRgb(name);
    if (!t || w <= 0 || h <= 0) return false;
    Image src;
    src.w = t->w;
    src.h = t->h;
    src.px = t->px;
    out = resizeImage(src, w, h);
    return !out.empty();
}

bool pictureCover(const std::string& name, int w, int h, Image& out) {
    out = Image();
    const Tex* t = texRgb(name);
    if (!t || w <= 0 || h <= 0 || t->w <= 0 || t->h <= 0) return false;
    const float k = std::max(float(w) / float(t->w), float(h) / float(t->h));
    const int sw = std::max(w, int(std::ceil(float(t->w) * k))), sh = std::max(h, int(std::ceil(float(t->h) * k)));
    Image src;
    src.w = t->w;
    src.h = t->h;
    src.px = t->px;
    const Image big = resizeImage(src, sw, sh);
    if (big.empty()) return false;
    out.w = w;
    out.h = h;
    out.px.resize(size_t(w) * size_t(h));
    const int x0 = (sw - w) / 2, y0 = (sh - h) / 2;
    for (int y = 0; y < h; ++y) {
        const uint32_t* row = big.px.data() + size_t(y + y0) * size_t(sw) + size_t(x0);
        std::copy(row, row + w, out.px.data() + size_t(y) * size_t(w));
    }
    return true;
}

bool skyPulse(const std::string& sky, int w, int h, SkyPulse& out) {
    out = SkyPulse();
    auto it = pack().skies.find(sky);
    if (it == pack().skies.end() || it->second.pulseMask.empty() || w <= 0 || h <= 0) return false;
    const Grey* pm = texGrey(it->second.pulseMask);
    if (!pm) return false;
    out.w = w;
    out.h = h;
    out.col = it->second.pulseCol;
    out.period = it->second.pulsePeriod;
    out.dir = it->second.pulseDir;
    const float ku = float(pm->w) / float(w), kv = float(pm->h) / float(h), half = float(pm->w) * 0.5f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float u = (float(x) + 0.5f) * ku, v = (float(y) + 0.5f) * kv;
            const int m = sampleGreyClamp(*pm, int((u - 0.5f) * 256.f), int((v - 0.5f) * 256.f));
            if (m < 10) continue;
            const float rr = std::min(1.f, std::hypot(u - half, v - float(pm->h) * 0.5f) / half);
            out.at.push_back(uint32_t(y) * uint32_t(w) + uint32_t(x));
            out.m.push_back(uint8_t(m));
            out.rr.push_back(uint16_t(rr * 65535.f));
        }
    return !out.at.empty();
}

void drawSkyPulse(Canvas& c, const SkyPulse& p, int x0, int y0, uint32_t now, int strength) {
    if (p.at.empty() || strength <= 0) return;
    const float t = float(now % 3600000u) / 1000.f;
    const float base = 2.f * kPi * t / p.period;
    const float waves = 2.f * kPi * 1.6f * float(p.dir) / 65535.f;
    const int cr = int((p.col >> 16) & 255) * strength >> 8, cg = int((p.col >> 8) & 255) * strength >> 8, cb = int(p.col & 255) * strength >> 8;
    int shape[256];
    for (int k = 0; k < 256; ++k) {
        const float sn = std::sin(float(k) / 256.f * 2.f * kPi);
        const float wv = sn > 0.f ? sn * sn * sn * sn * sn * sn : 0.f;
        shape[k] = int((0.16f + 0.84f * wv) * 256.f);
    }
    uint32_t* px = c.pixels();
    const int W = c.pw(), H = c.ph();
    const Rect cl = c.toPx(c.clip());
    for (size_t i = 0; i < p.at.size(); ++i) {
        const int x = x0 + int(p.at[i] % uint32_t(p.w)), y = y0 + int(p.at[i] / uint32_t(p.w));
        if (x < 0 || y < 0 || x >= W || y >= H || x < cl.x || y < cl.y || x >= cl.x + cl.w || y >= cl.y + cl.h) continue;
        const float ph = base - waves * float(p.rr[i]);
        const int a = (int(p.m[i]) * shape[int(ph / (2.f * kPi) * 256.f) & 255]) >> 8;
        uint32_t& d = px[size_t(y) * size_t(W) + size_t(x)];
        d = addRGB(d, cr * a >> 8, cg * a >> 8, cb * a >> 8);
    }
}

namespace {
uint32_t ramp3(float v, uint32_t a, uint32_t b, uint32_t c) {  // v 0..1 through three colours
    v = std::max(0.f, std::min(1.f, v));
    return v < 0.5f ? mix(a, b, int(v * 2.f * 255.f)) : mix(b, c, int((v - 0.5f) * 2.f * 255.f));
}
}  // namespace

void tendrils(Canvas& c, Rect area, uint32_t now, uint32_t seed, int count, float reach, int alpha) {
    if (alpha <= 0 || count <= 0) return;
    const float t = float(now % 3600000u) / 1000.f;
    const float H = float(area.h), W = float(area.w);
    for (int b = 0; b < count; ++b) {
        const uint32_t h = hash32(seed * 2654435761u + uint32_t(b) * 40503u + 7u);
        auto fr = [&](int k) { return float((h >> (k * 5)) & 31u) / 31.f; };
        // the root, beyond the top (right half) or the right edge
        float sx, sy;
        if (fr(0) < 0.62f) {
            sx = float(area.x) + W * (0.45f + 0.6f * fr(1));
            sy = float(area.y) - H * (0.05f + 0.08f * fr(2));
        } else {
            sx = float(area.x) + W * (1.03f + 0.06f * fr(1));
            sy = float(area.y) + H * (0.4f * fr(2) - 0.05f);
        }
        const float ang = (205.f + 45.f * fr(3)) * 3.14159265f / 180.f;  // down and to the left
        const float len = H * reach * (0.6f + 0.5f * fr(4));
        const float curl = (fr(5) - 0.5f) * 2.f;
        const float w0 = H * (0.02f + 0.026f * fr(6));
        const float tone = fr(0) * 0.6f + fr(3) * 0.4f;
        const float sway = 0.12f + 0.1f * fr(2), spd = 0.25f + 0.25f * fr(4), ph = fr(1) * 6.28f;
        const uint32_t root = mix(rgb(120, 14, 40), rgb(150, 30, 120), int(tone * 255.f));
        const uint32_t body = mix(rgb(230, 50, 70), rgb(255, 80, 170), int(tone * 255.f));
        const uint32_t tip = rgb(255, 175, 90);
        const bool small = c.scale() < 1.5f;  // the Miyoo: fewer, coarser strands (the same look at its size)
        const int strands = small ? 2 : 3;
        for (int s = 0; s < strands; ++s) {
            const float off = (float(s) - 1.f) * 0.5f;
            const float sp = 0.6f + 0.25f * float(s);
            const int steps = small ? 16 : 26;
            float px = sx, py = sy, lx = 0.f, ly = 0.f;
            for (int k = 0; k <= steps; ++k) {
                const float u = float(k) / float(steps);
                // the strand bends more towards its tip, and sways there
                const float a = ang + curl * u * u * 1.4f + sway * u * u * std::sin(t * spd + ph + u * 2.5f + float(s) * 0.7f);
                const float spread = w0 * off * 1.5f * std::sin(3.14159265f * std::min(1.f, u * 1.3f)) + 0.3f * w0 * std::sin(u * 4.f + ph + float(s));
                const float qx = px - std::sin(a) * spread, qy = py - std::cos(a) * spread;
                if (k > 0) {
                    const float wd = std::max(0.6f, w0 * sp * float(std::pow(double(1.f - u), 1.2)));  // (double pow: powf is a newer symbol on the Miyoo)
                    const uint32_t col = ramp3(u, root, body, tip);
                    const int al = alpha * (180 + int(60.f * (1.f - u))) / 255;
                    c.line(lx, ly, qx, qy, rgb(255, 60, 150), wd * 3.f, al / 6);  // the glow
                    c.line(lx, ly, qx, qy, col, wd, al);
                    if (wd > 2.f) c.line(lx, ly, qx, qy, mix(col, rgb(255, 255, 255), 115), wd * 0.3f, al / 2);  // a lit core
                }
                lx = qx;
                ly = qy;
                const float seg = len * sp * 1.1f / float(steps);
                px += std::cos(a) * seg;
                py -= std::sin(a) * seg;
            }
            // an ember at the tip, flickering
            const float fl = 0.6f + 0.4f * std::sin(t * 7.f + ph + float(s) * 2.f);
            c.fillCircle(lx, ly, 1.6f + 1.2f * fl, rgb(255, 200, 120), int(float(alpha) * 0.7f * fl));
        }
    }
}

void grade(Canvas& c, Rect area) {
    static uint8_t lr[256], lg[256], lb[256];
    static bool made = false;
    if (!made) {  // violet in the shadows, a little warmth in the lights
        made = true;
        for (int v = 0; v < 256; ++v) {
            const float f = float(v) / 255.f, sh = (1.f - f) * (1.f - f), hi = f * f;
            lr[v] = uint8_t(std::min(255.f, float(v) + sh * 16.f + hi * 8.f));
            lg[v] = uint8_t(std::min(255.f, float(v) * 0.95f + sh * 4.f));
            lb[v] = uint8_t(std::max(0.f, std::min(255.f, float(v) + sh * 26.f - hi * 10.f)));
        }
    }
    const Rect p = c.toPx(area), clip = c.pxClip();
    const int x0 = std::max(p.x, clip.x), x1 = std::min(p.x + p.w, clip.x + clip.w);
    const int y0 = std::max(p.y, clip.y), y1 = std::min(p.y + p.h, clip.y + clip.h);
    // the print lines: a dark diagonal every few pixels (the box art's period on a 640 x 480 screen)
    const int period = std::max(4, int(c.scale() * 3.2f + 0.5f));
    uint32_t* px = c.pixels();
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = px + size_t(y) * size_t(c.pw());
        int ph = (x0 + y) % period;
        for (int x = x0; x < x1; ++x) {
            const uint32_t q = row[x];
            uint32_t r = lr[(q >> 16) & 255], g = lg[(q >> 8) & 255], b = lb[q & 255];
            if (ph == 0) {
                r = r * 236 >> 8;
                g = g * 236 >> 8;
                b = b * 236 >> 8;
            }
            row[x] = (q & 0xFF000000u) | (r << 16) | (g << 8) | b;
            if (++ph == period) ph = 0;
        }
    }
}

void flame(Canvas& c, float x, float y, float dir, float len, float width, uint32_t now, uint32_t seed, int alpha) {
    if (alpha <= 0 || len <= 0.f) return;
    const float t = float(now % 3600000u) / 1000.f;
    const float ph = float(seed % 628u) / 100.f;
    const float fl = 0.8f + 0.2f * std::sin(t * 23.f + ph) + 0.1f * std::sin(t * 41.f + ph * 2.f);
    const int n = 9;
    for (int k = n - 1; k >= 0; --k) {
        const float u = float(k) / float(n - 1);
        const float px = x + dir * len * fl * u;
        const float py = y + std::sin(t * 9.f + ph + u * 4.f) * width * 0.15f * u;
        const float r = width * (1.f - 0.75f * u) * (0.85f + 0.15f * fl);
        const uint32_t col = u < 0.15f ? rgb(255, 250, 225) : u < 0.4f ? rgb(255, 210, 110) : u < 0.7f ? rgb(255, 130, 60) : rgb(220, 50, 70);
        c.fillCircle(px, py, r * 1.6f, col, int(float(alpha) * 0.18f * (1.f - u)));
        c.fillCircle(px, py, r, col, int(float(alpha) * (0.9f - 0.6f * u)));
    }
}

}  // namespace voidfall
