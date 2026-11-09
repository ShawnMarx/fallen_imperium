// Card designs drawn by the game itself (owner, 2026-10-21: draw them rather
// than use the scanned templates): objective cards and backs, technology
// cards, strategy cards, control tokens, exploration card backs and the
// faction labels on the turn banners. In the style of the printed game -
// dark faces, glowing frames in the card's colour - built from lines and
// gradients, so they stay sharp at any size. Each design is drawn once per
// size and kept, so screens can show many of them cheaply. Faction symbols and
// tech colours come from the owner's icon pack (plain shapes without it).
#pragma once
#include <cstdint>

#include "engine/data.h"
#include "gfx/canvas.h"

namespace cards {
// A public or secret objective, face up, filling r (2:3 works best). Small
// cards show the name only; bigger ones add our short description.
void objective(Canvas& c, int objective, Rect r, int alpha = 255);
// Face down: kind 1 = stage I, 2 = stage II, 3 = secret.
void objectiveBack(Canvas& c, int kind, Rect r, int alpha = 255);

// A short tile (technology grid): the card's frame and colour symbol at the
// right; the caller writes the name.
void techTile(Canvas& c, int tech, Rect r, int alpha = 255);
// A full technology card with its name and text (about 3:2 landscape).
void tech(Canvas& c, int tech, Rect r, int alpha = 255);
// Wireframes, fitted into r (centred, keeping their shape) as thin lines in
// col: a unit's (traced from the board game's unit cards, owner's
// permission) and a technology's (unit upgrades show their unit, the others
// have drawings of their own). The cards above already include them.
void unitWireframe(Canvas& c, ti::UnitType type, Rect r, uint32_t col, int alpha = 255);
void techWireframe(Canvas& c, int tech, Rect r, uint32_t col, int alpha = 255);

// A strategy card for card 0..7 (initiative 1..8) at width w, with its number
// and name; wide cards (380+) also carry both abilities.
int strategyHeight(int w);
void strategy(Canvas& c, int card, int x, int y, int w, int alpha = 255);

// A control token: a starry disc, a glow in the player's colour and the
// faction's symbol.
void controlToken(Canvas& c, int faction, uint32_t colour, int cx, int cy, int diameter, int alpha = 255);

// A command token in the board game's shape - a triangle with clipped
// corners - w pixels wide (about 0.8 w tall), centred on (cx, cy): a starry
// face, a glow and rim in the player's colour and the faction's symbol, or
// ship side up (as tokens sit in the fleet pool). Very small tokens are a plain
// coloured shape.
int commandTokenHeight(int w);
void commandToken(Canvas& c, int faction, uint32_t colour, int cx, int cy, int w, int alpha = 255, bool shipSide = false);

// An exploration card back: deck 0 cultural, 1 hazardous, 2 industrial, 3 frontier.
void exploreBack(Canvas& c, int deck, Rect r, int alpha = 255);
// The back of an action card (0.33.1: the owner's picture, art/action_back-*.png; drawn in its place if missing)
void actionBack(Canvas& c, Rect r, int alpha = 255);

// A faction's label (turn banners): a slanted space banner with the faction's
// symbol on a panel in the player's colour and its full name.
void factionLabel(Canvas& c, int faction, uint32_t colour, int x, int y, int w, int h, int alpha = 255);
}  // namespace cards
