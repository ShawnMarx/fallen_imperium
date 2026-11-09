// Computer players and faction abilities, leaders and faction notes (Stage 5).
// Simple rules of thumb, like the rest of the AI; every answer still goes
// through the engine's validators.
#include <algorithm>
#include <cmath>
#include <cstdlib>
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
        if (o.id == id && o.enabled) return true;
    return false;
}
int firstReal(const Decision& d) {
    for (const Option& o : d.options)
        if (o.enabled && o.id >= 0) return o.id;
    for (const Option& o : d.options)
        if (o.enabled) return o.id;
    return d.options.empty() ? 0 : d.options[0].id;
}
bool yesNo(const Decision& d) { return d.options.size() == 2 && has(d, 1) && has(d, 0) && d.options[0].label == "Yes"; }
Answer yes(bool b) { return one(b ? 1 : 0); }

// Best option by a score; options with id < 0 ("no", "cancel") are skipped.
template <class F>
int bestBy(const Decision& d, F score, float minScore = -1e9f) {
    int best = -1;
    float bv = minScore;
    for (const Option& o : d.options) {
        if (!o.enabled || o.id < 0) continue;
        float v = score(o.id);
        if (v > bv) {
            bv = v;
            best = o.id;
        }
    }
    return best;
}

float planetWorth(const Game& g, int x) { return float(planetResources(g, x) + planetInfluence(g, x)); }

// Planets near enemy ships are where extra infantry matter most.
float defendScore(const Game& g, int p, int x) {
    const PlanetState& ps = g.planets[x];
    float v = planetWorth(g, x) * 0.3f - float(ps.units[p].groundForces()) * 0.5f;
    for (int16_t a : g.systems[ps.system].adj)
        if (otherShipsIn(g, p, a)) v += 2.f;
    if (g.systems[ps.system].home == p) v += 0.5f;
    return v;
}

int ownOption(const Game& g, const Decision& d, int p) {
    // options labelled "<player>'s <unit>": pick the strongest of our own
    std::string mine = playerName(g, p) + "'s ";
    int best = -1, bestCombat = 99;
    for (const Option& o : d.options) {
        if (o.id < 0 || o.label.compare(0, mine.size(), mine) != 0) continue;
        int t = o.id % 16;
        int c = unitDef(g, p, UnitType(t)).combat;
        if (c > 0 && c < bestCombat) {
            bestCombat = c;
            best = o.id;
        }
    }
    return best;
}

int numberBefore(const std::string& s, const std::string& marker) {
    size_t at = s.find(marker);
    if (at == std::string::npos || at == 0) return 0;
    size_t b = at;
    while (b > 0 && s[b - 1] >= '0' && s[b - 1] <= '9') --b;
    return std::atoi(s.substr(b, at - b).c_str());
}

// Value of keeping an owned technology (for Rin, the Jol-Nar hero) - 0.44.6: on the same scale as the replacements
// (its own research want; it used to be a flat 0.5 / 1.5, so the bot swapped X-89 and Hyper Metabolism back and
// forth), a little more for what we know works
float keepValue(const Game& g, int p, int t) { return aiTechWant(g, p, t) + 0.5f; }

// Combat odds in the active system: is the opponent clearly stronger in space?
bool outgunned(const Game& g, int p) {
    int sys = g.activeSystem;
    if (sys < 0) return false;
    int mine = combatStrength(g, p, sys, true), theirs = 0;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p) theirs = std::max(theirs, combatStrength(g, q, sys, true));
    return theirs * 10 >= mine * 6;
}

int enemyNonFighters(const Game& g, int p, int sys) {
    if (sys < 0) return 0;
    int n = 0;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p) n += g.systems[sys].space[q].nonFighterShips();
    return n;
}

float targetValue(const Game& g, int p, int sys);
// Wormhole Generator / Creuss Iff (0.44.7): how good a system is for Creuss wormhole token k - a new link near targets
// and near our fleet; never a second token in one system, a wormhole of that kind already there, or a supernova
// (half the placements used to be stacked or in supernovas)
float creussSpot(const Game& g, int p, int sys, int k) {
    if (sys < 0 || sys >= int(g.systems.size()) || k < 0 || k > 2) return -9.f;
    const uint8_t bit = k == 0 ? kAlpha : k == 1 ? kBeta : kGamma;
    for (int i = 0; i < 3; ++i)
        if (i != k && g.creussTokens[i] == sys) return -6.f;
    if (g.systems[size_t(sys)].d().wormholes & bit) return -5.f;
    if (g.systems[size_t(sys)].d().anomalies & kSupernova) return -5.f;
    float v = targetValue(g, p, sys);
    if (sys == g.nexus && !g.systems[size_t(sys)].nexusActive) v -= 1.f;  // (0.44.9: it comes back when the nexus opens)
    bool fleet = hasShips(g, p, sys);
    for (int16_t a : g.systems[size_t(sys)].adj) fleet = fleet || hasShips(g, p, a);
    if (fleet) v += 1.f;
    if (g.systems[size_t(sys)].home == p) v -= 1.f;
    return v;
}

// How attractive a system's surroundings are as a destination (planets of others nearby, Mecatol Rex).
float targetValue(const Game& g, int p, int sys) {
    float v = 0;
    std::vector<int> near = {sys};
    for (int16_t a : g.systems[sys].adj) near.push_back(a);
    for (int s : near)
        for (int16_t x : g.systems[s].planets)
            if (g.planets[x].owner != p) v += planetWorth(g, x) * 0.3f;
    int mec = g.mecatol();
    if (mec >= 0 && (sys == mec || std::find(g.systems[sys].adj.begin(), g.systems[sys].adj.end(), mec) != g.systems[sys].adj.end()))
        v += 2.f;
    return v;
}

int enemyLight(const Game& g, int p, int sys) {  // other players' infantry and fighters in a system
    int n = 0;
    for (int q = 0; q < g.numPlayers; ++q) {
        if (q == p) continue;
        n += g.systems[sys].space[q].count(UnitType::Fighter) + g.systems[sys].space[q].count(UnitType::Infantry);
        for (int16_t x : g.systems[sys].planets) n += g.planets[x].units[q].count(UnitType::Infantry);
    }
    return n;
}

// batch 4 helpers
// Airo Shir Aur: the best (own fleet -> adjacent enemy fleet) throw, as option id system * 1000 + target, or -1.
int bestHurl(const Game& g, int p, float* value) {
    int best = -1;
    float bv = 0.f;
    for (size_t s = 0; s < g.systems.size(); ++s) {
        if (!hasShips(g, p, int(s))) continue;
        for (int16_t a : g.systems[s].adj)
            for (int v = 0; v < g.numPlayers; ++v) {
                if (v == p || !hasShips(g, v, a)) continue;
                float odds = fightOdds(g, p, g.systems[s].space[p], v, g.systems[a].space[v], true);
                float w = odds * float(g.systems[a].space[v].nonFighterShips() + 1);
                if (odds >= 0.75f && w > bv) {
                    bv = w;
                    best = int(s) * 1000 + a;
                }
            }
    }
    if (value) *value = bv;
    return best;
}
// Vortex: what one capture is worth - a fighter / infantry is only a token (LRR 17.3), worth something to the
// Vuil'raith alone; nothing from a player blockading one of our docks (LRR 17.6); units on planets count (0.44.8)
float vortexWorth(const Game& g, int p, int q, int t) {
    if (t == int(UnitType::Fighter) || t == int(UnitType::Infantry)) return isFaction(g, p, "cabal") ? float(unitCost2(g, q, UnitType(t))) : 0.2f;
    return float(unitCost2(g, q, UnitType(t)));
}
bool vortexBlocked(const Game& g, int p, int q) {
    for (size_t s = 0; s < g.systems.size(); ++s) {
        bool dock = false;
        for (int16_t x : g.systems[s].planets)
            if (g.planets[x].units[p].count(UnitType::SpaceDock)) dock = true;
        if (dock && hasShips(g, q, int(s)) && !hasShips(g, p, int(s))) return true;
    }
    return false;
}
int capturableCost(const Game& g, int p) {  // Vortex: the most valuable capture in reach
    float best = 0;
    for (size_t s = 0; s < g.systems.size(); ++s) {
        bool dock = false;
        for (int16_t x : g.systems[s].planets)
            if (g.planets[x].units[p].count(UnitType::SpaceDock)) dock = true;
        if (!dock) continue;
        for (int16_t a : g.systems[s].adj)
            for (int q = 0; q < g.numPlayers; ++q) {
                if (q == p || vortexBlocked(g, p, q)) continue;
                for (int t = 0; t <= int(UnitType::Mech); ++t) {
                    if (isStructure(UnitType(t)) || unitsAvailable(g, q, UnitType(t)) <= 0) continue;
                    int n = g.systems[a].space[q].n[t];
                    for (int16_t x : g.systems[a].planets) n += g.planets[x].units[q].n[t];
                    if (n) best = std::max(best, vortexWorth(g, p, q, t));
                }
            }
    }
    return int(best + 0.5f);
}
}  // namespace

float aiFreeTacticalValue(const Game& g, int p);  // ai.cpp
float aiFactionComponentValue(const Game& g, int p, int option) {
    const Player& me = g.players[p];
    const Ab ab = Ab(option >= 6000 ? option - 6000 : option - 700);
    if (float tv = 0.f; option >= 6000 && aiTeComponentValue(g, p, int(ab), tv)) return tv;  // breakthroughs (ai_te.cpp)
    if (float dv = 0.f; option >= 6000 && aiDsComponentValue(g, p, int(ab), dv)) return dv;  // Discordant Stars (ai_ds.cpp)
    switch (ab) {
        case Ab::HacanAgent: return me.commodities + 2 <= commodityValue(g, me) ? 0.6f : 0.f;
        case Ab::OrbitalDrop: return me.strategy >= 2 ? 1.1f : 0.4f;
        case Ab::ProductionBiomes: return me.strategy >= 2 ? 2.4f : me.strategy >= 1 ? 2.2f : 0.f;  // (0.44.6: 4 trade goods - worth a turn)
        case Ab::NaaluAgent:  // Z'eu Omega: a free tactical action (0.43.0, C3: a promised one is worth keeping the word)
            return g.fixes >= 2 && g.agentFor[p] >= 0 ? 2.5f : aiFreeTacticalValue(g, p);
        case Ab::XxchaAgent: {
            float best = 0;
            for (int x : controlledPlanets(g, p))
                if (g.planets[x].exhausted) best = std::max(best, planetWorth(g, x));
            return best >= 4 ? 0.6f : 0.f;
        }
        case Ab::SolHero: {
            int n = 0;
            for (const SystemState& s : g.systems)
                if (s.hasToken(p)) ++n;
            return n >= 4 && me.tactic >= 1 ? 1.2f : 0.f;
        }
        case Ab::LetnevHero: {
            for (const SystemState& s : g.systems)
                if (s.space[p].nonFighterShips() >= me.fleet && me.tactic >= 1) return 1.1f;
            return 0.f;
        }
        case Ab::JolnarHero: return g.round >= 5 ? 0.9f : 0.f;
        // 0.43.0 (debug passes: offered ~500 times each, never used - no value here)
        case Ab::ArgentHero: {  // Mirik Aun Sissiri: Raise a Fleet / Command an Armada in one move, or a late regroup
            int movable = 0, best = 0, most = 0;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                if (g.systems[s].home != p) movable += g.systems[s].space[p].nonFighterShips();
                most = std::max(most, g.systems[s].space[p].nonFighterShips());
            }
            for (size_t s = 0; s < g.systems.size(); ++s)
                if (g.systems[s].hasToken(p) && !otherShipsIn(g, p, int(s)))
                    best = std::max(best, movable + (g.systems[s].home == p ? g.systems[s].space[p].nonFighterShips() : 0));
            if (aiLevelScale(g) > 0.f) {
                const Goals& G = aiGoals(g, p);
                if (G.fleetTarget > 0 && best >= G.fleetTarget && most < G.fleetTarget) return 1.5f;
            }
            return g.round >= 5 && best >= 4 ? 0.35f : 0.f;  // (before passing: the fleet gathered for the next round)
        }
        case Ab::NomadHero: {  // Ahk-Syl Siven: the flagship moves again this round
            if (g.round < 3 || me.tactic < 1) return 0.f;
            for (const SystemState& s : g.systems)
                if (s.hasToken(p) && s.space[p].count(UnitType::Flagship)) return 0.9f;
            return 0.f;
        }
        case Ab::XxchaHero: {
            if (!teXxchaHero(g)) return g.round >= 5 ? 1.0f : 0.f;
            // (0.57.1) Thunder's Edge's Xxekir Grom: 4 free PDS / mechs and the planets under them readied - best with
            // spent planets to ready (then spend them again) or enemies close to home
            int spent = 0, value = 0;
            for (int x : controlledPlanets(g, p))
                if (g.planets[size_t(x)].exhausted && !isStation(g, x)) ++spent, value += std::max(planetResources(g, x), planetInfluence(g, x));
            const bool late = g.round >= 5 || me.vp >= g.vpGoal - 3;
            if (spent >= 3 && value >= 6) return 1.2f;
            return late ? 0.8f : 0.f;
        }
        // batch 2
        case Ab::ArborecAgent: return 0.8f;  // a free upgrade
        case Ab::ArborecHero: return g.round >= 3 && spendable(g, p, false) >= 8 ? 1.0f : 0.f;
        case Ab::L1Z1XHero: return g.round >= 5 && unitsOnBoard(g, p, UnitType::Dreadnought) >= 2 ? 0.6f : 0.f;
        case Ab::PromiseOfProtection: return me.tradeGoods >= 2 ? 0.6f : 0.f;
        case Ab::TradeConvoys: return aiTradeConvoysValue(g, p);
        case Ab::DarkPact: return aiDarkPactValue(g, p);
        case Ab::StallTactics: return me.actionCards.size() >= 7 ? 0.35f : 0.f;  // stall instead of passing
        case Ab::MageonImplants: return 0.9f;
        case Ab::YinHero: return g.round >= 3 ? 1.0f : 0.f;
        case Ab::YssarilHero: {
            int cards = 0;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p) cards += int(g.players[q].actionCards.size());
            return g.round >= 3 && cards >= 6 ? 0.9f : 0.f;
        }
        // batch 3
        case Ab::WormholeGenerator: {  // (0.44.7: by the best spot - a badly placed token is worth moving)
            float gain = 0.f;
            for (int k = 0; k < 3; ++k) {
                float best = -9.f;
                for (size_t s = 0; s < g.systems.size(); ++s) best = std::max(best, creussSpot(g, p, int(s), k));
                const float now = g.creussTokens[k] >= 0 ? creussSpot(g, p, g.creussTokens[k], k) : -1.f;
                gain = std::max(gain, best - now);
            }
            return gain >= 4.f ? 1.f : gain >= 2.f ? 0.5f : 0.1f;
        }
        case Ab::StarForge: return me.strategy >= 2 ? 0.8f : 0.3f;
        case Ab::Inferno: {  // (0.43.0 debug passes: a cruiser at the front for a spare token - more so with foes about)
            if (me.strategy < 2) return 0.f;
            float near = 0.f;
            for (size_t s = 0; s < g.systems.size(); ++s)
                if (g.systems[s].space[size_t(p)].count(UnitType::Flagship)) {
                    std::vector<int> zone{int(s)};
                    for (int16_t a : g.systems[s].adj) zone.push_back(a);
                    for (int z : zone)
                        for (int q = 0; q < g.numPlayers; ++q)
                            if (q != p && g.systems[size_t(z)].space[size_t(q)].nonFighterShips() > 0) near = 0.3f;
                }
            return (me.strategy >= 3 ? 1.1f : 0.6f) + near;
        }
        case Ab::MuaatAgent: return spendable(g, p, false) >= 4 ? 0.7f : 0.f;
        case Ab::FiresOfGashlai: return 0.6f;
        case Ab::BloodPact: return 0.35f;
        case Ab::LazaxGate: return 0.4f;
        case Ab::CreussHero: return g.round >= 4 ? 0.7f : 0.f;
        case Ab::SaarHero: {
            int best = 0;
            for (size_t s = 0; s < g.systems.size(); ++s) best = std::max(best, enemyLight(g, p, int(s)));
            return best >= 3 ? 0.9f : 0.f;
        }
        case Ab::WinnuHero: return g.round >= 4 ? 1.0f : 0.f;
        case Ab::NekroHero: return g.round >= 3 ? 1.0f : 0.f;
        case Ab::EmpyreanHero: return g.round >= 4 ? 0.9f : 0.f;
        case Ab::NekroAgent: {
            bool spare = me.tactic >= 3;
            if (me.actionCards.size() >= 3)
                for (int16_t c : me.actionCards)
                    if (aiCardKeepValue(c) < 3.f) spare = true;
            return spare ? 0.5f : 0.f;
        }
        // batch 4
        case Ab::Fabrication:
        case Ab::BlackMarketForgery:
            for (int t = 0; t < 3; ++t)
                if (relicFragments(g, p, t) >= 2) return 1.2f;
            return me.tactic <= 1 ? 0.6f : 0.2f;
        case Ab::Terraform: return 0.8f;
        case Ab::Vortex: {
            const int c = capturableCost(g, p);
            return c >= 6 ? 0.8f : c >= 2 ? 0.4f : 0.05f;  // (0.44.8: not an action for a token)
        }
        case Ab::MahactHero: {
            float v = 0;
            return bestHurl(g, p, &v) >= 0 ? 1.2f : 0.f;
        }
        case Ab::NaazHero: return g.round >= 3 ? 1.2f : 0.f;
        case Ab::TitansHero: {
            int d = findPlanet("elysium");
            for (int x : controlledPlanets(g, p))
                if (g.planets[x].def == d) return g.round >= 3 ? 1.0f : 0.f;
            return 0.f;
        }
        // batch 5: the Council Keleres
        case Ab::ExecutiveOrder: {
            if (g.round < 2) return 0.f;
            int mine = me.tradeGoods, best = 0;
            for (int x : readyPlanets(g, p)) mine += std::max(planetInfluence(g, x), planetResources(g, x));
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && !g.players[q].eliminated) best = std::max(best, availableVotes(g, q));
            return mine >= best ? (fquirkIs(g, p, "keleres", 1) ? 0.9f : 0.6f) : 0.25f;
        }
        case Ab::KeleresHarka: return g.round >= 2 ? 0.9f : 0.f;
        case Ab::CabalHero: {
            int ships = 0;
            for (size_t s = 0; s < g.systems.size(); ++s) {
                if (!dimensionalTear(g, int(s))) continue;
                std::vector<int> zone = {int(s)};
                for (int16_t a : g.systems[s].adj) zone.push_back(a);
                for (int z : zone)
                    for (int q = 0; q < g.numPlayers; ++q)
                        if (q != p) ships += g.systems[z].space[q].nonFighterShips();
            }
            return ships >= 3 ? 1.0f : 0.f;
        }
        default: return 0.f;
    }
}

bool aiFactionAnswer(const Game& g, const Decision& d, Rng& rng, Answer& out) {
    (void)rng;
    if (d.tag != Tag::Faction) return false;
    const int p = d.player;
    const Player& me = g.players[p];
    const Ab ab = Ab(d.ability);
    const bool yn = yesNo(d);
    if (aiTeAnswer(g, d, out)) return true;  // Thunder's Edge (ai_te.cpp)
    if (aiDsAnswer(g, d, rng, out)) return true;  // Discordant Stars (ai_ds.cpp)
    switch (ab) {
        case Ab::OrbitalDrop:
        case Ab::MilitarySupport:
        case Ab::SardakkAgent:
        case Ab::InfantryReturn:
            if (yn) {
                // T'ro used for someone else: no
                out = yes(ab != Ab::SardakkAgent || d.prompt.find("to let") == std::string::npos);
                return true;
            }
            {
                int x = bestBy(d, [&](int id) { return defendScore(g, p, id); });
                out = one(x >= 0 ? x : firstReal(d));
            }
            return true;
        case Ab::SolMechDeploy: out = yes(spendable(g, p, false) >= 5); return true;
        case Ab::NaaluAgent:  // Z'eu: the (old) peek is for people; Z'eu Omega's free tactical action goes to itself
            if (yn) out = yes(false);
            else out = one(g.fixes >= 2 && g.agentFor[p] >= 0 && has(d, g.agentFor[p]) ? g.agentFor[p] : p);  // promised: theirs
            return true;
        case Ab::AiDevAlgorithm: {  // 0.30.0: keep the planet when money is tight, else keep the card for production
            if (d.prompt.find("instead of your other waiver") != std::string::npos) {  // (0.44.9: AIDA before a one-shot waiver)
                out = yes(true);
                return true;
            }
            int cost = 6;
            out = yes(spendable(g, p, false) < cost + 4);
            return true;
        }
        case Ab::LetnevMechDeploy: out = yes(spendable(g, p, false) >= 4); return true;
        case Ab::SolCommander:
        case Ab::Tekklar:
        case Ab::Brilliant:
        case Ab::ResearchAgreement:
        case Ab::JolnarCommander:
        case Ab::XxchaAgent:
            if (yn) {
                out = yes(true);
                return true;
            }
            if (ab == Ab::XxchaAgent) {  // ready our best exhausted planet
                int x = bestBy(d, [&](int id) { return g.planets[id].owner == p ? planetWorth(g, id) : -1.f; }, 0.f);
                out = one(x >= 0 ? x : (has(d, -1) ? -1 : firstReal(d)));
                return true;
            }
            out = one(firstReal(d));
            return true;
        case Ab::SolAgent:
        case Ab::LetnevAgent: {
            int c = ownOption(g, d, p);
            out = one(c >= 0 ? c : -1);
            return true;
        }
        case Ab::Munitions: {
            int sys = g.activeSystem;
            bool hard = true;
            if (sys >= 0) {
                int mine = combatStrength(g, p, sys, true), theirs = 0;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) theirs = std::max(theirs, combatStrength(g, q, sys, true));
                hard = theirs * 10 >= mine * 6;
            }
            out = yes(hard || me.tradeGoods >= 6);
            return true;
        }
        case Ab::WarFunding: out = yes(outgunned(g, p)); return true;
        case Ab::WrathOfKenara: {
            int n = std::min<int>(int(d.options.size()), std::max(0, int(me.tradeGoods)));
            n = std::min(n, d.maxPick);
            out = Answer();
            for (const Option& o : d.options)
                if (int(out.picks.size()) < n) out.picks.push_back(o.id);
            return true;
        }
        case Ab::QuantumDatahub: {
            // 0.44.5: the cards as this player values them now (not a fixed table), against the price - 3 trade
            // goods, a strategy token - and a little for what the other player gains or loses by the swap
            int best = -1;
            float bv = 4.5f;
            if (me.tradeGoods >= 3 + aiGoals(g, p).keepTg)
                for (const Option& o : d.options) {
                    if (o.id < 0) continue;
                    const int mine = o.id / 100, theirs = o.id % 100;
                    const int q = theirs >= 0 && theirs < kStrategyCards ? g.cardOwner[theirs] : -1;
                    float gain = aiStrategyCardValue(g, p, theirs) - aiStrategyCardValue(g, p, mine);
                    if (q >= 0 && q < g.numPlayers) gain -= 0.3f * (aiStrategyCardValue(g, q, mine) - aiStrategyCardValue(g, q, theirs));
                    if (gain > bv) {
                        bv = gain;
                        best = o.id;
                    }
                }
            out = one(best);
            return true;
        }
        case Ab::ProductionBiomes: {  // give the 2 trade goods to the player furthest behind
            int q = bestBy(d, [&](int id) { return -float(g.players[id].vp); });
            out = one(q >= 0 ? q : firstReal(d));
            return true;
        }
        case Ab::HacanAgent: out = one(has(d, p) ? p : -1); return true;
        case Ab::HacanCommander: {
            int k = me.tradeGoods >= 6 ? 2 : 0;
            out = one(has(d, k) ? k : 0);
            return true;
        }
        case Ab::HacanHero: out = yes(g.round >= 4); return true;
        case Ab::SpatialConduit: {
            // worth it when ships elsewhere can't reach the active system normally
            int sys = g.activeSystem, newly = 0;
            if (sys >= 0)
                for (size_t s = 0; s < g.systems.size(); ++s) {
                    if (int(s) == sys || g.systems[s].hasToken(p)) continue;
                    const Force& f = g.systems[s].space[p];
                    for (int t : kShipIdx)
                        if (f.n[t] && unitDef(g, p, UnitType(t)).move > 0 && !shipReach(g, p, int(s), sys, UnitType(t)).ok)
                            newly += f.n[t];
                }
            bool enemy = false;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && sys >= 0 && hasUnits(g, q, sys)) enemy = true;
            // (0.44.7: also to land on an empty planet - unowned or someone else's - with 2 or more ships coming in)
            bool planet = false;
            if (sys >= 0)
                for (int16_t x : g.systems[size_t(sys)].planets) planet = planet || g.planets[size_t(x)].owner != p;
            out = yes(newly >= 2 && (enemy || planet));
            return true;
        }
        case Ab::JolnarAgent: {
            if (yn) {
                out = yes(d.prompt.find("to let") == std::string::npos);
                return true;
            }
            int planets = 0;
            for (int x : controlledPlanets(g, p))
                if (g.planets[x].units[p].count(UnitType::Infantry)) ++planets;
            int spare = std::max(0, unitsOnBoard(g, p, UnitType::Infantry) - planets);
            int k = 0;
            for (const Option& o : d.options)
                if (o.id <= spare) k = std::max(k, o.id);
            out = one(k);
            return true;
        }
        case Ab::JolnarHero: {
            if (d.prompt.find(" with?") != std::string::npos) {  // choosing the replacement
                int t = bestBy(d, [&](int id) { return aiTechWant(g, p, id); });
                out = one(t >= 0 ? t : -1);
                return true;
            }
            // which technology to replace: a weak one whose colour has a better choice
            int best = -1;
            float bv = 0.9f;
            for (const Option& o : d.options) {
                if (o.id < 0) continue;
                float want = 0;
                for (int u = 0; u < kTechCount; ++u)
                    if (kTechs[u].color == kTechs[o.id].color && !kTechs[u].unitUpgrade && techAvailableToResearch(g, p, u))
                        want = std::max(want, aiTechWant(g, p, u));
                float gain = want - keepValue(g, p, o.id);
                if (gain > bv) {
                    bv = gain;
                    best = o.id;
                }
            }
            out = one(best);
            return true;
        }
        case Ab::Exotrireme: {
            if (yn) {
                int sys = g.activeSystem >= 0 ? g.activeSystem : 0;
                int strong = 0;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) {
                        const Force& f = g.systems[sys].space[q];
                        strong += f.count(UnitType::WarSun) * 3 + f.flagships() * 3 + f.count(UnitType::Dreadnought) * 2 +
                                  f.count(UnitType::Carrier) + f.count(UnitType::Cruiser);
                    }
                out = yes(strong >= 3);
                return true;
            }
            static const UnitType kOrder[] = {UnitType::WarSun, UnitType::Flagship, UnitType::Dreadnought, UnitType::Carrier,
                                              UnitType::Cruiser, UnitType::Destroyer, UnitType::Fighter};
            out = Answer();
            for (UnitType t : kOrder)
                for (const Option& o : d.options)
                    if (o.id / 16 == int(t) && int(out.picks.size()) < d.maxPick) out.picks.push_back(o.id);
            if (int(out.picks.size()) < d.minPick) out.picks.push_back(firstReal(d));
            return true;
        }
        case Ab::SardakkCommander: {
            // bring ground forces from planets that keep at least one
            out = Answer();
            int sys = g.activeSystem;
            bool target = false;
            if (sys >= 0)
                for (int16_t x : g.systems[sys].planets)
                    if (g.planets[x].owner != p && !isDMZ(g, x)) target = true;
            if (target)
                for (const Option& o : d.options)
                    if (o.id >= 0 && g.planets[o.id].units[p].groundForces() >= 2 && g.planets[o.id].system != sys)
                        out.picks.push_back(o.id);
            return true;
        }
        case Ab::SardakkCommanderUnit: {
            // the mech (it can sustain damage) when the planet keeps an infantry behind; else the infantry
            const int x = d.value;
            const bool mech = x >= 0 && x < int(g.planets.size()) && g.planets[size_t(x)].units[size_t(p)].count(UnitType::Infantry) >= 1;
            out = Answer();
            out.picks.push_back(mech ? 1 : 0);
            return true;
        }
        case Ab::SardakkHero: {
            int sys = g.activeSystem;
            int mine = sys >= 0 ? combatStrength(g, p, sys, true) : 0, theirs = 0;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && sys >= 0) theirs = std::max(theirs, combatStrength(g, q, sys, true));
            out = yes(theirs > mine && sys >= 0 && g.systems[sys].space[p].groundForces() >= 3);
            return true;
        }
        case Ab::PeaceAccords: {
            int x = bestBy(d, [&](int id) { return planetWorth(g, id) + (g.planets[id].owner < 0 ? 1.f : 0.f); });
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::Quash:
        case Ab::PoliticalFavor: {
            int a = d.agenda >= 0 ? d.agenda : g.currentAgenda;
            bool use = false;
            if (a >= 0 && (ab == Ab::PoliticalFavor || me.strategy >= 2)) {
                float conf = 0;
                int o = aiPredictOutcome(g, a, &conf);
                use = aiOutcomeValue(g, p, a, o) < -1.0f;
            }
            out = yes(use);
            return true;
        }
        case Ab::InstinctTraining: {
            // 0.44.6: a Sabotage against our own good card - cancel the Sabotage; a strong card against us - the last
            // strategy token is worth it too (it used to need 2, which the secondaries had usually taken)
            if (d.value > 0) {
                const int owner = (d.value - 1) / 1000, card = (d.value - 1) % 1000;
                out = yes(owner == p && me.strategy >= 1 && aiCardKeepValue(card) >= 1.5f);
                return true;
            }
            const bool strong = d.card >= 0 && aiCardKeepValue(d.card) >= 2.f;
            out = yes((me.strategy >= 2 || (me.strategy >= 1 && strong)) && d.card >= 0 && aiWantsCancel(g, p, d.card, d.targetPlayer));
            return true;
        }
        case Ab::NullificationField: out = yes(me.strategy >= 2); return true;
        case Ab::XxchaHero: {
            if (teXxchaHero(g)) {  // (0.57.1) where the PDS / mechs go: spent planets first (they are readied), then the
                // planets worth most, mechs where enemies are near, PDS where there are none yet
                int best = -1;
                float bv = -1e9f;
                for (const Option& o : d.options) {
                    if (!o.enabled || o.id < 0) continue;
                    const int x = o.id / 2;
                    const bool mech = o.id % 2;
                    const PlanetState& ps = g.planets[size_t(x)];
                    float v = float(planetResources(g, x) + planetInfluence(g, x)) * 0.3f;
                    if (ps.exhausted) v += 3.f + float(std::max(planetResources(g, x), planetInfluence(g, x)));
                    bool enemies = false;
                    for (int16_t a : g.systems[size_t(ps.system)].adj)
                        for (int q = 0; q < g.numPlayers; ++q)
                            if (q != p && g.systems[size_t(a)].space[size_t(q)].ships() > 0) enemies = true;
                    const int pdsHere = ps.units[size_t(p)].count(UnitType::PDS);
                    if (mech) v += enemies ? 1.5f : 0.6f;
                    else v += pdsHere == 0 ? 1.2f : 0.2f;
                    if (ps.units[size_t(p)].total() == 0) v += 0.5f;
                    if (v > bv) bv = v, best = o.id;
                }
                out = one(best >= 0 ? best : firstReal(d));
                return true;
            }
            if (d.maxPick > 1 || d.minPick != 1) {  // which agendas
                std::vector<std::pair<float, int>> v;
                for (const Option& o : d.options) {
                    if (!o.enabled || o.id < 0) continue;
                    v.push_back({-aiOutcomeValue(g, p, o.id, 0), o.id});
                }
                std::sort(v.begin(), v.end());
                out = Answer();
                for (auto& x : v)
                    if (int(out.picks.size()) < d.maxPick) out.picks.push_back(x.second);
                return true;
            }
            int a = d.agenda;
            int o = bestBy(d, [&](int id) { return a >= 0 ? aiOutcomeValue(g, p, a, id) : 0.f; });
            out = one(o >= 0 ? o : firstReal(d));
            return true;
        }
        // ---------------- batch 2
        case Ab::Mitosis:
        case Ab::YinSpinner:
        case Ab::YssarilMechDeploy: {
            int x = bestBy(d, [&](int id) {
                if (id >= 20000) {  // Yin Spinner Omega: in space with ships (cargo for an invasion) - 0.44.6: only with room
                    const int s = id - 20000;
                    if (s < 0 || s >= int(g.systems.size())) return -1.f;
                    const int room = shipCapacity(g, p, s) + dockFighterSlots(g, p, s) - transportedInSpace(g, p, s);
                    return room > 0 ? 0.3f : -1.f;
                }
                int pl = id % 10000;
                return defendScore(g, p, pl) + (id >= 10000 ? 0.5f : 0.f);
            });
            out = one(x >= 0 ? x : firstReal(d));
            return true;
        }
        case Ab::Bioplasmosis: {
            if (d.prompt.find("from which") != std::string::npos) {
                int x = bestBy(d, [&](int id) {
                    int n = g.planets[id].units[p].count(UnitType::Infantry);
                    return n >= 4 ? float(n) - defendScore(g, p, id) : -1.f;
                }, 0.f);
                out = one(x >= 0 ? x : -1);
                return true;
            }
            if (d.prompt.find("how many") != std::string::npos) {
                int k = 1;
                for (const Option& o : d.options) k = std::max(k, o.id);
                out = one(std::max(1, k / 2));
                return true;
            }
            // (0.44.6) only where they are clearly needed more than where they stand - moving half of them makes the
            // source weaker and the target stronger, so the gap must be bigger than that (it used to move them back and
            // forth between two planets)
            const int src = d.value - 1;
            if (src >= 0 && src < int(g.planets.size())) {
                const float k = float(std::max(1, g.planets[size_t(src)].units[size_t(p)].count(UnitType::Infantry) / 2));
                const float base = defendScore(g, p, src);
                int y = bestBy(d, [&](int id) { return defendScore(g, p, id) - base - k - 1.f; }, 0.f);
                out = one(y >= 0 ? y : -1);
                return true;
            }
            int y = bestBy(d, [&](int id) { return defendScore(g, p, id); });
            out = one(y >= 0 ? y : -1);
            return true;
        }
        case Ab::ArborecFlagship: out = yes(spendable(g, p, false) >= 3); return true;
        case Ab::ArborecCommander: out = yes(spendable(g, p, false) >= 2); return true;
        case Ab::Stymie: {
            // freeze the most ships of the player whose token it is (Stymie Omega: the mover - the active player)
            int a = codexOn(g) && g.activePlayer >= 0 ? int(g.activePlayer) : playerOfFaction(g, "arborec");
            int s = bestBy(d, [&](int id) { return a >= 0 ? float(g.systems[id].space[a].ships()) : 0.f; }, 0.5f);
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::ArborecAgent: {
            int c = bestBy(d, [&](int id) {
                int t = (id / 16) % 16, u = id % 16;
                return float(unitCost2(g, p, UnitType(u)) - unitCost2(g, p, UnitType(t))) + (u == int(UnitType::Dreadnought) ? 2.f : 0.f);
            });
            out = one(c >= 0 ? c : -1);
            return true;
        }
        case Ab::ArborecHero: {
            int s = spendable(g, p, false) >= 3 ? firstReal(d) : -1;
            out = one(has(d, s) ? s : -1);
            return true;
        }
        case Ab::L1Z1XHero: {
            if (d.maxPick > 1 || d.minPick != 1 || d.prompt.find("which ships") != std::string::npos) {  // which ships: all of them
                out = Answer();
                for (const Option& o : d.options)
                    if (o.enabled && o.id >= 0) out.picks.push_back(o.id);
                return true;
            }
            int s = bestBy(d, [&](int id) {
                float v = 0;
                for (int16_t a : g.systems[id].adj)
                    for (int16_t x : g.systems[a].planets)
                        if (g.planets[x].owner >= 0 && g.planets[x].owner != p) v += planetWorth(g, x);
                return v;
            });
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::Ambush: {
            out = Answer();
            for (const Option& o : d.options)
                if (int(out.picks.size()) < d.maxPick) out.picks.push_back(o.id);
            return true;
        }
        case Ab::Pillage: out = one(has(d, 0) ? 0 : has(d, 1) ? 1 : -1); return true;
        case Ab::SalvageOps: {
            int t = bestBy(d, [&](int id) { return float(unitCost2(g, p, UnitType(id))); });
            out = one(t >= 0 && spendable(g, p, false) >= unitCost2(g, p, UnitType(t)) / 2 + 2 ? t : -1);
            return true;
        }
        case Ab::MentakHero: out = yes(enemyNonFighters(g, p, g.activeSystem) >= 3 || g.round >= 6); return true;
        case Ab::Foresight: {
            if (!outgunned(g, p) || g.activeSystem < 0) {
                out = one(-1);
                return true;
            }
            int to = bestBy(d, [&](int id) { return hasUnits(g, p, id) ? 1.f : 0.f; });
            out = one(to >= 0 ? to : firstReal(d));
            return true;
        }
        case Ab::Indoctrination: {
            int c = has(d, 1) && spendable(g, p, true) >= 6 ? 1 : spendable(g, p, true) >= 4 ? 0 : -1;
            out = one(c);
            return true;
        }
        case Ab::Devotion:
        case Ab::ImpulseCore: {
            int sys = g.activeSystem;
            bool worth = enemyNonFighters(g, p, sys) >= 1 && (outgunned(g, p) || ab == Ab::ImpulseCore);
            int t = has(d, int(UnitType::Destroyer)) ? int(UnitType::Destroyer) : has(d, int(UnitType::Cruiser)) ? int(UnitType::Cruiser) : -1;
            out = one(worth ? t : -1);
            return true;
        }
        case Ab::YinHero: {
            int x = bestBy(d, [&](int id) {
                int def = 0;
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p) def += g.planets[id].units[q].groundForces();
                return def <= 1 ? planetWorth(g, id) - float(def) * 2.f : -1.f;
            }, 0.5f);
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::StallTactics: {
            int c = bestBy(d, [&](int id) { return 5.f - aiCardKeepValue(id); });
            out = one(c >= 0 ? c : -1);
            return true;
        }
        case Ab::MageonImplants:
        case Ab::SpyNet: {
            if (yn) {
                out = yes(true);
                return true;
            }
            if (d.prompt.find("whose") != std::string::npos) {
                int q = bestBy(d, [&](int id) { return float(g.players[id].actionCards.size()); });
                out = one(q >= 0 ? q : -1);
                return true;
            }
            int c = bestBy(d, [&](int id) { return aiCardKeepValue(id); });
            out = one(c >= 0 ? c : firstReal(d));
            return true;
        }
        case Ab::YssarilHero: {
            if (d.prompt.find("Kyver: show") != std::string::npos) {
                int c = bestBy(d, [&](int id) { return 5.f - aiCardKeepValue(id); });
                out = one(c >= 0 ? c : firstReal(d));
                return true;
            }
            out = one(numberBefore(d.options.size() > 1 ? d.options[1].detail : std::string(), " action cards") >= 5 ? 0 : 1);
            return true;
        }
        // ---------------- batch 3
        case Ab::WormholeGenerator:
        case Ab::CreussIff:
        case Ab::IcarusDrive: {
            if (yn) {
                if (ab == Ab::IcarusDrive) {
                    bool offBoard = false;
                    for (int16_t t : g.creussTokens)
                        if (t < 0) offBoard = true;
                    out = yes(offBoard && g.activeSystem >= 0 && !wormholesOf(g, g.activeSystem));
                } else {
                    out = yes(true);
                }
                return true;
            }
            if (d.prompt.find("Which Creuss") != std::string::npos) {
                // the token that gains most by a new place (one not on the board yet, or one badly placed)
                int k = -1;
                float kv = -1e9f;
                for (int i = 0; i < 3; ++i) {
                    if (!has(d, i)) continue;
                    float best = -9.f;
                    for (size_t s = 0; s < g.systems.size(); ++s) best = std::max(best, creussSpot(g, p, int(s), i));
                    const float now = g.creussTokens[i] >= 0 ? creussSpot(g, p, g.creussTokens[i], i) : -1.f;
                    if (best - now > kv) kv = best - now, k = i;
                }
                // (0.44.9: the original Wormhole Generator in the status phase - only when a token gains by it)
                if (g.phase == Phase::Status && kv < 0.5f && has(d, -1)) k = -1;
                out = one(k >= 0 ? k : (g.phase == Phase::Status && has(d, -1) ? -1 : firstReal(d)));
                return true;
            }
            const int k = d.prompt.find("alpha") != std::string::npos ? 0 : d.prompt.find("beta") != std::string::npos ? 1 : 2;
            int s = bestBy(d, [&](int id) { return creussSpot(g, p, id, k); });
            out = one(s >= 0 ? s : firstReal(d));
            return true;
        }
        case Ab::CreussHero: {
            // bring the strongest fleet next to the most valuable targets
            int a = -1, best = 0;
            for (const Option& o : d.options) {
                int v = combatStrength(g, p, o.id, true);
                if (v > best) best = v, a = o.id;
            }
            int b = -1;
            float bv = 0;
            for (const Option& o : d.options) {
                if (o.id == a) continue;
                float v = targetValue(g, p, o.id) - (a >= 0 ? targetValue(g, p, a) : 0.f);
                if (v > bv) bv = v, b = o.id;
            }
            out = Answer();
            if (a >= 0 && b >= 0) {
                out.picks = {a, b};
            } else {
                for (const Option& o : d.options)
                    if (out.picks.size() < 2) out.picks.push_back(o.id);
            }
            return true;
        }
        case Ab::DimensionalSplicer: {
            if (yn) {  // (0.44.9) "before Assault Cannon?" - no: the cannon first, the Splicer then picks from what is left
                out = yes(false);
                return true;
            }
            // (0.44.8) the combat's system and opponent come with the question (neutral units too); a hit that a
            // Sustain Damage would just absorb is worth little
            int t = bestBy(d, [&](int id) {
                if (id < 0) return -1.f;
                int sys = d.value >= 0 && d.value < int(g.systems.size()) ? d.value : (g.activeSystem >= 0 ? g.activeSystem : 0);
                int o = d.targetPlayer >= 0 && d.targetPlayer < kSides ? d.targetPlayer : -1;
                for (int q = 0; q < g.numPlayers && o < 0; ++q)
                    if (q != p && g.systems[sys].space[q].n[id]) o = q;
                if (o < 0) return 0.f;
                const Force& f = g.systems[sys].space[o];
                bool canSustain = unitSustains(g, o, UnitType(id)) && f.dmg[id] < f.n[id];
                const float c = float(unitDef(g, o, UnitType(id)).cost2);
                return (canSustain ? 0.3f * c : c) + 1.f;
            });
            out = one(t >= 0 ? t : firstReal(d));
            return true;
        }
        case Ab::StarForge: {
            if (d.prompt.find("place what") != std::string::npos) {
                int sys = -1;
                for (size_t s = 0; s < g.systems.size(); ++s)
                    if (g.systems[s].space[p].count(UnitType::WarSun)) sys = int(s);
                int room = sys >= 0 ? shipCapacity(g, p, sys) - g.systems[sys].space[p].count(UnitType::Fighter) -
                                          g.systems[sys].space[p].groundForces()
                                    : 0;
                int t = room >= 2 && has(d, int(UnitType::Fighter)) ? int(UnitType::Fighter)
                        : has(d, int(UnitType::Destroyer))          ? int(UnitType::Destroyer)
                                                                    : firstReal(d);
                out = one(t);
                return true;
            }
            out = one(bestBy(d, [&](int id) { return targetValue(g, p, id); }));
            return true;
        }
        case Ab::MuaatHero: {
            int sys = g.activeSystem;
            bool mine = false, theirs = false;
            if (sys >= 0) {
                for (int16_t x : g.systems[sys].planets) {
                    if (g.planets[x].owner == p) mine = true;
                    else if (g.planets[x].owner >= 0) theirs = true;
                }
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p && hasUnits(g, q, sys)) theirs = true;
            }
            out = yes(theirs && !mine);
            return true;
        }
        case Ab::SaarCommander: {  // send the new units to the dock with enemy ships next to it
            auto threat = [&](int s) {
                int n = 0;
                for (int16_t a : g.systems[s].adj)
                    if (otherShipsIn(g, p, a)) ++n;
                return n;
            };
            int here = g.activeSystem >= 0 ? threat(g.activeSystem) : 0;
            int c = bestBy(d, [&](int id) { return float(threat(id / 1000)); }, float(here));
            out = one(c >= 0 && here == 0 ? c : -1);
            return true;
        }
        case Ab::SaarHero: {
            int s = bestBy(d, [&](int id) { return float(enemyLight(g, p, id)); });
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::ChaosMapping: {
            // (0.44.7) home, or - once the floating factories have left it - the factory facing the most enemy ships
            if (spendable(g, p, false) < 3) {
                out = one(-1);
                return true;
            }
            const int home = me.homeSystem;
            int s = bestBy(d, [&](int id) {
                if (id < 0 || id >= int(g.systems.size())) return -1.f;
                float v = id == home ? 1.f : 0.5f;
                std::vector<int> zone{id};
                for (int16_t a : g.systems[size_t(id)].adj) zone.push_back(a);
                for (int z : zone)
                    for (int q = 0; q < g.numPlayers; ++q)
                        if (q != p) v += 0.5f * float(g.systems[size_t(z)].space[size_t(q)].nonFighterShips());
                return v;
            }, 0.f);
            out = one(s);
            return true;
        }
        case Ab::WinnuAgent: out = yes(spendable(g, p, false) >= 2); return true;
        case Ab::WinnuHero: {
            static const float kPrimary[8] = {1.0f, 0.6f, 1.0f, 0.9f, 0.7f, 0.9f, 1.4f, 1.3f};
            int c = bestBy(d, [&](int id) { return id >= 0 && id < 8 ? kPrimary[id] : 0.f; });
            out = one(c >= 0 ? c : firstReal(d));
            return true;
        }
        case Ab::GalacticThreat: {
            float conf = 0;
            int a = d.agenda >= 0 ? d.agenda : g.currentAgenda;
            int o = a >= 0 ? aiPredictOutcome(g, a, &conf) : -1;
            out = one(has(d, o) ? o : firstReal(d));
            return true;
        }
        case Ab::TechSingularity: {
            int t = bestBy(d, [&](int id) {
                if (id >= 30000) {  // Valefar Assimilator Z: that faction's flagship abilities on ours
                    const char* k = kFactions[id - 30000].key;
                    float v = 2.5f;
                    for (const char* strong : {"sardakk", "winnu", "jolnar", "letnev", "mahact", "l1z1x", "naaz", "argent"})
                        if (!std::strcmp(k, strong)) v = 3.8f;
                    return v * (unitsOnBoard(g, p, UnitType::Flagship) > 0 ? 1.f : 0.7f);
                }
                float v = aiTechWant(g, p, id % 10000) + (id >= 10000 ? 1.f : 0.f);
                // 0.43.0 (owner: "verify the Nekro steal with Valefar"): moving a placed token gives that technology
                // up - X was moved again and again while Y stayed unused
                if (id >= 10000) {
                    const int cur = id >= 20000 ? me.assimY : me.assimX;
                    if (cur >= 0) v -= aiTechWant(g, p, cur) + 0.5f;
                }
                return v;
            });
            out = one(t >= 0 ? t : -1);
            return true;
        }
        case Ab::NekroAgent: {
            int c = bestBy(d, [&](int id) {
                if (id == 1000) return me.tactic >= 3 ? 1.f : -1.f;
                if (id >= 1000) return -1.f;
                return 3.f - aiCardKeepValue(id);
            }, 0.f);
            out = one(c);
            return true;
        }
        case Ab::NekroHero: {
            if (d.prompt.find("gain which") != std::string::npos) {
                int t = bestBy(d, [&](int id) { return aiTechWant(g, p, id); });
                out = one(t >= 0 ? t : firstReal(d));
                return true;
            }
            int x = bestBy(d, [&](int id) { return planetWorth(g, id); });
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::Aetherstream: {  // (0.44.7) ours: always; a neighbour's: as with Aetherpassage - behind us, not coming at us
            const int a = d.targetPlayer;
            if (a < 0 || a == p) {
                out = yes(true);
                return true;
            }
            out = yes(g.players[size_t(a)].vp < me.vp && !hostileFlag(g, p, a) && (g.activeSystem < 0 || !hasUnits(g, p, g.activeSystem)));
            return true;
        }
        case Ab::Aetherpassage: {
            int a = g.activePlayer;
            bool ok = a >= 0 && g.players[a].vp < me.vp && (g.activeSystem < 0 || !hasUnits(g, p, g.activeSystem));
            out = yes(ok);
            return true;
        }
        case Ab::Dynamo: out = yes(spendable(g, p, true) >= 4); return true;
        case Ab::Watcher: out = yes(d.card >= 0 && aiWantsCancel(g, p, d.card, d.targetPlayer)); return true;
        // ---------------- batch 4
        case Ab::RaidFormation:
        case Ab::Vortex: {
            int t = -1;
            if (ab == Ab::RaidFormation) {
                int o = -1;  // the opponent in the active combat
                for (int q = 0; q < g.numPlayers; ++q)
                    if (q != p && g.activeSystem >= 0 && hasShips(g, q, g.activeSystem)) o = q;
                t = bestBy(d, [&](int id) { return o >= 0 ? float(unitCost2(g, o, UnitType(id))) : 0.f; });
            } else {
                t = bestBy(d, [&](int id) { return id < 0 ? -1.f : vortexWorth(g, p, id / 16, id % 16); });
            }
            out = one(t >= 0 ? t : firstReal(d));
            return true;
        }
        case Ab::ArgentAgent: {
            if (yn) {
                out = yes(true);
                return true;
            }
            if (d.prompt.find("how many") != std::string::npos) {
                int best = -1, bk = 0;
                for (const Option& o : d.options)
                    if (o.id % 16 > bk) bk = o.id % 16, best = o.id;
                out = one(best >= 0 ? best : firstReal(d));
                return true;
            }
            int x = bestBy(d, [&](int id) { return defendScore(g, p, id); });
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::ArgentHero: {
            if (d.maxPick > 1 || d.minPick != 1) {  // which ships: everything not guarding home
                out = Answer();
                for (const Option& o : d.options)
                    if (o.enabled && o.id >= 0 && g.systems[o.id / 16].home != p) out.picks.push_back(o.id);
                return true;
            }
            for (const Option& o : d.options)
                if (o.id < 0 && o.label == "Done") {
                    out = one(-1);
                    return true;
                }
            // (0.43.0: for Raise a Fleet / Command an Armada - where most of the fleet already is)
            const bool fleetGoal = aiLevelScale(g) > 0.f && aiGoals(g, p).fleetTarget > 0;
            int s = bestBy(d, [&](int id) { return fleetGoal ? float(g.systems[id].space[p].nonFighterShips()) + targetValue(g, p, id) * 0.01f
                                                             : targetValue(g, p, id); });
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::MahactAgent: {
            int s = me.strategy <= 2 ? firstReal(d) : -1;
            out = one(has(d, s) ? s : firstReal(d));
            return true;
        }
        case Ab::MahactHero: {
            float v = 0;
            int c = bestHurl(g, p, &v);
            out = one(has(d, c) ? c : -1);
            return true;
        }
        case Ab::CrimsonLegionnaire: out = one(has(d, 1) ? 1 : 0); return true;
        case Ab::GeneticRecombination: {
            const int a = d.agenda >= 0 ? d.agenda : g.currentAgenda;
            if (d.prompt.find(": make ") != std::string::npos) {  // Mahact: which outcome to force
                const int voter = d.value;
                int o = -1;
                float bv = 1.0f;
                if (voter >= 0 && voter < g.numPlayers && availableVotes(g, voter) >= 3 && a >= 0)
                    for (const Option& op : d.options) {
                        if (op.id == kNoVote) continue;
                        float v = aiOutcomeValue(g, p, a, op.id);
                        if (v > bv) bv = v, o = op.id;
                    }
                out = one(o >= 0 ? o : kNoVote);
                return true;
            }
            // the voter: vote for it unless it hurts more than a fleet token
            bool vote = a < 0 || aiOutcomeValue(g, p, a, d.value) >= -1.5f || !has(d, 0);
            out = one(vote ? 1 : 0);
            return true;
        }
        case Ab::Scepter: {
            int s = bestBy(d, [&](int id) {
                float v = 0;
                for (int16_t x : g.systems[id].planets)
                    if (g.planets[x].owner == p) v += planetWorth(g, x);
                return v;
            });
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::DistantSuns: {
            int c = bestBy(d, [&](int id) {
                switch (kExploreCards[id].kind) {
                    case ExploreKind::Attach: return 3.f;
                    case ExploreKind::Fragment: return 2.5f;
                    case ExploreKind::Instant: return 2.f;
                    default: return 1.f;
                }
            });
            out = one(c >= 0 ? c : firstReal(d));
            return true;
        }
        case Ab::Fabrication:
        case Ab::BlackMarketForgery: {
            int c = -1;
            for (const Option& o : d.options)
                if (o.id >= 0 && o.id < 10 && c < 0) c = o.id;
            if (c < 0 && has(d, 10)) c = 10;
            out = one(c);
            return true;
        }
        case Ab::NaazAgent: {
            int x = bestBy(d, [&](int id) { return planetWorth(g, id) + (g.planets[id].units[p].count(UnitType::Mech) ? 3.f : 0.f); });
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::NaazHero: {
            static const float kSec[8] = {0.6f, 0.3f, 0.6f, 0.8f, 0.5f, 0.5f, 1.0f, 0.7f};
            std::vector<std::pair<float, int>> v;
            for (const Option& o : d.options)
                if (o.enabled && o.id >= 0 && o.id < 8) v.push_back({-(kSec[o.id] + (o.id == 6 && spendable(g, p, false) >= 4 ? 0.5f : 0.f)), o.id});
            std::sort(v.begin(), v.end());
            out = Answer();
            for (auto& x : v)
                if (int(out.picks.size()) < d.maxPick) out.picks.push_back(x.second);
            return true;
        }
        case Ab::Supercharge: {
            // (0.44.8) a close fight - neutral fleets count; on the ground only against 2+ defenders or a close fight
            int sys = g.activeSystem;
            bool close = true;
            if (sys >= 0 && g.groundCombatPlanet < 0) {
                int mine = combatStrength(g, p, sys, true), theirs = 0;
                for (int q = 0; q < kSides; ++q)
                    if (q != p && (q < g.numPlayers || q == kNeutral)) theirs = std::max(theirs, combatStrength(g, q, sys, true));
                close = theirs * 10 >= mine * 4;
            } else if (g.groundCombatPlanet >= 0) {
                const int x = g.groundCombatPlanet;
                int mine = groundForcesOn(g, p, x), theirs = 0;
                for (int q = 0; q < kSides; ++q)
                    if (q != p) theirs += groundForcesOn(g, q, x);
                close = theirs >= 2 || theirs * 10 >= mine * 5;
            }
            out = yes(close);
            return true;
        }
        case Ab::NomadMercer: {
            const int np = int(g.planets.size());
            auto stackAt = [&](int loc) {
                if (loc < 0) return 0;
                return loc < np ? g.planets[size_t(loc)].units[size_t(p)].groundForces()
                                : loc - np < int(g.systems.size()) ? g.systems[size_t(loc - np)].space[size_t(p)].groundForces() : 0;
            };
            if (yn) {
                bool want = false;
                const int act = g.activeSystem >= 0 ? g.activeSystem : d.value > 0 && d.value < int(g.systems.size()) ? d.value : -1;
                if (act >= 0)
                    for (int16_t x : g.systems[act].planets)
                        if (g.planets[x].owner == p && defendScore(g, p, x) > 1.0f) want = true;
                // (0.55.0: asked 67 times in 80 games, never used) also to stage spare troops at the front for free: from
                // a stack of 3+ to our planets here when enemies or Mecatol Rex are within 2 (it readies every round)
                if (!want && act >= 0 && g.round >= 2 && g.systems[act].home != p) {
                    bool front = false;
                    for (int x : systemsWithin(g, act, 2)) {
                        if (x == g.mecatol()) front = true;
                        for (int q = 0; q < g.numPlayers && !front; ++q)
                            if (q != p && (g.systems[size_t(x)].space[size_t(q)].nonFighterShips() > 0 || g.systems[size_t(x)].home == q))
                                front = true;
                    }
                    int spare = 0;
                    for (size_t x = 0; x < g.planets.size(); ++x)
                        if (g.planets[x].system != act) spare = std::max(spare, g.planets[x].units[size_t(p)].groundForces() - 2);
                    want = front && spare >= 1 && aiTune("mercer", 1.f) > 0.f;
                }
                out = yes(want);
                return true;
            }
            if (d.maxPick > 1 || d.minPick != 1) {  // which ground forces: from the biggest stacks
                std::vector<int> ids;
                for (const Option& o : d.options)
                    if (o.id >= 0) ids.push_back(o.id);
                std::stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return stackAt(a / 64) > stackAt(b / 64); });
                out = Answer();
                for (int id : ids)
                    if (int(out.picks.size()) < d.maxPick) out.picks.push_back(id);
                if (out.picks.empty()) out.picks.push_back(firstReal(d));
                return true;
            }
            int x = bestBy(d, [&](int id) { return defendScore(g, p, id); });
            out = one(x >= 0 ? x : firstReal(d));
            return true;
        }
        case Ab::NomadThundarian: {
            const int mine = d.value / 100, theirs = d.value % 100;
            out = yes(theirs - mine >= 2 || (mine == 0 && theirs >= 2));
            return true;
        }
        case Ab::NomadArtuno: out = yes(d.value >= 2); return true;
        case Ab::QuantumManipulator:
        case Ab::Amalgamation: {
            int k = -1;
            for (const Option& o : d.options) k = std::max(k, o.id);
            out = one(k);
            return true;
        }
        case Ab::Cavalry: {
            if (yn) {
                out = yes(true);
                return true;
            }
            int t = bestBy(d, [&](int id) { return float(unitDef(g, p, UnitType(id)).combat); });
            out = one(t >= 0 ? t : -1);
            return true;
        }
        case Ab::Terragenesis: out = one(has(d, 10000) ? 10000 : -1); return true;
        case Ab::Hecatoncheires: {
            int x = g.groundCombatPlanet;
            bool threat = false;
            for (int y : controlledPlanets(g, p))
                if (defendScore(g, p, y) > 1.5f && (d.prompt.find(std::string(kPlanets[g.planets[y].def].name)) != std::string::npos)) threat = true;
            (void)x;
            out = yes(threat || unitsOnBoard(g, p, UnitType::PDS) >= 4);
            return true;
        }
        case Ab::TitansAgent: {
            const int hits = d.value;
            bool use = false;
            if (g.groundCombatPlanet >= 0) {
                use = hits >= groundForcesOn(g, p, g.groundCombatPlanet);
            } else if (g.activeSystem >= 0) {
                const Force& f = g.systems[g.activeSystem].space[p];
                int cheap = f.count(UnitType::Fighter);
                for (int t : kShipIdx)
                    if (unitSustains(g, p, UnitType(t))) cheap += f.n[t] - f.dmg[t];
                use = hits > cheap;
            }
            out = yes(use || hits >= 3);
            return true;
        }
        case Ab::Terraform: {
            int x = bestBy(d, [&](int id) { return planetWorth(g, id); });
            out = one(x >= 0 ? x : -1);
            return true;
        }
        case Ab::CabalAgent: {
            int t = bestBy(d, [&](int id) { return float(unitCost2(g, p, UnitType(id))); });
            out = one(t >= 0 && unitCost2(g, p, UnitType(t)) >= 6 ? t : -1);
            return true;
        }
        // ---- batch 5: the Council Keleres
        case Ab::LawsOrder: {
            const int active = d.value;
            const bool gavel = fquirkIs(g, p, "keleres", 1);
            bool want = false;
            for (const Law& l : g.laws) {
                const char* k = kAgendas[l.agenda].key;
                auto is = [&](const char* key) { return !std::strcmp(k, key); };
                if (active == p) {  // laws in the way of our own turn
                    if (is("minister_peace") && l.owner >= 0 && l.owner != p) want = true;
                    if (is("censure") && l.owner == p && !me.actionCards.empty()) want = true;
                    if (is("revolution")) {
                        for (int8_t c : me.cards)
                            if (c == 6 && !((me.cardsUsed >> c) & 1)) want = true;  // Technology still to play
                    }
                    if ((is("travel_ban") || is("conventions") || is("articles_war")) && gavel) want = true;
                    if (gavel && l.owner >= 0 && l.owner != p) want = true;  // someone else's elected law
                } else if (l.owner == active && active >= 0) {  // a law that helps the player whose turn it is
                    if (is("minister_war") || (gavel && g.players[active].vp >= me.vp + 2)) want = true;
                }
            }
            if (!want) {
                out = one(0);
                return true;
            }
            out = one(has(d, 1) ? 1 : has(d, 2) ? 2 : 0);  // a commodity first
            return true;
        }
        case Ab::KeleresAgent:
        case Ab::KeleresCommander:
            out = yn ? yes(true) : one(firstReal(d));
            return true;
        case Ab::ExecutiveOrder: out = one(has(d, 0) ? 0 : firstReal(d)); return true;  // the top of the deck
        case Ab::HegemonicTrade: {  // (0.44.5: the biggest gain, more for a space dock producing here)
            int s = bestBy(d, [&](int id) {
                if (id < 0 || id >= int(g.planets.size())) return 0.f;
                float v = float(planetInfluence(g, id) - planetResources(g, id));
                if (g.planets[size_t(id)].system == g.activeSystem && g.planets[size_t(id)].units[size_t(p)].count(UnitType::SpaceDock)) v *= 1.5f;
                return v;
            }, 1.f);
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::AgencySupply: {
            if (spendable(g, p, false) < 3) {
                out = one(-1);
                return true;
            }
            int s = bestBy(d, [&](int id) {
                float v = float(productionCapacity(g, p, id));
                for (int16_t a : g.systems[id].adj) v += 0.6f * float(enemyNonFighters(g, p, a));
                if (blockaded(g, p, id)) v -= 3.f;
                return v;
            }, 1.f);
            out = one(s >= 0 ? s : -1);
            return true;
        }
        case Ab::KeleresOdlynn:
        case Ab::KeleresRider: {
            if (yn) {
                bool use = true;
                if (ab == Ab::KeleresRider) {  // give up our vote for a likely 1 card + 2 trade goods
                    float conf = 0;
                    const int o = aiPredictOutcome(g, d.agenda, &conf);
                    const float stake = std::fabs(aiOutcomeValue(g, p, d.agenda, o));
                    use = conf >= 0.55f && (availableVotes(g, p) <= 3 || stake < 0.5f);
                } else {
                    use = g.round >= 3 || g.players[p].vp >= g.vpGoal - 4;
                }
                out = yes(use);
                return true;
            }
            if (ab == Ab::KeleresRider) {
                float conf = 0;
                const int o = aiPredictOutcome(g, d.agenda, &conf);
                out = one(has(d, o) ? o : firstReal(d));
            } else {  // Odlynn Myrr: our 6 votes go behind the outcome we want
                const int o = bestBy(d, [&](int id) { return aiOutcomeValue(g, p, d.agenda, id); });
                out = one(o >= 0 ? o : firstReal(d));
            }
            return true;
        }
        case Ab::KeleresKuuasi: {
            bool use = false;
            const int sys = g.activeSystem;
            if (sys >= 0)
                for (int q = 0; q < g.numPlayers; ++q) {
                    if (q == p || !hasShips(g, q, sys)) continue;
                    const float odds = fightOdds(g, p, g.systems[sys].space[p], q, g.systems[sys].space[q], true);
                    if (odds < 0.85f || g.systems[sys].space[q].nonFighterShips() >= 3) use = true;
                }
            out = one(use ? firstReal(d) : -1);
            return true;
        }
        default: break;
    }
    if (yn) {
        out = yes(true);
        return true;
    }
    out = one(firstReal(d));
    return true;
}

}  // namespace ti
