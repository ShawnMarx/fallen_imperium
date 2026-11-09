// Computer-player personalities (Stage 6, 0.12.0 - the owner's Bot_AI design):
// every bot has a core archetype (how it plays and trades), an evolution path
// (how it changes as the game goes on), one generic trait and one of its
// faction's two traits. Traits and paths only shift priorities - they never
// give anything material. A per-pair B-Factor (betrayal factor) grows when
// others wrong the bot; every 8 of its turns a d100 at or under it arms a
// betrayal, which turns into a hidden betrayal objective against that player.
//
// All personality randomness comes from prand() (a hash of the game seed),
// not the game's Rng, so it never changes dice, decks or undo.
#pragma once
#include <cstdint>
#include <string>

#include "engine/state.h"

namespace ti {

enum class Arch : int8_t {
    Politician, Peacekeeper, Warlord, Betrayer, Swindler, Merchant, Opportunist, Rusher, Turtler, Chaotic, Count
};
enum class Evo : int8_t {
    LateBloomer, Vengeful, Mirror, Kingslayer, Ambitious, Learner, RisingDiplomat, BorderWatch, Scavenger, ThreeActs, Count
};
enum class Quirk : int8_t {
    Reckless, HolyDiver, ChessMaster, Kneecapper, TechHoarder, Collector, GrudgeKeeper, BargainHunter, Showboat,
    CautiousAdmiral, Spoiler, Homebody, Gambler, Count
};
enum class Betrayal : int8_t {
    BreakPromise, TradeAwayNote, Threaten, AttackWeakest, Embargo, VoteAgainst, RefuseOffers, TakePlanet, LoanScam,
    Rumor,
    Kingslay,  // 0.19.0 (owner): someone has sat on Mecatol Rex too long - take it from them (never rolled at random)
    Count
};

// The owner's price lists (trade goods) and B-Factor numbers per archetype.
struct ArchDef {
    const char* key;
    const char* name;
    const char* blurb;  // one line for the screens
    float tra, cea, sup, all, pol, comm, agent;
    float noteMult;     // faction notes: default value x this
    float bStart, bGain;
};
extern const ArchDef kArchetypes[int(Arch::Count)];

struct NamedText {
    const char* name;
    const char* text;
};
extern const NamedText kEvolutions[int(Evo::Count)];
extern const NamedText kQuirks[int(Quirk::Count)];
extern const NamedText kBetrayals[int(Betrayal::Count)];
// The two traits of a faction (index 0 / 1); {"", ""} for an unknown key.
NamedText factionQuirk(int faction, int which);

// Deterministic "random" number for personality decisions: hash of the game
// seed and up to four values, in [0, mod).
uint32_t prand(const Game& g, uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0, uint32_t mod = 100);

inline bool isBot(const Game& g, int p) { return p >= 0 && p < g.numPlayers && g.players[p].pers.arch >= 0; }
// The archetype a bot plays as right now (Chaotic: this round's pick); -1 for a human.
inline int archNow(const Game& g, int p) { return isBot(g, p) ? g.players[p].pers.now : -1; }
inline bool archIs(const Game& g, int p, Arch a) { return archNow(g, p) == int(a); }
inline bool evoIs(const Game& g, int p, Evo e) { return isBot(g, p) && g.players[p].pers.evo == int(e); }
inline bool quirkIs(const Game& g, int p, Quirk q) { return isBot(g, p) && g.players[p].pers.quirk == int(q); }
// Faction trait 'which' (0 / 1) of the faction with this key?
bool fquirkIs(const Game& g, int p, const char* factionKey, int which);

// Archetypes are hidden until the end of round 3 ("Our Enemies' Ambitions
// have been Revealed!").
inline bool archetypesRevealed(const Game& g) { return g.archRevealed; }
std::string archName(const Game& g, int p);  // "Warlord" (or "" for a human / unknown)

// How a bot feels about another player, for the "attitude" hint (after the reveal).
enum class Attitude : int8_t { Friendly, Wary, Hostile };
Attitude attitudeOf(const Game& g, int bot, int toward);
// The score behind it: about 15 = neutral, under 11 friendly, 33+ hostile.
float attitudeScore(const Game& g, int bot, int toward);
const char* attitudeName(Attitude a);

}  // namespace ti
