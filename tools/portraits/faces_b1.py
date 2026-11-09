import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import math  # noqa: E401

import numpy as np

from pix import Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, save_gif, FRAMES  # noqa: E402

D = __import__('os').path.dirname(__import__('os').path.abspath(__file__))


# ---------------------------------------------------------------- helpers (local to this file)
def blade_pts(base, tip, w, bend=0.0, prof=(0, 0.75, 1.0, 0.9, 0.6, 0.0)):
    """outline of a leaf / tuft / flame tongue from base to tip; bend (px) pushes it sideways, most at the tip"""
    (x0, y0), (x1, y1) = base, tip
    L = math.hypot(x1 - x0, y1 - y0) or 1
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    nx, ny = -uy, ux
    n = len(prof) - 1
    left, right, mid = [], [], []
    for i, wf in enumerate(prof):
        s = i / n
        b = bend * s * s
        cx, cy = x0 + (x1 - x0) * s + nx * b, y0 + (y1 - y0) * s + ny * b
        mid.append((cx, cy))
        left.append((cx + nx * w * wf, cy + ny * w * wf))
        right.append((cx - nx * w * wf, cy - ny * w * wf))
    return left + right[::-1], mid


def tube(p, pts, r0, r1=None, step=0.5):
    """a mask for a round tube (root, cable, hose) along a polyline, tapering from r0 to r1"""
    r1 = r0 if r1 is None else r1
    m = np.zeros((p.h, p.w), bool)
    segs = list(zip(pts[:-1], pts[1:]))
    total = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in segs) or 1
    run = 0.0
    for (x0, y0), (x1, y1) in segs:
        L = math.hypot(x1 - x0, y1 - y0)
        k = max(1, int(L / step))
        for i in range(k + 1):
            s = i / k
            cx, cy = x0 + (x1 - x0) * s + p.dx, y0 + (y1 - y0) * s + p.dy
            r = r0 + (r1 - r0) * (run + L * s) / total
            m |= (p.xx + 0.5 - cx) ** 2 + (p.yy + 0.5 - cy) ** 2 <= r * r
        run += L
    return m


def stroke_in(p, pts, col, clip, width=1):
    """a stroke kept inside a mask"""
    m = p.line_mask(pts, width) & clip
    p.rgb[m] = hexrgb(col) if isinstance(col, str) else col
    p.a |= m


def grain(x0, y0, y1, knots):
    """a wood-grain line running down from (x0, y0), bowing away from knots (kx, ky, push, spread)"""
    pts = []
    for y in range(y0, y1 + 1):
        x = x0
        for kx, ky, A, sy in knots:
            dx = x0 - kx
            if abs(dx) < 7:
                sgn = 1 if dx >= 0 else -1
                x += sgn * A * math.exp(-((y - ky) ** 2) / (2 * sy * sy)) * (1 - abs(dx) / 7)
        pts.append((x, y))
    return pts


def veil(p, mask, col, a, bg):
    """translucent paint: blend col over what is there (or over the backdrop where nothing is)"""
    c = np.array(hexrgb(col) if isinstance(col, str) else col, float)
    base = p.rgb.astype(float)
    base[~p.a] = bg
    a = np.broadcast_to(np.asarray(a, float), mask.shape)
    m = mask
    p.rgb[m] = (base[m] * (1 - a[m, None]) + c * a[m, None]).astype(np.uint8)
    p.a |= m


def glitch(p, y0, y1, shift, tint=None, k=0.35):
    """a codec glitch: one band of rows torn sideways (and tinted)"""
    p.rgb[y0:y1] = np.roll(p.rgb[y0:y1], shift, axis=1)
    p.a[y0:y1] = np.roll(p.a[y0:y1], shift, axis=1)
    if tint is not None:
        c = np.array(hexrgb(tint) if isinstance(tint, str) else tint, float)
        band = p.rgb[y0:y1].astype(float)
        m = p.a[y0:y1]
        band[m] = band[m] * (1 - k) + c * k
        p.rgb[y0:y1] = band.astype(np.uint8)


def mirror(pts):
    return [(64 - x, y) for x, y in pts]


# ---------------------------------------------------------------- 1. Arborec
def arborec(t=0.0):
    """The Green: a bark-plated tree-being with a crown of fronds, moss mantle, sap-lit eyes.
    Moves: fronds sway, the bark breathes, the eyes and spore pods pulse, spores drift up."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    bark = ramp('#7c5634', spread=0.72, hue_shift=0.04)
    root = ramp('#6a4a32', spread=0.75, hue_shift=0.04)
    moss = ramp('#88b436', spread=0.6)
    leaf = ramp('#2e7a44', spread=0.72)
    leaf2 = ramp('#6cb03c', spread=0.6)
    pod = ramp('#d8f040', spread=0.45)
    body, head = breath(t), breath(t, -0.1)
    gk = pulse(t, cycles=2)
    with p.at(0, head):
        # the crown of fronds: a dark back layer, a bright front layer
        back = [((25, 17), (1, 6), 4.6, 0.0), ((28, 14), (10, -6), 4.6, 0.2), ((36, 14), (54, -6), 4.6, 0.45),
                ((39, 17), (63, 6), 4.6, 0.65)]
        front = [((23, 21), (2, 22), 3.8, 0.1), ((27, 15), (19, -6), 4.0, 0.33), ((32, 14), (32, -8), 4.0, 0.55),
                 ((37, 15), (45, -6), 4.0, 0.75), ((41, 21), (62, 22), 3.8, 0.9)]
        for layer, rp in ((back, leaf), (front, leaf2)):
            for base, tip, w, ph in layer:
                bend = 1.6 * wave(t, ph)
                pts, mid = blade_pts(base, tip, w, bend, prof=(0.3, 0.8, 1.0, 0.9, 0.6, 0.0))
                p.fill(p.poly(pts), rp, radius=2.2, texture=0.04, seed=int(ph * 50))
                p.stroke(mid[1:-1], rp[3] if rp is leaf2 else rp[2])
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (8, 46), (20, 43), (32, 47), (44, 43), (56, 46), (64, 52), (64, 66)]), root,
               radius=9, texture=0.1, seed=30)
        # two great roots over the shoulders
        for pts, r0, r1, b in (([(24, 50), (14, 50), (6, 55), (2, 66)], 3.0, 4.4, 0.05),
                               ([(40, 50), (50, 50), (58, 55), (62, 66)], 3.0, 4.4, -0.12)):
            p.fill(tube(p, pts, r0, r1), bark, radius=3, bias=b, texture=0.1, seed=int(r0 * 10))
        for seg in ([(9, 56), (5, 62)], [(55, 56), (59, 62)], [(15, 52), (10, 54)], [(49, 52), (54, 54)]):
            p.stroke(seg, bark[1])
        # moss on the shoulders, studded with glowing spore pods
        for pts, b, sd in (([(0, 48), (5, 45), (13, 44), (15, 47), (9, 50), (3, 53), (0, 53)], 0.0, 31),
                           ([(64, 48), (59, 45), (51, 44), (49, 47), (55, 50), (61, 53), (64, 53)], -0.1, 32),
                           ([(16, 58), (22, 56), (26, 60), (20, 63), (15, 62)], 0.0, 37),
                           ([(44, 59), (50, 57), (52, 62), (45, 64)], -0.1, 38)):
            p.fill(p.poly(pts), moss, radius=2.5, bias=b, texture=0.32, seed=sd)
        for cx, cy, ph in ((6, 47, 0.0), (11, 46, 0.3), (53, 46, 0.55), (59, 48, 0.8), (20, 59, 0.15), (48, 60, 0.65)):
            k = pulse(t, ph, 2)
            p.pixels([(cx, cy + 1), (cx + 1, cy + 1), (cx + 1, cy)], pod[1])
            p.glow(cx, cy, mix(pod[3], '#fbffd8', k), mix('#a0d830', '#f4ff80', k), r=0, strength=0.35 + 0.55 * k)
        # the trunk of a neck
        p.fill(p.poly([(25, 40), (39, 40), (40, 50), (24, 50)]), root, radius=4, bias=-0.05, texture=0.1, seed=33)
        for x0, x1 in ((27, 26), (30, 30), (34, 35), (37, 38)):
            p.stroke([(x0, 41), (x1, 49)], root[1])
        # a ruff of broad leaves round the neck
        for base, tip, w, ph, rp in (((27, 47), (13, 55), 3.2, 0.1, leaf), ((37, 47), (51, 55), 3.2, 0.6, leaf),
                                     ((29, 48), (20, 59), 3.4, 0.35, leaf2), ((35, 48), (44, 59), 3.4, 0.85, leaf2),
                                     ((32, 48), (32, 61), 3.4, 0.2, leaf2)):
            pts, mid = blade_pts(base, tip, w, 1.2 * wave(t, ph), prof=(0.4, 0.9, 1.0, 0.85, 0.5, 0.0))
            p.fill(p.poly(pts), rp, radius=2, texture=0.04, seed=int(ph * 70))
            p.stroke(mid[1:-1], rp[3])
    with p.at(0, head):
        head_m = (p.ellipse(32, 30, 12.5, 17) & ~p.poly([(18, 37), (28, 49), (18, 49)]) & ~p.poly([(46, 37), (36, 49), (46, 49)]))
        p.fill(head_m, bark, radius=8, texture=0.1, seed=34)
        # wood grain flowing round the eye-knots
        knots = [(26, 29.5, 3.2, 4.0), (38, 29.5, 3.2, 4.0), (32, 41, 2.0, 2.5)]
        for x0 in (21, 24, 27, 30, 34, 37, 40, 43):
            pts = grain(x0, 13, 48, knots)
            stroke_in(p, pts, bark[1], head_m)
            stroke_in(p, [(x + 1, y) for x, y in pts], bark[3], head_m & (p.yy < 40 + head))
        # moss cap where the fronds grow out, lichen on one cheek
        p.fill(p.poly([(20, 22), (21, 15), (27, 11), (37, 11), (43, 15), (44, 22), (40, 19), (35, 20), (32, 18), (28, 20), (24, 19)]),
               moss, radius=3, texture=0.32, seed=35)
        p.fill(p.poly([(20, 35), (23, 35), (24, 40), (21, 39)]), moss, radius=1, texture=0.3, seed=36)
        # heavy overhanging brow, deep knot-hole sockets, sap-lit eyes
        p.fill(p.poly([(19, 24), (31, 25), (33, 25), (45, 24), (44, 28), (33, 28), (31, 28), (20, 28)]), bark, radius=2, bias=0.2)
        for cx in (26, 38):
            p.fill(p.ellipse(cx + 0.5, 29.5, 3.6, 2.6), bark, flat=0, edge=False)
            p.fill(p.ellipse(cx + 0.5, 30.5, 2.6, 1.4), bark, flat=1, edge=False)
            p.glow(cx, 29, '#fbffc8', mix('#8ae020', '#e8ff50', gk), r=1, strength=0.6 + 0.4 * gk)
        # nose ridge and a flat, unfeeling mouth-knot lit by sap
        p.stroke([(32, 28), (32, 36)], bark[4])
        p.stroke([(33, 29), (33, 36)], bark[1])
        p.pixels([(31, 37), (34, 37)], bark[0])
        p.stroke([(27, 41), (37, 41)], bark[0])
        p.stroke([(28, 42), (36, 42)], bark[3])
        p.pixels([(29, 41), (30, 41), (31, 41), (33, 41), (34, 41)], mix('#4a6a18', '#c0f848', gk))
        # vines trailing from the jaw
        for pts, ph in (([(25, 42), (23, 47), (24, 52)], 0.0), ([(39, 42), (41, 47), (40, 52)], 0.5)):
            s = sway(t, ph, 1)
            p.stroke([pts[0], pts[1], (pts[2][0] + s, pts[2][1])], moss[1])
            p.px(pts[2][0] + s + 1, pts[2][1], moss[3])
            p.px(pts[1][0] - 1, pts[1][1], moss[4])
    p.rim('#b4ff40', side=1, amount=0.45)
    p.rim('#c070ff', side=-1, amount=0.28)
    p.outline('#06120a')
    for x0, ph in ((8, 0.0), (20, 0.5), (45, 0.25), (56, 0.7), (12, 0.85), (52, 0.4)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(52 - u * 30)), mix('#f4ffa0', '#3a6a20', u))
    p.backdrop = (8, 26, 22)
    return p


# ---------------------------------------------------------------- 2. Barony of Letnev
def letnev(t=0.0):
    """a pale aristocrat-officer: peaked cap with the Letnev star, high crimson-lined collar, gold epaulettes and braid,
    a red cyber-monocle, one brow arched in disdain. Moves: breathing, the monocle pulses and a gleam crosses it, the cap
    badge glints, the epaulette fringe swings."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    coat = ramp('#2c2a3c', spread=0.8)
    crim = ramp('#b81a32', spread=0.65)
    gold = ramp('#d8a838', spread=0.6, hue_shift=0.04)
    skin = ramp('#e6cec6', spread=0.55)
    hair = ramp('#6a6474', spread=0.6)
    lens = ramp('#ff2a3a', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    gl = pulse(t, cycles=2)
    g = int(t * FRAMES) % FRAMES
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (9, 44), (21, 41), (32, 45), (43, 41), (55, 44), (64, 50), (64, 66)]), coat, radius=8,
               texture=0.05, seed=40)
        # armoured cuirass and crimson lapels
        p.fill(p.poly([(22, 46), (42, 46), (41, 66), (23, 66)]), coat, radius=5, bias=0.12)
        p.stroke([(25, 56), (32, 58), (39, 56)], coat[3])
        p.fill(p.poly([(15, 45), (24, 46), (29, 66), (19, 66)]), crim, radius=3)
        p.fill(p.poly([(49, 45), (40, 46), (35, 66), (45, 66)]), crim, radius=3, bias=-0.12)
        p.stroke([(16, 46), (20, 65)], crim[4])
        for y in (51, 55, 59, 63):
            p.px(32, y, gold[4])
            p.px(32, y + 1, gold[1])
        # gold braid looping from the right shoulder to the lapel
        for loop, c in (([(47, 47), (45, 53), (41, 56), (38, 53), (39, 49)], 3), ([(48, 49), (46, 57), (41, 60), (37, 57), (38, 51)], 2)):
            p.stroke(loop, gold[c])
            for k, (x, y) in enumerate(loop[1:-1]):
                p.px(x, y, gold[c + 1])
        # epaulettes: gold boards with a swinging fringe
        for side in (1, -1):
            board = [(1, 49), (8, 44), (19, 42), (20, 46), (11, 50)]
            p.fill(p.poly(board if side == 1 else mirror(board)), gold, radius=2, bias=0 if side == 1 else -0.12)
            for x in range(1, 12):
                xx = x if side == 1 else 64 - x
                y0 = 50 if x < 10 else 51 - (x - 10)
                sw = sway(t, 0.06 * x + (0 if side == 1 else 0.4), 1) if x % 3 == 0 else 0
                p.stroke([(xx, y0), (xx + sw, y0 + 4)], gold[3] if x % 2 else gold[1])
        # medal: the Letnev star
        p.pixels([(22, 50), (23, 50), (24, 50), (25, 50), (26, 50)], '#e8e8f0')
        p.pixels([(23, 50), (25, 50)], '#3a5ac8')
        p.stroke([(24, 51), (24, 52)], gold[1])
        p.fill(p.poly([(24, 52), (25, 54), (27, 55), (25, 56), (24, 58), (23, 56), (21, 55), (23, 54)]), gold, flat=3)
        p.pixels([(24, 53), (23, 55)], gold[4])
        p.px(24, 55, crim[2])
        p.pixels([(25, 57), (24, 58), (26, 55)], gold[1])
    with p.at(0, head):
        # high collar, crimson lining, gold piping
        p.fill(p.poly([(16, 47), (17, 32), (23, 29), (28, 41), (28, 47)]), coat, radius=3)
        p.fill(p.poly([(48, 47), (47, 32), (41, 29), (36, 41), (36, 47)]), coat, radius=3, bias=-0.12)
        p.stroke([(22, 31), (27, 42)], crim[3], width=2)
        p.stroke([(42, 31), (37, 42)], crim[2], width=2)
        p.stroke([(17, 32), (23, 29)], gold[3])
        p.stroke([(47, 32), (41, 29)], gold[2])
        p.pixels([(19, 37), (20, 37), (19, 40), (20, 40), (45, 37), (44, 37), (45, 40), (44, 40)], gold[4])
        # neck and a black stock with a ruby pin
        p.fill(p.poly([(27, 38), (37, 38), (37, 47), (27, 47)]), skin, radius=4, bias=-0.3)
        p.fill(p.poly([(27, 43), (37, 43), (36, 48), (28, 48)]), coat, radius=2)
        p.glow(32, 45, '#ffd0d0', crim[3], r=0, strength=0.7)
        # a long, narrow, pale face with a sharp jaw
        face = p.poly([(22, 17), (42, 17), (43, 26), (41, 34), (36, 40), (32, 42), (28, 40), (23, 34), (21, 26)])
        p.fill(face, skin, radius=8, texture=0.03, seed=41)
        p.fill(p.ellipse(21, 26, 1.8, 3.2), skin, radius=1)
        p.fill(p.ellipse(43, 26, 1.8, 3.2), skin, radius=1, bias=-0.1)
        p.fill(p.poly([(21, 17), (24, 17), (23, 25), (21, 23)]), hair, radius=1)
        p.fill(p.poly([(43, 17), (40, 17), (41, 25), (43, 23)]), hair, radius=1, bias=-0.1)
        p.pixels([(25, 31), (26, 32)], skin[1])
        p.pixels([(39, 31), (38, 32)], skin[1])
        p.stroke([(37, 36), (38, 33)], skin[1])
        # the peaked cap
        p.fill(p.poly([(14, 11), (16, 5), (24, 2), (40, 2), (48, 5), (50, 11), (45, 14), (19, 14)]), coat, radius=5,
               texture=0.04, seed=42)
        p.stroke([(18, 6), (25, 3), (31, 3)], coat[4])
        p.fill(p.poly([(18, 11), (46, 11), (46, 15), (18, 15)]), crim, radius=2)
        p.fill(p.poly([(17, 15), (47, 15), (44, 19), (32, 20), (20, 19)]), coat, radius=2, bias=0.05)
        p.stroke([(21, 16), (29, 16)], coat[4])
        p.stroke([(18, 14), (46, 14)], gold[3])
        # the cap badge: the Letnev star in gold and crimson
        p.fill(p.poly([(32, 2), (34, 6), (38, 7), (34, 9), (32, 13), (30, 9), (26, 7), (30, 6)]), gold, radius=1.5)
        p.pixels([(32, 5), (32, 6), (31, 7), (32, 7), (33, 7), (32, 8), (32, 9)], crim[3])
        if 6 <= g <= 8:
            p.glow(31, 5, '#ffffff', '#ffe8a0', r=0, strength=0.9 - 0.2 * abs(g - 7))
        # visor shade; one brow arched in disdain, one pinned by the monocle
        p.stroke([(22, 20), (42, 20)], skin[1])
        p.stroke([(22, 23), (24, 21), (26, 21), (29, 22)], hair[0])
        p.stroke([(34, 23), (41, 23)], hair[0])
        # the natural eye, cold and half-lidded, looking down at you
        p.pixels([(24, 25), (25, 25), (26, 25), (27, 25), (28, 25)], '#ece6ec')
        p.pixels([(26, 25), (27, 25)], '#4a7ac8')
        p.px(26, 25, '#2a3a60')
        p.stroke([(23, 24), (29, 24)], '#3a2a34')
        p.stroke([(24, 26), (28, 26)], skin[1])
        # the cybernetic monocle
        p.fill(p.ellipse(38, 25.5, 3.6, 3.4), gold, radius=2)
        p.fill(p.ellipse(38, 25.5, 2.3, 2.2), lens, radius=2, bias=0.1 * gl)
        p.glow(37, 25, '#fff0f0', mix('#ff2030', '#ff8070', gl), r=1, strength=0.6 + 0.4 * gl)
        if 14 <= g <= 16:
            k = g - 14
            p.pixels([(36 + k, 27 - k), (37 + k, 27 - k)], '#ffffff')
        p.stroke([(41, 28), (43, 33), (42, 38)], gold[2])
        p.pixels([(43, 31), (42, 36)], gold[4])
        # nose, a thin superior smirk
        p.stroke([(32, 24), (32, 31)], skin[3])
        p.stroke([(33, 25), (33, 31)], skin[1])
        p.pixels([(31, 32), (34, 32)], skin[0])
        p.stroke([(28, 36), (32, 36), (35, 35), (36, 34)], '#7a2e3c')
        p.stroke([(29, 37), (33, 37)], skin[4])
        p.px(37, 34, skin[1])
        p.stroke([(30, 39), (33, 39)], skin[1])
    p.rim('#ff3a5a', side=1, amount=0.5)
    p.rim('#7ab0ff', side=-1, amount=0.25)
    p.outline('#0c0610')
    p.backdrop = (50, 6, 16)
    return p


# ---------------------------------------------------------------- 3. Clan of Saar
def saar(t=0.0):
    """a kindly rodent nomad, 3/4 to the right: long ears, goggles pushed up, a wrapped scarf, patched travel cloak.
    Moves: breathing, an ear twitch, whiskers tremble, a glint crosses the goggles, the scarf end swings."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    fur = ramp('#bc8650', spread=0.62, hue_shift=0.05)
    muz = ramp('#e6c69c', spread=0.45)
    ear_in = ramp('#9a6064', spread=0.5)
    coat = ramp('#6a6280', spread=0.72)
    patch = ramp('#9a3a4a', spread=0.6)
    patch2 = ramp('#a8b440', spread=0.55)
    scarf = ramp('#2a8e88', spread=0.62)
    brass = ramp('#cc9a3c', spread=0.6)
    glass = ramp('#8af0b0', spread=0.45)
    leather = ramp('#4c3226', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    tw = 2.0 if blink(t, 0.3, 2) else 0.0
    ear_prof = (0.7, 0.95, 1.0, 1.0, 0.8, 0.35)
    in_prof = (0.5, 0.9, 1.0, 0.95, 0.7, 0.2)
    with p.at(0, head):
        # the far ear, behind the skull
        b = 0.8 * wave(t, 0.2)
        pts, _ = blade_pts((37, 16), (51, -1), 6.0, b, prof=ear_prof)
        p.fill(p.poly(pts), fur, radius=3, bias=-0.12, texture=0.05, seed=57)
        pts, _ = blade_pts((38, 15), (48.5, 2), 3.2, b, prof=in_prof)
        p.fill(p.poly(pts), ear_in, radius=2, bias=-0.2, edge=False)
    with p.at(0, body):
        # a patched travel cloak
        p.fill(p.poly([(0, 66), (0, 54), (8, 48), (20, 45), (32, 47), (46, 45), (56, 48), (64, 54), (64, 66)]), coat,
               radius=9, texture=0.07, seed=50)
        for seg in ([(8, 50), (3, 58)], [(56, 50), (60, 57)], [(14, 52), (12, 62)], [(50, 53), (52, 63)]):
            p.stroke(seg, coat[1])
        p.fill(p.poly([(3, 57), (10, 55), (11, 62), (4, 63)]), patch, radius=2)
        for x, y in ((4, 56), (6, 56), (8, 55), (11, 57), (11, 59), (11, 61)):
            p.px(x, y, '#e8dcb0')
        p.fill(p.poly([(54, 56), (60, 57), (59, 62), (53, 61)]), patch2, radius=2, bias=-0.1)
        for x, y in ((54, 55), (56, 56), (58, 56), (53, 58), (53, 60)):
            p.px(x, y, '#e8dcb0')
        p.fill(p.poly([(20, 36), (40, 36), (42, 47), (18, 47)]), fur, radius=3, bias=-0.3, texture=0.1, seed=60)
        # strap, buckle, a pouch
        p.stroke([(16, 47), (38, 66)], leather[2], width=2)
        p.stroke([(17, 46), (39, 65)], leather[3])
        p.fill(p.poly([(24, 53), (28, 53), (29, 57), (25, 57)]), brass, radius=1)
        p.px(26, 55, leather[0])
        # the scarf, wrapped round the neck
        sc = p.poly([(17, 46), (22, 41), (32, 39), (43, 40), (48, 45), (44, 51), (32, 52), (21, 51)])
        p.fill(sc, scarf, radius=4, texture=0.06, seed=51)
        for seg in ([(22, 43), (27, 50)], [(29, 41), (33, 51)], [(36, 41), (39, 50)], [(42, 42), (45, 47)]):
            stroke_in(p, seg, scarf[1], sc)
            stroke_in(p, [(x - 1, y) for x, y in seg], scarf[4], sc)
        p.fill(p.poly([(24, 46), (28, 46), (28, 49), (24, 49)]), patch2, radius=1)
        p.pixels([(23, 45), (25, 45), (27, 45), (29, 47)], '#e8dcb8')
        # its loose end, hanging in front and swinging
        s = sway(t, 0.3, 1)
        tail = [(37, 47), (42, 47), (46 + s, 58), (39 + s, 60)]
        p.fill(p.poly(tail), scarf, radius=3, bias=-0.05, texture=0.06, seed=59)
        p.stroke([(40, 49), (42 + s, 58)], scarf[1])
        for k in range(4):
            p.stroke([(39 + s + k * 2, 60 - k // 2), (39 + s + k * 2, 62 - k // 2)], scarf[3] if k % 2 else scarf[2])
    with p.at(0, head):
        # round skull and a fluffy cheek
        p.fill(p.ellipse(29, 26, 13.5, 12.5), fur, radius=8, texture=0.1, seed=53)
        p.fill(p.poly([(17, 29), (14, 33), (17, 34), (15, 38), (20, 37), (21, 41), (26, 38), (30, 36)]), fur, radius=3,
               texture=0.1, seed=54)
        p.stroke([(18, 33), (21, 36)], fur[1])
        # the near ear (it twitches once a loop)
        b = 0.7 * wave(t, 0.55)
        pts, _ = blade_pts((23, 17), (6 + tw, -1 + tw), 6.8, b, prof=ear_prof)
        p.fill(p.poly(pts), fur, radius=3, texture=0.05, seed=55)
        pts, _ = blade_pts((21.5, 15.5), (9 + tw, 2.5 + tw), 3.6, b, prof=in_prof)
        p.fill(p.poly(pts), ear_in, radius=2, edge=False)
        # the snout
        p.fill(p.poly([(32, 22), (43, 24), (51, 28), (56, 32), (54, 36), (46, 38), (36, 39), (28, 35)]), muz, radius=4,
               texture=0.05, seed=56)
        p.stroke([(36, 22), (45, 25), (51, 29)], muz[4])
        p.fill(p.ellipse(55, 32, 2.2, 1.9), ramp('#d87a80'), radius=1)
        p.px(54, 31, '#ffe0e0')
        p.stroke([(54, 34), (52, 36), (47, 37)], leather[0])
        p.pixels([(49, 38), (50, 38), (49, 39), (50, 39)], '#fff6e4')
        p.px(50, 39, '#d8ccb8')
        p.stroke([(39, 39), (47, 40)], muz[1])
        p.stroke([(44, 31), (48, 32)], muz[2])
        # a big, kind eye
        p.fill(p.ellipse(36, 25.5, 3.0, 3.2), leather, flat=0, edge=False)
        p.pixels([(35, 24), (36, 24), (35, 25)], '#ffffff')
        p.pixels([(37, 27), (38, 27)], '#6ab8a0')
        p.stroke([(32, 21), (36, 20), (39, 21)], fur[1])
        p.stroke([(34, 29), (38, 29)], fur[3])
        # goggles pushed up on the brow
        p.stroke([(16, 21), (25, 18), (33, 16), (42, 17)], leather[1], width=2)
        for cx, cy, rx in ((25.5, 16, 4.6), (37, 14.5, 3.5)):
            p.fill(p.ellipse(cx, cy, rx, rx * 0.92), brass, radius=2)
            p.fill(p.ellipse(cx, cy, rx - 1.5, rx * 0.92 - 1.5), glass, radius=2, bias=0.1)
        p.stroke([(30, 15), (33, 15)], brass[1])
        g = int(t * FRAMES) % FRAMES
        if 4 <= g <= 8:
            k = g - 4
            p.pixels([(23 + k, 18 - k), (24 + k, 18 - k)], '#ffffff')
            if k >= 2:
                p.px(35 + k - 2, 16 - (k - 2), '#ffffff')
        else:
            p.px(24, 15, glass[4])
            p.px(36, 13, glass[4])
    p.rim('#ffc040', side=1, amount=0.5)
    p.rim('#40e0c0', side=-1, amount=0.28)
    p.outline('#140a0a')
    # whiskers (after the outline: fine hairs)
    with p.at(0, head):
        for (x0, y0), (x1, y1), ph in (((52, 34), (63, 29), 0.0), ((52, 35), (64, 35), 0.33), ((51, 36), (62, 40), 0.66)):
            w = sway(t, ph, 1, 3)
            p.stroke([(x0, y0), (x1, y1 + w)], '#f4ecd4')
            p.px(x1, y1 + w, '#b8a888')
    p.backdrop = (40, 16, 24)
    return p


# ---------------------------------------------------------------- 4. Embers of Muaat
def muaat(t=0.0):
    """a Gashlai: living fire sealed in a heavy Corinthian containment helm - a face of fire in the T-slot, a crest of
    flame pouring from the crown, magma light seeping from the joints. Moves: flames flicker and sway, the glow and seams
    pulse, embers rise."""
    from scipy import ndimage
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    red = ramp('#c42a12', spread=0.55, hue_shift=0.08)
    fire = ramp('#ff7414', spread=0.45, hue_shift=0.1)
    yel = ramp('#ffd44a', spread=0.35)
    suit = ramp('#3a3c52', spread=0.8)
    iron = ramp('#4a4658', spread=0.75)
    body, head = breath(t), breath(t, -0.1)
    gk = pulse(t, cycles=2)
    fs = int(t * 12) % 12
    seam = mix(fire[3], yel[4], gk)

    def lick(pts, ph, amp=1):
        """sway every other vertex (the flame tips)"""
        out = []
        for i, (x, y) in enumerate(pts):
            if i % 2 == 1:
                out.append((x + sway(t, ph + i * 0.13, amp, 2), y + sway(t, ph + i * 0.13 + 0.25, 1, 2)))
            else:
                out.append((x, y))
        return out

    with p.at(0, head):
        # the crest of living flame, three layers
        outer = [(19, 16), (10, 4), (20, 8), (17, -4), (25, 3), (26, -8), (30, 1), (33, -10), (36, 1), (40, -7), (41, 3),
                 (48, -3), (45, 8), (55, 3), (46, 16)]
        p.fill(p.poly(lick(outer, 0.0)), red, radius=3, bias=0.15, texture=0.16, seed=70 + fs)
        mid = [(21, 14), (17, 6), (23, 8), (22, -1), (27, 5), (29, -4), (32, 3), (34, -7), (36, 3), (38, -3), (40, 5),
               (44, 0), (42, 8), (48, 6), (44, 14)]
        p.fill(p.poly(lick(mid, 0.3)), fire, radius=3, bias=0.2, texture=0.14, seed=82 + fs, edge=False)
        inner = [(24, 14), (23, 6), (27, 9), (28, 1), (31, 7), (33, -2), (35, 7), (37, 1), (39, 9), (42, 5), (41, 14)]
        p.fill(p.poly(lick(inner, 0.6)), yel, radius=2, bias=0.25, texture=0.12, seed=94 + fs, edge=False)
    with p.at(0, body):
        # the exosuit: huge layered pauldrons, magma in the joints
        p.fill(p.poly([(0, 66), (0, 50), (8, 44), (22, 42), (32, 46), (42, 42), (56, 44), (64, 50), (64, 66)]), suit, radius=9,
               texture=0.05, seed=61)
        for side in (1, -1):
            plates = ([(0, 50), (3, 41), (13, 37), (23, 40), (24, 47), (12, 51), (0, 56)],
                      [(0, 58), (2, 52), (13, 49), (22, 50), (21, 56), (10, 60), (0, 63)])
            for k, pl in enumerate(plates):
                p.fill(p.poly(pl if side == 1 else mirror(pl)), suit, radius=5 - k, bias=(0.05 if side == 1 else -0.12) - 0.05 * k,
                       texture=0.04, seed=63 + k)
            for seg in ([(1, 51), (12, 47), (23, 48)], [(1, 59), (11, 55), (21, 57)]):
                p.stroke(seg if side == 1 else mirror(seg), seam)
            p.stroke([(3, 41), (13, 37)] if side == 1 else mirror([(3, 41), (13, 37)]), suit[4] if side == 1 else suit[3])
            for x, y in ((6, 44), (17, 41), (5, 54), (16, 52)):
                p.px(x if side == 1 else 64 - x, y, suit[4] if side == 1 else suit[3])
        # chest plate with a vent grille
        p.fill(p.poly([(22, 52), (42, 52), (40, 66), (24, 66)]), iron, radius=4)
        for y in (55, 58, 61):
            p.stroke([(26, y), (38, y)], mix(fire[2], yel[3], gk))
            p.stroke([(26, y + 1), (38, y + 1)], iron[0])
        p.fill(p.poly([(14, 33), (50, 33), (50, 46), (14, 46)]), suit, flat=0)
        # the gorget
        p.fill(p.poly([(17, 42), (32, 44), (47, 42), (49, 51), (32, 54), (15, 51)]), iron, radius=4, bias=0.05)
        p.stroke([(18, 47), (32, 49), (46, 47)], seam)
    with p.at(0, head):
        # the helm: a rounded dome sweeping into cheek guards
        helm = p.poly([(15, 24), (16, 17), (20, 12), (26, 11), (38, 11), (44, 12), (48, 17), (49, 24), (49, 34), (45, 42),
                       (38, 46), (26, 46), (19, 42), (15, 34)])
        p.fill(helm, suit, radius=9, texture=0.05, seed=62)
        p.stroke([(17, 22), (20, 16), (25, 13)], suit[4])
        # the open crown: a containment ring with the fire welling out of it
        p.fill(p.ellipse(32, 12, 12.5, 2.8), iron, radius=2, bias=0.1)
        p.fill(p.ellipse(32, 11.5, 10, 1.4), yel, flat=3, edge=False)
        p.stroke([(23, 11), (41, 11)], '#fff2c0')
        for x in (21, 27, 37, 43):
            p.px(x, 13, iron[4])
        # a central ridge down the brow
        p.fill(p.poly([(30, 14), (34, 14), (33, 22), (31, 22)]), suit, radius=1.5, bias=0.1)
        # the T-slot: a frowning eye-slot over a long mouth-slot, full of fire
        slot = p.poly([(19, 21), (29, 24), (35, 24), (45, 21), (44, 28), (36, 30), (35, 42), (29, 42), (28, 30), (20, 28)])
        # firelight spilling onto the helm round the slot
        d = ndimage.distance_transform_edt(~slot)
        for r, a in ((1, 0.55), (2, 0.32), (3, 0.16)):
            veil(p, helm & (d > r - 1) & (d <= r) & ~slot, mix('#ff6a10', '#ffb030', gk), a * (0.75 + 0.25 * gk), (34, 8, 8))
        p.fill(slot, fire, radius=2, bias=0.2, texture=0.14, seed=100 + fs)
        p.fill(p.poly([(23, 24), (30, 26), (34, 26), (41, 24), (40, 27), (34, 28), (33, 39), (31, 39), (30, 28), (24, 27)]), yel,
               radius=2, bias=0.2, edge=False, texture=0.1, seed=110 + fs)
        # white-hot eyes, slanted
        for pts in ([(22, 24), (29, 27)], [(42, 24), (35, 27)]):
            p.stroke(pts, '#fff6c8')
        p.pixels([(25, 25), (26, 26), (38, 26), (39, 25)], '#ffffff')
        # grille bars across the mouth-slot
        for y in (33, 36, 39):
            p.stroke([(28, y), (36, y)], iron[1])
            p.stroke([(28, y - 1), (36, y - 1)], mix(fire[3], '#ffffff', 0.3 * gk))
        # seams and rivets on the dome
        p.stroke([(16, 30), (19, 30)], seam)
        p.stroke([(48, 30), (45, 30)], seam)
        p.stroke([(22, 44), (26, 46), (38, 46), (42, 44)], seam)
        for x, y in ((18, 18), (46, 18), (17, 37), (47, 37), (24, 12), (40, 12)):
            p.px(x, y, suit[4])
    p.rim('#ffb21a', side=1, amount=0.55)
    p.rim('#ff3a20', side=-1, amount=0.35)
    p.outline('#120606')
    for x0, ph in ((6, 0.0), (12, 0.4), (52, 0.7), (58, 0.15), (20, 0.55), (45, 0.85), (33, 0.3)):
        u = (t * 2 + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(40 - u * 40)), mix('#ffe890', '#a02010', u))
    p.backdrop = (34, 8, 8)
    return p


# ---------------------------------------------------------------- 5. Emirates of Hacan
def hacan(t=0.0):
    """a proud lion merchant-lord: a great flowing mane, gold circlet and ear-rings, deep red robe under gold mantles,
    a collar of three suns. Moves: breathing, the mane's tufts sway, an ear-ring and the circlet gem glint."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    fur = ramp('#dca24c', spread=0.6, hue_shift=0.05)
    mane = ramp('#a84a24', spread=0.72)
    mane2 = ramp('#d8782e', spread=0.6)
    muz = ramp('#f4dcae', spread=0.35)
    robe = ramp('#901c30', spread=0.7)
    gold = ramp('#e4ac32', spread=0.6, hue_shift=0.04)
    gem = ramp('#ff6a20', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    g = int(t * FRAMES) % FRAMES
    cx, cy = 32, 27

    def tufts(n, a0, a1, r0, L, w, droop, ph0, rp, radius=2.5, off=0.0):
        for i in range(n):
            a = math.radians(a0 + (a1 - a0) * (i + off) / max(1, n - 1))
            base = (cx + r0 * math.cos(a), cy + r0 * math.sin(a))
            tip = (cx + L * math.cos(a), cy + L * math.sin(a) + droop * abs(math.cos(a)))
            bend = (1 if math.cos(a) < 0 else -1) * 1.5 + 0.9 * wave(t, ph0 + i * 0.09)
            pts, mid = blade_pts(base, tip, w, bend, prof=(0.6, 1.0, 1.0, 0.85, 0.55, 0.0))
            p.fill(p.poly(pts), rp, radius=radius, texture=0.08, seed=70 + i)
            p.stroke(mid[2:-1], rp[1])

    with p.at(0, head):
        # the mane, back layer: long dark tufts all round, the lowest falling to the shoulders
        tufts(15, 140, 400, 9, 27, 5.0, 7, 0.0, mane)
        p.fill(p.poly([(19, 36), (45, 36), (47, 52), (17, 52)]), mane, radius=4, bias=-0.2, texture=0.12, seed=69)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 53), (10, 47), (22, 45), (32, 48), (42, 45), (54, 47), (64, 53), (64, 66)]), robe, radius=9,
               texture=0.05, seed=72)
        p.stroke([(26, 50), (30, 66)], robe[1])
        p.stroke([(38, 50), (34, 66)], robe[1])
        # gold mantles on the shoulders, engraved
        for side in (1, -1):
            pl = [(0, 54), (2, 48), (10, 44), (19, 45), (21, 50), (12, 55), (0, 60)]
            p.fill(p.poly(pl if side == 1 else mirror(pl)), gold, radius=4, bias=0 if side == 1 else -0.12, texture=0.04)
            e = [(2, 55), (11, 51), (19, 50)]
            p.stroke(e if side == 1 else mirror(e), gold[1])
            for x, y in ((6, 50), (13, 48)):
                p.px(x if side == 1 else 64 - x, y, gem[3])
    with p.at(0, head):
        # front layer of the mane: shorter, brighter tufts framing the face
        tufts(12, 160, 380, 8, 21, 4.2, 4, 0.4, mane2, radius=2, off=0.5)
        # round ears set in the mane, each with a gold ring
        for ex, b in ((17, 0.0), (47, -0.15)):
            p.fill(p.ellipse(ex, 13, 3.6, 3.4), fur, radius=2, bias=b)
            p.fill(p.ellipse(ex, 13.5, 1.8, 1.7), mane, flat=0, edge=False)
        p.stroke([(15, 16), (14, 18), (15, 20), (17, 20), (18, 18)], gold[3])
        p.stroke([(49, 16), (50, 18), (49, 20), (47, 20), (46, 18)], gold[2])
        if 9 <= g <= 11:
            p.glow(15, 19, '#ffffff', '#ffe890', r=0, strength=0.9 - 0.25 * abs(g - 10))
    with p.at(0, body):
        # the collar of three suns, over the mane
        p.fill(p.poly([(17, 47), (32, 52), (47, 47), (45, 54), (32, 59), (19, 54)]), gold, radius=3)
        for x, y in ((23, 53), (32, 56), (41, 53)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 2.8, 2.8), gem, radius=2)
            p.px(x - 1, y - 1, '#fff0c0')
        p.stroke([(19, 55), (32, 60), (45, 55)], gold[1])
    with p.at(0, head):
        # the face: broad brow, long nose bridge, a full muzzle
        face = p.poly([(21, 15), (43, 15), (46, 23), (45, 31), (40, 40), (32, 44), (24, 40), (19, 31), (18, 23)])
        p.fill(face, fur, radius=8, texture=0.05, seed=73)
        p.fill(p.poly([(29, 21), (35, 21), (36, 31), (28, 31)]), fur, radius=3, bias=0.18, edge=False)
        p.stroke([(23, 31), (26, 36)], fur[1])
        p.stroke([(41, 31), (38, 36)], fur[1])
        p.fill(p.ellipse(28.5, 36, 4.4, 3.2), muz, radius=3, bias=0.1, edge=False)
        p.fill(p.ellipse(35.5, 36, 4.4, 3.2), muz, radius=3, edge=False)
        p.fill(p.ellipse(32, 41, 3.2, 2.2), muz, radius=2, edge=False)
        for x, y in ((26, 36), (28, 37), (36, 37), (38, 36), (27, 35)):
            p.px(x, y, muz[0])
        # broad nose and a proud, closed smile
        p.fill(p.poly([(28, 30), (36, 30), (35, 32), (32, 34), (29, 32)]), ramp('#5a2a24'), radius=2)
        p.px(30, 31, '#b07a6a')
        p.stroke([(32, 34), (32, 36)], '#4a2018')
        p.stroke([(28, 38), (30, 38), (32, 36), (34, 38), (36, 38)], '#4a2018')
        p.pixels([(27, 37), (37, 37)], '#4a2018')
        # heavy brows over steady amber eyes
        p.stroke([(21, 23), (25, 22), (29, 24)], fur[0], width=2)
        p.stroke([(35, 24), (39, 22), (43, 23)], fur[0], width=2)
        for x0, side in ((23, 1), (36, -1)):
            p.stroke([(x0, 25), (x0 + 5, 25)], '#2a1008')
            p.pixels([(x0 + i, 26) for i in range(1, 5)], '#ffb020')
            p.px(x0 + 3, 26, '#2a1008')
            p.px(x0 + 2, 26, '#fff0a0')
            p.pixels([(x0, 26), (x0 + 5, 26)], '#5a2a10')
            p.stroke([(x0 + 1, 27), (x0 + 4, 27)], fur[1])
        p.stroke([(29, 27), (30, 30)], fur[1])
        p.stroke([(35, 27), (34, 30)], fur[1])
        # the circlet and its sun-gem
        p.fill(p.poly([(20, 18), (32, 15), (44, 18), (44, 20), (32, 17), (20, 20)]), gold, radius=2)
        p.fill(p.ellipse(32.5, 17, 2.7, 2.7), gem, radius=2)
        gk = pulse(t, 0.6, 1)
        p.glow(32, 16, '#fff4d0', mix('#ff8a30', '#ffd060', gk), r=0, strength=0.5 + 0.5 * gk)
    p.rim('#40e8ff', side=1, amount=0.42)
    p.rim('#ff5ab0', side=-1, amount=0.3)
    p.outline('#140812')
    p.backdrop = (30, 10, 40)
    return p


# ---------------------------------------------------------------- 6. Federation of Sol
def sol(t=0.0):
    """a composed human diplomat-commander in white-and-blue armour with the Sol sun, a HUD lens over one eye.
    Moves: breathing, the HUD readout scrolls and the earpiece light blinks, an eye blink."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    white = ramp('#d8e0ec', spread=0.5)
    blue = ramp('#2c5cd0', spread=0.65)
    navy = ramp('#1c2858', spread=0.7)
    skin = ramp('#dca486', spread=0.5)
    hair = ramp('#5e3e2a', spread=0.62)
    sun = ramp('#f4c840', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    g = int(t * FRAMES) % FRAMES
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (8, 44), (20, 42), (32, 46), (44, 42), (56, 44), (64, 50), (64, 66)]), navy, radius=8,
               texture=0.04, seed=80)
        p.fill(p.poly([(19, 46), (45, 46), (46, 66), (18, 66)]), white, radius=6, texture=0.03, seed=81)
        p.fill(p.poly([(29, 48), (35, 48), (36, 66), (28, 66)]), blue, radius=2)
        p.stroke([(32, 50), (32, 66)], blue[4])
        for side in (1, -1):
            pl = [(0, 53), (3, 45), (11, 41), (21, 42), (24, 50), (14, 55), (0, 61)]
            p.fill(p.poly(pl if side == 1 else mirror(pl)), white, radius=5, bias=0 if side == 1 else -0.12, texture=0.03)
            st = [(1, 53), (10, 47), (21, 47)]
            p.stroke(st if side == 1 else mirror(st), blue[2], width=2)
            st2 = [(2, 57), (12, 52), (22, 52)]
            p.stroke(st2 if side == 1 else mirror(st2), sun[2])
        # the Sol emblem: a sun with its blue world
        p.fill(p.ellipse(24.5, 57.5, 3.8, 3.8), sun, radius=2, bias=0.1)
        p.fill(p.ellipse(22, 58.5, 2.3, 2.5), blue, radius=1.5, bias=0.25)
        p.px(25, 56, '#fff8d8')
        # high collar with a gold edge
        p.fill(p.poly([(21, 40), (43, 40), (45, 48), (32, 51), (19, 48)]), blue, radius=3)
        p.stroke([(20, 47), (32, 50), (44, 47)], sun[3])
    with p.at(0, head):
        p.fill(p.poly([(26, 36), (38, 36), (38, 45), (26, 45)]), skin, radius=4, bias=-0.15)
        face = p.poly([(21, 15), (43, 15), (44, 24), (43, 31), (40, 38), (35, 42), (29, 42), (24, 38), (21, 31), (20, 24)])
        p.fill(face, skin, radius=10, bias=0.12, texture=0.02, seed=82)
        p.fill(p.ellipse(19.5, 26, 1.8, 3.2), skin, radius=1)
        p.px(19, 26, skin[1])
        # short, neat hair with a side part
        p.fill(p.poly([(19, 21), (19, 12), (23, 7), (30, 5), (38, 5), (43, 8), (45, 14), (45, 21), (43, 16), (38, 13), (30, 14),
                       (24, 15), (21, 17)]), hair, radius=4, texture=0.1, seed=83, edge=False)
        p.stroke([(19, 20), (19, 12), (23, 7), (30, 5), (38, 5), (43, 8), (45, 14), (45, 20)], hair[0])
        p.pixels([(24, 15), (25, 15), (37, 13), (40, 14)], hair[1])
        p.stroke([(25, 7), (23, 12)], hair[0])
        p.stroke([(27, 7), (33, 6), (39, 7)], hair[4])
        p.stroke([(29, 9), (36, 8)], hair[3])
        # firm, level brows
        p.stroke([(23, 22), (29, 22)], hair[0], width=2)
        p.stroke([(35, 22), (41, 22)], hair[0], width=2)
        # steady eyes (with a blink)
        if blink(t, 0.62, 2):
            p.stroke([(24, 25), (28, 25)], '#6a3a2a')
            p.stroke([(36, 25), (40, 25)], '#6a3a2a')
        else:
            for x0 in (24, 36):
                p.pixels([(x0 + i, 25) for i in range(5)], '#f2eeea')
                p.pixels([(x0 + 1, 25), (x0 + 2, 25), (x0 + 3, 25)], '#3a6ab0')
                p.px(x0 + 2, 25, '#141a2a')
                p.px(x0 + 1, 25, '#8ab8f0')
                p.stroke([(x0, 24), (x0 + 4, 24)], '#4a2a20')
        p.stroke([(25, 27), (27, 27)], skin[1])
        p.stroke([(37, 27), (39, 27)], skin[1])
        # nose, a steady near-smile, a strong chin
        p.stroke([(32, 24), (32, 31)], skin[3])
        p.stroke([(33, 25), (33, 31)], skin[1])
        p.pixels([(30, 32), (31, 33), (34, 32), (33, 33)], skin[0])
        p.stroke([(28, 35), (29, 36), (34, 36), (36, 35)], '#7a3a30')
        p.stroke([(30, 37), (34, 37)], skin[4])
        p.pixels([(27, 34), (37, 34)], skin[1])
        p.stroke([(31, 40), (33, 40)], skin[1])
        p.pixels([(25, 30), (26, 31)], skin[1])
        p.pixels([(40, 30), (39, 31)], skin[1])
        # earpiece and HUD lens
        p.fill(p.ellipse(44.5, 26, 2.4, 3.4), white, radius=2, bias=-0.1)
        p.stroke([(44, 23), (42, 23)], white[3])
        lens_m = p.poly([(34, 21), (43, 21), (43, 28), (35, 28)])
        veil(p, lens_m, '#46e0ff', 0.3, (8, 16, 44))
        p.stroke([(34, 21), (43, 21)], mix('#46e0ff', '#ffffff', 0.5))
        p.stroke([(43, 21), (43, 27)], mix('#46e0ff', '#ffffff', 0.2))
        k = (g // 2) % 4
        for i, (x, y) in enumerate([(36, 27), (37, 27), (39, 27), (40, 27), (41, 27)]):
            if (i + k) % 3:
                p.px(x, y, '#c0f8ff')
        p.px(41, 22 + (g // 3) % 4, '#e0ffff')
        if (g // 6) % 2 == 0:
            p.glow(45, 25, '#ffffff', '#46e0ff', r=0, strength=0.8)
        else:
            p.px(45, 25, '#1a6a8a')
    p.rim('#46e8ff', side=1, amount=0.5)
    p.rim('#ffb050', side=-1, amount=0.25)
    p.outline('#060a1a')
    p.backdrop = (8, 16, 44)
    return p


# ---------------------------------------------------------------- 7. Ghosts of Creuss
def ghost(t=0.0):
    """an energy being in a dark containment cowl: a faint face of light behind a silver containment band, a glowing core
    with the Creuss swirl turning round it. Moves: wisps curl up beside it, the core pulses and its swirl turns, the face
    fades in and out."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    robe = ramp('#2a2456', spread=0.8)
    robe2 = ramp('#1a1838', spread=0.8)
    steel = ramp('#8a98b8', spread=0.6)
    cyan = ramp('#46d8ff', spread=0.45)
    bg = (12, 8, 30)
    body, head = breath(t), breath(t, -0.1)
    fk = 0.5 + 0.5 * pulse(t, 0.0, 1)
    ck = pulse(t, 0.25, 2)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 54), (8, 48), (20, 45), (32, 48), (44, 45), (56, 48), (64, 54), (64, 66)]), robe2, radius=9,
               texture=0.04, seed=90)
        for seg in ([(6, 52), (3, 64)], [(14, 50), (12, 64)], [(58, 52), (61, 64)], [(50, 50), (52, 64)]):
            p.stroke(seg, robe2[1])
        # the robe parts over the core
        p.fill(p.poly([(26, 47), (38, 47), (41, 66), (23, 66)]), robe2, flat=0, edge=False)
        veil(p, p.ellipse(32, 57, 9, 8) & p.poly([(26, 47), (38, 47), (41, 66), (23, 66)]), '#3a8ad8', 0.25 + 0.15 * ck, bg)
        p.stroke([(26, 47), (23, 66)], robe[3])
        p.stroke([(38, 47), (41, 66)], robe[1])
        # the core in its ring, the swirl turning round it
        p.fill(p.ellipse(32, 57, 5.4, 5.4), steel, radius=3)
        p.fill(p.ellipse(32, 57, 3.6, 3.6), cyan, radius=2, bias=0.2 + 0.2 * ck)
        for k in range(3):
            a0 = 2 * math.pi * (k / 3 + t / 3)
            arm = [(32 + (3.0 + 4.2 * s) * math.cos(a0 + 1.5 * s), 57 + (3.0 + 4.2 * s) * math.sin(a0 + 1.5 * s)) for s in
                   (0, 0.25, 0.5, 0.75, 1.0)]
            p.stroke(arm, mix(cyan[2], cyan[4], ck))
            p.px(int(round(arm[-1][0])), int(round(arm[-1][1])), '#e8fcff')
        p.glow(32, 57, '#ffffff', mix('#7ae8ff', '#e0fbff', ck), r=2, strength=0.6 + 0.4 * ck)
        # clasps and hem runes
        for x in (21, 43):
            p.fill(p.ellipse(x, 50, 2.0, 2.0), steel, radius=1)
            p.px(x, 50, mix('#46d8ff', '#ffffff', ck))
        for x, y in ((5, 59), (8, 57), (11, 56), (53, 56), (56, 57), (59, 59)):
            p.px(x, y, cyan[1])
    with p.at(0, head):
        # the cowl: tall, its tip folding back, draping onto the shoulders
        hood = p.poly([(9, 50), (10, 34), (13, 21), (18, 12), (25, 5), (31, 1), (37, 1), (41, 4), (46, 11), (51, 21), (54, 34),
                       (55, 50), (44, 52), (20, 52)])
        p.fill(hood, robe, radius=10, texture=0.05, seed=91)
        for seg, c in (([(17, 14), (13, 28), (12, 48)], 0), ([(23, 8), (17, 22), (16, 34)], 0), ([(47, 14), (51, 28), (52, 48)], 0),
                       ([(41, 6), (47, 20), (48, 32)], 0)):
            p.stroke(seg, robe[c])
            p.stroke([(x + 1, y) for x, y in seg], robe[3] if seg[0][0] < 32 else robe[2])
        p.stroke([(31, 2), (37, 2)], robe[4])
        for seg in ([(14, 50), (20, 46)], [(50, 50), (44, 46)]):
            p.stroke(seg, robe[1])
        # the deep cowl and the form of light within it
        cowl = p.ellipse(32, 29, 11.5, 15.5)
        p.fill(cowl, robe, flat=0, edge=False)
        d = np.hypot((p.xx + 0.5 - 32) / 8.5, (p.yy + 0.5 - (28 + head)) / 12.5)
        veil(p, cowl & (d < 1.0), '#5aa0ff', np.clip((1 - d) * 0.85 * fk, 0, 1), bg)
        veil(p, cowl & (d < 0.6), '#d8f6ff', np.clip((0.6 - d) * 1.5 * fk, 0, 1), bg)
        # wisps curling up inside the cowl
        for x0, ph in ((27, 0.0), (37, 0.5)):
            for i in range(7):
                y = 39 - i * 2 - int(t * 8 + ph * 8) % 2
                x = x0 + int(round(1.5 * math.sin(2 * math.pi * (i / 6 - t + ph))))
                m = p.ellipse(x + 0.5, y + 0.5, 0.6, 0.6) & cowl
                veil(p, m, '#bdf0ff', 0.35 * fk * (1 - i / 7), bg)
        # the faint face: soft eyes, a trace of a brow and nose, no mouth
        for ex in (28, 36):
            p.glow(ex, 27, mix('#a8d8ff', '#ffffff', fk), mix('#3a70c0', '#a0e0ff', fk), r=0, strength=0.4 + 0.6 * fk)
            p.pixels([(ex - 1, 27), (ex + 1, 27)], mix('#4a80c8', '#d0f0ff', fk))
        p.pixels([(26, 24), (27, 24), (29, 24), (35, 24), (37, 24), (38, 24)], mix('#2a4a90', '#7ab8f0', fk))
        p.pixels([(32, 29), (32, 30), (32, 31)], mix('#3060a8', '#9ad0ff', fk))
        # the cowl's inner edge catches the light
        from scipy import ndimage
        dd = ndimage.distance_transform_edt(~cowl)
        for r, a in ((1, 0.55), (2, 0.25)):
            veil(p, hood & (dd > r - 1) & (dd <= r), mix('#3a70d0', '#8ad8ff', fk), a, bg)
        # containment clasps on the cowl
        for x, y in ((13, 36), (51, 36)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 2.2, 2.2), steel, radius=1.5)
            p.px(x, y, mix(cyan[3], '#ffffff', ck))
        p.stroke([(15, 36), (19, 36)], steel[1])
        p.stroke([(49, 36), (45, 36)], steel[1])
    with p.at(0, body):
        p.stroke([(12, 50), (22, 48), (32, 49), (42, 48), (52, 50)], steel[2])
        for x in (17, 27, 37, 47):
            p.px(x, 49 if x in (27, 37) else 49, mix(cyan[2], '#ffffff', ck))
    with p.at(0, head):
        # the being's light leaking up out of the cowl
        for base, tip, w, ph in (((28, 15), (24, 3), 2.0, 0.0), ((32, 14), (32, 0), 2.4, 0.33), ((36, 15), (40, 3), 2.0, 0.66)):
            pts, mid = blade_pts(base, tip, w, 1.6 * wave(t, ph), prof=(0.8, 1.0, 0.9, 0.7, 0.4, 0.0))
            veil(p, p.poly(pts), '#7ac8ff', 0.28 * (0.5 + 0.5 * fk), bg)
            veil(p, p.line_mask(mid[:-1]), '#d8f6ff', 0.3 * (0.5 + 0.5 * fk), bg)
    p.rim('#5af0ff', side=1, amount=0.5)
    p.rim('#b060ff', side=-1, amount=0.32)
    p.outline('#06041a')
    # wisps of the being escaping, curling up beside the cowl (translucent, after the outline)
    for x0, y0, L, ph, A in ((6, 54, 34, 0.0, 2.5), (58, 52, 34, 0.5, 2.5), (4, 30, 22, 0.3, 1.5), (60, 30, 22, 0.8, 1.5)):
        u = (t + ph) % 1.0
        for k in range(5):
            uu = u - k * 0.025
            if not 0.04 < uu < 0.92:
                continue
            x = x0 + A * math.sin(2 * math.pi * (uu * 1.4 + ph))
            y = y0 - uu * L
            m = np.zeros((64, 64), bool)
            xi, yi = int(round(x)), int(round(y))
            if 0 <= xi < 64 and 0 <= yi < 64:
                m[yi, xi] = True
                fade = math.sin(math.pi * uu) * (1 - k * 0.2)
                veil(p, m, '#e8fcff' if k == 0 else '#8ad8ff', 0.9 * fade, bg)
    for x0, ph in ((14, 0.1), (50, 0.6), (22, 0.35), (44, 0.85)):
        u = (t + ph) % 1.0
        if u < 0.9:
            p.px(x0 + sway(t, ph, 1, 1), int(round(62 - u * 20)), mix('#e0fbff', '#3a4a90', u))
    p.backdrop = bg
    return p


# ---------------------------------------------------------------- 8. L1Z1X Mindnet
def l1z1x(t=0.0):
    """a gaunt cyborg of the Mindnet under a brutalist chrome cranium: half a starved face, half gunmetal with one great
    red optic, thick cables to the shoulders. Moves: the optic scans and pulses, cables sway, a codec glitch tears the
    image once a loop."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    chrome = ramp('#a8b2c4', spread=0.85)
    gun = ramp('#4a5264', spread=0.75)
    dark = ramp('#24262e', spread=0.7)
    skin = ramp('#9c90a8', spread=0.7)
    red = ramp('#e01828', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    ok = pulse(t, cycles=2)
    glow_red = mix(red[2], '#ff7070', ok)
    with p.at(0, body):
        p.fill(p.poly([(16, 30), (48, 30), (54, 44), (10, 44)]), dark, radius=4, texture=0.05, seed=104)
        for x in (20, 26, 38, 44):
            p.stroke([(x, 33), (x - (1 if x < 32 else -1) * 2, 42)], dark[3])
    with p.at(0, head):
        # thick ribbed cables from the skull to the shoulders
        for pts, ph in (([(14, 26), (10, 34), (8, 46)], 0.0), ([(50, 26), (54, 34), (56, 46)], 0.5),
                        ([(18, 28), (16, 36), (17, 46)], 0.25), ([(46, 28), (48, 36), (47, 46)], 0.75)):
            bx = sway(t, ph, 1)
            pp = [pts[0], (pts[1][0] + bx, pts[1][1]), pts[2]]
            p.fill(tube(p, pp, 1.7), gun, radius=1.5, texture=0.03)
            for k in range(1, 4):
                s = k / 4
                x = pp[0][0] + (pp[2][0] - pp[0][0]) * s + bx * (1 - abs(2 * s - 1))
                y = pp[0][1] + (pp[2][1] - pp[0][1]) * s
                p.px(int(round(x)), int(round(y)), dark[0])
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 47), (6, 41), (18, 39), (26, 45), (38, 45), (46, 39), (58, 41), (64, 47), (64, 66)]), gun,
               radius=8, texture=0.05, seed=100)
        # massive pauldrons, slabbed
        for side in (1, -1):
            pl = [(0, 52), (2, 43), (14, 39), (24, 42), (23, 52), (10, 57), (0, 60)]
            p.fill(p.poly(pl if side == 1 else mirror(pl)), chrome, radius=5, bias=0 if side == 1 else -0.18, texture=0.03)
            for seg, c in (([(2, 52), (22, 47)], None), ([(3, 44), (14, 40)], 4)):
                sg = seg if side == 1 else mirror(seg)
                p.stroke(sg, glow_red if c is None else chrome[c if side == 1 else 2])
            for x, y in ((5, 47), (18, 43), (8, 55), (19, 51)):
                p.px(x if side == 1 else 64 - x, y, chrome[0])
        # the Mindnet sigil on the left pauldron
        p.fill(p.poly([(7, 48), (15, 46), (15, 52), (7, 54)]), dark, flat=0)
        p.pixels([(8, 49), (9, 49), (9, 50), (9, 51), (10, 51), (11, 51), (11, 50), (11, 49), (12, 48), (13, 48), (13, 49),
                  (13, 50), (13, 51)], red[3])
        # chest plate and a segmented neck
        p.fill(p.poly([(22, 48), (42, 48), (40, 66), (24, 66)]), gun, radius=4, bias=0.1)
        for y in (54, 58, 62):
            p.stroke([(27, y), (37, y)], dark[1])
        p.fill(p.poly([(25, 38), (39, 38), (40, 49), (24, 49)]), dark, radius=3)
        for y in (40, 43, 46):
            p.stroke([(25, y), (39, y)], gun[3])
    with p.at(0, head):
        # the brutalist cranium
        p.fill(p.poly([(14, 24), (14, 9), (19, 2), (45, 2), (50, 9), (50, 24), (45, 27), (19, 27)]), chrome, radius=7,
               texture=0.03, seed=101)
        for seg in ([(15, 11), (28, 11)], [(36, 11), (49, 11)], [(22, 3), (22, 11)], [(42, 3), (42, 11)]):
            p.stroke(seg, chrome[1])
            p.stroke([(x + (seg[0][1] == seg[1][1]) * 0, y + 1) if seg[0][1] == seg[1][1] else (x + 1, y) for x, y in seg], chrome[4])
        p.fill(p.poly([(29, 2), (35, 2), (35, 16), (29, 16)]), chrome, radius=2, bias=0.15)
        p.stroke([(29, 3), (29, 15)], chrome[4])
        # side modules with red tell-tales
        for x0, b in ((12, 0.0), (46, -0.15)):
            p.fill(p.poly([(x0, 16), (x0 + 6, 15), (x0 + 6, 30), (x0, 29)]), gun, radius=2, bias=b)
            p.px(x0 + 3, 18, glow_red)
            p.px(x0 + 3, 21, red[1])
        # a recessed brow band with vents
        p.fill(p.poly([(18, 17), (46, 17), (46, 21), (18, 21)]), dark, radius=1)
        for x in range(20, 45, 3):
            p.stroke([(x, 18), (x, 20)], dark[3])
        for x, y in ((19, 8), (45, 8), (24, 5), (40, 5), (19, 14), (45, 14)):
            p.px(x, y, chrome[0])
        # the flesh half: starved, contemptuous
        p.fill(p.poly([(19, 21), (32, 21), (32, 45), (27, 43), (22, 37), (20, 29)]), skin, radius=6, texture=0.05, seed=102)
        p.fill(p.poly([(21, 30), (26, 31), (28, 38), (24, 38)]), skin, flat=1, edge=False)
        p.stroke([(21, 29), (26, 30)], skin[4])
        p.stroke([(22, 38), (25, 42)], skin[1])
        p.fill(p.ellipse(26.5, 25.5, 3.6, 2.4), skin, flat=0, edge=False)
        p.pixels([(24, 25), (25, 25), (26, 25), (27, 25), (28, 25)], red[2])
        p.pixels([(25, 25), (26, 25), (27, 25)], mix('#ff6a6a', '#ffb0b0', ok))
        p.stroke([(23, 23), (29, 24)], skin[0])
        p.stroke([(30, 25), (31, 32)], skin[3])
        p.px(30, 33, skin[0])
        p.stroke([(25, 39), (31, 38)], '#2a1a2a')
        p.px(24, 40, skin[0])
        p.stroke([(26, 40), (30, 40)], skin[3])
        # the machine half
        p.fill(p.poly([(32, 21), (46, 21), (46, 30), (43, 38), (38, 44), (32, 46)]), gun, radius=5, bias=-0.05, seed=103)
        for x in range(34, 41, 2):
            p.stroke([(x, 37), (x, 42)], dark[0])
        p.stroke([(32, 21), (32, 46)], dark[0])
        for y in (24, 31, 38):
            p.pixels([(31, y), (33, y)], chrome[3])
        # the great optic in a heavy housing
        p.fill(p.ellipse(39, 27, 5.8, 5.4), chrome, radius=3, bias=-0.05)
        p.fill(p.ellipse(39, 27, 4.0, 3.7), dark, flat=0)
        p.fill(p.ellipse(39, 27, 3.2, 2.9), red, radius=3, bias=0.1 * ok)
        sx = 39 + sway(t, 0.0, 1)
        p.glow(sx, 26, '#ffe8e8', mix('#ff3030', '#ff9090', ok), r=1, strength=0.6 + 0.4 * ok)
        p.px(sx, 26, '#ffffff')
        p.stroke([(44, 27), (46, 27)], glow_red)
    p.rim('#ff2a3a', side=1, amount=0.5)
    p.rim('#3ad0ff', side=-1, amount=0.25)
    p.outline('#060608')
    if blink(t, 0.42, 1):
        glitch(p, 23 + head, 30 + head, 2, '#ff2040', 0.3)
        glitch(p, 50, 54, -2, '#40e0ff', 0.25)
    elif blink(t, 0.46, 1):
        glitch(p, 12 + head, 15 + head, -1, '#40e0ff', 0.2)
    p.backdrop = (20, 6, 10)
    return p


# ---------------------------------------------------------------- 9. Mentak Coalition
def mentak(t=0.0):
    """a weathered pirate, 3/4 to the right: red bandana, brass cyber-eye, stitched scar, a lopsided grin with a gold
    tooth, gold hoop, high-collared coat and an amber scarf. Moves: breathing, bandana tails and scarf end sway, the
    cyber-eye pulses, the gold tooth and ear-ring glint."""
    t = t % 1.0  # exact loop: t=1 is t=0
    p = Portrait()
    skin = ramp('#c88a60', spread=0.6)
    stub = ramp('#5a4038', spread=0.5)
    coat = ramp('#2e3650', spread=0.8)
    band = ramp('#c42a3a', spread=0.65)
    scarf = ramp('#e0962a', spread=0.6)
    gold = ramp('#eab032', spread=0.55)
    brass = ramp('#b8883a', spread=0.6)
    lining = ramp('#7a1a2a', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    g = int(t * FRAMES) % FRAMES
    gk = pulse(t, cycles=1)
    with p.at(0, head):
        # bandana tails flowing back from the knot
        for base, tip, w, ph in (((17, 18), (4, 27), 2.8, 0.0), ((17, 20), (7, 33), 2.6, 0.35)):
            pts, mid = blade_pts(base, tip, w, 1.4 * wave(t, ph), prof=(0.8, 1.0, 0.95, 0.85, 0.65, 0.3))
            p.fill(p.poly(pts), band, radius=2, bias=-0.1)
            p.stroke(mid[1:-1], band[1])
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (8, 46), (20, 43), (32, 46), (46, 43), (56, 46), (64, 52), (64, 66)]), coat, radius=9,
               texture=0.05, seed=110)
        p.fill(p.poly([(16, 32), (48, 32), (50, 48), (14, 48)]), coat, flat=0)
        p.fill(p.poly([(24, 47), (44, 47), (40, 66), (28, 66)]), lining, radius=3)
        p.stroke([(9, 50), (6, 62)], coat[1])
        p.stroke([(55, 50), (58, 62)], coat[1])
        for y in (52, 57, 62):
            p.px(25 + (y - 52) // 4, y, gold[4])
            p.px(43 - (y - 52) // 4, y, gold[3])
    with p.at(0, head):
        # the high collar standing up round the jaw
        p.fill(p.poly([(10, 52), (12, 32), (19, 29), (25, 46)]), coat, radius=3, texture=0.05, seed=115)
        p.stroke([(18, 30), (23, 45)], lining[3], width=2)
        p.stroke([(12, 33), (18, 30)], gold[3])
        p.fill(p.poly([(55, 52), (53, 35), (47, 36), (43, 47)]), coat, radius=3, bias=-0.15)
        p.stroke([(48, 37), (45, 46)], lining[2])
        p.stroke([(53, 35), (47, 36)], gold[2])
        p.fill(p.poly([(26, 38), (40, 38), (41, 47), (25, 47)]), skin, radius=4, bias=-0.3)
    with p.at(0, body):
        # the scarf knotted at the throat, its end swinging
        s = sway(t, 0.2, 1)
        p.fill(p.poly([(37, 47), (43, 47), (47 + s, 59), (41 + s, 61)]), scarf, radius=2, bias=-0.1, texture=0.06, seed=116)
        p.stroke([(40, 49), (44 + s, 59)], scarf[1])
        p.fill(p.poly([(24, 43), (32, 45), (43, 43), (45, 49), (34, 52), (25, 49)]), scarf, radius=3, texture=0.04, seed=111)
        p.stroke([(27, 46), (33, 49)], scarf[1])
        p.stroke([(34, 45), (41, 44)], scarf[4])
    with p.at(0, head):
        # the head, turned to the right: brow, nose and chin in profile
        hm = p.poly([(18, 26), (19, 17), (24, 11), (32, 8), (40, 9), (45, 14), (46, 20), (47, 24), (50, 30), (49, 32), (46, 33),
                     (46, 35), (45, 38), (43, 42), (38, 45), (32, 46), (26, 43), (21, 37), (18, 31)])
        p.fill(hm, skin, radius=8, texture=0.08, seed=112)
        jaw = p.poly([(20, 33), (25, 41), (32, 45), (38, 44), (43, 41), (45, 36), (41, 39), (36, 40), (30, 39), (24, 36)])
        p.fill(jaw, stub, radius=2, edge=False, texture=0.3, seed=113)
        # near ear and its gold hoop
        p.fill(p.ellipse(20.5, 27, 2.4, 3.6), skin, radius=1.5)
        p.px(20, 27, skin[1])
        p.stroke([(19, 31), (17, 33), (18, 35), (20, 35), (21, 33)], gold[3])
        if 16 <= g <= 18:
            p.glow(18, 35, '#ffffff', '#ffe890', r=0, strength=0.9 - 0.25 * abs(g - 17))
        # the bandana and its knot
        p.fill(p.poly([(17, 22), (18, 13), (24, 7), (33, 5), (41, 7), (46, 13), (47, 18), (41, 16), (33, 16), (25, 18), (19, 22)]),
               band, radius=4, texture=0.06, seed=114)
        p.fill(p.ellipse(17.5, 19, 2.8, 2.6), band, radius=2, bias=-0.1)
        for x, y in ((24, 10), (30, 8), (37, 8), (27, 13), (34, 12), (42, 12), (21, 16)):
            p.px(x, y, '#f4d4c8')
        p.stroke([(19, 21), (25, 18), (33, 16), (41, 16), (47, 18)], band[1])
        # the brass cyber-eye (near), wired back to the temple
        p.stroke([(27, 24), (22, 22)], '#2a2420')
        p.fill(p.ellipse(31, 24.5, 3.9, 3.7), brass, radius=2)
        p.fill(p.ellipse(31, 24.5, 2.2, 2.2), ramp('#ff8a20'), radius=2)
        p.glow(31, 24, '#fff4c0', mix('#ff8a20', '#ffd060', gk), r=1, strength=0.55 + 0.45 * gk)
        p.px(28, 22, brass[4])
        # the far eye: narrowed, sly, crow's feet
        p.stroke([(37, 21), (43, 22)], stub[0])
        p.pixels([(39, 24), (40, 24), (41, 24), (42, 24)], '#f0e4dc')
        p.pixels([(40, 24), (41, 24)], '#3a2418')
        p.stroke([(38, 23), (43, 23)], '#3a2018')
        p.pixels([(44, 24), (44, 26), (39, 25), (42, 25)], skin[1])
        # nose
        p.stroke([(43, 22), (46, 29)], skin[3])
        p.pixels([(45, 31), (46, 31)], skin[0])
        p.stroke([(42, 32), (45, 32)], skin[1])
        # a stitched scar down the near cheek
        p.stroke([(25, 28), (29, 37)], '#e09a90')
        p.stroke([(26, 28), (30, 37)], skin[1])
        for x, y in ((25, 30), (27, 33), (28, 36)):
            p.px(x, y, '#5a2a24')
        # a lopsided extorting grin with a gold tooth
        p.stroke([(36, 33), (44, 32)], skin[1])
        p.pixels([(35, 34), (36, 34), (37, 34), (38, 34), (39, 34)], '#f0e4c8')
        p.px(37, 34, gold[4])
        p.px(37, 35, gold[2])
        if 4 <= g <= 6:
            p.glow(37, 34, '#ffffff', '#ffe080', r=0, strength=0.85)
        p.stroke([(33, 32), (34, 34), (36, 35), (40, 35), (45, 33)], '#3a1810')
        p.stroke([(37, 36), (43, 35)], skin[3])
        p.pixels([(32, 31), (32, 32)], skin[1])
        p.stroke([(37, 39), (42, 39)], skin[3])
    p.rim('#ffa030', side=1, amount=0.5)
    p.rim('#40e0d0', side=-1, amount=0.3)
    p.outline('#08060a')
    p.backdrop = (6, 24, 30)
    return p


ALL = {k: globals()[k] for k in ('arborec', 'letnev', 'saar', 'muaat', 'hacan', 'sol', 'ghost', 'l1z1x', 'mentak')}


def render(p, scale):
    return p.image(scale, bg=tuple(getattr(p, 'backdrop', (12, 14, 30))) + (255,))


def make_sheet(keys, path):
    from PIL import Image, ImageDraw
    S = 256
    cols = 3
    rows = (len(keys) + cols - 1) // cols
    W = cols * (2 * S + 40) + 10
    H = rows * (S + 34) + 10
    sheet = Image.new('RGBA', (W, H), (235, 235, 240, 255))
    d = ImageDraw.Draw(sheet)
    for i, k in enumerate(keys):
        x0 = 10 + (i % cols) * (2 * S + 40)
        y0 = 10 + (i // cols) * (S + 34)
        for j, tt in enumerate((0.0, 0.5)):
            sheet.alpha_composite(render(ALL[k](tt), 4), (x0 + j * (S + 6), y0))
        d.text((x0, y0 + S + 4), k, fill=(20, 20, 30, 255))
    sheet.save(path)


def review(k, path, scale=6, ts=(0.0, 0.25, 0.5, 0.75)):
    from PIL import Image
    ims = [render(ALL[k](tt), scale) for tt in ts]
    W = sum(im.width for im in ims) + 8 * (len(ims) - 1)
    out = Image.new('RGBA', (W, ims[0].height), (255, 255, 255, 255))
    x = 0
    for im in ims:
        out.alpha_composite(im, (x, 0))
        x += im.width + 8
    out.save(path)


if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] == 'review':
        for k in args[1:]:
            review(k, '/tmp/b1rev_%s.png' % k)
    else:
        keys = args or list(ALL)
        make_sheet(list(ALL), D + '/sheet_faces_b1.png')
        for k in keys:
            save_gif(ALL[k], D + '/anim_%s.gif' % k, scale=4)
