#include "ui/codec.h"

#include <cmath>
#include <cstdio>

#include "gfx/font.h"
#include "gfx/image.h"
#include "gfx/picture.h"
#include "ui/icons.h"
#include "ui/portraits.h"
#include "ui/theme.h"

namespace codec {
namespace {

struct Back {
    bool tried = false;
    Image src;
    Image scaled;
    int pw = 0, ph = 0, gen = -1;
};

const Image* backImage(Canvas& c, bool alert, int ph) {
    static Back b[2];
    Back& k = b[alert ? 1 : 0];
    if (!k.tried) {
        k.tried = true;
        loadPicture(alert ? "codec_alert" : "codec_space", k.src);
    }
    if (k.src.empty()) return nullptr;
    const int pw = c.pw();
    if (k.pw != pw || k.ph != ph || k.gen != Canvas::scaleGeneration()) {
        // cover: scale to fill the width and the band's height, keep the proportions, crop the middle
        const float s = std::max(float(pw) / float(k.src.w), float(ph) / float(k.src.h));
        const int w = std::max(1, int(float(k.src.w) * s + 0.5f)), h = std::max(1, int(float(k.src.h) * s + 0.5f));
        k.scaled = resizeImage(k.src, w, h);
        k.pw = pw;
        k.ph = ph;
        k.gen = Canvas::scaleGeneration();
    }
    return &k.scaled;
}

uint32_t hash32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

// the canvas without the centred 640 view (the backdrop covers the whole width)
struct WholeCanvas {
    Canvas& c;
    int ox, vw;
    Rect clip;
    explicit WholeCanvas(Canvas& cc) : c(cc), ox(cc.viewX()), vw(cc.width()), clip(cc.clip()) {
        c.resetView();
        c.resetClip();
    }
    ~WholeCanvas() {
        c.setView(ox, vw);
        c.setClip(clip);
    }
};

}  // namespace

void backdrop(Canvas& c, uint32_t now, bool alert, int y0, int y1) {
    WholeCanvas whole(c);
    const int py0 = c.Y(y0), py1 = c.Y(y1);
    const Image* img = backImage(c, alert, py1 - py0);
    if (!img) {
        c.fillRect(0, y0, c.width(), y1 - y0, alert ? rgb(26, 4, 10) : rgb(10, 8, 24));
        return;
    }
    // crop the middle of the scaled picture into the band
    const int ox = (img->w - c.pw()) / 2, oy = (img->h - (py1 - py0)) / 2;
    Image band;
    band.w = c.pw();
    band.h = py1 - py0;
    band.px.resize(size_t(band.w) * size_t(band.h));
    for (int y = 0; y < band.h; ++y)
        std::copy(img->px.begin() + ptrdiff_t(size_t(y + oy) * size_t(img->w) + size_t(ox)),
                  img->px.begin() + ptrdiff_t(size_t(y + oy) * size_t(img->w) + size_t(ox) + size_t(band.w)),
                  band.px.begin() + ptrdiff_t(size_t(y) * size_t(band.w)));
    c.pxBlit(band, 0, py0);
    // a few stars twinkle
    for (uint32_t i = 0; i < 26; ++i) {
        const uint32_t h = hash32(i * 2654435761U + (alert ? 77U : 5U));
        const int x = int(h % uint32_t(c.width())), y = y0 + int((h >> 12) % uint32_t(std::max(1, y1 - y0)));
        const float k = 0.5f + 0.5f * std::sin(float(now) / (700.f + float(h % 900U)) + float(h % 628U) / 100.f);
        if (k < 0.55f) continue;
        const int a = int((k - 0.55f) / 0.45f * 230.f);
        const uint32_t col = (h >> 7) % 3U == 0 ? rgb(255, 200, 240) : rgb(200, 225, 255);
        c.fillRect(x, y, 1, 1, rgb(255, 255, 255), a);
        if (k > 0.85f) {
            c.fillRect(x - 2, y, 5, 1, col, a / 2);
            c.fillRect(x, y - 2, 1, 5, col, a / 2);
        }
    }
}

void glowFrame(Canvas& c, Rect r, uint32_t col, int radius, int alpha) {
    for (int k = 3; k >= 1; --k)
        c.frameRoundRect(r.x - k, r.y - k, r.w + 2 * k, r.h + 2 * k, radius + k, col, 1, alpha * (4 - k) * 18 / 255);
    c.frameRoundRect(r.x, r.y, r.w, r.h, radius, col, 2, alpha);
}

void window(Canvas& c, int faction, Rect r, uint32_t col, uint32_t now, uint32_t age, int alpha, int phase, bool alert) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 8, alert ? rgb(24, 4, 10) : rgb(6, 8, 20), alpha * 240 / 255);
    const int pad = 6, size = std::min(r.w, r.h) - 2 * pad;
    Rect in{r.x + pad, r.y + pad, size, size};
    // switching on: a bright line opens into the picture
    const float open = age >= 320 ? 1.f : float(age) / 320.f;
    const int bandH = std::max(1, int(float(in.h) * open));
    const Rect band{in.x, in.y + (in.h - bandH) / 2, in.w, bandH};
    const Rect old = c.clip();
    Rect cl = band;
    {  // (intersect with what is clipped already)
        const int x0 = std::max(cl.x, old.x), y0 = std::max(cl.y, old.y);
        const int x1 = std::min(cl.x + cl.w, old.x + old.w), y1 = std::min(cl.y + cl.h, old.y + old.h);
        cl = Rect{x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    }
    c.setClip(cl);
    c.fillRect(in.x, in.y, in.w, in.h, portraits::backdrop(faction), alpha);
    if (!portraits::draw(c, faction, in.x, in.y, in.w, now, alpha, phase)) drawFactionIcon(c, faction, in.x + in.w / 6, in.y + in.h / 6, in.w * 2 / 3, alpha);
    c.setClip(old);
    if (open < 1.f) {
        const int a = int(255.f * (1.f - open));
        c.fillRect(in.x, band.y, in.w, 1, rgb(230, 250, 255), std::min(alpha, a + 40));
        c.fillRect(in.x, band.y + band.h - 1, in.w, 1, rgb(230, 250, 255), std::min(alpha, a + 40));
    } else if (((now / 2900U) + uint32_t(phase)) % 4U == 0 && (now % 2900U) < 140U) {
        // now and then a line of interference rolls down the screen
        const int y = in.y + int((now % 2900U) * uint32_t(in.h) / 140U);
        c.fillRect(in.x, y, in.w, 2, rgb(200, 240, 255), alpha / 4);
    }
    glowFrame(c, r, col, 8, alpha);
}

void plate(Canvas& c, int faction, Rect r, const std::string& name, const std::string& sub, uint32_t col, int alpha) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 6, rgb(10, 12, 30), alpha * 235 / 255);
    const int is = r.h - 8;
    drawFactionIcon(c, faction, r.x + 4, r.y + 4, is, alpha);
    const int tx = r.x + is + 9, tw = r.w - is - 12;
    drawText(c, FONT_SMB, tx, r.y + 2, fitText(FONT_SMB, name, tw), col, alpha);
    drawText(c, FONT_XS, tx, r.y + r.h - 14, fitText(FONT_XS, sub, tw), rgb(170, 180, 210), alpha);
}

void signal(Canvas& c, int cx, int y, const std::string& freq, uint32_t col, uint32_t now, bool talking, bool lost, int alpha) {
    drawTextAligned(c, FONT_XS, cx - 80, y, 160, Align::Center, lost ? "CHANNEL LOST" : "SUBSPACE", rgb(160, 170, 220), alpha);
    drawTextAligned(c, FONT_LG, cx - 80, y + 11, 160, Align::Center, lost ? "--.--" : freq, col, alpha);
    const int n = 14, bw = 5, gap = 2, x0 = cx - (n * (bw + gap) - gap) / 2, base = y + 56, maxH = 20;
    const uint32_t tick = now / 90U;
    for (int i = 0; i < n; ++i) {
        const uint32_t h = hash32(tick * 131U + uint32_t(i) * 977U);
        int bh;
        if (lost) bh = (h % 7U == 0U) ? 6 + int(h % 14U) : 2;
        else if (talking) bh = 3 + int(h % uint32_t(maxH - 2));
        else bh = 2 + int((hash32(tick / 4U * 7U + uint32_t(i)) % 4U));
        for (int k = 0; k < bh; k += 3) {
            const float t = float(k) / float(maxH);
            const uint32_t cc = lost ? rgb(255, int(60 + 90 * (1 - t)), 60) : rgb(int(70 + 185 * t), int(220 - 150 * t), int(255 - 60 * t));
            c.fillRect(x0 + i * (bw + gap), base - k - 2, bw, 2, cc, alpha);
        }
    }
}

Rect speechArea(Rect r) { return Rect{r.x + 10, r.y + 24, r.w - 20, r.h - 30}; }

void speechBox(Canvas& c, Rect r, const std::string& who, uint32_t col, const speech::Speech& s, int shown, uint32_t now,
               bool alert, int alpha) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 8, alert ? rgb(26, 4, 10) : rgb(8, 6, 22), alpha * 235 / 255);
    glowFrame(c, r, col, 8, alpha);
    drawText(c, FONT_SMB, r.x + 10, r.y + 5, fitText(FONT_SMB, who, r.w - 20), col, alpha);
    const Rect a = speechArea(r);
    speech::draw(c, s, a.x, a.y, shown, now, alpha);
    if (shown >= 0 && shown < s.chars && (now / 260U) % 2U == 0U) {  // the cursor at the end of what's typed
        int cx = a.x, cy = a.y, ch = s.font ? s.font->lineHeight - 4 : 12;
        for (const speech::Word& w : s.words) {
            if (w.start > shown) break;
            const int vis = std::min(int(w.text.size()), shown - w.start);
            cx = a.x + w.x + textWidth(*w.font, w.text.substr(0, size_t(std::max(0, vis)))) + 2;
            cy = a.y + w.y + 2;
        }
        c.fillRect(cx, cy, 6, ch, col, alpha);
    }
}

std::string frequency(uint32_t seed) {
    char b[16];
    std::snprintf(b, sizeof(b), "%u.%02u", 6U + (seed % 3U), (seed / 3U) % 100U);
    return b;
}

}  // namespace codec
