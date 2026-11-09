// The game's icons (owner-supplied artwork packed into icons.dat by
// tools/gen_icons.py). Every draw call falls back to a plain shape when the
// icon file is missing, so screens never break without it.
//
// Names: "faction/<key>" (the 24 PoK factions + "keleres"), and
//   action_card agenda biotic biotic_dark codex commodities commodity cultural
//   cybernetic cybernetic_dark frontier hazardous industrial influence
//   influence_bg legendary legendary_bg legendary_black legendary_complete
//   legendary_white pok promissory propulsion propulsion_dark public1 public2
//   relic resources resources_bg secret secret_public trade_good trade_goods
//   warfare warfare_dark; "unit/<type>" (flagship warsun dreadnought carrier
//   cruiser destroyer fighter mech infantry pds spacedock: the owner's
//   unit reference card, see tools/gen_unit_art.py)
#pragma once
#include <string>

#include "engine/data.h"
#include "gfx/canvas.h"
#include "gfx/font.h"

struct Image;

namespace icons {
// True when the pack loaded (looked for as icons.dat next to the game, then
// assets/icons.dat for runs from the source tree).
bool available();
int count();
// Draws the named icon centred in the size x size box at (x, y); mono icons
// are painted `tint`. Returns false (drawing nothing) when it is missing.
bool draw(Canvas& c, const std::string& name, int x, int y, int size, int alpha = 255, uint32_t tint = 0xE2E6F0);
// Paints only the icon's shape in colour `col` (greyed-out or coloured looks).
bool drawShape(Canvas& c, const std::string& name, int x, int y, int size, uint32_t col, int alpha = 255);
// A unit picture ("unit/<carrier|cruiser|...>", from the owner's unit
// reference card; ships face right) in colour col, centred on (cx, cy) and
// as large as fits maxW x maxH; mirror = facing left. False when missing.
bool drawUnitArt(Canvas& c, const std::string& name, float cx, float cy, int maxW, int maxH, uint32_t col, bool mirror = false,
                 int alpha = 255);
// The master picture (premultiplied ARGB, 128 px on its longer side) or nullptr (0.49.0: the pixel-art ships).
const Image* image(const std::string& name);
// The same, standing with its bottom edge on footY (ground units, 0.28.0).
bool drawUnitArtOn(Canvas& c, const std::string& name, float cx, float footY, int maxW, int maxH, uint32_t col, bool mirror = false,
                   int alpha = 255);
// Voidfall (0.51.1): the same, painted (re-lit like the tiles; see icons.cpp). out: its size drawn, in units.
bool drawUnitArtOnPainted(Canvas& c, const std::string& name, float cx, float footY, int maxW, int maxH, uint32_t col, bool mirror,
                          int alpha, float* outW = nullptr, float* outH = nullptr);
bool drawUnitArtPainted(Canvas& c, const std::string& name, float cx, float cy, int maxW, int maxH, uint32_t col, bool mirror = false,
                        int alpha = 255);  // centred
}  // namespace icons

// Faction emblem in a size x size box; a disc with the faction's initial
// when the icon is missing. faction = index into kFactions (-1 draws nothing).
void drawFactionIcon(Canvas& c, int faction, int x, int y, int size, int alpha = 255);
// Planet trait / tech specialty icons in a size x size box (fallback: a dot).
void drawTraitIcon(Canvas& c, ti::Trait t, int x, int y, int size, int alpha = 255);
void drawSkipIcon(Canvas& c, ti::TechColor t, int x, int y, int size, int alpha = 255);
// Resource / influence value: the icon with the number on it (fallback: the
// old circle badge). cx, cy = centre; size = icon height.
void drawResourceIcon(Canvas& c, int cx, int cy, int size, int value, const FontData& f, bool influence);
