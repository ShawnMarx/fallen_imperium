#include "engine/map.h"
#include "engine/query.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>

namespace ti {

namespace {
// Factions whose home system sits beside the board while another tile takes its place on the board: the Creuss
// (their gate, tile 17) and the Crimson Rebellion (the Sorrow, tile 94 - Thunder's Edge, 0.35.0).
int gateOf(int faction) {
    if (faction < 0 || faction >= kFactionCount) return -1;
    if (!std::strcmp(kFactions[faction].key, "ghost")) return findSystem("17");
    if (!std::strcmp(kFactions[faction].key, "crimson")) return findSystem("94");
    return -1;
}
bool isGate(int def) { return def >= 0 && (def == findSystem("17") || def == findSystem("94")); }
}  // namespace

Hex layoutPosToHex(int pos) {
    int ring = pos / 100, idx = pos % 100;
    if (ring == 0) return {0, 0};
    // the rings are computed once (map building calls this very often)
    static std::vector<std::vector<Hex>> rings;
    if (ring < 0 || ring > 12) return {99, 99};
    if (rings.empty())
        for (int r = 0; r <= 12; ++r) rings.push_back(hexRing(r));
    const std::vector<Hex>& r = rings[size_t(ring)];
    if (idx < 1 || idx > int(r.size())) return {99, 99};
    return r[size_t(idx - 1)];
}

bool hyperlaneJoins(const SystemState& s, int a, int b) {
    const SystemDef& d = s.d();
    int ua = ((a - s.rotation) % 6 + 6) % 6, ub = ((b - s.rotation) % 6 + 6) % 6;
    return (d.lanes[ua] >> ub) & 1;
}

float tileValue(int def) {
    const SystemDef& d = kSystems[def];
    float v = 0;
    for (int i = 0; i < d.planetCount; ++i) {
        const PlanetDef& p = kPlanets[d.planets[i]];
        // "optimal" value: a planet is usually spent for its better number
        if (p.res > p.inf) v += p.res;
        else if (p.inf > p.res) v += p.inf;
        else v += p.res * 0.5f + p.inf * 0.5f;
        if (p.skip != TechColor::None) v += 0.5f;
        if (p.legendary) v += 1.0f;
    }
    if (d.wormholes) v += 0.5f;
    if (d.anomalies & (kSupernova | kAsteroid)) v -= 0.3f;
    return v;
}

namespace {

bool isAnomalyTile(int def) { return kSystems[def].anomalies != 0; }

struct Placed {
    int pos;
    int def;
    int rotation;
    int home;  // player, -1
};

// Rejects layouts where anomalies touch or same-type wormholes touch.
bool layoutLegal(const std::vector<Placed>& cells) {
    for (size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].def < 0) continue;
        Hex a = layoutPosToHex(cells[i].pos);
        for (size_t j = i + 1; j < cells.size(); ++j) {
            if (cells[j].def < 0) continue;
            Hex b = layoutPosToHex(cells[j].pos);
            if (hexDistance(a, b) != 1) continue;
            const SystemDef& da = kSystems[cells[i].def];
            const SystemDef& db = kSystems[cells[j].def];
            if (da.back == Back::Hyperlane || db.back == Back::Hyperlane) continue;
            if (isAnomalyTile(cells[i].def) && isAnomalyTile(cells[j].def)) return false;
            if ((da.wormholes & db.wormholes) & (kAlpha | kBeta)) return false;
        }
    }
    return true;
}

const LayoutDef* layoutFor(int players) {
    for (int i = 0; i < kLayoutCount; ++i)
        if (kLayouts[i].players == players) return &kLayouts[i];
    return nullptr;
}

}  // namespace

int southernmostSeat(int players) {
    const LayoutDef* lay = layoutFor(players);
    if (!lay) return -1;
    int best = -1;
    float bestY = -1e9f, bestX = 1e9f;
    for (int c = 0; c < lay->count; ++c) {
        const LayoutCell& cell = kLayoutCells[lay->first + c];
        if (cell.kind != 1) continue;
        float x, y;
        hexToPixel(layoutPosToHex(cell.pos), 1.f, x, y);
        // lowest first; between equally low homes, the one nearest the middle
        if (y > bestY + 0.01f || (std::fabs(y - bestY) <= 0.01f && std::fabs(x) < bestX)) {
            bestY = y;
            bestX = std::fabs(x);
            best = cell.a;
        }
    }
    return best;
}

bool buildGalaxy(Game& g, Rng& rng, const std::vector<int>& factionBySeat, std::string* err, bool balanced, std::vector<int>* sliceOwner) {
    const int n = int(factionBySeat.size());
    const LayoutDef* lay = layoutFor(n);
    if (!lay) {
        if (err) *err = "no map layout for this many players";
        return false;
    }

    // Tile pools (base + PoK blue and red tiles; 0.33.0: Uncharted Space's only with its module).
    std::vector<int> blue, red;
    for (int i = 0; i < kSystemCount; ++i) {
        if (!systemInGame(g, i)) continue;
        if (kSystems[i].back == Back::Blue) blue.push_back(i);
        if (kSystems[i].back == Back::Red) red.push_back(i);
    }

    // Collect the layout cells.
    std::vector<Placed> base;
    std::vector<std::pair<int, int>> sliceCells;  // index into base, (player*8 + slot)
    for (int c = 0; c < lay->count; ++c) {
        const LayoutCell& cell = kLayoutCells[lay->first + c];
        Placed pl{cell.pos, -1, 0, -1};
        if (cell.kind == 0) {
            pl.def = cell.a;
            pl.rotation = cell.b;
        } else if (cell.kind == 1) {
            pl.home = cell.a;
        } else {
            sliceCells.push_back({int(base.size()), cell.a * 8 + cell.b});
        }
        base.push_back(pl);
    }
    // Home tiles (Creuss puts its gate on the board).
    for (Placed& pl : base)
        if (pl.home >= 0) {
            int f = factionBySeat[pl.home];
            pl.def = gateOf(f) >= 0 ? gateOf(f) : kFactions[f].homeSystem;
        }

    // Try many random slice deals; keep the most even one that follows the
    // placement rules.
    std::vector<Placed> best;
    float bestScore = 1e9f;
    const int tries = balanced ? 900 : 160;
    for (int t = 0; t < tries; ++t) {
        std::vector<int> b = blue, r = red;
        rng.shuffle(b);
        rng.shuffle(r);
        std::vector<std::array<int, 5>> slices(n);
        for (int p = 0; p < n; ++p) slices[p] = {b[3 * p], b[3 * p + 1], b[3 * p + 2], r[2 * p], r[2 * p + 1]};
        float lo = 1e9f, hi = -1e9f, penalty = 0;
        for (int p = 0; p < n; ++p) {
            float v = 0;
            int legendary = 0;
            for (int d : slices[p]) {
                v += tileValue(d);
                for (int i = 0; i < kSystems[d].planetCount; ++i)
                    if (kPlanets[kSystems[d].planets[i]].legendary) ++legendary;
            }
            if (legendary > 1) penalty += 3;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        if (balanced) {  // 0.43.0: resources, influence, wormholes and anomalies spread evenly too
            float rlo = 1e9f, rhi = -1e9f, ilo = 1e9f, ihi = -1e9f;
            int whlo = 99, whhi = 0, anlo = 99, anhi = 0, leg = 0;
            for (int p = 0; p < n; ++p) {
                int rs = 0, in = 0, wh = 0, an = 0;
                for (int d : slices[p]) {
                    for (int i = 0; i < kSystems[d].planetCount; ++i) {
                        rs += kPlanets[kSystems[d].planets[i]].res;
                        in += kPlanets[kSystems[d].planets[i]].inf;
                        leg += kPlanets[kSystems[d].planets[i]].legendary ? 1 : 0;
                    }
                    wh += kSystems[d].wormholes ? 1 : 0;
                    an += kSystems[d].anomalies ? 1 : 0;
                }
                rlo = std::min(rlo, float(rs)), rhi = std::max(rhi, float(rs));
                ilo = std::min(ilo, float(in)), ihi = std::max(ihi, float(in));
                whlo = std::min(whlo, wh), whhi = std::max(whhi, wh);
                anlo = std::min(anlo, an), anhi = std::max(anhi, an);
            }
            penalty += 0.6f * (rhi - rlo) + 0.6f * (ihi - ilo) + 1.5f * float(std::max(0, whhi - whlo - 1)) + 1.5f * float(std::max(0, anhi - anlo - 1));
        }
        float score = (hi - lo) + penalty;
        if (score >= bestScore) continue;
        // arrange each slice's tiles in its slots; retry arrangements for legality
        bool ok = false;
        std::vector<Placed> cells;
        for (int attempt = 0; attempt < 12 && !ok; ++attempt) {
            cells = base;
            for (int p = 0; p < n; ++p)
                for (int i = 4; i > 0; --i) std::swap(slices[p][i], slices[p][rng.below(i + 1)]);
            for (auto& sc : sliceCells) {
                int player = sc.second / 8, slot = sc.second % 8;
                cells[sc.first].def = slices[player][slot % 5];
            }
            ok = layoutLegal(cells);
        }
        if (!ok) continue;
        bestScore = score;
        best = cells;
    }
    if (best.empty()) {
        if (err) *err = "could not build a legal map";
        return false;
    }

    // which seat's slice each cell is (0.43.0: the seeded galaxy's breakdown)
    if (sliceOwner) {
        sliceOwner->assign(best.size(), -1);
        for (auto& sc : sliceCells) (*sliceOwner)[size_t(sc.first)] = sc.second / 8;
    }
    // Create the systems and planets.
    g.systems.clear();
    g.planets.clear();
    // (0.43.0 debug passes: room for planets that appear later - Mirage, Avernus, Thunder's Edge, minor factions - so
    // the vector doesn't move under a PlanetState& that a rule still holds; ASan found one)
    g.planets.reserve(256);
    g.systems.reserve(160);  // (the same for systems: the Fracture, Star Charts, Nova Seed... added in play)
    for (const Placed& pl : best) {
        SystemState s;
        s.def = int16_t(pl.def);
        s.pos = layoutPosToHex(pl.pos);
        s.rotation = int8_t(pl.rotation);
        if (pl.home >= 0 && !isGate(pl.def)) s.home = int8_t(pl.home);
        g.systems.push_back(s);
    }
    // The Creuss home system sits off the board, beside its gate.
    for (const Placed& pl : best) {
        if (pl.home < 0 || !isGate(pl.def)) continue;
        Hex gate = layoutPosToHex(pl.pos);
        Hex out = gate;
        int bestRing = -1;
        for (int d = 0; d < 6; ++d) {
            Hex h = hexNeighbor(gate, d);
            if (hexRingOf(h) > bestRing && g.findSystemAt(h) < 0) {
                bestRing = hexRingOf(h);
                out = h;
            }
        }
        SystemState s;
        s.def = int16_t(kFactions[factionBySeat[pl.home]].homeSystem);
        s.pos = out;
        s.offBoard = true;
        s.home = int8_t(pl.home);
        g.systems.push_back(s);
    }
    // The wormhole nexus (PoK) starts beside the board, inactive (LRR 100.1):
    // put it just outside the board's top-left corner.
    {
        int nexusDef = findSystem("82b");
        int maxRing = 0;
        for (const SystemState& s : g.systems) maxRing = std::max(maxRing, hexRingOf(s.pos));
        std::vector<Hex> usedHexes, homes;
        for (const SystemState& s : g.systems) {
            usedHexes.push_back(s.pos);
            if (s.home >= 0) homes.push_back(s.pos);
        }
        const Hex best = nexusSpot(maxRing, usedHexes, homes);
        if (nexusDef >= 0) {
            SystemState s;
            s.def = int16_t(nexusDef);
            s.pos = best;
            s.offBoard = true;
            g.nexus = int16_t(g.systems.size());
            g.systems.push_back(s);
        }
    }
    for (size_t i = 0; i < g.systems.size(); ++i) {
        SystemState& s = g.systems[i];
        const SystemDef& d = s.d();
        for (int k = 0; k < d.planetCount; ++k) {
            PlanetState p;
            p.def = d.planets[k];
            p.system = int16_t(i);
            s.planets.push_back(int16_t(g.planets.size()));
            g.planets.push_back(p);
        }
        if (!std::strcmp(d.key, "18")) s.custodians = true;
    }
    computeAdjacency(g);
    return true;
}

// ------------------------------------------------------------------ galaxy draft
namespace {
int addSystem(Game& g, int def, Hex pos, int rotation, int home, bool offBoard) {
    SystemState s;
    s.def = int16_t(def);
    s.pos = pos;
    s.rotation = int8_t(rotation);
    s.home = int8_t(home);
    s.offBoard = offBoard;
    const int idx = int(g.systems.size());
    const SystemDef& d = kSystems[def];
    for (int k = 0; k < d.planetCount; ++k) {
        PlanetState p;
        p.def = d.planets[k];
        p.system = int16_t(idx);
        s.planets.push_back(int16_t(g.planets.size()));
        g.planets.push_back(p);
    }
    if (!std::strcmp(d.key, "18")) s.custodians = true;
    g.systems.push_back(s);
    return idx;
}
}  // namespace

int addBoardSystem(Game& g, int def, Hex pos) { return addSystem(g, def, pos, 0, -1, false); }

bool systemInGame(const Game& g, int def) {
    switch (kSystems[def].source) {
        case Source::Uncharted: return (g.modules & kModUncharted) != 0;
        case Source::ThundersEdge: return (g.modules & kModThundersEdge) != 0;
        default: return true;
    }
}

std::vector<Hex> boardEdgeSpots(const Game& g) {
    std::vector<Hex> out;
    auto taken = [&](Hex h) {
        for (const SystemState& s : g.systems)
            if (s.pos == h) return true;  // (off-board ones too: the wormhole nexus, the Creuss home)
        return false;
    };
    for (const SystemState& s : g.systems) {
        if (s.offBoard) continue;
        for (int d = 0; d < 6; ++d) {
            const Hex h = hexNeighbor(s.pos, d);
            if (taken(h) || std::find(out.begin(), out.end(), h) != out.end()) continue;
            int n = 0;
            for (int e = 0; e < 6; ++e) {
                const int j = g.findSystemAt(hexNeighbor(h, e));
                if (j >= 0 && !g.systems[size_t(j)].isHyperlane()) ++n;
            }
            if (n >= 2) out.push_back(h);
        }
    }
    return out;
}

std::vector<int> blueTilesLeft(const Game& g) {
    std::vector<int> v;
    for (int i = 0; i < kSystemCount; ++i) {
        if (kSystems[i].back != Back::Blue) continue;
        if (!systemInGame(g, i)) continue;
        bool used = std::find(g.purgedTiles.begin(), g.purgedTiles.end(), int16_t(i)) != g.purgedTiles.end();
        for (const SystemState& s : g.systems)
            if (s.def == i) used = true;
        if (!used) v.push_back(i);
    }
    return v;
}

bool startGalaxyDraft(Game& g, Rng& rng, const std::vector<int>& factionBySeat, std::string* err) {
    const int n = int(factionBySeat.size());
    const LayoutDef* lay = layoutFor(n);
    if (!lay) {
        if (err) *err = "no map layout for this many players";
        return false;
    }
    g.systems.clear();
    g.planets.clear();
    g.planets.reserve(256);  // (see buildGalaxy)
    g.systems.reserve(160);
    g.draft = GalaxyDraft();
    std::set<std::pair<int, int>> layoutHexes;
    int blueCells = 0, redCells = 0;
    for (int c = 0; c < lay->count; ++c) {
        const LayoutCell& cell = kLayoutCells[lay->first + c];
        const Hex h = layoutPosToHex(cell.pos);
        layoutHexes.insert({h.q, h.r});
        if (cell.kind == 0) {
            addSystem(g, cell.a, h, cell.b, -1, false);
        } else if (cell.kind == 1) {
            const int f = factionBySeat[cell.a];
            const bool creuss = gateOf(f) >= 0;  // (Creuss gate / the Sorrow)
            addSystem(g, creuss ? gateOf(f) : kFactions[f].homeSystem, h, 0, creuss ? -1 : cell.a, false);
        } else {
            DraftCell dc;
            dc.pos = h;
            dc.ring = int8_t(cell.pos / 100);
            g.draft.cells.push_back(dc);
            if (cell.b % 5 < 3) ++blueCells;
            else ++redCells;
        }
    }
    // The Creuss home system sits off the board beside its gate (as buildGalaxy).
    for (int c = 0; c < lay->count; ++c) {
        const LayoutCell& cell = kLayoutCells[lay->first + c];
        if (cell.kind != 1 || gateOf(factionBySeat[cell.a]) < 0) continue;
        const Hex gate = layoutPosToHex(cell.pos);
        Hex out = gate;
        int bestRing = -1;
        for (int d = 0; d < 6; ++d) {
            const Hex h = hexNeighbor(gate, d);
            if (hexRingOf(h) > bestRing && !layoutHexes.count({h.q, h.r})) {
                bestRing = hexRingOf(h);
                out = h;
            }
        }
        layoutHexes.insert({out.q, out.r});
        addSystem(g, kFactions[factionBySeat[cell.a]].homeSystem, out, 0, cell.a, true);
    }
    // The wormhole nexus beside the board's top-left corner (as buildGalaxy).
    {
        const int nexusDef = findSystem("82b");
        int maxRing = 0;
        for (const auto& h : layoutHexes) maxRing = std::max(maxRing, hexRingOf(Hex{h.first, h.second}));
        std::vector<Hex> usedHexes, homes;
        for (const auto& h : layoutHexes) usedHexes.push_back(Hex{h.first, h.second});
        for (const SystemState& s : g.systems)
            if (s.home >= 0) homes.push_back(s.pos);
        const Hex best = nexusSpot(maxRing, usedHexes, homes);
        if (nexusDef >= 0) g.nexus = int16_t(addSystem(g, nexusDef, best, 0, -1, true));
    }
    for (int p = 0; p < n; ++p)
        for (size_t i = 0; i < g.systems.size(); ++i)
            if (g.systems[i].home == p) g.players[p].homeSystem = int16_t(i);

    // Deal the tiles: each player gets the blue and red tiles of one slice of
    // the layout (3 blue, 2 red). Of 40 random deals the most even one is used
    // (and no hand holds two legendary planets).
    std::vector<int> blue, red;
    for (int i = 0; i < kSystemCount; ++i) {
        if (!systemInGame(g, i)) continue;  // (0.33.0)
        if (kSystems[i].back == Back::Blue) blue.push_back(i);
        if (kSystems[i].back == Back::Red) red.push_back(i);
    }
    const int nb = blueCells / n, nr = redCells / n;
    if (nb * n > int(blue.size()) || nr * n > int(red.size())) {
        if (err) *err = "not enough system tiles";
        return false;
    }
    float bestScore = 1e9f;
    std::vector<std::vector<int16_t>> bestHands;
    for (int t = 0; t < 40; ++t) {
        std::vector<int> b = blue, r = red;
        rng.shuffle(b);
        rng.shuffle(r);
        std::vector<std::vector<int16_t>> hands(static_cast<size_t>(n));
        float lo = 1e9f, hi = -1e9f, penalty = 0;
        for (int p = 0; p < n; ++p) {
            for (int k = 0; k < nb; ++k) hands[size_t(p)].push_back(int16_t(b[size_t(p * nb + k)]));
            for (int k = 0; k < nr; ++k) hands[size_t(p)].push_back(int16_t(r[size_t(p * nr + k)]));
            float v = 0;
            int legendary = 0;
            for (int d : hands[size_t(p)]) {
                v += tileValue(d);
                for (int i = 0; i < kSystems[d].planetCount; ++i)
                    if (kPlanets[kSystems[d].planets[i]].legendary) ++legendary;
            }
            if (legendary > 1) penalty += 3;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        const float score = (hi - lo) + penalty;
        if (score < bestScore) {
            bestScore = score;
            bestHands = hands;
        }
    }
    g.draft.hands = bestHands;
    g.draft.total = int16_t(g.draft.cells.size());
    g.draft.active = true;
    computeAdjacency(g);
    return true;
}

bool draftTouchesBadly(const Game& g, int def, int cell) {
    const SystemDef& d = kSystems[def];
    const Hex at = g.draft.cells[size_t(cell)].pos;
    for (int k = 0; k < 6; ++k) {
        const int j = g.findSystemAt(hexNeighbor(at, k));
        if (j < 0 || g.systems[j].offBoard) continue;
        const SystemDef& o = g.systems[j].d();
        if (o.back == Back::Hyperlane) continue;
        if (d.anomalies && o.anomalies) return true;
        if ((d.wormholes & o.wormholes) & (kAlpha | kBeta)) return true;
    }
    return false;
}

std::vector<std::pair<int, int>> draftPlacements(const Game& g, int p) {
    std::vector<std::pair<int, int>> all, good;
    if (!g.draft.active || p < 0 || p >= int(g.draft.hands.size())) return all;
    int ring = 1 << 20;
    for (const DraftCell& c : g.draft.cells)
        if (c.system < 0) ring = std::min(ring, int(c.ring));
    std::vector<int> tiles;
    for (int16_t t : g.draft.hands[size_t(p)])
        if (std::find(tiles.begin(), tiles.end(), int(t)) == tiles.end()) tiles.push_back(t);
    for (int t : tiles)
        for (size_t c = 0; c < g.draft.cells.size(); ++c) {
            if (g.draft.cells[c].system >= 0 || g.draft.cells[c].ring != ring) continue;
            all.push_back({t, int(c)});
            if (!draftTouchesBadly(g, t, int(c))) good.push_back({t, int(c)});
        }
    return good.empty() ? all : good;
}

int placeDraftTile(Game& g, int p, int def, int cell) {
    std::vector<int16_t>& hand = g.draft.hands[size_t(p)];
    auto it = std::find(hand.begin(), hand.end(), int16_t(def));
    if (it != hand.end()) hand.erase(it);
    DraftCell& c = g.draft.cells[size_t(cell)];
    c.system = int16_t(addSystem(g, def, c.pos, 0, -1, false));
    ++g.draft.placed;
    computeAdjacency(g);
    return c.system;
}

void finishGalaxyDraft(Game& g) {
    g.draft = GalaxyDraft();
    computeAdjacency(g);
}

// Where the Wormhole Nexus sits beside the board: on the ring just outside it,
// as far from every home as possible (0.24.0, owner: next to a home it took the
// spaces of that player's info tiles), then toward the top left as before.
// Only where it is drawn - it is never adjacent to anything on the board.
Hex nexusSpot(int maxRing, const std::vector<Hex>& used, const std::vector<Hex>& homes) {
    Hex best{0, 0};
    int bestFar = -1;
    float bx = 1e9f, by = 1e9f;
    for (Hex h : hexRing(maxRing + 1)) {
        if (std::find(used.begin(), used.end(), h) != used.end()) continue;
        int far = 99;
        for (const Hex& hm : homes) far = std::min(far, hexDistance(h, hm));
        far = std::min(far, 4);  // beyond 4 it's all the same: keep to the top left
        float x, y;
        hexToPixel(h, 1.f, x, y);
        if (far > bestFar || (far == bestFar && x + y < bx + by)) {
            bestFar = far;
            bx = x;
            by = y;
            best = h;
        }
    }
    return best;
}

void computeAdjacency(Game& g) {
    const int n = int(g.systems.size());
    const bool travelBan = lawInPlay(g, "travel_ban");
    const bool joinAB = lawInPlay(g, "wormhole_recon") || g.fx.lostStarChart;
    for (int i = 0; i < n; ++i) {
        SystemState& s = g.systems[i];
        s.adj.clear();
        s.edge = false;
        if (s.isHyperlane()) continue;
        std::set<int> adj;
        if (!s.offBoard) {
            for (int d = 0; d < 6; ++d) {
                Hex nh = hexNeighbor(s.pos, d);
                int j = g.findSystemAt(nh);
                if (j < 0) {
                    s.edge = true;
                    continue;
                }
                if (g.systems[j].fracture != s.fracture) {  // Thunder's Edge: the Fracture doesn't touch the board
                    s.edge = true;
                    continue;
                }
                if (!g.systems[j].isHyperlane()) {
                    adj.insert(j);
                    continue;
                }
                // follow hyperlanes (they can chain)
                std::vector<std::pair<int, int>> stack{{j, (d + 3) % 6}};
                std::set<std::pair<int, int>> seen;
                while (!stack.empty()) {
                    auto cur = stack.back();
                    stack.pop_back();
                    if (!seen.insert(cur).second) continue;
                    const SystemState& h = g.systems[cur.first];
                    for (int x = 0; x < 6; ++x) {
                        if (!hyperlaneJoins(h, cur.second, x)) continue;
                        int k = g.findSystemAt(hexNeighbor(h.pos, x));
                        if (k < 0) continue;
                        if (g.systems[k].isHyperlane()) stack.push_back({k, (x + 3) % 6});
                        else adj.insert(k);
                    }
                }
            }
        }
        // wormholes (Enforced Travel Ban switches alpha / beta off; Wormhole
        // Reconstruction and Lost Star Chart join alpha and beta together)
        uint8_t wh = wormholesOf(g, i) & (kAlpha | kBeta | kGamma | kDelta | kEpsilon);  // (epsilon: the Crimson Rebellion's)
        if (travelBan) wh &= uint8_t(~(kAlpha | kBeta));
        uint8_t links = wh;
        if (joinAB && (wh & (kAlpha | kBeta))) links |= kAlpha | kBeta;
        if (links) {
            for (int j = 0; j < n; ++j) {
                uint8_t other = wormholesOf(g, j);
                if (travelBan) other &= uint8_t(~(kAlpha | kBeta));
                if (j != i && (other & links)) adj.insert(j);
            }
        }
        // Thunder's Edge: every ingress system and every egress of the Fracture are adjacent (not ingress to ingress,
        // nor egress to egress); the Fracture's systems are on the edge of the board
        if (g.fractureIn) {
            if (s.fracture) s.edge = true;
            if (s.ingress || s.egress)
                for (int j = 0; j < n; ++j)
                    if (j != i && ((s.ingress && g.systems[j].egress) || (s.egress && g.systems[j].ingress))) adj.insert(j);
        }
        // Incursion (Crimson Rebellion, Thunder's Edge): systems with active breaches are adjacent to each other
        if (s.breach == 2)
            for (int j = 0; j < n; ++j)
                if (j != i && g.systems[j].breach == 2) adj.insert(j);
        // Cosmic Phenomenae (galactic event): entropic scars are adjacent to each other
        if (eventOn(g, kEvCosmic) && (s.d().anomalies & kScar))
            for (int j = 0; j < n; ++j)
                if (j != i && (g.systems[j].d().anomalies & kScar)) adj.insert(j);
        // Abyssal Starpaths (Uncharted Space): two systems without planets joined for a tactical action
        if (g.fx.starA >= 0 && g.fx.starB >= 0) {
            if (i == g.fx.starA) adj.insert(g.fx.starB);
            if (i == g.fx.starB) adj.insert(g.fx.starA);
        }
        adj.erase(i);
        s.adj.assign(adj.begin(), adj.end());
        if (i == g.nexus) s.edge = true;  // LRR 100.1c
    }
}

std::vector<SliceStats> sliceStats(const Game& g, const std::vector<int>& sliceOwner, int players) {
    std::vector<SliceStats> out(size_t(std::max(0, players)));
    for (size_t i = 0; i < sliceOwner.size() && i < g.systems.size(); ++i) {
        const int p = sliceOwner[i];
        if (p < 0 || p >= players) continue;
        SliceStats& st = out[size_t(p)];
        const SystemDef& d = kSystems[g.systems[i].def];
        st.wormholes += d.wormholes ? 1 : 0;
        st.anomalies += d.anomalies ? 1 : 0;
        for (int k = 0; k < d.planetCount; ++k) {
            const PlanetDef& pd = kPlanets[d.planets[k]];
            st.res += pd.res;
            st.inf += pd.inf;
            st.optimal += std::max(pd.res, pd.inf);
            st.planets += 1;
            st.legendary += pd.legendary ? 1 : 0;
            st.skips += pd.skip != TechColor::None ? 1 : 0;
        }
    }
    return out;
}

}  // namespace ti
