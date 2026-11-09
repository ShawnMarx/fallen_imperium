// Galaxy setup draft (0.15.0): where a computer player puts each of its tiles.
//
// As people do at the table (owner, 2026-10-22): good planets - legendary ones
// above all - go next to your own home; asteroid fields, gravity rifts,
// supernovas and empty space go far from you and next to your rivals; nebulas
// too, unless your faction likes them. Exceptions: factions starting with
// Antimass Deflectors don't mind asteroid fields, the Embers of Muaat like
// supernovas, the Empyrean like nebulas. Added: alpha / beta wormholes next to
// your home are a back door (except for the Creuss, who live by wormholes);
// anomalies on a rival's road to Mecatol Rex slow them down, so a Rusher keeps
// its own road clear; defensive personalities (Turtler, Peacekeeper) don't
// mind a nebula next door; how much a bot spends on hurting others depends on
// its personality.
//
// Each choice is scored as "what it gives me minus (a share of) what it gives
// the others", where a tile counts for whoever is close to it. A tile placed
// now in a poor spot also loses the better spot it could get later, so the
// score is compared against the best place for that tile anywhere on the map
// (the bot keeps a legendary planet for a space next to its home).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ai/ai_internal.h"
#include "engine/map.h"
#include "engine/personality.h"
#include "engine/query.h"

namespace ti {
namespace {

bool factionIs(const Game& g, int p, const char* key) { return !std::strcmp(kFactions[g.players[p].faction].key, key); }

bool startsWith(const Game& g, int p, const char* tech) {
    const int t = findTech(tech);
    for (int x : kFactions[g.players[p].faction].startTechs)
        if (x == t && t >= 0) return true;
    return false;
}

int archOf(const Game& g, int p) { return g.players[p].pers.arch; }

// How much a tile is worth to player p when it is right next to their home.
float tileWorth(const Game& g, int p, int def) {
    const SystemDef& d = kSystems[def];
    float v = 0;
    for (int i = 0; i < d.planetCount; ++i) {
        const PlanetDef& pd = kPlanets[d.planets[i]];
        v += std::max(pd.res, pd.inf) + 0.4f * std::min(pd.res, pd.inf);
        if (pd.skip != TechColor::None) v += 0.6f;
        if (pd.legendary) v += 3.0f;  // the owner: legendary planets go near home
        v += 0.3f;                     // another planet to explore and hold
    }
    const int arch = archOf(g, p);
    const bool defensive = arch == int(Arch::Turtler) || arch == int(Arch::Peacekeeper);
    if (d.anomalies & kAsteroid) v += startsWith(g, p, "amd") ? 0.3f : -1.6f;
    if (d.anomalies & kRift) v -= 1.2f;
    if (d.anomalies & kSupernova) v += factionIs(g, p, "muaat") ? 1.2f : -1.7f;
    if (d.anomalies & kNebula) v += factionIs(g, p, "empyrean") ? 1.6f : defensive ? 0.3f : -0.6f;
    if (d.planetCount == 0 && d.anomalies == 0 && (d.wormholes & (kAlpha | kBeta)) == 0) v -= 0.9f;  // empty space
    if (d.wormholes & (kAlpha | kBeta)) v += factionIs(g, p, "ghost") ? 1.5f : -1.0f;  // a back door into your home
    return v;
}

// How much of that a player gets from a space this far from their home.
float nearness(int dist) {
    static const float k[5] = {1.4f, 1.0f, 0.5f, 0.2f, 0.05f};
    return k[std::max(0, std::min(4, dist))];
}

struct DraftView {
    std::vector<Hex> home;  // per player (the Creuss: their gate)
    Hex mecatol{0, 0};
};

DraftView viewOf(const Game& g) {
    DraftView v;
    v.home.assign(size_t(g.numPlayers), Hex{0, 0});
    for (int p = 0; p < g.numPlayers; ++p) {
        int hs = g.players[p].homeSystem;
        if (hs >= 0) {
            const SystemState& s = g.systems[size_t(hs)];
            v.home[size_t(p)] = s.pos;
            if (s.offBoard)  // the Creuss / the Crimson Rebellion: the gate / the Sorrow on the board next to it
                for (size_t i = 0; i < g.systems.size(); ++i)
                    if (!g.systems[i].offBoard && hexDistance(g.systems[i].pos, s.pos) == 1 &&
                        (!std::strcmp(g.systems[i].d().key, "17") || !std::strcmp(g.systems[i].d().key, "94")))
                        v.home[size_t(p)] = g.systems[i].pos;
        }
    }
    return v;
}

// The tile in this space as seen by player q: worth to them, scaled by how
// close it is; anomalies on their road to Mecatol Rex weigh more.
float effectFor(const Game& g, const DraftView& v, int q, int def, Hex at) {
    const Hex home = v.home[size_t(q)];
    const int dist = hexDistance(home, at);
    float w = tileWorth(g, q, def) * nearness(dist);
    const bool onRoad = hexDistance(home, at) + hexDistance(at, v.mecatol) == hexDistance(home, v.mecatol);
    if (onRoad && kSystems[def].anomalies && w < 0) w *= archOf(g, q) == int(Arch::Rusher) ? 1.9f : 1.4f;
    return w;
}

// How much p cares about hurting the others (and helping them by mistake).
float spiteOf(const Game& g, int p) {
    switch (Arch(archOf(g, p))) {
        case Arch::Warlord:
        case Arch::Rusher:
        case Arch::Betrayer: return 0.8f;
        case Arch::Opportunist:
        case Arch::Swindler: return 0.65f;
        case Arch::Peacekeeper:
        case Arch::Politician: return 0.4f;
        case Arch::Merchant: return 0.5f;
        default: return 0.55f;
    }
}

float placementScore(const Game& g, const DraftView& v, int p, int def, Hex at) {
    float s = effectFor(g, v, p, def, at);
    const float spite = spiteOf(g, p);
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p) s -= spite * effectFor(g, v, q, def, at);
    return s;
}

}  // namespace

bool aiGalaxyAnswer(const Game& g, const Decision& d, Rng& rng, Answer& out) {
    if (d.tag != Tag::GalaxyPlace) return false;
    const int p = d.player;
    const DraftView v = viewOf(g);
    // best place for each tile anywhere among the open spaces (now or later)
    std::vector<std::pair<int, float>> bestLater;
    auto laterOf = [&](int def) {
        for (const auto& b : bestLater)
            if (b.first == def) return b.second;
        float best = -1e9f;
        for (size_t c = 0; c < g.draft.cells.size(); ++c)
            if (g.draft.cells[c].system < 0) best = std::max(best, placementScore(g, v, p, def, g.draft.cells[c].pos));
        bestLater.push_back({def, best});
        return best;
    };
    // easier bots judge less sharply
    static const float kNoise[4] = {2.0f, 0.9f, 0.3f, 0.f};
    const float noise = kNoise[std::max(0, std::min(3, int(aiLevelOf(g))))];
    int bestId = -1;
    float bestV = -1e9f;
    for (const Option& o : d.options) {
        if (!o.enabled) continue;
        const int def = o.id / 1000, cell = o.id % 1000;
        if (cell < 0 || cell >= int(g.draft.cells.size())) continue;
        const float now = placementScore(g, v, p, def, g.draft.cells[size_t(cell)].pos);
        // a better space later isn't certain - but a legendary planet is worth waiting for
        bool legendary = false;
        for (int i = 0; i < kSystems[def].planetCount; ++i) legendary |= kPlanets[kSystems[def].planets[i]].legendary;
        const float later = laterOf(def) * (legendary ? 0.95f : 0.8f);
        float val = now - std::max(0.f, later - now);
        if (noise > 0) val += noise * (float(rng.below(1000)) / 1000.f - 0.5f);
        if (val > bestV) {
            bestV = val;
            bestId = o.id;
        }
    }
    if (bestId < 0)
        for (const Option& o : d.options)
            if (o.enabled) {
                bestId = o.id;
                break;
            }
    if (std::getenv("TI4_DRAFTDEBUG") && bestId >= 0) {
        const Hex at = g.draft.cells[size_t(bestId % 1000)].pos;
        std::fprintf(stderr, "draft p%d %s -> ring %d dist-home %d (%s); value %.2f\n", p, kSystems[bestId / 1000].name,
                     int(g.draft.cells[size_t(bestId % 1000)].ring), hexDistance(at, v.home[size_t(p)]), kFactions[g.players[p].faction].key, bestV);
    }
    out = Answer();
    out.picks.push_back(bestId);
    return true;
}

}  // namespace ti
