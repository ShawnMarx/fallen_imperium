import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))  # noqa: E702
import math  # noqa: E402

from pix import Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, save_gif  # noqa: E402,F401


# ---------------------------------------------------------------- helpers (local to this file)
def bez(a, b, c, n=10):
    """points along a quadratic Bezier a -> c pulled toward b"""
    out = []
    for i in range(n + 1):
        u = i / n
        out.append(((1 - u) ** 2 * a[0] + 2 * (1 - u) * u * b[0] + u * u * c[0],
                    (1 - u) ** 2 * a[1] + 2 * (1 - u) * u * b[1] + u * u * c[1]))
    return out


def taper(p, pts, w0, w1):
    """mask of a polyline that narrows from width w0 to w1 (horns, tendrils, fins)"""
    n = len(pts)
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        if i == 0:
            dx, dy = pts[1][0] - x, pts[1][1] - y
        elif i == n - 1:
            dx, dy = x - pts[i - 1][0], y - pts[i - 1][1]
        else:
            dx, dy = pts[i + 1][0] - pts[i - 1][0], pts[i + 1][1] - pts[i - 1][1]
        ln = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / ln, dx / ln
        w = (w0 + (w1 - w0) * i / max(1, n - 1)) / 2
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    return p.poly(left + right[::-1])


FONT = {  # 3x5 stencil glyphs
    '0': ('###', '#.#', '#.#', '#.#', '###'), '1': ('.#.', '##.', '.#.', '.#.', '###'),
    '2': ('##.', '..#', '.#.', '#..', '###'), '3': ('##.', '..#', '.#.', '..#', '##.'),
    '4': ('#.#', '#.#', '###', '..#', '..#'), '5': ('###', '#..', '##.', '..#', '##.'),
    '6': ('.##', '#..', '###', '#.#', '###'), '7': ('###', '..#', '.#.', '.#.', '.#.'),
    '8': ('###', '#.#', '###', '#.#', '###'), '9': ('###', '#.#', '###', '..#', '##.'),
    '-': ('...', '...', '###', '...', '...'), 'P': ('##.', '#.#', '##.', '#..', '#..'),
    'R': ('##.', '#.#', '##.', '#.#', '#.#'), 'D': ('##.', '#.#', '#.#', '#.#', '##.'),
}


def stencil(p, x, y, text, col, gap=1):
    for ch in text:
        g = FONT[ch]
        for j, row in enumerate(g):
            for i, c in enumerate(row):
                if c == '#':
                    p.px(x + i, y + j, col)
        x += 3 + gap


def rot_ellipse(p, cx, cy, rx, ry, ang, n=40):
    """mask of an ellipse whose rx axis is turned by ang radians (y down)"""
    ca, sa = math.cos(ang), math.sin(ang)
    pts = []
    for i in range(n):
        th = 2 * math.pi * i / n
        x, y = rx * math.cos(th), ry * math.sin(th)
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    return p.poly(pts)


def mirror_pts(pts, cx=32):
    return [(2 * cx - x, y) for x, y in pts]


# ---------------------------------------------------------------- 1. Nokar Sellships
def nokar(t=0.0):
    """a goblin mercenary in a pointed metal hood with ear-spikes, big green goggle eyes, a gas-mask snout,
    turned-up leather coat collar and a bandolier. Moves: breath, the hood follows, the goggles pulse and the
    right lens zooms in on you, an ear twitches, the respirator puffs."""
    p = Portrait()
    metal = ramp('#4e7068', spread=0.75)
    dark = ramp('#26302f', spread=0.72)
    coat = ramp('#94682c', spread=0.7, hue_shift=0.04)
    lining = ramp('#4a3424', spread=0.6)
    strap = ramp('#3e2a1c', spread=0.6)
    brass = ramp('#d8a440', spread=0.55)
    body, head = breath(t), breath(t, -0.1)
    g = pulse(t, cycles=2)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (10, 46), (22, 44), (42, 44), (54, 46), (64, 52), (64, 66)]), coat, radius=9,
               texture=0.05, seed=31)
        p.fill(p.poly([(23, 44), (41, 44), (37, 56), (32, 60), (27, 56)]), dark, radius=3)
        # the turned-up leather collar rising behind the head, lining showing
        p.fill(p.poly([(3, 54), (4, 38), (8, 27), (14, 26), (22, 44), (16, 52)]), coat, radius=4, seed=33)
        p.fill(p.poly([(8, 30), (13, 29), (20, 44), (14, 48), (8, 44)]), lining, radius=3, edge=False)
        p.fill(p.poly([(61, 54), (61, 36), (58, 22), (51, 24), (42, 44), (48, 52)]), coat, radius=4, bias=-0.12, seed=34)
        p.fill(p.poly([(57, 26), (52, 27), (44, 44), (50, 48), (57, 44)]), lining, radius=3, edge=False)
        p.stroke([(4, 38), (8, 27), (14, 26)], coat[4])
        p.stroke([(51, 24), (58, 22), (61, 36)], coat[3])
        # lapels
        p.fill(p.poly([(14, 46), (23, 45), (31, 58), (27, 66), (19, 66)]), coat, radius=3, bias=0.1)
        p.fill(p.poly([(50, 46), (41, 45), (33, 58), (37, 66), (45, 66)]), coat, radius=3, bias=-0.15)
        p.stroke([(23, 46), (30, 58)], coat[4])
        # bandolier with brass shells
        p.fill(taper(p, [(2, 50), (16, 56), (30, 63), (36, 67)], 5, 5), strap, radius=2)
        for k in range(6):
            x, y = 4 + k * 5, 50 + int(k * 2.4)
            p.fill(p.poly([(x, y), (x + 2, y), (x + 2, y + 3), (x, y + 3)]), brass, radius=1, edge=False)
            p.px(x, y, brass[4])
            p.px(x + 1, y + 3, strap[0])
        # orange merc patch (hex aperture) on the right shoulder
        p.fill(p.poly([(52, 54), (55, 52), (58, 54), (58, 58), (55, 60), (52, 58)]), ramp('#ff8a24'), radius=1)
        p.px(55, 56, '#2a1a40')
    with p.at(0, head):
        # goblin ear-spikes on the hood (the left one twitches)
        with p.at(0, -1 if blink(t, 0.3, 2) else 0):
            p.fill(p.poly([(18, 14), (0, 3), (3, 10), (16, 23)]), metal, radius=2)
            p.stroke([(15, 15), (3, 7)], metal[4])
        p.fill(p.poly([(47, 12), (63, 1), (61, 9), (49, 21)]), metal, radius=2, bias=-0.12)
        # the pointed hood, its tip swept up and back
        p.fill(p.poly([(15, 36), (14, 21), (18, 12), (26, 6), (36, 3), (51, 0), (48, 6), (50, 13), (51, 25), (49, 36)]), metal,
               radius=8, texture=0.06, seed=32)
        p.stroke([(19, 12), (27, 7), (36, 4), (48, 1)], metal[4])
        p.stroke([(16, 19), (25, 15), (36, 14), (48, 15)], metal[1])
        for x, y in [(19, 17), (47, 19), (30, 10), (40, 8), (22, 31)]:
            p.px(x, y, metal[4])
        # dark recess under the brim
        p.fill(p.ellipse(32.5, 25.5, 17, 7.5), dark, flat=0, edge=False)
        # panel seams and rivets on the hood
        p.stroke([(31, 3), (29, 9), (29, 15)], metal[1])
        p.stroke([(30, 4), (28, 9)], metal[3])
        # gas-mask snout
        p.fill(p.poly([(24, 28), (42, 28), (42, 36), (37, 45), (32, 47), (28, 45), (24, 36)]), dark, radius=5, texture=0.04)
        for y in (38, 40, 42):
            p.stroke([(30, y), (36, y)], dark[0])
            p.stroke([(31, y - 1), (35, y - 1)], dark[3])
        p.stroke([(33, 30), (33, 35)], dark[4])
        # filter canister on the right cheek
        p.fill(p.ellipse(44.5, 38.5, 3.8, 3.6), brass, radius=2)
        p.stroke([(42, 38), (47, 38)], brass[0])
        p.px(43, 37, brass[4])
        # goggles: brass rims, glowing green lenses (the right lens zooms)
        for cx, zoomable in ((25, False), (40, True)):
            p.fill(p.ellipse(cx + 0.5, 24.5, 6.2, 6.0), brass, radius=3, bias=-0.05)
            p.fill(p.ellipse(cx + 0.5, 24.5, 4.4, 4.2), dark, flat=0, edge=False)
            zoom = zoomable and blink(t, 0.45, 5)
            r = 2.2 if zoom else 3.6
            lens = [mix('#0a5a3c', '#108a58', g), mix('#18b878', '#30d890', g), mix('#40f0a8', '#70ffc0', g),
                    mix('#90ffd0', '#c0ffe4', g), (232, 255, 244)]
            p.fill(p.ellipse(cx + 0.5, 24.5, r, r), lens, radius=2.5, edge=False)
            if zoom:
                ring = p.ellipse(cx + 0.5, 24.5, 4.0, 3.9) & ~p.ellipse(cx + 0.5, 24.5, 3.0, 2.9)
                p.rgb[ring] = (24, 140, 90)
            p.glow(cx - 1, 23, '#ffffff', mix('#7affc0', '#c0ffe0', g), r=0, strength=0.6 + 0.4 * g)
        p.stroke([(31, 24), (34, 24)], brass[1])
        # goggle straps
        p.stroke([(15, 25), (18, 24)], strap[2], width=2)
        p.stroke([(47, 24), (50, 25)], strap[1], width=2)
    p.rim('#3cffa8', side=1, amount=0.5)
    p.rim('#ff9a3a', side=-1, amount=0.3)
    p.outline('#0a0d0b')
    # respirator puffs drifting off the canister
    hd = breath(t, -0.1)
    for ph in (0.0, 0.5):
        u = (t * 2 + ph) % 1.0
        if u < 0.7:
            x = int(round(49 + u * 8))
            y = int(round(38 + hd - u * 10))
            c = mix('#c0f0d8', '#183028', u / 0.7)
            p.pixels([(x, y), (x + 1, y)], c)
            if 0.15 < u < 0.55:
                p.pixels([(x, y - 1), (x + 1, y - 1)], mix(c, (10, 24, 22), 0.35))
    p.backdrop = (10, 24, 22)
    return p


# ---------------------------------------------------------------- 2. Olradin League
def olradin(t=0.0):
    """a shrewd mouse-folk haggler: great round ears, glossy bead eyes, pink nose, whiskers, a teal scarf, a patched navy
    coat, a glowing datapad in one paw. Moves: breath, the head follows, whiskers twitch, the nose sniffs, an ear flicks,
    a blink, the scarf tail sways, the datapad ticks."""
    p = Portrait()
    fur = ramp('#968a90', spread=0.72)
    cream = ramp('#dccabc', spread=0.45)
    pink = ramp('#ee8c9e', spread=0.5)
    coat = ramp('#2c3a64', spread=0.78)
    scarf = ramp('#2a94b6', spread=0.62)
    patch = ramp('#b0582c', spread=0.6)
    brass = ramp('#dcaa40', spread=0.55)
    dark = ramp('#1e1a26', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (12, 46), (26, 48), (40, 48), (52, 46), (64, 52), (64, 66)]), coat, radius=9,
               texture=0.07, seed=41)
        # coat front, brass buttons, a patch on the right shoulder
        p.stroke([(32, 52), (31, 66)], coat[0])
        for y in (57, 63):
            for x in (28, 36):
                p.fill(p.ellipse(x + 0.5, y + 0.5, 1.6, 1.6), brass, radius=1)
                p.px(x, y, brass[4])
        p.fill(p.poly([(48, 55), (57, 53), (59, 61), (50, 63)]), patch, radius=2)
        for x, y in [(48, 54), (50, 54), (52, 53), (54, 53), (56, 52), (59, 55), (59, 57), (59, 59)]:
            p.px(x, y, '#ecd4a4')
    with p.at(0, head):
        # ears (the right one flicks)
        p.fill(p.ellipse(12.5, 14, 10.4, 10.7), fur, radius=7, texture=0.05)
        p.fill(p.ellipse(13.5, 15, 6.8, 7.3), pink, radius=6, bias=-0.05)
        p.stroke([(9, 10), (11, 8)], pink[4])
        flick = blink(t, 0.25, 2)
        with p.at(1 if flick else 0, -1 if flick else 0):
            p.fill(p.ellipse(52, 12.5, 9.8, 10.3), fur, radius=7, bias=-0.08, texture=0.05)
            p.fill(p.ellipse(51, 13.5, 6.4, 7.1), pink, radius=6, bias=-0.1)
        # head and pointed muzzle (turned a little to the right)
        p.fill(p.ellipse(31.5, 24, 14.5, 12.5) | p.poly([(18, 28), (46, 25), (43, 35), (39, 42), (34, 44), (27, 38)]), fur,
               radius=9, texture=0.1, seed=42)
        p.fill(p.poly([(26, 31), (32, 28), (42, 29), (41, 36), (38, 42), (34, 44), (28, 38)]), cream, radius=4, edge=False,
               texture=0.08, seed=43)
        # tuft and cheek fluff
        p.pixels([(30, 11), (31, 10), (32, 11), (33, 10), (34, 11)], fur[4])
        p.pixels([(29, 12), (32, 12), (35, 12)], fur[2])
        p.pixels([(17, 31), (16, 32), (18, 33), (46, 29), (47, 30)], fur[3])
        # eyes: glossy black beads under lowered lids - sly
        shut = blink(t, 0.7, 2)
        for i, (cx, cy) in enumerate(((24, 23), (39, 22))):
            if shut:
                p.stroke([(cx - 2, cy + 1), (cx + 3, cy + 1)], '#1a1218')
                p.stroke([(cx - 2, cy + 2), (cx + 3, cy + 2)], fur[1])
            else:
                p.fill(p.ellipse(cx + 0.5, cy + 0.5, 3.1, 3.3), ramp('#2a2034', spread=0.9), radius=2.5)
                p.pixels([(cx - 1, cy - 1), (cx, cy - 1), (cx - 1, cy)], '#ffffff')
                p.px(cx + 2, cy + 2, '#8a7af0')
                if i == 1:  # one eye narrowed: weighing you up
                    p.stroke([(cx - 3, cy - 3), (cx + 3, cy - 2)], fur[1])
                    p.stroke([(cx - 2, cy - 2), (cx + 3, cy - 1)], '#1a1218')
            # brows
        p.stroke([(20, 18), (23, 17), (27, 18)], fur[4])
        p.stroke([(36, 18), (40, 18), (43, 19)], fur[1])
        # nose (sniffs) and a crooked smirk
        sniff = -1 if (blink(t, 0.1, 1) or blink(t, 0.18, 1)) else 0
        with p.at(0, sniff):
            p.fill(p.ellipse(37.5, 42, 2.7, 2.0), pink, radius=2)
            p.px(36, 41, '#ffe4ea')
        p.stroke([(37, 44), (37, 45)], cream[0])
        p.pixels([(31, 39), (32, 41), (30, 41), (42, 38), (43, 40), (41, 40)], cream[0])
        p.stroke([(34, 46), (37, 45), (40, 45), (42, 43)], '#3a2228')
        # a glowing datapad held up in the left paw
        with p.at(0, body - head):
            p.fill(p.poly([(3, 40), (15, 38), (17, 57), (6, 59)]), dark, radius=2)
            p.fill(p.poly([(5, 42), (14, 40), (15, 54), (7, 56)]), ramp('#2ad8b0', spread=0.4), radius=3, bias=0.15)
            tick = int(t * 6) % 3
            for k, w in enumerate((5, 3 + tick, 6 - tick, 4)):
                y = 44 + k * 3
                p.stroke([(7, y), (7 + w, y - 1)], '#e8fff8' if k == tick else '#0e6a5a')
            p.fill(p.poly([(3, 58), (10, 56), (17, 58), (18, 66), (3, 66)]), coat, radius=3, bias=0.1)
            p.fill(p.poly([(5, 51), (12, 51), (13, 57), (5, 58)]), pink, radius=2, bias=0.1)
            p.stroke([(8, 52), (8, 55)], pink[1])
            p.stroke([(10, 52), (10, 55)], pink[1])
            p.pixels([(6, 51), (9, 51), (11, 51)], pink[4])
            p.fill(p.poly([(3, 46), (5, 45), (6, 49), (4, 50)]), pink, radius=1, bias=0.05)
    with p.at(0, body):
        # scarf: a thick roll around the neck, a knot with a tail
        p.fill(p.poly([(18, 47), (24, 43), (33, 47), (44, 42), (50, 46), (46, 53), (33, 55), (21, 53)]), scarf, radius=4,
               texture=0.06, seed=44)
        p.stroke([(22, 49), (32, 51), (45, 47)], scarf[1])
        p.stroke([(25, 46), (33, 48)], scarf[4])
        with p.at(sway(t, 0.2, 1), 0):
            p.fill(p.poly([(42, 49), (49, 48), (51, 59), (48, 65), (44, 63), (45, 56)]), scarf, radius=3, bias=-0.1)
            p.stroke([(47, 51), (48, 61)], scarf[1])
        p.fill(p.ellipse(44.5, 48.5, 3, 2.6), scarf, radius=2, bias=0.1)
    p.rim('#ff9a3c', side=1, amount=0.5)
    p.rim('#4ad0ff', side=-1, amount=0.25)
    p.outline('#100a12')
    # whiskers (after the outline: thin, loose), twitching
    hd = breath(t, -0.1)
    tw = sway(t, 0.0, 1, cycles=3) if t < 0.5 else 0
    for (x0, y0), (x1, y1) in (((28, 39), (16, 35)), ((28, 40), (15, 41)), ((29, 41), (18, 45)),
                               ((44, 38), (58, 34)), ((44, 39), (60, 40)), ((43, 40), (57, 45))):
        dy1 = tw if x1 > 32 else -tw
        p.stroke([(x0, y0 + hd), (x1, y1 + hd + dy1)], '#e8dfe8')
    p.backdrop = (30, 14, 24)
    return p


# ---------------------------------------------------------------- 3. Zealots of Rhodun
def rhodun(t=0.0):
    """a horned priestess: a silver horned helm-mask with one cyan eye of the faith and a glowing visor, a serene lower
    face, a deep blue cowl and scarf, silver pauldrons, a triangle pendant. Moves: breath, the head follows, the visor and
    the orb pulse, the scarf tails sway, holy motes rise."""
    p = Portrait()
    hood = ramp('#2c4cb0', spread=0.8)
    scarf = ramp('#3266d0', spread=0.75)
    mask = ramp('#aab2c8', spread=0.62)
    horn = ramp('#cfc8c0', spread=0.6, hue_shift=0.03)
    steel = ramp('#7480a4', spread=0.7)
    skin = ramp('#b6a0c4', spread=0.55)
    dark = ramp('#141a36', spread=0.6)
    g = pulse(t, cycles=2)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (14, 44), (32, 48), (50, 44), (64, 50), (64, 66)]), steel, radius=8, texture=0.05)
        p.fill(p.poly([(0, 49), (5, 42), (15, 43), (20, 52), (13, 61), (0, 61)]), steel, radius=4)
        p.fill(p.poly([(64, 49), (59, 42), (49, 43), (44, 52), (51, 61), (64, 61)]), steel, radius=4, bias=-0.12)
        p.stroke([(2, 48), (6, 44), (14, 45)], steel[4])
        p.stroke([(62, 48), (58, 44), (50, 45)], steel[2])
        for x, y in ((6, 52), (58, 52)):
            p.px(x, y, '#9ff4ff')
    with p.at(0, head):
        # the cowl, drawn up over the head
        p.fill(p.poly([(10, 56), (10, 38), (14, 22), (20, 12), (27, 6), (37, 6), (44, 12), (50, 22), (54, 38), (54, 56), (46, 52),
                       (18, 52)]), hood, radius=8, texture=0.03, seed=51)
        p.fill(p.ellipse(32.5, 31, 12.6, 19), dark, radius=4, bias=-0.25)
        p.stroke([(13, 32), (15, 46)], hood[1])
        p.stroke([(51, 32), (50, 46)], hood[0])
        p.stroke([(17, 26), (18, 40)], hood[3])
        p.stroke([(47, 26), (47, 40)], hood[1])
        p.stroke([(16, 20), (22, 11), (27, 8)], hood[4])
        # horns: out, then up and curling in (a lyre), ridged
        for side in (-1, 1):
            pts = bez((32 + side * 6, 13), (32 + side * 29, 17), (32 + side * 23, -4), 12)
            p.fill(taper(p, pts, 8, 1), horn, radius=3, bias=-0.12 if side > 0 else 0.05)
            for k in (1, 3, 5, 7, 9):
                (x0, y0), (x1, y1) = pts[k], pts[k + 1]
                nx, ny = -(y1 - y0), (x1 - x0)
                ln = math.hypot(nx, ny) or 1
                w = (8 - 7 * k / 12) / 2 - 0.5
                p.stroke([(x0 + nx / ln * w, y0 + ny / ln * w), (x0 - nx / ln * w, y0 - ny / ln * w)], horn[1])
        # the skull-mask: a long goat face
        p.fill(p.poly([(22, 20), (24, 13), (29, 9), (35, 9), (40, 13), (42, 20), (41, 28), (38, 34), (37, 42), (34, 47), (30, 47),
                       (27, 42), (26, 34), (23, 28)]), mask, radius=6, texture=0.04, seed=52)
        p.stroke([(25, 14), (29, 10)], mask[4])
        p.stroke([(33, 22), (33, 41)], mask[1])
        p.stroke([(27, 30), (29, 38), (30, 45)], mask[3])
        p.stroke([(37, 30), (35, 38), (34, 45)], mask[0])
        p.pixels([(30, 44), (34, 44)], dark[0])
        # angled sockets with the visor-light inside
        vis = [mix('#1a70c0', '#30a0e8', g), mix('#40c8ff', '#80e8ff', g), mix('#b0f4ff', '#ffffff', g)]
        for s in (-1, 1):
            sock = [(32 + s * 10, 22), (32 + s * 3, 23), (32 + s * 3, 26), (32 + s * 9, 26)]
            p.fill(p.poly(sock), dark, flat=1)
        p.stroke([(24, 24), (29, 24)], vis[1])
        p.stroke([(36, 24), (41, 24)], vis[1])
        p.pixels([(26, 24), (27, 24), (38, 24), (39, 24)], vis[2])
        # engraved inlays on the cheeks
        for s, c1, c2 in ((-1, mask[1], mask[4]), (1, mask[0], mask[2])):
            p.stroke([(32 + s * 8, 28), (32 + s * 4, 30), (32 + s * 4, 33)], c1)
            p.px(32 + s * 6, 31, '#7ff0ff')
            p.stroke([(32 + s * 9, 29), (32 + s * 7, 35), (32 + s * 4, 40)], c2)
        # a rune of light down the bridge
        p.stroke([(32, 20), (32, 38)], mix(mask[2], '#7ff0ff', 0.35 + 0.4 * g))
        p.pixels([(31, 30), (33, 30), (32, 39)], mix(mask[2], '#7ff0ff', 0.3 + 0.4 * g))
        # the eye of the faith in the brow
        p.fill(p.ellipse(32.5, 16, 3.4, 3.4), mask, radius=2, bias=-0.15)
        p.fill(p.ellipse(32.5, 16, 2.3, 2.3), ramp('#3ac8ff', spread=0.4), radius=2)
        p.glow(32, 15, '#ffffff', mix('#40d0ff', '#c0f8ff', g), r=1, strength=0.55 + 0.45 * g)
        # a circlet of beads across the cowl
        for x, y in [(20, 17), (22, 15), (42, 15), (44, 17)]:
            p.px(x, y, '#9ff4ff')
    with p.at(0, body):
        # a stole hanging either side of the chest, embroidered with the faith's triangle
        stole = ramp('#5a8cf0', spread=0.6)
        for x0, ph in ((19, 0.1), (40, 0.35)):
            with p.at(sway(t, ph, 1), 0):
                p.fill(p.poly([(x0, 50), (x0 + 6, 50), (x0 + 6, 67), (x0, 67)]), stole, radius=2, bias=0.05 if x0 < 32 else -0.12)
                p.pixels([(x0 + 2, 60), (x0 + 3, 60), (x0 + 4, 60), (x0 + 3, 61), (x0 + 2, 59), (x0 + 4, 59), (x0 + 3, 58)],
                         '#8ff4ff')
                p.stroke([(x0 + 1, 52), (x0 + 1, 66)], stole[3] if x0 < 32 else stole[1])
        # scarf wrapped around the throat
        p.fill(p.poly([(16, 48), (22, 42), (32, 46), (42, 42), (48, 48), (44, 54), (32, 56), (20, 54)]), scarf, radius=5,
               texture=0.04, seed=53)
        p.stroke([(19, 50), (32, 53), (45, 50)], scarf[1])
        p.stroke([(21, 46), (30, 49)], scarf[4])
        p.stroke([(36, 48), (43, 45)], scarf[2])
        # pendant: the triangle and its orb
        p.stroke([(28, 55), (32, 57), (36, 55)], horn[2])
        p.fill(p.poly([(27, 57), (38, 57), (32, 66)]), horn, radius=2)
        p.glow(32, 59, '#ffffff', mix('#40d0ff', '#b0f6ff', 1 - g), r=1, strength=0.5 + 0.4 * (1 - g))
    p.rim('#46e6ff', side=1, amount=0.5)
    p.rim('#b06aff', side=-1, amount=0.3)
    p.outline('#070a1c')
    # holy motes rising
    for x0, ph in ((7, 0.0), (57, 0.3), (13, 0.62), (52, 0.8), (3, 0.45)):
        u = (t + ph) % 1.0
        y = int(round(62 - u * 40))
        x = x0 + sway(t, ph, 1, cycles=2)
        if u < 0.85:
            c = mix('#c8f8ff', '#1a3070', u)
            p.px(x, y, c)
            p.px(x, y + 1, mix(c, (12, 14, 42), 0.5))
            if u < 0.35:
                p.pixels([(x - 1, y), (x + 1, y)], mix(c, (12, 14, 42), 0.6))
    p.backdrop = (12, 14, 42)
    return p


# ---------------------------------------------------------------- 4. Roh'Dhna Mechatronics
def rohdhna(t=0.0):
    """a heavy industrial mech: a domed helm with a visor slot and a scanning optic, a rangefinder cluster, a chin grille,
    piston neck, huge plated pauldrons with hazard stripes and a stencilled serial, glowing chest vents.
    Moves: breath (pistons), the optic sweeps the visor, the vents pulse, the status LED blinks."""
    p = Portrait()
    plate = ramp('#5c6884', spread=0.75)
    dark = ramp('#242836', spread=0.7)
    steel = ramp('#8e96a8', spread=0.6)
    vent = ramp('#ff8a2a', spread=0.5)
    blue = ramp('#3a9cff', spread=0.5)
    g = pulse(t, cycles=1)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (14, 46), (50, 46), (64, 50), (64, 66)]), dark, radius=8, texture=0.05)
        # chest plate with vents and the gear emblem
        p.fill(p.poly([(19, 48), (45, 48), (47, 60), (41, 67), (23, 67), (17, 60)]), plate, radius=5, texture=0.04, seed=61)
        p.stroke([(20, 49), (44, 49)], plate[4])
        for k in range(4):
            y = 53 + k * 3
            c = mix(vent[2], '#ffe08a', g if k % 2 == 0 else 1 - g)
            p.stroke([(22, y), (29, y)], c)
            p.px(21, y, vent[0])
        gear = p.ellipse(38.5, 57.5, 4.2, 4.2)
        for a in range(6):
            ang = a * math.pi / 3 + 0.3
            gear |= p.ellipse(38.5 + 4.6 * math.cos(ang), 57.5 + 4.6 * math.sin(ang), 1.2, 1.2)
        p.fill(gear, blue, radius=2)
        p.fill(p.ellipse(38.5, 57.5, 1.6, 1.6), dark, flat=0, edge=False)
        # neck pistons
        for x in (23, 27, 36, 40):
            p.fill(p.poly([(x, 38), (x + 2, 38), (x + 2, 50), (x, 50)]), steel, radius=1)
        p.fill(p.poly([(30, 40), (34, 40), (34, 50), (30, 50)]), dark, radius=1)
        # massive pauldrons
        p.fill(p.poly([(-2, 42), (2, 33), (14, 31), (19, 38), (20, 56), (-2, 60)]), plate, radius=6, texture=0.04, seed=62)
        p.fill(p.poly([(66, 42), (62, 33), (50, 31), (45, 38), (44, 56), (66, 60)]), plate, radius=6, bias=-0.12,
               texture=0.04, seed=63)
        p.stroke([(0, 45), (19, 44)], plate[0])
        p.stroke([(45, 44), (64, 45)], plate[0])
        p.stroke([(3, 34), (13, 32)], plate[4])
        p.stroke([(51, 32), (61, 34)], plate[3])
        # hazard stripes on the left pauldron, the serial on the right
        p.fill(p.poly([(1, 47), (18, 47), (18, 53), (1, 53)]), dark, flat=1, edge=False)
        for k in range(5):
            p.stroke([(1 + k * 4, 53), (4 + k * 4, 47)], '#f0c43a')
        stencil(p, 49, 46, '67-', '#ecdfa0')
        stencil(p, 49, 52, '52P', '#ecdfa0')
        for x, y in [(3, 37), (16, 36), (48, 36), (61, 37), (3, 57), (61, 57)]:
            p.px(x, y, steel[4])
    with p.at(0, head):
        # antenna with a status LED
        p.stroke([(45, 8), (50, -1)], steel[2])
        p.stroke([(46, 8), (51, -1)], steel[0])
        led = not blink(t, 0.5, 6)
        p.glow(50, 0, '#fff0f0' if led else '#5a2020', '#ff3a3a' if led else '#401010', r=0, strength=0.85)
        # side housings
        for cx, b in ((15.5, 0.0), (49.5, -0.12)):
            p.fill(p.ellipse(cx, 24, 4.6, 7.4), steel, radius=3, bias=b)
            p.fill(p.ellipse(cx, 24, 2.2, 3.6), dark, radius=2)
        # the helm
        p.fill(p.poly([(17, 16), (21, 7), (32, 2), (43, 7), (48, 16), (49, 30), (45, 40), (38, 44), (26, 44), (19, 40),
                       (16, 30)]), plate, radius=8, texture=0.04, seed=64)
        p.stroke([(21, 8), (32, 3), (42, 7)], plate[4])
        p.stroke([(32, 3), (32, 15)], plate[1])
        p.stroke([(33, 4), (33, 15)], plate[3])
        for x, y in [(20, 13), (45, 13), (19, 34), (46, 34), (26, 8), (39, 8)]:
            p.px(x, y, steel[4])
        # visor slot with the scanning optic
        p.fill(p.poly([(18, 18), (47, 18), (47, 26), (18, 26)]), dark, flat=0, edge=False)
        p.stroke([(19, 21), (46, 21)], blue[1])
        p.stroke([(19, 22), (46, 22)], blue[0])
        p.stroke([(19, 23), (46, 23)], blue[1])
        sx = int(round(32 + 10 * wave(t, 0.0, 1)))
        for x in range(sx - 3, sx + 4):
            p.pixels([(x, 21), (x, 22), (x, 23)], blue[3])
        p.glow(sx, 22, '#ffffff', '#a8e0ff', r=1)
        p.stroke([(17, 17), (48, 17)], plate[0])
        p.stroke([(17, 27), (48, 27)], plate[4])
        # rangefinder cluster on the right cheek
        p.fill(p.ellipse(43, 31, 3.2, 3.2), dark, radius=2)
        p.glow(43, 31, '#fff0d0', '#ff9a3a', r=0, strength=0.5 + 0.4 * pulse(t, 0.25, 3))
        p.px(41, 33, blue[3])
        # chin grille
        p.fill(p.poly([(23, 31), (38, 31), (37, 41), (25, 41)]), dark, radius=2)
        for x in range(25, 37, 2):
            p.stroke([(x, 32), (x, 39)], dark[3])
    p.rim('#46a8ff', side=1, amount=0.5)
    p.rim('#ff9a3a', side=-1, amount=0.3)
    p.outline('#07080e')
    p.backdrop = (10, 14, 30)
    return p


# ---------------------------------------------------------------- 5. Tnelis Syndicate
def tnelis(t=0.0):
    """a fleshy, ridge-skulled boss with a beard of mouth-tendrils, a fedora with a green band, trench-coat collar up,
    a cigar curled in one tendril. Moves: breath, the tendrils sway, the cigar ember glows, smoke curls up,
    the red eyes narrow."""
    p = Portrait()
    skin = ramp('#8e7486', spread=0.68)
    coat = ramp('#9a7a4e', spread=0.7, hue_shift=0.03)
    lining = ramp('#4a3020', spread=0.6)
    hat = ramp('#363240', spread=0.78)
    shirt = ramp('#2a2632', spread=0.6)
    tie = ramp('#c02a3a', spread=0.6)
    band = ramp('#38a860', spread=0.6)
    cig = ramp('#8a5a34', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (12, 47), (52, 47), (64, 52), (64, 66)]), coat, radius=6, texture=0.03, seed=71)
        # the collar turned up behind the head
        p.fill(p.poly([(2, 54), (3, 41), (7, 32), (18, 27), (24, 40), (21, 50)]), coat, radius=2.5, seed=72)
        p.fill(p.poly([(7, 33), (18, 28), (20, 32), (9, 37)]), lining, radius=2, edge=False)
        p.fill(p.poly([(62, 54), (61, 41), (57, 32), (47, 27), (41, 40), (44, 50)]), coat, radius=2.5, bias=-0.12, seed=73)
        p.fill(p.poly([(57, 33), (47, 28), (45, 32), (55, 37)]), lining, radius=2, edge=False)
        p.stroke([(3, 41), (7, 32)], coat[4])
        p.stroke([(57, 32), (61, 41)], coat[3])
        p.stroke([(9, 38), (18, 33), (22, 42)], coat[3])
        p.stroke([(55, 38), (46, 33), (43, 42)], coat[1])
        # shirt, tie and a gold pin
        p.fill(p.poly([(22, 47), (42, 47), (37, 67), (27, 67)]), shirt, radius=3)
        p.fill(p.poly([(30, 50), (34, 50), (35, 62), (32, 67), (29, 62)]), tie, radius=2)
        # lapels
        p.fill(p.poly([(13, 48), (22, 47), (30, 60), (26, 67), (16, 67)]), coat, radius=2, bias=0.1)
        p.fill(p.poly([(51, 48), (42, 47), (34, 60), (38, 67), (48, 67)]), coat, radius=2, bias=-0.15)
        for x, y in ((12, 58), (52, 58)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 1.5, 1.5), ramp('#3a2a20'), radius=1)
        p.stroke([(22, 48), (29, 60)], coat[4])
        p.glow(32, 57, '#fff4c0', '#e0b040', r=0, strength=0.7)
    with p.at(0, head):
        # the bulbous head under the hat
        p.fill(p.ellipse(32.5, 27, 15, 13.5), skin, radius=9, texture=0.1, seed=74)
        # forehead ridges
        for x0 in (22, 26, 30, 34, 38, 42):
            p.stroke([(x0, 17), (x0 + (1 if x0 < 32 else 0), 23)], skin[1])
            p.stroke([(x0 + 1, 17), (x0 + (2 if x0 < 32 else 1), 22)], skin[3])
        # brow ridge and red eyes in the shadow of the brim
        p.fill(p.poly([(18, 24), (31, 27), (34, 27), (47, 24), (46, 29), (19, 29)]), skin, flat=0, edge=False)
        p.stroke([(18, 23), (31, 26)], skin[3])
        p.stroke([(34, 26), (47, 23)], skin[2])
        narrow = blink(t, 0.66, 3)
        for cx in (25, 39):
            if narrow:
                p.stroke([(cx - 1, 28), (cx + 2, 28)], '#ff4040')
            else:
                p.pixels([(cx - 1, 28), (cx, 28), (cx + 1, 28), (cx + 2, 28), (cx, 27), (cx + 1, 27)], '#ff3030')
                p.glow(cx + 1, 28, '#ffe8d8', '#ff3020', r=0, strength=0.75)
        p.glow(32, 21, '#ffd0c0', '#c03040', r=0, strength=0.5)  # the third eye, half-open
        p.pixels([(30, 32), (34, 32)], skin[0])
        p.stroke([(31, 31), (33, 31)], skin[3])
        # the beard of tendrils, back row first
        for k, (x0, dx, ln, ph) in enumerate(((24, -5, 52, 0.8), (27, -2, 57, 0.0), (30, -1, 54, 0.2), (33, 1, 58, 0.4),
                                              (36, 2, 53, 0.6))):
            s = sway(t, ph, 1)
            pts = bez((x0, 34), (x0 + dx * 0.2, 46), (x0 + dx + s, ln), 9)
            p.fill(taper(p, pts, 4, 1.5), skin, radius=2, bias=-0.06 * (k % 2))
            for j in (2, 4, 6, 8):
                x, y = pts[j]
                p.px(int(round(x)), int(round(y)), skin[1])
        # the cigar tendril curls out to the right
        cs = sway(t, 0.5, 1) if t < 0.5 else 0
        p.fill(taper(p, bez((39, 34), (45, 44), (44, 47 + cs), 8), 4, 2), skin, radius=2, bias=-0.1)
        p.fill(p.poly([(43, 44 + cs), (54, 42 + cs), (55, 44 + cs), (44, 47 + cs)]), cig, radius=1)
        p.stroke([(50, 43 + cs), (50, 45 + cs)], '#d8b040')
        ember = pulse(t, 0.0, 2)
        p.glow(55, 43 + cs, mix('#ffb040', '#fff4c0', ember), mix('#c03010', '#ff7020', ember), r=0, strength=0.9)
        # fedora: brim, crown with a pinch, green band
        p.fill(p.poly([(6, 18), (18, 13), (47, 13), (59, 16), (55, 20), (45, 17), (22, 17), (10, 21)]), hat, radius=3)
        p.fill(p.poly([(17, 14), (19, 4), (26, 0), (39, 0), (46, 4), (48, 14)]), hat, radius=6, texture=0.04)
        p.stroke([(28, 1), (32, 6), (36, 1)], hat[0])
        p.stroke([(22, 4), (26, 2)], hat[4])
        p.fill(p.poly([(18, 10), (47, 10), (48, 14), (17, 14)]), band, radius=1)
        p.pixels([(41, 11), (42, 12), (43, 11)], '#ffd84a')
    p.rim('#4aff8a', side=1, amount=0.45)
    p.rim('#ff4a6a', side=-1, amount=0.3)
    p.outline('#0a080c')
    # cigar smoke curling up
    hd = breath(t, -0.1)
    for ph in (0.0, 0.33, 0.66):
        u = (t + ph) % 1.0
        x = int(round(56 + sway(t, ph, 1, cycles=2) + u * 3))
        y = int(round(41 + hd - u * 24))
        if u < 0.8:
            c = mix('#c8c0d0', '#1a2a20', u / 0.8)
            p.px(x, y, c)
            if 0.2 < u < 0.5:
                p.px(x + 1, y - 1, c)
    p.backdrop = (12, 24, 18)
    return p


# ---------------------------------------------------------------- 6. Vaden Banking Clans
def vaden(t=0.0):
    """a grinning reptilian loan shark: a broad gecko head with swept fins pierced by gold rings, yellow slit eyes,
    a gold monocle, a wide toothy grin with a gold tooth, teal high-collared coat, yellow waistcoat, a heavy gold chain
    with a coin medallion. Moves: breath, the head follows, the coin twists on its chain, gold glints, the eyes glance."""
    p = Portrait()
    skin = ramp('#c4aeaa', spread=0.62)
    coat = ramp('#1e8084', spread=0.75)
    lining = ramp('#0e3c40', spread=0.6)
    shirt = ramp('#ecc430', spread=0.5)
    gold = ramp('#e6ac2e', spread=0.55)
    dark = ramp('#24202a', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 52), (12, 47), (52, 47), (64, 52), (64, 66)]), coat, radius=7, texture=0.04, seed=81)
        # the high collar standing behind the head, gold piping
        p.fill(p.poly([(2, 56), (3, 41), (7, 31), (18, 27), (24, 40), (21, 52)]), coat, radius=2.5, seed=82)
        p.fill(p.poly([(7, 32), (18, 28), (20, 32), (9, 37)]), lining, radius=2, edge=False)
        p.fill(p.poly([(62, 56), (61, 43), (57, 35), (48, 32), (42, 42), (44, 52)]), coat, radius=2.5, bias=-0.12, seed=83)
        p.fill(p.poly([(57, 36), (48, 33), (46, 37), (55, 40)]), lining, radius=2, edge=False)
        p.stroke([(3, 41), (7, 31), (18, 27)], gold[3])
        p.stroke([(48, 32), (57, 35), (61, 43)], gold[2])
        p.stroke([(9, 38), (19, 33), (23, 42)], coat[3])
        p.stroke([(55, 41), (47, 37), (44, 44)], coat[1])
        # neck, ridged
        p.fill(p.poly([(19, 30), (40, 34), (44, 50), (22, 50)]), skin, radius=5, bias=-0.2)
        for y in (40, 43, 46):
            p.stroke([(23, y), (36, y)], skin[1])
        # waistcoat
        p.fill(p.poly([(20, 48), (44, 48), (39, 67), (25, 67)]), shirt, radius=3, texture=0.04)
        p.stroke([(24, 52), (27, 66)], shirt[1])
        p.stroke([(40, 52), (37, 66)], shirt[0])
        # white shirt collar points and a dark cravat for the coin to hang on
        p.fill(p.poly([(25, 48), (39, 48), (35, 59), (32, 62), (29, 59)]), ramp('#1c3e48', spread=0.6), radius=2)
        p.fill(p.poly([(23, 47), (29, 48), (27, 52)]), ramp('#e8eef0', spread=0.4), radius=1)
        p.fill(p.poly([(41, 47), (35, 48), (37, 52)]), ramp('#e8eef0', spread=0.4), radius=1, bias=-0.1)
        for y in (62, 65):
            p.pixels([(32, y), (33, y)], gold[3])
        # lapels
        p.fill(p.poly([(12, 48), (21, 47), (28, 60), (24, 67), (14, 67)]), coat, radius=2, bias=0.1)
        p.fill(p.poly([(52, 48), (43, 47), (36, 60), (40, 67), (50, 67)]), coat, radius=2, bias=-0.15)
        p.stroke([(21, 48), (28, 60), (24, 66)], gold[3])
        p.stroke([(43, 48), (36, 60), (40, 66)], gold[2])
        # heavy gold chain and the coin medallion, twisting on it
        for k, (x, y) in enumerate(bez((20, 47), (32, 58), (44, 47), 10)):
            p.px(int(round(x)), int(round(y)), gold[3] if k % 2 else gold[1])
        w = abs(math.cos(math.pi * 2 * t))
        cx, cy = 32, 56
        if w > 0.7:
            p.fill(p.ellipse(cx + 0.5, cy + 0.5, 3.8, 3.8), gold, radius=2.5)
            p.fill(p.ellipse(cx + 0.5, cy + 0.5, 2.0, 2.0), gold, flat=1, edge=False)
            p.pixels([(cx, cy - 1), (cx, cy), (cx, cy + 1), (cx + 1, cy - 1), (cx - 1, cy + 1)], gold[4])
        elif w > 0.35:
            p.fill(p.ellipse(cx + 0.5, cy + 0.5, 2.3, 3.8), gold, radius=2)
            p.stroke([(cx, cy - 1), (cx, cy + 1)], gold[4])
        else:
            p.fill(p.poly([(cx - 1, cy - 3), (cx + 1, cy - 3), (cx + 1, cy + 4), (cx - 1, cy + 4)]), gold, radius=1)
    with p.at(0, head):
        # swept head-fins, pierced with gold rings
        p.fill(p.poly([(24, 11), (7, -1), (11, 6), (21, 17)]), skin, radius=2, bias=0.05)
        p.fill(p.poly([(19, 16), (2, 10), (7, 16), (18, 23)]), skin, radius=2)
        p.stroke([(21, 12), (11, 4)], skin[1])
        p.stroke([(17, 17), (6, 13)], skin[1])
        for x, y in ((12, 4), (7, 13)):
            p.pixels([(x, y - 1), (x + 1, y), (x, y + 1), (x - 1, y)], gold[3])
            p.px(x + 1, y - 1, gold[4])
        # lower jaw, then the skull and the long upper jaw
        p.fill(p.poly([(20, 28), (36, 31), (52, 30), (54, 33), (46, 37), (30, 39), (21, 35)]), skin, radius=4, bias=-0.15,
               texture=0.06, seed=85)
        p.fill(p.ellipse(27, 19, 11.4, 11.2) | p.poly([(25, 11), (40, 12), (50, 17), (57, 23), (57, 28), (52, 30), (38, 31),
                                                        (27, 32), (18, 27)]), skin, radius=8, texture=0.07, seed=84)
        for x, y in [(22, 12), (27, 10), (18, 17), (31, 13), (21, 25), (37, 13), (44, 16), (48, 19), (52, 22)]:
            p.px(x, y, skin[1])
        p.stroke([(36, 13), (46, 16), (53, 21)], skin[4])
        # brow ridge, cheekbone
        p.fill(p.poly([(27, 15), (38, 14), (40, 17), (28, 18)]), skin, radius=1, bias=0.2)
        p.stroke([(22, 24), (28, 26), (36, 27)], skin[1])
        # the grin: a long jaw-line with sharp teeth, a gold one
        p.stroke([(25, 28), (27, 30), (30, 31), (38, 32), (46, 32), (53, 30), (56, 28)], '#2e161c')
        p.stroke([(22, 20), (24, 26)], skin[1])
        p.stroke([(40, 23), (50, 24)], skin[1])
        for x in range(31, 55, 2):
            y = 32 if x < 46 else (31 if x < 52 else 30)
            p.px(x, y + 1, '#f4ece0')
            p.px(x + 1, y - 1, '#e8e0d4')
        p.pixels([(41, 33), (41, 34)], '#ffd040')
        p.pixels([(55, 24), (56, 25)], skin[0])
        # eyes: glowing yellow slits, glancing sideways
        look = 1 if 0.3 < t < 0.62 else 0
        p.fill(p.ellipse(33.5, 20.5, 3.4, 2.4), dark, flat=0, edge=False)
        p.fill(p.ellipse(33.5, 20.5, 2.6, 1.6), [(150, 96, 10), (210, 150, 20), (255, 210, 50), (255, 232, 110),
                                                (255, 244, 180)], radius=2, edge=False)
        p.pixels([(33 + look, 19), (33 + look, 20), (33 + look, 21)], '#2a1a10')
        p.px(31, 20, '#fff6c0')
        p.stroke([(31, 19), (36, 19)], skin[1])  # a heavy, smug lid
        p.fill(p.ellipse(46, 18.5, 2.2, 1.7), dark, flat=0, edge=False)
        p.pixels([(45, 18), (46, 18), (47, 18)], '#ffd83a')
        p.px(46 + look, 18, '#2a1a10')
        # gold monocle on the near eye, its chain falling to the coat
        ring = p.ellipse(33.5, 20.5, 4.8, 4.5) & ~p.ellipse(33.5, 20.5, 3.5, 3.2)
        p.fill(ring, gold, flat=3, edge=False)
        p.fill(ring & p.poly([(28, 22), (40, 22), (40, 27), (28, 27)]), gold, flat=1, edge=False)
        p.pixels([(30, 17), (31, 16)], gold[4])
        p.stroke([(30, 25), (26, 32), (24, 40)], gold[2])
    p.rim('#ffc83a', side=1, amount=0.45)
    p.rim('#3affe0', side=-1, amount=0.3)
    p.outline('#061010')
    # gold glints
    for (x, y), at in (((13, 3 + head), 0.2), ((34, 54 + body), 0.7), ((36, 18 + head), 0.45), ((8, 12 + head), 0.9)):
        if blink(t, at, 2):
            p.pixels([(x, y), (x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)], '#fff6c0')
    p.backdrop = (6, 30, 30)
    return p


# ---------------------------------------------------------------- 7. Vaylerian Scourge
def vaylerian(t=0.0):
    """a deep-sea raider: fan fins at the temples with crimson edges, a blade crest, dark blue skin dotted with glowing
    photophores, burning red eyes, a fanged mouth, gill slits, a pearl-studded violet shell carapace.
    Moves: breath, the head follows, the fins ripple, the gills flare, the eyes and photophores pulse, bubbles rise."""
    p = Portrait()
    skin = ramp('#2c6a8c', spread=0.72)
    shell = ramp('#3c3a70', spread=0.75)
    pearl = ramp('#e8dce8', spread=0.4)
    memb = ramp('#44b4d4', spread=0.6)
    spine = ramp('#1a3c6a', spread=0.6)
    g = pulse(t, cycles=2)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (14, 45), (32, 49), (50, 45), (64, 50), (64, 66)]), shell, radius=8, texture=0.05)
        p.fill(p.poly([(0, 48), (6, 40), (18, 42), (23, 52), (15, 61), (0, 61)]), shell, radius=5, texture=0.05, seed=91)
        p.fill(p.poly([(64, 48), (58, 40), (46, 42), (41, 52), (49, 61), (64, 61)]), shell, radius=5, bias=-0.12,
               texture=0.05, seed=92)
        p.stroke([(3, 46), (8, 42), (16, 43)], shell[4])
        p.stroke([(61, 46), (56, 42), (48, 43)], shell[3])
        p.stroke([(4, 58), (14, 56), (20, 50)], shell[1])
        p.stroke([(60, 58), (50, 56), (44, 50)], shell[0])
        for x, y, r in ((9, 50, 2.3), (15, 54, 1.7), (54, 50, 2.3), (49, 54, 1.7), (32, 61, 2.4)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, r, r), pearl, radius=2)
        # neck with gill slits
        p.fill(p.poly([(25, 36), (39, 36), (41, 50), (32, 54), (23, 50)]), skin, radius=4, bias=-0.2, texture=0.05)
        for k, y in enumerate((41, 44, 47)):
            c = mix('#5a1030', '#ff4a6a', g if k != 1 else 1 - g)
            p.stroke([(24, y), (29, y + 1)], c)
            p.stroke([(40, y), (35, y + 1)], c)
        # spiky collar
        for pts in ([(16, 45), (11, 34), (22, 43)], [(48, 45), (53, 34), (42, 43)], [(21, 47), (19, 38), (26, 46)],
                    [(43, 47), (45, 38), (38, 46)]):
            p.fill(p.poly(pts), shell, radius=2)
    with p.at(0, head):
        # fan fins at the temples: membrane between spines, crimson edges; each spine tip ripples
        for side in (-1, 1):
            base = [(32 + side * 10, 13), (32 + side * 11, 19), (32 + side * 11, 25), (32 + side * 10, 31)]
            tips = [(32 + side * 24, 2), (32 + side * 29, 12), (32 + side * 29, 23), (32 + side * 24, 34)]
            tips = [(x, y + sway(t, 0.18 * k + (0.5 if side > 0 else 0), 1)) for k, (x, y) in enumerate(tips)]
            edge = []
            for k in range(len(tips)):
                edge.append(tips[k])
                if k < len(tips) - 1:  # scalloped membrane between spines
                    edge.append(((tips[k][0] + tips[k + 1][0]) / 2 - side * 3, (tips[k][1] + tips[k + 1][1]) / 2))
            p.fill(p.poly(base[:1] + edge + base[-1:]), memb, radius=3, bias=-0.15 if side > 0 else 0.05, texture=0.05)
            p.stroke(edge, mix('#c02050', '#ff4a7a', g))
            for (bx, by), (tx, ty) in zip(base, tips):
                p.stroke([(bx, by), (tx, ty)], spine[2] if side < 0 else spine[1])
                p.px(int(round(tx)), int(round(ty)), '#ffd0dc')
        # the blade crest, seen edge-on
        p.fill(p.poly([(29, 10), (30, 0), (32, -4), (34, 0), (35, 10)]), memb, radius=2)
        p.stroke([(32, -3), (32, 9)], spine[1])
        # the head: long, pointed chin
        p.fill(p.poly([(32, 5), (39, 8), (43, 16), (44, 26), (41, 35), (36, 42), (32, 44), (28, 42), (23, 35), (20, 26), (21, 16),
                       (25, 8)]), skin, radius=8, texture=0.08, seed=93)
        # skull ridges and dark speckles
        p.stroke([(32, 6), (32, 16)], skin[3])
        p.stroke([(28, 8), (27, 16)], skin[3])
        p.stroke([(36, 8), (37, 16)], skin[2])
        for x, y in [(30, 12), (34, 13), (25, 18), (39, 17), (23, 30), (41, 29), (29, 33), (36, 32)]:
            p.px(x, y, skin[0])
        # brow and cheek ridges
        p.stroke([(22, 21), (29, 23)], skin[4])
        p.stroke([(42, 21), (35, 23)], skin[3])
        p.stroke([(24, 30), (28, 36)], skin[1])
        p.stroke([(40, 30), (36, 36)], skin[0])
        # burning red eyes, slanted
        ec = mix('#e02030', '#ff5a5a', g)
        for s in (-1, 1):
            p.fill(p.poly([(32 + s * 10, 23), (32 + s * 3, 25), (32 + s * 3, 27), (32 + s * 9, 26)]), skin, flat=0, edge=False)
            p.stroke([(32 + s * 9, 24), (32 + s * 4, 26)], ec)
            p.glow(32 + s * 6, 25, '#fff0f0', mix('#ff2a2a', '#ff8a8a', g), r=0, strength=0.6 + 0.4 * g)
        # nostril slits and a fanged mouth
        p.pixels([(31, 31), (33, 31)], skin[0])
        p.stroke([(26, 36), (29, 38), (35, 38), (38, 36)], '#0a1424')
        p.pixels([(28, 37), (29, 39), (35, 39), (36, 37)], '#eef4f4')
        # photophores: glowing dots that pulse in turn
        for k, (x, y) in enumerate([(26, 14), (38, 14), (24, 20), (40, 20), (25, 32), (39, 32), (32, 20)]):
            k2 = pulse(t, k / 7.0, 1)
            p.px(x, y, mix(skin[2], '#9ff8ff', k2))
    p.rim('#3ff0ff', side=1, amount=0.5)
    p.rim('#ff3a6a', side=-1, amount=0.3)
    p.outline('#040a14')
    # bubbles rising
    for x0, ph, big in ((9, 0.0, True), (55, 0.4, False), (5, 0.7, False), (59, 0.15, True), (47, 0.85, False)):
        u = (t + ph) % 1.0
        y = int(round(62 - u * 44))
        x = x0 + sway(t, ph, 1, cycles=2)
        if u < 0.9:
            c = mix('#bff4ff', '#1a4060', u)
            if big:
                p.pixels([(x, y - 1), (x - 1, y), (x + 1, y), (x, y + 1)], c)
            else:
                p.px(x, y, c)
    p.backdrop = (4, 18, 34)
    return p


# ---------------------------------------------------------------- 8. Veldyr Sovereignty
def veldyr(t=0.0):
    """a sovereign in a smooth, swept-back white helm: a dark glossy faceplate edged with amber lights, a black ear-disc,
    cable locks falling into a high cowled collar, cyan ring lights on the shoulders and a cyan glyph.
    Moves: breath, the head follows, the amber lights chase, the cyan lights pulse, a reflection sweeps the visor,
    the cables sway."""
    p = Portrait()
    white = ramp('#e2e6ee', spread=0.5, hue_shift=0.04)
    robe = ramp('#3e3850', spread=0.75)
    lining = ramp('#1c1828', spread=0.6)
    dark = ramp('#15171e', spread=0.7)
    cab = ramp('#2c2e38', spread=0.7)
    g = pulse(t, cycles=2)
    cyan = mix('#1aa0b0', '#8ffcff', g)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 50), (12, 45), (52, 45), (64, 50), (64, 66)]), robe, radius=8, texture=0.05, seed=101)
        # the cowl standing high behind the helm
        # the cowl lying in folds around the neck, high behind the helm
        p.fill(p.poly([(3, 58), (4, 44), (9, 34), (18, 29), (32, 30), (46, 29), (55, 34), (60, 44), (61, 58), (32, 50)]), robe,
               radius=5, seed=102)
        p.fill(p.poly([(10, 37), (18, 32), (46, 32), (54, 37), (46, 44), (18, 44)]), lining, radius=3, edge=False)
        p.stroke([(5, 44), (9, 35), (18, 30)], robe[4])
        p.stroke([(46, 30), (55, 35), (59, 44)], robe[3])
        p.stroke([(8, 46), (14, 40)], robe[1])
        p.stroke([(56, 46), (50, 40)], robe[0])
        # shoulder ring lights and the chest glyph
        for x, y in ((8, 56), (56, 56)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 4.6, 4.2), dark, radius=2)
            ring = p.ellipse(x + 0.5, y + 0.5, 3.4, 3.0) & ~p.ellipse(x + 0.5, y + 0.5, 2.0, 1.7)
            p.rgb[ring] = cyan
            p.a |= ring
            p.px(x, y, '#e8ffff')
        p.fill(p.poly([(20, 45), (46, 45), (40, 56), (33, 58), (26, 56)]), lining, radius=2)
    with p.at(0, head):
        # the dark mechanical face under the shell
        face = p.poly([(20, 20), (30, 11), (44, 11), (50, 19), (51, 31), (46, 39), (37, 43), (27, 41), (20, 34)])
        p.fill(face, ramp('#1c2232', spread=0.75), radius=5, bias=0.05)
        # cable locks falling from the jaw
        for k, x0 in enumerate(range(24, 47, 3)):
            s = sway(t, 0.12 * k, 1) if k % 2 else 0
            top = 38 if 28 < x0 < 44 else 35
            p.fill(taper(p, [(x0, top), (x0 + 0.5, 45), (x0 + s - (1 if x0 > 40 else 0), 53)], 3, 2), cab, radius=1)
            p.px(x0, 42 + (k % 3) * 3, cab[3])
        # glossy faceplate with an inner lens and a reflection sweeping it
        p.stroke([(36, 39), (45, 36), (49, 30)], dark[2])
        p.fill(p.ellipse(40.5, 27.5, 3.6, 3.6), dark, radius=2)
        p.fill(p.ellipse(40.5, 27.5, 2.2, 2.2), dark, flat=2, edge=False)
        p.glow(40, 27, mix('#ff9a30', '#ffd080', g), '#a04a10', r=0, strength=0.45)
        rx = int(round(22 + 34 * ((t * 1.0) % 1.0)))
        refl = p.line_mask([(rx, 40), (rx + 6, 14)], 2) & face
        p.rgb[refl] = (64, 78, 104)
        # the white shell, swept back over the skull and down behind the jaw
        shell = p.poly([(48, 15), (44, 7), (34, 2), (22, 3), (13, 10), (9, 21), (11, 32), (17, 39), (24, 40), (27, 34), (32, 26),
                        (39, 19)])
        p.fill(shell, white, radius=10, texture=0.02)
        p.stroke([(16, 9), (24, 5), (33, 4)], white[4])
        p.stroke([(12, 26), (15, 33), (21, 38)], white[1])
        p.stroke([(19, 16), (24, 24), (26, 31)], white[2])
        # black ear disc on the shell
        p.fill(p.ellipse(17.5, 24.5, 3.4, 3.8), ramp('#9aa2b4', spread=0.6), radius=2)
        p.fill(p.ellipse(17.5, 24.5, 2.0, 2.3), dark, flat=1, edge=False)
        p.px(17, 24, cyan)
        # amber lights along the shell's edge, one chasing bright
        lights = [(26, 36), (28, 32), (31, 28), (34, 24), (37, 21), (41, 18), (45, 16)]
        k = int(t * 14) % len(lights)
        for i, (x, y) in enumerate(lights):
            if i == k:
                p.glow(x, y, '#fff4c0', '#ffb030', r=0, strength=0.9)
            else:
                p.px(x, y, '#e08a1c' if (i + k) % 2 else '#b86a14')
    with p.at(0, body):
        # high wrapped collar in front of the cables
        p.stroke([(20, 46), (24, 52)], robe[4])
        p.stroke([(47, 46), (44, 52)], robe[1])
        p.stroke([(30, 56), (30, 61), (34, 61), (34, 64)], cyan)
        p.stroke([(34, 56), (34, 58)], cyan)
    p.rim('#3ff0ff', side=1, amount=0.5)
    p.rim('#ffb43a', side=-1, amount=0.25)
    p.outline('#05060c')
    p.backdrop = (8, 14, 32)
    return p


# ---------------------------------------------------------------- 9. Zelian Purifiers
def _pebble(p, x0, y0, r, rock, seed, outline):
    """a small drifting rock drawn over the finished portrait, with its own outline"""
    from scipy import ndimage
    pts = []
    for i in range(6):
        a = i * math.pi / 3 + seed
        rr = r * (0.75 + 0.25 * ((seed * 7 + i * 3) % 4) / 3)
        pts.append((x0 + rr * math.cos(a), y0 + rr * math.sin(a)))
    m = p.poly(pts)
    ring = ndimage.binary_dilation(m) & ~m
    p.fill(m, rock, radius=1.5, texture=0.1, seed=int(seed * 10))
    p.rgb[ring] = hexrgb(outline)
    p.a |= ring


def zelian(t=0.0):
    """a craggy stone golem: a chiselled boulder head under a heavy brow, white-hot eyes and a burning crack of a mouth,
    boulder shoulders split by glowing seams, pebbles floating about it. Moves: breath, the cracks pulse, the jaw grinds,
    grit falls, pebbles drift."""
    p = Portrait()
    rock = ramp('#865650', spread=0.85, hue_shift=0.05, sat_boost=0.2)
    rock2 = ramp('#4c3044', spread=0.75)
    hot = ramp('#ff3a5a', spread=0.45)
    g = pulse(t, cycles=2)
    glow_c = mix('#e0204a', '#ff7a98', g)
    core_c = mix('#ffc8d4', '#ffffff', g)
    body, head = breath(t), breath(t, -0.1)

    def crack(seg, core_at=(), wide=False):
        if wide:
            p.stroke(seg, mix('#5a0a24', '#a01838', g), width=2)
        p.stroke(seg, glow_c)
        for x, y in core_at:
            p.px(x, y, core_c)

    with p.at(0, body):
        p.fill(p.poly([(-2, 66), (-2, 44), (3, 37), (9, 35), (14, 32), (20, 35), (25, 52), (20, 66)]), rock, radius=7, texture=0.1,
               seed=111)
        p.fill(p.poly([(66, 66), (66, 44), (61, 37), (55, 35), (50, 32), (44, 35), (39, 52), (44, 66)]), rock, radius=7, bias=-0.1,
               texture=0.1, seed=112)
        p.fill(p.poly([(3, 37), (9, 35), (14, 32), (19, 38), (8, 44)]), rock, radius=2, bias=0.35, edge=False, texture=0.1)
        p.fill(p.poly([(61, 37), (55, 35), (50, 32), (45, 38), (56, 44)]), rock, radius=2, bias=0.1, edge=False, texture=0.1)
        p.fill(p.poly([(0, 52), (8, 44), (14, 50), (6, 60), (0, 60)]), rock, radius=2, bias=-0.15, edge=False, texture=0.1)
        p.fill(p.poly([(64, 52), (56, 44), (50, 50), (58, 60), (64, 60)]), rock, radius=2, bias=-0.35, edge=False, texture=0.1)
        p.fill(p.poly([(18, 50), (25, 45), (39, 45), (46, 50), (44, 66), (20, 66)]), rock2, radius=6, texture=0.1, seed=113)
        crack([(3, 41), (8, 47), (6, 55), (9, 62)], [(8, 47), (6, 55)], wide=True)
        crack([(14, 36), (17, 42)])
        crack([(61, 41), (56, 48), (58, 56)], [(56, 48)], wide=True)
        crack([(50, 36), (47, 42)])
        crack([(28, 50), (32, 56), (30, 63)], [(32, 56)], wide=True)
        crack([(37, 48), (35, 53)])
    with p.at(0, head):
        outline_pts = [(15, 16), (17, 9), (22, 6), (25, 2), (33, 1), (38, 4), (43, 3), (48, 9), (50, 17), (48, 22), (50, 27),
                       (51, 33), (46, 40), (40, 45), (31, 47), (23, 44), (17, 38), (13, 31), (15, 26), (13, 21)]
        p.fill(p.poly(outline_pts), rock, radius=8, texture=0.1, seed=114)
        # chiselled facets
        for pts, b in (([(17, 9), (22, 6), (25, 2), (33, 1), (30, 10), (20, 13)], 0.4), ([(33, 1), (38, 4), (43, 3), (48, 9),
                                                                                         (40, 11), (30, 10)], 0.15),
                       ([(15, 16), (20, 13), (22, 20), (15, 26), (13, 21)], 0.25), ([(48, 9), (50, 17), (48, 22), (44, 21),
                                                                                      (42, 12)], -0.3),
                       ([(13, 31), (17, 38), (23, 44), (25, 38), (19, 32)], 0.05), ([(50, 27), (51, 33), (46, 40), (40, 45), (39, 38),
                                                                                    (45, 32)], -0.35)):
            p.fill(p.poly(pts), rock, radius=2, bias=b, edge=False, texture=0.1, seed=115)
        p.stroke([(20, 13), (30, 10), (40, 11)], rock[1])
        p.stroke([(30, 10), (33, 2)], rock[1])
        crack([(25, 3), (27, 7), (25, 10)], [(27, 7)], wide=True)
        crack([(47, 10), (43, 14), (45, 18)], [(43, 14)], wide=True)
        crack([(15, 28), (19, 31)])
        crack([(50, 30), (46, 32)])
        # heavy brow ledge and the shadow under it
        for cx in (24.5, 39.5):
            p.fill(p.ellipse(cx, 25.5, 6.2, 3.4), rock, flat=0, edge=False)
        p.fill(p.poly([(14, 17), (24, 17), (31, 20), (33, 20), (40, 17), (50, 16), (49, 21), (33, 24), (31, 24), (15, 21)]), rock,
               radius=2, bias=0.15, texture=0.12)
        # white-hot eyes
        for cx in (25, 39):
            p.pixels([(cx - 3, 25), (cx - 2, 25), (cx - 1, 25), (cx, 25), (cx + 1, 25), (cx + 2, 25)], mix('#5a0a24', '#a01838', g))
            p.pixels([(cx - 3, 26), (cx - 2, 26), (cx - 1, 26), (cx, 26), (cx + 1, 26), (cx + 2, 26)], glow_c)
            p.pixels([(cx - 2, 26), (cx - 1, 26), (cx, 26), (cx + 1, 26)], core_c)
            p.glow(cx - 1, 26, '#ffffff', hot[3], r=0, strength=0.45 + 0.35 * g)
        # a nose of stone
        p.fill(p.poly([(30, 24), (35, 24), (36, 32), (29, 32)]), rock, radius=2, bias=0.1)
        # the grinding jaw and the burning crack of the mouth
        grind = 1 if blink(t, 0.4, 3) else (-1 if blink(t, 0.9, 2) else 0)
        with p.at(grind, 0):
            p.fill(p.poly([(21, 37), (43, 37), (41, 44), (32, 47), (24, 44)]), rock2, radius=3, texture=0.1, seed=116)
            p.stroke([(24, 44), (32, 46), (40, 44)], rock2[0])
        crack([(21, 36), (25, 38), (28, 36), (32, 38), (36, 36), (40, 38), (43, 36)], [(25, 38), (32, 38), (40, 38)], wide=True)
    p.rim('#ff4a6a', side=1, amount=0.5)
    p.rim('#9a6aff', side=-1, amount=0.25)
    p.outline('#0e0608')
    # drifting pebbles (with their own outline, so the rim light leaves them alone)
    for (x0, y0, r), ph in (((7, 14, 3.2), 0.0), ((57, 21, 2.8), 0.35), ((9, 28, 2.2), 0.6), ((56, 7, 2.3), 0.8)):
        yy = y0 + head + sway(t, ph, 1)
        _pebble(p, x0, yy, r, ramp('#9e6a60', spread=0.7), ph * 6 + 1, '#0e0608')
        p.px(int(round(x0 + r * 0.6)), int(round(yy + r * 0.3)), glow_c)
    # falling grit
    for x0, ph, y0 in ((23, 0.0, 45), (41, 0.3, 45), (17, 0.55, 39), (47, 0.8, 41), (33, 0.15, 48)):
        u = (t * 2 + ph) % 1.0
        y = int(round(y0 + u * 22))
        if u < 0.85 and y < 64:
            p.px(x0 + (1 if u > 0.5 else 0), y, mix('#d8b0a0', '#4a2a28', u))
    p.backdrop = (30, 8, 16)
    return p


ALL = {'nokar': nokar, 'olradin': olradin, 'rhodun': rhodun, 'rohdhna': rohdhna, 'tnelis': tnelis, 'vaden': vaden,
       'vaylerian': vaylerian, 'veldyr': veldyr, 'zelian': zelian}


def sheet(path, keys=None, scale=4):
    from PIL import Image, ImageDraw
    keys = keys or list(ALL)
    per_row = 3
    cw, ch = 64 * scale * 2 + 30, 64 * scale + 40
    rows = (len(keys) + per_row - 1) // per_row
    im = Image.new('RGB', (per_row * cw + 10, rows * ch + 10), (24, 24, 28))
    d = ImageDraw.Draw(im)
    for i, k in enumerate(keys):
        x0, y0 = 10 + (i % per_row) * cw, 10 + (i // per_row) * ch
        for j, t in enumerate((0.0, 0.5)):
            pp = ALL[k](t)
            bg = tuple(pp.backdrop) + (255,)
            im.paste(pp.image(scale, bg=bg).convert('RGB'), (x0 + j * (64 * scale + 8), y0))
        d.text((x0, y0 + 64 * scale + 6), k, fill=(240, 240, 240))
    im.save(path)


if __name__ == '__main__':
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    sheet(os.path.join(here, 'sheet_faces_b7.png'))
    for k, f in ALL.items():
        save_gif(f, os.path.join(here, 'anim_%s.gif' % k), scale=4)
