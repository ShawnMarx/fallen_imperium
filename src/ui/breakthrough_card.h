// Breakthrough cards (0.44.3, owner: "I can't see where my breakthrough is. Can you create a breakthrough card in this
// format and place it in the Technology menu ... and under secrets and tech in the faction sheet"). Drawn by the game
// in the layout of the Thunder's Edge breakthrough cards: a dark title band with the faction's emblem, the name and the
// faction, a light text panel, the frame shading from one synergy colour to the other, the two colours with the
// exchange arrows in a band at the foot, and the exhaust mark when the ability exhausts the card. Our own wording.
#pragma once
#include <cstdint>
#include <string>

#include "engine/state.h"
#include "gfx/canvas.h"

namespace btcard {

enum State : uint8_t { NotGained, Ready, Exhausted };

// The card of `faction`'s breakthrough in r (any size: small cards show less text). False: the faction has none.
bool draw(Canvas& c, int faction, Rect r, State st, bool sel = false, uint32_t now = 0);
// Player p's breakthrough state.
State stateOf(const ti::Game& g, int p);
// Whether p's faction has a breakthrough in this game (Thunder's Edge breakthroughs on).
bool has(const ti::Game& g, int p);
// What it means for the player now: gained / how it is gained, the synergy, exhausted (for the large view).
std::string statusLine(const ti::Game& g, int p, int viewer);
std::string synergyLine(int faction);
// The large view: the card at full height on the left, what it means now on the right (a panel filling r).
void drawLarge(Canvas& c, const ti::Game& g, int p, int viewer, Rect r, uint32_t now);

}  // namespace btcard
