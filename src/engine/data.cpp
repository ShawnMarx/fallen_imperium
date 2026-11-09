#include "engine/data.h"

#include <cstring>

namespace ti {

#include "engine/gen/gamedata.inc"

const int kPlanetCount = int(sizeof(kPlanets) / sizeof(kPlanets[0]));
const int kSystemCount = int(sizeof(kSystems) / sizeof(kSystems[0]));
const int kUnitCount = int(sizeof(kUnits) / sizeof(kUnits[0]));
const int kTechCount = int(sizeof(kTechs) / sizeof(kTechs[0]));
const int kFactionCount = int(sizeof(kFactions) / sizeof(kFactions[0]));
const int kObjectiveCount = int(sizeof(kObjectives) / sizeof(kObjectives[0]));
const int kLayoutCount = int(sizeof(kLayouts) / sizeof(kLayouts[0]));
const int kActionCardCount = int(sizeof(kActionCards) / sizeof(kActionCards[0]));
const int kAgendaCount = int(sizeof(kAgendas) / sizeof(kAgendas[0]));
const int kExploreCardCount = int(sizeof(kExploreCards) / sizeof(kExploreCards[0]));
const int kRelicCount = int(sizeof(kRelics) / sizeof(kRelics[0]));
const int kLeaderCount = int(sizeof(kLeaders) / sizeof(kLeaders[0]));

int findLeader(const char* key) {
    for (int i = 0; i < kLeaderCount; ++i)
        if (!std::strcmp(kLeaders[i].key, key)) return i;
    return -1;
}

int findSystem(const char* key) {
    for (int i = 0; i < kSystemCount; ++i)
        if (!std::strcmp(kSystems[i].key, key)) return i;
    return -1;
}
int findTech(const char* key) {
    for (int i = 0; i < kTechCount; ++i)
        if (!std::strcmp(kTechs[i].key, key)) return i;
    return -1;
}
int findUnit(const char* key) {
    for (int i = 0; i < kUnitCount; ++i)
        if (!std::strcmp(kUnits[i].key, key)) return i;
    return -1;
}
int findFaction(const char* key) {
    for (int i = 0; i < kFactionCount; ++i)
        if (!std::strcmp(kFactions[i].key, key)) return i;
    return -1;
}
int findObjective(const char* key) {
    for (int i = 0; i < kObjectiveCount; ++i)
        if (!std::strcmp(kObjectives[i].key, key)) return i;
    return -1;
}

int findActionCard(const char* key) {
    for (int i = 0; i < kActionCardCount; ++i)
        if (!std::strcmp(kActionCards[i].key, key)) return i;
    return -1;
}
int findAgenda(const char* key) {
    for (int i = 0; i < kAgendaCount; ++i)
        if (!std::strcmp(kAgendas[i].key, key)) return i;
    return -1;
}

int findExploreCard(const char* key) {
    for (int i = 0; i < kExploreCardCount; ++i)
        if (!std::strcmp(kExploreCards[i].key, key)) return i;
    return -1;
}
int findRelic(const char* key) {
    for (int i = 0; i < kRelicCount; ++i)
        if (!std::strcmp(kRelics[i].key, key)) return i;
    return -1;
}
int findPlanet(const char* key) {
    for (int i = 0; i < kPlanetCount; ++i)
        if (!std::strcmp(kPlanets[i].key, key)) return i;
    return -1;
}

int genericUnit(UnitType t) {
    static int cache[kUnitTypes];
    static bool ready = false;
    if (!ready) {
        for (int k = 0; k < kUnitTypes; ++k) {
            cache[k] = -1;
            for (int i = 0; i < kUnitCount; ++i)
                if (kUnits[i].type == UnitType(k) && kUnits[i].faction < 0 && kUnits[i].upgradeOf < 0) {
                    cache[k] = i;
                    break;
                }
        }
        ready = true;
    }
    return cache[int(t)];
}

int unitByKey(const char* key) {
    for (int i = 0; i < kUnitCount; ++i)
        if (!std::strcmp(kUnits[i].key, key)) return i;
    return -1;
}

const char* unitTypeName(UnitType t, bool plural) {
    static const char* one[] = {"Carrier", "Cruiser", "Destroyer", "Dreadnought", "Fighter", "Flagship",
                                "War Sun", "Infantry", "Mech", "PDS", "Space Dock", "Second flagship"};
    static const char* many[] = {"Carriers", "Cruisers", "Destroyers", "Dreadnoughts", "Fighters", "Flagships",
                                 "War Suns", "Infantry", "Mechs", "PDS", "Space Docks", "Second flagships"};
    int i = int(t);
    if (i < 0 || i >= kUnitTypes) return "?";
    return plural ? many[i] : one[i];
}

const char* unitTypeShort(UnitType t) {
    static const char* s[] = {"CV", "CA", "DD", "DN", "FF", "FS", "WS", "IN", "MC", "PD", "SD", "FS2"};
    int i = int(t);
    return (i >= 0 && i < kUnitTypes) ? s[i] : "?";
}

const char* traitName(Trait t) {
    switch (t) {
        case Trait::Cultural: return "Cultural";
        case Trait::Hazardous: return "Hazardous";
        case Trait::Industrial: return "Industrial";
        default: return "";
    }
}

const char* colorName(TechColor c) {
    switch (c) {
        case TechColor::Blue: return "Propulsion";
        case TechColor::Green: return "Biotic";
        case TechColor::Red: return "Warfare";
        case TechColor::Yellow: return "Cybernetic";
        default: return "Faction";
    }
}

const char* factionShortName(int f) {
    static const struct {
        const char* key;
        const char* name;
    } names[] = {{"arborec", "Arborec"}, {"ghost", "Creuss"},   {"hacan", "Hacan"},       {"jolnar", "Jol-Nar"},
                 {"l1z1x", "L1Z1X"},     {"letnev", "Letnev"},  {"mentak", "Mentak"},     {"muaat", "Muaat"},
                 {"naalu", "Naalu"},     {"nekro", "Nekro"},    {"saar", "Saar"},         {"sardakk", "Sardakk N'orr"},
                 {"sol", "Sol"},         {"winnu", "Winnu"},    {"xxcha", "Xxcha"},       {"yin", "Yin"},
                 {"yssaril", "Yssaril"}, {"argent", "Argent"},  {"cabal", "Vuil'raith"},  {"empyrean", "Empyrean"},
                 {"mahact", "Mahact"},   {"nomad", "Nomad"},    {"naaz", "Naaz-Rokha"},   {"titans", "Titans of Ul"},
                 {"keleres", "Keleres"}, {"bastion", "Last Bastion"}, {"ralnel", "Ral Nel"}, {"crimson", "Crimson"},
                 {"deepwrought", "Deepwrought"}, {"firmament", "Firmament"}, {"obsidian", "Obsidian"}};
    if (f < 0 || f >= kFactionCount) return "?";
    for (const auto& n : names)
        if (!std::strcmp(n.key, kFactions[f].key)) return n.name;
    return kFactions[f].name;
}

// ---------------------------------------------------------------------------
// Technology descriptions (our own short wording).
namespace {
struct KeyText {
    const char* key;
    const char* text;
    bool done;  // effect implemented in the rules engine
};

const KeyText kTechText[] = {
    {"amd", "Asteroid fields don't stop your ships. Space cannon fired at your units by other players rolls 1 lower.", true},
    {"gd", "After you activate a system, one of your ships gets +1 move for that tactical action.", true},
    {"fl", "On each of your turns in the action phase you may take 2 actions instead of 1.", true},
    {"lwd", "Other players' ships don't block your ships' movement through their systems.", true},
    {"det", "After your tactical action: explore the frontier token in the active system if your ships are there. Your ships may also retreat into adjacent systems without other players' units.", true},
    {"sr", "Action: exhaust it - 1 ship is built in a system with one of your space docks (paid as usual).", true},
    {"nm", "Draw 1 extra action card in each status phase.", true},
    {"dxa", "When you win a ground combat: you may put 1 of your infantry from reinforcements on that planet.", true},
    {"hm", "Gain 1 extra command token during each status phase.", true},
    {"x89_base", "After your bombardment destroys at least 1 enemy infantry on a planet, you may destroy all enemy infantry there.", true},
    {"pa", "Use technology specialties without exhausting planets, even exhausted ones. In the action phase, exhaust specialty planets for 1 trade good each.", true},
    {"bs", "At the end of your turn, exhaust to ready one of your tech-specialty planets or another technology.", true},
    {"ps", "When your units use bombardment or space cannon, one of them rolls 1 extra die.", true},
    {"md_base", "Exhaust at the start of a ground combat round on a planet with your planetary-shield unit: your opponent rolls no dice that round.", true},
    {"da", "Each combat round, after hits are assigned, repair 1 of your damaged units that did not take damage this round.", true},
    {"asc", "Space combat start, with 3 or more non-fighter ships of yours there: the other side loses 1 non-fighter ship of their choice.", true},
    {"aida", "Exhaust to ignore 1 prerequisite of a unit upgrade, or to cut production cost by your number of unit upgrades.", true},
    {"sar", "After producing, exhaust to place a mech on a planet you control in that system; gain a trade good when one of your mechs is destroyed.", true},
    {"st", "When your units produce, the combined cost is reduced by 1.", true},
    {"gls", "Exhaust before your space cannon fires: its hits must be assigned to non-fighter ships if possible.", true},
    {"td", "At the start of your turn, exhaust to move up to 4 of your ground forces (from planets or space) onto planets you control.", true},
    {"ie", "After you gain control of a planet, you may produce units there with combined cost up to its resource value.", true},
    {"sdn", "When you activate a system, you may explore a planet there that holds your units.", true},
    {"pi", "At the end of your turn, exhaust to rearrange your command tokens. When voting in the agenda phase, you may cast 3 extra votes (exhausted if that outcome loses).", true},
    // Faction technologies (Stage 5, batch 1)
    {"l4", "During your invasions, other players can't use space cannon against your units.", true},
    {"nes", "Sustain Damage on your units soaks up 2 hits each time instead of 1.", true},
    {"qdn", "At the end of the strategy phase, you may spend 1 strategy token and give another player 3 trade goods to swap one of your strategy cards for one of theirs.", true},
    {"pm", "Action: exhaust and spend 1 strategy token to gain 4 trade goods; another player of your choice gains 2.", true},
    {"ers", "After another player activates a system that contains your ships, gain 4 trade goods.", true},
    {"scc", "After you activate a system with your units, exhaust: this activation, it counts as adjacent to every system with your units.", true},
    {"vpw", "After the ground combat rolls, if your opponent scored at least 1 hit, you score 1 extra hit.", true},
    {"it", "When another player plays an action card, you may exhaust this and spend 1 strategy token to cancel it.", true},
    {"nf", "After another player activates a system with your ships, you may exhaust this and spend 1 strategy token to end their turn.", true},
    // Faction technologies (Stage 5, batch 2)
    {"bio", "At the end of the status phase, you may move any of your infantry from planets you control to your planets in the same or adjacent systems.", true},
    {"is", "When you research a technology, you may exhaust this and spend 2 resources to ignore all its prerequisites.", true},
    {"mc", "Each trade good you spend is worth 2 resources or 2 influence.", true},
    {"so", "After a space combat you were in, gain 1 trade good; if you won, you may produce 1 ship there of a type destroyed in that combat.", true},
    {"ng", "After another player activates a system with your ships, they lose 1 token from their fleet pool.", true},
    {"yso", "After your units produce, put 1 infantry from your reinforcements on a planet you control in that system.", true},
    {"ic", "At the start of a space combat, you may destroy 1 of your cruisers or destroyers there to make 1 hit that must go to a non-fighter ship if possible.", true},
    {"tp", "During your turns, players who have passed can't play action cards.", true},
    {"mi", "Action: exhaust to look at another player's action cards and take 1 of them.", true},
    // Faction technologies (Stage 5, batch 3)
    {"wg", "Action: exhaust to place or move a Creuss wormhole token into a system with a planet you control, or a non-home system without other players' ships.", true},
    {"ds", "At the start of a space combat in a system with a wormhole and your ships, you may make 1 hit on one of your opponent's ships.", true},
    {"mr", "Supernovas are open to your ships as a destination (not to pass through); a supernova holding your units produces 5 for you.", true},
    {"cm", "Other players can't activate asteroid fields with your ships. At the start of your turn you may produce 1 unit in a system with your producing units.", true},
    {"lgf", "During your tactical actions, if you don't control Mecatol Rex, its system counts as an alpha and beta wormhole. Action: if you control it, exhaust to put 1 infantry there.", true},
    {"htp", "When your units produce, you may exhaust this to swap a planet's resources and influence for that production.", true},
    {"vax", "When you would gain another player's technology with a faction ability, you may put the X token on one of their faction technologies instead: this card then has that technology's text.", true},
    {"vay", "When you would gain another player's technology with a faction ability, you may put the Y token on one of their faction technologies instead: this card then has that technology's text.", true},
    {"as", "After you or a neighbour activates a system next to an anomaly, you may give that player's ships +1 move this tactical action.", true},
    {"vw", "After a player moves ships into a system with your units, they must give you 1 promissory note from their hand, if they can.", true},
    // Faction technologies (Stage 5, batch 4)
    {"ah", "Other players can't move ships through systems with your structures. Planets with your structures have Production 1.", true},
    {"gr", "Before a player votes, you may exhaust this: they must cast at least 1 vote for the outcome you choose, or lose a token from their fleet pool.", true},
    {"pfa", "After you explore a planet, ready it.", true},
    {"sc", "Exhaust when a combat round begins: every combat die of yours gets +1 that round.", true},
    {"tcs", "After any player's agent is exhausted, you may exhaust this to ready it. If it is another player's agent, you may then trade with that player (neighbours or not).", true},
    {"vtx", "Action: exhaust to capture 1 unit from another player's reinforcements, of a type they have next to one of your space docks.", true},
    // Council Keleres (Thunder's Edge, 0.21.0)
    {"asn", "Once per action, after one of your units uses Production, you may use the Production of another of your units, in any system.", true},
    {"executiveorder", "Action: exhaust to draw the top or bottom agenda; it is voted on now, as if you were the speaker, and your trade goods and resources count as votes.", true},
    // Thunder's Edge factions (0.35.0)
    {"proxima", "Ground combat round start: you may fire 3 dice (8+) at the enemy's ground forces, then the same at your own. Bombardment hits on your ground forces lose 1 per galvanized unit of yours there.", true},
    {"helios2", "Your space docks: the planet gets 2 more resources, Production = its resources + 4, up to 3 fighters need no capacity.", true},
    {"hydrothermal", "At the start of the status phase: gain 1 trade good for each ocean card in play.", true},
    {"radical", "At the start of the status phase: you may swap one of your technologies (not a unit upgrade) for one of its colour with exactly 1 more prerequisite.", true},
    {"subatomic", "When one of your ships is destroyed: you may produce one of the same type at a space dock in your home system.", true},
    {"exile2", "Unit upgrade (destroyer): Exile II - combat 7, anti-fighter barrage 6 (x3); after any combat within 2 systems it may open a breach (active or not).", true},
    {"nanomachines", "Action, exhausting it - one of three: a PDS on a planet of yours; every damaged unit of yours repaired; or an action card from your hand swapped for the top of the deck.", true},
    {"linkship2", "Unit upgrade (destroyer): Linkship II - move 4, anti-fighter barrage 6 (x3). Every Linkship can fire the space cannon of one of your structures carried in its space area.", true},
    {"planesplitter-firm", "When you gain it: the Fracture comes into play. It turns over when you become the Obsidian.", true},
    {"parasite-firm", "At the start of the status phase: you may put 1 infantry from your reinforcements on a planet you control in your home system. It turns over when you become the Obsidian.", true},
    {"planesplitter-obs", "When you perform a strategic action: you may move an ingress token into a system with or next to your units. (It can't be researched.)", true},
    {"parasite-obs", "At the start of your turn: destroy 1 of another player's infantry in or next to a system with your infantry. (It can't be researched.)", true},
    // Discordant Stars batch 1 (0.37.0)
    {"dskortg", "After you win a combat: exhaust it to take your command token back from that system.", true},
    {"dskortr", "Once per space combat, after one of your non-fighter ships is destroyed: 1 hit on one of your opponent's ships that isn't a carrier.", true},
    {"dsvaylr", "Once per action, after you win a ground combat: draw 1 action card.", true},
    {"dslaneb", "Action: spend a strategy token (and exhaust it) or purge a relic fragment to move 1 ship to a neighbouring system without other players' ships.", true},
    {"dslaner", "After you explore or purge relic fragments, a token goes on it. At the start of a combat round, take tokens off to reroll that many missed dice.", true},
    {"dskyrog", "In the agenda phase, after an outcome you voted for is resolved: put 2 infantry on one of your planets.", true},
    {"dskyroy", "At the start of the strategy phase: put any of your trade goods on strategy cards, then 2 from the supply on one.", true},
    {"dscheir", "After another player activates a system with your structure: exhaust to produce 1 ship there; each of your structures there you remove makes it 3 cheaper.", true},
    {"dsceldr", "At the end of your turn: exhaust it and spend a strategy token to remove your command token from a system with your space dock (you may activate it again).", true},
    // Discordant Stars batch 2 (0.38.0)
    {"dskhrag", "When you would spend a strategy token, you may exhaust one of your planets instead.", true},
    {"dsrhodb", "Once during your turn, when you exhaust planets with technology specialties: produce 1 ship in a system with one of them.", true},
    {"dsrhody", "When you activate a system, or one with your units is activated: one technology of another player there has no text until the end of the tactical action.", true},
    {"dszelir", "When your bombardment hits a planet, it makes 1 more hit.", true},
    {"dsrohdy", "At the start of the agenda phase: for each of your unit upgrades, choose a player - they must produce 1 ship at a space dock or war sun of theirs.", true},
    {"dstnely", "After a round of space combat in a system with your mech: damage it to make 1 hit on a cruiser, dreadnought or destroyer there.", true},
    {"dskolug", "When you pass, gain 1 commodity for each other player who has passed. Action: exhaust it.", true},
    {"dskolur", "After a system is activated: exhaust to give one player +1 or -1 to all their rolls this tactical action.", true},
    // Discordant Stars batch 3 (0.39.0)
    {"dsbentg", "Each of your planets with a technology specialty or an attachment has Production 1, as if it were a unit.", true},
    {"dsbenty", "When your units use Production: exhaust it - one of them matches the best Production on the board, or +2 to the total.", true},
    {"dsdihmy", "When you explore a planet: you may produce 1 ship in its system. Action: exhaust it and one of your planets to explore that planet.", true},
    {"dsflorg", "When another player would perform a strategic action: exhaust it and spend a strategy token - their turn ends, the action doesn't happen and the card stays ready.", true},
    {"dsgledb", "When you move: your non-fighter ships that carry no fighters or infantry may move 1 further.", true},
    {"dsaugug", "In the status phase you may score a second public objective instead of a secret objective.", true},
    {"dsauguy", "At the start of the status phase: you may spend 3 trade goods to research a technology.", true},
    {"dskjalg", "Once per action, after any hits in a combat round in a system with a Glory token: gain 1 trade good. After any player rolls combat dice: exhaust it to let them reroll their misses.", true},
    // Discordant Stars batch 4 (0.40.0)
    {"dskollb", "At the start of a space combat on your turn: a ship of yours that wasn't in the system at the start of the action rolls 1 die - a hit on its combat value or more.", true},
    {"dskolly", "Action: exhaust it - a neighbour with relic fragments gains 2 trade goods and gives you the fragment you choose.", true},
    {"dsmortr", "After one of your units is destroyed: exhaust it to put one of that type in a neighbouring system with your ships.", true},
    {"dsmorty", "Each system with your ships and no planets has Production 2, as if it were a unit.", true},
    {"dsvadeb", "After you activate a system: exhaust it - each trade good you spend lets one of your non-fighter ships move 1 further.", true},
    {"dsvadey", "After your hit is cancelled by Sustain Damage: exhaust to destroy that unit. After you make hits in a combat round: 1 trade good for 1 more hit.", true},
    {"dscymiy", "At the start of your turn: exhaust it to put another player's command token in a non-home system with your units (not Mecatol Rex).", true},
    {"dsnivyy", "After your units roll for combat: one of them with Sustain Damage becomes damaged to reroll its dice.", true},
    {"dsolrab", "After you activate a system: exhaust one of your planets sharing a trait with a planet there - +1 move to all your ships this action.", true},
    {"dsolrar", "At the start of your turn: exhaust it and one of your planets to ready a non-home planet (not Mecatol Rex).", true},
    // Discordant Stars batch 5 (0.41.0)
    {"dsfreeg", "At the start of the agenda phase you may exhaust a cultural, a hazardous and an industrial planet (anyone's). When you cast votes with a cultural, a hazardous and an industrial planet among those you exhaust: 4 more votes.", true},
    {"dsfreey", "At the start of a ground combat: up to 2 of your ground forces there fire first; each hit lands on the opponent's ground forces.", true},
    {"dsedyng", "Once per agenda phase, when your prediction comes true: you may score a public objective you qualify for; every other player who voted for it draws a secret objective.", true},
    {"dsedyny", "You vote last. When an outcome you voted for or predicted is resolved, each player who voted for it gains 1 commodity. Exhaust it to let 1 relic change hands in your transaction.", true},
    {"dsnokay", "In the action phase (not an action): exhaust it to use the Production of 1 of your units, then put a command token from your reinforcements in its system.", true},
    {"dsaxisb", "After you activate a system: exhaust it - one of your ships rolls: 1-3 it is lost, 4-10 it moves 2 further this action.", true},
    {"dsaxisy", "Action: exhaust it to place or move one of your space docks onto a planet you control without one.", true},
    {"dsmykog", "After an opponent's combat roll: exhaust it to roll their misses again - any hits land on their own units.", true},
    {"dsghemg", "After you win a space combat: ready 1 of your planets.", true},
    // Discordant Stars batch 6 (0.42.0)
    {"dsveldr", "After each round of space combat: one of your units in the system fires its space cannon at the opponent's ships.", true},
    {"dslizhb", "After you activate a non-home system: exhaust it - one of your ships counts as next to the active system this action.", true},
    {"dsmirvr", "At the start of an invasion of your system: one of your planets there gains Planetary Shield. When units land on your planet: exhaust it to destroy one.", true},
    {"dsghotg", "Up to 3 ships in your home system don't count against your fleet pool. When you vote: exhaust it for as many votes as your fleet tokens.", true},
    {"dsghoty", "When you produce: 1 more unit in each of up to 2 systems with your ships and your command token.", true},
};
}  // namespace

// 0.31.0: the Codex Omega wording, shown while a Codex game is on (setCodexTexts)
bool g_codexTexts = false;
bool g_codexLatest = true;  // 0.43.3: Thunder's Edge's Codex (the game's Setup::fixes >= 4)
namespace {
// 0.43.3 (owner): the Omega cards carry the sign after their names while the Codex is on - the cards Thunder's Edge's
// Codex puts in place of the originals: 3 faction technologies, Magen Defense Grid, X-89 Bacterial Weapon, 3 secret
// objectives, 5 promissory notes (factionNoteName), 1 mech and 6 leaders
struct OmegaName {
    char table;  // t technology, u unit, l leader, o objective
    const char* key;
    const char* omega;
    const char* plain;  // (filled in on the first switch)
};
OmegaName kOmegaNames[] = {
    {'t', "md_base", "Magen Defense Grid \xCE\xA9", nullptr},  {'t', "x89_base", "X-89 Bacterial Weapon \xCE\xA9", nullptr},
    {'t', "wg", "Wormhole Generator \xCE\xA9", nullptr},       {'t', "yso", "Yin Spinner \xCE\xA9", "Yin Spinner"},
    {'t', "mr", "Magmus Reactor \xCE\xA9", nullptr},           {'u', "naalu_mech", "Iconoclast \xCE\xA9", nullptr},
    {'l', "naaluagent", "Z'eu \xCE\xA9", nullptr},             {'l', "naalucommander", "M'aban \xCE\xA9", nullptr},
    {'l', "yinagent", "Brother Milor \xCE\xA9", nullptr},      {'l', "yincommander", "Brother Omar \xCE\xA9", nullptr},
    {'l', "yinhero", "Dannel of the Tenth \xCE\xA9", nullptr}, {'l', "xxchahero", "Xxekir Grom \xCE\xA9", nullptr},
    {'o', "mew", "Make an Example of Their World \xCE\xA9", nullptr},
    {'o', "ttfd", "Turn Their Fleets to Dust \xCE\xA9", nullptr},
    {'o', "fwp", "Fight with Precision \xCE\xA9", nullptr},
};
const char** omegaSlot(char table, const char* key) {
    switch (table) {
        case 't':
            for (int i = 0; i < kTechCount; ++i)
                if (!std::strcmp(kTechs[i].key, key)) return &kTechs[i].name;
            break;
        case 'u':
            for (int i = 0; i < kUnitCount; ++i)
                if (!std::strcmp(kUnits[i].key, key)) return &kUnits[i].name;
            break;
        case 'l':
            for (int i = 0; i < kLeaderCount; ++i)
                if (!std::strcmp(kLeaders[i].key, key)) return &kLeaders[i].name;
            break;
        default:
            for (int i = 0; i < kObjectiveCount; ++i)
                if (!std::strcmp(kObjectives[i].key, key)) return &kObjectives[i].name;
            break;
    }
    return nullptr;
}
}  // namespace
void setCodexTexts(bool on, bool latest) {
    g_codexTexts = on;
    g_codexLatest = latest;
    for (OmegaName& n : kOmegaNames) {
        const char** slot = omegaSlot(n.table, n.key);
        if (!slot) continue;
        if (!n.plain) n.plain = *slot;  // (Yin Spinner: the data's name says "Omega" - 0.44.6 shows the plain card's name)
        *slot = on ? n.omega : n.plain;
        // 0.44.6: the Codex IV versions (Thunder's Edge / the latest Codex) are Omega Omega
        if (on && latest && n.table == 't' && (!std::strcmp(n.key, "md_base") || !std::strcmp(n.key, "x89_base")))
            *slot = !std::strcmp(n.key, "md_base") ? "Magen Defense Grid \xCE\xA9\xCE\xA9" : "X-89 Bacterial Weapon \xCE\xA9\xCE\xA9";
    }
}
bool codexTexts() { return g_codexTexts; }
bool g_teXxchaTexts = false;
void setThundersEdgeTexts(bool xxchaHero) {
    g_teXxchaTexts = xxchaHero;
    if (const char** slot = omegaSlot('l', "xxchahero"))
        *slot = xxchaHero ? "Xxekir Grom" : g_codexTexts ? "Xxekir Grom \xCE\xA9" : "Xxekir Grom";
}
bool teXxchaHeroTexts() { return g_teXxchaTexts; }
// 0.44.9 (owner's answer P1): games without the Codex / Thunder's Edge play the original Wormhole Generator
bool g_wgOriginal = false;
void setWormholeGeneratorOriginal(bool on) { g_wgOriginal = on; }
std::string plainName(const char* name) {
    std::string s = name ? name : "";
    while (s.size() >= 2 && s.compare(s.size() - 2, 2, "\xCE\xA9") == 0) s.resize(s.size() - 2);  // (0.44.6: Omega Omega too)
    if (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

const char* techSummary(int t) {
    if (t < 0 || t >= kTechCount) return "";
    if (g_wgOriginal && !std::strcmp(kTechs[t].key, "wg"))
        return "Start of the status phase: you may put a Creuss wormhole token (or move one) into a system with a planet "
               "you control, or a non-home system without other players' ships.";
    if (g_codexTexts) {
        if (!std::strcmp(kTechs[t].key, "md_base"))
            return "Whenever a system with your structures is activated (by anyone), each structure there gains 1 infantry "
                   "beside it. A ground combat on a planet with your structures opens with 1 free hit on the enemy.";
        if (!std::strcmp(kTechs[t].key, "yso"))
            return "After you produce units, put up to 2 infantry from your reinforcements on any planets you control or in "
                   "space with any of your ships.";
        if (!std::strcmp(kTechs[t].key, "x89_base") && g_codexLatest)
            return "Your units' bombardment and ground combat rolls make twice as many hits. Each planet you bombard is exhausted.";
    }
    for (const auto& k : kTechText)
        if (!std::strcmp(k.key, kTechs[t].key)) return k.text;
    if (kTechs[t].unitUpgrade) return "Unit upgrade: improves the matching unit's stats.";
    return "Faction technology. Its special effect arrives with faction abilities (Stage 5).";
}

bool techImplemented(int t) {
    if (t < 0 || t >= kTechCount) return false;
    if (kTechs[t].unitUpgrade) return true;
    for (const auto& k : kTechText)
        if (!std::strcmp(k.key, kTechs[t].key)) return k.done;
    return false;
}

// ---------------------------------------------------------------------------
// Objective descriptions (our own short wording).
namespace {
const KeyText kObjText[] = {
    // Stage I (base)
    {"corner", "Control 4 planets that share the same trait.", true},
    {"develop", "Own 2 unit upgrade technologies.", true},
    {"diversify", "Own 2 technologies in each of 2 colours.", true},
    {"monument", "Spend 8 resources.", true},
    {"expand_borders", "Control 6 planets in non-home systems.", true},
    {"research_outposts", "Control 3 planets that have technology specialties.", true},
    {"intimidate", "Have ships in 2 systems next to the Mecatol Rex system.", true},
    {"lead", "Spend a total of 3 tokens from your tactic and/or strategy pools.", true},
    {"trade_routes", "Spend 5 trade goods.", true},
    {"sway_council", "Spend 8 influence.", true},
    // Stage I (PoK)
    {"amass_wealth", "Spend 3 influence, 3 resources and 3 trade goods.", true},
    {"build_defenses", "Have 4 or more structures on the board.", true},
    {"lost_outposts", "Control 2 planets that have attachments.", true},
    {"engineer_marvel", "Have your flagship or a war sun on the board.", true},
    {"deep_space", "Have units in 3 systems that contain no planets.", true},
    {"infrastructure", "Have structures on 3 planets outside your home system.", true},
    {"make_history", "Have units in 2 systems that contain a legendary planet, Mecatol Rex or an anomaly.", true},
    {"outer_rim", "Have units in 3 systems on the edge of the board, other than your home system.", true},
    {"push_boundaries", "Control more planets than each of 2 of your neighbours.", true},
    {"raise_fleet", "Have 5 or more non-fighter ships in one system.", true},
    // Stage II (base)
    {"centralize_trade", "Spend 10 trade goods.", true},
    {"conquer", "Control a planet in another player's home system.", true},
    {"brain_trust", "Control 5 planets that have technology specialties.", true},
    {"golden_age", "Spend 16 resources.", true},
    {"galvanize", "Spend a total of 6 tokens from your tactic and/or strategy pools.", true},
    {"manipulate_law", "Spend 16 influence.", true},
    {"master_science", "Own 2 technologies in each of 4 colours.", true},
    {"revolutionize", "Own 3 unit upgrade technologies.", true},
    {"subdue", "Control 11 planets in non-home systems.", true},
    {"unify_colonies", "Control 6 planets that share the same trait.", true},
    // Stage II (PoK)
    {"supremacy", "Have your flagship or a war sun in another player's home system or the Mecatol Rex system.", true},
    {"become_legend", "Have units in 4 systems that contain a legendary planet, Mecatol Rex or an anomaly.", true},
    {"command_armada", "Have 8 or more non-fighter ships in one system.", true},
    {"massive_cities", "Have 7 or more structures on the board.", true},
    {"control_borderlands", "Have units in 5 systems on the edge of the board, other than your home system.", true},
    {"vast_reserves", "Spend 6 influence, 6 resources and 6 trade goods.", true},
    {"vast_territories", "Have units in 5 systems that contain no planets.", true},
    {"protect_border", "Have structures on 5 planets outside your home system.", true},
    {"ancient_monuments", "Control 3 planets that have attachments.", true},
    {"distant_lands", "Control 2 planets that are each in or next to a different other player's home system.", true},
    // Secret (base)
    {"ans", "Own 2 faction technologies (Valefar Assimilators don't count).", true},
    {"btgk", "Have ships in a system with an alpha wormhole and ships in a system with a beta wormhole.", true},
    {"ctr", "Have ships in 6 systems.", true},
    {"csl", "Have ships in a system that contains another player's space dock.", true},
    {"dtgs", "Destroy another player's war sun or flagship.", true},
    {"eap", "Have 4 PDS on the board.", true},
    {"faa", "Control 4 cultural planets.", true},
    {"fsn", "Discard 5 action cards.", true},
    {"fwm", "Have 3 space docks on the board.", true},
    {"gamf", "Have 5 dreadnoughts on the board.", true},
    {"lsc", "Have ships in 3 systems that are each next to an anomaly.", true},
    {"mew", "Destroy the last of a player's ground forces on a planet with bombardment.", true},
    {"mlp", "Own 4 technologies of the same colour.", true},
    {"mrm", "Control 4 hazardous planets.", true},
    {"mp", "Control 4 industrial planets.", true},
    {"ose", "Control Mecatol Rex and have 3 or more ships in its system.", true},
    {"sar", "Win a combat against a player who has the most victory points.", true},
    {"te", "Have ships in a system next to another player's home system.", true},
    {"ttfd", "Destroy the last of a player's non-fighter ships in the active system with space cannon fire.", true},
    {"uf", "Win a space combat in a system containing your flagship, without losing the flagship.", true},
    // Secret (PoK)
    {"bam", "Lose control of a planet in a home system.", true},
    {"baf", "Win a combat against a player whose promissory note you held at the start of your tactical action.", true},
    {"btv", "Win a combat in an anomaly.", true},
    {"dts", "Win a combat in another player's home system.", true},
    {"dfat", "Have units in the wormhole nexus.", true},
    {"dyp", "Have 3 or more non-fighter ships in the active system at the end of a space combat.", true},
    {"dhw", "Purge 2 of your relic fragments.", true},
    {"dp", "There are 3 or more laws in play.", true},
    {"dtd", "You, or a planet you control, are elected by an agenda.", true},
    {"eh", "Control planets with a combined influence of at least 12.", true},
    {"fwp", "Destroy the last of a player's fighters in the active system with anti-fighter barrage.", true},
    {"fc", "Be neighbours with all other players.", true},
    {"hrm", "Control planets with a combined resource value of at least 12.", true},
    {"mtm", "Have 1 mech on each of 4 planets.", true},
    {"otf", "Have 9 or more ground forces on a planet without one of your space docks.", true},
    {"pem", "Have units with combined production of at least 8 in one system.", true},
    {"pe", "Be the last player to pass in a game round.", true},
    {"sai", "Control a legendary planet.", true},
    {"syc", "Control a planet in a system that contains a planet controlled by another player.", true},
    {"sb", "Have another player's promissory note in your play area.", true},
};
}  // namespace

const char* objectiveSummary(int o) {
    if (o < 0 || o >= kObjectiveCount) return "";
    if (g_codexTexts) {  // the Omega versions (0.43.3): whatever destroys them during that step counts
        if (!std::strcmp(kObjectives[o].key, "mew")) return "Destroy the last of a player's ground forces on a planet during the bombardment step.";
        if (!std::strcmp(kObjectives[o].key, "ttfd"))
            return "Destroy the last of a player's non-fighter ships in the active system during the space cannon offense step.";
        if (!std::strcmp(kObjectives[o].key, "fwp"))
            return "Destroy the last of a player's fighters in the active system during the anti-fighter barrage step.";
    }
    for (const auto& k : kObjText)
        if (!std::strcmp(k.key, kObjectives[o].key)) return k.text;
    return "";
}

bool objectiveImplemented(int o) {
    if (o < 0 || o >= kObjectiveCount) return false;
    for (const auto& k : kObjText)
        if (!std::strcmp(k.key, kObjectives[o].key)) return k.done;
    return false;
}

// ---------------------------------------------------------------------------
const char* strategyName(int c) {
    static const char* n[] = {"Leadership", "Diplomacy", "Politics", "Construction",
                              "Trade",      "Warfare",   "Technology", "Imperial"};
    return (c >= 0 && c < kStrategyCards) ? n[c] : "?";
}

// 0.43.3 (tester): with Thunder's Edge, its revised Construction and Warfare (set when a game starts or opens)
bool g_teStrategy = false;
void setStrategyTexts(bool thundersEdge) { g_teStrategy = thundersEdge; }

const char* strategyPrimary(int c) {
    if (g_teStrategy && c == 3)
        return "Either place a PDS or space dock on a planet you control, or produce at one of your space docks. Then place 1 more PDS "
               "or space dock.";
    if (g_teStrategy && c == 5)
        return "Rearrange your command tokens, make a tactical action without placing a command token (in any system, even one "
               "with your token - it still counts as activated), then rearrange them again.";
    static const char* t[] = {
        "Gain 3 command tokens, then buy more at 3 influence each.",
        "Pick a system with your planet (not Mecatol Rex): other players can't activate it. Ready up to 2 of your planets.",
        "Choose a new speaker (another player), draw 2 action cards, then look at the top 2 agendas and put each on top or bottom.",
        "Place a PDS or space dock on a planet you control, then you may place another PDS.",
        "Gain 3 trade goods and refill your commodities; you may let other players refill theirs.",
        "Take back one of your command tokens from the board, gain 1 token and rearrange your tokens.",
        "Research 1 technology; you may pay 6 resources to research a second.",
        "Score a public objective you qualify for. Then gain 1 victory point if you control Mecatol Rex, otherwise draw a secret objective.",
    };
    return (c >= 0 && c < kStrategyCards) ? t[c] : "";
}

const char* strategySecondary(int c) {
    if (g_teStrategy && c == 3) return "Spend a strategy token to place a PDS or space dock on a planet you control.";
    if (g_teStrategy && c == 5) return "Spend a strategy token to use the Production of your units in your home system.";
    static const char* t[] = {
        "Buy command tokens at 3 influence each (no strategy token needed).",
        "Spend a strategy token to ready up to 2 of your planets.",
        "Spend a strategy token to draw 2 action cards.",
        "Spend a strategy token, placing it in any system, to put a PDS or space dock on your planet there.",
        "Spend a strategy token to refill your commodities.",
        "Spend a strategy token to produce at one space dock in your home system.",
        "Spend a strategy token and 4 resources to research 1 technology.",
        "Spend a strategy token to draw a secret objective.",
    };
    return (c >= 0 && c < kStrategyCards) ? t[c] : "";
}

// Thunder's Edge galactic events (0.36.0), in our own words; done = playable (ticked on the Modules screen)
const GalacticEventDef& galacticEvent(int ev) {
    static const GalacticEventDef k[kEventCount] = {
        {"commerce", "Age of Commerce",
         "Anyone may trade with anyone. Commodities have no limit: refilling adds your commodity value to what you have. A deal may "
         "share a technology (not a faction one): the receiver gains it too.", true},
        {"void", "Call of the Void",
         "Activating a system in the Fracture gives your ships +1 move; moving units into a Fracture system gains you 1 command token.", true},
        {"wilds", "Dangerous Wilds",
         "Neutral infantry guard each hazardous planet (as many as its resources) and come back each round while nobody holds it. "
         "The first to take one may research a technology, ignoring as many prerequisites as its resources.", true},
        {"hidden", "Hidden Agenda",
         "Votes are secret: only the totals are shown once everyone has voted (the Argent's votes are seen first). No deals during "
         "the agenda phase.", true},
        {"exploration", "Age of Exploration",
         "A relic takes 2 matching fragments instead of 3 (the Naaz-Rokha's need not match). Dark Energy Tap gains an action: "
         "exhaust it to add a new system beside a non-home edge system with your ships.", true},
        {"civilized", "Civilized Society",
         "All public objectives start face up and any number can be scored in a status phase. The game ends at the end of the "
         "status phase in which someone reaches the goal: most points wins (ties: planets' influence plus trade goods).", true},
        {"minor", "Minor Factions",
         "Home systems of factions not in play sit in the second ring, guarded by neutral infantry; their planets have all three "
         "traits. Hold every planet of one to use that faction's Alliance (commander) ability.", true},
        {"atomics", "Stellar Atomics",
         "Each player starts with an atomics token. Action: spend it to destroy every ground force and structure on any non-home "
         "planet. Without it you can't vote or play action cards in the agenda phase.", true},
        {"fighters", "Age of Fighters",
         "Everyone starts with Fighter II (the Naalu with theirs). Fighters that count against your fleet pool can each carry 1 "
         "ground force. Non-fighter ships that are destroyed are purged.", true},
        {"totalwar", "Total War",
         "Destroying other players' units puts commodities equal to their cost (infantry and fighters 1) on a planet of your home "
         "system. Taking a planet with commodities lets you move them home. Action: discard 10 of them for 1 victory point.", true},
        {"wild", "Wild, Wild Galaxy",
         "Action cards hit harder: Direct Hit on mechs, Flank Speed +2, Maneuvering Jets cancels every space cannon hit, Morale "
         "Boost +2, Sabotage takes the card, Shields Holding in ground combat, Skilled Retreat without a token, War Machine -5, "
         "Diplomatic Pressure 3 notes; Stellar Converter and Nova Seed anywhere.", true},
        {"wormholes", "Weird Wormholes",
         "A ship that moves through an alpha, beta or gamma wormhole rolls: 1-5 it becomes the next smaller ship, 6-10 the next "
         "bigger one (fighter, destroyer, cruiser, dreadnought, carrier, flagship, war sun), from your reinforcements.", true},
        {"monuments", "Monuments to the Ages",
         "Placing a structure on a non-home planet, you may pay 5 trade goods for a monument instead (3 in play at most). Each "
         "status phase puts 1 commodity under each; every 3 is a victory point for the planet's holder. A new holder may destroy it.", true},
        {"cosmic", "Cosmic Phenomenae",
         "Defenders in nebulae get +3; Fighter I can't fight in asteroid fields; Production in or next to a supernova +1; ships "
         "leaving a gravity rift may take +1 move (lost on 1-5); entropic scars are adjacent to each other.", true},
        {"cultural", "Cultural Exchange Program",
         "Each player plays with the leaders of another faction in the game, its commander unlocked (the Firmament take The "
         "Obsidian relic instead).", false},
        {"mercenaries", "Mercenaries for Hire",
         "The Alliances of factions not in play form a deck. Action: pay 3 trade goods to take the top one and use that faction's "
         "commander ability.", true},
        {"mobilization", "Rapid Mobilization",
         "A quick start: the Fracture is in play; everyone puts an infantry on each planet next to home, a space dock with their "
         "flagship and 3 fighters, gains their breakthrough and researches a technology (the Nomad two).", true},
        {"orthodoxy", "Zealous Orthodoxy",
         "The first player to score 2 secret objectives gains 1 more victory point, and every player gains that faction's Alliance "
         "ability.", true},
        {"warsun", "Advent of the War Sun",
         "Everyone starts with War Sun; the Muaat instead get their commander unlocked and a second war sun at home (and lose "
         "Fires of the Gashlai).", true},
        {"conventions", "Conventions of War Abandoned",
         "Each bombardment hit destroys 3 units. X-89 Bacterial Weapon gains an action: exhaust it to purge a planet in a system "
         "with your bombarding units. A player whose home planets are all purged is out.", true},
    };
    static const GalacticEventDef none{"", "", "", false};
    return ev >= 0 && ev < kEventCount ? k[ev] : none;
}

}  // namespace ti
