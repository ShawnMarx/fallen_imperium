// The seats of power (0.44.2, a tester: "I'm leaving Mecatol on purpose untouched in all my games at least till mid
// round 3 ... nobody takes it"; the owner: when someone leaves Mecatol open, everyone should try for it - "an attack of
// opportunity"). Mecatol Rex (once the custodians are gone) and Styx (Thunder's Edge: 1 victory point while held) are
// seats: the bots stage for the custodians, hold a seat they own with ships, and go for one its holder left open.
// The values themselves are added in ai.cpp (buildPlan, planetValue, the guards) and ai_goals.cpp (the 6 influence).
#include <cstring>

#include "ai/ai_internal.h"
#include "engine/query.h"

namespace ti {

namespace {
bool isKey(const Game& g, int pl, const char* k) { return !std::strcmp(kPlanets[size_t(g.planets[size_t(pl)].def)].key, k); }
}  // namespace

bool aiSeatPlanet(const Game& g, int pl) {
    if (pl < 0 || pl >= int(g.planets.size()) || g.planets[size_t(pl)].destroyed) return false;
    if (isKey(g, pl, "styx")) return true;
    return isKey(g, pl, "mr") && g.custodiansRemoved;
}

bool aiSeatOpen(const Game& g, int pl) {
    if (!aiSeatPlanet(g, pl)) return false;
    const PlanetState& ps = g.planets[size_t(pl)];
    if (ps.owner < 0) return groundForcesOn(g, kNeutral, pl) == 0;
    const int q = ps.owner, sys = ps.system;
    return g.systems[size_t(sys)].space[size_t(q)].nonFighterShips() == 0 && ps.units[size_t(q)].groundForces() <= 1;
}

bool aiSeatHeldIn(const Game& g, int p, int sys) {
    if (sys < 0 || sys >= int(g.systems.size())) return false;
    for (int16_t x : g.systems[size_t(sys)].planets)
        if (g.planets[size_t(x)].owner == p && aiSeatPlanet(g, x)) return true;
    return false;
}

int aiStyxPlanet(const Game& g) {
    for (size_t x = 0; x < g.planets.size(); ++x)
        if (!g.planets[x].destroyed && isKey(g, int(x), "styx")) return int(x);
    return -1;
}

bool aiMecatolStaged(const Game& g, int p) {
    const int mec = g.mecatol();
    if (mec < 0) return false;
    auto loaded = [&](int s) {
        const Force& f = g.systems[size_t(s)].space[size_t(p)];
        if (f.groundForces() == 0) return false;
        for (int t : kShipIdx)
            if (f.n[t] && unitDef(g, p, UnitType(t)).capacity > 0) return true;
        return false;
    };
    if (loaded(mec)) return true;
    for (int16_t a : g.systems[size_t(mec)].adj)
        if (loaded(a)) return true;
    return false;
}

}  // namespace ti
