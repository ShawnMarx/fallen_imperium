// Faction ground units (0.44.0, the owner's "Ground Combat Animations Overhaul",
// docs/owner/ground_overhaul/groundcombat.txt + his inspiration pictures there):
// the soldiers, mechs, landed fighters and a few structures of 21 factions
// drawn in their own shapes on the ground-combat screen only (not in space
// combat, not on the map). The game's own soldiers are the "Default"; Options
// > "Ground combat" switches every faction back to them.
//
// Everything here is drawing: ground_battle.cpp keeps the fight (who stands
// where, who fires what at whom) and asks these functions to draw each unit.
// Sizes are the battle's: s = a soldier's height (a mech's for mech()). A
// soldier's (x, y) is his middle, as the game's soldier has always been drawn
// (the ground under him is y + 0.44 s); a mech's or structure's is its feet.
// `lift` raises a unit off its shadow (things that hover).
#pragma once
#include <cstdint>

class Canvas;

namespace gunits {

enum Look : uint8_t {
    Default,
    Arborec,
    Creuss,
    Muaat,
    Naalu,
    Nekro,
    Sardakk,
    Cabal,  // the Vuil'raith
    Empyrean,
    Mahact,  // also the Crimson Rebellion (owner: "use Mahact")
    Titans,
    Axis,
    Florzen,
    Khrask,    // L'tokk Khrask
    Mortheus,  // the Glimmer of Mortheus
    Myko,
    Rohdhna,
    Zelian,
    Cheiran,
    Ghoti,
    Kyro,
    kLooks
};

Look lookOf(const char* factionKey);  // Default for the factions without one
const char* lookName(Look l);
void setFactionUnits(bool on);  // Options > Ground combat: Faction units (on) / Default (off)
bool factionUnits();

struct Pose {
    float x = 0, y = 0;  // screen units (see above)
    float s = 10;        // a soldier's height (a mech's, a structure's)
    float lift = 0;      // how far above its shadow (screen units)
    uint32_t col = 0;    // the player's (muted) colour
    int dir = 1;         // 1: the enemy is below (the invader), -1: above
    float walk = 0;      // stride
    bool firing = false;
    int a = 255;
    uint32_t now = 0;
    float phase = 0;  // per unit, 0..2pi
    int idx = 0;      // which man of his squad
    int style = 0;    // the tileset: 0 Simplistic, 1 Retrowave, 2 Voidfall
};

// A soldier of the squad (Default: the game's own).
void infantry(Canvas& c, Look l, const Pose& p);
// A fallen one (pool 0..1: his blood / sap / slime spreading; Mortheus's fade away).
void fallen(Canvas& c, Look l, const Pose& p, float pool);
// A mech in the faction's own shape; false = the caller draws the default art (then calls mechOverlay).
bool mech(Canvas& c, Look l, const Pose& p, bool wreck);
void mechOverlay(Canvas& c, Look l, const Pose& p, bool wreck);  // Creuss / Mahact seams, Vuil'raith growths
// (0.50.0) Without drawing: whether mech() / structure() / fighter() draw something of their own for the look, and
// whether the look's soldiers / mechs move with the clock (Pose::now, phase) - the pixel-art cache keys on it.
bool hasMech(Look l);
bool hasStructure(Look l, bool pds);
bool hasFighter(Look l);
bool infantryTimed(Look l);
// A landed fighter (the Naalu's crystal fighter); false = the default art.
bool fighter(Canvas& c, Look l, const Pose& p);
// A structure in the faction's shape (Titans' PDS robot, Zelian rocks, Mortheus nebulae); false = the default art.
bool structure(Canvas& c, Look l, bool pds, const Pose& p, bool ruined);
float structureScale(Look l, bool pds);  // how much bigger than the default structure

// Creuss / Mahact units phase out and back in every second: the alpha (0..255) to draw them with now.
int phaseAlpha(Look l, uint32_t now, float phase);
bool phases(Look l);
// One soldier per infantry (Mortheus: each piece is one nebula).
bool onePerPiece(Look l);
float infantryScale(Look l);  // how big the soldier is next to the default one
// Hovering / floating soldiers: how high above their shadow (sizes).
float hover(Look l, int idx);

// The weapons (what each fires; ground_battle turns these into shots).
enum Weapon : uint8_t {
    WDefault,      // rifles and laser carbines (infantry), gatling lasers (mechs)
    WSpore,        // Arborec: a green spore cloud that stays over the enemy
    WSeed,         // Arborec mech: slow orange seed bombs - the overgrowth spreads where they land
    WOrangePlasma, // Muaat
    WFlame2,       // Muaat mech: two flamethrowers
    WRedPulse,     // Nekro: red pulse lasers
    WRedGatling,   // Nekro drone: a red gatling laser
    WSpine,        // Sardakk: bone spines
    WBlueBall,     // Sardakk beetle: big blue plasma balls
    WGreenFlame,   // Vuil'raith horrors: green flames...
    WSlimeShot,    // ...and organic slime bullets
    WPurpleLaser,  // Titans
    WPurpleBeam,   // Titans mech: a long purple beam
    WRocket,       // Titans PDS robot: big rockets
    WLightning,    // Florzen: purple / blue lightning
    WHyperBeam,    // Florzen mech: the hyper beam
    WCrystal,      // Khrask: light green crystal shards
    WBigCrystal,   // Khrask mech
    WStar,         // Mortheus: a slow star that goes supernova
    WDarkStar,     // Mortheus mech: a star that collapses into a black hole
    WRock,         // Zelian: slow rocks
    WBigRock,      // Zelian mech
    WSlimeStream,  // Cheiran: slimethrowers
    WBigSlime,     // Cheiran mech
    WGas,          // Ghoti: superheated gas
    WBigGas,       // Ghoti angler: a wider spread
    WDroidLaser,   // Roh'Dhna droids
    WCrystalFighter,  // Naalu crystal fighters
    kWeapons
};
Weapon infantryWeapon(Look l, int idx);
Weapon mechWeapon(Look l);
// Where the weapon is, from the unit's feet, in its size s (y already follows dir).
void muzzle(Look l, bool mech, int dir, int idx, float& dx, float& dy);

// The blood (sap, ichor, oil...) of a look's soldiers.
uint32_t bloodOf(Look l);
// Voidfall (0.51.1, owner): the painted troops. The default soldier (and the looks built on him) is painted by hand
// (paintedByHand); the others are drawn and re-lit (ui/painted.h) - paintIdx is the part of a man's index their
// pictures depend on (for the picture cache). setVoidPaint(false): Voidfall as it was (test pictures only).
bool voidPaint();
void setVoidPaint(bool on);
bool paintedByHand(Look l, int idx);
int paintIdx(Look l, int idx);
// The default mech painted (its feet at footY).
void paintedMech(Canvas& c, const Pose& p, float footY, float sz, uint32_t mc, bool mirror, bool wreck);
// A soft (Simplistic), hard pixel (Retrowave) or painted (Voidfall) glow.
void glow(Canvas& c, float x, float y, float r, uint32_t col, int a, int style);

}  // namespace gunits
