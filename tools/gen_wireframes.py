#!/usr/bin/env python3
"""Wireframes for unit and technology cards (Stage 7, owner 2026-10-22).

Units: the owner's template outlines (TI4templates.zip > StandardUnits >
Generic*.xcf, layers "<unit>outline.png"; the dreadnought from
"dreadnoughtoutlinefull.png" and the war sun from "warsunoutline.png" in
GenericDreadnought.xcf; he has permission to use them), saved as
docs/owner/wireframes_src/<unit>.png, are traced 1:1 into vector lines: the
white strokes are thinned to one-pixel skeletons and followed into
polylines, so the game draws them as crisp lines at any size (like its other
card designs). The flagship template is an outline only (no inner detail).

Technologies without a unit: my own wireframes (the owner: "create a
wireframe of what you think the technology would look like"), built from
simple shapes below.

usage: gen_wireframes.py [preview.png]
  writes src/ui/gen/wireframes.inc (and a preview sheet if asked)
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "owner", "wireframes_src")
OUT = os.path.join(ROOT, "src", "ui", "gen", "wireframes.inc")

# ------------------------------------------------------------------ tracing
def rdp(pts, eps):
    if len(pts) < 3:
        return pts
    a, b = np.array(pts[0], float), np.array(pts[-1], float)
    ab = b - a
    n = np.hypot(*ab)
    best, idx = -1.0, 0
    for i in range(1, len(pts) - 1):
        p = np.array(pts[i], float)
        d = abs(ab[0] * (a[1] - p[1]) - ab[1] * (a[0] - p[0])) / n if n > 1e-9 else np.hypot(*(p - a))
        if d > best:
            best, idx = d, i
    if best <= eps:
        return [pts[0], pts[-1]]
    return rdp(pts[: idx + 1], eps)[:-1] + rdp(pts[idx:], eps)


def trace(path, eps=0.75, min_len=5, thr=55):
    """The template's white strokes (grey and noisy after scanning: a low
    threshold keeps them joined) thinned to 1 px and followed into lines."""
    from skimage.morphology import skeletonize, closing, disk, remove_small_objects
    img = np.array(Image.open(path).convert("RGBA")).astype(float)
    lum = img[..., 0] * 0.3 + img[..., 1] * 0.59 + img[..., 2] * 0.11
    mask = (img[..., 3] > 100) & (lum > thr)
    mask = closing(mask, disk(1))
    mask = remove_small_objects(mask, max_size=6)
    sk = skeletonize(mask)
    h, w = sk.shape
    nb8 = [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (-1, 1), (1, -1), (1, 1)]

    def neigh(p):
        y, x = p
        return [(y + dy, x + dx) for dy, dx in nb8 if (y + dy, x + dx) in pixels]

    pixels = set(zip(*np.nonzero(sk)))
    deg = {p: len(neigh(p)) for p in pixels}
    visited = set()
    paths = []

    def walk(start):
        path = [start]
        visited.add(start)
        cur = start
        while True:
            cand = [q for q in neigh(cur) if q not in visited]
            if not cand:
                # close the gap to an already traced pixel (junctions)
                done = [q for q in neigh(cur) if q in visited and (len(path) < 2 or q != path[-2])]
                if done and len(path) > 1:
                    path.append(done[0])
                break
            # prefer straight (4-connected) steps
            cand.sort(key=lambda q: abs(q[0] - cur[0]) + abs(q[1] - cur[1]))
            nxt = cand[0]
            visited.add(nxt)
            path.append(nxt)
            cur = nxt
        return path

    for p in sorted(pixels, key=lambda p: (deg[p], p)):  # endpoints first
        if p in visited:
            continue
        if deg[p] == 1 or deg[p] == 0:
            paths.append(walk(p))
    for p in sorted(pixels):
        if p not in visited:
            paths.append(walk(p))
    out = []
    for pth in paths:
        if len(pth) < min_len:
            continue
        pts = [(x + 0.5, y + 0.5) for y, x in pth]
        out.append(rdp(pts, eps))
    return out, w, h


# ------------------------------------------------------------------ my own drawings
# A drawing is a list of polylines in a box 1.0 wide and `H` tall.
TECH_H = 0.62


class Pen:
    def __init__(self):
        self.polys = []

    def line(self, *pts):
        self.polys.append([(float(x), float(y)) for x, y in pts])
        return self

    def loop(self, *pts):
        p = [(float(x), float(y)) for x, y in pts]
        self.polys.append(p + [p[0]])
        return self

    def circle(self, cx, cy, r, n=None, a0=0.0, a1=360.0, ry=None):
        ry = r if ry is None else ry
        n = n or max(10, int(40 * max(r, ry) / 0.2))
        pts = []
        for i in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * i / n)
            pts.append((cx + r * math.cos(a), cy + ry * math.sin(a)))
        self.polys.append(pts)
        return self

    def rect(self, x, y, w, h):
        return self.loop((x, y), (x + w, y), (x + w, y + h), (x, y + h))

    def poly(self, cx, cy, r, n, rot=0.0):
        pts = [(cx + r * math.cos(math.radians(rot + 360 * i / n)), cy + r * math.sin(math.radians(rot + 360 * i / n))) for i in range(n)]
        return self.loop(*pts)

    def gear(self, cx, cy, r, teeth=8, depth=0.18):
        pts = []
        for i in range(teeth * 4):
            a = 2 * math.pi * i / (teeth * 4)
            rr = r if (i // 2) % 2 == 0 else r * (1 - depth)
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        self.loop(*pts)
        return self.circle(cx, cy, r * 0.35)

    def wave(self, x0, x1, y, amp, cycles, n=40):
        return self.line(*[(x0 + (x1 - x0) * i / n, y + amp * math.sin(2 * math.pi * cycles * i / n)) for i in range(n + 1)])

    def spiral(self, cx, cy, r, turns=2.5, n=80):
        return self.line(*[(cx + r * (i / n) * math.cos(2 * math.pi * turns * i / n), cy + r * (i / n) * math.sin(2 * math.pi * turns * i / n)) for i in range(n + 1)])

    def arrow(self, x0, y0, x1, y1, head=0.04):
        self.line((x0, y0), (x1, y1))
        a = math.atan2(y1 - y0, x1 - x0)
        for s in (-1, 1):
            self.line((x1, y1), (x1 - head * math.cos(a + s * 0.5), y1 - head * math.sin(a + s * 0.5)))
        return self

    def hexagon(self, cx, cy, r):
        return self.poly(cx, cy, r, 6, 0)

    def ship(self, cx, cy, s, ang=0.0):
        """A small generic hull, pointing right (rotated by ang degrees)."""
        base = [(1.0, 0), (0.3, -0.35), (-0.8, -0.35), (-1.0, -0.15), (-1.0, 0.15), (-0.8, 0.35), (0.3, 0.35)]
        c, sn = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        pts = [(cx + s * (x * c - y * sn), cy + s * (x * sn + y * c)) for x, y in base]
        self.loop(*pts)
        self.line((cx + s * (-0.6 * c), cy + s * (-0.6 * sn)), (cx + s * (0.5 * c), cy + s * (0.5 * sn)))
        return self

    def dna(self, x0, x1, cy, amp, cycles=2, n=60):
        a = [(x0 + (x1 - x0) * i / n, cy + amp * math.sin(2 * math.pi * cycles * i / n)) for i in range(n + 1)]
        b = [(x0 + (x1 - x0) * i / n, cy - amp * math.sin(2 * math.pi * cycles * i / n)) for i in range(n + 1)]
        self.line(*a)
        self.line(*b)
        for i in range(0, n + 1, 5):
            self.line(a[i], b[i])
        return self

    def planet(self, cx, cy, r):
        self.circle(cx, cy, r)
        self.circle(cx, cy, r, ry=r * 0.25, a0=-10, a1=190)
        return self.circle(cx, cy, r * 1.6, ry=r * 0.35, a0=-20, a1=200)


def techs():
    """Non-unit technologies: key -> drawing (box 1.0 x TECH_H)."""
    H = TECH_H
    d = {}

    def new(k):
        p = Pen()
        d[k] = p
        return p

    # ---- blue (propulsion)
    p = new("amd")  # Antimass Deflectors: a shield bubble shrugging off rocks
    p.circle(0.42, 0.33, 0.2, 60, -70, 250)
    p.ship(0.42, 0.33, 0.1)
    for x, y, r in ((0.78, 0.15, 0.04), (0.84, 0.36, 0.05), (0.72, 0.5, 0.03)):
        p.poly(x, y, r, 6, 17)
        p.arrow(x - 0.05, y, 0.66, 0.33 + (y - 0.33) * 0.5, 0.025)
    p = new("gd")  # Gravity Drive: engine with gravity rings
    p.ship(0.3, 0.31, 0.16)
    for i, r in enumerate((0.08, 0.13, 0.18)):
        p.circle(0.62 + i * 0.07, 0.31, r * 0.5, ry=r, a0=-90, a1=90, n=20)
    p.line((0.12, 0.31), (0.02, 0.31))
    p = new("fl")  # Fleet Logistics: two arrows, ships in sequence
    for i in range(3):
        p.ship(0.22 + i * 0.25, 0.22, 0.07)
        p.ship(0.22 + i * 0.25, 0.44, 0.07)
    p.arrow(0.1, 0.33, 0.9, 0.33)
    p = new("lwd")  # Light/Wave Deflector: path bending round an obstacle
    p.circle(0.5, 0.31, 0.12, 40)
    p.circle(0.5, 0.31, 0.06, 20)
    p.line(*[(0.08 + 0.84 * t, 0.31 - 0.2 * math.sin(math.pi * t)) for t in np.linspace(0, 1, 30)])
    p.arrow(0.84, 0.17, 0.92, 0.31)
    p = new("det")  # Dark Energy Tap: siphon from a frontier vortex
    p.spiral(0.3, 0.31, 0.2, 2.5)
    p.line((0.5, 0.31), (0.66, 0.31))
    p.rect(0.66, 0.22, 0.2, 0.18)
    p.line((0.7, 0.26), (0.82, 0.26), (0.82, 0.36), (0.7, 0.36))
    p = new("sr")  # Sling Relay: a ring gate flinging a ship
    p.circle(0.3, 0.31, 0.18, 60)
    p.circle(0.3, 0.31, 0.12, 40)
    for a in range(0, 360, 45):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.line((0.3 + 0.12 * ca, 0.31 + 0.12 * sa), (0.3 + 0.18 * ca, 0.31 + 0.18 * sa))
    p.ship(0.72, 0.31, 0.08)
    p.arrow(0.5, 0.31, 0.6, 0.31)
    # ---- green (biotic)
    p = new("nm")  # Neural Motivator: a brain with sparks
    p.circle(0.5, 0.3, 0.2, 50, ry=0.15)
    p.line(*[(0.5 + 0.02 * math.sin(i), 0.15 + 0.3 * i / 12) for i in range(13)])
    for x in (0.38, 0.62):
        p.circle(x, 0.28, 0.07, 20, 180, 360)
    for a in (-40, 0, 40, 200, 160):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.line((0.5 + 0.22 * ca, 0.3 + 0.17 * sa), (0.5 + 0.32 * ca, 0.3 + 0.25 * sa))
    p = new("dxa")  # Dacxive Animators: a figure rising from a cell
    p.circle(0.5, 0.42, 0.14, 40, ry=0.07)
    p.line((0.5, 0.42), (0.5, 0.2))
    p.circle(0.5, 0.15, 0.05, 20)
    p.line((0.42, 0.28), (0.5, 0.24), (0.58, 0.28))
    p.dna(0.15, 0.32, 0.31, 0.06, 1.5)
    p.dna(0.68, 0.85, 0.31, 0.06, 1.5)
    p = new("hm")  # Hyper Metabolism: a heart pulse
    p.line((0.05, 0.33), (0.3, 0.33), (0.36, 0.18), (0.44, 0.5), (0.52, 0.1), (0.6, 0.42), (0.66, 0.33), (0.95, 0.33))
    p.circle(0.84, 0.18, 0.05, 20)
    p.line((0.84, 0.13), (0.84, 0.23))
    p.line((0.79, 0.18), (0.89, 0.18))
    p = new("x89_base")  # X-89 Bacterial Weapon: a spiky microbe over a planet
    p.circle(0.5, 0.3, 0.1, 30)
    for a in range(0, 360, 30):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.line((0.5 + 0.1 * ca, 0.3 + 0.1 * sa), (0.5 + 0.16 * ca, 0.3 + 0.16 * sa))
        p.circle(0.5 + 0.17 * ca, 0.3 + 0.17 * sa, 0.012, 6)
    p.circle(0.5, 0.62, 0.42, 60, 180, 360, ry=0.13)
    p = new("pa")  # Psychoarchaeology: a relic dig with a mind's eye
    p.loop((0.2, 0.5), (0.5, 0.18), (0.8, 0.5))
    p.line((0.32, 0.38), (0.68, 0.38))
    p.circle(0.5, 0.31, 0.06, 20, ry=0.03)
    p.circle(0.5, 0.31, 0.02, 10)
    p.line((0.1, 0.5), (0.9, 0.5))
    p = new("bs")  # Bio-Stims: a syringe and cells
    p.rect(0.2, 0.27, 0.4, 0.08)
    p.line((0.6, 0.31), (0.75, 0.31))
    p.line((0.2, 0.31), (0.12, 0.31))
    p.line((0.12, 0.25), (0.12, 0.37))
    for x in (0.28, 0.36, 0.44, 0.52):
        p.line((x, 0.27), (x, 0.3))
    for x, y in ((0.82, 0.2), (0.86, 0.42), (0.7, 0.48)):
        p.circle(x, y, 0.035, 12)
    # ---- red (warfare)
    p = new("ps")  # Plasma Scoring: a plasma bolt
    p.circle(0.25, 0.31, 0.08, 30)
    p.circle(0.25, 0.31, 0.13, 30)
    p.line((0.38, 0.31), (0.92, 0.31))
    p.wave(0.38, 0.92, 0.31, 0.04, 4)
    p = new("md_base")  # Magen Defense Grid: a hexagon shield over a planet
    for (x, y) in ((0.5, 0.22), (0.41, 0.27), (0.59, 0.27), (0.5, 0.32), (0.41, 0.37), (0.59, 0.37)):
        p.hexagon(x, y, 0.05)
    p.circle(0.5, 0.62, 0.45, 60, 180, 360, ry=0.17)
    p = new("da")  # Duranium Armor: layered plating
    for i in range(4):
        p.loop((0.2 + i * 0.03, 0.12 + i * 0.07), (0.8 - i * 0.03, 0.12 + i * 0.07), (0.75 - i * 0.03, 0.18 + i * 0.07), (0.25 + i * 0.03, 0.18 + i * 0.07))
    p.line((0.2, 0.5), (0.8, 0.5))
    p = new("asc")  # Assault Cannon: a big barrel firing
    p.rect(0.1, 0.25, 0.45, 0.12)
    p.rect(0.55, 0.28, 0.15, 0.06)
    for i in range(3):
        p.line((0.74 + i * 0.05, 0.31 - 0.04 * (i - 1)), (0.8 + i * 0.05, 0.31 - 0.08 * (i - 1)))
    p.circle(0.2, 0.42, 0.06, 20)
    p.circle(0.4, 0.42, 0.06, 20)
    p = new("aida")  # AI Development Algorithm: a neural net
    nodes = [(0.15, 0.15), (0.15, 0.31), (0.15, 0.47), (0.5, 0.2), (0.5, 0.42), (0.85, 0.31)]
    for a in nodes[:3]:
        for b in nodes[3:5]:
            p.line(a, b)
    for b in nodes[3:5]:
        p.line(b, nodes[5])
    for x, y in nodes:
        p.circle(x, y, 0.035, 12)
    p = new("sar")  # Self-Assembly Routines: a robot arm building
    p.line((0.15, 0.5), (0.3, 0.3), (0.5, 0.2), (0.6, 0.3))
    p.circle(0.3, 0.3, 0.025, 10)
    p.circle(0.5, 0.2, 0.025, 10)
    p.line((0.6, 0.3), (0.64, 0.36))
    p.line((0.6, 0.3), (0.56, 0.36))
    p.hexagon(0.75, 0.42, 0.07)
    p.hexagon(0.75, 0.42, 0.04)
    # ---- yellow (cybernetic)
    p = new("st")  # Sarween Tools: a wrench and gear
    p.gear(0.35, 0.31, 0.15)
    p.line((0.5, 0.31), (0.85, 0.31))
    p.circle(0.88, 0.31, 0.05, 20, 30, 330)
    p = new("gls")  # Graviton Laser System: targeting reticle
    p.circle(0.5, 0.31, 0.18, 50)
    p.circle(0.5, 0.31, 0.08, 30)
    for a in (0, 90, 180, 270):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.line((0.5 + 0.12 * ca, 0.31 + 0.12 * sa), (0.5 + 0.24 * ca, 0.31 + 0.24 * sa))
    p.line((0.05, 0.05), (0.42, 0.26))
    p = new("td")  # Transit Diodes: pulses between planets
    p.circle(0.18, 0.31, 0.1, 30)
    p.circle(0.82, 0.31, 0.1, 30)
    for k in (-0.05, 0.0, 0.05):
        p.line((0.28, 0.31 + k), (0.72, 0.31 + k))
    for x in (0.4, 0.5, 0.6):
        p.loop((x - 0.02, 0.27), (x + 0.02, 0.31), (x - 0.02, 0.35))
    p = new("ie")  # Integrated Economy: a planet feeding a factory
    p.planet(0.3, 0.31, 0.1)
    p.rect(0.62, 0.25, 0.22, 0.17)
    p.line((0.66, 0.25), (0.66, 0.14), (0.7, 0.14), (0.7, 0.25))
    p.arrow(0.45, 0.33, 0.6, 0.33)
    p = new("sdn")  # Scanlink Drone Network: a drone with scan cone
    p.circle(0.3, 0.2, 0.06, 20)
    p.line((0.2, 0.2), (0.4, 0.2))
    p.line((0.3, 0.26), (0.18, 0.52))
    p.line((0.3, 0.26), (0.42, 0.52))
    p.circle(0.3, 0.52, 0.12, 30, 0, 180, ry=0.03)
    for x, y in ((0.65, 0.2), (0.8, 0.35), (0.65, 0.45)):
        p.circle(x, y, 0.03, 10)
    p.line((0.65, 0.2), (0.8, 0.35), (0.65, 0.45))
    p = new("pi")  # Predictive Intelligence: an eye of circuits
    p.circle(0.5, 0.31, 0.3, 50, 200, 340)
    p.circle(0.5, 0.31, 0.3, 50, 20, 160)
    p.circle(0.5, 0.31, 0.1, 30)
    p.circle(0.5, 0.31, 0.04, 14)
    for x in (0.1, 0.9):
        p.line((x, 0.31), (0.5 + (x - 0.5) * 0.65, 0.31))
    # ---- PoK generic / faction (non-unit)
    p = new("l4")  # L4 Disruptors (Letnev): a jammer dish
    p.circle(0.4, 0.3, 0.16, 40, 120, 300)
    p.line((0.4, 0.3), (0.4, 0.52))
    for r in (0.07, 0.12, 0.17):
        p.circle(0.55, 0.3, r, 20, -45, 45)
    p = new("cm")  # Chaos Mapping (Saar): broken grid
    for x in (0.2, 0.4, 0.6, 0.8):
        p.line((x, 0.1), (x + 0.03, 0.25), (x - 0.02, 0.4), (x + 0.02, 0.52))
    for y in (0.15, 0.31, 0.47):
        p.line((0.1, y), (0.4, y + 0.02), (0.6, y - 0.03), (0.9, y))
    p = new("qdn")  # Quantum Datahub Node (Hacan): crossed data rings
    p.circle(0.5, 0.31, 0.22, 50, ry=0.08)
    p.circle(0.5, 0.31, 0.08, 30, ry=0.22)
    p.circle(0.5, 0.31, 0.05, 20)
    p = new("as")  # Aetherstream (Empyrean): currents
    for i in range(4):
        p.wave(0.05, 0.95, 0.15 + i * 0.11, 0.03, 2 + i * 0.25)
    p = new("mc")  # Mirror Computing (Mentak): two facing prisms
    p.loop((0.2, 0.5), (0.35, 0.12), (0.45, 0.5))
    p.loop((0.55, 0.5), (0.65, 0.12), (0.8, 0.5))
    p.line((0.4, 0.3), (0.6, 0.3))
    p = new("pfa")  # Pre-Fab Arcologies (Naaz-Rokha): stacked towers
    for i, h in enumerate((0.3, 0.42, 0.24, 0.36)):
        p.rect(0.2 + i * 0.15, 0.52 - h, 0.12, h)
    p.line((0.1, 0.52), (0.9, 0.52))
    p = new("vax")  # Valefar Assimilator X (Nekro): absorbing glyph
    p.circle(0.5, 0.31, 0.2, 40)
    p.line((0.36, 0.17), (0.64, 0.45))
    p.line((0.64, 0.17), (0.36, 0.45))
    p = new("vay")
    p.circle(0.5, 0.31, 0.2, 40)
    p.line((0.36, 0.17), (0.5, 0.31), (0.64, 0.17))
    p.line((0.5, 0.31), (0.5, 0.5))
    p = new("scc")  # Spatial Conduit Cylinder (Jol-Nar): a tube between two systems
    p.hexagon(0.18, 0.31, 0.12)
    p.hexagon(0.82, 0.31, 0.12)
    p.circle(0.3, 0.31, 0.03, 10, ry=0.08)
    p.circle(0.7, 0.31, 0.03, 10, ry=0.08)
    p.line((0.3, 0.23), (0.7, 0.23))
    p.line((0.3, 0.39), (0.7, 0.39))
    p = new("lgf")  # Lazax Gate Folding (Winnu): a folded gate
    p.loop((0.3, 0.1), (0.7, 0.1), (0.7, 0.52), (0.3, 0.52))
    p.line((0.3, 0.1), (0.5, 0.31), (0.7, 0.1))
    p.line((0.3, 0.52), (0.5, 0.31), (0.7, 0.52))
    p = new("it")  # Instinct Training (Xxcha): a reflex shield on a claw
    p.circle(0.5, 0.31, 0.2, 40, 180, 360)
    p.line((0.3, 0.31), (0.7, 0.31))
    p.line((0.4, 0.45), (0.5, 0.33), (0.6, 0.45))
    p = new("mi")  # Mageon Implants (Yssaril): a hooded eye
    p.circle(0.5, 0.31, 0.25, 40, 200, 340)
    p.circle(0.5, 0.18, 0.25, 40, 20, 160)
    p.circle(0.5, 0.31, 0.05, 14)
    p = new("bio")  # Bioplasmosis (Arborec): roots between planets
    p.circle(0.2, 0.25, 0.08, 20)
    p.circle(0.8, 0.25, 0.08, 20)
    p.line((0.2, 0.33), (0.3, 0.45), (0.5, 0.5), (0.7, 0.45), (0.8, 0.33))
    p.line((0.4, 0.48), (0.42, 0.56))
    p.line((0.6, 0.48), (0.58, 0.56))
    p = new("ah")  # Aerie Hololattice (Argent): a lattice
    for i in range(5):  # a diamond lattice with nodes
        p.line((0.1 + i * 0.2, 0.1), (0.1 + i * 0.2 - 0.2, 0.52))
        p.line((0.1 + i * 0.2 - 0.2, 0.1), (0.1 + i * 0.2, 0.52))
    p.line((0.05, 0.1), (0.95, 0.1))
    p.line((0.05, 0.52), (0.95, 0.52))
    p.circle(0.5, 0.31, 0.04, 14)
    p = new("nes")  # Non-Euclidean Shielding (Letnev): bent shield
    p.circle(0.5, 0.6, 0.4, 40, 200, 340)
    p.circle(0.5, 0.6, 0.3, 40, 210, 330)
    p.ship(0.5, 0.45, 0.07, -90)
    p = new("mr")  # Magmus Reactor (Muaat): a reactor in a star
    p.circle(0.5, 0.31, 0.2, 40)
    for a in range(0, 360, 40):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.line((0.5 + 0.2 * ca, 0.31 + 0.2 * sa), (0.5 + 0.28 * ca, 0.31 + 0.28 * sa))
    p.rect(0.44, 0.25, 0.12, 0.12)
    p = new("pm")  # Production Biomes (Hacan): a greenhouse dome
    p.circle(0.5, 0.48, 0.3, 40, 180, 360)
    p.line((0.2, 0.48), (0.8, 0.48))
    for x in (0.35, 0.5, 0.65):
        p.line((x, 0.48), (x, 0.32))
        p.circle(x, 0.29, 0.03, 10)
    p = new("vw")  # Voidwatch (Empyrean): an eye in a nebula
    p.circle(0.5, 0.31, 0.3, 40, 200, 340)
    p.circle(0.5, 0.31, 0.3, 40, 20, 160)
    p.circle(0.5, 0.31, 0.08, 20)
    p.wave(0.05, 0.95, 0.52, 0.02, 3)
    p = new("wg")  # Wormhole Generator (Creuss): a swirl being made
    p.spiral(0.5, 0.31, 0.22, 3)
    p.line((0.15, 0.55), (0.3, 0.45))
    p.line((0.85, 0.55), (0.7, 0.45))
    p = new("ds")  # Dimensional Splicer (Creuss): cut through space
    p.circle(0.3, 0.31, 0.16, 40)
    p.circle(0.7, 0.31, 0.16, 40)
    p.line((0.5, 0.08), (0.5, 0.55))
    p = new("is")  # Inheritance Systems (L1Z1X): a chip passing a crown
    p.rect(0.3, 0.2, 0.4, 0.25)
    for x in (0.35, 0.45, 0.55, 0.65):
        p.line((x, 0.2), (x, 0.14))
        p.line((x, 0.45), (x, 0.51))
    p.line((0.4, 0.32), (0.45, 0.27), (0.5, 0.32), (0.55, 0.27), (0.6, 0.32))
    p = new("gr")  # Genetic Recombination (Mahact): a double helix
    p.dna(0.1, 0.9, 0.31, 0.12, 2.5)
    p = new("so")  # Salvage Operations (Mentak): grapple on a wreck
    p.loop((0.55, 0.5), (0.65, 0.3), (0.85, 0.35), (0.9, 0.5))
    p.line((0.1, 0.1), (0.6, 0.38))
    p.line((0.6, 0.38), (0.58, 0.44))
    p.line((0.6, 0.38), (0.66, 0.4))
    p = new("ng")  # Neuroglaive (Naalu): a mind wave
    p.circle(0.25, 0.31, 0.1, 30)
    for r in (0.15, 0.22, 0.29):
        p.circle(0.25, 0.31, r, 20, -40, 40)
    p = new("sc")  # Supercharge (Naaz-Rokha): a lightning bolt
    p.loop((0.55, 0.05), (0.35, 0.33), (0.5, 0.33), (0.42, 0.58), (0.68, 0.25), (0.53, 0.25))
    p = new("tcs")  # Temporal Command Suite (Nomad): an hourglass
    p.loop((0.38, 0.1), (0.62, 0.1), (0.52, 0.31), (0.62, 0.52), (0.38, 0.52), (0.48, 0.31))
    p.line((0.34, 0.1), (0.66, 0.1))
    p.line((0.34, 0.52), (0.66, 0.52))
    p = new("vpw")  # Valkyrie Particle Weave (Titans): a woven mesh
    for i in range(6):
        p.wave(0.1, 0.9, 0.12 + i * 0.075, 0.025, 3, 30)
    p = new("ers")  # E-Res Siphons (Jol-Nar): a siphon ring
    p.circle(0.5, 0.31, 0.18, 40)
    for a in range(0, 360, 60):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.arrow(0.5 + 0.32 * ca, 0.31 + 0.32 * sa, 0.5 + 0.2 * ca, 0.31 + 0.2 * sa, 0.03)
    p = new("vtx")  # Vortex (Vuil'raith): a maw
    p.spiral(0.5, 0.31, 0.25, 2)
    p.circle(0.5, 0.31, 0.25, 40)
    p = new("htp")  # Hegemonic Trade Policy (Winnu): scales
    p.line((0.5, 0.1), (0.5, 0.5))
    p.line((0.25, 0.18), (0.75, 0.18))
    for x in (0.25, 0.75):
        p.line((x, 0.18), (x - 0.08, 0.36))
        p.line((x, 0.18), (x + 0.08, 0.36))
        p.circle(x, 0.36, 0.08, 20, 0, 180, ry=0.03)
    p.line((0.4, 0.5), (0.6, 0.5))
    p = new("nf")  # Nullification Field (Xxcha): a stop field
    p.hexagon(0.5, 0.31, 0.22)
    p.hexagon(0.5, 0.31, 0.15)
    p.line((0.4, 0.21), (0.6, 0.41))
    p = new("yso")  # Yin Spinner Omega: spinning infantry
    p.circle(0.5, 0.31, 0.2, 40, 0, 300)
    p.arrow(0.6, 0.14, 0.7, 0.21)
    p.circle(0.5, 0.25, 0.04, 14)
    p.line((0.5, 0.29), (0.5, 0.4))
    p = new("ic")  # Impulse Core (Yin): a ram
    p.ship(0.4, 0.31, 0.16)
    for i in range(3):
        p.line((0.66 + i * 0.06, 0.2), (0.7 + i * 0.06, 0.31), (0.66 + i * 0.06, 0.42))
    p = new("tp")  # Transparasteel Plating (Yssaril): layered glass
    for i in range(3):
        p.loop((0.2 + i * 0.08, 0.15 + i * 0.05), (0.7 + i * 0.08, 0.15 + i * 0.05), (0.6 + i * 0.08, 0.45 + i * 0.05), (0.1 + i * 0.08, 0.45 + i * 0.05))
    p = new("iihq")  # I.I.H.Q. Modernization (Keleres)
    p.planet(0.5, 0.31, 0.12)
    p.rect(0.44, 0.08, 0.12, 0.08)
    p = new("asn")  # Agency Supply Network (Keleres)
    for x, y in ((0.2, 0.2), (0.5, 0.45), (0.8, 0.2)):
        p.rect(x - 0.06, y - 0.05, 0.12, 0.1)
    p.line((0.26, 0.22), (0.44, 0.43))
    p.line((0.74, 0.22), (0.56, 0.43))
    p = new("executiveorder")  # Executive Order (Keleres, Thunder's Edge): a decree, its seal and a gavel
    p.rect(0.22, 0.08, 0.32, 0.46)
    for i in range(4):
        p.line((0.27, 0.16 + i * 0.07), (0.49, 0.16 + i * 0.07))
    p.circle(0.46, 0.46, 0.06, 18)
    p.loop((0.62, 0.14), (0.78, 0.04), (0.84, 0.13), (0.68, 0.23))
    p.line((0.73, 0.18), (0.86, 0.4))
    return {k: v.polys for k, v in d.items()}


# ------------------------------------------------------------------ output
UNITS = ["carrier", "cruiser", "destroyer", "dreadnought", "fighter", "flagship", "warsun", "infantry", "mech", "pds", "spacedock"]


def main():
    arts = []  # (key, aspect h/w, polylines normalised to width 1)
    for u in UNITS:
        polys, w, h = trace(os.path.join(SRC, u + ".png"))
        arts.append((u, h / w, [[(x / w, y / w) for x, y in pl] for pl in polys]))
        print(u, len(polys), "lines", sum(len(p) for p in polys), "points", file=sys.stderr)
    for k, polys in techs().items():
        arts.append(("t:" + k, TECH_H, polys))
    # C++ data: points scaled to 0..4000 of the width
    pts, polys, table = [], [], []
    for key, aspect, pl in arts:
        first = len(polys)
        for line in pl:
            if len(line) < 2:
                continue
            polys.append((len(pts) // 2, len(line)))
            for x, y in line:
                pts += [int(round(x * 4000)), int(round(y * 4000))]
        table.append((key, aspect, first, len(polys) - first))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("// Generated by tools/gen_wireframes.py - do not edit.\n")
        f.write("// Unit outlines traced from the owner's templates (docs/owner/wireframes_src), my own tech drawings.\n")
        f.write(f"static const int16_t kWirePts[{len(pts)}] = {{\n")
        for i in range(0, len(pts), 24):
            f.write("  " + ",".join(str(v) for v in pts[i:i + 24]) + ",\n")
        f.write("};\n")
        f.write(f"static const WirePoly kWirePolys[{len(polys)}] = {{\n")
        for i in range(0, len(polys), 8):
            f.write("  " + ",".join(f"{{{a},{b}}}" for a, b in polys[i:i + 8]) + ",\n")
        f.write("};\n")
        f.write(f"static const WireArt kWireArts[{len(table)}] = {{\n")
        for key, aspect, first, n in table:
            f.write(f'  {{"{key}", {aspect:.4f}f, {first}, {n}}},\n')
        f.write("};\n")
    print("points", len(pts) // 2, "polylines", len(polys), "arts", len(table), file=sys.stderr)
    if len(sys.argv) > 1:  # preview sheet
        cols, cw, ch = 6, 300, 200
        rows = (len(arts) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cw, rows * ch), (10, 12, 20))
        d = ImageDraw.Draw(sheet)
        for i, (key, aspect, pl) in enumerate(arts):
            ox, oy = (i % cols) * cw, (i // cols) * ch
            sc = min((cw - 20), (ch - 30) / aspect)
            x0 = ox + (cw - sc) / 2
            y0 = oy + 8 + (ch - 30 - sc * aspect) / 2
            for line in pl:
                d.line([(x0 + x * sc, y0 + y * sc) for x, y in line], fill=(150, 200, 230), width=1)
            d.text((ox + 6, oy + ch - 18), key, fill=(230, 200, 120))
        sheet.save(sys.argv[1])


if __name__ == "__main__":
    main()
