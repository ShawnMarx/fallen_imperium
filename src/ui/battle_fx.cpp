#include "ui/battle_fx.h"

#include <algorithm>
#include <cmath>

#include "gfx/canvas.h"

namespace battlefx {

namespace {

constexpr float kPi = 3.14159265f;

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 0x9E3779B9u) {
        if (!s) s = 1;
    }
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f() { return float(next() % 100000u) / 100000.f; }
    float r(float lo, float hi) { return lo + (hi - lo) * f(); }
};

struct Line {  // a crack as a polyline, with each point's distance from the break
    std::vector<float> x, y, d;
};

bool inBox(float x, float y, float xMin, float xMax, float yMin, float yMax, float m) {
    return x >= xMin - m && x <= xMax + m && y >= yMin - m && y <= yMax + m;
}

// where a crack is `dist` from the break (false if it never gets that far)
bool pointAt(const Line& l, float dist, float& px, float& py) {
    for (size_t i = 1; i < l.d.size(); ++i)
        if (l.d[i] >= dist) {
            const float span = std::max(1e-4f, l.d[i] - l.d[i - 1]);
            const float k = std::max(0.f, std::min(1.f, (dist - l.d[i - 1]) / span));
            px = l.x[i - 1] + (l.x[i] - l.x[i - 1]) * k;
            py = l.y[i - 1] + (l.y[i] - l.y[i - 1]) * k;
            return true;
        }
    return false;
}

void addLine(std::vector<Crack>& out, const Line& l, float w) {
    for (size_t i = 1; i < l.x.size(); ++i) out.push_back({l.x[i - 1], l.y[i - 1], l.x[i], l.y[i], 0.5f * (l.d[i - 1] + l.d[i]), w});
}

}  // namespace

void crackNet(std::vector<Crack>& cracks, std::vector<Shard>* shards, uint32_t seed, int kind, float ix, float iy, float len, int spokes,
              float xMin, float xMax, float yMin, float yMax) {
    cracks.clear();
    if (shards) shards->clear();
    Rng r(seed);
    spokes = std::max(3, spokes);
    std::vector<Line> lines;
    const float base = r.r(0.f, 2.f * kPi);
    for (int i = 0; i < spokes; ++i) {
        Line l;
        float x, y, a;
        if (kind == 1) {  // off the tear: from a point along it, out to one side
            const float k = (float(i / 2) + 0.5f) / float((spokes + 1) / 2);
            x = ix;
            y = iy - len + 2.f * len * k + r.r(-0.04f, 0.04f);
            a = (i % 2 ? kPi : 0.f) + r.r(-0.75f, 0.75f);
        } else {
            x = ix;
            y = iy;
            a = base + 2.f * kPi * (float(i) + r.r(-0.3f, 0.3f)) / float(spokes);
        }
        const float x0 = x, y0 = y;
        l.x.push_back(x);
        l.y.push_back(y);
        l.d.push_back(kind == 1 ? 0.f : 0.f);
        for (int s = 0; s < 60; ++s) {  // straight-ish, with small jags
            const float step = r.r(0.07f, 0.13f);
            const float dir = a + r.r(-0.22f, 0.22f);
            x += std::cos(dir) * step;
            y += std::sin(dir) * step;
            l.x.push_back(x);
            l.y.push_back(y);
            l.d.push_back(kind == 1 ? std::fabs(x - x0) + 0.3f * std::fabs(y - y0) : std::hypot(x - x0, y - y0));
            if (!inBox(x, y, xMin, xMax, yMin, yMax, 0.08f)) break;
        }
        lines.push_back(l);
        addLine(cracks, l, 1.f);
        // a branch or two
        const int nb = r.f() < 0.6f ? 1 : 0;
        for (int b = 0; b < nb && l.x.size() > 4; ++b) {
            const size_t at = 2 + size_t(r.next() % uint32_t(l.x.size() - 3));
            Line br;
            float bx = l.x[at], by = l.y[at], bd = l.d[at];
            const float ba = a + (r.f() < 0.5f ? -1.f : 1.f) * r.r(0.4f, 0.9f);
            br.x.push_back(bx);
            br.y.push_back(by);
            br.d.push_back(bd);
            const int n = 2 + int(r.next() % 3u);
            for (int s = 0; s < n; ++s) {
                const float step = r.r(0.06f, 0.1f);
                bx += std::cos(ba + r.r(-0.2f, 0.2f)) * step;
                by += std::sin(ba + r.r(-0.2f, 0.2f)) * step;
                bd += step;
                br.x.push_back(bx);
                br.y.push_back(by);
                br.d.push_back(bd);
            }
            addLine(cracks, br, 0.55f);
        }
    }
    // rings of chords between neighbouring cracks (an impact; a tear: between cracks on the same side)
    static const float rings[] = {0.16f, 0.36f, 0.62f, 0.95f, 1.35f};
    const int nr = 5;
    std::vector<std::vector<int>> hit(size_t(spokes), std::vector<int>(nr, 0));
    std::vector<std::vector<float>> hx(size_t(spokes), std::vector<float>(nr, 0.f)), hy = hx;
    for (int i = 0; i < spokes; ++i)
        for (int k = 0; k < nr; ++k) {
            const float want = rings[k] * r.r(0.9f, 1.1f);
            hit[size_t(i)][size_t(k)] = pointAt(lines[size_t(i)], want, hx[size_t(i)][size_t(k)], hy[size_t(i)][size_t(k)]) ? 1 : 0;
        }
    for (int i = 0; i < spokes; ++i) {
        const int j = kind == 1 ? i + 2 : (i + 1) % spokes;  // the next crack round (a tear: the next one on its side)
        if (j >= spokes) continue;
        for (int k = 0; k < nr; ++k) {
            if (!hit[size_t(i)][size_t(k)] || !hit[size_t(j)][size_t(k)]) continue;
            if (r.f() > (k < 2 ? 0.6f : 0.35f)) continue;
            const float ax = hx[size_t(i)][size_t(k)], ay = hy[size_t(i)][size_t(k)], bx = hx[size_t(j)][size_t(k)], by = hy[size_t(j)][size_t(k)];
            const float mx = 0.5f * (ax + bx) + r.r(-0.03f, 0.03f), my = 0.5f * (ay + by) + r.r(-0.03f, 0.03f);
            cracks.push_back({ax, ay, mx, my, rings[k], 0.6f});
            cracks.push_back({mx, my, bx, by, rings[k], 0.6f});
        }
        // the shards between the two cracks, ring by ring
        if (!shards) continue;
        for (int k = 0; k + 1 < nr; ++k) {
            if (!hit[size_t(i)][size_t(k)] || !hit[size_t(j)][size_t(k)] || !hit[size_t(i)][size_t(k + 1)] || !hit[size_t(j)][size_t(k + 1)]) continue;
            if (r.f() < 0.35f) continue;
            Shard s;
            s.xs[0] = hx[size_t(i)][size_t(k)];
            s.ys[0] = hy[size_t(i)][size_t(k)];
            s.xs[1] = hx[size_t(i)][size_t(k + 1)];
            s.ys[1] = hy[size_t(i)][size_t(k + 1)];
            s.xs[2] = hx[size_t(j)][size_t(k + 1)];
            s.ys[2] = hy[size_t(j)][size_t(k + 1)];
            s.xs[3] = hx[size_t(j)][size_t(k)];
            s.ys[3] = hy[size_t(j)][size_t(k)];
            const float t = r.f();
            s.tint = t < 0.45f ? 0 : t < 0.75f ? 1 : t < 0.9f ? 2 : 3;
            shards->push_back(s);
        }
    }
}

void drawShards(Canvas& c, const std::vector<Shard>& v, float cx, float cy, float R, int alpha) {
    static const uint32_t tints[4] = {rgb(170, 22, 36), rgb(6, 2, 10), rgb(230, 96, 40), rgb(120, 40, 150)};
    static const int as[4] = {46, 90, 34, 40};
    for (const Shard& s : v) {
        float xs[4], ys[4];
        for (int k = 0; k < 4; ++k) {
            xs[k] = cx + s.xs[k] * R;
            ys[k] = cy + s.ys[k] * R;
        }
        c.fillPolygon(xs, ys, 4, tints[s.tint], as[s.tint] * alpha / 255);
        // the lit facet edge of the piece (towards the break)
        c.line(xs[0], ys[0], xs[3], ys[3], rgb(255, 170, 150), std::max(1.f, R * 0.004f), 70 * alpha / 255);
    }
}

void drawCracks(Canvas& c, const std::vector<Crack>& v, float cx, float cy, float R, int alpha, int glow) {
    for (const Crack& k : v)  // the bloom
        c.line(cx + k.x0 * R, cy + k.y0 * R, cx + k.x1 * R, cy + k.y1 * R, rgb(122, 6, 20), std::max(3.f, R * 0.045f * k.w), 34 * alpha * glow / 65025);
    for (const Crack& k : v)  // the gap
        c.line(cx + k.x0 * R, cy + k.y0 * R, cx + k.x1 * R, cy + k.y1 * R, rgb(12, 2, 6), std::max(2.f, R * 0.012f * k.w), 200 * alpha / 255);
    for (const Crack& k : v)  // the hot line inside
        c.line(cx + k.x0 * R, cy + k.y0 * R, cx + k.x1 * R, cy + k.y1 * R, rgb(255, 68, 36), std::max(1.f, R * 0.0055f * k.w),
               (k.w >= 1.f ? 150 : 90) * alpha * glow / 65025);
}

void drawCrackPulse(Canvas& c, const std::vector<Crack>& v, float cx, float cy, float R, uint32_t now, float period, int dir, int strength) {
    if (strength <= 0) return;
    const float t = float(now % 3600000u) / 1000.f;
    const float base = 2.f * kPi * t / std::max(0.5f, period);
    for (const Crack& k : v) {
        const float ph = base - float(dir) * 2.f * kPi * 1.6f * k.d;
        const float sn = std::sin(ph);
        if (sn <= 0.f) continue;
        const float w = sn * sn * sn * sn * sn * sn;
        if (w < 0.05f) continue;
        const int a = int(float(strength) * w);
        const float x0 = cx + k.x0 * R, y0 = cy + k.y0 * R, x1 = cx + k.x1 * R, y1 = cy + k.y1 * R;
        c.line(x0, y0, x1, y1, rgb(255, 70, 30), std::max(2.f, R * 0.03f * k.w), a / 4);
        c.line(x0, y0, x1, y1, rgb(255, 120, 60), std::max(1.f, R * 0.01f * k.w), a * 3 / 4);
        if (w > 0.4f) c.line(x0, y0, x1, y1, rgb(255, 240, 210), 1.f, a);
    }
}

void bolt(Canvas& c, float x0, float y0, float x1, float y1, uint32_t seed, float wiggle, int alpha, float width, bool electric) {
    if (alpha <= 0) return;
    const uint32_t glow = electric ? rgb(90, 150, 255) : rgb(255, 50, 60), mid = electric ? rgb(170, 210, 255) : rgb(255, 130, 110),
                   core = electric ? rgb(240, 248, 255) : rgb(255, 236, 220);
    Rng r(seed);
    const int n = 10;
    float px = x0, py = y0;
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
    const float nx = -dy / len, ny = dx / len;
    for (int i = 1; i <= n; ++i) {
        const float k = float(i) / float(n);
        const float off = i == n ? 0.f : r.r(-wiggle, wiggle);
        const float x = x0 + dx * k + nx * off, y = y0 + dy * k + ny * off;
        c.line(px, py, x, y, glow, std::max(2.f, width * 5.f), alpha / 4);
        c.line(px, py, x, y, mid, std::max(1.f, width * 2.2f), alpha * 3 / 4);
        c.line(px, py, x, y, core, std::max(1.f, width), alpha);
        if ((i == n / 2 || i == n / 3) && r.f() < 0.65f) {  // a fork
            const float fx = x + nx * len * r.r(-0.22f, 0.22f) + dx * 0.14f, fy = y + ny * len * r.r(-0.22f, 0.22f) + dy * 0.14f;
            const float mx = 0.5f * (x + fx) + nx * r.r(-wiggle, wiggle) * 0.5f, my = 0.5f * (y + fy) + ny * r.r(-wiggle, wiggle) * 0.5f;
            c.line(x, y, mx, my, mid, std::max(1.f, width * 1.4f), alpha / 2);
            c.line(mx, my, fx, fy, mid, std::max(1.f, width), alpha / 3);
        }
        px = x;
        py = y;
    }
}

void tear(Canvas& c, float x, float y, float halfW, float halfH, uint32_t seed, uint32_t col, int alpha) {
    const int n = 24;
    float xs[2 * n + 2], ys[2 * n + 2];
    for (int side = 0; side < 2; ++side) {
        Rng r(seed * 31u + uint32_t(side) * 7u + 1u);  // the same jags every frame
        for (int i = 0; i <= n; ++i) {
            const int j = side == 0 ? i : n - i;
            const float v = -1.f + 2.f * float(j) / float(n);
            const float prof = (1.f - v * v) * (1.f - 0.25f * std::fabs(v));
            const float jag = (j == 0 || j == n) ? 0.f : r.r(-0.35f, 0.35f);
            xs[side * (n + 1) + i] = x + (side == 0 ? 1.f : -1.f) * halfW * prof * (1.f + jag);
            ys[side * (n + 1) + i] = y + v * halfH;
        }
    }
    c.fillPolygon(xs, ys, 2 * n + 2, col, alpha);
}

void river(Canvas& c, const float* xs, const float* ys, int n, float w, uint32_t col, int alpha) {
    if (n < 2) return;
    std::vector<float> nx(static_cast<size_t>(n)), ny(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {  // the normal at each point: the average of its two segments'
        const int a = std::max(0, i - 1), b = std::min(n - 1, i + 1);
        const float dx = xs[b] - xs[a], dy = ys[b] - ys[a], l = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
        nx[size_t(i)] = -dy / l;
        ny[size_t(i)] = dx / l;
    }
    for (int i = 0; i + 1 < n; ++i) {
        const float h = w * 0.5f;
        float qx[4] = {xs[i] + nx[size_t(i)] * h, xs[i + 1] + nx[size_t(i + 1)] * h, xs[i + 1] - nx[size_t(i + 1)] * h, xs[i] - nx[size_t(i)] * h};
        float qy[4] = {ys[i] + ny[size_t(i)] * h, ys[i + 1] + ny[size_t(i + 1)] * h, ys[i + 1] - ny[size_t(i + 1)] * h, ys[i] - ny[size_t(i)] * h};
        c.fillPolygon(qx, qy, 4, col, alpha);
    }
}

void ellipse(Canvas& c, float x, float y, float rx, float ry, uint32_t col, int alpha) {
    float xs[20], ys[20];
    for (int i = 0; i < 20; ++i) {
        const float a = 2.f * kPi * float(i) / 20.f;
        xs[i] = x + std::cos(a) * rx;
        ys[i] = y + std::sin(a) * ry;
    }
    c.fillPolygon(xs, ys, 20, col, alpha);
}

}  // namespace battlefx
