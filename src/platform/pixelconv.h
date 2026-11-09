// Converts the canvas into whatever pixel format the screen uses, centring it
// and optionally turning it 180 degrees.
#pragma once
#include <cstdint>
#include <cstring>

#include "gfx/canvas.h"

struct PixelFormatDesc {
    int bpp = 32;  // 16 or 32
    int rShift = 16, gShift = 8, bShift = 0;
    int rBits = 8, gBits = 8, bBits = 8;
};

inline void convertFrame(const Canvas& c, uint8_t* dst, int pitch, int dstW, int dstH, const PixelFormatDesc& f,
                         bool rotate) {
    const int cw = c.pw(), ch = c.ph();  // the real pixels (0.27.0)
    const int w = cw < dstW ? cw : dstW, h = ch < dstH ? ch : dstH;
    const int ox = (dstW - w) / 2, oy = (dstH - h) / 2;
    const uint32_t* src = c.pixels();
    const bool native32 = f.bpp == 32 && f.rShift == 16 && f.gShift == 8 && f.bShift == 0;
    for (int y = 0; y < h; ++y) {
        const uint32_t* s = src + size_t(y) * cw;
        int dy = rotate ? (dstH - 1 - (oy + y)) : (oy + y);
        uint8_t* row = dst + size_t(dy) * pitch;
        if (f.bpp == 32) {
            uint32_t* d = reinterpret_cast<uint32_t*>(row);
            if (!rotate && native32) {
                std::memcpy(d + ox, s, size_t(w) * 4);
                continue;
            }
            for (int x = 0; x < w; ++x) {
                uint32_t p = s[x];
                uint32_t v = native32 ? p
                                      : (((p >> 16) & 255u) >> (8 - f.rBits) << f.rShift) |
                                            (((p >> 8) & 255u) >> (8 - f.gBits) << f.gShift) |
                                            ((p & 255u) >> (8 - f.bBits) << f.bShift);
                int dx = rotate ? (dstW - 1 - (ox + x)) : (ox + x);
                d[dx] = v;
            }
        } else {  // 16 bpp
            uint16_t* d = reinterpret_cast<uint16_t*>(row);
            for (int x = 0; x < w; ++x) {
                uint32_t p = s[x];
                uint16_t v = uint16_t((((p >> 16) & 255u) >> (8 - f.rBits) << f.rShift) |
                                      (((p >> 8) & 255u) >> (8 - f.gBits) << f.gShift) |
                                      ((p & 255u) >> (8 - f.bBits) << f.bShift));
                int dx = rotate ? (dstW - 1 - (ox + x)) : (ox + x);
                d[dx] = v;
            }
        }
    }
}
