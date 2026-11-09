// Small RGBA images (the game's icons). Pixels are premultiplied
// 0xAARRGGBB, so drawing one is a single multiply-add per channel.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Image {
    int w = 0, h = 0;
    bool mono = false;    // shape only (white); drawn in a chosen colour
    bool shaded = false;  // a grey picture painted in a chosen colour (unit art)
    std::vector<uint32_t> px;
    bool empty() const { return w <= 0 || h <= 0; }
};

// Area-averaged resize (every source pixel contributes by the share of it
// that a destination pixel covers). Good for shrinking icons; also enlarges.
Image resizeImage(const Image& src, int w, int h);

// Reads an icon pack written by tools/gen_icons.py. Returns false (and leaves
// `out` empty) when the file is missing or damaged.
struct NamedImage {
    std::string name;
    Image img;
};
bool loadIconPack(const std::string& path, std::vector<NamedImage>& out);
