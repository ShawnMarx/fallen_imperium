// Faction abilities, leaders, faction promissory notes and special unit
// abilities - in our own short wording (never the printed card text).
// Factions are switched on in batches (Stage 5); factionImplemented() says
// which ones work so far.
#include <cstring>
#include <vector>

#include "engine/data.h"
#include "engine/state.h"

namespace ti {

namespace {

struct KT {
    const char* key;
    const char* text;
};

const char* find(const KT* t, int n, const char* key) {
    for (int i = 0; i < n; ++i)
        if (!std::strcmp(t[i].key, key)) return t[i].text;
    return "";
}
#define FIND(table, key) find(table, int(sizeof(table) / sizeof(table[0])), key)

// Factions whose abilities work (Stage 5 batches).
const char* kDone[] = {"sol", "letnev", "hacan", "jolnar", "sardakk", "xxcha",                // batch 1 (0.6.0)
                       "arborec", "l1z1x", "mentak", "naalu", "yin", "yssaril",             // batch 2 (0.7.0)
                       "ghost", "muaat", "saar", "winnu", "nekro", "empyrean",              // batch 3 (0.8.0)
                       "argent", "mahact", "naaz", "nomad", "titans", "cabal",              // batch 4 (0.9.0)
                       "keleres",                                                           // batch 5 (0.21.0)
                       "bastion", "ralnel", "deepwrought", "crimson", "firmament", "obsidian",  // batch 6 (0.35.0, Thunder's Edge)
                       "kortali", "vaylerian", "lanefir", "kyro", "cheiran", "celdauri",        // Discordant Stars batch 1 (0.37.0)
                       "khrask", "zealots", "zelian", "rohdhna", "tnelis", "kolume",           // Discordant Stars batch 2 (0.38.0)
                       "bentor", "dihmohn", "florzen", "gledge", "augers", "kjalengard",       // Discordant Stars batch 3 (0.39.0)
                       "kollecc", "mortheus", "vaden", "cymiae", "nivyn", "olradin",           // Discordant Stars batch 4 (0.40.0)
                       "freesystems", "edyn", "nokar", "axis", "mykomentori", "ghemina",      // Discordant Stars batch 5 (0.41.0)
                       "veldyr", "lizho", "mirveda", "ghoti"};                                // Discordant Stars batch 6 (0.42.0)

// Faction abilities, keyed by their name.
const KT kAbility[] = {
    // Federation of Sol
    {"Orbital Drop", "Action: spend 1 strategy token to put 2 infantry from your reinforcements on a planet you control."},
    {"Versatile", "Each status phase you gain 1 extra command token."},
    // Barony of Letnev
    {"Munitions Reserves", "At the start of each space combat round you may pay 2 trade goods to reroll any of your dice that round."},
    {"Armada", "You may have 2 more non-fighter ships in each system than your fleet pool tokens."},
    // Emirates of Hacan
    {"Masters of Trade", "Using the secondary ability of Trade costs you no command token."},
    {"Guild Ships", "You may trade with players who aren't your neighbours."},
    {"Arbiters", "Action cards can be part of your trades (yours, and the other player's cards in a trade with you)."},
    // Universities of Jol-Nar
    {"Fragile", "Your units' combat rolls get -1."},
    {"Brilliant", "When you follow Technology (secondary), you may resolve its primary ability instead."},
    {"Analytical", "When you research a technology that isn't a unit upgrade, you may ignore 1 prerequisite."},
    // Sardakk N'orr
    {"Unrelenting", "Your units' combat rolls get +1."},
    // Xxcha Kingdom
    {"Peace Accords", "After you resolve either ability of Diplomacy, you may take an empty planet (not Mecatol Rex or a space station) next to one of your planets."},
    {"Quash", "When an agenda is revealed, you may spend 1 strategy token to discard it; the next agenda is voted on instead."},
    // Arborec
    {"Mitosis", "Your space docks can't produce infantry. At the start of the status phase, put 1 infantry from your reinforcements on a planet you control."},
    // L1Z1X Mindnet
    {"Assimilate", "When you take a planet, its PDS and space docks become yours instead of being destroyed."},
    {"Harrow", "After each round of ground combat, your ships in the active system may bombard your opponent's ground forces on that planet."},
    // Mentak Coalition
    {"Ambush", "At the start of a space combat, up to 2 of your cruisers / destroyers there roll 1 die each; each hit must be assigned by your opponent."},
    {"Pillage", "After a neighbour gains trade goods and then has 3 or more, you may take 1 of their trade goods or commodities."},
    // Naalu Collective
    {"Telepathic", "At the end of the strategy phase you take the Naalu \"0\" token: you go first in the action phase."},
    {"Foresight", "After another player moves ships into a system with your ships, you may spend 1 strategy token to move your ships from there to an adjacent system without other players' ships."},
    // Yin Brotherhood
    {"Indoctrination", "At the start of a ground combat, you may spend 2 influence to swap 1 of your opponent's infantry there for 1 of yours."},
    {"Devotion", "After each space combat round, you may destroy 1 of your cruisers or destroyers there to make 1 hit against your opponent's ships."},
    // Yssaril Tribes
    {"Stall Tactics", "Action: discard 1 action card."},
    {"Scheming", "When you draw action cards, draw 1 extra, then discard 1 card from your hand."},
    {"Crafty", "You can hold any number of action cards."},
    // Ghosts of Creuss
    {"Quantum Entanglement", "For you, every system with an alpha or beta wormhole is adjacent to every other one. Nothing can stop this."},
    {"Slipstream", "During your tactical actions, your ships that start at home or in a system with an alpha or beta wormhole get +1 move."},
    {"Creuss Gate", "The Creuss Gate stands where your home system would be; your home system is off the board, linked to it by delta wormholes."},
    // Embers of Muaat
    {"Star Forge", "Action: spend 1 strategy token to put 2 fighters or 1 destroyer in a system with your war sun."},
    {"Gashlai Physiology", "Your ships can move through supernovas."},
    // Clan of Saar
    {"Scavenge", "After you gain control of a planet, gain 1 trade good."},
    {"Nomadic", "You can score objectives even without the planets of your home system."},
    // Winnu
    {"Blood Ties", "Removing the custodians token from Mecatol Rex costs you no influence."},
    {"Reclamation", "After a tactical action in which you took Mecatol Rex, you may put 1 PDS and 1 space dock there."},
    // Nekro Virus
    {"Galactic Threat", "You can't vote. Once per agenda phase, after an agenda is revealed, you may predict its outcome; if right, gain a technology of a player who voted for it."},
    {"Technological Singularity", "Once per combat, after one of your opponent's units is destroyed, you may gain one of their technologies."},
    {"Propagation", "You can't research. Whenever you would research a technology, gain 3 command tokens instead."},
    // Empyrean
    {"Voidborn", "Nebulae don't affect your ships' movement."},
    {"Aetherpassage", "After another player activates a system, you may let them move ships through systems with your ships."},
    {"Dark Whispers", "You have 2 faction promissory notes: Dark Pact and Blood Pact."},
    // Argent Flight
    {"Zeal", "You always vote first in the agenda phase. When you cast at least 1 vote, cast 1 more vote for each player in the game (you included)."},
    {"Raid Formation", "When your units use anti-fighter barrage, each hit beyond your opponent's fighters damages one of their ships that has Sustain Damage (your choice)."},
    // Mahact Gene-Sorcerers
    {"Edict", "When you win a combat, put 1 of your opponent's command tokens from their reinforcements in your fleet pool (1 per player). They add to your fleet limit but can't be moved."},
    {"Imperia", "While another player's command token is in your fleet pool, you can use their commander once it is unlocked."},
    {"Hubris", "You have no Alliance note, and nobody can give you theirs."},
    // Naaz-Rokha Alliance
    {"Distant Suns", "When you explore a planet with one of your mechs, you may draw 2 cards instead of 1 and resolve the one you choose."},
    {"Fabrication", "Action: purge 2 relic fragments of the same type to gain a relic, or purge 1 fragment to gain 1 command token."},
    // Nomad
    {"The Company", "You have 3 agents: Field Marshal Mercer, The Thundarian and Artuno the Betrayer."},
    {"Future Sight", "In the agenda phase, after an outcome you voted for or predicted is resolved, gain 1 trade good."},
    // Titans of Ul
    {"Terragenesis", "After you explore a planet without a sleeper token, you may put a sleeper token there or move one there (5 at most)."},
    {"Awaken", "After you activate a system with your sleeper tokens, you may replace each of them with a PDS."},
    {"Coalescence", "If your flagship or Awaken puts your units where another player's units are, they must fight in the space / ground combat."},
    // Vuil'raith Cabal
    {"Devour", "Your opponent's units (not structures) destroyed in combat against you are captured."},
    {"Amalgamation", "When you produce a unit, you may give back 1 captured unit of that type to produce it for free."},
    {"Riftmeld", "When you research a unit upgrade, you may give back 1 captured unit of that type to ignore all its prerequisites."},
    // Council Keleres (Thunder's Edge version)
    {"The Tribunii", "At setup you take the home system of the Mentak, the Xxcha or the Argent Flight - one that isn't in the game - and the Keleres hero that goes with it."},
    {"Council Patronage", "At the start of the strategy phase, refill your commodities, then gain 1 trade good."},
    {"Law's Order", "At the start of any player's turn you may pay 1 trade good or 1 commodity: every law is blank until that turn ends."},
    // Thunder's Edge factions (0.35.0)
    {"Liberate", "When you gain control of a planet: ready it if it holds at least as many of your infantry as its resources; if not, put 1 infantry on it."},
    {"Galvanize", "A galvanized unit rolls 1 more die in combat. It keeps the token while it moves with your fleet; galvanized units are the last of a group to be destroyed."},
    {"Phoenix Standard", "At the end of a combat: you may galvanize 1 of your units that took part."},
    {"Survival Instinct", "After another player activates a system with your ships: move up to 2 of your ships into it from adjacent systems without your command tokens."},
    {"Miniaturization", "Any of your ships can carry your PDS and space docks, without using capacity (in space they do nothing). At the end of your tactical action, put them on planets you control in their systems."},
    {"Research Team", "When ground forces are committed to a planet where you and another player both have ground forces: you may coexist there instead of fighting."},
    {"Oceanbound", "When your units start coexisting on a planet: gain an ocean card, ready (1 resource or 1 influence, spent like a planet). You keep no more ocean cards than planets where your units coexist."},
    {"Sundered", "You can use only epsilon wormholes. Other players' units that move into your home system are destroyed."},
    {"Incursion", "When you activate a system with a breach, you may flip it. Systems with active breaches are adjacent to each other. At the end of the status phase, a player with ships where an active breach is may remove it."},
    {"Sorrow", "Your home system sits beside the board; the Sorrow takes its place on the board, with an inactive breach. The two are joined by their epsilon wormholes."},
    {"Plots Within Plots", "You may score secret objectives other players have already scored, if you meet them (outside your own secret limits). You gain no point: a facedown plot card with that player's control token goes into your play area instead."},
    {"Puppets of the Blade", "While you have a plot card in play - Action: become the Obsidian for good. Your faction sheet, leaders, planets and note are replaced by the Obsidian's."},
    {"Nocturne", "Nobody picks this faction at setup: the Firmament turn into it."},
    {"The Blade's Orchestra", "When the Obsidian come into play: your home system, double-sided cards and plot cards turn over; Cronos Hollow and Tallin Hollow ready."},
    {"Marionettes", "The players whose control tokens are on a plot card are that plot's puppets: the plot works against them."},
    // Discordant Stars (0.37.0) - Kortali Tribunal
    {"Zealous", "When you spend a command token on Warfare's secondary ability, you may resolve its primary ability instead."},
    {"Ruthless", "At the start of a ground combat on an exhausted planet you don't control: 1 hit on one of your opponent's ground forces there."},
    // Vaylerian Scourge
    {"Cargo Raiders", "In the first round of a space combat, your opponent can only announce a retreat by spending 1 trade good."},
    {"Scour", "Once per tactical action, after you gain control of a planet: discard 1 action card to ready it."},
    {"Raze", "After another player's structures are destroyed on a planet where you have units: you may refill your commodities."},
    // Lanefir Remnants
    {"A New Edifice", "You can't use technology specialties. When you would gain a relic, purge it instead and explore up to 3 of your planets."},
    {"Iconoclasm", "Researching a technology that isn't a unit upgrade, you may purge 1 relic fragment to ignore 1 prerequisite."},
    {"War Stories", "Once per action, after you win a combat: explore one of your planets - or, if the system has no planets, its frontier token."},
    // Kyro Sodality
    {"Contagion", "After you resolve either ability of Politics: 1 infantry from your reinforcements invades a planet in or next to a system with one of your planets."},
    {"Plague Reservoir", "Once per action, invading a planet with your units, you may fight a ground combat there even without enemy ground forces. You win it (Dacxive Animators: 1 more infantry)."},
    {"Subversive", "A combat you take part in that would end in a draw counts as won by you."},
    // Cheiran Hordes
    {"Teeming", "You have 2 more dreadnoughts and 1 more mech in your reinforcements."},
    {"Moult", "After you win a space combat as the defender: produce 1 ship there, 1 cheaper for each of your non-fighter ships destroyed in it."},
    {"Byssus", "Your mechs on planets you control count as structures for your own abilities (not for objectives)."},
    // Celdauri Trade Confederation
    {"Projection of Power", "At the start of a space combat in or next to a system with your space dock, one of your ships there gains Anti-Fighter Barrage 6."},
    {"Industrialists", "You have 1 more space dock in your reinforcements."},
    // Discordant Stars batch 2 (0.38.0) - L'tokk Khrask
    {"Lithoids", "In the agenda phase your planets cast votes equal to their resources instead of their influence."},
    {"Garden Worlds", "Your planets with no ground forces on them have 1 more resource."},
    {"Meteor Slings", "Bombarding another player's planet, you may turn any of your hits into infantry from your reinforcements landing there (they fight in the ground combat)."},
    // Zealots of Rhodun
    {"Conspirators", "Once per agenda phase, after an agenda is revealed and if you aren't the speaker: you may vote after the speaker."},
    {"Ancient Knowledge", "Each technology specialty you use ignores 1 more prerequisite of its colour, and exhausting a planet for its specialty gives you 1 commodity."},
    // Zelian Purifier
    {"Obsessive Designs", "In the action phase, after you research a unit upgrade: produce units of that type at a space dock in your home system, 2 cheaper in all."},
    {"Biophobic", "In the agenda phase each planet you exhaust casts exactly 1 vote."},
    {"Paranoia", "Nothing but your own command tokens can stop you activating, or moving ships into, your home system."},
    // Roh'Dhna Mechatronics
    {"Industrious", "After you place a space dock in a system without other players' ships: pay 6 resources to replace it with a war sun in space."},
    {"Recycled Materials", "After you activate a system: return a cruiser, carrier or dreadnought of yours there to your reinforcements for trade goods equal to its cost minus 1."},
    {"Orbital Foundries", "For objectives, each of your war suns counts as a structure on one of your planets."},
    // Tnelis Syndicate
    {"Plausible Deniability", "When you draw secret objectives, draw 1 more, then shuffle 1 of yours back into the deck."},
    {"Information Brokers", "You may hold 1 more unscored secret objective."},
    {"Stealth Insertion", "Your units put onto a planet with another player's units must fight in its ground combat."},
    // Monks of Kolume
    {"Starfall Gunnery", "On your own actions up to 3 of your non-fighter ships have Space Cannon 8; on other players' actions only 1 of your units fires its space cannon."},
    {"Deliberate Action", "You can't rearrange your command tokens in the status phase. When you pass, you may put 1 token from your reinforcements into an empty pool."},
    {"Meditation", "Action: spend 1 strategy token to ready one of your technologies."},
    // Bentor Conglomerate
    {"Secret Maps", "At the end of your tactical action: you may explore 1 planet in the active system that has your unit with Production (or is one) and that you didn't explore during it."},
    {"Ancient Blueprints", "The first time you gain a cultural, hazardous, industrial and unknown relic fragment, a Fragment token of that kind goes on your faction sheet."},
    {"Fortune Seekers", "Once per action, after you explore a planet or a frontier token: gain 1 commodity."},
    // Dih-Mohn Flotilla
    {"Capital Fleet", "Your destroyers count as half a ship against your fleet pool."},
    {"Migrant Fleet", "After you explore a frontier token: you may explore one of your planets in a system next to it."},
    {"Flotilla", "You can't have more infantry than non-fighter ships in the space area of a system."},
    // Florzen Profiteers
    {"Mercenaries", "At the start of a space combat: you may remove up to 2 of your fighters from a system next to it; a player in the combat places that many fighters there."},
    {"Data Leak", "When you would gain a relic: draw 1 more, keep 1 and shuffle the other back into the relic deck."},
    {"Shadow Markets", "When you explore a planet, you may use the deck of a trait one of your other planets has."},
    // Gledge Union
    {"Mantle Cracking", "Action: put a Core token (3 in all) on one of your non-home planets other than Mecatol Rex and gain 4 trade goods."},
    {"Celestial Reclamation", "A planet with a Core token has base values of 2 resources and 0 influence; the token never leaves it."},
    {"Deep Mining", "When you would explore a planet with your mech or structure on it, you may gain 1 trade good instead."},
    // Augurs of Ilyxum
    {"Oracle AI", "After the speaker reveals a public objective, the next unrevealed one goes on your faction sheet - you may look at it."},
    {"Limited Vision", "A stage II objective can go on your sheet only once all stage I objectives are revealed."},
    {"Probability Algorithms", "The objective on your faction sheet is the next public objective the speaker reveals."},
    // Berserkers of Kjalengard
    {"Heroism", "Your fighters and infantry destroyed in combat are captured. When you pass: for each Glory token on the board, you may return 4 captured units for 1 command token."},
    {"Glory", "After you win a combat: place or move a Glory token (3 in all) into that system - or spend a strategy token to research a unit upgrade of a unit type that fought."},
    {"Valor", "In a system with a Glory token, each natural 10 on a combat roll makes 1 more hit."},
    // Kollecc Society
    {"Cloaked Fleets", "After you produce ships, up to 2 of them may go onto your faction sheet (captured - cloaked)."},
    {"Shroud of Lith", "After you move into a system with your non-fighter ships: up to 2 cloaked ships and 2 cloaked ground forces appear there."},
    {"Treasure Hunters", "At the start of your turn you may look at the top card of a cultural, hazardous or industrial deck, and at the top relic."},
    // Glimmer of Mortheus
    {"Facsimile", "At the start of a space combat you defend: pay influence equal to the cost of one of the attacker's ships to put one of that type in."},
    {"Illusory Presence", "In the agenda phase, after another player votes: exhaust up to 2 planets - they cast 1 vote more than those planets' influence."},
    // Vaden Banking Clans
    {"Fine Print", "After a player resolves the secondary of your strategy card: 1 of their control tokens goes on your faction sheet."},
    {"Collateralized Loans", "After an opponent's ship is destroyed in space combat: take back 1 of their tokens to put a ship of that type in the system."},
    {"Binding Debts", "At the start of the status phase each neighbour may pay you 1 trade good to take back up to 2 of their tokens."},
    // Savages of Cymiae
    {"Autonetic Memory", "When you draw action cards, draw 1 less: take 1 from the discard pile instead, or put 1 infantry on one of your planets."},
    {"Cybernetic Madness", "After taking a card from the discard pile, discard 1. Your resolved action cards are purged, not discarded."},
    // Nivyn Star Kings
    {"Celestial Guides", "Your units don't roll for gravity rifts; anomalies in or next to systems with your structures don't slow you."},
    {"Singularity Point", "The system with the Wound token is both a nebula and a gravity rift."},
    {"Voidsailors", "When you explore a frontier token, draw 1 more card, resolve one and shuffle the other back."},
    // Olradin League
    {"Policies", "Three policy cards, each on its + or - side (chosen at the start): The People, The Environment, The Economy."},
    // Free Systems Compact
    {"Rally to the Cause", "Once per action, after you produce ships at home: produce up to 2 ships in a system with a cultural, hazardous or industrial planet, no legendary planet and nobody else's units."},
    {"Diplomats", "Once per action, when you pay: exhaust the card of a planet nobody controls (not Mecatol Rex) for its resources or influence."},
    {"Free People", "The cards of the planets nobody controls lie face up on the board, ready for your Diplomats."},
    // Edyn Mandate
    {"Grace", "Once per action phase, after a strategy card's primary: resolve the secondary of a lower card nobody has used yet, without a command token."},
    {"Decree", "Other players' ships can't move through anomalies with your ground forces in them - your Sigils included."},
    {"Radiance", "After an agenda is revealed you may predict its outcome; if you're right, another player's command token goes into one of your Sigils."},
    // Nokar Sellships
    {"Hired Guns", "When another player activates a system with your ships: offer them up to 3 for 1 trade good each - they fight as theirs and may be kept at the end of the action."},
    {"Private Fleet", "4 more destroyers in your reinforcements."},
    {"Desperados", "When you produce, 1 destroyer doesn't count against the limit. Your destroyers move 1 further when you aren't the active player."},
    // Shipwrights of Axis
    {"Military Industrial Complex", "After you gain or refill commodities: spend them to put Axis Order cards in your play area. You can't give commodities in transactions."},
    {"Arms Dealers", "Axis Order cards change hands in deals; their holders call in the ships at the end of their turn. You can't use them yourself."},
    // Myko-Mentori
    {"Prescient Memories", "At the start of each strategy phase, roll your 4 Omen dice."},
    {"Divination", "Before you roll a die you may spend an Omen die instead: the roll takes its result."},
    {"Necrophage", "+1 commodity value for each space dock you have. After the first round of a combat: gain 1 commodity or turn 1 into a trade good."},
    // Ghemina Raiders
    {"The Lady & The Lord", "Two flagships - the Lady and the Lord - and two heroes."},
    {"Rule of Two", "In a space combat with exactly 2 of your non-fighter ships, both of the same type: +2 to their rolls."},
    // Veldyr Sovereignty
    {"Corporate Entity", "You have 4 faction promissory notes: the Branch Offices. They don't count toward objectives."},
    {"Holding Company", "At the start of the status phase, for each planet with a Branch Office: gain 1 commodity or turn 1 into a trade good."},
    {"Targeted Acquisition", "At the start of the status phase you may give one of your Branch Offices to a neighbour."},
    // Li-Zho Dynasty
    {"Cunning", "After your tactical action in a system: attach a Trap from your reinforcements to a planet there with your infantry."},
    {"Subterfuge", "Traps lie face down - only you know which is which. They don't count toward objectives."},
    // Mirveda Protectorate
    {"Privileged Citizenry", "Each system holds as many of your PDS as you have fleet tokens; for objectives your PDS count as structures on your planets."},
    {"Combat Drones", "Your space docks can't produce infantry. At the start of an invasion your fighters there may become infantry (no capacity needed during it)."},
    // Ghoti Wayfarers
    {"Abyssal Embrace", "Your home tile is Ghoti Space - an empty system."},
    {"Mobile Command", "The system with your flagship is your home system. While your flagship is off the board you can't score public objectives."},
    {"Spawning Grounds", "You hold the Ghoti planet card (3/3) - it isn't on the board and can't be lost. Ships moving into a system without planets: 1 commodity."},
};

// Leaders, keyed by leader key: what they do.
const KT kLeaderText[] = {
    // Council Keleres
    {"keleresagent", "Any time: exhaust so that a player (you, or another) can pay with commodities as if they were trade goods - for one payment."},
    {"kelerescommander", "After you perform a component action, you may perform one more action."},
    {"keleresheroharka", "Action: turn over action cards until 3 with component actions appear; take those 3 and shuffle the rest back. Then purge this card."},
    {"keleresheroodlynn", "After an agenda is revealed: cast up to 6 more votes on it and predict an outcome; for each player who abstains or votes for another outcome, gain 1 trade good and 1 command token. Then purge this card."},
    {"keleresherokuuasi", "At the start of a space combat round in a system with a planet you control: put your flagship and up to 2 cruisers or destroyers from your reinforcements into it. Then purge this card."},
    {"solagent", "At the start of a ground combat round: exhaust to give 1 ground force in the active system an extra die this round."},
    {"solcommander", "At the start of a ground combat on a planet you control, you may place 1 infantry from your reinforcements there."},
    {"solhero", "Action: return all your command tokens on the board to your reinforcements. Then purge this card."},
    {"letnevagent", "At the start of a space combat round: exhaust to give 1 ship in the active system an extra die this round."},
    {"letnevcommander", "After one of your units uses Sustain Damage, you may gain 1 trade good."},
    {"letnevhero", "Action: for the rest of this round, laws and your fleet pool don't limit how many non-fighter ships you have in a system. Purged at the end of the round."},
    {"hacanagent", "During the action phase: exhaust to gain 2 commodities, or to refill another player's commodities."},
    {"hacancommander", "When you vote, you may spend trade goods: each one adds 2 votes."},
    {"hacanhero", "When your units produce: you may make every unit free this time. Then purge this card."},
    {"jolnaragent", "When a player pays resources to research: exhaust; they may remove any of their infantry from the board, each one reducing the cost by 1."},
    {"jolnarcommander", "After you roll dice for a unit ability (barrage, bombardment, space cannon), you may reroll any of them."},
    {"jolnarhero", "Action: swap any of your technologies that aren't unit upgrades for other technologies of the same colour. Then purge this card."},
    {"sardakkagent", "At the end of a player's tactical action: exhaust; they may put 2 infantry from their reinforcements on a planet they control in the active system."},
    {"sardakkcommander", "When committing ground forces, you may also land up to 1 ground force from each planet in the active system and in adjacent systems without your command token."},
    {"sardakkhero", "After you move ships into the active system: you may skip straight to landing ground forces. Afterwards purge this card and return your ships in that system to your reinforcements."},
    {"xxchaagent", "Action: exhaust to ready any planet; if it is next to a planet you control, you may also remove 1 infantry from it."},
    {"xxchacommander", "When you vote, each planet you exhaust gives 1 extra vote, and nothing can stop you from voting."},
    {"xxchahero", "Action: look at the top 5 agendas, reveal 2 of them and resolve each as if you had cast 1 vote for the outcome you choose; discard the rest. Then purge this card."},
    {"arborecagent", "Action: exhaust to let a player swap one of their non-fighter ships for one from their reinforcements that costs up to 2 more."},
    {"arboreccommander", "After another player activates a system with your units that have Production, you may produce 1 unit there."},
    {"arborechero", "Action: produce any number of units in any systems that contain your ground forces. Then purge this card."},
    {"l1z1xagent", "After a player activates a system: exhaust to let them swap 1 of their infantry in the active system for a mech."},
    {"l1z1xcommander", "Planetary shields don't stop your bombardment."},
    {"l1z1xhero", "Action: pick a system without other players' ships and move your flagship and any of your dreadnoughts there from anywhere. Then purge this card."},
    {"mentakagent", "After Pillage is used on another player: exhaust; you and that player each draw 1 action card."},
    {"mentakcommander", "After you win a space combat, your opponent must give you 1 promissory note from their hand."},
    {"mentakhero", "At the start of a space combat you're in: purge it; for every ship of your opponent destroyed in this combat, put a ship of that type from your reinforcements there."},
    {"naaluagent", "After an agenda is revealed: exhaust to look at the top card of the agenda deck."},
    {"naalucommander", "You may look at the top and bottom cards of the agenda deck and your neighbours' promissory notes at any time."},
    {"naaluhero", "At the end of the status phase: every other player gives you 1 promissory note from their hand. Then purge this card."},
    {"yinagent", "After a player's cruiser or destroyer is destroyed: exhaust; they may put up to 2 fighters from their reinforcements in that system."},
    {"yincommander", "Counts as a green technology for prerequisites."},
    {"yinhero", "Action: land up to 3 infantry from your reinforcements on any planets outside home systems and fight the ground combats; nobody can use space cannon against them. Then purge this card."},
    {"yssarilagent", "Can be used as any other player's agent (even if that agent is exhausted)."},
    {"yssarilcommander", "After another player activates a system with your units, you may look at their action cards."},
    {"yssarilhero", "Action: each other player shows you an action card; take it, or make them discard 3 random action cards. Then purge this card."},
    {"ghostagent", "After a player activates a system with an alpha, beta or gamma wormhole: exhaust; this tactical action it is adjacent to every other system with a wormhole."},
    {"ghostcommander", "After your ships move: each moving ship with capacity that went through a wormhole may bring 1 fighter from your reinforcements, if there's room."},
    {"ghosthero", "Action: swap the places of 2 systems that contain wormholes or your units (not the Creuss home system or the Wormhole Nexus). Then purge this card."},
    {"muaatagent", "Action: exhaust; a player produces up to 2 units costing 4 or less in a system with one of their war suns or their flagship."},
    {"muaatcommander", "After you spend a token from your strategy pool, you may gain 1 trade good."},
    {"muaathero", "After you move a war sun into a non-home system other than Mecatol Rex: destroy every other player's units and planets there; it becomes a supernova. Then purge this card."},
    {"saaragent", "When a player activates a system: exhaust; 1 of their ships moves as far as the fastest ship on the board this movement."},
    {"saarcommander", "When you produce fighters or infantry, you may place them at any of your space docks that aren't blockaded."},
    {"saarhero", "Action: pick a system next to one of your space docks; destroy all other players' infantry and fighters there. Then purge this card."},
    {"winnuagent", "When a player's units produce: exhaust; the units cost 2 less in total."},
    {"winnucommander", "+2 to your combat rolls at Mecatol Rex, in your home system and in systems with a legendary planet."},
    {"winnuhero", "Action: resolve the primary ability of any strategy card; other players may then use its secondary. Then purge this card."},
    {"nekroagent", "During the action phase: exhaust; a player may discard 1 action card or spend 1 command token to gain 2 trade goods."},
    {"nekrocommander", "After you gain a technology, you may draw 1 action card."},
    {"nekrohero", "Action: pick a technology-specialty planet in a system with your units: destroy other players' units there, gain trade goods equal to its resources + influence and a technology of its specialty colour. Then purge this card."},
    {"empyreanagent", "After a player moves ships into a system without planets: exhaust; that player gains 1 command token."},
    {"empyreancommander", "After another player moves ships into a system with one of your command tokens, you may return that token to your reinforcements."},
    {"empyreanhero", "Action: put a frontier token in every system without planets, then explore each one in a system with your ships. Then purge this card."},
    {"argentagent", "When a player produces ground forces: exhaust; they may place them on any planets they control in that system or adjacent systems."},
    {"argentcommander", "When your units roll for a unit ability (barrage, bombardment, space cannon), one of them rolls 1 extra die."},
    {"argenthero", "Action: move any of your ships from anywhere to systems that hold your command tokens and no other players' ships. Then purge this card."},
    {"mahactagent", "When you would spend a command token for the secondary of a strategic action: exhaust and remove one of the active player's tokens from the board to use instead."},
    {"mahactcommander", "During your tactical actions you may activate systems that hold your command token; then both tokens go back to your reinforcements and your turn ends."},
    {"mahacthero", "Action: move every unit in one system's space area to an adjacent system with another player's ships and fight a space combat there; nobody can retreat. Then purge this card."},
    {"naazagent", "At the end of a player's turn: exhaust; they may explore one of their planets."},
    {"naazcommander", "After you take a planet from another player, you may explore it."},
    {"naazhero", "Action: gain a relic and use the secondary of up to 2 strategy cards that are unchosen or ready, paying with tokens from your reinforcements. Then purge this card."},
    {"nomadagentmercer", "At the end of a player's turn: exhaust; they may move up to 2 of their ground forces from anywhere onto their planets in the active system."},
    {"nomadagentthundarian", "After the combat dice are rolled: exhaust; no hits are assigned and both players roll this round again."},
    {"nomadagentartuno", "When you gain trade goods: exhaust and put the same number on this card; you gain them when it readies."},
    {"nomadcommander", "You can produce your flagship for free."},
    {"nomadhero", "Action: this round your flagship and the units it carries can move out of systems with your command tokens. Purged at the end of the round."},
    {"titansagent", "Before a hit is assigned to your units: exhaust to cancel it."},
    {"titanscommander", "When your units produce, gain 1 trade good."},
    {"titanshero", "Action: ready Elysium; it gets +3 resources, +3 influence and Space Cannon 5 (x3). Then purge this card (it stays on Elysium)."},
    {"cabalagent", "After another player refills commodities: exhaust; their commodities become trade goods and you capture 1 unit from their reinforcements that costs no more than their commodity value."},
    {"cabalcommander", "When you produce fighters or infantry, up to 2 of them don't count against your production limit."},
    {"cabalhero", "Action: each other player rolls for each non-fighter ship in or next to a Dimensional Tear; on 1-3 you capture it (and what it carried). Then purge this card."},
    // Thunder's Edge factions (0.35.0)
    {"bastionagent", "When a player's unit is destroyed: exhaust to galvanize another of that player's units in that system."},
    {"bastioncommander", "Your action cards can't be cancelled by Sabotage, and the Nekro Virus can't put assimilator tokens on your cards."},
    {"bastionhero", "When one of your galvanized units is destroyed: purge her to roll 1 die for each other unit in its system; each roll that reaches that unit's combat value destroys the unit rolled for."},
    {"ralnelagent", "Action: exhaust to draw 2 action cards; give 1 of them to another player."},
    {"ralnelcommander", "When you retreat: up to 2 of your ships may retreat to another adjacent system without other players' ships instead; put a command token from your reinforcements there."},
    {"ralnelhero", "After the last player passes: you may stop being passed - gain 2 command tokens and draw 1 action card. Then purge this card."},
    {"deepwroughtagent", "When another player researches: exhaust to let them ignore 1 prerequisite; then you may put 1 infantry, coexisting, on one of their non-home planets."},
    {"deepwroughtcommander", "When another player pays resources to research, they may pay 1 less; then you gain 1 commodity (or turn 1 into a trade good)."},
    {"deepwroughthero", "Action: purge a technology (not a unit upgrade) from the game - every player who owns it loses it and researches another. Then purge this card."},
    {"crimsonagent", "Action: exhaust and pick a player: they swap the places of 2 of their ships (with what they carry)."},
    {"crimsoncommander", "At the end of any combat: gain 1 commodity (or turn 1 into a trade good)."},
    {"crimsonhero", "When you produce ships: you may put them on this card. At the start of a space combat: purge it to bring every ship on it into the active system."},
    {"firmamentagent", "When a player moves ships: exhaust - space cannon can't be fired at those ships during the move."},
    {"firmamentcommander", "For scoring secret objectives, planets in systems with your ships count as yours."},
    {"firmamenthero", "Action: put 1 of your plot cards in play with another player's control token; then you may put a player's token on one of your plots (not twice on one). Then purge this card."},
    {"obsidianagent", "When a ship is destroyed in a combat: exhaust - its owner's opponent must destroy one of their ships of that type in the system."},
    {"obsidiancommander", "+1 to your units' combat rolls in the Fracture."},
    {"obsidianhero", "Action: ready all your planets. Then purge this card."},
    // Discordant Stars (0.37.0)
    {"kortaliagent", "When a player wins a ground combat: exhaust to let them take 1 of their opponent's relic fragments at random."},
    {"kortalicommander", "In the first round of a combat, you may cancel 1 hit against your units."},
    {"kortalihero", "When you win a combat: purge her to take 1 of your opponent's relics, then gain 1 command token for each legendary planet or planet with a technology specialty you control."},
    {"vaylerianagent", "After a player gains control of a planet: exhaust to let them draw 1 action card."},
    {"vayleriancommander", "After you activate a system: your ships have 2 more capacity this tactical action."},
    {"vaylerianhero", "After you activate a system: purge him to take back 1 of your command tokens from the board and gain 1 command token; your ships get +1 move and +1 to combat rolls this tactical action."},
    {"lanefiragent", "When a player explores: exhaust to let them discard the card they drew and draw another."},
    {"lanefircommander", "After you explore a planet: you may put 1 infantry from your reinforcements on it."},
    {"lanefirhero", "Action: reveal 3 relics. Gain 1 command token for each relic in play, and you may swap other players' relics for revealed ones. The rest are shuffled back; purged."},
    {"kyroagent", "After a player loses or draws a ground combat: exhaust to let them refill their commodities; you put infantry equal to their commodity value minus 1 on one of your planets."},
    {"kyrocommander", "When you cast votes: remove up to 4 of your infantry from the board for 1 more vote each."},
    {"kyrohero", "At the start of the status phase: attach it to one of your strategy cards for good - that card has initiative 9, and trade goods placed on it go to you."},
    {"cheiranagent", "At the end of a player's turn: exhaust to let them move 1 of their command tokens to a neighbouring system with their structure and none of their tokens."},
    {"cheirancommander", "While defending a ground combat: cancel hits against you for 1 commodity or trade good each."},
    {"cheiranhero", "Action: your starting units again, from your reinforcements, into a system with your structure and no other ships (ground forces on your planets). Purged afterwards."},
    {"celdauriagent", "At the end of a player's tactical action: exhaust to let them pay 2 trade goods or commodities for a space dock on their planet in that system."},
    {"celdauricommander", "After you activate a system with your space dock, gain 1 commodity. Your systems with space docks are next to each other for your ships."},
    {"celdaurihero", "After a player activates a system other than Mecatol Rex: purge him to place a space dock on your planet there, turn your commodities into trade goods and use Production there."},
    // Discordant Stars batch 2 (0.38.0)
    {"khraskagent", "When a player spends influence: exhaust to let them use the resources of 1 of their non-home planets as influence as well."},
    {"khraskcommander", "At the start of an invasion in a system with planets: put 1 infantry from your reinforcements in its space area, if you have room."},
    {"khraskhero", "Action: purge it to exhaust or ready up to 4 planets."},
    {"zealotsagent", "After a player researches a technology: exhaust to let them produce 1 ship at home or in a system with their planet that has a technology specialty."},
    {"zealotscommander", "Researching, you may use 1 of your planets' technology specialties to ignore any one prerequisite - of any "
                         "colour, not only its own. (Using a specialty exhausts that planet, as always.)"},
    {"zealotshero", "Action: gain up to 3 technologies that aren't faction technologies. At the start of the next strategy phase you keep only 1 of them."},
    {"zelianagent", "At the start of an invasion: exhaust to let the active player swap 1 of their infantry in the space area for a mech."},
    {"zeliancommander", "Your dreadnoughts and war suns without Anti-Fighter Barrage have Anti-Fighter Barrage 5."},
    {"zelianhero", "After you move a unit with Bombardment into a non-home system (not Mecatol Rex): gain trade goods equal to its planets' resources, destroy every unit on them and the planets themselves; purged."},
    {"rohdhnaagent", "After a player's units use Production: exhaust to let them gain 1 command token."},
    {"rohdhnacommander", "When you spend a command token on Construction's secondary ability, you may resolve its primary ability instead."},
    {"rohdhnahero", "After you move ships into the active system: purge him to produce ships there with every unit's Production as if it were yours, blockade or not."},
    {"tnelisagent", "At the start of an invasion: exhaust to let one of the active player's ships use its Anti-Fighter Barrage as Bombardment."},
    {"tneliscommander", "After you activate a system: you may damage one of your ships to let one ship move 1 further this tactical action."},
    {"tnelishero", "After another player scores a secret objective: attach it there - you can score that objective like a public one; purged when you do."},
    {"kolumeagent", "Action: exhaust to let a player move 1 of their command tokens to another pool."},
    {"kolumecommander", "+1 to your units' unit ability rolls (space cannon, bombardment, anti-fighter barrage)."},
    {"kolumehero", "Action: this round, after each of your component actions, one of your units fires its space cannon at ships in or next to its system. Purged at the end of the round."},
    {"bentoragent", "Action: exhaust to give a player 1 commodity for each of your Fragment tokens (any beyond their commodity value become trade goods)."},
    {"bentorcommander", "Your commodity value is 1 higher."},
    {"bentorhero", "Action: explore each planet you control, in any order. Then purge him."},
    {"dihmohnagent", "When a player commits units to a planet: exhaust to let them put 1 infantry from their reinforcements there too."},
    {"dihmohncommander", "Counts as a red technology prerequisite. At the start of a space combat where you have 3 or more kinds of non-fighter ships: 1 hit on your opponent's ships."},
    {"dihmohnhero", "After the dice of a space combat round are rolled: purge him - every ship in the system becomes damaged, and your ships can't be destroyed this round."},
    {"florzenagent", "At the start of a player's turn: exhaust to let them move an attachment from one of their non-home planets to one of theirs in a system next to it (not Mecatol Rex)."},
    {"florzencommander", "After the second agenda is revealed: ready one of your planets and explore it."},
    {"florzenhero", "Action: 2 fighters in each system with your planets and no other ships; then move 1 attachment from a planet in a system with your ships to your home system. Purge her."},
    {"gledgeagent", "When a player's units use Production: exhaust to give them 3 more Production."},
    {"gledgecommander", "When your units use Production: 1 more for each space dock you control."},
    {"gledgehero", "Action: reveal exploration cards (not frontier) until 3 attachments; attach each to one of your planets of its type, discard the rest. Then purge him."},
    {"augersagent", "After a player explores a legendary planet or one with a technology specialty: exhaust to give them 2 trade goods."},
    {"augerscommander", "Counts as a yellow technology prerequisite. When you cast votes, cast 1 more for every 2 technologies you own."},
    {"augershero", "Action: draw 3 public objectives; one may replace the objective on your faction sheet, the rest go to the bottom. Then purge her."},
    {"kjalengardagent", "At the start of a combat: exhaust to move a Glory token into that system; the active player gains 1 commodity per neighbour."},
    {"kjalengardcommander", "Researching a unit upgrade: each of your unit upgrades covers 1 prerequisite of a colour both ask for."},
    {"kjalengardhero", "Action: for each Glory token, take back 1 of your command tokens in or next to its system and gain 1 command token. Then purge him."},
    {"kolleccagent", "At the start of a player's turn: exhaust to let them reveal 2 cards of an exploration deck matching one of their planets - they keep the relic fragments."},
    {"kollecccommander", "Counts as a blue technology prerequisite. Retreating from a system without your command token places no token."},
    {"kollecchero", "Action: draw a relic for each player, keep 1, shuffle the rest back. Then purge him."},
    {"mortheusagent", "When a player produces units: exhaust to let one of the new ships go to a neighbouring system without other players' ships."},
    {"mortheuscommander", "At the start of a space combat on another player's turn: one of your non-fighter ships has Sustain Damage for the combat."},
    {"mortheushero", "Action: in each system with your ships, put one more of one of those ship types. Then purge him."},
    {"vadenagent", "At the start of a player's turn: exhaust to give them commodities equal to one of their planets' influence."},
    {"vadencommander", "At the start of the status phase: 1 trade good for each secret objective you scored, 1 commodity for each public one."},
    {"vadenhero", "Action: each token on your sheet goes back - its owner gives you 1 trade good, 2 commodities or a promissory note. Then purge him."},
    {"cymiaeagent", "When a player discards or purges an action card: exhaust to let them draw 1."},
    {"cymiaecommander", "After you take a planet in a tactical action: discard an action card to put a mech on it."},
    {"cymiaehero", "Action: reveal 1 action card more than there are players; give 1 to each other player, keep the rest; they each discard 1. Then purge it."},
    {"nivynagent", "When a player's unit would use an ability (not Production) in or next to an anomaly: exhaust to stop that unit."},
    {"freesystemsagent", "While ready, it has the abilities of the legendary planets any player controls: exhaust it to use one."},
    {"freesystemscommander", "After you take a non-home planet in a tactical action: produce 1 ship in that system if your ships are there."},
    {"freesystemshero", "Action: ready one of your non-home planets (not Mecatol Rex) and clear every unit from it - no units can be put there again. Then purge him."},
    {"edynagent", "After a player passes: exhaust to let a player of your choice perform 1 action; then look at the top agenda and you may discard it."},
    {"edyncommander", "When you could score a public objective, you may draw 1 secret objective instead."},
    {"edynhero", "Action: for each Sigil, draw an agenda and resolve it as if you alone voted, for the outcome you choose. Then purge him."},
    {"nokaragent", "After movement: exhaust to let the active player put 1 destroyer in the active system - then you may trade with them."},
    {"nokarcommander", "In a space combat after you announce a retreat: cancel up to 2 hits. You may retreat to systems next to your home."},
    {"nokarhero", "Action: until the end of the round, each of your planets has Production 3. Then purge him."},
    {"axisagent", "Action: exhaust to let a player put a cruiser or destroyer with their ships - another player: you gain 2 commodities."},
    {"axiscommander", "After a player uses an Axis Order: you may spend 6 resources to gain that unit's upgrade."},
    {"axishero", "Action: give Axis Orders from your play area to other players; each one gives you a promissory note. Then purge her."},
    {"mykomentoriagent", "Before another player rolls: exhaust to make one of their dice one of your Omen dice (it stays near your sheet)."},
    {"mykomentoricommander", "When another player's space combat hits come at you: spend 1 commodity or trade good to cancel 1."},
    {"mykomentorihero", "Other players' heroes that would be purged are bound to it instead; resolve one of them as your own. Purged with everything bound to it."},
    {"gheminaagent", "When the active player makes hits in a combat round: exhaust for 1 more hit."},
    {"gheminacommander", "After you win a space combat where there are no planets, or a ground combat on a planet with a structure: gain 1 trade good."},
    {"gheminaherolord", "Action: take a non-home planet (not Mecatol Rex) without other players' units, in or next to a system with your flagship; ready it. Then purge him."},
    {"gheminaherolady", "Action: destroy every unit on a non-home planet (not Mecatol Rex) in or next to a system with your flagship. Then purge her."},
    {"veldyragent", "When a player researches: exhaust to let them pay with one planet's influence as resources."},
    {"veldyrcommander", "Researching a technology owned by a player with a Branch Office planet: ignore 1 prerequisite."},
    {"veldyrhero", "Action: for each Branch Office, gain a unit upgrade of a unit type its planet's owner has upgraded. Then purge him."},
    {"lizhoagent", "At the end of a player's turn: exhaust to let them put 2 fighters with their ships."},
    {"lizhocommander", "In a combat round with only 1 of your non-fighter ships (or 1 infantry) fighting: 1 unit rolls 1 more die."},
    {"lizhohero", "Action: attach or move each Trap to any planet; up to 12 fighters in systems with your Traps and no other ships. Then purge her."},
    {"mirvedaagent", "After a player gains a unit upgrade: exhaust - they may spend a strategy token to research a technology of one of its prerequisite colours."},
    {"mirvedacommander", "Your space cannon may also fire at units next to their system."},
    {"mirvedahero", "Action: up to 3 PDS; then all your PDS fire at one system - space cannon at the ships, bombardment at one planet. Then purge it."},
    {"ghotiagent", "When a player produces: exhaust for 2 more units, or 1 trade good."},
    {"ghoticommander", "After you activate a system: systems without planets are next to each other during this action."},
    {"ghotihero", "Action: in each system with your ships, one of them becomes a ship costing up to 2 more. Then purge her."},
    {"nivyncommander", "When your damaged units roll for combat, up to 2 of them roll 1 more die."},
    {"nivynhero", "Action: every unit that can sustain damage becomes damaged (not your mechs); this round your units are next to the Wound. Purged at the end of the round."},
    {"olradinagent", "After a player exhausts a cultural, hazardous or industrial planet: exhaust to let them ready one of theirs of another trait."},
    {"olradincommander", "When you pass: exhaust a non-home planet (not Mecatol Rex) for trade goods equal to its resources or influence."},
    {"olradinhero", "Action: 1 infantry on each planet you control; you may flip 1 policy. Then purge him."},
};

// Commander / hero unlock conditions.
const KT kUnlock[] = {
    {"kelerescommander", "Pay 1 trade good after you play an action card that has a component action."},
    {"solcommander", "Control planets with at least 12 resources in total."},
    {"letnevcommander", "Have 5 non-fighter ships in one system."},
    {"hacancommander", "Have 10 trade goods."},
    {"jolnarcommander", "Own 8 technologies."},
    {"sardakkcommander", "Control 5 planets outside home systems."},
    {"xxchacommander", "Control planets with at least 12 influence in total."},
    {"arboreccommander", "Have 12 ground forces on planets you control."},
    {"l1z1xcommander", "Have 4 dreadnoughts on the board."},
    {"mentakcommander", "Have 4 cruisers on the board."},
    {"naalucommander", "Have ground forces in or next to the Mecatol Rex system."},
    {"yincommander", "Use one of your faction abilities."},
    {"yssarilcommander", "Have 7 action cards."},
    {"ghostcommander", "Have units in 3 systems with alpha or beta wormholes."},
    {"muaatcommander", "Produce a war sun."},
    {"saarcommander", "Have 3 space docks on the board."},
    {"winnucommander", "Control Mecatol Rex, or take part in a combat in its system."},
    {"nekrocommander", "Own 3 technologies (a Valefar Assimilator counts only while its token is on a technology)."},
    {"empyreancommander", "Be neighbours with every other player."},
    {"argentcommander", "Have 6 units with anti-fighter barrage, space cannon or bombardment on the board."},
    {"mahactcommander", "Have 2 other players' command tokens in your fleet pool."},
    {"naazcommander", "Have mechs in 3 systems."},
    {"nomadcommander", "Have scored a secret objective."},
    {"titanscommander", "Have 5 structures on the board."},
    {"cabalcommander", "Have units in 3 gravity rifts."},
    {"bastioncommander", "Have 3 galvanized units on the board."},
    {"ralnelcommander", "Be the last player to pass in an action phase."},
    {"deepwroughtcommander", "Have an ocean card."},
    {"crimsoncommander", "Put a breach in a system with another player's unit."},
    {"firmamentcommander", "Have a plot card in play."},
    {"obsidiancommander", "Have units in the Fracture."},
    {"kortalicommander", "Destroy the last of a player's units in a system."},
    {"vayleriancommander", "Control 3 planets with the same trait."},
    {"lanefircommander", "7 relic fragments have been purged in this game."},
    {"kyrocommander", "Have 6 infantry and 6 fighters on the board."},
    {"cheirancommander", "Have 4 structures on non-home planets."},
    {"celdauricommander", "Have a space dock outside and not next to your home system."},
    {"khraskcommander", "Win a ground combat on a planet you don't control."},
    {"zealotscommander", "Own a technology with 2 or more prerequisites."},
    {"zeliancommander", "Have ships in 2 systems with asteroid fields."},
    {"rohdhnacommander", "Have units with Production 7 or more in one system."},
    {"tneliscommander", "Have 6 destroyers on the board."},
    {"kolumecommander", "Ready one of your technologies in the action phase."},
    {"bentorcommander", "Have 3 Fragment tokens on your faction sheet."},
    {"dihmohncommander", "Research a ship unit upgrade."},
    {"florzencommander", "Have the outcome you voted for resolved."},
    {"gledgecommander", "Have non-fighter ships costing 10 or more in one system."},
    {"augerscommander", "Win a combat in a system with a legendary planet or one with a technology specialty."},
    {"kjalengardcommander", "Have 2 Glory tokens on the board."},
    {"kollecccommander", "Have 4 relic fragments."},
    {"mortheuscommander", "Have ships in 3 systems that aren't your home or next to it."},
    {"vadencommander", "Have tokens of different players on your sheet - one for every 2 players in the game."},
    {"cymiaecommander", "Have 10 infantry on the board."},
    {"nivyncommander", "Have units in 2 anomalies other than your home."},
    {"olradincommander", "Control a cultural, a hazardous and an industrial planet."},
    {"freesystemscommander", "Every non-legendary planet on the board is controlled."},
    {"edyncommander", "Have a law in play."},
    {"nokarcommander", "Another player takes over your ships (Hired Guns)."},
    {"axiscommander", "Have Axis Orders of all 4 kinds in players' play areas."},
    {"mykomentoricommander", "Have 4 commodities."},
    {"gheminacommander", "Have both of your flagships on the board."},
    {"veldyrcommander", "Two other players control planets with Branch Offices."},
    {"lizhocommander", "Have 4 Traps on the board."},
    {"mirvedacommander", "Own 2 unit upgrades."},
    {"ghoticommander", "Have units in 3 systems without planets."},
};

// Flagships, mechs and other special units.
const KT kUnitText[] = {
    {"keleres_flagship", "Other players must pay 2 influence to activate the system it is in."},
    {"keleres_mech", "Other players must pay 1 influence (for each of these mechs) to land ground forces on its planet."},
    {"sol_flagship", "At the end of the status phase, put 1 infantry from your reinforcements in this system's space area."},
    {"sol_mech", "Deploy: after you use Orbital Drop, you may pay 3 resources to put 1 mech on that planet."},
    {"sol_infantry2", "When destroyed, roll a die: 5 or more and it returns to a planet in your home system at the start of your next turn."},
    {"infantry2", "When destroyed, roll a die: 6 or more and it returns to a planet in your home system at the start of your next turn."},
    {"letnev_flagship", "Other players' units in this system lose Planetary Shield. It is repaired at the start of each space combat round."},
    {"letnev_mech", "Deploy: at the start of a ground combat round you may pay 2 resources to swap 1 of your infantry in it for this mech."},
    {"hacan_flagship", "After you roll a die in space combat here, you may spend 1 trade good to add 1 to it."},
    {"hacan_mech", "Its planet card can be traded; your units there move to another planet you control."},
    {"jolnar_flagship", "Each natural 9 or 10 it rolls in combat makes 2 extra hits."},
    {"jolnar_mech", "Your infantry on its planet aren't affected by Fragile."},
    {"sardakk_flagship", "Your other ships in this system get +1 on combat rolls."},
    {"sardakk_mech", "After it uses Sustain Damage in ground combat, it makes 1 hit against your opponent's ground forces there."},
    {"sardakk_dreadnought2", "Can't be destroyed by Direct Hit. After a space combat round you may destroy it to destroy up to 2 ships in this system."},
    {"l1z1x_dreadnought2", "Can't be destroyed by Direct Hit."},
    {"xxcha_flagship", "Its space cannon can also fire at ships in adjacent systems."},
    {"xxcha_mech", "Its space cannon can also fire at ships in adjacent systems."},
    {"arborec_flagship", "After you activate its system, you may produce up to 5 units there."},
    {"arborec_mech", "Deploy: when you use Mitosis you may put a mech instead of the infantry. Has Production 2 and Planetary Shield."},
    {"arborec_infantry", "Production 1."},
    {"arborec_infantry2", "Production 2. When destroyed, roll a die: 6 or more and it returns to a home planet at the start of your next turn."},
    {"l1z1x_flagship", "In space combat, hits made by it and your dreadnoughts there must go to non-fighter ships if possible."},
    {"l1z1x_mech", "When not in a ground combat, it can bombard planets in its system like a ship."},
    {"mentak_flagship", "Other players' ships in this system can't use Sustain Damage."},
    {"mentak_mech", "Other players' ground forces on its planet can't use Sustain Damage."},
    {"naalu_flagship", "During an invasion here, your fighters can land and fight as ground forces; afterwards they return to space."},
    {"naalu_mech", "+2 to its combat rolls against an opponent who has at least 1 relic fragment."},
    {"naalu_fighter2", "Can move on its own (move 2). Fighters beyond your ships' capacity count as half a ship each against your fleet pool."},
    {"fighter2", "Can move on its own (move 2). Fighters beyond your ships' capacity count against your fleet pool instead."},
    {"yin_flagship", "When it is destroyed, all ships in its system are destroyed."},
    {"yin_mech", "Deploy: when you use Indoctrination you may pay 1 more influence to put this mech instead of the infantry."},
    {"yssaril_flagship", "Can move through systems that contain other players' ships."},
    {"yssaril_mech", "Deploy: after you use Stall Tactics, you may put 1 mech on a planet you control."},
    {"ghost_flagship", "Its system contains a delta wormhole (linked to the Creuss home system and the Creuss Gate)."},
    {"ghost_mech", "After any player activates its system, you may remove it to put a Creuss wormhole token there."},
    {"muaat_flagship", "Action: spend 1 strategy token to put 1 cruiser in its system."},
    {"muaat_mech", "When you use Star Forge in its system or next to it, you may put 1 infantry with it."},
    {"nekro_flagship", "At the start of a space combat, your ground forces in its space area may fight as ships."},
    {"nekro_mech", "+2 to its combat rolls against an opponent with a Valefar Assimilator token on one of their technologies."},
    {"saar_mech", "Deploy: after you gain control of a planet, you may spend 1 trade good to put 1 mech there."},
    {"winnu_flagship", "Rolls one die for each non-fighter ship your opponent has in its system."},
    {"winnu_mech", "After a tactical action in which you took its planet, you may put 1 PDS or 1 space dock there."},
    {"empyrean_flagship", "After one of your units in or next to its system uses Sustain Damage, you may spend 2 influence to repair it."},
    {"empyrean_mech", "Remove it to cancel an action card played by a player with units in or next to its system."},
    {"argent_destroyer2", "When it uses anti-fighter barrage, each 9 or 10 also destroys 1 of your opponent's infantry in the space area."},
    {"argent_flagship", "Other players can't use space cannon against your ships in this system."},
    {"argent_mech", "Needs no capacity while transported or while in a space area with one of your ships that has capacity."},
    {"cabal_flagship", "Every other unit (not structures) destroyed in this system is captured - your own too."},
    {"cabal_mech", "Your infantry destroyed on this planet are captured by you."},
    {"cabal_spacedock", "Production 5. Its system is a gravity rift (you don't roll for it). Up to 6 fighters here need no capacity."},
    {"cabal_spacedock2", "Production 7. Its system is a gravity rift (you don't roll for it). Up to 12 fighters here need no capacity."},
    {"mahact_flagship", "+2 to its combat rolls against an opponent whose command token isn't in your fleet pool."},
    {"mahact_infantry", "After it is destroyed, gain 1 commodity or turn 1 commodity into a trade good."},
    {"mahact_infantry2", "After it is destroyed, gain 1 commodity or turn 1 commodity into a trade good; it returns to a home planet at the start of your next turn."},
    {"mahact_mech", "After a player whose token is in your fleet pool activates its system, you may give them that token back to end their turn."},
    {"naaz_flagship", "Your mechs in this system roll 1 extra die in combat."},
    {"naaz_mech", "In the space area of the active system it fights in space combat as a ship (Z-Grav Eidolon: combat 8, 2 dice)."},
    {"nomad_flagship", "Counts as adjacent to every system with your mechs."},
    {"nomad_flagship2", "Counts as adjacent to every system with your mechs."},
    {"nomad_mech", "In a space area during combat it can use Sustain Damage to cancel a hit against your ships there."},
    {"titans_flagship", "Deploy: after you activate a system with your PDS, you may replace 1 of them with this ship."},
    {"titans_mech", "Deploy: when you would place a PDS on a planet, you may place 1 mech and 1 infantry there instead."},
    {"titans_pds", "Hel-Titan: a structure and a ground force (it fights in ground combat). Can't be transported. Production 1."},
    {"titans_pds2", "Hel-Titan: a structure and a ground force. Can't be transported. Production 1. Its space cannon also reaches adjacent systems."},
    // Thunder's Edge factions (0.35.0)
    {"bastion_flagship", "+1 to its combat rolls for each non-home system holding a planet you control."},
    {"bastion_mech", "When it is destroyed while galvanized: galvanize up to 3 of your infantry in its system."},
    {"bastion_spacedock", "Its planet has 1 more resource. Production = the planet's resources + 2. Up to 3 fighters here need no capacity."},
    {"bastion_spacedock2", "Its planet has 2 more resources. Production = the planet's resources + 4. Up to 3 fighters here need no capacity."},
    {"ralnel_flagship", "When it retreats: you may destroy 1 ship without Sustain Damage in the system it leaves."},
    {"ralnel_mech", "At the end of a ground combat round on its planet: up to 2 of your ground forces may come there from planets in adjacent systems."},
    {"ralnel_destroyer", "Can fire the space cannon of one of your structures carried in its space area (each structure once)."},
    {"ralnel_destroyer2", "Can fire the space cannon of one of your structures carried in its space area (every Linkship can use the same one)."},
    {"deepwrought_flagship", "Can move through systems with your units even if other players' units are there; each one it moves through that way gives it 1 more move."},
    {"deepwrought_mech", "When another player activates its system while it coexists: it and your infantry on its planet may go to a planet in your home system."},
    {"crimson_destroyer", "Exile I - at the end of any combat in its system or an adjacent one: you may put an inactive breach in that system."},
    {"crimson_destroyer2", "Exile II - at the end of any combat within 2 systems: you may put an active or inactive breach in that system."},
    {"crimson_flagship", "While it is in a system with an active breach, other players' units in systems with active breaches lose their unit abilities."},
    {"crimson_mech", "When you invade a system with an active breach: you may commit 1 mech from your reinforcements even with no units there."},
    {"firmament_flagship", "With units of a player whose token is on one of your plots in the active system: +1 move, and it is repaired at the end of each combat round."},
    {"firmament_mech", "When ground forces are committed to its planet: your units there may coexist instead of fighting. When you become the Obsidian, it turns into Viper Hollow."},
    {"obsidian_mech", "If it was coexisting when it turned over: you take its planet, and the other player's units coexist there."},
    // Discordant Stars (0.37.0)
    {"kortali_flagship", "After you win a space combat here: let it become damaged to gain 1 command token."},
    {"kortali_mech", "Its Planetary Shield can't be taken away (not even by war suns)."},
    {"vaylerian_flagship", "At the start of a space combat here: choose a neighbouring system your opponent can't retreat to."},
    {"vaylerian_cruiser", "Raider I - if your opponent can't announce a retreat this round, its hits go to non-fighter ships. It carries ground forces only."},
    {"vaylerian_cruiser2", "Raider II - if your opponent can't announce a retreat this round, its hits can't be cancelled and go to non-fighter ships."},
    {"vaylerian_mech", "During your tactical actions, space cannon hits can't be assigned to one of your ships in its system."},
    {"lanefir_flagship", "At the end of a tactical action here: you may explore one of your planets in this system."},
    {"lanefir_mech", "Deploy: at the start of your turn, purge 1 relic fragment to put a mech on one of your planets."},
    {"kyro_flagship", "When you commit units to a planet here, you may commit 1 more infantry from your reinforcements."},
    {"kyro_mech", "When it would be destroyed while damaged: discard 1 action card to repair it instead."},
    {"cheiran_flagship", "Rolls 1 more die in combat if its system has or is next to one of your structures."},
    {"cheiran_dreadnought", "Chitin Hulk I - when it is destroyed, you may put 1 fighter in its system."},
    {"cheiran_dreadnought2", "Chitin Hulk II - Direct Hit can't destroy it; when it is destroyed, you may put 1 fighter or 1 destroyer in its system."},
    {"cheiran_mech", "When it is destroyed, you may put 1 infantry on its planet."},
    {"celdauri_flagship", "You may produce ships with other players' space docks in its system."},
    {"celdauri_mech", "Deploy: after a player activates a system, pay 1 commodity or trade good to put a mech on your planet with a space dock there."},
    {"celdauri_spacedock", "Trade Port I - Production = 2 + its planet's resources or influence. Up to 3 fighters here need no capacity. Anti-Fighter Barrage 6 (x2)."},
    {"celdauri_spacedock2", "Trade Port II - Production = 4 + its planet's resources or influence. Up to 3 fighters need no capacity. Its Anti-Fighter Barrage 6 (x2) fires every round."},
    // Discordant Stars batch 2 (0.38.0)
    {"khrask_flagship", "At the start of a space combat here: 2 of your ships without Sustain Damage gain it for this combat."},
    {"khrask_cruiser", "Shattered Sky I - Bombardment 8."},
    {"khrask_cruiser2", "Shattered Sky II - 2 capacity, Bombardment 6."},
    {"khrask_mech", "In the first round of a ground combat on its planet, only your mechs roll - nobody else."},
    {"zealots_flagship", "+1 to its combat rolls for each unit upgrade your opponent owns."},
    {"zealots_mech", "+1 to its combat rolls for each faction technology your opponent owns."},
    {"zelian_flagship", "Rolls 1 more combat die for each asteroid field next to it."},
    {"zelian_infantry", "Impactor I - Bombardment 9 (from the space area, before landing)."},
    {"zelian_infantry2", "Impactor II - Bombardment 8. When destroyed: on 6+ it comes back home at the start of your next turn."},
    {"zelian_mech", "At the start of an invasion, if it's in the space area: remove it to destroy 1 unit on a planet there."},
    {"rohdhna_flagship", "Deploy: after you activate a system, pay 4 resources to replace 1 of your non-fighter ships there with it."},
    {"rohdhna_warsun", "Terrafactory I - Production 5; it can't move and can't be produced (Industrious makes it)."},
    {"rohdhna_warsun2", "Terrafactory II - moves 2, Production 5, Bombardment 3 (x3); other players' units here lose Planetary Shield."},
    {"rohdhna_mech", "Deploy: after Recycled Materials, put a mech in that system's space area or on your planet there."},
    {"tnelis_flagship", "At the start of each combat round: one of your opponent's ships rolls 1 die less."},
    {"tnelis_destroyer", "Blockade Runner I - Anti-Fighter Barrage 9 (x3)."},
    {"tnelis_destroyer2", "Blockade Runner II - Anti-Fighter Barrage 6 (x4); it moves through systems with other players' ships."},
    {"tnelis_mech", "Deploy: after you move a destroyer into a non-home system (not Mecatol Rex), pay 3 resources to put a mech on a planet there."},
    {"kolume_flagship", "Space Cannon 7; your units' space cannon hits in its system can't be cancelled."},
    {"kolume_mech", "Space Cannon 8 (x2), but its hits only destroy fighters. When you spend a strategy token, it is repaired."},
    {"bentor_flagship", "+1 to its combat and unit ability rolls for each Fragment token on your sheet."},
    {"bentor_mech", "Deploy: when a Fragment token goes on your sheet, put a mech on one of your planets."},
    {"dihmohn_flagship", "After it moves into the active system: produce up to 2 units there with a combined cost of 4 or less."},
    {"dihmohn_dreadnought", "Aegis I - when another of your ships here would be destroyed in combat, this ship may become damaged instead."},
    {"dihmohn_dreadnought2", "Aegis II - combat 4, move 2; when another of your ships here would be destroyed by anything, this ship may become damaged instead."},
    {"dihmohn_mech", "At the start of a combat in its system: repair 1 of your units in that combat."},
    {"florzen_flagship", "Other players can't play action cards during a space combat in its system."},
    {"florzen_fighter", "Corsair I - moves on its own (move 1); those beyond your ships' capacity count against your fleet pool. Anti-Fighter Barrage 9."},
    {"florzen_fighter2", "Corsair II - combat 8, move 3, moves on its own; those beyond capacity count against your fleet pool. Anti-Fighter Barrage 8."},
    {"florzen_mech", "You may pay for it with influence instead of resources."},
    {"gledge_flagship", "1 more die on its combat and ability rolls for each mech in or next to its system."},
    {"gledge_mech", "When you exhaust its planet for resources, 1 of its influence counts as a resource too."},
    {"gledge_pds", "Orion Platform I - when its space cannon rolls a natural 9 or 10 against another player, explore its planet."},
    {"gledge_pds2", "Orion Platform II - Space Cannon 5, also at ships next door; when it hits another player's units, explore its planet."},
    {"augers_flagship", "1 more die on its combat rolls for each secret objective you have scored."},
    {"augers_mech", "Deploy: after you research a technology, put a mech on one of your legendary or specialty planets."},
    {"kjalengard_flagship", "After the first round of a combat in its system: put up to 2 of your captured units back into the fight."},
    {"kjalengard_carrier", "Star Dragon I - it may ignore the movement effects of anomalies (not supernovas)."},
    {"kjalengard_carrier2", "Star Dragon II - move 2, capacity 6, combat 7; it may ignore the movement effects of all anomalies."},
    {"kjalengard_mech", "When you pass: 1 infantry from your reinforcements on its planet if a Glory token is in or next to its system."},
    {"kollecc_flagship", "When it retreats, every retreating unit may go onto your faction sheet (cloaked) instead."},
    {"kollecc_mech", "At the end of a tactical action in its system: up to 2 ground forces from its planet go onto your faction sheet (cloaked)."},
    {"mortheus_flagship", "After you activate its system: a frontier token there if it has no planets."},
    {"mortheus_mech", "When a player lands on a planet of yours next to its system: it may swap places with 1 of your infantry there."},
    {"vaden_flagship", "After its bombardment hits: gain 1 trade good."},
    {"vaden_mech", "At the end of a ground combat round: take back 1 of your opponent's tokens to put 1 infantry on its planet."},
    {"cymiae_flagship", "After you win a combat in its system: take 1 of your opponent's action cards at random."},
    {"cymiae_infantry", "Unholy Abomination I - combat 5."},
    {"cymiae_infantry2", "Unholy Abomination II - combat 4; destroyed, it comes back on a 6+ at the start of your turn, onto any of your planets."},
    {"cymiae_mech", "At the start of each ground combat round: repaired, if it is your only mech on the planet."},
    {"nivyn_flagship", "Other players' units in its system are removed instead of destroyed."},
    {"nivyn_mech", "Voidflare Warden I - after its system is activated: become damaged to put the Wound token there."},
    {"nivyn_mech2", "Voidflare Warden II - combat 4; after any system is activated: become damaged to put the Wound token there."},
    {"olradin_flagship", "When it moves into the active system, your other ships move 1 further this action."},
    {"olradin_mech", "Exemplar - a lone one on a planet: +1 resource (2+ \"+\" policies) or +1 influence (2+ \"-\" policies)."},
    {"olradin_mech_positive", "Exemplar Bonum - a lone one on a planet: +1 resource."},
    {"olradin_mech_negative", "Exemplar Malus - a lone one on a planet: +1 influence."},
    {"freesystems_flagship", "1 more die on its combat rolls for each planet in its system of the most common trait there."},
    {"freesystems_mech", "Deploy: after Rally to the Cause, 1 trade good for a mech on one of your planets in or next to that system."},
    {"edyn_flagship", "+1 to its combat rolls for each law in play."},
    {"edyn_mech", "Its system is a Sigil - an anomaly."},
    {"nokar_flagship", "+1 to its combat rolls for every 2 destroyers you have on the board."},
    {"nokar_destroyer", "Sabre I - destroyed in combat: roll a die, 9+ makes 1 hit on your opponent's ships."},
    {"nokar_destroyer2", "Sabre II - combat 7; destroyed in combat: roll a die, 7+ makes 1 hit on your opponent's ships."},
    {"nokar_mech", "For your retreats, its system counts as next to the active system."},
    {"axis_flagship", "After it hits in a round of space combat: repair 1 of your ships in its system."},
    {"axis_mech", "A space dock on its planet has Production 5."},
    {"mykomentori_flagship", "Once per round of space combat, when a non-fighter ship in its system is destroyed: gain 1 commodity."},
    {"mykomentori_mech", "Destroyed: on a 6+ it waits on its card; at the start of your turn it may replace one of your infantry."},
    {"mykomentori_spacedock", "Mycelium Ring I - Production: planet's resources + 2. Deploy: when you take a planet, 4 infantry there may become a space dock."},
    {"mykomentori_spacedock2", "Mycelium Ring II - Production: planet's resources + 5. Deploy: 3 infantry may become a space dock."},
    {"ghemina_flagship_lord", "The Lord: capacity 7; after a planet in its system is explored, the explorer puts 1 infantry there. The Lady: capacity 3, Bombardment 5 (x4); 1 trade good when someone else's structure in her system is destroyed."},
    {"celdauri_celagrom", "The Celagrom: at the start of a space combat it may produce 1 hit on one of the opponent's ships (you pick the ship type). It rolls 1 more die while you have another flagship on the board."},
    {"ghemina_flagship_lady", "The Lady: capacity 3, Bombardment 5 (x4); 1 trade good when someone else's structure in her system is destroyed."},
    {"ghemina_mech", "After you win a ground combat on its planet with exactly 1 other mech there: explore the planet."},
    {"ghemina_carrier", "Combat Transport I - move 2."},
    {"ghemina_carrier2", "Combat Transport II - move 2, capacity 6; in a ground combat on a planet in its system with 2 or fewer of your infantry: 1 of your dice rolls again."},
    {"veldyr_flagship", "1 more die on its combat rolls for each round of the combat already fought."},
    {"veldyr_dreadnought", "Lancer Dreadnought I - Bombardment 5, Space Cannon 8."},
    {"veldyr_dreadnought2", "Lancer Dreadnought II - move 2, Bombardment 5, Space Cannon 5; Direct Hit can't destroy it."},
    {"veldyr_mech", "After its system is activated: remove it to put a PDS on its planet."},
    {"lizho_flagship", "Only a hit assigned to it can destroy it."},
    {"lizho_fighter", "Heavy Bomber I - combat 9, Bombardment 9."},
    {"lizho_fighter2", "Heavy Bomber II - combat 8, move 2, Bombardment 8; moves on its own (beyond capacity they count against the fleet pool)."},
    {"lizho_mech", "At the start of your turn: move a Trap from the board onto its planet."},
    {"mirveda_flagship", "After each round of space combat in its system: 1 fighter."},
    {"mirveda_pds", "Gauss Cannon I - in the space area; moves and retreats like a ship (move 1); Space Cannon 6, Bombardment 6. Blockaded, it is destroyed."},
    {"mirveda_pds2", "Gauss Cannon II - move 2, capacity 1, Space Cannon 4, Bombardment 4; blockaded, it is destroyed."},
    {"mirveda_mech", "+1 to its combat rolls for every 2 unit upgrades you own."},
    {"ghoti_flagship", "It is also a space dock with Production equal to your fleet tokens."},
    {"ghoti_mech", "When you produce ships at home: 1 of them may appear in its system (unless blockaded)."},
};

// Faction promissory notes, keyed by faction key.
const KT kNoteName[] = {
    {"sol", "Military Support"}, {"letnev", "War Funding"}, {"hacan", "Trade Convoys"},
    {"jolnar", "Research Agreement"}, {"sardakk", "Tekklar Legion"}, {"xxcha", "Political Favor"},
    {"arborec", "Stymie"}, {"ghost", "Creuss Iff"}, {"l1z1x", "Cybernetic Enhancements"}, {"mentak", "Promise of Protection"},
    {"muaat", "Fires of the Gashlai"}, {"naalu", "Gift of Prescience"}, {"nekro", "Antivirus"}, {"saar", "Ragh's Call"},
    {"winnu", "Acquiescence"}, {"yin", "Greyfire Mutagen"}, {"yssaril", "Spy Net"}, {"argent", "Strike Wing Ambuscade"},
    {"cabal", "Crucible"}, {"empyrean", "Dark Pact"}, {"mahact", "Scepter of Dominion"}, {"nomad", "The Cavalry"},
    {"naaz", "Black Market Forgery"}, {"titans", "Terraform"}, {"keleres", "Keleres Rider"},
    {"bastion", "Raise the Standard"}, {"ralnel", "Nano-Link Permit"}, {"crimson", "Sever"}, {"deepwrought", "Share Knowledge"},
    {"firmament", "Black Ops"}, {"obsidian", "Malevolency"},
    {"kortali", "Blessing of the Queens"}, {"vaylerian", "Clan's Favor"}, {"lanefir", "Spoils of War"}, {"kyro", "Kyro Rider"},
    {"cheiran", "Carcinisation"}, {"celdauri", "Trade Alliance"},
    {"khrask", "Stone Speakers"}, {"zealots", "Favor of Rhodun"}, {"zelian", "Hyperkinetic Ordnance"}, {"rohdhna", "Automatons"},
    {"tnelis", "Plots Within Plots"}, {"kolume", "Combinatorial Bypass"},
    {"bentor", "Encryption Key"}, {"dihmohn", "Combat Drills"}, {"florzen", "Underground Market"}, {"gledge", "Gledge Base"},
    {"augers", "Read the Fates"}, {"kjalengard", "Vassalage"},
    {"kollecc", "AI Survey"}, {"mortheus", "Secrets of the Weave"}, {"vaden", "Vaden Handshake"}, {"cymiae", "Algorithmic Replication"},
    {"nivyn", "Nivyn Guidance"}, {"olradin", "Incite Revolution"},
    {"freesystems", "Broadcast Teams"}, {"edyn", "Edyn Rider"}, {"nokar", "Nokar Navigator"}, {"axis", "Industry Secrets"},
    {"mykomentori", "Gift of Insight"}, {"ghemina", "Raid Leaders"},
    {"lizho", "Trusted Counselor"}, {"mirveda", "Rapid Excavation"}, {"ghoti", "Ghoti Relay"},
};
const KT kNoteText[] = {
    {"sol", "At the start of the Sol player's turn: Sol loses 1 strategy token (if any), then you may put 2 infantry on a planet you control. The note goes back to Sol."},
    {"letnev", "At the start of a space combat round: the Letnev player loses 2 trade goods and you may reroll any of your dice this round. The note goes back to Letnev."},
    {"hacan", "Played face up: you may trade with players who aren't your neighbours. It goes back to Hacan if you activate a system with their units."},
    {"jolnar", "After Jol-Nar researches a technology that isn't a faction technology, you gain it too. The note goes back to Jol-Nar."},
    {"sardakk", "At the start of a ground combat during an invasion: +1 to your combat rolls this combat (and -1 to N'orr's if you fight them). The note goes back to N'orr."},
    {"xxcha", "When an agenda is revealed: Xxcha loses 1 strategy token, the agenda is discarded and the next one is voted on instead. The note goes back to Xxcha."},
    {"arborec", "After the Arborec player moves ships into a system with your units: you may put 1 command token from their reinforcements in any non-home system. The note goes back to Arborec."},
    {"l1z1x", "When you gain command tokens in the status phase, gain 1 more. The note goes back to L1Z1X."},
    {"mentak", "Play it face up: Mentak can't Pillage you. It goes back to Mentak if you activate a system with their units."},
    {"naalu", "At the end of the strategy phase: you take the Naalu \"0\" token (you go first) and Naalu can't use Telepathic this round. Back to Naalu at the end of the status phase."},
    {"yin", "At the start of a ground combat against 2 or more ground forces that aren't Yin's: swap 1 of your opponent's infantry for 1 of yours. The note goes back to Yin."},
    {"yssaril", "At the start of your turn: look at Yssaril's action cards and take 1. The note goes back to Yssaril."},
    {"ghost", "At the start of your turn: place or move a Creuss wormhole token (a system with your planet, or a non-home system without other ships). The note goes back to Creuss."},
    {"muaat", "Action: Muaat loses a fleet token and you gain the War Sun technology. The note goes back to Muaat."},
    {"saar", "After you land units on a planet: the Saar player's ground forces there move to one of their other planets. The note goes back to Saar."},
    {"winnu", "When the Winnu player resolves a strategic action: you may use the secondary without a command token. The note goes back to Winnu."},
    {"nekro", "At the start of a combat: play it face up; the Nekro player can't use Technological Singularity against you. It goes back if you activate a system with their units."},
    {"empyrean", "Played face up: when you give the Empyrean as many commodities as your commodity value, you each gain 1 trade good. It goes back to the Empyrean if you activate a system with their units."},
    {"argent", "When your units roll for a unit ability, one of them rolls 1 extra die. The note goes back to Argent."},
    {"mahact", "At the start of the strategy phase: pick a non-home system with your units; the Mahact player and every player whose token is in Mahact's fleet pool put a token there. The note goes back to Mahact."},
    {"naaz", "Action: purge 2 relic fragments of the same type to gain a relic. The note goes back to Naaz-Rokha."},
    {"nomad", "At the start of a space combat (not against the Nomad): one of your ships fights with the Nomad flagship's combat and barrage values and can absorb 1 hit. Back to the Nomad after the combat."},
    {"titans", "Action: attach it to a non-home planet you control (not Mecatol Rex): +1 resource, +1 influence and all three traits."},
    {"cabal", "After you activate a system: your ships don't roll for gravity rifts during this movement. The note goes back to the Vuil'raith."},
    {"keleres", "After an agenda is revealed: you don't vote on it, but predict its outcome. If you're right, draw 1 action card and gain 2 trade goods. The note goes back to the Keleres."},
    {"bastion", "At the end of a combat: galvanize 1 of your units that took part. The note goes back to the Last Bastion."},
    {"ralnel", "After you activate a system: move your structures from adjacent systems without your command tokens onto your planets in it. The note goes back to the Ral Nel."},
    {"deepwrought", "Action: play it face up and gain one of the Deepwrought's technologies (not a faction technology or unit upgrade) until the end of the status phase; then the technology and the note go back."},
    {"crimson", "Action: play it face up and pick a system with your units: its wormholes do nothing during movement until the end of the status phase; then the note goes back."},
    {"firmament", "When you receive it: the Firmament may put a facedown plot card with your control token in their play area. Then you gain 2 command tokens and 2 trade goods, and the note is purged."},
    {"obsidian", "At the end of one of your tactical actions: you may pay 1 influence to give it to a neighbour (even as the Obsidian). At the end of the status phase, its holder - unless the Obsidian - loses 1 fleet token."},
    {"kortali", "Action: play it face up and take 1 of your command tokens back from the board. It goes back to the Kortali at the start of the status phase."},
    {"vaylerian", "At the start of a space combat round: the Vaylerian discard 1 action card; your ships move to a neighbouring system without other ships (a command token there) and the combat is a draw. The note goes back."},
    {"lanefir", "After you win a combat: play it face up to explore one of your planets. It goes back at the start of the Lanefir's turn."},
    {"kyro", "After an agenda is revealed: you don't vote on it, but predict its outcome. If you're right, put 3 infantry on one of your planets. The note goes back to the Kyro."},
    {"cheiran", "When one of your non-fighter ships is destroyed in a combat: put 2 fighters in that system. The note goes back to the Cheiran."},
    {"celdauri", "Goes face up: you can use the Celdauri commander (once unlocked). It goes back if you activate a system with Celdauri units."},
    {"khrask", "During your tactical action, when your bombardment hits: turn any of those hits into infantry landing on that planet. The note goes back to the Khrask."},
    {"zealots", "When you ignore a prerequisite while researching: ignore 1 more of the same colour. The note goes back to the Rhodun."},
    {"zelian", "When your bombardment makes hits: 1 hit more. The note goes back to the Zelian."},
    {"rohdhna", "At the end of your turn: attach it for good to one of your non-home planets (not Mecatol Rex) - that planet has Production 3."},
    {"tnelis", "When you draw secret objectives: draw 1 more, then shuffle 1 of yours back into the deck. The note goes back to the Tnelis."},
    {"kolume", "At the start of an invasion: play it - in this invasion other players' units lose Space Cannon and Planetary Shield. It goes back at the start of your next turn."},
    {"bentor", "At the end of your turn: attach it for good to one of your non-home planets - that planet has a technology specialty of the colour you choose."},
    {"dihmohn", "When one of your ships would be destroyed: it isn't - the note goes face up instead, and back to the Dih-Mohn at the start of the status phase."},
    {"florzen", "When you would gain a relic: draw 1 more, keep 1, shuffle the other back. The note goes back to the Florzen."},
    {"gledge", "At the end of your turn: attach it for good to one of your non-home planets - that planet has 2 more resources."},
    {"augers", "Goes face up: you know the objective on the Augurs' faction sheet. It goes back if you activate a system with their units."},
    {"kjalengard", "At the start of a space combat: +1 to your fighters' combat rolls this combat; the Kjalengard capture your fighters destroyed in it. The note goes back."},
    {"kollecc", "At the start of your turn: look at the top card of an exploration deck. The note goes back to the Kollecc."},
    {"mortheus", "At the start of a space combat you defend: pay influence equal to the cost of one of the attacker's ships to put one of that type in. The note goes back."},
    {"vaden", "After you activate a system: each trade good you spend lets one of your non-fighter ships move 1 further. The note goes back."},
    {"cymiae", "Action: take 1 action card from the discard pile. Then the note is purged."},
    {"nivyn", "After you activate a system: ignore every anomaly during this activation. The note goes back."},
    {"olradin", "Action: play it face up - exhaust one of your planets and ready one of your non-home planets (not Mecatol Rex). It goes back at the start of the status phase."},
    {"freesystems", "When you take a planet in a tactical action with no other ships there: produce up to 2 ships in that system. The note goes back."},
    {"edyn", "After an agenda is revealed: you don't vote on it, but predict its outcome. If you're right, another player's command token goes into a system with your units. The note goes back."},
    {"nokar", "At the start of a space combat: this combat you may retreat to any system with a planet you control. The note goes back."},
    {"axis", "When your units produce: +4 Production and 1 cheaper. It goes back at the start of the status phase."},
    {"mykomentori", "Action: play it face up - once per turn you may roll one of your dice again. It goes back if you activate a system with their units."},
    {"ghemina", "After you activate a system: one of your non-fighter ships gains capacity 2 and Bombardment 5 (x2) this action. The note goes back."},
    {"lizho", "At the start of a space combat you take part in: 2 fighters into the active system. The note goes back."},
    {"mirveda", "At the start of your turn: move up to 2 of your PDS to a planet you control. The note goes back."},
    {"ghoti", "At the start of your turn: produce up to 2 ships in a system with your ships. It goes back at the start of the status phase."},
};

}  // namespace

// Discordant Stars batches (0.37.0 = 1): a game's random pools only use the batches it was started with.
int dsBatch(int faction) {
    if (faction < 0 || faction >= kFactionCount || kFactions[faction].source != Source::Discordant || !factionImplemented(faction)) return 0;
    static const char* kBatch1[] = {"kortali", "vaylerian", "lanefir", "kyro", "cheiran", "celdauri"};
    static const char* kBatch2[] = {"khrask", "zealots", "zelian", "rohdhna", "tnelis", "kolume"};
    static const char* kBatch3[] = {"bentor", "dihmohn", "florzen", "gledge", "augers", "kjalengard"};
    static const char* kBatch4[] = {"kollecc", "mortheus", "vaden", "cymiae", "nivyn", "olradin"};
    static const char* kBatch5[] = {"freesystems", "edyn", "nokar", "axis", "mykomentori", "ghemina"};
    static const char* kBatch6[] = {"veldyr", "lizho", "mirveda", "ghoti"};
    for (const char* k : kBatch1)
        if (!std::strcmp(k, kFactions[faction].key)) return 1;
    for (const char* k : kBatch2)
        if (!std::strcmp(k, kFactions[faction].key)) return 2;
    for (const char* k : kBatch3)
        if (!std::strcmp(k, kFactions[faction].key)) return 3;
    for (const char* k : kBatch4)
        if (!std::strcmp(k, kFactions[faction].key)) return 4;
    for (const char* k : kBatch5)
        if (!std::strcmp(k, kFactions[faction].key)) return 5;
    for (const char* k : kBatch6)
        if (!std::strcmp(k, kFactions[faction].key)) return 6;
    return 99;
}
bool factionInPool(int faction, int dsLevel) {
    if (!factionImplemented(faction)) return false;
    if (faction >= 0 && faction < kFactionCount && kFactions[faction].source == Source::Discordant) {
        const int b = dsBatch(faction);
        return b > 0 && b <= dsLevel;
    }
    return true;
}

bool factionImplemented(int faction) {
    if (faction < 0 || faction >= kFactionCount) return false;
    static std::vector<int8_t> done;  // looked up once (this is called very often)
    if (done.empty()) {
        done.assign(size_t(kFactionCount), 0);
        for (int f = 0; f < kFactionCount; ++f)
            for (const char* k : kDone)
                if (!std::strcmp(k, kFactions[f].key)) done[size_t(f)] = 1;
    }
    return done[size_t(faction)] != 0;
}

const char* abilityText(int faction, int i) {
    if (faction < 0 || faction >= kFactionCount || i < 0 || i > 2 || !kFactionInfo[faction].abilities[i]) return "";
    return FIND(kAbility, kFactionInfo[faction].abilities[i]);
}

const KT kLeaderOmega[] = {
    {"naaluagent", "Action: exhaust and choose a player (you too): they take a tactical action in a non-home system without placing a command token; the system still counts as activated."},
    {"yinagent", "After any player's unit is destroyed in a combat: exhaust; that player puts 2 fighters in that system (a ship) or 2 infantry on that planet (a ground force)."},
    {"yincommander", "Counts as a green technology for prerequisites. Researching a technology another player owns, you may return 1 infantry to ignore all its prerequisites."},
    {"xxchahero", "Once unlocked: each planet you exhaust pays its resources and influence together, counted as either."},
};
const char* leaderText(int leader) {
    if (leader < 0 || leader >= kLeaderCount) return "";
    if (teXxchaHeroTexts() && !std::strcmp(kLeaders[leader].key, "xxchahero"))  // (0.57.1: Thunder's Edge's version)
        return "Action: place up to 4 PDS or mechs, in any mix, on planets you control, and ready each planet you put a unit on. "
               "Then purge this card.";
    if (codexTexts())
        for (const KT& k : kLeaderOmega)
            if (!std::strcmp(k.key, kLeaders[leader].key)) return k.text;
    return FIND(kLeaderText, kLeaders[leader].key);
}
const char* leaderUnlock(int leader) {
    if (leader < 0 || leader >= kLeaderCount) return "";
    if (kLeaders[leader].type == kAgent) return "Ready from the start of the game.";
    if (kLeaders[leader].type == kHero) return "Have 3 scored objectives.";
    return FIND(kUnlock, kLeaders[leader].key);
}

const char* unitAbilityText(int unit) {
    if (unit < 0 || unit >= kUnitCount) return "";
    if (codexTexts() && !std::strcmp(kUnits[unit].key, "naalu_mech"))
        return "Other players can't use anti-fighter barrage against your units in this system.";
    return FIND(kUnitText, kUnits[unit].key);
}

const char* factionNoteName(int faction, int type) {
    if (faction < 0 || faction >= kFactionCount) return "";
    if (type == kNoteAlliance) return "Alliance";
    if (type == kNoteFaction && codexTexts()) {  // 0.43.3: the Codex's Omega notes carry the sign
        static const char* kOmegaNotes[][2] = {{"letnev", "War Funding \xCE\xA9"},        {"yin", "Greyfire Mutagen \xCE\xA9"},
                                               {"arborec", "Stymie \xCE\xA9"},            {"winnu", "Acquiescence \xCE\xA9"},
                                               {"l1z1x", "Cybernetic Enhancements \xCE\xA9"}};
        for (const auto& n : kOmegaNotes)
            if (!std::strcmp(kFactions[faction].key, n[0])) return n[1];
    }
    // (0.44.1: the Veldyr before the generic faction note - their first Branch Office, Tax Haven, had no name)
    if (!std::strcmp(kFactions[faction].key, "veldyr") && type >= kNoteFaction && type <= kNoteFaction4) {  // Discordant Stars (0.42.0)
        static const char* kBranch[4] = {"Branch Office: Tax Haven", "Branch Office: Broadcast Hub", "Branch Office: Reserve Bank",
                                         "Branch Office: Orbital Shipyard"};
        return kBranch[type - kNoteFaction];
    }
    if (type == kNoteFaction) return FIND(kNoteName, kFactions[faction].key);
    if (type == kNoteFaction2 && !std::strcmp(kFactions[faction].key, "empyrean")) return "Blood Pact";
    return "";
}

const char* factionNoteText(int faction, int type) {
    if (faction < 0 || faction >= kFactionCount) return "";
    if (type == kNoteAlliance)
        return "Goes face up in your play area: while you hold it you can use its owner's commander ability (once their commander is unlocked).";
    if (type == kNoteFaction && codexTexts()) {
        if (!std::strcmp(kFactions[faction].key, "letnev"))
            return "After both sides roll in a space combat: reroll all of your opponent's dice and any of your misses. The note goes back to Letnev.";
        if (!std::strcmp(kFactions[faction].key, "arborec"))
            return "After another player moves ships into a system with your units: put 1 of their command tokens in any non-home system. The note goes back to Arborec.";
    }
    if (!std::strcmp(kFactions[faction].key, "veldyr") && type >= kNoteFaction && type <= kNoteFaction4)  // Discordant Stars (0.42.0)
        return type <= kNoteFaction2
                   ? "When you receive it: attach it to one of your non-home planets (not Mecatol Rex) without a Branch Office - +1 influence."
                   : "When you receive it: attach it to one of your non-home planets (not Mecatol Rex) without a Branch Office - +1 resource.";
    if (type == kNoteFaction) return FIND(kNoteText, kFactions[faction].key);
    if (type == kNoteFaction2 && !std::strcmp(kFactions[faction].key, "empyrean"))
        return "Action: play it face up. When you and the Empyrean vote for the same outcome, you cast 4 more votes. It goes back if you activate a system with their units.";
    return "";
}

}  // namespace ti
