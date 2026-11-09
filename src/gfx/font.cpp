#include "gfx/font.h"

#include <cmath>

#include "gfx/font_data.inc"
#include "gfx/hdfont.h"

namespace {

// Decode one UTF-8 code point; advances i. Invalid bytes become '?'.
uint32_t nextCp(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : -1;
    if (extra < 0) return '?';
    uint32_t cp = c & (0x3F >> extra);
    for (int k = 0; k < extra; ++k) {
        if (i >= s.size()) return '?';
        cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    }
    return cp;
}

const GlyphInfo* findGlyph(const FontData& f, uint32_t cp) {
    if (cp >= 32 && cp < 127) return &f.glyphs[cp - 32];  // ASCII is stored first, in order
    for (int i = 95; i < f.count; ++i)
        if (f.glyphs[i].cp == cp) return &f.glyphs[i];
    return &f.glyphs['?' - 32];
}

}  // namespace

int textWidth(const FontData& f, const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) w += findGlyph(f, nextCp(s, i))->adv;
    return w;
}

static int s_halo = 0;
TextHalo::TextHalo(int alpha) : prev_(s_halo) { s_halo = alpha; }
TextHalo::~TextHalo() { s_halo = prev_; }

int drawText(Canvas& c, const FontData& f, int x, int y, const std::string& s, uint32_t color, int alpha) {
    if (s_halo > 0 && alpha > 0) {
        const int h = s_halo, ha = h * alpha / 255;
        s_halo = 0;
        drawText(c, f, x + 1, y, s, 0, ha);
        drawText(c, f, x - 1, y, s, 0, ha);
        drawText(c, f, x, y + 1, s, 0, ha);
        drawText(c, f, x, y - 1, s, 0, ha);
        s_halo = h;
    }
    if (!c.unscaled() && hdfont::available()) {
        // the sharp picture (0.27.0): glyphs rasterised at the real size, spaced so the line
        // is exactly as wide as on the 640x480 screen (the layout never changes)
        const float sc = c.scale();
        std::vector<const hdfont::Glyph*> gs;
        float natural = 0;
        for (size_t i = 0; i < s.size();) {
            const hdfont::Glyph* g = hdfont::glyph(f, nextCp(s, i), sc);
            gs.push_back(g);
            if (g) natural += g->adv;
        }
        const int lw = textWidth(f, s);
        const float k = natural > 0.f ? float(lw) * sc / natural : 1.f;
        const int baseline = int(std::floor(float(y + f.ascent) * sc + 0.5f));
        float pen = float(c.X(x));
        for (const hdfont::Glyph* g : gs) {
            if (!g) continue;
            const int gx = int(std::floor(pen + 0.5f)) + g->xo, gy = baseline + g->yo;
            for (int j = 0; j < g->h; ++j)
                for (int q = 0; q < g->w; ++q) {
                    const int a = g->bits[size_t(j) * size_t(g->w) + size_t(q)];
                    if (a) c.pxPixel(gx + q, gy + j, color, alpha >= 255 ? a : (a * alpha) / 255);
                }
            pen += g->adv * k;
        }
        return lw;
    }
    const int baseline = y + f.ascent;
    int pen = x;
    for (size_t i = 0; i < s.size();) {
        const GlyphInfo* g = findGlyph(f, nextCp(s, i));
        const uint8_t* src = f.bits + g->off;
        const int gx = pen + g->xo, gy = baseline + g->yo;
        for (int j = 0; j < g->h; ++j) {
            for (int k = 0; k < g->w; ++k) {
                int a = src[j * g->w + k];
                if (a) c.pixel(gx + k, gy + j, color, alpha >= 255 ? a : (a * alpha) / 255);
            }
        }
        pen += g->adv;
    }
    return pen - x;
}

int drawTextAligned(Canvas& c, const FontData& f, int x, int y, int w, Align al, const std::string& s,
                    uint32_t color, int alpha) {
    int tw = textWidth(f, s);
    int px = al == Align::Left ? x : al == Align::Center ? x + (w - tw) / 2 : x + w - tw;
    return drawText(c, f, px, y, s, color, alpha);
}

int drawTextShadow(Canvas& c, const FontData& f, int x, int y, const std::string& s, uint32_t color,
                   uint32_t shadow) {
    drawText(c, f, x + 1, y + 1, s, shadow, 200);
    return drawText(c, f, x, y, s, color);
}

std::vector<std::string> wrapText(const FontData& f, const std::string& s, int maxW) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(f, cand) > maxW) {
            lines.push_back(line);
            line = word;
        } else {
            line = cand;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') {
            flushWord();
        } else if (ch == '\n') {
            flushWord();
            lines.push_back(line);
            line.clear();
        } else {
            word += ch;
        }
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

std::string fitText(const FontData& f, const std::string& s, int maxW) {
    if (textWidth(f, s) <= maxW) return s;
    std::string t = s;
    while (!t.empty()) {
        // drop one UTF-8 code point from the end
        size_t n = t.size() - 1;
        while (n > 0 && ((unsigned char)t[n] & 0xC0) == 0x80) --n;
        t.erase(n);
        if (textWidth(f, t + "\xE2\x80\xA6") <= maxW) return t + "\xE2\x80\xA6";
    }
    return t;
}
