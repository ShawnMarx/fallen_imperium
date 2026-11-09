// The space combat, played out on the system's tile (0.22.0, owner's Polish
// list: "Advance Wars 2 style"). Purely for show - the dice decide everything:
// the two fleets fly in from the left and right, every kind of ship moves and
// shoots in its own way, hits explode on the hull and keep smoking, destroyed
// ships break apart into debris that stays until the end, war suns hang in
// the background like moons. The backdrop is the tile itself: a planet's
// horizon, the planets of a multi-planet system, a red giant, a gravity
// rift, the Wormhole Nexus or open space.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/state.h"
#include "gfx/canvas.h"
#include "ui/dice.h"

namespace battle {

class SpaceBattle {
public:
    SpaceBattle();
    ~SpaceBattle();
    // A new fight in system `sys`. speed: the game's speed factor (1 normal, 1.6 slow, 0.45 fast).
    void start(const ti::Game& g, int sys, int attacker, int defender, const ti::Force& a, const ti::Force& b, uint32_t now, float speed);
    void clear();
    bool active() const { return active_; }
    int system() const { return sys_; }
    bool involves(int p) const { return active_ && (p == player_[0] || p == player_[1]); }
    // One side rolls (a combat round, an anti-fighter barrage...); shown = ms the roll is on screen.
    void roll(int player, const std::vector<int8_t>& dice, const std::vector<int8_t>& need, int hits, uint32_t now, uint32_t shown);
    void damaged(int player, ti::UnitType t, int count, uint32_t now);
    void destroyed(int player, ti::UnitType t, int count, uint32_t now);
    void retreat(int player, uint32_t now);
    void end(int winner, const std::string& text, uint32_t now);
    // Moves the fight on to `now` and draws it inside `area` (the hex fills it).
    // confirm: a "continue" line at the bottom (the fight is over, waiting for A).
    void draw(Canvas& c, const ti::Game& g, Rect area, uint32_t now, const std::string& confirm);

    struct Impl;

private:
    bool active_ = false;
    int sys_ = -1;
    int player_[2] = {-1, -1};
    std::unique_ptr<Impl> m_;
};

}  // namespace battle
