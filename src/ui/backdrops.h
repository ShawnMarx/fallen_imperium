// Animated backdrops for the menus (0.30.0, owner's 0.29.1 feedback), drawn in
// chunky pixels (2x2 on the 640x480 screen) like the Retrowave tiles:
//  - the New Game screen (Retrowave tileset): a bleeding red tear in the cosmos on
//    the left, an eldritch green tentacle that reaches out of it now and then, and a
//    small, distant cruiser crossing the top of the screen with its thrusters lit;
//  - the Credits: a dark pane over a starscape rolling left, with ships streaking
//    across in hyperspace.
#pragma once
#include <cstdint>

class Canvas;

namespace backdrops {
void drawRetroNewGame(Canvas& c, uint32_t now);
// The Voidfall New Game backdrop (0.33.1): the painted picture from the Voidfall pack, the void's tendrils
// swaying over it, embers drifting and now and then a ship crossing with its engines lit. false (nothing
// drawn) without the pack.
bool drawVoidfallNewGame(Canvas& c, uint32_t now);
void drawCredits(Canvas& c, uint32_t now);
// The pixel cruiser (facing right), its bow at (x, y) in screen units, scale = pixel size.
void drawPixelCruiser(Canvas& c, float x, float y, int scale, uint32_t now, int alpha = 255);
}  // namespace backdrops
