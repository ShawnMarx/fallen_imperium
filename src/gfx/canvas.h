// Software renderer. Everything is drawn into a plain 32-bit pixel buffer
// (0x00RRGGBB) which the platform layer then copies to the screen. This keeps
// drawing identical on the Miyoo, on a PC and in automated tests.
//
// Sharp picture (0.27.0, owner: "increase the resolution so it looks crisp on
// computer monitors"): a canvas has a size in the game's own 640x480 units
// (every screen is laid out in those) and a scale. With scale 1 nothing
// changes - one unit is one pixel (the Miyoo, the tests, "Original" on a PC).
// With scale 2.25 the same calls draw into a 1440x1080 buffer: shapes, lines
// and circles are worked out at the real pixels, text is rasterised at that
// size, pictures come at that size (see icons / cards), so everything stays
// sharp instead of being blown up.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

struct Image;

inline constexpr uint32_t rgb(int r, int g, int b) {
    return (uint32_t(r & 255) << 16) | (uint32_t(g & 255) << 8) | uint32_t(b & 255);
}
inline uint32_t mix(uint32_t d, uint32_t s, int a) {  // a: 0..255 weight of s
    if (a >= 255) return s;
    if (a <= 0) return d;
    // Red and blue are blended together in one multiply; the >> 8 (not / 255)
    // keeps red's fractional bits out of the blue channel.
    const uint32_t w = uint32_t(a) + (uint32_t(a) >> 7);  // 0..256
    uint32_t rb = (((s & 0xFF00FF) * w + (d & 0xFF00FF) * (256 - w)) >> 8) & 0xFF00FF;
    uint32_t g = (((s & 0x00FF00) * w + (d & 0x00FF00) * (256 - w)) >> 8) & 0x00FF00;
    return rb | g;
}
inline uint32_t shade(uint32_t c, float k) {  // k<1 darker, k>1 lighter
    auto ch = [&](int v) { int r = int(v * k); return r < 0 ? 0 : r > 255 ? 255 : r; };
    return rgb(ch((c >> 16) & 255), ch((c >> 8) & 255), ch(c & 255));
}

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

class Canvas {
public:
    Canvas(int w, int h, float scale = 1.f);
    // size in the game's units (640 x 480 for the screen; wider on a widescreen PC, 0.46.0)
    int width() const { return vw_; }
    int height() const { return h_; }
    // Widescreen (0.46.0): a screen laid out for 640 units drawn centred on a wider canvas.
    // setView(ox, vw): unit 0 lands at ox, width() says vw and the clip starts as the view;
    // fills and lines that touch the view's left / right edge run on to the canvas's edges,
    // so the backgrounds, bars and dimmers of those screens still cover the whole width.
    void setView(int ox, int vw);
    void resetView() { setView(0, w_); }
    int viewX() const { return ox_; }
    int fullWidth() const { return w_; }
    bool inView() const { return ox_ != 0 || vw_ != w_; }
    // nothing smaller than the view (or the whole canvas) is clipped out
    bool clipIsView() const { return lclip_.x <= 0 && lclip_.y <= 0 && lclip_.x + lclip_.w >= vw_ && lclip_.y + lclip_.h >= h_; }
    // the real pixels
    float scale() const { return s_; }
    bool unscaled() const { return one_; }
    int pw() const { return pw_; }
    int ph() const { return ph_; }
    const uint32_t* pixels() const { return px_.data(); }
    uint32_t* pixels() { return px_.data(); }
    // game units -> pixels (whole units land on whole pixels; scale 1 = unchanged)
    int X(int x) const { return one_ ? x + ox_ : int(std::floor(float(x + ox_) * s_ + 0.5f)); }
    int Y(int y) const { return one_ ? y : int(std::floor(float(y) * s_ + 0.5f)); }
    int S(int len) const { return one_ ? len : int(std::floor(float(len) * s_ + 0.5f)); }  // a length
    float fx(float len) const { return len * s_; }                  // a length in real pixels
    float fX(float x) const { return (x + float(ox_)) * s_; }       // a position across, in real pixels
    float fY(float y) const { return y * s_; }                      // a position down, in real pixels
    Rect toPx(Rect r) const { return {X(r.x), Y(r.y), X(r.x + r.w) - X(r.x), Y(r.y + r.h) - Y(r.y)}; }

    void setClip(Rect r);
    void resetClip();
    Rect clip() const { return lclip_; }  // in game units
    Rect pxClip() const { return {cx0_, cy0_, cx1_ - cx0_, cy1_ - cy0_}; }

    void clear(uint32_t c);
    void pixel(int x, int y, uint32_t c, int a = 255);
    void fillRect(int x, int y, int w, int h, uint32_t c, int a = 255);
    void frameRect(int x, int y, int w, int h, uint32_t c, int t = 1, int a = 255);
    void fillRoundRect(int x, int y, int w, int h, int r, uint32_t c, int a = 255);
    void frameRoundRect(int x, int y, int w, int h, int r, uint32_t c, int t = 1, int a = 255);
    void hline(int x0, int x1, int y, uint32_t c, int a = 255);
    void vline(int x, int y0, int y1, uint32_t c, int a = 255);
    void line(float x0, float y0, float x1, float y1, uint32_t c, float thick = 1.f, int a = 255);
    // Convex or concave polygon, pixel-centre sampling, optional soft edge.
    void fillPolygon(const float* xs, const float* ys, int n, uint32_t c, int a = 255);
    void fillCircle(float cx, float cy, float r, uint32_t c, int a = 255);   // anti-aliased
    void ring(float cx, float cy, float r, float thick, uint32_t c, int a = 255);  // anti-aliased
    void verticalGradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    // Draws a premultiplied image with its top-left at (x, y). `a` fades it.
    // The image is in real pixels (made at this canvas's scale).
    void blit(const Image& img, int x, int y, int a = 255);
    // Uses only the image's shape (alpha), painted in colour c.
    void blitTint(const Image& img, int x, int y, uint32_t c, int a = 255);
    // A grey picture painted in colour c: its dark parts go dark, mid greys
    // take the colour, the brightest parts turn pale (the unit pictures).
    // mirror: flipped left to right.
    void blitShade(const Image& img, int x, int y, uint32_t c, int a = 255, bool mirror = false);

    // The same in real pixels (for code that works on the pixels themselves).
    void pxPixel(int x, int y, uint32_t c, int a = 255);
    void pxSpan(int x0, int x1, int y, uint32_t c, int a = 255);  // [x0, x1)
    void pxBlit(const Image& img, int x, int y, int a = 255);
    // another canvas's pixels, as they are, with their top left at real pixel (x, y) (0.47.0: pop-up windows)
    void pxCopy(const Canvas& src, int x, int y);
    void pxBlitTint(const Image& img, int x, int y, uint32_t c, int a = 255);
    void pxBlitShade(const Image& img, int x, int y, uint32_t c, int a = 255, bool mirror = false);

    // The screen's scale: pictures made for drawing on the screen (cards,
    // icons, battle backdrops) are made at this scale. Set by the app.
    static float& screenScale();
    // Bumped whenever the screen's scale changes (caches start over).
    static int& scaleGeneration();

private:
    int w_, h_, pw_, ph_;
    int ox_ = 0, vw_ = 0;  // the view (widescreen, 0.46.0)
    float s_;
    bool one_;
    bool edgeStretch(int& x, int& w);  // a fill touching the view's edge: on to the canvas edge
    void restoreClipX();
    std::vector<uint32_t> px_;
    Rect lclip_;
    int cx0_, cy0_, cx1_, cy1_;
    int savedCx0_ = -1, savedCx1_ = -1;
    void span(int x0, int x1, int y, uint32_t c, int a) { pxSpan(x0, x1, y, c, a); }
    void pxFillRect(int x0, int y0, int x1, int y1, uint32_t c, int a);
    void pxRoundRect(int x, int y, int w, int h, int r, uint32_t c, int a);
    void pxFrameRoundRect(int x, int y, int w, int h, int r, int t, uint32_t c, int a);
    void pxLine(float x0, float y0, float x1, float y1, uint32_t c, float thick, int a);
    void pxPolygon(const float* xs, const float* ys, int n, uint32_t c, int a);
    void pxCircle(float cx, float cy, float r, uint32_t c, int a);
    void pxRing(float cx, float cy, float r, float thick, uint32_t c, int a);
};
