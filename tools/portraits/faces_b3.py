import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))  # noqa: E702
import math  # noqa: E402

import numpy as np  # noqa: E402
from scipy import ndimage  # noqa: E402

from pix import (Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, LIGHT, FRAMES,  # noqa: E402,F401
                 save_gif)

D = __import__('os').path.dirname(__import__('os').path.abspath(__file__))


# ---------------------------------------------------------------- helpers (local to this file)
def breath2(t, phase=0.0, cycles=1, amp=1):
    """breath() with its own tempo (a second creature breathing on another beat)"""
    return -int(round(amp * (0.5 - 0.5 * math.cos(2 * math.pi * (t * cycles + phase)))))


def col(c):
    return hexrgb(c) if isinstance(c, str) else tuple(c)


def behind(p, mask, c, k=1.0):
    """paint only where nothing is drawn yet (after the outline: light, ghosts, wisps behind the figure)"""
    m = mask & ~p.a
    bg = np.array(getattr(p, 'backdrop', (0, 0, 0)), float)
    p.rgb[m] = (bg * (1 - k) + np.array(col(c), float) * k).astype(np.uint8)
    p.a |= m


def tint(p, mask, c, k):
    """blend a colour over what is drawn (glow wash)"""
    m = mask & p.a
    p.rgb[m] = (p.rgb[m] * (1 - k) + np.array(col(c), float) * k).astype(np.uint8)


def arc(cx, cy, rx, ry, a0, a1, n=24):
    """points on an ellipse arc, angles in degrees (0 = right, 90 = down)"""
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def blobs(p, pts, r0, r1):
    """a tapering limb: the union of discs along a polyline, radius r0 -> r1"""
    m = np.zeros((p.h, p.w), bool)
    seg = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        L = max(1, int(math.hypot(x1 - x0, y1 - y0) * 2))
        seg += [(x0 + (x1 - x0) * i / L, y0 + (y1 - y0) * i / L) for i in range(L)]
    seg.append(pts[-1])
    for i, (x, y) in enumerate(seg):
        r = r0 + (r1 - r0) * i / max(1, len(seg) - 1)
        m |= p.ellipse(x, y, r, r)
    return m


_NOISE = {}


def noise(seed, sigma=2.5, size=96):
    if (seed, sigma) not in _NOISE:
        rng = np.random.default_rng(seed)
        n = ndimage.gaussian_filter(rng.random((size, size)), sigma, mode='wrap')
        n = (n - n.min()) / (n.max() - n.min()) - 0.5
        _NOISE[(seed, sigma)] = n
    return _NOISE[(seed, sigma)]


def cloud(p, mask, ramp_, t=0.0, radius=6.0, bias=0.0, amp=0.9, sigma=2.5, seed=1, churn=0.5, edge=False, levels=None):
    """like Portrait.fill, but the lighting is broken up by soft cloudy noise that slowly churns over the loop
    (nebula, smoke, flesh). The noise is periodic in t, so the loop stays seamless."""
    if not mask.any():
        return mask
    d = ndimage.distance_transform_edt(np.pad(mask, 1))[1:-1, 1:-1]
    hgt = np.sqrt(np.clip(d / radius, 0, 1))
    gy, gx = np.gradient(hgt)
    k = 2.2
    nx, ny, nz = -gx * k, -gy * k, np.ones_like(gx)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    lam = (nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]) / ln + bias
    oy, ox = 16 - p.dy, 16 - p.dx
    a, b = math.cos(2 * math.pi * t), math.sin(2 * math.pi * t)
    n = noise(seed, sigma) * (1 - churn) + churn * (noise(seed + 1, sigma) * a + noise(seed + 2, sigma) * b)
    lam = lam + amp * n[oy:oy + p.h, ox:ox + p.w]
    lam = 0.5 + (lam - 0.5) * 1.35
    if levels is None:
        levels = np.linspace(0.2, 0.9, len(ramp_) - 1)
    idx = np.clip(np.digitize(lam, levels), 0, len(ramp_) - 1)
    cols = np.array(ramp_, np.uint8)[idx]
    p.rgb[mask] = cols[mask]
    p.a |= mask
    if edge:
        rim = mask & ~ndimage.binary_erosion(mask)
        p.rgb[rim] = np.array(ramp_[0], np.uint8)
    return mask


def twinkle(p, stars, t, cycles=2, cols=('#5a3a9a', '#d8a0ff', '#ffffff'), cross=True):
    """stars that brighten and fade on their own phases; the brightest get a small cross"""
    for i, (x, y) in enumerate(stars):
        k = pulse(t, i * 0.137 + (x * 0.031), cycles)
        if k < 0.3:
            c = cols[0]
        elif k < 0.75:
            c = cols[1]
        else:
            c = cols[2]
        p.px(x, y, c)
        if cross and k > 0.9 and i % 2 == 0:
            for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p.px(x + ddx, y + ddy, cols[1])


# ---------------------------------------------------------------- 1. The Empyrean
def empyrean(t=0.0):
    """a tall being of violet-pink nebula light behind a smooth pearl mask with a void window (the emblem's
    half-disc). Moves: the nebula churns, stars twinkle, wisps drift, the mask's light pulses (and two faint,
    narrow eyes surface in the void at its peak)."""
    p = Portrait()
    p.backdrop = (16, 6, 34)
    neb = [hexrgb('#1c0a3a'), hexrgb('#3a1670'), hexrgb('#5e24a8'), hexrgb('#8a36c8'), hexrgb('#c450d8'),
           hexrgb('#f08ae8')]
    pearl = ramp('#cdc4ec', spread=0.55, hue_shift=0.03)
    void = ramp('#1a0c3a', spread=0.7)
    ring = ramp('#b8b4d8', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    gk = pulse(t, 0.0, 1)
    with p.at(0, head):
        # a faint silver ring behind the head (the emblem's circle)
        rm = p.ellipse(32, 23, 25, 25) & ~p.ellipse(32, 23, 23, 23)
        p.fill(rm, ring, radius=1.5, bias=-0.1)
        # the tall cowl of nebula light
        cowl = p.poly([(12, 50), (11, 32), (15, 16), (23, 5), (32, -4), (41, 5), (49, 16), (53, 32), (52, 50)])
        cloud(p, cowl, neb, t, radius=7, bias=-0.12, amp=0.75, sigma=2.2, seed=31, edge=True, churn=0.35)
    with p.at(0, body):
        sh = p.poly([(0, 66), (0, 54), (9, 46), (21, 43), (32, 47), (43, 43), (55, 46), (64, 54), (64, 66)])
        cloud(p, sh, neb, t, radius=9, bias=-0.05, amp=0.85, sigma=2.4, seed=41, edge=True, churn=0.35)
        # a pearl torc at the throat
        p.stroke(arc(32, 41, 12, 7, 20, 160), pearl[3], width=2)
        p.stroke(arc(32, 41, 12, 7, 30, 150), pearl[4])
        p.fill(p.poly([(29, 47), (35, 47), (32, 52)]), pearl, radius=2)
        twinkle(p, [(6, 58), (14, 52), (19, 60), (27, 56), (40, 57), (47, 52), (55, 60), (60, 55), (10, 63), (35, 62),
                    (50, 63)], t)
    with p.at(0, head):
        # the neck: the void with stars
        p.fill(p.poly([(27, 34), (37, 34), (38, 46), (32, 49), (26, 46)]), void, radius=3, bias=-0.1)
        twinkle(p, [(16, 30), (20, 14), (26, 2), (38, 3), (45, 14), (48, 30), (14, 42), (50, 42), (32, -1), (18, 24),
                    (46, 23), (30, 42), (34, 45), (15, 36), (49, 37)], t, cycles=2)
        # light gathers in the cowl around the mask, breathing with the mask's glow
        halo = p.ellipse(32, 20, 15, 19) & ~p.ellipse(32, 20, 11, 15) & cowl
        tint(p, halo, '#ff7ae8', 0.12 + 0.22 * gk)
        # the mask: smooth, tall, wide at the brow and tapering to a point
        mk = p.ellipse(32, 18, 10, 14) | p.poly([(22.5, 22), (41.5, 22), (37, 33), (32, 39), (27, 33)])
        p.fill(mk, pearl, radius=7)
        p.stroke([(32, 6), (32, 9)], pearl[4])
        # the emblem's triangle, a facet in the mask that warms with the glow
        tri = p.poly([(24, 21), (40, 21), (32, 37)]) & mk
        tint(p, tri, '#9a4ae0', 0.16 + 0.2 * gk)
        p.stroke([(24, 22), (32, 37)], pearl[4])
        p.stroke([(40, 22), (32, 37)], mix(pearl[1], '#c060f0', gk))
        p.stroke([(32, 24), (32, 33)], mix(pearl[2], '#e8a0ff', gk))
        # the void window at eye level, a half-disc like the emblem's
        win = p.poly([(23, 15)] + arc(32, 15, 9, 6.5, 0, 180)[::-1] + [(41, 15)])
        p.fill(win, void, radius=2.5, bias=-0.15)
        p.pixels([(27, 19), (38, 18), (34, 20), (30, 17)], '#6a4ac0')
        # the window's lip of light pulses; at its peak two narrow eyes surface in the dark
        p.stroke([(23, 15), (41, 15)], mix(pearl[3], '#ffc8ff', gk))
        p.stroke([(24, 14), (40, 14)], pearl[4])
        if gk > 0.3:
            ec = mix('#4a2280', '#ff5ae8', min(1, (gk - 0.3) / 0.55))
            p.pixels([(26, 16), (27, 16), (28, 17)], ec)
            p.pixels([(38, 16), (37, 16), (36, 17)], ec)
            if gk > 0.8:
                p.pixels([(27, 16), (37, 16)], '#ffe8ff')
        # a violet gem on the brow
        p.fill(p.poly([(29.5, 7), (34.5, 7), (32, 11.5)]), ramp('#9a46e0', spread=0.6), radius=2)
        p.glow(32, 8, '#ffe8ff', '#d070ff', r=0, strength=0.4 + 0.5 * gk)
    p.rim('#ff5ae0', side=1, amount=0.5)
    p.rim('#5ad8ff', side=-1, amount=0.28)
    p.outline('#0a0418')
    # smoky wisps of nebula curling up from the shoulders, behind the figure
    for side, (x0, y0), ph, ln in ((-1, (10, 46), 0.0, 22), (-1, (13, 30), 0.45, 16), (1, (54, 46), 0.2, 22),
                                   (1, (51, 30), 0.7, 16)):
        pts = []
        for i in range(13):
            u = i / 12
            x = x0 + side * (u * 6 + 2.2 * u * wave(t, ph + u * 0.6)) + side * 1
            y = y0 - u * ln
            pts.append((x, y + head))
        core = p.line_mask(pts, 1)
        m2 = ndimage.binary_dilation(core)
        behind(p, core & (p.yy > y0 - ln * 0.75 + head), '#e070e8', 0.5)
        behind(p, core, '#b050d8', 0.35)
        behind(p, m2, '#6a2aa8', 0.22)
    # star motes rising
    for x0, ph in ((5, 0.0), (58, 0.33), (9, 0.6), (55, 0.85)):
        u = (t + ph) % 1.0
        y = int(round(56 - u * 50))
        m = np.zeros((p.h, p.w), bool)
        x = x0 + sway(t, ph, 1)
        if 0 <= y < p.h:
            m[y, x] = True
        behind(p, m, '#ffd8ff', 0.9 * (1 - abs(u - 0.5) * 1.6))
    return p


# ---------------------------------------------------------------- 2. The Naaz-Rokha Alliance
def naaz(t=0.0):
    """a heavy Rokha warrior (horned brow, tusks, green scales, steel armour) with a little Naaz engineer
    on its right shoulder (huge eyes, brass goggles, a wrench). Moves: the Rokha breathes slowly, the Naaz
    twice as fast on its own beat, the Naaz looks around, its goggles glint."""
    p = Portrait()
    p.backdrop = (14, 30, 12)
    sc = ramp('#3f8a52', spread=0.78)
    belly = ramp('#b6bf72', spread=0.55)
    horn = ramp('#d6c49a', spread=0.55)
    steel = ramp('#5f6a7c', spread=0.75)
    enam = ramp('#3fcf6c', spread=0.5)
    nskin = ramp('#e8a04a', spread=0.6)
    leather = ramp('#7a4a2c', spread=0.6)
    brass = ramp('#d8a838', spread=0.5)
    body, head = breath(t), breath(t, -0.12)
    nb = breath2(t, 0.3, cycles=2)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 48), (10, 41), (22, 40), (32, 44), (44, 40), (56, 40), (64, 46), (64, 66)]), steel,
               radius=8, texture=0.05)
        p.stroke([(14, 56), (24, 52), (34, 53), (42, 58)], steel[1])
        p.stroke([(18, 62), (28, 58), (38, 60)], steel[1])
        # left pauldron: domed, rivets, enamel stripe
        p.fill(p.ellipse(9, 50, 12, 10), steel, radius=6, texture=0.04)
        p.stroke(arc(9, 50, 9, 7, 200, 330), enam[3], width=2)
        for x, y in ((3, 55), (9, 58), (15, 56)):
            p.px(x, y, steel[4])
        # right pauldron: a flat ledge (the Naaz's seat)
        p.fill(p.poly([(38, 46), (42, 41), (60, 39), (66, 42), (66, 60), (40, 60)]), steel, radius=5, texture=0.04,
               bias=-0.05)
        p.stroke([(41, 44), (60, 41), (65, 43)], steel[4])
        p.stroke([(42, 47), (64, 45)], enam[2], width=2)
        # the alliance's sign stamped on the breastplate: the chevron and the small moon
        p.fill(p.poly([(22, 61), (27, 51), (32, 61), (29.5, 61), (27, 56), (24.5, 61)]), enam, radius=1.5)
        p.fill(p.ellipse(32, 52.5, 2, 2), enam, radius=1.2)
        # gorget
        p.fill(p.poly([(14, 42), (20, 36), (38, 36), (43, 42), (38, 49), (19, 49)]), steel, radius=4, bias=0.05)
        p.stroke([(18, 45), (28, 47), (39, 45)], steel[1])
    with p.at(0, head):
        # thick horns curving out from the temples, then up
        p.fill(p.poly([(16, 20), (10, 19), (5, 16), (2, 11), (2, 4), (4, 1), (5, 6), (7, 11), (11, 13), (17, 14)]), horn,
               radius=2.5, texture=0.06)
        p.fill(p.poly([(40, 20), (46, 19), (51, 16), (53, 11), (53, 4), (51, 1), (50, 6), (48, 11), (44, 13), (39, 14)]),
               horn, radius=2.5, bias=-0.1, texture=0.06)
        for x, y in ((4, 9), (6, 13), (9, 16), (51, 9), (49, 13), (46, 16)):
            p.px(x, y, horn[1])
        # the head: a broad reptile skull, wider at the jaw
        hd = p.poly([(14, 22), (16, 14), (23, 9), (33, 9), (40, 14), (42, 22), (44, 30), (42, 38), (36, 43), (20, 43),
                     (14, 38), (12, 30)])
        p.fill(hd, sc, radius=8, texture=0.12, seed=51)
        # bony scutes along the crown
        for x, y in ((24, 9), (28, 8), (32, 9)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 1.8, 1.6), horn, radius=1)
        for x, y in ((20, 13), (35, 13), (17, 30), (39, 30), (15, 27), (41, 26), (19, 34), (37, 34)):
            p.px(x, y, sc[4] if x < 28 else sc[3])
            p.px(x + 1, y + 1, sc[1])
        # heavy brow plates angled down to the snout: a permanent scowl
        p.fill(p.poly([(13, 21), (22, 17), (28, 21), (27, 24), (21, 23), (14, 25)]), sc, radius=2, bias=0.18)
        p.fill(p.poly([(43, 21), (34, 17), (28, 21), (29, 24), (35, 23), (42, 25)]), sc, radius=2, bias=0.05)
        p.pixels([(16, 19), (20, 17), (36, 17), (40, 19)], horn[3])
        # small, deep-set eyes, reptile slits
        for cx in (19, 36):
            p.fill(p.ellipse(cx + 0.5, 25.5, 3.2, 1.8), sc, flat=0, edge=False)
            p.pixels([(cx - 1, 25), (cx, 25), (cx + 1, 25), (cx + 2, 25)], '#ffb418')
            p.pixels([(cx, 26), (cx + 1, 26)], '#c06008')
            p.px(cx - 1 if cx < 28 else cx + 2, 25, '#fff0a0')
            p.px(cx + 1 if cx < 28 else cx, 25, '#2a1008')
            p.glow(cx + 0.5 if False else cx, 25, '#fff0a0', '#ff9a18', r=0, strength=0.3)
            p.px(cx + 1 if cx < 28 else cx, 25, '#2a1008')
        # a broad snout with nostril slits
        p.fill(p.ellipse(28, 31, 8.5, 5.5), sc, radius=4, bias=0.1, texture=0.1, seed=52, edge=False)
        p.stroke([(28, 24), (28, 28)], sc[3])
        p.pixels([(24, 30), (25, 31), (32, 31), (31, 30)], '#10200c')
        # the lower jaw, pale and plated
        p.fill(p.poly([(16, 37), (27, 39), (40, 37), (37, 43), (28, 45), (19, 43)]), belly, radius=2, texture=0.1, seed=54)
        p.stroke([(20, 41), (28, 43), (36, 41)], belly[1])
        # a wide, grim mouth with fangs
        p.stroke([(15, 36), (20, 37), (36, 37), (41, 36)], '#10180c')
        p.pixels([(19, 38), (23, 38), (33, 38), (37, 38)], '#f0e8d0')
        p.pixels([(19, 39), (37, 39)], '#c8b890')
        # tusks rising from the jaw
        ivory = ramp('#f2ead4', spread=0.45)
        p.fill(p.poly([(19, 44), (15, 39), (13, 33), (12, 29), (15, 32), (18, 37), (22, 42)]), ivory, radius=1.8)
        p.fill(p.poly([(37, 44), (41, 39), (43, 33), (44, 29), (41, 32), (38, 37), (34, 42)]), ivory, radius=1.8,
               bias=-0.1)
        # a war-scar through the brow
        p.stroke([(37, 20), (40, 28)], sc[0])
        p.pixels([(38, 22), (39, 25)], '#d08a7a')
    # the Naaz engineer on the ledge
    look = [0] * 5 + [1] * 6 + [0] * 2 + [-1] * 7 + [0] * 4
    lk = look[int(t * FRAMES) % FRAMES]
    with p.at(0, body):
        with p.at(0, nb):
            # legs dangling over the ledge
            p.fill(p.poly([(47, 39), (50, 39), (49, 46), (46, 46)]), nskin, radius=1.5)
            p.fill(p.poly([(53, 39), (56, 39), (57, 46), (54, 46)]), nskin, radius=1.5, bias=-0.1)
            # a small body in a green work vest with a tool belt
            p.fill(p.ellipse(51.5, 35, 6, 6), enam, radius=3, bias=-0.1)
            p.stroke([(46, 39), (57, 39)], leather[2])
            p.pixels([(49, 39), (53, 39)], brass[3])
            # an arm raising the wrench
            with p.at(lk, 0):
                # the big head, frilled ears
                p.fill(p.poly([(45, 22), (40, 17), (43, 17), (47, 19)]), nskin, radius=1.5)
                p.fill(p.poly([(58, 22), (63, 17), (60, 17), (56, 19)]), nskin, radius=1.5, bias=-0.1)
                p.fill(p.ellipse(51.5, 24.5, 7, 6.5), nskin, radius=4, texture=0.06, seed=53)
                # goggles pushed up on the forehead
                p.stroke([(45, 20), (58, 19)], leather[1])
                for gx in (48.5, 54.5):
                    p.fill(p.ellipse(gx, 19.5, 2.6, 2.4), brass, radius=1.5)
                    p.fill(p.ellipse(gx, 19.5, 1.4, 1.3), ramp('#38e0e0', spread=0.4), flat=2, edge=False)
                f = int(t * FRAMES) % FRAMES
                glint = {9: [(47, 19)], 10: [(48, 19), (54, 19)], 11: [(49, 20), (55, 19)], 12: [(56, 20)]}
                p.pixels(glint.get(f, []), '#ffffff')
                # huge eyes, the pupils follow the look
                for ex in (48.5, 54.5):
                    p.fill(p.ellipse(ex, 25.5, 2.5, 2.6), ramp('#f4f0d8', spread=0.3), radius=1.5, bias=0.1)
                    px0 = int(ex - 0.5) + lk
                    p.pixels([(px0, 25), (px0 + 1, 25), (px0, 26), (px0 + 1, 26)], '#1a1020')
                    p.px(px0, 25, '#ffffff')
                # a pleased little grin
                p.stroke([(50, 29), (51, 30), (53, 30), (54, 29)], nskin[0])
            # an arm raising the wrench high, in front of the ear
            p.stroke([(56, 33), (59, 29)], nskin[2], width=2)
            wr = ramp('#b8c4d0', spread=0.5)
            p.fill(p.poly([(58, 31), (60, 15), (62, 15), (60, 31)]), wr, radius=1)
            p.fill(p.poly([(58, 16), (58, 11), (60, 9), (60, 13), (62, 13), (62, 9), (64, 11), (64, 16)]), wr,
                   radius=1.2)
            p.fill(p.ellipse(59.5, 28.5, 1.8, 1.8), nskin, radius=1)
    p.rim('#7aff8a', side=1, amount=0.45)
    p.rim('#ffc44a', side=-1, amount=0.25)
    p.outline('#081008')
    return p


# ---------------------------------------------------------------- 3. The Nomad
def nomad(t=0.0):
    """a lone hero in a hooded travel coat and a long mauve scarf, weathered and set-jawed, a temporal
    device (teal core, amber orbits - the emblem) on the chest strap, after-images trailing behind him.
    Moves: the scarf tail sways, the device pulses and its orbits turn, the after-image flickers."""
    p = Portrait()
    p.backdrop = (26, 16, 32)
    coat = ramp('#3c5c6a', spread=0.75)
    lining = ramp('#1e2a36', spread=0.6)
    scarf = ramp('#b25a72', spread=0.65)
    skin = ramp('#c48a68', spread=0.6)
    stub = ramp('#6a4a40', spread=0.5)
    leather = ramp('#6a4630', spread=0.6)
    brass = ramp('#d8a040', spread=0.55)
    teal = ramp('#3ae0d0', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    dk = pulse(t, 0.0, 2)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (8, 44), (20, 41), (30, 44), (40, 42), (52, 44), (62, 50), (64, 66)]), coat,
               radius=8, texture=0.06, seed=61)
        p.stroke([(10, 50), (14, 60), (13, 66)], coat[1])
        p.stroke([(46, 50), (50, 58), (54, 66)], coat[1])
        # the coat's open front and a chest strap
        p.fill(p.poly([(26, 47), (36, 47), (40, 66), (22, 66)]), lining, radius=3)
        p.stroke([(46, 44), (32, 54), (18, 66)], leather[2], width=3)
        p.stroke([(45, 44), (31, 54), (17, 66)], leather[3])
    with p.at(0, head):
        # the hood, worn and patched
        hood = p.poly([(12, 46), (11, 30), (14, 17), (21, 7), (31, 2), (41, 4), (48, 11), (52, 22), (53, 34), (50, 44)])
        p.fill(hood, coat, radius=7, texture=0.07, seed=62)
        p.stroke([(16, 22), (14, 34), (15, 44)], coat[1])
        p.stroke([(19, 12), (24, 7)], coat[4])
        p.stroke([(21, 9), (18, 15), (17, 21)], coat[3])
        p.stroke([(31, 3), (27, 8), (24, 14)], coat[1])
        p.stroke([(16, 28), (19, 36)], coat[0])
        p.pixels([(16, 30), (18, 29), (17, 32), (19, 31), (18, 34), (20, 33)], '#b8a070')
        # the dark opening, turned to the right, with a turned-back edge
        p.fill(p.poly([(26, 14), (33, 8), (43, 9), (49, 15), (52, 25), (52, 38), (46, 45), (30, 45), (25, 30)]), lining,
               radius=3, bias=-0.2)
        p.stroke([(25, 31), (25, 18), (29, 11), (35, 7), (43, 8)], coat[4])
        p.stroke([(26, 32), (26, 19), (30, 12)], coat[2])
        # the face: weathered, 3/4 to the right
        face = p.poly([(30, 20), (34, 14), (41, 12), (47, 13), (50, 17), (51, 24), (51, 33), (49, 38), (44, 42),
                       (38, 43), (33, 40), (30, 34), (29, 27)])
        p.fill(face, skin, radius=5, texture=0.05, seed=63)
        # the far side of the face in shadow, a cheekbone catching the light
        p.fill(p.poly([(46, 26), (49, 21), (51, 24), (51, 33), (49, 38), (46, 39), (47, 33)]), skin, flat=1, edge=False)
        p.stroke([(32, 28), (35, 29)], skin[4])
        p.stroke([(33, 27), (36, 27)], skin[3])
        # hair: a dark fringe under the hood
        hair = ramp('#4a3026', spread=0.55)
        p.fill(p.poly([(29, 22), (31, 16), (36, 12), (44, 11), (50, 15), (51, 20), (48, 17), (45, 18), (42, 16),
                       (38, 18), (34, 17), (31, 22)]), hair, radius=1.5, texture=0.2, seed=67)
        p.pixels([(33, 18), (39, 17), (46, 17)], hair[0])
        # brows low and hard, sockets in shadow
        p.stroke([(32, 21), (35, 20), (39, 21)], hair[1])
        p.stroke([(44, 21), (48, 20)], hair[1])
        p.stroke([(33, 22), (38, 22)], skin[1])
        p.stroke([(44, 22), (47, 22)], skin[0])
        # narrowed eyes with a teal glint (time looks back out of them)
        p.pixels([(33, 23), (34, 23), (35, 23), (36, 23), (37, 23)], '#e8dccc')
        p.pixels([(35, 23), (36, 23)], '#1e4a50')
        p.px(36, 23, teal[3])
        p.pixels([(44, 23), (45, 23), (46, 23)], '#d8c8b8')
        p.px(45, 23, '#1e4a50')
        p.stroke([(33, 24), (37, 24)], skin[1])
        p.px(38, 24, skin[1])
        p.px(37, 25, skin[2])
        # the nose: lit bridge, shadowed far side, a hooked tip
        p.stroke([(41, 22), (42, 28)], skin[3])
        p.stroke([(43, 23), (44, 29)], skin[1])
        p.pixels([(42, 29), (43, 30), (44, 30)], skin[4])
        p.pixels([(41, 31), (42, 31), (45, 30)], skin[0])
        # lines of a hard life, and the set mouth
        p.stroke([(39, 30), (38, 33)], skin[1])
        p.stroke([(39, 35), (42, 35), (46, 34)], '#3a1818')
        p.pixels([(40, 36), (41, 36), (42, 36)], skin[3])
        p.px(47, 34, skin[1])
        # stubble along the jaw
        p.fill(p.poly([(30, 33), (34, 37), (39, 38), (46, 37), (50, 34), (49, 38), (44, 42), (38, 43), (33, 40)]), stub,
               radius=2, texture=0.3, seed=64, edge=False)
        # a scar down the cheek
        p.stroke([(34, 26), (36, 32)], '#8a4a44')
        p.pixels([(33, 25), (35, 30)], skin[4])
        # the scarf, wrapped high around the throat
        sc = p.poly([(25, 42), (33, 38), (40, 40), (48, 38), (53, 42), (52, 50), (40, 53), (28, 52), (23, 48)])
        p.fill(sc, scarf, radius=4, texture=0.08, seed=65)
        p.stroke([(27, 45), (36, 44), (44, 42), (51, 44)], scarf[1])
        p.stroke([(28, 49), (38, 49), (49, 47)], scarf[1])
        p.stroke([(30, 42), (39, 41)], scarf[4])
    # the scarf's tail streams out to the right, swaying (behind the shoulder: drawn under nothing - so before?)
    with p.at(0, body):
        s1, s2 = sway(t, 0.0, 1), sway(t, 0.2, 2)
        p.fill(p.poly([(47, 43), (53, 42), (58, 44 + s1), (66, 47 + s2), (66, 55 + s2), (60, 53 + s1), (54, 52),
                       (48, 51)]), scarf, radius=3, texture=0.08, seed=66, bias=-0.05)
        p.stroke([(50, 47), (57, 48 + s1), (65, 51 + s2)], scarf[1])
        p.stroke([(52, 44), (58, 45 + s1), (64, 48 + s2)], scarf[4])
        # the temporal device on the strap
        p.fill(p.ellipse(28.5, 56.5, 5.5, 5.5), brass, radius=3)
        p.fill(p.ellipse(28.5, 56.5, 3.4, 3.4), teal, radius=2, bias=0.1 + 0.25 * dk)
        p.glow(28, 56, '#ffffff', mix('#3ae0d0', '#b8fff4', dk), r=1, strength=0.5 + 0.5 * dk)
        # its light on the scarf and coat
        near = np.hypot(p.xx - 28.5, p.yy - 56.5 - body) < 10
        tint(p, near & ~(np.hypot(p.xx - 28.5, p.yy - 56.5 - body) < 6), '#3ae0d0', 0.08 + 0.12 * dk)
        for k in range(3):
            a = 2 * math.pi * (t + k / 3)
            p.glow(int(round(28 + 6.5 * math.cos(a))), int(round(56 + 6.5 * math.sin(a))), '#fff0b0', '#ffa030',
                   r=0, strength=0.7)
    p.rim('#3ae0d0', side=1, amount=0.45)
    p.rim('#ff9a3a', side=-1, amount=0.25)
    p.outline('#0a0a12')
    # after-images: the silhouette echoes to the left (and, on a glitch beat, a far one to the right)
    sil = p.a.copy()
    gl = blink(t, 0.45, 2) or blink(t, 0.9, 1)
    off = -3 if not gl else -6
    def echo(dx, c, k_fill, k_edge):
        e = np.roll(sil, dx, axis=1)
        e &= (p.xx < 64 + dx) if dx < 0 else (p.xx >= dx)
        edge = e & ~ndimage.binary_erosion(e)
        behind(p, edge, c, k_edge)
        behind(p, e, c, k_fill)
    echo(off, '#3ae0d0', 0.14 + 0.06 * dk, 0.38 + 0.15 * dk)
    if gl:
        echo(3, '#ffa030', 0.12, 0.35)
    else:
        echo(off - 3, '#b25a72', 0.06, 0.18)
    return p


# ---------------------------------------------------------------- 4. The Titans of Ul
def titans(t=0.0):
    """a colossal head of dark stone under a bronze headdress, a squared braided beard, half-closed eyes of
    gold fire, runes and cracks of light, the emblem's pink orb on the brow. Moves: one very slow breath,
    the runes pulse in a slow wave, dust drifts down in the light."""
    p = Portrait()
    p.backdrop = (40, 12, 32)
    stone = ramp('#4c4452', spread=0.7)
    bronze = ramp('#b47c3a', spread=0.68, hue_shift=0.04)
    dbronze = ramp('#6a4428', spread=0.6)
    orb = ramp('#ff4fae', spread=0.5)
    body, head = breath(t), breath(t, -0.15)

    def rune(x, y, kind, ph):
        k = pulse(t, ph, 1)
        c = mix('#b05a10', '#ffc040', k)
        hi = mix('#ffb040', '#fff4c0', k)
        glyphs = {0: [(0, 0), (0, 1), (0, 2), (1, 1)], 1: [(0, 0), (1, 0), (1, 1), (1, 2), (0, 2)],
                  2: [(0, 0), (1, 1), (0, 2), (2, 0)], 3: [(1, 0), (0, 1), (1, 1), (2, 1), (1, 2)],
                  4: [(0, 0), (2, 0), (1, 1), (1, 2)]}
        pts = [(x + a_, y + b_) for a_, b_ in glyphs[kind % 5]]
        p.pixels(pts, c)
        p.px(*pts[0], hi)

    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 44), (10, 38), (22, 40), (32, 46), (42, 40), (54, 38), (64, 44), (64, 66)]), stone,
               radius=9, texture=0.14, seed=71)
        # bronze pauldrons with rune rows
        p.fill(p.poly([(0, 47), (9, 41), (19, 43), (21, 52), (11, 57), (0, 58)]), bronze, radius=4, texture=0.05)
        p.fill(p.poly([(64, 47), (55, 41), (45, 43), (43, 52), (53, 57), (64, 58)]), bronze, radius=4, bias=-0.1,
               texture=0.05)
        p.stroke([(1, 52), (10, 48), (19, 49)], dbronze[1])
        p.stroke([(63, 52), (54, 48), (45, 49)], dbronze[0])
        for i, x in enumerate((3, 8, 13)):
            rune(x, 53 - i, i, 0.1 * i + 0.5)
        for i, x in enumerate((48, 53, 58)):
            rune(x, 51 + i, i + 2, 0.1 * i + 0.8)
        # a crack in the stone shoulder
        p.stroke([(24, 62), (27, 57), (25, 53)], mix('#c06010', '#ffb040', pulse(t, 0.3)))
    with p.at(0, head):
        # the headdress: flared bronze plates framing the face
        p.fill(p.poly([(15, 14), (9, 36), (8, 48), (19, 50), (20, 30)]), bronze, radius=4, texture=0.05, seed=72)
        p.fill(p.poly([(49, 14), (55, 36), (56, 48), (45, 50), (44, 30)]), bronze, radius=4, bias=-0.12, texture=0.05,
               seed=73)
        for y in (24, 31, 38, 45):
            p.stroke([(10 + (y - 24) // 10, y + 2), (19, y - 1)], dbronze[1])
            p.stroke([(54 - (y - 24) // 10, y + 2), (45, y - 1)], dbronze[0])
        # the great stone face
        hd = p.poly([(16, 14), (21, 6), (32, 4), (43, 6), (48, 14), (48, 30), (45, 40), (39, 46), (32, 48), (25, 46),
                     (19, 40), (16, 30)])
        p.fill(hd, stone, radius=9, texture=0.12, seed=74)
        # the crown band with the orb of the emblem
        p.fill(p.poly([(15, 15), (17, 7), (24, 2), (32, 0), (40, 2), (47, 7), (49, 15), (40, 13), (32, 12), (24, 13)]),
               bronze, radius=3, texture=0.05)
        p.stroke([(17, 13), (24, 11), (32, 10), (40, 11), (47, 13)], dbronze[1])
        p.fill(p.ellipse(32, 6.5, 3.6, 3.6), orb, radius=2.5)
        p.glow(31, 5, '#fff0fa', '#ff9ad8', r=0, strength=0.5 + 0.3 * pulse(t, 0.5))
        p.stroke(arc(32, 6.5, 5.5, 5, 120, 240), dbronze[0])
        p.stroke(arc(32, 6.5, 5.5, 5, -60, 60), dbronze[0])
        for i, x in enumerate((19, 23, 39, 43)):
            rune(x, 7 if x in (23, 39) else 9, i + 1, 0.25 + 0.06 * i)
        # a heavy, straight brow
        p.fill(p.poly([(17, 18), (31, 19), (33, 19), (47, 18), (47, 22), (33, 22), (31, 22), (17, 22)]), stone, radius=2,
               bias=0.12)
        p.stroke([(18, 22), (46, 22)], stone[0])
        # half-closed eyes of gold fire: serene
        for x0 in (21, 36):
            p.fill(p.poly([(x0 - 1, 23), (x0 + 8, 23), (x0 + 7, 26), (x0, 26)]), stone, flat=0, edge=False)
            p.stroke([(x0, 24), (x0 + 7, 24)], '#ff9a20')
            p.stroke([(x0 + 1, 24), (x0 + 6, 24)], '#ffd860')
            p.stroke([(x0 + 2, 24), (x0 + 5, 24)], '#fff6c8')
            p.stroke([(x0 + 1, 25), (x0 + 6, 25)], '#8a3a10')
        # glow spilling onto the cheeks
        tint(p, p.poly([(19, 26), (30, 26), (29, 29), (20, 29)]), '#ff9a30', 0.18)
        tint(p, p.poly([(34, 26), (45, 26), (44, 29), (35, 29)]), '#ff9a30', 0.12)
        # a broad, long nose
        p.fill(p.poly([(30, 22), (34, 22), (35, 31), (36, 33), (28, 33), (29, 31)]), stone, radius=2, bias=0.1,
               edge=False)
        p.stroke([(30, 23), (29, 31)], stone[4])
        p.stroke([(35, 24), (36, 32)], stone[0])
        p.pixels([(29, 33), (30, 33), (34, 33), (35, 33)], stone[0])
        # cheek rune columns
        for i, y in enumerate((29, 33)):
            rune(22, y, i, 0.45 + 0.08 * i)
            rune(40, y, i + 3, 0.55 + 0.08 * i)
        # a squared beard of braided stone, tapering, bound in bronze
        bd = p.poly([(23, 39), (41, 39), (41, 49), (37, 58), (27, 58), (23, 49)])
        p.fill(bd, stone, radius=4, texture=0.1, seed=75)
        for k, (x0, x1, y1) in enumerate(((23, 27, 51), (27, 31, 58), (31, 35, 58), (35, 39, 58), (39, 42, 51))):
            br = p.poly([(x0, 40), (x1, 40), (x1, y1 - 2), (x0 + (x1 - x0) / 2, y1), (x0, y1 - 2)]) & bd
            p.fill(br, stone, radius=1.6, bias=0.05 - 0.04 * k, texture=0.06, seed=90 + k)
            for y in range(43, y1 - 1, 3):
                p.px(x0 + 1, y, stone[0])
        for y, x0, x1 in ((47, 23, 41), (53, 25, 39)):
            p.fill(p.poly([(x0, y), (x1, y), (x1, y + 2), (x0, y + 2)]) & bd, bronze, radius=1)
            p.pixels([(x0 + 2, y), (x1 - 3, y)], bronze[4])
        # the moustache and the closed, carved mouth
        p.fill(p.poly([(23, 37), (28, 34), (32, 35), (36, 34), (41, 37), (42, 41), (37, 38), (32, 39), (27, 38),
                       (22, 41)]), stone, radius=2, bias=0.12, texture=0.08, seed=76)
        p.stroke([(28, 39), (36, 39)], stone[0])
        p.stroke([(29, 40), (35, 40)], stone[3])
        # cracks of light through the stone
        ck = pulse(t, 0.35, 1)
        for seg in ([(44, 15), (42, 19)], [(46, 26), (43, 30), (45, 34)], [(18, 30), (20, 34), (19, 38)]):
            p.stroke(seg, mix('#a04a10', '#ffb040', ck))
        p.pixels([(43, 30), (20, 34)], mix('#ffb040', '#fff0b0', ck))
    p.rim('#ff4fb0', side=1, amount=0.45)
    p.rim('#ffb040', side=-1, amount=0.25)
    p.outline('#140810')
    # dust motes drifting slowly down through the light
    for x0, ph, sp in ((6, 0.0, 1), (58, 0.3, 1), (13, 0.62, 1), (52, 0.8, 1), (30, 0.45, 1)):
        u = (t * sp + ph) % 1.0
        y = int(round(4 + u * 40))
        x = x0 + sway(t, ph, 1)
        m = np.zeros((p.h, p.w), bool)
        if 0 <= y < 64:
            m[y, x] = True
        behind(p, m, '#ffd890', 0.8 * math.sin(math.pi * u))
    return p


# ---------------------------------------------------------------- 5. The Vuil'raith Cabal
def cabal(t=0.0):
    """an eldritch head of crimson-purple flesh before a turning rift (the emblem's sawblade ring): many eyes
    of different colours - each its own horror, blinking on its own time - a great half-lidded eye, a
    crescent maw of needle teeth lit from within, tentacles writhing behind and hanging below. Moves:
    tentacles writhe, eyes blink at different times, the maw's glow pulses, the rift turns."""
    p = Portrait()
    p.backdrop = (26, 4, 12)
    fl = [hexrgb(c) for c in ('#14030c', '#2e0822', '#521238', '#7c1c4a', '#a8325c', '#d85c6c')]
    fl5 = [hexrgb(c) for c in ('#1a0412', '#3c0c2a', '#62163e', '#8e2650', '#c04a66')]
    mk = pulse(t, 0.0, 2)
    gl = mix('#c02010', '#ff7a2a', mk)
    body, head = breath(t), breath(t, -0.1)

    def tentacles(specs, r0):
        for i, (side, base, ph, L, up) in enumerate(specs):
            pts = []
            for j in range(9):
                u = j / 8
                x = base[0] + side * (u * L * 0.55 + 3 * u * wave(t, ph + u * 0.7))
                y = base[1] + up * u * L + 2 * u * wave(t, ph + 0.25 + u * 0.6)
                pts.append((x, y))
            m = blobs(p, pts, r0[i], 0.6)
            p.fill(m, fl5, radius=2, texture=0.08, seed=80 + i, bias=-0.05)
            for j in (3, 5, 7):
                x, y = pts[j]
                p.px(int(round(x)), int(round(y)) + (1 if up < 0 else 0), fl[5])

    with p.at(0, body):
        # tentacles rising behind the shoulders
        tentacles([(-1, (12, 46), 0.0, 22, -1), (-1, (18, 42), 0.33, 16, -1), (1, (52, 46), 0.5, 22, -1),
                   (1, (46, 42), 0.15, 16, -1)], (3.4, 2.6, 3.4, 2.6))
        # a hunched mass of flesh, eyes on the shoulders
        sh = p.poly([(0, 66), (0, 50), (12, 43), (24, 45), (32, 49), (40, 45), (52, 43), (64, 50), (64, 66)])
        cloud(p, sh, fl, t, radius=9, amp=0.6, sigma=2.2, seed=81, churn=0.3, edge=True, bias=-0.1)
        for i, (x, y) in enumerate(((7, 55), (57, 56), (13, 61), (52, 62))):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 2.2, 1.5), fl5, flat=0, edge=False)
            if blink(t, 0.2 + 0.27 * i, 2):
                p.stroke([(x - 1, y), (x + 1, y)], fl[2])
            else:
                c = ('#ffd040', '#a0ff60', '#ff70e0', '#60e0ff')[i]
                p.glow(x, y, '#1a0008', c, r=0, strength=0.7)
                p.px(x, y, '#1a0008')
    with p.at(0, head):
        # the great lumpy cranium, tapering into a split jaw
        hd = (p.ellipse(32, 17, 17, 15) | p.ellipse(20, 9, 7, 6) | p.ellipse(44, 9, 7, 6) | p.ellipse(32, 4, 8, 5)
              | p.poly([(16, 20), (48, 20), (46, 34), (39, 45), (32, 50), (25, 45), (18, 34)]))
        cloud(p, hd, fl, t, radius=9, amp=0.55, sigma=2.0, seed=83, churn=0.25, edge=True, bias=-0.1)
        for seg in ([(21, 4), (25, 8)], [(43, 4), (39, 8)], [(32, 0), (32, 4)], [(15, 15), (17, 22)], [(49, 15), (47, 22)],
                    [(25, 8), (23, 13)], [(39, 8), (41, 13)], [(20, 26), (23, 33)], [(44, 26), (41, 33)]):
            p.stroke(seg, fl[1])
        p.pixels([(19, 6), (43, 6), (30, 2), (16, 13), (21, 27)], fl[5])
        # the maw: a vertical split from brow to chin, opening onto the rift
        open_ = 4.6 + 1.2 * mk
        lips = [(32 - open_ * math.sin(math.pi * u) ** 0.8 - 1.2, 20 + 29 * u) for u in np.linspace(0, 1, 16)]
        lips += [(32 + open_ * math.sin(math.pi * u) ** 0.8 + 1.2, 20 + 29 * u) for u in np.linspace(1, 0, 16)]
        rimm = p.poly(lips)
        p.fill(rimm, fl, radius=1.5, bias=0.25, edge=False)
        inner = [(32 - (open_ - 1) * math.sin(math.pi * u) ** 0.8, 21 + 27 * u) for u in np.linspace(0, 1, 16)]
        inner += [(32 + (open_ - 1) * math.sin(math.pi * u) ** 0.8, 21 + 27 * u) for u in np.linspace(1, 0, 16)]
        maw = p.poly(inner)
        p.fill(maw, [hexrgb('#0c0104'), hexrgb('#2a0306'), hexrgb('#5a0808'), hexrgb('#8a1010'), gl], radius=3,
               bias=-0.55, edge=False)
        # the rift burning in the throat
        for y in range(27, 44):
            w = 1.6 * math.sin(math.pi * (y - 26) / 18)
            for x in range(29, 36):
                d = abs(x + 0.5 - 32)
                if d < w + 0.3:
                    p.px(x, y, mix(gl, '#ffe0a0', 1 - d / (w + 0.3)))
                elif d < w + 1.3:
                    p.px(x, y, mix('#5a0808', gl, 0.6))
        p.glow(32, 35, '#ffffff', '#ffc070', r=1, strength=0.3 + 0.5 * mk)
        # needle teeth lining both edges, pointing in
        for i, y in enumerate(range(23, 47, 3)):
            u = (y - 20) / 29
            e = (open_ - 0.5) * math.sin(math.pi * u) ** 0.8
            L = (2, 3, 3, 2, 3)[i % 5]
            if e < 1.5:
                continue
            for k in range(L):
                p.px(int(round(32 - e)) + k, y, '#e8d4b0' if k < L - 1 else '#9a7a60')
            for k in range(L if i % 2 else L - 1):
                p.px(int(round(32 + e)) - k, y + 1, '#d8c4a0' if k < L - 1 else '#8a6a50')
        # the rift lights the face from within
        lit = hd & ~maw & (np.hypot((p.xx - 32) * 1.2, p.yy - 35 - head) < 16)
        tint(p, lit, gl, 0.1 + 0.12 * mk)
        # eyes in a crown above the maw, each its own colour and its own time; the great one half-lidded
        p.fill(p.ellipse(32, 14, 6.5, 3.6), [hexrgb('#1a0408')] * 5, flat=0, edge=False)
        if not blink(t, 0.55, 2):
            p.fill(p.ellipse(32, 14.5, 5.5, 2.8), ramp('#ffb020', spread=0.5), radius=2, edge=False, bias=0.1)
            p.stroke([(32, 13), (32, 16)], '#1a0408')
            p.px(29, 14, '#fff6d0')
            p.fill(p.poly([(25, 13), (28, 10), (36, 10), (39, 13), (36, 13), (32, 12), (28, 13)]), fl5, radius=1.5)
            p.glow(32, 15, '#ffe080', '#ffa020', r=1, strength=0.25)
        else:
            p.fill(p.ellipse(32, 13.5, 6.5, 3.2), fl5, radius=2)
            p.stroke([(26, 15), (32, 16), (38, 15)], fl[0])
        p.stroke([(25, 10), (28, 8), (36, 8), (39, 10)], fl[5])
        A, M, G = '#ffc030', '#ff4ad0', '#b0ff40'
        eyes = [((21, 9), 3, A, 0.1), ((43, 9), 3, M, 0.35), ((17, 17), 2, M, 0.7), ((47, 17), 2, A, 0.85),
                ((26, 3), 2, G, 0.22), ((38, 3), 2, A, 0.6), ((21, 22), 2, A, 0.45), ((43, 22), 2, G, 0.95),
                ((32, 5), 1, M, 0.78), ((15, 25), 1, G, 0.05), ((49, 25), 1, M, 0.3)]
        for (x, y), r, c, at in eyes:
            p.fill(p.ellipse(x + 0.5, y + 0.5, r + 1.2, 1.9 if r > 1 else 1.5), [hexrgb('#140208')] * 5, flat=0,
                   edge=False)
            if blink(t, at, 2):
                p.stroke([(x - r, y), (x + r, y)], fl[3])
                continue
            p.glow(x, y, '#ffffff', c, r=0, strength=0.35)
            p.stroke([(x - r, y), (x + r, y)], c)
            if r > 1:
                p.stroke([(x - r + 1, y - 1), (x + r - 1, y - 1)], mix(c, '#ffffff', 0.4))
                p.px(x, y + 1, mix(c, '#000000', 0.4))
            p.px(x, y, '#140208')
            if r > 2:
                p.px(x, y - 1, '#140208')
        # facial tentacles hanging beside the maw
        tentacles([(-1, (23, 38), 0.6, 18, 1), (1, (41, 38), 0.1, 18, 1), (-1, (27, 46), 0.35, 14, 1),
                   (1, (37, 46), 0.85, 14, 1)], (3.4, 3.4, 2.8, 2.8))
    p.rim('#ff3fa0', side=1, amount=0.5)
    p.rim('#ff8a2a', side=-1, amount=0.35)
    p.outline('#0e0208')
    # the rift behind: a spiral ring of teeth turning one tooth per loop (light, drawn behind the figure)
    cx, cy = 32, 24
    ang = np.arctan2(p.yy + 0.5 - cy, p.xx + 0.5 - cx)
    rr = np.hypot(p.xx + 0.5 - cx, p.yy + 0.5 - cy)
    saw = ((ang / (2 * np.pi)) * 9 + t) % 1.0
    ring_ = (rr < 26 + 4 * saw) & (rr > 25)
    behind(p, (rr > 25) & (rr < 26.2), '#ff9a50', 0.6 + 0.3 * mk)
    behind(p, ring_ & (saw > 0.5), '#ff4a2a', 0.5)
    behind(p, ring_, '#d01830', 0.35)
    behind(p, (rr <= 25) & (rr > 22), '#ff4a2a', 0.1 + 0.08 * mk)
    # motes drawn into the rift
    for i, ph in enumerate((0.0, 0.27, 0.55, 0.8)):
        u = (t + ph) % 1.0
        a_ = 2 * math.pi * (ph * 3 + u * 0.25)
        r = 31 - 8 * u
        x, y = int(round(cx + r * math.cos(a_))), int(round(cy + r * math.sin(a_)))
        m = np.zeros((p.h, p.w), bool)
        if 0 <= x < 64 and 0 <= y < 64:
            m[y, x] = True
        behind(p, m, '#ffc080', 0.9 * math.sin(math.pi * u))
    return p


# ---------------------------------------------------------------- 6. Council Keleres
def keleres(t=0.0):
    """a Keleres officer, 3/4 right: white-and-blue helm with gold trim and a wrap-around visor, firm jaw
    below it, comm earpiece and boom mic, blue armour with a white pauldron, the gold sunburst badge with the
    emblem's violet gem. Moves: breathing, a HUD light sweeps across the visor, the earpiece blinks."""
    p = Portrait()
    p.backdrop = (10, 16, 42)
    blue = ramp('#2a56c0', spread=0.75)
    white = ramp('#dfe6f2', spread=0.5, hue_shift=0.03)
    gold = ramp('#e8b430', spread=0.55)
    visor = ramp('#14204c', spread=0.8)
    skin = ramp('#c48c6c', spread=0.6)
    dark = ramp('#1c2236', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 48), (10, 42), (24, 41), (34, 44), (46, 41), (58, 45), (64, 50), (64, 66)]), blue,
               radius=8, texture=0.04)
        # chest plate: white with gold trim
        p.fill(p.poly([(20, 50), (34, 47), (52, 49), (56, 66), (18, 66)]), white, radius=6, bias=-0.05)
        p.stroke([(20, 50), (34, 47), (52, 49)], gold[3])
        p.stroke([(34, 48), (34, 66)], white[1])
        p.stroke([(22, 58), (33, 56)], white[1])
        # the badge: a gold sunburst with a violet gem
        bx, by = 44, 56
        rays = []
        for k in range(16):
            a_ = math.pi * 2 * k / 16
            r = 5.6 if k % 2 == 0 else 3.4
            rays.append((bx + 0.5 + r * math.cos(a_), by + 0.5 + r * math.sin(a_)))
        p.fill(p.poly(rays), gold, radius=1.5, bias=0.35)
        p.fill(p.ellipse(bx + 0.5, by + 0.5, 2.2, 2.2), ramp('#a040e0', spread=0.5), radius=1.5)
        p.px(bx - 1, by - 1, '#f8d8ff')
        p.glow(bx, by, '#f8d8ff', '#c070ff', r=0, strength=0.3)
        # the near pauldron
        p.fill(p.ellipse(9, 50, 13, 10), white, radius=6)
        p.fill(p.poly([(0, 49), (9, 44), (20, 48), (19, 52), (9, 49), (0, 53)]), blue, radius=2)
        p.stroke(arc(9, 50, 12, 9, 190, 330), gold[3])
        p.pixels([(5, 56), (13, 56)], white[4])
        # the collar
        p.fill(p.poly([(22, 40), (28, 36), (44, 36), (49, 41), (46, 48), (25, 48)]), blue, radius=3, bias=0.05)
        p.stroke([(23, 44), (34, 46), (47, 43)], gold[2])
    with p.at(0, head):
        # the lower face below the visor: a hard jaw, a set mouth
        face = p.poly([(29, 24), (48, 23), (49, 29), (47, 34), (43, 38), (37, 39), (32, 37), (29, 32)])
        p.fill(face, skin, radius=4, texture=0.05)
        p.fill(p.poly([(29, 24), (48, 23), (48, 26), (30, 27)]), skin, flat=0, edge=False)
        p.fill(p.poly([(30, 32), (33, 36), (37, 38), (43, 37), (47, 33), (46, 36), (42, 39), (37, 40), (32, 38)]), skin,
               flat=1, edge=False)
        p.stroke([(46, 26), (47, 29)], skin[4])
        p.pixels([(45, 30), (46, 30)], skin[1])
        p.stroke([(39, 33), (45, 33)], '#4a2018')
        p.pixels([(40, 34), (41, 34), (42, 34), (43, 34)], skin[3])
        p.px(38, 32, skin[1])
        p.pixels([(41, 36), (42, 36)], skin[4])
        p.stroke([(32, 28), (34, 33)], skin[1])
        # the helm
        helm = p.poly([(18, 30), (16, 18), (19, 9), (26, 4), (35, 3), (43, 5), (49, 11), (51, 18), (50, 25), (28, 27),
                       (27, 32), (22, 36)])
        p.fill(helm, white, radius=8)
        # blue side panel and cheek guard
        p.fill(p.poly([(17, 22), (27, 20), (29, 28), (28, 34), (23, 37), (18, 31)]), blue, radius=3)
        p.stroke([(17, 21), (27, 19), (30, 28)], gold[3])
        # a crest ridge along the crown and gold trim
        p.stroke([(27, 4), (37, 3), (45, 6), (50, 12)], gold[3])
        p.stroke([(25, 6), (19, 12)], white[4])
        p.stroke([(29, 8), (39, 7), (46, 11)], blue[2], width=2)
        # the wrap-around visor
        vz = p.poly([(25, 15), (40, 13), (51, 14), (52, 19), (50, 23), (28, 24)])
        p.fill(vz, visor, radius=2.5, bias=0.05)
        p.stroke([(25, 14), (40, 12), (51, 13)], gold[3])
        p.stroke([(27, 23), (50, 22)], visor[0])
        p.pixels([(30, 16), (31, 16), (32, 16)], visor[4])
        # HUD: a light sweeps across the visor, with readouts
        sx = 26 + int(((t * 2) % 1.0) * 30)
        for dxx, k in ((0, 1.0), (-1, 0.55), (-2, 0.3), (-3, 0.15)):
            col_ = mix(visor[2], '#7af4ff', k)
            p.stroke([(sx + dxx, 15), (sx + dxx, 22)], col_)
        p.pixels([(44, 17), (45, 17), (44, 18)], '#ff4a5a')
        p.pixels([(47, 20), (48, 20)], '#7af4ff')
        p.pixels([(33, 20), (34, 20), (35, 20), (37, 20)], '#3a8ac8')
        tint(p, vz & (abs(p.xx - sx) <= 4), '#7af4ff', 0.2)
        # the earpiece and boom mic
        p.fill(p.ellipse(21.5, 25, 3.2, 3.8), dark, radius=2)
        p.stroke([(22, 28), (25, 33), (31, 35)], dark[3])
        p.px(32, 35, dark[4])
        p.stroke([(20, 21), (20, 15)], dark[3])
        on = blink(t, 0.0, 3) or blink(t, 0.5, 3)
        p.glow(21, 25, '#ffffff' if on else '#406040', '#40ff80' if on else '#204030', r=0, strength=0.9 if on else 0.4)
        p.px(20, 14, '#ff5060' if blink(t, 0.25, 2) else dark[2])
    p.rim('#5af0ff', side=1, amount=0.45)
    p.rim('#ff4a6a', side=-1, amount=0.28)
    p.outline('#060818')
    return p


# ---------------------------------------------------------------- 7. The Last Bastion
def lastbastion(t=0.0):
    """a brash veteran, 3/4 right: buzz-cut, broken nose, scars, a lopsided defiant grin that runs into a
    riveted cybernetic jaw (steel teeth on that side), dented gunmetal plate with warning-yellow hazard
    stripes and a cracked shoulder lamp. Moves: breathing, the damaged lamp and the jaw light stutter,
    sparks and smoke drift up from the broken lamp."""
    p = Portrait()
    p.backdrop = (38, 24, 10)
    skin = ramp('#c88e68', spread=0.6)
    hair = ramp('#3c3632', spread=0.55)
    gun = ramp('#4e5866', spread=0.75)
    yel = ramp('#f2c21e', spread=0.5)
    cyb = ramp('#8e9aaa', spread=0.6)
    stub = ramp('#6e5446', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    f = int(t * FRAMES) % FRAMES
    lamp_on = f not in (3, 4, 9, 15, 16, 17, 21)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 46), (10, 40), (22, 40), (32, 44), (44, 40), (56, 42), (64, 48), (64, 66)]), gun,
               radius=8, texture=0.07, seed=101)
        # chest plate, stencilled unit number, scratches
        p.fill(p.poly([(22, 48), (34, 46), (50, 48), (52, 66), (20, 66)]), gun, radius=5, bias=0.05, texture=0.05)
        p.pixels([(37, 56), (38, 56), (39, 56), (37, 57), (39, 57), (37, 58), (39, 58), (37, 59), (39, 59), (37, 60),
                  (38, 60), (39, 60)], yel[2])
        p.pixels([(41, 56), (42, 56), (43, 56), (43, 57), (42, 58), (42, 59), (42, 60)], yel[2])
        p.stroke([(25, 52), (30, 57)], gun[4])
        p.stroke([(45, 51), (48, 54)], gun[1])
        # the near pauldron: angular, dented, hazard stripes
        pd = p.poly([(0, 43), (7, 37), (19, 37), (25, 43), (23, 54), (9, 58), (0, 59)])
        p.fill(pd, gun, radius=5, texture=0.06, seed=102)
        band = p.poly([(0, 52), (24, 47), (23, 54), (9, 58), (0, 59)]) & pd
        p.fill(band, yel, radius=2)
        for k in range(-2, 8):
            x = k * 4
            st = p.line_mask([(x, 61), (x + 5, 46)], 2) & band
            p.rgb[st] = hexrgb('#1a1610')
        p.stroke([(1, 51), (24, 46)], gun[1])
        for x, y in ((8, 42), (15, 45)):
            p.px(x, y, gun[0])
            p.px(x - 1, y - 1, gun[4])
        # the far pauldron with a cracked lamp
        p.fill(p.poly([(44, 42), (55, 39), (64, 43), (66, 57), (48, 55)]), gun, radius=4, bias=-0.1, texture=0.06)
        p.stroke([(48, 53), (64, 52)], yel[2], width=2)
        p.fill(p.ellipse(55.5, 46.5, 2.6, 2.4), gun, flat=0)
        if lamp_on:
            p.glow(55, 46, '#fff0c0', '#ff8a20', r=1, strength=0.9 if f % 5 else 0.6)
        else:
            p.pixels([(55, 46), (56, 47)], '#5a2a10')
        p.stroke([(54, 44), (57, 48)], '#1a1610')
        # the gorget
        p.fill(p.poly([(21, 41), (27, 36), (43, 36), (48, 41), (44, 48), (25, 48)]), gun, radius=3, bias=0.08)
        p.stroke([(23, 44), (34, 46), (46, 43)], yel[3])
    with p.at(0, head):
        # a square head, 3/4 right
        hd = p.poly([(19, 20), (20, 11), (26, 6), (36, 5), (44, 8), (48, 14), (49, 22), (49, 30), (46, 38), (40, 43),
                     (32, 44), (26, 40), (21, 34), (19, 28)])
        p.fill(hd, skin, radius=7, texture=0.08, seed=103)
        # the near ear, notched
        p.fill(p.poly([(18, 22), (21, 22), (22, 30), (19, 30), (17, 26)]), skin, radius=1.5, bias=-0.1)
        p.px(17, 24, p.backdrop)
        # buzz-cut hair, greying at the temple
        p.fill(p.poly([(19, 19), (20, 10), (26, 5), (36, 4), (44, 7), (48, 13), (47, 16), (41, 12), (31, 11), (23, 14),
                       (21, 21)]), hair, radius=3, texture=0.35, seed=104)
        p.pixels([(21, 16), (22, 14), (20, 18), (22, 17)], '#9a948c')
        # stubble on the human jaw
        p.fill(p.poly([(20, 31), (24, 36), (28, 39), (26, 41), (21, 35)]), stub, radius=2,
               texture=0.3, seed=105, edge=False)
        # heavy brows; the far one cocked up
        p.stroke([(26, 20), (33, 21)], hair[1], width=2)
        p.stroke([(38, 19), (44, 18)], hair[1], width=2)
        # eyes: narrowed, cocky
        p.pixels([(27, 24), (28, 24), (29, 24), (30, 24), (31, 24)], '#ece0d0')
        p.pixels([(29, 24), (30, 24)], '#2a4a6a')
        p.px(29, 24, '#4a8aca')
        p.stroke([(27, 25), (31, 25)], skin[1])
        p.stroke([(26, 23), (32, 23)], skin[0])
        p.pixels([(40, 23), (41, 23), (42, 23), (43, 23)], '#e0d4c4')
        p.pixels([(41, 23), (42, 23)], '#2a4a6a')
        p.stroke([(40, 22), (43, 22)], skin[0])
        # a broken nose
        p.stroke([(35, 22), (36, 25), (35, 28)], skin[3])
        p.stroke([(37, 23), (38, 29)], skin[1])
        p.pixels([(35, 30), (36, 30), (38, 30)], skin[0])
        p.px(37, 29, skin[4])
        # scars: through the near brow, across the cheek
        p.stroke([(28, 17), (30, 27)], '#8c4a3e')
        p.stroke([(22, 31), (27, 29)], '#8c4a3e')
        p.pixels([(29, 19), (30, 23)], skin[4])
        # the cybernetic jaw: riveted plate over the far jaw
        steel = [hexrgb(c) for c in ('#4c5464', '#6e7a8c', '#909cb0', '#b4bfcf', '#dfe6f0')]
        jaw = hd & p.poly([(26, 48), (28, 40), (34, 39), (40, 36), (44, 31), (52, 26), (52, 48)])
        p.fill(jaw, steel, radius=3, texture=0.05, bias=0.2)
        p.stroke([(28, 40), (34, 39), (40, 36), (44, 31), (50, 27)], '#16161c')
        p.stroke([(33, 43), (40, 42), (46, 36)], steel[1])
        p.stroke([(32, 42), (39, 41), (45, 35)], steel[3])
        for x, y in ((46, 31), (30, 42), (48, 37), (36, 43)):
            p.px(x, y, steel[4])
            p.px(x + 1, y + 1, steel[0])
        # exposed wiring at the seam
        p.pixels([(48, 28), (49, 29)], '#c03020')
        p.pixels([(47, 29)], '#e0a020')
        p.fill(p.ellipse(44.5, 37.5, 1.8, 1.8), [hexrgb('#16161c')] * 5, flat=0, edge=False)
        jl = lamp_on or f % 2
        if jl:
            p.glow(44, 37, '#fff0c0', '#ff4a10', r=0, strength=0.9)
        else:
            p.px(44, 37, '#5a1a08')
        # the grin: lopsided, teeth showing, steel on the cyber side
        p.fill(p.poly([(28, 35), (32, 35), (38, 34), (42, 32), (41, 35), (36, 37), (31, 37)]),
               [hexrgb('#2a0e0a')] * 5, flat=0, edge=False)
        p.stroke([(30, 35), (37, 35)], '#ece4d4')
        p.stroke([(38, 34), (41, 33)], '#b8c4d4')
        p.pixels([(32, 35), (35, 35)], '#c8bcac')
        p.stroke([(29, 34), (27, 33)], skin[1])
        p.stroke([(31, 38), (35, 38)], skin[3])
        p.px(26, 32, skin[1])
    p.rim('#ff8a2a', side=1, amount=0.5)
    p.rim('#5ad0ff', side=-1, amount=0.25)
    p.outline('#0e0a06')
    # smoke and sparks from the broken lamp
    for x0, ph in ((55, 0.0), (56, 0.33), (54, 0.66)):
        u = (t + ph) % 1.0
        y = 42 - u * 34 + body
        x = x0 + 2.5 * math.sin(2 * math.pi * (u * 0.9 + ph))
        r = 1.0 + 1.6 * u
        m = (np.hypot(p.xx + 0.5 - x, (p.yy + 0.5 - y) * 1.2) < r)
        behind(p, m, '#9a9080', 0.38 * (1 - u) ** 0.7)
    for x0, ph in ((55, 0.1), (53, 0.45), (58, 0.7)):
        u = (t * 2 + ph) % 1.0
        if u < 0.6:
            x = x0 + int(round(u * 8 * (1 if x0 > 54 else -1)))
            y = int(round(45 - u * 22 + u * u * 20)) + body
            p.px(x, y, mix('#fff4a0', '#ff6a10', u / 0.6))
    return p


# ---------------------------------------------------------------- 8. The Ral Nel Consortium
def ralnel(t=0.0):
    """a polished consortium executive, 3/4 right: tall grey-blue elongated skull, a sleek green sensor visor,
    a thin knowing smile, a high stiff collar lined in the emblem's emerald, the leaf-crest pin on the lapel.
    Moves: breathing, the visor's lenses flicker and scan, data lights run along the temple and collar."""
    p = Portrait()
    p.backdrop = (6, 26, 26)
    skin = ramp('#6e84a8', spread=0.65, hue_shift=0.03)
    suit = ramp('#2c303e', spread=0.7)
    em = ramp('#2ac46e', spread=0.55)
    shirt = ramp('#d8dce8', spread=0.45)
    visor = ramp('#0e1e1c', spread=0.7)
    body, head = breath(t), breath(t, -0.1)
    f = int(t * FRAMES) % FRAMES
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (10, 44), (22, 42), (32, 46), (42, 43), (54, 44), (64, 50), (64, 66)]), suit,
               radius=8, texture=0.03)
        # shirt and a jade clasp at the throat
        p.fill(p.poly([(28, 44), (42, 44), (38, 58), (35, 62)]), shirt, radius=3)
        p.fill(p.poly([(33, 46), (37, 46), (36, 58), (35, 60), (34, 58)]), suit, radius=1.5, bias=0.1)
        # lapels with emerald piping
        p.fill(p.poly([(16, 46), (28, 44), (35, 62), (30, 66), (22, 66)]), suit, radius=3, bias=0.12)
        p.fill(p.poly([(50, 46), (42, 44), (35, 62), (38, 66), (46, 66)]), suit, radius=3, bias=0.0)
        p.stroke([(28, 45), (35, 62)], em[3])
        p.stroke([(42, 45), (36, 62)], em[2])
        # the consortium crest: a green leaf-gem
        p.fill(p.poly([(22, 50), (25, 52), (26, 56), (23, 60), (20, 56), (21, 52)]), em, radius=1.6)
        p.px(22, 52, '#d8ffe8')
        p.stroke([(23, 53), (23, 58)], em[1])
        # the high stiff collar, lined in green
        p.fill(p.poly([(16, 48), (17, 36), (22, 29), (27, 33), (28, 44)]), suit, radius=3, bias=0.05)
        p.fill(p.poly([(19, 45), (19, 37), (22, 32), (25, 35), (26, 43)]), em, radius=2, bias=-0.15)
        p.fill(p.poly([(42, 44), (45, 34), (49, 30), (52, 38), (50, 46)]), suit, radius=3, bias=-0.1)
        p.stroke([(46, 35), (49, 32)], em[2])
        # data lights along the collar edge
        for i, (x, y) in enumerate(((18, 46), (18, 43), (18, 40), (19, 37))):
            on = (f // 2) % 6 == i
            p.px(x, y, '#c8ffd8' if on else em[1])
    with p.at(0, head):
        # the long neck
        p.fill(p.poly([(29, 34), (41, 34), (41, 46), (29, 46)]), skin, radius=3, bias=-0.25)
        # the elongated skull, swept up and back
        sk = p.poly([(24, 36), (21, 26), (17, 16), (15, 7), (17, 0), (23, -4), (31, -3), (38, 2), (44, 9), (48, 16),
                     (50, 22), (50, 27), (48, 32), (45, 37), (40, 41), (33, 41), (27, 39)])
        p.fill(sk, skin, radius=8, texture=0.04, seed=111)
        # ridges sweeping back over the crown
        p.stroke([(36, 5), (27, 0), (21, 0)], skin[3])
        p.stroke([(40, 10), (29, 4), (20, 4)], skin[1])
        p.stroke([(42, 14), (31, 9), (21, 10)], skin[1])
        p.stroke([(20, 8), (19, 16)], skin[4])
        # a glowing seam along the crown
        p.stroke([(38, 6), (30, 1), (22, 1)], em[2])
        p.px(30, 1, '#c8ffd8')
        # temple implant: data lights in sequence
        p.fill(p.poly([(22, 13), (26, 12), (28, 22), (24, 23)]), suit, radius=1.5, bias=0.1)
        for i, (x, y) in enumerate(((24, 14), (25, 17), (26, 20))):
            on = (f // 2 + 2) % 4 == i
            p.px(x, y, '#d8ffe8' if on else em[0])
            if on:
                p.glow(x, y, '#d8ffe8', '#3ae07a', r=0, strength=0.4)
        # the sleek sensor visor
        vz = p.poly([(29, 19), (44, 18), (51, 19), (51, 24), (44, 25), (30, 25)])
        p.fill(vz, visor, radius=2, bias=0.1)
        p.stroke([(29, 18), (44, 17), (50, 18)], skin[4])
        p.stroke([(30, 24), (50, 23)], visor[0])
        # lenses: steady glow with a flicker, and a slow scan line
        glitch = blink(t, 0.4, 1) or blink(t, 0.83, 1)
        for lx, ly, big in ((37, 21, 1), (47, 21, 0)):
            if glitch and big:
                p.pixels([(lx - 2, ly), (lx + 2, ly)], '#5aff9a')
                continue
            p.glow(lx, ly, '#eafff0', '#3ae07a', r=big, strength=0.75 + 0.25 * pulse(t, lx * 0.05, 2))
        sx = 30 + (f % 12) * 2
        if sx < 51:
            p.stroke([(sx, 20), (sx, 23)], mix(visor[3], '#7affb0', 0.5))
        # a narrow nose, a thin, knowing smile
        p.stroke([(48, 25), (49, 29)], skin[1])
        p.px(49, 30, skin[0])
        p.fill(p.poly([(31, 26), (40, 26), (38, 31), (33, 32)]), skin, flat=1, edge=False)
        p.stroke([(31, 26), (40, 26)], skin[0])
        p.stroke([(39, 34), (44, 34), (46, 33), (47, 32)], '#1a1e30')
        p.pixels([(40, 35), (41, 35), (42, 35), (43, 35)], skin[3])
        p.px(38, 33, skin[1])
        p.stroke([(36, 39), (42, 39)], skin[3])
    p.rim('#5aff9a', side=1, amount=0.45)
    p.rim('#b07aff', side=-1, amount=0.3)
    p.outline('#04100e')
    return p


# ---------------------------------------------------------------- 9. The Deepwrought Scholarate
def deepwrought(t=0.0):
    """a kindly anglerfish scholar: wide head of deep-sea blue with a pale belly, big gentle eyes (a brass
    monocle on one), a soft smile of needle teeth, a glowing lure on a stalk; a brass pressure collar with a
    gauge, indigo scholar's robes, brass scroll cases on the shoulder. Moves: the lure bobs and glows,
    bubbles rise, the side fins flutter, breathing."""
    p = Portrait()
    p.backdrop = (4, 22, 42)
    skin = ramp('#2c5e92', spread=0.7)
    belly = ramp('#9cc2d0', spread=0.45)
    robe = ramp('#2e2c72', spread=0.75)
    brass = ramp('#cc983a', spread=0.6)
    fin = ramp('#3a96bc', spread=0.55)
    leather = ramp('#6a3e2a', spread=0.55)
    body, head = breath(t), breath(t, -0.12)
    lk = pulse(t, 0.0, 2)
    bob = int(round(1.2 * wave(t, 0.1, 1)))
    with p.at(0, body):
        # scroll cases rising behind the near shoulder
        for (x0, y0), (x1, y1), w in (((6, 56), (10, 32), 3.2), ((13, 58), (18, 36), 2.6)):
            p.fill(blobs(p, [(x0, y0), (x1, y1)], w, w), brass, radius=2.2)
            p.fill(p.ellipse(x1 + 0.5, y1 - 0.5, w + 0.6, 2.0), leather, radius=1.2)
            p.stroke([(x0 + (x1 - x0) * 0.45 - w, y0 + (y1 - y0) * 0.45), (x0 + (x1 - x0) * 0.45 + w, y0 + (y1 - y0) * 0.45 + 1)],
                     leather[1])
        p.fill(p.poly([(17, 37), (21, 33), (22, 36), (19, 40)]), ramp('#e8dcc0', spread=0.4), radius=1)
        p.fill(p.poly([(0, 66), (0, 50), (10, 44), (22, 43), (32, 47), (42, 43), (54, 44), (64, 50), (64, 66)]), robe,
               radius=8, texture=0.06, seed=121)
        p.stroke([(12, 52), (16, 66)], robe[1])
        p.stroke([(50, 52), (47, 66)], robe[1])
        # strap of the scroll cases
        p.stroke([(14, 44), (28, 66)], leather[2], width=3)
        p.stroke([(13, 44), (27, 66)], leather[3])
        p.pixels([(20, 55), (21, 55), (20, 56), (21, 56)], brass[3])
        # the brass pressure collar with a gauge
        col_ = p.ellipse(32, 46, 17, 6.5) & ~p.ellipse(32, 44, 12, 4.5)
        p.fill(col_ | p.poly([(17, 44), (47, 44), (47, 47), (17, 47)]) & p.ellipse(32, 46, 17, 6.5), brass, radius=3)
        for x in (19, 25, 39, 45):
            p.px(x, 48 if x in (25, 39) else 46, brass[4])
        p.fill(p.ellipse(32, 51, 3.6, 3.6), brass, radius=2)
        p.fill(p.ellipse(32, 51, 2.3, 2.3), ramp('#d8f0f0', spread=0.3), radius=1.5, edge=False)
        p.stroke([(32, 51), (33 + int(round(wave(t, 0, 1))), 49)], '#c02020')
    with p.at(0, head):
        # side fins, fluttering
        for side in (-1, 1):
            s = sway(t, 0.25 if side < 0 else 0.6, 1, cycles=2)
            bx = 32 + side * 17
            pts = [(bx, 24), (bx + side * (8 + s), 18 + s), (bx + side * (10 + s), 24), (bx + side * (8 + s), 31 - s),
                   (bx, 32)]
            p.fill(p.poly(pts), fin, radius=2, bias=-0.05 * side)
            for yy in (21, 25, 29):
                p.stroke([(bx + side, 28 - (28 - yy) // 2), (bx + side * (8 + s), yy + (s if yy > 25 else -s if yy < 25 else 0))],
                         fin[1] if side > 0 else fin[3])
        # the wide anglerfish head
        hd = p.ellipse(32, 26, 17, 15) | p.poly([(15, 30), (49, 30), (46, 40), (38, 45), (26, 45), (18, 40)])
        p.fill(hd, skin, radius=9, texture=0.07, seed=122)
        # the pale underjaw
        p.fill(p.poly([(18, 34), (32, 37), (46, 34), (44, 41), (37, 45), (27, 45), (20, 41)]), belly, radius=3,
               texture=0.05, seed=123)
        # glowing spots along the crown
        for i, (x, y) in enumerate(((20, 16), (24, 13), (40, 13), (44, 16), (17, 21), (47, 21))):
            p.px(x, y, mix('#3a8ac0', '#aaffff', pulse(t, i * 0.15, 1)))
        # the lure's stalk, from the crown arcing forward
        stalk = [(32, 12), (33, 7), (37, 3), (43, 2 + bob // 2), (48, 5 + bob), (49, 8 + bob)]
        p.stroke(stalk, skin[1], width=1)
        p.stroke([(32, 12), (33, 7), (37, 3)], skin[3])
        # big, gentle eyes; a brass monocle on the right
        for ex in (24, 40):
            p.fill(p.ellipse(ex + 0.5, 25, 4.4, 4.2), skin, flat=0, edge=False)
            p.fill(p.ellipse(ex + 0.5, 25.5, 3.6, 3.5), ramp('#f0e6b8', spread=0.4), radius=2, edge=False, bias=0.1)
            p.fill(p.ellipse(ex + 0.5, 26, 2.3, 2.5), [hexrgb('#0a1020')] * 5, flat=0, edge=False)
            p.pixels([(ex - 1, 24), (ex - 1, 25), (ex, 24)], '#ffffff')
            p.px(ex + 2, 27, '#8ab0c8')
            # soft lids, drooping kindly at the outer corners
            if blink(t, 0.7, 2):
                p.fill(p.ellipse(ex + 0.5, 25.5, 4.4, 4.2), skin, radius=2, bias=0.2, edge=False)
                p.stroke([(ex - 3, 25), (ex - 1, 27), (ex + 2, 27), (ex + 4, 25)], skin[0])
            else:
                o = -1 if ex < 32 else 1
                p.stroke([(ex - 3, 21 + (1 if o < 0 else 0)), (ex + 3, 21 + (1 if o > 0 else 0))], skin[0])
        mono = p.ellipse(40.5, 25.5, 5.4, 5.4) & ~p.ellipse(40.5, 25.5, 4.3, 4.3)
        p.fill(mono, brass, radius=1.2, edge=False)
        tint(p, p.ellipse(40.5, 25.5, 4.3, 4.3), '#aaf0ff', 0.12)
        p.pixels([(38, 22), (37, 23)], '#ffffff')
        p.stroke([(45, 28), (47, 33), (46, 39)], brass[2])
        # brows lifted kindly
        p.stroke([(20, 18), (23, 17), (27, 18)], skin[4])
        p.stroke([(37, 18), (41, 17), (44, 18)], skin[3])
        # a wide, soft smile of small needle teeth
        smile = [(18, 34), (24, 37), (32, 38), (40, 37), (46, 34)]
        p.stroke(smile, '#0e1a30')
        for x in range(21, 45, 3):
            yy = int(round(34 + 4 * (1 - ((x - 32) / 14) ** 2)))
            p.px(x, yy - 1, '#e8f0f0')
        p.pixels([(17, 33), (47, 33)], skin[1])
        # gill slits
        p.stroke([(17, 26), (18, 30)], skin[0])
        p.stroke([(47, 26), (46, 30)], skin[0])
        # the lure: a bobbing bulb of light
        p.fill(p.ellipse(49.5, 10.5 + bob, 2.3, 2.3), ramp('#7af8ff', spread=0.3), radius=1.5)
        p.glow(49, 10 + bob, '#ffffff', mix('#7af8ff', '#e8ffff', lk), r=1, strength=0.55 + 0.45 * lk)
    p.rim('#5af0ff', side=1, amount=0.45)
    p.rim('#ffb84a', side=-1, amount=0.25)
    p.outline('#020a18')
    # the lure's light on the water around it
    halo = (np.hypot(p.xx - 49, p.yy - 10 - bob - head) < 5.5)
    behind(p, halo, '#7af8ff', 0.12 + 0.12 * lk)
    # bubbles rising
    for x0, ph, r in ((10, 0.0, 1), (55, 0.35, 0), (16, 0.6, 0), (52, 0.8, 1), (6, 0.25, 0)):
        u = (t + ph) % 1.0
        y = int(round(44 - u * 46))
        x = x0 + sway(t, ph, 1, 2)
        m = np.zeros((p.h, p.w), bool)
        if r and 1 <= y < 63:
            m[y - 1, x] = m[y + 1, x] = m[y, x - 1] = m[y, x + 1] = True
        elif 0 <= y < 64:
            m[y, x] = True
        behind(p, m, '#bff6ff', 0.6)
        if r and 1 <= y < 63:
            hm = np.zeros((p.h, p.w), bool)
            hm[y - 1, x - 1] = True
            behind(p, hm, '#ffffff', 0.5)
    return p


ALL = {'empyrean': empyrean, 'naaz': naaz, 'nomad': nomad, 'titans': titans, 'cabal': cabal,
       'keleres': keleres, 'lastbastion': lastbastion, 'ralnel': ralnel, 'deepwrought': deepwrought}

if __name__ == '__main__':
    from PIL import Image, ImageDraw
    S = 64 * 4
    cols = 3
    keys = list(ALL)
    rows = (len(keys) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (2 * S + 30) + 10, rows * (S + 30) + 10), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    for i, k in enumerate(keys):
        x0, y0 = 10 + (i % cols) * (2 * S + 30), 10 + (i // cols) * (S + 30)
        for j, t in enumerate((0.0, 0.5)):
            pp = ALL[k](t)
            sheet.paste(pp.image(4, bg=tuple(pp.backdrop) + (255,)).convert('RGB'), (x0 + j * (S + 4), y0))
        d.text((x0, y0 + S + 4), k, fill=(0, 0, 0))
        save_gif(ALL[k], f'{D}/anim_{k}.gif', scale=4)
    sheet.save(f'{D}/sheet_faces_b3.png')
