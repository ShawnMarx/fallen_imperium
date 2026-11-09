// ti4 --uitest OUTDIR : screenshots for checking the screens without playing
// by hand.
//  1. plays AI games until each kind of decision comes up for player 1 and
//     saves its on-screen editor (ui_*.ppm);
//  2. saves the reference screens (faction sheet, technology, planet and
//     unit cards) from a mid-game position (info_*.ppm);
//  3. runs the real game screen with computer players only at normal speed
//     and saves frames of the announcements, flashes and combat panel
//     (watch_*.ppm).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "ai/ai.h"
#include "app/app.h"
#include "core/settings.h"
#include "engine/breakthroughs.h"
#include "engine/deal.h"
#include "engine/engine.h"
#include "engine/map.h"
#include "engine/query.h"
#include "engine/rules.h"
#include "game/habits.h"
#include "game/session.h"
#include "gfx/canvas.h"
#include "platform/platform.h"
#include "ui/play_area.h"
#include "ui/transition.h"
#include "ui/cards.h"
#include "ui/decision_ui.h"
#include "ui/info_screens.h"
#include "ui/map_view.h"
#include "ui/retro.h"
#include "ui/backdrops.h"
#include "ui/breakthrough_card.h"
#include "ui/ground_battle.h"
#include "ui/ground_units.h"
#include "ui/hits.h"
#include "ui/icons.h"
#include "ui/painted.h"
#include "ui/space_battle.h"
#include "ui/tile_view.h"
#include "ui/voidfall.h"
#include "ui/theme.h"
#include "ui/widgets.h"

std::unique_ptr<Scene> makeGameScene(std::unique_ptr<GameSession> session);

namespace {

// TI4_SCALE=2.25: the screens drawn as on a 1080p PC monitor (0.27.0)
float testScale() {
    static const float s = [] {
        const char* e = std::getenv("TI4_SCALE");
        const float v = e ? float(std::atof(e)) : 1.f;
        Canvas::screenScale() = v > 1.f ? v : 1.f;
        return v > 1.f ? v : 1.f;
    }();
    return s;
}

// TI4_SCREENW=854: the game screens drawn as on a 16:9 PC (widescreen, 0.46.0; 768 = the Steam Deck)
int testW() {
    static const int w = [] {
        const char* e = std::getenv("TI4_SCREENW");
        const int v = e ? std::max(640, std::min(1200, std::atoi(e))) : 640;
        theme::screenWRef() = v;
        return v;
    }();
    return w;
}

// as the app draws a screen: the whole width, or laid out for 640 and centred
void drawTop(Scene& sc, App& app, Canvas& c, uint32_t t) {
    if (sc.wide() || c.width() <= theme::kScreenW) {
        sc.draw(app, c, t);
        return;
    }
    c.clear(0);
    CentredView v(c);
    sc.draw(app, c, t);
}

void savePpm(const Canvas& c, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", c.pw(), c.ph());
    for (int i = 0; i < c.pw() * c.ph(); ++i) {
        uint32_t p = c.pixels()[i];
        unsigned char rgbv[3] = {uint8_t(p >> 16), uint8_t(p >> 8), uint8_t(p)};
        std::fwrite(rgbv, 1, 3, f);
    }
    std::fclose(f);
}

// Which screenshot (if any) a decision should produce.
std::string screenName(const ti::Game& g, const ti::Decision& d) {
    using namespace ti;
    switch (d.tag) {
        case Tag::BombardPlan: return "bombard";
        case Tag::RemoveExcess: return "remove_excess";
        case Tag::ActivateConfirm: return "activate_confirm";
        case Tag::TokenSpend: return "token_spend";
        case Tag::CannonTarget: return "cannon_target";
        case Tag::ActionChoice: return d.options.size() >= 4 ? "action_choice" : "";
        case Tag::PlayActionCard: return "play_card";
        case Tag::VoteOutcome:  // (0.44.6: by the agenda's kind - "vote" used to need a For / Against without Abstain)
            return d.agenda >= 0 && d.agenda < kAgendaCount && kAgendas[d.agenda].elect != Elect::ForAgainst ? "vote_elect" : "vote";
        case Tag::VotePlanets: return d.planets.size() >= 3 ? "vote_planets" : "";
        case Tag::Predict: return "predict";
        case Tag::AgendaPeek: return "agenda_peek";
        case Tag::DiscardActionCard: return "discard_card";
        case Tag::ExploreChoice: return "explore_choice";
        case Tag::RelicChoice: return "relic_choice";
        case Tag::LegendaryAbility: return "legendary";
        case Tag::ExploreOrder: return "explore_order";
        case Tag::Faction: return d.options.size() >= 3 ? "faction_pick" : "faction_yesno";
        case Tag::Negotiate:
            if (g.phase == Phase::Agenda) return d.options.size() >= 3 ? "negotiate_vote" : "";
            return d.options.size() >= 3 ? "negotiate" : "";
        case Tag::DealOffer: return g.phase == Phase::Agenda ? "deal_vote" : "deal";
        case Tag::DealResponse: {
            Deal x;
            if (!decodeDeal(d.deal, x)) return "";
            if (!x.promises.empty() && x.contract()) return "deal_response";
            return x.contract() ? "deal_response_simple" : "";
        }
        case Tag::DealCounter: return "deal_counter";
        case Tag::TradeX1Offer: return "trade_x1_offer";
        case Tag::TradeX1Accept: return "trade_x1_accept";
        case Tag::TradeX1Settle: return "trade_x1_settle";
        case Tag::TradeReplenish: return "trade_gift";
        default: break;
    }
    switch (d.kind) {
        case DecisionKind::Move: {
            Rng r(1);
            Answer a = aiAnswer(g, d, r);
            if (a.data.size() < 8) return "";
            for (size_t i = 0; i + 3 < a.data.size(); i += 4)  // something picked up from a planet
                if (a.data[i + 1] >= 0) return "move_pickup";
            return "move";
        }
        case DecisionKind::Produce: return d.amount >= 4 ? "produce" : "";
        case DecisionKind::Hits: return d.hits >= 1 ? "hits" : "";
        case DecisionKind::Land: return "land";
        case DecisionKind::Pay: return d.planets.size() >= 3 ? "pay" : "";
        case DecisionKind::Tokens: return d.redistribute ? "tokens" : "";
        case DecisionKind::Deal: return "deal";
        case DecisionKind::Pick:
            if (d.onMap) return d.options.size() > 3 ? "pick_map" : "";
            return d.options.size() >= 3 ? "pick" : "";
    }
    return "";
}

void drawInfo(InfoScreen& s, const std::string& path, const std::vector<Btn>& presses) {
    Input in;
    for (Btn b : presses) {
        in.beginFrame();
        in.handle(b, true, 100);
        s.update(in);
        in.beginFrame();
        in.handle(b, false, 150);
    }
    Canvas c(640, 480, testScale());
    drawSpaceBackground(c, false);
    s.draw(c, 500);
    savePpm(c, path);
}

int decisionScreens(const std::string& outDir) {
    using namespace ti;
    const std::set<std::string> wanted = {"pick",   "pick_map",       "pay",        "move",        "move_pickup",   "produce",
                                          "hits",   "land",           "tokens",     "bombard",     "remove_excess", "action_choice",
                                          "play_card", "vote",       "vote_elect", "vote_planets", "predict",     "agenda_peek",
                                          "discard_card", "explore_choice", "relic_choice", "legendary", "faction_pick",
                                          "faction_yesno", "negotiate", "negotiate_vote", "deal_vote", "deal", "deal_counter", "deal_response",
                                          "deal_response_simple", "trade_x1_offer", "trade_x1_accept", "trade_gift"};
    std::set<std::string> done;
    bool infoDone = false, relicDone = false, attachDone = false;
    for (uint32_t seed = 1; seed < 120 && (done.size() < wanted.size() || !infoDone || !relicDone || !attachDone); ++seed) {
        Engine e;
        Setup s;
        s.players = 3 + int(seed % 6);
        // seat 1 (whose screens are captured) plays one of the factions with working abilities
        static const char* kWorking[] = {"sol", "letnev", "hacan", "jolnar", "sardakk", "xxcha",
                                         "arborec", "l1z1x", "mentak", "naalu", "yin", "yssaril",
                                         "ghost", "muaat", "saar", "winnu", "nekro", "empyrean",
                                         "argent", "mahact", "naaz", "nomad", "titans", "cabal"};
        s.factions = {-1, findFaction(kWorking[seed % 24])};
        s.human = -1;
        s.seed = seed * 101;
        e.start(s);
        Rng rng(seed);
        for (int steps = 0; e.waiting() && steps < 6000; ++steps) {
            const Decision& d = e.decision();
            std::string name = d.player == 1 ? screenName(e.game(), d) : "";
            if (!name.empty() && wanted.count(name) && !done.count(name)) {
                Canvas c(640, 480, testScale());
                MapView map;
                map.setViewport(Rect{0, 22, 420, 430});
                map.setCursor(e.game(), d.system >= 0 ? d.system : e.game().players[1].homeSystem);
                drawSpaceBackground(c, false);
                std::unique_ptr<DecisionUI> ui = makeDecisionUI(e.game(), d, map);
                if (d.kind == DecisionKind::Deal && name != "deal_vote") {  // the composer: fill in the suggested deal, move to a promise
                    std::vector<Btn> keys = {Btn::Y};
                    if (name == "deal")  // down to "Promise 1", choose "Stay out of a system", then its system row
                        keys.insert(keys.end(), {Btn::Up, Btn::Up, Btn::Right, Btn::Right, Btn::Down, Btn::Down});
                    for (Btn b : keys) {
                        Input in;
                        in.beginFrame();
                        in.handle(b, true, 100);
                        ui->update(in, 100);
                    }
                }
                map.setActiveSystem(e.game().activeSystem);
                map.draw(c, e.game(), 0);
                ui->draw(c, 0);
                drawHintBar(c, ui->hints());
                savePpm(c, outDir + "/ui_" + name + ".ppm");
                std::printf("uitest: %s (seed %u, step %d): %s\n", name.c_str(), seed, steps, d.prompt.c_str());
                done.insert(name);
            }
            // 0.28.0: research from the technology cards (the grid, then a card opened)
            if (d.player == 1 && d.tag == Tag::Research && d.kind == DecisionKind::Pick && !done.count("research")) {
                for (int k = 0; k < 2; ++k) {
                    Canvas c(640, 480, testScale());
                    MapView map;
                    map.setViewport(Rect{0, 22, 420, 430});
                    drawSpaceBackground(c, false);
                    map.draw(c, e.game(), 0);
                    std::unique_ptr<DecisionUI> ui = makeDecisionUI(e.game(), d, map);
                    std::vector<Btn> keys = {Btn::Right};
                    if (k == 1) keys.push_back(Btn::A);
                    for (Btn b : keys) {
                        Input in;
                        in.beginFrame();
                        in.handle(b, true, 100);
                        ui->update(in, 100);
                    }
                    ui->draw(c, 0);
                    drawHintBar(c, ui->hints());
                    savePpm(c, outDir + (k ? "/ui_research_card.ppm" : "/ui_research.ppm"));
                }
                done.insert("research");
            }
            // Reference screens from a position in round 4.
            if (!infoDone && e.game().round >= 4 && e.game().activeSystem < 0) {
                const Game& g = e.game();
                drawInfo(*makeFactionSheet(g, 1), outDir + "/info_faction.ppm", {});
                drawInfo(*makeFactionSheet(g, 1), outDir + "/info_faction_cards.ppm", {Btn::A});
                drawInfo(*makeFactionSheet(g, 1), outDir + "/info_faction_card.ppm", {Btn::A, Btn::Right, Btn::Right, Btn::Right, Btn::A});
                drawInfo(*makeTechScreen(g, 1), outDir + "/info_tech.ppm", {});
                drawInfo(*makeTechScreen(g, 1), outDir + "/info_tech_all.ppm", {Btn::R1, Btn::R1, Btn::Down, Btn::Right});
                drawInfo(*makeTechScreen(g, 1), outDir + "/info_tech_card.ppm", {Btn::R1, Btn::R1, Btn::Down, Btn::A});
                {  // 0.43.3: the Codex's Omega cards carry the sign; Thunder's Edge's Construction / Warfare wording
                    setCodexTexts(true, true);
                    drawInfo(*makeTechScreen(g, 1), outDir + "/info_tech_codex.ppm", {Btn::R1, Btn::R1, Btn::Down, Btn::Right});
                    setCodexTexts(false);
                    setStrategyTexts(true);
                    Canvas c(640, 480, testScale());
                    drawSpaceBackground(c, false);
                    cards::strategy(c, 5, 10, 10, 400);
                    cards::strategy(c, 3, 10, 245, 400);
                    savePpm(c, outDir + "/info_strategy_te.ppm");
                    setStrategyTexts((g.modules & kModThundersEdge) != 0);
                }
                {  // 0.29.0: a Valefar Assimilator token on a technology seat 1 copied from seat 2
                    Game g2 = g;
                    for (int16_t t : g2.players[2].techs) {
                        g2.players[1].assimX = t;
                        break;
                    }
                    drawInfo(*makeTechScreen(g2, 1), outDir + "/info_tech_valefar.ppm", {});
                }
                drawInfo(*makePlanetScreen(g, 1), outDir + "/info_planets.ppm", {});
                drawInfo(*makePlanetScreen(g, 1), outDir + "/info_planet_card.ppm", {Btn::A});
                drawInfo(*makeUnitScreen(g, 1), outDir + "/info_units.ppm", {});
                drawInfo(*makeUnitScreen(g, 1), outDir + "/info_unit_card.ppm", {Btn::Right, Btn::Right, Btn::Right, Btn::A});
                drawInfo(*makeHandScreen(g, 1), outDir + "/info_hand.ppm", {});
                drawInfo(*makeHandScreen(g, 1), outDir + "/info_hand_card.ppm", {Btn::A});
                drawInfo(*makeHandScreen(g, 1), outDir + "/info_notes.ppm", {Btn::R1});
                drawInfo(*makeLawScreen(g, 1), outDir + "/info_laws.ppm", {Btn::R1});
                drawInfo(*makeLawScreen(g, 1), outDir + "/info_law_card.ppm", {Btn::R1, Btn::Down, Btn::A});
                std::printf("uitest: reference screens (seed %u, round %d)\n", seed, g.round);
                infoDone = true;
            }
            // Relics tab once player 2 (index 1) holds a relic and a fragment.
            if (!relicDone && e.game().activeSystem < 0 && !e.game().players[1].relics.empty() &&
                !e.game().players[1].fragments.empty()) {
                const Game& g = e.game();
                drawInfo(*makeHandScreen(g, 1), outDir + "/info_relics.ppm", {Btn::R1, Btn::R1});
                drawInfo(*makeHandScreen(g, 1), outDir + "/info_relic_card.ppm", {Btn::R1, Btn::R1, Btn::A});
                drawInfo(*makeFactionSheet(g, 1), outDir + "/info_faction_relics.ppm", {});
                std::printf("uitest: relic screens (seed %u, round %d)\n", seed, g.round);
                relicDone = true;
            }
            // A planet card with an attachment (or a legendary planet).
            if (!attachDone && e.game().activeSystem < 0) {
                const Game& g = e.game();
                int k = 0, at = -1;
                for (size_t pl = 0; pl < g.planets.size(); ++pl) {
                    if (g.planets[pl].owner != 1) continue;
                    if (!g.planets[pl].attachments.empty() && g.round >= 3) {
                        at = k;
                        break;
                    }
                    ++k;
                }
                if (at >= 0) {
                    std::vector<Btn> presses(size_t(at), Btn::Right);
                    drawInfo(*makePlanetScreen(g, 1), outDir + "/info_planets_attach.ppm", presses);
                    presses.push_back(Btn::A);
                    drawInfo(*makePlanetScreen(g, 1), outDir + "/info_planet_attach.ppm", presses);
                    std::printf("uitest: attachment planet screens (seed %u, round %d)\n", seed, g.round);
                    attachDone = true;
                }
            }
            Answer a = aiAnswer(e.game(), d, rng);
            if (!e.answer(a)) break;
            e.takeEvents();
        }
    }
    // Faction cards (faction sheet, A) of the newest batch: 3 scroll positions each.
    for (const char* key : {"argent", "mahact", "naaz", "nomad", "titans", "cabal"}) {
        Engine e;
        Setup s;
        s.players = 6;
        s.factions = {-1, findFaction(key)};
        s.human = -1;
        s.seed = 4242;
        e.start(s);
        Rng rng(9);
        for (int steps = 0; e.waiting() && steps < 3000 && e.game().phase != Phase::Action; ++steps) {
            Answer a = aiAnswer(e.game(), e.decision(), rng);
            if (!e.answer(a)) break;
            e.takeEvents();
        }
        const Game& g = e.game();
        std::string base = outDir + "/info_fc_" + key;
        drawInfo(*makeFactionSheet(g, 1), base + "_sheet.ppm", {});
        drawInfo(*makeFactionSheet(g, 1), base + "_1.ppm", {Btn::A});
        drawInfo(*makeFactionSheet(g, 1), base + "_2.ppm", {Btn::A, Btn::Down, Btn::Down, Btn::Down});
        drawInfo(*makeFactionSheet(g, 1), base + "_3.ppm", {Btn::A, Btn::Down, Btn::Down, Btn::Down, Btn::Down, Btn::Down, Btn::Down});
    }
    for (const std::string& w : wanted)
        if (!done.count(w)) std::printf("uitest: no '%s' screen came up\n", w.c_str());
    std::printf("uitest: %zu decision screens captured\n", done.size());
    return int(done.size());
}

// Watch the computer players on the real game screen and save frames of the
// announcements.
// (0.42.2: as the bots change, one game no longer shows every announcement - space cannon, a relic -
// so more games are watched until each frame has come up once)
static int watchGame(const std::string& outDir, uint32_t seed, std::set<std::string>& done, bool overlays, size_t& wantCount) {
    std::unique_ptr<Platform> plat = makeHeadlessPlatform("", outDir);
    Settings settings;
    settings.setInt("ai_speed", 1);
    App app(*plat, settings);
    GameSession::savePath() = outDir + "/watch_save.ti4";
    std::unique_ptr<GameSession> session(new GameSession());
    ti::Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = seed;  // 19: a game that reaches the agenda phase early
    if (!session->startNew(s)) return 0;
    GameSession* sp = session.get();
    std::unique_ptr<Scene> scene = makeGameScene(std::move(session));
    Input in;
    Canvas c(testW(), 480, testScale());
    // what to capture: name -> (feed predicate, delay after it appears)
    struct Want {
        const char* name;
        ti::EventKind kind;
        const char* text;  // must appear in the line ("" = any)
        uint32_t delay;
    };
    const Want wants[] = {
        {"strategy_pick", ti::EventKind::StrategyPick, "", 450},
        {"turn", ti::EventKind::TurnStart, "", 300},
        {"activate", ti::EventKind::Activate, "", 500},
        {"activate_enemy", ti::EventKind::Activate, "enemy", 600},
        {"move", ti::EventKind::Move, "", 450},
        {"strategic_action", ti::EventKind::ActionChosen, "plays", 700},
        {"combat_start", ti::EventKind::CombatStart, "", 700},
        {"combat_dice", ti::EventKind::Dice, "Combat round", 500},
        {"combat_end", ti::EventKind::CombatEnd, "", 600},
        {"cannon", ti::EventKind::Dice, "Space cannon", 500},
        {"turn_end", ti::EventKind::TurnEnd, "ends turn", 500},
        {"pass", ti::EventKind::TurnEnd, "passes", 500},
        {"score", ti::EventKind::Score, "", 600},
        {"objective", ti::EventKind::Phase, "New public objective", 600},
        {"round_end", ti::EventKind::RoundEnd, "", 600},
        {"research", ti::EventKind::Research, "", 500},
        {"action_card", ti::EventKind::ActionCard, "", 600},
        {"agenda", ti::EventKind::Agenda, "", 900},
        {"votes", ti::EventKind::Vote, "", 500},
        {"agenda_result", ti::EventKind::AgendaResult, "", 900},
        {"explore_back", ti::EventKind::Explore, "", 250},
        {"explore", ti::EventKind::Explore, "", 1200},
        {"relic", ti::EventKind::Relic, "", 700},
        {"leader", ti::EventKind::Leader, "", 700},
        {"ability", ti::EventKind::Ability, "", 600},
        {"deal_offer", ti::EventKind::Deal, " offers ", 500},
        {"deal_made", ti::EventKind::Deal, "make a deal", 600},
        {"deal_declined", ti::EventKind::Deal, "declines", 500},
        {"reveal", ti::EventKind::Reveal, "", 700},
        {"betrayal", ti::EventKind::Betrayal, "", 900},
        {"betrayal_talk", ti::EventKind::Betrayal, "", 7400},  // 0.57.0: the betrayer's words and the answer
        {"deal_talk", ti::EventKind::Deal, " offers ", 1400},  // 0.57.0: the comms strip, words typed
        {"secret_scored", ti::EventKind::ObjectiveScored, "", 700},  // 0.16.0: cards shown large until A
        {"relic_used", ti::EventKind::RelicUsed, "", 700},
        {"component", ti::EventKind::ComponentUsed, "", 700},
        {"planetfall", ti::EventKind::Planetfall, "", 1700},  // 0.24.0: the band of light coming down
        {"planetfall_council", ti::EventKind::Planetfall, "", 3400},
    };
    std::map<std::string, uint32_t> pending;  // name -> capture time
    wantCount = sizeof(wants) / sizeof(wants[0]);
    uint32_t lastFeedTime = 0;
    int captured = 0;
    uint32_t holdSince = 0;
    bool releaseA = false;
    // the Players and Objectives overlays (round 4, so objectives and secrets are on the board)
    bool overlaysDone = !overlays;
    auto captureOverlays = [&](uint32_t t) {
        auto press = [&](Btn b) {
            in.beginFrame();
            in.update(t);
            in.handle(b, true, t);
            scene->update(app, in, t);
            t += 33;
            in.beginFrame();
            in.update(t);
            in.handle(b, false, t);
            scene->update(app, in, t);
            t += 33;
        };
        const std::pair<Btn, const char*> overlays[] = {{Btn::Select, "players"}, {Btn::X, "objectives"}};
        for (const auto& o : overlays) {
            press(o.first);
            drawTop(*scene, app, c, t);
            savePpm(c, outDir + "/overlay_" + o.second + ".ppm");
            press(Btn::B);
        }
        // the faction web with archetypes and attitudes (after round 3)
        press(Btn::Menu);
        press(Btn::Down);
        press(Btn::A);
        press(Btn::Right);
        drawTop(*scene, app, c, t);
        savePpm(c, outDir + "/overlay_web.ppm");
        press(Btn::B);
        press(Btn::B);
            overlaysDone = true;
    };
    for (uint32_t t = 1000; t < 1000 + 30 * 60 * 1000 && done.size() < sizeof(wants) / sizeof(wants[0]); t += 33) {
        in.beginFrame();
        in.update(t);
        // Cards that wait for A: read for a moment, then press A.
        if (releaseA) {
            in.handle(Btn::A, false, t);
            releaseA = false;
        } else if (sp->holding()) {
            if (!holdSince) holdSince = t;
            // (the planetfall's banner comes late; 0.57.0: the betrayal's two lines type out)
            if (t - holdSince > (sp->heldKind() == ti::EventKind::Planetfall ? 4200u : sp->heldKind() == ti::EventKind::Betrayal ? 7600u : 1500u)) {
                in.handle(Btn::A, true, t);
                releaseA = true;
                holdSince = 0;
            }
        }
        scene->update(app, in, t);
        const auto& feed = sp->feed();
        // the feed is capped, so new lines are found by their timestamps
        size_t start = feed.size();
        while (start > 0 && feed[start - 1].time >= lastFeedTime) --start;
        for (size_t i = start; i < feed.size(); ++i) {
            for (const Want& w : wants) {
                if (done.count(w.name) || pending.count(w.name)) continue;
                if (feed[i].kind != w.kind) continue;
                if (*w.text && feed[i].text.find(w.text) == std::string::npos) continue;
                pending[w.name] = t + w.delay;
            }
        }
        if (!feed.empty()) lastFeedTime = feed.back().time + 1;
        for (auto it = pending.begin(); it != pending.end();) {
            if (int32_t(t - it->second) >= 0) {
                drawTop(*scene, app, c, t);
                savePpm(c, outDir + "/watch_" + it->first + ".ppm");
                std::printf("uitest: watch_%s at %.1f s\n", it->first.c_str(), (t - 1000) / 1000.0);
                done.insert(it->first);
                ++captured;
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        if (!overlaysDone && sp->game().round >= 4 && sp->humanToAnswer() == false && !sp->holding() && pending.empty()) {
            captureOverlays(t + 33);
            t += 33 * 40;
        }
        if (sp->over()) break;
    }
    if (!overlaysDone) captureOverlays(1000 + 31 * 60 * 1000);
    std::remove(GameSession::savePath().c_str());
    return captured;
}

int watchScreens(const std::string& outDir) {
    std::set<std::string> done;
    size_t wantCount = 0;
    int captured = 0;
    for (uint32_t seed : {19u, 23u, 31u, 37u}) {
        captured += watchGame(outDir, seed, done, seed == 19u, wantCount);
        if (done.size() >= wantCount) break;
    }
    std::printf("uitest: %d watch frames captured\n", captured);
    return captured;
}

// The human plays Politics: the two agenda cards it looks at are shown large (wait for A).
int agendaPeekScreen(const std::string& outDir) {
    std::unique_ptr<Platform> plat = makeHeadlessPlatform("", outDir);
    Settings settings;
    settings.setInt("ai_speed", 2);
    App app(*plat, settings);
    GameSession::savePath() = outDir + "/peek_save.ti4";
    std::unique_ptr<GameSession> session(new GameSession());
    ti::Setup s;
    s.players = 4;
    s.human = 0;
    s.seed = 7;
    if (!session->startNew(s)) return 0;
    GameSession* sp = session.get();
    std::unique_ptr<Scene> scene = makeGameScene(std::move(session));
    Input in;
    Canvas c(testW(), 480, testScale());
    ti::Rng rng(3);
    int shots = 0;
    bool releaseA = false;
    uint32_t holdSince = 0;
    for (uint32_t t = 1000; t < 1000 + 40 * 60 * 1000 && shots < 2 && !sp->over(); t += 33) {
        in.beginFrame();
        in.update(t);
        if (releaseA) {
            in.handle(Btn::A, false, t);
            releaseA = false;
        } else if (sp->holding()) {
            if (!holdSince) holdSince = t;
            if (t - holdSince > 900) {
                if (sp->heldKind() == ti::EventKind::AgendaPeek) {
                    drawTop(*scene, app, c, t);
                    savePpm(c, outDir + "/watch_agenda_peek_" + std::to_string(++shots) + ".ppm");
                    std::printf("uitest: agenda peek card %d at %.1f s\n", shots, (t - 1000) / 1000.0);
                }
                in.handle(Btn::A, true, t);
                releaseA = true;
                holdSince = 0;
            }
        } else if (sp->humanToAnswer()) {
            // the human seat: take and play Politics whenever possible, otherwise as the AI would
            const ti::Decision& d = sp->engine().decision();
            ti::Answer a;
            bool chosen = false;
            for (const ti::Option& o : d.options)
                if ((d.tag == ti::Tag::StrategyPick && o.id == 2) || (d.tag == ti::Tag::ActionChoice && o.id == 12)) {
                    a.picks = {o.id};
                    chosen = true;
                }
            if (!chosen) a = ti::aiAnswer(sp->game(), d, rng);
            std::string err;
            sp->submit(a, &err, t);
        }
        scene->update(app, in, t);
    }
    std::remove(GameSession::savePath().c_str());
    return shots;
}

// The deal screens driven by button presses, as a player would: everything
// they answer must pass the rules' checks. Half the screens get random
// presses (crash / validation fuzzing), the rest a sensible sequence.
int dealDrive() {
    using namespace ti;
    int viaUi = 0, rejected = 0, made = 0;
    Rng keys(77);
    const Btn fuzzKeys[] = {Btn::Up, Btn::Down, Btn::Left, Btn::Right, Btn::A, Btn::A, Btn::Y, Btn::L1, Btn::R1, Btn::Down, Btn::Right};
    for (uint32_t seed = 1; seed <= 10; ++seed) {
        Engine e;
        Setup s;
        s.players = 3 + int(seed % 6);
        s.human = -1;
        s.seed = seed * 31 + 5;
        e.start(s);
        Rng rng(seed);
        MapView map;
        map.setViewport(Rect{0, 22, 420, 430});
        uint32_t t = 100;
        for (int steps = 0; e.waiting() && steps < 30000; ++steps) {
            const Decision& d = e.decision();
            Answer a;
            bool ui = d.player == 1 && (d.kind == DecisionKind::Deal || d.tag == Tag::Negotiate || d.tag == Tag::DealResponse);
            bool neg = false;
            if (d.player == 1 && d.tag == Tag::ActionChoice)
                for (const Option& o : d.options)
                    if (o.id == 650 && o.enabled) neg = true;
            if (neg && keys.below(3) == 0) {
                a.picks = {650};
            } else if (ui) {
                std::unique_ptr<DecisionUI> w = makeDecisionUI(e.game(), d, map);
                const bool fuzz = keys.below(2) == 0;
                bool done = false;
                for (int k = 0; k < 300 && !done; ++k) {
                    Btn b;
                    if (fuzz) b = k > 250 ? (keys.below(4) ? Btn::Start : Btn::B) : fuzzKeys[keys.below(int(sizeof fuzzKeys / sizeof fuzzKeys[0]))];
                    else if (d.kind == DecisionKind::Deal) b = k == 0 ? Btn::Y : k < 4 ? Btn::Down : k < 6 ? Btn::Right : Btn::Start;
                    else if (d.tag == Tag::DealResponse) b = k < 2 ? (keys.below(2) ? Btn::Right : Btn::Left) : Btn::A;
                    else b = k < 2 ? Btn::Right : (k > 20 ? Btn::B : Btn::A);
                    Input in;
                    in.beginFrame();
                    in.handle(b, true, t);
                    t += 50;
                    if (w->update(in, t)) {
                        Answer x = w->answer();
                        std::string err = d.validate ? d.validate(x) : "";
                        if (err.empty()) {
                            a = x;
                            done = true;
                        } else {
                            w->setError(err);
                        }
                    }
                    if (k % 40 == 0) {
                        Canvas c(640, 480, testScale());
                        w->draw(c, t);
                    }
                }
                if (done) ++viaUi;
                else a = aiAnswer(e.game(), d, rng);
            } else {
                a = aiAnswer(e.game(), d, rng);
            }
            size_t before = e.game().deals.size();
            if (!e.answer(a)) {
                std::printf("uitest: deal screens: answer rejected (seed %u, %s)\n", seed, d.prompt.c_str());
                ++rejected;
                break;
            }
            const Game& g = e.game();
            for (size_t i = before; i < g.deals.size(); ++i)
                if (g.deals[i].result == 1 && (g.deals[i].from == 1 || g.deals[i].to == 1)) ++made;
            e.takeEvents();
        }
    }
    std::printf("uitest: deal screens answered %d decisions (%d deals made), %d rejected\n", viaUi, made, rejected);
    return rejected;
}

// Round-1 Trade house rule (RULES-CHECK 0.14.0 #4): a human with Trade
// chooses whom to refill (no X-1 offers yet); a computer player refills
// everyone. Returns the number of problems.
int tradeRound1Check() {
    using namespace ti;
    int humanChose = 0, botFree = 0, problems = 0;
    for (uint32_t seed = 1; seed <= 24; ++seed) {
        Engine e;
        Setup s;
        s.players = 3 + int(seed % 6);
        s.human = 0;
        s.seed = seed * 53 + 9;
        const bool humanTrade = seed % 2 == 0;  // odd seeds: the human avoids Trade
        e.start(s);
        Rng rng(seed);
        for (int steps = 0; e.waiting() && steps < 20000 && e.game().round <= 1; ++steps) {
            const Decision& d = e.decision();
            Answer a = aiAnswer(e.game(), d, rng);
            if (d.tag == Tag::StrategyPick && d.player == 0) {
                for (const Option& o : d.options)
                    if (o.enabled && (o.id == 4) == humanTrade) {
                        a.picks = {o.id};
                        if (humanTrade) break;
                    }
            }
            if (d.tag == Tag::TradeReplenish && d.player == 0 && e.game().round == 1) ++humanChose;
            if (d.tag == Tag::TradeX1Offer && e.game().round == 1) {
                std::printf("uitest: round-1 trade: X-1 offer in round 1 (seed %u)\n", seed);
                ++problems;
            }
            if (d.tag == Tag::TradeReplenish && d.player != 0 && e.game().round == 1) {
                std::printf("uitest: round-1 trade: a computer player was asked whom to refill (seed %u)\n", seed);
                ++problems;
            }
            if (!e.answer(a)) {
                std::printf("uitest: round-1 trade: answer rejected (seed %u, %s)\n", seed, d.prompt.c_str());
                ++problems;
                break;
            }
            for (const Event& ev : e.takeEvents())
                if (ev.text.find("round 1: free for everyone") != std::string::npos) ++botFree;
        }
    }
    std::printf("uitest: round-1 trade: the human chose %d times, computer players refilled %d players for free, %d problems\n",
                humanChose, botFree, problems);
    return problems + (humanChose == 0 || botFree == 0 ? 1 : 0);
}

// The human starts an offer from the menu's faction web (MENU, Negotiations, A).
int menuOfferScreens(const std::string& outDir) {
    std::unique_ptr<Platform> plat = makeHeadlessPlatform("", outDir);
    Settings settings;
    settings.setInt("ai_speed", 3);
    App app(*plat, settings);
    GameSession::savePath() = outDir + "/menu_save.ti4";
    std::unique_ptr<GameSession> session(new GameSession());
    ti::Setup s;
    s.players = 5;
    s.human = 0;
    s.seed = 11;
    if (!session->startNew(s)) return 0;
    GameSession* sp = session.get();
    std::unique_ptr<Scene> scene = makeGameScene(std::move(session));
    Input in;
    Canvas c(testW(), 480, testScale());
    ti::Rng rng(3);
    uint32_t t = 1000;
    auto frame = [&](Btn b, bool press) {
        in.beginFrame();
        in.update(t);
        if (press) in.handle(b, true, t);
        scene->update(app, in, t);
        t += 33;
        in.beginFrame();
        in.update(t);
        if (press) in.handle(b, false, t);
        scene->update(app, in, t);
        t += 33;
    };
    auto shot = [&](const std::string& name) {
        drawTop(*scene, app, c, t);
        savePpm(c, outDir + "/menu_" + name + ".ppm");
        std::printf("uitest: menu_%s\n", name.c_str());
    };
    int shots = 0;
    const bool late = true;  // also: the web once the archetypes are revealed
    for (int guard = 0; guard < 200000 && !sp->over() && shots == 0; ++guard) {
        if (sp->holding()) {
            frame(Btn::A, true);
            continue;
        }
        if (!sp->humanToAnswer()) {
            frame(Btn::A, false);
            continue;
        }
        const ti::Decision& d = sp->engine().decision();
        bool neg = false;
        for (const ti::Option& o : d.options)
            if (d.tag == ti::Tag::ActionChoice && o.id == 650 && o.enabled) neg = true;
        if (neg && sp->game().round >= 2) {
            frame(Btn::A, false);  // let the scene build the decision screen
            frame(Btn::Menu, true);
            frame(Btn::Down, true);
            frame(Btn::A, true);
            shot("web");
            frame(Btn::A, true);  // make an offer
            for (int k = 0; k < 10; ++k) frame(Btn::A, false);
            shot("composer");
            frame(Btn::Y, true);
            shot("composer_suggested");
            frame(Btn::Start, true);
            for (int k = 0; k < 40; ++k) frame(Btn::A, false);
            shot("after_offer");
            shots = 4;
            break;
        }
        ti::Answer a = ti::aiAnswer(sp->game(), d, rng);
        std::string err;
        sp->submit(a, &err, t);
        frame(Btn::A, false);
    }
    // keep playing (the computer answers for you) until the archetypes are revealed, then look at the web
    for (int guard = 0; late && guard < 400000 && !sp->over(); ++guard) {
        if (sp->game().archRevealed && sp->humanToAnswer() && !sp->holding()) {
            frame(Btn::A, false);
            frame(Btn::Menu, true);
            frame(Btn::Down, true);
            frame(Btn::A, true);
            shot("web_revealed");
            frame(Btn::Right, true);
            shot("web_revealed2");
            frame(Btn::B, true);
            frame(Btn::B, true);
            frame(Btn::Select, true);  // the Players screen... only while the computer plays; fine if it's a decision
            break;
        }
        if (sp->holding()) {
            frame(Btn::A, true);
            continue;
        }
        if (!sp->humanToAnswer()) {
            frame(Btn::A, false);
            continue;
        }
        ti::Answer a = ti::aiAnswer(sp->game(), sp->engine().decision(), rng);
        std::string err;
        sp->submit(a, &err, t);
        frame(Btn::A, false);
    }
    std::remove(GameSession::savePath().c_str());
    return shots;
}

}  // namespace

// Saar's floating factories move like ships (bug in 0.12.0: they couldn't move at all).
// Plays games until Saar moves into a system its factory can reach, checks that the
// move is accepted, plays it and saves the move screen.
// The Wormhole Nexus warp (0.16.0): the map around the nexus at each zoom,
// and 24 frames 80 ms apart at zoom 2 (nexus_fNN) for a preview.
void nexusScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 77;
    s.draft = 0;
    e.start(s);
    Rng rng(3);
    for (int steps = 0; e.waiting() && steps < 4000 && e.game().phase == Phase::Setup; ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    const Game& g = e.game();
    if (g.nexus < 0) return;
    MapView map;
    map.setViewport(Rect{0, 22, 420, 366});
    for (int z : {0, 1, 2, 3}) {
        map.setZoom(z);
        map.setCursor(g, g.nexus);
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        map.draw(c, g, 1000);
        savePpm(c, outDir + "/art_nexus_zoom" + std::to_string(z) + ".ppm");
    }
    map.setZoom(2);
    for (int f = 0; f < 24; ++f) {
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        map.draw(c, g, uint32_t(f * 73));  // 24 x 73 ms = a quarter turn: a seamless loop
        char name[32];
        std::snprintf(name, sizeof name, "/nexus_f%02d.ppm", f);
        savePpm(c, outDir + name);
    }
}

// Uncharted Space (0.33.0): Star Charts / Azdel's Key add tiles at the edge of the board during the game.
// Two are added to a finished galaxy; the board is drawn in each tileset (the info tiles and the decor
// beside the board have to make room).
int edgeScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 77;
    s.draft = 0;
    s.modules = 3;
    e.start(s);
    Rng rng(3);
    for (int steps = 0; e.waiting() && steps < 4000 && e.game().phase == Phase::Setup; ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    const size_t before = g.systems.size();
    for (int k = 0; k < 2; ++k) {
        const std::vector<Hex> spots = boardEdgeSpots(g);
        const std::vector<int> tiles = blueTilesLeft(g);
        if (spots.empty() || tiles.empty()) return 0;
        addBoardSystem(g, tiles[size_t(k * 7) % tiles.size()], spots[size_t(k * 5) % spots.size()]);
    }
    computeAdjacency(g);
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        MapView map;
        map.setViewport(Rect{0, 22, 640, 440});
        map.setZoom(0);
        map.setCursor(g, int(before));
        for (int f = 0; f < 4; ++f) {  // (Voidfall builds its tiles over a few frames)
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            map.draw(c, g, 1000 + uint32_t(f) * 200);
            if (f == 3) savePpm(c, outDir + "/edge_tiles_" + std::to_string(ts) + ".ppm");
        }
        ++n;
    }
    retro::setTileset(retro::kSimplistic);
    std::printf("edge tiles: %zu -> %zu systems, adjacent to %zu and %zu systems\n", before, g.systems.size(), g.systems[before].adj.size(),
                g.systems[before + 1].adj.size());
    return n;
}

// Galaxy setup draft (0.15.0): the human's placement screen (tiles, spaces on
// the map, the confirmation) part-way through, and the finished galaxy.
// Returns true if the screens came up and the draft finished.
// The owner's mouse flow (0.44.1): click one tile, then another, point at one of the second tile's spaces (its
// preview must show there - it didn't), click, then the "Yes" button. The answer must be that tile on that space.
bool draftMouseFlow(const ti::Engine& e, const ti::Decision& d, MapView& map, const std::string& outDir, const std::string& tag) {
    using namespace ti;
    const Game& g = e.game();
    std::unique_ptr<DecisionUI> ui = makeDecisionUI(g, d, map);
    std::vector<int> tiles;  // the hand strip's order (as GalaxyDraftUI)
    for (int16_t t : g.draft.hands[size_t(d.player)])
        if (std::find(tiles.begin(), tiles.end(), int(t)) == tiles.end()) tiles.push_back(t);
    if (tiles.size() < 2) return true;
    Input in;
    uint32_t now = 1000;
    auto draw = [&](const std::string& name) {
        Canvas c(640, 480, testScale());
        hits::clear();
        drawSpaceBackground(c, false);
        map.draw(c, g, 250);
        ui->draw(c, 250);
        drawHintBar(c, ui->hints());
        if (!name.empty()) savePpm(c, outDir + "/" + name + ".ppm");
    };
    auto frame = [&](float x, float y, bool click) {  // as App::mouseAndPan
        in.beginFrame();
        Mouse& m = in.mouseState();
        m.active = true;
        m.moved = x != m.x || y != m.y;
        m.x = x;
        m.y = y;
        if (m.moved) hits::hover(x, y);
        Btn b = Btn::None;
        if (click) {
            m.leftPress = true;
            b = hits::press(x, y);
            if (b != Btn::None) in.handle(b, true, now);
        }
        now += 20;
        const bool done = ui->update(in, now);
        if (b != Btn::None) {
            in.beginFrame();
            in.handle(b, false, now);
        }
        draw("");
        return done;
    };
    const int n = int(tiles.size()), slotW = std::min(78, (408 - 16) / n), x0 = 6 + (408 - slotW * n) / 2;
    auto slotX = [&](int i) { return float(x0 + slotW * i + slotW / 2); };
    const int second = 1;
    draw("");
    frame(slotX(0), 340.f, true);
    frame(slotX(0) + 3, 338.f, false);  // (the hand moves on a little after a click)
    frame(slotX(second), 340.f, true);
    frame(slotX(second) + 3, 338.f, false);
    frame(slotX(second) + 6, 320.f, false);
    // a point over one of the second tile's spaces
    std::vector<int> legal;
    for (const Option& o : d.options)
        if (o.enabled && o.id / 1000 == tiles[size_t(second)]) legal.push_back(o.id % 1000);
    if (legal.empty()) return true;
    float px = -1, py = -1;
    int cell = -1;
    for (int y = 30; y < 296 && cell < 0; y += 3)
        for (int x = 4; x < 420 && cell < 0; x += 3) {
            const int c = map.hitDraftCell(g, float(x), float(y));
            if (c >= 0 && std::find(legal.begin(), legal.end(), c) != legal.end()) {
                cell = c;
                px = float(x);
                py = float(y);
            }
        }
    if (cell < 0) {
        std::printf("uitest: draft mouse: no space of the second tile on screen\n");
        return false;
    }
    frame(px, py - 1, false);
    frame(px, py, false);
    draw(tag + "_mouse_spot");
    bool preview = false;  // the tile shows on the space (the screen is in its "space" step)
    for (const auto& h : ui->hints()) preview = preview || h.second == "Place here";
    if (!preview) {
        std::printf("uitest: draft mouse: pointing at a space of the second tile shows no preview\n");
        return false;
    }
    frame(px, py, true);  // "Place the tile here?"
    float yx = -1, yy = -1;  // the "Yes" button (the only place a click presses A now)
    for (int y = 24; y < 300 && yx < 0; y += 2)
        for (int x = 4; x < 424 && yx < 0; x += 2)
            if (hits::press(float(x), float(y)) == Btn::A) {
                yx = float(x + 10);
                yy = float(y + 8);
            }
    if (yx < 0) {
        std::printf("uitest: draft mouse: no Yes button\n");
        return false;
    }
    frame(yx, yy, false);  // over "Yes"
    draw(tag + "_mouse_confirm");
    const bool done = frame(yx, yy, true);
    const Answer a = ui->answer();
    const int want = tiles[size_t(second)] * 1000 + cell;
    if (!done || a.picks.size() != 1 || a.picks[0] != want) {
        std::printf("uitest: draft mouse: expected tile %d on space %d, got %s %d\n", tiles[size_t(second)], cell, done ? "answer" : "no answer",
                    a.picks.empty() ? -1 : a.picks[0]);
        return false;
    }
    std::printf("uitest: draft mouse flow ok (second tile previewed and placed with Yes)\n");
    return true;
}

bool draftScreens(const std::string& outDir) {
    using namespace ti;
    bool shown = false, finished = false;
    for (int players : {6, 4, 8}) {
        Engine e;
        Setup s;
        s.players = players;
        s.human = 1;
        s.seed = 4242u + uint32_t(players);
        e.start(s);
        Rng rng(7);
        MapView map;
        map.setViewport(Rect{0, 22, 420, 366});
        const std::string tag = "draft" + std::to_string(players);
        bool shot = false;
        for (int steps = 0; e.waiting() && steps < 400 && e.game().phase == Phase::Setup; ++steps) {
            const Decision& d = e.decision();
            if (d.tag == Tag::GalaxyPlace && d.player == 1 && e.game().draft.placed >= players + 2 && !shot) {
                std::unique_ptr<DecisionUI> ui = makeDecisionUI(e.game(), d, map);
                auto press = [&](Btn b) {
                    Input in;
                    in.beginFrame();
                    in.handle(b, true, 100);
                    ui->update(in, 120);
                };
                auto save = [&](const std::string& name) {
                    Canvas c(640, 480, testScale());
                    drawSpaceBackground(c, false);
                    map.draw(c, e.game(), 250);
                    ui->draw(c, 250);
                    drawHintBar(c, ui->hints());
                    savePpm(c, outDir + "/" + name + ".ppm");
                };
                if (players == 6 && !draftMouseFlow(e, d, map, outDir, tag)) return false;
                press(Btn::Right);
                save(tag + "_hand");
                press(Btn::A);
                press(Btn::Right);
                save(tag + "_spot");
                press(Btn::A);
                save(tag + "_confirm");
                press(Btn::A);
                std::string err;
                if (!e.answer(ui->answer(), &err)) {
                    std::printf("uitest: draft: the screen's answer was rejected: %s\n", err.c_str());
                    return false;
                }
                shot = shown = true;
                continue;
            }
            if (!e.answer(aiAnswer(e.game(), d, rng))) break;
        }
        if (!e.game().draft.active && e.game().phase != Phase::Setup) {
            finished = true;
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            map.setZoom(0);
            map.draw(c, e.game(), 0);
            savePpm(c, outDir + "/" + tag + "_done.ppm");
        } else {
            std::printf("uitest: draft (%d players) didn't finish\n", players);
            return false;
        }
    }
    std::printf("uitest: galaxy draft screens %s\n", shown && finished ? "ok" : "missing");
    return shown && finished;
}

bool floatingFactoryCheck(const std::string& outDir) {
    using namespace ti;
    for (uint32_t seed = 1; seed < 40; ++seed) {
        Engine e;
        Setup s;
        s.players = 4;
        s.factions = {-1, findFaction("saar"), -1, -1};
        s.human = -1;
        s.seed = seed * 7919;
        e.start(s);
        Rng rng(seed);
        for (int steps = 0; e.waiting() && steps < 4000; ++steps) {
            const Decision& d = e.decision();
            const Game& g = e.game();
            if (d.player == 1 && d.kind == DecisionKind::Move) {
                for (size_t from = 0; from < g.systems.size(); ++from) {
                    if (int(from) == d.system || g.systems[from].hasToken(1)) continue;
                    if (!g.systems[from].space[1].count(UnitType::SpaceDock)) continue;
                    if (!shipReach(g, 1, int(from), d.system, UnitType::SpaceDock, 0).ok) continue;
                    std::vector<int> plan = {int(from), -1, int(UnitType::SpaceDock), 1};
                    std::string err = checkMovePlan(g, 1, d.system, plan);
                    if (!err.empty()) {
                        std::printf("uitest: FLOATING FACTORY move refused: %s\n", err.c_str());
                        return false;
                    }
                    Canvas c(640, 480, testScale());
                    MapView map;
                    map.setViewport(Rect{0, 22, 420, 430});
                    map.setCursor(g, d.system);
                    drawSpaceBackground(c, false);
                    std::unique_ptr<DecisionUI> ui = makeDecisionUI(g, d, map);
                    map.setActiveSystem(g.activeSystem);
                    map.draw(c, g, 0);
                    ui->draw(c, 0);
                    drawHintBar(c, ui->hints());
                    savePpm(c, outDir + "/ui_move_floating.ppm");
                    const int before = g.systems[d.system].space[1].count(UnitType::SpaceDock);
                    Answer a;
                    a.data = plan;
                    if (!e.answer(a, &err)) {
                        std::printf("uitest: FLOATING FACTORY answer rejected: %s\n", err.c_str());
                        return false;
                    }
                    const int after = e.game().systems[d.system].space[1].count(UnitType::SpaceDock);
                    std::printf("uitest: floating factory moved from system %d (seed %u): %d -> %d in the active system\n",
                                int(from), seed, before, after);
                    return after == before + 1 || after == 0;  // 0: destroyed by a blockade right away
                }
            }
            Answer a = aiAnswer(g, d, rng);
            if (!e.answer(a)) break;
            e.takeEvents();
        }
    }
    std::printf("uitest: no chance to move a floating factory came up\n");
    return false;
}

// 0.44.2 (tester: "with Fighter II the game always assumes they fly on their own - I cannot carry them"): a carrier II
// with fighter IIs, 3 systems away, Gravity Drive - the fighters ride in the carrier, the carrier takes the +1
std::string g_uitestDir = ".";
bool fighterCarryCheck() {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 4;
    s.human = -1;
    s.seed = 4421u;
    e.start(s);
    Rng rng(3);
    for (int steps = 0; e.waiting() && steps < 20000; ++steps) {
        const Decision& d = e.decision();
        if (d.kind == DecisionKind::Move && d.system >= 0) break;
        if (!e.answer(aiAnswer(e.game(), d, rng))) return false;
    }
    if (!e.waiting() || e.decision().kind != DecisionKind::Move) return false;
    Game g = e.game();  // a copy to set the scene in
    const int p = e.decision().player, act = e.decision().system;
    auto unitIdx = [](const char* key) {
        for (int i = 0; i < kUnitCount; ++i)
            if (!std::strcmp(kUnits[i].key, key)) return int16_t(i);
        return int16_t(-1);
    };
    Player& pl = g.players[size_t(p)];
    pl.unit[int(UnitType::Carrier)] = unitIdx("carrier2");
    pl.unit[int(UnitType::Fighter)] = unitIdx("fighter2");
    if (!pl.hasTech(findTech("gd"))) pl.techs.push_back(int16_t(findTech("gd")));
    g.fixes = 7;
    int from = -1;
    for (size_t x = 0; x < g.systems.size() && from < 0; ++x) {
        if (int(x) == act || g.systems[x].hasToken(p) || g.systems[x].isHyperlane()) continue;
        bool other = false;
        for (int q = 0; q < g.numPlayers; ++q)
            if (q != p && !g.systems[x].space[size_t(q)].empty()) other = true;
        if (other) continue;
        if (shipReach(g, p, int(x), act, UnitType::Carrier, 1).ok && !shipReach(g, p, int(x), act, UnitType::Carrier, 0).ok) from = int(x);
    }
    if (from < 0) {
        std::printf("uitest: fighter carry: no system 3 away\n");
        return false;
    }
    g.systems[size_t(from)].space[size_t(p)] = Force();
    g.systems[size_t(from)].space[size_t(p)].add(UnitType::Carrier, 1);
    g.systems[size_t(from)].space[size_t(p)].add(UnitType::Fighter, 2);
    const int C = int(UnitType::Carrier), F = int(UnitType::Fighter);
    const std::string carried = checkMovePlan(g, p, act, {from, -1, C, 1, from, -1, F, 2});
    const std::string alone1 = checkMovePlan(g, p, act, {from, -1, F, 1});  // one fighter flies with the +1
    const std::string alone2 = checkMovePlan(g, p, act, {from, -1, F, 2});  // two can't
    {  // the move screen with 2 dreadnoughts (move 1) there too: listed under "Can't reach", with why
        Game shown = g;
        shown.systems[size_t(from)].space[size_t(p)].add(UnitType::Dreadnought, 2);
        Decision d = e.decision();
        Canvas cv(640, 480, testScale());
        MapView map;
        map.setViewport(Rect{0, 22, 420, 430});
        map.setCursor(shown, act);
        drawSpaceBackground(cv, false);
        std::unique_ptr<DecisionUI> ui = makeDecisionUI(shown, d, map);
        map.draw(cv, shown, 0);
        ui->draw(cv, 0);
        drawHintBar(cv, ui->hints());
        savePpm(cv, g_uitestDir + "/ui_move_unreachable.ppm");
    }
    g.fixes = 6;
    const std::string old = checkMovePlan(g, p, act, {from, -1, C, 1, from, -1, F, 2});
    std::printf("uitest: fighter carry: carried \"%s\", one alone \"%s\", two alone \"%s\" (before 0.44.2: \"%s\")\n", carried.c_str(),
                alone1.c_str(), alone2.c_str(), old.c_str());
    return carried.empty() && alone1.empty() && !alone2.empty();
}

// 0.44.2 (tester: Exotrireme IIs "don't show up" for a system 3 away): every ship that a plain breadth-first count says
// can reach the active system (only through empty, anomaly-free systems with nothing of anyone else in them) must be
// reachable for shipReach too. TI4_REACHGAMES games, Sardakk in every one with Exotrireme II.
int reachCheck() {
    using namespace ti;
    const int games = std::getenv("TI4_REACHGAMES") ? std::atoi(std::getenv("TI4_REACHGAMES")) : 12;
    int checked = 0, bad = 0;
    for (int gi = 0; gi < games; ++gi) {
        Engine e;
        Setup s;
        s.players = 3 + gi % 6;
        s.human = -1;
        s.seed = 9100u + uint32_t(gi);
        s.factions.assign(size_t(s.players), -1);
        s.factions[0] = findFaction("sardakk");
        s.modules = gi % 2 ? (kModCodex | kModThundersEdge) : kModCodex;
        e.start(s);
        Rng rng(uint32_t(gi) + 1);
        bool given = false;
        for (int steps = 0; e.waiting() && steps < 60000; ++steps) {
            const Decision& d = e.decision();
            const Game& g = e.game();
            if (g.round > 6) break;
            if (d.kind == DecisionKind::Move && d.system >= 0) {
                Game c = g;
                if (!given)  // (the copy only: Exotrireme II for the Sardakk)
                    for (int i = 0; i < kUnitCount; ++i)
                        if (!std::strcmp(kUnits[i].key, "sardakk_dreadnought2")) c.players[0].unit[int(UnitType::Dreadnought)] = int16_t(i);
                const int p = d.player, act = d.system;
                const int n = int(c.systems.size());
                auto clear = [&](int u) {
                    if (anomaliesFor(c, p, u) || c.systems[size_t(u)].isHyperlane() || neutralUnitsIn(c, u)) return false;
                    for (int q = 0; q < c.numPlayers; ++q)
                        if (q != p && hasUnits(c, q, u)) return false;
                    return true;
                };
                for (int from = 0; from < n; ++from) {
                    if (from == act || c.systems[size_t(from)].hasToken(p) || !clear(from)) continue;
                    for (int t : kShipIdx) {
                        if (!c.systems[size_t(from)].space[size_t(p)].n[t] || t == int(UnitType::Fighter)) continue;
                        const int mv = moveValue(c, p, from, UnitType(t));
                        std::vector<int> dist(size_t(n), -1);
                        std::deque<int> q{from};
                        dist[size_t(from)] = 0;
                        while (!q.empty()) {
                            const int u = q.front();
                            q.pop_front();
                            if (u != from && (u == act || !clear(u))) continue;
                            for (int16_t v : c.systems[size_t(u)].adj)
                                if (dist[size_t(v)] < 0) {
                                    dist[size_t(v)] = dist[size_t(u)] + 1;
                                    q.push_back(v);
                                }
                        }
                        if (dist[size_t(act)] < 0 || dist[size_t(act)] > mv || !shipsMayEnter(c, p, act)) continue;
                        ++checked;
                        if (!shipReach(c, p, from, act, UnitType(t), 0).ok) {
                            ++bad;
                            if (bad <= 10)
                                std::printf("uitest: REACH game %d round %d: %s %s from system %d to %d - %d away, move %d, refused\n", gi, int(c.round),
                                            kFactions[size_t(c.players[size_t(p)].faction)].key, unitTypeName(UnitType(t)), from, act,
                                            dist[size_t(act)], mv);
                        }
                    }
                }
            }
            if (!e.answer(aiAnswer(g, d, rng))) break;
        }
    }
    std::printf("uitest: reach check: %d ship groups checked, %d refused\n", checked, bad);
    return bad;
}

// The card designs (owner's templates) at the sizes the screens use.
void artScreens(const std::string& outDir) {
    using namespace ti;
    auto findObj = [](const char* key) {
        for (int o = 0; o < kObjectiveCount; ++o)
            if (!std::strcmp(kObjectives[o].key, key)) return o;
        return 0;
    };
    {
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        int s1 = -1, s2 = -1, sa = -1, ss = -1;
        for (int o = 0; o < kObjectiveCount; ++o) {
            const ObjectiveDef& d = kObjectives[o];
            if (d.kind == 1 && s1 < 0) s1 = o;
            if (d.kind == 2 && s2 < 0) s2 = o;
            if (d.kind == 3 && d.phase == ObjPhase::Action && sa < 0) sa = o;
            if (d.kind == 3 && d.phase == ObjPhase::Status && ss < 0) ss = o;
        }
        (void)findObj;
        cards::objective(c, s1, Rect{8, 8, 200, 300});
        cards::objective(c, s2, Rect{216, 8, 120, 180});
        cards::objective(c, sa, Rect{344, 8, 120, 180});
        cards::objective(c, ss, Rect{472, 8, 64, 96});
        cards::objectiveBack(c, 1, Rect{544, 8, 64, 96});
        cards::objectiveBack(c, 2, Rect{472, 112, 64, 96});
        cards::objectiveBack(c, 3, Rect{544, 112, 64, 96});
        for (int k = 0; k < 4; ++k) cards::exploreBack(c, k, Rect{216 + k * 72, 200, 64, 96});
        cards::actionBack(c, Rect{510, 220, 120, 190});  // 0.33.1: the owner's action card back
        cards::controlToken(c, 0, rgb(220, 60, 60), 240, 340, 48);
        cards::controlToken(c, 5, rgb(60, 130, 230), 300, 340, 28);
        cards::controlToken(c, 10, rgb(60, 200, 90), 340, 340, 16);
        cards::strategy(c, 4, 216, 370, 200);
        cards::strategy(c, 7, 420, 330, 210);
        savePpm(c, outDir + "/art_cards1.ppm");
    }
    {
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        int blue2 = -1, mixed = -1, faction = -1;
        for (int t = 0; t < kTechCount; ++t) {
            const TechDef& d = kTechs[t];
            if (blue2 < 0 && d.color == TechColor::Blue && d.req[0] == 2 && d.faction < 0) blue2 = t;
            if (mixed < 0 && d.unitUpgrade && d.req[0] && d.req[3]) mixed = t;
            if (faction < 0 && d.faction >= 0 && d.color == TechColor::Green) faction = t;
        }
        cards::tech(c, blue2, Rect{8, 8, 420, 273});
        cards::tech(c, mixed, Rect{436, 8, 196, 128});
        cards::tech(c, faction, Rect{436, 144, 196, 128});
        cards::strategy(c, 0, 8, 290, 420);
        savePpm(c, outDir + "/art_cards2.ppm");
    }
    {  // wireframes on technology cards and tiles (0.15.0)
        auto techByKey = [](const char* key) {
            for (int t = 0; t < kTechCount; ++t)
                if (!std::strcmp(kTechs[t].key, key)) return t;
            return 0;
        };
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        const char* keys[9] = {"ws", "dn2", "cv2", "inf2", "amd", "nm", "gls", "sar", "wg"};
        for (int i = 0; i < 9; ++i) cards::tech(c, techByKey(keys[i]), Rect{6 + (i % 3) * 211, 6 + (i / 3) * 158, 205, 133});
        savePpm(c, outDir + "/art_wire_cards.ppm");
        Canvas b(640, 480, testScale());
        drawSpaceBackground(b, false);
        cards::tech(b, techByKey("ff2"), Rect{6, 6, 420, 273});
        const char* tiles[8] = {"sd2", "pds2", "cr2", "dd2", "det", "x89_base", "td", "mr"};
        for (int i = 0; i < 8; ++i) cards::techTile(b, techByKey(tiles[i]), Rect{436, 6 + i * 58, 198, 52});
        for (int u = 0; u < kUnitTypes; ++u)
            cards::unitWireframe(b, UnitType(u), Rect{6 + (u % 6) * 71, 290 + (u / 6) * 95, 66, 88}, rgb(170, 200, 235));
        savePpm(b, outDir + "/art_wire_tiles.ppm");
    }
    {
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        static const char* keys[] = {"arborec", "letnev", "mahact", "naaz", "sol", "cabal", "jolnar", "l1z1x"};
        static const uint32_t cols[] = {rgb(60, 200, 90), rgb(220, 60, 60), rgb(230, 200, 60), rgb(60, 200, 90),
                                        rgb(60, 130, 230), rgb(220, 60, 60), rgb(150, 80, 220), rgb(230, 120, 40)};
        for (int i = 0; i < 8; ++i) cards::factionLabel(c, findFaction(keys[i]), cols[i], 10 + (i % 2) * 315, 10 + (i / 2) * 58, 300, 50);
        cards::factionLabel(c, findFaction("yssaril"), rgb(60, 200, 90), 10, 250, 220, 36);
        cards::factionLabel(c, findFaction("ghost"), rgb(60, 130, 230), 240, 250, 160, 26);
        savePpm(c, outDir + "/art_labels.ppm");
    }
    {  // command tokens at the sizes the game uses (banner, faction sheet, map, token prompt)
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        auto findFaction = [](const char* key) {
            for (int f = 0; f < kFactionCount; ++f)
                if (!std::strcmp(kFactions[f].key, key)) return f;
            return 0;
        };
        const char* keys[8] = {"sol", "hacan", "jolnar", "letnev", "naalu", "nekro", "muaat", "creuss"};
        const int sizes[7] = {62, 40, 23, 21, 17, 15, 9};
        for (int i = 0; i < 8; ++i) {
            const int f = findFaction(keys[i]);
            const uint32_t col = theme::player[i];
            int x = 40;
            for (int k = 0; k < 7; ++k) {
                cards::commandToken(c, f, col, x, 34 + i * 56, sizes[k]);
                x += sizes[k] + 14;
            }
            for (int k = 0; k < 3; ++k) {  // ship side (fleet pool)
                cards::commandToken(c, f, col, x + 10, 34 + i * 56, sizes[k + 1], 255, true);
                x += sizes[k + 1] + 14;
            }
        }
        savePpm(c, outDir + "/art_tokens.ppm");
    }
    {  // control tokens on the map at the closest zooms (round 3 of a 6-player game)
        Engine e;
        Setup s;
        s.players = 6;
        s.human = -1;
        s.seed = 4242;
        e.start(s);
        Rng rng(3);
        auto boardTokens = [](const Game& gg) {
            int n = 0;
            for (const SystemState& ss : gg.systems)
                for (int q = 0; q < gg.numPlayers; ++q) n += ss.hasToken(q) ? 1 : 0;
            return n;
        };
        // round 3, mid action phase (command tokens on the board, territory grown)
        for (int steps = 0; e.waiting() && steps < 20000 && e.game().round < 4; ++steps) {
            const Game& gg = e.game();
            if (gg.round == 3 && gg.phase == Phase::Action && boardTokens(gg) >= 9) break;
            if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
            e.takeEvents();
        }
        const Game& g = e.game();
        int sys = g.players[0].homeSystem;
        for (int pl = 0; pl < int(g.planets.size()); ++pl)  // a non-home planet held without units
            if (g.planets[pl].owner >= 0 && g.planets[pl].units[g.planets[pl].owner].empty() && g.systems[g.planets[pl].system].home < 0) {
                sys = g.planets[pl].system;
                break;
            }
        for (int z : {0, 1, 2, 3}) {
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            MapView map;
            map.setViewport(Rect{0, 22, 640, 430});
            map.setZoom(z);
            map.setCursor(g, sys);
            map.draw(c, g, 0);
            savePpm(c, outDir + "/art_map_zoom" + std::to_string(z) + ".ppm");
        }
        if (std::getenv("TI4_MAPBENCH")) {  // test only: map drawing time with / without borders
            for (int z : {1, 2, 3})
                for (bool b : {false, true}) {
                    Canvas c(640, 480, testScale());
                    MapView map;
                    map.setViewport(Rect{0, 22, 420, 430});
                    map.setZoom(z);
                    map.setCursor(g, sys);
                    map.setBorders(b);
                    map.draw(c, g, 0);  // warm the token cache
                    double best = 1e9;
                    for (int rep = 0; rep < 12; ++rep) {
                        std::clock_t t0 = std::clock();
                        for (int k = 0; k < 100; ++k) map.draw(c, g, uint32_t(k));
                        best = std::min(best, double(std::clock() - t0) * 1000.0 / CLOCKS_PER_SEC / 100.0);
                    }
                    std::printf("mapbench zoom %d borders %d: %.3f ms per draw (best of 12)\n", z, int(b), best);
                }
        }
        {  // the same map without borders
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            MapView map;
            map.setViewport(Rect{0, 22, 640, 430});
            map.setZoom(1);
            map.setCursor(g, sys);
            map.setBorders(false);
            map.draw(c, g, 0);
            savePpm(c, outDir + "/art_map_noborders.ppm");
        }
    }
}

// The expanded tile (0.20.0): a few systems of a game in round 4 drawn large,
// and a space combat on a tile. Returns how many were drawn.
int tileScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4242;
    s.draft = 0;
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 4 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    const Game& g = e.game();
    std::vector<std::pair<std::string, int>> picks;
    picks.push_back({"mecatol", g.mecatol()});
    if (g.players[0].homeSystem >= 0) picks.push_back({"home", g.players[0].homeSystem});
    int busiest = -1, most = 0;
    for (size_t i = 0; i < g.systems.size(); ++i) {
        int n = 0;
        for (int q = 0; q < g.numPlayers; ++q) n += g.systems[i].space[size_t(q)].ships();
        for (int16_t x : g.systems[i].planets)
            for (int q = 0; q < g.numPlayers; ++q) n += g.planets[size_t(x)].units[size_t(q)].groundForces();
        if (n > most && g.systems[i].home < 0) {
            most = n;
            busiest = int(i);
        }
    }
    if (busiest >= 0) picks.push_back({"busiest", busiest});
    for (size_t i = 0; i < g.systems.size(); ++i) {
        const SystemDef& d = g.systems[i].d();
        if ((d.anomalies & kNebula) && picks.size() < 9) picks.push_back({"nebula", int(i)});
        if ((d.anomalies & kAsteroid) && picks.size() < 9) picks.push_back({"asteroids", int(i)});
        if ((d.anomalies & kRift) && picks.size() < 9) picks.push_back({"rift", int(i)});
        if ((d.anomalies & kSupernova) && picks.size() < 9) picks.push_back({"supernova", int(i)});
        if (d.wormholes && d.planetCount == 0 && picks.size() < 9) picks.push_back({"wormhole", int(i)});
        if (d.planetCount == 3 && picks.size() < 10) picks.push_back({"three", int(i)});
    }
    if (std::getenv("TI4_TILEBENCH")) {  // test only: ms per expanded tile draw
        Canvas c(640, 480, testScale());
        for (const auto& pk : picks) {
            if (pk.second < 0) continue;
            const uint32_t t0 = monotonicMs();
            for (int k = 0; k < 50; ++k) tileview::draw(c, g, pk.second, Rect{20, 68, 600, 380}, uint32_t(k * 16));
            std::printf("tilebench %-10s %.2f ms\n", pk.first.c_str(), double(monotonicMs() - t0) / 50.0);
        }
    }
    std::set<std::string> seen;
    int drawn = 0;
    for (const auto& pk : picks) {
        if (pk.second < 0 || seen.count(pk.first)) continue;
        seen.insert(pk.first);
        Canvas c(640, 480, testScale());
        drawSpaceBackground(c, false);
        tileview::draw(c, g, pk.second, Rect{40, 30, 560, 420}, 1234);
        savePpm(c, outDir + "/tile_" + pk.first + ".ppm");
        ++drawn;
    }
    // a space combat on the busiest tile
    if (busiest >= 0) {
        tileview::CombatShow cs;
        cs.attacker = 0;
        cs.defender = 1;
        cs.a.add(UnitType::Dreadnought, 2);
        cs.a.add(UnitType::Carrier, 1);
        cs.a.add(UnitType::Cruiser, 2);
        cs.a.add(UnitType::Fighter, 3);
        cs.a.dmg[int(UnitType::Dreadnought)] = 1;
        cs.b.add(UnitType::WarSun, 1);
        cs.b.add(UnitType::Destroyer, 3);
        cs.b.add(UnitType::Fighter, 5);
        cs.rolls.push_back({0, {3, 9, 7, 10, 2, 8}, {5, 5, 7, 7, 9, 9}, 3, 1000});
        cs.rolls.push_back({1, {4, 6, 9, 1}, {3, 3, 9, 9}, 2, 1100});
        cs.losses.push_back({1, int(UnitType::Destroyer), 1, 1200});
        for (int f : {0, 1}) {
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            tileview::draw(c, g, busiest, Rect{40, 30, 560, 420}, f == 0 ? 1300 : 1700, &cs);
            savePpm(c, outDir + "/tile_combat" + std::to_string(f) + ".ppm");
            ++drawn;
        }
    }
    return drawn;
}

// The animated space combat (0.22.0): a made-up fight over several backdrops,
// frames saved along its timeline (TI4_UITEST_ONLY=battle; TI4_BATTLEBENCH=1:
// ms per frame).
int battleScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4242;
    s.draft = 0;
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 3 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    std::vector<std::pair<std::string, int>> picks;
    picks.push_back({"mecatol", g.mecatol()});
    for (size_t i = 0; i < g.systems.size(); ++i) {
        const SystemDef& d = g.systems[i].d();
        int alive = 0;
        for (int16_t x : g.systems[i].planets) alive += g.planets[size_t(x)].destroyed ? 0 : 1;
        auto add = [&](const char* k) {
            for (const auto& p : picks)
                if (p.first == k) return;
            picks.push_back({k, int(i)});
        };
        if (d.anomalies & kRift) add("rift");
        else if (d.anomalies & kSupernova) add("supernova");
        else if (d.anomalies & kNebula) add("nebula");
        else if (alive == 1 && g.systems[i].home < 0 && int(i) != g.mecatol()) add("planet");
        else if (alive >= 2 && g.systems[i].home < 0) add("planets");
        else if (alive == 0 && d.wormholes) add("wormhole");
        else if (alive == 0 && !d.anomalies) add("empty");
    }
    if (const char* only = std::getenv("TI4_BATTLE_ONLY")) {
        std::vector<std::pair<std::string, int>> keep;
        for (const auto& p : picks)
            if (p.first == only) keep.push_back(p);
        picks = keep;
    }
    Force a, b;
    a.add(UnitType::Flagship, 1);
    a.add(UnitType::Dreadnought, 2);
    a.add(UnitType::Carrier, 1);
    a.add(UnitType::Cruiser, 2);
    a.add(UnitType::Destroyer, 2);
    a.add(UnitType::Fighter, 6);
    a.add(UnitType::WarSun, 1);
    b.add(UnitType::Dreadnought, 2);
    b.add(UnitType::Carrier, 2);
    b.add(UnitType::Cruiser, 1);
    b.add(UnitType::Destroyer, 2);
    b.add(UnitType::Fighter, 12);
    b.add(UnitType::WarSun, 1);
    b.dmg[int(UnitType::Dreadnought)] = 1;
    if (std::getenv("TI4_BATTLE_GAUSS"))  // 0.43.0: the attacker brings two Mirveda Gauss Cannons
        for (int u = 0; u < kUnitCount; ++u)
            if (!std::strcmp(kUnits[u].key, "mirveda_pds")) {
                g.players[0].unit[int(UnitType::PDS)] = int16_t(u);
                a.add(UnitType::PDS, 2);
            }
    const Rect area{0, 22, 420, 366};
    int drawn = 0;
    for (const auto& pk : picks) {
        if (pk.second < 0) continue;
        // a PDS and a dock on its planets, for the show
        for (int16_t x : g.systems[size_t(pk.second)].planets) {
            g.planets[size_t(x)].units[1].add(UnitType::PDS, 1);
            if (pk.first == "mecatol" || pk.first == "planets") g.planets[size_t(x)].units[1].add(UnitType::SpaceDock, 1);
        }
        battle::SpaceBattle bt;
        const uint32_t t0 = 100000;
        bt.start(g, pk.second, 0, 1, a, b, t0, 1.f);
        std::vector<int8_t> d1 = {3, 8, 9, 2, 10, 6, 7, 1, 5}, n1 = {9, 9, 5, 5, 3, 3, 7, 7, 9};
        std::vector<int8_t> d2 = {4, 7, 2, 9, 9, 1, 6, 8}, n2 = {5, 5, 9, 9, 6, 6, 9, 9};
        const uint32_t frames[] = {300, 900, 2200, 2700, 3400, 4200, 5000, 6200, 7000, 7600, 8400, 9500, 11000, 12500};
        size_t fi = 0;
        Canvas c(640, 480, testScale());
        double ms = 0;
        int nms = 0;
        for (uint32_t t = 0; t <= 13000; t += 20) {
            const uint32_t now = t0 + t;
            if (t == 2000) bt.roll(0, d1, n1, 6, now, 1600);
            if (t == 3600) bt.roll(1, d2, n2, 3, now, 1600);
            if (t == 5600) {
                bt.damaged(1, UnitType::Dreadnought, 1, now);
                bt.destroyed(1, UnitType::Fighter, 3, now);
                bt.destroyed(1, UnitType::Destroyer, 1, now);
            }
            if (t == 6200) {
                bt.destroyed(1, UnitType::Carrier, 1, now);
                bt.destroyed(1, UnitType::Cruiser, 1, now);
                bt.damaged(0, UnitType::WarSun, 1, now);
                bt.damaged(0, UnitType::Flagship, 1, now);
            }
            if (t == 6800) {
                bt.destroyed(1, UnitType::Dreadnought, 1, now);
                bt.destroyed(0, UnitType::Destroyer, 1, now);
            }
            if (t == 8200) {
                bt.destroyed(1, UnitType::WarSun, 1, now);
                bt.destroyed(0, UnitType::Flagship, 1, now);
            }
            if (t == 12000) bt.end(0, "Attacker wins", now);
            const char* every = std::getenv("TI4_BATTLE_EVERY");  // test only: a frame every N ms (for a preview animation)
            const bool shot = every ? t % uint32_t(std::max(20, std::atoi(every))) == 0
                                    : fi < sizeof frames / sizeof frames[0] && t >= frames[fi];
            const bool bench = std::getenv("TI4_BATTLEBENCH") != nullptr;
            if (!shot && !bench && t % 40 != 0) continue;
            drawSpaceBackground(c, false);
            const uint32_t m0 = monotonicMs();
            bt.draw(c, g, area, now, t >= 12000 ? std::string("A: Continue") : std::string());
            ms += double(monotonicMs() - m0);
            ++nms;
            if (shot) {
                char name[96];
                std::snprintf(name, sizeof name, "/battle_%s_%05u.ppm", pk.first.c_str(), unsigned(t));
                savePpm(c, outDir + name);
                ++fi;
                ++drawn;
            }
        }
        if (std::getenv("TI4_BATTLEBENCH")) std::printf("battlebench %-10s %.2f ms per frame\n", pk.first.c_str(), ms / std::max(1, nms));
    }
    return drawn;
}

// The ground combat (0.23.0): a made-up fight on planets of each kind
// (TI4_UITEST_ONLY=ground; TI4_BATTLE_ONLY / TI4_BATTLE_EVERY / TI4_BATTLEBENCH as above).
int groundScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4242;
    s.draft = 0;
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 3 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    std::vector<std::pair<std::string, int>> picks;  // (name, planet)
    for (size_t x = 0; x < g.planets.size(); ++x) {
        const PlanetDef& pd = kPlanets[size_t(g.planets[x].def)];
        const bool home = g.systems[size_t(g.planets[x].system)].home >= 0;
        auto add = [&](const std::string& k) {
            for (const auto& p : picks)
                if (p.first == k) return;
            picks.push_back({k, int(x)});
        };
        if (!std::strcmp(pd.key, "mr")) add("mecatol");
        else if (home) add("home");
        else if (pd.trait == Trait::Cultural) add("cultural");
        else if (pd.trait == Trait::Industrial) add("industrial");
        else if (pd.trait == Trait::Hazardous) {
            add("hazardous1");
            if (int(x) % 2) add("hazardous2");
            else add("hazardous3");
        }
    }
    if (const char* only = std::getenv("TI4_BATTLE_ONLY")) {
        std::vector<std::pair<std::string, int>> keep;
        for (const auto& p : picks)
            if (p.first == only) keep.push_back(p);
        picks = keep;
    }
    Force a, b;
    a.add(UnitType::Infantry, 5);
    a.add(UnitType::Mech, 2);
    b.add(UnitType::Infantry, 4);
    b.add(UnitType::Mech, 1);
    b.add(UnitType::PDS, 2);
    b.add(UnitType::SpaceDock, 1);
    if (std::getenv("TI4_BATTLE_BIG")) {  // test only: bigger armies for the preview clips
        a.add(UnitType::Infantry, 4);
        a.add(UnitType::Mech, 1);
        b.add(UnitType::Infantry, 5);
        b.add(UnitType::Mech, 1);
    }
    const Rect area{0, 22, 420, 366};
    int drawn = 0, k = 0;
    for (const auto& pk : picks) {
        battle::GroundBattle bt;
        const uint32_t t0 = 200000 + uint32_t(k++) * 7777;  // (the hazardous kind depends on it)
        bt.start(g, g.planets[size_t(pk.second)].system, pk.second, 0, 1, a, b, t0, 1.f);
        std::vector<int8_t> d1 = {3, 8, 9, 2, 10, 6, 7}, n1 = {8, 8, 8, 8, 8, 6, 6};
        std::vector<int8_t> d2 = {4, 9, 2, 9, 1}, n2 = {8, 8, 8, 8, 6};
        const uint32_t frames[] = {300, 1000, 2200, 3000, 4400, 5200, 6400, 7400, 8400, 9800, 11500};
        size_t fi = 0;
        Canvas c(640, 480, testScale());
        double ms = 0;
        int nms = 0;
        for (uint32_t t = 0; t <= 12000; t += 20) {
            const uint32_t now = t0 + t;
            if (t == 2000) bt.roll(0, d1, n1, 3, now, 1600);
            if (t == 3600) bt.roll(1, d2, n2, 2, now, 1600);
            if (t == 5600) {
                bt.destroyed(1, UnitType::Infantry, 2, now);
                bt.damaged(1, UnitType::Mech, 1, now);
                bt.destroyed(0, UnitType::Infantry, 1, now);
            }
            if (t == 7000) {
                bt.destroyed(1, UnitType::Mech, 1, now);
                bt.destroyed(0, UnitType::Infantry, 1, now);
            }
            if (t == 8200) bt.destroyed(1, UnitType::Infantry, 2, now);
            if (t == 11000) bt.end(0, "Attacker wins", now);
            const char* every = std::getenv("TI4_BATTLE_EVERY");
            const bool shot = every ? t % uint32_t(std::max(20, std::atoi(every))) == 0
                                    : fi < sizeof frames / sizeof frames[0] && t >= frames[fi];
            const bool bench = std::getenv("TI4_BATTLEBENCH") != nullptr;
            if (!shot && !bench && t % 40 != 0) continue;
            drawSpaceBackground(c, false);
            const uint32_t m0 = monotonicMs();
            bt.draw(c, g, area, now, t >= 11000 ? std::string("A: Continue") : std::string());
            ms += double(monotonicMs() - m0);
            ++nms;
            if (shot) {
                char name[96];
                std::snprintf(name, sizeof name, "/ground_%s_%05u.ppm", pk.first.c_str(), unsigned(t));
                savePpm(c, outDir + name);
                ++fi;
                ++drawn;
            }
        }
        if (std::getenv("TI4_BATTLEBENCH")) std::printf("groundbench %-11s %.2f ms per frame\n", pk.first.c_str(), ms / std::max(1, nms));
    }
    return drawn;
}

// 0.52.0 (owner D117): the Ghemina's Lady and Lord as two units - the expanded tile in each tileset, a space battle (the
// 0.54.0 (TI4_UITEST_ONLY=habits): the habit profile - a game played to the end by the computer with seat 0 marked as a
// person, recorded twice (the second replaces the first), then a second game; prints the profile each time.
int habitChecks(const std::string& outDir) {
    using namespace ti;
    Settings st;
    st.load(outDir + "/habits_settings.cfg");
    int ok = 0;
    for (int gameNo = 0; gameNo < 2; ++gameNo) {
        Engine e;
        Setup s;
        s.players = 4;
        s.human = 0;
        s.seed = 900 + uint32_t(gameNo);
        s.draft = 0;
        s.modules = kModCodex;
        s.habits = habits::load(st);
        e.start(s);
        Rng rng(7 + uint32_t(gameNo));
        for (int steps = 0; e.waiting() && steps < 200000; ++steps)
            if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
        const Game& g = e.game();
        habits::record(st, g, e.setup());
        if (gameNo == 0) habits::record(st, g, e.setup());  // (counted again: replaces)
        const HabitProfile h = habits::load(st);
        std::printf("habits after game %d (round %d, raids %d, Mecatol turns %d): games %d attacks %.2f raids %.2f mecatol %.2f broken %.2f kept %.2f deals %.2f\n",
                    gameNo + 1, g.round, int(g.players[0].raids), int(g.players[0].mecHeld), h.games, double(h.attacks), double(h.raids),
                    double(h.mecatol), double(h.broken), double(h.kept), double(h.deals));
        if (h.games == gameNo + 1) ++ok;
    }
    return ok == 2;
}

// 0.54.0 (TI4_UITEST_ONLY=tester): the tester's report - a Vuil'raith carrier picks up infantry on a detour through
// their home (a Dimensional Tear: a gravity rift they don't roll for). Prints the move check with fixes 17 and 18.
// Waypoints (0.56.0, owner's M2 follow-up): a carrier next to a gravity rift picks up an infantry in the rift on its way
// to the system beyond - refused without a route (the rift rolls), allowed with the route through it; then the move
// editor's Waypoint Mode on the map (TI4_UITEST_ONLY=waypoints), and the crosshair button with its tooltip.
int waypointScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 2468;
    s.draft = 0;
    s.modules = kModCodex;
    e.start(s);
    Rng rng(4);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 2 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    const Game& g0 = e.game();
    int shots = 0, checked = 0;
    for (size_t R = 0; R < g0.systems.size() && !checked; ++R) {
        const SystemState& rs = g0.systems[R];
        if (!(rs.d().anomalies & kRift) || rs.home >= 0 || rs.offBoard) continue;
        for (int16_t A : rs.adj)
            for (int16_t T : rs.adj) {
                if (checked || A == T || adjacentSystems(g0, A, T)) continue;
                const SystemState& sa = g0.systems[size_t(A)];
                const SystemState& st = g0.systems[size_t(T)];
                if (sa.home >= 0 || st.home >= 0 || sa.isHyperlane() || st.isHyperlane() || sa.d().anomalies || st.d().anomalies) continue;
                int p = -1;  // a player whose ships roll in rifts
                for (int q = 0; q < g0.numPlayers && p < 0; ++q)
                    if (riftRollsLeaving(g0, q, int(R))) p = q;
                if (p < 0) continue;
                Game t = g0;
                for (SystemState& x : t.systems) x.tokens = 0;
                for (int q = 0; q < kSides; ++q)
                    for (int x : {int(A), int(R), int(T)}) t.systems[size_t(x)].space[size_t(q)] = Force{};
                t.systems[size_t(A)].space[size_t(p)].add(UnitType::Carrier, 1);
                t.systems[size_t(R)].space[size_t(p)].add(UnitType::Infantry, 1);
                t.phase = Phase::Action;
                t.activePlayer = p;
                t.activeSystem = T;
                const int C = int(UnitType::Carrier), I = int(UnitType::Infantry);
                const std::string plain = checkMovePlan(t, p, T, {A, -1, C, 1, int(R), -1, I, 1});
                const std::string routed = checkMovePlan(t, p, T, {A, -1, C, 1, int(R), -1, I, 1, kWaypointTuple, A, int(R), 0});
                const std::string bad = checkMovePlan(t, p, T, {A, -1, C, 1, kWaypointTuple, A, T == A ? 0 : int(T), 0});
                const Reach rr = shipReachVia(t, p, A, {int(R)}, T, UnitType::Carrier, 0);
                std::printf("waypoints: carrier %s -> %s, infantry waiting in the rift %s: no route: %s; route through the rift: %s "
                            "(rolls %d); a bad route: %s\n",
                            kSystems[sa.def].name, kSystems[st.def].name, kSystems[rs.def].name, plain.empty() ? "allowed" : plain.c_str(),
                            routed.empty() ? "allowed" : routed.c_str(), rr.riftRolls, bad.empty() ? "allowed" : bad.c_str());
                if (plain.empty() || !routed.empty() || rr.riftRolls != 1 || bad.empty()) continue;
                checked = 1;
                // the move editor on the map, as a player would use it
                Decision d;
                d.player = p;
                d.kind = DecisionKind::Move;
                d.tag = Tag::Movement;
                d.system = T;
                d.prompt = std::string("Move ships into ") + kSystems[st.def].name;
                d.context = "Ships can't leave systems that hold your command tokens. They can pick up units on the way.";
                const Game* gp = &t;
                const int pp = p, tt = T;
                d.validate = [gp, pp, tt](const Answer& a) { return checkMovePlan(*gp, pp, tt, a.data); };
                MapView map;
                map.setViewport(Rect{0, 22, 420, 366});
                map.setDrawArea(Rect{0, 22, 640, 430});
                map.setCursor(t, T);
                map.setZoom(2);
                std::unique_ptr<DecisionUI> ui = makeDecisionUI(t, d, map);
                uint32_t now = 1000;
                auto press = [&](Btn b) {
                    Input in;
                    in.beginFrame();
                    in.handle(b, true, now);
                    now += 40;
                    return ui->update(in, now);
                };
                auto shot = [&](const std::string& name) {
                    Canvas c(640, 480, testScale());
                    hits::clear();
                    drawSpaceBackground(c, false);
                    map.setActiveSystem(T);
                    map.draw(c, t, now);
                    ui->draw(c, now);
                    drawHintBar(c, ui->hints());
                    savePpm(c, outDir + "/" + name + ".ppm");
                    ++shots;
                };
                press(Btn::Right);  // one carrier (the first row)
                shot("wp_1_list");
                press(Btn::X);  // Waypoint Mode
                shot("wp_2_start");
                map.setCursor(t, int(R));
                press(Btn::A);
                shot("wp_3_rift");
                map.setCursor(t, T);
                press(Btn::A);  // the active system ends the route
                shot("wp_4_done");
                // the infantry in the rift is offered now: take it, then move
                bool took = false;
                for (int k = 0; k < 12 && !took; ++k) {
                    press(Btn::Down);
                    press(Btn::Right);
                    took = checkMovePlan(t, p, T, ui->answer().data).empty() && ui->answer().data.size() >= 12;
                    if (!took) press(Btn::Left);
                }
                shot("wp_5_pickup");
                const bool done = press(Btn::Start);
                std::printf("waypoints: editor answer %s (%zu numbers, %s)\n", done ? "ready" : "not ready", ui->answer().data.size(),
                            checkMovePlan(t, p, T, ui->answer().data).empty() ? "legal" : checkMovePlan(t, p, T, ui->answer().data).c_str());
                if (!done || !took) checked = 0;
            }
    }
    if (!checked) {
        std::printf("waypoints: no rift scenario found / the route check failed\n");
        return 0;
    }
    // the crosshair button on the real game screen: a human move, the mouse over it (the tooltip), then a click
    std::unique_ptr<Platform> plat = makeHeadlessPlatform("", outDir);
    Settings settings;
    settings.setInt("ai_speed", 3);
    App app(*plat, settings);
    GameSession::savePath() = outDir + "/wp_save.ti4";
    std::unique_ptr<GameSession> session(new GameSession());
    Setup s2;
    s2.players = 4;
    s2.human = 0;
    s2.seed = 9;
    if (!session->startNew(s2)) return 0;
    GameSession* sp = session.get();
    std::unique_ptr<Scene> scene = makeGameScene(std::move(session));
    Input in;
    Canvas c(testW(), 480, testScale());
    Rng r2(3);
    bool got = false;
    for (uint32_t tm = 1000; tm < 1000 + 30 * 60 * 1000 && !got && !sp->over(); tm += 33) {
        in.beginFrame();
        in.update(tm);
        if (sp->holding()) {
            in.handle(Btn::A, true, tm);
        } else if (sp->humanToAnswer()) {
            const Decision& d = sp->engine().decision();
            if (d.kind == DecisionKind::Move && d.tag == Tag::Movement && d.system >= 0) {
                scene->update(app, in, tm);
                hits::clear();
                drawTop(*scene, app, c, tm);
                const float bx = float(testW() - 220 - 6 - 13), by = float(452 - 48 - 2 - 6 - 13);
                in.beginFrame();
                Mouse& m = in.mouseState();
                m.active = true;
                m.moved = true;
                m.x = bx;
                m.y = by;
                scene->update(app, in, tm + 20);
                hits::clear();
                drawTop(*scene, app, c, tm + 20);
                savePpm(c, outDir + "/wp_scene_tooltip.ppm");
                hits::press(bx, by);  // the click
                in.beginFrame();
                scene->update(app, in, tm + 60);
                hits::clear();
                drawTop(*scene, app, c, tm + 80);
                savePpm(c, outDir + "/wp_scene_mode.ppm");
                shots += 2;
                got = true;
                break;
            }
            std::string err;
            sp->submit(aiAnswer(sp->game(), d, r2), &err, tm);
        }
        scene->update(app, in, tm);
        if (sp->holding()) {
            in.beginFrame();
            in.handle(Btn::A, false, tm + 10);
        }
    }
    std::remove(GameSession::savePath().c_str());
    std::printf("waypoints: %d screens (scene %s)\n", shots, got ? "ok" : "no human move found");
    return got ? shots : 0;
}

int testerChecks(const std::string&) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 4;
    s.human = -1;
    s.seed = 4242;
    s.draft = 0;
    s.modules = kModCodex;
    s.factions = {findFaction("cabal"), findFaction("sol"), findFaction("hacan"), findFaction("letnev")};
    e.start(s);
    Rng rng(3);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 2 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    const int p = 0;
    const int home = g.players[size_t(p)].homeSystem;
    int ok = 0;
    for (int16_t a : g.systems[size_t(home)].adj)
        for (int16_t b : g.systems[size_t(home)].adj) {
            if (a == b || ok) continue;
            if (g.systems[size_t(a)].home >= 0 || g.systems[size_t(b)].home >= 0) continue;
            if (g.systems[size_t(a)].d().anomalies || g.systems[size_t(b)].d().anomalies) continue;
            if (adjacentSystems(g, a, b)) continue;  // (the detour through the home is the only way)
            Game t = g;
            for (SystemState& x : t.systems) x.tokens = 0;
            for (int q = 0; q < t.numPlayers; ++q) t.systems[size_t(a)].space[size_t(q)] = Force{}, t.systems[size_t(b)].space[size_t(q)] = Force{};
            t.systems[size_t(a)].space[size_t(p)].add(UnitType::Carrier, 1);
            t.activePlayer = p;
            t.activeSystem = b;
            int planet = -1;
            for (int16_t x : t.systems[size_t(home)].planets)
                if (t.planets[size_t(x)].units[size_t(p)].count(UnitType::Infantry)) planet = x;
            if (planet < 0) continue;
            const std::vector<int> data = {a, -1, int(UnitType::Carrier), 1, home, planet, int(UnitType::Infantry), 1};
            for (int fx : {17, 18}) {
                t.fixes = fx;
                const std::string r = checkMovePlan(t, p, b, data);
                std::printf("tester: carrier %s -> %s picking up 1 infantry in %s (fixes %d): %s\n", kSystems[t.systems[size_t(a)].def].name,
                            kSystems[t.systems[size_t(b)].def].name, kSystems[t.systems[size_t(home)].def].name, fx, r.empty() ? "allowed" : r.c_str());
                if (fx == 18 && r.empty()) ok = 1;
            }
        }
    if (!ok) std::printf("tester: no allowed detour found\n");
    return ok;
}

// Lady damaged, then lost) and the Units screen (TI4_UITEST_ONLY=lady).
int ladyScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 777;
    s.draft = 0;
    s.modules = kModCodex | kModDiscordant;
    s.factions = {findFaction("ghemina"), findFaction("sol"), findFaction("letnev"), findFaction("hacan"), findFaction("nekro"), findFaction("arborec")};
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 3 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    int gh = -1;
    for (int q = 0; q < g.numPlayers; ++q)
        if (!std::strcmp(kFactions[size_t(g.players[size_t(q)].faction)].key, "ghemina")) gh = q;
    if (gh < 0 || g.players[size_t(gh)].unit[int(UnitType::Flagship2)] < 0) {
        std::printf("lady: no Ghemina with the Lady (fixes %d)\n", g.fixes);
        return 0;
    }
    int sys = -1;
    for (size_t i = 0; i < g.systems.size() && sys < 0; ++i)
        if (g.systems[i].home < 0 && g.systems[i].planets.size() >= 2 && int(i) != g.mecatol()) sys = int(i);
    int drawn = 0;
    {
        Force& f = g.systems[size_t(sys)].space[size_t(gh)];
        f = Force{};
        f.add(UnitType::Flagship, 1);
        f.add(UnitType::Flagship2, 1);
        f.add(UnitType::Dreadnought, 1);
        f.add(UnitType::Fighter, 3);
        f.dmg[int(UnitType::Flagship2)] = 1;
        for (int ts = 0; ts < 3; ++ts) {
            retro::setTileset(ts);
            Canvas c(640, 480, testScale());
            drawSpaceBackground(c, false);
            tileview::draw(c, g, sys, Rect{40, 30, 560, 420}, 1234);
            char name[64];
            std::snprintf(name, sizeof name, "/lady_tile_%d.ppm", ts);
            savePpm(c, outDir + name);
            ++drawn;
        }
        retro::setTileset(retro::kSimplistic);
    }
    drawInfo(*makeUnitScreen(g, gh), outDir + "/lady_units.ppm", {});
    drawInfo(*makeUnitScreen(g, gh), outDir + "/lady_unit_card.ppm", {Btn::Right, Btn::Right, Btn::Right, Btn::Right, Btn::Right, Btn::Right, Btn::A});
    drawn += 2;
    Force a, b;
    a.add(UnitType::Flagship, 1);
    a.add(UnitType::Flagship2, 1);
    a.add(UnitType::Carrier, 1);
    a.add(UnitType::Cruiser, 2);
    a.add(UnitType::Fighter, 4);
    b.add(UnitType::Dreadnought, 2);
    b.add(UnitType::Carrier, 1);
    b.add(UnitType::Destroyer, 2);
    b.add(UnitType::Fighter, 6);
    const int def = gh == 0 ? 1 : 0;
    for (int ts = 0; ts < 3; ++ts) {
        retro::setTileset(ts);
        battle::SpaceBattle bt;
        const uint32_t t0 = 100000;
        bt.start(g, sys, gh, def, a, b, t0, 1.f);
        Canvas c(640, 480, testScale());
        const uint32_t frames[] = {2600, 7000, 9000};
        size_t fi = 0;
        for (uint32_t t = 0; t <= 9000; t += 20) {
            const uint32_t now = t0 + t;
            if (t == 4000) bt.damaged(gh, UnitType::Flagship2, 1, now);
            if (t == 6000) bt.destroyed(gh, UnitType::Flagship2, 1, now);
            if (fi >= sizeof frames / sizeof frames[0] || t < frames[fi]) {
                if (t % 40) continue;
            }
            drawSpaceBackground(c, false);
            bt.draw(c, g, Rect{0, 22, 420, 366}, now, std::string());
            if (fi < sizeof frames / sizeof frames[0] && t >= frames[fi]) {
                char name[64];
                std::snprintf(name, sizeof name, "/lady_battle_%d_%05u.ppm", ts, unsigned(t));
                savePpm(c, outDir + name);
                ++fi;
                ++drawn;
            }
        }
    }
    retro::setTileset(retro::kSimplistic);
    return drawn;
}

// (prototype, owner's question 2026-11-08) The default soldier and mech: Simplistic, Voidfall now, Voidfall painted
// (TI4_UITEST_ONLY=voidpaint).
int voidPaintScreens(const std::string& outDir) {
    Canvas c(600, 262, testScale());
    c.clear(rgb(10, 8, 16));
    const char* titles[3] = {"Simplistic", "Voidfall now", "Voidfall painted (idea)"};
    const uint32_t team[2] = {mix(theme::player[1], rgb(110, 110, 104), 95), mix(theme::player[0], rgb(110, 110, 104), 95)};
    for (int k = 0; k < 3; ++k) {
        const int x0 = 4 + k * 198, y0 = 4, pw = 194, ph = 254;
        const bool vf = k > 0;
        retro::setTileset(vf ? retro::kVoidfall : retro::kSimplistic);
        gunits::setVoidPaint(k == 2);
        c.verticalGradient(x0, y0, pw, ph, vf ? rgb(70, 46, 82) : rgb(98, 94, 86), vf ? rgb(92, 60, 88) : rgb(86, 84, 78));
        for (int r = 0; r < 2; ++r) {
            const uint32_t col = team[r];
            gunits::Pose p;
            p.col = col;
            p.now = 777777;
            p.phase = 1.f;
            p.style = vf ? 2 : 0;
            p.s = 60.f;
            // the invader faces you (and fires); the defender is seen from behind
            p.x = float(x0 + 34);
            p.y = float(y0 + 58 + r * 104);
            p.dir = r == 0 ? 1 : -1;
            p.firing = r == 0;
            p.walk = 0.6f;
            gunits::infantry(c, gunits::Default, p);
            // the mech
            const float sz = 92.f, fx = float(x0 + 128), fy = float(y0 + 100 + r * 104) + 8.f;
            gunits::Pose m = p;
            m.x = fx;
            m.y = fy;
            m.s = sz;
            m.firing = false;
            if (k == 2) {
                gunits::paintedMech(c, m, fy, sz, col, r == 1, false);
            } else {
                c.fillCircle(fx - sz * 0.12f, fy, sz * 0.16f, rgb(0, 0, 0), 80);
                c.fillCircle(fx + sz * 0.12f, fy, sz * 0.16f, rgb(0, 0, 0), 80);
                icons::drawUnitArtOn(c, "unit/mech", fx, fy, int(sz * 0.9f), int(sz), col, r == 1, 255);
            }
        }
        if (vf) voidfall::grade(c, Rect{x0, y0, pw, ph});
        c.fillRect(x0, y0, pw, 15, rgb(0, 0, 0), 150);
        drawTextAligned(c, FONT_SMB, x0, y0 + 1, pw, Align::Center, titles[k], rgb(240, 236, 226));
    }
    gunits::setVoidPaint(false);
    retro::setTileset(retro::kSimplistic);
    savePpm(c, outDir + "/voidpaint.ppm");
    return 1;
}

// 0.44.0 (the ground combat overhaul): every faction look in a fight (TI4_UITEST_ONLY=groundfx; TI4_GFX_ONLY=<attacker key>,
// TI4_GFX_TS=0|1|2 the tileset (default all three), TI4_BATTLE_EVERY=ms frames for a clip) and a gallery of the units.
int groundFxScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4242;
    s.draft = 0;
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 3 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    Game g = e.game();
    auto factionIdx = [](const char* key) {
        for (int f = 0; f < kFactionCount; ++f)
            if (!std::strcmp(kFactions[f].key, key)) return f;
        return 0;
    };
    int planet = -1;
    for (size_t x = 0; x < g.planets.size() && planet < 0; ++x)
        if (g.systems[size_t(g.planets[x].system)].home < 0 && std::strcmp(kPlanets[size_t(g.planets[x].def)].key, "mr")) planet = int(x);
    static const char* kPairs[][2] = {{"arborec", "ghost"},    {"muaat", "arborec"},     {"naalu", "nekro"},      {"nekro", "sardakk"},
                                      {"sardakk", "cabal"},    {"cabal", "empyrean"},    {"empyrean", "mahact"},  {"mahact", "titans"},
                                      {"titans", "axis"},      {"crimson", "florzen"},   {"axis", "khrask"},      {"florzen", "mortheus"},
                                      {"khrask", "mykomentori"}, {"mortheus", "rohdhna"}, {"mykomentori", "zelian"}, {"rohdhna", "cheiran"},
                                      {"zelian", "ghoti"},     {"cheiran", "kyro"},      {"ghoti", "muaat"},      {"kyro", "naalu"},
                                      {"ghost", "titans"},     {"sol", "letnev"}};
    const char* onlyKey = std::getenv("TI4_GFX_ONLY");
    // 0.57.2 (owner's demo, "Nekro vs Sol"): TI4_GFX_VS=attacker,defender fights just that pair
    std::vector<std::pair<std::string, std::string>> pairs;
    if (const char* vs = std::getenv("TI4_GFX_VS")) {
        const std::string v = vs;
        const size_t k = v.find(',');
        if (k != std::string::npos) pairs.push_back({v.substr(0, k), v.substr(k + 1)});
    }
    if (pairs.empty())
        for (const auto& pr : kPairs) pairs.push_back({pr[0], pr[1]});
    const char* tsEnv = std::getenv("TI4_GFX_TS");
    const char* every = std::getenv("TI4_BATTLE_EVERY");
    const uint32_t step = every ? uint32_t(std::max(20, std::atoi(every))) : 0;
    const Rect area{0, 22, 420, 366};
    int drawn = 0;
    for (int ts = 0; ts < 3; ++ts) {
        if (tsEnv && std::atoi(tsEnv) != ts) continue;
        retro::setTileset(ts);
        for (const auto& pp : pairs) {
            const char* pr[2] = {pp.first.c_str(), pp.second.c_str()};
            if (onlyKey && std::strcmp(onlyKey, pr[0])) continue;
            g.players[0].faction = factionIdx(pr[0]);
            g.players[1].faction = factionIdx(pr[1]);
            Force a, b;
            a.add(UnitType::Infantry, 5);
            a.add(UnitType::Mech, 2);
            if (!std::strcmp(pr[0], "naalu")) a.add(UnitType::Fighter, 3);
            b.add(UnitType::Infantry, 4);
            b.add(UnitType::Mech, 1);
            b.add(UnitType::PDS, 1);
            b.add(UnitType::SpaceDock, 1);
            battle::GroundBattle bt;
            const uint32_t t0 = 500000;
            bt.start(g, g.planets[size_t(planet)].system, planet, 0, 1, a, b, t0, 1.f);
            const uint32_t frames[] = {700, 2600, 4200, 6000, 8200, 10400};
            size_t fi = 0;
            double benchMs = 0, worst = 0;
            int benchN = 0;
            Canvas c(640, 480, testScale());
            for (uint32_t t = 0; t <= 10400; t += 20) {
                const uint32_t now = t0 + t;
                if (t == 3000) bt.roll(0, {3, 8, 9, 2, 10, 6, 7}, {8, 8, 8, 8, 8, 6, 6}, 3, now, 1600);
                if (t == 4400) bt.roll(1, {4, 9, 2, 9, 1}, {8, 8, 8, 8, 6}, 2, now, 1600);
                if (t == 6000) {
                    bt.destroyed(1, UnitType::Infantry, 2, now);
                    bt.destroyed(1, UnitType::Mech, 1, now);
                    bt.destroyed(0, UnitType::Infantry, 1, now);
                }
                if (t == 7400) bt.destroyed(0, UnitType::Mech, 1, now);
                const bool shot = step ? t % step == 0 : fi < sizeof frames / sizeof frames[0] && t >= frames[fi];
                const bool bench = std::getenv("TI4_BATTLEBENCH") != nullptr;
                if (!shot && !bench && t % 40 != 0) continue;
                drawSpaceBackground(c, false);
                const uint32_t m0 = monotonicMs();
                bt.draw(c, g, area, now, std::string());
                benchMs += double(monotonicMs() - m0);
                ++benchN;
                worst = std::max(worst, double(monotonicMs() - m0));
                if (shot && !bench) {
                    char name[128];
                    std::snprintf(name, sizeof name, "/groundfx_%d_%s_%05u.ppm", ts, pr[0], unsigned(t));
                    savePpm(c, outDir + name);
                    ++fi;
                    ++drawn;
                }
                if (shot && bench) ++fi;
            }
            if (std::getenv("TI4_BATTLEBENCH"))
                std::printf("groundfx bench ts %d %-12s %.2f ms per frame (worst %.0f)\n", ts, pr[0], benchMs / std::max(1, benchN), worst);
        }
        // the gallery: every look's soldier, its fallen and its mech, large
        Canvas c(640, 480, testScale() * 2.f);
        c.clear(rgb(52, 62, 44));
        const int cols = 7;
        for (int l = 0; l < gunits::kLooks; ++l) {
            const int cx = (l % cols) * 91 + 46, cy = (l / cols) * 160 + 10;
            c.fillRect(cx - 44, cy, 88, 150, rgb(70, 84, 58));
            drawTextAligned(c, FONT_XS, cx - 45, cy + 2, 90, Align::Center, gunits::lookName(gunits::Look(l)), rgb(240, 240, 220));
            gunits::Pose p;
            p.col = mix(theme::player[(l + 2) % 8], rgb(110, 110, 104), 95);
            p.now = 777777;
            p.phase = 1.f;
            p.style = ts;
            p.x = float(cx - 22);
            p.y = float(cy + 52);
            p.s = 30.f * gunits::infantryScale(gunits::Look(l));
            p.dir = 1;
            p.firing = true;
            // Voidfall (0.51.1): painted - the default soldier by hand, the others drawn and re-lit as in a fight
            const gunits::Look lk = gunits::Look(l);
            auto paint = [&](uint64_t k, const gunits::Pose& q, float bl, float bt, float bw, float bh,
                             const std::function<void(Canvas&, const gunits::Pose&)>& fn) {
                if (ts == 2) {
                    const painted::Lit* lit = painted::relight(
                        c, k * 131u + uint64_t(l) * 7919u + 17u, bl, bt, bw, bh,
                        [&](Canvas& o, float ax, float ay) {
                            gunits::Pose z = q;
                            z.x = ax;
                            z.y = ay;
                            fn(o, z);
                        },
                        0);
                    if (lit) {
                        painted::drawLit(c, *lit, q.x, q.y, 255);
                        return;
                    }
                }
                fn(c, q);
            };
            auto man = [&](uint64_t k, const gunits::Pose& q) {
                if (ts == 2 && gunits::paintedByHand(lk, q.idx)) gunits::infantry(c, lk, q);
                else paint(k, q, -1.5f * q.s, -1.9f * q.s, 3.f * q.s, 2.9f * q.s, [&](Canvas& o, const gunits::Pose& z) { gunits::infantry(o, lk, z); });
            };
            man(1, p);
            p.idx = 1;
            p.x = float(cx - 22);
            p.y = float(cy + 112);
            p.dir = -1;
            p.firing = false;
            man(2, p);
            p.x = float(cx + 6);
            p.y = float(cy + 132);
            paint(3, p, -1.4f * p.s, -1.1f * p.s, 2.8f * p.s, 2.2f * p.s, [&](Canvas& o, const gunits::Pose& z) { gunits::fallen(o, lk, z, 1.f); });
            p.idx = 0;
            p.x = float(cx + 22);
            p.y = float(cy + 84);
            p.s = 46.f;
            p.dir = 1;
            if (gunits::hasMech(lk)) {
                paint(4, p, -1.2f * p.s, -1.7f * p.s, 2.4f * p.s, 2.f * p.s, [&](Canvas& o, const gunits::Pose& z) { gunits::mech(o, lk, z, false); });
            } else {
                if (ts == 2) gunits::paintedMech(c, p, p.y, p.s, p.col, false, false);
                else icons::drawUnitArtOn(c, "unit/mech", p.x, p.y, int(p.s * 0.9f), int(p.s), p.col, false, 255);
                gunits::mechOverlay(c, lk, p, false);
            }
        }
        char name[96];
        std::snprintf(name, sizeof name, "/groundfx_gallery_%d.ppm", ts);
        savePpm(c, outDir + name);
        ++drawn;
    }
    retro::setTileset(retro::kSimplistic);
    return drawn;
}

// 0.43.2: the space battles in the Fracture's seven systems and Thunder's Edge's, the ground battles on their
// planets - one frame each 1.6 s in (TI4_BATTLE_EVERY=N: a frame every N ms of a 9 s fight, for a clip;
// TI4_BATTLE_ONLY=<name> keeps one: frac1..7, te, cocytus, styx, lethe, phlegethon, thundersedge)
int specialBattles(const ti::Game& g0, const std::string& outDir, int ts) {
    using namespace ti;
    Game g = g0;
    const char* every = std::getenv("TI4_BATTLE_EVERY");
    const char* only = std::getenv("TI4_BATTLE_ONLY");
    const uint32_t step = every ? uint32_t(std::max(20, std::atoi(every))) : 0;
    const uint32_t endT = every ? 4400u : 1600u;
    const Rect area{0, 22, 420, 366};
    int n = 0;
    for (size_t sy = 0; sy < g.systems.size(); ++sy) {
        const SystemState& ss = g.systems[sy];
        if (!ss.fracture && int(sy) != g.teSystem) continue;
        const std::string name = ss.fracture ? std::string("frac") + (kSystems[ss.def].key + 8) : std::string("te");
        if (only && name != only) continue;
        Force a, b;
        a.add(UnitType::Dreadnought, 2);
        a.add(UnitType::Carrier, 1);
        a.add(UnitType::Cruiser, 2);
        a.add(UnitType::Fighter, 4);
        b.add(UnitType::Dreadnought, 1);
        b.add(UnitType::Destroyer, 2);
        b.add(UnitType::Cruiser, 1);
        b.add(UnitType::Fighter, 5);
        battle::SpaceBattle bt;
        const uint32_t t0 = 300000;
        bt.start(g, int(sy), 0, 1, a, b, t0, 1.f);
        Canvas c(640, 480, testScale());
        for (uint32_t t = 0; t <= endT; t += 20) {
            const uint32_t now = t0 + t;
            if (t == 2000) bt.roll(0, {3, 8, 9, 2, 10, 6}, {5, 5, 9, 9, 7, 7}, 3, now, 1600);
            if (t == 4200) bt.destroyed(1, UnitType::Destroyer, 1, now);
            if (t == 5000) bt.destroyed(1, UnitType::Fighter, 2, now);
            const bool shot = step ? t % step == 0 : t == endT;
            if (!shot && t % 40 != 0) continue;
            drawSpaceBackground(c, false);
            bt.draw(c, g, area, now, std::string());
            if (shot) {
                char nm[96];
                std::snprintf(nm, sizeof nm, "/special_space_%d_%s_%05u.ppm", ts, name.c_str(), unsigned(t));
                savePpm(c, outDir + nm);
                ++n;
            }
        }
    }
    for (size_t x = 0; x < g.planets.size(); ++x) {
        const char* key = kPlanets[size_t(g.planets[x].def)].key;
        bool want = false;
        for (const char* k : {"cocytus", "styx", "lethe", "phlegethon", "thundersedge"}) want = want || !std::strcmp(key, k);
        if (!want || g.planets[x].system < 0 || (only && std::strcmp(only, key) != 0)) continue;
        Force a, b;
        a.add(UnitType::Infantry, 5);
        a.add(UnitType::Mech, 1);
        b.add(UnitType::Infantry, 4);
        b.add(UnitType::PDS, 1);
        battle::GroundBattle bt;
        const uint32_t t0 = 400000;
        bt.start(g, g.planets[x].system, int(x), 0, 1, a, b, t0, 1.f);
        Canvas c(640, 480, testScale());
        for (uint32_t t = 0; t <= endT; t += 20) {
            const uint32_t now = t0 + t;
            if (t == 2000) bt.roll(0, {3, 8, 9, 2, 10}, {8, 8, 8, 8, 6}, 2, now, 1600);
            if (t == 4400) bt.destroyed(1, UnitType::Infantry, 2, now);
            const bool shot = step ? t % step == 0 : t == endT;
            if (!shot && t % 40 != 0) continue;
            drawSpaceBackground(c, false);
            bt.draw(c, g, area, now, std::string());
            if (shot) {
                char nm[96];
                std::snprintf(nm, sizeof nm, "/special_ground_%d_%s_%05u.ppm", ts, key, unsigned(t));
                savePpm(c, outDir + nm);
                ++n;
            }
        }
    }
    return n;
}

// Thunder's Edge placeholders and the Mallice warp (0.24.0): the whole map on a
// big canvas, each placeholder tile large, the Wormhole Nexus expanded
// (TI4_UITEST_ONLY=decor).
// Thunder's Edge (0.34.0): a game played until the Fracture and Thunder's Edge are in play - the map (before and
// after) and the Fracture's systems large, in each tileset. TI4_FRACSEED picks the game.
int fractureScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = std::getenv("TI4_FRACSEED") ? uint32_t(std::atoi(std::getenv("TI4_FRACSEED"))) : 707u;
    s.modules = kModCodex | kModThundersEdge;
    e.start(s);
    Rng rng(5);
    Game before;
    bool haveBefore = false;
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (!haveBefore && g.round >= 2 && g.phase == Phase::Action) {
            before = g;
            haveBefore = true;
        }
        if (g.fractureIn && g.teSystem >= 0 && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    const Game& g = e.game();
    std::printf("fracture: in play %d, Thunder's Edge %s, round %d\n", g.fractureIn ? 1 : 0, g.teSystem >= 0 ? "revealed" : "not yet", int(g.round));
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        for (int which = 0; which < 2; ++which) {
            const Game& gg = which == 0 && haveBefore ? before : g;
            Canvas big(1280, 1040);
            big.clear(rgb(6, 8, 16));
            MapView map;
            map.setViewport(Rect{0, 0, 1280, 1040});
            map.setInfoTiles(true);
            map.setZoom(1);
            map.setCursor(gg, std::max(0, gg.mecatol()));
            for (int f = 0; f < 6; ++f) map.draw(big, gg, 5000 + uint32_t(f) * 150);
            savePpm(big, outDir + "/fracture_map_" + std::to_string(ts) + (which == 0 ? "_before" : "_after") + ".ppm");
            ++n;
        }
        for (size_t sy = 0; sy < g.systems.size(); ++sy) {
            if (!g.systems[sy].fracture && int(sy) != g.teSystem && !g.systems[sy].ingress) continue;
            Canvas c(640, 480, testScale());
            c.clear(rgb(6, 8, 16));
            for (int k = 0; k < 8; ++k) tileview::draw(c, g, int(sy), Rect{40, 30, 560, 420}, 1234 + uint32_t(k) * 100);
            savePpm(c, outDir + "/fracture_tile_" + std::to_string(ts) + "_" + (g.systems[sy].ingress ? "ingress_" : "") +
                           kSystems[g.systems[sy].def].key + ".ppm");
        }
        n += specialBattles(g, outDir, ts);  // 0.43.2
        {  // 0.43.0: an ingress system at the closest zoom
            for (size_t sy = 0; sy < g.systems.size(); ++sy) {
                if (!g.systems[sy].ingress) continue;
                Canvas big(640, 480, testScale());
                big.clear(rgb(6, 8, 16));
                MapView map;
                map.setViewport(Rect{0, 0, 640, 480});
                map.setZoom(3);
                map.setCursor(g, int(sy));
                for (int f = 0; f < 6; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
                savePpm(big, outDir + "/fracture_ingress_zoom_" + std::to_string(ts) + ".ppm");
                break;
            }
        }
    }
    retro::setTileset(retro::kSimplistic);
    return n;
}

// 0.43.0: play areas beside the faction tiles - a game with the Axis, Myko-Mentori, Olradin, Augurs, Firmament and
// Celdauri played into round 4; the map at two zooms around each player's tiles (TI4_PLAYSEED)
int playAreaScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = std::getenv("TI4_PLAYSEED") ? uint32_t(std::atoi(std::getenv("TI4_PLAYSEED"))) : 4321u;
    s.modules = kModCodex | kModUncharted | kModThundersEdge | kModDiscordant;
    for (const char* f : {"axis", "mykomentori", "olradin", "augers", "firmament", "celdauri"}) s.factions.push_back(findFaction(f));
    e.start(s);
    Rng rng(9);
    int exped = 0;
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (g.round >= 4 && g.phase == Phase::Action) break;
        const Decision& d = e.decision();
        if (exped < 2 && d.tag == Tag::Faction && d.ability == int(Ab::Expedition)) {  // the winglet menu (0.43.0)
            Canvas c(640, 480, testScale());
            MapView map;
            map.setViewport(Rect{0, 22, 420, 430});
            drawSpaceBackground(c, false);
            map.draw(c, g, 0);
            std::unique_ptr<DecisionUI> ui = makeDecisionUI(g, d, map);
            Input in;
            in.beginFrame();
            in.handle(Btn::Right, true, 100);
            ui->update(in, 100);
            ui->draw(c, 1200);
            drawHintBar(c, ui->hints());
            savePpm(c, outDir + "/ui_expedition" + std::to_string(exped++) + ".ppm");
        }
        if (!e.answer(aiAnswer(g, d, rng))) break;
    }
    const Game& g = e.game();
    int n = 0;
    for (int p = 0; p < g.numPlayers; ++p) {
        const auto items = playarea::of(g, p, p);
        std::printf("play area %d %s:", p, playerName(g, p).c_str());
        for (const auto& it : items) std::printf(" [%s]", it.name.c_str());
        std::printf("\n");
        for (int z : {2, 3}) {
            Canvas big(640, 480, testScale());
            big.clear(rgb(6, 8, 16));
            MapView map;
            map.setViewport(Rect{0, 0, 640, 480});
            map.setInfoTiles(true);
            map.setViewer(p);
            map.setZoom(z);
            map.setCursor(g, MapView::kInfoBase + p);
            for (int f = 0; f < 4; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
            savePpm(big, outDir + "/playarea_" + std::to_string(p) + "_z" + std::to_string(z) + ".ppm");
            ++n;
        }
        if (items.empty()) continue;
        {  // 0.43.4: can the D-pad reach the play area from the info tiles (and come back)? (up to 3 presses)
            MapView map;
            map.setViewport(Rect{0, 0, 640, 480});
            map.setInfoTiles(true);
            map.setViewer(p);
            std::vector<int> frontier = {MapView::kInfoBase + p}, seen = frontier;
            int depth = -1;
            for (int d = 1; d <= 3 && depth < 0; ++d) {
                std::vector<int> next;
                for (int from : frontier)
                    for (int dir = 0; dir < 4; ++dir) {
                        map.setCursor(g, from);
                        if (!map.moveCursor(g, dir)) continue;
                        const int to = map.cursor();
                        if (to == MapView::kPlayBase + p) depth = d;
                        if (std::find(seen.begin(), seen.end(), to) == seen.end()) seen.push_back(to), next.push_back(to);
                    }
                frontier = next;
            }
            int back = -1;
            for (int dir = 0; dir < 4 && back < 0; ++dir) {
                map.setCursor(g, MapView::kPlayBase + p);
                if (map.moveCursor(g, dir) && map.cursor() == MapView::kInfoBase + p) back = dir;
            }
            std::printf("play area %d: reached from the info tiles in %d press(es); back to them with dir %d\n", p, depth, back);
        }
        {  // 0.43.4: the cursor on the play area itself, then its window (the first card, then two to the right)
            Canvas big(640, 480, testScale());
            big.clear(rgb(6, 8, 16));
            MapView map;
            map.setViewport(Rect{0, 0, 640, 480});
            map.setInfoTiles(true);
            map.setViewer(p);
            map.setZoom(3);
            map.setCursor(g, MapView::kPlayBase + p);
            for (int f = 0; f < 4; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
            savePpm(big, outDir + "/playarea_" + std::to_string(p) + "_cursor.ppm");
            std::unique_ptr<InfoScreen> win = makePlayAreaScreen(g, p, p);
            for (int k = 0; k < 3 && k < int(items.size()); ++k) {
                Canvas c(640, 480, testScale());
                drawSpaceBackground(c, false);
                map.draw(c, g, 5600);
                win->draw(c, 1200);
                savePpm(c, outDir + "/playwin_" + std::to_string(p) + "_" + std::to_string(k) + ".ppm");
                ++n;
                Input in;
                in.beginFrame();
                in.handle(Btn::Right, true, 100);
                win->update(in);
            }
            if (p == 3) {  // the Augurs' Oracle AI as another player sees it (face down)
                std::unique_ptr<InfoScreen> other = makePlayAreaScreen(g, 0, p);
                for (int k = 0; k < int(items.size()); ++k) {
                    Canvas c(640, 480, testScale());
                    drawSpaceBackground(c, false);
                    other->draw(c, 1200);
                    savePpm(c, outDir + "/playwin_" + std::to_string(p) + "_seenby0_" + std::to_string(k) + ".ppm");
                    Input in;
                    in.beginFrame();
                    in.handle(Btn::Right, true, 100);
                    other->update(in);
                }
            }
        }
    }
    return n;
}

// 0.35.0: a Thunder's Edge game with the Firmament played until they become the Obsidian - the map, the turned-over
// home system (96b) in every tileset and both sides' faction sheets (plots row)
int obsidianScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = std::getenv("TI4_FRACSEED") ? uint32_t(std::atoi(std::getenv("TI4_FRACSEED"))) : 1300u;
    s.modules = kModCodex | kModThundersEdge;
    s.factions = {findFaction("firmament"), findFaction("hacan"), findFaction("sol"), findFaction("deepwrought"), findFaction("crimson"),
                  findFaction("bastion")};
    e.start(s);
    Rng rng(5);
    Game before;
    bool haveBefore = false;
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (!haveBefore && g.players[0].plotsIn && g.phase == Phase::Action) {
            before = g;
            haveBefore = true;
        }
        if (isFaction(g, 0, "obsidian") && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    const Game& g = e.game();
    std::printf("obsidian: faction %s, plots %d, round %d\n", kFactions[g.players[0].faction].key, plotCount(g.players[0]), int(g.round));
    if (haveBefore) drawInfo(*makeFactionSheet(before, 0), outDir + "/obsidian_sheet_firmament.ppm", {});
    // 0.44.1: Your cards > Faction (the plots, played or not)
    if (haveBefore) drawInfo(*makeHandScreen(before, 0), outDir + "/obsidian_cards_firmament.ppm", {Btn::L1});
    drawInfo(*makeHandScreen(g, 0), outDir + "/obsidian_cards.ppm", {Btn::L1});
    drawInfo(*makeHandScreen(g, 0), outDir + "/obsidian_cards_detail.ppm", {Btn::L1, Btn::A});
    drawInfo(*makeFactionSheet(g, 0), outDir + "/obsidian_sheet.ppm", {});
    drawInfo(*makeFactionSheet(g, 1), outDir + "/obsidian_sheet_other.ppm", {Btn::Right});
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        const int home = g.players[0].homeSystem;
        Canvas big(1280, 1040);
        big.clear(rgb(6, 8, 16));
        MapView map;
        map.setViewport(Rect{0, 0, 1280, 1040});
        map.setInfoTiles(true);
        map.setZoom(1);
        map.setCursor(g, std::max(0, home));
        for (int f = 0; f < 6; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
        savePpm(big, outDir + "/obsidian_map_" + std::to_string(ts) + ".ppm");
        Canvas c(640, 480, testScale());
        c.clear(rgb(6, 8, 16));
        for (int k = 0; k < 8; ++k) tileview::draw(c, g, home, Rect{40, 30, 560, 420}, 1234 + uint32_t(k) * 100);
        savePpm(c, outDir + "/obsidian_home_" + std::to_string(ts) + ".ppm");
        ++n;
    }
    retro::setTileset(retro::kSimplistic);
    return n;
}

// 0.44.1: Your cards > Faction for the Axis (Axis Orders), the Olradin (policies), the Li-Zho (traps) and the
// Deepwrought (ocean cards, breakthrough) - a Thunder's Edge + Discordant Stars game played into round 3
int factionCardScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4401u;
    s.modules = kModCodex | kModThundersEdge | kModDiscordant;
    s.dsLevel = kDsLevel;
    s.factions = {findFaction("axis"), findFaction("olradin"), findFaction("lizho"), findFaction("deepwrought"), findFaction("sol"),
                  findFaction("hacan")};
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (g.round >= 3 && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    const Game& g = e.game();
    int n = 0;
    for (int p = 0; p < 6; ++p) {
        const std::string tag = outDir + "/fcards_" + kFactions[size_t(g.players[size_t(p)].faction)].key;
        drawInfo(*makeHandScreen(g, p), tag + ".ppm", {Btn::L1});
        drawInfo(*makeHandScreen(g, p), tag + "_detail.ppm", {Btn::L1, Btn::A});
        ++n;
    }
    std::printf("faction cards: round %d\n", int(g.round));
    return n;
}

// 0.44.3: breakthrough cards - every faction's card on sheets (portrait and compact), then a Thunder's Edge game's
// Technology screen (Yours, Breakthroughs, the card large), the faction sheet's secrets and technology page and its
// large view, and Your cards > Faction
int breakthroughScreens(const std::string& outDir) {
    using namespace ti;
    std::vector<int> fs;
    for (int f = 0; f < kFactionCount; ++f)
        if (breakthroughOf(kFactions[f].key)) fs.push_back(f);
    int n = 0;
    for (size_t at = 0, sheet = 0; at < fs.size(); at += 12, ++sheet) {
        Canvas c(960, 480, testScale());
        c.clear(rgb(14, 18, 30));
        for (size_t k = 0; k < 12 && at + k < fs.size(); ++k) {
            const int col = int(k % 6), row = int(k / 6);
            const btcard::State st = (at + k) % 7 == 3 ? btcard::NotGained : (at + k) % 7 == 5 ? btcard::Exhausted : btcard::Ready;
            btcard::draw(c, fs[at + k], Rect{10 + col * 158, 8 + row * 236, 150, 226}, st, false, 500);
        }
        savePpm(c, outDir + "/bt_sheet" + std::to_string(sheet) + ".ppm");
        ++n;
    }
    for (size_t at = 0, sheet = 0; at < fs.size(); at += 24, ++sheet) {
        Canvas c(640, 480, testScale());
        c.clear(rgb(14, 18, 30));
        for (size_t k = 0; k < 24 && at + k < fs.size(); ++k) {
            const int col = int(k % 3), row = int(k / 3);
            const btcard::State st = (at + k) % 7 == 3 ? btcard::NotGained : (at + k) % 7 == 5 ? btcard::Exhausted : btcard::Ready;
            btcard::draw(c, fs[at + k], Rect{8 + col * 210, 6 + row * 59, 200, 54}, st, k == 4, 500);
        }
        savePpm(c, outDir + "/bt_compact" + std::to_string(sheet) + ".ppm");
        ++n;
    }
    {  // the large card for the longest texts
        std::vector<int> byLen = fs;
        std::stable_sort(byLen.begin(), byLen.end(), [](int a, int b) {
            return std::strlen(breakthroughOf(kFactions[a].key)->text) > std::strlen(breakthroughOf(kFactions[b].key)->text);
        });
        byLen.resize(std::min<size_t>(byLen.size(), 3));
        byLen.push_back(findFaction("arborec"));
        for (size_t k = 0; k < byLen.size(); ++k) {
            Canvas c(640, 480, testScale());
            c.clear(rgb(14, 18, 30));
            btcard::draw(c, byLen[k], Rect{40, 20, 316, 440}, btcard::Ready, false, 500);
            btcard::draw(c, byLen[k], Rect{380, 20, 240, 96}, btcard::Ready, false, 500);
            btcard::draw(c, byLen[k], Rect{380, 130, 196, 78}, btcard::NotGained, false, 500);
            savePpm(c, outDir + "/bt_large_" + kFactions[size_t(byLen[k])].key + ".ppm");
            ++n;
        }
    }
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4431u;
    s.modules = kModCodex | kModThundersEdge;
    for (const char* k : {"arborec", "letnev", "nekro", "cabal", "mahact", "sol"}) s.factions.push_back(findFaction(k));
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 200000; ++steps) {
        const Game& g = e.game();
        if (g.round >= 3 && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    Game g = e.game();  // (a copy: the Arborec have their card, Letnev's is exhausted, the Nekro don't have theirs)
    g.players[0].breakthrough = true;
    g.players[0].btExhausted = false;
    g.players[1].breakthrough = true;
    g.players[1].btExhausted = true;
    g.players[2].breakthrough = false;
    std::printf("breakthroughs: round %d\n", int(g.round));
    const std::vector<Btn> tab3 = {Btn::R1, Btn::R1, Btn::R1};
    for (int p : {0, 2}) {
        const std::string tag = outDir + "/bt_" + kFactions[size_t(g.players[size_t(p)].faction)].key;
        drawInfo(*makeTechScreen(g, p), tag + "_tech.ppm", {});
        drawInfo(*makeTechScreen(g, p), tag + "_tech_detail.ppm", {Btn::A});
        std::vector<Btn> b = tab3;
        drawInfo(*makeTechScreen(g, p), tag + "_tech_all.ppm", b);
        b.push_back(Btn::Right);
        b.push_back(Btn::A);
        drawInfo(*makeTechScreen(g, p), tag + "_tech_all_detail.ppm", b);
        drawInfo(*makeFactionSheet(g, p, p, true), tag + "_sheet.ppm", {});
        drawInfo(*makeFactionSheet(g, p, p, true), tag + "_sheet_large.ppm", {Btn::Y});
        drawInfo(*makeHandScreen(g, p), tag + "_hand.ppm", {Btn::L1});
        n += 7;
    }
    return n;
}

// 0.44.1: the faction pieces on the faction sheet (the tray in the bottom right, then Y = the large view) for the
// owner's 14 factions, from three games played into round 4 (TI4_PIECESROUND: another round)
int pieceScreens(const std::string& outDir) {
    using namespace ti;
    const int round = std::getenv("TI4_PIECESROUND") ? std::atoi(std::getenv("TI4_PIECESROUND")) : 4;
    struct G {
        int modules;
        const char* f[6];
    } games[3] = {{kModCodex | kModThundersEdge, {"cabal", "creuss", "naalu", "nekro", "titans", "empyrean"}},
                  {kModCodex | kModThundersEdge | kModDiscordant, {"vaden", "firmament", "edyn", "mykomentori", "bastion", "crimson"}},
                  {kModCodex | kModThundersEdge | kModDiscordant, {"kjalengard", "veldyr", "sol", "hacan", "lizho", "axis"}}};
    int n = 0;
    for (int gi = 0; gi < 3; ++gi) {
        Engine e;
        Setup s;
        s.players = 6;
        s.human = -1;
        s.seed = 4410u + uint32_t(gi);
        s.modules = games[gi].modules;
        s.dsLevel = kDsLevel;
        for (const char* k : games[gi].f) s.factions.push_back(findFaction(k));
        e.start(s);
        Rng rng(5);
        for (int steps = 0; e.waiting() && steps < 200000; ++steps) {
            const Game& g = e.game();
            if (g.round >= round && g.phase == Phase::Action) break;
            if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
        }
        const Game& g = e.game();
        std::printf("pieces: game %d round %d\n", gi, int(g.round));
        for (int p = 0; p < 6; ++p) {
            const std::string key = kFactions[size_t(g.players[size_t(p)].faction)].key;
            if (key == "sol" || key == "hacan" || key == "lizho" || key == "axis") continue;
            drawInfo(*makeFactionSheet(g, p), outDir + "/pieces_" + key + ".ppm", {});
            drawInfo(*makeFactionSheet(g, p), outDir + "/pieces_" + key + "_open.ppm", {Btn::Y});
            ++n;
        }
    }
    return n;
}

// 0.36.0: a Thunder's Edge game with galactic events (Minor Factions, Stellar Atomics, Total War, Monuments, Mercenaries)
// played into round 4 - the map in every tileset, a faction sheet and the Modules / galactic events screens' data
int eventScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = std::getenv("TI4_FRACSEED") ? uint32_t(std::atoi(std::getenv("TI4_FRACSEED"))) : 2024u;
    s.modules = kModCodex | kModThundersEdge;
    s.events = (1 << kEvMinor) | (1 << kEvAtomics) | (1 << kEvTotalWar) | (1 << kEvMonuments) | (1 << kEvMercenaries);
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (g.round >= 4 && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    const Game& g = e.game();
    int mon = 0, spoils = 0;
    for (const PlanetState& ps : g.planets) {
        mon += ps.monument >= 0;
        spoils += ps.spoils;
    }
    std::printf("events: round %d, monuments %d, spoils %d\n", int(g.round), mon, spoils);
    for (int p = 0; p < 2; ++p) drawInfo(*makeFactionSheet(g, p), outDir + "/events_sheet_" + std::to_string(p) + ".ppm", {});
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        Canvas big(1280, 1040);
        big.clear(rgb(6, 8, 16));
        MapView map;
        map.setViewport(Rect{0, 0, 1280, 1040});
        map.setInfoTiles(true);
        map.setZoom(1);
        map.setCursor(g, std::max(0, g.mecatol()));
        for (int f = 0; f < 6; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
        savePpm(big, outDir + "/events_map_" + std::to_string(ts) + ".ppm");
        ++n;
    }
    retro::setTileset(retro::kSimplistic);
    return n;
}

// 0.37.0: a Discordant Stars game (batch 1) played into round 3 - the map in every tileset, each faction's sheet
// (both pages) and its home system in the tile view (TI4_UITEST_ONLY=ds)
int dsScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = std::getenv("TI4_FRACSEED") ? uint32_t(std::atoi(std::getenv("TI4_FRACSEED"))) : 3700u;
    s.modules = kModCodex | kModDiscordant;
    s.factions = {findFaction("kortali"), findFaction("vaylerian"), findFaction("lanefir"), findFaction("kyro"), findFaction("cheiran"),
                  findFaction("celdauri")};
    if (const char* set = std::getenv("TI4_DSSET"); set && std::string(set) == "2")  // batch 2 (0.38.0)
        s.factions = {findFaction("khrask"), findFaction("zealots"), findFaction("zelian"), findFaction("rohdhna"), findFaction("tnelis"),
                      findFaction("kolume")};
    if (const char* set = std::getenv("TI4_DSSET"); set && std::string(set) == "3")  // batch 3 (0.39.0)
        s.factions = {findFaction("bentor"), findFaction("dihmohn"), findFaction("florzen"), findFaction("gledge"), findFaction("augers"),
                      findFaction("kjalengard")};
    if (const char* set = std::getenv("TI4_DSSET"); set && std::string(set) == "4")  // batch 4 (0.40.0)
        s.factions = {findFaction("kollecc"), findFaction("mortheus"), findFaction("vaden"), findFaction("cymiae"), findFaction("nivyn"),
                      findFaction("olradin")};
    if (const char* set = std::getenv("TI4_DSSET"); set && std::string(set) == "6")  // batch 6 (0.42.0)
        s.factions = {findFaction("veldyr"), findFaction("lizho"), findFaction("mirveda"), findFaction("ghoti"), findFaction("vaden"),
                      findFaction("axis")};
    if (const char* set = std::getenv("TI4_DSSET"); set && std::string(set) == "5")  // batch 5 (0.41.0)
        s.factions = {findFaction("freesystems"), findFaction("edyn"), findFaction("nokar"), findFaction("axis"), findFaction("mykomentori"),
                      findFaction("ghemina")};
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 90000; ++steps) {
        const Game& g = e.game();
        if (g.round >= 3 && g.phase == Phase::Action) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
    }
    const Game& g = e.game();
    std::printf("ds: round %d\n", int(g.round));
    for (int p = 0; p < g.numPlayers; ++p) {
        const std::string k = kFactions[g.players[size_t(p)].faction].key;
        drawInfo(*makeFactionSheet(g, p), outDir + "/ds_sheet_" + k + ".ppm", {});
        drawInfo(*makeFactionSheet(g, p), outDir + "/ds_sheet2_" + k + ".ppm", {Btn::X});
    }
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        Canvas big(1280, 1040);
        big.clear(rgb(6, 8, 16));
        MapView map;
        map.setViewport(Rect{0, 0, 1280, 1040});
        map.setInfoTiles(true);
        map.setZoom(1);
        map.setCursor(g, std::max(0, g.mecatol()));
        for (int f = 0; f < 6; ++f) map.draw(big, g, 5000 + uint32_t(f) * 150);
        savePpm(big, outDir + "/ds_map_" + std::to_string(ts) + ".ppm");
        for (int p = 0; p < g.numPlayers; ++p) {
            Canvas c(640, 480, testScale());
            c.clear(rgb(6, 8, 16));
            for (int k = 0; k < 8; ++k) tileview::draw(c, g, g.players[size_t(p)].homeSystem, Rect{40, 30, 560, 420}, 1234 + uint32_t(k) * 100);
            savePpm(c, outDir + "/ds_home_" + std::string(kFactions[g.players[size_t(p)].faction].key) + "_" + std::to_string(ts) + ".ppm");
        }
        ++n;
    }
    retro::setTileset(retro::kSimplistic);
    return n;
}

int decorScreens(const std::string& outDir) {
    using namespace ti;
    int drawn = 0;
    if (const char* ts = std::getenv("TI4_TILESET")) retro::setTileset(std::atoi(ts));  // 0.43.0: any tileset (default Simplistic)
    for (int players : {6, 3, 4, 5, 8}) {
        Engine e;
        Setup s;
        s.players = players;
        s.human = -1;
        s.seed = 4242 + uint32_t(players);
        e.start(s);
        Rng rng(5);
        for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 2 && e.game().phase == Phase::Action); ++steps)
            if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
        const Game& g = e.game();
        Canvas big(1280, 1040);
        big.clear(rgb(6, 8, 16));  // (drawSpaceBackground only knows the 640 x 480 screen)
        MapView map;
        map.setViewport(Rect{0, 0, 1280, 1040});
        map.setInfoTiles(true);
        map.setZoom(1);
        map.setCursor(g, std::max(0, g.mecatol()));
        map.draw(big, g, 5000);
        savePpm(big, outDir + "/decor_map_" + std::to_string(players) + "p.ppm");
        ++drawn;
        if (players != 6) continue;
        const auto& dec = map.decorTiles(g);
        if (const char* fe = std::getenv("TI4_DECORFRAMES")) {  // 0.43.1 (test only): N frames of all eight, 40 ms apart, for a clip
            const int n = std::max(1, std::atoi(fe));
            for (int f = 0; f < n; ++f) {
                Canvas c(1600, 720);
                c.clear(rgb(6, 8, 16));
                for (const decor::Tile& t : dec) {
                    const int i = std::max(0, std::min(7, t.index));
                    decor::paint(c, t, 200.f + 400.f * float(i % 4), 180.f + 360.f * float(i / 4), 190.f, 2000u + uint32_t(f) * 40u, false);
                }
                char nm[32];
                std::snprintf(nm, sizeof nm, "/decor_anim_%04d.ppm", f);
                savePpm(c, outDir + nm);
            }
        }
        for (const decor::Tile& t : dec) {
            for (uint32_t now : {1000u, 1450u}) {
                Canvas c(640, 480, testScale());
                c.clear(rgb(6, 8, 16));
                const bool te = t.kind == decor::ThundersEdge;
                decor::paint(c, t, 320.f, 250.f, te ? 150.f : 200.f, now, true);
                if (te) {  // 0.43.0: the winglets (two slices joined, for the look)
                    const uint32_t claim[6] = {theme::player[1], 0, 0, theme::player[3], 0, 0};
                    decor::drawWinglets(c, 320.f, 250.f, 150.f, claim, now);
                }
                savePpm(c, outDir + "/decor_tile" + std::to_string(t.index) + "_" + std::to_string(now) + ".ppm");
                ++drawn;
            }
        }
        if (g.nexus >= 0)
            for (uint32_t now : {1000u, 1800u}) {
                Canvas c(640, 480, testScale());
                c.clear(rgb(6, 8, 16));
                tileview::draw(c, g, g.nexus, Rect{20, 68, 600, 380}, now);
                savePpm(c, outDir + "/decor_nexus_" + std::to_string(now) + ".ppm");
                ++drawn;
            }
    }
    return drawn;
}

// Retrowave (0.26.0): the map at every zoom in both tilesets, every printed tile
// and every planet in the pixel art, how long a map takes to draw
// (TI4_UITEST_ONLY=retro; TI4_RETROBENCH=1 prints the timings).
static double monotonicUs() {
    timespec ts{};
    monotonicNow(&ts);
    return double(ts.tv_sec) * 1e6 + double(ts.tv_nsec) / 1e3;
}
int retroScreens(const std::string& outDir) {
    using namespace ti;
    // 0.32.0 (owner): the rolling stars behind Credits and New Game vanished once the game had run a while
    for (uint32_t late : {5000u, 3000000000u}) {
        Canvas c(640, 480, testScale());
        c.clear(rgb(0, 0, 0));
        backdrops::drawCredits(c, late);
        int lit = 0;
        for (int i = 0; i < c.pw() * c.ph(); ++i)
            if ((c.pixels()[i] & 0xFF) > 120) ++lit;
        std::printf("credits stars at %u ms: %d bright pixels\n", late, lit);
        savePpm(c, outDir + "/credits_" + std::to_string(late) + ".ppm");
        backdrops::drawRetroNewGame(c, late);
        savePpm(c, outDir + "/newgame_" + std::to_string(late) + ".ppm");
    }
    if (!retro::available()) {
        std::printf("uitest: retro.dat not found\n");
        return 0;
    }
    int drawn = 0;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 777;
    e.start(s);
    Rng rng(5);
    for (int steps = 0; e.waiting() && steps < 60000 && !(e.game().round >= 3 && e.game().phase == Phase::Action); ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    const Game& g = e.game();
    for (int art = 0; art < retro::kTilesets; ++art) {  // (0.32.0: and Voidfall)
        if (!retro::tilesetAvailable(art)) continue;
        retro::setTileset(art);
        const std::string tag = art == retro::kVoidfall ? "voidfall" : art ? "retro" : "simple";
        {
            Canvas big(1280, 1040);
            big.clear(rgb(6, 8, 16));
            MapView map;
            map.setViewport(Rect{0, 0, 1280, 1040});
            map.setInfoTiles(true);
            map.setZoom(1);
            map.setCursor(g, std::max(0, g.mecatol()));
            for (uint32_t now = 4000; now <= 5000; now += 33) map.draw(big, g, now);
            savePpm(big, outDir + "/retro_map_" + tag + ".ppm");
            ++drawn;
        }
        for (int z = 0; z < MapView::kZoomLevels; ++z) {
            Canvas c(640, 480, testScale());
            c.clear(rgb(6, 8, 16));
            MapView map;
            map.setViewport(Rect{0, 24, 640, 456});
            map.setZoom(z);
            map.setCursor(g, std::max(0, g.mecatol()));
            const uint32_t cold0 = monotonicMs();
            map.draw(c, g, 4000);
            const uint32_t coldMs = monotonicMs() - cold0;
            for (uint32_t now = 4033; now <= 5000; now += 33) map.draw(c, g, now);  // (Voidfall builds a few tiles a frame)
            savePpm(c, outDir + "/retro_zoom" + std::to_string(z) + "_" + tag + ".ppm");
            ++drawn;
            if (std::getenv("TI4_RETROBENCH")) {
                const uint32_t t0 = monotonicMs();
                int n = 0;
                const uint32_t step = std::getenv("TI4_RETROSTEP") ? uint32_t(std::atoi(std::getenv("TI4_RETROSTEP"))) : 33u;
                double worst = 0, steady = 0, steadyWorst = 0;
                int ns = 0;
                for (uint32_t now = 6000; monotonicMs() - t0 < 2500; now += step, ++n) {
                    const double f0 = monotonicUs();
                    map.draw(c, g, now);
                    const double ms = (monotonicUs() - f0) / 1000.0;
                    worst = std::max(worst, ms);
                    if (now >= 6000 + 1500) {  // after the first 1.5 s of game time: everything built
                        steady += ms;
                        steadyWorst = std::max(steadyWorst, ms);
                        ++ns;
                    }
                }
                std::printf("retrobench %-8s zoom %d: %.2f ms per map, worst %.2f ms, first draw %u ms; steady %.2f ms, worst %.2f ms\n",
                            tag.c_str(), z, double(monotonicMs() - t0) / std::max(1, n), worst, coldMs, steady / std::max(1, ns),
                            steadyWorst);
            }
        }
    }
    retro::setOn(true);
    // the expanded tile view: Mecatol Rex, the homes, a few others, the nexus
    {
        std::vector<int> pick;
        if (g.mecatol() >= 0) pick.push_back(g.mecatol());
        for (int p = 0; p < 2; ++p) pick.push_back(g.players[size_t(p)].homeSystem);
        int planets3 = -1, worm = -1, anomaly = -1, busy = -1;
        int most = 0;
        for (size_t i = 0; i < g.systems.size(); ++i) {
            const SystemState& ss = g.systems[i];
            if (ss.home >= 0 || ss.isHyperlane()) continue;
            if (planets3 < 0 && ss.planets.size() >= 2) planets3 = int(i);
            if (worm < 0 && ss.d().wormholes && !ss.planets.empty()) worm = int(i);
            if (anomaly < 0 && ss.d().anomalies) anomaly = int(i);
            int units = 0;
            for (int q = 0; q < g.numPlayers; ++q) units += ss.space[size_t(q)].ships();
            if (units > most) {
                most = units;
                busy = int(i);
            }
        }
        for (int x : {planets3, worm, anomaly, busy, int(g.nexus)})
            if (x >= 0) pick.push_back(x);
        int k = 0;
        for (int sys : pick) {
            for (int art = 0; art < retro::kTilesets; ++art) {
                if (!retro::tilesetAvailable(art)) continue;
                retro::setTileset(art);
                Canvas c(640, 480, testScale());
                c.clear(rgb(6, 8, 16));
                tileview::draw(c, g, sys, Rect{20, 68, 600, 380}, 4000);
                savePpm(c, outDir + "/retro_tile" + std::to_string(k) + (art == retro::kVoidfall ? "_voidfall" : art ? "_retro" : "_simple") + ".ppm");
                ++drawn;
            }
            ++k;
        }
        retro::setOn(true);
    }
    for (int ts : {retro::kRetrowave, retro::kVoidfall}) {
    if (!retro::tilesetAvailable(ts)) continue;
    retro::setTileset(ts);
    const std::string vt = ts == retro::kVoidfall ? "_voidfall" : "";
    // every printed tile at 1:1 (corner radius 48) and twice that
    {
        Canvas c(1280, 1260);  // (0.33.0: 11 rows with Uncharted Space)
        c.clear(rgb(14, 16, 24));
        int k = 0;
        for (int d = 0; d < kSystemCount; ++d, ++k) {
            const float x = 54.f + float(k % 12) * 102.f, y = 50.f + float(k / 12) * 112.f;
            MapView::drawTile(c, d, x, y, 48.f, 3000);
            drawTextAligned(c, FONT_XS, int(x) - 48, int(y) + 42, 96, Align::Center, kSystems[d].key, theme::dim);
        }
        savePpm(c, outDir + "/retro_alltiles" + vt + ".ppm");
        ++drawn;
    }
    // the special tiles large (0.32.0): Mecatol Rex, the Vuil'raith home, a supernova, the Creuss gate,
    // a gravity rift, a hyperlane
    {
        Canvas c(1280, 1040, testScale());
        c.clear(rgb(14, 16, 24));
        const char* keys[6] = {"18", "54", "43", "17", "41", "84b"};
        for (int k = 0; k < 6; ++k) {
            int def = -1;
            for (int d = 0; d < kSystemCount; ++d)
                if (!std::strcmp(kSystems[d].key, keys[k])) def = d;
            if (def < 0) continue;
            const float x = 220.f + float(k % 3) * 420.f, y = 270.f + float(k / 3) * 500.f;
            MapView::drawTile(c, def, x, y, 200.f, 3000);
        }
        savePpm(c, outDir + "/retro_special" + vt + ".ppm");
        ++drawn;
    }
    // (0.45.0, test only) TI4_PLANETKEY=thundersedge: one planet large, at four moments, and on a hex the size of the map's
    if (const char* pk = std::getenv("TI4_PLANETKEY")) {
        int pl = -1;
        for (int i = 0; i < kPlanetCount; ++i)
            if (!std::strcmp(kPlanets[i].key, pk)) pl = i;
        if (pl >= 0) {
            Canvas c(1280, 400, testScale());
            c.clear(rgb(14, 16, 24));
            for (int k = 0; k < 4; ++k) {
                retro::Tile t;
                t.bg = -1;
                retro::Piece p;
                p.kind = retro::Piece::Planet;
                p.id = pl;
                p.r = 14.f;
                t.pieces.push_back(p);
                retro::draw(c, t, 160.f + float(k) * 320.f, 140.f, 220.f, 3000u + uint32_t(k) * 1700u);
                retro::draw(c, t, 160.f + float(k) * 320.f, 330.f, 96.f, 3000u + uint32_t(k) * 1700u);
            }
            savePpm(c, outDir + "/retro_planet_" + pk + vt + ".ppm");
            ++drawn;
        }
    }
    // every planet, big
    {
        Canvas c(1280, 1260);
        c.clear(rgb(14, 16, 24));
        for (int pl = 0; pl < kPlanetCount; ++pl) {
            retro::Tile t;
            t.bg = -1;
            retro::Piece p;
            p.kind = retro::Piece::Planet;
            p.id = pl;
            p.r = 14.f;
            t.pieces.push_back(p);
            const float x = 54.f + float(pl % 12) * 102.f, y = 46.f + float(pl / 12) * 112.f;
            retro::draw(c, t, x, y, 96.f, 3000);
            drawTextAligned(c, FONT_XS, int(x) - 50, int(y) + 40, 100, Align::Center, kPlanets[pl].name, theme::dim);
        }
        savePpm(c, outDir + "/retro_planets" + vt + ".ppm");
        ++drawn;
    }
    }
    retro::setOn(false);
    return drawn;
}

// 0.57.0 (TI4_UITEST_ONLY=codec; TI4_CODEC_ONLY=<key> just one): the comms codec for every faction offering a deal
// to the player (portrait, voice, speech typed out), plus the threat / vote / counter looks and one half-typed frame.
int codecScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = 1;
    s.seed = 4242;
    s.factions = {findFaction("hacan"), findFaction("sol")};
    e.start(s);
    Rng rng(3);
    for (int steps = 0; e.waiting() && steps < 4000 && e.game().phase != Phase::Action; ++steps)
        if (!e.answer(aiAnswer(e.game(), e.decision(), rng))) break;
    const char* only = std::getenv("TI4_CODEC_ONLY");
    int n = 0;
    auto shot = [&](Game g, int f, int listener, const std::string& name, int kind, uint32_t at) {
        g.players[0].faction = int8_t(f);
        g.players[0].human = false;
        g.players[1].faction = int8_t(listener);
        g.players[1].human = true;
        Deal deal;
        deal.from = 0;
        deal.to = 1;
        deal.give.tg = 2;
        deal.get.comm = 2;
        Decision d;
        d.player = 1;
        d.kind = DecisionKind::Pick;
        d.tag = Tag::DealResponse;
        d.targetPlayer = 0;
        d.agenda = kind == 2 ? 0 : -1;
        if (kind == 1) deal.threat = g.players[1].homeSystem;
        d.deal = encodeDeal(deal);
        d.prompt = playerName(g, 0) + (kind == 1 ? " makes a demand" : " offers a deal");
        d.options = {{1, "Accept", "The deal is made now."}, {0, "Decline", "No deal."}};
        if (kind != 3) d.options.push_back({2, "Counter", "Propose different terms."});
        Canvas c(testW(), 480, testScale());  // (TI4_SCREENW=854: as on a 16:9 PC - the codec is centred, its backdrop full width)
        MapView map;
        map.setViewport(Rect{0, 22, 420, 430});
        drawSpaceBackground(c, false);
        map.draw(c, g, 0);
        std::unique_ptr<DecisionUI> ui = makeDecisionUI(g, d, map);
        {
            CentredView v(c);
            ui->draw(c, 1);
            ui->draw(c, 1 + at);
        }
        drawHintBar(c, ui->hints());
        savePpm(c, outDir + "/codec_" + name + ".ppm");
        ++n;
    };
    const Game g0 = e.game();
    const int sol = findFaction("sol");
    for (int f = 0; f < kFactionCount; ++f) {
        if (only && std::strcmp(only, kFactions[f].key)) continue;
        shot(g0, f, f == sol ? findFaction("hacan") : sol, kFactions[f].key, 0, 9000);
    }
    if (!only) {
        shot(g0, findFaction("mahact"), findFaction("nivyn"), "vote_mahact_nivyn", 2, 9000);
        shot(g0, findFaction("muaat"), findFaction("bastion"), "threat_muaat", 1, 9000);
        shot(g0, findFaction("l1z1x"), sol, "counter_l1z1x", 3, 9000);
        shot(g0, findFaction("mortheus"), sol, "typing_mortheus", 0, 1400);
        shot(g0, findFaction("hacan"), findFaction("ralnel"), "pair_hacan_ralnel", 0, 9000);
    }
    std::printf("codec: %d screens\n", n);
    return n;
}

// 0.57.1 (tester: "flags and mechs on the planets in the main map view"; TI4_UITEST_ONLY=flags): a game played into
// round 4, the map round a system with mechs on a planet at three zooms in each tileset
int flagScreens(const std::string& outDir) {
    using namespace ti;
    Engine e;
    Setup s;
    s.players = 6;
    s.human = -1;
    s.seed = 4242;
    e.start(s);
    Rng rng(3);
    int sys = -1;
    for (int steps = 0; e.waiting() && steps < 40000 && e.game().round < 7; ++steps) {
        const Game& g = e.game();
        if (g.round >= 4 && g.phase == Phase::Action)
            for (const PlanetState& ps : g.planets)
                if (ps.owner >= 0 && ps.units[size_t(ps.owner)].count(UnitType::Mech) && g.systems[size_t(ps.system)].home < 0) sys = ps.system;
        if (sys >= 0) break;
        if (!e.answer(aiAnswer(g, e.decision(), rng))) break;
        e.takeEvents();
    }
    if (sys < 0) return 0;
    const Game& g = e.game();
    int n = 0;
    for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
        if (!retro::tilesetAvailable(ts)) continue;
        retro::setTileset(ts);
        for (int z : {1, 2, 3}) {
            Canvas c(640, 480, testScale());
            MapView map;
            map.setViewport(Rect{0, 22, 640, 430});
            map.setZoom(z);
            map.setCursor(g, sys);
            for (int f = 0; f < 12; ++f) {  // (Voidfall builds a few tiles a frame)
                drawSpaceBackground(c, false);
                map.draw(c, g, 1000 + uint32_t(f) * 100);
            }
            savePpm(c, outDir + "/map_flags_" + std::to_string(ts) + "_z" + std::to_string(z) + ".ppm");
            ++n;
        }
    }
    retro::setTileset(retro::kSimplistic);
    std::printf("flags: %d maps (round %d, system %s)\n", n, int(g.round), kSystems[g.systems[size_t(sys)].def].key);
    return n;
}

// 0.57.1 (tester: "Peace Accords offers space stations"; TI4_UITEST_ONLY=peace): Thunder's Edge games with the Xxcha;
// every Peace Accords question is checked for space stations among its choices, with the fix (20) and without (19)
int peaceAccordsCheck() {
    using namespace ti;
    int bad[2] = {0, 0}, asked[2] = {0, 0};
    for (int fx = 0; fx < 2; ++fx)
        for (uint32_t seed = 1; seed <= 40; ++seed) {
            Engine e;
            Setup s;
            s.players = 6;
            s.human = -1;
            s.seed = seed * 37;
            s.modules = kModCodex | kModThundersEdge;
            s.fixes = fx == 0 ? 19 : 20;
            s.factions = {findFaction("xxcha")};
            e.start(s);
            Rng rng(seed);
            for (int steps = 0; e.waiting() && steps < 30000; ++steps) {
                const Game& g = e.game();
                const Decision& d = e.decision();
                if (d.tag == Tag::Faction && d.ability == int(Ab::PeaceAccords)) {
                    ++asked[fx];
                    for (const Option& o : d.options)
                        if (o.id >= 0 && isStation(g, o.id)) {
                            ++bad[fx];
                            std::printf("peace: fixes %d seed %u: offered %s\n", s.fixes, seed, o.label.c_str());
                        }
                }
                if (!e.answer(aiAnswer(g, d, rng))) break;
                e.takeEvents();
            }
        }
    std::printf("peace: fixes 19 - %d questions, %d with a station; fixes 20 - %d questions, %d with a station\n", asked[0], bad[0],
                asked[1], bad[1]);
    return bad[1] == 0 ? 0 : 1;
}

int runUiTest(const std::string& outDir) {
    if (std::getenv("TI4_KEYBOARD_KEYS")) setKeyboardKeys(true);  // the Windows version's key names
    if (std::getenv("TI4_RETRO")) retro::setOn(true);              // test only: every screen in the Retrowave tileset
    if (std::getenv("TI4_VOIDFALL")) retro::setTileset(retro::kVoidfall);  // ... or in Voidfall (0.32.0)
    if (const char* only = std::getenv("TI4_UITEST_ONLY")) {  // test only: just the deal screens
        if (std::string(only) == "watch") return watchScreens(outDir) >= 14 ? 0 : 1;
        if (std::string(only) == "art") {
            artScreens(outDir);
            return 0;
        }
        if (std::string(only) == "floating") return floatingFactoryCheck(outDir) ? 0 : 1;
        g_uitestDir = outDir;
        if (std::string(only) == "fighters") return fighterCarryCheck() ? 0 : 1;
        if (std::string(only) == "reach") return reachCheck() == 0 ? 0 : 1;
        if (std::string(only) == "tiles") return tileScreens(outDir) >= 4 ? 0 : 1;
        if (std::string(only) == "trade") return tradeRound1Check();
        if (std::string(only) == "draft") return draftScreens(outDir) ? 0 : 1;
        if (std::string(only) == "nexus") {
            nexusScreens(outDir);
            return 0;
        }
        if (std::string(only) == "edge") return edgeScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "ustiles" || std::string(only) == "tetiles") {  // the module's tiles in each tileset, 6 x 4
            const bool te = std::string(only) == "tetiles";  // (0.34.0: Thunder's Edge - 24 of its tiles a sheet)
            std::vector<int> defs;
            for (int i = 0; i < ti::kSystemCount; ++i)
                if (ti::kSystems[i].source == (te ? ti::Source::ThundersEdge : ti::Source::Uncharted)) defs.push_back(i);
            const int page = std::getenv("TI4_TILEPAGE") ? std::atoi(std::getenv("TI4_TILEPAGE")) : 0;
            if (page > 0) defs.erase(defs.begin(), defs.begin() + std::min<long>(long(defs.size()), long(page) * 24));
            if (defs.size() > 24) defs.resize(24);
            for (int ts : {retro::kSimplistic, retro::kRetrowave, retro::kVoidfall}) {
                if (!retro::tilesetAvailable(ts)) continue;
                retro::setTileset(ts);
                for (int f = 0; f < 12; ++f) {  // (Voidfall builds a few tiles a frame)
                    Canvas c(640, 480, testScale());
                    c.clear(rgb(6, 8, 16));
                    for (size_t k = 0; k < defs.size(); ++k) {
                        const float x = 56.f + float(k % 6) * 105.f, y = 62.f + float(k / 6) * 112.f;
                        MapView::drawTile(c, defs[k], x, y, 50.f, 1000 + uint32_t(f) * 100);
                        drawTextAligned(c, FONT_XS, int(x) - 50, int(y) + 44, 100, Align::Center,
                                        std::string(ti::kSystems[defs[k]].key) + " " + ti::kSystems[defs[k]].name, theme::dim);
                    }
                    if (f == 11) savePpm(c, outDir + (te ? "/te_tiles_" : "/us_tiles_") + std::to_string(ts) + ".ppm");
                }
            }
            retro::setTileset(retro::kSimplistic);
            return 0;
        }
        if (std::string(only) == "battle") return battleScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "ground") return groundScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "groundfx") return groundFxScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "voidpaint") return voidPaintScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "lady") return ladyScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "tester") return testerChecks(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "waypoints") return waypointScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "habits") return habitChecks(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "decor") return decorScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "fracture") return fractureScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "obsidian") return obsidianScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "codec") return codecScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "flags") return flagScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "peace") return peaceAccordsCheck();
        if (std::string(only) == "fcards") return factionCardScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "pieces") return pieceScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "bt") return breakthroughScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "playarea") return playAreaScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "intro") {  // 0.43.0: the game-start flood (hologram look) at a few moments
            for (uint32_t t : {600u, 1500u, 2600u}) {
                Canvas c(640, 480, testScale());
                c.clear(rgb(40, 60, 90));
                transition::drawCover(c, t);
                savePpm(c, outDir + "/intro_cover_" + std::to_string(t) + ".ppm");
            }
            for (uint32_t t : {500u, 1300u, 2200u}) {
                Canvas c(640, 480, testScale());
                c.clear(rgb(40, 60, 90));
                transition::drawReveal(c, t);
                savePpm(c, outDir + "/intro_reveal_" + std::to_string(t) + ".ppm");
            }
            return 0;
        }
        if (std::string(only) == "events") return eventScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "ds") return dsScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only).compare(0, 5, "tile:") == 0) {  // 0.30.0: one printed tile, map size and large, a few moments
            const std::string key = std::string(only).substr(5);
            int def = -1;
            for (int i = 0; i < ti::kSystemCount; ++i)
                if (key == ti::kSystems[i].key) def = i;
            if (def < 0) return 1;
            // (0.34.0) TI4_TILESET=0/1/2 picks the tileset (default Retrowave)
            retro::setTileset(std::getenv("TI4_TILESET") ? std::atoi(std::getenv("TI4_TILESET")) : retro::kRetrowave);
            for (uint32_t now : {1000u, 2600u, 4200u}) {
                for (int f = 0; f < 12; ++f) {  // (Voidfall builds a few tiles a frame)
                    Canvas c(640, 480, testScale());
                    c.clear(rgb(6, 8, 16));
                    MapView::drawTile(c, def, 120.f, 120.f, 60.f, now + uint32_t(f));
                    MapView::drawTile(c, def, 420.f, 250.f, 192.f, now + uint32_t(f));
                    if (f == 11) savePpm(c, outDir + "/tile_" + key + "_" + std::to_string(now) + ".ppm");
                }
            }
            retro::setTileset(retro::kSimplistic);
            return 0;
        }
        if (std::string(only) == "retro") return retroScreens(outDir) > 0 ? 0 : 1;
        if (std::string(only) == "deals") {
            int m = menuOfferScreens(outDir);
            int r = dealDrive();
            return m > 0 && r == 0 ? 0 : 1;
        }
    }
    int decisions = decisionScreens(outDir);
    int watched = watchScreens(outDir);
    if (!agendaPeekScreen(outDir)) std::printf("uitest: no agenda peek card came up\n");
    if (!menuOfferScreens(outDir)) std::printf("uitest: no offer from the menu came up\n");
    int dealRejects = dealDrive();
    bool floating = floatingFactoryCheck(outDir);
    g_uitestDir = outDir;
    const bool fighterCarry = fighterCarryCheck();
    int tradeProblems = tradeRound1Check();
    const bool draft = draftScreens(outDir);
    nexusScreens(outDir);
    artScreens(outDir);
    const int tiles = tileScreens(outDir);
    return tiles >= 4 && decisions >= 14 && watched >= 14 && dealRejects == 0 && floating && fighterCarry && tradeProblems == 0 && draft ? 0 : 1;
}
