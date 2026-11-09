// Hex grid maths for the galaxy. Tiles are flat-topped (as on the TI4 board)
// and addressed with axial coordinates (q, r). Mecatol Rex is (0, 0).
#pragma once
#include <cmath>
#include <cstdlib>
#include <vector>

struct Hex {
    int q = 0, r = 0;
    bool operator==(const Hex& o) const { return q == o.q && r == o.r; }
    bool operator!=(const Hex& o) const { return !(*this == o); }
};

// Directions, clockwise from the top: N, NE, SE, S, SW, NW.
inline Hex hexDir(int d) {
    static const Hex dirs[6] = {{0, -1}, {1, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}};
    return dirs[((d % 6) + 6) % 6];
}
inline Hex hexAdd(Hex a, Hex b) { return {a.q + b.q, a.r + b.r}; }
inline Hex hexNeighbor(Hex h, int d) { return hexAdd(h, hexDir(d)); }
inline int hexDistance(Hex a, Hex b) {
    int dq = a.q - b.q, dr = a.r - b.r;
    return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}
inline int hexRingOf(Hex h) { return hexDistance(h, {0, 0}); }

// Pixel centre of a hex for a given tile radius (centre to corner).
inline void hexToPixel(Hex h, float size, float& x, float& y) {
    x = size * 1.5f * float(h.q);
    y = size * 1.7320508f * (float(h.r) + float(h.q) * 0.5f);
}

// All hexes of one ring, starting at the top and going clockwise. This is
// the same order TI4 map strings use (ring 1 = positions 1-6, ring 2 = 7-18,
// ring 3 = 19-36, ring 4 = 37-60).
inline std::vector<Hex> hexRing(int radius) {
    std::vector<Hex> out;
    if (radius == 0) {
        out.push_back({0, 0});
        return out;
    }
    Hex h{0, -radius};
    static const int walk[6] = {2, 3, 4, 5, 0, 1};  // SE, S, SW, NW, N, NE
    for (int side = 0; side < 6; ++side)
        for (int step = 0; step < radius; ++step) {
            out.push_back(h);
            h = hexNeighbor(h, walk[side]);
        }
    return out;
}

// Map-string position index (0 = Mecatol) for a hex, or -1 if beyond ring 4.
inline int hexPositionIndex(Hex h) {
    int ring = hexRingOf(h);
    if (ring > 4) return -1;
    int base = 0;
    for (int r = 1; r < ring; ++r) base += 6 * r;
    std::vector<Hex> rr = hexRing(ring);
    for (size_t i = 0; i < rr.size(); ++i)
        if (rr[i] == h) return ring == 0 ? 0 : 1 + base + int(i);
    return -1;
}
