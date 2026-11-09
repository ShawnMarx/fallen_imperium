import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import math  # noqa: E402

import numpy as np  # noqa: E402
from scipy import ndimage  # noqa: E402

from pix import Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, save_gif  # noqa: E402

D = __import__('os').path.dirname(__import__('os').path.abspath(__file__))


# ---- helpers (local to this file)
def M(pts):
    """mirror polygon points about the vertical centre line"""
    return [(64 - x, y) for x, y in pts]


def MP(pts):
    """mirror pixel coordinates about the vertical centre line"""
    return [(63 - x, y) for x, y in pts]


def bez(a, b, c, n=12):
    out = []
    for i in range(n + 1):
        s = i / n
        out.append(((1 - s) ** 2 * a[0] + 2 * (1 - s) * s * b[0] + s * s * c[0],
                    (1 - s) ** 2 * a[1] + 2 * (1 - s) * s * b[1] + s * s * c[1]))
    return out


def taper(a, b, c, w0, w1=0.6, n=14):
    """a tapered curved spike / horn / strand along a quadratic curve a -> c (control b)"""
    pts = bez(a, b, c, n)
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        j0, j1 = max(0, i - 1), min(n, i + 1)
        dx, dy = pts[j1][0] - pts[j0][0], pts[j1][1] - pts[j0][1]
        ln = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / ln, dx / ln
        w = (w0 + (w1 - w0) * i / n) / 2
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    return left + right[::-1]


def drift(p, t, specs, rise=24, y0=62, speed=1, fade=('#ffffff', '#000000'), cut=0.85, amp=1):
    """loose motes drifting up on (t*speed+phase)%1 (drawn after the outline)"""
    for x0, ph in specs:
        u = (t * speed + ph) % 1.0
        if u < cut:
            p.px(x0 + sway(t, ph, amp, cycles=speed), int(round(y0 - u * rise)), mix(fade[0], fade[1], u / cut))


# =====================================================================================================
def kolume(t=0.0):
    """Monks of Kolume: a contemplative robot in an orange robe, a brass dome and a calm cyan visor.
    Moves: slow breathing sway (the head rocks a beat later), the visor glow pulses, motes drift up."""
    p = Portrait()
    robe = ramp('#e8782a', spread=0.72, hue_shift=0.07)
    inner = ramp('#7a2a28', spread=0.6)
    brass = ramp('#b8843c', spread=0.85, hue_shift=0.06)
    dark = ramp('#2c2c40', spread=0.7)
    plate = ramp('#5a4644', spread=0.7)
    cyan = ramp('#46e6ff', spread=0.4)
    body, head = breath(t), breath(t, -0.12)
    rock = sway(t, 0.0, 1)
    g = pulse(t, 0.1, 1)
    with p.at(0, body):
        # the cowl, fallen back around the neck, rising behind the head; dark inside
        p.fill(p.poly([(8, 52), (8, 36), (13, 24), (22, 17), (32, 15), (42, 17), (51, 24), (56, 36), (56, 52), (44, 46),
                       (32, 44), (20, 46)]), robe, radius=6, bias=-0.05, texture=0.05, seed=31)
        p.fill(p.poly([(13, 46), (14, 33), (19, 24), (26, 20), (32, 19), (38, 20), (45, 24), (50, 33), (51, 46), (44, 43),
                       (32, 41), (20, 43)]), inner, radius=4, bias=-0.2)
        p.stroke([(9, 50), (9, 36), (14, 25), (22, 18)], robe[4])
        p.stroke([(55, 50), (55, 36), (50, 25)], robe[1])
        # the robe over the shoulders
        p.fill(p.poly([(0, 66), (0, 51), (8, 45), (20, 42), (32, 45), (44, 42), (56, 45), (64, 51), (64, 66)]), robe, radius=9,
               texture=0.02, seed=32)
        # chest plate in the robe's V: circuit traces and the core
        p.fill(p.poly([(21, 46), (43, 46), (38, 66), (26, 66)]), plate, radius=4, texture=0.03)
        tc = mix('#a8481a', '#ffa040', g)
        for seg in ([(24, 49), (27, 49), (27, 54), (29, 54)], [(40, 49), (38, 49), (38, 52), (35, 52), (35, 54)],
                    [(31, 48), (31, 51), (33, 51)], [(26, 58), (28, 58)], [(38, 57), (36, 57)]):
            p.stroke(seg, tc)
        p.pixels([(24, 49), (40, 49), (31, 48), (26, 58), (38, 57), (33, 51)], mix('#ffb060', '#fff0c0', g))
        p.fill(p.poly([(29, 56), (35, 56), (35, 63), (29, 63)]), dark, radius=2)
        for x in (30, 32, 34):
            p.stroke([(x, 58), (x, 61)], mix(cyan[1], cyan[3], g))
        p.glow(32, 59, '#ffffff', '#7ff0ff', r=1, strength=0.3 + 0.45 * g)
        # robe folds and lapels
        p.stroke([(21, 46), (26, 66)], robe[0])
        p.stroke([(43, 46), (38, 66)], robe[0])
        p.stroke([(20, 47), (24, 64)], robe[4])
        p.stroke([(3, 60), (9, 53), (17, 50)], robe[1])
        p.stroke([(4, 61), (10, 55)], robe[3])
        p.stroke([(61, 60), (55, 53), (47, 50)], robe[1])
        p.stroke([(12, 64), (15, 57)], robe[1])
        p.stroke([(52, 64), (49, 57)], robe[0])
        # segmented neck
        p.fill(p.poly([(28, 34), (36, 34), (37, 46), (27, 46)]), dark, radius=3)
        for y in (40, 43):
            p.stroke([(28, y), (36, y)], dark[3])
    with p.at(rock, head):
        # ear pods with short horns
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            cx = 19.5 if s == 1 else 44.5
            p.fill(p.poly(f([(19, 17), (11, 15), (12, 18), (18, 22)])), brass, radius=2, bias=0.15 if s == 1 else -0.1)
            p.fill(p.ellipse(cx, 21, 3.2, 5), brass, radius=2.5, bias=0.0 if s == 1 else -0.18)
            p.fill(p.ellipse(cx, 21, 1.5, 2.6), dark, radius=1.5)
            p.px(int(cx) - 1 if s == 1 else int(cx), 19, cyan[2])
        # the head: a polished dome narrowing to a jaw
        hd = p.ellipse(32, 20, 12.5, 15) | p.poly([(20, 22), (44, 22), (43, 31), (38, 37), (26, 37), (21, 31)])
        p.fill(hd, brass, radius=6, texture=0.02, seed=33)
        p.stroke([(37, 6), (41, 9), (43, 13)], brass[2])
        p.stroke([(40, 10), (42, 13), (43, 17)], mix(brass[1], cyan[2], 0.45))
        p.stroke([(24, 9), (26, 7), (29, 6)], '#fff8e8')
        p.pixels([(24, 10), (23, 11), (31, 5)], brass[4])
        # warm light bounced up from the robe onto the jaw, a seam at the temples
        p.stroke([(22, 30), (25, 34), (28, 36)], mix(brass[2], robe[3], 0.6))
        p.stroke([(42, 30), (40, 33)], mix(brass[1], robe[2], 0.5))
        p.stroke([(20, 17), (21, 13), (24, 10)], brass[2])
        p.stroke([(44, 17), (43, 13)], brass[1])
        # the visor: a dark band of glass, eyes closed in contentment, a light that comes and goes
        p.fill(p.poly([(21, 20), (43, 20), (43, 26), (40, 28), (24, 28), (21, 26)]), dark, radius=2)
        p.stroke([(22, 19), (32, 18), (42, 19)], brass[4])
        p.stroke([(23, 21), (41, 21)], mix(dark[1], cyan[1], g))
        for cx in (27, 37):
            arc = [(cx - 2, 23), (cx - 1, 24), (cx, 24), (cx + 1, 24), (cx + 2, 23)]
            p.pixels(arc, mix(cyan[2], cyan[4], g))
            p.pixels([(cx - 1, 24), (cx, 24), (cx + 1, 24)], mix(cyan[3], '#ffffff', g))
            p.glow(cx, 24, '#ffffff', '#9ff4ff', r=1, strength=0.2 + 0.4 * g)
        # cheek seams and the speaking grille
        p.stroke([(24, 29), (26, 34), (29, 36)], brass[1])
        p.stroke([(40, 29), (38, 34), (35, 36)], brass[0])
        p.fill(p.poly([(29, 30), (35, 30), (35, 35), (29, 35)]), dark, radius=1.5)
        for y in (31, 33):
            p.stroke([(30, y), (34, y)], dark[3])
        p.stroke([(32, 28), (32, 29)], brass[1])
    p.rim('#46e6ff', side=1, amount=0.45)
    p.rim('#ff5ab4', side=-1, amount=0.22)
    p.outline('#0a0c14')
    drift(p, t, ((5, 0.0), (13, 0.45), (51, 0.2), (59, 0.7), (46, 0.9), (18, 0.62)), rise=34, y0=58,
          fade=('#ffe2a0', '#3a6080'))
    p.backdrop = (8, 30, 36)
    return p


# =====================================================================================================
def kortali(t=0.0):
    """Kortali Tribunal: an insect queen, a flared hood of carapace crowned with spines, violet veins and eyes.
    Moves: breath, the spines sway, veins and eyes pulse, the mandibles twitch, the cyan heart-vein flows."""
    p = Portrait()
    chit = ramp('#6c5a90', spread=0.85)
    hood = ramp('#3c2850', spread=0.8)
    dark = ramp('#2c1e3c', spread=0.75)
    edge = ramp('#ff6a2a', spread=0.5)
    vio = ramp('#c060ff', spread=0.45)
    cyan = ramp('#3ff0e0', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    g = pulse(t, 0.0, 2)

    def spine(a, b, c, w, s, bias):
        pts = taper(a, b, c, w, 0.8)
        p.fill(p.poly(pts if s == 1 else M(pts)), hood, radius=2.5, bias=bias)
        line = bez(a, b, c, 10)
        line = [(x - 0.9, y - 0.4) for x, y in line[1:8]]
        p.stroke(line if s == 1 else M(line), edge[3] if s == 1 else edge[1])

    with p.at(0, body):
        # spiked shoulder carapace, glossy
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            bias = 0.0 if s == 1 else -0.15
            for a, b, c in (((7, 44), (3, 37), (2, 28)), ((16, 40), (15, 33), (17, 26))):
                spine(a, b, c, 6, s, bias)
            p.fill(p.poly(f([(0, 66), (0, 46), (6, 41), (18, 38), (27, 43), (28, 52), (24, 66)])), hood, radius=6, bias=bias,
                   texture=0.04, seed=41)
            p.stroke(f([(1, 46), (7, 42), (18, 39), (26, 44)]), edge[3] if s == 1 else edge[1])
            p.stroke(f([(3, 50), (10, 46), (17, 45)]), hood[4] if s == 1 else hood[3])
            p.stroke(f([(4, 49), (8, 47)]), '#e8d8ff' if s == 1 else hood[4])
            for i, (x, y) in enumerate(((6, 53), (12, 50), (9, 59))):
                p.px(*(MP([(x, y)])[0] if s == -1 else (x, y)), mix(vio[1], '#f0c8ff', pulse(t, 0.13 * i + 0.4 * (s == -1), 2)))
            p.stroke(f([(4, 57), (14, 51), (23, 53)]), hood[0])
            p.stroke(f([(5, 63), (13, 58), (22, 60)]), hood[0])
        # thorax and the cyan heart-vein
        p.fill(p.poly([(23, 40), (41, 40), (42, 66), (22, 66)]), dark, radius=5)
        for y in (48, 54, 60):
            p.stroke([(24, y), (31, y + 2)], dark[0])
            p.stroke([(40, y), (33, y + 2)], dark[0])
            p.stroke([(24, y + 1), (30, y + 3)], dark[3])
            p.stroke([(40, y + 1), (34, y + 3)], dark[2])
        vein = [(32, 44), (31, 47), (33, 50), (31, 53), (33, 56), (32, 59), (31, 62), (33, 65)]
        p.stroke(vein, cyan[2])
        fr = int(round(t * 24)) % 24
        vm = p.line_mask(vein)
        for i in range(3):
            yy = 42 + ((fr + i * 8) % 24)
            xs = [x for x in range(28, 37) if 0 <= yy < 64 and vm[yy, x]]
            if xs:
                p.px(xs[0] - p.dx, yy - p.dy, '#e8ffff')
        p.glow(32, 50, '#ffffff', '#6ff8f0', r=1, strength=0.3 + 0.4 * g)
    with p.at(0, head):
        # the spine crown, three pairs; the tips sway
        for i, (a, b, c, w) in enumerate((((25, 9), (21, 1), (16, -4), 5), ((17, 16), (8, 11), (2, 3), 6),
                                          ((13, 27), (4, 26), (0, 17), 5.5))):
            dx = sway(t, 0.15 * i, 1)
            for s in (1, -1):
                spine(a, b, (c[0] + dx * s, c[1]), w, s, 0.05 if s == 1 else -0.15)
        # the hood of carapace behind the head, orange-lit edges, ribbed inside
        hp = [(32, 4), (42, 8), (50, 15), (54, 26), (52, 37), (44, 34), (32, 32), (20, 34), (12, 37), (10, 26), (14, 15), (22, 8)]
        p.fill(p.poly(hp), hood, radius=5, texture=0.04, seed=42)
        p.fill(p.poly([(29, 7), (32, -3), (35, 7)]), hood, radius=2)
        p.stroke([(29, 6), (32, -2)], edge[3])
        p.stroke([(12, 36), (10, 26), (14, 15), (22, 8), (32, 4)], edge[3])
        p.stroke([(52, 36), (54, 26), (50, 15), (42, 8), (32, 5)], edge[1])
        for a, b in (((20, 12), (24, 18)), ((15, 21), (21, 23)), ((14, 30), (21, 29)), ((44, 12), (40, 18)), ((49, 21), (43, 23)),
                     ((50, 30), (43, 29))):
            p.stroke([a, b], hood[0])
        for a, b in (((19, 13), (23, 18)), ((14, 22), (20, 23)), ((13, 31), (20, 30))):
            p.stroke([a, b], hood[3])
        # bioluminescent spots along the hood
        for i, (x, y) in enumerate(((17, 15), (14, 24), (15, 32), (46, 15), (49, 24), (48, 32))):
            k2 = pulse(t, 0.17 * i, 2)
            p.px(x, y, mix(vio[1], '#f0c8ff', k2))
        # neck
        p.fill(p.poly([(27, 36), (37, 36), (39, 46), (25, 46)]), hood, radius=3, bias=-0.1)
        for y in (41, 44):
            p.stroke([(27, y), (37, y)], hood[0])
        # the long head
        hd = [(25, 13), (32, 9), (39, 13), (43, 21), (41, 30), (36, 38), (32, 43), (28, 38), (23, 30), (21, 21)]
        p.fill(p.poly(hd), chit, radius=6, bias=0.08, texture=0.04, seed=43)
        p.stroke([(32, 10), (32, 21)], chit[4])
        p.stroke([(33, 11), (33, 21)], chit[1])
        p.stroke([(26, 14), (23, 19)], chit[4])
        p.pixels([(27, 13), (24, 16)], '#f0e8ff')
        # violet veins branching from the crest
        vc = mix(vio[1], vio[3], g)
        for seg in ([(31, 12), (28, 15), (25, 16)], [(34, 12), (37, 15), (40, 16)], [(29, 15), (29, 19)],
                    [(36, 15), (36, 19)], [(32, 21), (32, 26)], [(31, 17), (27, 19)], [(34, 17), (38, 19)]):
            p.stroke(seg, vc)
        p.pixels([(28, 15), (37, 15), (32, 23)], mix(vio[3], '#ffffff', g))
        # cheek plates down to a pointed chin
        p.stroke([(23, 30), (27, 33), (31, 41)], chit[1])
        p.stroke([(41, 30), (37, 33), (33, 41)], chit[0])
        p.stroke([(24, 29), (28, 32)], chit[4])
        # great slanted compound eyes and two small ones above
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            p.fill(p.poly(f([(21, 20), (30, 24), (29, 28), (24, 29), (21, 25)])), dark, flat=0, edge=False)
            p.fill(p.poly(f([(22, 21), (29, 24), (28, 27), (24, 28), (22, 25)])), vio, radius=1.5, bias=0.1 + 0.2 * g)
            facets = [(23, 23), (25, 25), (27, 25), (24, 26), (26, 27)]
            p.pixels(facets if s == 1 else MP(facets), mix(vio[1], vio[2], g))
            ex = 26 if s == 1 else 37
            p.glow(ex, 24, '#ffffff', mix('#c070ff', '#f0c8ff', g), r=0, strength=0.6 + 0.4 * g)
            sx = 28 if s == 1 else 35
            p.glow(sx, 18, '#f0d8ff', '#b45cff', r=0, strength=0.4 + 0.4 * g)
        # mandibles (twitch now and then)
        tw = 1 if (blink(t, 0.3, 2) or blink(t, 0.42, 2)) else 0
        for s in (1, -1):
            pts = taper((28, 36), (21 + tw, 41), (27 + tw, 48), 3.6, 0.7)
            p.fill(p.poly(pts if s == 1 else M(pts)), chit, radius=1.5, bias=0.3 if s == 1 else 0.1)
            hl = [(26, 38), (23, 41), (24, 44)]
            p.stroke(hl if s == 1 else MP(hl), edge[3] if s == 1 else edge[1])
        p.stroke([(30, 39), (34, 39)], '#120818')
    p.rim('#c070ff', side=1, amount=0.5)
    p.rim('#ff7a3a', side=-1, amount=0.3)
    p.outline('#0c0612')
    p.backdrop = (10, 14, 40)
    return p


# =====================================================================================================
def kyro(t=0.0):
    """Kyro Sodality: a gaunt skeletal zealot, a long ridged skull, a plague-glow under the skin, a rotten grin.
    Moves: breath, the back tendrils sway, the sickly glow pulses, spores drift up."""
    p = Portrait()
    skin = ramp('#647e6a', spread=0.72, hue_shift=0.05)
    sinew = ramp('#3c4c44', spread=0.7)
    mantle = ramp('#4a2a4c', spread=0.75)
    acid = ramp('#b4ff3c', spread=0.4)
    body, head = breath(t), breath(t, -0.12)
    g = pulse(t, 0.0, 2)
    gc = mix('#6ac02a', '#d8ff70', g)
    with p.at(0, head):
        # tendrils hanging from the back of the skull
        for i, (a, b, c, w) in enumerate((((18, 18), (8, 20), (4, 32), 4.5), ((20, 25), (13, 32), (11, 44), 4))):
            dx = sway(t, 0.2 * i + 0.1, 1)
            p.fill(p.poly(taper(a, b, (c[0] + dx, c[1]), w, 0.8)), sinew, radius=2, bias=0.05)
            p.px(c[0] + dx, c[1] - 1, mix('#6ac02a', '#d8ff70', pulse(t, 0.2 * i, 2)))
    with p.at(0, body):
        # tattered mantle, a ragged collar standing up behind the neck
        p.fill(p.poly([(15, 44), (17, 33), (21, 38), (24, 30), (27, 37), (30, 33), (31, 44)]), mantle, radius=3, bias=-0.1)
        p.fill(p.poly([(42, 44), (44, 33), (47, 38), (50, 32), (52, 38), (55, 35), (56, 45)]), mantle, radius=3, bias=-0.2)
        p.fill(p.poly([(0, 66), (0, 50), (6, 44), (17, 40), (27, 43), (29, 66)]), mantle, radius=6, texture=0.06, seed=51)
        p.fill(p.poly([(64, 66), (64, 54), (60, 47), (52, 42), (45, 43), (44, 66)]), mantle, radius=6, bias=-0.1, texture=0.06,
               seed=52)
        p.stroke([(3, 52), (10, 47), (18, 46)], mantle[3])
        p.stroke([(6, 60), (14, 54), (22, 56)], mantle[0])
        p.stroke([(62, 56), (55, 50), (48, 50)], mantle[0])
        # plague stains seeping through the cloth
        for seg in ([(19, 47), (20, 52), (19, 56)], [(52, 49), (53, 54)], [(23, 58), (24, 63)]):
            p.stroke(seg, mix(acid[0], acid[2], g))
        # the bare ribbed chest
        p.fill(p.poly([(26, 44), (46, 42), (46, 66), (28, 66)]), skin, radius=5, texture=0.08, seed=53)
        for y in (48, 52, 56, 60):
            p.stroke([(28, y + 1), (33, y), (36, y + 1)], sinew[0])
            p.stroke([(38, y + 1), (41, y), (45, y + 1)], sinew[0])
            p.stroke([(29, y + 2), (33, y + 1)], skin[3])
        p.stroke([(37, 45), (37, 66)], gc)
        p.stroke([(37, 52), (40, 55)], acid[1])
        p.glow(37, 50, '#f4ffc0', '#b4ff3c', r=0, strength=0.4 + 0.5 * g)
        # ragged edges of the mantle
        for x, y in ((27, 50), (28, 58), (45, 48), (44, 57)):
            p.fill(p.poly([(x - 2, y - 3), (x + 2, y), (x - 1, y + 4)]), mantle, radius=1, flat=1)
        # the clasp: two linked rings
        p.stroke([(9, 50), (11, 48), (13, 50), (11, 52), (9, 50)], '#3ff0e0')
        p.stroke([(12, 52), (14, 50), (16, 52), (14, 54), (12, 52)], '#e8f040')
        # the thin neck with cords
        p.fill(p.poly([(31, 33), (41, 33), (42, 46), (31, 46)]), skin, radius=3, bias=-0.15, texture=0.06, seed=54)
        p.stroke([(33, 35), (34, 45)], sinew[0])
        p.stroke([(39, 35), (40, 45)], sinew[1])
        p.stroke([(36, 37), (37, 45)], gc)
        # plague sores on neck and shoulder
        for x, y in ((31, 41), (21, 44), (49, 45)):
            p.glow(x, y, '#f4ffc0', '#a0f030', r=0, strength=0.4 + 0.4 * g)
    with p.at(0, head):
        # bony spines along the back of the skull
        for a, c in (((21, 6), (14, 0)), ((16, 13), (8, 9))):
            p.fill(p.poly(taper(a, ((a[0] + c[0]) / 2, (a[1] + c[1]) / 2), c, 5, 0.6)), skin, radius=1.5, bias=-0.05)
        # the long skull, the face toward the viewer's right
        skull = [(15, 18), (17, 9), (25, 3), (35, 2), (44, 6), (49, 13), (51, 20), (50, 26), (48, 32), (46, 37), (42, 40),
                 (37, 40), (33, 36), (28, 30), (20, 27)]
        p.fill(p.poly(skull), skin, radius=8, texture=0.05, seed=55)
        # a bony crest down the middle of the cranium, veins under the skin
        crest = bez((45, 10), (34, 1), (18, 9), 12)
        p.stroke(crest, skin[4])
        p.stroke([(x, y + 1) for x, y in crest], skin[1])
        for seg in ([(30, 6), (27, 11), (28, 15), (25, 19)], [(27, 11), (22, 13)], [(38, 7), (36, 12), (32, 15)],
                    [(36, 12), (39, 15)], [(22, 17), (24, 22), (27, 24)]):
            p.stroke(seg, sinew[1])
        for seg in ([(28, 15), (25, 19)], [(36, 12), (32, 15)]):
            p.stroke(seg, gc)
        # sunken temple, cheekbone and the hollow under it
        p.stroke([(34, 19), (35, 25), (37, 29)], skin[1])
        p.stroke([(39, 26), (46, 27)], skin[4])
        p.fill(p.poly([(38, 28), (46, 28), (44, 31), (39, 31)]), skin, flat=0, edge=False)
        # heavy brow and deep sockets with burning eyes
        p.stroke([(36, 17), (43, 18), (50, 17)], skin[4])
        for cx, w in ((40, 3.2), (47.5, 2.2)):
            p.fill(p.ellipse(cx, 21.5, w, 2.4), sinew, flat=0, edge=False)
        gl = mix('#a0f030', '#e0ff60', g)
        p.pixels([(39, 21), (40, 21), (41, 21), (47, 21), (48, 21)], gc)
        p.glow(40, 21, '#ffffd0', gl, r=0, strength=0.6 + 0.4 * g)
        p.glow(48, 21, '#ffffd0', gl, r=0, strength=0.5 + 0.3 * g)
        # nasal slits
        p.pixels([(48, 25), (48, 26), (50, 25)], sinew[0])
        # the rotten grin
        p.fill(p.poly([(37, 32), (50, 30), (49, 34), (39, 35)]), sinew, flat=0, edge=False)
        for x in range(38, 50, 2):
            p.px(x, 32 if x < 44 else 31, '#d8d8a0')
            p.px(x + 1, 34 if x < 44 else 33, '#b8b880')
        p.stroke([(36, 31), (37, 32)], skin[1])
        p.stroke([(40, 36), (46, 35)], skin[3])
        # sores on the skull
        for x, y in ((26, 12), (30, 21), (22, 22), (37, 6)):
            p.glow(x, y, '#f4ffc0', '#a0f030', r=0, strength=0.35 + 0.45 * g)
        # a glowing drip from the grin
        u = (t * 2) % 1.0
        if u < 0.6:
            p.px(45, 35 + int(u * 10), acid[3] if u < 0.3 else acid[2])
    p.rim('#7aff5a', side=1, amount=0.45)
    p.rim('#c060ff', side=-1, amount=0.25)
    p.outline('#060c08')
    drift(p, t, ((8, 0.0), (18, 0.33), (54, 0.15), (59, 0.6), (47, 0.82), (24, 0.7), (35, 0.5)), rise=40, y0=60,
          fade=('#d8ff70', '#2a5020'))
    p.backdrop = (12, 28, 16)
    return p


# =====================================================================================================
def lanefir(t=0.0):
    """Lanefir Remnants: a weathered frog-lizard scavenger, a scaled crest swept into a horn, mustard scarf, teal coat.
    Moves: breath, the throat flutters, the heavy lid blinks, the scarf drape sways, the armband light pulses."""
    p = Portrait()
    skin = ramp('#6a9c3a', spread=0.75, hue_shift=0.06)
    scale = ramp('#8aa2aa', spread=0.72)
    belly = ramp('#cec48a', spread=0.55)
    scarf = ramp('#cc9a2a', spread=0.66, hue_shift=0.04)
    coat = ramp('#22668a', spread=0.78)
    steel = ramp('#a0a8b4', spread=0.6)
    body, head = breath(t), breath(t, -0.12)
    g = pulse(t, 0.0, 2)
    with p.at(0, body):
        # coat, high collar at the back
        p.fill(p.poly([(0, 66), (0, 50), (7, 44), (18, 42), (34, 50), (46, 47), (56, 49), (64, 56), (64, 66)]), coat, radius=9,
               texture=0.04, seed=61)
        p.fill(p.poly([(6, 47), (9, 33), (19, 36), (21, 47)]), coat, radius=3, bias=-0.05)
        p.stroke([(9, 34), (7, 46)], coat[4])
        for x, y in ((50, 57), (54, 62), (47, 54), (58, 59), (61, 63), (52, 52)):
            p.px(x, y, '#e8c860')
        p.stroke([(52, 52), (55, 56), (60, 56)], '#c8a040')
        p.stroke([(48, 58), (52, 61), (50, 65)], '#c8a040')
        # armband with its light
        p.fill(p.poly([(1, 52), (13, 48), (15, 55), (3, 59)]), steel, radius=2)
        p.stroke([(2, 53), (13, 49)], steel[4])
        p.stroke([(3, 58), (14, 54)], steel[0])
        p.glow(8, 54, '#fff4d0', mix('#ff7a10', '#ffc040', g), r=1, strength=0.55 + 0.45 * g)
    with p.at(0, head):
        # the horn: the crest sweeping back into a long curved spike
        hn = ((31, 13), (14, 14), (5, 3))
        p.fill(p.poly(taper(*hn, 8, 0.6)), scale, radius=2.5, texture=0.1, seed=60)
        p.stroke(bez((30, 10), (14, 11), (6, 3), 8)[:7], scale[4])
        for s in (0.3, 0.5, 0.7):
            x, y = bez(*hn, 20)[int(s * 20)]
            p.stroke([(x - 1, y - 1.5), (x + 1, y + 1.5)], scale[1])
    with p.at(0, body):
        # neck and pale ribbed throat, fluttering
        fl = 1 if pulse(t, 0.0, 4) > 0.6 else 0
        p.fill(p.poly([(22, 30), (40, 30), (46, 36), (44, 46), (24, 46)]), skin, radius=4, bias=-0.15)
        p.fill(p.poly([(33, 31), (48, 30), (50 + fl, 35), (47, 42), (35, 43)]), belly, radius=3, texture=0.04)
        for y in (35, 39):
            p.stroke([(37, y), (47 + (fl if y == 39 else 0), y - 1)], belly[1])
        # the scarf: wound high at the back, draped down the front
        sx = sway(t, 0.25, 1)
        sc = [(13, 49), (15, 40), (23, 35), (31, 39), (40, 42), (50, 40), (56, 44), (54, 52), (48 + sx, 60), (44 + sx, 66),
              (30 + sx, 66), (27, 58), (19, 55)]
        p.fill(p.poly(sc), scarf, radius=5, texture=0.05, seed=62)
        p.stroke([(16, 43), (24, 40), (33, 45), (43, 45), (53, 45)], scarf[1])
        p.stroke([(16, 41), (22, 37), (30, 41)], scarf[4])
        p.stroke([(22, 50), (30, 52), (38, 56), (44 + sx, 62)], scarf[1])
        p.stroke([(33, 48), (42, 51), (49, 50)], scarf[1])
        p.stroke([(30, 55), (34 + sx, 64)], scarf[4])
        p.stroke([(37, 58), (40 + sx, 65)], scarf[1])
    with p.at(0, head):
        # the head: a flat crown, a long rounded snout to the right
        hd = [(20, 25), (21, 16), (27, 11), (35, 9), (43, 10), (50, 13), (56, 17), (60, 22), (60, 26), (57, 29), (50, 31),
              (42, 33), (34, 34), (27, 33), (22, 30)]
        p.fill(p.poly(hd), skin, radius=7, texture=0.06, seed=64)
        # the pale lower jaw
        p.fill(p.poly([(37, 30), (46, 30), (55, 28), (58, 28), (52, 31), (44, 33), (36, 34)]), belly, radius=2, bias=-0.1,
               edge=False)
        # crest of spiky scales and the scaled cap
        for x, y in ((26, 11), (30, 9), (34, 8), (38, 8), (42, 9), (46, 11)):
            p.fill(p.poly([(x - 2, y + 2), (x - 3, y - 2), (x + 2, y + 1)]), scale, radius=1.5)
        cap = [(21, 17), (26, 12), (34, 10), (42, 11), (49, 14), (46, 15), (43, 14), (40, 16), (36, 15), (33, 17), (29, 16),
               (26, 19), (24, 18), (22, 22)]
        p.fill(p.poly(cap), scale, radius=3, texture=0.2, seed=65, edge=False)
        for x, y in ((25, 15), (28, 13), (32, 12), (36, 12), (40, 13), (31, 14), (35, 14), (44, 13), (23, 18)):
            p.px(x, y, scale[4])
            p.px(x + 1, y + 1, scale[0])
        for x, y in ((38, 17), (30, 18), (45, 16), (27, 21), (34, 18), (48, 16)):
            p.px(x, y, scale[1])
        # mottled hide and old scars
        for x, y in ((49, 18), (52, 21), (47, 25), (26, 25), (30, 28), (24, 21), (41, 25), (35, 23), (54, 24)):
            p.pixels([(x, y), (x + 1, y)], skin[1])
        p.stroke([(29, 21), (32, 26)], skin[0])
        # the wide lipless mouth, a knowing curl at the back
        p.stroke([(59, 26), (55, 28), (47, 29), (40, 29), (37, 27)], '#22301a')
        p.px(36, 27, skin[1])
        p.pixels([(57, 20), (58, 20)], '#22301a')
        p.px(57, 19, skin[4])
        # the heavy-lidded golden eye under a scaled brow
        p.fill(p.ellipse(44.5, 18.5, 4, 3.2), skin, radius=2, bias=0.2)
        p.fill(p.ellipse(44.5, 19, 2.8, 2), skin, flat=0, edge=False)
        if blink(t, 0.55, 2):
            p.stroke([(42, 19), (47, 19)], skin[1])
        else:
            p.pixels([(42, 19), (43, 19), (44, 19), (45, 19), (46, 19), (43, 20), (44, 20), (45, 20)], '#e8b828')
            p.pixels([(44, 19), (44, 20)], '#1c1408')
            p.px(45, 19, '#fff0a0')
            p.stroke([(41, 18), (47, 18)], skin[0])
            p.stroke([(42, 17), (46, 17)], skin[2])
        p.stroke([(40, 16), (44, 15), (48, 16)], skin[4])
        p.stroke([(41, 15), (45, 14)], scale[3])
    p.rim('#ffb43a', side=1, amount=0.5)
    p.rim('#3ae0ff', side=-1, amount=0.25)
    p.outline('#0a0c0a')
    p.backdrop = (34, 14, 40)
    return p


# =====================================================================================================
def lizho(t=0.0):
    """Li-Zho Dynasty: a horned dragon lord in a gold-trimmed robe, crimson mane, long golden barbels.
    Moves: breath, the mane and barbels sway, smoke curls from the nostril, the eye glints and blinks."""
    p = Portrait()
    scale = ramp('#566c36', spread=0.75, hue_shift=0.05)
    horn = ramp('#e4d8b4', spread=0.6)
    mane = ramp('#d82a3c', spread=0.65)
    throat = ramp('#dcd0ac', spread=0.55)
    robe = ramp('#34182c', spread=0.8)
    gold = ramp('#d8a82e', spread=0.65)
    jade = ramp('#22e0a0', spread=0.4)
    body, head = breath(t), breath(t, -0.12)
    g = pulse(t, 0.0, 2)
    with p.at(0, head):
        # horns sweeping back, ringed
        for a, b, c, w in (((27, 12), (16, 6), (5, -1), 7), ((32, 10), (26, 2), (21, -4), 5)):
            p.fill(p.poly(taper(a, b, c, w, 0.7)), horn, radius=2.5)
            for s in (0.25, 0.45, 0.65):
                (x, y) = bez(a, b, c, 20)[int(s * 20)]
                p.stroke([(x - 1, y - 1.5), (x + 1, y + 1.5)], horn[1])
        # the crimson mane down the back of the head and neck
        p.fill(p.poly([(24, 14), (12, 18), (7, 30), (10, 44), (16, 52), (27, 46), (24, 30)]), mane, radius=3, bias=-0.25)
        for i, (a, b, c, w) in enumerate((((23, 15), (13, 13), (5, 18), 4.5), ((22, 20), (12, 21), (4, 28), 5),
                                          ((22, 26), (12, 30), (5, 38), 5), ((23, 32), (14, 38), (8, 47), 5),
                                          ((25, 38), (18, 45), (13, 54), 4.5))):
            dx = sway(t, 0.1 * i, 1)
            p.fill(p.poly(taper(a, b, (c[0] + dx, c[1]), w, 0.8)), mane, radius=2, texture=0.06, seed=70 + i)
            p.stroke(bez(a, b, (c[0] + dx, c[1]), 8)[1:6], mane[4] if i % 2 == 0 else mane[3])
    with p.at(0, body):
        # robe with a high collar and gold pauldrons
        p.fill(p.poly([(0, 66), (0, 50), (10, 45), (22, 45), (33, 55), (44, 47), (54, 45), (64, 50), (64, 66)]), robe, radius=9,
               texture=0.04, seed=71)
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            p.fill(p.poly(f([(0, 56), (0, 44), (3, 35), (9, 40), (18, 42), (22, 48), (12, 53)])), gold, radius=3,
                   bias=0.0 if s == 1 else -0.15)
            p.stroke(f([(3, 37), (4, 45), (10, 49), (19, 47)]), gold[1] if s == 1 else gold[0])
            p.stroke(f([(4, 36), (8, 41), (17, 43)]), gold[4] if s == 1 else gold[3])
        p.stroke([(22, 46), (33, 56), (44, 48)], gold[3], width=2)
        for x in (12, 52):
            p.stroke([(x - 5, 61), (x, 57), (x + 5, 61)], gold[1])
            p.px(x, 59, gold[3])
        p.fill(p.ellipse(33, 60, 4, 4), gold, radius=2)
        p.fill(p.ellipse(33, 60, 2.2, 2.2), jade, radius=1.5, bias=0.1 + 0.2 * g)
        p.px(32, 59, '#e0fff0')
        # the long neck, pale throat plates
        p.fill(p.poly([(23, 28), (38, 31), (42, 48), (34, 55), (25, 48)]), scale, radius=4, bias=-0.1, texture=0.06, seed=74)
        p.fill(p.poly([(32, 32), (41, 32), (41, 48), (35, 53), (31, 46)]), throat, radius=3)
        for y in range(35, 52, 3):
            p.stroke([(32, y + 1), (36, y - 1), (41, y)], throat[1])
            p.stroke([(33, y + 2), (36, y)], throat[4])
    with p.at(0, head):
        # cheek frill behind the jaw
        p.fill(p.poly([(32, 25), (16, 26), (26, 30), (15, 35), (27, 34), (20, 42), (31, 35), (34, 31)]), mane, radius=2,
               bias=0.1)
        p.stroke([(30, 27), (19, 27)], mane[4])
        p.stroke([(28, 32), (18, 35)], mane[3])
        # upper head and long snout
        up = [(21, 25), (22, 15), (30, 9), (38, 10), (43, 13), (46, 16), (54, 18), (60, 21), (62, 24), (61, 26), (52, 27),
              (44, 28), (38, 29), (30, 30), (23, 29)]
        p.fill(p.poly(up), scale, radius=6, texture=0.05, seed=72)
        # lower jaw, the mouth a little open
        p.fill(p.poly([(30, 30), (38, 29), (46, 29), (56, 28), (59, 29), (56, 32), (46, 34), (38, 36), (31, 35)]), scale,
               radius=3, bias=-0.12, texture=0.06, seed=73)
        p.stroke([(38, 29), (46, 29), (57, 28)], '#1c0a10')
        for x in (47, 52, 56):
            p.px(x, 29, '#f4f0dc')
        p.px(50, 28, '#f4f0dc')
        p.stroke([(40, 33), (48, 32), (54, 31)], scale[1])
        # brow ridge with spikes, nose ridge, scale glints
        for x, y in ((33, 10), (37, 10), (41, 12)):
            p.fill(p.poly([(x - 2, y + 2), (x - 3, y - 3), (x + 2, y + 1)]), scale, radius=1, bias=0.2)
        p.stroke([(34, 15), (40, 14), (45, 16)], scale[4])
        p.stroke([(47, 17), (54, 19), (59, 21)], scale[4])
        p.stroke([(46, 18), (52, 20)], scale[3])
        p.stroke([(36, 21), (40, 22), (44, 21)], scale[1])
        p.stroke([(48, 23), (56, 24)], scale[1])
        for x, y in ((26, 16), (29, 13), (33, 13), (24, 20), (28, 18), (31, 16)):
            p.px(x, y, scale[4])
            p.px(x + 1, y + 1, scale[1])
        for x, y in ((35, 22), (32, 25), (28, 22), (50, 21), (54, 22)):
            p.px(x, y, gold[2])
        # nostril
        p.pixels([(59, 22), (60, 22)], '#1c0a10')
        # cheek scales
        for x, y in ((27, 26), (31, 27), (25, 23), (29, 22)):
            p.stroke([(x - 1, y), (x, y + 1), (x + 2, y + 1)], scale[1])
            p.px(x, y, scale[3])
        # the eye: amber, slitted, under a heavy brow
        p.stroke([(36, 16), (40, 15), (44, 16)], scale[0])
        p.fill(p.ellipse(41, 18.5, 3.4, 2.1), scale, flat=0, edge=False)
        if blink(t, 0.7, 2):
            p.stroke([(38, 19), (44, 18)], scale[1])
        else:
            p.pixels([(39, 18), (40, 18), (41, 18), (42, 18), (43, 18), (39, 19), (40, 19), (41, 19), (42, 19)], '#ff8a1a')
            p.pixels([(41, 18), (41, 19)], '#280808')
            p.px(42, 18, mix('#ffd060', '#ffffff', g))
            p.px(38, 19, '#c04010')
            p.glow(42, 18, mix('#ffd060', '#ffffff', g), '#ff9a2a', r=0, strength=0.25 + 0.3 * g)
        # barbels from the lip, swaying
        for (a, b, c), ph in ((((59, 27), (64, 34), (61, 44)), 0.0), (((52, 31), (55, 39), (51, 47)), 0.3)):
            dx = sway(t, ph, 1)
            p.stroke(bez(a, b, (c[0] + dx, c[1]), 8), gold[2])
            p.px(c[0] + dx, c[1], gold[4])
    p.rim('#30e0c0', side=1, amount=0.45)
    p.rim('#ff5a3a', side=-1, amount=0.3)
    p.outline('#0c0608')
    # smoke from the nostril
    for ph in (0.0, 0.5):
        u = (t * 2 + ph) % 1.0
        y = int(round(20 - u * 14 + breath(t, -0.12)))
        x = 60 + int(round(u * 2)) + sway(t, ph, 1, 2)
        if u < 0.8:
            p.px(x, y, mix('#d0c8d0', '#30404a', u))
    p.backdrop = (6, 30, 32)
    return p


# =====================================================================================================
def ltokk(t=0.0):
    """L'tokk Khrask: an ancient faceted stone sage, hunched, a glowing face of cracks and a beard of crystal shards,
    moss, flowers and a sapling growing on it. Moves: very slow breath, the leaves sway, the cracks glow and fade,
    pollen drifts."""
    p = Portrait()
    stone = ramp('#587680', spread=0.92)
    deep = ramp('#26363e', spread=0.6)
    crys = ramp('#36c8b8', spread=0.6)
    moss = ramp('#5aa22c', spread=0.65, hue_shift=0.05)
    leaf = ramp('#8ad444', spread=0.5)
    glow = ramp('#4affb4', spread=0.4)
    body, head = breath(t), breath(t, -0.15)
    g = pulse(t, 0.0, 1)
    gc = mix(glow[1], glow[3], g)

    def facets(fs, ramp_, lines=True):
        for pts, k in fs:
            p.fill(p.poly(pts), ramp_, flat=k, edge=False)
        if lines:
            for pts, k in fs:
                p.stroke(list(pts) + [pts[0]], ramp_[max(0, k - 2)] if k > 1 else ramp_[0])

    def flower(x, y, c):
        p.pixels([(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)], c)
        p.px(x, y, '#ffd040' if c != '#ffe060' else '#ff8a40')

    with p.at(0, body):
        # massive hunched shoulders
        facets([([(0, 66), (0, 44), (6, 37), (14, 38), (18, 46), (10, 56), (4, 66)], 3),
                ([(14, 38), (24, 42), (28, 50), (18, 46)], 4), ([(18, 46), (28, 50), (30, 58), (10, 56)], 2),
                ([(4, 66), (10, 56), (30, 58), (32, 66)], 1), ([(28, 50), (40, 46), (46, 56), (30, 58)], 2),
                ([(40, 46), (50, 38), (58, 40), (56, 50), (46, 56)], 1), ([(58, 40), (64, 46), (64, 66), (56, 50)], 0),
                ([(46, 56), (56, 50), (64, 66), (32, 66), (30, 58)], 0)], stone)
        for seg in ([(16, 48), (20, 53), (18, 58)], [(47, 50), (50, 55)], [(56, 53), (59, 58)]):
            p.stroke(seg, gc)
        # moss and ferns over the left shoulder, a vine hanging
        p.fill(p.ellipse(9, 40, 9, 4) | p.ellipse(18, 41, 6, 3) | p.ellipse(4, 45, 4, 4), moss, radius=3, texture=0.3, seed=81)
        p.stroke([(14, 43), (15, 48), (14, 52)], moss[2])
        p.px(15, 50, leaf[3])
        for i, (a, b, c) in enumerate((((5, 38), (1, 31), (3, 25)), ((11, 37), (10, 31), (14, 27)), ((18, 39), (20, 34), (19, 30)))):
            dx = sway(t, 0.2 + 0.25 * i, 1)
            pts = bez(a, b, (c[0] + dx, c[1]), 6)
            p.stroke(pts, leaf[1])
            for (x, y) in pts[1:-1]:
                p.px(int(round(x)) - 1, int(round(y)), leaf[3])
                p.px(int(round(x)) + 1, int(round(y)) + 1, leaf[2])
            p.px(int(round(pts[-1][0])), int(round(pts[-1][1])), leaf[4])
        p.fill(p.ellipse(53, 40, 5, 2.5), moss, radius=2, texture=0.3, seed=82, bias=-0.1)
        for x, y, c in ((8, 38, '#ff7aa8'), (16, 39, '#ffe060'), (3, 43, '#f4f0ff'), (52, 39, '#ff7aa8'), (12, 42, '#f4f0ff')):
            flower(x, y, c)
    with p.at(0, head):
        # the sapling growing from the crown (behind the moss)
        lx = sway(t, 0.0, 1)
        p.stroke([(32, 6), (31, 2), (32 + lx, -1)], '#6a5a30')
        for (x, y), s in (((30 + lx, 1), -1), ((33 + lx, 0), 1), ((30, 4), -1)):
            p.fill(p.poly([(x, y), (x + 3 * s, y - 2), (x + 4 * s, y), (x + 1 * s, y + 1)]), leaf, radius=1, edge=False)
        # the faceted head, bowed forward
        facets([([(20, 14), (28, 6), (32, 13), (24, 20)], 4), ([(28, 6), (38, 6), (40, 12), (32, 13)], 3),
                ([(38, 6), (46, 11), (48, 17), (40, 12)], 2), ([(18, 26), (20, 14), (24, 20), (28, 28), (21, 36)], 3),
                ([(21, 36), (28, 28), (30, 38), (28, 42)], 2), ([(28, 42), (30, 38), (36, 44)], 1)], stone)
        # the craggy face: forehead, sockets under a heavy brow, a wedge of a nose, a long crack of a mouth
        facets([([(24, 20), (32, 13), (40, 12), (48, 17), (51, 21), (29, 21)], 4),
                ([(29, 21), (51, 21), (50, 24), (46, 26), (40, 26), (31, 25)], 2),
                ([(30, 21), (39, 21), (39, 25), (32, 25)], 0), ([(43, 21), (50, 21), (49, 24), (44, 25)], 0),
                ([(28, 22), (31, 25), (40, 26), (40, 34), (34, 40), (29, 36), (28, 28)], 3),
                ([(40, 26), (46, 26), (52, 31), (46, 32), (41, 31)], 4),
                ([(41, 31), (46, 32), (52, 31), (48, 34), (42, 34)], 1),
                ([(46, 26), (50, 24), (51, 29)], 1),
                ([(40, 34), (48, 34), (49, 37), (44, 42), (38, 39)], 1),
                ([(34, 40), (38, 39), (44, 42), (38, 45)], 2)], stone)
        p.stroke([(25, 20), (32, 14), (40, 13), (47, 17)], stone[4])
        p.stroke([(36, 37), (40, 36), (45, 38)], deep[0])
        # glowing cracks running from the eyes
        for seg in ([(35, 26), (34, 30), (36, 34)], [(33, 20), (35, 16), (34, 13)], [(45, 20), (43, 15)],
                    [(29, 30), (31, 33)], [(46, 34), (47, 37)]):
            p.stroke(seg, gc)
        # deep-set eyes, old and patient
        for cx, w in ((36, 2), (46, 1)):
            p.pixels([(cx - w + i, 23) for i in range(2 * w)], mix(glow[2], '#ffffff', g))
            p.glow(cx, 23, '#ffffff', glow[3], r=1, strength=0.35 + 0.45 * g)
        # a beard of crystal shards
        sh = [([(32, 41), (37, 44), (34, 52), (32, 48)], 3), ([(37, 44), (43, 42), (41, 54), (38, 51)], 2),
              ([(43, 42), (46, 39), (47, 47), (44, 50)], 1)]
        facets(sh, crys)
        for x, y in ((34, 51), (41, 53), (46, 47)):
            p.px(x, y, mix(glow[2], '#ffffff', g))
        # moss cap with flowers, dripping over the edge
        p.fill(p.ellipse(31, 7, 8, 3) | p.ellipse(39, 7, 5, 2.5) | p.ellipse(23, 11, 4, 3) | p.ellipse(20, 16, 2, 3), moss,
               radius=2.5, texture=0.3, seed=83)
        p.stroke([(27, 9), (26, 13)], moss[2])
        p.stroke([(42, 8), (43, 11)], moss[1])
        for x, y, c in ((26, 6, '#ff7aa8'), (36, 5, '#ffe060'), (41, 7, '#f4f0ff'), (21, 11, '#ffe060')):
            flower(x, y, c)
    p.rim('#4affb4', side=1, amount=0.45)
    p.rim('#ffb070', side=-1, amount=0.22)
    p.outline('#060a0c')
    drift(p, t, ((4, 0.0), (12, 0.5), (22, 0.3), (52, 0.25), (59, 0.75), (46, 0.6)), rise=30, y0=50,
          fade=('#fff0a0', '#406040'), speed=1)
    p.backdrop = (16, 12, 36)
    return p


# =====================================================================================================
def mirveda(t=0.0):
    """Mirveda Protectorate: a stern guardian in a bone-white helm under a fan of rust-red plume, a T-visor of light,
    a shield crest on the brow. Moves: breath, the plume ripples, a light sweeps along the visor and pulses."""
    p = Portrait()
    bone = ramp('#e8dcc0', spread=0.8, hue_shift=0.07)
    rust = ramp('#b8402a', spread=0.68)
    dark = ramp('#241c2c', spread=0.7)
    blue = ramp('#5ab4ff', spread=0.4)
    pink = ramp('#ff6ab4', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    g = pulse(t, 0.0, 2)

    def star(x, y, k):
        p.pixels([(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)], mix(pink[2], pink[4], k))
        p.pixels([(x - 2, y), (x + 2, y), (x, y - 2), (x, y + 2)], mix(blue[1], blue[3], k))
        p.px(x, y, '#ffffff')

    with p.at(0, head):
        # the transverse plume: a fan of rust-red bristles, its edge rippling
        cx, cy = 32 + p.dx, 18 + p.dy
        X = p.xx + 0.5 - cx
        Y = (p.yy + 0.5 - cy) * 1.4
        r = np.hypot(X, Y)
        th = np.arctan2(Y, X)
        rout = 26 + 1.3 * np.sin(5 * th + 2 * math.pi * t)
        fan = (r < rout) & (r > 5) & (th < -0.12) & (th > -math.pi + 0.12)
        p.fill(fan, rust, radius=4, texture=0.12, seed=90)
        for i in range(17):
            a = -math.pi + 0.2 + i * (math.pi - 0.4) / 16
            r1 = 24 + 1.3 * math.sin(5 * a + 2 * math.pi * t)
            pts = [(32 + math.cos(a) * 9, 18 + math.sin(a) * 9 / 1.4), (32 + math.cos(a) * r1, 18 + math.sin(a) * r1 / 1.4)]
            p.stroke(pts, rust[1] if i % 2 else rust[4] if i < 8 else rust[3])
    with p.at(0, body):
        # a rust mantle, bone pauldrons in lames
        p.fill(p.poly([(16, 66), (18, 46), (32, 50), (46, 46), (48, 66)]), rust, radius=5, texture=0.05)
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            b = 0.0 if s == 1 else -0.18
            for pts in ([(0, 66), (0, 55), (10, 51), (23, 53), (25, 66)], [(0, 57), (0, 48), (8, 44), (21, 45), (26, 50), (23, 55),
                                                                          (10, 53)],
                        [(0, 49), (4, 41), (14, 37), (24, 40), (27, 46), (20, 47), (8, 46)]):
                p.fill(p.poly(f(pts)), bone, radius=3.5, bias=b, texture=0.02, seed=91)
            p.stroke(f([(1, 48), (5, 41), (14, 38), (24, 41)]), rust[2], width=2)
            for x, y in ((8, 44), (14, 42), (20, 43)):
                p.px(*(MP([(x, y)])[0] if s == -1 else (x, y)), bone[0])
        # the gorget and the shield emblem
        p.fill(p.poly([(23, 44), (41, 44), (40, 52), (24, 52)]), bone, radius=3, bias=-0.1)
        p.stroke([(24, 48), (40, 48)], bone[1])
        p.fill(p.poly([(26, 51), (38, 51), (38, 57), (32, 64), (26, 57)]), rust, radius=3)
        p.stroke([(26, 51), (38, 51), (38, 57), (32, 63), (26, 57), (26, 51)], bone[3])
        star(32, 56, g)
    with p.at(0, head):
        # the great helm, a keel down its crown
        helm = [(20, 16), (24, 10), (32, 7), (40, 10), (44, 16), (46, 26), (45, 36), (41, 44), (32, 48), (23, 44), (19, 36),
                (18, 26)]
        p.fill(p.poly(helm), bone, radius=8, bias=0.02, texture=0.02, seed=92)
        p.stroke([(32, 8), (32, 18)], bone[4])
        p.stroke([(33, 9), (33, 18)], bone[1])
        # cheek guards, engraved and riveted
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            p.fill(p.poly(f([(19, 28), (29, 30), (30, 44), (24, 43), (20, 37)])), bone, radius=3,
                   bias=0.05 if s == 1 else -0.25)
            p.stroke(f([(21, 32), (27, 33), (28, 41)]), bone[1] if s == 1 else bone[0])
            for x, y in ((22, 35), (25, 40)):
                p.px(*(MP([(x, y)])[0] if s == -1 else (x, y)), bone[0])
        # the stern brow, a V over the visor
        p.fill(p.poly([(18, 18), (32, 22), (46, 18), (46, 22), (32, 26), (18, 22)]), bone, radius=2, bias=0.2)
        # the T-visor and its light
        vis = [(20, 23), (31, 26), (33, 26), (44, 23), (44, 25), (34, 28), (34, 40), (30, 40), (30, 28), (20, 25)]
        p.fill(p.poly(vis), dark, flat=0, edge=False)
        vl, vh = mix(blue[1], blue[2], g), mix(blue[3], '#e8f6ff', g)

        def bar_y(x):
            return 24 + int(round(3 * (1 - min(1.0, abs(x - 32) / 11))))

        for x in range(21, 44):
            p.px(x, bar_y(x), vl)
            p.px(x, bar_y(x) - 1, mix(dark[1], blue[0], 0.6 + 0.4 * g))
        p.stroke([(31, 28), (31, 38)], vl)
        p.stroke([(32, 28), (32, 38)], mix(vl, vh, 0.5))
        for ex in (26, 38):
            p.px(ex, bar_y(ex), mix(blue[3], '#ffffff', g))
        u = 0.5 - 0.5 * math.cos(2 * math.pi * t)
        sx = 22 + int(round(u * 20))
        p.pixels([(sx - 1, bar_y(sx - 1)), (sx, bar_y(sx)), (sx + 1, bar_y(sx + 1))], vh)
        p.glow(sx, bar_y(sx), '#ffffff', '#a8e0ff', r=1, strength=0.55)
        # breathing holes
        for y in (42, 44):
            p.pixels([(28, y), (36, y)], dark[0])
        # the shield crest on the brow
        p.fill(p.poly([(28, 10), (36, 10), (36, 15), (32, 20), (28, 15)]), rust, radius=2)
        p.stroke([(28, 10), (36, 10)], bone[4])
        p.glow(32, 14, '#ffffff', mix(pink[2], pink[4], g), r=0, strength=0.6 + 0.3 * g)
    p.rim('#5ab4ff', side=1, amount=0.45)
    p.rim('#ff6ab4', side=-1, amount=0.28)
    p.outline('#0e0a12')
    p.backdrop = (18, 14, 42)
    return p


# =====================================================================================================
_NZ = {}


def _noise(seed, res, size=96):
    """smooth tileable value noise 0..1 (size x size), cached"""
    key = (seed, res, size)
    if key not in _NZ:
        rng = np.random.default_rng(seed)
        z = ndimage.zoom(rng.random((res, res)), size / res, order=3, mode='grid-wrap')
        _NZ[key] = (z - z.min()) / (z.max() - z.min())
    return _NZ[key]


def mortheus(t=0.0):
    """Glimmer of Mortheus: a face of nebula gas and starlight with two star-eyes, wrapped in a two-armed spiral of
    billowing cloud. Moves: the cloud flows slowly along the spiral (one lump-spacing per loop, so it is seamless),
    the face fades in and out of the gas, stars twinkle."""
    p = Portrait()
    vio = ramp('#5a3cbc', spread=0.75)
    gold = ramp('#e4a04a', spread=0.62, hue_shift=0.06)
    mag = ramp('#c44aa8', spread=0.7)
    blue = ramp('#2c58c0', spread=0.75)
    teal = ramp('#2cb4aa', spread=0.55)
    f = pulse(t, 0.0, 1)  # face presence 0..1
    cx, cy = 32.0, 29.0

    def lumps(a0, a1, r0, r1, n, size, ph=0.0, sq=0.92, smin=0.0, smax=1.0, wob=0.0):
        m = np.zeros((64, 64), bool)
        for i in range(n):
            s = ((i + t + ph) % n) / n
            if not (smin <= s <= smax):
                continue
            a = math.radians(a0 + (a1 - a0) * s)
            R = r0 + (r1 - r0) * s
            x, y = cx + R * math.cos(a), cy + R * math.sin(a) * sq
            k = math.sin(math.pi * s) ** 0.5
            rr = k * size * (1 + wob * math.sin(2 * math.pi * 3 * s + ph * 9))
            if rr < 1.0:
                continue
            m |= p.ellipse(x, y, rr, rr * 0.9)
        return m

    # the cloud bank below, drifting
    base = lumps(160, 20, 34, 34, 9, 10, sq=0.95) | p.ellipse(32, 68, 34, 12) | lumps(170, 10, 40, 40, 14, 4, ph=0.4,
                                                                                     sq=0.9, wob=0.4)
    p.fill(base, blue, radius=6, bias=-0.1, texture=0.06, seed=9)
    p.fill(lumps(150, 30, 26, 26, 8, 7.5, ph=0.5), vio, radius=4, texture=0.06, seed=30)
    # two spiral arms wrapping the head (behind it)
    armA = dict(a0=-170, a1=50, r0=13, r1=31, n=16, size=7, wob=0.3)
    armB = dict(a0=10, a1=230, r0=13, r1=31, n=16, size=7, wob=0.3)
    puffA = lumps(-175, 45, 18, 37, 22, 3.6, ph=0.37, wob=0.45)
    puffB = lumps(5, 225, 18, 37, 22, 3.6, ph=0.61, wob=0.45)
    p.fill(lumps(**armA) | puffA, gold, radius=4, texture=0.1, seed=50)
    p.fill(lumps(**armB) | puffB, mag, radius=4, bias=-0.05, texture=0.1, seed=70)
    # dark dust lanes along the arms
    for (a0, a1), col in (((-150, 20), gold[1]), ((30, 200), mag[0])):
        pts = []
        for i in range(16):
            s = i / 15
            aa = math.radians(a0 + (a1 - a0) * s + 12 * math.sin(2 * math.pi * (t + s)))
            R = 15 + 15 * s
            pts.append((cx + R * math.cos(aa), cy + R * math.sin(aa) * 0.92))
        p.stroke(pts[3:12], col)
    # the face of gas: violet cloud when faint, a teal glow when present
    fr = [mix(v, c, f) for v, c in zip(vio, teal)]
    hd = (p.ellipse(32, 28, 12.5, 14.5) | p.ellipse(32, 39, 9, 6.5)) & ~p.poly([(19, 34), (27, 46), (19, 46)]) \
        & ~p.poly([(45, 34), (37, 46), (45, 46)])
    under, under_a = p.rgb.copy(), p.a.copy()
    p.fill(hd, fr, radius=7, texture=0.14, seed=91, edge=False)
    # the face's edge dissolves into the gas (dithered), it has no hard contour
    ring = hd & ~ndimage.binary_erosion(hd, iterations=2)
    chk = ((p.xx + p.yy) % 2) == 0
    back = ring & chk & under_a
    p.rgb[back] = under[back]
    gone = ring & chk & ~under_a
    p.a[gone] = False
    for x, y in ((28, 18), (37, 17), (24, 33), (40, 36), (30, 44), (35, 20)):
        p.px(x, y, mix(fr[3], '#ffffff', 0.5 + 0.5 * f))
    # its features, shading in and out
    if f > 0.15:
        for ex in (26, 38):
            p.fill(p.ellipse(ex + 0.5, 27.5, 4.2, 2.6), fr, flat=1 if f < 0.5 else 0, edge=False)
        p.stroke([(21, 23), (25, 22), (30, 24)], mix(fr[3], fr[4], f))
        p.stroke([(34, 24), (39, 22), (43, 23)], mix(fr[2], fr[3], f))
        p.stroke([(32, 26), (32, 33)], mix(fr[2], fr[4], f))
        p.stroke([(33, 27), (33, 33)], mix(fr[2], fr[1], f))
        p.pixels([(30, 34), (34, 34)], mix(fr[3], fr[1], f))
        p.stroke([(28, 39), (32, 40), (36, 39)], mix(fr[2], fr[0], f))
        p.stroke([(29, 41), (35, 41)], mix(fr[2], fr[3], f))
        p.stroke([(23, 31), (26, 35)], mix(fr[3], fr[4], f))
        p.stroke([(41, 31), (38, 35)], mix(fr[2], fr[1], f))
    # the arms' inner ends curl in front of the face's edge
    p.fill(lumps(smax=0.22, **armA), gold, radius=3, texture=0.07, seed=51)
    p.fill(lumps(smax=0.22, **armB), mag, radius=3, bias=-0.05, texture=0.07, seed=71)
    p.rim('#5af0ff', side=1, amount=0.35)
    p.rim('#ff6ad8', side=-1, amount=0.3)
    p.outline('#07061c')
    # a dithered haze of gas around the whole cloud
    dist, (iy, ix) = ndimage.distance_transform_edt(~p.a, return_indices=True)
    inner = ndimage.binary_erosion(p.a, iterations=2)
    d2, (jy, jx) = ndimage.distance_transform_edt(~inner, return_indices=True)
    halo = (dist > 0) & (dist <= 2.5) & (((p.xx + p.yy) % 2) == 0)
    hc = p.rgb[jy, jx].astype(float) * 0.45 + np.array([6, 6, 20], float) * 0.55
    p.rgb[halo] = hc[halo].astype(np.uint8)
    p.a |= halo
    # star-eyes
    for ex in (26, 38):
        p.glow(ex, 27, '#ffffff', mix('#7ab0ff', '#d8f8ff', f), r=1, strength=0.35 + 0.65 * f)
        if f > 0.45:
            p.pixels([(ex - 2, 27), (ex + 2, 27), (ex, 25), (ex, 29)], '#e8f4ff')
        if f > 0.8:
            p.pixels([(ex - 3, 27), (ex + 3, 27)], '#9ab8ff')
    # twinkling stars, in front and behind
    for i, (x, y) in enumerate(((4, 5), (58, 4), (61, 22), (2, 24), (57, 58), (6, 56), (20, 12), (44, 46), (13, 31),
                                (50, 20), (36, 4), (12, 47), (52, 36), (27, 8))):
        kk = pulse(t, i * 0.37, 2 if i % 2 else 1)
        if kk > 0.25:
            p.px(x, y, mix('#5060a0', '#ffffff', kk))
        if kk > 0.85:
            p.pixels([(x - 1, y), (x + 1, y)], '#8090d0')
    p.backdrop = (6, 6, 20)
    return p


# =====================================================================================================
def myko(t=0.0):
    """Myko-Mentori: a fungal seer under a great white cap pitted with dark glossy spots, a long ribbed face in
    its shadow, a blue tattered cloak. Moves: the cap tilts and sways, the gills glow and fade, spores drift down
    and away, breath."""
    p = Portrait()
    cap = ramp('#e2dccc', spread=0.66, hue_shift=0.06)
    gill = ramp('#7a3c74', spread=0.6)
    face = ramp('#b4b0a6', spread=0.66)
    spot = ramp('#283044', spread=0.65)
    cloak = ramp('#2a6aa2', spread=0.78)
    root = ramp('#8a8070', spread=0.6)
    tag = ramp('#a0acb8', spread=0.55)
    body, head = breath(t), breath(t, -0.12)
    g = pulse(t, 0.0, 2)
    tilt = wave(t, 0.1, 1) * 1.3
    with p.at(0, body):
        # cloak with a high collar, metal tags
        p.fill(p.poly([(0, 66), (0, 46), (7, 39), (18, 36), (25, 42), (32, 46), (39, 42), (46, 36), (57, 39), (64, 46),
                       (64, 66)]), cloak, radius=8, texture=0.05, seed=111)
        # the fibrous root-body at the chest
        p.fill(p.poly([(23, 42), (41, 42), (39, 66), (25, 66)]), root, radius=4, texture=0.12, seed=112)
        for x in (26, 29, 32, 35, 38):
            p.stroke([(x, 45), (x + (1 if x < 32 else -1), 52), (x, 59), (x + 1, 66)], root[0])
            p.stroke([(x + 1, 46), (x + 1, 52)], root[3])
        # cloak edges over the chest, tattered
        for s in (1, -1):
            f = (lambda q: q) if s == 1 else M
            p.fill(p.poly(f([(14, 38), (24, 41), (27, 52), (25, 58), (23, 54), (21, 62), (16, 66), (8, 66)])), cloak, radius=4,
                   bias=0.05 if s == 1 else -0.15, texture=0.05, seed=113)
            p.stroke(f([(16, 41), (23, 44), (25, 52)]), cloak[4] if s == 1 else cloak[3])
        for x, y in ((8, 48), (14, 54), (50, 47), (56, 54)):
            p.fill(p.poly([(x - 2, y - 2), (x + 2, y - 2), (x + 2, y + 2), (x - 2, y + 2)]), tag, radius=1)
            p.px(x, y, tag[0])
            p.px(x - 1, y - 1, tag[4])
        p.stroke([(3, 60), (8, 57), (12, 61)], cloak[1])
        p.stroke([(61, 60), (56, 57), (52, 61)], cloak[0])
    with p.at(0, head):
        # the long ribbed face in the cap's shadow
        fc = [(23, 26), (41, 26), (42, 33), (39, 42), (35, 50), (29, 50), (25, 42), (22, 33)]
        p.fill(p.poly(fc), face, radius=6, bias=0.1, texture=0.05, seed=114)
        for x in (28, 30, 32, 34, 36):
            y1 = 48 if 29 < x < 35 else 45
            p.stroke([(x, 39), (x, y1)], face[1])
            p.stroke([(x + 1, 39), (x + 1, y1 - 1)], face[3])
        p.stroke([(25, 36), (31, 37)], face[0])
        p.stroke([(39, 36), (33, 37)], face[0])
        p.stroke([(25, 35), (30, 36)], face[4])
        p.pixels([(32, 33), (32, 35)], face[0])
        p.stroke([(24, 27), (40, 27)], face[0])
        p.stroke([(25, 28), (39, 28)], face[1])
        # deep sockets with green eyes
        for ex in (28, 36):
            p.fill(p.ellipse(ex + 0.5, 31.5, 3.2, 2.5), spot, flat=0, edge=False)
            p.glow(ex, 31, '#f0ffe0', mix('#3adf5a', '#a8ff7a', g), r=0, strength=0.6 + 0.4 * g)
        # the gills under the cap, glowing
        gl = [(5, 23), (20, 25), (32, 26), (44, 25), (59, 23), (52, 28), (40, 30), (32, 30), (24, 30), (12, 28)]
        p.fill(p.poly(gl), gill, radius=2, bias=-0.15)
        gc = mix('#8a2a8a', '#ff7ae0', g)
        for x in range(8, 58, 2):
            p.stroke([(x, 25 if 16 < x < 48 else 24), (32 + (x - 32) * 0.72, 29)], gc if x % 4 == 0 else gill[1])
        # the great cap, tilting a little
        cp = [(0, 23 + tilt), (4, 15 + tilt), (14, 7 + tilt * 0.6), (26, 3 + tilt * 0.2), (32, 2), (38, 3 - tilt * 0.2),
              (50, 7 - tilt * 0.6), (60, 15 - tilt), (64, 23 - tilt), (57, 26 - tilt), (44, 27), (32, 28), (20, 27), (7, 26 + tilt)]
        p.fill(p.poly(cp), cap, radius=14, texture=0.0, seed=115, levels=(0.2, 0.42, 0.62, 0.84))
        p.stroke([(5, 15 + int(round(tilt))), (14, 8), (26, 4)], cap[4])
        p.stroke([(8, 25), (20, 26), (32, 27), (44, 26), (56, 25)], cap[1])
        # pitted spots, glossy; a few glow with omen-light
        for i, (x, y, r) in enumerate(((12, 16, 3.2), (22, 9, 2.6), (32, 15, 4.2), (43, 9, 2.6), (52, 16, 3.4),
                                       (24, 21, 2.0), (41, 21, 2.2), (6, 21, 1.8), (58, 21, 1.8))):
            dy = tilt * (32 - x) / 28
            yy = y + dy
            p.fill(p.ellipse(x + 0.5, yy + 0.5, r, r * 0.8), spot, radius=1.5, bias=-0.15)
            if r > 1.9:
                p.px(int(x - r / 2), int(round(yy - r / 3)), spot[4])
                p.stroke([(int(x - r + 1), int(round(yy + r * 0.8))), (int(x + r - 1), int(round(yy + r * 0.8)))], cap[4])
            if i in (2, 4, 1, 6):
                p.glow(x, int(round(yy)), mix('#ffa0f0', '#ffffff', g), mix('#b040c0', '#ff6ae0', g), r=0,
                       strength=0.25 + 0.5 * g)
    p.rim('#ff4ad8', side=1, amount=0.45)
    p.rim('#6aff7a', side=-1, amount=0.25)
    p.outline('#0c0610')
    # spores drifting down from the gills
    for x0, ph in ((14, 0.0), (24, 0.4), (40, 0.2), (50, 0.65), (33, 0.85), (6, 0.5), (58, 0.3), (19, 0.75)):
        u = (t + ph) % 1.0
        if u < 0.85:
            y = int(round(31 + u * 30))
            x = x0 + sway(t, ph, 2) + int(round(u * (x0 - 32) / 5))
            p.px(x, y, mix('#c0ff9a' if x0 % 2 else '#ffa0f0', '#3a2040', u / 0.85))
    p.backdrop = (34, 8, 38)
    return p


ALL = {'kolume': kolume, 'kortali': kortali, 'kyro': kyro, 'lanefir': lanefir, 'lizho': lizho, 'ltokk': ltokk,
       'mirveda': mirveda, 'mortheus': mortheus, 'myko': myko}


def sheet(path, keys=None, cols=3):
    from PIL import Image, ImageDraw
    keys = keys or list(ALL)
    cw, ch = 2 * 256 + 12 + 20, 256 + 34
    rows = (len(keys) + cols - 1) // cols
    sh = Image.new('RGBA', (cols * cw + 10, rows * ch + 10), (255, 255, 255, 255))
    dr = ImageDraw.Draw(sh)
    for i, k in enumerate(keys):
        x0, y0 = 10 + (i % cols) * cw, 10 + (i // cols) * ch
        for j, tt in enumerate((0.0, 0.5)):
            pp = ALL[k](tt)
            bg = tuple(getattr(pp, 'backdrop', (12, 14, 30))) + (255,)
            sh.alpha_composite(pp.image(4, bg=bg), (x0 + j * (256 + 12), y0))
        dr.text((x0, y0 + 260), k, fill=(0, 0, 0, 255))
    sh.save(path)


def strip(key, path, ts=(0.0, 0.25, 0.5, 0.75), scale=4):
    from PIL import Image
    w = 64 * scale
    im = Image.new('RGBA', (len(ts) * (w + 8), w), (255, 255, 255, 255))
    for j, tt in enumerate(ts):
        pp = ALL[key](tt)
        bg = tuple(getattr(pp, 'backdrop', (12, 14, 30))) + (255,)
        im.alpha_composite(pp.image(scale, bg=bg), (j * (w + 8), 0))
    im.save(path)


def zoom(key, t=0.0, scale=8):
    pp = ALL[key](t)
    bg = tuple(getattr(pp, 'backdrop', (12, 14, 30))) + (255,)
    pp.image(scale, bg=bg).save(D + '/b6tmp/zoom_%s.png' % key)


if __name__ == '__main__':
    args = sys.argv[1:]
    if not args or args[0] == 'all':
        sheet(D + '/sheet_faces_b6.png')
        for k, fn in ALL.items():
            save_gif(fn, D + '/anim_%s.gif' % k, scale=4)
    elif args[0] == 'sheet':
        sheet(D + '/sheet_faces_b6.png')
    elif args[0] == 'zoom':
        zoom(args[1], float(args[2]) if len(args) > 2 else 0.0)
    else:
        for k in args:
            strip(k, D + '/b6tmp/strip_%s.png' % k)
