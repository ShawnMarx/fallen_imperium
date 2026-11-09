// Personalities in play (0.12.0): turns a bot's archetype, evolution path,
// generic and faction traits, memory of other players and its hidden
// betrayal objective into a Style - multipliers and switches the planner,
// production, research, strategy cards, deals and votes read. A Style never
// gives anything material; it only shifts what the bot wants.
#include "ai/ai_internal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "engine/deal.h"
#include "engine/personality.h"
#include "engine/query.h"

namespace ti {

namespace {
// 0.43.4 (owner, E8): how many victory points the holder of Mecatol Rex leads everyone else by (negative: behind)
int kingslayLead(const Game& g, int holder) {
    if (holder < 0 || holder >= g.numPlayers) return 0;
    int best = -100;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != holder && !g.players[size_t(q)].eliminated) best = std::max(best, int(g.players[size_t(q)].vp));
    return best < -50 ? 0 : int(g.players[size_t(holder)].vp) - best;
}

bool fac(const Game& g, int p, const char* key) { return !std::strcmp(kFactions[g.players[p].faction].key, key); }

int leaderOtherThan(const Game& g, int p) {
    int best = -1, bv = -1;
    for (int q = 0; q < g.numPlayers; ++q)
        if (q != p && !g.players[q].eliminated && g.players[q].vp > bv) {
            bv = g.players[q].vp;
            best = q;
        }
    return best;
}

int shipsNear(const Game& g, int q, int p) {  // q's non-fighter ships in or next to p's systems
    int n = 0;
    for (size_t s = 0; s < g.systems.size(); ++s) {
        int k = g.systems[s].space[q].nonFighterShips();
        if (!k) continue;
        bool near = false;
        for (int s2 : systemsWithin(g, int(s), 1)) {
            if (g.systems[s2].home == p) near = true;
            for (int16_t x : g.systems[s2].planets)
                if (g.planets[x].owner == p) near = true;
            if (near) break;
        }
        if (near) n += k;
    }
    return n;
}

void archetype(Style& st, const Game& g, int p, Arch a) {
    const ArchDef& d = kArchetypes[int(a)];
    st.tra = d.tra;
    st.cea = d.cea;
    st.sup = d.sup;
    st.all = d.all;
    st.pol = d.pol;
    st.comm = d.comm;
    st.agent = d.agent;
    st.noteMult = d.noteMult;
    switch (a) {
        case Arch::Politician:
            st.card[2] += 1.5f;
            st.dealRate = 1.3f;
            st.persistent = true;
            break;
        case Arch::Peacekeeper:
            st.aggression = 0.6f;
            st.minOdds = 0.7f;
            st.peaceDeals = 1.f;
            st.acceptBias = -0.3f;
            st.homeGuard = 1;
            st.farPenalty = 0.3f;
            st.card[1] += 1.f;  // Diplomacy
            break;
        case Arch::Warlord:
            st.aggression = 1.5f;
            st.minOdds = 0.5f;
            st.fleetTokens = 5;
            st.card[5] += 1.5f;  // Warfare
            st.dealRate = 0.5f;
            st.acceptBias = 1.0f;
            st.acceptChance = 0.3f;
            st.threat = 1.f;
            st.keepPromise = 0.85f;
            st.bigFights = 1.2f;
            break;
        case Arch::Betrayer:
            st.aggression = 1.1f;
            st.minOdds = 0.55f;
            st.allianceDeals = 1.f;
            st.dealRate = 1.2f;
            st.acceptBias = -0.2f;
            st.keepPromise = 0.8f;
            break;
        case Arch::Swindler:
            if (g.round <= 2) st.expansion *= 1.1f;  // self-preservation starts with a solid slice
            st.aggression = 0.8f;
            st.minOdds = 0.65f;
            st.homeGuard = 1;
            st.promiseSell = 1.f;
            st.dealRate = 1.3f;
            st.keepPromise = 0.5f;
            st.retreatOdds = 0.35f;
            break;
        case Arch::Merchant:
            st.aggression = 0.85f;
            st.minOdds = 0.62f;
            st.card[4] += 2.f;  // Trade
            st.dealRate = 1.8f;
            st.acceptBias = -0.1f;
            st.threat = 0.3f;
            st.lend = 0.5f;
            break;
        case Arch::Opportunist: {
            st.aggression = 1.1f;
            st.minOdds = 0.55f;
            st.threat = 0.4f;
            // kind to the weak, hard on whoever crowds it
            const int mine = shipsNear(g, p, p);
            for (int q = 0; q < g.numPlayers && q < 8; ++q) {
                if (q == p) continue;
                int theirs = shipsNear(g, q, p);
                if (theirs >= 2 && theirs * 2 >= mine) st.vs[q] *= 1.25f;
                else if (theirs == 0) st.vs[q] *= 0.85f;
            }
            break;
        }
        case Arch::Rusher:
            // 0.17.0 (owner: too weak, doesn't build its slice): round 1 takes the planets
            // around home (on the way in), the Mecatol Rex rush starts in round 2
            st.mecatol = g.round <= 1 ? 1.15f : g.round <= 4 ? 1.9f : 1.5f;
            if (g.round <= 2) st.expansion *= 1.15f;
            st.unit[int(UnitType::Cruiser)] = 1.3f;  // fast ships reach Mecatol Rex in one move
            st.techPref[2] = "gd";                   // Gravity Drive: carriers too
            st.card[7] += 2.f;  // Imperial
            st.homeGuard = 1;
            st.peaceDeals = 1.f;
            break;
        case Arch::Turtler:
            st.aggression = 0.7f;
            st.minOdds = 0.68f;
            st.expansion = 1.25f;
            st.farPenalty = 0.5f;
            st.homeGuard = 1;
            st.fleetTokens = 5;
            st.dealRate = 0.5f;
            st.card[3] += 1.f;  // Construction
            st.pdsPref = 1.f;
            for (int q = 0; q < g.numPlayers && q < 8; ++q)  // fierce against anyone who attacked it
                if (g.players[p].pers.attacks[q] > 0) st.vs[q] *= 2.f;
            break;
        default: break;
    }
}

void evolution(Style& st, const Game& g, int p, Evo e) {
    const Personality& pe = g.players[p].pers;
    const int r = g.round;
    switch (e) {
        case Evo::LateBloomer:
            if (r <= 3) {
                st.aggression *= 0.75f;
                st.expansion *= 1.15f;
                st.dealRate *= 1.2f;
            } else {
                st.aggression *= 1.f + 0.12f * float(std::min(r - 3, 5));
                st.threat += 0.15f * float(std::min(r - 3, 5));
            }
            break;
        case Evo::Vengeful:
            for (int q = 0; q < g.numPlayers && q < 8; ++q) st.vs[q] *= std::min(2.f, 1.f + 0.25f * pe.attacks[q]);
            break;
        case Evo::Mirror:
            for (int q = 0; q < g.numPlayers && q < 8; ++q) {
                float m = 1.f + 0.2f * pe.attacks[q] + 0.3f * pe.broke[q] - 0.1f * pe.deals[q] - 0.1f * pe.kept[q];
                st.vs[q] *= std::max(0.6f, std::min(2.f, m));
                st.biasVs[q] += 0.4f * (pe.attacks[q] + pe.broke[q]) - 0.3f * (pe.deals[q] + pe.kept[q]);
            }
            break;
        case Evo::Kingslayer:
            if (r >= 4) {
                int l = leaderOtherThan(g, p);
                if (l >= 0 && g.players[l].vp >= g.players[p].vp && g.players[l].vp > 0) {
                    st.vs[l] *= 1.6f;
                    st.refuseFrom[l] = true;
                    st.voteAgainst = l;
                }
            }
            break;
        case Evo::Ambitious:
            if (g.players[p].vp >= g.vpGoal - 3) {
                st.keepPromise = 0.f;
                st.aggression *= 1.2f;
                st.dealRate *= 0.6f;
            }
            break;
        case Evo::Learner:
            for (int q = 0; q < g.numPlayers && q < 8; ++q)
                st.biasVs[q] += std::max(-1.5f, std::min(1.5f, 0.3f * pe.refused[q] - 0.2f * pe.gifts[q] - 0.1f * pe.deals[q]));
            break;
        case Evo::RisingDiplomat: {
            int deals = 0, refused = 0;
            for (int q = 0; q < 8; ++q) {
                deals += pe.deals[q];
                refused += pe.refused[q];
            }
            st.acceptBias += std::max(-1.f, std::min(1.f, 0.15f * float(refused - deals)));
            break;
        }
        case Evo::BorderWatch: {
            int crowd = 0;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p && !g.players[q].eliminated) crowd += shipsNear(g, q, p);
            if (crowd >= 3) {
                st.homeGuard += 1;
                st.aggression *= 0.8f;
                st.peaceDeals += 1.f;
            }
            break;
        }
        case Evo::Scavenger:
            for (int q = 0; q < g.numPlayers && q < 8; ++q)
                if (q != p && pe.lostBig[q] > 0 && pe.lostBig[q] >= r - 1 && areNeighbours(g, p, q)) st.vs[q] *= 1.8f;
            break;
        case Evo::ThreeActs:
            if (r <= 2) {
                st.expansion *= 1.3f;
                st.aggression *= 0.7f;
            } else if (r <= 4) {
                st.homeGuard += 1;
                st.research *= 1.2f;
            } else {
                st.aggression *= 1.4f;
            }
            break;
        default: break;
    }
}

void quirk(Style& st, const Game& g, int p, Quirk q) {
    switch (q) {
        case Quirk::Reckless:
            st.anomaly += 1.5f;
            st.minOdds -= 0.05f;
            break;
        case Quirk::HolyDiver: st.mecatol *= 1.6f; break;
        case Quirk::ChessMaster: st.stage *= 1.6f; break;
        case Quirk::Kneecapper: {
            int best = -1, lead = 0;
            for (const Player& pl : g.players) lead = std::max<int>(lead, pl.vp);
            for (int o = 0; o < g.numPlayers; ++o) {
                if (o == p || g.players[o].eliminated || !areNeighbours(g, p, o)) continue;
                if (g.players[o].vp < g.players[p].vp + 2 || g.players[o].vp < lead) continue;  // a neighbour leading by 2+
                if (best < 0 || g.players[o].vp > g.players[best].vp ||
                    (g.players[o].vp == g.players[best].vp && prand(g, 30, p, o, g.round, 2)))
                    best = o;
            }
            if (best >= 0) {
                st.homeHitTarget = best;
                st.homeHit = 8.f;
                if (best < 8) st.vs[best] *= 1.5f;
            }
            break;
        }
        case Quirk::TechHoarder:
            st.research *= 1.4f;
            st.card[6] += 2.f;
            break;
        case Quirk::Collector:
            st.explore += 1.5f;
            st.fragment = 2.f;
            break;
        case Quirk::GrudgeKeeper:  // its B-Factor never cools (rules side); hits back harder
            for (int o = 0; o < g.numPlayers && o < 8; ++o)
                if (g.players[p].pers.attacks[o] > 0) st.vs[o] *= 1.3f;
            break;
        case Quirk::BargainHunter: st.acceptBias += 0.4f; break;
        case Quirk::Showboat:
            st.aggression *= 1.1f;
            st.unit[int(UnitType::Flagship)] = 2.f;
            st.unit[int(UnitType::WarSun)] = 2.f;
            break;
        case Quirk::CautiousAdmiral:
            st.minOdds = std::max(st.minOdds, 0.75f);
            st.retreatOdds = std::max(st.retreatOdds, 0.4f);
            break;
        case Quirk::Spoiler: st.spoiler = 1.f; break;
        case Quirk::Homebody:
            st.homeGuard += 1;
            st.farPenalty += 0.5f;
            break;
        case Quirk::Gambler: st.minOdds -= 0.1f; break;
        default: break;
    }
}

void factionQuirk(Style& st, const Game& g, int p) {
    const int w = g.players[p].pers.fquirk;
    auto is = [&](const char* k, int which) { return w == which && fac(g, p, k); };
    if (is("naalu", 0)) {
        st.unit[int(UnitType::Fighter)] = 2.f;
        st.techPref[0] = "ff2";
    } else if (is("naalu", 1)) {
        st.notePay = 1.3f;
        st.sweetener = 1.f;
    } else if (is("muaat", 0)) {
        st.threat += 1.f;
    } else if (is("muaat", 1)) {
        st.unit[int(UnitType::WarSun)] = 2.5f;
        st.unit[int(UnitType::Flagship)] = 2.f;
        st.techPref[0] = "ws";
        st.threat += 0.5f;
    } else if (is("arborec", 0)) {
        st.unit[int(UnitType::Infantry)] = 2.f;
        st.unit[int(UnitType::Mech)] = 2.f;
        st.techPref[0] = "inf2";
        st.expansion *= 0.9f;
    } else if (is("arborec", 1)) {
        st.pdsPref += 1.f;
        st.mecatol *= 1.2f;
    } else if (is("hacan", 0)) {
        st.tgKeep += 2;
        st.acValue = 3.f;
    } else if (is("hacan", 1)) {
        st.acValue = 2.f;
        st.sellCards = 1.f;
        st.threat += 0.5f;
    } else if (is("mahact", 0)) {
        st.smallFights = 1.3f;
    } else if (is("mahact", 1)) {
        st.fleetTokens = 5;
        st.bigFights = 1.3f;
        st.threat += 0.5f;
    } else if (is("l1z1x", 0)) {
        st.unit[int(UnitType::Dreadnought)] = 2.f;
        st.techPref[0] = "dn2";
        st.threat += 0.5f;
    } else if (is("l1z1x", 1)) {
        st.explore += 0.f;  // handled in planet values: enemy structures
        st.aggression *= 1.1f;
    } else if (is("sol", 0)) {
        st.sup += 1.f;
        st.cea += 1.f;
        st.expansion *= 1.1f;
    } else if (is("sol", 1)) {
        st.unit[int(UnitType::Infantry)] = 1.6f;
        st.minOdds -= 0.05f;
    } else if (is("yin", 0)) {
        st.smallFights = 1.25f;
    } else if (is("yin", 1)) {
        st.unit[int(UnitType::Cruiser)] = 1.5f;
        st.unit[int(UnitType::Destroyer)] = 1.5f;
        st.aggression *= 1.1f;
    } else if (is("sardakk", 0)) {
        st.neverRetreat = true;
        if (g.round >= 2 && g.round <= 3) {  // the nearest neighbour
            int best = -1, bd = 1 << 20;
            for (int q = 0; q < g.numPlayers; ++q) {
                if (q == p || g.players[q].eliminated || !areNeighbours(g, p, q)) continue;
                int dd = hexDistance(g.systems[g.players[p].homeSystem].pos, g.systems[g.players[q].homeSystem].pos);
                if (dd < bd) {
                    bd = dd;
                    best = q;
                }
            }
            if (best >= 0 && best < 8) st.vs[best] *= 1.6f;
        }
    } else if (is("sardakk", 1)) {
        st.unit[int(UnitType::Infantry)] = 1.5f;
        st.unit[int(UnitType::Mech)] = 1.5f;
        if (g.round >= 5) {
            int l = leaderOtherThan(g, p);
            if (l >= 0 && areNeighbours(g, p, l)) {
                st.homeHitTarget = l;
                st.homeHit = std::max(st.homeHit, 6.f);
            }
        }
    } else if (is("empyrean", 0)) {
        st.pres *= 1.4f;
        st.explore += 1.f;
    } else if (is("empyrean", 1)) {
        st.noteMult *= 0.7f;  // its Dark Pact / Blood Pact go out cheaply
        st.dealRate *= 1.2f;
    } else if (is("yssaril", 0)) {
        st.card[2] += 1.f;
        st.acValue = 3.f;
    } else if (is("yssaril", 1)) {
        st.smallFights = 1.3f;
        st.unit[int(UnitType::Flagship)] = 1.8f;
    } else if (is("nekro", 0)) {
        int best = -1;
        size_t most = 0;
        for (int q = 0; q < g.numPlayers; ++q)
            if (q != p && !g.players[q].eliminated && areNeighbours(g, p, q) && g.players[q].techs.size() > most) {
                most = g.players[q].techs.size();
                best = q;
            }
        if (best >= 0 && best < 8) st.vs[best] *= 1.5f;
    } else if (is("nekro", 1)) {
        st.pres *= 1.3f;
        st.expansion *= 1.15f;
    } else if (is("cabal", 0)) {
        st.homeGuard += 1;
    } else if (is("cabal", 1)) {
        st.bigFights = 1.3f;
    } else if (is("titans", 0)) {
        st.aggression *= g.round <= 3 ? 0.7f : 1.4f;
    } else if (is("titans", 1)) {
        st.pdsPref += 1.5f;
        st.homeGuard += 1;
    } else if (is("mentak", 0)) {
        st.noteAdj = -1.f;
        st.acValue = 3.f;
    } else if (is("mentak", 1)) {
        st.smallFights = 1.35f;
    } else if (is("saar", 0)) {
        st.homeGuard = std::max(0, st.homeGuard - 1);
        st.expansion *= 1.2f;
    } else if (is("saar", 1)) {
        st.expansion *= 1.2f;
        st.dealRate *= 1.3f;
    } else if (is("argent", 0)) {
        st.peaceDeals += 0.5f;
        st.dealRate *= 1.2f;
    } else if (is("argent", 1)) {
        st.unit[int(UnitType::Destroyer)] = 2.f;
        st.techPref[0] = "dd2";
        st.aggression *= 1.1f;
    } else if (is("naaz", 0)) {
        st.explore += 1.5f;
        st.fragment = 2.f;
    } else if (is("naaz", 1)) {
        st.unit[int(UnitType::Mech)] = 1.6f;
    } else if (is("xxcha", 0)) {
        st.aggression *= 0.8f;
        st.peaceDeals += 1.f;
        st.dealRate *= 1.3f;
    } else if (is("xxcha", 1)) {
        st.pdsPref += 1.5f;
        st.farPenalty += 0.3f;
    } else if (is("letnev", 0)) {
        st.unit[int(UnitType::Dreadnought)] = 2.f;
        st.techPref[0] = "dn2";
    } else if (is("letnev", 1)) {
        st.tgKeep += 2;
        st.lend += 1.f;
    } else if (is("jolnar", 0)) {
        st.research *= 1.4f;
        st.aggression *= 0.7f;
    } else if (is("jolnar", 1)) {
        st.dealRate *= 1.3f;
        st.noteMult *= 0.8f;
    } else if (is("ghost", 0)) {
        st.aggression *= 1.1f;
    } else if (is("ghost", 1)) {
        st.aggression *= 1.2f;
        st.retreatOdds = std::max(st.retreatOdds, 0.4f);
    } else if (is("winnu", 0)) {
        st.mecatol *= 2.f;
    } else if (is("winnu", 1)) {
        st.noteMult *= 0.7f;
        st.dealRate *= 1.2f;
    } else if (is("nomad", 0)) {
        st.homeGuard = std::max(0, st.homeGuard - 1);
        st.aggression *= 1.1f;
    } else if (is("nomad", 1)) {
        st.tgKeep += 1;
        st.noteMult *= 0.8f;
        st.dealRate *= 1.2f;
    } else if (is("keleres", 0)) {  // Internal Affairs: promise breakers and aggressors are its targets
        for (int q = 0; q < g.numPlayers && q < 8; ++q) {
            if (q == p || g.players[q].eliminated) continue;
            int offences = g.players[q].promisesBroken;
            for (int r = 0; r < g.numPlayers; ++r)
                if (r != q && r != p && ((g.hostile >> (q * 8 + r)) & 1)) ++offences;
            st.vs[q] *= 1.f + 0.12f * float(std::min(offences, 4));
        }
        st.peaceDeals += 0.5f;
        st.keepPromise = std::max(st.keepPromise, 0.95f);
    } else if (is("keleres", 1)) {  // Gavel of the Council (Law's Order and votes: ai_faction / ai_cards)
        st.mecatol *= 1.3f;
        st.card[2] += 1.f;  // Politics
        st.techPref[0] = "executiveorder";
    } else if (is("bastion", 0)) {  // Phoenix Banner: small sure fights for galvanize tokens
        st.smallFights *= 1.4f;
        st.minOdds -= 0.05f;
    } else if (is("bastion", 1)) {  // Liberator: infantry for Liberate; protection for neighbours
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.5f);
        st.peaceDeals += 0.4f;
    } else if (is("ralnel", 0)) {  // Last Gecko Out (passing late: ai.cpp)
        st.tgKeep += 1;
    } else if (is("ralnel", 1)) {  // Skittering Ring: small groups everywhere
        st.pres *= 1.3f;
        st.bigFights *= 0.85f;
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.3f);
    } else if (is("crimson", 0)) {  // Empty House: no home guard, all forward
        st.homeGuard = 0;
        st.aggression *= 1.1f;
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.3f);
    } else if (is("crimson", 1)) {  // Quiet Breaches (ai_te: inactive breaches, our own closed)
        st.aggression *= 0.95f;
    } else if (is("deepwrought", 0)) {  // Visiting Scholar (ai_te: coexist unless sure to win)
        st.aggression *= 0.9f;
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.3f);
    } else if (is("deepwrought", 1)) {  // Library Card (ai_te: Visionaria Select, Radical Advancement)
        st.research *= 1.2f;
    } else if (is("firmament", 0) || is("obsidian", 0)) {  // Puppet Master: deals with every neighbour (Black Ops)
        st.dealRate *= 1.3f;
        st.pres *= 1.15f;
    } else if (is("firmament", 1) || is("obsidian", 1)) {  // Coup Clock (ai_te: The Sowing, the coup at 2 plots)
        st.tgKeep += 1;
    } else if (is("kortali", 0)) {  // Exhausted Prey (Ruthless)
        st.aggression *= 1.15f;
        st.unit[int(UnitType::Dreadnought)] = std::max(st.unit[int(UnitType::Dreadnought)], 1.4f);
    } else if (is("kortali", 1)) {  // Zealot's Banner: leaves Warfare to others
        st.card[5] -= 1.5f;
        st.card[0] += 0.5f;
    } else if (is("vaylerian", 0)) {  // Corsair Code: small fights, raiders
        st.smallFights *= 1.3f;
        st.unit[int(UnitType::Cruiser)] = std::max(st.unit[int(UnitType::Cruiser)], 1.8f);
    } else if (is("vaylerian", 1)) {  // Card Sharp (ai_ds: Scour on cheaper planets)
        st.card[2] += 1.f;
    } else if (is("lanefir", 0)) {  // Grave Robber
        st.expansion *= 1.15f;
        st.smallFights *= 1.15f;
    } else if (is("lanefir", 1)) {  // Iconoclast Scholar (ai_ds: Troubadour)
        st.research *= 1.15f;
    } else if (is("kyro", 0)) {  // Patient Plague: Politics for Contagion
        st.card[2] += 1.5f;
    } else if (is("kyro", 1)) {  // Pale Rider (ai_ds: Silas Deriga, Kyro Rider)
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.5f);
    } else if (is("cheiran", 0)) {  // Hive Bastion
        st.pdsPref += 1.f;
        st.homeGuard += 1;
        st.unit[int(UnitType::Mech)] = std::max(st.unit[int(UnitType::Mech)], 1.4f);
    } else if (is("cheiran", 1)) {  // Moulting Season: defender
        st.aggression *= 0.85f;
        st.unit[int(UnitType::Dreadnought)] = std::max(st.unit[int(UnitType::Dreadnought)], 1.5f);
    } else if (is("celdauri", 0)) {  // Merchant Princes
        st.dealRate *= 1.3f;
    } else if (is("celdauri", 1)) {  // Dock Barons
        st.card[3] += 1.f;
    } else if (is("khrask", 0)) {  // Stone Garden
        st.unit[int(UnitType::Infantry)] = std::min(st.unit[int(UnitType::Infantry)], 0.7f);
        st.pdsPref += 1.f;
    } else if (is("khrask", 1)) {  // Crystal Bloc
        st.card[2] += 1.f;
        st.dealRate *= 1.15f;
    } else if (is("zealots", 0)) {  // Relic Hunters
        st.research *= 1.2f;
        st.expansion *= 1.1f;
    } else if (is("zealots", 1)) {  // Last Word
        st.card[2] += 1.f;
    } else if (is("zelian", 0)) {  // Purifier's Zeal
        st.aggression *= 1.15f;
        st.smallFights *= 1.2f;
    } else if (is("zelian", 1)) {  // Hammerfall
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.6f);
        st.techPref[0] = "inf2";
    } else if (is("rohdhna", 0)) {  // Foundry Lords
        st.unit[int(UnitType::WarSun)] = std::max(st.unit[int(UnitType::WarSun)], 2.5f);
        st.card[3] += 1.f;
    } else if (is("rohdhna", 1)) {  // Scrap Economy
        st.tgKeep += 1;
        st.dealRate *= 1.1f;
    } else if (is("tnelis", 0)) {  // Shadow Brokers
        st.card[7] += 1.5f;
    } else if (is("tnelis", 1)) {  // Runner Fleets
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.8f);
        st.smallFights *= 1.2f;
    } else if (is("kolume", 0)) {  // Silent Gunners
        st.aggression *= 1.1f;
        st.unit[int(UnitType::Dreadnought)] = std::max(st.unit[int(UnitType::Dreadnought)], 1.4f);
    } else if (is("kolume", 1)) {  // Patient Order
        st.research *= 1.15f;
        st.card[0] += 1.f;
    } else if (is("bentor", 0)) {  // Map Collectors
        st.explore += 1.f;
        st.expansion *= 1.15f;
        st.card[3] += 0.5f;
    } else if (is("bentor", 1)) {  // Company Store
        st.dealRate *= 1.25f;
        st.card[4] += 0.5f;
    } else if (is("dihmohn", 0)) {  // Grand Armada
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.8f);
        st.bigFights *= 1.15f;
    } else if (is("dihmohn", 1)) {  // Pressgangs (ai_ds: Impressment Programs)
        st.explore += 1.f;
        st.techPref[0] = "dsdihmy";
    } else if (is("florzen", 0)) {  // Swarm Lords
        st.unit[int(UnitType::Fighter)] = std::max(st.unit[int(UnitType::Fighter)], 1.8f);
        st.unit[int(UnitType::Carrier)] = std::max(st.unit[int(UnitType::Carrier)], 1.3f);
    } else if (is("florzen", 1)) {  // Extortionists (ai_ds: Blackmail Programs)
        st.techPref[0] = "dsflorg";
        st.spoiler += 0.5f;
    } else if (is("gledge", 0)) {  // Deep Core Miners
        st.unit[int(UnitType::Mech)] = std::max(st.unit[int(UnitType::Mech)], 1.8f);
        st.tgKeep += 1;
    } else if (is("gledge", 1)) {  // Platform Builders
        st.pdsPref += 1.5f;
        st.card[3] += 1.f;
    } else if (is("augers", 0)) {  // Foresight
        st.card[7] += 1.f;
    } else if (is("augers", 1)) {  // Data Brokers (ai_ds: Sentient Datapool)
        st.tgKeep += 2;
        st.research *= 1.15f;
    } else if (is("kjalengard", 0)) {  // Saga Seekers
        st.aggression *= 1.2f;
        st.smallFights *= 1.25f;
    } else if (is("kjalengard", 1)) {  // Shield Wall
        st.unit[int(UnitType::Carrier)] = std::max(st.unit[int(UnitType::Carrier)], 1.5f);
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.4f);
    } else if (is("kollecc", 0)) {  // Hidden Hand (ai_ds: Cloaked Fleets / Shroud of Lith)
        st.aggression *= 1.1f;
        st.unit[int(UnitType::Cruiser)] = std::max(st.unit[int(UnitType::Cruiser)], 1.4f);
    } else if (is("kollecc", 1)) {  // Relic Collectors
        st.explore += 1.f;
        st.techPref[0] = "dskolly";
    } else if (is("mortheus", 0)) {  // Mirror Wall
        st.aggression *= 0.9f;
        st.homeGuard += 1;
    } else if (is("mortheus", 1)) {  // Void Foundries
        st.techPref[0] = "dsmorty";
        st.pres *= 1.15f;
    } else if (is("vaden", 0)) {  // Loan Sharks
        st.card[4] += 0.8f;
        st.card[6] += 0.5f;
    } else if (is("vaden", 1)) {  // Gold Engines
        st.tgKeep += 2;
    } else if (is("cymiae", 0)) {  // Card Hoarders
        st.card[2] += 1.f;
    } else if (is("cymiae", 1)) {  // Undying Horde
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.7f);
    } else if (is("nivyn", 0)) {  // Rift Riders
        st.anomaly += 1.f;
    } else if (is("nivyn", 1)) {  // Wound Keepers
        st.homeGuard += 1;
        st.unit[int(UnitType::Mech)] = std::max(st.unit[int(UnitType::Mech)], 1.4f);
    } else if (is("olradin", 0)) {  // Popular Front
        st.expansion *= 1.15f;
    } else if (is("olradin", 1)) {  // Council Hall
        st.card[2] += 1.f;
    } else if (is("freesystems", 0)) {  // Liberators (ai_ds: Rally to the Cause, Liberator)
        st.expansion *= 1.15f;
        st.unit[int(UnitType::Mech)] = std::max(st.unit[int(UnitType::Mech)], 1.3f);
    } else if (is("freesystems", 1)) {  // Diplomatic Corps
        st.card[2] += 0.8f;
        st.techPref[0] = "dsfreeg";
    } else if (is("edyn", 0)) {  // Sigil Wardens
        st.unit[int(UnitType::Mech)] = std::max(st.unit[int(UnitType::Mech)], 1.6f);
        st.anomaly += 0.5f;
    } else if (is("edyn", 1)) {  // Oracles
        st.card[2] += 0.8f;
        st.card[7] += 0.6f;
    } else if (is("nokar", 0)) {  // Privateers (ai_ds: Hired Guns)
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.8f);
    } else if (is("nokar", 1)) {  // Mercenary Admiralty
        st.aggression *= 1.1f;
        st.unit[int(UnitType::Destroyer)] = std::max(st.unit[int(UnitType::Destroyer)], 1.5f);
    } else if (is("axis", 0)) {  // War Profiteers (ai_deals: Axis Orders for trade goods)
        st.card[4] += 1.f;
    } else if (is("axis", 1)) {  // Shipyards
        st.card[3] += 0.8f;
        st.unit[int(UnitType::Dreadnought)] = std::max(st.unit[int(UnitType::Dreadnought)], 1.4f);
    } else if (is("mykomentori", 0)) {  // Fungal Bloom (Mycelium Ring)
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.6f);
    } else if (is("mykomentori", 1)) {  // Seers (ai_ds: Divination)
        st.homeGuard += 1;
    } else if (is("ghemina", 0)) {  // Twin Raiders
        st.aggression *= 1.15f;
        st.unit[int(UnitType::Flagship)] = std::max(st.unit[int(UnitType::Flagship)], 1.5f);
    } else if (is("ghemina", 1)) {  // Plunderers
        st.smallFights *= 1.2f;
    } else if (is("veldyr", 0)) {  // Board of Directors (ai_ds: Targeted Acquisition)
        st.card[4] += 0.6f;
    } else if (is("veldyr", 1)) {  // Lancer Line
        st.unit[int(UnitType::Dreadnought)] = std::max(st.unit[int(UnitType::Dreadnought)], 1.6f);
    } else if (is("lizho", 0)) {  // Trapmasters
        st.unit[int(UnitType::Infantry)] = std::max(st.unit[int(UnitType::Infantry)], 1.3f);
    } else if (is("lizho", 1)) {  // Bomber Wings
        st.unit[int(UnitType::Fighter)] = std::max(st.unit[int(UnitType::Fighter)], 1.7f);
    } else if (is("mirveda", 0)) {  // Flying Batteries
        st.unit[int(UnitType::PDS)] = std::max(st.unit[int(UnitType::PDS)], 1.5f);
    } else if (is("mirveda", 1)) {  // Drone Carriers
        st.unit[int(UnitType::Carrier)] = std::max(st.unit[int(UnitType::Carrier)], 1.4f);
        st.unit[int(UnitType::Fighter)] = std::max(st.unit[int(UnitType::Fighter)], 1.5f);
    } else if (is("ghoti", 0)) {  // Wandering Court
        st.aggression *= 1.05f;
    } else if (is("ghoti", 1)) {  // Deep Void
        st.expansion *= 1.15f;
    }
}

// Competitive: the personality still shows in deals, grudges and betrayals, but its
// habits on the board are moved most of the way back to plain strong play.
void competitiveDamp(Style& s) {
    if (!s.on) return;
    auto toward = [](float v, float neutral, float keep) { return neutral + (v - neutral) * keep; };
    const float k = 0.35f;
    s.aggression = toward(s.aggression, 1.f, k);
    for (float& v : s.vs) v = toward(v, 1.f, 0.6f);  // grudges toward players keep more of their bite
    s.minOdds = toward(s.minOdds, 0.6f, k);
    s.expansion = std::max(1.f, toward(s.expansion, 1.f, k));
    s.farPenalty *= k;
    s.mecatol = std::max(1.f, toward(s.mecatol, 1.f, k));
    s.homeGuard = std::min(s.homeGuard, 1);
    s.stage = std::max(1.f, toward(s.stage, 1.f, k));
    s.anomaly *= k;
    s.pres = std::max(1.f, toward(s.pres, 1.f, k));
    s.smallFights = toward(s.smallFights, 1.f, k);
    s.bigFights = toward(s.bigFights, 1.f, k);
    for (float& c : s.card) c *= k;
    for (float& u : s.unit) u = toward(u, 1.f, k);
    s.research = toward(s.research, 1.f, k);
    s.pdsPref *= k;
    s.tgKeep = std::min(s.tgKeep, 1);
    s.dealRate = std::min(s.dealRate, 1.2f);
}

Style compute(const Game& g, int p) {
    Style st;
    if (!isBot(g, p)) return st;
    const Personality& pe = g.players[p].pers;
    st.on = true;
    archetype(st, g, p, Arch(pe.now >= 0 ? pe.now : pe.arch));
    if (pe.evo >= 0) evolution(st, g, p, Evo(pe.evo));
    if (pe.quirk >= 0) quirk(st, g, p, Quirk(pe.quirk));
    if (pe.fquirk >= 0) factionQuirk(st, g, p);
    // the hidden betrayal objective
    if (pe.target >= 0) {
        st.betrayTarget = pe.target;
        st.betrayKind = pe.kind;
        st.betrayArg = pe.arg;
        const int t = pe.target;
        // how hard it pushes now (0.25.0 betrayal timing): 1 = the 0.12-0.24 strength
        const float f = std::max(0.1f, std::min(1.6f, aiBetrayUrge(g, p) / 1.5f));
        switch (Betrayal(pe.kind)) {
            case Betrayal::BreakPromise:
            case Betrayal::AttackWeakest:
            case Betrayal::TakePlanet: st.vs[t] *= 1.f + 0.6f * f; break;
            case Betrayal::RefuseOffers: st.refuseFrom[t] = true; break;
            case Betrayal::VoteAgainst: st.voteAgainst = t; break;
            case Betrayal::Kingslay:  // take Mecatol Rex from them: gather a fleet, go
                st.vs[t] *= 1.f + f;
                st.mecatol *= 1.f + 1.5f * f;
                if (f >= 0.5f) st.minOdds = std::min(st.minOdds, 0.5f);
                st.stage *= 1.f + 0.5f * f;
                st.aggression *= 1.f + 0.2f * f;
                st.unit[int(UnitType::Dreadnought)] *= 1.3f;
                st.refuseFrom[t] = true;
                // 0.43.0 (owner: "bots are still afraid to attack a fortified Mecatol - maybe have them also target that
                // player's home"): their home is a target too - its fleet sits on Mecatol Rex
                st.homeHitTarget = t;
                st.homeHit = std::max(st.homeHit, 5.f + 4.f * f);
                // 0.43.4 (owner, E8): more push when the holder is ahead on points (1 or more over everyone else)
                if (kingslayLead(g, t) >= 1) {
                    st.vs[t] *= 1.3f;
                    st.mecatol *= 1.25f;
                    st.aggression *= 1.15f;
                    st.minOdds = std::min(st.minOdds, 0.45f);
                    st.homeHit += 2.f;
                }
                break;
            default: break;
        }
    }
    // Kingslay (0.43.0): the other bots lean against the long-time holder of Mecatol Rex too - one bot alone seldom
    // breaks a fortified seat
    for (int q = 0; q < g.numPlayers; ++q) {
        const Personality& qe = g.players[size_t(q)].pers;
        if (q == p || !isBot(g, q) || qe.kind != int8_t(Betrayal::Kingslay) || qe.target < 0 || qe.target == p) continue;
        if (pe.target == qe.target && pe.kind == int8_t(Betrayal::Kingslay)) continue;
        const bool ahead = kingslayLead(g, qe.target) >= 1;  // (0.43.4, E8: a holder ahead on points draws more)
        st.vs[qe.target] *= ahead ? 1.4f : 1.25f;
        st.mecatol *= ahead ? 1.3f : 1.2f;
    }
    st.minOdds = std::max(0.35f, std::min(0.85f, st.minOdds));
    return st;
}

// 0.54.0 (owner: bots that adapt to the players' habits): what the people did in their earlier games (Game::habits,
// fixed when the game started) shapes how each bot treats their seats - how far it trusts their promises, how much
// it guards home from them, how hard it watches them. Weighs in fully from the third game remembered.
void applyHabits(Style& s, const Game& g, int p) {
    const HabitProfile& h = g.habits;
    if (h.games <= 0 || !s.on || aiTune("habits", 1.f) <= 0.f || aiLevelScale(g) <= 0.f) return;
    const float conf = std::min(1.f, float(h.games) / 3.f) * aiTune("habits", 1.f);
    const int myHome = g.players[size_t(p)].homeSystem;
    for (int q = 0; q < g.numPlayers; ++q) {
        if (q == p || !g.players[size_t(q)].human) continue;
        // an attacker (6-player bots average about half an attack on others a round): watched and answered in kind
        const float aggr = std::max(0.5f, std::min(2.f, h.attacks / 0.5f));
        s.vs[size_t(q)] *= 1.f + 0.15f * conf * (aggr - 1.f);
        // a raider of home systems: its neighbours keep one more ship at home
        const int qHome = g.players[size_t(q)].homeSystem;
        if (h.raids >= 0.5f && myHome >= 0 && qHome >= 0 && conf >= 0.5f) {  // (bots: ~0.12 a game)
            bool near = false;
            for (int x : systemsWithin(g, qHome, 3)) near = near || x == myHome;
            if (near) s.homeGuard += 1;
        }
        // a promise breaker: deals with them need more in them
        const float trust = (h.broken + 0.5f) / (h.broken + h.kept + 1.f);  // (share of promises broken)
        if (h.broken >= 0.5f && trust > 0.35f) s.biasVs[size_t(q)] += 4.f * conf * (trust - 0.35f);  // (bots: ~0.27)
        // a keeper of promises and a maker of deals: a little more welcome
        if (h.deals >= 6.f && trust < 0.2f) s.biasVs[size_t(q)] -= 0.5f * conf;
    }
    // one who sits on Mecatol Rex: the bots value contesting it more (aiMecatolHill reads the habit as well)
    if (h.mecatol >= 6.f) s.mecatol *= 1.f + 0.15f * conf;  // (bots: ~3.3 turns a game)
}

// Test only (0.55.0, the owner's final bot pass): TI4_PERSONA="2:breaker,4:camper" plays those seats like a person
// with one strong habit, to check the bots answer it and can't be farmed by it. Kinds: breaker (promises everything,
// keeps nothing), camper (sits on Mecatol Rex), spammer (offers deals all the time, re-offers after a no), turtle
// (never attacks, guards home with everything), rusher (attacks whatever it can).
}  // namespace

int aiPersona(int p) {
    static int kind[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    static const bool read = [] {
        const char* s = std::getenv("TI4_PERSONA");
        if (!s) return true;
        static const char* kNames[] = {"", "breaker", "camper", "spammer", "turtle", "rusher"};
        for (const char* c = s; *c;) {
            const int seat = *c - '0';
            const char* colon = std::strchr(c, ':');
            if (!colon) break;
            const char* end = std::strchr(colon, ',');
            const size_t len = end ? size_t(end - colon - 1) : std::strlen(colon + 1);
            for (int k = 1; k <= 5; ++k)
                if (seat >= 0 && seat < 8 && std::strlen(kNames[k]) == len && !std::strncmp(colon + 1, kNames[k], len)) kind[seat] = k;
            if (!end) break;
            c = end + 1;
        }
        return true;
    }();
    (void)read;
    return p >= 0 && p < 8 ? kind[p] : 0;
}

namespace {

void applyPersona(Style& s, const Game& g, int p) {
    switch (aiPersona(p)) {
        case 1:  // breaker
            s.keepPromise = 0.f, s.peaceDeals = std::max(s.peaceDeals, 1.f), s.allianceDeals = std::max(s.allianceDeals, 1.f);
            s.promiseSell = std::max(s.promiseSell, 1.f), s.dealRate *= 2.f, s.acceptChance = 1.f;
            break;
        case 2:  // camper
            s.mecatol *= 3.f, s.stage *= 1.5f, s.homeGuard = 0, s.unit[int(UnitType::Dreadnought)] *= 1.5f;
            break;
        case 3:  // spammer
            s.dealRate *= 4.f, s.persistent = true, s.peaceDeals = std::max(s.peaceDeals, 1.f), s.lend = std::max(s.lend, 1.f);
            break;
        case 4:  // turtle
            s.aggression *= 0.2f, s.minOdds = std::max(s.minOdds, 0.95f), s.homeGuard += 3, s.pdsPref += 3.f, s.expansion *= 0.7f;
            for (float& v : s.vs) v *= 0.3f;
            break;
        case 5:  // rusher
            s.aggression *= 2.f, s.minOdds = std::min(s.minOdds, 0.45f), s.bigFights *= 1.5f, s.smallFights *= 1.5f;
            s.neverRetreat = true;
            for (int q = 0; q < g.numPlayers; ++q)
                if (q != p) s.vs[size_t(q)] *= 1.5f;
            break;
        default: break;
    }
}

}  // namespace

const Style& aiStyle(const Game& g, int p) {
    // cached per player until the game moves on
    static Style cache[8];
    static size_t key[8] = {};
    static const Game* who[8] = {};
    static int round[8] = {};
    static Style none;
    if (p < 0 || p >= 8 || p >= g.numPlayers) return none;
    const size_t k = g.log.size() * 31 + size_t(g.phase) * 7 + size_t(g.activePlayer + 1);
    if (who[p] != &g || key[p] != k || round[p] != g.round) {
        AiSeat seat(p);  // its own difficulty
        cache[p] = compute(g, p);
        if (aiCompetitiveSeat(g, p)) competitiveDamp(cache[p]);
        if (cache[p].on) aiCoreApply(cache[p], g, p);  // the Strategy Core: not damped, it is how the faction plays well
        applyHabits(cache[p], g, p);                       // (0.54.0) the people's habits from earlier games
        if (cache[p].on) applyPersona(cache[p], g, p);     // (test only: TI4_PERSONA)
        cache[p].unit[int(UnitType::Flagship2)] = cache[p].unit[int(UnitType::Flagship)];  // (0.52.0: the Ghemina's Lady too)
        who[p] = &g;
        key[p] = k;
        round[p] = g.round;
    }
    return cache[p];
}

}  // namespace ti
