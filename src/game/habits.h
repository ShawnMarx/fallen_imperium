// The people at this device, as the bots remember them (0.54.0, owner: "adapting to the habits of all players ... the
// profile being fixed at the start of each game"). Kept in the settings ("habits"); a new game gets a copy in its
// Setup (saved with it), and the bots' personalities react to it (ai_style.cpp).
#pragma once
#include "engine/engine.h"

class Settings;

namespace habits {

// the profile so far (games = 0 when none, or when the player switched it off)
ti::HabitProfile load(const Settings& s);
bool enabled(const Settings& s);
void setEnabled(Settings& s, bool on);
// what the human seats did in this game, added to the profile (a game is counted once: counted again - after more
// rounds - it replaces its earlier count)
void record(Settings& s, const ti::Game& g, const ti::Setup& setup);

}  // namespace habits
