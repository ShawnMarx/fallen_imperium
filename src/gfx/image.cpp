#include "gfx/image.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

Image resizeImage(const Image& src, int w, int h) {
    Image out;
    if (src.empty() || w <= 0 || h <= 0) return out;
    out.w = w;
    out.h = h;
    out.mono = src.mono;
    out.shaded = src.shaded;
    out.px.assign(size_t(w) * h, 0);
    const double sx = double(src.w) / w, sy = double(src.h) / h;
    // Per destination column/row: the source range it covers.
    for (int y = 0; y < h; ++y) {
        double y0 = y * sy, y1 = (y + 1) * sy;
        if (sy < 1) {  // enlarging: sample a one-pixel window centred on the point
            double c = (y + 0.5) * sy - 0.5;
            y0 = std::max(0.0, c);
            y1 = std::min(double(src.h), y0 + 1);
        }
        for (int x = 0; x < w; ++x) {
            double x0 = x * sx, x1 = (x + 1) * sx;
            if (sx < 1) {
                double c = (x + 0.5) * sx - 0.5;
                x0 = std::max(0.0, c);
                x1 = std::min(double(src.w), x0 + 1);
            }
            double acc[4] = {0, 0, 0, 0}, area = 0;
            for (int yy = int(y0); yy < int(std::ceil(y1)) && yy < src.h; ++yy) {
                double wy = std::min(y1, double(yy + 1)) - std::max(y0, double(yy));
                if (wy <= 0) continue;
                for (int xx = int(x0); xx < int(std::ceil(x1)) && xx < src.w; ++xx) {
                    double wx = std::min(x1, double(xx + 1)) - std::max(x0, double(xx));
                    if (wx <= 0) continue;
                    double k = wx * wy;
                    uint32_t p = src.px[size_t(yy) * src.w + xx];
                    acc[0] += k * (p >> 24);
                    acc[1] += k * ((p >> 16) & 255);
                    acc[2] += k * ((p >> 8) & 255);
                    acc[3] += k * (p & 255);
                    area += k;
                }
            }
            if (area <= 0) continue;
            uint32_t ch[4];
            for (int i = 0; i < 4; ++i) ch[i] = uint32_t(std::min(255.0, acc[i] / area + 0.5));
            for (int i = 1; i < 4; ++i) ch[i] = std::min(ch[i], ch[0]);
            out.px[size_t(y) * w + x] = (ch[0] << 24) | (ch[1] << 16) | (ch[2] << 8) | ch[3];
        }
    }
    return out;
}

namespace {
struct Reader {
    const std::vector<uint8_t>& b;
    size_t i = 0;
    bool ok = true;
    bool need(size_t n) {
        if (i + n > b.size()) ok = false;
        return ok;
    }
    uint32_t u8() { return need(1) ? b[i++] : 0; }
    uint32_t u16() {
        if (!need(2)) return 0;
        uint32_t v = b[i] | (b[i + 1] << 8);
        i += 2;
        return v;
    }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (uint32_t(b[i + 3]) << 24);
        i += 4;
        return v;
    }
};
}  // namespace

bool loadIconPack(const std::string& path, std::vector<NamedImage>& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> b;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    if (b.size() < 8 || std::memcmp(b.data(), "TI4I", 4) != 0) return false;
    Reader r{b};
    r.i = 4;
    if (r.u16() != 1) return false;
    int count = int(r.u16());
    for (int k = 0; k < count && r.ok; ++k) {
        NamedImage e;
        int len = int(r.u8());
        if (!r.need(size_t(len))) break;
        e.name.assign(reinterpret_cast<const char*>(&b[r.i]), size_t(len));
        r.i += size_t(len);
        e.img.w = int(r.u16());
        e.img.h = int(r.u16());
        const int flags = r.u8();
        e.img.mono = (flags & 1) != 0;
        e.img.shaded = (flags & 2) != 0;
        size_t bytes = r.u32();
        if (!r.need(bytes) || e.img.w <= 0 || e.img.h <= 0 || e.img.w > 1024 || e.img.h > 1024) {
            r.ok = false;
            break;
        }
        size_t end = r.i + bytes, total = size_t(e.img.w) * e.img.h;
        e.img.px.reserve(total);
        while (r.i < end && e.img.px.size() < total) {
            uint32_t hdr = r.u8();
            if (hdr < 128) {
                uint32_t p = r.u32();
                e.img.px.insert(e.img.px.end(), std::min<size_t>(hdr + 1, total - e.img.px.size()), p);
            } else {
                for (uint32_t j = 0; j < hdr - 127 && r.i + 4 <= end && e.img.px.size() < total; ++j)
                    e.img.px.push_back(r.u32());
            }
        }
        r.i = end;
        if (e.img.px.size() != total) {
            r.ok = false;
            break;
        }
        out.push_back(std::move(e));
    }
    if (!r.ok) out.clear();
    return !out.empty();
}
