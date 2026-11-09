// The factions' codec portraits (0.57.0): 64 x 64 pixel-art busts, 24-frame loops (breathing, swaying, glowing),
// drawn by tools/portraits (owner-approved, two at a time from his pictures) and baked into one atlas,
// art/portraits-pc.png (a row per faction; tools/portraits/bake_atlas.py). Drawn enlarged with whole-pixel
// steps and CRT scanlines, like the Metal Gear codec the owner asked for.
#pragma once
#include <cstdint>

#include "gfx/canvas.h"

namespace portraits {
// True when faction f has a portrait (and the atlas loaded).
bool has(int faction);
// Faction f's portrait filling the square (x, y, size) in game units, the frame chosen by `now` (ms); `phase`
// shifts the loop (two windows don't breathe together). Draws nothing when missing (returns false).
bool draw(Canvas& c, int faction, int x, int y, int size, uint32_t now, int alpha = 255, int phase = 0);
// The portrait's backdrop colour (its painted background's base), for frames and fills around it.
uint32_t backdrop(int faction);
}  // namespace portraits
