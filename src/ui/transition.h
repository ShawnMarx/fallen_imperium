// The game-start transition (0.28.0, owner): Start game -> a flood of
// electric-blue 1s and 0s from the left and right edges (2.5 s) -> the game
// screen comes through (1.7 s) -> a gold "GAME START!" banner with its sound.
// 0.28.1 (owner): no fade to black (the flood is the fade) and the music plays on.
#pragma once
#include <cstdint>

class Canvas;

namespace transition {
constexpr uint32_t kFade = 0, kFlood = 2500;     // New Game screen: the flood (no fade since 0.28.1)
constexpr uint32_t kReveal = 1700, kBanner = 1500;  // game screen: the flood draws back, then the banner
// New Game screen, t = ms since Start (0 .. kFade + kFlood).
void drawCover(Canvas& c, uint32_t t);
// Game screen, t = ms since it opened (0 .. kReveal + kBanner).
void drawReveal(Canvas& c, uint32_t t);
// Set by New Game just before it opens the game screen; the game screen takes it.
extern bool pending;
}  // namespace transition
