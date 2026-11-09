// Thunder's Edge placeholders (0.24.0, owner): The Fracture (its seven
// systems on three tiles) and the Thunder's Edge tile, expedition side up,
// beside the board. They are only drawn - not part of the game (no rules,
// no adjacency) - but the cursor can stand on them and A opens them large,
// like any tile. Animated: red nebula, pink lightning, the egress vortexes,
// the storm on Thunder's Edge.
#pragma once
#include <string>
#include <vector>

#include "engine/state.h"
#include "galaxy/hex.h"
#include "gfx/canvas.h"

namespace decor {

enum Kind { Cocytus, Egress, Rift, Styx, LethePhlegethon, ThundersEdge };

struct Tile {
    Kind kind = Rift;
    Hex pos{};
    int index = 0;  // 0-6 the Fracture's systems, 7 Thunder's Edge
};

// Where they go on this map: free spaces just outside the board, clear of the
// homes, the info tiles and the Wormhole Nexus. `taken`: hexes already used.
std::vector<Tile> place(const ti::Game& g, const std::vector<Hex>& taken);

std::string name(const Tile& t);
// 0.44.3 (owner: "both still say placeholder"): what the tile is in this game - the Fracture inactive / not in play,
// Thunder's Edge's expedition so far, or the module not being in the game.
std::string subtitle(const ti::Game& g, const Tile& t);
// A few lines on how it comes into play (the side panel).
std::string about(const ti::Game& g, const Tile& t);

// The tile painted in a hex of radius r centred on (cx, cy). large: the
// expanded view (names, values, slices of the expedition).
void paint(Canvas& c, const Tile& t, float cx, float cy, float r, uint32_t now, bool large);

// 0.43.0 (owner, P14): the six expedition "winglets" round Thunder's Edge in the large view - one tab on each edge
// with what joining that part of the expedition costs (slices 0 resources, 1 influence, 2 action cards, 3 secret,
// 4 specialty planet, 5 trade goods); claimCol[k] = the colour of the player who took slice k (0 = still open).
// selected: the winglet highlighted (the expedition menu); claimFaction: the joiners' factions (their control tokens).
// Returns the winglets' centres in wx / wy when given (mouse hits).
void drawWinglets(Canvas& c, float cx, float cy, float r, const uint32_t claimCol[6], uint32_t now, int selected = -1,
                  const int* claimFaction = nullptr, const bool* open = nullptr, float* wxOut = nullptr, float* wyOut = nullptr);

// A Fracture system in play (fracNo 1..7 = its tile fracture1..7): under = the ground before the planets
// (Simplistic), else a wash over the art tile; plus its lightning / egress vortex and the red rim.
void fractureLook(Canvas& c, int fracNo, float cx, float cy, float r, uint32_t now, bool under);

// 0.43.0 (owner: "ingress tokens do not show up on the map"): a pulsing white-red arrow pointing down
// into a turning white-red vortex, centred on (x, y) with vortex radius r
void drawIngress(Canvas& c, float x, float y, float r, uint32_t now);

}  // namespace decor
