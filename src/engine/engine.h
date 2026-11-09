// The rules engine. The game itself runs as straight-line code on its own
// stack (a coroutine); whenever a player must decide something it pauses and
// hands a Decision to whoever drives the engine (UI or AI). answer() resumes it.
//
// Saving = the setup plus the list of answers. Loading replays them, which
// reproduces the game exactly because all randomness comes from a seeded Rng.
// Undo works the same way: replay every answer except the last one.
#pragma once
#if defined(__EMSCRIPTEN__)
#include <emscripten/fiber.h>
#elif !defined(_WIN32)
#include <ucontext.h>
#endif

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/decision.h"
#include "engine/rng.h"
#include "engine/state.h"

namespace ti {

struct Setup {
    int players = 6;
    int human = 0;               // seat played by the human, -1 = all AI
    std::vector<int> factions;   // per seat; -1 = random
    uint32_t seed = 1;
    int vpGoal = 10;
    int humanColor = -1;         // colour index for the human seat, -1 = default
    int aiLevel = 2;             // computer players: 0 Easy, 1 Normal, 2 Hard, 3 Competitive
    bool autoplay = false;       // the human seat is played by the computer too (watch a game)
    std::vector<int> archetypes; // per seat: computer player's archetype (Arch), -1 = random
    // New Game faction menu (0.19.0, owner). Saves without these lines load empty / 0.
    std::vector<int> colors;     // per seat: colour index, -1 = the next free one
    std::vector<int> levels;     // per seat: computer difficulty (0-3), -1 = aiLevel
    int humans = 0;              // bit per seat: more seats played by people (hot-seat), besides 'human'
    bool isHuman(int seat) const { return seat >= 0 && (seat == human || ((humans >> seat) & 1)); }
    // Trade card rules (0.14.0): bit 0 = X-1 trades offered with the primary,
    // bit 1 = house rule: round 1, everyone refills for free, bit 2 (0.15.0) =
    // ...only when a computer player plays it (a human chooses as usual), bit 3
    // (0.17.0) = the community X-1 (a neighbour washes its X commodities for X-1
    // trade goods; or the holder buys its Trade Agreement; non-neighbours owe 1).
    // Saves without the line (0.13 and older) load as 0, the old rules; 0.14-0.16
    // saves keep their 0.14 X-1 (bit 3 off).
    int trade = 15;
    // Galaxy setup (0.15.0): 1 = players draft the galaxy (deal tiles, place
    // them in turn as the rulebook describes), 0 = the balanced random map of
    // older versions. Saves without the line load as 0.
    int draft = 1;
    // Seeded galaxy (0.43.0, owner): with draft 0, a number the player types makes the map - the same seed and
    // player count give the same tiles in the same places; slices balanced harder (resources, influence,
    // legendary planets, wormholes, anomalies). 0 = the random map from the game's own seed. Saved as "mapseed".
    uint32_t mapSeed = 0;
    // Extra rules (0.19.0): bit 0 = Kingslay (a bot is sent to take Mecatol Rex
    // from a player who has held it for more than 5 of their turns), bit 1
    // (0.21.0) = the Council Keleres can be dealt as a random faction, bit 2
    // (0.27.0) = a Competitive bot with a random personality is never a Rusher
    // or Chaotic. Saves without the line load as 0.
    int extras = 255;  // bit 4 (0.30.0): AI Development Algorithm choice, X-1 paybacks in the agenda phase;
                      // bit 5 (0.32.0): Trade Convoys is played as an ACTION instead of placed at once;
                      // bit 6 (0.32.1): Dark Pact too;
                      // bit 7 (0.54.0): "Mecatol Rex: Contested" - bots fight over its holder's fleet (Imperial's
                      // points; off = "Calm": they only take it when left open)
    // Modules (0.31.0, roadmap Stage 11): the content in play besides the base game + Prophecy of Kings -
    // kModCodex (the Codex Omega cards replace their originals), kModUncharted (Uncharted Space),
    // kModThundersEdge, kModDiscordant. Saves without the line load as 0 (the content they were made with).
    int modules = 1;
    // Galactic events (0.36.0, Thunder's Edge): bit per GalacticEvent. Saves without the line load as 0.
    int events = 0;
    // Discordant Stars (0.37.0): how many of its faction batches the random pools may draw from (saved as "dslevel"
    // with the module, so a game keeps dealing the same factions when later batches arrive)
    int dsLevel = kDsLevel;
    // Rule fixes (0.33.0) that change what happened in older games, so they only apply to games started
    // with them: 1 = Lucky Shot destroys the ship that was picked (it misread odd-numbered systems).
    // Saves without the line load as 0.
    int fixes = 20;  // 1 Lucky Shot target (0.33.0), 2 the 0.43.0 fixes, 3 the Fracture's planets at its ends (0.43.2),
                    // 4 a token gained in a tactical action no longer gives another action (0.43.3),
                    // 5 the 0.43.4 fixes (entropic scars at the start of the status phase, ...),
                    // 6 the Veldyr start with all 4 Branch Offices (0.44.1: Tax Haven was missing),
                    // 7 the 0.44.2 fixes (G'hom Sek'kus asks infantry or mech, ...),
                    // 8 the 0.44.4 breakthrough fixes (the Creuss's Particle Synthesis was never found, ...),
                    // 9 the 0.44.5 cybernetic technology fixes (Sarween Tools only with Production, ...),
                    // 10 the 0.44.6 biotic technology fixes,
                    // 11 the 0.44.7 propulsion technology fixes,
                    // 12 the 0.44.8 fixes (The Nucleus for Avernus's controller, Ember Colossus with it; warfare techs)
                    // 13 the 0.44.9 RULES-CHECK answers (W / P / G / C / B lists of 0.44.4-0.44.8)
                    // 14 the 0.45.0 bug fixes (owner's 0.44.9 list: capacity timing, X-1 commodity value, ...)
                    // 15 the 0.46.1 fixes (the fleet pool checked at all times; Pass after an extra action, ...)
                    // 16 the Ghemina's Lady is her own unit (UnitType::Flagship2, 0.52.0, owner D117)
                    // 17 the Celdauri's Celagrom is its own unit (UnitType::Flagship2, 0.52.1)
                    // 18 the 0.54.0 tester fixes (a retreat takes ground forces from the planets; pick-ups on a
                    //    detour through a gravity rift the ship doesn't roll for - the Vuil'raith's own Tears)
                    // 19 the 0.55.0 fixes (a ship the Vuil'raith capture can't also be rebuilt by Subatomic Splicer)
                    // 20 the 0.57.1 tester fixes (Peace Accords / Kantrus don't take space stations; ...)
    // 0.54.0: the human players' habits from their earlier games (the bots adapt to them); saved as "habits" when known
    HabitProfile habits;
    // The Council Keleres (0.21.0): per seat, the faction whose home system a
    // Keleres seat takes (The Tribunii: Mentak, Xxcha or Argent; -1 = chosen
    // at random among the ones not in the game). Saves without it load empty.
    std::vector<int> tribunii;
};

enum class EventKind : uint8_t {
    Log,          // plain message
    Dice,         // a roll (dice / need / hits filled in)
    Destroyed,    // unit + count destroyed
    Damaged,      // unit used Sustain Damage
    Score,        // victory point(s) gained
    Phase,        // phase change / objective revealed
    StrategyPick, // player takes strategy card 'card' in the strategy phase
    TurnStart,    // player's turn begins
    TurnEnd,      // player's turn ends
    RoundEnd,
    ActionChosen, // card: 0..7 strategic, 20 tactical, 21 pass
    Activate,     // tactical action activates 'system' (hostile = enemy units there)
    Move,         // ships moved into 'system' from 'from'
    Land,         // ground forces landed on 'planet'
    Produce,
    Secondary,    // player uses a strategy card's secondary ability (card)
    CombatStart,  // a = attacker forces, b = defender forces; space or ground
    CombatEnd,    // player = winner (-1 draw)
    Retreat,
    Control,      // player gains control of 'planet'
    Research,
    Eliminated,
    Warning,
    ActionCard,   // player plays action card 'card' (other = player it targets, if any)
    Agenda,       // agenda 'card' is revealed
    Vote,         // player casts 'count' votes (text says for what)
    AgendaResult, // the agenda is resolved (text says how)
    Explore,      // player explores (card = exploration card, planet / system)
    Relic,        // player gains relic 'card'
    Leader,       // a leader ('card' = kLeaders index) is unlocked / used (label says which)
    Ability,      // a faction ability, faction technology or faction note is used (label = its name)
    AgendaPeek,   // the human player looks at agenda 'card' (Politics, Z'eu) - shown large until A (label = where from)
    Deal,         // negotiations: an offer, a deal made / declined, a promise broken (player, other)
    Reveal,       // end of round 3: the computer players' archetypes are revealed (text lists them)
    Betrayal,     // a computer player turns on another: player = offended bot, other = target, card = Betrayal kind,
                  // label = its name, text = what it will do (system = where, count = third player for a rumour)
    TilePlaced,   // galaxy setup: player placed a system tile ('system' = where it now is)
    // 0.16.0 (owner: cards shown large when used)
    ObjectiveScored,  // player scored objective 'count' (kObjectives index; before the Score event)
    RelicUsed,        // player exhausts or purges relic 'card' (label = "uses" / "purges")
    ComponentUsed,    // player uses a component action (not an action card or relic): label = name
                      // + '\n' + what it does, count = the action option id
    Planetfall,       // 0.24.0: the custodians token comes off Mecatol Rex (player = who) - the planetfall scene
    CardDrawn,        // 0.29.0: player drew action card 'count' (display only; shown only to that player)
    ObjectiveRevealed,// 0.29.0: public objective 'count' revealed (display only; the Phase log line stays)
    Note,             // 0.29.0: a feed line that stays out of the game log (old saves replay unchanged)
};

constexpr int8_t kActionTactical = 20;
constexpr int8_t kActionPass = 21;

struct Event {
    EventKind kind = EventKind::Log;
    int8_t player = -1;
    int8_t other = -1;   // opponent / second player
    int16_t system = -1;
    int16_t planet = -1;
    std::string text;
    std::string label;         // Dice: what was rolled ("Combat round 2", "Space cannon", ...)
    std::vector<int8_t> dice;  // rolled values
    std::vector<int8_t> need;  // value each die needed to hit
    int hits = 0;
    int16_t card = -1;   // strategy card (0..7), action kind, action card or agenda (int16: 0.34.0 - over 127 action cards)
    int8_t unit = -1;    // UnitType for Destroyed / Damaged
    int16_t count = 0;
    bool hostile = false;
    bool space = true;   // space or ground combat
    Force a, b;          // combat snapshots
    std::vector<int16_t> from;
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void start(const Setup& s);
    bool started() const { return started_; }
    bool finished() const { return finished_; }
    bool waiting() const { return hasPending_; }
    const Decision& decision() const { return pending_; }
    // Returns false (and fills err) if the answer is not acceptable.
    bool answer(const Answer& a, std::string* err = nullptr);

    // Undo: go back to the previous decision if it belonged to the same
    // player and nothing random or hidden happened since.
    bool canUndo() const;
    bool undo();
    // 0.43.0 (owner: "I cannot undo an activation when I have a second decision" - computer players were asked
    // something in between): answers to drop to get back to this player's previous decision, passing over the
    // answers of players `computer` names; 0 when that can't be done (dice rolled or something revealed since)
    int undoDepth(const std::function<bool(int)>& computer) const;
    bool undoSteps(int n);

    const Game& game() const { return g_; }
    const Setup& setup() const { return setup_; }
    int answerCount() const { return int(answers_.size()); }
    std::vector<Event> takeEvents();
    std::string crashMessage() const { return crash_; }

    // Save / load (text). load() replays every answer.
    std::string saveText() const;
    bool loadText(const std::string& text, std::string* err);

    // --- used by the rules code (runs inside the coroutine) -------------
    Game& g() { return g_; }
    Rng& rng() { return rng_; }
    Answer ask(Decision d);
    void emit(Event e);
    void log(int player, const std::string& text, EventKind kind = EventKind::Log, int system = -1);
    void revealed() { ++reveals_; }  // hidden information became known: undo can't cross this
    std::function<void()> beforeAsk;  // the rules check state-based triggers (leader unlocks) before each question

private:
    struct AskRecord {
        int8_t player;
        uint32_t rng;
        uint32_t reveals;
    };
    void fiberMain();
    void resume();
    void switchToGame();  // main code -> the game's stack
    void switchToMain();  // the game's stack -> main code
    bool replay(const std::vector<Answer>& answers, std::string* err);

    Setup setup_;
    Game g_;
    Rng rng_;
    std::vector<Answer> answers_;
    std::vector<AskRecord> asks_;
    std::vector<Event> events_;
    uint32_t reveals_ = 0;

    bool started_ = false, finished_ = false, hasPending_ = false, aborting_ = false;
    Decision pending_;
    Answer current_;
    std::string crash_;

#ifdef _WIN32
    // Windows has no ucontext: the game runs on a Win32 fiber instead.
    static void __stdcall fiberProc(void* self);
    void* mainFiber_ = nullptr;
    void* fiber_ = nullptr;
#elif defined(__EMSCRIPTEN__)
    // The web build: no ucontext in WebAssembly, so the game runs on an
    // Emscripten fiber (stack switching through Asyncify).
    static void fiberProc(void* self);
    emscripten_fiber_t mainFib_{}, gameFib_{};
    std::vector<char> stack_, mainAsync_, gameAsync_;
#else
    static void trampoline(uint32_t lo, uint32_t hi);
    ucontext_t mainCtx_{}, fiberCtx_{};
    std::vector<char> stack_;
#endif
};

}  // namespace ti
