#include "ui/icons.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "core/log.h"
#include "gfx/image.h"
#include "ui/painted.h"
#include "ui/theme.h"
#include "ui/widgets.h"

using namespace ti;

namespace {
struct Pack {
    bool tried = false;
    std::map<std::string, Image> masters;
    std::map<std::pair<std::string, int>, Image> scaled;  // (name, box size)
};

Pack& pack() {
    static Pack p;
    if (!p.tried) {
        p.tried = true;
        std::vector<NamedImage> list;
        for (const char* path : {"icons.dat", "assets/icons.dat", "../assets/icons.dat"}) {
            if (loadIconPack(path, list)) {
                for (NamedImage& e : list) p.masters[e.name] = std::move(e.img);
                LOG("icons: %d loaded from %s", int(p.masters.size()), path);
                break;
            }
        }
        if (p.masters.empty()) LOG("icons: icons.dat not found - using plain shapes");
    }
    return p;
}

const Image* sized(const std::string& name, int size) {
    if (size <= 0) return nullptr;
    Pack& p = pack();
    auto key = std::make_pair(name, size);
    auto it = p.scaled.find(key);
    if (it != p.scaled.end()) return &it->second;
    auto m = p.masters.find(name);
    if (m == p.masters.end()) return nullptr;
    const Image& src = m->second;
    int w = size, h = size;
    if (src.w >= src.h) h = std::max(1, (src.h * size + src.w / 2) / src.w);
    else w = std::max(1, (src.w * size + src.h / 2) / src.h);
    if (p.scaled.size() > 1500) p.scaled.clear();  // sizes are few; this is just a guard
    return &(p.scaled[key] = resizeImage(src, w, h));
}
}  // namespace

namespace icons {
bool available() { return !pack().masters.empty(); }
const Image* image(const std::string& name) {
    Pack& p = pack();
    auto m = p.masters.find(name);
    return m == p.masters.end() ? nullptr : &m->second;
}
int count() { return int(pack().masters.size()); }

// (0.27.0: pictures are made at the canvas's real size - sharp on a big screen)
bool draw(Canvas& c, const std::string& name, int x, int y, int size, int alpha, uint32_t tint) {
    const int px = c.X(x), py = c.Y(y), ps = c.X(x + size) - px;
    const Image* im = sized(name, ps);
    if (!im) return false;
    int ox = px + (ps - im->w) / 2, oy = py + (ps - im->h) / 2;
    if (im->mono) c.pxBlitTint(*im, ox, oy, tint, alpha);
    else c.pxBlit(*im, ox, oy, alpha);
    return true;
}

bool drawUnitArt(Canvas& c, const std::string& name, float cx, float cy, int maxW, int maxH, uint32_t col, bool mirror, int alpha) {
    Pack& p = pack();
    auto m = p.masters.find(name);
    if (m == p.masters.end() || maxW <= 0 || maxH <= 0) return false;
    const Image& src = m->second;
    // the box size along the longer side that keeps the picture inside maxW x maxH
    int size = src.w >= src.h ? std::min(maxW, maxH * src.w / std::max(1, src.h)) : std::min(maxH, maxW * src.h / std::max(1, src.w));
    const Image* im = sized(name, std::max(2, c.S(size)));
    if (!im) return false;
    c.pxBlitShade(*im, int(c.fX(cx)) - im->w / 2, int(c.fY(cy)) - im->h / 2, col, alpha, mirror);
    return true;
}

bool drawUnitArtOn(Canvas& c, const std::string& name, float cx, float footY, int maxW, int maxH, uint32_t col, bool mirror, int alpha) {
    Pack& p = pack();
    auto m = p.masters.find(name);
    if (m == p.masters.end() || maxW <= 0 || maxH <= 0) return false;
    const Image& src = m->second;
    int size = src.w >= src.h ? std::min(maxW, maxH * src.w / std::max(1, src.h)) : std::min(maxH, maxW * src.h / std::max(1, src.w));
    const Image* im = sized(name, std::max(2, c.S(size)));
    if (!im) return false;
    c.pxBlitShade(*im, int(c.fX(cx)) - im->w / 2, int(c.fY(footY)) - im->h, col, alpha, mirror);
    return true;
}

}  // namespace icons

// Voidfall (0.51.1): the painted unit picture - the same picture re-lit - darks pulled toward the
// void's violet, the light warm, the lower right in shadow, a warm edge toward the key light (up and left) and a
// violet rim where the void's light catches it (up and right). Baked once per picture, size, mirror and colour.
namespace {
Image bakePainted(const Image& im, bool mirror, uint32_t col) {
    const int w = im.w, h = im.h;
    Image o;
    o.w = w;
    o.h = h;
    o.px.assign(size_t(w) * size_t(h), 0u);
    auto A = [&](int x, int y) -> int {
        if (x < 0 || y < 0 || x >= w || y >= h) return 0;
        return int(im.px[size_t(y) * size_t(w) + size_t(mirror ? w - 1 - x : x)] >> 24);
    };
    const int d = std::max(1, int(std::lround(float(std::max(w, h)) / 70.f)));
    const uint32_t vio = rgb(40, 16, 74), shadeC = painted::kDeep, rimC = painted::kRim;
    const uint32_t warmHi = painted::kWarmHi;
    const uint32_t keyC = mix(mix(col, rgb(255, 255, 255), 100), rgb(255, 226, 190), 120);
    const int cr = int((col >> 16) & 255), cg = int((col >> 8) & 255), cb = int(col & 255);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int sa = A(x, y);
            if (!sa) continue;
            const uint32_t p = im.px[size_t(y) * size_t(w) + size_t(mirror ? w - 1 - x : x)];
            const int lum = std::min(255, int((p >> 8) & 255) * 255 / sa);
            uint32_t c;
            if (lum < 217) {
                const int f = 50 + 205 * lum / 217;
                c = rgb(cr * f / 255, cg * f / 255, cb * f / 255);
                c = mix(c, vio, (255 - f) * 110 / 255);  // the darks go violet
            } else {
                c = mix(col, warmHi, (lum - 217) * 160 / 38);  // the light goes warm
            }
            const float t = float(y) / float(std::max(1, h - 1)), u = float(x) / float(std::max(1, w - 1));
            const float g = std::max(0.f, std::min(1.f, (0.62f * t + 0.38f * u - 0.34f) / 0.6f));
            c = mix(c, shadeC, int(g * 130.f));
            const float lift = std::max(0.f, std::min(1.f, (0.5f - (0.6f * t + 0.4f * u)) / 0.5f));
            c = mix(c, warmHi, int(lift * 50.f));  // the key light on the upper left
            // (only the outer edges: a narrow gap inside the picture has the picture again 3 steps on)
            const int key = sa * (255 - std::max(A(x - d, y - d), A(x - 3 * d, y - 3 * d))) / 255;
            const int rim = sa * (255 - std::max(A(x + d, y - d), A(x + 3 * d, y - 3 * d))) / 255;
            c = mix(c, keyC, key * 90 / 255);
            c = mix(c, rimC, rim * 200 / 255);
            const uint32_t r = (c >> 16) & 255, gg = (c >> 8) & 255, b = c & 255;
            o.px[size_t(y) * size_t(w) + size_t(x)] =
                (uint32_t(sa) << 24) | ((r * uint32_t(sa) / 255) << 16) | ((gg * uint32_t(sa) / 255) << 8) | (b * uint32_t(sa) / 255);
        }
    return o;
}
std::map<std::tuple<std::string, int, bool, uint32_t>, Image>& painted() {
    static std::map<std::tuple<std::string, int, bool, uint32_t>, Image> m;
    return m;
}
}  // namespace

namespace {
const Image* paintedImage(Canvas& c, const std::string& name, int maxW, int maxH, uint32_t col, bool mirror) {
    Pack& p = pack();
    auto m = p.masters.find(name);
    if (m == p.masters.end() || maxW <= 0 || maxH <= 0) return nullptr;
    const Image& src = m->second;
    int size = src.w >= src.h ? std::min(maxW, maxH * src.w / std::max(1, src.h)) : std::min(maxH, maxW * src.h / std::max(1, src.w));
    const int ps = std::max(2, c.S(size));
    const Image* im = sized(name, ps);
    if (!im) return nullptr;
    auto& cache = painted();
    const auto key = std::make_tuple(name, ps, mirror, col);
    auto it = cache.find(key);
    if (it == cache.end()) {
        if (cache.size() > 300) cache.clear();  // a guard: sizes and colours are few
        it = cache.emplace(key, bakePainted(*im, mirror, col)).first;
    }
    return &it->second;
}
}  // namespace

namespace icons {
bool drawUnitArtOnPainted(Canvas& c, const std::string& name, float cx, float footY, int maxW, int maxH, uint32_t col, bool mirror,
                          int alpha, float* outW, float* outH) {
    const Image* im = paintedImage(c, name, maxW, maxH, col, mirror);
    if (!im) return false;
    c.pxBlit(*im, int(c.fX(cx)) - im->w / 2, int(c.fY(footY)) - im->h, alpha);
    if (outW) *outW = float(im->w) / c.scale();
    if (outH) *outH = float(im->h) / c.scale();
    return true;
}
bool drawUnitArtPainted(Canvas& c, const std::string& name, float cx, float cy, int maxW, int maxH, uint32_t col, bool mirror, int alpha) {
    const Image* im = paintedImage(c, name, maxW, maxH, col, mirror);
    if (!im) return false;
    c.pxBlit(*im, int(c.fX(cx)) - im->w / 2, int(c.fY(cy)) - im->h / 2, alpha);
    return true;
}
}  // namespace icons

namespace icons {
bool drawShape(Canvas& c, const std::string& name, int x, int y, int size, uint32_t col, int alpha) {
    const int px = c.X(x), py = c.Y(y), ps = c.X(x + size) - px;
    const Image* im = sized(name, ps);
    if (!im) return false;
    c.pxBlitTint(*im, px + (ps - im->w) / 2, py + (ps - im->h) / 2, col, alpha);
    return true;
}
}  // namespace icons

void drawFactionIcon(Canvas& c, int faction, int x, int y, int size, int alpha) {
    if (faction < 0 || faction >= kFactionCount) return;
    // (Thunder's Edge emblems are packed as lastbastion; the Firmament shares the Obsidian's)
    std::string key = kFactions[faction].key;
    if (key == "bastion") key = "lastbastion";
    if (key == "firmament") key = "obsidian";
    // (Discordant Stars emblems are packed under the picker's names)
    if (key == "augers") key = "ilyxum";
    if (key == "khrask") key = "ltokk";
    if (key == "mykomentori") key = "myko";
    if (key == "zealots") key = "rhodun";
    if (icons::draw(c, "faction/" + key, x, y, size, alpha)) return;
    float r = size * 0.45f, cx = x + size / 2.f, cy = y + size / 2.f;
    c.fillCircle(cx, cy, r, theme::panelHi, alpha);
    c.ring(cx, cy, r, 1.5f, theme::gold, alpha);
    const FontData& f = size >= 30 ? FONT_MDB : size >= 18 ? FONT_SMB : FONT_XS;
    drawTextAligned(c, f, x, int(cy) - f.lineHeight / 2, size, Align::Center, std::string(1, kFactions[faction].name[0]),
                    theme::gold, alpha);
}

void drawTraitIcon(Canvas& c, Trait t, int x, int y, int size, int alpha) {
    const char* n = t == Trait::Cultural ? "cultural" : t == Trait::Hazardous ? "hazardous" : t == Trait::Industrial ? "industrial" : nullptr;
    if (!n) return;
    if (icons::draw(c, n, x, y, size, alpha)) return;
    uint32_t col = t == Trait::Cultural ? theme::cultural : t == Trait::Hazardous ? theme::hazardous : theme::industrial;
    c.fillCircle(x + size / 2.f, y + size / 2.f, size * 0.35f, col, alpha);
}

void drawSkipIcon(Canvas& c, TechColor t, int x, int y, int size, int alpha) {
    static const char* n[4] = {"propulsion", "biotic", "warfare", "cybernetic"};
    static const uint32_t col[4] = {rgb(80, 146, 240), rgb(84, 196, 100), rgb(228, 80, 70), rgb(238, 206, 70)};
    int i = int(t);
    if (i < 0 || i > 3) return;
    if (icons::draw(c, n[i], x, y, size, alpha)) return;
    c.fillCircle(x + size / 2.f, y + size / 2.f, size * 0.35f, col[i], alpha);
}

void drawResourceIcon(Canvas& c, int cx, int cy, int size, int value, const FontData& f, bool influence) {
    const char* n = influence ? "influence_bg" : "resources_bg";
    if (icons::draw(c, n, cx - size / 2, cy - size / 2, size)) {
        drawTextAligned(c, f, cx - size / 2, cy - f.lineHeight / 2, size, Align::Center, std::to_string(value), theme::text);
        return;
    }
    drawValueBadge(c, cx, cy, size / 2, value, influence ? theme::influence : theme::resource, f);
}
