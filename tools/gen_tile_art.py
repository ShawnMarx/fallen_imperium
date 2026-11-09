#!/usr/bin/env python3
"""Animated pixel-art tiles - review batch (owner request 2026-10-22).

Anomalies, wormholes, Mecatol Rex, Mallice and the planet tiles that hold an
anomaly or a wormhole, plus wormhole / ion storm / Dimensional Tear tokens, in
a chunky pixel style (in the spirit of the TI4 Lab map picture the owner
sent). Everything is drawn from code - noise, lit spheres, particles - in a
fixed palette with ordered (Bayer) dithering; no source images.

Geometry follows the game's map (ui/map_view.cpp): flat-top hexes, corner
radius 48 px (96x84), planets and wormholes where drawSystem puts them, so the
art can be dropped in later. Every animation loops seamlessly (64 frames x
100 ms): every moving part is periodic in the loop time t (0..1).

NOT used by the game yet - see the Stage 7 task "animated pixel-art tiles".

usage: gen_tile_art.py OUTDIR [piece ...]
  writes OUTDIR/<piece>.png (frame strip, 1x), OUTDIR/<piece>.gif (3x),
  OUTDIR/sheet.png (first frames, 2x) and OUTDIR/pieces.json
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

TAU = 2 * math.pi
SQ3 = math.sqrt(3)
F = 64          # frames per loop
FRAME_MS = 100  # 6.4 s loop
R = 48          # hex corner radius (px)
W, H = 96, 84   # tile canvas
K = 1           # art scale (0.48.0): pixels per design pixel - gen_retro.py --scale 2 bakes the sharp PC art. Every
                # shape is placed in design pixels (Cv.X / Y); only the pixel grid, the dither and the dots get finer
TOK = 40        # token canvas

# ---------------------------------------------------------------- palette
RAMPS = {
    "space": ["0a0d1c", "0f152a", "141c36", "1a2444", "212c52"],
    "star": ["2e3a64", "5a6ea8", "98aee0", "dfe8ff", "ffffff"],
    "starwarm": ["6a5a44", "b8965e", "f6d898", "fff4dc"],
    "nebpink": ["1a0f2e", "2a1446", "3f1a5e", "582078", "78288c", "9c3492", "c24690", "e46696", "fa98ac", "ffd2d2"],
    "nebblue": ["101838", "18265a", "20387e", "2a4ea2", "3a6cc2", "5890da", "86b4ee", "c2dcff"],
    "nebdark": ["06040a", "0c0814", "130d20", "1b122e", "25183e", "321f52", "422868", "56347e"],
    "fire": ["1a0606", "3a0a06", "5e1206", "8a1e08", "b8320c", "dc5212", "f27a1e", "fca234", "ffc95a", "ffe48e", "fff6cf", "ffffff"],
    "crimson": ["12030a", "2a0610", "4c0a16", "72101c", "9c1a22", "c42a26", "e44a30", "fa7444", "ffa66c", "ffd2a8"],
    "rock": ["0e0e14", "1a1a22", "282731", "383641", "4a4752", "5e5a64", "76717a", "928c93", "b0aaad", "d2ccc6"],
    "dust": ["161c2c", "1c2232", "232838", "2b2f3e"],
    "alpha": ["200d04", "45200a", "74340c", "a84d10", "d66a18", "f48c2a", "ffb258", "ffd696", "fff0d6"],
    "beta": ["06200e", "0d3d1a", "175f28", "228538", "34ab4a", "58cc62", "8ee68a", "c4f5b8", "effff0"],
    "gamma": ["1e0622", "3c0c46", "621470", "8c1e98", "b82fbc", "dc52d8", "f282e8", "ffb8f4", "ffe8fc"],
    "delta": ["04182a", "082e4c", "0e4a72", "16699a", "228cc0", "38b0de", "66d0f0", "a6e8ff", "e6faff"],
    "epsilon": ["200408", "420a12", "6c101e", "98182a", "c42438", "e6404c", "fa6e6c", "ffa8a0", "ffe2dc"],  # Thunder's Edge
    "rift": ["08050f", "120a20", "1e1034", "2c164c", "3c1e64", "50287c", "683896", "844eb2", "a46ccc"],
    "hot": ["3a1004", "6e2006", "a8380a", "d85a12", "f68a24", "ffb84a", "ffde84", "fff4c8", "ffffff"],
    "ocean": ["040c1e", "081834", "0e2850", "143c6e", "1c548e", "286eaa", "3c8cc4", "62acda"],
    "land": ["0c1a0e", "163018", "244620", "345e2a", "467836", "5e9244", "7cac58"],
    "cloud": ["2e3648", "4c566c", "748098", "a2aec0", "d0d8e4", "f4f8fc"],
    "sand": ["24160a", "422812", "66401a", "8c5a24", "b07a34", "cc9a48", "e4c068", "f4e0a0"],
    "lavarock": ["0a0606", "160c0c", "241412", "341e1a", "462822", "5a342c"],
    "lava": ["5e1206", "a8300a", "e8601a", "ffa034", "ffd86a", "fff4b0"],
    "rust": ["160a06", "2a140c", "421e12", "5c2a18", "783820", "944a2a", "ac5e38", "c27a50", "d89a70"],
    "lavender": ["120e26", "1e183e", "2e2658", "403672", "54488e", "6c5ea8", "887ac0", "a89ad6", "ccc2ea"],
    "ice": ["081626", "102a44", "1a4262", "265e80", "367e9e", "4e9cb8", "70bcd2", "9cd6e6", "cceef6", "f6feff"],
    "steel": ["0a101a", "121a2a", "1a263c", "243450", "304464", "3e5678", "506a8c", "6680a0", "8098b4", "a2b6cc"],
    "city": ["4a300c", "845612", "c2841a", "f0b434", "ffdc74", "fff6c8"],
    "gold": ["302208", "56400e", "826018", "ae8424", "d4aa3c", "f4d478", "fff0c0"],
    "warp": ["0a081a", "141232", "1e1c4e", "28286c", "34388a", "4250a8", "586ec4", "7a94dc", "a6bef0", "dceaff"],
    "atmo": ["18325e", "264e8a", "3c74b8", "62a0e0", "9ccaf8", "d4ecff"],
    "redb": ["240606", "4c0a0a", "861414", "c42020", "ec3a34", "ff7a6a"],
    "teal": ["041a18", "0a302c", "104a44", "18665c", "248476", "34a290", "50bea8", "80d8c4", "bcf0e2", "f0fffa"],
    "storm": ["0a1226", "142444", "1e386a", "2c5290", "4072b4", "5c96d2", "86baea", "bcdcfa", "f0f8ff"],
    "tear": ["000000", "0e0418", "220836", "380e54", "521676", "6e249a", "9040c0", "b470e0", "e4b8ff"],
    "black": ["000000", "04030a"],
    # 0.26.0 Retrowave (tools/gen_retro.py): more planet kinds and the neon of the hyperlanes
    "jungle": ["06140a", "0c2412", "12361a", "1a4a22", "24602a", "317832", "42903c", "5aa84a", "7cc060", "a6d884"],
    "toxic": ["141606", "26280a", "3a3c0e", "525212", "6c6a18", "88841e", "a6a028", "c4be3a", "dcd860", "f0f09a"],
    "gas": ["1c1008", "34200e", "4e3216", "6a4620", "885c2c", "a6763c", "c0925a", "d6b07e", "e8cca4", "f6e8d0"],
    "neonc": ["041a22", "06303e", "0a4c5e", "106a80", "188ca2", "26aec2", "40ccda", "6ae4ec", "a8f6f8", "e8ffff"],
    "neonp": ["220418", "3e0a2c", "5e1044", "82185c", "a82276", "cc3090", "ea4aa8", "fa74c0", "ffa8dc", "ffe0f2"],
}
PAL = [(255, 0, 255)]   # index 0 = transparent
PALMAP = {}
RI = {}
for _name, _cols in RAMPS.items():
    _ids = []
    for _c in _cols:
        _rgb = tuple(int(_c[i:i + 2], 16) for i in (0, 2, 4))
        if _rgb not in PALMAP:
            PALMAP[_rgb] = len(PAL)
            PAL.append(_rgb)
        _ids.append(PALMAP[_rgb])
    RI[_name] = np.array(_ids, np.int16)
PAL_NP = np.array(PAL, np.uint8)

_BAYER8 = np.array([
    [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]], float)


def bayer(h, w):
    return (np.tile(_BAYER8, (h // 8 + 1, w // 8 + 1))[:h, :w] + 0.5) / 64.0


# ---------------------------------------------------------------- canvas
class Cv:
    """Palette-index canvas; X/Y are pixel-centre coordinates from the middle."""

    def __init__(self, w, h):
        k = K
        self.k = k
        self.bw, self.bh = w, h          # in design pixels
        self.w, self.h = w * k, h * k    # in pixels
        self.idx = np.zeros((self.h, self.w), np.int16)
        ys, xs = np.mgrid[0:self.h, 0:self.w]
        self.X = (xs + 0.5) / k - w / 2.0
        self.Y = (ys + 0.5) / k - h / 2.0
        self.B = bayer(self.h, self.w)

    def shade(self, ramp, v, mask=None, dither=1.0):
        r = RI[ramp]
        n = len(r)
        v = np.broadcast_to(np.asarray(v, float), self.idx.shape)
        x = np.clip(v, 0, 1) * (n - 1) + (self.B - 0.5) * dither
        vals = r[np.clip(np.rint(x), 0, n - 1).astype(int)]
        if mask is None:
            self.idx[:] = vals
        else:
            self.idx[mask] = vals[mask]

    def alpha(self, ramp, v, a, mask=None, dither=1.0):
        """Dithered coverage: draw where a > the Bayer threshold."""
        m = np.broadcast_to(np.asarray(a, float), self.idx.shape) > self.B
        if mask is not None:
            m = m & mask
        self.shade(ramp, v, m, dither)

    def flat(self, mask, ramp, level):
        r = RI[ramp]
        self.idx[mask] = r[int(round(np.clip(level, 0, 1) * (len(r) - 1)))]

    def dots(self, x, y, ramp, level, mask=None, size=None):
        """Dots at design-pixel positions; each covers one design pixel (size x size pixels: size 1 = one fine
        pixel on the sharp art)."""
        x = np.atleast_1d(np.asarray(x, float))
        y = np.atleast_1d(np.asarray(y, float))
        level = np.broadcast_to(np.asarray(level, float), x.shape)
        s = self.k if size is None else size
        ix0 = np.floor(x * self.k + self.w / 2 - (s - 1) / 2).astype(int)
        iy0 = np.floor(y * self.k + self.h / 2 - (s - 1) / 2).astype(int)
        r = RI[ramp]
        li = np.clip(np.rint(level * (len(r) - 1)), 0, len(r) - 1).astype(int)
        for oy in range(s):
            for ox in range(s):
                ix, iy = ix0 + ox, iy0 + oy
                ok = (ix >= 0) & (ix < self.w) & (iy >= 0) & (iy < self.h)
                if mask is not None:
                    ok &= mask[np.clip(iy, 0, self.h - 1), np.clip(ix, 0, self.w - 1)]
                self.idx[iy[ok], ix[ok]] = r[li[ok]]


def hex_mask(cv, inset=0.0):
    ax, ay = np.abs(cv.X), np.abs(cv.Y)
    return (ay <= R * SQ3 / 2 - inset) & (SQ3 * ax + ay <= SQ3 * R - 2 * inset)


def edge_dist(cv):
    ax, ay = np.abs(cv.X), np.abs(cv.Y)
    return np.minimum(R * SQ3 / 2 - ay, (SQ3 * R - SQ3 * ax - ay) / 2)


def disc_mask(cv, r):
    return cv.X ** 2 + cv.Y ** 2 <= r * r


def polar(cv, cx, cy):
    dx, dy = cv.X - cx, cv.Y - cy
    return np.hypot(dx, dy), np.arctan2(dy, dx)


# ---------------------------------------------------------------- noise
class N2:
    """Periodic 2D noise from a random Fourier series (integer wave vectors,
    period P). Components with |k| >= tk change with time (one phase turn per
    loop), the large ones stay put so clouds shimmer rather than slide."""

    def __init__(self, seed, n=48, kmax=8, beta=1.3, P=96.0, tk=3, tm=1):
        rng = np.random.default_rng(seed)
        k = []
        while len(k) < n:
            kx, ky = rng.integers(-kmax, kmax + 1, 2)
            if kx == 0 and ky == 0:
                continue
            k.append((kx, ky))
        self.k = np.array(k, float)
        mag = np.hypot(self.k[:, 0], self.k[:, 1])
        self.a = mag ** (-beta)
        self.ph = rng.uniform(0, TAU, n)
        self.m = np.where(mag >= tk, rng.choice([-tm, tm], n), 0) if tm else np.zeros(n)
        self.P = P
        self.sd = math.sqrt((self.a ** 2).sum() / 2)

    def __call__(self, X, Y, t=0.0):
        arg = (TAU / self.P) * (X[..., None] * self.k[:, 0] + Y[..., None] * self.k[:, 1]) + self.ph + TAU * self.m * t
        return np.clip(0.5 + (np.cos(arg) @ self.a) / (3.2 * self.sd), 0, 1)


class N3:
    """Smooth 3D noise for sphere surfaces (seamless on a turning sphere)."""

    def __init__(self, seed, n=48, fmin=1.5, fmax=12.0, beta=1.2, tm=0, tfrac=0.5):
        rng = np.random.default_rng(seed)
        d = rng.normal(size=(n, 3))
        d /= np.linalg.norm(d, axis=1)[:, None]
        f = np.exp(rng.uniform(math.log(fmin), math.log(fmax), n))
        self.k = d * f[:, None]
        self.a = f ** (-beta)
        self.ph = rng.uniform(0, TAU, n)
        self.m = np.where(rng.random(n) < tfrac, rng.choice([-tm, tm], n), 0) if tm else np.zeros(n)
        self.sd = math.sqrt((self.a ** 2).sum() / 2)

    def __call__(self, P3, t=0.0):
        arg = P3 @ self.k.T + self.ph + TAU * self.m * t
        return np.clip(0.5 + (np.cos(arg) @ self.a) / (3.2 * self.sd), 0, 1)


def hash01(x):
    return np.modf(np.abs(np.sin(x * 12.9898 + 78.233) * 43758.5453))[0]


# ---------------------------------------------------------------- common layers
def space(cv, seed, mask, ramp="space", lo=0.1, hi=0.85):
    n = N2(seed, n=28, kmax=4, beta=1.6, tm=0)(cv.X, cv.Y)
    cv.shade(ramp, lo + (hi - lo) * n, mask)


def stars(cv, seed, t, mask, density=0.011, warm=0.25, cross=True, ramp="star", bright=1.0):
    rng = np.random.default_rng(seed)
    ys, xs = np.nonzero(mask)
    k = cv.k
    count = max(1, int(len(xs) * density / (k * k)))
    pick = rng.choice(len(xs), count, replace=False)
    x = cv.X[ys[pick], xs[pick]]
    y = cv.Y[ys[pick], xs[pick]]
    b0 = rng.uniform(0.15, 1.0, count) ** 1.6 * bright
    mm = rng.integers(1, 4, count)
    ph = rng.uniform(0, TAU, count)
    tw = rng.random(count) < 0.5
    b = b0 * np.where(tw, 0.6 + 0.4 * np.cos(TAU * mm * t + ph), 1.0)
    wm = rng.random(count) < warm
    if k == 1:
        cv.dots(x[~wm], y[~wm], ramp, np.clip(b[~wm], 0, 1), mask)
        cv.dots(x[wm], y[wm], "starwarm", np.clip(b[wm], 0, 1), mask)
        if cross:
            big = (b0 > 0.72) & tw & (np.cos(TAU * mm * t + ph) > 0.55)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.dots(x[big] + dx, y[big] + dy, ramp, np.clip(b[big] - 0.45, 0, 1), mask)
        return
    # the sharp art (0.48.0): bright stars a design pixel wide, the rest one fine pixel, finer crosses, and a
    # second layer of faint far stars
    x = np.floor(x * k) / k + 0.5 / k   # (on the fine grid)
    y = np.floor(y * k) / k + 0.5 / k
    bright = b0 > 0.5
    for sel, size in ((bright, k), (~bright, 1)):
        cv.dots(x[sel & ~wm], y[sel & ~wm], ramp, np.clip(b[sel & ~wm], 0, 1), mask, size=size)
        cv.dots(x[sel & wm], y[sel & wm], "starwarm", np.clip(b[sel & wm], 0, 1), mask, size=size)
    if cross:
        big = (b0 > 0.72) & tw & (np.cos(TAU * mm * t + ph) > 0.55)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            for step, fade in ((1, 0.4), (2, 0.62)):  # fine pixels out from the star's block (k wide)
                off = (k - 1 + step) if dx + dy > 0 else -step
                cv.dots(x[big] + dx * abs(off) / k, y[big] + dy * abs(off) / k, ramp, np.clip(b[big] - fade, 0, 1), mask, size=1)
    far = int(count * 1.3)
    rf = np.random.default_rng(seed + 7919)
    pf = rf.choice(len(xs), min(far, len(xs)), replace=False)
    fb = rf.uniform(0.12, 0.42, len(pf)) * bright_scale(t, rf, len(pf))
    cv.dots(cv.X[ys[pf], xs[pf]], cv.Y[ys[pf], xs[pf]], ramp, np.clip(fb, 0, 1), mask, size=1)


def bright_scale(t, rng, n):
    """slow twinkle of the far stars (seamless over the loop)"""
    mm = rng.integers(1, 3, n)
    ph = rng.uniform(0, TAU, n)
    return 0.75 + 0.25 * np.cos(TAU * mm * t + ph)


def anomaly_border(cv):
    """The red dashed edge of an anomaly tile (as on the printed tiles)."""
    hm = hex_mask(cv)
    de = edge_dist(cv)
    ring = hm & (de < 2.0)
    th = np.mod(np.arctan2(cv.Y, cv.X), TAU)
    k = np.floor(th / (TAU / 6)).astype(int) % 6
    ax_ = R * np.cos(k * TAU / 6)
    ay_ = R * np.sin(k * TAU / 6)
    bx_ = R * np.cos((k + 1) * TAU / 6)
    by_ = R * np.sin((k + 1) * TAU / 6)
    along = ((cv.X - ax_) * (bx_ - ax_) + (cv.Y - ay_) * (by_ - ay_)) / (R * R)
    dash = (np.floor(np.clip(along, 0, 0.999) * 7) % 2) == 0
    cv.flat(ring & dash & (de < 1.0), "redb", 0.85)
    cv.flat(ring & dash & (de >= 1.0), "redb", 0.55)
    cv.flat(ring & ~dash & (de < 1.0), "redb", 0.0)


def plain_edge(cv):
    hm = hex_mask(cv)
    cv.flat(hm & (edge_dist(cv) < 1.0), "space", 0.0)


# ---------------------------------------------------------------- planets
LIGHT = np.array([-0.62, -0.52, 0.58])
LIGHT /= np.linalg.norm(LIGHT)


def sphere(cv, cx, cy, r, t, spin=1, tilt=0.35):
    dx = (cv.X - cx) / r
    dy = (cv.Y - cy) / r
    d2 = dx * dx + dy * dy
    m = d2 <= 1.0
    nz = np.sqrt(np.clip(1 - d2, 0, 1))
    lam = np.clip(dx * LIGHT[0] + dy * LIGHT[1] + nz * LIGHT[2], 0, 1)
    ct, st = math.cos(tilt), math.sin(tilt)
    x1 = dx * ct + dy * st
    y1 = -dx * st + dy * ct
    a = TAU * spin * t
    x2 = x1 * math.cos(a) + nz * math.sin(a)
    z2 = -x1 * math.sin(a) + nz * math.cos(a)
    return m, lam, np.stack([x2, y1, z2], -1), nz, d2, dx, dy


def lit(lam, amb=0.14):
    return amb + (1 - amb) * np.clip(lam * 1.12, 0, 1) ** 0.85


def atmosphere(cv, cx, cy, r, ramp, strength=1.0, width=1.6):
    dx = (cv.X - cx) / r
    dy = (cv.Y - cy) / r
    d = np.hypot(dx, dy)
    ring = (d > 1.0) & (d <= 1.0 + width / r)
    lam = np.clip((dx * LIGHT[0] + dy * LIGHT[1]) / np.maximum(d, 1e-6) * 0.9 + 0.25, 0, 1)
    cv.alpha(ramp, 0.35 + 0.6 * lam, (0.25 + 0.75 * lam) * strength, ring)


def planet(cv, cx, cy, r, t, kind, seed, spin=1, tilt=0.35, atmo=None):
    m, lam, P, nz, d2, dx, dy = sphere(cv, cx, cy, r, t, spin, tilt)
    L = lit(lam)
    if kind == "ocean":          # Quann: blue ocean world, green land, moving clouds
        land_n = N3(seed, fmin=1.2, fmax=7)(P)
        land = land_n > 0.56
        det = N3(seed + 1, fmin=6, fmax=16)(P)
        cv.shade("ocean", (0.35 + 0.55 * np.clip(land_n / 0.56, 0, 1) ** 3) * L, m & ~land)
        cv.shade("land", (0.35 + 0.55 * det) * L, m & land)
        cl = N3(seed + 2, fmin=2, fmax=10, tm=1, tfrac=0.6)(P, t)
        ice = np.abs(P[..., 1]) > 0.86
        cv.shade("cloud", (0.45 + 0.55 * np.clip((cl - 0.58) / 0.3, 0, 1)) * L, m & ((cl > 0.58) | ice))
    elif kind == "desert":       # Lodor: dunes and dark rock
        n = N3(seed, fmin=1.5, fmax=10)(P)
        band = 0.5 + 0.32 * np.sin(P[..., 1] * 7.0 + 5.0 * (n - 0.5))
        rockm = n > 0.7
        cv.shade("sand", (0.3 + 0.7 * band) * L, m & ~rockm)
        cv.shade("rust", (0.25 + 0.5 * n) * L, m & rockm)
    elif kind == "lava":         # Atlas: dark rock, glowing cracks
        n = N3(seed, fmin=2, fmax=12)(P)
        cv.shade("lavarock", (0.25 + 0.75 * n) * L, m)
        c = N3(seed + 3, fmin=2.5, fmax=9)(P)
        crack = np.abs(c - 0.5) < 0.045
        pulse = 0.55 + 0.3 * np.cos(TAU * (2 * t) + 9.0 * c)
        cv.shade("lava", pulse + 0.25 * (1 - np.abs(c - 0.5) / 0.045), m & crack)
    elif kind == "rust":         # Cormund: rust-brown, craters
        n = N3(seed, fmin=1.5, fmax=12)(P)
        cr = N3(seed + 5, fmin=8, fmax=18)(P)
        v = (0.3 + 0.6 * n) * L
        v = np.where(cr > 0.72, v * 0.6, v)
        cv.shade("rust", v, m)
    elif kind == "lavender":     # Everra: soft violet world with pale clouds
        n = N3(seed, fmin=1.2, fmax=8)(P)
        band = 0.5 + 0.3 * np.sin(P[..., 1] * 5.0 + 4.0 * (n - 0.5))
        cv.shade("lavender", (0.25 + 0.7 * band) * L, m)
        cl = N3(seed + 2, fmin=2, fmax=9, tm=1)(P, t)
        cv.shade("lavender", np.clip(0.7 + 0.3 * cl, 0, 1) * L, m & (cl > 0.64))
    elif kind == "ice":          # Creuss: icy crystal world with glowing veins
        n = N3(seed, fmin=1.5, fmax=12)(P)
        cv.shade("ice", (0.3 + 0.65 * n) * L, m)
        c = N3(seed + 7, fmin=3, fmax=10)(P)
        vein = np.abs(c - 0.5) < 0.04
        cv.shade("delta", 0.75 + 0.25 * np.cos(TAU * t * 2 + c * 12), m & vein)
    elif kind == "mallice":      # Mallice: teal ice with bright fissures
        n = N3(seed, fmin=1.5, fmax=10)(P)
        cv.shade("teal", (0.3 + 0.65 * n) * L, m)
        c = N3(seed + 8, fmin=2.5, fmax=8)(P)
        vein = np.abs(c - 0.5) < 0.05
        cv.shade("gamma", 0.65 + 0.3 * np.cos(TAU * t + c * 10), m & vein)
    elif kind == "dark":         # The Dark: a dim violet world, lit only at the rim
        n = N3(seed, fmin=1.5, fmax=10)(P)
        cv.shade("lavender", (0.1 + 0.25 * n) * (0.3 + 0.7 * lam), m)
        rim = m & (d2 > 0.6) & (lam > 0.35)
        cv.shade("rift", 0.45 + 0.5 * lam, rim)
        g = N3(seed + 9, fmin=6, fmax=16)(P)
        glow = m & (g > 0.78) & (lam < 0.25)
        cv.shade("gamma", 0.3 + 0.25 * (0.5 + 0.5 * np.cos(TAU * 2 * t + g * 20)), glow & (cv.B < 0.5))
    elif kind == "city":         # Mecatol Rex: the old imperial city world
        cont = N3(seed, fmin=1.2, fmax=6)(P) > 0.4
        lon = np.arctan2(P[..., 0], P[..., 2])
        lat = np.arcsin(np.clip(P[..., 1], -1, 1))
        gx = lon / TAU * 22
        gy = (lat / math.pi + 0.5) * 11
        cell = np.floor(gx) + 1000 * np.floor(gy)
        h = hash01(cell)
        street = (np.modf(gx + 100)[0] < 0.22) | (np.modf(gy + 100)[0] < 0.22)
        day = L
        land = m & cont
        cv.shade("ocean", (0.3 + 0.45 * hash01(cell * 0.37)) * day, m & ~cont)
        cv.shade("steel", np.where(street, 0.25, 0.45 + 0.5 * h) * day, land)
        night = lam < 0.2
        lights = land & night & ~street & (h > 0.5)
        flick = 0.5 + 0.3 * np.cos(TAU * (t * 3) + h * 40)
        cv.shade("city", flick * (1.0 - lam * 2.5), lights)
    if atmo:
        atmosphere(cv, cx, cy, r, atmo)
    return m


# ---------------------------------------------------------------- wormholes
GLYPHS = {
    "alpha": [".##.#", "#..#.", "#..#.", "#..#.", ".##.#"],
    "beta": [".##..", "#..#.", "###..", "#..#.", "#..#.", "###..", "#...."],
    "gamma": ["#...#", "#...#", ".#.#.", "..#..", "..#..", ".#.#.", "..#.."],
    "delta": [".###.", "#....", ".##..", "#..#.", "#..#.", ".##.."],
    "epsilon": [".###.", "#....", "###..", "#....", ".###."],
}


def glyph(cv, cx, cy, kind, scale=1):
    g = GLYPHS[kind]
    gh, gw = len(g), len(g[0])
    k = cv.k
    on = np.zeros((cv.h, cv.w), bool)
    x0 = int(round((cx + cv.bw / 2 - gw * scale / 2) * k))
    y0 = int(round((cy + cv.bh / 2 - gh * scale / 2) * k))
    s = scale * k
    for j, row in enumerate(g):
        for i, ch in enumerate(row):
            if ch == "#":
                on[y0 + j * s:y0 + (j + 1) * s, x0 + i * s:x0 + (i + 1) * s] = True
    out = np.zeros_like(on)
    hw = 1 if k == 1 else max(1, k // 2)  # the dark halo: one design pixel, on the sharp art one fine pixel (open letters)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
        out |= np.roll(np.roll(on, dy * hw, 0), dx * hw, 1)
    cv.flat(out & ~on, kind, 0.0)
    cv.flat(on, kind, 1.0)


def vortex(cv, cx, cy, rv, kind, t, seed, mask=None, letter=True, spin=1, arms=3, particles=14, glow=0.0):
    r, th = polar(cv, cx, cy)
    q = r / rv
    region = q < 1.3 if mask is None else (q < 1.3) & mask
    # soft outer glow
    if glow > 0:
        gr = (q >= 1.0) & (q < 1.0 + glow)
        if mask is not None:
            gr &= mask
        a = (1 - (q - 1.0) / glow) ** 2 * 0.55
        cv.alpha(kind, 0.15 + 0.25 * a, a, gr)
    sp = np.cos(arms * th + 5.0 * np.log(q + 0.06) - TAU * spin * t)
    tex = N2(seed, n=24, kmax=10, beta=1.0, P=96.0, tk=1)(th * 96 / TAU, q * 20, t)
    prof = np.clip((q - 0.2) / 0.35, 0, 1) * np.clip((1.18 - q) / 0.3, 0, 1)
    v = prof * (0.45 + 0.35 * sp + 0.25 * (tex - 0.5)) + 0.65 * np.exp(-((q - 0.98) / 0.07) ** 2)
    a = np.clip(v * 1.6, 0, 1)
    cv.alpha(kind, np.clip(v, 0, 1), a, region & (q >= 0.28))
    # the dark throat with the far side's stars turning the other way
    core = q < 0.3
    if mask is not None:
        core &= mask
    cv.shade(kind, 0.06 + 0.12 * (q / 0.3), core)
    rng = np.random.default_rng(seed + 50)
    n = max(5, int(rv * 0.9))
    sr = rng.uniform(0.05, 0.27, n) * rv
    sa = rng.uniform(0, TAU, n) - TAU * t * spin
    cv.dots(cx + sr * np.cos(sa), cy + sr * np.sin(sa), kind, rng.uniform(0.55, 0.95, n), mask)
    # matter spiralling in
    if particles:
        p0 = rng.uniform(0, 1, particles)
        a0 = rng.uniform(0, TAU, particles)
        sp_ = rng.integers(1, 3, particles)
        p = np.mod(p0 + sp_ * t, 1.0)
        pr = rv * (1.35 - 1.05 * p ** 0.8)
        pa = a0 + 2.6 * p ** 1.5 + TAU * spin * t / arms
        lvl = np.clip(0.45 + 0.55 * p, 0, 1) * np.clip(p / 0.15, 0, 1)
        cv.dots(cx + pr * np.cos(pa), cy + pr * np.sin(pa), kind, lvl, mask)
    if letter:
        glyph(cv, cx, cy, kind, 2 if rv >= 15 else 1)


# ---------------------------------------------------------------- gravity rift
def black_hole(cv, cx, cy, rh, t, seed, mask, disc=True, haze=True, inflow=60, reach=40.0, tilt=-0.18):
    r, th = polar(cv, cx, cy)
    rng = np.random.default_rng(seed)
    if haze:   # violet spiral haze
        q = r / reach
        h = N2(seed + 1, n=32, kmax=10, beta=1.1, P=96.0, tk=1)(np.mod(th + 3.0 * np.log(r + 1) - TAU * t, TAU) * 96 / TAU, r * 0.8, t)
        a = np.clip(1 - q, 0, 1) ** 1.3 * (0.35 + 0.65 * h)
        cv.alpha("rift", 0.2 + 0.7 * a, a, mask & (q < 1))
    if inflow:  # stars and gas pulled in along spirals
        p0 = rng.uniform(0, 1, inflow)
        a0 = rng.uniform(0, TAU, inflow)
        sp = rng.integers(1, 3, inflow)
        for lag, dim in ((0.0, 1.0), (0.012, 0.55), (0.024, 0.3)):
            p = np.mod(p0 + sp * t - lag, 1.0)
            pr = rh + (reach - rh) * (1 - p) ** 1.25
            pa = a0 + 5.0 * p ** 2
            lvl = np.clip(0.3 + 0.7 * p, 0, 1) * np.clip(p / 0.12, 0, 1) * dim
            hot = p > 0.7
            cv.dots((cx + pr * np.cos(pa))[~hot], (cy + pr * np.sin(pa))[~hot], "rift", lvl[~hot], mask)
            cv.dots((cx + pr * np.cos(pa))[hot], (cy + pr * np.sin(pa))[hot], "hot", lvl[hot] * 0.8, mask)
    ct, st = math.cos(tilt), math.sin(tilt)
    u = (cv.X - cx) * ct + (cv.Y - cy) * st
    w = (-(cv.X - cx) * st + (cv.Y - cy) * ct) / 0.3
    rd = np.hypot(u, w)
    ang = np.arctan2(w, u)
    hole = r < rh
    if disc:
        tex = N2(seed + 2, n=28, kmax=12, beta=0.9, P=96.0, tk=1)(np.mod(ang - TAU * t, TAU) * 96 / TAU, rd * 2.2, 0)
        din = rh * 1.25
        dout = rh * 3.3
        band = (rd > din) & (rd < dout)
        heat = np.clip(1 - (rd - din) / (dout - din), 0, 1)
        v = np.clip(0.25 + 0.75 * heat * (0.55 + 0.6 * tex), 0, 1)
        back = band & (w < 0) & mask
        front = band & (w >= 0) & mask
        cv.alpha("hot", v * 0.9, 0.35 + 0.65 * heat, back)
    # photon ring (the far side of the disc bent over the top)
    ring = (r >= rh) & (r < rh + 1.6) & mask
    rv = 0.55 + 0.4 * np.clip(-np.sin(th), 0, 1) + 0.1 * np.cos(4 * th - TAU * 2 * t)
    cv.shade("hot", rv, ring)
    cv.flat(hole & mask, "black", 0.0)
    if disc:
        cv.alpha("hot", v, 0.4 + 0.6 * heat, front & ~hole)


# ---------------------------------------------------------------- nebula
def nebula(cv, t, seed, mask, ramp_a="nebpink", ramp_b="nebblue", band=(0.6, 0.8, 30.0), cut=0.42, amt=1.0, gain=1.0,
           vmax=0.92, hole=None):
    w1 = N2(seed + 1, n=24, kmax=4, beta=1.4, tk=2)
    w2 = N2(seed + 2, n=24, kmax=4, beta=1.4, tk=2)
    wx = (w1(cv.X, cv.Y, t) - 0.5) * 18
    wy = (w2(cv.X, cv.Y, t) - 0.5) * 18
    d = N2(seed, n=64, kmax=9, beta=1.35, tk=4)(cv.X + wx, cv.Y + wy, t)
    if band:
        bx, by, bw = band
        d = d * 0.62 + 0.62 * np.exp(-((cv.X * bx + cv.Y * by) / bw) ** 2) - 0.14
    d = d * gain
    if hole:   # thin the clouds around a planet
        hx, hy, hr = hole
        d = d - 0.35 * np.clip(1 - (np.hypot(cv.X - hx, cv.Y - hy) - hr) / 8.0, 0, 1)
    hue = N2(seed + 3, n=16, kmax=3, beta=1.6, tm=0)(cv.X, cv.Y)
    fine = N2(seed + 4, n=48, kmax=18, beta=0.9, tk=6)(cv.X + wx, cv.Y + wy, t)
    a = np.clip((d - cut) / 0.2, 0, 1) * amt
    v = np.clip((d - cut) / (1.0 - cut), 0, 1) ** 0.85
    v = np.clip(v * (0.72 + 0.6 * (fine - 0.5)) + 0.12 * v, 0, 1) * vmax
    pink = (hue + (cv.B - 0.5) * 0.12) > 0.48
    cv.alpha(ramp_a, v, a, mask & pink)
    cv.alpha(ramp_b, v, a, mask & ~pink)
    return d


# ---------------------------------------------------------------- stars on fire
def star_body(cv, cx, cy, rs, t, seed, ramp="fire", lo=0.5, swirl=0.0, spin=1):
    m, lam, P, nz, d2, dx, dy = sphere(cv, cx, cy, rs, t, spin=spin, tilt=0.2)
    if swirl:
        lat = P[..., 1]
        P = P.copy()
        a = swirl * lat * 3.0
        x, z = P[..., 0] * np.cos(a) - P[..., 2] * np.sin(a), P[..., 0] * np.sin(a) + P[..., 2] * np.cos(a)
        P[..., 0], P[..., 2] = x, z
    n = N3(seed, fmin=2, fmax=14, beta=1.0, tm=1, tfrac=0.7)(P, t)
    v = lo + (1 - lo) * (0.35 + 0.65 * n) * (0.55 + 0.45 * nz)
    cv.shade(ramp, v, m)
    return m


def corona(cv, cx, cy, rs, t, seed, mask, ramp="fire", reach=2.5, rays=1.0, lo=0.15, hi=0.6):
    r, th = polar(cv, cx, cy)
    q = (r - rs) / (rs * (reach - 1))
    reg = (r > rs - 0.5) & (q < 1) & mask
    ray = N2(seed, n=32, kmax=14, beta=0.8, P=96.0, tk=1)(th * 96 / TAU, r * 0.4, t)
    f = np.clip(1 - q, 0, 1) ** 1.8
    a = f * (0.55 + rays * 0.9 * (ray - 0.5))
    cv.alpha(ramp, lo + (hi - lo) * np.clip(a, 0, 1), np.clip(a, 0, 1), reg)


def shockwave(cv, cx, cy, r0, r1, t, mask, ramp="fire", phase=0.0, level=0.8, width=1.2):
    r, th = polar(cv, cx, cy)
    p = np.mod(t + phase, 1.0)
    rr = r0 + (r1 - r0) * p ** 0.7
    ring = (np.abs(r - rr) < width) & mask
    a = (1 - p) ** 1.2
    cv.alpha(ramp, level * (0.5 + 0.5 * a), a * (0.6 + 0.4 * np.cos(6 * th)), ring)


def embers(cv, cx, cy, r0, r1, t, seed, n, mask, ramp="fire"):
    rng = np.random.default_rng(seed)
    p0 = rng.uniform(0, 1, n)
    a0 = rng.uniform(0, TAU, n)
    sp = rng.integers(1, 3, n)
    p = np.mod(p0 + sp * t, 1.0)
    pr = r0 + (r1 - r0) * p
    pa = a0 + 0.4 * p
    cv.dots(cx + pr * np.cos(pa), cy + pr * np.sin(pa), ramp, np.clip(0.95 - 0.7 * p, 0, 1), mask)


def flare(cv, cx, cy, rs, ang, t, phase, ramp, mask, height=0.9):
    """A plasma loop rising off the limb and falling back."""
    p = 0.5 - 0.5 * math.cos(TAU * (t + phase))
    hgt = rs * (0.12 + height * p)
    s = np.linspace(0, math.pi, 40)
    base = 0.16
    a = ang + (s / math.pi - 0.5) * 2 * base
    for dr, lv in ((0.0, 0.85), (-0.8, 0.7), (0.8, 0.5)):
        rr = rs - 0.5 + (hgt + dr) * np.sin(s)
        cv.dots(cx + rr * np.cos(a), cy + rr * np.sin(a), ramp, lv + 0.1 * np.sin(s), mask)


# ---------------------------------------------------------------- asteroids
def rock(cv, x, y, s, t, seed, spin, mask, ramp="rock", lum=1.0):
    rng = np.random.default_rng(seed)
    harm = [(j, rng.uniform(0.07, 0.2) / (j - 1), rng.uniform(0, TAU)) for j in range(2, 6)]
    lx = cv.X - x
    ly = cv.Y - y
    near = (np.abs(lx) < s * 1.6 + 1) & (np.abs(ly) < s * 1.6 + 1)
    if not near.any():
        return
    ang = -TAU * spin * t + rng.uniform(0, TAU)
    rx = lx * math.cos(ang) - ly * math.sin(ang)
    ry = lx * math.sin(ang) + ly * math.cos(ang)
    rho = np.hypot(rx, ry)
    al = np.arctan2(ry, rx)
    rb = s * (1 + sum(c * np.cos(j * al + ph) for j, c, ph in harm))
    inside = near & (rho <= rb) & mask
    q = np.clip(rho / np.maximum(rb, 0.1), 0, 1)
    nzz = np.sqrt(1 - q * q)
    lam = np.clip((lx / np.maximum(rb, 0.1)) * LIGHT[0] + (ly / np.maximum(rb, 0.1)) * LIGHT[1] + nzz * LIGHT[2], 0, 1)
    tex = 0.5 + 0.5 * np.sin(rx * 1.9 + seed) * np.sin(ry * 2.3 + seed * 0.7)
    v = (0.08 + 0.8 * lam * (0.75 + 0.25 * tex)) * lum
    v = np.where(rho > rb - 0.9, v * 0.45, v)
    cv.shade(ramp, v, inside)


def asteroid_field(cv, t, seed, mask, n=26, smin=1.4, smax=6.0, belt=None, avoid=None):
    rng = np.random.default_rng(seed)
    rocks = []
    tries = 0
    while len(rocks) < n and tries < 2000:
        tries += 1
        s = smin + (smax - smin) * rng.random() ** 2.2
        if belt:
            bx, by, bw = belt
            u = rng.uniform(-46, 46)
            off = rng.normal(0, bw)
            x, y = u * bx - off * by, u * by + off * bx
        else:
            x, y = rng.uniform(-44, 44), rng.uniform(-38, 38)
        if SQ3 * abs(x) + abs(y) > SQ3 * R - 2 * (s + 3) or abs(y) > R * SQ3 / 2 - (s + 3):
            continue
        if avoid and math.hypot(x - avoid[0], y - avoid[1]) < avoid[2] + s:
            continue
        if any(math.hypot(x - a, y - b) < s + c + 1.5 for a, b, c, *_ in rocks):
            continue
        rocks.append((x, y, s, rng.integers(0, 1 << 30), int(rng.choice([-1, 0, 1])), rng.uniform(0.4, 1.8), rng.uniform(0, TAU)))
    for x, y, s, sd, spin, bob, ph in sorted(rocks, key=lambda r: r[2]):
        bx = x + bob * math.cos(TAU * t + ph)
        by = y + 0.6 * bob * math.sin(TAU * t + ph)
        rock(cv, bx, by, s, t, int(sd), spin, mask)
    return rocks


def dust(cv, t, seed, mask, amount=0.35):
    d = N2(seed, n=40, kmax=8, beta=1.2, tk=3)(cv.X, cv.Y, t)
    a = np.clip((d - 0.55) / 0.3, 0, 1) * amount
    cv.alpha("dust", 0.4 + 0.6 * d, a, mask)


def glints(cv, t, seed, mask, n=10):
    rng = np.random.default_rng(seed)
    ys, xs = np.nonzero(mask)
    pick = rng.choice(len(xs), n, replace=False)
    ph = rng.uniform(0, 1, n)
    on = np.mod(t * 2 + ph, 1.0) < 0.08
    cv.dots(cv.X[ys[pick], xs[pick]][on], cv.Y[ys[pick], xs[pick]][on], "star", 1.0, mask)


# ---------------------------------------------------------------- warp (Mallice)
def warp(cv, t, seed, mask, intensity=1.0, nb=110, density=0.75, speeds=(1, 2, 3), ramp="warp", rmax=64.0):
    r, th = polar(cv, 0, 0)
    rng = np.random.default_rng(seed)
    b = np.floor(np.mod(th, TAU) / TAU * nb).astype(int) % nb
    on = rng.random(nb) < density
    p0 = rng.uniform(0, 1, nb)
    sp = rng.choice(speeds, nb)
    ln = rng.uniform(0.08, 0.22, nb)
    u = np.log1p(r) / math.log1p(rmax)
    rel = np.mod(u - (p0[b] + sp[b] * t), 1.0)
    inside = (rel < ln[b]) & on[b] & mask & (r > 2)
    lvl = (0.35 + 0.65 * rel / ln[b]) * intensity * np.clip(u * 1.6, 0, 1)
    cv.alpha(ramp, lvl, np.clip(lvl * 1.4, 0, 1), inside)


# ---------------------------------------------------------------- tokens
def token_base(cv, ramp, seed, t):
    m = disc_mask(cv, 19.5)
    space(cv, seed, m, lo=0.05, hi=0.6)
    stars(cv, seed + 1, t, m, density=0.02, cross=False)
    return m


def token_rim(cv, ramp):
    r = np.hypot(cv.X, cv.Y)
    cv.flat((r > 18.4) & (r <= 19.5), ramp, 0.75)
    cv.flat((r > 17.4) & (r <= 18.4), ramp, 0.2)


def bolt(cv, t, seed, mask, cx=0.0, cy=0.0, reach=16.0, every=3, ramp="storm"):
    """Lightning: a jagged line that flashes a few times per loop."""
    for k in range(every):
        rng = np.random.default_rng(seed + k * 31)
        start = (k + rng.uniform(0, 0.6)) / every
        age = np.mod(t - start, 1.0)
        if age > 0.06:
            continue
        a = rng.uniform(0, TAU)
        x, y = cx + reach * math.cos(a), cy + reach * math.sin(a)
        pts = [(x, y)]
        for _ in range(14):
            ang = math.atan2(cy - y, cx - x) + rng.normal(0, 0.8)
            x += 1.3 * math.cos(ang)
            y += 1.3 * math.sin(ang)
            pts.append((x, y))
        xs = np.array([p[0] for p in pts])
        ys = np.array([p[1] for p in pts])
        lvl = 1.0 if age < 0.03 else 0.7
        cv.dots(xs, ys, ramp, lvl, mask)
        cv.dots(xs + 1, ys, ramp, lvl * 0.6, mask)


# ---------------------------------------------------------------- pieces
PIECES = []


def piece(name, label, kind="tile", note=""):
    def deco(fn):
        PIECES.append({"name": name, "label": label, "kind": kind, "note": note, "fn": fn})
        return fn
    return deco


def tile_canvas():
    cv = Cv(W, H)
    return cv, hex_mask(cv)


# planet / wormhole spots as in map_view.cpp drawSystem (size = R)
WH_PLANET = (0.45 * R, -0.45 * R)    # wormhole on a tile with planets
WH_R_PLANET = 0.19 * R
PL_R = 0.24 * R                      # one ordinary planet (game disc 0.2)
HOME_R = 0.29 * R


@piece("18_mecatol_rex", "18 · Mecatol Rex", note="The old imperial city world: city lights on the night side, a gold orbital ring with a station.")
def p18(t):
    cv, hm = tile_canvas()
    space(cv, 1801, hm)
    stars(cv, 1802, t, hm)
    pr = 0.36 * R
    ring_a, ring_b, tilt = 27.0, 7.5, -0.22
    ph = np.linspace(0, TAU, 900, endpoint=False)
    ex, ey = ring_a * np.cos(ph), ring_b * np.sin(ph)
    rx = ex * math.cos(tilt) - ey * math.sin(tilt)
    ry = ex * math.sin(tilt) + ey * math.cos(tilt)
    dash = (np.floor(ph / TAU * 64 - 4 * t) % 4) != 3
    front = np.sin(ph) > 0
    cv.dots(rx[~front & dash], ry[~front & dash], "gold", 0.35, hm)
    st_ph = TAU * t + 0.6
    st_front = math.sin(st_ph) > 0

    def station():
        sx = ring_a * math.cos(st_ph)
        sy = ring_b * math.sin(st_ph)
        x = sx * math.cos(tilt) - sy * math.sin(tilt)
        y = sx * math.sin(tilt) + sy * math.cos(tilt)
        cv.dots([x, x + 1, x, x + 1], [y, y, y + 1, y + 1], "city", 1.0, hm)

    if not st_front:
        station()
    planet(cv, 0, 0, pr, t, "city", 1804, spin=1, tilt=0.3, atmo="atmo")
    cv.dots(rx[front & dash], ry[front & dash], "gold", 0.65, hm)
    if st_front:
        station()
    plain_edge(cv)
    return cv


@piece("41_gravity_rift", "41 · Gravity Rift", note="A black hole: lensed accretion disc, violet spiral haze, stars dragged in.")
def p41(t):
    cv, hm = tile_canvas()
    space(cv, 4101, hm, lo=0.0, hi=0.6)
    stars(cv, 4102, t, hm, density=0.008)
    black_hole(cv, 0, 0, 7.5, t, 4103, hm)
    anomaly_border(cv)
    return cv


@piece("42_nebula", "42 · Nebula", note="Drifting pink and blue clouds with stars inside.")
def p42(t):
    cv, hm = tile_canvas()
    space(cv, 4201, hm)
    stars(cv, 4202, t, hm, density=0.012)
    nebula(cv, t, 4203, hm)
    stars(cv, 4204, t, hm, density=0.004, bright=1.2)
    anomaly_border(cv)
    return cv


@piece("43_supernova", "43 · Supernova", note="A white-hot star with a flickering corona and a shock wave every loop.")
def p43(t):
    cv, hm = tile_canvas()
    space(cv, 4301, hm, lo=0.0, hi=0.6)
    stars(cv, 4302, t, hm, density=0.008)
    shockwave(cv, 0, 0, 16, 46, t, hm, "fire", level=0.75)
    corona(cv, 0, 0, 15, t, 4303, hm, reach=2.7, lo=0.2, hi=0.75)
    embers(cv, 0, 0, 16, 44, t, 4304, 50, hm)
    star_body(cv, 0, 0, 15, t, 4305, lo=0.62)
    anomaly_border(cv)
    return cv


@piece("44_asteroid_field", "44 · Asteroid Field", note="Tumbling rocks of every size, dust and the odd glint.")
def p44(t):
    cv, hm = tile_canvas()
    space(cv, 4401, hm)
    stars(cv, 4402, t, hm, density=0.01)
    dust(cv, t, 4403, hm, 0.4)
    asteroid_field(cv, t, 4404, hm, n=30, smin=1.5, smax=6.8)
    glints(cv, t, 4405, hm, 8)
    anomaly_border(cv)
    return cv


@piece("45_asteroid_field", "45 · Asteroid Field", note="A dense diagonal belt with big boulders.")
def p45(t):
    cv, hm = tile_canvas()
    space(cv, 4501, hm)
    stars(cv, 4502, t, hm, density=0.01)
    dust(cv, t, 4503, hm, 0.5)
    asteroid_field(cv, t, 4504, hm, n=40, smin=1.2, smax=7.5, belt=(0.8, -0.6, 13.0))
    glints(cv, t, 4505, hm, 8)
    anomaly_border(cv)
    return cv


@piece("39_alpha_wormhole", "39 · Alpha Wormhole", note="Orange alpha wormhole: spiral arms, the far side's stars in the throat.")
def p39(t):
    cv, hm = tile_canvas()
    space(cv, 3901, hm)
    stars(cv, 3902, t, hm)
    vortex(cv, 0, 0, 17, "alpha", t, 3903, hm, particles=22, glow=0.7)
    plain_edge(cv)
    return cv


@piece("40_beta_wormhole", "40 · Beta Wormhole", note="Green beta wormhole.")
def p40(t):
    cv, hm = tile_canvas()
    space(cv, 4001, hm)
    stars(cv, 4002, t, hm)
    vortex(cv, 0, 0, 17, "beta", t, 4003, hm, particles=22, glow=0.7, spin=-1)
    plain_edge(cv)
    return cv


@piece("25_quann", "25 · Quann (beta wormhole)", note="Ocean world with moving clouds; beta wormhole where the game puts it.")
def p25(t):
    cv, hm = tile_canvas()
    space(cv, 2501, hm)
    stars(cv, 2502, t, hm)
    vortex(cv, *WH_PLANET, WH_R_PLANET, "beta", t, 2503, hm, particles=8, glow=0.5, spin=-1)
    planet(cv, 0, 0, PL_R, t, "ocean", 2504, atmo="atmo")
    plain_edge(cv)
    return cv


@piece("26_lodor", "26 · Lodor (alpha wormhole)", note="Desert world with dune bands; alpha wormhole.")
def p26(t):
    cv, hm = tile_canvas()
    space(cv, 2601, hm)
    stars(cv, 2602, t, hm)
    vortex(cv, *WH_PLANET, WH_R_PLANET, "alpha", t, 2603, hm, particles=8, glow=0.5)
    planet(cv, 0, 0, PL_R, t, "desert", 2604, atmo="sand")
    plain_edge(cv)
    return cv


@piece("64_atlas", "64 · Atlas (beta wormhole)", note="Volcanic world with pulsing lava cracks; beta wormhole.")
def p64(t):
    cv, hm = tile_canvas()
    space(cv, 6401, hm)
    stars(cv, 6402, t, hm)
    vortex(cv, *WH_PLANET, WH_R_PLANET, "beta", t, 6403, hm, particles=8, glow=0.5, spin=-1)
    planet(cv, 0, 0, PL_R, t, "lava", 6404)
    plain_edge(cv)
    return cv


@piece("67_cormund", "67 · Cormund (gravity rift)", note="A rust world being torn apart: debris streams into a small black hole.")
def p67(t):
    cv, hm = tile_canvas()
    space(cv, 6701, hm, lo=0.0, hi=0.65)
    stars(cv, 6702, t, hm, density=0.008)
    bx, by = -23.0, 19.0
    black_hole(cv, bx, by, 5.0, t, 6703, hm, inflow=30, reach=26.0)
    # debris stream from the planet to the hole
    rng = np.random.default_rng(6704)
    n = 40
    p0 = rng.uniform(0, 1, n)
    off = rng.normal(0, 1.2, n)
    sp = rng.integers(1, 3, n)
    p = np.mod(p0 + sp * t, 1.0)
    sx, sy = -6.0, 9.0          # leaves the planet's lower-left limb
    cx_, cy_ = -20.0, 2.0       # bends round
    x = (1 - p) ** 2 * sx + 2 * (1 - p) * p * cx_ + p * p * bx
    y = (1 - p) ** 2 * sy + 2 * (1 - p) * p * cy_ + p * p * by
    x = x + off * (1 - p)
    lv = np.clip(0.9 - 0.5 * p, 0, 1)
    cv.dots(x, y, "rust", lv, hm)
    cv.dots(x[p > 0.6], y[p > 0.6], "hot", lv[p > 0.6], hm)
    planet(cv, 0, 0, PL_R, t, "rust", 6705)
    anomaly_border(cv)
    return cv


@piece("68_everra", "68 · Everra (nebula)", note="Violet world half-veiled by the nebula.")
def p68(t):
    cv, hm = tile_canvas()
    space(cv, 6801, hm)
    stars(cv, 6802, t, hm)
    d = nebula(cv, t, 6803, hm, band=(0.8, -0.6, 34.0), hole=(0, 0, PL_R))
    planet(cv, 0, 0, PL_R, t, "lavender", 6804, atmo="lavender")
    # a thin veil drifting over the planet
    pm = disc_mask(cv, PL_R + 2)
    a = np.clip((d - 0.5) / 0.3, 0, 1) * 0.35
    cv.alpha("nebpink", 0.55, a, pm & hm)
    anomaly_border(cv)
    return cv


@piece("79_asteroids_alpha", "79 · Asteroid Field + Alpha Wormhole", note="An alpha wormhole inside an asteroid field.")
def p79(t):
    cv, hm = tile_canvas()
    space(cv, 7901, hm)
    stars(cv, 7902, t, hm)
    dust(cv, t, 7903, hm, 0.35)
    vortex(cv, 0, 0, 14, "alpha", t, 7904, hm, particles=14, glow=0.5)
    asteroid_field(cv, t, 7905, hm, n=26, smax=5.5, avoid=(0, 0, 19))
    glints(cv, t, 7906, hm, 6)
    anomaly_border(cv)
    return cv


@piece("80_supernova", "80 · Supernova", note="A molten, swirling sun with plasma loops on the limb.")
def p80(t):
    cv, hm = tile_canvas()
    space(cv, 8001, hm, lo=0.0, hi=0.6)
    stars(cv, 8002, t, hm, density=0.008)
    corona(cv, 0, 0, 18, t, 8003, hm, reach=2.2, lo=0.15, hi=0.6)
    embers(cv, 0, 0, 18, 42, t, 8004, 34, hm)
    star_body(cv, 0, 0, 18, t, 8005, lo=0.3, swirl=0.45)
    for k, (ang, ph) in enumerate(((-2.3, 0.0), (0.4, 0.35), (2.2, 0.7), (-0.9, 0.55))):
        flare(cv, 0, 0, 18, ang, t, ph, "fire", hm, height=0.4)
    anomaly_border(cv)
    return cv


@piece("81_muaat_supernova", "81 · Muaat Supernova (Adjudicator Ba'al)", note="The supernova the Muaat hero makes: a red giant blowing out a ring of debris.")
def p81(t):
    cv, hm = tile_canvas()
    space(cv, 8101, hm, lo=0.0, hi=0.55)
    stars(cv, 8102, t, hm, density=0.008)
    shockwave(cv, 0, 0, 16, 46, t, hm, "crimson", level=0.9, width=1.8)
    shockwave(cv, 0, 0, 16, 46, t, hm, "crimson", phase=0.5, level=0.7, width=1.4)
    corona(cv, 0, 0, 16, t, 8103, hm, ramp="crimson", reach=2.4, lo=0.3, hi=0.85)
    # debris ring flung outwards
    rng = np.random.default_rng(8104)
    for i in range(22):
        p = np.mod(rng.uniform() + t, 1.0)
        a = rng.uniform(0, TAU) + 0.3 * p
        rr = 17 + 28 * p
        rock(cv, rr * math.cos(a), rr * math.sin(a), 1.6 + 1.8 * rng.uniform(), t, 8110 + i, 1, hm, ramp="rock", lum=1.15 - 0.5 * p)
    embers(cv, 0, 0, 16, 44, t, 8105, 40, hm, "crimson")
    star_body(cv, 0, 0, 16, t, 8106, ramp="crimson", lo=0.5)
    anomaly_border(cv)
    return cv


def mallice(t, active):
    cv, hm = tile_canvas()
    space(cv, 8201, hm, ramp="space", lo=0.0, hi=0.7)
    r, th = polar(cv, 0, 0)
    gl = np.clip(1 - r / 40, 0, 1) ** 1.5 * (0.9 if active else 0.4)
    cv.alpha("warp", 0.2 + 0.5 * gl, gl, hm)
    stars(cv, 8202, t, hm, density=0.006 if active else 0.01)
    if active:
        warp(cv, t, 8203, hm, intensity=1.0, nb=120, density=0.8, speeds=(1, 2, 3))
        warp(cv, t, 8204, hm, intensity=0.9, nb=60, density=0.4, speeds=(1, 2), ramp="gamma")
    else:
        warp(cv, t, 8203, hm, intensity=0.45, nb=80, density=0.35, speeds=(1,))
    kinds = ["alpha", "beta", "gamma"] if active else ["gamma"]
    step = 0.19 * R * 2.0
    for k, kind in enumerate(kinds):
        vortex(cv, WH_PLANET[0], WH_PLANET[1] + k * step, 0.17 * R, kind, t, 8210 + k, hm, particles=6, glow=0.4, spin=1 if k % 2 == 0 else -1)
    planet(cv, 0, 0, PL_R, t, "mallice", 8205, atmo="teal" if active else None)
    if not active:   # dimmer, as if seen through a veil
        cv.idx[(cv.B < 0.18) & disc_mask(cv, PL_R) & hm] = RI["teal"][1]
    plain_edge(cv)
    return cv


@piece("82a_mallice_inactive", "82 · Mallice — Wormhole Nexus, inactive", note="Faint warp, only the gamma wormhole.")
def p82a(t):
    return mallice(t, False)


@piece("82b_mallice_active", "82 · Mallice — Wormhole Nexus, active", note="Full warp tunnel behind the planet; alpha, beta and gamma wormholes (game positions).")
def p82b(t):
    return mallice(t, True)


@piece("17_creuss_gate", "17 · Creuss Gate (delta wormhole)", note="The Creuss gate: a big delta wormhole with energy streamers.")
def p17(t):
    cv, hm = tile_canvas()
    space(cv, 1701, hm)
    stars(cv, 1702, t, hm)
    r, th = polar(cv, 0, 0)
    arm = np.cos(4 * th + 0.18 * r - TAU * t)
    a = np.clip(1 - r / 44, 0, 1) * np.clip((arm - 0.55) / 0.45, 0, 1) * 0.8
    cv.alpha("delta", 0.25 + 0.35 * a, a, hm & (r > 18))
    vortex(cv, 0, 0, 20, "delta", t, 1703, hm, particles=26, glow=0.6)
    plain_edge(cv)
    return cv


@piece("51_creuss", "51 · Creuss (home, delta wormhole)", note="Icy Creuss with glowing veins; the delta wormhole of the home system.")
def p51(t):
    cv, hm = tile_canvas()
    space(cv, 5101, hm)
    stars(cv, 5102, t, hm)
    vortex(cv, *WH_PLANET, WH_R_PLANET, "delta", t, 5103, hm, particles=8, glow=0.5)
    planet(cv, 0, 0, HOME_R, t, "ice", 5104, atmo="delta")
    plain_edge(cv)
    return cv


@piece("56_the_dark", "56 · The Dark (Empyrean home, nebula)", note="A dark nebula hiding an almost unlit world.")
def p56(t):
    cv, hm = tile_canvas()
    space(cv, 5601, hm, lo=0.0, hi=0.5)
    stars(cv, 5602, t, hm, density=0.006, bright=0.7)
    nebula(cv, t, 5603, hm, ramp_a="nebdark", ramp_b="nebdark", band=None, cut=0.36, gain=1.05, vmax=1.0, hole=(0, 0, HOME_R))
    planet(cv, 0, 0, HOME_R, t, "dark", 5604, atmo="rift")
    anomaly_border(cv)
    return cv


def token(name, label, note):
    def deco(fn):
        PIECES.append({"name": name, "label": label, "kind": "token", "note": note, "fn": fn})
        return fn
    return deco


def worm_token(kind, seed, t, spin=1):
    cv = Cv(TOK, TOK)
    m = token_base(cv, kind, seed, t)
    vortex(cv, 0, 0, 15, kind, t, seed + 2, m, particles=10, glow=0.25, spin=spin)
    token_rim(cv, kind)
    return cv


@token("token_alpha", "Alpha wormhole token (Creuss)", "Round token; the same wormhole style.")
def t_alpha(t):
    return worm_token("alpha", 9001, t)


@token("token_beta", "Beta wormhole token (Creuss)", "")
def t_beta(t):
    return worm_token("beta", 9011, t, spin=-1)


@token("token_gamma", "Gamma wormhole token (exploration)", "")
def t_gamma(t):
    return worm_token("gamma", 9021, t)


def ion_storm(kind, seed, t):
    cv = Cv(TOK, TOK)
    m = token_base(cv, "storm", seed, t)
    r, th = polar(cv, 0, 0)
    tex = N2(seed + 1, n=36, kmax=10, beta=1.0, P=96.0, tk=1)(np.mod(th + 2.2 * np.log(r + 1) - TAU * t, TAU) * 96 / TAU, r * 1.2, t)
    a = np.clip((tex - 0.3) / 0.4, 0, 1) * np.clip(1.2 - r / 19, 0, 1)
    cv.alpha("storm", 0.25 + 0.6 * tex, a, m)
    bolt(cv, t, seed + 5, m, reach=15.0, every=3)
    glyph(cv, 0, 0, kind, 2)
    token_rim(cv, "storm")
    return cv


@token("token_ion_storm_alpha", "Ion Storm token — alpha side", "Swirling storm with lightning; flips to beta.")
def t_ion_a(t):
    return ion_storm("alpha", 9101, t)


@token("token_ion_storm_beta", "Ion Storm token — beta side", "")
def t_ion_b(t):
    return ion_storm("beta", 9111, t)


@token("token_dimensional_tear", "Dimensional Tear (Vuil'raith)", "A crack in space that works as a gravity rift.")
def t_tear(t):
    cv = Cv(TOK, TOK)
    m = disc_mask(cv, 19.5)
    space(cv, 9201, m, ramp="nebdark", lo=0.1, hi=0.7)
    stars(cv, 9202, t, m, density=0.015, cross=False)
    r, th = polar(cv, 0, 0)
    h = N2(9203, n=24, kmax=8, beta=1.1, P=96.0, tk=1)(np.mod(th + 2.5 * np.log(r + 1) - TAU * t, TAU) * 96 / TAU, r, t)
    a = np.clip(1 - r / 19, 0, 1) * (0.3 + 0.7 * h)
    cv.alpha("tear", 0.3 + 0.5 * a, a, m)
    # a jagged tear across the token: wide in the middle, black inside, hot edges
    rng = np.random.default_rng(9204)
    n = 9
    xs = np.linspace(-14, 14, n)
    ys = -xs * 0.55 + rng.normal(0, 2.2, n)
    ys[0], ys[-1] = 7.5, -7.5
    d = np.full(cv.idx.shape, 99.0)
    sacc = np.zeros(cv.idx.shape)
    for i in range(n - 1):
        ax, ay, bx, by = xs[i], ys[i], xs[i + 1], ys[i + 1]
        vx, vy = bx - ax, by - ay
        u = np.clip(((cv.X - ax) * vx + (cv.Y - ay) * vy) / (vx * vx + vy * vy), 0, 1)
        dd = np.hypot(cv.X - (ax + u * vx), cv.Y - (ay + u * vy))
        closer = dd < d
        d = np.where(closer, dd, d)
        sacc = np.where(closer, (i + u) / (n - 1), sacc)
    pulse = 0.5 + 0.5 * math.cos(TAU * 2 * t)
    wid = 0.4 + (2.4 + 0.5 * pulse) * np.sin(math.pi * sacc)
    cv.alpha("tear", 0.45 + 0.25 * pulse, np.clip(1 - (d - wid) / 4.5, 0, 1) * 0.8, (d < wid + 4.5) & m)
    cv.shade("tear", 0.9 + 0.1 * pulse, (d < wid + 1.0) & m)
    cv.flat((d < wid) & m, "tear", 0.0)
    inner = (d < wid - 0.8) & m
    cv.alpha("tear", 0.3, 0.25, inner)
    embers(cv, 0, 0, 18, 2, t, 9205, 18, m, "tear")
    token_rim(cv, "tear")
    return cv


# ---------------------------------------------------------------- output
def render(p):
    frames = []
    for f in range(F):
        cv = p["fn"](f / F)
        frames.append(cv.idx.copy())
    return np.stack(frames)


def save(p, frames, out):
    used = np.unique(frames)
    used = used[used != 0]
    lut = np.zeros(len(PAL), np.uint8)
    lut[used] = np.arange(1, len(used) + 1)
    pal = [0, 0, 0] + [c for i in used for c in PAL[i]]
    pal += [0] * (768 - len(pal))
    n, h, w = frames.shape
    strip = np.concatenate(list(lut[frames]), axis=1)
    im = Image.fromarray(strip, "P")
    im.putpalette(pal)
    im.info["transparency"] = 0
    im.save(os.path.join(out, p["name"] + ".png"), transparency=0, optimize=True)
    gif = []
    for fr in frames:
        big = lut[fr].repeat(3, 0).repeat(3, 1)
        g = Image.fromarray(big, "P")
        g.putpalette(pal)
        gif.append(g)
    gif[0].save(os.path.join(out, p["name"] + ".gif"), save_all=True, append_images=gif[1:], duration=FRAME_MS,
                loop=0, transparency=0, disposal=2, optimize=False)
    return {"name": p["name"], "label": p["label"], "kind": p["kind"], "note": p["note"], "w": w, "h": h,
            "frames": n, "ms": FRAME_MS, "colors": int(len(used))}


def rgba(frames_idx):
    rgb = PAL_NP[frames_idx]
    a = np.where(frames_idx == 0, 0, 255).astype(np.uint8)
    return np.dstack([rgb, a])


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/tile_art"
    want = set(sys.argv[2:])
    os.makedirs(out, exist_ok=True)
    meta = []
    firsts = []
    for p in PIECES:
        if want and p["name"] not in want:
            continue
        frames = render(p)
        meta.append(save(p, frames, out))
        firsts.append((p, frames[0]))
        print(p["name"], frames.shape, meta[-1]["colors"], "colours", flush=True)
    # contact sheet of first frames (2x)
    cols = 5
    cw, ch = W * 2 + 12, H * 2 + 12
    rows = (len(firsts) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cw + 12, rows * ch + 12), (14, 16, 24, 255))
    for i, (p, fr) in enumerate(firsts):
        im = Image.fromarray(rgba(fr), "RGBA").resize((fr.shape[1] * 2, fr.shape[0] * 2), Image.NEAREST)
        x = 12 + (i % cols) * cw + (W * 2 - im.width) // 2
        y = 12 + (i // cols) * ch + (H * 2 - im.height) // 2
        sheet.alpha_composite(im, (x, y))
    sheet.save(os.path.join(out, "sheet.png"))
    with open(os.path.join(out, "pieces.json"), "w") as f:
        json.dump(meta, f, indent=1)


if __name__ == "__main__":
    main()
