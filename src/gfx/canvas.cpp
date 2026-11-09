#include "gfx/canvas.h"

#include <algorithm>
#include <cmath>

#include "gfx/image.h"

Canvas::Canvas(int w, int h, float scale)
    : w_(w), h_(h), vw_(w), s_(scale > 0.f ? scale : 1.f), one_(!(scale > 0.f) || std::fabs(scale - 1.f) < 1e-4f) {
    if (one_) s_ = 1.f;
    pw_ = one_ ? w : int(std::floor(float(w) * s_ + 0.5f));
    ph_ = one_ ? h : int(std::floor(float(h) * s_ + 0.5f));
    px_.assign(size_t(pw_) * size_t(ph_), 0);
    resetClip();
}

float& Canvas::screenScale() {
    static float s = 1.f;
    return s;
}

int& Canvas::scaleGeneration() {
    static int g = 0;
    return g;
}

void Canvas::setClip(Rect r) {
    const int x0 = std::max(-ox_, r.x), y0 = std::max(0, r.y);
    int x1 = std::min(w_ - ox_, r.x + r.w), y1 = std::min(h_, r.y + r.h);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    lclip_ = {x0, y0, x1 - x0, y1 - y0};
    cx0_ = std::max(0, X(x0));
    cy0_ = Y(y0);
    cx1_ = std::min(pw_, X(x1));
    cy1_ = std::min(ph_, Y(y1));
}

void Canvas::resetClip() {
    if (inView()) {
        setClip(Rect{0, 0, vw_, h_});
        return;
    }
    lclip_ = {0, 0, w_, h_};
    cx0_ = 0;
    cy0_ = 0;
    cx1_ = pw_;
    cy1_ = ph_;
}

void Canvas::pxCopy(const Canvas& src, int x, int y) {
    const int x0 = std::max(cx0_, x), x1 = std::min(cx1_, x + src.pw());
    const int y0 = std::max(cy0_, y), y1 = std::min(cy1_, y + src.ph());
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; ++yy)
        std::copy(src.pixels() + size_t(yy - y) * size_t(src.pw()) + size_t(x0 - x), src.pixels() + size_t(yy - y) * size_t(src.pw()) + size_t(x1 - x),
                  px_.data() + size_t(yy) * size_t(pw_) + size_t(x0));
}

void Canvas::setView(int ox, int vw) {
    ox_ = std::max(0, std::min(ox, w_));
    vw_ = std::max(1, std::min(vw, w_ - ox_));
    resetClip();
}

// The older screens' backgrounds, bars and dimmers start at 0 or end at the
// view's right edge: on a wider screen they carry on to the canvas's edges
// (unless the screen clipped itself to something smaller than its view).
bool Canvas::edgeStretch(int& x, int& w) {
    if (!inView() || w <= 0 || x > 0 || x + w < vw_) return false;  // only what spans the whole view
    if (lclip_.x > 0 || lclip_.x + lclip_.w < vw_) return false;    // clipped inside the view
    x = -ox_;
    w = w_;
    savedCx0_ = cx0_;
    savedCx1_ = cx1_;
    cx0_ = 0;
    cx1_ = pw_;
    return true;
}

void Canvas::restoreClipX() {
    if (savedCx0_ >= 0) cx0_ = savedCx0_;
    if (savedCx1_ >= 0) cx1_ = savedCx1_;
    savedCx0_ = savedCx1_ = -1;
}

void Canvas::clear(uint32_t c) { std::fill(px_.begin(), px_.end(), c); }

// ------------------------------------------------------------ real pixels
void Canvas::pxPixel(int x, int y, uint32_t c, int a) {
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_ || a <= 0) return;
    uint32_t& d = px_[size_t(y) * pw_ + x];
    d = mix(d, c, a);
}

void Canvas::pxSpan(int x0, int x1, int y, uint32_t c, int a) {  // [x0, x1)
    if (y < cy0_ || y >= cy1_ || a <= 0) return;
    x0 = std::max(x0, cx0_);
    x1 = std::min(x1, cx1_);
    if (x1 <= x0) return;
    uint32_t* p = &px_[size_t(y) * pw_ + x0];
    if (a >= 255) {
        std::fill(p, p + (x1 - x0), c);
    } else {
        for (int x = x0; x < x1; ++x, ++p) *p = mix(*p, c, a);
    }
}

void Canvas::pxFillRect(int x0, int y0, int x1, int y1, uint32_t c, int a) {  // [x0, x1) x [y0, y1)
    for (int yy = std::max(y0, cy0_); yy < std::min(y1, cy1_); ++yy) span(x0, x1, yy, c, a);
}

// --------------------------------------------------------------- game units
void Canvas::pixel(int x, int y, uint32_t c, int a) {
    if (one_) {
        pxPixel(x + ox_, y, c, a);
        return;
    }
    pxFillRect(X(x), Y(y), X(x + 1), Y(y + 1), c, a);
}

void Canvas::fillRect(int x, int y, int w, int h, uint32_t c, int a) {
    if (w <= 0 || h <= 0) return;
    const bool st = edgeStretch(x, w);
    pxFillRect(X(x), Y(y), X(x + w), Y(y + h), c, a);
    if (st) restoreClipX();
}

void Canvas::frameRect(int x, int y, int w, int h, uint32_t c, int t, int a) {
    fillRect(x, y, w, t, c, a);
    fillRect(x, y + h - t, w, t, c, a);
    fillRect(x, y + t, t, h - 2 * t, c, a);
    fillRect(x + w - t, y + t, t, h - 2 * t, c, a);
}

void Canvas::hline(int x0, int x1, int y, uint32_t c, int a) {
    if (x1 < x0) std::swap(x0, x1);
    int w = x1 + 1 - x0;
    const bool st = edgeStretch(x0, w);
    pxFillRect(X(x0), Y(y), X(x0 + w), Y(y + 1), c, a);
    if (st) restoreClipX();
}

void Canvas::vline(int x, int y0, int y1, uint32_t c, int a) {
    if (y1 < y0) std::swap(y0, y1);
    if (one_) {
        for (int y = y0; y <= y1; ++y) pxPixel(x + ox_, y, c, a);
        return;
    }
    pxFillRect(X(x), Y(y0), X(x + 1), Y(y1 + 1), c, a);
}

static inline float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

// Rounded rectangles: straight parts as spans, corners sampled per pixel.
void Canvas::pxRoundRect(int x, int y, int w, int h, int r, uint32_t c, int a) {
    r = std::max(0, std::min(r, std::min(w, h) / 2));
    if (r == 0) {
        pxFillRect(x, y, x + w, y + h, c, a);
        return;
    }
    pxFillRect(x, y + r, x + w, y + h - r, c, a);
    pxFillRect(x + r, y, x + w - r, y + r, c, a);
    pxFillRect(x + r, y + h - r, x + w - r, y + h, c, a);
    const float rf = float(r);
    for (int j = 0; j < r; ++j) {
        for (int i = 0; i < r; ++i) {
            float dx = rf - (i + 0.5f), dy = rf - (j + 0.5f);
            float cov = clamp01(rf + 0.5f - std::sqrt(dx * dx + dy * dy));
            if (cov <= 0.f) continue;
            int aa = int(cov * a);
            pxPixel(x + i, y + j, c, aa);
            pxPixel(x + w - 1 - i, y + j, c, aa);
            pxPixel(x + i, y + h - 1 - j, c, aa);
            pxPixel(x + w - 1 - i, y + h - 1 - j, c, aa);
        }
    }
}

void Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint32_t c, int a) {
    if (one_) {
        pxRoundRect(x + ox_, y, w, h, r, c, a);
        return;
    }
    const int px = X(x), py = Y(y);
    pxRoundRect(px, py, X(x + w) - px, Y(y + h) - py, S(r), c, a);
}

void Canvas::pxFrameRoundRect(int x, int y, int w, int h, int r, int t, uint32_t c, int a) {
    r = std::max(0, std::min(r, std::min(w, h) / 2));
    if (r == 0) {
        pxFillRect(x, y, x + w, y + t, c, a);
        pxFillRect(x, y + h - t, x + w, y + h, c, a);
        pxFillRect(x, y + t, x + t, y + h - t, c, a);
        pxFillRect(x + w - t, y + t, x + w, y + h - t, c, a);
        return;
    }
    pxFillRect(x + r, y, x + w - r, y + t, c, a);
    pxFillRect(x + r, y + h - t, x + w - r, y + h, c, a);
    pxFillRect(x, y + r, x + t, y + h - r, c, a);
    pxFillRect(x + w - t, y + r, x + w, y + h - r, c, a);
    const float rf = float(r), inner = float(r - t);
    for (int j = 0; j < r; ++j) {
        for (int i = 0; i < r; ++i) {
            float dx = rf - (i + 0.5f), dy = rf - (j + 0.5f);
            float d = std::sqrt(dx * dx + dy * dy);
            float cov = std::min(clamp01(rf + 0.5f - d), clamp01(d - inner + 0.5f));
            if (cov <= 0.f) continue;
            int aa = int(cov * a);
            pxPixel(x + i, y + j, c, aa);
            pxPixel(x + w - 1 - i, y + j, c, aa);
            pxPixel(x + i, y + h - 1 - j, c, aa);
            pxPixel(x + w - 1 - i, y + h - 1 - j, c, aa);
        }
    }
}

void Canvas::frameRoundRect(int x, int y, int w, int h, int r, uint32_t c, int t, int a) {
    if (one_) {
        if (std::max(0, std::min(r, std::min(w, h) / 2)) == 0) {
            frameRect(x, y, w, h, c, t, a);
            return;
        }
        pxFrameRoundRect(x + ox_, y, w, h, r, t, c, a);
        return;
    }
    const int px = X(x), py = Y(y);
    pxFrameRoundRect(px, py, X(x + w) - px, Y(y + h) - py, S(r), std::max(1, S(t)), c, a);
}

// Anti-aliased thick line using distance to the segment.
void Canvas::pxLine(float x0, float y0, float x1, float y1, uint32_t c, float thick, int a) {
    const float hw = thick * 0.5f;
    int bx0 = int(std::floor(std::min(x0, x1) - hw - 1)), bx1 = int(std::ceil(std::max(x0, x1) + hw + 1));
    int by0 = int(std::floor(std::min(y0, y1) - hw - 1)), by1 = int(std::ceil(std::max(y0, y1) + hw + 1));
    bx0 = std::max(bx0, cx0_);
    by0 = std::max(by0, cy0_);
    bx1 = std::min(bx1, cx1_ - 1);
    by1 = std::min(by1, cy1_ - 1);
    const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = by0; y <= by1; ++y) {
        for (int x = bx0; x <= bx1; ++x) {
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float t = len2 > 0 ? clamp01((px * dx + py * dy) / len2) : 0.f;
            float ex = px - t * dx, ey = py - t * dy;
            float cov = clamp01(hw + 0.5f - std::sqrt(ex * ex + ey * ey));
            if (cov > 0.f) pxPixel(x, y, c, int(cov * a));
        }
    }
}

void Canvas::line(float x0, float y0, float x1, float y1, uint32_t c, float thick, int a) {
    if (one_) pxLine(x0 + float(ox_), y0, x1 + float(ox_), y1, c, thick, a);
    else pxLine(fX(x0), y0 * s_, fX(x1), y1 * s_, c, thick * s_, a);
}

void Canvas::pxPolygon(const float* xs, const float* ys, int n, uint32_t c, int a) {
    if (n < 3) return;
    float miny = ys[0], maxy = ys[0];
    for (int i = 1; i < n; ++i) {
        miny = std::min(miny, ys[i]);
        maxy = std::max(maxy, ys[i]);
    }
    int y0 = std::max(cy0_, int(std::ceil(miny - 0.5f)));
    int y1 = std::min(cy1_ - 1, int(std::floor(maxy - 0.5f)));
    float cross[32];
    for (int y = y0; y <= y1; ++y) {
        const float sy = y + 0.5f;
        int k = 0;
        for (int i = 0, j = n - 1; i < n && k < 32; j = i++) {
            float ya = ys[j], yb = ys[i];
            if ((ya <= sy && yb > sy) || (yb <= sy && ya > sy)) {
                cross[k++] = xs[j] + (sy - ya) * (xs[i] - xs[j]) / (yb - ya);
            }
        }
        std::sort(cross, cross + k);
        for (int i = 0; i + 1 < k; i += 2) {
            int xa = int(std::ceil(cross[i] - 0.5f));
            int xb = int(std::ceil(cross[i + 1] - 0.5f));
            span(xa, xb, y, c, a);
        }
    }
}

void Canvas::fillPolygon(const float* xs, const float* ys, int n, uint32_t c, int a) {
    if (one_ && ox_ == 0) {
        pxPolygon(xs, ys, n, c, a);
        return;
    }
    float bx[64], by[64];
    std::vector<float> vx, vy;
    float* px = bx;
    float* py = by;
    if (n > 64) {
        vx.resize(size_t(n));
        vy.resize(size_t(n));
        px = vx.data();
        py = vy.data();
    }
    for (int i = 0; i < n; ++i) {
        px[i] = fX(xs[i]);
        py[i] = ys[i] * s_;
    }
    pxPolygon(px, py, n, c, a);
}

void Canvas::pxCircle(float cx, float cy, float r, uint32_t c, int a) {
    if (r <= 0) return;
    int y0 = std::max(cy0_, int(std::floor(cy - r - 1))), y1 = std::min(cy1_ - 1, int(std::ceil(cy + r + 1)));
    const float rin = std::max(0.f, r - 0.5f), rin2 = rin * rin, rout2 = (r + 0.5f) * (r + 0.5f);
    for (int y = y0; y <= y1; ++y) {
        float dy = y + 0.5f - cy;
        float dy2 = dy * dy;
        if (dy2 >= rout2) continue;
        // fully covered middle span
        int mx0 = 1, mx1 = 0;
        if (dy2 < rin2) {
            float half = std::sqrt(rin2 - dy2);
            mx0 = int(std::ceil(cx - half - 0.5f));
            mx1 = int(std::floor(cx + half - 0.5f));
        }
        float outer = std::sqrt(rout2 - dy2);
        int ex0 = int(std::floor(cx - outer - 0.5f)), ex1 = int(std::ceil(cx + outer - 0.5f));
        for (int x = ex0; x <= ex1; ++x) {
            if (mx0 <= mx1 && x == mx0) {
                span(mx0, mx1 + 1, y, c, a);
                x = mx1;
                continue;
            }
            float dx = x + 0.5f - cx;
            float cov = clamp01(r + 0.5f - std::sqrt(dx * dx + dy2));
            if (cov > 0.f) pxPixel(x, y, c, int(cov * a));
        }
    }
}

void Canvas::fillCircle(float cx, float cy, float r, uint32_t c, int a) {
    if (one_) pxCircle(cx + float(ox_), cy, r, c, a);
    else pxCircle(fX(cx), cy * s_, r * s_, c, a);
}

void Canvas::pxRing(float cx, float cy, float r, float thick, uint32_t c, int a) {
    int y0 = std::max(cy0_, int(std::floor(cy - r - 1))), y1 = std::min(cy1_ - 1, int(std::ceil(cy + r + 1)));
    int x0 = std::max(cx0_, int(std::floor(cx - r - 1))), x1 = std::min(cx1_ - 1, int(std::ceil(cx + r + 1)));
    const float inner = r - thick;
    for (int y = y0; y <= y1; ++y) {
        float dy = y + 0.5f - cy;
        for (int x = x0; x <= x1; ++x) {
            float dx = x + 0.5f - cx;
            float d = std::sqrt(dx * dx + dy * dy);
            float cov = std::min(clamp01(r + 0.5f - d), clamp01(d - inner + 0.5f));
            if (cov > 0.f) pxPixel(x, y, c, int(cov * a));
        }
    }
}

void Canvas::ring(float cx, float cy, float r, float thick, uint32_t c, int a) {
    if (one_) pxRing(cx + float(ox_), cy, r, thick, c, a);
    else pxRing(fX(cx), cy * s_, r * s_, thick * s_, c, a);
}

void Canvas::verticalGradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    const bool st = edgeStretch(x, w);
    const int x0 = X(x), x1 = X(x + w), y0 = Y(y), y1 = Y(y + h), ph = y1 - y0;
    for (int j = 0; j < ph; ++j) {
        int a = ph > 1 ? (j * 255) / (ph - 1) : 0;
        span(x0, x1, y0 + j, mix(top, bottom, a), 255);
    }
    if (st) restoreClipX();
}

// ------------------------------------------------------------------ images
void Canvas::blit(const Image& img, int x, int y, int a) { pxBlit(img, X(x), Y(y), a); }
void Canvas::blitTint(const Image& img, int x, int y, uint32_t c, int a) { pxBlitTint(img, X(x), Y(y), c, a); }
void Canvas::blitShade(const Image& img, int x, int y, uint32_t c, int a, bool mirror) { pxBlitShade(img, X(x), Y(y), c, a, mirror); }

void Canvas::pxBlit(const Image& img, int x, int y, int a) {
    if (img.empty() || a <= 0) return;
    if (img.mono) {
        pxBlitTint(img, x, y, rgb(255, 255, 255), a);
        return;
    }
    const int x0 = std::max(x, cx0_), x1 = std::min(x + img.w, cx1_);
    const int y0 = std::max(y, cy0_), y1 = std::min(y + img.h, cy1_);
    if (x0 >= x1 || y0 >= y1) return;  // 0.43.0: wholly off-screen (no pointer past the picture)
    const uint32_t fade = uint32_t(std::min(a, 255)) + (a >= 255 ? 1 : 0);  // 1..256
    for (int yy = y0; yy < y1; ++yy) {
        const uint32_t* s = &img.px[size_t(yy - y) * img.w + (x0 - x)];
        uint32_t* d = &px_[size_t(yy) * pw_ + x0];
        for (int xx = x0; xx < x1; ++xx, ++s, ++d) {
            uint32_t p = *s;
            uint32_t sa = p >> 24;
            if (!sa) continue;
            uint32_t sr = (p >> 16) & 255, sg = (p >> 8) & 255, sb = p & 255;
            if (fade < 256) {
                sa = (sa * fade) >> 8;
                sr = (sr * fade) >> 8;
                sg = (sg * fade) >> 8;
                sb = (sb * fade) >> 8;
            }
            const uint32_t inv = 255 - sa;
            const uint32_t q = *d;
            uint32_t r = sr + (((q >> 16) & 255) * inv + 127) / 255;
            uint32_t g = sg + (((q >> 8) & 255) * inv + 127) / 255;
            uint32_t b = sb + ((q & 255) * inv + 127) / 255;
            *d = (std::min(r, 255u) << 16) | (std::min(g, 255u) << 8) | std::min(b, 255u);
        }
    }
}

void Canvas::pxBlitTint(const Image& img, int x, int y, uint32_t c, int a) {
    if (img.empty() || a <= 0) return;
    const int x0 = std::max(x, cx0_), x1 = std::min(x + img.w, cx1_);
    const int y0 = std::max(y, cy0_), y1 = std::min(y + img.h, cy1_);
    if (x0 >= x1 || y0 >= y1) return;  // 0.43.0: wholly off-screen (no pointer past the picture)
    for (int yy = y0; yy < y1; ++yy) {
        const uint32_t* s = &img.px[size_t(yy - y) * img.w + (x0 - x)];
        uint32_t* d = &px_[size_t(yy) * pw_ + x0];
        for (int xx = x0; xx < x1; ++xx, ++s, ++d) {
            int sa = int(*s >> 24);
            if (sa) *d = mix(*d, c, a >= 255 ? sa : sa * a / 255);
        }
    }
}

void Canvas::pxBlitShade(const Image& img, int x, int y, uint32_t c, int a, bool mirror) {
    if (img.empty() || a <= 0) return;
    // colour ramp for the 256 grey levels, worked out once per call
    uint32_t ramp[256];
    const int cr = int((c >> 16) & 255), cg = int((c >> 8) & 255), cb = int(c & 255);
    for (int t = 0; t < 256; ++t) {
        int r, g, b;
        if (t < 217) {
            const int f = 50 + 205 * t / 217;
            r = cr * f / 255, g = cg * f / 255, b = cb * f / 255;
        } else {
            const int u = (t - 217) * 90 / 38;
            r = cr + (255 - cr) * u / 255, g = cg + (255 - cg) * u / 255, b = cb + (255 - cb) * u / 255;
        }
        ramp[t] = uint32_t((r << 16) | (g << 8) | b);
    }
    const int x0 = std::max(x, cx0_), x1 = std::min(x + img.w, cx1_);
    const int y0 = std::max(y, cy0_), y1 = std::min(y + img.h, cy1_);
    if (x0 >= x1 || y0 >= y1) return;  // 0.43.0: wholly off-screen (no pointer past the picture)
    for (int yy = y0; yy < y1; ++yy) {
        const uint32_t* row = &img.px[size_t(yy - y) * img.w];
        uint32_t* d = &px_[size_t(yy) * pw_ + x0];
        for (int xx = x0; xx < x1; ++xx, ++d) {
            const int sx = mirror ? img.w - 1 - (xx - x) : xx - x;
            const uint32_t p = row[sx];
            const int sa = int(p >> 24);
            if (!sa) continue;
            const int lum = std::min(255, int((p >> 8) & 255) * 255 / sa);
            *d = mix(*d, ramp[lum], a >= 255 ? sa : sa * a / 255);
        }
    }
}
