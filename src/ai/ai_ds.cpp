// Computer players and the Discordant Stars factions' questions (0.37.0): Ab::Ds* (rules_ds.cpp). Called from
// aiFactionAnswer and aiFactionComponentValue. Rules of thumb like the rest of the AI; the engine checks every answer.
// Anything not handled here falls back to aiFactionAnswer's default (yes / the first option).
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "ai/ai.h"
#include "ai/ai_internal.h"
#include "engine/personality.h"
#include "engine/deal.h"
#include "engine/query.h"

namespace ti {

namespace {
Answer one(int id) {
    Answer a;
    a.picks.push_back(id);
    return a;
}
Answer yes(bool b) { return one(b ? 1 : 0); }
bool isYesNo(const Decision& d) { return d.options.size() == 2 && d.options[0].id == 1 && d.options[1].id == 0 && d.options[0].label == "Yes"; }
bool has(const Decision& d, int id) {
    for (const Option& o : d.options)
        if (o.enabled && o.id == id) return true;
    return false;
}
template <class F>
int bestBy(const Decision& d, F score, bool allowNegative = false) {
    int best = -1000000;
    float bv = -1e9f;
    for (const Option& o : d.options) {
        if (!o.enabled || (o.id < 0 && !allowNegative)) continue;
        const float v = score(o.id);
        if (v > bv) {
            bv = v;
            best = o.id;
        }
    }
    return best;
}
int maxOption(const Decision& d) {
    int m = -1;
    for (const Option& o : d.options)
        if (o.enabled) m = std::max(m, o.id);
    return m;
}
// how much an exploration card is worth on average (keep it, or let Vassa Hagi draw another)
float exploreWorth(int c) {
    if (c < 0 || c >= kExploreCardCount) return 1.f;
    if (!std::strcmp(kExploreCards[c].key, "dmz")) return -1.f;
    switch (kExploreCards[c].kind) {
        case ExploreKind::Fragment: return 2.f;
        case ExploreKind::Attach: return 1.6f;
        case ExploreKind::Chart: return 1.3f;
        case ExploreKind::Token: return 0.4f;
        default: return 1.f;
    }
}
// the action card we'd miss least
int cheapestCard(const Decision& d) {
    return bestBy(d, [](int c) { return -aiCardKeepValue(c); });
}
// enemy strength next to / in a system (non-fighter ships)
int threatNear(const Game& g, int p, int sys) {
    int n = 0;
    std::vector<int> zone{sys};
    for (int16_t a : g.systems[size_t(sys)].adj) zone.push_back(a);
    for (int z : zone)
        for (int q = 0; q < g.numPlayers; ++q)
            if (q != p) n += g.systems[size_t(z)].space[size_t(q)].nonFighterShips();
    return n;
}

// Sanctification Field (Rhodun, 0.44.5): what silencing one technology does in this tactical action - unit upgrades
// of the units here, the combat technologies, Gravity Drive of the active player (before they move) and the
// invasion ones; the research appeal no longer decides (it always picked Fleet Logistics or Sarween Tools)
float sanctifyValue(const Game& g, int z, int c) {
    if (c < 0) return 0.f;
    const int q = c / 1000, t = c % 1000;
    const int sys = g.activeSystem;
    if (q < 0 || q >= g.numPlayers || t < 0 || t >= kTechCount || sys < 0) return 0.f;
    const bool active = q == g.activePlayer;
    const TechDef& d = kTechs[t];
    if (d.unitUpgrade) {
        int type = -1;
        for (int u = 0; u < kUnitCount && type < 0; ++u)
            if (kUnits[u].tech == t) type = int(kUnits[u].type);
        if (type < 0) return 0.f;
        int n = g.systems[size_t(sys)].space[size_t(q)].count(UnitType(type));
        for (int16_t x : g.systems[size_t(sys)].planets) n += g.planets[size_t(x)].units[size_t(q)].count(UnitType(type));
        if (active)  // (ships that may still move in)
            for (int16_t a : g.systems[size_t(sys)].adj) n += g.systems[size_t(a)].space[size_t(q)].count(UnitType(type));
        return float(n) * (0.5f + 0.5f * float(unitCost2(g, q, UnitType(type))) / 2.f);
    }
    static const struct {
        const char* key;
        float active, other;  // the active player's / a defender's
    } kRelevant[] = {{"gd", 3.f, 0.f},   {"asc", 3.f, 3.f}, {"da", 2.5f, 2.5f}, {"ps", 2.f, 2.f},   {"amd", 1.5f, 0.5f},
                     {"gls", 0.5f, 1.5f}, {"x89_base", 2.f, 1.f}, {"l4", 2.f, 0.f}, {"md_base", 0.f, 2.f}, {"dxa", 1.5f, 1.f},
                     {"nes", 2.f, 2.f},  {"vw", 2.f, 2.f},  {"st", 1.f, 0.f},   {"aida", 0.5f, 0.f}, {"ie", 1.f, 0.f},
                     // warfare debug pass (0.44.8)
                     {"ds", 2.f, 2.f},   {"sc", 1.5f, 1.5f}, {"vpw", 1.f, 1.5f}, {"proxima", 1.f, 1.f}, {"mr", 1.f, 0.f}};
    // a Nekro's Valefar Assimilator: worth what the copied technology is worth (0.44.8, fixes 12 - before that a
    // blanked Valefar card kept its copy)
    if (g.fixes >= 12 && (!std::strcmp(d.key, "vax") || !std::strcmp(d.key, "vay"))) {
        const int copy = !std::strcmp(d.key, "vax") ? g.players[size_t(q)].assimX : g.players[size_t(q)].assimY;
        return copy >= 0 && copy != t ? sanctifyValue(g, z, q * 1000 + copy) : 0.f;
    }
    for (const auto& r : kRelevant)
        if (!std::strcmp(d.key, r.key)) return active ? r.active : r.other;
    (void)z;
    return 0.1f;
}

// Emergency Deployment (Axis, 0.44.5): how good a planet is for a new space dock - its resources, away from our
// other docks and from enemy fleets; never a DMZ (the engine leaves those out)
float dockSpot(const Game& g, int p, int x) {
    if (x < 0 || x >= int(g.planets.size())) return -1.f;
    const PlanetState& ps = g.planets[size_t(x)];
    const int sys = ps.system;
    float v = float(planetResources(g, x)) + 0.3f * float(planetInfluence(g, x));
    if (g.systems[size_t(sys)].home == p) v -= 3.f;  // (home has one)
    for (int16_t y : g.systems[size_t(sys)].planets)
        if (g.planets[size_t(y)].units[size_t(p)].count(UnitType::SpaceDock)) v -= 2.f;  // (one in this system already)
    v -= 1.2f * float(threatNear(g, p, sys));
    return v;
}
// the best spot, or -1.f
float bestDockSpot(const Game& g, int p) {
    float best = -1.f;
    for (int x : controlledPlanets(g, p))
        if (!g.planets[size_t(x)].units[size_t(p)].count(UnitType::SpaceDock) && !isDMZ(g, x)) best = std::max(best, dockSpot(g, p, x));
    return best;
}

// Mercenary Captains (Nokar, 0.44.4): how good a system is for the neutral fleet - next to the points leader's home or
// planets, near enemy fleets; never our own systems or next to our home
float nokarSpot(const Game& g, int p, int s) {
    const SystemState& sy = g.systems[size_t(s)];
    const int home = g.players[size_t(p)].homeSystem;
    if (s == home) return -10.f;
    for (int16_t x : sy.planets)
        if (g.planets[size_t(x)].owner == p) return -10.f;
    if (home >= 0)
        for (int16_t a : sy.adj)
            if (a == home) return -10.f;
    int leader = -1;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p && !g.players[size_t(q)].eliminated && (leader < 0 || g.players[size_t(q)].vp > g.players[size_t(leader)].vp)) leader = q;
    float v = 0.f;
    if (leader >= 0) {
        const int lh = g.players[size_t(leader)].homeSystem;
        for (int16_t a : sy.adj)
            if (a == lh) v += 3.f;
        for (int16_t x : sy.planets)
            if (g.planets[size_t(x)].owner == leader) v += 2.f;
    }
    return v + 0.3f * float(threatNear(g, p, s));
}
// Spark Thrusters (Lanefir, 0.43.0 debug passes): a lone ship with nothing aboard slips over to join a fleet of ours
// next door (option id = (from * 100 + to) * 16 + type); -1 = not worth it
float sparkScore(const Game& g, int p, int id) {
    if (id < 0) return -1.f;
    const int from = (id / 16) / 100, to = (id / 16) % 100;
    const UnitType t = UnitType(id % 16);
    if (from < 0 || to < 0 || size_t(from) >= g.systems.size() || size_t(to) >= g.systems.size()) return -1.f;
    if (t == UnitType::Fighter || from == g.players[size_t(p)].homeSystem) return -1.f;
    const Force& src = g.systems[size_t(from)].space[size_t(p)];
    if (src.nonFighterShips() != 1 || transportedInSpace(g, p, from) > 0) return -1.f;  // (nothing left behind)
    const int join = g.systems[size_t(to)].space[size_t(p)].nonFighterShips();
    if (join == 0 || fleetUseIn(g, p, to) + 1 > g.players[size_t(p)].fleet) return -1.f;
    return 0.5f + 0.1f * float(std::min(join, 5)) + 0.2f * float(std::min(threatNear(g, p, from), 3));
}
float sparkBest(const Game& g, int p) {
    float best = -1.f;
    for (size_t s = 0; s < g.systems.size(); ++s) {
        const Force& f = g.systems[s].space[size_t(p)];
        if (f.nonFighterShips() != 1) continue;
        for (int a : routeNeighbors(g, p, int(s))) {
            if (g.systems[size_t(a)].isHyperlane() || otherShipsIn(g, p, a)) continue;
            for (int t : kShipIdx)
                if (f.n[t]) best = std::max(best, sparkScore(g, p, (int(s) * 100 + a) * 16 + t));
        }
    }
    return best;
}
// the relic we'd rather keep (Data Leak)
float relicPref(int r) {
    if (r < 0 || r >= kRelicCount) return 0.f;
    static const struct {
        const char* key;
        float v;
    } kPref[] = {{"shard", 9}, {"emphidia", 6}, {"obsidian", 5}, {"emelpar", 5}, {"thalnos", 5}, {"dominusorb", 4}, {"mawofworlds", 4},
                 {"prophetstears", 4}, {"stellarconverter", 4}, {"codex", 4}, {"quantumcore", 5}, {"heartofixth", 4}, {"thetriad", 4}};
    for (const auto& k : kPref)
        if (!std::strcmp(kRelics[r].key, k.key)) return k.v;
    return 3.f;
}
float planetWorth(const Game& g, int x) { return x < 0 ? -1.f : float(planetResources(g, x) + planetInfluence(g, x)); }
// how good an objective is for us now: scored at once beats almost there
float objectiveFit(const Game& g, int p, int o) {
    if (o < 0) return -1.f;
    if (meetsObjective(g, p, o)) return 10.f + float(kObjectives[o].points);
    const ObjProgress pr = objectiveProgress(g, p, o);
    return pr.need > 0 ? 5.f * float(pr.have) / float(pr.need) : 0.f;
}
// the side we fight in the active combat (space)
float spaceOdds(const Game& g, int p) {
    const int sys = g.activeSystem;
    if (sys < 0) return 0.5f;
    for (int q = 0; q < kSides; ++q) {
        if (q == p || (q >= g.numPlayers && q != kNeutral)) continue;
        if (!g.systems[size_t(sys)].space[size_t(q)].ships()) continue;
        const Force& mine = g.systems[size_t(sys)].space[size_t(p)];
        const Force& theirs = g.systems[size_t(sys)].space[size_t(q)];
        return p == g.activePlayer ? fightOdds(g, p, mine, q, theirs, true) : 1.f - fightOdds(g, q, theirs, p, mine, true);
    }
    return 0.5f;
}
}  // namespace

bool aiDsAnswer(const Game& g, const Decision& d, Rng& rng, Answer& out) {
    const int p = d.player;
    if (p < 0 || p >= g.numPlayers) return false;
    const Player& me = g.players[size_t(p)];
    const Ab ab = Ab(d.ability);
    if (int(ab) < int(Ab::DsZealous)) return false;
    const bool yn = isYesNo(d);
    switch (ab) {
        case Ab::DsPyndil: {  // (0.43.4) the type with the most capacity that can reach the active system
            const int act = g.activeSystem;
            out = one(bestBy(d, [&](int t) {
                float v = float(unitDef(g, p, UnitType(t)).capacity);
                for (size_t s = 0; s < g.systems.size(); ++s)
                    if (g.systems[s].space[size_t(p)].n[size_t(t)] > 0 && act >= 0)
                        v += hexDistance(g.systems[s].pos, g.systems[size_t(act)].pos) <= 2 ? 3.f : 0.f;
                return v;
            }));
            return true;
        }
        case Ab::DsClansFavor:  // only to save a fleet that is losing
            if (yn) {
                out = yes(spaceOdds(g, p) < 0.3f && g.systems[size_t(std::max(0, int(g.activeSystem)))].space[size_t(p)].nonFighterShips() >= 2);
                return true;
            }
            return false;
        case Ab::DsAts: {  // rerolls: a couple per round, all of them in a close fight
            const int m = maxOption(d);
            out = one(std::min(m, d.value ? (spaceOdds(g, p) < 0.7f ? m : 2) : 1));
            return true;
        }
        case Ab::DsPhquaiset: out = one(maxOption(d)); return true;
        case Ab::DsNadalia:
            if (yn) {
                out = yes(d.value >= 3 || g.round >= 6 || me.vp >= g.vpGoal - 3);
                return true;
            }
            return false;
        case Ab::DsPustule:
        case Ab::DsScour: {
            if (ab == Ab::DsScour && d.value < (fquirkIs(g, p, "vaylerian", 1) ? 2 : 3)) {  // a planet worth a card (Card Sharp: cheaper)
                out = one(-1);
                return true;
            }
            const int c = cheapestCard(d);
            out = one(c >= 0 && aiCardKeepValue(c) < 2.5f ? c : -1);
            return true;
        }
        case Ab::DsChitin: out = one(has(d, int(UnitType::Destroyer)) ? int(UnitType::Destroyer) : has(d, int(UnitType::Fighter)) ? int(UnitType::Fighter) : -1); return true;
        case Ab::DsDyln:
            if (yn) {
                out = yes(d.value == 1 && (g.round >= 3 || me.vp >= g.vpGoal - 4));
                return true;
            }
            return false;
        case Ab::DsBroodPod:
            if (yn) {
                out = yes(spendable(g, p, false) >= 3 && threatNear(g, p, std::max(0, int(g.activeSystem))) > 0);
                return true;
            }
            out = Answer();  // keep the structures
            return true;
        case Ab::DsTitus:
            if (d.value != p || g.round < 3 || spendable(g, p, false) < 4) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int x) { return float(planetResources(g, x) + planetInfluence(g, x)); }));
            return true;
        case Ab::DsMinuteman:
            if (d.value == p && threatNear(g, p, std::max(0, int(g.activeSystem))) == 0) {  // our own move with nobody about
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int x) { return float(planetResources(g, x) + planetInfluence(g, x)); }));
            return true;
        case Ab::DsNobin:
            out = one(bestBy(d, [&](int x) { return float(planetResources(g, x) + planetInfluence(g, x)); }));
            return true;
        case Ab::DsEmergencyMob: {
            // (0.44.8) a dock system to activate again - home or one with enemies near - while we still have turns, a
            // tactic token and money to build (it used to wait for home with 2 strategy and at most 1 tactic: never)
            const int home = me.homeSystem;
            if (me.passed || me.tactic < 1 || aiFreeSpend(g, p, false) < 3 || me.strategy < 1) {
                out = one(-1);
                return true;
            }
            const int s = bestBy(d, [&](int id) {
                if (id < 0 || id >= int(g.systems.size())) return -1.f;
                return float(threatNear(g, p, id)) + (id == home ? 1.f : 0.f);
            }, 0.5f);
            out = one(s >= 0 && (me.strategy >= 2 || threatNear(g, p, s) > 0) ? s : -1);
            return true;
        }
        case Ab::DsUdosh:  // a resource planet pays for influence: worth it when it carries 2+ resources
            if (isYesNo(d)) {
                int best = 0;
                for (int x : readyPlanets(g, p))
                    if (g.planets[size_t(x)].system != g.players[size_t(p)].homeSystem) best = std::max(best, planetResources(g, x));
                out = yes(best >= 2);
            } else {
                out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : float(planetResources(g, x) * 2 + planetInfluence(g, x)) - 3.f; }, true));
            }
            return true;
        case Ab::DsKkavras: {  // (0.43.0 debug passes: asked ~600 times, always no) our own token off a system whose
            // fleet or dock we'd use again this round, onto one that doesn't matter
            if (g.activePlayer != p || g.players[size_t(p)].tactic < 1) {  // (only for our own token, at our turn's end)
                out = one(-1);
                return true;
            }
            auto worth = [&](int s) {
                bool dock = false;
                for (int16_t x : g.systems[size_t(s)].planets) dock = dock || g.planets[size_t(x)].units[size_t(p)].count(UnitType::SpaceDock) > 0;
                return float(g.systems[size_t(s)].space[size_t(p)].nonFighterShips()) + (dock ? 2.f : 0.f);
            };
            auto score = [&](int id) { return id < 0 ? -1.f : worth(id / 1000) - worth(id % 1000) - 2.f; };
            const int b = bestBy(d, score);
            out = one(b >= 0 && score(b) > 0.f ? b : -1);
            return true;
        }
        case Ab::DsTroubadour:
            if (d.prompt == "Troubadour: end your turn now?") {  // 0.43.0: the bots act (waiting isn't in their plans)
                out = yes(false);
                return true;
            }
            if (fragmentCount(g, p) < (fquirkIs(g, p, "lanefir", 1) ? 1 : 2) && unitsOnBoard(g, p, UnitType::Mech) >= 2) {  // (Iconoclast Scholar)
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int x) {
                const int s = g.planets[size_t(x)].system;
                return float(threatNear(g, p, s)) + 0.1f * float(planetResources(g, x) + planetInfluence(g, x));
            }));
            return true;
        case Ab::DsVassa:
            if (yn) {
                out = yes(exploreWorth(d.value) <= 0.5f);
                return true;
            }
            return false;
        case Ab::DsEdifice:  // explore all of them
            out = Answer();
            for (const Option& o : d.options)
                if (o.enabled && int(out.picks.size()) < d.maxPick) out.picks.push_back(o.id);
            return true;
        case Ab::DsContagion: {
            // 0.43.0 (D14): the personality decides how bold the plague is - the aggressive ones go for a planet held by a
            // single defender (Subversive: a draw is theirs), the rest only for empty ones
            const int a = archNow(g, p);
            const bool bold = a == int(Arch::Warlord) || a == int(Arch::Betrayer) || a == int(Arch::Rusher) || a == int(Arch::Chaotic) ||
                              a == int(Arch::Opportunist);
            const int allowed = bold && g.fixes >= 2 ? 1 : 0;
            out = one(bestBy(d, [&](int x) {
                const PlanetState& ps = g.planets[size_t(x)];
                int foes = neutralGroundOn(g, x);
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) foes += groundForcesOn(g, q, x);
                if (foes > allowed) return -10.f;
                float v = float(planetResources(g, x) + planetInfluence(g, x)) + 1.f;
                if (ps.owner >= 0 && ps.owner != p) {
                    const float rel = float(me.pers.bf[size_t(ps.owner)]);
                    v += 1.f - 0.05f * rel;  // a rival's empty planet is a fine prize
                }
                if (!std::strcmp(kPlanets[ps.def].key, "mr")) v += 4.f;
                return foes ? v * 0.6f : v;
            }));
            if (out.picks[0] >= 0) {
                int foes = neutralGroundOn(g, out.picks[0]);
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) foes += groundForcesOn(g, q, out.picks[0]);
                if (foes > allowed) out = one(-1);
            } else {
                out = one(-1);
            }
            return true;
        }
        case Ab::DsKyroRider: {  // the same judgement as the Keleres Rider
            Decision d2 = d;
            d2.ability = int(Ab::KeleresRider);
            return aiFactionAnswer(g, d2, rng, out);
        }
        case Ab::DsSilas: {
            const int m = maxOption(d);
            out = one(g.round >= 5 || me.vp >= g.vpGoal - 3 || fquirkIs(g, p, "kyro", 1) ? m : 0);  // (Pale Rider: always)
            return true;
        }
        case Ab::DsVector:  // the card we would like most ourselves (our own trade goods stay with us - 0.43.0)
            if (d.prompt.find("your own") != std::string::npos) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int c) { return aiStrategyCardValue(g, p, c); }));
            return true;
        case Ab::DsVenerable:
            if (d.prompt.find("swap") != std::string::npos) {  // a swap only to hurt the leader
                int leader = -1;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p && (leader < 0 || g.players[size_t(q)].vp > g.players[size_t(leader)].vp)) leader = q;
                int pickId = -1;
                for (const Option& o : d.options)
                    if (o.enabled && o.id >= 0 && o.id / 1000 == leader && g.players[size_t(leader)].vp >= g.vpGoal - 4) pickId = o.id;
                out = one(pickId);
                return true;
            }
            return false;
        case Ab::DsSparkMove: out = one(bestBy(d, [&](int id) { return sparkScore(g, p, id); })); return true;
        case Ab::DsSpark: out = one(has(d, 1) && (g.players[size_t(p)].strategy >= 3 || !has(d, 0)) ? 1 : 0); return true;  // pay with?
        // ---- batch 2 (0.38.0)
        case Ab::DsMeteorSlings:
        case Ab::DsStoneSpeakers: {  // hits beyond the defenders become infantry
            const int pl = d.value;
            int defenders = 0;
            for (int q = 0; q < kSides; ++q)
                if (q != p && pl >= 0) defenders += g.planets[size_t(pl)].units[size_t(q)].groundForces();
            const int m = maxOption(d);
            int hits = m;
            for (const Option& o : d.options) hits = std::max(hits, o.id);
            out = one(std::max(0, std::min(m, hits - defenders)));
            return true;
        }
        case Ab::DsBypass:
            if (yn) {
                bool guns = false;
                const int sys = g.activeSystem;
                if (sys >= 0)
                    for (int16_t x : g.systems[size_t(sys)].planets)
                        for (int q = 0; q < g.numPlayers; ++q)
                            if (q != p && (g.planets[size_t(x)].units[size_t(q)].count(UnitType::PDS) || g.planets[size_t(x)].units[size_t(q)].count(UnitType::Mech)))
                                guns = true;
                out = yes(guns);
                return true;
            }
            return false;
        case Ab::DsCollider:
            out = one(bestBy(d, [&](int c) {
                const int t = (c / 8) % 16;
                return t == int(UnitType::Mech) ? 3.f : t == int(UnitType::PDS) ? 2.5f : t == int(UnitType::SpaceDock) ? 2.f : 1.f;
            }));
            return true;
        case Ab::DsObsessive:
            if (yn) {
                out = yes(spendable(g, p, false) >= 4);
                return true;
            }
            return false;
        case Ab::DsTurra:
            if (yn) {
                out = yes(d.value >= 1);
                return true;
            }
            return false;
        case Ab::DsDeliberate: out = one(has(d, 0) ? 0 : has(d, 2) ? 2 : has(d, 1) ? 1 : -1); return true;
        case Ab::DsStoneEmbrace:
            out = one(bestBy(d, [&](int x) { return -float(planetResources(g, x) + planetInfluence(g, x)); }));
            if (me.strategy >= 3) out = one(-1);  // (plenty of tokens: keep the planets)
            return true;
        case Ab::DsIndustrious:
            if (yn) {
                out = yes(g.round >= 2);
                return true;
            }
            return false;
        case Ab::DsCataclysm:
            if (yn) {
                out = yes(d.value >= 4 || g.round >= 6);
                return true;
            }
            return false;
        case Ab::DsPrimespace:
            if (yn) {
                out = yes(d.value >= 6 && spendable(g, p, false) >= 6);
                return true;
            }
            return false;
        case Ab::DsAutomatons: out = one(bestBy(d, [&](int x) { return float(planetResources(g, x)); })); return true;
        case Ab::DsContractual:
            // (0.44.9) ourselves only with money to spare (a ship we pay for); the others: whoever's votes and lead hurt most
            out = one(bestBy(d, [&](int q) {
                if (q == p) return spendable(g, p, false) >= 8 && g.players[size_t(p)].vp < g.vpGoal - 3 ? 1.f : -1.f;
                return float(availableVotes(g, q)) + (g.players[size_t(q)].vp >= me.vp ? 3.f : 0.f);
            }));
            return true;
        case Ab::DsVehlTikar: {  // wake our own spent planets, best first
            std::vector<std::pair<float, int>> c;
            for (const Option& o : d.options)
                if (o.enabled && g.planets[size_t(o.id)].owner == p && g.planets[size_t(o.id)].exhausted)
                    c.push_back({float(planetResources(g, o.id) + planetInfluence(g, o.id)), o.id});
            std::sort(c.rbegin(), c.rend());
            out = Answer();
            for (size_t i = 0; i < c.size() && int(i) < d.maxPick; ++i) out.picks.push_back(c[i].second);
            return true;
        }
        case Ab::DsSaintBinal: {
            std::vector<std::pair<float, int>> c;
            for (const Option& o : d.options)
                if (o.enabled) c.push_back({aiTechWant(g, p, o.id), o.id});
            std::sort(c.rbegin(), c.rend());
            out = Answer();
            for (size_t i = 0; i < c.size() && int(i) < d.maxPick; ++i) out.picks.push_back(c[i].second);
            return true;
        }
        case Ab::DsSaintKeep:
        case Ab::DsMeditation: out = one(bestBy(d, [&](int t) { return aiTechWant(g, p, t); })); return true;
        case Ab::DsFran: out = one(has(d, p) ? p : maxOption(d)); return true;
        case Ab::DsSanctification: out = one(bestBy(d, [&](int c) { return sanctifyValue(g, p, c); }, 0.5f)); return true;
        case Ab::DsOmniscience: {
            const int act = d.value;
            // (0.44.8) only when there will be rolls this action: other units in the active system, or ours bombarding /
            // firing space cannon there (it used to go on empty and home activations, and the card is exhausted)
            bool rolls = false;
            if (g.activeSystem >= 0) {
                const int s = g.activeSystem;
                for (int q = 0; q < kSides && !rolls; ++q)
                    if (q != p && q != act && hasUnits(g, q, s)) rolls = true;
                if (act != p && act >= 0) rolls = rolls || hasUnits(g, p, s);
                if (act == p)
                    for (int16_t x : g.systems[size_t(s)].planets) rolls = rolls || (g.planets[size_t(x)].owner >= 0 && g.planets[size_t(x)].owner != p);
            }
            if (!rolls) out = one(-1);
            else if (act == p) out = one(p * 2 + 1);
            else if (act >= 0 && g.activeSystem >= 0 && hasUnits(g, p, g.activeSystem)) out = one(act * 2);
            else out = one(-1);
            return true;
        }
        case Ab::DsRecycled: {
            const int sys = std::max(0, int(g.activeSystem));
            const Force& f = g.systems[size_t(sys)].space[size_t(p)];
            int pick = has(d, int(UnitType::Dreadnought)) && f.damaged(UnitType::Dreadnought) && me.tradeGoods < 3 ? int(UnitType::Dreadnought) : -1;
            // Autofabricator (their mech, 0.43.0 debug passes - never came up): a spare cruiser well away from any
            // fight comes back as a trade good and a mech
            if (pick < 0 && mechAbilities(g) && unitsAvailable(g, p, UnitType::Mech) > 0 && has(d, int(UnitType::Cruiser)) &&
                threatNear(g, p, sys) == 0 && f.nonFighterShips() >= 2 && g.round >= 2)
                pick = int(UnitType::Cruiser);
            out = one(pick);
            return true;
        }
        case Ab::DsAutofabricator: {  // on one of our planets (it holds it), else the space area
            int x = -2;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0) x = o.id;
            out = one(x);
            return true;
        }
        case Ab::DsKyvir:
            out = one(g.round >= 3 && spendable(g, p, false) >= 6 ? (has(d, int(UnitType::Destroyer)) ? int(UnitType::Destroyer) : has(d, int(UnitType::Cruiser)) ? int(UnitType::Cruiser) : -1) : -1);
            return true;
        case Ab::DsFillipo: {  // 0.43.0 (D51): sparingly - only when one more move brings a ship into a fight or a landing
            const int act = g.activeSystem;
            bool target = false;
            if (act >= 0) {
                target = otherShipsIn(g, p, act);
                for (int16_t x : g.systems[size_t(act)].planets) target = target || g.planets[size_t(x)].owner != p;
            }
            out = one(bestBy(d, [&](int c) {
                if (c < 0 || !target) return c < 0 ? 0.f : -1.f;
                const int s = c / 16;
                if (s == act || otherShipsIn(g, p, s)) return -1.f;  // (not a ship about to fight where it is)
                for (int t : kShipIdx)
                    if (t != int(UnitType::Fighter) && g.systems[size_t(s)].space[size_t(p)].n[t] &&
                        !shipReach(g, p, s, act, UnitType(t)).ok && shipReach(g, p, s, act, UnitType(t), 1).ok)
                        return 1.f + 0.1f * float(unitCost2(g, p, UnitType(t)));
                return -1.f;
            }, true));
            return true;
        }
        // ---- batch 3 (0.39.0)
        case Ab::DsSecretMaps:
        case Ab::DsMigrant:
        case Ab::DsAuctioneer:
        case Ab::DsGledgeBase: out = one(bestBy(d, [&](int x) { return planetWorth(g, x); })); return true;
        case Ab::DsIledrith: out = one(bestBy(d, [&](int x) { return float(threatNear(g, p, g.planets[size_t(x)].system)) * 2.f + planetWorth(g, x); })); return true;
        case Ab::DsPilgrimage:  // (0.44.7) where the ship helps most - enemies near; not without money for one
            if (spendable(g, p, false) < 2) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : 1.f + float(threatNear(g, p, s)) + (g.systems[size_t(s)].home == p ? 0.5f : 0.f); }, 0.f));
            return true;
        case Ab::DsIndoctrination:  // (0.44.6: the threatened, valuable planet - not always home)
            out = one(bestBy(d, [&](int x) {
                const PlanetState& ps = g.planets[size_t(x)];
                return float(threatNear(g, p, ps.system)) * 2.f + planetWorth(g, x) * 0.5f - 0.6f * float(ps.units[size_t(p)].groundForces()) -
                       (g.systems[size_t(ps.system)].home == p ? 1.f : 0.f);
            }));
            return true;
        case Ab::DsEncryptionKey:
            if (d.prompt.find("which colour") != std::string::npos) {  // the colour our wanted technologies miss most
                out = one(bestBy(d, [&](int c) {
                    float v = 0;
                    for (int t = 0; t < kTechCount; ++t) {
                        if (me.hasTech(t) || !techAvailableToResearch(g, p, t)) continue;
                        int miss[4];
                        missingPrereqs(g, p, t, miss);
                        if (miss[c] > 0) v += aiTechWant(g, p, t);
                    }
                    return v;
                }));
                return true;
            }
            out = one(bestBy(d, [&](int x) { return x < 0 ? -1.f : (planetHasSkip(g, x) ? 0.f : 5.f) + planetWorth(g, x) * 0.1f; }));
            return true;
        case Ab::DsMgur: out = one(has(d, p) ? p : maxOption(d)); return true;
        case Ab::DsReplicators:
            if (d.prompt == "Exhaust Merged Replicators?") {  // 0.43.0: the biggest gain we can pay for
                out = one(d.value >= 2 ? bestBy(d, [&](int c) { return c < 0 ? 0.f : float(c >= 100 ? std::min(c - 100, d.value) : 2); }) : -1);
                return true;
            }
            out = yes(d.value >= 2);
            return true;
        case Ab::DsDurran: out = yes(d.value >= 2); return true;
        case Ab::DsMaximus: out = yes(spendable(g, p, false) >= 2); return true;
        case Ab::DsAegis: out = yes(d.value != int(UnitType::Fighter)); return true;
        case Ab::DsCombatDrills: out = yes(d.value >= 0 && d.value < kUnitTypes && unitCost2(g, p, UnitType(d.value)) >= 6); return true;
        case Ab::DsImpressment: out = yes(spendable(g, p, false) >= 3); return true;
        case Ab::DsImpressAction: out = one(bestBy(d, [&](int x) { return -planetWorth(g, x); })); return true;
        case Ab::DsJginFaru: out = yes(d.value > 0); return true;
        case Ab::DsYpru: {
            const int sys = g.activeSystem;
            const int mine = sys >= 0 ? g.systems[size_t(sys)].space[size_t(p)].nonFighterShips() : 0;
            out = yes(d.value >= 3 || (d.value >= 2 && mine <= d.value + 1));
            return true;
        }
        case Ab::DsMercenaries: {  // only into our own fights
            if (d.value == 0) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : float(g.systems[size_t(s)].space[size_t(p)].count(UnitType::Fighter)) - 2.f * float(threatNear(g, p, s)); }));
            return true;
        }
        case Ab::DsMercTarget: out = one(has(d, p) ? p : bestBy(d, [&](int q) { return -float(g.players[size_t(q)].vp); })); return true;
        case Ab::DsDataLeak: out = one(bestBy(d, [&](int r) { return relicPref(r); })); return true;
        case Ab::DsBlackMarkets: {  // the deck whose fragments we collect
            out = one(bestBy(d, [&](int deck) {
                float n = 0;
                for (int16_t f : me.fragments)
                    if (isRelicFragment(f) && int(kExploreCards[f].type) == deck) n += 1;
                return n + (d.options.size() && d.options[0].id == deck ? 0.5f : 0.f);
            }));
            return true;
        }
        case Ab::DsBlackmail: {
            const int card = d.value / 16, actor = d.value % 16;
            const int av = actor >= 0 && actor < g.numPlayers ? g.players[size_t(actor)].vp : 0;
            const bool imperial = card == 7;
            out = yes((imperial && av >= me.vp && (me.strategy >= 2 || av >= g.vpGoal - 3)) || (av >= std::max(6, me.vp + 2) && me.strategy >= 2));
            return true;
        }
        case Ab::DsUnderground: out = yes(true); return true;
        case Ab::DsSalGavda:
            if (d.onPlanets) {
                out = one(bestBy(d, [&](int x) { return -float(threatNear(g, p, g.planets[size_t(x)].system)); }));
                return true;
            }
            out = one(bestBy(d, [&](int c) { return c < 0 ? 0.f : (threatNear(g, p, g.planets[size_t(c / 16)].system) >= 3 ? 1.f : -1.f); }, true));
            return true;
        case Ab::DsQuaxdol:
            out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : (g.planets[size_t(x)].exhausted ? 3.f : 1.f) + planetWorth(g, x) * 0.2f; }, true));
            return true;
        case Ab::DsBanua:
            if (d.onPlanets) {
                out = one(maxOption(d));
                return true;
            }
            out = one(bestBy(d, [&](int c) { return c < 0 ? 0.5f : (g.planets[size_t(c / 16)].owner != p ? 2.f : 0.f); }, true));
            return true;
        case Ab::DsPrivateer: {  // influence for mechs while enough is left for tokens and votes
            const int inf = spendable(g, p, true, false);
            out = one(bestBy(d, [&](int k) { return k == 0 ? 0.5f : (inf - k >= 4 ? float(k) : -1.f); }));
            return true;
        }
        case Ab::DsMantle: out = one(bestBy(d, [&](int x) { return x < 0 ? -9.f : 2.f - float(planetResources(g, x)) - 0.6f * float(planetInfluence(g, x)); })); return true;
        case Ab::DsDeepMining: out = yes(d.value == 0); return true;
        case Ab::DsGorthrim:
            if (d.onPlanets) {  // a harmful attachment goes on the least valuable planet
                const bool bad = d.value >= 0 && d.value < kExploreCardCount && !std::strcmp(kExploreCards[d.value].key, "dmz");
                out = one(bestBy(d, [&](int x) { return bad ? -planetWorth(g, x) : planetWorth(g, x); }));
                return true;
            }
            out = one(maxOption(d));
            return true;
        case Ab::DsPsychographics: out = one(bestBy(d, [&](int o) { return float(kObjectives[o].points); })); return true;
        case Ab::DsUnity: out = one(bestBy(d, [&](int o) { return float(kObjectives[o].points); })); return true;  // (0.44.6: the most points)
        case Ab::DsDatapool: out = yes(me.tradeGoods - 3 >= aiGoals(g, p).keepTg); return true;  // (0.44.5: spend objectives first)
        case Ab::DsClodho: out = yes(true); return true;
        case Ab::DsAtropha: {
            const float keep = objectiveFit(g, p, me.dsSheet);
            const int best = bestBy(d, [&](int o) { return objectiveFit(g, p, o); });
            out = one(best >= 0 && objectiveFit(g, p, best) > keep + 0.5f ? best : -1);
            return true;
        }
        case Ab::DsGlory:
            if (d.prompt.find("Move which") != std::string::npos) {  // a token where we have nothing left
                out = one(bestBy(d, [&](int s) { return hasUnits(g, p, s) ? 0.f : 1.f; }));
                return true;
            }
            out = one(has(d, 2) && me.strategy >= 2 ? 2 : has(d, 1) ? 1 : -1);
            return true;
        case Ab::DsHeroism:
        case Ab::DsMerkismathr: out = yes(true); return true;
        case Ab::DsZhrgar: out = yes(d.value >= 2 && (d.targetPlayer < 0 || d.targetPlayer == p)); return true;  // (0.44.6: not for others)
        case Ab::DsVassalage: out = yes(d.value >= 3); return true;
        case Ab::DsYgegnad: out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : 1.f + float(g.systems[size_t(s)].space[size_t(p)].ships()); })); return true;
        // ---- batch 4 (0.40.0)
        case Ab::DsCloaked: {  // keep a reserve of 2 cloaked ships for ambushes; home ships stay when threatened
            int held = 0;
            for (int16_t c : me.captured)
                if (c / 16 == p && !isGroundForce(UnitType(c % 16))) ++held;
            const int want = std::max(0, 2 - held);
            out = one(d.value == 1 && threatNear(g, p, me.homeSystem) > 0 ? 0 : std::min(want, maxOption(d)));
            return true;
        }
        case Ab::DsShroud: out = yes(true); return true;  // units on the sheet do nothing; bring them out whenever the fleet moves
        case Ab::DsTreasure:
        case Ab::DsAiSurvey: out = one(-1); return true;
        case Ab::DsNightingale:
        case Ab::DsSkirmishers:
        case Ab::DsSieve:
        case Ab::DsDuuban:
        case Ab::DsFinePrint:
        case Ab::DsKrovoz:
        case Ab::DsCollector:
        case Ab::DsSkhot:
        case Ab::DsPreserve: out = yes(true); return true;
        case Ab::DsSeeker:
        case Ab::DsCaptainDust: {  // the fragment kind we collect most of
            auto have = [&](int kind) {
                float n = 0;
                for (int16_t f : me.fragments)
                    if (isRelicFragment(f) && int(kExploreCards[f].type) == kind) n += 1;
                return n;
            };
            if (ab == Ab::DsSeeker) out = one(bestBy(d, [&](int c) { return have(int(kExploreCards[c % 1000].type)); }));
            else out = one(bestBy(d, [&](int deck) { return have(deck) + 0.5f; }));
            return true;
        }
        case Ab::DsGriphyn: out = one(bestBy(d, [&](int r) { return relicPref(r); })); return true;
        case Ab::DsNightshade: out = one(d.value == 0 ? maxOption(d) : 0); return true;
        case Ab::DsFacsimile:
            if (spaceOdds(g, p) > 0.85f) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int t) { return t < 0 ? -1.f : float(unitCost2(g, p, UnitType(t))); }));
            return true;
        case Ab::DsIllusory: {  // help an outcome we voted for: the 2 planets with the most influence
            Answer a;
            if (d.value == 1) {
                std::vector<std::pair<int, int>> v;
                for (const Option& o : d.options) v.push_back({planetInfluence(g, o.id), o.id});
                std::sort(v.rbegin(), v.rend());
                for (size_t i = 0; i < v.size() && i < 2; ++i) a.picks.push_back(v[i].second);
            }
            out = a;
            return true;
        }
        case Ab::DsFractal:
            if (d.value == int(UnitType::Fighter) || d.value == int(UnitType::Infantry)) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : float(g.systems[size_t(s)].space[size_t(p)].ships()); }));
            return true;
        case Ab::DsWalik: {
            if (d.value < 2) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int s) {
                if (s < 0) return 0.f;
                float v = 0;
                for (int16_t x : g.systems[size_t(s)].planets) v += g.planets[size_t(x)].owner < 0 ? planetWorth(g, x) : 0.f;
                return v - 2.f * float(threatNear(g, p, s));
            }, true));
            return true;
        }
        case Ab::DsBayan: out = one(bestBy(d, [&](int t) { return t < 0 ? -1.f : float(unitCost2(g, p, UnitType(t))); })); return true;
        case Ab::DsCollateral: out = yes(d.value != int(UnitType::Fighter)); return true;
        case Ab::DsBindingDebts: out = yes(me.tradeGoods >= 2); return true;
        case Ab::DsMidas: {  // (0.44.7) the trade goods that make the move worth the most more than they cost
            const int act = g.activeSystem;
            if (act < 0) {
                out = one(0);
                return true;
            }
            const float base = aiMovePlanValue(g, p, act);
            int best = 0;
            float bv = 0.5f;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id <= 0) continue;
                Game h = g;
                h.fx.dsBoost = int8_t(o.id);
                h.fx.dsBoostOwner = int8_t(p);
                const float gain = aiMovePlanValue(h, p, act) - base - 0.9f * float(o.id);
                if (gain > bv) bv = gain, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::DsKrovozHit: out = yes(d.value >= 2); return true;
        case Ab::DsYudri: out = one(bestBy(d, [&](int x) { return x < 0 ? -1.f : float(planetInfluence(g, x)); })); return true;
        case Ab::DsPutrivPay: out = one(has(d, 1) ? 1 : has(d, 0) ? 0 : maxOption(d)); return true;
        case Ab::DsAutonetic:
            if (d.prompt.find("which card") != std::string::npos) {
                out = one(bestBy(d, [&](int c) { return aiCardKeepValue(c); }));
                return true;
            }
            if (d.prompt.find("which planet") != std::string::npos) {
                out = one(bestBy(d, [&](int x) { return float(threatNear(g, p, g.planets[size_t(x)].system)); }));
                return true;
            }
            {
                float best = 0;
                for (int16_t c : g.actionDiscard) best = std::max(best, aiCardKeepValue(c));
                out = one(has(d, 1) && best >= 3.f ? 1 : has(d, 2) && int(me.actionCards.size()) >= actionCardLimit(g, p) - 1 ? 2 : 0);
            }
            return true;
        case Ab::DsRecursiveWorm: {  // a rival's token where their fleet next door could hit us
            int best = -1;
            float bv = 1.5f;
            for (const Option& o : d.options) {
                if (o.id < 0) continue;
                const int s = o.id / 16, q = o.id % 16;
                float v = 0;
                for (int16_t a : g.systems[size_t(s)].adj) v += float(g.systems[size_t(a)].space[size_t(q)].nonFighterShips());
                if (v > bv) {
                    bv = v;
                    best = o.id;
                }
            }
            out = one(best);
            return true;
        }
        case Ab::DsKoryl: {
            const int c = cheapestCard(d);
            out = one(c >= 0 && aiCardKeepValue(c) < 2.f ? c : -1);
            return true;
        }
        case Ab::DsVoiceUnited: out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); })); return true;
        case Ab::DsAlgorithmic: out = one(bestBy(d, [&](int c) { return aiCardKeepValue(c); })); return true;
        case Ab::DsVoidsailors: out = one(bestBy(d, [&](int c) { return exploreWorth(c); })); return true;
        case Ab::DsVoidwake: out = yes(d.value >= 2 || (d.value >= 1 && commanderUsable(g, p, "nivyn"))); return true;  // (0.44.5: the commander rewards damage)
        case Ab::DsWarden: out = yes(d.value == 2); return true;
        case Ab::DsGuidance: out = yes(false); return true;
        case Ab::DsSuldhan: out = yes(d.value >= 1); return true;
        case Ab::DsPolicy: {  // Control (votes), Preserve (fragments), Empower (commodities) - by temperament
            // (0.43.0 debug passes: every bot set the same three, so Connect / Plunder / Exploit never came up)
            const Style& S = aiStyle(g, p);
            if (d.prompt.find("The People") != std::string::npos) out = one(S.on && S.pol < 1.f && S.aggression >= 1.1f ? 0 : 1);
            else if (d.prompt.find("The Environment") != std::string::npos) out = one(S.on && S.explore >= 1.5f ? 1 : 0);  // (Plunder costs 2 production)
            else out = one(S.on && S.aggression >= 1.2f ? 1 : 0);
            return true;
        }
        case Ab::DsConnect:  // an infantry to one of our planets that needs it (an enemy fleet next to it, a thin guard)
            out = one(bestBy(d, [&](int y) {
                if (y < 0) return 0.5f;
                const int sys = g.planets[size_t(y)].system;
                return float(threatNear(g, p, sys)) - float(g.planets[size_t(y)].units[size_t(p)].groundForces());
            }, true));
            return true;
        case Ab::DsExploit: out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : float(g.systems[size_t(s)].space[size_t(p)].ships()); })); return true;
        case Ab::DsPlunder: out = yes(d.value >= 2); return true;
        case Ab::DsFalseFlag:
            if (d.prompt.find("ready which") != std::string::npos) {
                // (0.44.8) a real gain: our planet worth 2 or more than the cheapest ready one we'd exhaust for it (it
                // used to trade a 3/1 for a 2/0)
                float cheapest = 1e9f;
                for (int x : readyPlanets(g, p)) cheapest = std::min(cheapest, planetWorth(g, x));
                const int y = bestBy(d, [&](int x) { return x < 0 ? -1.f : g.planets[size_t(x)].owner == p ? planetWorth(g, x) - cheapest : -9.f; }, 1.9f);
                out = one(y >= 0 ? y : -1);
                return true;
            }
            out = one(bestBy(d, [&](int x) { return -planetWorth(g, x); }));
            return true;
        case Ab::DsGeosympathic: {  // (0.44.7) when +1 move for everyone is worth a planet exhausted - the cheapest one
            const int act = g.activeSystem;
            if (act < 0) {
                out = one(-1);
                return true;
            }
            Game h = g;
            h.fx.dsMove = 1;
            const float gain = aiMovePlanValue(h, p, act) - aiMovePlanValue(g, p, act);
            int cheap = -1;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0 && (cheap < 0 || planetWorth(g, o.id) < planetWorth(g, cheap))) cheap = o.id;
            out = one(cheap >= 0 && gain > 0.4f * planetWorth(g, cheap) + 0.5f ? cheap : -1);
            return true;
        }
        case Ab::DsIncite:
            out = one(d.prompt.find("ready which") != std::string::npos ? bestBy(d, [&](int x) { return planetWorth(g, x); })
                                                                         : bestBy(d, [&](int x) { return -planetWorth(g, x); }));
            return true;
        case Ab::DsBaggil:
        case Ab::DsKnak: out = one(bestBy(d, [&](int x) { return x < 0 ? -1.f : float(std::max(planetResources(g, x), planetInfluence(g, x))); })); return true;
        case Ab::DsPahn: out = one(-1); return true;
        // ---- batch 5 (0.41.0)
        case Ab::DsRally:  // the free system with the richest planets
            out = one(bestBy(d, [&](int s) {
                if (s < 0) return -1.f;
                float v = 0;
                for (int16_t x : g.systems[size_t(s)].planets) v += planetWorth(g, x);
                return v - 0.5f * float(threatNear(g, p, s));
            }));
            return true;
        case Ab::DsDiplomats:  // the planet that covers most of the bill without waste
            out = one(bestBy(d, [&](int x) {
                if (x < 0) return -1.f;
                const int v = d.prompt.find("influence") != std::string::npos ? planetInfluence(g, x) : planetResources(g, x);
                return v <= d.value ? float(v) : float(d.value) - 0.5f * float(v - d.value);
            }));
            return true;
        case Ab::DsDaedalon:  // 0.43.0: an empty planet first; a defended one only against a single defender
            out = one(bestBy(d, [&](int x) {
                if (x < 0) return 0.2f;
                int foes = neutralGroundOn(g, x);
                for (int q = 0; q < g.numPlayers; ++q) foes += q != p ? groundForcesOn(g, q, x) : 0;
                if (foes > 1) return -1.f;
                return planetWorth(g, x) * (foes ? 0.5f : 1.f) + (foes ? 0.f : 1.f);
            }, true));
            return true;
        // ---- Discordant Stars breakthroughs (0.43.0)
        case Ab::DsCelagrom: {  // (0.52.1) the Celagrom's hit: the costliest enemy ship that can't simply shrug it off
            const int opp = d.value;
            out = one(bestBy(d, [&](int t) {
                if (t < 0 || opp < 0) return 0.f;
                const UnitDef& u = unitDef(g, opp, UnitType(t));
                return float(std::max<int>(1, u.cost2)) * (u.sustain ? 0.35f : 1.f) + 0.1f;
            }));
            return true;
        }
        case Ab::DsbtCeldauri:  // the Celagrom joins the biggest fleet near a front
            out = one(bestBy(d, [&](int s) { return float(g.systems[size_t(s)].space[size_t(p)].nonFighterShips()) + float(threatNear(g, p, s)); }));
            return true;
        case Ab::DsbtAxis: out = one(-1); return true;
        case Ab::DsbtFlorzen:  // the richest other player (more notes to take and more to lose)
            out = one(g.round >= 2 ? bestBy(d, [&](int q) { return q < 0 ? 0.5f : float(g.players[size_t(q)].notes.size()) + 0.2f * float(g.players[size_t(q)].tradeGoods); }, true) : -1);
            return true;
        case Ab::DsbtCymiae:
            if (d.prompt.find("purge a promissory") != std::string::npos) {  // a leader's token out, else the trade goods - if a spare note
                int spare = 0;  // (only notes we can do without: Ceasefire, Political Secret, Trade Agreement - and not in round 1-2)
                for (int16_t n : me.notes)
                    if (g.fixes < 8 || noteOwner(n) == p)  // (0.44.4: only our own notes can be purged)
                        spare += noteType(n) == kNoteCeasefire || noteType(n) == kNotePoliticalSecret || noteType(n) == kNoteTradeAgreement ? 1 : 0;
                if (spare == 0 || g.round <= 2) {
                    out = one(-1);
                    return true;
                }
                out = one(bestBy(d, [&](int c) {
                    if (c < 0) return 0.f;
                    if (c == 100) return 1.5f;
                    return 1.f + float(g.players[size_t(c)].vp) * 0.2f;
                }));
            } else if (d.prompt.find("Purge which") != std::string::npos) {  // a generic note we don't need
                out = one(bestBy(d, [&](int n) {
                    const int t = noteType(n);
                    return t == kNotePoliticalSecret ? 1.f : t == kNoteTradeAgreement ? 0.8f : t == kNoteCeasefire ? 0.6f : -2.f;
                }));
            } else {  // 0.44.4: into a system with other players' ships - the token locks them there
                out = one(bestBy(d, [&](int s) {
                    int n = 0;
                    for (int q = 0; q < g.numPlayers; ++q)
                        if (q != p) n += g.systems[size_t(s)].space[size_t(q)].nonFighterShips();
                    return float(n) - 0.1f * float(threatNear(g, p, s));
                }));
            }
            return true;
        case Ab::DsbtGhoti: {  // 0.44.4: on passing, pull a lone ship under threat into a neighbouring stack of ours
            auto score = [&](int s, int a, int t) {
                if (t == int(UnitType::Fighter) || t == int(UnitType::Flagship) || t == int(UnitType::WarSun)) return 0.f;
                const Force& here = g.systems[size_t(s)].space[size_t(p)];
                if (here.nonFighterShips() != 1 || here.groundForces() + here.count(UnitType::Fighter) > 0) return 0.f;
                if (g.systems[size_t(s)].home >= 0) return 0.f;
                const int threat = threatNear(g, p, s);
                return threat >= 2 ? float(threat) + 0.5f * float(g.systems[size_t(a)].space[size_t(p)].nonFighterShips()) : 0.f;
            };
            if (d.prompt.find("Move which ship") != std::string::npos) {
                out = one(bestBy(d, [&](int m) { return score(m / 4096, (m / 16) % 256, m % 16); }));
                return true;
            }
            float best = 0.f;
            if (has(d, 1))
                for (size_t s = 0; s < g.systems.size(); ++s)
                    for (int t : kShipIdx)
                        if (g.systems[s].space[size_t(p)].n[t])
                            for (int16_t a : g.systems[s].adj)
                                if (hasShips(g, p, a) && !otherShipsIn(g, p, a)) best = std::max(best, score(int(s), a, t));
            out = one(best >= 2.f ? 1 : -1);
            return true;
        }
        // 0.44.4: an action card instead of a weak exploration card only (Decision::value = the card)
        case Ab::DsbtAugers:
            out = yes(int(me.actionCards.size()) < actionCardLimit(g, p) - 1 && (d.value <= 0 || aiExploreCardValue(d.value) <= 1.05f));
            return true;
        // 0.44.4: a free technology beats a command token unless tokens are short
        case Ab::DsbtKolume: out = one(has(d, 1) && me.tactic + me.fleet + me.strategy >= 6 ? 1 : has(d, 0) ? 0 : has(d, 1) ? 1 : -1); return true;
        case Ab::DsbtLanefir:
            if (isYesNoOptions(d)) out = yes(true);
            else out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : 1.f + planetWorth(g, x) * 0.1f; }));
            return true;
        case Ab::DsbtNokar:
            if (d.prompt.find("goes where") != std::string::npos)  // 0.44.4: in the leader's way - never in ours
                out = one(bestBy(d, [&](int s) { return s < 0 ? 0.f : nokarSpot(g, p, s); }, true));
            else out = one(me.commodities + 2 <= commodityCap(g, me) ? 0 : 1);
            return true;
        case Ab::DsbtZealots: out = one(has(d, 0) ? 0 : has(d, 1) ? 1 : -1); return true;  // (0.44.4: the free technology first)
        case Ab::DsbtKyro: out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); })); return true;
        case Ab::DsbtBentor:
            // 0.44.4: the frontier deck while Ancient Blueprints still wants the unknown fragment (only that deck has them)
            if (isYesNoOptions(d))
                out = yes(isFaction(g, p, "bentor") && !(me.dsFrags & (1 << int(ExploreType::Frontier))) &&
                          !g.exploreDeck[size_t(ExploreType::Frontier)].empty());
            else out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : 1.f + planetWorth(g, x) * 0.1f; }));
            return true;
        case Ab::DsbtKollecc:  // a relic fragment or an attachment from the museum, else draw
            out = one(bestBy(d, [&](int c) {
                if (c < 0) return 0.5f;
                return isRelicFragment(c) ? 2.f : kExploreCards[c].kind == ExploreKind::Attach ? 1.5f : 0.2f;
            }));
            return true;
        case Ab::DsbtFreesys:
        case Ab::DsbtLizho:
            if (d.prompt.find("comes from") != std::string::npos) {  // 0.44.4: spare infantry far from danger, never the home's last ones
                out = one(bestBy(d, [&](int y) {
                    if (y >= 100000) return 2.f;  // (riding in space: spare)
                    const PlanetState& ps = g.planets[size_t(y)];
                    const int inf = ps.units[size_t(p)].count(UnitType::Infantry);
                    float v = float(inf) - float(threatNear(g, p, ps.system)) * 0.5f - planetWorth(g, y) * 0.1f;
                    if (isHomePlanet(g, y) && inf <= 2) v -= 5.f;
                    return v;
                }));
                return true;
            }
            out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : 0.5f + planetWorth(g, x) * 0.1f; }));
            return true;
        case Ab::DsbtTnelis:  // losing the fight: a secret for safety (the one furthest from done)
            out = one(d.value < 0 ? bestBy(d, [&](int o) { return o < 0 ? -50.f : float(objectiveProgress(g, p, o).missing()); }) : -1);
            return true;
        case Ab::DsbtOlradin:
            if (isYesNoOptions(d)) {  // 0.44.4: rise up only where our infantry outnumber the defenders (the card goes for good)
                int good = 0, bad = 0;
                for (size_t x = 0; x < g.planets.size(); ++x) {
                    if (!((g.planets[x].coexist >> p) & 1)) continue;
                    const int mine = g.planets[x].units[size_t(p)].count(UnitType::Infantry);
                    if (!mine) continue;
                    int theirs = 0;
                    for (int q = 0; q < kSides; ++q)
                        if (q != p) theirs += g.planets[x].units[size_t(q)].groundForces();
                    (mine > theirs ? good : bad) += 1;
                }
                out = yes(good >= 1 && good >= bad && (good >= 2 || g.round >= 5));
            }
            else out = one(me.tradeGoods >= 3 ? bestBy(d, [&](int c) { return c < 0 ? 0.f : 0.5f + planetWorth(g, c % 1000) * 0.1f; }) : -1);
            return true;
        case Ab::DsbtKhrask: out = yes(true); return true;  // (coexisting keeps the infantry alive)
        case Ab::DsbtDihmohn:  // (ships in the middle of the fight if we can pay for 2; else the frontier token)
            out = one(has(d, 1) && spendable(g, p, false) >= 3 && aiFreeSpend(g, p, false) >= 2 ? 1 : has(d, 0) ? 0 : -1);
            return true;
        case Ab::DsbtGhemina: out = yes(true); return true;  // (a retreat announced: leave before their dice)
        case Ab::DsbtVaden: out = yes(false); return true;
        case Ab::DsbtCheiran:  // (a damaged dreadnought / war sun comes back as fresh units; never the flagship)
            out = one(bestBy(d, [&](int c) {
                if (c < 0) return 0.5f;
                const int s = c / 16, t = c % 16;
                if (t == int(UnitType::Flagship) || !g.systems[size_t(s)].space[size_t(p)].dmg[t]) return 0.f;
                return float(unitCost2(g, p, UnitType(t)));
            }, true));
            return true;
        case Ab::DsbtVaylerian: {  // 0.44.4: no fight here - send a spare warship to guard a threatened planet of ours next door
            if (d.prompt.find("discard which") != std::string::npos) {
                out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); }));
                return true;
            }
            const int act = g.activeSystem;
            bool fight = false;
            for (int q = 0; q < kSides && act >= 0; ++q) fight = fight || (q != p && g.systems[size_t(act)].space[size_t(q)].ships() > 0);
            float cheapest = 99.f;
            for (int16_t c : me.actionCards) cheapest = std::min(cheapest, aiCardKeepValue(c));
            int best = -1;
            float bv = 0.f;
            if (!fight && act >= 0 && g.systems[size_t(act)].space[size_t(p)].nonFighterShips() >= 2 && cheapest <= 1.2f)
                for (const Option& o : d.options) {
                    if (!o.enabled || o.id < 0) continue;
                    const int a = o.id / 16, t = o.id % 16;
                    if (t != int(UnitType::Destroyer) && t != int(UnitType::Cruiser) && t != int(UnitType::Dreadnought)) continue;
                    bool ours = false;
                    for (int16_t x : g.systems[size_t(a)].planets) ours = ours || g.planets[size_t(x)].owner == p;
                    if (!ours || g.systems[size_t(a)].space[size_t(p)].nonFighterShips() > 0) continue;
                    const float v = float(threatNear(g, p, a)) - (t == int(UnitType::Dreadnought) ? 1.f : 0.f);
                    if (v >= 2.f && v > bv) bv = v, best = o.id;
                }
            out = one(best);
            return true;
        }
        case Ab::DsbtMortheus: {
            // 0.44.4: a stronger ship in for the weakest one here (one without cargo to lose), never one guarding home
            const int act = g.activeSystem;
            auto worth = [&](int t) { return float(unitCost2(g, p, UnitType(t))) + float(unitDef(g, p, UnitType(t)).combatDice); };
            auto weakestHere = [&]() {
                float w = 99.f;
                if (act < 0) return w;
                const Force& f = g.systems[size_t(act)].space[size_t(p)];
                for (int t : kShipIdx)
                    if (f.n[t] && unitDef(g, p, UnitType(t)).capacity == 0) w = std::min(w, worth(t));
                return w;
            };
            if (d.prompt.find("swap in") != std::string::npos) {
                const float floor = weakestHere();
                out = one(bestBy(d, [&](int c) {
                    if (c < 0) return 0.f;
                    const int s = c / 16, t = c % 16;
                    if (s == g.players[size_t(p)].homeSystem || unitDef(g, p, UnitType(t)).capacity > 0) return -1.f;
                    return worth(t) - floor - 2.f;
                }, true));
            } else {
                out = one(bestBy(d, [&](int t) { return -worth(t) - (unitDef(g, p, UnitType(t)).capacity > 0 ? 10.f : 0.f); }));
            }
            return true;
        }
        case Ab::DsbtEdyn:
            if (d.prompt.find("In return") != std::string::npos) {  // 0.44.4 (the other player): a secret rather than a guest on our planet
                out = one(has(d, 0) ? 0 : 1);
                return true;
            }
            if (d.prompt.find("to which player") != std::string::npos)
                out = one(bestBy(d, [&](int q) { return q < 0 ? -1.f : float(g.players[size_t(q)].vp); }));
            else if (d.prompt.find("which of your secrets") != std::string::npos)
                out = one(bestBy(d, [&](int o) { return float(objectiveProgress(g, p, o).missing()); }));
            else out = one(bestBy(d, [&](int x) { return planetWorth(g, x); }));
            return true;
        case Ab::DsPlague: out = one(bestBy(d, [&](int x) { return x < 0 ? 0.f : 1.f + planetWorth(g, x) * 0.1f; })); return true;  // a free infantry
        case Ab::DsEnvoy:  // 0.43.0: the biggest vote another player has on that trait (the points leader's first)
            out = one(bestBy(d, [&](int x) {
                if (x < 0) return 0.5f;
                const int q = g.planets[size_t(x)].owner;
                if (q == p) return -5.f;
                int lead = 0;
                for (int r = 0; r < g.numPlayers; ++r) lead = std::max(lead, int(g.players[size_t(r)].vp));
                return float(planetInfluence(g, x)) + (g.players[size_t(q)].vp == lead ? 0.6f : 0.f);
            }));
            return true;
        case Ab::DsOtto:
        case Ab::DsWarSong:
        case Ab::DsKantrus: out = one(bestBy(d, [&](int x) { return planetWorth(g, x); })); return true;
        case Ab::DsKorela:
            out = one(bestBy(d, [&](int x) {
                float n = 0;
                for (int q = 0; q < kSides; ++q)
                    if (q != p) n += float(g.planets[size_t(x)].units[size_t(q)].total());
                return n - 3.f * float(g.planets[size_t(x)].units[size_t(p)].total());
            }));
            return true;
        case Ab::DsGrace: {  // the secondary worth most now
            static const float kPref[8] = {0.2f, 0.6f, 0.5f, 1.2f, 0.9f, 1.f, 1.5f, 0.7f};
            out = one(bestBy(d, [&](int c) { return c < 0 ? -1.f : kPref[c & 7]; }));
            return true;
        }
        case Ab::DsRadiance: {
            float conf = 0;
            const int o = aiPredictOutcome(g, d.agenda, &conf);
            out = one(has(d, o) ? o : maxOption(d));
            return true;
        }
        case Ab::DsEdynRider: {  // the same judgement as the Keleres Rider
            Decision d2 = d;
            d2.ability = int(Ab::KeleresRider);
            return aiFactionAnswer(g, d2, rng, out);
        }
        case Ab::DsAllant: out = one(has(d, p) && me.tactic >= 1 ? p : -1); return true;  // (an extra action of our own)
        case Ab::DsAllantPeek: out = yes(false); return true;
        case Ab::DsKadryn: out = yes(d.value == 0); return true;
        case Ab::DsMidir: out = one(bestBy(d, [&](int o) { return aiOutcomeValue(g, p, d.agenda, o); }, true)); return true;
        case Ab::DsSigilToken: {  // lock the leader out
            out = one(bestBy(d, [&](int c) { return c < 0 ? -1.f : float(g.players[size_t(c % 16)].vp) + 0.1f * float(threatNear(g, c % 16, c / 16)); }));
            return true;
        }
        case Ab::DsHired: {  // sell the small ships where they fight someone else - and leave none of ours to be fought
            if (d.value != 3) {
                out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int t) { return t < 0 ? -100.f : -float(unitCost2(g, p, UnitType(t))); }));
            return true;
        }
        case Ab::DsHiredBuy: out = yes((d.value & 1) || me.tradeGoods >= 4); return true;
        case Ab::DsHireOffer: {  // 0.43.0: sell ships to a player about to fight someone else - not to a near winner
            const int b = g.activePlayer;
            const bool nearWin = b >= 0 && g.players[size_t(b)].vp >= g.vpGoal - 3;
            const bool rich = b >= 0 && g.players[size_t(b)].tradeGoods + g.players[size_t(b)].commodities >= 2;
            out = yes((d.value & 1) && !nearWin && rich && !(b >= 0 && hostileFlag(g, p, b)));
            return true;
        }
        case Ab::DsHirePick:  // the cheapest ships, the ones already there first
            out = one(bestBy(d, [&](int c) {
                if (c < 0) return -50.f;
                return -float(unitCost2(g, p, UnitType(c % 16))) + (c / 16 == g.activeSystem ? 0.5f : 0.f);
            }));
            return true;
        case Ab::DsNavigator: out = yes(spaceOdds(g, p) < 0.45f); return true;
        case Ab::DsSalSparrow: out = yes(p == g.activePlayer && d.value > 0); return true;  // (room in the fleet pool)
        case Ab::DsSkarvald: out = yes(p == g.activePlayer); return true;
        case Ab::DsOrderBuy: {  // the dearest order first: they sell for more
            int best = -1;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0 && (best < 0 || o.id < best)) best = o.id;
            out = one(best);
            return true;
        }
        case Ab::DsOrderUse: {  // where the new ship is needed most - and fits the fleet pool (0.42.2), else keep the order
            const int n = d.prompt.find("2 destroyers") != std::string::npos ? 2 : 1;
            out = one(bestBy(d, [&](int s) {
                if (s < 0) return 0.5f;
                if (fleetLimit(g, p) - fleetUseIn(g, p, s) < n) return -1.f;
                return 1.f + float(threatNear(g, p, s));
            }, true));
            return true;
        }
        case Ab::DsRiftEngines: {  // 0.43.0 (D113): a ship that reaches a fight only with the +2 (not a big one - 30% lost)
            const int act = g.activeSystem;
            bool target = false;
            if (act >= 0) {
                target = otherShipsIn(g, p, act);
                for (int16_t x : g.systems[size_t(act)].planets) target = target || (g.planets[size_t(x)].owner != p);
            }
            out = one(bestBy(d, [&](int c) {
                if (c < 0 || !target) return c < 0 ? 0.f : -1.f;
                const int s = c / 16;
                const UnitType t = UnitType(c % 16);
                if (t == UnitType::Flagship || t == UnitType::WarSun) return -1.f;
                if (shipReach(g, p, s, act, t).ok || !shipReach(g, p, s, act, t, 2).ok) return -1.f;
                // (0.44.7) only when the plan really uses it - the gain against a 30% chance of losing the ship
                Game h = g;
                h.fx.dsRiftType = int8_t(t);
                h.fx.dsRiftOwner = int8_t(p);
                h.fx.dsRiftSys = int16_t(s);
                const float gain = aiMovePlanValue(h, p, act) - aiMovePlanValue(g, p, act) - 0.3f * float(unitCost2(g, p, t)) / 2.f;
                return gain > 0.3f ? gain : -1.f;
            }, true));
            return true;
        }
        case Ab::DsTckVsk: out = yes(spendable(g, p, false) >= 8); return true;
        case Ab::DsOmen: {  // a sure hit on a die that would usually miss; the smallest Omen that does it
            int best = -1;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0 && o.id % 100 >= 6 && (best < 0 || o.id / 100 < best / 100)) best = o.id;
            out = one(fquirkIs(g, p, "mykomentori", 1) && best >= 0 && best % 100 < 8 ? -1 : best);
            return true;
        }
        case Ab::DsLactarius:
            if (d.prompt.find("change one of") != std::string::npos) {  // 0.43.0: any roll, dice in view
                const int q = d.value, act = g.activeSystem;
                const bool facing = act >= 0 && q != p && hasUnits(g, p, act) && hasUnits(g, q, act);  // they fight us
                int lead = 0;
                for (int r = 0; r < g.numPlayers; ++r) lead = std::max(lead, int(g.players[size_t(r)].vp));
                const bool leader = q != p && g.players[size_t(q)].vp == lead && lead >= g.vpGoal - 3;
                out = one(bestBy(d, [&](int c) {
                    if (c < 0) return 0.f;
                    bool spoils = false;  // "A hit becomes a miss"
                    for (const Option& o : d.options)
                        if (o.id == c) spoils = o.detail.find("hit becomes a miss") != std::string::npos;
                    // an enemy's hit spoiled when they fight us (or lead the race), our own miss saved
                    if (q == p) return spoils ? -1.f : 1.f;
                    return spoils && (facing || leader) ? 1.f : -1.f;
                }, true));
            } else {
                out = yes(d.value <= 5);
            }
            return true;
        case Ab::DsInsightReroll:
            if (d.prompt.find("one of your dice") != std::string::npos) {  // 0.43.0: the die likeliest to hit
                out = one(bestBy(d, [&](int i) {
                    if (i < 0) return 0.f;
                    for (const Option& o : d.options)
                        if (o.id == i) {
                            const size_t k = o.detail.find("needs ");
                            const int need = k == std::string::npos ? 10 : std::atoi(o.detail.c_str() + k + 6);
                            return need <= 8 ? float(11 - need) : -1.f;
                        }
                    return -1.f;
                }, true));
            } else {
                out = yes(true);
            }
            return true;
        case Ab::DsPsychoactive: out = yes(d.value >= 2); return true;  // (0.44.6: once a round - 2 misses are worth it)
        case Ab::DsWhichFlagship:
            if (d.prompt.find("build") != std::string::npos) out = one(1);
            else if (d.prompt.find("moved") != std::string::npos) out = one(d.value > 0 ? 1 : 0);
            else out = one(0);
            return true;
        case Ab::DsRaidLeaders:
            out = one(has(d, int(UnitType::Dreadnought)) ? int(UnitType::Dreadnought) : has(d, int(UnitType::Cruiser)) ? int(UnitType::Cruiser)
                      : has(d, int(UnitType::Destroyer)) ? int(UnitType::Destroyer) : -1);
            return true;
        case Ab::DsShipmonger: {
            // 0.42.2 (#187: 23 ships in 60 games went straight back over the fleet pool): the ship goes where the
            // fleet pool has room - a front first; a player gets it only if they have room somewhere
            auto room = [&](int q, int s) { return fleetLimit(g, q) - fleetUseIn(g, q, s); };
            auto anyRoom = [&](int q) {
                for (size_t s = 0; s < g.systems.size(); ++s)
                    if (hasShips(g, q, int(s)) && room(q, int(s)) > 0) return true;
                return false;
            };
            if (d.prompt.find("which player") != std::string::npos) {  // ourselves if it fits, else the commodities
                if (has(d, p) && anyRoom(p)) {
                    out = one(p);
                    return true;
                }
                out = one(bestBy(d, [&](int q) {
                    if (q == p) return -5.f;
                    return (anyRoom(q) ? 2.f : 0.f) - float(g.players[size_t(q)].vp) * 0.3f;
                }));
                return true;
            }
            if (d.prompt.find("which ship") != std::string::npos) {
                out = one(has(d, int(UnitType::Cruiser)) ? int(UnitType::Cruiser) : int(UnitType::Destroyer));
                return true;
            }
            if (d.prompt.find("where") != std::string::npos) {
                out = one(bestBy(d, [&](int s) {
                    if (s < 0) return -9.f;
                    float v = room(p, s) > 0 ? 10.f : 0.f;
                    for (int16_t a : g.systems[size_t(s)].adj)
                        if (otherShipsIn(g, p, a)) v += 1.f;  // a front
                    return v + float(g.systems[size_t(s)].space[size_t(p)].nonFighterShips()) * 0.2f;
                }));
                return true;
            }
            return false;
        }
        case Ab::DsDemiQueen:
            if (d.prompt.find("give the") != std::string::npos) {  // to whoever holds the most promissory notes
                out = one(bestBy(d, [&](int q) { return q < 0 ? -1.f : float(g.players[size_t(q)].notes.size()); }));
                return true;
            }
            return false;
        // ---- batch 6 (0.42.0)
        case Ab::DsBranchPlanet: out = one(bestBy(d, [&](int x) { return planetWorth(g, x); })); return true;  // (our own planet: the best)
        case Ab::DsHolding: out = one(has(d, 0) ? 0 : 1); return true;
        case Ab::DsTargeted:
            if (d.prompt.find("which neighbour") != std::string::npos) {  // the neighbour with the most planets to host it
                out = one(bestBy(d, [&](int q) { return q < 0 ? -1.f : float(controlledPlanets(g, q).size()); }));
                return true;
            }
            out = one(maxOption(d) >= 0 ? bestBy(d, [&](int n) { return n < 0 ? -1.f : 1.f; }) : -1);
            return true;
        case Ab::DsSolis: out = one(bestBy(d, [&](int x) { return x < 0 ? -1.f : float(planetInfluence(g, x)) - 0.6f * float(planetResources(g, x)); })); return true;
        case Ab::DsAurora: out = yes(d.value == 1); return true;  // (when someone else comes)
        case Ab::DsAuberon: out = one(bestBy(d, [&](int t) { return t < 0 ? -1.f : aiTechWant(g, p, t); })); return true;
        case Ab::DsTrapPlace: {  // the nastiest trap first
            static const int kOrder[5] = {1, 2, 3, 0, 4};
            for (int k : kOrder)
                if (has(d, k)) {
                    out = one(k);
                    return true;
                }
            out = one(-1);
            return true;
        }
        case Ab::DsTrapPlanet: out = one(bestBy(d, [&](int x) { return planetWorth(g, x) + 2.f * float(threatNear(g, p, g.planets[size_t(x)].system)); })); return true;
        case Ab::DsTrapReveal: out = yes(true); return true;
        case Ab::DsGridTarget: out = one(bestBy(d, [&](int q) { return q < 0 ? -1.f : 1.f; })); return true;
        case Ab::DsSiphonTake: {
            int best = -1;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0 && (best < 0 || o.id == 0)) best = o.id;
            out = one(best);
            return true;
        }
        case Ab::DsWraith: {  // 0.43.0 (D127): the biggest ship that can't get into the fight otherwise
            const int act = g.activeSystem;
            bool target = false;
            if (act >= 0) {
                target = otherShipsIn(g, p, act);
                for (int16_t x : g.systems[size_t(act)].planets) target = target || g.planets[size_t(x)].owner != p;
            }
            out = one(bestBy(d, [&](int c) {
                if (c < 0 || !target) return c < 0 ? 0.f : -1.f;
                const int s = c / 16;
                const UnitType t = UnitType(c % 16);
                if (t == UnitType::Fighter || s == act || shipReach(g, p, s, act, t).ok || otherShipsIn(g, p, s)) return -1.f;
                return 0.5f + 0.1f * float(unitCost2(g, p, t));
            }, true));
            return true;
        }
        case Ab::DsCounselor: out = yes(true); return true;
        case Ab::DsVasra: out = one(p == g.activePlayer ? bestBy(d, [&](int s) { return s < 0 ? -1.f : 1.f + float(threatNear(g, p, s)); }) : -1); return true;
        case Ab::DsKhazRin:
            if (d.prompt.find("where does") != std::string::npos) {  // keep a Trap where it is unless it sits in reserve
                if (has(d, -1) && d.options[0].label.rfind("Leave it on", 0) == 0) {
                    out = one(-1);
                    return true;
                }
                out = one(bestBy(d, [&](int x) { return x < 0 ? -1.f : (g.planets[size_t(x)].owner == p ? planetWorth(g, x) + 3.f : 0.f); }));
                return true;
            }
            if (d.prompt.find("how many fighters") != std::string::npos) {
                out = one(d.value > 0 ? std::min(maxOption(d), d.value - 1) : maxOption(d));  // (0.46.1: only what fits)
                return true;
            }
            return false;
        case Ab::DsOroZhin: out = one(-1); return true;
        case Ab::DsCombatDrones: out = yes(true); return true;
        case Ab::DsOrbitalGrid:
        case Ab::DsOrbitalShield: out = d.options.size() == 2 && isYesNo(d) ? yes(true) : one(bestBy(d, [&](int x) { return planetWorth(g, x); })); return true;
        case Ab::DsExcavation: out = one(-1); return true;
        case Ab::DsLogicMachina: out = yes(me.strategy >= 2); return true;
        case Ab::DsWrathMachina:
            if (d.prompt.find("which system") != std::string::npos) {
                out = one(bestBy(d, [&](int s) {
                    float v = 0;
                    for (int q = 0; q < g.numPlayers; ++q)
                        if (q != p) v += float(g.systems[size_t(s)].space[size_t(q)].nonFighterShips()) * (1.f + 0.2f * float(g.players[size_t(q)].vp));
                    return v;
                }));
                return true;
            }
            return false;
        case Ab::DsEmergency: {  // (0.44.5) a new dock from reinforcements; moving one only away from danger
            if (d.prompt.find("which space dock") != std::string::npos) {
                if (has(d, -1)) {
                    out = one(-1);
                    return true;
                }
                const int home = me.homeSystem;
                out = one(bestBy(d, [&](int x) {
                    if (x < 0 || x >= int(g.planets.size()) || g.planets[size_t(x)].system == home) return -99.f;
                    return float(threatNear(g, p, g.planets[size_t(x)].system)) - float(planetResources(g, x));
                }, -98.f));
                if (out.picks[0] < 0) out = one(bestBy(d, [](int) { return 0.f; }));
                return true;
            }
            out = one(bestBy(d, [&](int x) { return dockSpot(g, p, x); }, -1e9f));
            return true;
        }
        case Ab::DsLocalContracts: {  // (0.44.5) where 1 unit's Production does most; a token already there costs nothing
            const int home = me.homeSystem;
            out = one(bestBy(d, [&](int s) {
                if (s < 0 || s >= int(g.systems.size())) return -99.f;
                const bool token = g.systems[size_t(s)].hasToken(p);
                // (a token at home locks it for the rest of the round)
                return float(singleUnitProduction(g, p, s)) + 0.5f * float(threatNear(g, p, s)) + (token ? 2.f : 0.f) - (s == home && !token ? 3.f : 0.f);
            }, -98.f));
            if (out.picks[0] < 0) out = one(bestBy(d, [](int) { return 0.f; }));
            return true;
        }
        case Ab::DsDeliverance:  // (0.44.8) the kind of ship the hit destroys and costs them most
            if (yn) {
                out = yes(true);
                return true;
            }
            out = one(bestBy(d, [&](int t) {
                const int s = g.activeSystem;
                float v = 0.f;
                for (int q = 0; q < g.numPlayers && s >= 0; ++q) {
                    if (q == p) continue;
                    const Force& f = g.systems[size_t(s)].space[size_t(q)];
                    if (!f.n[size_t(t)]) continue;
                    const bool soaks = unitSustains(g, q, UnitType(t)) && f.damaged(UnitType(t)) < f.n[size_t(t)];
                    v = std::max(v, float(unitCost2(g, q, UnitType(t))) * (soaks ? 0.3f : 1.f));
                }
                return v;
            }, -1.f));
            return true;
        case Ab::DsDaedalonTech: {  // (0.44.5) the hit where it destroys the most: a damaged dreadnought, a cruiser...
            if (yn) {
                out = yes(true);
                return true;
            }
            const int s = g.activeSystem;
            out = one(bestBy(d, [&](int t) {
                if (t == int(UnitType::Dreadnought)) {
                    int dmg = 0;
                    for (int q = 0; q < g.numPlayers && s >= 0; ++q)
                        if (q != p) dmg += g.systems[size_t(s)].space[size_t(q)].damaged(UnitType::Dreadnought);
                    return dmg > 0 ? 4.f : 0.5f;
                }
                return t == int(UnitType::Cruiser) ? 2.f : 1.f;
            }));
            return true;
        }
        case Ab::DsParallel: out = one(spendable(g, p, false) >= 3 ? bestBy(d, [&](int s) { return s < 0 ? -1.f : 1.f + float(threatNear(g, p, s)); }) : -1); return true;
        case Ab::DsGhotiRelay: out = one(bestBy(d, [&](int s) { return s < 0 ? -1.f : 1.f + float(threatNear(g, p, s)); })); return true;
        case Ab::DsBecece: out = one(d.player == g.activePlayer || has(d, 1) ? (spendable(g, p, false) >= 4 ? 1 : 2) : -1); return true;
        case Ab::DsNmenmede: out = one(bestBy(d, [&](int c) { return c < 0 ? -1.f : float(unitCost2(g, p, UnitType(c % 16)) - unitCost2(g, p, UnitType(c / 16))); })); return true;
        case Ab::DsTioleombp: out = one(-1); return true;
        case Ab::DsNetworked:
        case Ab::DsGhotiPlanet: {
            const bool votes = d.prompt.find("votes") != std::string::npos;
            if (votes && g.currentAgenda >= 0) {
                // (0.44.6) the extra votes on a contested agenda, or on the phase's last one - not spent on the first
                // agenda whatever it is (neither is ready again before the status phase)
                float conf = 1.f;
                aiPredictOutcome(g, g.currentAgenda, &conf);
                out = yes(g.agendaNumber >= 1 || conf < 0.65f);
                return true;
            }
            out = yes(votes || d.prompt.find("influence") != std::string::npos || d.prompt.find("resources") != std::string::npos);
            return true;
        }
        default: return false;
    }
}

// Component actions (6000 + Ab): Blessing of the Queens, Spark Thrusters, Thakt Clqua, The Venerable.
bool aiDsComponentValue(const Game& g, int p, int ab, float& v) {
    if (ab < int(Ab::DsZealous)) return false;
    const Player& me = g.players[size_t(p)];
    switch (Ab(ab)) {
        case Ab::DsBlessing: {  // a command token back where our ships are needed again
            v = 0.f;
            for (size_t s = 0; s < g.systems.size(); ++s)
                if (g.systems[s].hasToken(p) && g.systems[s].space[size_t(p)].nonFighterShips() >= 2) v = std::max(v, 0.8f + 0.2f * float(threatNear(g, p, int(s))));
            return true;
        }
        case Ab::DsSpark: {  // a strategy token we can spare (3+) or a 5th relic fragment
            const int t = findTech("dslaneb");
            const bool token = t >= 0 && ownsTechKey(g, p, "dslaneb") && !me.techExhausted(t) && me.strategy >= 3;
            v = (token || fragmentCount(g, p) >= 5) && sparkBest(g, p) > 0.f ? 0.45f : 0.f;
            return true;
        }
        case Ab::DsbtCymiae: v = 0.f; return true;   // (milling the action deck: rarely worth an action)
        case Ab::DsbtBentor: v = g.round >= 3 ? 0.2f : 0.f; return true;  // (the others explore too: a foothold late, before passing)
        case Ab::DsbtEdyn: v = g.round >= 3 ? 0.6f : 0.f; return true;
        case Ab::DsbtKolume: v = me.tactic + me.fleet + me.strategy < 8 ? 1.0f : 0.6f; return true;
        case Ab::DsbtNokar: {  // 0.44.4: only with a good spot for the neutral fleet
            float best = 0.f;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                if (g.systems[s].isHyperlane() || g.systems[s].fracture || g.systems[s].offBoard) continue;
                bool ships = false;
                for (int q = 0; q < g.numPlayers && !ships; ++q) ships = hasShips(g, q, int(s));
                if (!ships) best = std::max(best, nokarSpot(g, p, int(s)));
            }
            v = g.round >= 2 && best >= 2.f ? 0.6f : 0.f;
            return true;
        }
        case Ab::DsEncryptionKey:  // 0.43.0: attaching a note is an action now - worth it before the late game
        case Ab::DsGledgeBase: v = g.round <= 6 ? 1.1f : 0.6f; return true;
        case Ab::DsVehlTikar: {
            float tired = 0;
            for (int x : controlledPlanets(g, p))
                if (g.planets[size_t(x)].exhausted) tired += float(planetResources(g, x) + planetInfluence(g, x));
            v = tired >= 8 ? 3.f : 0.f;
            return true;
        }
        case Ab::DsSaintBinal: v = g.round >= 3 ? 4.f : 0.5f; return true;
        case Ab::DsMeditation:  // (0.43.0: it also unlocks Issac of Sinci - 0 unlocks in 18 games before)
            v = me.strategy >= 2 ? (leaderStatus(g, p, kCommander) == kLocked && g.round >= 2 ? 1.1f : 0.6f) : 0.f;
            return true;
        case Ab::DsBiothermics: {  // (0.43.0) a stall instead of passing: Applied Biothermics pays for each player who
            int still = 0;         // passed before us
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && !g.players[size_t(q)].passed && !g.players[size_t(q)].eliminated) ++still;
            v = still > 0 && me.commodities < commodityCap(g, me) ? 0.35f : 0.f;
            return true;
        }
        case Ab::DsFran: {  // (0.43.0) out of tactic tokens with a spare fleet token: one more tactical action
            int fleetNeed = 0;
            for (const SystemState& s : g.systems) fleetNeed = std::max(fleetNeed, s.space[size_t(p)].nonFighterShips());
            v = me.tactic == 0 && (me.fleet > fleetNeed + 1 || me.strategy >= 2) ? 0.9f : 0.f;
            return true;
        }
        case Ab::DsWonell: v = g.round >= 4 ? 1.0f : 0.f; return true;
        case Ab::DsThakt: v = g.round >= 3 ? 6.f : 1.f; return true;
        // ---- batch 4 (0.40.0)
        case Ab::DsSeeker: v = 0.8f; return true;
        case Ab::DsGriphyn: v = g.round >= 3 ? 3.5f : 1.f; return true;
        case Ab::DsBayan: {
            int n = 0;
            for (const SystemState& s : g.systems) n += s.space[size_t(p)].nonFighterShips() ? 1 : 0;
            v = g.round >= 3 && n >= 3 ? 0.8f * float(n) : 0.f;
            return true;
        }
        case Ab::DsPutriv: {
            int n = 0;
            for (int q = 0; q < g.numPlayers; ++q) n += q != p ? g.dsDebt[q] : 0;
            v = n >= 3 ? 0.9f * float(n) : 0.f;
            return true;
        }
        case Ab::DsVoiceUnited: v = g.round >= 3 ? 2.5f : 0.5f; return true;
        case Ab::DsAlgorithmic: {
            float best = 0;
            for (int16_t c : g.actionDiscard) best = std::max(best, aiCardKeepValue(c));
            v = std::max(0.f, best - 1.f);
            return true;
        }
        case Ab::DsKwill: {
            int foe = 0, mine = 0;
            for (const SystemState& s : g.systems)
                for (int q = 0; q < g.numPlayers; ++q)
                    for (int t : kShipIdx)
                        if (s.space[size_t(q)].n[t] > s.space[size_t(q)].dmg[t] && unitSustains(g, q, UnitType(t)))
                            (q == p ? mine : foe) += s.space[size_t(q)].n[t] - s.space[size_t(q)].dmg[t];
            v = g.round >= 3 ? std::max(0.f, 0.4f * float(foe - 2 * mine)) : 0.f;
            return true;
        }
        case Ab::DsPahn: v = g.round >= 3 ? 0.25f * float(controlledPlanets(g, p).size()) : 0.f; return true;
        case Ab::DsIncite: v = 0.4f; return true;
        // ---- batch 5 (0.41.0)
        case Ab::DsCordo: v = 1.2f; return true;
        case Ab::DsOtto: v = g.round >= 4 ? 2.f : 0.f; return true;
        case Ab::DsMidir: {
            int n = 0;
            for (size_t s = 0; s < g.systems.size(); ++s)
                for (int16_t x : g.systems[s].planets) n += g.planets[size_t(x)].units[size_t(p)].count(UnitType::Mech) ? 1 : 0;
            v = g.round >= 3 ? 1.5f * float(std::min(n, 4)) : 0.f;
            return true;
        }
        case Ab::DsLocalContracts: v = spendable(g, p, false) >= 4 ? 1.f : 0.f; return true;
        case Ab::DsStarsails: v = g.round >= 3 && controlledPlanets(g, p).size() >= 5 && spendable(g, p, false) >= 6 ? 2.5f : 0.f; return true;
        case Ab::DsEmergency: {  // (0.44.5: worth it for a new dock on a good planet; not for moving one around)
            const float spot = unitsAvailable(g, p, UnitType::SpaceDock) > 0 ? bestDockSpot(g, p) : -1.f;
            v = spot >= 2.f ? 1.5f + 0.25f * std::min(spot, 4.f) : 0.f;
            return true;
        }
        case Ab::DsShipmonger: v = 0.9f; return true;
        case Ab::DsDemiQueen: v = g.round >= 3 ? 2.f : 0.f; return true;
        case Ab::DsInsight: v = 0.8f; return true;
        case Ab::DsKorela: v = g.round >= 3 ? 2.5f : 0.5f; return true;
        case Ab::DsKantrus: v = g.round >= 3 ? 3.f : 0.5f; return true;
        // ---- batch 6 (0.42.0)
        case Ab::DsAuberon: v = g.round >= 3 ? 3.f : 0.5f; return true;
        case Ab::DsKhazRin: v = g.round >= 4 ? 2.5f : 0.f; return true;
        case Ab::DsWrathMachina: v = g.round >= 3 ? 3.f : 0.f; return true;
        case Ab::DsNmenmede: v = g.round >= 3 ? 3.f : 0.f; return true;
        // ---- batch 3 (0.39.0)
        case Ab::DsMantle: {
            float best = -9.f;
            for (int x : controlledPlanets(g, p))
                if (x >= 0 && g.planets[size_t(x)].system != me.homeSystem && std::strcmp(kPlanets[g.planets[size_t(x)].def].key, "mr") &&
                    std::find(g.dsCores.begin(), g.dsCores.end(), int16_t(x)) == g.dsCores.end())
                    best = std::max(best, 2.f - float(planetResources(g, x)) - 0.6f * float(planetInfluence(g, x)));
            v = best > -9.f ? 2.5f + 0.4f * best : 0.f;
            return true;
        }
        case Ab::DsMgur: {
            int n = 0;
            for (unsigned b = me.dsFrags; b; b &= b - 1) ++n;
            v = n >= 2 ? 0.5f * float(n) : 0.f;
            return true;
        }
        case Ab::DsKenTucc: {
            int n = 0;
            for (int x : controlledPlanets(g, p))
                n += hasTraitOf(g, x, Trait::Cultural) || hasTraitOf(g, x, Trait::Hazardous) || hasTraitOf(g, x, Trait::Industrial) ? 1 : 0;
            v = n >= 5 ? 0.8f * float(n) : 0.f;
            return true;
        }
        case Ab::DsImpressAction: v = 0.9f; return true;
        case Ab::DsGorthrim: v = g.round >= 3 ? 4.f : 1.f; return true;
        case Ab::DsAtropha: v = g.round >= 2 ? 2.5f : 0.5f; return true;
        case Ab::DsYgegnad: {
            int n = 0;
            for (int16_t gl : g.dsGlory) {
                bool tok = g.systems[size_t(gl)].hasToken(p);
                for (int16_t a : g.systems[size_t(gl)].adj) tok = tok || g.systems[size_t(a)].hasToken(p);
                n += tok ? 1 : 0;
            }
            v = n >= 2 ? 1.2f * float(n) : 0.f;
            return true;
        }
        case Ab::DsBanua: {
            int n = 0;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                bool planet = false;
                for (int16_t x : g.systems[s].planets) planet = planet || g.planets[size_t(x)].owner == p;
                n += planet && !otherShipsIn(g, p, int(s)) ? 1 : 0;
            }
            v = g.round >= 3 && n >= 4 ? 0.4f * float(n) : 0.f;
            return true;
        }
        case Ab::DsVenerable: {
            int inPlay = 0;
            for (const Player& q : g.players) inPlay += int(q.relics.size());
            v = inPlay >= 2 || g.round >= 6 ? 1.f + 0.8f * float(inPlay) : 0.f;
            (void)me;
            return true;
        }
        default: v = 0.f; return true;
    }
}

}  // namespace ti
