// The "Retrowave" tileset (0.26.0, owner: "the pixel animated graphics should
// be called Retrowave"; the current look is "Simplistic" and stays). The art
// comes from assets/retro.dat (tools/gen_retro.py, the gen_tile_art style
// the owner approved): animated backgrounds, wormhole / token sprites and a
// texture for every planet. A tile is composed at the art's own resolution
// (corner radius 48 - 96 x 84 pixels) from its background, its wormholes and
// its planets, turned and lit as spheres, then drawn at the tile's size.
// Also here: the chunky pixel look of the battles (drawn small, enlarged).
#pragma once
#include <cstdint>
#include <vector>

#include "gfx/canvas.h"

namespace retro {

constexpr float kArtR = 48.f;  // corner radius of the art

// Tilesets (setting "tileset"): 0 Simplistic (drawn shapes), 1 Retrowave (this file), 2 Voidfall
// (0.32.0, src/ui/voidfall.*: high definition, animated). A tileset whose art is missing falls back
// to Simplistic.
enum { kSimplistic = 0, kRetrowave = 1, kVoidfall = 2, kTilesets = 3 };
bool available();  // retro.dat found and read
bool on();         // Retrowave chosen and available (the pixel look: chunky battles, whole-number scales)
bool art();        // an art tileset (Retrowave or Voidfall): tiles are drawn by draw()
bool hd();         // Voidfall chosen and available
int tileset();     // the one in use
void setTileset(int t);
bool tilesetAvailable(int t);
void setOn(bool on);  // Retrowave on / Simplistic (tests)
const char* tilesetName(int t);  // "Simplistic" / "Retrowave" / "Voidfall"

// One thing on a tile, in art units from the tile's centre (corner radius 48).
struct Piece {
    enum Kind : uint8_t { Planet, Wormhole, IonStorm, Tear };
    Kind kind = Planet;
    int id = -1;              // Planet: kPlanets index; Wormhole: 0 alpha 1 beta 2 gamma 3 delta; IonStorm: 0 alpha 1 beta
    float x = 0, y = 0, r = 0;
    bool dim = false;         // an exhausted planet
};

struct Tile {
    int bg = -1;              // background animation (backgroundFor)
    int def = -1;             // the printed system (setBackground; the Voidfall tileset picks its own sky)
    uint32_t seed = 0;
    bool nexusActive = false;
    uint8_t flip = 0;         // starfields: 1 mirrored left-right, 2 top-bottom
    uint32_t lanes = 0;       // hyperlanes: bit (a * 6 + b) joins edge a to edge b (a < b, 0 = north, clockwise)
    const char* sky = nullptr;  // Voidfall (0.43.1): a named sky instead of the printed system's (Thunder's Edge's storm)
    std::vector<Piece> pieces;
};

// The background for a printed tile: its own for anomalies, Mallice, the
// Creuss gate and The Dark, else one of the starfields (seed picks which and
// how it is mirrored). Sets t.bg and t.flip.
void setBackground(Tile& t, int systemDef, uint32_t seed, bool nexusActive = false);

// Draws the tile centred at (cx, cy) with corner radius `size` screen pixels.
void draw(Canvas& c, const Tile& t, float cx, float cy, float size, uint32_t now, int alpha = 255);
// The largest whole-number scale at which the art fits a tile of this size
// (the expanded view draws at whole steps so every art pixel is the same size).
float snapScale(float size);
// Pixels of art per design pixel in the pack in use (0.48.0): 2 for the sharp PC art (retro-pc.dat, used when the
// screen is enlarged 1.5x or more), else 1. Things drawn "in art pixels" (the Fracture's dots, the Thunder's Edge
// storm cells, the chunky passes) are that much finer.
int artScale();

// Chunky pixels (the battles in Retrowave): the area is cut into factor x
// factor blocks, each block takes its average colour moved onto the
// Retrowave palette with ordered dithering.
void chunky(Canvas& c, Rect area, int factor);
// The grid chunky(c, area, factor) cuts into: its origin and block size in real pixels (0.49.0: the pixel-art ships
// are drawn on it, so the pass leaves them as they are).
void chunkyGrid(const Canvas& c, Rect area, int factor, int& ox, int& oy, int& fpx);

}  // namespace retro
