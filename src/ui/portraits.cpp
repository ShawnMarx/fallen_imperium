#include "ui/portraits.h"

#include <cstring>
#include <map>
#include <vector>

#include "core/log.h"
#include "engine/data.h"
#include "gfx/image.h"
#include "gfx/picture.h"

using namespace ti;

namespace portraits {
namespace {

#include "ui/gen/portrait_keys.inc"

constexpr int kRows = int(sizeof(kPortraitKeys) / sizeof(kPortraitKeys[0]));

struct Atlas {
    bool tried = false, ok = false;
    Image img;
    std::vector<int> rowOf;
};

Atlas& atlas() {
    static Atlas a;
    if (!a.tried) {
        a.tried = true;
        a.rowOf.assign(size_t(kFactionCount), -1);
        for (int f = 0; f < kFactionCount; ++f) {
            for (int r = 0; r < kRows; ++r)
                if (!std::strcmp(kPortraitKeys[r], kFactions[f].key)) a.rowOf[size_t(f)] = r;
        }
        a.ok = loadPicture("portraits", a.img) && a.img.w >= kPortraitFrames * kPortraitSize && a.img.h >= kRows * kPortraitSize;
        LOG("portraits: %s (%dx%d)", a.ok ? "loaded" : "missing", a.img.w, a.img.h);
    }
    return a;
}

// one frame enlarged to n x n real pixels (nearest: whole-pixel steps), with CRT scanlines
struct Scaled {
    std::vector<Image> frames;  // kPortraitFrames, made when first drawn
};

const Image& frameAt(int row, int frame, int n) {
    static std::map<uint64_t, Scaled> cache;
    static int gen = -1;
    if (gen != Canvas::scaleGeneration() || cache.size() > 24) {
        cache.clear();
        gen = Canvas::scaleGeneration();
    }
    Scaled& s = cache[(uint64_t(row) << 32) | uint64_t(n)];
    if (s.frames.empty()) s.frames.resize(size_t(kPortraitFrames));
    Image& im = s.frames[size_t(frame)];
    if (im.empty()) {
        const Atlas& a = atlas();
        im.w = im.h = n;
        im.px.resize(size_t(n) * size_t(n));
        const int sx0 = frame * kPortraitSize, sy0 = row * kPortraitSize;
        const int line = n >= 160 ? 3 : 2;  // the scanlines' pitch in real pixels
        for (int y = 0; y < n; ++y) {
            const uint32_t* src = &a.img.px[size_t(sy0 + y * kPortraitSize / n) * size_t(a.img.w) + size_t(sx0)];
            uint32_t* dst = &im.px[size_t(y) * size_t(n)];
            const bool dark = y % line == 0;
            for (int x = 0; x < n; ++x) {
                uint32_t p = src[x * kPortraitSize / n];
                if (dark) {  // 70 / 255 darker, like the mock-ups
                    const uint32_t r = ((p >> 16) & 255) * 185 / 255, g = ((p >> 8) & 255) * 185 / 255, b = (p & 255) * 185 / 255;
                    p = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
                dst[x] = p;
            }
        }
    }
    return im;
}

}  // namespace

bool has(int faction) {
    const Atlas& a = atlas();
    return a.ok && faction >= 0 && faction < kFactionCount && a.rowOf[size_t(faction)] >= 0;
}

bool draw(Canvas& c, int faction, int x, int y, int size, uint32_t now, int alpha, int phase) {
    if (!has(faction) || size <= 0) return false;
    const int row = atlas().rowOf[size_t(faction)];
    const int frame = int(((now + uint32_t(phase)) / uint32_t(kPortraitFrameMs)) % uint32_t(kPortraitFrames));
    // whole pixels: the largest multiple of 64 real pixels that fits, centred (the window's fill shows round it)
    // (unless that loses more than a fifth: then nearest at the full size)
    const int full = c.S(size), snap = full / kPortraitSize * kPortraitSize, n = snap * 5 >= full * 4 ? snap : full;
    const int off = (full - n) / 2;
    c.pxBlit(frameAt(row, frame, n), c.X(x) + off, c.Y(y) + off, alpha);
    return true;
}

uint32_t backdrop(int faction) {
    if (!has(faction)) return rgb(10, 12, 24);
    const Atlas& a = atlas();
    const uint32_t p = a.img.px[size_t(a.rowOf[size_t(faction)] * kPortraitSize + kPortraitSize - 1) * size_t(a.img.w)];  // bottom-left corner
    return p & 0xFFFFFF;
}

}  // namespace portraits
