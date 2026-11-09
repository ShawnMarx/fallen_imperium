// Thunder's Edge breakthroughs (0.34.0): one per faction, gained with the first expedition slice (or Thunder's Edge
// itself, Brilliance, The Quantumcore...). Each grants synergy between two technology colours (Nekro: none) and a
// faction ability. Names from the data set; the wording is our own. done = the ability is implemented
// (rules_breakthroughs.cpp); a breakthrough that isn't still gives its synergy.
#include "engine/breakthroughs.h"

#include <cstring>

namespace ti {

namespace {
constexpr TechColor B = TechColor::Blue, G_ = TechColor::Green, R = TechColor::Red, Y = TechColor::Yellow, N = TechColor::None;
const BreakthroughDef kBt[] = {
    {"arborec", "Psychospore", G_, R, "Action: exhaust it to take one of your command tokens back from a system with your infantry, "
                                     "then place 1 infantry there.", true},
    {"argent", "Wing Transfer", B, Y, "After you activate a system holding only your units: put command tokens from your reinforcements "
                                     "in adjacent systems holding only your units; at the end of the action, ships may move among those "
                                     "systems.", true},
    {"empyrean", "Void Tether", B, G_, "After you activate a system with or next to your units or planets: place or move a Void Tether on "
                                      "one of its borders - other players don't count those two systems as adjacent unless you allow it.",
     true},
    {"letnev", "Gravleash Maneuvers", B, R, "Before your space combat rolls: +X to one ship's roll, X = the ship types you have in the "
                                           "fight. Moving ships all use the best move value among the ships that started with them.", true},
    {"muaat", "Stellar Genesis", Y, R, "When you gain it: put the Avernus planet next to one of your planets (not a home system) and take "
                                      "it, ready. Your war suns leaving Avernus's system may take it along.", true},
    {"cabal", "Al'Raith Ix Ianovar", G_, R, "The Fracture enters play (no roll); move up to 2 ingress tokens into gravity rifts. Your "
                                           "ships starting in the Fracture get +1 move.", true},
    {"l1z1x", "Fealty Uplink", G_, R, "When you gain control of a planet, place infantry on it equal to its influence.", true},
    {"sardakk", "N'orr Supremacy", B, R, "After you win a combat: gain 1 command token, or research a unit upgrade.", true},
    {"sol", "Bellum Gloriosum", G_, Y, "When you produce a ship with capacity, you may also produce ground forces or fighters up to "
                                      "its capacity, outside your Production limit.", true},
    {"winnu", "Imperator", B, R, "+1 to your combat rolls for each Support for the Throne in your opponent's play area. After you "
                                "activate a system with a legendary planet, 1 of your ships moves 1 further.", true},
    {"yin", "Yin Ascendant", G_, Y, "When you gain it and when you score a public objective: gain the Alliance ability of a random "
                                   "faction nobody plays.", true},
    {"ghost", "Particle Synthesis", B, Y, "Each wormhole in a system with your ships has Production 1. Units you produce in a "
                                          "system with wormholes cost 1 less per wormhole.", true},
    {"hacan", "Auto-Factories", Y, R, "When you produce 3 or more non-fighter ships, put 1 command token from your reinforcements in "
                                     "your fleet pool.", true},
    {"mahact", "Vaults of the Heir", G_, Y, "Action: exhaust it and purge one of your technologies to gain a relic.", true},
    {"mentak", "The Table's Grace", G_, Y, "With Cruiser II: your cruisers become Corsairs (combat 6, move 3, capacity 2) and may "
                                          "move through other players' ships when the active system holds other players' non-fighter "
                                          "ships.", true},
    {"nekro", "Valefar Assimilator Z", N, N, "When one of your faction abilities would give you another player's technology, you may "
                                            "instead give your flagship that faction's flagship abilities.", true},
    {"titans", "Slumberstate Computing", G_, Y, "When Coalescence would start a ground combat and you commit nothing else, your units "
                                               "may coexist instead. Status phase: you and each player you coexist with draw 1 "
                                               "more action card.", true},
    {"xxcha", "Archon's Gift", G_, Y, "You may spend influence as resources and resources as influence.", true},
    {"nomad", "Thunder's Paradox", G_, Y, "At the start of any player's turn: exhaust 1 of your agents to ready another agent.", true},
    {"jolnar", "Specialist Compounds", G_, Y, "When you research with the Technology card: exhaust a planet with a technology "
                                             "specialty instead of spending resources, and research a technology of that colour.",
     true},
    {"naalu", "Mindsieve", G_, R, "When you would use the secondary of another player's strategy card: give them a promissory note "
                                 "instead of spending a command token.", true},
    {"saar", "Deorbit Barrage", B, R, "Action: exhaust it and spend resources: on a planet up to 2 systems from an asteroid field "
                                     "with your ships, roll that many dice; each 4 or more is a hit on a ground force there.", true},
    {"yssaril", "Deepgloom Executable", G_, Y, "Other players may ask to use your Stall Tactics; when you allow it, you may "
                                              "trade with them afterwards.", true},
    {"keleres", "I.I.H.Q. Modernization", G_, Y, "When you gain it: take the Custodia Vigilia planet and its legendary ability. You are "
                                                "neighbours of everyone with units or planets in or next to Mecatol Rex.", true},
    {"naaz", "Absolute Synergy", B, G_, "With 4 mechs in one system, return 3 of them to flip this onto your mech card: Eidolon "
                                       "Maximum (combat 4 x4, move 3, sustain damage; a ship and a ground force).", true},
    // Thunder's Edge factions (part 4)
    {"bastion", "The Icon", Y, R, "When you produce ships: exhaust it to place them in a system with your command token, your ground "
                                 "forces and no other player's ships.", true},
    {"crimson", "Resonance Generator", B, R, "Your ships starting in your home system or in a system with an active breach get +1 "
                                            "move. Action: exhaust it to flip a breach, or place an active breach where you have units.",
     true},
    {"firmament", "The Sowing", G_, Y, "When you gain it and at the start of the status phase: place up to 3 of your trade goods on it. "
                                      "(The Obsidian: The Reaping.)", true},
    {"obsidian", "The Reaping", G_, Y, "Each time you win a combat against a puppeted player, put 1 trade good on it; at the start of "
                                      "the status phase, take them and as many again.", true},
    {"deepwrought", "Visionaria Select", G_, Y, "Action: exhaust it - every other player may spend 3 trade goods and give you a "
                                               "promissory note to research a generic technology; you gain each one researched.", true},
    {"ralnel", "Data Skimmer", G_, Y, "In the action phase, before you pass: other players' discarded action cards go onto this card. "
                                     "When you pass, keep 1 of them and discard the rest.", true},
    // Discordant Stars (0.43.0, owner D1: with Thunder's Edge too) - done is set as each is implemented (rules_dsbt.cpp)
    {"axis", "Arms Brokerage", B, R, "At the start of another player's turn, exhaust it: that player uses 1 of your unit upgrades as their own "
                                    "this turn. After you refill commodities, gain a trade good per unit upgrade you own.", true},
    {"bentor", "Historian Conclave", B, G_, "Action: exhaust it - each other player explores 1 of their planets; you may put 1 infantry, "
                                           "coexisting, on one of those planets. Exploring a planet, you may exhaust it to explore the "
                                           "frontier deck in that system instead.", true},
    {"celdauri", "Trade Protectorate", Y, R, "When you gain it: a second flagship, the Celagrom token, goes in a system with your ships.", true},
    {"cheiran", "Matriphagy", B, R, "End of your turn: exhaust it to swap one of your non-fighter ships for units from your reinforcements "
                                   "costing less in total.", true},
    {"cymiae", "Synaptic Interference", Y, R, "After you win a combat: purge a promissory note from your hand to push another player's "
                                             "command token out of the active system, or gain 2 trade goods. Action: exhaust it to discard "
                                             "the top 3 action cards.", true},
    {"dihmohn", "Exodus Engineering", B, Y, "Once per space combat, after an opponent's non-fighter ship is destroyed: exhaust it to put a "
                                           "frontier token there - or produce 1 or 2 non-fighter ships there (beside the enemy's ships), which readies it.", true},
    {"edyn", "Celestial Ambassadors", G_, B, "Action: exhaust it and show another player one of your unscored secrets - they show you one "
                                            "of theirs at random, or let you put 1 ground force, coexisting, on one of their planets.", true},
    {"freesystems", "Galactic Movement", G_, Y, "After you spend a strategy token: put 1 infantry, coexisting, on a cultural, hazardous or "
                                               "industrial planet with the active player's ground forces.", true},
    {"ghemina", "Raiding Parties", G_, R, "Each combat round one of your units rolls twice and keeps the better roll. Announcing a retreat, "
                                         "exhaust it to retreat at once.", true},
    {"ghoti", "Cult of the All-Mother", G_, B, "When you pass, exhaust it: move an ingress token into an empty system, or one of your ships "
                                              "next door to your ships. Your units may retreat into any empty system.", true},
    {"gledge", "Dhonraz Installations", Y, B, "Your planets with Core tokens also count as space stations (not for control). Your space "
                                             "stations have Space Cannon 5 (offence, in their own system).", true},
    {"augers", "Cryptic Insights", Y, G_, "When you explore a planet, you may discard the card to draw 1 action card instead.", true},
    {"kjalengard", "Bannerhalls", R, G_, "A Bannerhall for each Glory token on the board. Once per action, when anyone wins a combat where "
                                        "a Glory token is, every Bannerhall readies. (A Bannerhall: a planet card worth 1 resource or 1 influence, not for votes.)", true},
    {"kollecc", "The Collector's Museum", B, G_, "When you explore a planet: exhaust it to resolve a card from that deck's discard pile "
                                                "instead; it is shuffled back into the deck afterwards.", true},
    {"kolume", "Synchronicity VI", B, Y, "Action: exhaust it to gain 1 command token, or research a technology of the colour of one of your "
                                        "exhausted ones. Each time you exhaust a technology, gain 1 commodity.", true},
    {"florzen", "Reverie Implants", B, G_, "End of your turn: exhaust it - you and another player each secretly spend 0, 1 or 2 trade goods; "
                                          "if the amounts differ, take a random promissory note from them or explore one of your planets.", true},
    {"kortali", "The Queens' Wrath", B, R, "With Dreadnought II: flip it - your dreadnoughts become Tribunes (move 2, capacity 1, combat 5, "
                                          "bombardment 5, sustain damage; they repair when they hit, reroll while undamaged).", true},
    {"kyro", "Strain Dispersal", Y, R, "Researching: discard an action card to ignore 1 prerequisite. Once per action, winning a combat: a "
                                      "commodity (or a commodity into a trade good) for each technology the loser has and you don't.", true},
    {"lanefir", "Erasure Corps", G_, R, "When another player purges components (not action cards): gain 1 command token or ready it. In the "
                                       "action phase: exhaust it to explore one of your planets.", true},
    {"lizho", "Professional Intrigue", G_, R, "After a tactical action in a non-home system: move 1 of your infantry there, coexisting, onto "
                                             "a planet. Whoever ends your coexistence discards an action card.", true},
    {"khrask", "Mended Grove", G_, Y, "Meteor Slings: when every hit is cancelled, your infantry may coexist instead (not committed this "
                                     "action). The Grove: a planet card worth 2 resources.", true},
    {"mirveda", "Stabilization Arrays", B, Y, "With Fighter II: flip it - your fighters become the Eye of the Curator (move 3, combat 8, "
                                             "they move without carriers and take half a slot of capacity each).", true},
    {"mortheus", "Mirage Craft", R, Y, "Defending a space combat: swap one of your ships there with one of yours in a system with your "
                                      "command token (the swapped ships carry nothing).", true},
    {"mykomentori", "Dreamwalkers", G_, Y, "Other players' infantry destroyed in combat on your planets are captured. After each ground "
                                          "combat round on your planet, roll: 7+ puts a captured infantry there as yours.", true},
    {"nivyn", "Anomaly Stabilization", B, R, "The Wound's system is also an entropic scar; you ignore entropic scars, and they count as "
                                            "ingress tokens for you.", true},
    {"nokar", "Mercenary Captains", Y, R, "Action: exhaust it to put 1 neutral cruiser and 2 neutral destroyers in a system (not the "
                                        "Fracture) without other ships. When another player wins a space combat where your ships are: "
                                        "gain 2 commodities or 1 command token.", true},
    {"olradin", "Insurrectionist Networking", G_, R, "Once per action, after a policy ability: spend 1 trade good to move 1 of your "
                                                    "infantry, coexisting, onto a planet next door. When you pass: purge it to invade every "
                                                    "planet with your coexisting infantry.", true},
    {"zealots", "Rhodun's Reliquary", Y, R, "After you take a legendary or Fracture planet: research 1 technology, or take back 1 of your "
                                           "command tokens in that system.", true},
    {"rohdhna", "The Prodigy's Triumph", Y, B, "With Terrafactory II: flip it - your war suns become Omni-Forgeworlds (production 7, "
                                              "capacity 8, combat 3 x3).", true},
    {"tnelis", "Hidden Hardpoints", R, B, "End of a ground combat round: discard a secret objective - your units there coexist instead. "
                                         "Whenever someone ends your coexistence, draw a secret objective.", true},
    {"vaden", "High Security Vault", R, G_, "Nobody takes your trade goods, commodities or relic fragments without your leave. When another "
                                           "player gives back one of your promissory notes, 1 of their control tokens goes on your sheet.", true},
    {"vaylerian", "Gravitic Disruption", R, B, "After moving ships into the active system: discard an action card to move one of them on to "
                                              "a system next door without other players' ships.", true},
    {"veldyr", "Aurora Research Base", Y, B, "Your home system is also a nebula. When you pass, explore the frontier deck in your home "
                                            "system. Their space cannon hits in a nebula can't be cancelled.", true},
    {"zelian", "Awakened Planetoids", G_, R, "Your planets in the active system fight beside your ships (space) and ground forces (ground): "
                                            "combat 10 minus their resources (at least 1); they can't be hit.", true},
};
}  // namespace

const BreakthroughDef* breakthroughOf(const char* factionKey) {
    if (!factionKey) return nullptr;
    for (const BreakthroughDef& b : kBt)
        if (!std::strcmp(b.faction, factionKey)) return &b;
    return nullptr;
}

}  // namespace ti
