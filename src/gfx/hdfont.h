// Text for the sharp PC picture (0.27.0): the game's fonts rasterised at the
// screen's real size with stb_truetype (public domain / MIT) from DejaVu Sans
// subsets (tools/gen_hdfont.py). Not built for the Miyoo (always 640x480).
#pragma once
#include <cstdint>
#include <vector>

struct FontData;

namespace hdfont {

struct Glyph {
    int w = 0, h = 0, xo = 0, yo = 0;  // bitmap size; offset from the pen / baseline
    float adv = 0;                     // advance in real pixels
    std::vector<uint8_t> bits;
};

bool available();
// The glyph of `cp` for font `f` drawn `scale` times its size (cached).
const Glyph* glyph(const FontData& f, uint32_t cp, float scale);

}  // namespace hdfont
