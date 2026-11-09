import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import math  # noqa: E402

import numpy as np  # noqa: E402
from pix import Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, save_gif, FRAMES  # noqa: E402

D = __import__('os').path.dirname(__import__('os').path.abspath(__file__))


# ---------------------------------------------------------------- helpers
def M(pts):
    """mirror points about the vertical centre line (pixel x -> 63 - x)"""
    return [(63 - x, y) for x, y in pts]


def rgb(c):
    return hexrgb(c) if isinstance(c, str) else tuple(c)


def path_px(pts):
    """the pixels along a polyline, in order (no repeats)"""
    out = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            q = (int(round(x0 + (x1 - x0) * i / n)), int(round(y0 + (y1 - y0) * i / n)))
            if not out or out[-1] != q:
                out.append(q)
    return out


def travel(p, pts, base, hot, t, phase=0.0, cycles=1, length=0.22):
    """a polyline in `base` with a bright pulse running along it (veins of light)"""
    px = path_px(pts)
    n = len(px)
    head = (t * cycles + phase) % 1.0
    for i, (x, y) in enumerate(px):
        u = i / max(n - 1, 1)
        d = (head - u) % 1.0
        if d < length:
            p.px(x, y, mix(hot, base, d / length))
        else:
            p.px(x, y, base)


def tint(p, mask, col, amt):
    c = np.array(rgb(col), float)
    m = mask & p.a
    p.rgb[m] = (p.rgb[m] * (1 - amt) + c * amt).astype(np.uint8)


def dots(p, mask, col, step=3, off=0, stagger=True):
    """a staggered grid of single pixels inside a mask (scales, studs, compound-eye facets)"""
    ys, xs = np.nonzero(mask)
    c = rgb(col)
    for y, x in zip(ys, xs):
        sx = (step // 2) if (stagger and (y // step) % 2) else 0
        if (y + off) % step == 0 and (x + sx + off) % step == 0:
            p.rgb[y, x] = c


def glitch(p, y0, y1, dx):
    """shift a band of rows sideways (a corrupted scanline frame)"""
    y0, y1 = max(0, y0), min(p.h, y1)
    p.rgb[y0:y1] = np.roll(p.rgb[y0:y1], dx, axis=1)
    p.a[y0:y1] = np.roll(p.a[y0:y1], dx, axis=1)


def scutes(p, mask, seeds, seam, ring=None, centre=None, margin=0, mcentre=(32, 32), mstep=18):
    """tortoise-shell plates: a Voronoi of seed points inside mask (plus an optional ring of marginal plates),
    dark seams between plates, a growth ring inside each, a lighter centre"""
    from scipy import ndimage
    ys, xs = np.nonzero(mask)
    S = np.array([(x + p.dx, y + p.dy) for x, y in seeds], float)
    d = (xs[:, None] - S[None, :, 0]) ** 2 + (ys[:, None] - S[None, :, 1]) ** 2
    lab = np.full(mask.shape, -1)
    lab[ys, xs] = np.argmin(d, axis=1)
    if margin:
        edt = ndimage.distance_transform_edt(np.pad(mask, 1))[1:-1, 1:-1]
        marg = mask & (edt <= margin)
        cx, cy = mcentre[0] + p.dx, mcentre[1] + p.dy
        ang = np.degrees(np.arctan2(p.yy - cy, p.xx - cx)) % 360
        lab[marg] = 1000 + (ang[marg] // mstep).astype(int)
    sm = np.zeros_like(mask)
    sm[:, :-1] |= (lab[:, :-1] != lab[:, 1:]) & mask[:, 1:] & mask[:, :-1]
    sm[:-1, :] |= (lab[:-1, :] != lab[1:, :]) & mask[1:, :] & mask[:-1, :]
    inside = mask & ~sm
    dd = ndimage.distance_transform_edt(np.pad(inside, 1))[1:-1, 1:-1]
    if ring is not None:
        rm = inside & (dd >= 2) & (dd < 3) & (lab < 1000)
        p.rgb[rm] = rgb(ring)
    if centre is not None:
        cm = inside & (dd >= 4) & (lab < 1000)
        tint(p, cm, centre, 0.35)
    p.rgb[sm] = rgb(seam)


# ---------------------------------------------------------------- 1. Naalu
def _hood_outline(t, tips, cx=31.5, cy=28, dip=0.08, phase=0.0, neck=(24, 50)):
    """a scalloped fan: rib tips (left half, bottom to top) mirrored to the right; each tip ripples outward in turn"""
    def side(sign):
        out, ribs = [], []
        for i, (x, y) in enumerate(tips):
            xx = x if sign < 0 else 63 - x
            ux, uy = xx - cx, y - cy
            n = math.hypot(ux, uy) or 1
            s = sway(t, phase - 0.11 * i + (0.5 if sign > 0 else 0), 1) if i < len(tips) - 1 else 0
            tp = (xx + ux / n * s, y + uy / n * s)
            if out:
                a = out[-1]
                mx_, my_ = (a[0] + tp[0]) / 2, (a[1] + tp[1]) / 2
                out.append((mx_ + (cx - mx_) * dip, my_ + (cy - my_) * dip))
            out.append(tp)
            ribs.append(tp)
        return out, ribs
    lo, lr = side(-1)
    ro, rr = side(1)
    return [neck] + lo + ro[::-1] + [(63 - neck[0], neck[1])], lr, rr


def naalu(t=0.0):
    """a serpent matriarch: a ribbed cobra hood fanned behind her head, jade scales, gold circlet and neck rings.
    Moves: breathing, the hood's rib tips ripple, the tongue flicks twice, the psychic gem and the eyes shimmer."""
    p = Portrait()
    scl = ramp('#56c28a', spread=0.7)
    hood = ramp('#1a6a80', spread=0.8)
    memb = ramp('#10324e', spread=0.7)
    band = ramp('#2a86a6', spread=0.6)
    gold = ramp('#e2b236', spread=0.62, hue_shift=0.04)
    robe = ramp('#3e2878', spread=0.8)
    psy = ramp('#b450ff', spread=0.5)
    pale = ramp('#b8e6b4', spread=0.5, hue_shift=0.04)
    body, head = breath(t), breath(t, -0.1)
    shim = pulse(t, 0.0, 2)
    with p.at(0, head):
        # the hood: ribs radiating to scalloped tips that ripple outward in turn
        tips = [(13, 44), (6, 36), (3, 27), (4, 18), (8, 10), (15, 4), (23, 0), (32, -2)]
        outl, lr, rr = _hood_outline(t, tips, neck=(25, 52))
        p.fill(p.poly(outl), hood, radius=4, texture=0.05, seed=31)
        inner = p.poly([(26, 50), (16, 43), (9, 35), (7, 26), (9, 17), (13, 10), (20, 5), (27, 3), (32, 2), (37, 3), (44, 5),
                        (51, 10), (55, 17), (57, 26), (55, 35), (48, 43), (38, 50)])
        p.fill(inner, memb, radius=6, bias=0.05, texture=0.05, seed=34)
        # scales over the membrane, a pale band of the cobra's markings, ribs to the scalloped tips
        dots(p, inner, hood[2], step=3)
        ring = p.ellipse(32, 25, 20, 22) & ~p.ellipse(32, 25, 17.5, 19.5) & inner
        p.fill(ring, band, radius=2, edge=False, texture=0.1, seed=35)
        dots(p, ring, gold[3], step=4)
        for ribs, cxx, cols in ((lr, 32, (hood[3], hood[2])), (rr, 31, (hood[2], hood[1]))):
            for i, (x, y) in enumerate(ribs[:-1]):
                p.stroke([(cxx + (x - cxx) * 0.66, 28 + (y - 28) * 0.66), (x, y)], cols[i % 2])
                p.px(int(round(x)), int(round(y)), gold[4] if cxx == 32 else gold[3])
        # shadow close around the head
        p.fill(p.ellipse(32, 25, 13, 16.5) & inner, memb, flat=0, edge=False)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (1, 56), (12, 50), (24, 49), (32, 52), (39, 49), (51, 50), (62, 56), (63, 66)]), robe,
               radius=8, texture=0.05)
        # gold scale pauldrons
        pl = [(1, 58), (10, 51), (21, 51), (17, 58), (6, 64)]
        p.fill(p.poly(pl), gold, radius=3, texture=0.05)
        p.fill(p.poly(M(pl)), gold, radius=3, bias=-0.12, texture=0.05)
        for x, y in [(6, 58), (10, 55), (14, 54), (10, 60), (14, 57), (18, 53)]:
            p.pixels([(x, y), (x + 1, y)], gold[1])
            p.pixels([(63 - x, y), (62 - x, y)], gold[0])
        # the long neck with stacked gold rings
        p.fill(p.poly([(27, 34), (36, 34), (37, 55), (26, 55)]), scl, radius=4, bias=-0.15, texture=0.06)
        p.fill(p.poly([(30, 36), (33, 36), (33, 55), (30, 55)]), pale, radius=2, bias=-0.1, edge=False)
        for y in (42, 45):
            p.stroke([(26, y), (37, y)], gold[3])
            p.stroke([(26, y + 1), (37, y + 1)], gold[1])
            p.pixels([(26, y + 1), (37, y + 1)], gold[0])
        # a broad collar of gold and jade beads
        p.fill(p.ellipse(32, 48, 15, 10) & ~p.ellipse(32, 46, 7, 5) & p.poly([(0, 47), (63, 47), (63, 66), (0, 66)]), gold,
               radius=3, texture=0.05)
        for rr_, col in ((9.5, '#2fd0b0'), (12.5, '#b050ff')):
            for a in range(-70, 71, 14):
                x = 32 + rr_ * 1.25 * math.sin(math.radians(a))
                y = 47 + rr_ * 0.75 * math.cos(math.radians(a))
                p.px(int(round(x)), int(round(y)), col)
        p.fill(p.ellipse(32, 58.5, 2.5, 2.5), gold, radius=2)
        p.glow(32, 58, '#ffffff', mix('#b060ff', '#ff80ff', shim), r=0)
    with p.at(0, head):
        # the face: a graceful oval with a narrow chin
        face = p.ellipse(32, 25, 10.5, 14) & ~p.poly([(20, 30), (29, 41), (20, 41)]) & ~p.poly([(44, 30), (35, 41), (44, 41)])
        p.fill(face, scl, radius=7, texture=0.07, seed=33)
        dots(p, face & ~p.ellipse(32, 26, 7, 11.5), scl[1], step=2)
        # cheekbones, brow ridges, sockets
        p.stroke([(23, 28), (26, 31)], scl[4])
        p.stroke([(41, 28), (38, 31)], scl[1])
        p.stroke([(23, 21), (26, 20), (30, 21)], scl[4])
        p.stroke([(34, 21), (38, 20), (41, 21)], scl[3])
        for cx in (26.5, 37.5):
            p.fill(p.ellipse(cx, 24, 3.8, 2.0), scl, flat=0, edge=False)
        # almond eyes: gold irises, slit pupils, lids half lowered (calm, knowing)
        iris = mix('#ffb020', '#fff090', shim)
        for x0, o in ((24, -1), (35, 1)):
            p.pixels([(x0, 24), (x0 + 1, 24), (x0 + 2, 24), (x0 + 3, 24), (x0 + 4, 24)], iris)
            p.pixels([(x0 + 1, 25), (x0 + 2, 25), (x0 + 3, 25)], '#e07a10')
            p.pixels([(x0 + 2, 24), (x0 + 2, 25)], '#140810')
            p.stroke([(x0, 23), (x0 + 4, 23)], '#0a2a26')
            p.px(x0 + (-1 if o < 0 else 5), 23, '#0a2a26')
            p.px(x0 + (-2 if o < 0 else 6), 22, scl[0])
        sp = int(t * FRAMES) % 12
        if sp < 3:
            p.px(25 + sp // 2, 24, '#ffffff')
            p.px(36 + sp // 2, 24, '#ffffff')
        # nose slits, a faint knowing smile
        p.pixels([(31, 30), (33, 30)], scl[0])
        p.stroke([(29, 34), (30, 35), (34, 35), (35, 34)], '#3a1040')
        p.stroke([(30, 36), (34, 36)], '#8a3a7a')
        p.pixels([(28, 33), (36, 33)], scl[1])
        p.stroke([(31, 37), (33, 37)], scl[3])
        # the tongue flicks twice
        if blink(t, 0.30, 2) or blink(t, 0.40, 2):
            ln = 3 if blink(t, 0.30, 1) or blink(t, 0.40, 1) else 2
            p.stroke([(32, 36), (32, 36 + ln)], '#ff3a6a')
            p.pixels([(31, 37 + ln), (33, 37 + ln)], '#ff6a8a')
        # gold circlet and the psychic gem
        p.fill(p.poly([(21, 18), (32, 14), (43, 18), (43, 20), (32, 16), (21, 20)]), gold, radius=2)
        p.fill(p.poly([(32, 10), (35, 15), (32, 20), (29, 15)]), gold, radius=2)
        p.fill(p.ellipse(32, 15.5, 1.7, 2.4), psy, radius=1)
        p.glow(32, 15, '#ffffff', mix('#a050ff', '#ff70ff', shim), r=1, strength=0.5 + 0.5 * shim)
        # gold drops hanging at the temples
        for x in (21, 42):
            p.stroke([(x, 20), (x, 27)], gold[3] if x < 32 else gold[2])
            p.fill(p.ellipse(x + 0.5, 29, 1.3, 1.9), gold, radius=1)
    p.rim('#ff4fd2', side=1, amount=0.5)
    p.rim('#ffd060', side=-1, amount=0.25)
    p.outline('#081414')
    # psychic motes rising
    for x0, ph in ((3, 0.0), (60, 0.33), (6, 0.66), (57, 0.15)):
        u = (t + ph) % 1.0
        if u < 0.8:
            p.px(x0 + sway(t, ph, 1, 2), int(round(52 - u * 14)), mix('#ffb0ff', '#401060', u))
    p.backdrop = (22, 10, 40)
    return p


# ---------------------------------------------------------------- 2. Nekro
def nekro(t=0.0):
    """a machine skull of black steel: a visor-eye that scans, a furnace-red maw, cables, circuitry carrying virus light.
    Moves: the red eye sweeps the visor, light pulses along the circuit veins and cables, the image glitches."""
    p = Portrait()
    steel = ramp('#8a92a4', spread=0.7)
    blk = ramp('#262834', spread=0.8)
    red = ramp('#ff2238', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    vein, hot = '#5e0c1a', '#ff8090'
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 54), (11, 49), (24, 51), (32, 55), (40, 51), (52, 49), (63, 54), (63, 66)]), blk, radius=8,
               texture=0.05)
        # shoulder plates with red seams
        sp = [(0, 56), (10, 50), (21, 52), (17, 59), (4, 63)]
        p.fill(p.poly(sp), steel, radius=3, texture=0.05)
        p.fill(p.poly(M(sp)), steel, radius=3, bias=-0.15, texture=0.05)
        travel(p, [(2, 59), (10, 54), (18, 55)], vein, hot, t, 0.1)
        travel(p, M([(2, 59), (10, 54), (18, 55)]), vein, hot, t, 0.6)
        for x, y in [(6, 55), (14, 53)]:
            p.px(x, y, steel[4])
            p.px(63 - x, y, steel[2])
        # neck: vertebra segments
        for y in (48, 52, 56):
            p.fill(p.poly([(27, y), (36, y), (35, y + 3), (28, y + 3)]), steel, radius=2)
        p.glow(32, 61, '#ffe0e0', '#ff2238', r=1, strength=0.35 + 0.55 * k)
    with p.at(0, head):
        # cables falling from the skull's base to the shoulders
        for pts, ph in (([(20, 36), (15, 44), (12, 52), (12, 62)], 0.0), ([(43, 36), (48, 44), (51, 52), (51, 62)], 0.5),
                        ([(23, 42), (21, 50), (22, 62)], 0.25), ([(40, 42), (42, 50), (41, 62)], 0.75)):
            p.stroke(pts, blk[0], width=3)
            p.stroke(pts, blk[2])
            travel(p, [(x + 1, y) for x, y in pts], blk[0], red[3], t * 1, ph, 2, 0.15)
        # cranium
        skull = p.ellipse(32, 21, 16.5, 17) | p.poly([(17, 26), (47, 26), (44, 38), (20, 38)])
        p.fill(skull, blk, radius=9, texture=0.05, seed=41)
        # steel crest and temple plates
        p.fill(p.poly([(28, 4), (36, 4), (37, 19), (32, 22), (27, 19)]), steel, radius=3)
        p.stroke([(32, 5), (32, 20)], steel[1])
        p.stroke([(28, 10), (36, 10)], steel[1])
        tp = [(16, 14), (23, 8), (25, 19), (18, 23)]
        p.fill(p.poly(tp), steel, radius=2)
        p.fill(p.poly(M(tp)), steel, radius=2, bias=-0.15)
        p.pixels([(20, 13), (21, 18)], steel[0])
        p.pixels([(43, 13), (42, 18)], steel[0])
        # circuit veins carrying the virus light
        for pts, ph in (([(19, 9), (22, 6), (26, 6)], 0.0), ([(26, 9), (26, 14), (24, 14)], 0.2),
                        ([(44, 9), (41, 6), (37, 6)], 0.5), ([(37, 9), (37, 14), (39, 14)], 0.7),
                        ([(16, 24), (16, 20)], 0.35), ([(47, 24), (47, 20)], 0.85), ([(29, 1 + 4), (29, 3)], 0.6)):
            travel(p, pts, vein, hot, t, ph, 1, 0.3)
            p.px(*pts[-1], red[3])
        # heavy brow plate in an angry V
        p.fill(p.poly([(16, 20), (30, 22), (32, 24), (34, 22), (48, 20), (47, 24), (34, 26), (32, 27), (30, 26), (17, 24)]),
               steel, radius=2, bias=0.1)
        # the visor and the scanning eye
        p.fill(p.poly([(18, 25), (30, 27), (32, 28), (34, 27), (46, 25), (45, 31), (32, 32), (19, 31)]), blk, flat=0, edge=False)
        for x in range(19, 46):
            p.px(x, 28, mix('#4a0812', '#7a1020', k))
        sx = int(round(32 + 11 * wave(t, 0.0, 1)))
        for d in range(-5, 6):
            if 19 <= sx + d <= 45:
                p.px(sx + d, 28, mix(red[4], '#4a0812', min(1, abs(d) / 5)))
                if abs(d) < 3:
                    p.px(sx + d, 29, mix(red[2], '#2a0610', abs(d) / 3))
                    p.px(sx + d, 27, mix(red[2], '#2a0610', abs(d) / 3))
        p.glow(sx, 28, '#ffffff', '#ff3048', r=1, strength=0.9)
        # angular cheek plates with vents
        ck = [(16, 29), (21, 32), (24, 39), (19, 41), (16, 35)]
        p.fill(p.poly(ck), steel, radius=2)
        p.fill(p.poly(M(ck)), steel, radius=2, bias=-0.2)
        for y in (34, 36, 38):
            p.stroke([(18, y), (20, y + 1)], blk[0])
            p.stroke([(45, y), (43, y + 1)], blk[0])
        # nose cavity
        p.fill(p.poly([(30, 33), (34, 33), (32, 36)]), blk, flat=0, edge=False)
        # jaw and the furnace-red maw
        p.fill(p.poly([(20, 37), (44, 37), (42, 46), (37, 51), (27, 51), (22, 46)]), steel, radius=4, texture=0.05)
        p.fill(p.poly([(24, 40), (40, 40), (38, 47), (26, 47)]), ramp('#5a0a18'), flat=0, edge=False)
        p.stroke([(26, 43), (38, 43)], mix(red[1], red[3], k))
        p.stroke([(27, 44), (37, 44)], mix(red[0], red[2], k))
        for x in range(25, 40, 2):
            p.stroke([(x, 40), (x, 42)], steel[4])
            p.px(x, 42, steel[2])
            p.stroke([(x + 1, 45), (x + 1, 47)], steel[2])
            p.px(x + 1, 45, steel[3])
        p.stroke([(22, 46), (27, 50), (37, 50), (42, 46)], steel[1])
    p.rim('#ff2a3a', side=1, amount=0.55)
    p.rim('#7ab0ff', side=-1, amount=0.22)
    p.outline('#07050a')
    # corrupted scanlines now and then
    def band(y0, y1, dx, col):
        glitch(p, y0, y1, dx)
        m = np.zeros_like(p.a)
        m[y0:y1] = True
        tint(p, m, col, 0.35)
    if blink(t, 0.42, 1):
        band(18, 22, 3, '#ff2040')
        band(33, 35, -2, '#40e0ff')
    if blink(t, 0.86, 2):
        if blink(t, 0.86, 1):
            band(9, 12, -3, '#40e0ff')
            band(44, 46, 2, '#ff2040')
        else:
            band(27, 30, 4, '#ff2040')
    p.backdrop = (30, 6, 12)
    return p


# ---------------------------------------------------------------- 3. Sardakk
def sardakk(t=0.0):
    """a warrior bug: red-orange chitin, swept horns, angry compound eyes, sickle mandibles, a cord of trophy skulls.
    Moves: the mandibles clack (twice, then a screech), the antennae twitch, breathing."""
    p = Portrait()
    chit = ramp('#d8541e', spread=0.7)
    dark = ramp('#5a1810', spread=0.65)
    bone = ramp('#e6dab4', spread=0.5)
    eye = ramp('#b6f03a', spread=0.55)
    mand = ramp('#c89a5a', spread=0.62)
    body, head = breath(t), breath(t, -0.1)
    f = int(t * FRAMES) % FRAMES
    op = [0, 1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 2, 1, 0][f]
    with p.at(0, body):
        bod = ramp('#a23818', spread=0.72)
        p.fill(p.poly([(0, 66), (0, 53), (10, 47), (22, 48), (32, 53), (41, 48), (53, 47), (63, 53), (63, 66)]), bod, radius=8,
               texture=0.08)
        # spiked pauldrons
        for sp in ([(1, 52), (-2, 37), (8, 48)], [(9, 48), (10, 36), (16, 48)]):
            p.fill(p.poly(sp), chit, radius=2, bias=0.05)
            p.fill(p.poly(M(sp)), chit, radius=2, bias=-0.1)
        pl = [(0, 55), (9, 48), (21, 50), (15, 58), (3, 62)]
        p.fill(p.poly(pl), dark, radius=3, texture=0.08)
        p.fill(p.poly(M(pl)), dark, radius=3, bias=-0.15, texture=0.08)
        p.stroke([(3, 56), (12, 51)], chit[2])
        p.stroke([(60, 56), (51, 51)], chit[1])
        # neck and segmented thorax
        p.fill(p.poly([(23, 38), (40, 38), (42, 56), (21, 56)]), dark, radius=4, bias=-0.1, texture=0.08)
        for y in (42, 46):
            p.stroke([(24, y), (32, y + 1), (39, y)], dark[0])
        for y in (52, 57, 62):
            p.stroke([(22, y), (32, y + 1), (41, y)], dark[1])
        # trophy cord with skulls and teeth
        p.stroke([(9, 51), (18, 57), (32, 60), (46, 57), (54, 51)], '#3a2010')
        for x, y in [(13, 54), (24, 59), (39, 59), (50, 54)]:
            p.stroke([(x, y), (x, y + 2)], bone[4])
        for cx, cy in [(18, 58), (45, 58)]:
            p.fill(p.ellipse(cx + 0.5, cy + 0.5, 2.6, 2.4), bone, radius=2)
            p.pixels([(cx - 1, cy), (cx + 1, cy)], '#2a1410')
            p.pixels([(cx - 1, cy + 2), (cx + 1, cy + 2)], bone[1])
        p.fill(p.ellipse(32.5, 62, 3.4, 3.0), bone, radius=2)
        p.pixels([(31, 61), (33, 61), (31, 62), (33, 62)], '#2a1410')
        p.pixels([(30, 64), (32, 64), (34, 64)], bone[1])
    with p.at(0, head):
        # antennae (twitching tips)
        a1, a2 = sway(t, 0.0, 1, 3), sway(t, 0.4, 1, 3)
        p.stroke([(28, 9), (25, 4), (21 + a1, -1)], dark[3])
        p.stroke(M([(28, 9), (25, 4), (21 - a2, -1)]), dark[2])
        p.px(21 + a1, 0, chit[4])
        p.px(42 + a2, 0, chit[3])
        # swept horns
        for sp in ([(18, 14), (5, 2), (25, 9)], [(24, 10), (17, 0), (29, 8)]):
            p.fill(p.poly(sp), chit, radius=2, bias=0.05)
            p.fill(p.poly(M(sp)), chit, radius=2, bias=-0.1)
        # the head: a broad wedge narrowing to the jaw
        hd = p.poly([(13, 17), (20, 9), (32, 6), (43, 9), (50, 17), (50, 25), (45, 34), (38, 40), (32, 42), (25, 40), (18, 34),
                     (13, 25)])
        for sp in ([(15, 22), (8, 25), (14, 27)], [(17, 30), (11, 34), (18, 34)]):
            p.fill(p.poly(sp), chit, radius=1)
            p.fill(p.poly(M(sp)), chit, radius=1, bias=-0.15)
        p.fill(hd, chit, radius=8, texture=0.1, seed=51)
        p.stroke([(19, 12), (25, 9)], chit[4])
        p.stroke([(22, 14), (27, 12)], chit[1])
        p.stroke([(41, 14), (36, 12)], chit[1])
        # the frontal plate between the eyes
        p.fill(p.poly([(28, 9), (35, 9), (34, 24), (32, 32), (29, 24)]), chit, radius=3, bias=-0.2, texture=0.08)
        p.stroke([(32, 10), (32, 30)], dark[1])
        p.stroke([(30, 11), (30, 22)], chit[4])
        # compound eyes, glowing, cut by an angry brow
        el = [(13, 18), (27, 22), (28, 26), (24, 32), (17, 31), (13, 25)]
        p.fill(p.poly(el), eye, radius=3)
        p.fill(p.poly(M(el)), eye, radius=3, bias=-0.1)
        dots(p, p.poly(el) | p.poly(M(el)), eye[1], step=2)
        p.stroke([(13, 17), (27, 21)], dark[1], width=2)
        p.stroke([(50, 17), (36, 21)], dark[0], width=2)
        g = pulse(t, 0.2, 2)
        p.glow(17, 22, '#ffffff', mix('#d8ff70', '#ffffa0', g), r=0, strength=0.7)
        p.glow(46, 22, '#ffffff', mix('#d8ff70', '#ffffa0', g), r=0, strength=0.5)
        # cheek plates
        p.stroke([(18, 33), (24, 37), (27, 38)], dark[1])
        p.stroke([(45, 33), (39, 37), (36, 38)], dark[0])
        # the maw and the mandibles
        p.fill(p.poly([(26, 36), (37, 36), (36, 41 + op), (32, 43 + op), (27, 41 + op)]), ramp('#2a0606'), flat=1, edge=False)
        if op >= 2:
            p.fill(p.poly([(29, 38), (34, 38), (33, 40 + op), (32, 41 + op), (30, 40 + op)]), ramp('#c02a10'), flat=2 + (op > 2),
                   edge=False)
            p.pixels([(28, 37), (35, 37), (30, 37), (33, 37)], '#ffd8b0')
        p.stroke([(29, 41), (29, 45)], dark[3])
        p.stroke([(34, 41), (34, 45)], dark[2])
        lm = [(22, 35), (28, 37), (27, 41), (26, 46), (30 - op * 2, 51 + op), (27 - op * 2, 52 + op), (22, 49), (19, 43)]
        p.fill(p.poly(lm), mand, radius=2)
        p.fill(p.poly(M(lm)), mand, radius=2, bias=-0.15)
        for side in (1, -1):
            def X(x):
                return x if side > 0 else 63 - x
            p.stroke([(X(21), 39), (X(21), 45), (X(24), 49)], mand[4] if side > 0 else mand[3])
            p.pixels([(X(27), 42), (X(26), 45), (X(27), 48)], mand[1])
            p.pixels([(X(29 - op * 2), 51 + op), (X(28 - op * 2), 51 + op), (X(27 - op * 2), 52 + op)], '#2a1008')
    p.rim('#3fe6e6', side=1, amount=0.45)
    p.rim('#ffb43a', side=-1, amount=0.25)
    p.outline('#140604')
    # spit flecks on the screech
    if op == 3:
        for x, y in ((26, 50), (38, 49), (31, 53)):
            p.px(x + (f % 2), y + (f % 3), '#f0ffd0')
    p.backdrop = (36, 14, 10)
    return p


# ---------------------------------------------------------------- 4. Jol-Nar
def jolnar(t=0.0):
    """a haughty goldfish-scholar in a water-filled glass dome: pouting lips, monocle, barbel moustache, a brass ring
    collar and a high scholar's collar. Moves: bubbles rise in the dome, gills and fins flutter, a glint runs over the glass."""
    p = Portrait()
    suit = ramp('#4a38a8', spread=0.75)
    brass = ramp('#d8a840', spread=0.6, hue_shift=0.04)
    water = ramp('#1c6cb8', spread=0.7)
    fish = ramp('#ff9a3a', spread=0.6, hue_shift=0.05)
    fin = ramp('#ff5a8a', spread=0.6)
    cream = ramp('#fbe0b0', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (1, 55), (13, 48), (25, 47), (32, 50), (39, 47), (51, 48), (62, 55), (63, 66)]), suit, radius=9,
               texture=0.05)
        # brass air tanks over the shoulders
        tk = [(2, 46), (8, 43), (11, 56), (4, 59)]
        p.fill(p.poly(tk), brass, radius=2)
        p.fill(p.poly(M(tk)), brass, radius=2, bias=-0.15)
        p.stroke([(3, 51), (10, 49)], brass[0])
        p.stroke([(60, 51), (53, 49)], brass[0])
        # the high scholar's collar, flaring up beside the dome
        cl = [(12, 52), (8, 36), (17, 41), (27, 49), (25, 60)]
        p.fill(p.poly(cl), suit, radius=4, bias=0.05)
        p.fill(p.poly(M(cl)), suit, radius=4, bias=-0.2)
        p.stroke([(8, 37), (17, 42), (27, 50)], brass[3])
        p.stroke(M([(8, 37), (17, 42), (27, 50)]), brass[2])
        # buttons, a medal and a brass gauge
        for y in (53, 57, 61):
            p.px(32, y, brass[4])
            p.px(31, y, brass[1])
        p.fill(p.poly([(20, 56), (23, 56), (22, 60)]), ramp('#c02040'), radius=1)
        p.fill(p.ellipse(21.5, 61.5, 1.8, 1.8), brass, radius=1)
        p.fill(p.ellipse(42.5, 57.5, 3.2, 3.2), brass, radius=2)
        p.fill(p.ellipse(42.5, 57.5, 1.8, 1.8), cream, flat=3, edge=False)
        ang = 2 * math.pi * t
        p.px(42 + int(round(math.cos(ang))), 57 + int(round(math.sin(ang))), '#c02020')
    with p.at(0, head):
        # brass ring collar the dome sits in
        p.fill(p.ellipse(32, 43.5, 18, 3.5), brass, radius=2)
        # the dome and its water
        dome = p.ellipse(32, 23.5, 20.5, 20)
        p.fill(dome, water, radius=12, bias=-0.05)
        # dorsal fin (flutters)
        df = sway(t, 0.0, 1, 2)
        dfin = [(19, 17), (18, 7 + df), (22, 10), (24, 3 + df), (27, 8), (30, 3 - df), (32, 9), (36, 7 - df), (37, 13)]
        p.fill(p.poly(dfin), fin, radius=2)
        for (x0, y0), (x1, y1) in (((21, 15), (18, 8 + df)), ((25, 13), (24, 4 + df)), ((29, 12), (30, 4 - df)),
                                   ((33, 13), (36, 8 - df))):
            p.stroke([(x0, y0), (x1, y1)], fin[1])
        # pectoral fin
        pf = sway(t, 0.5, 1, 2)
        p.fill(p.poly([(22, 33), (14, 36 + pf), (15, 40 + pf), (24, 37)]), fin, radius=1)
        p.stroke([(21, 35), (15, 38 + pf)], fin[1])
        # the fish's head, turned to our right
        fh = p.ellipse(30, 25, 14, 12.5) | p.poly([(34, 15), (44, 19), (48, 25), (47, 32), (36, 36)])
        p.fill(fh, fish, radius=8, texture=0.06, seed=61)
        # scales behind the gill cover
        sc = fh & ~p.ellipse(37, 25, 10.5, 13)
        for y in range(15, 37, 4):
            for x in range(16, 30, 4):
                xx = x + (2 if (y // 4) % 2 else 0)
                if sc[y + p.dy, xx + p.dx] and sc[y + 1 + p.dy, xx + 2 + p.dx]:
                    p.pixels([(xx, y), (xx + 1, y + 1), (xx + 2, y + 1)], fish[1])
                    p.px(xx + 1, y, fish[4])
        # pale belly / throat
        p.fill(p.poly([(27, 33), (40, 31), (46, 31), (40, 36), (30, 37)]), cream, radius=2, edge=False)
        # gill cover (flutters)
        g = blink(t, 0.2, 3) or blink(t, 0.7, 3)
        gx = 1 if g else 0
        p.stroke([(29, 14), (27 - gx, 20), (27 - gx, 28), (30, 35)], fish[0])
        p.stroke([(30, 15), (28 - gx, 21), (28 - gx, 28), (31, 34)], fin[3] if g else fish[3])
        # pouting, downturned lips
        p.fill(p.poly([(44, 26), (50, 26), (51, 30), (48, 32), (43, 31)]), fin, radius=1)
        p.stroke([(44, 29), (51, 29)], '#5a1a2a')
        p.px(43, 30, '#5a1a2a')
        # barbel moustache, swaying
        b = sway(t, 0.25, 1)
        p.stroke([(44, 31), (41, 34), (40, 38 + b)], fin[3])
        p.stroke([(50, 31), (53, 34), (54, 38 - b)], fin[2])
        # the eye: heavy-lidded, behind a monocle
        p.fill(p.ellipse(39, 20.5, 3.6, 3.6), ramp('#f8f0d8'), radius=2)
        p.pixels([(39, 21), (40, 21), (39, 22), (40, 22)], '#1a1030')
        p.px(40, 21, '#3a2a60')
        p.fill(p.poly([(35, 17), (43, 17), (43, 20), (35, 20)]), fish, flat=2, edge=False)
        p.stroke([(35, 20), (43, 19)], fish[0])
        p.stroke([(34, 16), (37, 14), (42, 13), (44, 14)], fish[0])
        p.stroke([(35, 15), (38, 13), (42, 12)], fish[4])
        mono = p.ellipse(39, 20.5, 4.8, 4.8) & ~p.ellipse(39, 20.5, 3.7, 3.7)
        p.fill(mono, brass, radius=1, edge=False)
        p.px(36, 18, '#ffffff')
        p.stroke([(43, 23), (44, 27)], brass[2])
        # the water lies over the fish
        tint(p, dome, water[3], 0.1)
        # bubbles rising from the lips
        for i, (x0, ph) in enumerate(((48, 0.0), (46, 0.33), (50, 0.66), (45, 0.5))):
            u = (t * (1 if i < 3 else 2) + ph) % 1.0
            y = int(round(25 - u * 20))
            x = x0 + sway(t, ph, 1, 2) - int(u * 2)
            yy, xx = y + p.dy, x + p.dx
            if 0 <= yy < 64 and 0 <= xx < 64 and dome[yy, xx]:
                if i % 2 == 0 and u > 0.35:
                    p.pixels([(x, y - 1), (x - 1, y), (x + 1, y), (x, y + 1)], '#b8ecff')
                    p.px(x - 1, y - 1, '#ffffff')
                else:
                    p.px(x, y, '#d8f6ff')
        # glass highlights and the travelling glint
        p.stroke([(15, 19), (18, 12), (24, 7)], '#e8fcff')
        p.stroke([(16, 24), (16, 21)], '#a0e8ff')
        p.stroke([(47, 35), (50, 31)], '#7ad0ff')
        gl = path_px([(13, 26), (15, 15), (21, 8), (30, 5), (40, 6), (48, 11)])
        gi = int(t * len(gl) * 1.6)
        if gi < len(gl):
            gx_, gy_ = gl[gi]
            p.glow(gx_, gy_, '#ffffff', '#c0f8ff', r=0, strength=0.9)
        # front rim of the brass ring with rivets
        p.fill(p.ellipse(32, 45, 17.5, 2.5) & ~p.ellipse(32, 43.5, 17, 2.5), brass, radius=1, bias=0.1)
        for x in range(17, 48, 5):
            p.px(x, 45 if 20 < x < 44 else 44, brass[4])
    p.rim('#46f0ff', side=1, amount=0.45)
    p.rim('#ff5ad2', side=-1, amount=0.25)
    p.outline('#060a1c')
    p.backdrop = (10, 14, 44)
    return p


# ---------------------------------------------------------------- 5. Winnu
def winnu(t=0.0):
    """a gaunt, long-faced noble under a towering golden mitre with the red sun-ring, gold lappets, a high violet collar,
    looking down his nose. Moves: breathing, the mitre's ring and gems pulse in turn, a slow contemptuous blink."""
    p = Portrait()
    skin = ramp('#9088a8', spread=0.66)
    gold = ramp('#d8a030', spread=0.65, hue_shift=0.04)
    robe = ramp('#5c2a8c', spread=0.78)
    lin = ramp('#a02a2a', spread=0.7)
    gem = ramp('#ff5020', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (1, 54), (14, 48), (32, 52), (49, 48), (62, 54), (63, 66)]), robe, radius=9, texture=0.05)
        # the great collar, standing up behind the jaw, lined in red
        cl = [(5, 58), (4, 40), (7, 28), (12, 22), (18, 32), (23, 46), (27, 56)]
        p.fill(p.poly(cl), robe, radius=4, bias=0.05)
        p.fill(p.poly(M(cl)), robe, radius=4, bias=-0.2)
        li = [(10, 50), (10, 36), (13, 28), (18, 37), (22, 50)]
        p.fill(p.poly(li), lin, radius=3, bias=-0.1)
        p.fill(p.poly(M(li)), lin, radius=3, bias=-0.3)
        p.stroke([(4, 41), (7, 29), (12, 23), (18, 33), (23, 47)], gold[3])
        p.stroke(M([(4, 41), (7, 29), (12, 23), (18, 33), (23, 47)]), gold[2])
        for x, y in [(6, 46), (7, 38), (9, 31)]:
            p.px(x, y, gold[4])
            p.px(63 - x, y, gold[2])
        # dark inner robe behind the neck
        p.fill(p.poly([(20, 42), (44, 42), (46, 58), (18, 58)]), robe, flat=0, edge=False)
        # neck: long and thin, a cord of tendons
        p.fill(p.poly([(27, 40), (37, 40), (37, 53), (32, 56), (27, 53)]), skin, radius=3, bias=-0.25)
        p.stroke([(29, 44), (30, 52)], skin[1])
        p.stroke([(35, 44), (34, 52)], skin[0])
        # gold pectoral with the ring gem
        p.fill(p.poly([(20, 52), (32, 57), (43, 52), (41, 60), (32, 65), (22, 60)]), gold, radius=3)
        p.stroke([(23, 55), (32, 59), (40, 55)], gold[1])
        p.fill(p.ellipse(32, 60, 2.6, 2.6), gem, radius=2)
        p.glow(32, 60, '#fff0c0', '#ff8040', r=0, strength=0.4 + 0.6 * pulse(t, 0.66))
    with p.at(0, head):
        # side horns of the mitre
        hn = [(22, 12), (12, -1), (15, 9), (21, 18)]
        p.fill(p.poly(hn), gold, radius=2)
        p.fill(p.poly(M(hn)), gold, radius=2, bias=-0.15)
        # the towering mitre, fluted
        mit = p.poly([(21, 19), (22, 10), (25, 2), (28, -3), (35, -3), (38, 2), (41, 10), (42, 19)])
        p.fill(mit, gold, radius=6, texture=0.04)
        for x, c in ((25, 1), (28, 3), (35, 1), (38, 0)):
            p.stroke([(x, 1 if 26 < x < 37 else 5), (x, 16)], gold[c])
        # the red sun-ring with a gold star, pulsing
        rk = pulse(t, 0.0)
        ring = p.ellipse(32, 9, 4.6, 4.6) & ~p.ellipse(32, 9, 2.7, 2.7)
        p.fill(ring, gem, radius=1)
        tint(p, ring, '#ffb060', 0.5 * rk)
        p.pixels([(31, 6), (32, 7), (31, 7), (30, 8), (31, 8), (32, 8), (33, 8), (31, 9), (32, 9), (30, 10), (33, 10)], gold[4])
        p.glow(31, 8, '#ffffff', '#ffd080', r=0, strength=0.3 + 0.6 * rk)
        # long gold lappets falling beside the face
        lp = [(20, 18), (24, 18), (24, 42), (21, 44), (19, 40)]
        p.fill(p.poly(lp), gold, radius=2)
        p.fill(p.poly(M(lp)), gold, radius=2, bias=-0.15)
        for y in (24, 30, 36):
            p.stroke([(20, y), (23, y)], robe[1])
            p.stroke([(40, y), (43, y)], robe[0])
        # the long gaunt face
        face = p.ellipse(32, 29, 9.5, 16) & ~p.poly([(22, 35), (29, 47), (22, 47)]) & ~p.poly([(42, 35), (35, 47), (42, 47)])
        p.fill(face, skin, radius=7, texture=0.06, seed=71)
        # mitre rim over the brow, gems pulsing one after another
        p.fill(p.poly([(20, 16), (43, 16), (43, 20), (32, 19), (20, 20)]), gold, radius=2, bias=0.1)
        for i, x in enumerate((24, 28, 32, 36, 40)):
            g = pulse(t, -i * 0.12)
            p.px(x, 18, mix(gem[1], '#ffe0a0', g))
        # sunken eye sockets, hollow cheeks, high cheekbones
        for cx, s in ((27, 1), (37.5, 0)):
            p.fill(p.ellipse(cx, 25.5, 4.2, 2.8), skin, flat=s, edge=False)
        p.stroke([(24, 31), (26, 37), (29, 41)], skin[1])
        p.stroke([(40, 31), (38, 37), (35, 41)], skin[0])
        p.pixels([(24, 29), (25, 30)], skin[4])
        p.pixels([(40, 29), (39, 30)], skin[3])
        # brows: the left one raised in disdain
        p.stroke([(23, 22), (26, 21), (30, 23)], skin[4])
        p.stroke([(34, 24), (37, 23), (41, 24)], skin[3])
        p.stroke([(23, 23), (26, 22)], skin[0])
        # heavy-lidded eyes looking down the nose
        shut = blink(t, 0.55, 3)
        for x0 in (25, 35):
            if shut:
                p.stroke([(x0, 27), (x0 + 4, 27)], skin[0])
                p.stroke([(x0, 26), (x0 + 4, 26)], skin[2])
            else:
                p.stroke([(x0, 26), (x0 + 4, 26)], skin[0])
                p.pixels([(x0, 27), (x0 + 1, 27), (x0 + 2, 27), (x0 + 3, 27), (x0 + 4, 27)], '#e8dcc8')
                p.pixels([(x0 + 1, 27), (x0 + 2, 27)], '#b050ff')
                p.px(x0 + 2, 27, '#ffffff')
                p.px(x0 + 1, 28, '#6a2aa0')
        # long nose, nostrils shown (chin raised)
        p.stroke([(32, 24), (32, 34)], skin[4])
        p.stroke([(33, 25), (33, 34)], skin[1])
        p.pixels([(30, 35), (31, 35), (34, 35)], skin[0])
        p.px(32, 35, skin[2])
        # thin mouth, corners turned down, one lip curled
        p.stroke([(28, 40), (29, 39), (34, 39), (36, 38)], '#2a1438')
        p.pixels([(27, 41), (37, 39)], skin[0])
        p.stroke([(30, 41), (34, 41)], skin[3])
        p.stroke([(30, 44), (34, 44)], skin[1])
    p.rim('#46d8ff', side=1, amount=0.45)
    p.rim('#ff8a3a', side=-1, amount=0.25)
    p.outline('#100612')
    p.backdrop = (50, 18, 12)
    return p


# ---------------------------------------------------------------- 6. Xxcha
def xxcha(t=0.0):
    """an old tortoise diplomat: a broad wrinkled head with a horn beak, the domed shell rising behind, green robes
    with cream trim, a bead necklace with a jade pendant. Moves: slow breathing, a patient blink, the pendant glows."""
    p = Portrait()
    skin = ramp('#7ea05e', spread=0.66)
    horn = ramp('#cdb07a', spread=0.55)
    shell = ramp('#9a6a2c', spread=0.7, hue_shift=0.05)
    robe = ramp('#2f7a52', spread=0.75)
    cream = ramp('#e6dab8', spread=0.45)
    body, head = breath(t), breath(t, -0.15)
    jade = pulse(t, 0.25)
    with p.at(0, body):
        # the domed shell rising behind the head
        sh = p.ellipse(32, 36, 30.5, 35)
        p.fill(sh, shell, radius=12, texture=0.06, seed=81)
        scutes(p, sh, [(32, 3), (18, 7), (46, 7), (10, 19), (54, 19), (6, 33), (58, 33), (9, 47), (55, 47), (32, 22), (21, 26),
                       (43, 26)], seam='#2a1408', ring=shell[1], centre='#ffd870', margin=3, mcentre=(32, 36), mstep=16)
        # robe: green with cream trim, crossed over
        p.fill(p.poly([(0, 66), (1, 54), (12, 47), (24, 46), (32, 52), (40, 46), (52, 47), (62, 54), (63, 66)]), robe, radius=9,
               texture=0.05)
        # a cream stole over the shoulders, crossing low
        st = [(10, 50), (17, 47), (22, 47), (30, 61), (29, 66), (22, 66), (16, 56)]
        p.fill(p.poly(st), cream, radius=3, texture=0.04)
        p.fill(p.poly(M(st)), cream, radius=3, bias=-0.15, texture=0.04)
        for y in (53, 57, 61):
            p.pixels([(20 + (y - 53) // 2, y), (43 - (y - 53) // 2, y)], robe[2])
        p.stroke([(13, 51), (21, 49)], robe[1])
        p.stroke([(50, 51), (42, 49)], robe[0])
    with p.at(0, head):
        # the thick wrinkled neck
        p.fill(p.poly([(21, 34), (43, 34), (44, 50), (32, 55), (20, 50)]), skin, radius=5, bias=-0.15, texture=0.1, seed=83)
        for y in (42, 46):
            p.stroke([(23, y), (27, y + 1), (32, y), (37, y + 1), (41, y)], skin[1])
            p.stroke([(25, y + 1), (29, y + 2)], skin[3])
    with p.at(0, body):
        # bead necklace and the jade pendant
        beads = path_px([(19, 46), (25, 52), (32, 55), (39, 52), (45, 46)])
        for i, (x, y) in enumerate(beads):
            if i % 3 == 0:
                c = ramp(['#ffc040', '#d0503a', '#40c0a0'][(i // 3) % 3], spread=0.5)
                p.pixels([(x, y), (x + 1, y)], c[3])
                p.pixels([(x, y + 1), (x + 1, y + 1)], c[1])
        p.fill(p.poly([(32, 55), (35, 58), (32, 62), (29, 58)]), ramp('#38d8a0'), radius=2)
        p.glow(32, 58, mix('#d0fff0', '#ffffff', jade), mix('#30a070', '#80ffd0', jade), r=1, strength=0.35 + 0.6 * jade)
    with p.at(0, head):
        # the head: broad, flat-topped, a rounded snout
        hd = p.ellipse(32, 23, 15, 12) | p.ellipse(32, 30, 12, 8.5)
        p.fill(hd, skin, radius=8, texture=0.1, seed=82)
        # crown plates
        scutes(p, p.ellipse(32, 17, 10, 6) & hd, [(32, 13), (25, 16), (39, 16), (29, 20), (35, 20)], seam=skin[0],
               centre=skin[4])
        # eyes set to the sides under heavy wrinkled lids
        shut = blink(t, 0.7, 3)
        for cx, s in ((24, 1), (40, 0)):
            p.fill(p.ellipse(cx, 24.5, 3.8, 2.8), skin, flat=s, edge=False)
        for x0, sgn in ((21, 1), (38, -1)):
            if shut:
                p.stroke([(x0, 25), (x0 + 4, 25)], skin[0])
            else:
                p.pixels([(x0 + 1, 25), (x0 + 2, 25), (x0 + 3, 25)], '#24160c')
                p.px(x0 + 2, 24, '#24160c')
                p.px(x0 + 1 if sgn > 0 else x0 + 2, 24, '#ffd890')
            p.stroke([(x0, 23), (x0 + 2, 22), (x0 + 4, 23)], skin[4] if sgn > 0 else skin[3])
            p.stroke([(x0, 27), (x0 + 4, 27)], skin[1])
            p.stroke([(x0 + 1, 29), (x0 + 3, 28)], skin[1])
        # nostrils on the snout
        p.pixels([(30, 28), (33, 28)], skin[0])
        p.stroke([(29, 26), (32, 25), (35, 26)], skin[4])
        # the horn beak: a wide mouth line hooking down in the middle
        bk = p.poly([(21, 31), (28, 33), (32, 32), (36, 33), (43, 31), (41, 34), (34, 36), (32, 39), (30, 36), (23, 34)])
        p.fill(bk, horn, radius=2)
        p.stroke([(22, 33), (28, 35), (31, 36), (32, 38), (33, 36), (36, 35), (42, 33)], '#3a2410')
        p.stroke([(25, 37), (32, 41), (39, 37)], skin[1])
        p.pixels([(25, 32), (26, 32)], horn[4])
    p.rim('#7affd0', side=1, amount=0.45)
    p.rim('#ffc860', side=-1, amount=0.25)
    p.outline('#061410')
    p.backdrop = (10, 26, 30)
    return p


# ---------------------------------------------------------------- 7. Yin
def yin(t=0.0):
    """a clone monk: shaved head, pale, a glowing sigil on the brow, a fixed stare; white and gold robes under a tall
    pointed hood.
    Moves: breathing, the sigil and the eyes pulse, the hood's hem sways."""
    p = Portrait()
    skin = ramp('#ead6cc', spread=0.52, hue_shift=0.08)
    robe = ramp('#e6e2f0', spread=0.58)
    inner = ramp('#2e1650', spread=0.7)
    gold = ramp('#dcaa30', spread=0.62)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    with p.at(0, body):
        # the tall pointed hood standing behind the head
        hs = sway(t, 0.0, 1)
        hood = p.poly([(7 - hs, 62), (5, 42), (8, 24), (14, 11), (23, 3), (32, -2), (41, 3), (50, 11), (56, 24), (59, 42),
                       (57 + hs, 62)])
        p.fill(hood, robe, radius=8, texture=0.02)
        p.stroke([(8, 44), (9, 28), (14, 15)], robe[2])
        p.stroke([(56, 44), (55, 28), (50, 15)], robe[1])
        hin = [(14, 50), (12, 32), (15, 18), (22, 9), (32, 3), (42, 9), (49, 18), (52, 32), (50, 50)]
        p.fill(p.poly(hin), inner, radius=6, bias=-0.15)
        p.stroke(hin, gold[3])
        p.stroke([(x + (1 if x < 32 else -1), y + 1) for x, y in hin], gold[1])
        for x, y in [(10, 40), (11, 26), (16, 13), (53, 40), (52, 26), (47, 13)]:
            p.px(x, y, gold[4] if x < 32 else gold[2])
    with p.at(0, body):
        # robe and the gold-trimmed crossing front
        p.fill(p.poly([(0, 66), (2, 54), (13, 49), (25, 48), (32, 52), (39, 48), (51, 49), (62, 54), (63, 66)]), robe, radius=9,
               texture=0.02)
        p.fill(p.poly([(22, 47), (32, 62), (42, 47), (38, 47), (32, 56), (26, 47)]), gold, radius=2)
        p.stroke([(13, 50), (24, 49), (32, 63)], gold[2])
        p.stroke([(51, 50), (40, 49), (32, 63)], gold[1])
        for i in range(4):
            p.px(22 - i * 2, 51 + i * 3, gold[3])
            p.px(41 + i * 2, 51 + i * 3, gold[1])
        p.stroke([(6, 58), (14, 52), (18, 64)], robe[2])
        p.stroke([(57, 58), (49, 52), (45, 64)], robe[1])
        p.fill(p.ellipse(32.5, 59.5, 2, 2), ramp('#ffc830'), radius=1)
        p.glow(32, 59, '#ffffff', '#ffd060', r=0, strength=0.3 + 0.6 * k)
        # neck and a high wrapped collar
        p.fill(p.poly([(28, 37), (36, 37), (36, 48), (28, 48)]), skin, radius=3, bias=-0.2)
        p.fill(p.poly([(23, 45), (28, 44), (32, 47), (36, 44), (41, 45), (43, 51), (32, 56), (21, 51)]), robe, radius=3, bias=0.05)
        p.stroke([(23, 45), (28, 44), (32, 47), (36, 44), (41, 45)], gold[3])
    with p.at(0, head):
        hd = p.ellipse(32, 24.5, 11.5, 15) & ~p.poly([(20, 32), (29, 43), (20, 43)]) & ~p.poly([(44, 32), (35, 43), (44, 43)])
        p.fill(hd, skin, radius=8, texture=0.05, seed=91)
        # the shaven crown catches the light
        p.pixels([(26, 13), (27, 12), (28, 11), (29, 11), (27, 13)], skin[4])
        p.px(28, 12, '#fffaf4')
        # ears
        p.fill(p.ellipse(20.5, 26, 1.8, 3.2), skin, radius=1)
        p.fill(p.ellipse(43.5, 26, 1.8, 3.2), skin, radius=1, bias=-0.2)
        # cheekbones
        p.pixels([(24, 29), (25, 30), (25, 31)], skin[1])
        p.pixels([(40, 29), (39, 30), (39, 31)], skin[0])
        p.pixels([(24, 28), (25, 28)], skin[4])
        # sockets, brows drawn together, the stare
        for cx, s in ((27, 1), (37.5, 0)):
            p.fill(p.ellipse(cx, 24.5, 3.8, 2.4), skin, flat=s, edge=False)
        p.stroke([(23, 21), (27, 21), (30, 22)], skin[0])
        p.stroke([(34, 22), (37, 21), (41, 21)], skin[0])
        for x0 in (25, 35):
            p.glow(x0 + 2, 24, '#ffffff', '#9a50ff', r=1, strength=0.25 + 0.25 * k)
            p.pixels([(x0, 24), (x0 + 1, 24), (x0 + 2, 24), (x0 + 3, 24), (x0 + 4, 24)], '#fbf6ff')
            p.pixels([(x0 + 1, 25), (x0 + 2, 25), (x0 + 3, 25)], '#e0d6ea')
            p.pixels([(x0 + 1, 24), (x0 + 2, 24), (x0 + 3, 24)], mix('#8a40e0', '#c27aff', k))
            p.px(x0 + 2, 25, mix('#6a28b0', '#a050f0', k))
            p.px(x0 + 2, 24, '#ffffff' if k > 0.5 else '#1a0830')
            p.stroke([(x0, 23), (x0 + 4, 23)], '#4a2a40')
            p.px(x0 + (-1 if x0 < 30 else 5), 24, '#4a2a40')
        # nose, thin lips
        p.stroke([(32, 25), (32, 31)], skin[4])
        p.stroke([(33, 26), (33, 31)], skin[2])
        p.pixels([(31, 32), (34, 32)], skin[1])
        p.stroke([(29, 36), (35, 36)], '#8a5068')
        p.stroke([(30, 37), (34, 37)], skin[1])
        p.stroke([(30, 40), (34, 40)], skin[1])
        # the sigil on the brow: a gold ring with a burning core
        sig = p.ellipse(32, 15.5, 2.8, 2.8) & ~p.ellipse(32, 15.5, 1.4, 1.4)
        p.fill(sig, ramp('#ffc830'), radius=1, edge=False)
        p.glow(31, 15, '#ffffff', mix('#ff8a30', '#fff080', k), r=1, strength=0.35 + 0.6 * k)
        if k > 0.7:
            p.pixels([(31, 11), (31, 19), (27, 15), (35, 15)], '#ffe8a0')
    p.rim('#ff4fd2', side=1, amount=0.45)
    p.rim('#9a7aff', side=-1, amount=0.3)
    p.outline('#0e0618')
    p.backdrop = (32, 8, 44)
    return p


# ---------------------------------------------------------------- 8. Yssaril
def yssaril(t=0.0):
    """a small sly reptile spy in a dark hood: huge ears through the hood, big slanted yellow eyes, a toothy grin,
    a comm bead in one ear, a dagger held ready. Moves: the pupils dart, an ear twitches, the cloak sways, the blade glints."""
    p = Portrait()
    skin = ramp('#5eaa40', spread=0.7)
    cloak = ramp('#2e2a44', spread=0.8)
    steel = ramp('#c0ccd8', spread=0.55)
    eye = ramp('#ffc820', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    f = int(t * FRAMES) % FRAMES
    with p.at(0, body):
        cs = sway(t, 0.3, 1)
        p.fill(p.poly([(0, 66), (0, 50), (9, 43), (21, 44), (32, 50), (43, 44), (55, 43), (63 + cs, 50), (63, 66)]), cloak,
               radius=9, texture=0.06)
        p.stroke([(20, 46), (23, 56), (21 + cs, 64)], cloak[0])
        p.stroke([(9, 50), (6 - cs, 64)], cloak[1])
        p.stroke([(44, 46), (41, 56), (43 + cs, 64)], cloak[0])
        # clasp
        p.fill(p.ellipse(32, 51, 2.4, 2.4), ramp('#d82a2a'), radius=1)
        p.glow(32, 51, '#ffe0e0', '#ff5040', r=0, strength=0.6)
    with p.at(0, head):
        # the hood around the face
        p.fill(p.poly([(13, 48), (12, 30), (14, 17), (21, 7), (32, 3), (43, 7), (50, 17), (52, 30), (51, 48), (42, 46), (32, 50),
                       (22, 46)]), cloak, radius=6, texture=0.05)
        p.fill(p.poly([(18, 42), (17, 26), (21, 16), (32, 12), (43, 16), (47, 26), (46, 42), (32, 46)]), cloak, flat=0, edge=False)
        # ears: huge, poking out of the hood; the right one twitches
        tw = -1 if blink(t, 0.45, 2) or blink(t, 0.55, 1) else 0
        el = [(19, 22), (1, 11), (4, 18), (10, 25), (19, 29)]
        p.fill(p.poly(el), skin, radius=3, texture=0.06)
        p.fill(p.poly([(44, 22), (62, 11 + tw), (59, 18 + tw), (53, 25), (44, 29)]), skin, radius=3, bias=-0.15, texture=0.06)
        p.stroke([(16, 23), (6, 15), (12, 24)], ramp('#c86a6a')[1])
        p.stroke([(47, 23), (57, 15 + tw), (51, 24)], ramp('#c86a6a')[0])
        # comm bead in the left ear, blinking
        if f % 6 < 3:
            p.glow(9, 19, '#ffffff', '#3fe6e6', r=0, strength=0.8)
        else:
            p.px(9, 19, '#2a8080')
        # face: wide cheeks, pointed chin, mottled
        face = p.poly([(20, 19), (26, 15), (38, 15), (44, 19), (46, 27), (41, 36), (32, 43), (23, 36), (18, 27)])
        p.fill(face, skin, radius=7, texture=0.1, seed=101)
        for x, y in [(22, 30), (24, 32), (40, 31), (42, 28), (30, 39), (35, 40)]:
            p.px(x, y, skin[1])
        # the hood's brim shadows the brow
        tint(p, face & p.poly([(18, 15), (46, 15), (46, 22), (18, 22)]), '#10141a', 0.45)
        p.fill(p.poly([(17, 22), (22, 14), (32, 11), (42, 14), (47, 22), (42, 18), (32, 15), (22, 18)]), cloak, radius=2,
               bias=0.1)
        # big slanted eyes, slit pupils that dart
        dart = [0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, -2, -2, -2, -2, 0, 0, 0, 0, 0, 0][f]
        le = [(20, 23), (25, 21), (29, 23), (30, 27), (27, 28), (22, 27)]
        for pts, b, cx in ((le, 0.05, 26), (M(le), -0.1, 37)):
            p.fill(p.poly(pts), eye, radius=2, bias=b, edge=False)
            p.stroke(pts[:4], '#102010')
            px_ = cx + dart
            p.stroke([(px_, 23), (px_, 26)], '#120804')
            p.px(px_ + (1 if dart <= 0 else -1), 25, eye[1])
        p.px(23, 24, '#ffffff')
        p.px(36, 24, '#ffffff')
        p.glow(26, 25, '#ffffff', '#fff080', r=0, strength=0.25)
        # nostrils, a wide toothy grin (one corner higher)
        p.pixels([(30, 30), (33, 30)], skin[0])
        p.fill(p.poly([(23, 33), (32, 35), (42, 31), (40, 34), (32, 38), (26, 36)]), ramp('#5a1018'), flat=1, edge=False)
        p.stroke([(23, 33), (32, 35), (42, 31)], '#140808')
        for x in range(25, 41, 2):
            y = 35 if x < 33 else 35 - (x - 33) // 3
            p.px(x, y, '#f4f0dc')
            p.px(x + 1, y + 2 if x < 38 else y + 1, '#d8d4c0')
        p.stroke([(23, 36), (26, 38), (32, 39)], skin[1])
    with p.at(0, body):
        # a clawed hand with a dagger, ready
        p.fill(p.poly([(43, 52), (47, 50), (53, 41), (54, 42), (50, 52), (47, 54)]), steel, radius=1)
        p.stroke([(48, 50), (53, 42)], steel[4])
        p.fill(p.poly([(43, 52), (49, 56), (48, 58), (42, 54)]), ramp('#c8a040'), radius=1)
        p.fill(p.poly([(42, 66), (40, 59), (43, 55), (49, 57), (51, 62), (50, 66)]), skin, radius=3, bias=-0.1)
        p.stroke([(42, 58), (47, 59)], skin[0])
        p.stroke([(41, 61), (47, 62)], skin[0])
        p.pixels([(39, 58), (39, 61)], '#f0f0d8')
        if blink(t, 0.25, 2) or blink(t, 0.8, 1):
            p.glow(52, 43, '#ffffff', '#e0f8ff', r=0, strength=0.9)
    p.rim('#c8ff3a', side=1, amount=0.45)
    p.rim('#ff4fa0', side=-1, amount=0.25)
    p.outline('#060a06')
    p.backdrop = (12, 22, 18)
    return p


# ---------------------------------------------------------------- 9. Argent
def argent(t=0.0):
    """a falcon warrior, three-quarter to our right: white face, orange cap and malar stripe, a short hooked slate beak,
    a fierce eye under a heavy brow, swept crest feathers, armour of feather-shaped plates.
    Moves: breathing, the crest feathers ruffle in turn, the eye glints."""
    p = Portrait()
    white = ramp('#e4e6ee', spread=0.5)
    org = ramp('#f08420', spread=0.62, hue_shift=0.05)
    steel = ramp('#8a92a8', spread=0.68)
    slate = ramp('#46506a', spread=0.62)
    cere = ramp('#f2c430', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 47), (11, 41), (24, 43), (38, 46), (52, 45), (63, 50), (63, 66)]), slate, radius=9,
               texture=0.05)
        # feather-shaped plates over the left shoulder, overlapping
        for i, (x, y) in enumerate(((1, 44), (5, 49), (9, 54), (14, 59))):
            pl = p.poly([(x, y), (x + 13, y - 5), (x + 20, y - 3), (x + 6, y + 5)])
            p.fill(pl, steel, radius=2, bias=-0.04 * i, texture=0.04)
            p.stroke([(x + 3, y + 2), (x + 17, y - 3)], org[3] if i % 2 == 0 else org[2])
        # and the right shoulder
        for i, (x, y) in enumerate(((60, 49), (56, 54), (52, 59))):
            pl = p.poly([(x + 3, y + 1), (x - 10, y - 3), (x - 14, y), (x - 2, y + 5)])
            p.fill(pl, steel, radius=2, bias=-0.15)
            p.stroke([(x, y + 2), (x - 11, y - 1)], org[1])
        # a gorget of scale-feathers at the throat
        # an armoured collar, and the white throat feathers falling over it in points
        p.fill(p.poly([(17, 44), (44, 42), (49, 50), (40, 56), (22, 56), (14, 50)]), steel, radius=3, bias=0.05, texture=0.04)
        p.stroke([(16, 50), (22, 55), (40, 55), (48, 50)], org[2])
        thr = [(20, 33), (42, 32), (45, 42), (42, 47), (39, 44), (36, 49), (33, 45), (30, 50), (27, 45), (24, 48), (21, 43)]
        p.fill(p.poly(thr), white, radius=4, bias=-0.05, texture=0.05)
        for x, y in [(25, 40), (31, 41), (37, 39), (28, 36), (35, 35), (40, 42)]:
            p.pixels([(x, y), (x + 1, y + 1), (x + 2, y)], white[1])
        p.fill(p.ellipse(32, 52.5, 2.4, 2.4), ramp('#ff7a20'), radius=1)
        p.glow(32, 52, '#ffffff', '#ffb050', r=0, strength=0.4 + 0.5 * pulse(t, 0.3, 2))
    with p.at(0, head):
        # crest feathers swept back, ruffling in turn
        cf = (((24, 9), (4, 2), (22, 14)), ((21, 13), (1, 11), (20, 18)), ((19, 18), (2, 20), (20, 23)),
              ((20, 23), (6, 29), (22, 27)), ((28, 7), (14, -2), (26, 11)))
        for i, (a, b, c) in enumerate(cf):
            r = sway(t, -0.14 * i, 1)
            rmp = org if i % 2 == 0 else white
            p.fill(p.poly([a, (b[0], b[1] + r), c]), rmp, radius=2, bias=-0.05 * i)
            p.stroke([a, ((a[0] + b[0]) // 2, (a[1] + b[1] + r) // 2)], rmp[1])
        # head
        hd = p.ellipse(30, 22, 14, 14) | p.poly([(22, 28), (42, 26), (41, 37), (24, 37)])
        p.fill(hd, white, radius=8, texture=0.05, seed=111)
        # the orange cap and the malar stripe below the eye
        cap = p.poly([(16, 25), (16, 15), (21, 10), (29, 7), (37, 8), (43, 12), (42, 16), (36, 15), (33, 18), (32, 24), (36, 26),
                      (37, 33), (32, 34), (28, 28), (22, 28)])
        p.fill(cap & hd, org, radius=4, texture=0.08, seed=112)
        for x, y in [(21, 14), (25, 11), (29, 10), (19, 20), (24, 17), (28, 14), (22, 24), (27, 21)]:
            p.pixels([(x, y), (x + 1, y + 1)], org[1])
            p.px(x + 2, y, org[4])
        # white cheek streaks
        for x, y in [(24, 32), (28, 35), (33, 36)]:
            p.px(x, y, white[2])
        # heavy silver brow over a fierce eye
        p.fill(p.ellipse(38.5, 19.5, 3.4, 3.2), cere, radius=2)
        p.fill(p.ellipse(38.5, 19.5, 2.2, 2.2), ramp('#2a1408'), flat=1, edge=False)
        p.pixels([(38, 19), (39, 19), (38, 20)], '#0a0404')
        p.fill(p.poly([(33, 16), (38, 14), (44, 15), (43, 18), (38, 17), (34, 18)]), white, radius=2, bias=0.1)
        p.stroke([(34, 18), (38, 17), (43, 18)], slate[0])
        g = blink(t, 0.3, 2) or blink(t, 0.8, 1)
        p.px(37, 19, '#ffffff' if g else '#c8a080')
        if g:
            p.glow(37, 19, '#ffffff', '#fff0c0', r=0, strength=0.5)
        # the beak: yellow cere, slate hook
        bk = p.poly([(42, 16), (48, 17), (53, 20), (55, 25), (54, 30), (51, 33), (51, 29), (48, 26), (42, 26)])
        p.fill(bk, slate, radius=3, bias=0.05)
        p.fill(p.poly([(41, 16), (46, 16), (47, 22), (43, 24), (41, 23)]), cere, radius=2)
        p.px(45, 19, '#2a1a10')
        p.stroke([(47, 18), (52, 21), (54, 25)], slate[4])
        p.fill(p.poly([(52, 28), (54, 28), (53, 32), (51, 33)]), ramp('#1a1c26'), flat=1, edge=False)
        # lower mandible and gape
        p.fill(p.poly([(42, 27), (49, 27), (50, 29), (44, 31)]), slate, radius=1, bias=-0.1)
        p.stroke([(41, 26), (49, 27)], '#140c10')
    p.rim('#ff4f9a', side=1, amount=0.45)
    p.rim('#46b0ff', side=-1, amount=0.3)
    p.outline('#08080e')
    p.backdrop = (12, 18, 42)
    return p


ALL = {'naalu': naalu, 'nekro': nekro, 'sardakk': sardakk, 'jolnar': jolnar, 'winnu': winnu, 'xxcha': xxcha, 'yin': yin,
       'yssaril': yssaril, 'argent': argent}


def sheet(path, keys=None, scale=4):
    from PIL import Image, ImageDraw
    keys = keys or list(ALL)
    cols = 3
    cw, ch = 2 * 64 * scale + 10 + 20, 64 * scale + 30
    rows = (len(keys) + cols - 1) // cols
    sh = Image.new('RGB', (cols * cw + 10, rows * ch + 10), (230, 230, 230))
    d = ImageDraw.Draw(sh)
    for i, k in enumerate(keys):
        x0, y0 = 10 + (i % cols) * cw, 10 + (i // cols) * ch
        for j, tt in enumerate((0.0, 0.5)):
            p = ALL[k](tt)
            im = p.image(scale, bg=tuple(p.backdrop) + (255,)).convert('RGB')
            sh.paste(im, (x0 + j * (64 * scale + 10), y0))
        d.text((x0, y0 + 64 * scale + 4), k, fill=(0, 0, 0))
    sh.save(path)


if __name__ == '__main__':
    args = sys.argv[1:]
    sheet(D + '/sheet_faces_b2.png')
    if 'gif' in args:
        for k, f in ALL.items():
            save_gif(f, D + '/anim_%s.gif' % k, scale=4)
