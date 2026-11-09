// Colours shared by every screen.
#pragma once
#include "gfx/canvas.h"

namespace theme {
constexpr int kScreenW = 640;  // the width every screen was laid out for (the Miyoo's)
constexpr int kScreenH = 480;
// Widescreen (0.46.0): the screen's width in game units - 640 on the Miyoo and in 4:3,
// wider on a widescreen PC (always kScreenH tall). Set by the app with its canvas.
inline int& screenWRef() {
    static int w = kScreenW;
    return w;
}
inline int screenW() { return screenWRef(); }
// where a screen laid out for 640 units starts when it is drawn centred on a wider one
inline int legacyX() { return (screenW() - kScreenW) / 2; }

constexpr uint32_t bg = rgb(7, 10, 20);
constexpr uint32_t bgTop = rgb(12, 17, 34);
constexpr uint32_t panel = rgb(17, 23, 40);
constexpr uint32_t panelHi = rgb(27, 36, 62);
constexpr uint32_t border = rgb(44, 58, 92);
constexpr uint32_t text = rgb(226, 230, 240);
constexpr uint32_t dim = rgb(134, 145, 170);
constexpr uint32_t faint = rgb(80, 90, 115);
constexpr uint32_t gold = rgb(232, 182, 76);
constexpr uint32_t goldDim = rgb(150, 112, 40);
constexpr uint32_t cyan = rgb(84, 196, 232);
constexpr uint32_t red = rgb(224, 82, 82);
constexpr uint32_t green = rgb(92, 204, 122);
constexpr uint32_t black = rgb(0, 0, 0);

// Resource / influence badges
constexpr uint32_t resource = rgb(224, 178, 60);
constexpr uint32_t influence = rgb(70, 140, 230);

// Planet traits
constexpr uint32_t cultural = rgb(78, 140, 224);
constexpr uint32_t hazardous = rgb(214, 86, 76);
constexpr uint32_t industrial = rgb(80, 178, 104);

// The eight player colours
constexpr uint32_t player[8] = {rgb(222, 66, 66),  rgb(64, 128, 230), rgb(64, 176, 92),  rgb(232, 200, 60),
                                rgb(156, 92, 216), rgb(120, 120, 128), rgb(238, 140, 48), rgb(230, 108, 170)};
constexpr const char* playerColorName[8] = {"Red", "Blue", "Green", "Yellow", "Purple", "Black", "Orange", "Pink"};
constexpr uint32_t neutral = rgb(206, 200, 184);  // Thunder's Edge (0.34.0): neutral units (pale stone)
}  // namespace theme
