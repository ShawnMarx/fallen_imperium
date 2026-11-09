// The ships as pixel art (0.49.0, owner: "full-detail battles" in Retrowave - pixel-art ships in today's designs).
// Each ship is the owner's unit picture (icons "unit/<type>", from his unit reference card) cut down to a pixel
// sprite of the size it is shown at: a crisp silhouette with a dark outline, the hull shaded as a cylinder lit from
// above (the war sun as a sphere) in six tones of the player's colour, the picture's panel lines kept, a stripe of
// the full colour along the spine, lit windows that blink and the engine nozzles found at the stern. Sprites are
// made once per (type, length, colour) and drawn one sprite pixel = one block of the battle's pixel grid, so the
// Retrowave chunky pass leaves them as they are.
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "engine/data.h"
#include "gfx/canvas.h"

namespace pxship {

struct Sprite {
    int w = 0, h = 0;
    std::vector<uint32_t> px;  // 0 = clear, else 0xFF000000 | colour
    std::vector<int> lights;   // pixel indices of the windows
    struct Nozzle {
        int y0, y1, x;         // engine rows y0..y1, the flame starts left of column x (ships face right)
    };
    std::vector<Nozzle> nozzles;
    std::vector<int> hull;     // pixel indices inside the outline (for fire / sparks on a damaged hull)
};

// The ship `len` sprite pixels long in colour col; nullptr when the picture is missing.
const Sprite* get(ti::UnitType t, int len, uint32_t col);

struct Grid {
    int x = 0, y = 0, f = 0;   // the pixel grid: origin and block size, in real pixels (f 0: none)
};
struct Look {
    bool faceLeft = false;
    int alpha = 255;
    int whiten = 0;            // 0..255 towards white (a flagship imploding)
    bool engines = true;       // flames at the nozzles
    uint32_t seed = 0;         // the windows' blinking and the flames' flicker
    int hurt = 0;              // 0 whole; 1 damaged: a burning breach (fire pixels round hurtAt)
    int hurtX = 0, hurtY = 0;  // the breach, in sprite pixels (before mirroring)
};
// Draws it centred on (cx, cy) - real pixels - snapped to the grid.
void draw(Canvas& c, const Sprite& s, float cx, float cy, const Grid& g, const Look& k, uint32_t now);
// A ground unit's picture (mech, PDS, space dock) standing with its bottom edge on footY (real pixels).
void drawOn(Canvas& c, const Sprite& s, float cx, float footY, const Grid& g, const Look& k, uint32_t now);
// The sprite as large as the picture would be drawn in a maxW x maxH box (game units) on canvas c's grid g
// (nullptr: no grid or no picture).
const Sprite* fit(const Canvas& c, const Grid& g, ti::UnitType t, float maxW, float maxH, uint32_t col);
// The picture of t ("unit/<type>") or nullptr.
const char* pictureOf(ti::UnitType t);

// ---- any drawing as pixel art (0.50.0: the factions' ground troops). `draw` paints the unit with its anchor at
// (ax, ay) game units on the canvas it is given. It is drawn alone, four canvas pixels to a grid pixel, on black and
// on white (for its alpha), then cut down to grid pixels: solid where it covers half a pixel or more, dithered where
// it is faint (glows, shadows, smoke), colours posterised, a dark outline round the solid part. The box (left, top,
// w, h) is in game units from the anchor. Cached by key (callers put everything the picture depends on in it).
struct Pixelized {
    Sprite s;
    int ax = 0, ay = 0;     // the anchor's grid pixel in the sprite
    uint32_t used = 0;
};
const Pixelized* pixelize(const Canvas& c, const Grid& g, uint64_t key, float left, float top, float w, float h,
                          const std::function<void(Canvas&, float, float)>& draw, uint32_t now);
// Draws it with its anchor at real pixel (x, y), snapped to the grid.
void drawPixelized(Canvas& c, const Pixelized& p, float x, float y, const Grid& g, int alpha);

// ---- the battle effects as pixel art on the same grid (Retrowave; space and ground combat)
uint32_t hash(uint32_t h);
float dith(int gx, int gy);                                   // ordered dither threshold 0..1 (4 x 4)
inline int gridX(const Grid& g, float px) { return int(std::floor((px - float(g.x)) / float(g.f))); }
inline int gridY(const Grid& g, float py) { return int(std::floor((py - float(g.y)) / float(g.f))); }
void dot(Canvas& c, const Grid& g, int gx, int gy, uint32_t col, int a = 255);
// a fireball r grid pixels across at moment k (0..1): hot core, orange, red rim, a ragged boiling edge, dissolving
void fireball(Canvas& c, const Grid& g, int cx, int cy, float r, float k, uint32_t seed, uint32_t now);
// a blinding disc that hollows out into a thinning ring
void flash(Canvas& c, const Grid& g, int cx, int cy, float r, float k);
// a star of light (muzzle flash), two frames
void muzzle(Canvas& c, const Grid& g, int cx, int cy, float r, float k);
// a ragged little cloud that thins from its edge as it fades (fade 1..0)
void puff(Canvas& c, const Grid& g, int cx, int cy, float r, float fade, uint32_t seed, uint32_t col, int a);
// a hot pixel with a short tail behind it (moving vx, vy)
void spark(Canvas& c, const Grid& g, int x, int y, float vx, float vy, uint32_t tail, float k);

}  // namespace pxship
