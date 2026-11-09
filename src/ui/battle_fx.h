// Shared pieces of the battle backdrops (0.43.2, owner: "update the ground and space combat backgrounds in
// the Fracture for all tilesets ... have one unique to Thunder's Edge as well"):
//  - broken space: a net of cracks out from a point of impact (or off both sides of a tear), the shards
//    between them, the cracks drawn glowing and a light running down them;
//  - lightning: a forked bolt (the Fracture's red or Thunder's Edge's blue-white).
// Everything is in tile radii (R) from the middle of the hex, drawn at (cx + x R, cy + y R).
#pragma once
#include <cstdint>
#include <vector>

class Canvas;

namespace battlefx {

struct Crack {
    float x0, y0, x1, y1;
    float d;  // how far from the break (R) - the light runs out from it
    float w;  // 1 a main crack, less for the rings and branches
};
struct Shard {  // a piece of broken sky between two cracks
    float xs[4], ys[4];
    int tint;  // 0 crimson, 1 obsidian, 2 ember, 3 violet
};

// kind 0: an impact at (ix, iy), `spokes` cracks out from it and rings of chords between them;
// kind 1: a tear - a vertical slit at x = ix from y = iy - len to iy + len, cracks off both sides.
// The net reaches the box xMin..xMax, yMin..yMax.
void crackNet(std::vector<Crack>& cracks, std::vector<Shard>* shards, uint32_t seed, int kind, float ix, float iy, float len, int spokes,
              float xMin, float xMax, float yMin, float yMax);
// The shards, tinted (alpha 0..255 scales them).
void drawShards(Canvas& c, const std::vector<Shard>& v, float cx, float cy, float R, int alpha);
// The cracks: a dark gap, the red glow round it, a hot line inside. glow 0..255.
void drawCracks(Canvas& c, const std::vector<Crack>& v, float cx, float cy, float R, int alpha, int glow = 255);
// The light running down the cracks: waves out from the break (dir 1) or in to it (dir -1), one every
// `period` seconds. strength 0..255.
void drawCrackPulse(Canvas& c, const std::vector<Crack>& v, float cx, float cy, float R, uint32_t now, float period, int dir, int strength);
// A forked bolt from (x0, y0) to (x1, y1) (screen units); width = its core's thickness.
void bolt(Canvas& c, float x0, float y0, float x1, float y1, uint32_t seed, float wiggle, int alpha, float width, bool electric);
// A tear in space: a pointed lens with jagged lips, centred at (x, y) (screen units).
void tear(Canvas& c, float x, float y, float halfW, float halfH, uint32_t seed, uint32_t col, int alpha);
// A band of even width along a polyline of n points (rivers; no overlaps at the joints).
void river(Canvas& c, const float* xs, const float* ys, int n, float w, uint32_t col, int alpha);
// A filled ellipse (screen units).
void ellipse(Canvas& c, float x, float y, float rx, float ry, uint32_t col, int alpha);

}  // namespace battlefx
