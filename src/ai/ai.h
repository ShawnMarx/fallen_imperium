// Computer players. Given the game state and a question, produce an answer.
// The AI always answers legally (every answer is checked by the rules); its
// strategy is a set of hand-written priorities, not a search.
#pragma once
#include "engine/decision.h"
#include "engine/rng.h"
#include "engine/state.h"

namespace ti {

Answer aiAnswer(const Game& g, const Decision& d, Rng& rng);

// Helpers that the UI also uses for "suggested" defaults.
Answer aiPayment(const Game& g, const Decision& d);
Answer aiHits(const Game& g, const Decision& d);
Answer aiTokens(const Game& g, const Decision& d);

// Negotiations, for the deal screens: the deal the AI would propose to q in
// p's place (countering = the offer being countered, or null); false = nothing.
bool aiSuggestDeal(const Game& g, int p, int q, const Deal* countering, Deal& out);
// The outcome of this agenda that seems best for p (a default for vote deals).
int aiFavouredOutcome(const Game& g, int p, int agenda);
// How computer player q would judge this deal: >= 0 means it would accept.
float aiDealMargin(const Game& g, int q, const Deal& d);

// Rough chance (0..1) that 'att' beats 'def' in a fight between these forces.
float fightOdds(const Game& g, int att, const Force& a, int def, const Force& d, bool space);

}  // namespace ti
