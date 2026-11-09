// Tempo scorecard for the simulator (0.25.0, TI4_TEMPO=1): how well the bots
// do the groundwork every good player does - take their slice by round 2,
// fight for the equidistant systems, set up the custodians, turn resources
// into units every round and score one public objective a round. Printed per
// difficulty, archetype and faction so the groundwork weights can be tuned.
#pragma once
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "engine/decision.h"
#include "engine/engine.h"
#include "engine/state.h"

namespace ti {

class TempoScore {
public:
    void reset() {  // before each game
        started_ = false;
        mecOwner_ = openSince_ = -1;
        mecShipsSeen_ = false;
    }
    // once per game, after setup (the galaxy is built)
    void startGame(const Game& g);
    // every answer: production spend
    void answered(const Game& g, const Decision& d, const Answer& a);
    // after every answer: round starts, end of the action phase
    void step(const Game& g);
    void endGame(const Game& g);
    // (0.53.0 Plan Ahead) the engine's events after each answer: combats - Styx, home systems, big fights
    void events(const Game& g, const std::vector<Event>& evs);
    void report(std::FILE* f) const;

private:
    struct Acc {
        double n = 0, own = 0, other = 0, none = 0;
    };
    struct Group {  // per archetype / faction / difficulty
        double seats = 0, wins = 0, vp4 = 0, vp6 = 0, vpEnd = 0, sliceOwn3 = 0, sliceN3 = 0, sliceNone3 = 0;
        double spent13 = 0, unspent = 0, unspentN = 0, pub = 0, pubRounds = 0, eq3 = 0;
    };
    // per game
    std::vector<int> slice_;  // per system: slice owner, -1 none, -2 equidistant
    std::vector<std::vector<int>> tied_;  // per system: players tied for an equidistant
    std::vector<int> near_;               // per system: moves from the nearest home
    int lastRound_ = 0;
    Phase lastPhase_ = Phase::Setup;
    bool started_ = false, mecatolTaken_ = false;
    std::vector<double> spent_;   // per player: resources spent on units in rounds 1-3
    std::vector<int> pubPrev_, secPrev_;
    std::vector<int> eqHeld3_;
    std::vector<int> vp4_, vp6_;
    std::vector<double> unspentSum_, unspentN_;
    std::vector<int> pubRounds_;  // status phases with a public scored
    std::vector<int> sliceOwn3_, sliceN3_, sliceNone3_;
    // totals
    int games_ = 0;
    Acc atR_[7];
    double eqOwned_[7] = {}, eqTotal_[7] = {}, eqTied_[7] = {};
    double zeroAt3_ = 0, seats_ = 0;
    double mecRound_ = 0, mecGames_ = 0, mecBy3_ = 0, mecBy4_ = 0;
    // 0.44.2 Mecatol watch, at the end of each round (index = round just over): ships there, held, held and
    // guarded (the holder's ships there or 2+ ground forces), left open (custodians gone, nobody guarding it)
    double mecShips_[10] = {}, mecHeld_[10] = {}, mecGuard_[10] = {}, mecOpen_[10] = {}, mecN_[10] = {};
    double mecFirstShips_ = 0, mecFirstShipsN_ = 0, mecChanges_ = 0, openSpells_ = 0, openRounds_ = 0;
    int mecOwner_ = -1, openSince_ = -1;
    bool mecShipsSeen_ = false;
    int lastTurnP_ = -1;
    double turnsR_[4] = {}, canLandR_[4] = {}, richR_[4] = {}, bothR_[4] = {};
    double stagedR_[5] = {}, stagedN_[5] = {};
    double saarN_[10] = {}, saarFF_[10] = {}, saarOut_[10] = {}, saarBall_[10] = {};  // Saar: factories, out of home, a ball (3+ ships)  // players with a loaded ship in or next to Mecatol Rex at the start of rounds 2-4  // custodians chances at turn starts, rounds 1-3
    double spentR_[10] = {}, spentRN_[10] = {}, producedR_[10] = {};
    double unspentR_[10] = {}, unspentRN_[10] = {};
    double pubR_[10] = {}, secR_[10] = {}, scoredPubR_[10] = {}, seenR_[10] = {};
    double vpAt_[11] = {}, vpAtN_[11] = {};
    double openBy_[7][4] = {}, openAnom_[7] = {}, sliceBy_[7][4] = {};  // unclaimed slice planets by moves from home (1, 2, 3+)
    double betrayGiven_ = 0, betrayDone_ = 0, betrayRounds_ = 0;
    std::vector<int> betrayFrom_, betrayCount_;  // per player: round the current betrayal began, completed count
    std::map<std::string, std::pair<double, double>> betrayByLevel_;  // given, completed
    std::vector<double> spentRound_;  // per player, this round
    std::map<std::string, Group> byArch_, byFaction_, byLevel_, bySeat_;
    // 0.53.0 (Plan Ahead): operations scorecard
    struct Fight {
        double n = 0, won = 0, ratio = 0;  // space combats started by a player, the attacker won, attacker / defender ships
    };
    Fight fAll_, fBig_, fHome_, fMecatol_, fStyx_, fNeutral_;  // big: the defender had 4+ non-fighter ships
    double styxGames_ = 0, styxAttacked_ = 0, styxTaken_ = 0, styxRound_ = 0;  // games with Styx on the board, ...
    double homeTaken_ = 0;  // home planets taken from their owner
    double styxFirst_ = 0;  // games where someone held Styx
    double styxAppear_ = 0;  // round the Fracture came into play (summed)
    int styxPl_ = -1, styxHolder_ = -1;
    bool bigNow_ = false;
    std::vector<int> homeLast_;  // per home planet: its owner last seen
    std::vector<int> threats_;   // 0.55.0: who could win at this round's scoring
    int threatRound_ = -1;
    double threatWon_ = 0, threatMissed_ = 0, threatRounds_ = 0, surpriseWins_ = 0;
    int combatSys_ = -1, combatAtt_ = -1, combatDef_ = -1;
    bool styxSeen_ = false, styxHit_ = false, styxOwned_ = false;
    std::vector<int> homeOwner_;  // per planet: the player whose home it is (-1 none)
    void roundStart(const Game& g);
    void actionPhaseOver(const Game& g);
    void printGroups(std::FILE* f, const char* title, const std::map<std::string, Group>& m) const;
};

}  // namespace ti
