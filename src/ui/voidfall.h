// The "Voidfall" tileset (0.32.0, owner: "New tileset = Voidfall. Use the Voidfall art for the aesthetic
// and palette ... Do not make this pixelated, but high definition. Make this animated as well.")
//
// The art comes from voidfall-pc.dat / voidfall-miyoo.dat (tools/gen_voidfall.py): painted nebula skies,
// moving layers (drifting veils, a rift's turning spiral, a star's corona, sliding asteroids) and a
// surface map for every planet. A tile is built at the screen's own resolution - the sky, a fine
// triangle lattice and the printed tile's double edge - and kept; on every animation step the moving
// layers, twinkling stars, the lattice's passing shimmer, turning planets, wormholes and storms are
// drawn over it. Tiles are described the same way as for the Retrowave tileset (retro::Tile).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gfx/canvas.h"
#include "gfx/image.h"
#include "ui/retro.h"

namespace voidfall {

bool available();  // the pack was found and read
// Draws the tile centred at (cx, cy) with corner radius `size` (game units).
void draw(Canvas& c, const retro::Tile& t, float cx, float cy, float size, uint32_t now, int alpha = 255);

// ---- the Voidfall battle look and New Game backdrop (0.33.1, owner: "its own battle look and New Game
// backdrop to match the theme")
// The sky a tile is painted with ("star3", "41", ...).
const char* skyOf(const retro::Tile& t);
// A picture from the pack ("newgame", or a sky) fitted to w x h real pixels; false if it isn't there.
bool picture(const std::string& name, int w, int h, Image& out);
// The same, covering w x h without stretching (widescreen, 0.46.0): enlarged until it fills both ways,
// the overflow trimmed equally from both sides.
bool pictureCover(const std::string& name, int w, int h, Image& out);
// 0.43.2: the light running down a sky's cracks (the Fracture's skies) for a battle backdrop painted with
// picture(sky, w, h): the cracks' pixels found once (false: the sky has none), then each frame drawn onto the
// canvas's real pixels with the backdrop's top left at (x0, y0); strength 0..256.
struct SkyPulse {
    int w = 0, h = 0;
    std::vector<uint32_t> at;  // y * w + x
    std::vector<uint8_t> m;    // how much crack
    std::vector<uint16_t> rr;  // distance from the middle (65535 = half the sky's width)
    uint32_t col = 0;
    float period = 3.f;
    int dir = 1;
};
bool skyPulse(const std::string& sky, int w, int h, SkyPulse& out);
void drawSkyPulse(Canvas& c, const SkyPulse& p, int x0, int y0, uint32_t now, int strength);
// The void's tendrils (after the box art): bundles of tapering strands of crimson and magenta, embers at
// the tips, reaching in from beyond the upper right of `area` and swaying. reach: how far in (share of
// the area's height).
void tendrils(Canvas& c, Rect area, uint32_t now, uint32_t seed, int count, float reach, int alpha);
// The painted look over an area (real pixels): violet shadows, warm lights and the box art's diagonal
// print lines.
void grade(Canvas& c, Rect area);
// A ship's engine flame: from (x, y) away along dir (-1 = to the left), len / width in game units.
void flame(Canvas& c, float x, float y, float dir, float len, float width, uint32_t now, uint32_t seed, int alpha = 255);

}  // namespace voidfall
