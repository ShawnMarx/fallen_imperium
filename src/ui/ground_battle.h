// The ground combat, played out on the planet's surface (0.23.0, owner's
// Polish list + his ground-combat picture): an almost top-down field whose
// terrain follows the planet's trait (cultural: city ruins; industrial:
// refineries and irradiated plants; hazardous: a sandstorm desert, a
// volcanic waste with lava geysers or a frozen tundra in a blizzard, one at
// random per fight). The invaders come in from the top, the defenders from
// the bottom, in muted versions of their colours. Each infantry is a squad
// of six soldiers (the owner's middle ground) whose men fall one by one when
// it is destroyed; mechs are big walkers with gatling lasers; PDS and space
// docks tower in the corners. Purely for show - the dice decide everything.
// 0.44.0 (the owner's ground combat overhaul): only the invaders make
// planetfall (they drop in; the defenders are on the field from the start),
// and 21 factions fight with their own units, weapons and effects
// (ui/ground_units.*; Options > Ground combat: Default = the old look).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/state.h"
#include "gfx/canvas.h"
#include "ui/dice.h"

namespace battle {

class GroundBattle {
public:
    GroundBattle();
    ~GroundBattle();
    void start(const ti::Game& g, int sys, int planet, int attacker, int defender, const ti::Force& a, const ti::Force& b, uint32_t now,
               float speed);
    void clear();
    bool active() const { return active_; }
    int planet() const { return planet_; }
    bool involves(int p) const { return active_ && (p == player_[0] || p == player_[1]); }
    void roll(int player, const std::vector<int8_t>& dice, const std::vector<int8_t>& need, int hits, uint32_t now, uint32_t shown);
    void damaged(int player, ti::UnitType t, int count, uint32_t now);
    void destroyed(int player, ti::UnitType t, int count, uint32_t now);
    void end(int winner, const std::string& text, uint32_t now);
    void draw(Canvas& c, const ti::Game& g, Rect area, uint32_t now, const std::string& confirm);

    struct Impl;

private:
    bool active_ = false;
    int planet_ = -1;
    int player_[2] = {-1, -1};
    std::unique_ptr<Impl> m_;
};

}  // namespace battle
