// Computer players and Thunder's Edge's own questions (0.34.0): the expedition to Thunder's Edge, placing it and
// the ingress tokens, and the breakthroughs' choices (Ab::Expedition .. Ab::Bt*). Called from aiFactionAnswer and
// aiFactionComponentValue. Rules of thumb like the rest of the AI; the engine checks every answer.
#include <algorithm>
#include <cstring>

#include "ai/ai.h"
#include "ai/ai_internal.h"
#include "engine/deal.h"
#include "engine/personality.h"
#include "engine/query.h"

namespace ti {

namespace {
Answer one(int id) {
    Answer a;
    a.picks.push_back(id);
    return a;
}
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
bool promisedNoAttack(const Game& g, int p, int q) {  // a no-attack pact either way
    for (const Promise* pr : activePromises(g, p, q))
        if (pr->kind == PromiseKind::NoAttack) return true;
    return false;
}
// the Firmament's traits stay with the Obsidian
bool firmQuirk(const Game& g, int p, int which) { return fquirkIs(g, p, "firmament", which) || fquirkIs(g, p, "obsidian", which); }
int firstReal(const Decision& d) {
    for (const Option& o : d.options)
        if (o.enabled && o.id >= 0) return o.id;
    return d.options.empty() ? -1 : d.options[0].id;
}

// moves from p's home to each system (plain adjacency; 99 = out of reach)
std::vector<int> fromHome(const Game& g, int p) {
    std::vector<int> d(g.systems.size(), 99);
    const int h = g.players[size_t(p)].homeSystem;
    if (h < 0) return d;
    std::vector<int> q{h};
    d[size_t(h)] = 0;
    for (size_t i = 0; i < q.size(); ++i)
        for (int16_t n : g.systems[size_t(q[i])].adj)
            if (d[size_t(n)] > d[size_t(q[i])] + 1) {
                d[size_t(n)] = d[size_t(q[i])] + 1;
                q.push_back(n);
            }
    return d;
}

int slicesOf(const Game& g, int p) {
    int n = 0;
    for (int8_t o : g.expedition) n += o == p ? 1 : 0;
    return n;
}
int slicesClaimed(const Game& g) {
    int n = 0;
    for (int8_t o : g.expedition) n += o >= 0 ? 1 : 0;
    return n;
}

float cheapestCards(const Game& g, int p, int k) {  // what giving up the k least-liked action cards costs
    std::vector<float> v;
    for (int16_t c : g.players[size_t(p)].actionCards) v.push_back(aiCardKeepValue(c));
    std::sort(v.begin(), v.end());
    float s = 0;
    for (int i = 0; i < k && i < int(v.size()); ++i) s += v[size_t(i)];
    return s;
}

// What a slice of the expedition costs this player, in the planner's units (about 1 per resource).
float sliceCost(const Game& g, int p, int s) {
    const Player& me = g.players[size_t(p)];
    switch (s) {
        case 0: return aiFreeSpend(g, p, false) >= 5 ? 4.5f : 99.f;
        case 1: return aiFreeSpend(g, p, true) >= 5 ? (g.round <= 2 ? 3.5f : 4.5f) : 99.f;
        case 2: return me.actionCards.size() >= 2 ? 1.2f + cheapestCards(g, p, 2) : 99.f;
        case 3: {
            if (me.secrets.empty()) return 99.f;
            int worst = 0;  // the secret furthest from done
            for (int16_t o : me.secrets) worst = std::max(worst, objectiveProgress(g, p, o).missing());
            return worst >= 3 ? 3.f : worst == 2 ? 5.f : 8.f;
        }
        case 4: {
            float best = 99.f;
            for (int x : readyPlanets(g, p))
                if (planetHasSkip(g, x)) best = std::min(best, 0.8f + 0.5f * float(planetResources(g, x) + planetInfluence(g, x)));
            return best;
        }
        case 5: return me.tradeGoods >= 3 + aiGoals(g, p).keepTg ? 3.2f : 99.f;
    }
    return 99.f;
}

// What another slice is worth: the first one gains the breakthrough; later ones count for who places infantry on
// Thunder's Edge (a legendary planet) when the sixth is claimed.
float sliceWorth(const Game& g, int p) {
    const Player& me = g.players[size_t(p)];
    if (!me.breakthrough) return aiCoreExpeditionLater(g, p) ? 3.5f : 9.f;  // 0.43.0: the guide's round
    const int mine = slicesOf(g, p), all = slicesClaimed(g);
    int rival = 0;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p) rival = std::max(rival, slicesOf(g, q));
    float v = 1.2f;
    if (mine + 1 >= rival) v += 1.5f;      // we'd lead (or tie) the expedition
    if (all == 5) v += 1.5f;               // the last slice: we choose where Thunder's Edge goes
    if (g.round >= 5) v *= 0.6f;           // late: the planet comes too late to matter much
    return v;
}
// Ahk Ravin (Crimson agent, 0.43.0 debug passes - owner T33): our weak ship at the front trades places with a strong
// one in a quiet system (ids system * 16 + type); the gain in fighting worth, 0 if there's none worth an agent
int threatAround(const Game& g, int p, int s) {
    int n = 0;
    std::vector<int> zone{s};
    for (int16_t a : g.systems[size_t(s)].adj) zone.push_back(a);
    for (int z : zone)
        for (int q = 0; q < g.numPlayers; ++q)
            if (q != p) n += g.systems[size_t(z)].space[size_t(q)].nonFighterShips();
    return n;
}
float fightWorth(const Game& g, int p, int t) {
    const UnitDef& u = unitDef(g, p, UnitType(t));
    if (u.combat <= 0) return 0.f;
    return float(std::max(0, 11 - u.combat)) * float(std::max<int>(1, u.combatDice)) + (u.sustain ? 3.f : 0.f);
}
float ravinPair(const Game& g, int p, int* front, int* back) {
    float best = 0.f;
    for (size_t f = 0; f < g.systems.size(); ++f) {
        const Force& ff = g.systems[f].space[size_t(p)];
        if (!ff.nonFighterShips() || threatAround(g, p, int(f)) == 0) continue;
        for (size_t b = 0; b < g.systems.size(); ++b) {
            if (b == f) continue;
            const Force& fb = g.systems[b].space[size_t(p)];
            if (!fb.nonFighterShips() || threatAround(g, p, int(b)) > 0) continue;
            for (int tw : kShipIdx) {
                if (!ff.n[tw] || tw == int(UnitType::Fighter) || tw == int(UnitType::Carrier)) continue;
                for (int ts : kShipIdx) {
                    if (!fb.n[ts] || ts == int(UnitType::Fighter) || ts == int(UnitType::Carrier)) continue;
                    const float gain = fightWorth(g, p, ts) - fightWorth(g, p, tw);
                    if (gain > best) {
                        best = gain;
                        if (front) *front = int(f) * 16 + tw;
                        if (back) *back = int(b) * 16 + ts;
                    }
                }
            }
        }
    }
    return best;
}
}  // namespace

// Vaults of the Heir (0.44.4): how much Mahact would miss an owned technology - its own worth, unit upgrades (the unit
// goes back), its colour while objectives want that colour or few of it are owned (prerequisites)
static float mahactKeep(const Game& g, int p, int t, const Goals& go) {
    const TechDef& d = kTechs[size_t(t)];
    float k = aiTechWant(g, p, t);
    if (d.unitUpgrade) k += 2.f + go.upgrade;
    if (!d.unitUpgrade && d.color != TechColor::None) {
        const int c = int(d.color);
        k += go.color[c] * 0.5f;
        if (techCount(g, p, d.color) <= 2) k += 1.f;
    }
    if (d.faction >= 0) k += 1.f;
    return k;
}

bool aiTeAnswer(const Game& g, const Decision& d, Answer& out) {
    const int p = d.player;
    const Player& me = g.players[size_t(p)];
    switch (Ab(d.ability)) {
        case Ab::Expedition: {
            int best = -1;
            float margin = 0.f;
            const float worth = sliceWorth(g, p);
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                const float m = worth - sliceCost(g, p, o.id) + aiCoreSliceBias(g, p, o.id);
                if (m > margin) margin = m, best = o.id;
            }
            out = one(has(d, best) ? best : -1);
            return true;
        }
        case Ab::ExpeditionPay: {
            if (d.prompt.find("action cards") != std::string::npos) {  // the 2 least-liked cards
                std::vector<std::pair<float, int>> v;
                for (const Option& o : d.options)
                    if (o.enabled && o.id >= 0 && o.id < int(me.actionCards.size()))
                        v.push_back({aiCardKeepValue(me.actionCards[size_t(o.id)]), o.id});
                std::sort(v.begin(), v.end());
                Answer a;
                for (size_t i = 0; i < v.size() && int(a.picks.size()) < std::max(1, d.maxPick); ++i) a.picks.push_back(v[i].second);
                out = a;
                return true;
            }
            if (d.prompt.find("secret") != std::string::npos) {  // the one furthest from done
                out = one(bestBy(d, [&](int o) { return float(objectiveProgress(g, p, o).missing()); }));
                return true;
            }
            out = one(bestBy(d, [&](int x) { return -float(planetResources(g, x) + planetInfluence(g, x)); }));  // the poorest planet
            return true;
        }
        case Ab::TePlace: {  // near our home and our fleets, far from the leader's
            const std::vector<int> mine = fromHome(g, p);
            int leader = -1;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && (leader < 0 || g.players[size_t(q)].vp > g.players[size_t(leader)].vp)) leader = q;
            const std::vector<int> theirs = leader >= 0 ? fromHome(g, leader) : std::vector<int>(g.systems.size(), 99);
            out = one(bestBy(d, [&](int s) {
                float v = -float(mine[size_t(s)]) * 2.f + float(std::min(theirs[size_t(s)], 6)) * 0.6f;
                if (hasShips(g, p, s)) v += 2.f;
                for (int16_t a : g.systems[size_t(s)].adj)
                    if (hasShips(g, p, a)) v += 0.8f;
                return v;
            }));
            return true;
        }
        case Ab::TeInfantry:  // ourselves when tied, else the weakest player
            if (has(d, p)) {
                out = one(p);
            } else {
                out = one(bestBy(d, [&](int q) { return -float(g.players[size_t(q)].vp); }));
            }
            return true;
        case Ab::IngressPlace:
        case Ab::BtCabal: {  // ingress tokens open the Fracture: near us
            const std::vector<int> mine = fromHome(g, p);
            if (Ab(d.ability) == Ab::BtCabal && d.prompt.find("move an ingress") != std::string::npos) {
                out = one(bestBy(d, [&](int s) { return float(mine[size_t(s)]); }));  // the one furthest from us moves
                if (!has(d, out.picks[0])) out = one(-1);
                return true;
            }
            out = one(bestBy(d, [&](int s) { return -float(mine[size_t(s)]); }));
            return true;
        }
        case Ab::BtArborec: {  // Psychospore: the token where our ships could still want to move from
            out = one(bestBy(d, [&](int s) {
                float v = float(g.systems[size_t(s)].space[size_t(p)].nonFighterShips()) * 1.5f;
                for (int16_t a : g.systems[size_t(s)].adj)
                    if (otherShipsIn(g, p, a)) v += 1.f;
                return v;
            }));
            return true;
        }
        case Ab::BtMahact: {  // Vaults of the Heir: the technology we'd miss least
            const Goals& go = aiGoals(g, p);
            out = one(bestBy(d, [&](int t) { return t < 0 ? -50.f : -mahactKeep(g, p, t, go); }));
            return true;
        }
        case Ab::BtSaar: {
            if (d.prompt.find("how many") != std::string::npos) {  // ~1.5 dice per defending ground force, within spare resources
                int target = 0;
                for (const Option& o : d.options) target = std::max(target, o.id);
                int gf = 1;  // (0.44.4: Decision::value = the planet)
                if (d.value > 0 && d.value < int(g.planets.size())) {
                    gf = 0;
                    for (int q = 0; q < kSides; ++q)
                        if (q != p) gf += g.planets[size_t(d.value)].units[size_t(q)].groundForces();
                }
                const int free = std::max(1, aiFreeSpend(g, p, false));
                const int want = std::max(1, std::min({std::max(2, (gf * 3 + 1) / 2), free, target}));
                out = one(has(d, want) ? want : firstReal(d));
                return true;
            }
            out = one(bestBy(d, [&](int x) {  // valuable planets with few defenders
                int gf = 0;
                for (int q = 0; q < kSides; ++q)
                    if (q != p) gf += g.planets[size_t(x)].units[size_t(q)].groundForces();
                float v = float(planetResources(g, x) + planetInfluence(g, x)) - float(gf) * 0.7f;
                for (int16_t a : g.systems[size_t(g.planets[size_t(x)].system)].adj)
                    if (hasShips(g, p, a)) v += 1.f;  // we could follow it up
                return v;
            }));
            return true;
        }
        case Ab::BtKeleres: {  // 0.44.4: Custodia Vigilia toward a payment worth it (value = amount needed), not any small one
            const int cv = std::max(custodiaValue(g, p, d.prompt.find("influence") != std::string::npos), 1);
            const bool influence = d.prompt.find("influence") != std::string::npos;
            const int need = d.value;
            out = one(need >= cv - 1 || aiFreeSpend(g, p, influence) < need ? 1 : 0);
            return true;
        }
        case Ab::BtSardakk: {  // N'orr Supremacy: a good unit upgrade beats a token
            if (has(d, 1)) {  // (0.43.4: "research a unit upgrade" - the research step picks which)
                float best = 0.f;
                for (int t = 0; t < kTechCount; ++t)
                    if (kTechs[t].unitUpgrade && techAvailableToResearch(g, p, t) && canResearchNow(g, p, t))
                        best = std::max(best, aiTechWant(g, p, t));
                out = one(best > 2.5f ? 1 : 0);
                return true;
            }
            const int t = bestBy(d, [&](int id) { return id >= 1000 ? aiTechWant(g, p, id - 1000) : 2.5f; });
            out = one(has(d, t) ? t : 0);
            return true;
        }
        case Ab::BtNomad: {  // Thunder's Paradox: only to cash in Artuno's stored trade goods
            int pick = -1;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0 || o.id >= 10000) continue;  // (0.44.4: not other players' agents)
                const int b = o.id % 16;
                if (b < int(me.leaders.size()) && std::strstr(kLeaders[me.leaders[size_t(b)].leader].key, "nomadagentartuno") &&
                    me.agentTG >= 2)
                    pick = o.id;
            }
            out = one(pick);
            return true;
        }
        case Ab::BtJolnar: {  // Specialist Compounds: a cheap planet instead of 4 / 6 resources
            const int cost = d.value > 0 ? d.value : 4;  // (0.44.4: the cost comes in Decision::value)
            int best = -1;
            float bv = 0.f;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                const int x = o.id / 4, c = o.id % 4;
                float want = 0.f;  // the best technology of that colour we could get
                for (int t = 0; t < kTechCount; ++t)
                    if (kTechs[t].color == TechColor(c) && !kTechs[t].unitUpgrade && canResearchNow(g, p, t))
                        want = std::max(want, aiTechWant(g, p, t));
                const float v = want + float(cost) - float(planetResources(g, x)) * 1.2f;
                if (v > bv) bv = v, best = o.id;
            }
            out = one(has(d, best) ? best : -1);
            return true;
        }
        case Ab::BtNaalu: {  // Mindsieve: give a cheap note, keep the token when tokens are plentiful
            // (never Support for the Throne or an Alliance: that's a point / our commander for them)
            const int n = bestBy(d, [&](int id) {
                const int t = noteType(id);
                return t == kNoteSupportThrone || t == kNoteAlliance ? -50.f : -noteDefaultValue(g, id);
            });
            const float nv = has(d, n) ? noteDefaultValue(g, n) : 99.f;
            const bool bad = !has(d, n) || noteType(n) == kNoteSupportThrone || noteType(n) == kNoteAlliance;
            const bool give = !bad && (nv <= 1.f || (me.strategy <= 0 && nv <= 2.f));
            out = one(give ? n : -1);
            return true;
        }
        case Ab::BtMuaat: {
            if (d.options.size() == 2 && has(d, 1) && has(d, 0)) {  // carry Avernus along: only into a quiet system
                const int s = g.activeSystem;
                out = one(s >= 0 && !otherShipsIn(g, p, s) ? 1 : 0);
                return true;
            }
            if (d.prompt.find("place what") != std::string::npos) {  // a destroyer where enemies are near, else fighters
                bool threat = false;
                for (size_t s = 0; s < g.systems.size(); ++s)
                    if (g.systems[s].space[size_t(p)].count(UnitType::WarSun))
                        for (int16_t a : g.systems[s].adj) threat = threat || otherShipsIn(g, p, a);
                const int t = threat && has(d, int(UnitType::Destroyer)) ? int(UnitType::Destroyer) : firstReal(d);
                out = one(t);
                return true;
            }
            if (d.prompt.find("Avernus planet") != std::string::npos) {  // near home, away from enemy fleets
                const std::vector<int> mine = fromHome(g, p);
                out = one(bestBy(d, [&](int s) {
                    float v = -float(mine[size_t(s)]);
                    for (int16_t a : g.systems[size_t(s)].adj)
                        if (otherShipsIn(g, p, a)) v -= 1.5f;
                    return v;
                }));
                return true;
            }
            break;
        }
        case Ab::BtTitans: {
            // (0.44.9) asked to let the Titans place a sleeper on our planet: only for a friend (Awaken can put their PDS there)
            if (d.value < 0 && d.targetPlayer >= 0 && d.targetPlayer < g.numPlayers) {
                const int t = d.targetPlayer;
                out = one(attitudeScore(g, p, t) < 9.f && !hostileFlag(g, p, t) && g.players[size_t(t)].vp + 2 < g.vpGoal ? 1 : 0);
                return true;
            }
            if (d.prompt.find("ask to place a sleeper") != std::string::npos) {  // (we never ask - the bots don't use it)
                out = one(-1);
                return true;
            }
            // coexist: two more action cards a round beats a fight on our own sleeper planet - unless we'd clearly
            int mine = 0, theirs = 0;  // win a planet worth having (0.44.4)
            if (d.value > 0 && d.value < int(g.planets.size())) {
                const PlanetState& ps = g.planets[size_t(d.value)];
                mine = ps.units[size_t(p)].groundForces();
                for (int q = 0; q < kSides; ++q)
                    if (q != p) theirs += ps.units[size_t(q)].groundForces();
            }
            const bool sure = mine >= 2 * theirs + 1 && mine >= 2;
            out = one(sure && d.value > 0 && planetResources(g, d.value) + planetInfluence(g, d.value) >= 3 ? 0 : 1);
            return true;
        }
        case Ab::BtNaaz:
            out = one(firstReal(d));
            return true;
        case Ab::BtEmpyrean: {  // tether the border toward the strongest enemy fleet
            auto sideV = [&](int s) {
                float v = 0.f;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) {
                        v += float(g.systems[size_t(s)].space[size_t(q)].nonFighterShips());
                        for (int16_t a : g.systems[size_t(s)].adj) v += 0.5f * float(g.systems[size_t(a)].space[size_t(q)].nonFighterShips());
                    }
                return v;
            };
            int best = -1;
            float bv = 0.5f;
            if (g.tethers.size() >= 2)  // 0.44.4: moving the older tether - only for a better border
                bv = std::max(bv, std::max(sideV(g.tethers[0].first), sideV(g.tethers[0].second)) + 0.5f);
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                const float v = sideV(o.id);
                if (v > bv) bv = v, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::CrimsonAgent: {  // Ahk Ravin: we swap our own ships (see ravinPair)
            int front = -1, back = -1;
            ravinPair(g, p, &front, &back);
            if (d.prompt.find("which player") != std::string::npos) out = one(has(d, p) ? p : firstReal(d));
            else if (d.prompt.find("first ship") != std::string::npos) out = one(has(d, front) ? front : firstReal(d));
            else out = one(has(d, back) ? back : firstReal(d));
            return true;
        }
        case Ab::Sever: {  // the system an enemy fleet could reach through its wormholes
            int best = -1, bv = 0;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                const uint8_t w = wormholesOf(g, o.id);
                int n = 0;
                for (size_t e = 0; e < g.systems.size(); ++e)
                    if (int(e) != o.id && (wormholesOf(g, int(e)) & w))
                        for (int q = 0; q < g.numPlayers; ++q)
                            if (q != p) n += g.systems[e].space[size_t(q)].nonFighterShips();
                if (n > bv) bv = n, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::BtArgent: {  // Wing Transfer (0.43.0 debug passes, owner T33: "have them use it")
            // enemy warships in or next to a system
            auto threat = [&](int s) {
                int n = 0;
                std::vector<int> zone{s};
                for (int16_t a : g.systems[size_t(s)].adj) zone.push_back(a);
                for (int z : zone)
                    for (int q = 0; q < g.numPlayers; ++q)
                        if (q != p) n += g.systems[size_t(z)].space[size_t(q)].nonFighterShips();
                return n;
            };
            const int act = g.activeSystem;
            out = Answer();
            if (d.prompt.find("move a ship") != std::string::npos) {
                // spare warships from the active system out to a threatened neighbour that holds fewer than the threat
                int best = -1;
                float bv = 0.f;
                for (const Option& o : d.options) {
                    if (!o.enabled || o.id < 0) continue;
                    const int a = o.id / 100000, b = (o.id % 100000) / 16, t = o.id % 16;
                    if (a != act || t == int(UnitType::Fighter) || t == int(UnitType::Carrier) || t == int(UnitType::WarSun)) continue;
                    const int here = g.systems[size_t(a)].space[size_t(p)].nonFighterShips();
                    const int there = g.systems[size_t(b)].space[size_t(p)].nonFighterShips();
                    const float v = float(threat(b) - there) - float(std::max(0, threat(a) + 1 - (here - 1)));
                    if (v > bv) bv = v, best = o.id;
                }
                out.picks.push_back(best);
                return true;
            }
            // the tokens: neighbours more exposed than the active system (they're ours alone; we won't need to
            // activate them to defend them)
            // (0.44.4: never where our own ships wait - a token there would hold them back from this move)
            if (act >= 0 && g.systems[size_t(act)].space[size_t(p)].nonFighterShips() >= 2)
                for (const Option& o : d.options)
                    if (o.enabled && o.id >= 0 && threat(o.id) > threat(act) && g.systems[size_t(o.id)].space[size_t(p)].nonFighterShips() == 0 &&
                        int(out.picks.size()) < std::min(2, d.maxPick))
                        out.picks.push_back(o.id);
            if (out.picks.empty() && d.minPick > 0) out.picks.push_back(firstReal(d));
            return true;
        }
        // ---- Thunder's Edge factions (0.35.0)
        case Ab::Galvanize: {  // the unit that rolls best: big ships first, then mechs, then the rest
            static const float kRank[kUnitTypes] = {3, 4, 2, 5, 1, 6, 1.5f, 2.5f, 0, 0, 0, 6};  // carrier .. space dock order below
            (void)kRank;
            out = one(bestBy(d, [&](int id) {
                const int t = id % 16;
                const int combat = unitDef(g, d.targetPlayer >= 0 ? d.targetPlayer : p, UnitType(t)).combat;
                return float(11 - std::max(1, combat)) + (UnitType(t) == UnitType::Fighter ? -3.f : 0.f);
            }));
            return true;
        }
        case Ab::PhoenixStandard:
        case Ab::RaiseStandard:
        case Ab::Hyperion:
            out = one(1);
            return true;
        case Ab::BastionAgent: {  // our own units, or a friend's against our enemies
            const int q = d.targetPlayer;
            out = one(q < 0 || q == p ? 1 : 0);
            return true;
        }
        case Ab::BastionHero: {  // worth it against a crowd
            int foes = 0;
            const int sys = g.activeSystem;
            if (sys >= 0)
                for (int q = 0; q < kSides; ++q)
                    if (q != p) {
                        foes += g.systems[size_t(sys)].space[size_t(q)].total();
                        for (int16_t x : g.systems[size_t(sys)].planets) foes += g.planets[size_t(x)].units[size_t(q)].total();
                    }
            out = one(foes >= 3 ? 1 : 0);
            return true;
        }
        case Ab::Proxima: {  // (0.44.8) with hit points to spare, or when our galvanized units soak up the backlash
            const int x = g.groundCombatPlanet;
            int mine = 0, theirs = 0, galv = 0;
            if (x >= 0)
                for (int q = 0; q < kSides; ++q) {
                    const Force& f = g.planets[size_t(x)].units[size_t(q)];
                    // hit points: mechs (Sustain Damage) count twice
                    const int hp = groundForcesOn(g, q, x) + f.count(UnitType::Mech) - f.dmg[int(UnitType::Mech)];
                    (q == p ? mine : theirs) += hp;
                    if (q == p)
                        for (int t = 0; t < kUnitTypes; ++t) galv += f.galv[t];
                }
            // expected backlash hits after the cancel: 3 dice at 8 = 0.9; 1 cancelled -> 0.24, 2+ -> 0.03
            const float self = g.fixes < 12 || galv == 0 ? 0.9f : galv == 1 ? 0.24f : 0.03f;
            out = one(theirs > 0 && (mine >= theirs + 2 || (self < 0.5f && mine >= 2)) ? 1 : 0);
            return true;
        }
        case Ab::Nano1: {  // (0.44.8) the PDS where it shields or fires best - not on a home that already has some
            int best = -1;
            float bv = -1e9f;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0 || o.id >= int(g.planets.size())) continue;
                const PlanetState& ps = g.planets[size_t(o.id)];
                const int sys = ps.system;
                float v = 0.3f * float(planetResources(g, o.id) + planetInfluence(g, o.id));
                const int have = ps.units[size_t(p)].count(UnitType::PDS);
                if (have == 0) v += 2.f;  // Planetary Shield
                if (isHomePlanet(g, o.id)) v -= have > 0 ? 3.f : 1.f;
                int foes = 0;
                for (int16_t a : g.systems[size_t(sys)].adj)
                    if (otherShipsIn(g, p, a)) ++foes;
                if (otherShipsIn(g, p, sys)) ++foes;
                v += 1.2f * float(std::min(foes, 3));
                if (std::strcmp(kPlanets[ps.def].key, "mr") == 0) v += 1.5f;
                if (v > bv) bv = v, best = o.id;
            }
            out = one(best >= 0 ? best : firstReal(d));
            return true;
        }
        case Ab::BtBastion: {  // The Icon: to the front
            int best = -1;
            float bv = 0.5f;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                float v = 0.f;
                for (int16_t a : g.systems[size_t(o.id)].adj)
                    if (otherShipsIn(g, p, a)) v += 1.f;
                if (v > bv) bv = v, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::SurvivalInstinct: {  // the hardest-hitting ship, unless its own system is under threat
            int best = -1;
            float bv = 0.f;
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                const int s = o.id / 16, t = o.id % 16;
                if (UnitType(t) == UnitType::Fighter) continue;
                bool threat = false;
                for (int16_t a : g.systems[size_t(s)].adj)
                    if (a != g.activeSystem && otherShipsIn(g, p, a)) threat = true;
                if (threat && g.systems[size_t(s)].space[size_t(p)].nonFighterShips() <= 1) continue;
                const float v = float(11 - std::max(1, int(unitDef(g, p, UnitType(t)).combat))) * float(std::max(1, int(unitDef(g, p, UnitType(t)).combatDice)));
                if (v > bv) bv = v, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::LastDispatch:
            out = one(bestBy(d, [&](int t) { return float(unitDef(g, p, UnitType(t)).cost2); }));
            return true;
        case Ab::RalNelCommander:
            out = one(-1);
            return true;
        case Ab::RalNelHero:
            out = one(1);
            return true;
        case Ab::BtRalNel:
            out = one(bestBy(d, [&](int c) { return aiCardKeepValue(c); }));
            return true;
        case Ab::RalNelAgent: {
            if (d.prompt.find("to whom") != std::string::npos) {  // the player furthest behind
                out = one(bestBy(d, [&](int q) { return -float(g.players[size_t(q)].vp) + (q == p ? -99.f : 0.f); }));
                return true;
            }
            out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); }));  // give the weaker card
            return true;
        }
        case Ab::Nano3:
            out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); }));
            return true;
        case Ab::ResearchTeam:
        case Ab::FirmamentMech: {  // coexist unless we clearly win the fight on that planet
            int x = g.groundCombatPlanet;
            const int other = d.targetPlayer;
            if (x < 0 && g.activeSystem >= 0)
                for (int16_t y : g.systems[size_t(g.activeSystem)].planets)
                    if (other >= 0 && groundForcesOn(g, other, y) > 0 && groundForcesOn(g, p, y) > 0) x = y;
            int mine = 0, theirs = 0;
            if (x >= 0 && other >= 0) {
                mine = groundForcesOn(g, p, x);
                theirs = groundForcesOn(g, other, x);
            }
            if (fquirkIs(g, p, "deepwrought", 0)) out = one(mine >= theirs * 3 + 3 ? 0 : 1);  // Visiting Scholar
            else out = one(mine >= theirs * 2 + 2 ? 0 : 1);
            return true;
        }
        case Ab::DeepwroughtAgent: {
            if (d.prompt.find("which of") != std::string::npos) {  // the richest planet to sit beside
                out = one(bestBy(d, [&](int x) { return float(planetResources(g, x) + planetInfluence(g, x)); }));
                return true;
            }
            const int q = d.targetPlayer;  // help anyone but the leader
            int lead = 0;
            for (int r = 0; r < g.numPlayers; ++r) lead = std::max<int>(lead, g.players[size_t(r)].vp);
            out = one(q >= 0 && g.players[size_t(q)].vp < lead ? 1 : 0);
            return true;
        }
        case Ab::DeepwroughtCommander:
        case Ab::Eanautic:
            out = one(1);
            return true;
        case Ab::Radical: {
            int best = -1;
            float bv = fquirkIs(g, p, "deepwrought", 1) ? -0.5f : 0.5f;  // Library Card: climb every round
            for (const Option& o : d.options) {
                if (!o.enabled || o.id < 0) continue;
                float gain = aiTechWant(g, p, o.id % 1000) - aiTechWant(g, p, o.id / 1000);
                if (!std::strcmp(kTechs[o.id / 1000].key, "radical")) gain -= 3.f;  // (0.44.9: it climbs again next round)
                if (gain > bv) bv = gain, best = o.id;
            }
            out = one(best);
            return true;
        }
        case Ab::DeepwroughtHero: {  // a technology the others lean on and we don't
            out = one(bestBy(d, [&](int t) {
                float v = 0.f;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (g.players[size_t(q)].hasTech(t)) v += q == p ? -3.f : 1.f;
                return v;
            }));
            return true;
        }
        case Ab::ShareKnowledge:
            out = one(bestBy(d, [&](int t) { return aiTechWant(g, p, t); }));
            return true;
        case Ab::BtDeepwrought: {
            if (d.options.size() == 2 && has(d, 1) && has(d, 0)) {  // buy a technology for 3 trade goods and a note
                float want = 0.f;
                for (int t = 0; t < kTechCount; ++t)
                    if (kTechs[t].faction < 0 && !(g.fixes >= 8 && kTechs[t].unitUpgrade) && canResearchNow(g, p, t))
                        want = std::max(want, aiTechWant(g, p, t));
                out = one(me.tradeGoods >= 3 + aiGoals(g, p).keepTg + 1 && want >= 5.f ? 1 : 0);
                return true;
            }
            out = one(bestBy(d, [&](int n) { return -noteDefaultValue(g, n) - (noteType(n) == kNoteSupportThrone ? 50.f : 0.f); }));
            return true;
        }
        case Ab::Incursion:  // open breaches, never close our own
            out = one(d.prompt.find(": open") != std::string::npos ? 1 : 0);
            return true;
        case Ab::Exile:
            if (fquirkIs(g, p, "crimson", 1)) out = one(has(d, 1) ? 1 : 0);  // Quiet Breaches: inactive
            else out = one(has(d, 2) ? 2 : has(d, 1) ? 1 : 0);
            return true;
        case Ab::RemoveBreach:  // the Crimson keep theirs (Quiet Breaches: closed); everyone else closes them
            out = one(isFaction(g, p, "crimson") && !fquirkIs(g, p, "crimson", 1) ? 0 : 1);
            return true;
        case Ab::Subatomic:
            out = one(aiFreeSpend(g, p, false) >= 4 ? 1 : 0);
            return true;
        case Ab::CrimsonHero: {  // spring the phantom fleet in a fight; (0.55.0: never used before) hide new ships on the
            // card from round 3 while home is calm - a reserve that joins any space combat of ours at once
            if (d.prompt.find("purge") != std::string::npos) {
                out = one(1);
                return true;
            }
            const int home = g.players[size_t(p)].homeSystem;
            bool calm = home >= 0;
            if (home >= 0)
                for (int s2 : systemsWithin(g, home, 1))
                    if (otherShipsIn(g, p, s2)) calm = false;
            out = one(g.round >= 3 && calm && aiTune("phantom", 1.f) > 0.f && g.players[size_t(p)].phantom.nonFighterShips() < 6 ? 1 : 0);
            return true;
        }
        case Ab::Revenant:
            out = one(1);
            return true;
        case Ab::BtCrimson: {  // open an inactive breach, else a new active one next to enemies
            out = one(bestBy(d, [&](int c) {
                if (c < 1000) return g.systems[size_t(c)].breach == 1 ? 2.f : -5.f;
                float v = 0.f;
                for (int16_t a : g.systems[size_t(c - 1000)].adj)
                    for (int16_t x : g.systems[size_t(a)].planets)
                        if (g.planets[size_t(x)].owner >= 0 && g.planets[size_t(x)].owner != p) v += 0.5f;
                return v;
            }));
            return true;
        }
        // ---- the Firmament / the Obsidian
        case Ab::PlotsWithin:
        case Ab::BlackOps:
        case Ab::Enervate:
            out = one(1);
            return true;
        case Ab::PlotCard: {  // the plot that fits the victim (see the strategy guide)
            const int q = d.targetPlayer;
            out = one(bestBy(d, [&](int k) {
                if (q < 0 || q >= g.numPlayers) return float(k == kPlotAssail);
                const Player& v = g.players[size_t(q)];
                const bool nb = areNeighbours(g, p, q);
                switch (k) {
                    case kPlotSiphon: return float(kFactions[v.faction].commodities) * 0.7f;
                    case kPlotExtract: {
                        int lack = 0;
                        for (int16_t t : v.techs)
                            if (kTechs[t].faction < 0 && !me.hasTech(t)) ++lack;
                        return 0.5f + float(lack) * 0.5f;
                    }
                    case kPlotEnervate: return 2.0f;
                    case kPlotSeethe: {
                        float best = 0.f;
                        for (int x : controlledPlanets(g, q))
                            if (!isHomePlanet(g, x)) best = std::max(best, aiPlanetValue(g, p, x));
                        return (nb ? 0.6f : 0.3f) * best;
                    }
                    case kPlotAssail: return nb ? 2.4f : 0.8f;
                    default: return 0.f;
                }
            }));
            return true;
        }
        case Ab::FirmamentAgent:  // Myru Vos: our own fleet under space cannon, or as promised (0.43.0: not just for the way through)
            out = one(d.value == 1 || g.fixes < 2 || g.activePlayer != p ? 1 : 0);
            return true;
        case Ab::FirmamentHero: {  // the leader (the neighbour leader first)
            out = one(bestBy(d, [&](int q) { return float(g.players[size_t(q)].vp) + (areNeighbours(g, p, q) ? 1.5f : 0.f); }));
            return true;
        }
        case Ab::PlotToken: {  // the leader's token, on Assail or Siphon best
            out = one(bestBy(d, [&](int c) {
                const int k = c / 8, q = c % 8;
                return float(g.players[size_t(q)].vp) + (k == kPlotAssail && areNeighbours(g, p, q) ? 2.f : 0.f) + (k == kPlotSiphon ? 1.f : 0.f);
            }));
            return true;
        }
        case Ab::NeuralParasite:
        case Ab::Parasite2:
            out = one(bestBy(d, [&](int c) {
                if (Ab(d.ability) == Ab::NeuralParasite) return 1.f;
                const int q = c % 8, planet = c / 1024 - 1;
                if (planet < 0) return 0.5f;
                return (groundForcesOn(g, q, planet) == 1 ? 3.f : 1.f) + aiPlanetValue(g, p, planet) * 0.1f;
            }));
            return true;
        case Ab::Sowing: {  // bank when a coup is coming (2 for 1 on The Reaping); keep 2 to spend
            int want = 0;
            if (plotCount(me) >= 1 || g.round <= 2) want = std::max(0, std::min(3, int(me.tradeGoods) - 2));
            if (firmQuirk(g, p, 1)) want = std::min(3, int(me.tradeGoods));  // Coup Clock
            out = one(bestBy(d, [&](int k) { return -std::abs(float(k - want)); }));
            return true;
        }
        case Ab::Seethe: {
            const int q = d.targetPlayer;
            out = one(bestBy(d, [&](int c) {
                if (d.onPlanets || c >= 1000) {  // the reveal: every unit on the planet
                    float v = aiPlanetValue(g, p, c);
                    for (int s = 0; s < kSides; ++s)
                        if (s != p) v += float(g.planets[size_t(c)].units[size_t(s)].total()) * 0.6f;
                    return v - float(g.planets[size_t(c)].units[size_t(p)].total()) * 1.5f;
                }
                if (c <= 0) return 0.2f;  // an infantry in space
                const int x = c - 1;
                return q >= 0 && groundForcesOn(g, q, x) == 1 ? 2.f + aiPlanetValue(g, p, x) * 0.1f : 1.f;
            }, true));
            return true;
        }
        case Ab::Extract:
            if (d.options.size() == 2 && has(d, 1) && has(d, 0)) {  // pay 4 resources for the puppet's new technology
                out = one(aiFreeSpend(g, p, false) >= 6 ? 1 : 0);
                return true;
            }
            out = one(bestBy(d, [&](int t) { return aiTechWant(g, p, t); }));
            return true;
        case Ab::ObsidianAgent: {  // Vos Hollow: never against ourselves or a friend
            const int q = d.targetPlayer;
            out = one(q >= 0 && q != p && !promisedNoAttack(g, p, q) ? 1 : 0);
            return true;
        }
        case Ab::Malevolency: {  // pass it on - the Obsidian to whoever leads, the others to the Obsidian's best foe
            out = one(bestBy(d, [&](int q) { return float(g.players[size_t(q)].vp) + (isFaction(g, q, "obsidian") ? 0.5f : 0.f); }));
            return true;
        }
        case Ab::Planesplitter: {
            if (has(d, -1)) {  // which token: none if we can already reach one
                bool reach = false;
                for (size_t s = 0; s < g.systems.size(); ++s)
                    if (g.systems[s].ingress) {
                        if (hasShips(g, p, int(s))) reach = true;
                        for (int16_t a : g.systems[s].adj) reach = reach || hasShips(g, p, a);
                    }
                if (reach) {
                    out = one(-1);
                    return true;
                }
                const std::vector<int> dist = fromHome(g, p);
                out = one(bestBy(d, [&](int s) { return float(dist[size_t(s)]); }));
                return true;
            }
            out = one(bestBy(d, [&](int s) { return float(g.systems[size_t(s)].space[size_t(p)].nonFighterShips()); }));
            return true;
        }
        // ---- galactic events (0.36.0)
        case Ab::EvMobilize:
            if (d.onPlanets) {  // the space dock: the richest planet away from home
                out = one(bestBy(d, [&](int x) { return float(planetResources(g, x)) + (isHomePlanet(g, x) ? -5.f : 0.f); }));
                return true;
            }
            out = one(bestBy(d, [&](int t) { return aiTechWant(g, p, t); }));
            return true;
        case Ab::EvWildsTech:
            out = one(bestBy(d, [&](int t) { return aiTechWant(g, p, t); }));
            return true;
        case Ab::ScarTech: {  // entropic scar (0.43.4): a faction technology for a strategy token, if it's worth one
            int best = -1;
            float bv = 2.f;
            for (const Option& o : d.options)
                if (o.id >= 0 && aiTechWant(g, p, o.id) > bv) {
                    bv = aiTechWant(g, p, o.id);
                    best = o.id;
                }
            out = one(best);
            return true;
        }
        case Ab::EvSpoils:
            out = one(1);
            return true;
        case Ab::EvMonument:
            if (d.prompt.find("destroy") != std::string::npos) out = one(0);  // its points are ours now
            else out = one(g.round <= 6 && me.tradeGoods >= 5 ? 1 : 0);
            return true;
        case Ab::EvAtomics:
        case Ab::EvX89: {  // the leader's best planet with the most on it - never our own
            int lead = -1;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && (lead < 0 || g.players[size_t(q)].vp > g.players[size_t(lead)].vp)) lead = q;
            out = one(bestBy(d, [&](int x) {
                const PlanetState& ps = g.planets[size_t(x)];
                if (ps.owner == p) return -50.f;
                float v = aiPlanetValue(g, p, x) * 0.3f;
                for (int s = 0; s < kSides; ++s)
                    if (s != p) v += float(ps.units[size_t(s)].groundForces() + ps.units[size_t(s)].structures() * 2);
                v -= float(ps.units[size_t(p)].total()) * 3.f;
                if (ps.owner == lead) v += 2.f;
                return v;
            }));
            return true;
        }
        case Ab::EvExplore:
            out = one(firstReal(d));
            return true;
        case Ab::BtYssaril: {
            if (d.prompt.compare(0, 14, "Ask the Yssari") == 0) {  // (0.44.9: borrowing Scheming - always worth asking)
                out = one(1);
                return true;
            }
            if (has(d, 1) && has(d, 0) && (d.options.size() == 2 || has(d, 3))) {  // the Yssaril: share with anyone not ahead of us
                const int q = d.targetPlayer;
                out = one(q >= 0 && q < g.numPlayers && g.players[size_t(q)].vp <= me.vp ? 1 : 0);
                return true;
            }
            out = one(bestBy(d, [&](int c) { return -aiCardKeepValue(c); }));  // the card we like least
            return true;
        }
        default: return false;
    }
    return false;
}

// Breakthrough component actions (6000 + Ab): about what the plan would gain.
bool aiTeComponentValue(const Game& g, int p, int ab, float& v) {
    const Player& me = g.players[size_t(p)];
    switch (Ab(ab)) {
        case Ab::BtArborec: {  // 0.44.4: an infantry, and a system of ours open again (ships there may move / it can be activated)
            v = 0.5f;
            for (const SystemState& s : g.systems)
                if (s.hasToken(p) && s.space[size_t(p)].nonFighterShips() > 0) v = 0.8f;
            return true;
        }
        case Ab::BtMahact: {  // 0.44.4: a relic for the technology we'd miss least - not one objectives or prerequisites need
            const Goals& go = aiGoals(g, p);
            float low = 99.f;
            for (int16_t t : me.techs) low = std::min(low, mahactKeep(g, p, t, go));
            const float relics = float(me.relics.size());
            // (0.55.0: offered 433 times in 140 games, never used - the bar was too high)
            v = g.round >= 3 && low < aiTune("mahactlow", 4.f) ? std::max(0.f, 0.9f - 0.25f * relics) : 0.f;
            return true;
        }
        case Ab::BtSaar: v = aiFreeSpend(g, p, false) >= 3 ? 0.7f : 0.f; return true;
        case Ab::BtMuaat: v = 0.8f; return true;   // The Nucleus: Star Forge for free
        case Ab::BtYssaril: v = me.actionCards.size() >= 7 ? 0.3f : 0.f; return true;  // a stall instead of passing
        case Ab::RalNelAgent: v = 0.7f; return true;   // a card for us, one to make a friend
        case Ab::Nano1: v = g.round >= 2 ? 0.6f : 0.3f; return true;
        case Ab::Nano2: {
            int dmg = 0;
            for (const SystemState& s : g.systems)
                for (int t = 0; t < kUnitTypes; ++t) dmg += s.space[size_t(p)].dmg[t];
            v = dmg >= 2 ? 0.8f : 0.3f;
            return true;
        }
        case Ab::Nano3: v = 0.3f; return true;  // (0.43.0: just enough to be used before passing - exhausting is free then)
        case Ab::DeepwroughtHero: v = g.round >= 4 ? 0.7f : 0.f; return true;
        case Ab::ShareKnowledge: {
            const int dw = playerOfFaction(g, "deepwrought");
            float best = 0.f;
            if (dw >= 0)
                for (int16_t t : g.players[size_t(dw)].techs)
                    if (kTechs[t].faction < 0 && !kTechs[t].unitUpgrade && !me.hasTech(t)) best = std::max(best, aiTechWant(g, p, t));
            v = best >= 4.f ? 0.8f : 0.f;
            return true;
        }
        case Ab::BtDeepwrought: {
            int rich = 0;  // Library Card: as soon as two rivals hold 3 trade goods (0.44.4: and a note to give us)
            for (int q = 0; q < g.numPlayers; ++q) {
                if (q == p || g.players[size_t(q)].tradeGoods < 3) continue;
                bool note = false;
                for (int16_t n : g.players[size_t(q)].notes) note = note || noteOwner(n) != p;
                if (note) ++rich;
            }
            v = rich == 0 ? 0.f : fquirkIs(g, p, "deepwrought", 1) ? (rich >= 2 ? 1.6f : 0.4f) : 0.5f + 0.25f * float(std::min(rich, 3));
            return true;
        }
        case Ab::BtCrimson: {
            bool closed = false;
            for (const SystemState& s : g.systems) closed = closed || s.breach == 1;
            v = closed ? 0.5f : 0.3f;
            return true;
        }
        case Ab::CrimsonAgent: v = ravinPair(g, p, nullptr, nullptr) >= 5.f ? 0.7f : 0.f; return true;
        case Ab::Sever: {  // a wormhole we stand in that an enemy fleet could come through
            v = 0.f;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                const uint8_t w = wormholesOf(g, int(s));
                if (!w || !hasUnits(g, p, int(s))) continue;
                for (size_t e = 0; e < g.systems.size(); ++e)
                    if (e != s && (wormholesOf(g, int(e)) & w))
                        for (int q = 0; q < g.numPlayers; ++q)
                            if (q != p && g.systems[e].space[size_t(q)].nonFighterShips() >= 2) v = 0.45f;
            }
            return true;
        }
        case Ab::BtNaaz: v = g.round >= 2 ? 1.2f : 0.5f; return true;  // Eidolon Maximum
        case Ab::Puppets: {  // the coup: with 2 plots (1 from round 3), never with the game nearly over
            const int n = plotCount(me);
            int lead = 0;
            for (const Player& q : g.players) lead = std::max<int>(lead, q.vp);
            if (n == 0 || lead >= g.vpGoal - 1) {
                v = 0.f;
            } else if (firmQuirk(g, p, 1)) {  // Coup Clock: at 2 plots, as soon as the readied Hollows can be spent
                v = n >= 2 ? 1.8f : 0.f;
            } else {
                v = n >= 2 ? 1.6f : g.round >= 5 ? 1.5f : g.round >= 3 ? 1.1f : 0.f;
            }
            return true;
        }
        case Ab::FirmamentHero: v = 1.9f; return true;  // a plot (before the coup)
        // galactic events (0.36.0)
        case Ab::EvTotalWar: v = 3.0f; return true;  // a victory point
        case Ab::EvMercenary: v = me.tradeGoods >= 5 ? 1.0f : 0.4f; return true;
        case Ab::EvExplore: v = 0.5f; return true;
        case Ab::EvAtomics: {  // worth it on a rich enemy stronghold (2+ ground forces / structures)
            float best = 0.f;
            for (size_t x = 0; x < g.planets.size(); ++x) {
                const PlanetState& ps = g.planets[x];
                if (ps.destroyed || ps.owner == p || ps.owner < 0 || ps.system < 0 || isHomePlanet(g, int(x))) continue;
                const float n = float(ps.units[size_t(ps.owner)].groundForces() + ps.units[size_t(ps.owner)].structures() * 2);
                best = std::max(best, n);
            }
            v = best >= 5.f ? 1.4f : best >= 3.f && g.round >= 4 ? 0.9f : 0.f;
            return true;
        }
        case Ab::EvX89: {
            float best = 0.f;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                bool bomb = false;
                for (int t = 0; t < kUnitTypes; ++t)
                    if (g.systems[s].space[size_t(p)].n[t] && unitDef(g, p, UnitType(t)).bombard > 0) bomb = true;
                if (!bomb) continue;
                for (int16_t x : g.systems[s].planets)
                    if (g.planets[size_t(x)].owner >= 0 && g.planets[size_t(x)].owner != p) best = std::max(best, aiPlanetValue(g, p, x));
            }
            v = best >= 3.f ? 1.0f : 0.f;
            return true;
        }
        case Ab::ObsidianHero: {
            int tired = 0;
            for (int x : controlledPlanets(g, p))
                if (g.planets[size_t(x)].exhausted) tired += std::max(planetResources(g, x), planetInfluence(g, x));
            v = tired >= 7 ? 1.2f : 0.f;
            return true;
        }
        default: return false;
    }
}

}  // namespace ti
