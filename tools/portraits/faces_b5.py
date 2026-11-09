import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
"""Discordant Stars codec portraits, batch 5: edyn, florzen, freesystems, ghemina, ghoti, gledge, ilyxum,
kjalengard, kollecc. Each `key(t)` returns a 64x64 Portrait; t = 0..1 over a seamless 24-frame loop."""
import math

import numpy as np

from pix import Portrait, ramp, breath, sway, pulse, blink, mix, wave, hexrgb, save_gif, FRAMES


def mx(pts, c=64):
    """mirror a polygon about the vertical centre line (x -> 64 - x)"""
    return [(c - x, y) for x, y in pts]


def behind(p, mask, col, amount=1.0):
    """paint light only where nothing is drawn yet (a halo or a beam behind the figure, no outline)"""
    m = mask & ~p.a
    c = np.array(hexrgb(col) if isinstance(col, str) else col, float)
    bg = np.array(getattr(p, 'backdrop', (12, 14, 30)), float)
    p.rgb[m] = (bg * (1 - amount) + c * amount).astype(np.uint8)
    p.a |= m


def mote(p, x, y, col, over=False):
    """one loose particle pixel (after the outline); behind the figure unless over=True"""
    if 0 <= x < p.w and 0 <= y < p.h and (over or not p.a[y, x]):
        p.rgb[y, x] = hexrgb(col) if isinstance(col, str) else col
        p.a[y, x] = True


def spike(cx, cy, ang, length, w):
    """a thin triangle from (cx, cy) pointing at angle ang (degrees, 0 = right, -90 = up)"""
    a = math.radians(ang)
    tx, ty = cx + math.cos(a) * length, cy + math.sin(a) * length
    nx, ny = -math.sin(a) * w, math.cos(a) * w
    return [(cx - nx, cy - ny), (tx, ty), (cx + nx, cy + ny)]


# ---------------------------------------------------------------------------------------------------------------
def edyn(t=0.0):
    """The Edyn Mandate: a serene silver-and-gold knight, crystal crown, a gold light in the visor and the chest.
    Moves: armour breathes, the helm follows, the core and visor light swell slowly, a halo light circles,
    golden motes rise."""
    p = Portrait()
    p.backdrop = (16, 16, 44)
    silver = ramp('#c4c9d8', spread=0.6, hue_shift=0.05)
    steel = ramp('#565a78', spread=0.7)
    gold = ramp('#d8aa44', spread=0.7, hue_shift=0.04)
    bronze = ramp('#7a5a2a', spread=0.6)
    dark = ramp('#1c1a2e', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.25)
    with p.at(0, body):
        # under-armour and the gold chest plate with its core
        p.fill(p.poly([(4, 66), (8, 46), (22, 38), (42, 38), (56, 46), (60, 66)]), steel, radius=6, texture=0.05)
        p.fill(p.poly([(17, 66), (19, 51), (32, 46), (45, 51), (47, 66)]), bronze, radius=5)
        # rays of filigree from the core, lit in turn
        for j, (ex, ey) in enumerate(((21, 53), (25, 50), (39, 50), (43, 53), (20, 59), (44, 59))):
            on = pulse(t, j / 6.0, 1)
            p.stroke([(32 + (ex - 32) * 0.35, 60 + (ey - 60) * 0.35), (ex, ey)], mix(gold[2], gold[4], on))
        p.fill(p.ellipse(32, 60, 3.5, 4.5), gold, radius=2, bias=0.3 + 0.2 * k)
        p.glow(32, 60, '#fffbe0', mix('#ffb840', '#ffe890', k), r=2, strength=0.6 + 0.4 * k)
        # high collar plates rising behind the helm: dark steel, gold-edged
        for s in (1, -1):
            col = [(15, 44), (18, 30), (22, 27), (27, 38), (28, 47)]
            edge_ = [(18, 31), (22, 28), (26, 38), (27, 46)]
            if s < 0:
                col, edge_ = mx(col), mx(edge_)
            p.fill(p.poly(col), steel, radius=3, bias=0.05 if s > 0 else -0.1)
            p.stroke(edge_, gold[3] if s > 0 else gold[2])
        # pauldrons: two layered silver shells, gold-edged, with crescents
        for s, bias in ((1, 0.0), (-1, -0.14)):
            lo = [(0, 66), (0, 52), (8, 50), (18, 53), (22, 60), (14, 66)]
            hi = [(-1, 56), (0, 45), (6, 40), (16, 38), (25, 42), (24, 48), (14, 52), (5, 56)]
            e1 = [(0, 46), (6, 41), (16, 39), (24, 43)]
            e2 = [(1, 53), (8, 51), (17, 54), (21, 60)]
            if s < 0:
                lo, hi, e1, e2 = mx(lo), mx(hi), mx(e1), mx(e2)
            p.fill(p.poly(lo), silver, radius=4, bias=bias - 0.08, texture=0.04)
            p.stroke(e2, gold[2])
            p.fill(p.poly(hi), silver, radius=5, bias=bias, texture=0.04)
            p.stroke(e1, gold[3] if s > 0 else gold[2])
            cx = 12 if s > 0 else 52
            p.fill(p.poly([(cx - 5 * s, 47), (cx, 43), (cx + 5 * s, 45), (cx + 1 * s, 46)]), gold, radius=1)
        # gorget and the medallion
        p.fill(p.poly([(26, 38), (38, 38), (40, 47), (32, 51), (24, 47)]), steel, radius=3)
        p.fill(p.ellipse(32, 46, 3.6, 3.6), silver, radius=2)
        p.fill(p.ellipse(32, 46, 1.8, 1.8), dark, flat=1, edge=False)
        p.px(32, 46, mix('#ffd060', '#fff6d0', k))
    with p.at(0, head):
        # a crown of crystal spikes rising from a gold circlet, and a short gold spire
        crystal = ramp('#d4def2', spread=0.55, hue_shift=0.05)
        for ang, ln in ((-165, 5), (-145, 8), (-122, 10), (-58, 10), (-35, 8), (-15, 5)):
            a = math.radians(ang)
            p.fill(p.poly(spike(32 + math.cos(a) * 9, 16 + math.sin(a) * 8, ang, ln, 1.8)), crystal, radius=1.5,
                   bias=0.1 if ang < -90 else -0.08)
        p.fill(p.poly([(31, 0), (33, 0), (34, 9), (30, 9)]), gold, radius=1)
        p.fill(p.ellipse(32, 3, 1.6, 1.6), gold, radius=1, bias=0.2)
        # the helm: a smooth dome narrowing to the chin
        helm = p.ellipse(32, 20, 11.5, 12) | p.poly([(21, 21), (43, 21), (41, 33), (36, 40), (28, 40), (23, 33)])
        p.fill(helm, silver, radius=7, texture=0.04)
        # the gold circlet
        p.fill(p.poly([(21, 15), (26, 12), (32, 11), (38, 12), (43, 15), (43, 17), (38, 15), (32, 14), (26, 15),
                       (21, 18)]), gold, radius=1.5, bias=0.1)
        p.glow(32, 12, '#ffffff', '#a0d8ff', r=0, strength=0.6 + 0.4 * k)
        # the face mask: a shield, a shade darker, cheek lines falling to the chin
        p.fill(p.poly([(24, 22), (40, 22), (39, 31), (36, 37), (32, 39), (28, 37), (25, 31)]), silver, radius=4,
               bias=-0.12)
        p.stroke([(24, 26), (27, 34), (31, 39)], silver[1])
        p.stroke([(40, 26), (37, 34), (33, 39)], silver[1])
        for y in (31, 33, 35):
            p.pixels([(29, y), (35, y)], steel[0])
        # brow and the calm, lowered eye slits
        p.fill(p.poly([(22, 19), (32, 18), (42, 19), (42, 21), (32, 20), (22, 21)]), silver, radius=2, bias=0.2)
        sl = mix('#ffc860', '#fff4c8', k)
        p.stroke([(25, 23), (30, 24)], dark[0])
        p.stroke([(34, 24), (39, 23)], dark[0])
        p.stroke([(26, 24), (29, 24)], sl)
        p.stroke([(35, 24), (38, 24)], sl)
        p.pixels([(26, 23), (38, 23)], dark[1])
        # the seam of light, brow to chin
        p.stroke([(32, 19), (32, 39)], steel[0])
        span = 2 + int(round(3 * k))
        for y in range(24 - span, 25 + span + 4):
            p.px(32, y, mix(gold[2], '#fff2c0', k * (1 - abs(y - 25) / (span + 6))))
        p.glow(32, 24, '#ffffff', mix('#ffc050', '#fff0a0', k), r=1, strength=0.45 + 0.45 * k)
        p.stroke([(24, 12), (28, 10)], silver[4])
    p.rim('#b48cff', side=1, amount=0.45)
    p.rim('#ffd27a', side=-1, amount=0.25)
    p.outline('#0a0816')
    # the halo: a thin ring of light behind the crown, a bright point travelling round it
    with p.at(0, head):
        ring = p.ellipse(32, 19, 22, 22) & ~p.ellipse(32, 19, 21, 21)
        behind(p, ring, '#c89a40', 0.55)
        for j in range(3):
            a = 2 * math.pi * (t + j * 0.03) - math.pi / 2
            x, y = int(round(32 + math.cos(a) * 21.4)), int(round(19 + math.sin(a) * 21.4))
            m = np.zeros_like(p.a)
            if 0 <= x < 64 and 0 <= y + p.dy < 64:
                m[y + p.dy, x] = True
            behind(p, m, ('#fff6d0', '#ffd27a', '#c89a40')[j], 1.0)
    # golden motes drifting up, slow
    for x0, ph in ((6, 0.0), (58, 0.45), (13, 0.7), (51, 0.2)):
        u = (t + ph) % 1.0
        y = int(round(40 - u * 36))
        x = x0 + sway(t, ph, 1)
        if u < 0.85:
            mote(p, x, y, mix('#fff0b0', '#806030', u))
    return p


def florzen(t=0.0):
    """The Florzen Profiteers: a glowing jellyfish pirate - a tricorn crest on the bell, a scarred blind eye and a
    burning one, a gold ring through the margin, one tentacle hardened into a cutlass.
    Moves: the bell pulses, veins of light flow, tentacles and frills sway, the ring swings, bubbles rise."""
    p = Portrait()
    p.backdrop = (6, 18, 32)
    bell = ramp('#1f9c9c', spread=0.7, hue_shift=0.05)
    inner = ramp('#6af0d8', spread=0.45)
    frill = ramp('#4a3aa8', spread=0.68, hue_shift=0.05)
    hat = ramp('#1f4e62', spread=0.6)
    gold = ramp('#e0a830', spread=0.62)
    blade = ramp('#b8dce6', spread=0.55)
    cc = -breath(t)  # 0..1: the bell contracts
    k = pulse(t, 0.0, 2)
    # the oral arms: three ruffled ribbons under the bell, each swaying on its own beat
    for j, (x0, wd, ph) in enumerate(((21, 5, 0.0), (43, 5, 0.4), (32, 6, 0.2))):
        edge_l, edge_r = [], []
        for y in range(30, 69, 2):
            u = (y - 30) / 38
            c = x0 + wave(t, ph + y * 0.035) * (0.5 + 1.5 * u) + (x0 - 32) * 0.25 * u
            w = wd * (0.8 + 0.3 * u)
            edge_l.append((c - w - (1 if (y // 2) % 2 else 0), y))
            edge_r.append((c + w + (1 if (y // 2 + 1) % 2 else 0), y))
        p.fill(p.poly(edge_l + edge_r[::-1]), frill, radius=3.5, texture=0.08, seed=3 + j, bias=0.08 * (j == 2))
        p.stroke([(x + 2, y) for x, y in edge_l[2::2]], frill[4])
        p.stroke([((a[0] + b_[0]) / 2 + 1, a[1]) for a, b_ in zip(edge_l[3:], edge_r[3:])][::2], frill[1])
        for i in (5, 11, 16):
            on = pulse(t, j * 0.3 + i * 0.05, 2)
            x, y = edge_l[i]
            p.px(int(x) + 3, int(y), mix(frill[3], '#b8f8ff', on))
    # the cutlass: a tentacle coiled round a hardened, curved blade
    with p.at(0, sway(t, 0.35, 1)):
        p.fill(p.poly([(44, 60), (49, 51), (54, 43), (58, 36), (62, 30), (61, 37), (58, 45), (53, 53), (47, 62)]),
               blade, radius=2, bias=0.1)
        p.stroke([(48, 58), (53, 50), (57, 43), (60, 36)], blade[4])
        p.fill(p.poly([(41, 58), (46, 55), (52, 58), (51, 60), (46, 58), (42, 61)]), gold, radius=1.5)
        p.stroke([(43, 33), (47, 40), (46, 48), (44, 55), (45, 62), (42, 66)], bell[1], width=3)
        p.stroke([(43, 33), (47, 40), (46, 48), (44, 55), (45, 62), (42, 66)], inner[2], width=1)
    # the bell, contracting a pixel on the beat
    with p.at(0, -cc):
        dome = p.ellipse(32, 27 + cc, 28 - cc, 25 + cc) & (p.yy < 31 + p.dy + cc)
        skirt = np.zeros_like(dome)
        for x in range(8 + cc, 58 - cc, 6):
            skirt |= p.ellipse(x + 1, 31 + cc, 2.7, 3.0)
        p.fill(dome | skirt, bell, radius=15, texture=0.05, seed=5)
        p.stroke([(9, 21 + cc), (12, 14 + cc), (17, 9 + cc)], inner[4])
        p.stroke([(10, 24 + cc), (12, 19 + cc)], inner[3])
        # radial canals of light, lit in a wave, and the glowing margin
        for j, (bx, by) in enumerate(((11, 29), (18, 31), (25, 31), (39, 31), (46, 31), (53, 29))):
            on = pulse(t, j / 6.0, 2)
            p.stroke([(32 + (bx - 32) * 0.3, 9 + cc), (32 + (bx - 32) * 0.75, 20 + cc), (bx, by + cc)],
                     mix(bell[3], '#d8fff4', on * 0.8))
        for x in range(8 + cc, 58 - cc, 6):
            p.stroke([(x, 32 + cc), (x + 2, 33 + cc)], mix(inner[1], inner[3], k))
        # angry brows, a scarred milky eye and a burning one, a fanged grin with a gold tooth
        p.fill(p.poly([(16, 15), (29, 20), (28, 22), (17, 18)]), hat, radius=1)
        p.fill(p.poly([(48, 15), (35, 20), (36, 22), (47, 18)]), hat, radius=1)
        p.fill(p.ellipse(23, 22.5 + cc, 3.0, 2.0), ramp('#a8ccc8', spread=0.4), radius=1, bias=0.1)
        p.pixels([(22, 22 + cc), (23, 22 + cc)], '#e8f4f0')
        p.fill(p.ellipse(41, 22.5 + cc, 3.2, 2.1), hat, flat=0, edge=False)
        p.stroke([(37, 21 + cc), (44, 20 + cc)], hat[0])
        p.glow(41, 23 + cc, '#fff4c0', mix('#ff6a10', '#ffb030', pulse(t, 0.3, 3)), r=1)
        p.stroke([(16, 13 + cc), (30, 29 + cc)], '#ff5a8a')
        p.pixels([(18, 14 + cc), (20, 17 + cc), (25, 23 + cc), (27, 25 + cc), (29, 27 + cc)], '#ffc8d8')
        p.fill(p.poly([(25, 26), (31, 27), (36, 26), (40, 25), (39, 28), (33, 29), (26, 28)]), hat, flat=0, edge=False)
        p.pixels([(27, 27 + cc), (29, 28 + cc), (34, 28 + cc), (38, 26 + cc)], '#e8fff8')
        p.px(31, 28 + cc, gold[4])
    # the tricorn crest at the crown of the bell
    with p.at(0, -cc):
        p.fill(p.poly([(19, 8), (20, 0), (26, 4), (32, 2), (38, 4), (44, 0), (45, 8), (39, 10), (32, 11), (25, 10)]),
               hat, radius=3)
        p.stroke([(20, 2), (26, 5), (32, 3), (38, 5), (44, 2)], gold[3])
        p.stroke([(20, 8), (25, 9), (32, 10), (39, 9), (44, 8)], gold[1])
        p.pixels([(31, 5), (32, 5), (33, 5), (31, 6), (33, 6), (32, 7)], mix('#e8fff8', '#a8f0e0', k))
    with p.at(0, -cc):
        # the gold ring through the margin, swinging
        rx = 51 + sway(t, 0.1, 1)
        p.fill(p.ellipse(rx, 35 + cc, 2.6, 3.2) & ~p.ellipse(rx, 35 + cc, 1.2, 1.8), gold, radius=1, bias=0.1,
               edge=False)
        p.px(rx - 2, 34 + cc, gold[4])
    p.rim('#ff4fd8', side=1, amount=0.45)
    p.rim('#7affea', side=-1, amount=0.25)
    p.outline('#04080e')
    # trailing tentacles: thin glowing threads behind the body
    for x0, ph, col in ((7, 0.0, 0), (12, 0.3, 1), (17, 0.6, 2), (47, 0.15, 2), (52, 0.55, 1), (57, 0.8, 0)):
        for y in range(34 - cc, 64):
            u = (y - 34) / 30
            x = x0 + int(round(wave(t, ph - y * 0.03) * (0.4 + 1.2 * u)))
            if (y + col) % 7 != 0:
                mote(p, x, y, mix(('#5ae8d8', '#3ac0c0', '#88fff0')[col], '#123c50', u * 0.8))
    # bubbles rising
    for x0, ph in ((5, 0.0), (60, 0.33), (12, 0.66), (54, 0.5)):
        u = (t + ph) % 1.0
        y = int(round(64 - u * 60))
        x = x0 + sway(t, ph, 1, 2)
        if u < 0.9:
            mote(p, x, y, mix('#c8fff4', '#3a8a90', u))
            if u > 0.3:
                mote(p, x + 1, y + 1, mix('#6ad0c8', '#1a5060', u))
    return p


def lock(x, y, ang, ln, w, curl=0.0):
    """a tapered, slightly curled lock of fur / hair / flame from base (x, y) towards angle ang (degrees, 90 = down)"""
    a = math.radians(ang)
    ux, uy = math.cos(a), math.sin(a)
    nx, ny = -uy, ux
    pts = []
    for f, wf in ((0.0, 1.0), (0.45, 0.8), (0.8, 0.4)):
        c = curl * f * f * ln
        pts.append((x + ux * ln * f + nx * (w * wf + c), y + uy * ln * f + ny * (w * wf + c)))
    tip = (x + ux * ln + nx * curl * ln, y + uy * ln + ny * curl * ln)
    back = []
    for f, wf in ((0.8, 0.4), (0.45, 0.8), (0.0, 1.0)):
        c = curl * f * f * ln
        back.append((x + ux * ln * f + nx * (-w * wf + c), y + uy * ln * f + ny * (-w * wf + c)))
    return pts + [tip] + back


def taper(pts, w0, w1=0.4):
    """a polygon around a centre line, its half-width tapering from w0 to w1 (horns, tails, tentacles)"""
    n = len(pts)
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        a = pts[max(0, i - 1)]
        b = pts[min(n - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        d = math.hypot(dx, dy) or 1
        nx, ny = -dy / d, dx / d
        w = w0 + (w1 - w0) * i / (n - 1)
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    return left + right[::-1]


def jag(x0, x1, y_base, y_tip, n, lean=0, seed=0):
    """a jagged fur / spine edge: alternating points between y_base and y_tip from x0 to x1"""
    pts = []
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i / n
        pts.append((x + (lean if i % 2 else 0), y_tip if i % 2 else y_base))
    return pts


def freesystems(t=0.0):
    """The Free Systems Compact: a blue-grey furred primate envoy turned to the right, amber eyes and a calm, open
    look, a shaggy fur collar over a ringed spacer's neck seal, the Compact's violet stripes on the shoulder.
    Moves: chest breathes, the head follows, the fur stirs, the ear-comm light pulses, a slow blink."""
    p = Portrait()
    p.backdrop = (30, 12, 42)
    fur = ramp('#5f7296', spread=0.68, hue_shift=0.05)
    ruff = ramp('#8a6a50', spread=0.66, hue_shift=0.05)
    skin = ramp('#9c8a7e', spread=0.6, hue_shift=0.05)
    jacket = ramp('#4c5a38', spread=0.65)
    steel = ramp('#8e9aaa', spread=0.6)
    strap = ramp('#5a3a28', spread=0.55)
    violet = ramp('#b04ae0', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    with p.at(0, body):
        # the spacer's jacket and the ringed neck seal
        p.fill(p.poly([(-1, 66), (-1, 46), (8, 40), (28, 44), (50, 44), (60, 47), (65, 54), (65, 66)]), jacket,
               radius=7, texture=0.06)
        p.stroke([(46, 52), (48, 66)], jacket[1])
        p.pixels([(50, 55), (50, 59), (51, 63)], jacket[3])
        for j, y in enumerate((51, 47, 43)):
            p.fill(p.ellipse(38 + j, y, 10 - j, 3.2), violet if j == 1 else steel, radius=2.5, bias=0.05 * j)
            p.stroke([(32 + j, y - 1), (36 + j, y - 2)], violet[4] if j == 1 else steel[4])
    with p.at(0, head):
        # the far ear, the skull with its shaggy crown
        p.fill(p.ellipse(30, 22, 15, 15) | p.poly(jag(16, 42, 12, 6, 12, -2)), fur, radius=8, texture=0.16, seed=2)
        for j, (x, y) in enumerate(((19, 12), (23, 9), (27, 7), (32, 6), (37, 7))):
            dx = sway(t, j * 0.15, 1)
            p.stroke([(x + 2, y + 4), (x + dx - 1, y - 1)], fur[3] if j % 2 else fur[4])
        for x, y in ((18, 22), (20, 27), (22, 16), (25, 30)):
            p.stroke([(x, y), (x - 2, y + 3)], fur[1])
        # the near ear with its comm cuff
        p.fill(p.ellipse(16.5, 22, 3.8, 4.8), skin, radius=2.5, bias=-0.05)
        p.fill(p.ellipse(17, 22.5, 1.8, 2.8), skin, flat=1, edge=False)
        p.stroke([(13, 19), (15, 16), (19, 16), (21, 18)], steel[3], width=2)
        p.glow(14, 18, '#ffffff', mix('#8a3ae0', '#ff8af0', k), r=0, strength=0.5 + 0.5 * k)
        # the bare face: forehead, deep-set eyes under a heavy brow, a long muzzle
        face = p.poly([(31, 18), (38, 16), (46, 17), (50, 20), (52, 25), (55, 31), (53, 37), (47, 41), (38, 41),
                       (32, 37), (29, 30), (29, 22)])
        p.fill(face, skin, radius=6, texture=0.08, seed=4, bias=-0.06)
        p.fill(p.ellipse(46, 32.5, 8.5, 6.5), skin, radius=5, bias=0.1)
        p.fill(p.poly([(30, 19), (38, 17), (46, 18), (51, 20), (51, 22), (46, 21), (38, 21), (31, 23)]), skin,
               radius=1.5, bias=0.05, edge=False)
        p.fill(p.ellipse(35, 23.5, 4, 2.6), skin, flat=1, edge=False)
        for x, y in ((33, 16), (37, 15), (41, 16), (45, 17)):
            p.stroke([(x - 1, y - 2), (x + 1, y + 1)], fur[2])
        p.stroke([(31, 22), (37, 21)], skin[0])
        p.stroke([(44, 21), (50, 22)], skin[0])
        if blink(t, 0.55, 2):
            p.stroke([(33, 23), (37, 23)], skin[1])
            p.stroke([(45, 23), (48, 23)], skin[1])
        else:
            p.fill(p.ellipse(35, 23.5, 2.8, 1.8), skin, flat=0, edge=False)
            p.glow(35, 23, '#fff0b0', '#ff9a20', r=0, strength=0.85)
            p.px(36, 23, '#2a1408')
            p.fill(p.ellipse(47, 23.5, 1.8, 1.5), skin, flat=0, edge=False)
            p.glow(47, 23, '#ffe0a0', '#d87a18', r=0, strength=0.65)
        p.stroke([(33, 26), (38, 26)], skin[1])
        p.stroke([(31, 29), (34, 31)], skin[1])
        # nose ridge and nostrils, a calm, faintly smiling mouth
        p.stroke([(48, 25), (51, 29)], skin[3])
        p.pixels([(50, 30), (53, 30)], skin[0])
        p.stroke([(40, 37), (46, 37), (51, 36)], skin[0])
        p.px(39, 36, skin[0])
        p.stroke([(43, 39), (48, 39)], skin[3])
        for x, y in ((43, 33), (45, 34), (47, 33)):
            p.px(x, y, skin[1])
        # grey sideburns framing the face
        p.fill(p.poly([(22, 26), (29, 28), (33, 36), (38, 42), (30, 43), (23, 36)]), fur, radius=3, texture=0.14)
    with p.at(0, body):
        # the fur collar: a shaggy roll rising to the neck, tufted edge, long locks flowing out and down
        left = p.poly([(-1, 66), (-1, 46), (4, 43), (10, 40), (16, 37), (22, 36), (27, 38), (30, 43), (32, 50),
                       (31, 58), (28, 66)])
        right = p.poly([(45, 66), (45, 52), (47, 47), (52, 44), (58, 43), (65, 45), (65, 66)])
        for x, y, ang, ln in ((1, 45, -125, 4), (6, 42, -118, 4), (12, 39, -110, 4), (18, 37, -100, 4),
                              (24, 37, -80, 3), (29, 41, -40, 3), (31, 48, -5, 3), (32, 55, 5, 3), (47, 48, -140, 3),
                              (51, 45, -110, 4), (56, 44, -90, 4), (61, 44, -70, 4)):
            ang += 6 * wave(t, x * 0.03)
            m = p.poly(spike(x, y, ang, ln, 1.8))
            if x < 40:
                left |= m
            else:
                right |= m
        p.fill(left, ruff, radius=7, texture=0.07, seed=7)
        p.fill(right, ruff, radius=6, texture=0.07, seed=8, bias=-0.12)
        for pts in (((21, 39), (18, 45), (16, 51)), ((26, 40), (24, 47), (23, 54)), ((14, 41), (10, 46), (8, 53)),
                    ((8, 44), (4, 49), (2, 55)), ((29, 46), (28, 53), (27, 60)), ((18, 49), (15, 55), (14, 61)),
                    ((11, 53), (8, 59), (7, 65)), ((24, 56), (22, 61), (21, 66)), ((3, 58), (1, 63))):
            p.stroke(pts, ruff[1])
            p.stroke([(x - 1, y - 1) for x, y in pts[:2]], ruff[4] if pts[0][1] < 50 else ruff[3])
        for pts in (((52, 46), (54, 52), (55, 58)), ((58, 45), (61, 51), (62, 57)), ((48, 50), (49, 56), (49, 62)),
                    ((55, 56), (57, 62), (57, 66))):
            p.stroke(pts, ruff[0])
            p.stroke([(x - 1, y) for x, y in pts[:2]], ruff[2])
        # the Compact badge on the breast, violet stripes, its light pulsing
        p.fill(p.poly([(37, 58), (45, 58), (45, 64), (41, 66), (37, 64)]), violet, radius=2)
        p.stroke([(38, 60), (44, 60)], '#3a1458')
        p.stroke([(38, 62), (44, 62)], mix('#c070ff', '#ffb0f8', k))
    p.rim('#c86aff', side=1, amount=0.45)
    p.rim('#ffb070', side=-1, amount=0.2)
    p.outline('#0c0612')
    return p


def ghemina(t=0.0):
    """The Ghemina Raiders: the Lord in front, the Lady just behind him - purple hoods, horned, beaked bronze masks.
    Moves: they breathe on different beats; his eyes (blue) and hers (magenta) glow in turn, as one speaks and the
    other finishes; the third eyes flicker; motes drift."""
    p = Portrait()
    p.backdrop = (14, 8, 32)
    hood = ramp('#7c2cb0', spread=0.66, hue_shift=0.05)
    hood2 = ramp('#6a2696', spread=0.66, hue_shift=0.05)
    mask = ramp('#7a6448', spread=0.66)
    lmask = ramp('#8a8a9a', spread=0.6)
    dark = ramp('#1a0e24', spread=0.5)
    bone = ramp('#d6c6a2', spread=0.6)
    leather = ramp('#a8642a', spread=0.6)
    steel = ramp('#8a8ca0', spread=0.62)
    lord_b, lord_h = breath(t), breath(t, -0.1)
    lady_b, lady_h = breath(t, 0.5), breath(t, 0.4)
    kl = pulse(t, 0.0)    # the Lord speaks ...
    ky = pulse(t, 0.5)    # ... the Lady finishes
    # ---- the Lady, behind and to the right
    with p.at(0, lady_b):
        p.fill(p.poly([(36, 66), (38, 44), (46, 38), (56, 38), (65, 44), (65, 66)]), hood2, radius=6, texture=0.08)
        p.fill(p.ellipse(57, 48, 8, 7), steel, radius=4, bias=-0.05)
        p.stroke([(51, 46), (56, 42), (62, 43)], steel[4])
        p.fill(p.ellipse(57, 48, 2, 2), steel, radius=1, bias=0.2)
        p.stroke([(48, 56), (64, 54)], steel[1])
    with p.at(0, lady_h):
        p.fill(p.poly(taper([(56, 14), (60, 10), (62, 5), (61, 1)], 2.6, 0.5)), bone, radius=1.5, bias=-0.1)
        p.stroke([(59, 9), (61, 11)], bone[1])
        p.fill(p.poly([(37, 42), (37, 26), (41, 14), (47, 8), (54, 8), (59, 14), (62, 26), (62, 42), (50, 46)]), hood2,
               radius=6, texture=0.06, bias=-0.06)
        p.stroke([(42, 13), (49, 9), (56, 11)], steel[3])
        p.stroke([(41, 15), (48, 11), (57, 13)], steel[1])
        p.fill(p.poly([(42, 36), (41, 25), (44, 18), (50, 15), (56, 18), (58, 27), (56, 36), (50, 40)]), dark, flat=0,
               edge=False)
        p.fill(p.ellipse(50, 28, 6.2, 9), lmask, radius=3, bias=-0.1)
        p.fill(p.poly([(48, 33), (52, 33), (50, 38)]), lmask, radius=1, bias=-0.1)
        p.stroke([(50, 20), (50, 37)], lmask[1])
        p.stroke([(49, 21), (49, 33)], lmask[4])
        p.stroke([(44, 22), (47, 20), (50, 19), (53, 20), (56, 22)], mix('#c09a50', '#ffd890', 0.5))
        ey = mix('#4a1048', '#ff6ae0', ky)
        p.stroke([(45, 27), (48, 28)], dark[0], width=2)
        p.stroke([(55, 27), (52, 28)], dark[0], width=2)
        p.stroke([(45, 27), (48, 28)], ey)
        p.stroke([(55, 27), (52, 28)], ey)
        if ky > 0.4:
            p.glow(47, 28, '#ffffff', '#ff5ad8', r=1, strength=ky * 0.8)
            p.glow(53, 28, '#ffffff', '#ff5ad8', r=1, strength=ky * 0.8)
        p.px(50, 23, mix('#4a2050', '#ffb0f0', ky if not blink(t, 0.85, 1) else 0))
    # ---- the Lord, in front
    with p.at(0, lord_b):
        p.fill(p.poly([(-1, 66), (-1, 46), (6, 40), (16, 44), (26, 48), (36, 44), (44, 40), (50, 46), (52, 66)]), hood,
               radius=7, texture=0.08)
        for pts in (((6, 46), (8, 56), (7, 66)), ((14, 48), (17, 58), (16, 66)), ((42, 46), (40, 56), (42, 66)),
                    ((47, 48), (46, 58), (48, 66))):
            p.stroke(pts, hood[1])
        for pts in (((3, 48), (4, 56)), ((11, 50), (13, 58)), ((44, 50), (43, 56))):
            p.stroke(pts, hood[3])
        p.stroke([(4, 44), (10, 42)], hood[4])
        # the leather baldric and the round brooch
        p.fill(p.poly([(8, 50), (13, 48), (34, 66), (27, 66)]), leather, radius=2)
        p.stroke([(11, 50), (31, 66)], leather[3])
        for x, y in ((15, 54), (20, 58), (25, 62)):
            p.px(x, y, leather[4])
        p.fill(p.ellipse(36, 54, 4.5, 4.5), steel, radius=2.5)
        p.fill(p.ellipse(36, 54, 2, 2), mask, radius=1, bias=0.2)
    with p.at(0, lord_h):
        # horns curling out and up from the hood
        p.fill(p.poly(taper([(15, 17), (9, 14), (4, 10), (2, 5), (4, 1)], 3.4, 0.6)), bone, radius=2.5, bias=0.1)
        p.fill(p.poly(taper([(35, 17), (41, 14), (46, 10), (48, 5), (46, 1)], 3.4, 0.6)), bone, radius=2.5, bias=-0.05)
        for pts in (((10, 12), (9, 16)), ((6, 9), (4, 12)), ((40, 12), (41, 16)), ((44, 9), (46, 12))):
            p.stroke(pts, bone[1])
        # the hood, peaked, its folds
        p.fill(p.poly([(6, 50), (5, 32), (9, 18), (17, 10), (25, 7), (33, 10), (41, 18), (45, 32), (44, 50), (25, 52)]),
               hood, radius=8, texture=0.06)
        p.stroke([(9, 22), (14, 14), (22, 9)], hood[4])
        p.stroke([(7, 26), (8, 20)], hood[4])
        p.stroke([(8, 34), (10, 44), (14, 50)], hood[1])
        p.stroke([(42, 34), (40, 44), (36, 50)], hood[1])
        p.stroke([(27, 9), (34, 12), (39, 18)], hood[3])
        p.stroke([(6, 38), (7, 46)], hood[1])
        # the thick edge of the hood round its opening
        p.fill(p.poly([(11, 44), (10, 28), (15, 17), (25, 12), (35, 17), (40, 28), (39, 44), (25, 49)]), hood, radius=3,
               bias=0.12)
        p.stroke([(11, 30), (15, 19), (24, 14)], hood[4])
        # the dark of the hood and the beaked mask within
        p.fill(p.poly([(13, 42), (12, 28), (16, 19), (25, 14), (34, 19), (38, 28), (37, 42), (25, 47)]), dark, flat=0,
               edge=False)
        p.fill(p.poly([(15, 25), (25, 19), (35, 25), (35, 33), (30, 38), (25, 46), (20, 38), (15, 33)]), mask,
               radius=3, texture=0.05)
        p.fill(p.poly([(24, 20), (26, 20), (27, 38), (25, 45), (23, 38)]), mask, radius=1.5, bias=0.25)
        p.stroke([(17, 34), (21, 38), (24, 44)], mask[1])
        p.stroke([(33, 34), (29, 38), (26, 44)], mask[1])
        p.stroke([(18, 24), (22, 22)], mask[4])
        # angled eye slits, glowing as he speaks; the third eye
        ec = mix('#183060', '#7ad0ff', kl)
        p.stroke([(17, 28), (22, 30)], dark[0], width=2)
        p.stroke([(33, 28), (28, 30)], dark[0], width=2)
        p.stroke([(18, 29), (21, 30)], ec)
        p.stroke([(32, 29), (29, 30)], ec)
        if kl > 0.4:
            p.glow(20, 29, '#ffffff', '#5ab8ff', r=1, strength=kl * 0.8)
            p.glow(30, 29, '#ffffff', '#5ab8ff', r=1, strength=kl * 0.8)
        p.fill(p.ellipse(25, 24.5, 1.6, 1.6), dark, flat=0, edge=False)
        p.px(25, 24, mix('#203060', '#c0e8ff', kl if not blink(t, 0.35, 1) else 0))
    p.rim('#4a8aff', side=1, amount=0.45)
    p.rim('#ff5ad8', side=-1, amount=0.22)
    p.outline('#08040e')
    for x0, ph, col in ((3, 0.0, '#9a6aff'), (60, 0.5, '#ff7ae0'), (47, 0.25, '#7ab0ff')):
        u = (t + ph) % 1.0
        if u < 0.8:
            mote(p, x0 + sway(t, ph, 1, 2), int(round(62 - u * 40)), mix(col, '#2a1040', u))
    return p


def ghoti(t=0.0):
    """The Ghoti Wayfarers: an abyssal fish-thing, all jaw - needle teeth round a gaping maw, a red light burning in
    its throat, small amber eyes under spiny brows, a webbed crest. Moves: the jaw works open and shut a pixel, the
    throat-light throbs, the crest and gill-fins sway, lateral lights pulse in sequence, bubbles rise."""
    p = Portrait()
    p.backdrop = (4, 12, 28)
    skin = ramp('#1d6676', spread=0.74, hue_shift=0.05)
    belly = ramp('#24566a', spread=0.62)
    fin = ramp('#2a8a96', spread=0.6)
    web = ramp('#14485a', spread=0.5)
    gum = ramp('#8a2a5a', spread=0.6)
    throat = ramp('#3a0a18', spread=0.6)
    tooth = ramp('#cfe8ec', spread=0.45)
    head = breath(t, -0.05)
    jaw = 1 if wave(t, 0.1) > 0.2 else 0     # the jaw gapes a pixel wider for part of the loop
    k = pulse(t, 0.0, 2)
    with p.at(0, head):
        # the webbed dorsal crest and the gill fins, swaying
        tips = []
        for j, (x, y, ang, ln) in enumerate(((13, 13, -135, 9), (19, 9, -118, 10), (26, 7, -102, 10), (33, 6, -88, 10),
                                              (40, 7, -74, 9), (47, 10, -58, 8))):
            a2 = math.radians(ang + 7 * wave(t, j * 0.12))
            tips.append((x, y + 3, x + math.cos(a2) * ln, y + 3 + math.sin(a2) * ln))
        p.fill(p.poly([(tx * 0.8 + bx * 0.2, ty * 0.8 + by * 0.2) for bx, by, tx, ty in tips] +
                      [(bx, by + 2) for bx, by, tx, ty in tips[::-1]]), web, radius=2, bias=-0.1)
        for bx, by, tx, ty in tips:
            p.fill(p.poly(taper([(bx, by), (tx, ty)], 1.6, 0.3)), fin, radius=1, bias=0.15, edge=False)
            p.px(int(round(tx)), int(round(ty)), fin[4])
        for j, (x, y, ang, ln) in enumerate(((5, 26, -170, 8), (4, 32, 175, 9), (5, 38, 160, 8))):
            a2 = ang + 8 * wave(t, 0.3 + j * 0.15)
            p.fill(p.poly(spike(x, y, a2, ln, 2.4)), fin, radius=1.5)
        for j, (x, y, ang, ln) in enumerate(((59, 24, -10, 7), (60, 30, 5, 8), (59, 36, 20, 7))):
            a2 = ang + 8 * wave(t, 0.6 + j * 0.15)
            p.fill(p.poly(spike(x, y, a2, ln, 2.2)), fin, radius=1.5, bias=-0.1)
        # the lower jaw, working
        with p.at(0, jaw):
            p.fill(p.poly([(-1, 66), (-1, 44), (6, 40), (18, 48), (34, 53), (50, 48), (60, 40), (65, 44), (65, 66)]),
                   belly, radius=7, texture=0.08)
            for x in (8, 18, 46, 56):
                p.stroke([(x, 56), (x + 2, 66)], belly[1])
            p.stroke([(24, 57), (44, 57)], belly[3])
        # the great head, brow knobs over the eyes
        p.fill(p.poly([(3, 42), (3, 28), (7, 17), (16, 11), (32, 9), (48, 11), (57, 17), (61, 26), (61, 40), (54, 38),
                       (34, 32), (14, 38)]), skin, radius=10, texture=0.08, seed=4)
        p.fill(p.ellipse(15, 17, 6, 4), skin, radius=3, bias=0.1, edge=False)
        p.fill(p.ellipse(50, 16, 6, 4), skin, radius=3, bias=-0.05, edge=False)
        rng = np.random.default_rng(9)
        for x, y in rng.integers((8, 12), (58, 30), size=(22, 2)):
            if p.a[y + p.dy, x]:
                p.px(int(x), int(y), skin[3] if y < 20 else skin[2])
        # the gaping maw: throat, the light within, gums, teeth
        cy, ry = 41 + jaw / 2, 11.5 + jaw / 2
        mouth = p.ellipse(33, cy, 23, ry)
        p.fill(mouth, throat, radius=9, bias=-0.15)
        p.fill(p.ellipse(33, cy + 1, 14, ry - 4), throat, flat=0, edge=False)
        halo = p.ellipse(33, cy + 2, 8 + k * 1.5, 6 + k)
        hc = np.array(mix('#a01008', '#ff2a10', k), float)
        p.rgb[halo] = (p.rgb[halo] * 0.45 + hc * 0.55).astype(np.uint8)
        p.fill(p.ellipse(33, cy + 2, 3.2, 3.2), [mix('#ff3a1a', '#ff7a3a', k)], flat=0, edge=False)
        p.glow(33, int(cy + 2), '#ffffff', mix('#ff6a30', '#ffc070', k), r=1, strength=0.8 + 0.2 * k)
        lip = mouth & ~p.ellipse(33, cy, 21.5, ry - 1.5)
        p.fill(lip, gum, flat=2, edge=False)
        p.fill(lip & (p.yy < cy + p.dy - 4), gum, flat=3, edge=False)
        def fang(x, y, ln, up):
            # a needle tooth from the gum towards the throat
            dx, dy = 33 - x, (cy + 2) - y
            d = math.hypot(dx, dy) or 1
            ux, uy = dx / d * 0.55, dy / d
            n = math.hypot(ux, uy)
            tx, ty = x + ux / n * ln, y + uy / n * ln
            p.fill(p.poly(taper([(x, y), (tx, ty)], 1.1, 0.2)), tooth, radius=1, bias=0.1 if not up else -0.05,
                   edge=False)
            return tx, ty
        for j in range(14):
            a = math.pi + (j + 0.5) / 14 * math.pi
            x, y = 33 + math.cos(a) * 21.5, cy + math.sin(a) * (ry - 1)
            f = math.sin((j + 0.5) / 14 * math.pi)
            tx, ty = fang(x, y, (4, 8, 5, 9, 3)[j % 5] * (0.35 + 0.65 * f), False)
            if abs(x - 33) < 9:
                p.px(int(round(tx)), int(round(ty)), '#ff9a7a')
        for j in range(12):
            a = (j + 0.5) / 12 * math.pi
            x, y = 33 + math.cos(a) * 21, cy + math.sin(a) * (ry - 1) + jaw * 0.5
            f = math.sin(a)
            ln = (6, 3.5, 7, 4.5)[j % 4] * (0.35 + 0.65 * f) * (0.5 if abs(x - 33) < 5 else 1)
            fang(x, y, ln, True)
        # small amber eyes under spiny brows
        for x, y, s in ((15, 20, 1), (50, 19, -1)):
            p.fill(p.ellipse(x + 0.5, y + 0.5, 2.6, 2.0), skin, flat=0, edge=False)
            if blink(t, 0.7, 1):
                p.stroke([(x - 1, y), (x + 1, y)], skin[2])
            else:
                p.glow(x, y, '#fff0b0', '#ff8a20', r=0, strength=0.9)
            p.stroke([(x - 4 * s, y - 4), (x + 3 * s, y - 2)], skin[0])
            p.stroke([(x - 4 * s, y - 5), (x + 2 * s, y - 3)], skin[4])
        # lateral lights, pulsing in a wave
        for j, (x, y) in enumerate(((8, 31), (9, 27), (11, 24), (57, 28), (56, 25), (54, 22))):
            on = pulse(t, j * 0.16 + (0.5 if x > 30 else 0), 2)
            p.px(x, y, mix('#1a6a80', '#9afcff', on))
    p.rim('#3affea', side=1, amount=0.42)
    p.rim('#ff4a3a', side=-1, amount=0.2)
    p.outline('#02060c')
    for x0, ph in ((4, 0.0), (60, 0.35), (24, 0.7), (45, 0.55)):
        u = (t + ph) % 1.0
        y = int(round(64 - u * 62))
        if u < 0.92:
            mote(p, x0 + sway(t, ph, 1, 2), y, mix('#b8fff4', '#2a6a80', u))
    return p


def gledge(t=0.0):
    """The Gledge Union: a tired miner in a patinated diving-style helmet - brass rims, a big ear disc, heavy-lidded
    goggle lenses, a respirator with ribbed hoses coiled round the neck, a headlamp, the orange Union shoulder pad.
    Moves: breathing, the helmet a beat later, a puff of vapour from the valve on each exhale, the headlamp
    flickers, a slow, tired blink, dust drifts."""
    p = Portrait()
    p.backdrop = (32, 16, 12)
    dome = ramp('#4a8a96', spread=0.7, hue_shift=0.05)
    brass = ramp('#c8883a', spread=0.66, hue_shift=0.04)
    gun = ramp('#3a3e4a', spread=0.62)
    suit = ramp('#27324c', spread=0.62)
    pad = ramp('#ec8a2a', spread=0.62)
    hose = ramp('#5a5e66', spread=0.6)
    copper = ramp('#e08a48', spread=0.62)
    body, head = breath(t), breath(t, -0.12)
    lamp_on = not blink(t, 0.3, 1) and not blink(t, 0.36, 1)
    with p.at(0, body):
        p.fill(p.poly([(-1, 66), (-1, 48), (10, 42), (26, 44), (42, 46), (56, 48), (65, 54), (65, 66)]), suit, radius=7,
               texture=0.06)
        for pts in (((30, 50), (32, 66)), ((44, 52), (46, 66)), ((54, 54), (58, 66))):
            p.stroke(pts, suit[1])
        # chest canisters
        for x in (34, 39):
            p.fill(p.poly([(x, 54), (x + 4, 54), (x + 4, 66), (x, 66)]), gun, radius=1.5)
            p.stroke([(x + 1, 56), (x + 3, 56)], brass[2])
        # the orange Union shoulder pad
        p.fill(p.poly([(-1, 62), (-1, 46), (6, 41), (17, 41), (22, 47), (19, 58), (9, 63)]), pad, radius=5,
               texture=0.08)
        p.stroke([(1, 50), (8, 46), (18, 46)], pad[4])
        p.fill(p.poly([(3, 52), (16, 50), (16, 55), (3, 57)]), gun, radius=1.5)
        for x in (5, 9, 13):
            p.stroke([(x, 53), (x + 1, 55)], pad[2])
        p.pixels([(2, 47), (19, 48), (17, 57)], brass[3])
        # coiled hoses round the neck
        for j, (y, rx) in enumerate(((45, 15), (42, 14))):
            ring = p.ellipse(32, y, rx, 4.2) & ~p.ellipse(32, y - 0.5, rx - 2.4, 2.2)
            p.fill(ring, copper, radius=2, bias=0.3 + 0.05 * j)
            p.stroke([(22, y - 3), (30, y - 4)], copper[4])
        # the ribbed hose from the respirator down across the chest
        hp = [(44, 40), (47, 46), (48, 52), (46, 58), (44, 66)]
        p.fill(p.poly(taper(hp, 2.6, 2.6)), hose, radius=2)
        for i in range(len(hp) - 1):
            for f in (0.25, 0.75):
                x = hp[i][0] + (hp[i + 1][0] - hp[i][0]) * f
                y = hp[i][1] + (hp[i + 1][1] - hp[i][1]) * f
                p.stroke([(x - 2, y), (x + 2, y)], hose[1])
        p.fill(p.poly([(43, 59), (49, 59), (49, 61), (43, 61)]), brass, radius=1)
    with p.at(0, head):
        # the dome, the brass brow band and rivets
        p.fill(p.ellipse(30, 22, 18, 17.5), dome, radius=10, texture=0.1, seed=11)
        p.stroke([(14, 15), (22, 9), (32, 7), (42, 9), (47, 13)], brass[3])
        p.stroke([(13, 17), (22, 11), (32, 9), (42, 11), (48, 15)], brass[1])
        for x, y in ((18, 13), (25, 10), (32, 9), (39, 10), (45, 13)):
            p.px(x, y, brass[4])
        for x, y in ((20, 6), (16, 10), (24, 18), (18, 30), (22, 34)):
            p.px(x, y, dome[4] if y < 15 else dome[1])
        # the headlamp
        p.fill(p.poly([(37, 4), (43, 3), (45, 8), (39, 9)]), gun, radius=1.5)
        p.fill(p.ellipse(44, 5.5, 1.6, 2.4), [mix('#7a5a30', '#fff4d0', 1.0 if lamp_on else 0.2)], flat=0, edge=False)
        if lamp_on:
            p.glow(44, 5, '#ffffff', '#ffe0a0', r=1, strength=0.6)
        # the face plate
        plate = p.ellipse(40, 27, 14, 13.5) & (p.xx >= 26 + p.dx) & (p.yy >= 16 + p.dy)
        p.fill(plate | p.poly([(28, 17), (48, 15), (52, 19)]), gun, radius=5, texture=0.06)
        p.stroke([(27, 17), (38, 15), (49, 16)], brass[2])
        for x, y in ((28, 20), (28, 30), (31, 37), (52, 21), (53, 30)):
            p.px(x, y, brass[3])
        # the ear disc: brass rings
        for r, b in ((7, 0.0), (5, 0.15), (3, 0.0)):
            p.fill(p.ellipse(15, 25, r - 1, r), brass if r != 5 else gun, radius=2.5, bias=b)
        p.px(14, 24, brass[4])
        # goggle lenses: brass rims, orange glass, heavy shutters half-down (blink: shut)
        shut = blink(t, 0.6, 3)
        for cx, cy, r in ((35, 25, 4.6), (48, 24, 3.4)):
            p.fill(p.ellipse(cx, cy, r + 1.4, r + 1.4), brass, radius=2)
            p.fill(p.ellipse(cx, cy, r, r), ramp('#ff8a20', spread=0.5), radius=3, bias=0.1, edge=False)
            p.px(int(cx) - 1, int(cy), '#fff0c0')
            lid = p.ellipse(cx, cy, r, r) & (p.yy < cy + p.dy + (r if shut else -0.2))
            p.fill(lid, gun, radius=2, bias=0.1, edge=False)
            p.stroke([(cx - r, cy - (0 if not shut else -r + 1)), (cx + r - 1, cy - (0 if not shut else -r + 1))],
                     gun[0])
        if not shut:
            p.glow(35, 26, '#fff4c0', '#ff8a20', r=0, strength=0.7)
        # the respirator snout and its valve
        p.fill(p.ellipse(44, 36, 6, 5), gun, radius=3, bias=0.05)
        p.fill(p.ellipse(46, 37, 3.4, 3.4), brass, radius=2)
        p.fill(p.ellipse(46.5, 37.5, 1.6, 1.6), gun, flat=0, edge=False)
        p.stroke([(38, 34), (40, 39)], gun[3])
    p.rim('#ff8a3a', side=1, amount=0.42)
    p.rim('#5ae0ff', side=-1, amount=0.18)
    p.outline('#0e0806')
    # the headlamp's beam, faint, behind the figure
    if lamp_on:
        with p.at(0, head):
            behind(p, p.poly([(46, 5), (64, -1), (64, 9)]), '#ffe8b0', 0.16 + 0.05 * pulse(t, 0, 3))
            behind(p, p.poly([(46, 5), (64, -8), (64, 16)]), '#ffd890', 0.08)
    # a puff of vapour from the valve as the breath goes out
    u = (t - 0.55) % 1.0
    if u < 0.4:
        for j, (ox, oy) in enumerate(((0, 0), (1, -1), (2, 0))):
            x = 51 + int(u * 22) + ox
            y = 38 + breath(t, -0.12) - int(u * 10) + oy
            mote(p, x, y, mix('#e8e4dc', '#5a4038', u / 0.4), over=True)
    # dust drifting down
    for x0, ph in ((5, 0.0), (58, 0.4), (25, 0.75), (12, 0.3)):
        u = (t + ph) % 1.0
        mote(p, x0 + sway(t, ph, 1), int(round(u * 63)), mix('#c89a6a', '#4a2a1a', abs(u - 0.5) * 2))
    return p


def ilyxum(t=0.0):
    """The Augurs of Ilyxum: a bald blue-grey elfin seer, eyes closed in perfect certainty, a third eye on the brow,
    a high-collared violet coat, a crackling orb of foresight cradled below. Moves: slow breath, the head a beat
    later, the orb throbs and its lightning shifts, the third eye opens and looks, motes circle the orb."""
    p = Portrait()
    p.backdrop = (26, 10, 42)
    skin = ramp('#7c8cac', spread=0.62, hue_shift=0.05)
    coat = ramp('#3e2c66', spread=0.66, hue_shift=0.04)
    trim = ramp('#e0a040', spread=0.6)
    wrap = ramp('#8a2a4a', spread=0.6)
    orbr = ramp('#f0c060', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    see = pulse(t, 0.5)            # the third eye opens around mid-loop
    with p.at(0, body):
        p.fill(p.poly([(-1, 66), (-1, 50), (10, 42), (22, 44), (42, 44), (54, 42), (65, 50), (65, 66)]), coat, radius=7,
               texture=0.06)
        # the high collar, rising behind the head: gold-edged, lined in magenta, stitched with gold
        lining = ramp('#7a2a7a', spread=0.6)
        for s_ in (1, -1):
            pts = [(3, 66), (5, 42), (10, 26), (17, 19), (22, 32), (25, 46), (22, 66)]
            lin = [(13, 30), (17, 21), (21, 33), (24, 46), (21, 50), (16, 40)]
            ed = [(6, 42), (11, 27), (17, 21)]
            if s_ < 0:
                pts, ed, lin = mx(pts), mx(ed), mx(lin)
            p.fill(p.poly(pts), coat, radius=4, bias=0.05 if s_ > 0 else -0.12, texture=0.05)
            p.fill(p.poly(lin), lining, radius=2, bias=-0.1 if s_ > 0 else -0.2)
            p.stroke(ed, trim[3] if s_ > 0 else trim[2])
        # neck and the wine-red wraps
        p.fill(p.poly([(27, 32), (37, 32), (38, 46), (26, 46)]), skin, radius=3, bias=-0.1)
        p.stroke([(29, 38), (30, 44)], skin[1])
        p.stroke([(35, 38), (34, 44)], skin[1])
        p.fill(p.poly([(23, 44), (32, 41), (41, 44), (42, 50), (32, 52), (22, 50)]), wrap, radius=3, texture=0.06)
        p.stroke([(25, 47), (32, 45), (39, 47)], wrap[1])
        p.stroke([(26, 45), (31, 43)], wrap[4])
        p.stroke([(22, 50), (22, 66)], trim[2])
        p.stroke([(42, 50), (42, 66)], trim[1])
    with p.at(0, head):
        # elfin ears, swept out and up
        for s_ in (1, -1):
            pts = [(23, 21), (7, 13), (9, 17), (14, 23), (23, 28)]
            p.fill(p.poly(pts if s_ > 0 else mx(pts)), skin, radius=2, bias=0.05 if s_ > 0 else -0.12)
            ip = [(21, 23), (12, 17), (16, 23)]
            p.stroke(ip if s_ > 0 else mx(ip), skin[1])
        # the long, bald head
        p.fill(p.ellipse(32, 18, 12, 14.5) | p.poly([(20, 20), (44, 20), (41, 31), (36, 37), (28, 37), (23, 31)]), skin,
               radius=8, texture=0.06, seed=6)
        p.stroke([(25, 8), (29, 5)], skin[4])
        p.stroke([(23, 12), (24, 9)], skin[3])
        # brow creases, eyes closed in certainty, long nose, a knowing half-smile
        p.stroke([(24, 21), (29, 21)], skin[1])
        p.stroke([(35, 21), (40, 21)], skin[1])
        p.stroke([(25, 24), (27, 25), (29, 24)], skin[0])
        p.stroke([(35, 24), (37, 25), (39, 24)], skin[0])
        p.stroke([(25, 26), (28, 26)], skin[3])
        p.stroke([(36, 26), (39, 26)], skin[2])
        p.stroke([(32, 22), (32, 29)], skin[3])
        p.stroke([(33, 23), (33, 29)], skin[1])
        p.pixels([(31, 30), (34, 30)], skin[0])
        p.stroke([(29, 33), (33, 33), (35, 32)], skin[0])
        p.px(36, 31, skin[0])
        p.stroke([(30, 35), (34, 35)], skin[3])
        p.stroke([(24, 28), (26, 32)], skin[1])
        p.stroke([(40, 28), (38, 32)], skin[1])
        p.stroke([(27, 17), (37, 17)], skin[2])
        # the third eye: a slit that opens and looks
        p.fill(p.ellipse(32, 13, 1.6, 2.6), skin, flat=0, edge=False)
        if see > 0.55:
            p.glow(32, 13, '#ffffff', mix('#c070ff', '#ffd0ff', see), r=1, strength=see)
        else:
            p.stroke([(32, 12), (32, 14)], mix('#6a3a9a', '#c070ff', see * 1.5))
    with p.at(0, body):
        # the orb, held low in long fingers; lightning shifts inside
        p.fill(p.ellipse(32, 58, 9, 9), orbr, radius=6, bias=0.2 + 0.15 * k)
        f = int(t * 6) % 3
        bolts = (((26, 55), (29, 58), (28, 61), (32, 64)), ((35, 52), (33, 56), (37, 58), (36, 63)),
                 ((27, 53), (31, 54), (34, 52)), ((28, 60), (32, 59), (36, 61), (39, 58)))
        for i, bpts in enumerate(bolts):
            if (i + f) % 3 != 0:
                p.stroke(bpts, mix('#ffffff', '#ffe8a0', (i + f) % 2 * 0.5))
        p.glow(32, 57, '#ffffff', mix('#ffe080', '#fff8e0', k), r=2, strength=0.5 + 0.4 * k)
        lit = ramp('#a89cb0', spread=0.55, hue_shift=0.08)    # skin warmed by the orb's light
        for s_ in (1, -1):
            palm = [(13, 66), (16, 61), (21, 60), (24, 63), (23, 66)]
            p.fill(p.poly(palm if s_ > 0 else mx(palm)), lit, radius=3, bias=0.1 if s_ > 0 else -0.05)
            for fp in (((18, 61), (20, 56), (23, 52), (26, 50)), ((21, 61), (23, 58), (25, 56))):
                fp2 = list(fp) if s_ > 0 else mx(fp)
                p.fill(p.poly(taper(fp2, 1.6, 1.0)), lit, radius=2, bias=0.15 if s_ > 0 else 0.0)
                p.px(int(round(fp2[-1][0])) - (1 if s_ < 0 else 0), int(fp2[-1][1]), '#ffe8b0')
        for pts in (((8, 46), (10, 58)), ((14, 32), (13, 44)), ((56, 46), (54, 58)), ((50, 32), (51, 44))):
            p.stroke(pts, coat[1])
    p.rim('#c070ff', side=1, amount=0.45)
    p.rim('#ffc060', side=-1, amount=0.25)
    p.outline('#0c0616')
    # motes circling the orb
    for j in range(4):
        a = 2 * math.pi * (t + j / 4)
        x = int(round(32 + math.cos(a) * 13))
        y = int(round(57 + math.sin(a) * 4)) + body
        mote(p, x, y, mix('#fff0b0', '#c070ff', 0.5 + 0.5 * math.sin(a)), over=math.sin(a) > 0)
    return p


def kjalengard(t=0.0):
    """The Berserkers of Kjalengard: a green, toad-headed lizard warrior turned to the right - a horned iron helm, a
    sneer with a tusk, braided chin-spines with iron rings, a frost-white fur mantle, a leather pauldron burned with
    the glowing knot of the Kjalengard. Moves: breathing, the head a beat later, the braids swing, the runes pulse,
    a narrow-eyed blink, snow falls."""
    p = Portrait()
    p.backdrop = (10, 16, 36)
    skin = ramp('#5c9a3a', spread=0.66, hue_shift=0.06)
    spot = ramp('#3a6a34', spread=0.5)
    throat = ramp('#9aa860', spread=0.6, hue_shift=0.05)
    iron = ramp('#6c7286', spread=0.66)
    bone = ramp('#dccaa0', spread=0.6)
    fur = ramp('#b4bccc', spread=0.6, hue_shift=0.05)
    leather = ramp('#a2582c', spread=0.62)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    with p.at(0, body):
        # leather jerkin and the great pauldron with the glowing knot
        p.fill(p.poly([(-1, 66), (-1, 50), (12, 44), (34, 46), (50, 44), (65, 50), (65, 66)]), leather, radius=6,
               texture=0.08)
        p.stroke([(20, 52), (24, 66)], leather[1])
        p.stroke([(30, 54), (31, 66)], leather[1])
        p.fill(p.poly([(38, 52), (44, 44), (56, 41), (65, 43), (65, 66), (44, 66)]), leather, radius=5, texture=0.1,
               seed=4, bias=0.05)
        p.stroke([(44, 46), (56, 43), (64, 45)], leather[4])
        for x, y in ((43, 49), (47, 61), (62, 47), (63, 62)):
            p.px(x, y, iron[4])
        # the valknut: three interlocked triangles, lit in turn
        for j, (cx, cy, col) in enumerate(((51, 58, '#ff3a4a'), (57, 58, '#4a8aff'), (54, 53, '#b05aff'))):
            on = pulse(t, j / 3.0)
            c = mix('#4a2018', col, 0.4 + 0.6 * on)
            p.stroke([(cx - 3, cy + 2), (cx, cy - 3), (cx + 3, cy + 2), (cx - 3, cy + 2)], c)
        p.glow(54, 56, '#ffffff', mix('#b05aff', '#ff8af0', k), r=0, strength=0.3 + 0.4 * k)
        # the frost-white fur mantle
        mant = p.poly([(-1, 66), (-1, 44), (6, 40), (14, 38), (22, 40), (30, 44), (38, 46), (42, 50), (38, 54), (30, 54),
                       (22, 56), (14, 62), (8, 66)])
        for x, y, ang, ln in ((1, 43, -125, 4), (6, 40, -115, 4), (12, 38, -100, 4), (18, 38, -90, 4),
                              (24, 40, -70, 4), (30, 43, -55, 4), (36, 46, -40, 3), (41, 51, 10, 3), (36, 54, 60, 3),
                              (28, 55, 80, 3), (20, 58, 100, 3)):
            mant |= p.poly(spike(x, y, ang + 6 * wave(t, x * 0.04), ln, 1.8))
        p.fill(mant, fur, radius=6, texture=0.07, seed=5)
        for pts in (((10, 41), (8, 47), (6, 54)), ((17, 41), (15, 47), (14, 53)), ((24, 43), (23, 49)),
                    ((31, 46), (32, 51)), ((4, 46), (2, 52)), ((12, 52), (10, 58), (8, 64)), ((20, 50), (18, 55)),
                    ((3, 58), (1, 63))):
            p.stroke(pts, fur[1])
            p.stroke([(x - 1, y - 1) for x, y in pts[:2]], fur[4])
    with p.at(0, head):
        # horns of the helm, behind the head
        p.fill(p.poly(taper([(15, 13), (9, 8), (6, 2), (8, -3)], 3.0, 0.6)), bone, radius=2, bias=0.1)
        # spines down the back of the skull
        for j, (x, y, ang) in enumerate(((13, 24, -160), (12, 30, 180), (13, 36, 165))):
            p.fill(p.poly(spike(x + 1, y, ang, 5, 1.8)), skin, radius=1, bias=0.1)
        # the toad-like head: broad skull, long blunt snout, a heavy pale jaw
        p.fill(p.poly([(14, 40), (11, 30), (13, 20), (20, 14), (30, 13), (38, 15), (46, 16), (53, 20), (58, 25),
                       (60, 30), (58, 35), (52, 39), (42, 45), (28, 47)]), skin, radius=9, texture=0.1, seed=3)
        p.fill(p.poly([(22, 41), (32, 39), (44, 38), (56, 35), (52, 40), (42, 45), (30, 47), (20, 45)]), throat,
               radius=3, texture=0.06)
        for x in (27, 33, 39):
            p.stroke([(x, 42), (x + 2, 45)], throat[1])
        # mottled spots and warts
        for x, y, r in ((22, 26, 3.2), (32, 20, 2.4), (28, 33, 2.6), (40, 28, 2.2), (17, 33, 2.0)):
            p.fill(p.ellipse(x, y, r + 0.6, r), spot, radius=2, bias=0.15, edge=False, texture=0.1)
        for x, y, r in ((19, 22, 1.6), (23, 33, 1.3), (16, 28, 1.2), (51, 23, 1.0), (29, 25, 1.2), (36, 29, 1.0),
                        (18, 36, 1.2), (46, 27, 1.0), (54, 28, 0.9)):
            p.fill(p.ellipse(x, y, r + 0.4, r), spot, radius=1, bias=0.1, edge=False)
            p.px(x - 1, y - 1, skin[4])
        p.stroke([(20, 37), (26, 39), (31, 38)], skin[1])
        # the long, down-turned mouth, a sneer lifting over the tusk
        p.stroke([(28, 39), (30, 38), (37, 38), (45, 37), (52, 35), (59, 32)], spot[0])
        p.stroke([(27, 40), (28, 39)], spot[0])
        p.stroke([(36, 37), (44, 36), (51, 34), (56, 32)], skin[3])
        p.fill(p.poly(taper([(47, 39), (48, 35), (47, 32)], 1.8, 0.5)), bone, radius=1.2, bias=0.1)
        # nostril
        p.pixels([(57, 26), (58, 27)], skin[0])
        p.stroke([(54, 24), (57, 24)], skin[3])
        # the eye on its bulge, under a hard, slanted lid: narrowed, burning
        p.fill(p.ellipse(42, 19, 5.6, 4.6), skin, radius=3, bias=0.12, texture=0.06)
        p.fill(p.ellipse(42.5, 19.5, 3.2, 2.2), spot, flat=0, edge=False)
        if blink(t, 0.4, 2):
            p.stroke([(40, 19), (45, 19)], skin[1])
        else:
            p.glow(43, 19, '#fff6c0', mix('#ff3a10', '#ff9a20', pulse(t, 0.2, 3)), r=1)
            p.px(44, 19, '#ffd060')
        p.fill(p.poly([(36, 16), (42, 14), (48, 17), (48, 19), (43, 18), (37, 18)]), skin, radius=1.5, bias=-0.05)
        p.stroke([(38, 19), (43, 18), (47, 20)], spot[0])
        p.stroke([(38, 15), (43, 14)], skin[4])
        # the iron helm with its rune band and the near horn
        helm = p.ellipse(24, 15, 12, 8) & (p.yy < 17 + p.dy)
        p.fill(helm | p.poly([(12, 16), (36, 15), (36, 19), (12, 20)]), iron, radius=4, texture=0.05)
        p.fill(p.poly([(12, 16), (36, 15), (36, 19), (12, 20)]), iron, radius=1.5, bias=0.1)
        for j, x in enumerate((15, 20, 25, 30)):
            on = pulse(t, j * 0.18, 2)
            c = mix('#1a4a60', '#7af4ff', on)
            p.pixels([(x, 17), (x + 1, 18), (x, 18)] if j % 2 else [(x, 18), (x + 1, 17), (x + 1, 18)], c)
        p.stroke([(15, 9), (23, 7), (30, 8)], iron[4])
        p.stroke([(24, 7), (24, 15)], iron[1])
        p.fill(p.poly(taper([(33, 11), (39, 7), (43, 1), (42, -4)], 3.0, 0.6)), bone, radius=2, bias=-0.05)
        p.stroke([(39, 7), (41, 9)], bone[1])
        p.stroke([(42, 2), (44, 4)], bone[1])
        # braided chin-spines, iron-ringed
        gold = ramp('#e0a83a', spread=0.6)
        for j, (x0, y0, ln) in enumerate(((42, 44, 10), (49, 41, 9))):
            sw = sway(t, 0.25 * j + 0.1, 1)
            pts = [(x0 + sw * y / ln, y0 + y) for y in range(0, ln + 1)]
            p.fill(p.poly(taper(pts, 1.6, 1.2)), spot, radius=1, bias=-0.1)
            for i in range(1, ln):
                x, y = pts[i]
                xi = int(round(x))
                p.px(xi - 1 + (i % 2), int(y), skin[3])
            x, y = pts[ln // 2]
            p.stroke([(int(round(x)) - 1, int(y)), (int(round(x)) + 1, int(y))], gold[3])
            x, y = pts[-1]
            p.fill(p.ellipse(x + 0.5, y + 1.5, 1.9, 1.3), gold, radius=1, bias=0.2)
    p.rim('#ff4a4a', side=1, amount=0.42)
    p.rim('#6ad8ff', side=-1, amount=0.22)
    p.outline('#060a14')
    # snow falling, slow and slanting
    for x0, ph in ((4, 0.0), (20, 0.55), (36, 0.3), (52, 0.8), (60, 0.15), (12, 0.4)):
        u = (t + ph) % 1.0
        x = (x0 - int(u * 6) + sway(t, ph, 1)) % 64
        mote(p, x, int(u * 64) % 64, '#e8f0ff' if (x0 // 4) % 2 else '#9aa8c8')
    return p


def kollecc(t=0.0):
    """The Kollecc Society: a blue-skinned collector of relics turned to the right - ridged antennae curling back,
    finned ears, deep-set amber eyes under a creased brow, a sly half-smile; a high-collared dark coat, a blue
    scarf, copper chains and a relic amulet. Moves: breathing, the head a beat later, the antennae sway and their tips
    glow, the relic's gem pulses, a blink."""
    p = Portrait()
    p.backdrop = (36, 18, 12)
    skin = ramp('#2c62b0', spread=0.64, hue_shift=0.06)
    fin_m = ramp('#245a96', spread=0.6)
    coat = ramp('#363c4a', spread=0.66)
    scarf = ramp('#33307a', spread=0.62, hue_shift=0.04)
    copper = ramp('#cc7c40', spread=0.62)
    body, head = breath(t), breath(t, -0.1)
    k = pulse(t, 0.0, 2)
    with p.at(0, body):
        # the dark coat, its high collar
        p.fill(p.poly([(-1, 66), (-1, 50), (8, 44), (24, 44), (44, 44), (56, 46), (65, 52), (65, 66)]), coat, radius=7,
               texture=0.07)
        p.fill(p.poly([(2, 66), (4, 48), (10, 38), (17, 32), (22, 42), (24, 52), (20, 66)]), coat, radius=4, bias=0.08,
               texture=0.06)
        p.stroke([(5, 48), (10, 39), (16, 34)], copper[3])
        p.stroke([(18, 36), (22, 44), (23, 52)], copper[1])
        p.fill(p.poly([(46, 66), (46, 50), (52, 42), (58, 40), (60, 48), (56, 66)]), coat, radius=3, bias=-0.12)
        p.stroke([(52, 43), (57, 41)], copper[2])
        for pts in (((10, 52), (12, 66)), ((42, 52), (44, 66))):
            p.stroke(pts, coat[1])
        # the blue scarf wound round the neck
        p.fill(p.poly([(21, 40), (30, 37), (44, 38), (50, 42), (48, 50), (38, 53), (26, 51), (21, 46)]), scarf,
               radius=4, texture=0.06)
        for pts in (((24, 42), (32, 41), (44, 42)), ((24, 46), (34, 46), (46, 46)), ((27, 49), (38, 50))):
            p.stroke(pts, scarf[1])
        p.stroke([(25, 41), (31, 39)], scarf[4])
        p.stroke([(25, 45), (30, 44)], scarf[3])
        p.stroke([(22, 44), (28, 43), (36, 44), (48, 44)], copper[1])
        # copper chains and the relic amulet
        p.stroke([(24, 52), (28, 57), (34, 59), (40, 57), (44, 52)], copper[2])
        p.stroke([(22, 54), (26, 61), (34, 64), (42, 61), (46, 54)], copper[1])
        p.fill(p.poly([(31, 57), (37, 57), (38, 61), (34, 65), (30, 61)]), copper, radius=2)
        p.glow(34, 60, '#e8ffff', mix('#2ab8c8', '#8afcff', k), r=1, strength=0.6 + 0.4 * k)
    with p.at(0, head):
        # antennae: ridged, curling back, tips glowing
        for j, base in enumerate(([(25, 9), (22, 4), (17, 1), (12, 2)], [(35, 8), (38, 3), (43, 0), (48, 1)])):
            sw = sway(t, 0.3 * j, 1)
            tip = (base[-1][0] + (-2 if j == 0 else 2), base[-1][1] + 3 + sw)
            pts = base[:-1] + [(base[-1][0], base[-1][1] + sw * 0.5), tip]
            p.fill(p.poly(taper(pts, 2.8, 0.8)), skin, radius=1.5, bias=0.1 if j == 0 else -0.05)
            for i in range(len(pts) - 1):
                for f in (0.3, 0.75):
                    x = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * f
                    y = pts[i][1] + (pts[i + 1][1] - pts[i][1]) * f
                    p.px(int(round(x)), int(round(y)), skin[0] if f < 0.5 else skin[3])
            p.glow(int(tip[0]), int(tip[1]), '#fff0c0', mix('#c86a20', '#ffb050', pulse(t, 0.5 * j, 2)), r=0,
                   strength=0.8)
        # the finned ear: spined rays and a membrane
        p.fill(p.poly([(21, 17), (12, 12), (8, 17), (8, 24), (11, 29), (21, 28)]), fin_m, radius=2, bias=-0.05)
        for ang in (-150, -168, 172, 152):
            p.fill(p.poly(spike(20, 22, ang, 13, 1.0)), skin, flat=2, edge=False)
        # the long head and the narrow jaw
        p.fill(p.ellipse(29, 17, 12.5, 13.5) | p.poly([(19, 22), (40, 13), (46, 18), (48, 25), (47, 33), (43, 39),
                                                       (37, 42), (30, 39), (23, 32)]), skin, radius=9, texture=0.03,
               seed=12)
        # the face's front plane, turned from the light
        p.fill(p.poly([(37, 14), (42, 15), (46, 18), (48, 25), (47, 33), (43, 39), (37, 42), (33, 37), (31, 29),
                       (33, 21)]), skin, radius=4, bias=-0.12, edge=False)
        p.stroke([(33, 21), (31, 29), (33, 36), (36, 41)], skin[1])
        # a ridge down the forehead
        p.stroke([(32, 6), (34, 11), (37, 16)], skin[3])
        p.stroke([(33, 6), (35, 11), (38, 16)], skin[1])
        p.stroke([(21, 12), (25, 7)], skin[4])
        # heavy brow over deep sockets; amber eyes
        p.fill(p.ellipse(35, 23.5, 4.4, 3.0), skin, flat=0, edge=False)
        p.fill(p.ellipse(45, 23.5, 2.6, 2.4), skin, flat=0, edge=False)
        p.fill(p.poly([(28, 19), (36, 17), (40, 18), (44, 17), (48, 19), (47, 21), (43, 20), (40, 21), (36, 20),
                       (30, 22)]), skin, flat=3, edge=False)
        p.stroke([(29, 18), (36, 16)], skin[4])
        if blink(t, 0.45, 2):
            p.stroke([(32, 24), (37, 24)], skin[2])
            p.stroke([(44, 24), (46, 24)], skin[2])
        else:
            p.glow(35, 24, '#fff0c0', '#ffa030', r=1, strength=0.7)
            p.px(34, 24, '#ffd070')
            p.px(36, 24, '#1a0c04')
            p.glow(45, 24, '#ffe0a0', '#e08a20', r=0, strength=0.75)
        # the nose bridge, a sly half-smile, the cheekbone
        p.stroke([(41, 21), (45, 28)], skin[3])
        p.stroke([(42, 21), (46, 28)], skin[2])
        p.pixels([(45, 30), (46, 30)], skin[0])
        p.stroke([(36, 35), (40, 35), (43, 34), (45, 32)], skin[0])
        p.stroke([(37, 37), (41, 37)], skin[3])
        p.stroke([(38, 40), (41, 40)], skin[3])
        p.stroke([(29, 28), (34, 29)], skin[3])
    p.rim('#ff9a4a', side=1, amount=0.42)
    p.rim('#5ae8ff', side=-1, amount=0.22)
    p.outline('#0c0604')
    return p


ALL = {k: globals()[k] for k in ('edyn', 'florzen', 'freesystems', 'ghemina', 'ghoti', 'gledge', 'ilyxum',
                                        'kjalengard', 'kollecc') if k in globals()}


if __name__ == '__main__':
    from PIL import Image, ImageDraw
    D = __import__('os').path.dirname(__import__('os').path.abspath(__file__))
    keys = list(ALL)
    cols = 3
    cw, ch = 2 * 256 + 12 + 24, 256 + 34
    rows = (len(keys) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cw + 12, rows * ch + 12), (255, 255, 255))
    dr = ImageDraw.Draw(sheet)
    for i, k in enumerate(keys):
        x0, y0 = 12 + (i % cols) * cw, 12 + (i // cols) * ch
        for j, t in enumerate((0.0, 0.5)):
            p = ALL[k](t)
            sheet.paste(p.image(4, bg=tuple(p.backdrop) + (255,)).convert('RGB'), (x0 + j * 268, y0))
        dr.text((x0, y0 + 260), k, fill=(0, 0, 0))
        if len(sys.argv) < 2 or sys.argv[1] != 'nogif':
            save_gif(ALL[k], f'{D}/anim_{k}.gif', scale=4)
    sheet.save(f'{D}/sheet_faces_b5.png')
