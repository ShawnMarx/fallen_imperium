// The people at this device, as the bots remember them (0.54.0) - see habits.h.
#include "game/habits.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

#include "core/settings.h"

namespace habits {

namespace {
ti::HabitProfile parse(const std::string& text) {
    ti::HabitProfile h;
    std::istringstream in(text);
    in >> h.games >> h.attacks >> h.raids >> h.mecatol >> h.broken >> h.kept >> h.deals;
    if (!in || h.games < 0) return ti::HabitProfile();
    return h;
}
std::string print(const ti::HabitProfile& h) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "%d %.3f %.3f %.3f %.3f %.3f %.3f", h.games, double(h.attacks), double(h.raids), double(h.mecatol),
                  double(h.broken), double(h.kept), double(h.deals));
    return buf;
}
}  // namespace

bool enabled(const Settings& s) { return s.getInt("habits_on", 1) != 0; }
void setEnabled(Settings& s, bool on) { s.setInt("habits_on", on ? 1 : 0); }

ti::HabitProfile load(const Settings& s) {
    if (!enabled(s)) return ti::HabitProfile();
    return parse(s.get("habits", ""));
}

void record(Settings& s, const ti::Game& g, const ti::Setup& setup) {
    if (!enabled(s) || setup.autoplay || g.round < 3) return;  // (only games really played, a few rounds at least)
    std::vector<int> people;
    for (int q = 0; q < g.numPlayers; ++q)
        if (setup.isHuman(q)) people.push_back(q);
    if (people.empty()) return;
    // this game's habits, per person
    ti::HabitProfile obs;
    const float n = float(people.size()), rounds = float(std::max(1, g.round));
    for (int q : people) {
        float attacks = 0, kept = 0, deals = 0;
        for (int b = 0; b < g.numPlayers; ++b) {
            if (setup.isHuman(b)) continue;
            const ti::Personality& m = g.players[size_t(b)].pers;
            attacks += float(m.attacks[size_t(q)]);
            kept += float(m.kept[size_t(q)]);
            deals += float(m.deals[size_t(q)]);
        }
        obs.attacks += attacks / rounds / n;
        obs.kept += kept / n;
        obs.deals += deals / n;
        obs.raids += float(g.players[size_t(q)].raids) / n;
        obs.mecatol += float(g.players[size_t(q)].mecHeld) / n;
        obs.broken += float(g.players[size_t(q)].promisesBroken) / n;
    }
    // the same game counted again: start from the profile before it
    const std::string key = std::to_string(setup.seed);
    ti::HabitProfile prev = parse(s.get(s.get("habits_seed", "") == key ? "habits_prev" : "habits", ""));
    s.set("habits_prev", print(prev));
    s.set("habits_seed", key);
    // the average of the first five games, then each new game weighs a fifth
    const float w = 1.f / float(std::min(prev.games + 1, 5));
    ti::HabitProfile h;
    h.games = prev.games + 1;
    auto mixIn = [&](float a, float b) { return a * (1.f - w) + b * w; };
    h.attacks = mixIn(prev.attacks, obs.attacks);
    h.raids = mixIn(prev.raids, obs.raids);
    h.mecatol = mixIn(prev.mecatol, obs.mecatol);
    h.broken = mixIn(prev.broken, obs.broken);
    h.kept = mixIn(prev.kept, obs.kept);
    h.deals = mixIn(prev.deals, obs.deals);
    s.set("habits", print(h));
    s.save();
}

}  // namespace habits
