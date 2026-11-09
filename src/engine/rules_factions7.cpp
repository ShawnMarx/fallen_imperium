// Batch 6, part 2 (0.35.0): the Firmament / the Obsidian. Our own wording in faction_text.cpp; the AI for their
// questions in ai/ai_te.cpp. Nothing here runs unless the faction is in kDone (faction_text.cpp).
//   Plots Within Plots   - other players' scored secrets, met again: a facedown plot card (Player::plotsIn) with that
//                          player's control token (Player::plotTok), no victory point (plotsWithinPlots)
//   Puppets of the Blade - with a plot in play, an action turns the Firmament into the Obsidian for good (pl.faction
//                          changes; the home system 96a becomes 96b; techs, leaders, units, note, breakthrough flip;
//                          the plots are revealed - becomeObsidian)
//   Marionettes          - the players whose tokens are on a plot are its puppets (plotPuppet, query.cpp):
//                          Enervate (secondaryAbility), Siphon (siphon), Seethe, Assail (combatMods, unit ability
//                          rolls), Extract (extractCopy)
//   Myru Vos / Vos Hollow, Captain Aroz (meetsObjective) / Aroz Hollow, Sharsiss / Sharsiss Hollow, Black Ops /
//   Malevolency, Planesplitter, Neural Parasite, Heaven's Eye, Viper EX-23 (coexistInstead) / Viper Hollow,
//   The Sowing / The Reaping
#include <algorithm>
#include <cstring>

#include "engine/breakthroughs.h"
#include "engine/map.h"
#include "engine/rules.h"

namespace ti {

namespace {
void abilityEvent7(Engine& E, int p, const std::string& name, const std::string& text, int sys = -1) {
    Event e;
    e.kind = EventKind::Ability;
    e.player = int8_t(p);
    e.label = name;
    e.text = text;
    e.system = int16_t(sys);
    E.emit(e);
}
bool keyIs(int tech, const char* k) { return tech >= 0 && tech < kTechCount && !std::strcmp(kTechs[tech].key, k); }
}  // namespace

// A facedown plot card into p's play area with victim's control token. False if none is left.
bool Rules::placePlot(int p, int victim, const std::string& why) {
    if (p < 0 || p >= G.numPlayers || victim < 0 || victim >= G.numPlayers || victim == p) return false;
    Player& pl = G.players[size_t(p)];
    std::vector<Option> opts;
    for (int k = 0; k < kPlotCount; ++k)
        if (!((pl.plotsIn >> k) & 1)) opts.push_back({k, plotName(k), plotText(k)});
    if (opts.empty()) return false;
    curFor_ = victim;
    const int k = opts.size() == 1 ? opts[0].id
                                   : fpick(p, Ab::PlotCard, "Which plot card goes face down with " + pn(victim) + "'s token? (" + why + ")", opts,
                                           "Only you know which plot it is - until you become the Obsidian.");
    curFor_ = -1;
    pl.plotsIn = uint8_t(pl.plotsIn | (1u << k));
    pl.plotTok[size_t(k)] = uint8_t(pl.plotTok[size_t(k)] | (1u << victim));
    abilityEvent7(E, p, why, pn(p) + " places a facedown plot card with " + pn(victim) + "'s control token (" + why + ").");
    return true;
}

// Plots Within Plots: the Firmament may score secrets other players have scored (actionKey: the action-phase secret
// just achieved; nullptr: status-phase secrets met now). No victory point - a plot card with their token instead.
void Rules::plotsWithinPlots(int p, const char* actionKey) {
    if (!(G.modules & kModThundersEdge) || !fac(p, "firmament") || G.players[size_t(p)].eliminated) return;
    Player& pl = G.players[size_t(p)];
    for (int q = 0; q < G.numPlayers; ++q) {
        if (q == p) continue;
        for (int16_t o : std::vector<int16_t>(G.players[size_t(q)].scored)) {
            if (kObjectives[o].kind != 3 || plotCount(pl) >= kPlotCount) continue;
            if (std::find(pl.plotted.begin(), pl.plotted.end(), o) != pl.plotted.end()) continue;
            if (actionKey ? std::strcmp(kObjectives[o].key, actionKey) != 0 : (kObjectives[o].phase != ObjPhase::Status || !meetsObjective(G, p, o)))
                continue;
            curFor_ = q;
            const bool go = fyes(p, Ab::PlotsWithin, "Plots Within Plots: score " + pn(q) + "'s secret objective " + kObjectives[o].name + "?",
                                 "No victory point: a facedown plot card with " + pn(q) + "'s control token goes into your play area.");
            curFor_ = -1;
            if (!go || !payObjectiveCost(p, o)) continue;
            pl.plotted.push_back(o);
            say(p, pn(p) + " fulfils " + pn(q) + "'s secret objective " + kObjectives[o].name + " too (Plots Within Plots).");
            placePlot(p, q, "Plots Within Plots");
        }
    }
}

// Black Ops (the Firmament's note), received by another player: the Firmament may put a plot with their token; the
// receiver gains 2 command tokens and 2 trade goods, and the note is purged.
void Rules::blackOps(int r, int note) {
    const int f = noteOwner(note);
    say(r, pn(r) + " receives Black Ops from " + pn(f) + ".");
    if (fac(f, "firmament") && plotCount(G.players[size_t(f)]) < kPlotCount) {
        curFor_ = r;
        const bool put = fyes(f, Ab::BlackOps, "Black Ops: place a facedown plot card with " + pn(r) + "'s control token?", "");
        curFor_ = -1;
        if (put) placePlot(f, r, "Black Ops");
    }
    gainTokens(r, 2, false, "Black Ops: 2 command tokens.");
    gainTG(r, 2);
    abilityEvent7(E, r, "Black Ops", pn(r) + " gains 2 command tokens and 2 trade goods; Black Ops is purged.");
}

// Puppets of the Blade: the Firmament becomes the Obsidian (The Blade's Orchestra).
void Rules::becomeObsidian(int p) {
    const int obs = findFaction("obsidian");
    if (obs < 0 || !fac(p, "firmament")) return;
    Player& pl = G.players[size_t(p)];
    const std::string was = pn(p);
    // the note: Black Ops (in our hand, or purged) turns over into Malevolency, in our hand
    const int16_t note = makeNote(p, kNoteFaction);
    bool somewhere = false;
    for (const Player& q : G.players)
        for (const auto* v : {&q.notes, &q.notesInPlay})
            if (std::find(v->begin(), v->end(), note) != v->end()) somewhere = true;
    pl.faction = int8_t(obs);
    if (!somewhere) pl.notes.push_back(note);
    // the home system turns over: Cronos / Tallin -> Cronos Hollow / Tallin Hollow (units and all)
    const int home = pl.homeSystem;
    const int hd = findSystem("96b");
    if (home >= 0 && hd >= 0 && !std::strcmp(G.systems[size_t(home)].d().key, "96a")) {
        G.systems[size_t(home)].def = int16_t(hd);
        for (int16_t x : G.systems[size_t(home)].planets) {
            const char* k = kPlanets[G.planets[size_t(x)].def].key;
            const int nd = !std::strcmp(k, "cronos") ? findPlanet("cronoshollow") : !std::strcmp(k, "tallin") ? findPlanet("tallinhollow") : -1;
            if (nd >= 0) G.planets[size_t(x)].def = int16_t(nd);
        }
    }
    // faction technologies (whoever owns them)
    static const char* const kFlip[2][2] = {{"planesplitter-firm", "planesplitter-obs"}, {"parasite-firm", "parasite-obs"}};
    for (Player& q : G.players)
        for (const auto& f : kFlip) {
            const int a = findTech(f[0]), b = findTech(f[1]);
            for (auto* v : {&q.techs, &q.exhaustedTechs})
                for (int16_t& t : *v)
                    if (t == a && b >= 0) t = int16_t(b);
            if (G.fixes >= 9 && b >= 0) {  // (0.44.5: a Nekro Valefar token on it flips too)
                if (q.assimX == a) q.assimX = int16_t(b);
                if (q.assimY == a) q.assimY = int16_t(b);
            }
        }
    // leaders: the Obsidian's (the agent ready, the others locked)
    pl.leaders.clear();
    for (int i = 0; i < kLeaderCount; ++i)
        if (kLeaders[i].faction == obs) pl.leaders.push_back({int16_t(i), uint8_t(kLeaders[i].type == kAgent ? kReady : kLocked)});
    refreshUnits(p);  // Heaven's Hollow, Viper Hollow
    abilityEvent7(E, p, "Puppets of the Blade", "The " + was + " cast off their mask: they are the Obsidian now, and their plots are revealed!", home);
    // The Blade's Orchestra: Cronos Hollow and Tallin Hollow ready
    if (home >= 0)
        for (int16_t x : G.systems[size_t(home)].planets)
            if (G.planets[size_t(x)].owner == p) G.planets[size_t(x)].exhausted = false;
    // the plots are revealed
    for (int k = 0; k < kPlotCount; ++k) {
        if (!((pl.plotsIn >> k) & 1)) continue;
        std::string who;
        for (int q = 0; q < G.numPlayers; ++q)
            if ((pl.plotTok[size_t(k)] >> q) & 1) who += (who.empty() ? "" : ", ") + pn(q);
        say(p, std::string("Plot revealed: ") + plotName(k) + (who.empty() ? " (no puppet)." : " - puppets: " + who + "."));
    }
    for (int q = 0; q < G.numPlayers; ++q) {
        if (q == p || G.players[size_t(q)].eliminated) continue;
        if (plotPuppet(G, p, kPlotSeethe, q)) {  // Seethe: every unit on one of the puppet's non-home planets
            std::vector<Option> opts;
            for (int x : controlledPlanets(G, q)) {
                if (isHomePlanet(G, x) || G.planets[size_t(x)].destroyed) continue;
                int n = 0;
                for (const Force& f : G.planets[size_t(x)].units) n += f.total();
                opts.push_back({x, planetName(x), std::to_string(n) + " units"});
            }
            if (!opts.empty()) {
                curFor_ = q;
                const int x = opts.size() == 1 ? opts[0].id : fpick(p, Ab::Seethe, "Seethe: destroy every unit on which of " + pn(q) + "'s planets?", opts, "", false, true);
                curFor_ = -1;
                const int sys = G.planets[size_t(x)].system;
                for (int s = 0; s < kSides; ++s)
                    for (int t = 0; t < kUnitTypes; ++t) {
                        const int n = G.planets[size_t(x)].units[size_t(s)].n[t];
                        if (n) destroyUnits(s, UnitType(t), n, sys, x, p, "Seethe");
                    }
                abilityEvent7(E, p, "Seethe", "Everything on " + planetName(x) + " is destroyed (Seethe).", sys);
            }
        }
        if (plotPuppet(G, p, kPlotExtract, q)) {  // Extract: one of the puppet's technologies
            std::vector<Option> opts;
            for (int16_t t : G.players[size_t(q)].techs)
                if (kTechs[t].faction < 0 && !pl.hasTech(t)) opts.push_back({t, kTechs[t].name, techSummary(t)});
            if (!opts.empty()) {
                curFor_ = q;
                const int t = opts.size() == 1 ? opts[0].id : fpick(p, Ab::Extract, "Extract: gain which of " + pn(q) + "'s technologies?", opts);
                curFor_ = -1;
                researchTech(p, t, false);
                say(p, pn(p) + " extracts " + kTechs[t].name + " from " + pn(q) + ".");
            }
        }
    }
    // Viper Hollow: a coexisting Viper takes its planet; the other player's units coexist now
    if (mechAbilities(G))
        for (size_t x = 0; x < G.planets.size(); ++x) {
            PlanetState& ps = G.planets[x];
            if (!((ps.coexist >> p) & 1) || !ps.units[size_t(p)].count(UnitType::Mech)) continue;
            const int was = ps.owner;
            ps.coexist = uint16_t(ps.coexist & ~(1u << p));
            // (0.43.0 debug passes: the old owner coexists before the control changes hands - setControl can ask
            // questions (Integrated Economy) and the planet must not hold two armies meanwhile; fixes >= 2)
            if (G.fixes >= 2 && was >= 0 && was != p) ps.coexist = uint16_t(ps.coexist | (1u << was));
            setControl(int(x), p);
            if (was >= 0 && was != p) ps.coexist = uint16_t(ps.coexist | (1u << was));
            abilityEvent7(E, p, "Viper Hollow", pn(p) + "'s Viper takes " + planetName(int(x)) + (was >= 0 ? " from " + pn(was) : std::string()) + ".", ps.system);
        }
}

// Siphon: the puppet gained commodities - the Obsidian gain as many trade goods.
void Rules::siphon(int q, int gained) {
    if (gained <= 0) return;
    const int o = obsidianOf(G);
    if (!plotPuppet(G, o, kPlotSiphon, q)) return;
    gainTG(o, gained);
    say(o, pn(o) + " gains " + std::to_string(gained) + " trade good(s) as " + pn(q) + " gains commodities (Siphon).");
}

// Extract: the puppet gained a non-faction technology - the Obsidian may pay 4 resources for it too.
void Rules::extractCopy(int q, int tech) {
    const int o = obsidianOf(G);
    if (G.phase == Phase::Setup || !plotPuppet(G, o, kPlotExtract, q) || kTechs[tech].faction >= 0 || G.players[size_t(o)].hasTech(tech)) return;
    if (spendable(G, o, false) < 4) return;
    curFor_ = q;
    const bool go = fyes(o, Ab::Extract, std::string("Extract: pay 4 resources to gain ") + kTechs[tech].name + " too?", techSummary(tech));
    curFor_ = -1;
    if (!go || !pay(o, 4, false, "Extract", true)) return;
    researchTech(o, tech, false);
    say(o, pn(o) + " extracts " + kTechs[tech].name + " from " + pn(q) + ".");
}

// Planesplitter (Firmament): gaining it puts the Fracture into play.
void Rules::firmamentGained(int p, int tech) {
    if (keyIs(tech, "planesplitter-firm") && !G.fractureIn && G.phase != Phase::Setup) fractureEnter(p, false);
}

int Rules::assail(int p, int opp) const { return plotPuppet(G, p, kPlotAssail, opp) ? 1 : 0; }

// Heaven's Eye (Firmament flagship): with a player whose token is on one of our plots in the active system, it is
// repaired at the end of each combat round (and moves 1 further - moveValue).
void Rules::heavensEye(int att, int def, int sys) {
    for (int p : {att, def}) {
        if (p < 0 || p >= G.numPlayers || !fac(p, "firmament")) continue;
        Force& f = G.systems[size_t(sys)].space[size_t(p)];
        if (!f.count(UnitType::Flagship) || !f.dmg[int(UnitType::Flagship)]) continue;
        bool marked = false;
        for (int q = 0; q < G.numPlayers; ++q)
            if (anyPuppet(G, p, q) && hasUnits(G, q, sys)) marked = true;
        if (!marked) continue;
        f.dmg[int(UnitType::Flagship)] = 0;
        say(p, pn(p) + "'s Heaven's Eye is repaired.", sys);
    }
}

// Myru Vos (Firmament agent): when a player moves ships - no space cannon against them during this movement.
// (0.43.0, T30: and ships carrying nothing move through other players' ships - G.fx.myruThrough)
void Rules::myruVos(int p, int sys) {
    if (G.fx.solarFlare || playerOfFaction(G, "firmament") < 0) return;
    const int user = agentUser(p, "firmament");
    if (user < 0) return;
    bool threat = false;  // someone could fire space cannon into the active system
    std::vector<int> near{sys};
    for (int16_t a : G.systems[size_t(sys)].adj) near.push_back(a);
    for (int q = 0; q < G.numPlayers && !threat; ++q) {
        if (q == p) continue;
        for (int s : near) {
            if (G.systems[size_t(s)].space[size_t(q)].n[int(UnitType::PDS)]) threat = true;
            for (int16_t x : G.systems[size_t(s)].planets)
                for (int t = 0; t < kUnitTypes; ++t)
                    if (G.planets[size_t(x)].units[size_t(q)].n[t] && unitDef(G, q, UnitType(t)).cannon > 0 && (s == sys || t == int(UnitType::PDS)))
                        threat = true;
        }
    }
    // 0.43.0 (T30): its other half - ships that carry nothing move through other players' ships - matters when ships
    // of others stand within reach
    bool blockers = false;
    if (G.fixes >= 2)
        for (int s2 : systemsWithin(G, sys, 2))
            for (int q = 0; q < G.numPlayers && !blockers; ++q) blockers = q != p && hasShips(G, q, s2) && s2 != sys;
    if (!threat && !blockers) return;
    agentFor(user, p);
    curValue_ = threat ? 1 : 0;  // (for the AI)
    const bool use = fyes(user, Ab::FirmamentAgent,
                          user == p ? (G.fixes >= 2 ? "Exhaust Myru Vos: no space cannon against your moving ships, and ships carrying nothing may move through other players' ships?"
                                                    : "Exhaust Myru Vos: no space cannon against your moving ships?")
                                    : (G.fixes >= 2 ? "Exhaust Myru Vos: no space cannon against " + pn(p) + "'s moving ships, and their ships carrying nothing may move through other players' ships?"
                                                    : "Exhaust Myru Vos: no space cannon against " + pn(p) + "'s moving ships?"),
                          leaderText(findLeader("firmamentagent")));
    curFor_ = -1;
    curValue_ = 0;
    if (!use) return;
    spendAgent(user, "firmament");
    agentDone(user, p);
    G.fx.solarFlare = true;
    if (G.fixes >= 2) G.fx.myruThrough = int8_t(p);
    say(user, "Space cannon can't fire at " + pn(p) + "'s moving ships (Myru Vos).", sys);
}

// Vos Hollow (Obsidian agent): when a player's ship is destroyed in a combat, their opponent must destroy one of
// theirs of the same type in the system.
void Rules::vosHollow(int att, int def, int sys, const Force& beforeA, const Force& beforeD) {
    const int o = obsidianOf(G);
    if (o < 0 || att < 0 || def < 0 || att >= G.numPlayers || def >= G.numPlayers) return;
    for (int side : {att, def}) {
        const int opp = side == att ? def : att;
        const Force& before = side == att ? beforeA : beforeD;
        int user = agentUser(side, "obsidian");
        if (user < 0 && o != att && o != def && canUseAgent(o, "obsidian")) user = o;  // (a bystander)
        if (user < 0) continue;
        for (int t : kShipIdx) {
            if (G.systems[size_t(sys)].space[size_t(side)].n[t] >= before.n[t] || !G.systems[size_t(sys)].space[size_t(opp)].n[t]) continue;
            agentFor(user, side);
            curFor_ = opp;
            const bool use = fyes(user, Ab::ObsidianAgent,
                                  "Exhaust Vos Hollow: " + pn(opp) + " must destroy one of their " + unitTypeName(UnitType(t), true) + " here too?",
                                  leaderText(findLeader("obsidianagent")));
            curFor_ = -1;
            if (!use) continue;
            spendAgent(user, "obsidian");
            agentDone(user, side);
            destroyUnits(opp, UnitType(t), 1, sys, -1, side, "Vos Hollow");
            return;
        }
    }
}

// The Reaping: a trade good on the card for each combat won against a puppet.
void Rules::reapingWon(int att, int def, int sys, int planet) {
    for (int w : {att, def}) {
        const int l = w == att ? def : att;
        if (w < 0 || w >= G.numPlayers || l < 0 || l >= G.numPlayers || !breakthroughIs(G, w, "obsidian") || !anyPuppet(G, w, l)) continue;
        const bool won = planet >= 0 ? groundForcesOn(G, w, planet) > 0 && groundForcesOn(G, l, planet) == 0
                                     : hasShips(G, w, sys) && !hasShips(G, l, sys);
        if (!won) continue;
        ++G.players[size_t(w)].sowing;
        say(w, pn(w) + " puts a trade good on The Reaping (" + std::to_string(G.players[size_t(w)].sowing) + " there).", sys);
    }
}

// The Sowing: up to 3 of our trade goods onto the card (when gained, and at the start of each status phase).
void Rules::sowingBank(int p) {
    Player& pl = G.players[size_t(p)];
    if (!breakthroughIs(G, p, "firmament") || pl.tradeGoods <= 0) return;
    std::vector<Option> opts;
    for (int k = std::min<int>(3, pl.tradeGoods); k >= 0; --k)
        opts.push_back({k, k ? "Put " + std::to_string(k) + " trade good" + (k > 1 ? "s" : "") + " on it" : std::string("None"),
                        "They stay there; as the Obsidian (The Reaping) you get them back twice over."});
    const int k = fpick(p, Ab::Sowing, "The Sowing: put trade goods on it?", opts);
    if (k <= 0) return;
    pl.tradeGoods = int8_t(pl.tradeGoods - k);
    pl.sowing = int8_t(pl.sowing + k);
    say(p, pn(p) + " puts " + std::to_string(k) + " trade good(s) on The Sowing (" + std::to_string(pl.sowing) + " there).");
}

// Start of the status phase: Neural Parasite (Firmament), The Sowing, The Reaping, Seethe.
void Rules::firmamentStatusStart() {
    if (!(G.modules & kModThundersEdge)) return;
    for (int p = 0; p < G.numPlayers; ++p) {
        Player& pl = G.players[size_t(p)];
        if (pl.eliminated) continue;
        if (owns(p, "parasite-firm") && unitsAvailable(G, p, UnitType::Infantry) > 0 && pl.homeSystem >= 0) {
            std::vector<Option> opts;
            for (int16_t x : G.systems[size_t(pl.homeSystem)].planets)
                if (G.planets[size_t(x)].owner == p) opts.push_back({x, planetName(x), ""});
            if (!opts.empty()) {
                opts.push_back({-1, "No infantry", ""});
                const int x = fpick(p, Ab::NeuralParasite, "Neural Parasite: put 1 infantry on a planet in your home system?", opts, "", false, true);
                if (x >= 0) {
                    addUnits(p, UnitType::Infantry, 1, pl.homeSystem, x);
                    say(p, pn(p) + " places 1 infantry on " + planetName(x) + " (Neural Parasite).", pl.homeSystem);
                }
            }
        }
        sowingBank(p);
        if (breakthroughIs(G, p, "obsidian") && pl.sowing > 0) {  // The Reaping
            const int k = pl.sowing;
            pl.sowing = 0;
            gainTG(p, 2 * k);
            abilityEvent7(E, p, "The Reaping", pn(p) + " reaps " + std::to_string(2 * k) + " trade goods (" + std::to_string(k) + " on the card, as many again).");
        }
        for (int q = 0; q < G.numPlayers; ++q) {  // Seethe: 1 of the puppet's infantry
            if (!plotPuppet(G, p, kPlotSeethe, q)) continue;
            std::vector<Option> opts;  // id = planet + 1, or -(system + 1) for a space area
            for (size_t s = 0; s < G.systems.size(); ++s) {
                if (G.systems[s].space[size_t(q)].count(UnitType::Infantry)) opts.push_back({-int(s) - 1, sysName(int(s)) + " (space)", ""});
                for (int16_t x : G.systems[s].planets)
                    if (G.planets[size_t(x)].units[size_t(q)].count(UnitType::Infantry)) opts.push_back({x + 1, planetName(x), ""});
            }
            if (opts.empty()) continue;
            curFor_ = q;
            const int c = opts.size() == 1 ? opts[0].id : fpick(p, Ab::Seethe, "Seethe: destroy 1 of " + pn(q) + "'s infantry - where?", opts);
            curFor_ = -1;
            const int planet = c > 0 ? c - 1 : -1, sys = c > 0 ? G.planets[size_t(c - 1)].system : -c - 1;
            destroyUnits(q, UnitType::Infantry, 1, sys, planet, p, "Seethe");
        }
    }
}

// End of the status phase: Malevolency costs its holder (anyone but the Obsidian) a fleet token.
void Rules::firmamentStatusEnd() {
    const int o = obsidianOf(G);
    if (o < 0) return;
    const int16_t note = makeNote(o, kNoteFaction);
    for (int q = 0; q < G.numPlayers; ++q) {
        Player& pl = G.players[size_t(q)];
        if (q == o || pl.eliminated || std::find(pl.notes.begin(), pl.notes.end(), note) == pl.notes.end() || pl.fleet <= 0) continue;
        --pl.fleet;
        say(q, pn(q) + " loses a fleet token (Malevolency).");
    }
}

// Neural Parasite (Obsidian): at the start of our turn, 1 of another player's infantry in or next to a system with
// our infantry is destroyed.
void Rules::obsidianTurnStart(int p) {
    if (!(G.modules & kModThundersEdge) || !owns(p, "parasite-obs")) return;
    std::vector<char> near(G.systems.size(), 0);
    for (size_t s = 0; s < G.systems.size(); ++s) {
        bool inf = G.systems[s].space[size_t(p)].count(UnitType::Infantry) > 0;
        for (int16_t x : G.systems[s].planets) inf = inf || G.planets[size_t(x)].units[size_t(p)].count(UnitType::Infantry) > 0;
        if (!inf) continue;
        near[s] = 1;
        for (int16_t a : G.systems[s].adj) near[size_t(a)] = 1;
    }
    std::vector<Option> opts;  // id = (planet + 1, or 0 for space) * 1024 + system * 8 + player
    for (size_t s = 0; s < G.systems.size(); ++s) {
        if (!near[s]) continue;
        for (int q = 0; q < G.numPlayers; ++q) {
            if (q == p) continue;
            if (G.systems[s].space[size_t(q)].count(UnitType::Infantry)) opts.push_back({int(s) * 8 + q, pn(q) + "'s infantry in " + sysName(int(s)), ""});
            for (int16_t x : G.systems[s].planets)
                if (G.planets[size_t(x)].units[size_t(q)].count(UnitType::Infantry))
                    opts.push_back({(x + 1) * 1024 + int(s) * 8 + q, pn(q) + "'s infantry on " + planetName(x), ""});
        }
    }
    if (opts.empty()) return;
    const int c = opts.size() == 1 ? opts[0].id : fpick(p, Ab::Parasite2, "Neural Parasite: destroy which infantry?", opts);
    const int q = c % 8, sys = (c % 1024) / 8, planet = c / 1024 - 1;
    destroyUnits(q, UnitType::Infantry, 1, sys, planet, p, "Neural Parasite");
}

// Planesplitter (Obsidian): after a strategic action, an ingress token may move next to our units.
void Rules::planesplitter(int p) {
    if (!(G.modules & kModThundersEdge) || !owns(p, "planesplitter-obs") || !G.fractureIn) return;
    std::vector<Option> from, to;
    for (size_t s = 0; s < G.systems.size(); ++s) {
        const SystemState& sy = G.systems[s];
        if (sy.ingress) from.push_back({int(s), sysName(int(s)), ""});
        if (sy.ingress || sy.fracture || sy.isHyperlane()) continue;
        bool near = hasUnits(G, p, int(s));
        for (int16_t a : sy.adj) near = near || hasUnits(G, p, a);
        if (near) to.push_back({int(s), sysName(int(s)), ""});
    }
    if (from.empty() || to.empty()) return;
    from.push_back({-1, "Leave them", ""});
    const int a = fpick(p, Ab::Planesplitter, "Planesplitter: move an ingress token?", from, "Into a system with or next to your units.", true);
    if (a < 0) return;
    const int b = fpick(p, Ab::Planesplitter, "Move it to which system?", to, "", true);
    G.systems[size_t(a)].ingress = false;
    G.systems[size_t(b)].ingress = true;
    say(p, "An ingress token moves from " + sysName(a) + " to " + sysName(b) + " (Planesplitter).", b);
    computeAdjacency(G);
}

// Malevolency: at the end of its holder's tactical action, they may pay 1 influence to give it to a neighbour.
void Rules::malevolency(int p) {
    const int o = obsidianOf(G);
    if (o < 0 || p < 0 || G.players[size_t(p)].eliminated) return;
    const int16_t note = makeNote(o, kNoteFaction);
    const Player& pl = G.players[size_t(p)];
    if (std::find(pl.notes.begin(), pl.notes.end(), note) == pl.notes.end() || spendable(G, p, true) < 1) return;
    std::vector<Option> opts;
    for (int q = 0; q < G.numPlayers; ++q)
        if (q != p && !G.players[size_t(q)].eliminated && areNeighbours(G, p, q)) opts.push_back({q, pn(q), ""});
    if (opts.empty()) return;
    opts.push_back({-1, "Keep it", ""});
    const int q = fpick(p, Ab::Malevolency, "Malevolency: pay 1 influence to give it to a neighbour?", opts, factionNoteText(G.players[size_t(o)].faction, kNoteFaction));
    if (q < 0 || !pay(p, 1, true, "Malevolency", true)) return;
    giveNote(p, q, note);
}

// Enervate: the Obsidian use the secondary of their puppet's strategy card for free (Leadership: the primary).
bool Rules::enervateFree(int q) const {
    const int w = strategicPlayer_;
    return w >= 0 && w != q && plotPuppet(G, q, kPlotEnervate, w);
}
bool Rules::enervate(int q, int card) {
    if (card != 0 || !enervateFree(q)) return false;
    if (!fyes(q, Ab::Enervate, "Enervate: resolve Leadership's primary ability (not the secondary)?", plotText(kPlotEnervate))) return false;
    say(q, pn(q) + " uses Leadership's primary ability (Enervate).");
    leadership(q, true);
    return true;
}

std::vector<Option> Rules::factionActionOptions9(int p) {
    std::vector<Option> o;
    auto add = [&](Ab ab, const std::string& label, const std::string& detail) { o.push_back({6000 + int(ab), label, detail}); };
    const Player& pl = G.players[size_t(p)];
    if (fac(p, "firmament") && pl.plotsIn) add(Ab::Puppets, "Puppets of the Blade", "Become the Obsidian for good: your plots are revealed and their puppets suffer them.");
    if (fac(p, "firmament") && heroUnlocked(G, p)) {
        bool free = plotCount(pl) < kPlotCount;
        if (free) add(Ab::FirmamentHero, "Hero: Sharsiss", "Place a plot card with another player's token, then another token on one of your plots. Purged afterwards.");
    }
    if (fac(p, "obsidian") && heroUnlocked(G, p)) add(Ab::ObsidianHero, "Hero: Sharsiss Hollow", "Ready all your planets. Purged afterwards.");
    return o;
}

bool Rules::factionAction9(int p, Ab ab) {
    Player& pl = G.players[size_t(p)];
    switch (ab) {
        case Ab::Puppets:
            if (!fac(p, "firmament") || !pl.plotsIn) return false;
            becomeObsidian(p);
            return true;
        case Ab::FirmamentHero: {
            std::vector<Option> who;
            for (int q = 0; q < G.numPlayers; ++q)
                if (q != p && !G.players[size_t(q)].eliminated) who.push_back({q, pn(q), ""});
            if (who.empty() || plotCount(pl) >= kPlotCount) return false;
            const int q = fpick(p, Ab::FirmamentHero, "Sharsiss: a plot card with whose control token?", who);
            purgeHero(p);
            placePlot(p, q, "Sharsiss");
            std::vector<Option> tok;  // id = plot * 8 + player
            for (int k = 0; k < kPlotCount; ++k) {
                if (!((pl.plotsIn >> k) & 1)) continue;
                for (int x = 0; x < G.numPlayers; ++x)
                    if (x != p && !G.players[size_t(x)].eliminated && !((pl.plotTok[size_t(k)] >> x) & 1))
                        tok.push_back({k * 8 + x, pn(x) + "'s token on " + plotName(k), ""});
            }
            if (tok.empty()) return true;
            tok.push_back({-1, "No more tokens", ""});
            const int c = fpick(p, Ab::PlotToken, "Sharsiss: put another player's token on one of your plots?", tok);
            if (c < 0) return true;
            pl.plotTok[size_t(c / 8)] = uint8_t(pl.plotTok[size_t(c / 8)] | (1u << (c % 8)));
            say(p, pn(p) + " puts " + pn(c % 8) + "'s control token on a plot card (Sharsiss).");
            return true;
        }
        case Ab::ObsidianHero: {
            purgeHero(p);
            int n = 0;
            for (int x : controlledPlanets(G, p))
                if (G.planets[size_t(x)].exhausted) {
                    G.planets[size_t(x)].exhausted = false;
                    ++n;
                }
            abilityEvent7(E, p, "Sharsiss Hollow", pn(p) + " readies " + std::to_string(n) + " planet(s) (Sharsiss Hollow).");
            return true;
        }
        default:
            if (int(ab) >= int(Ab::DsZealous)) return dsAction(p, ab);  // Discordant Stars (rules_ds.cpp)
            return eventAction(p, ab);  // galactic events (rules_events.cpp)
    }
}

}  // namespace ti
