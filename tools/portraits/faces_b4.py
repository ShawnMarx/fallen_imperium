import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import math  # noqa: E402

import numpy as np  # noqa: E402
from scipy import ndimage  # noqa: E402

from pix import (FRAMES, Portrait, blink, breath, hexrgb, mix, pulse, ramp, save_gif, sway,  # noqa: E402,F401
                 wave)


# ---------------------------------------------------------------- helpers (this file only)
def mir(pts, cx=32):
    """mirror a point list across the vertical axis x = cx"""
    return [(2 * cx - x, y) for x, y in pts]


def frame_no(t):
    return int(round(t * FRAMES)) % FRAMES


def frng(t, seed=0):
    """a random generator that is the same for the same frame (keeps the loop seamless)"""
    return np.random.default_rng(frame_no(t) * 7919 + seed)


def twinkle(p, x, y, k, core='#ffffff', arm='#9fd8ff'):
    """a small star: k 0..1 = how bright (dot -> plus -> long plus)"""
    if k > 0.2:
        p.px(x, y, core)
    else:
        p.px(x, y, arm)
    if k > 0.45:
        p.pixels([(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)], arm)
    if k > 0.85:
        p.pixels([(x - 2, y), (x + 2, y), (x, y - 2), (x, y + 2)], mix(arm, '#000000', 0.35))


def glitch(p, t, frames_on, seed=0, tint=('#ff3ad0', '#3af0ff')):
    """displace a few horizontal slices and fringe part of them with a colour (a glitch frame)"""
    if frame_no(t) not in frames_on:
        return
    rng = frng(t, seed)
    for _ in range(3):
        y0 = int(rng.integers(6, 56))
        h = int(rng.integers(1, 3))
        dx = int(rng.choice([-2, -1, 1, 2]))
        sl = slice(y0, min(64, y0 + h))
        p.rgb[sl] = np.roll(p.rgb[sl], dx, axis=1)
        p.a[sl] = np.roll(p.a[sl], dx, axis=1)
        c = np.array(hexrgb(tint[0] if dx > 0 else tint[1]), float)
        x0 = int(rng.integers(8, 40))
        rows = p.rgb[sl, x0:x0 + int(rng.integers(8, 20))].astype(float)
        m = p.a[sl, x0:x0 + rows.shape[1]]
        rows[m] = rows[m] * 0.55 + c * 0.45
        p.rgb[sl, x0:x0 + rows.shape[1]] = rows.astype(np.uint8)


def energy(p, mask, cols, t, seed=0.0, gain=1.0):
    """fill a shape with living energy: brighter towards the middle, a periodic shimmer moving through it.
    cols = 4 colours, edge -> core"""
    if not mask.any():
        return mask
    d = ndimage.distance_transform_edt(np.pad(mask, 1))[1:-1, 1:-1]
    xx, yy = p.xx.astype(float), p.yy.astype(float)
    ph = 2 * math.pi * t
    n = 0.6 * np.sin(xx * 0.8 + 2.2 * np.sin(yy * 0.33 + ph * 2) + seed) + 0.4 * np.sin(yy * 0.55 + xx * 0.25 + ph + seed * 2)
    v = d * gain + n * 1.3
    idx = (v > 1.4).astype(int) + (v > 3.2) + (v > 5.2)
    c = np.array([hexrgb(x) if isinstance(x, str) else x for x in cols], np.uint8)[idx]
    p.rgb[mask] = c[mask]
    p.a |= mask
    return mask


def flame(p, bx, by, h, w, s, s2, cols, lean=0):
    """a tongue of fire in flat layers (outer, mid, core) sharing one base; s, s2 = sway of the tip and the waist"""
    for k, (fh, fw) in enumerate(((1.0, 1.0), (0.68, 0.62), (0.38, 0.32))):
        hh, ww = h * fh, max(1.0, w * fw)
        tip = (bx + lean * fh + s * fh, by - hh)
        pts = [(bx - ww, by), (bx - ww * 0.7 + s2 * 0.5, by - hh * 0.45), (bx - ww * 0.2 + s * 0.7, by - hh * 0.78), tip,
               (bx + ww * 0.35 + s2 * 0.6, by - hh * 0.6), (bx + ww * 0.8 + s2 * 0.3, by - hh * 0.3), (bx + ww, by)]
        p.fill(p.poly(pts), [hexrgb(cols[k]) if isinstance(cols[k], str) else cols[k]], flat=0, edge=False)


# ---------------------------------------------------------------- Crimson Rebellion
def crimson(t=0.0):
    """a being of red energy bound in a cracked black containment mask and harness; contempt in the slanted burning eyes.
    Moves: red wisps stream up, the cracks pulse, the energy flickers (flare / dim frames), sparks rise."""
    p = Portrait()
    plate = ramp('#3a2a3a', spread=0.85)
    steel = ramp('#463a4a', spread=0.75)
    body, head = breath(t), breath(t, -0.1)
    beat = pulse(t, cycles=2)
    flare = blink(t, 0.3, 1) or blink(t, 0.8, 1)
    dim = blink(t, 0.55, 1)
    kk = 1.0 if flare else (0.1 if dim else 0.35 + 0.5 * beat)
    hot = mix('#ff3a30', '#fff0d0', kk)
    seam = mix('#7a0a18', '#ff5a40', kk)
    ecols = ('#7a0618', '#d8142a', '#ff4a32', '#ffa070') if not dim else ('#5a0414', '#a00e22', '#e02a2a', '#ff6a50')
    if flare:
        ecols = ('#9a0a1e', '#ff2030', '#ff7a4a', '#ffd8b0')
    wcols = (ecols[0], ecols[2], ecols[3])

    # --- wisps streaming off the head and shoulders
    with p.at(0, head):
        for k, (bx, by, h, w, lean) in enumerate(((32, 10, 14, 5, 0), (23, 14, 14, 4, -3), (41, 14, 15, 4, 3),
                                                  (18, 24, 10, 3, -3), (46, 24, 10, 3, 3), (27, 9, 12, 3, -1), (37, 9, 13, 3, 1))):
            s = sway(t, k * 0.17, 1.5, cycles=2)
            s2 = sway(t, k * 0.17 + 0.3, 1, cycles=2)
            flame(p, bx, by, h + (2 if flare else 0), w, s, s2, wcols, lean)
    with p.at(0, body):
        for k, (bx, by, h, lean) in enumerate(((5, 50, 10, -1), (12, 46, 9, -1), (59, 50, 10, 1), (52, 46, 9, 1))):
            flame(p, bx, by, h, 3, sway(t, 0.4 + k * 0.3, 1, 2), sway(t, 0.1 + k * 0.3, 1, 2), wcols, lean)

        # --- the energy body, bound in a dark harness
        torso = p.poly([(0, 66), (0, 52), (6, 47), (16, 44), (26, 45), (32, 47), (38, 45), (48, 44), (58, 47), (64, 52), (64, 66)])
        energy(p, torso, ecols, t, seed=1.0, gain=0.7)
        for side in (-1, 1):
            # a strap from each shoulder to the core
            p.stroke([(32 + side * 17, 46), (32 + side * 5, 66)], steel[1], width=4)
            p.stroke([(32 + side * 17, 46), (32 + side * 5, 66)], steel[2] if side < 0 else steel[1], width=2)
            p.stroke([(32 + side * 16, 45), (32 + side * 4, 64)], steel[3] if side < 0 else steel[2])
            # the shoulder clamp
            sh = [(32 + side * 30, 52), (32 + side * 22, 46), (32 + side * 15, 46), (32 + side * 13, 52), (32 + side * 20, 57),
                  (32 + side * 28, 58)]
            p.fill(p.poly(sh), steel, radius=3, bias=-0.15 if side > 0 else 0.0, texture=0.05, seed=55)
            p.stroke([(32 + side * 28, 53), (32 + side * 21, 49), (32 + side * 15, 50)], seam)
            p.px(32 + side * 21, 49, hot)
            p.px(32 + side * 25, 55, steel[4])
        # the core where the straps meet: a bound orb of the being's light
        p.fill(p.ellipse(32, 59, 5.6, 5.6), steel, radius=3)
        energy(p, p.ellipse(32, 59, 3.6, 3.6), ecols, t, seed=3.0, gain=1.6)
        p.glow(32, 59, '#fff4e0', '#ff4a3a', r=1, strength=0.3 + 0.6 * kk)
        for dx, dy in ((0, -5), (0, 5), (-5, 0), (5, 0)):
            p.px(32 + dx, 59 + dy, steel[4] if dx <= 0 and dy <= 0 else steel[3])
        # the collar
        p.fill(p.poly([(22, 42), (42, 42), (44, 48), (20, 48)]), steel, radius=2.5)
        p.stroke([(22, 45), (42, 45)], seam)
        for x in (24, 32, 40):
            p.px(x, 45, hot)

    with p.at(0, head):
        # --- the energy that is its head
        energy(p, p.ellipse(32, 24, 14.2, 18.2), ecols, t, seed=2.0, gain=0.8)
        # --- the containment mask, held by clamps
        for side in (-1, 1):
            cl = [(32 + side * 9, 20), (32 + side * 16, 19), (32 + side * 17, 23), (32 + side * 16, 27), (32 + side * 9, 27)]
            p.fill(p.poly(cl), steel, radius=2, bias=-0.1 if side > 0 else 0.0)
            p.px(32 + side * 15, 23, hot)
        p.fill(p.poly([(30, 2), (34, 2), (34, 12), (30, 12)]), steel, radius=1.5)
        p.fill(p.ellipse(32, 3.0, 3.0, 2.2), steel, radius=1.5)
        p.px(32, 3, hot)
        mpts = [(23, 15), (28, 11), (36, 11), (41, 15), (43, 23), (41, 32), (37, 38), (32, 41), (27, 38), (23, 32), (21, 23)]
        broken = p.poly([(36, 10), (43, 12), (44, 19), (40, 18), (38, 14)])
        p.fill(p.poly(mpts) & ~broken, plate, radius=6, texture=0.04, seed=54)
        # ridge and cheek plates
        p.stroke([(32, 12), (32, 19)], plate[3])
        p.stroke([(33, 13), (33, 19)], plate[1])
        p.stroke([(24, 31), (28, 36), (31, 38)], plate[3])
        p.stroke([(40, 31), (36, 36), (33, 38)], plate[0])
        # a scowling brow: plates slanting down to the centre
        p.fill(p.poly([(21, 17), (31, 21), (31, 23), (21, 21)]), plate, radius=1.5, bias=0.15)
        p.fill(p.poly([(43, 17), (33, 21), (33, 23), (43, 21)]), plate, radius=1.5, bias=-0.1)
        # eyes: narrowed slits that burn
        for pts, core in (([(23, 22), (24, 23), (25, 23), (26, 24), (27, 24), (28, 25), (29, 25)], [(25, 23), (26, 24), (27, 24)]),
                          ([(41, 22), (40, 23), (39, 23), (38, 24), (37, 24), (36, 25), (35, 25)], [(39, 23), (38, 24), (37, 24)])):
            p.pixels([(x, y + 1) for x, y in pts[1:-1]], '#1a0408')
            p.pixels(pts, mix('#ff3a1a', '#ff8a3a', kk))
            p.pixels(core, hot)
        p.glow(26, 24, '#fff6e0', '#ff3a20', r=1, strength=0.35 + 0.5 * kk)
        p.glow(38, 24, '#fff6e0', '#ff3a20', r=1, strength=0.35 + 0.5 * kk)
        # the cracks
        for seg in ([(25, 13), (27, 16), (26, 19), (28, 21)], [(26, 27), (27, 30), (25, 33), (27, 37)], [(27, 30), (30, 31)],
                    [(40, 28), (38, 32), (40, 34)], [(38, 15), (36, 18), (37, 20)], [(32, 40), (33, 38), (31, 36)]):
            p.stroke(seg, seam)
        p.pixels([(27, 16), (27, 30), (38, 32), (36, 18)], hot)
        # the sneer: a split rising to one side
        p.stroke([(27, 34), (30, 34), (33, 33), (36, 32), (38, 30)], '#1a0408')
        p.stroke([(28, 33), (31, 33), (34, 32), (37, 31)], seam)
        p.pixels([(31, 33), (34, 32)], hot)
    p.rim('#ffb04a', side=1, amount=0.45)
    p.rim('#8a6aff', side=-1, amount=0.3)
    p.outline('#12040a')
    for x0, ph in ((12, 0.0), (52, 0.35), (21, 0.7), (44, 0.55), (34, 0.15)):
        u = (t * 2 + ph) % 1.0
        if u < 0.8:
            p.px(x0 + sway(t, ph, 1, 2), int(round(56 - u * 40)), mix('#ffd0a0', '#6a0a14', u))
    p.backdrop = (30, 4, 12)
    return p


# ---------------------------------------------------------------- Firmament / Obsidian (one figure, two states)
def _seraph(t, fallen):
    p = Portrait()
    if not fallen:
        hood = ramp('#2a3ca8', spread=0.75)
        mantle = ramp('#e2e6f2', spread=0.5, hue_shift=0.04)
        gold = ramp('#dcae3e', spread=0.62, hue_shift=0.04)
        mask = ramp('#eaeef8', spread=0.4, hue_shift=0.03)
        void = '#0a0f2c'
        light, light2 = '#4ab0ff', '#c8ecff'
    else:
        hood = ramp('#2e1840', spread=0.8)
        mantle = ramp('#3e3448', spread=0.75)
        gold = ramp('#8a2244', spread=0.62)
        mask = ramp('#2a2638', spread=0.95)
        void = '#12040e'
        light, light2 = '#ff2a7a', '#ffc4e4'
    body, head = breath(t), breath(t, -0.1)
    glo = pulse(t, cycles=1)
    fl = 0.4 + 0.6 * pulse(t, 0.2, 2)  # fallen: cracks flare
    if fallen and blink(t, 0.45, 2):
        fl = 1.0

    # --- the halo behind the head: a ring of turning stars / a crown of broken shards
    cx, cy, R = 32, 21, 23
    with p.at(0, head):
        if not fallen:
            ring = p.ellipse(cx, cy, R + 0.7, R + 0.7) & ~p.ellipse(cx, cy, R - 0.5, R - 0.5)
            p.fill(ring, ramp('#8aa6ec', spread=0.5), flat=2, edge=False)
            ring2 = p.ellipse(cx, cy, R - 2.6, R - 2.6) & ~p.ellipse(cx, cy, R - 3.3, R - 3.3)
            p.fill(ring2, ramp('#3a5ac8'), flat=2, edge=False)
            n = 10
            for k in range(n):
                a = 2 * math.pi * (k / n + t / n) - math.pi / 2
                x, y = int(round(cx + R * math.cos(a))), int(round(cy + R * math.sin(a)))
                # twinkle keyed to the star's place on the ring, so the loop wraps cleanly when the stars move up one slot
                twinkle(p, x - p.dx, y - p.dy, 0.3 + 0.7 * pulse(t, 0.37 * (k + t), 2), '#ffffff', '#9ad8ff')
        else:
            rng = np.random.default_rng(5)
            n = 12
            for k in range(n):
                if k in (3, 8):
                    continue  # missing pieces
                a = 2 * math.pi * k / n - math.pi / 2 + 0.1
                w = 0.09 + 0.06 * rng.random()
                ln = 4 + 5 * rng.random()
                jit = frng(t, k).integers(-1, 2, 2) if blink(t, (0.13 * k) % 1, 2) else (0, 0)
                r0 = R - 2.5 + 2 * rng.random()
                pts = [(cx + r0 * math.cos(a - w), cy + r0 * math.sin(a - w)),
                       (cx + (r0 + ln) * math.cos(a + 0.04), cy + (r0 + ln) * math.sin(a + 0.04)),
                       (cx + (r0 + 1.5) * math.cos(a + w), cy + (r0 + 1.5) * math.sin(a + w))]
                with p.at(int(jit[0]), int(jit[1])):
                    p.fill(p.poly(pts), ramp('#4a2a66', spread=0.8), radius=1.5)
                    mx, my = (pts[0][0] + pts[2][0]) / 2, (pts[0][1] + pts[2][1]) / 2
                    p.stroke([(mx, my), ((mx + pts[1][0]) / 2, (my + pts[1][1]) / 2)], mix('#c0206a', '#ff8ac0', fl))

    # --- fallen: flames climbing the sides of the hood
    if fallen:
        fcols = ('#a0104a', '#ff2a78', '#ffc0dc')
        with p.at(0, body):
            for k, (bx, by, h, w, lean) in enumerate(((6, 50, 20, 5, 2), (14, 42, 22, 4, 1), (58, 50, 20, 5, -2),
                                                      (50, 42, 22, 4, -1), (2, 56, 14, 4, 1), (62, 56, 14, 4, -1))):
                s = sway(t, k * 0.21, 1.5, cycles=2)
                s2 = sway(t, k * 0.21 + 0.3, 1, cycles=2)
                flame(p, bx, by, h, w, s, s2, fcols, lean)

    # --- the mantle over the shoulders
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (0, 54), (6, 49), (16, 45), (32, 47), (48, 45), (58, 49), (64, 54), (64, 66)]),
               mantle, radius=8, texture=0.04, seed=31)
        s = sway(t, 0.1, 1)
        for x0, c in ((7, 1), (14, 2), (50, 1), (57, 0)):
            p.stroke([(x0, 53), (x0 + 1 + s, 66)], mantle[c])
        # gold band along the mantle with stars
        p.stroke([(0, 56), (8, 51), (18, 48), (24, 49)], gold[3], width=2)
        p.stroke([(64, 56), (56, 51), (46, 48), (40, 49)], gold[2], width=2)
        for x, y in ((8, 60), (56, 60), (20, 61), (44, 61)):
            twinkle(p, x, y, 0.5, gold[4], gold[2])
        if fallen:
            # the mantle is charred and cracked, embers in the cracks
            for seg in ([(4, 60), (8, 57), (10, 62)], [(54, 58), (57, 61), (60, 59)], [(17, 55), (20, 60)]):
                p.stroke(seg, mix('#8a1040', light, fl))

    # --- the hood (the hems on the shoulders sway)
    with p.at(0, head):
        s = sway(t, 0.0, 1)
        left = [(32, 0), (27, 2), (22, 6), (19, 11), (17, 18), (16, 26), (16, 34), (14, 40), (9, 45), (3 + s, 49), (0 + s, 53),
                (6 + s, 55), (12 + s, 53), (18, 53), (23, 51), (27, 49), (32, 49)]
        hood_pts = left + list(reversed(mir(left[:-1])))
        p.fill(p.poly(hood_pts), hood, radius=5, texture=0.05, seed=33)
        p.stroke([(31, 1), (26, 3), (21, 8), (18, 16)], hood[4])
        p.stroke([(32, 1), (32, 5)], hood[2])
        # folds of the capelet
        p.stroke([(15, 42), (10, 48), (7 + s, 53)], hood[1])
        p.stroke([(49, 42), (54, 48), (57 + s, 53)], hood[0])
        p.stroke([(19, 45), (16 + s, 52)], hood[1])
        p.stroke([(45, 45), (48 + s, 52)], hood[0])
        p.stroke([(12, 47), (12 + s, 52)], hood[3])
        # stars sewn into the hood / embers smouldering in it
        for k, (x, y) in enumerate(((20, 9), (17, 30), (46, 14), (47, 33), (11, 49), (54, 50), (26, 4), (40, 5), (21, 47), (44, 47))):
            if not fallen:
                p.px(x, y, mix('#5a78e0', '#ffffff', pulse(t, k * 0.29, 2)))
            else:
                p.px(x, y, mix('#5a1030', '#ff5a9a', pulse(t, k * 0.29, 2)))
        # gold hem of the capelet
        p.stroke([(0 + s, 53), (6 + s, 55), (12 + s, 53), (18, 53), (23, 51), (27, 49), (32, 49)], gold[3])
        p.stroke([(64 - s, 53), (58 - s, 55), (52 - s, 53), (46, 53), (41, 51), (37, 49), (32, 49)], gold[2])
        # the clasp under the chin
        p.fill(p.poly([(27, 47), (32, 44), (37, 47), (36, 54), (32, 57), (28, 54)]), gold, radius=2.5)
        p.fill(p.ellipse(32, 50.5, 2.4, 2.8), ramp(light, spread=0.5), radius=2)
        p.glow(32, 50, '#ffffff', light2, r=0, strength=0.5 + 0.5 * (glo if not fallen else fl))
        # the opening, lit from within, with a gold edge
        opening = p.ellipse(32, 26, 13.2, 17.4)
        inside = p.ellipse(32, 26, 12.2, 16.4)
        p.fill(opening & ~inside, gold, flat=3)
        p.fill(opening & ~inside & (p.xx > 32 + p.dx), gold, flat=1)
        p.fill(inside, ramp(void), flat=2, edge=False)
        lit = inside & ~p.ellipse(32, 26.5, 11.0, 15.2)
        p.rgb[lit] = mix(void, light, 0.3 + 0.4 * (glo if not fallen else fl))

        # --- the mask
        mshape = p.ellipse(32, 26, 10.2, 14.6) & ~p.poly([(20, 32), (29, 45), (20, 45)]) & ~p.poly([(44, 32), (35, 45), (44, 45)])
        p.fill(mshape, mask, radius=7, texture=0.03 if not fallen else 0.0, seed=34)
        if not fallen:
            # gold brows and tear-lines, the chin edged in gold
            p.stroke([(23, 21), (26, 20), (30, 21)], gold[3])
            p.stroke([(34, 21), (38, 20), (41, 21)], gold[2])
            p.stroke([(25, 28), (25, 33)], gold[2])
            p.stroke([(39, 28), (39, 33)], gold[1])
            p.pixels([(25, 34), (39, 34)], light)
            p.stroke([(27, 38), (29, 39), (35, 39), (37, 38)], gold[2])
            # gold filigree curling over the temples
            for side in (-1, 1):
                p.pixels([(32 + side * 8, 17), (32 + side * 9, 18), (32 + side * 9, 19), (32 + side * 8, 20)], gold[2] if side < 0 else gold[1])
                p.pixels([(32 + side * 4, 15), (32 + side * 5, 14), (32 + side * 6, 14)], gold[3] if side < 0 else gold[2])
            # serene lowered eyes: slits of blue light
            for ex in (27, 37):
                p.pixels([(ex - 3, 24), (ex + 3, 24)], mask[1])
                p.pixels([(ex - 2, 25), (ex - 1, 25), (ex, 25), (ex + 1, 25), (ex + 2, 25)], mix(light, light2, 0.4 + 0.6 * glo))
                p.pixels([(ex - 1, 25), (ex, 25), (ex + 1, 25)], '#ffffff')
                p.pixels([(ex - 2, 26), (ex + 2, 26)], mask[2])
                p.glow(ex, 25, '#ffffff', light, r=1, strength=0.25 + 0.35 * glo)
                p.pixels([(ex - 1, 25), (ex, 25), (ex + 1, 25)], '#ffffff')
                p.stroke([(ex - 2, 23), (ex + 2, 23)], mask[2])
            # nose and calm lips
            p.stroke([(32, 23), (32, 31)], mask[4])
            p.pixels([(33, 30), (33, 31), (31, 32)], mask[1])
            p.pixels([(29, 35), (35, 35)], gold[1])
            p.stroke([(30, 36), (34, 36)], gold[3])
            p.stroke([(31, 37), (33, 37)], gold[1])
            # the star on the brow
            p.pixels([(32, 13), (32, 14), (30, 16), (34, 16), (32, 18)], gold[3])
            p.glow(32, 16, '#ffffff', mix(light, light2, glo), r=0, strength=0.6 + 0.4 * glo)
            p.pixels([(24, 17), (25, 16), (26, 15)], '#ffffff')
        else:
            # black glass: sharp highlights
            p.pixels([(24, 17), (25, 16), (26, 15), (27, 14)], '#b0a0e0')
            p.pixels([(25, 16), (26, 15)], '#ffffff')
            p.pixels([(38, 35), (39, 34)], '#6a5898')
            cc = mix('#6a0838', light, 0.25 + 0.75 * fl)
            hot = mix(light, light2, fl)
            # angry eyes, slanting in
            for ex, d in ((27, 1), (37, -1)):
                pts = [(ex - 3 * d, 23), (ex - 2 * d, 24), (ex - d, 24), (ex, 25), (ex + d, 25), (ex + 2 * d, 26)]
                p.pixels(pts, mix('#a0105a', light, glo))
                p.pixels([(ex - d, 25), (ex, 26), (ex + d, 26)], '#2a0614')
                p.pixels([(ex - d, 24), (ex, 25)], hot)
                p.glow(ex, 25, '#ffffff', light, r=1, strength=0.45 + 0.4 * fl)
            # the cracks, flaring
            for seg in ([(32, 12), (31, 16), (29, 19), (28, 22)], [(26, 28), (24, 32), (25, 36), (23, 38)],
                        [(35, 14), (38, 17), (41, 18)], [(37, 30), (39, 34), (37, 38)], [(30, 31), (32, 34)]):
                p.stroke(seg, cc)
            p.pixels([(31, 16), (24, 32), (38, 17), (39, 34)], hot)
            p.stroke([(28, 36), (32, 35), (36, 36)], cc)
            p.px(32, 35, hot)
            p.pixels([(30, 15), (34, 17)], gold[3])
            p.glow(32, 16, '#ffffff', light, r=0, strength=0.5 + 0.5 * fl)

    if not fallen:
        p.rim('#5ad8ff', side=1, amount=0.5)
        p.rim('#ffd27a', side=-1, amount=0.25)
        p.outline('#060820')
        for x0, ph in ((8, 0.0), (56, 0.45), (20, 0.7), (45, 0.2)):
            u = (t * 2 + ph) % 1.0
            if u < 0.8:
                p.px(x0 + sway(t, ph, 1, 2), int(round(60 - u * 24)), mix('#e0f4ff', '#2a4aa0', u))
        p.backdrop = (8, 14, 42)
    else:
        p.rim('#ff3a7a', side=1, amount=0.5)
        p.rim('#9a5aff', side=-1, amount=0.3)
        p.outline('#0c0410')
        for x0, ph in ((10, 0.0), (54, 0.4), (22, 0.65), (43, 0.15), (32, 0.85)):
            u = (t * 2 + ph) % 1.0
            if u < 0.85:
                p.px(x0 + sway(t, ph, 1, 2), int(round(58 - u * 30)), mix('#ffb0d0', '#5a0a2a', u))
        glitch(p, t, (6, 17))
        p.backdrop = (22, 6, 26)
    return p


def firmament(t=0.0):
    """serene masked guardian, robed in blue and gold, a halo of turning stars.
    Moves: halo stars turn and twinkle, hood hems sway, the light in the hood pulses, motes rise."""
    return _seraph(t, False)


def obsidian(t=0.0):
    """the Firmament fallen: black obsidian mask, cracks glowing red-violet, the halo shattered into shards, flames.
    Moves: cracks flare, shards jitter, flames lick, glitch frames."""
    return _seraph(t, True)


# ---------------------------------------------------------------- Shipwrights of Axis
def axis(t=0.0):
    """an insectoid shipwright in battered work armour: a bronze chitin head, huge amber compound eyes half-hidden
    under heavy brow plates (sizing up your war), brass goggle cups, one mandible capped in gold.
    Moves: antennae sway, mandibles click, a sheen slides over the eyes, welding sparks drift."""
    p = Portrait()
    chit = ramp('#9a6230', spread=0.72, hue_shift=0.05)
    dark = ramp('#2a3a44', spread=0.75)
    plate = ramp('#b4c2c8', spread=0.6)
    brass = ramp('#b8923a', spread=0.6)
    eye = ramp('#ffb81a', spread=0.4, hue_shift=0.05)
    red = ramp('#c0302a', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    click = blink(t, 0.2, 2) or blink(t, 0.36, 1)
    with p.at(0, head):
        # antennae, segmented, swaying
        for side, ph in ((-1, 0.0), (1, 0.4)):
            s1, s2 = sway(t, ph, 1), sway(t, ph + 0.15, 2)
            pts = [(32 + side * 3, 6), (32 + side * 6, 2), (32 + side * 12 + s1, 0), (32 + side * 19 + s1, 1), (32 + side * 25 + s2, 5)]
            p.stroke(pts[:2], chit[1], width=2)
            p.stroke(pts[1:], chit[2])
            p.px(32 + side * 6, 2, chit[4])
            p.px(32 + side * 25 + s2, 5, chit[4])
    with p.at(0, body):
        # jumpsuit, the segmented neck
        p.fill(p.poly([(0, 66), (0, 50), (10, 43), (22, 40), (32, 44), (42, 40), (54, 43), (64, 50), (64, 66)]), dark, radius=8,
               texture=0.05, seed=61)
        p.fill(p.poly([(25, 38), (39, 38), (41, 50), (23, 50)]), dark, radius=4)
        for y in (41, 44, 47):
            p.stroke([(25, y), (39, y)], dark[0])
            p.stroke([(26, y + 1), (38, y + 1)], dark[3])
        # tool harness across the chest, a brass credit-medal on a chain
        p.stroke([(14, 50), (44, 66)], ramp('#5a3a26')[1], width=3)
        p.stroke([(15, 49), (45, 65)], ramp('#5a3a26')[3])
        p.stroke([(26, 50), (32, 56), (38, 50)], brass[2])
        p.fill(p.ellipse(32, 58.5, 3.4, 3.4), brass, radius=2)
        p.pixels([(32, 57), (31, 58), (32, 58), (33, 58), (32, 59), (32, 60)], brass[0])
        p.px(31, 57, brass[4])
        # battered pauldrons with a hazard band
        for side in (-1, 1):
            sh = [(32 + side * 33, 45), (32 + side * 29, 38), (32 + side * 20, 36), (32 + side * 12, 40), (32 + side * 11, 50),
                  (32 + side * 20, 56), (32 + side * 33, 58)]
            p.fill(p.poly(sh), plate, radius=5, bias=-0.15 if side > 0 else 0.0, texture=0.04, seed=62 + side)
            p.fill(p.poly([(32 + side * 12, 50), (32 + side * 21, 53), (32 + side * 33, 53), (32 + side * 33, 59),
                           (32 + side * 20, 58), (32 + side * 11, 54)]), plate, radius=2, bias=-0.25)
            band = p.poly([(32 + side * 12, 44), (32 + side * 20, 41), (32 + side * 33, 43), (32 + side * 33, 47), (32 + side * 20, 45),
                           (32 + side * 12, 48)])
            p.fill(band, ramp('#e8d8b0'), flat=2 if side < 0 else 1, edge=False)
            for k in range(-2, 10):
                x = 32 + side * (10 + k * 4)
                stripe = p.poly([(x, 49), (x + side * 2, 49), (x + side * 6, 40), (x + side * 4, 40)])
                p.fill(band & stripe, red, flat=2 if side < 0 else 1, edge=False)
            p.stroke([(32 + side * 12, 48), (32 + side * 20, 45), (32 + side * 33, 47)], plate[0])
            p.stroke([(32 + side * 13, 40), (32 + side * 20, 37), (32 + side * 28, 39)], plate[4] if side < 0 else plate[3])
            for x, y in ((32 + side * 15, 51), (32 + side * 27, 51), (32 + side * 16, 56), (32 + side * 26, 57)):
                p.px(x, y, plate[4] if side < 0 else plate[3])
                p.px(x + 1, y + 1, plate[0])
            p.pixels([(32 + side * 23, 52), (32 + side * 24, 52), (32 + side * 30, 49)], plate[1])
        # a hose from the shoulder to the neck
        p.stroke([(15, 42), (20, 38), (25, 39)], dark[1], width=2)
        p.stroke([(16, 41), (20, 37)], dark[3])

    with p.at(0, head):
        # the head: a long chitin face, broad brow, tapering to the mouthparts
        hd = p.poly([(24, 8), (28, 5), (32, 4), (36, 5), (40, 8), (43, 14), (43, 24), (42, 32), (39, 39), (36, 44), (32, 46),
                     (28, 44), (25, 39), (22, 32), (21, 24), (21, 14)])
        p.fill(hd, chit, radius=7, texture=0.07, seed=63)
        # brow plates and the central keel
        p.stroke([(24, 11), (28, 9), (32, 8), (36, 9), (40, 11)], chit[1])
        p.stroke([(25, 9), (28, 7), (32, 6), (36, 7), (39, 9)], chit[4])
        p.stroke([(32, 9), (32, 21)], chit[1])
        p.stroke([(31, 10), (31, 20)], chit[4])
        p.stroke([(25, 14), (29, 16)], chit[1])
        p.stroke([(39, 14), (35, 16)], chit[1])
        # a welding visor pushed up on the brow
        p.fill(p.poly([(22, 9), (27, 4), (37, 4), (42, 9), (42, 13), (22, 13)]), dark, radius=3, bias=0.05)
        p.fill(p.poly([(25, 8), (39, 8), (39, 11), (25, 11)]), ramp('#2ad8e0', spread=0.5), radius=1.5, bias=0.2)
        p.stroke([(26, 9), (30, 9)], '#d8ffff')
        p.stroke([(22, 13), (42, 13)], dark[0])
        p.pixels([(21, 11), (43, 11)], brass[3])
        # face plates in rings down to the mouth
        for y, w in ((24, 7), (28, 6), (32, 5)):
            p.stroke([(32 - w, y), (32 + w, y)], chit[1])
            p.stroke([(32 - w + 1, y + 1), (32 + w - 2, y + 1)], chit[3])
        # cheek seams
        p.stroke([(23, 30), (26, 37), (29, 41)], chit[1])
        p.stroke([(41, 30), (38, 37), (35, 41)], chit[0])
        # huge round compound eyes bulging from the sides, in brass lens-cups
        sheen = int(round((t * 2 % 1) * 16)) - 4
        for side, cxe in ((-1, 19.5), (1, 44.5)):
            p.fill(p.ellipse(cxe, 19, 7.2, 7.6), brass, radius=2.5, bias=-0.15 if side > 0 else 0.05)
            for ang in (0.6, 2.2, 3.8, 5.4):
                p.px(int(round(cxe + 6.6 * math.cos(ang))), int(round(19 + 7.0 * math.sin(ang))), brass[4] if side < 0 else brass[3])
            em = p.ellipse(cxe, 19, 5.8, 6.2)
            p.fill(em, eye, radius=5, bias=0.25)
            for yy in range(12, 27):
                for xx in range(int(cxe) - 6, int(cxe) + 7):
                    if 0 <= xx + p.dx < 64 and em[yy + p.dy, xx + p.dx] and (xx * 2 + yy) % 5 == 0:
                        p.px(xx, yy, eye[1] if (xx + yy) % 2 else eye[4])
            for k in range(4):
                x, y = int(cxe) - 6 + sheen + k, 23 - k
                if 0 <= x + p.dx < 64 and em[y + p.dy, x + p.dx]:
                    p.px(x, y, '#fffbe0')
            p.pixels([(int(cxe) - 2, 15), (int(cxe) - 3, 16)], '#ffffff')
            p.glow(int(cxe), 19, '#fff6d0', '#ffd060', r=1, strength=0.25 + 0.35 * pulse(t, 0.2 * side, 2))
        # mandibles (one capped in gold, lifted in a smirk), clicking
        o = 1 if click else 0
        p.fill(p.poly([(29, 38), (35, 38), (35, 44), (29, 44)]), dark, flat=0, edge=False)
        p.fill(p.poly([(27, 35), (37, 35), (36, 39), (28, 39)]), chit, radius=2, bias=0.1)
        p.fill(p.poly([(24, 37), (29, 38), (31 - o, 44), (30 - o, 48), (27 - o, 46), (25, 42)]), chit, radius=2, bias=-0.05)
        p.fill(p.poly([(40, 36), (35, 38), (34 + o, 43), (35 + o, 46), (38 + o, 44), (40, 41)]), brass, radius=2)
        p.pixels([(29 - o, 47), (27, 42)], chit[0])
        p.pixels([(36 + o, 44), (37, 40)], brass[4])
        # palps beside the mouth
        for side, ph in ((-1, 0.1), (1, 0.6)):
            sp = sway(t, ph, 1, 2)
            p.stroke([(32 + side * 9, 38), (32 + side * 11, 42), (32 + side * 11 + sp, 46)], chit[1])
            p.px(32 + side * 11 + sp, 46, chit[3])
    p.rim('#46f0e0', side=1, amount=0.5)
    p.rim('#ffb050', side=-1, amount=0.25)
    p.outline('#0a0d0e')
    # welding sparks
    for x0, ph in ((8, 0.0), (56, 0.33), (50, 0.66), (14, 0.5)):
        u = (t * 2 + ph) % 1.0
        if u < 0.75:
            x = x0 + int(round(u * 4 * (1 if x0 < 32 else -1)))
            p.px(x, int(round(40 - u * 18 + 30 * u * u)), mix('#fff0a0', '#ff5a10', u))
    p.backdrop = (10, 24, 30)
    return p


# ---------------------------------------------------------------- Bentor Conglomerate
def bentor(t=0.0):
    """a gaunt, grey-skinned elder under a wide swept hood, a glowing relic-disc set in its crown, blue tendril-locks,
    a high gold-trimmed collar: a cold, calculating collector of fragments.
    Moves: the crown disc pulses as a scan ring turns, tendrils sway, the eyes blink, data motes rise."""
    p = Portrait()
    hood = ramp('#4a5a80', spread=0.75)
    coat = ramp('#2c3452', spread=0.8)
    gold = ramp('#d4a440', spread=0.6, hue_shift=0.04)
    skin = ramp('#a69ab4', spread=0.62)
    lock = ramp('#357a9c', spread=0.7)
    disc = ramp('#3ae0ff', spread=0.45)
    body, head = breath(t), breath(t, -0.1)
    glo = pulse(t, cycles=2)
    with p.at(0, body):
        # coat, high collar
        p.fill(p.poly([(0, 66), (0, 52), (8, 46), (20, 43), (32, 47), (44, 43), (56, 46), (64, 52), (64, 66)]), coat, radius=8,
               texture=0.05, seed=71)
        for side in (-1, 1):
            col = [(32 + side * 4, 50), (32 + side * 9, 36), (32 + side * 15, 38), (32 + side * 13, 52), (32 + side * 7, 60)]
            p.fill(p.poly(col), coat, radius=3, bias=-0.15 if side > 0 else 0.1)
            p.stroke([(32 + side * 9, 37), (32 + side * 14, 39), (32 + side * 12, 52)], gold[3] if side < 0 else gold[2])
            # embroidered gold panels down the front
            p.stroke([(32 + side * 6, 58), (32 + side * 9, 66)], gold[2])
            p.pixels([(32 + side * 17, 50), (32 + side * 22, 49), (32 + side * 27, 50)], gold[2])
            p.pixels([(32 + side * 18, 54), (32 + side * 24, 55)], gold[1])
        p.stroke([(4, 54), (12, 49), (20, 47)], gold[2])
        p.stroke([(60, 54), (52, 49), (44, 47)], gold[1])
        # the neck
        p.fill(p.poly([(28, 38), (36, 38), (37, 50), (32, 53), (27, 50)]), skin, radius=3, bias=-0.3)
        # an ornate pendant
        p.stroke([(27, 48), (32, 54), (37, 48)], gold[1])
        p.fill(p.poly([(29, 54), (32, 52), (35, 54), (35, 58), (32, 62), (29, 58)]), gold, radius=2)
        p.fill(p.ellipse(32, 57, 1.8, 2.2), disc, radius=1.5)
        p.glow(32, 57, '#ffffff', disc[3], r=0, strength=0.4 + 0.5 * glo)

    with p.at(0, head):
        # tendril-locks hanging from under the hood (behind the face)
        for k, (x0, x1, ln) in enumerate(((22, 19, 30), (20, 15, 26), (24, 22, 32), (42, 45, 30), (44, 49, 26), (40, 42, 32))):
            sp = sway(t, k * 0.19, 1)
            y0 = 20
            pts = [(x0, y0), ((x0 + x1) / 2, y0 + ln * 0.5), (x1 + sp, y0 + ln)]
            p.stroke(pts, lock[1], width=2)
            p.stroke(pts[:2] + [(x1 + sp, y0 + ln - 2)], lock[3] if x0 < 32 else lock[2])
            p.px(int(x1 + sp), y0 + ln, lock[4])
        # the long ears, swept back
        for side in (-1, 1):
            p.fill(p.poly([(32 + side * 8, 22), (32 + side * 19, 21), (32 + side * 22, 19), (32 + side * 17, 26), (32 + side * 9, 29)]),
                   skin, radius=2, bias=-0.15 if side > 0 else 0.0)
            p.stroke([(32 + side * 10, 24), (32 + side * 18, 22)], skin[1])
        # the long gaunt face
        face = p.poly([(23, 15), (41, 15), (42, 26), (40, 34), (37, 41), (34, 45), (30, 45), (27, 41), (24, 34), (22, 26)])
        p.fill(face, skin, radius=7, texture=0.12, seed=72)
        # cheekbones and hollows, wrinkles
        p.stroke([(24, 29), (27, 33), (28, 38)], skin[1])
        p.stroke([(40, 29), (37, 33), (36, 38)], skin[0])
        p.stroke([(25, 27), (28, 28)], skin[3])
        p.stroke([(39, 27), (36, 28)], skin[2])
        p.pixels([(26, 31), (27, 35), (38, 32)], skin[1])
        # a heavy, furrowed brow
        p.stroke([(23, 19), (27, 18), (30, 20)], skin[0], width=2)
        p.stroke([(41, 19), (37, 18), (34, 20)], skin[0], width=2)
        p.stroke([(24, 18), (28, 17)], skin[3])
        p.stroke([(31, 17), (31, 19)], skin[1])
        p.stroke([(33, 17), (33, 19)], skin[1])
        # cold blue eyes
        if blink(t, 0.7, 2):
            p.stroke([(25, 22), (29, 22)], skin[0])
            p.stroke([(35, 22), (39, 22)], skin[0])
        else:
            for ex in (27, 37):
                p.pixels([(ex - 2, 22), (ex - 1, 22), (ex, 22), (ex + 1, 22), (ex + 2, 22)], '#1a2a5a')
                p.pixels([(ex - 1, 22), (ex, 22), (ex + 1, 22)], '#4aa8ff')
                p.px(ex, 22, '#e0f6ff')
                p.glow(ex, 22, '#e0f6ff', '#3a8aff', r=0, strength=0.35 + 0.2 * glo)
            p.pixels([(25, 23), (29, 23), (35, 23), (39, 23)], skin[1])
        # a long nose, a thin mouth turned down
        p.stroke([(32, 21), (32, 31)], skin[4])
        p.stroke([(33, 22), (33, 31)], skin[1])
        p.pixels([(30, 32), (34, 32), (31, 33)], skin[0])
        p.stroke([(28, 38), (30, 37), (34, 37), (36, 38)], '#3a2a44')
        p.pixels([(28, 39), (36, 39)], skin[1])
        p.stroke([(31, 40), (33, 40)], skin[3])
        p.stroke([(30, 43), (34, 43)], skin[1])

        # the wide swept hood
        hd = [(32, 1), (39, 2), (45, 5), (52, 8), (58, 10), (63, 9), (60, 14), (53, 18), (46, 20), (42, 17), (32, 15), (22, 17),
              (18, 20), (11, 18), (4, 14), (1, 9), (6, 10), (12, 8), (19, 5), (25, 2)]
        p.fill(p.poly(hd), hood, radius=4, texture=0.06, seed=73)
        p.stroke([(1, 9), (6, 10), (12, 8), (19, 5), (25, 2), (32, 1)], hood[4])
        p.stroke([(4, 14), (11, 18), (18, 20), (22, 17), (32, 15), (42, 17), (46, 20), (53, 18), (60, 14)], gold[2])
        for x, y in ((10, 12), (16, 10), (48, 10), (54, 12), (14, 15), (50, 15)):
            p.px(x, y, gold[3])
        p.stroke([(20, 9), (14, 13)], hood[1])
        p.stroke([(44, 9), (50, 13)], hood[0])
        # the relic-disc in its gold bezel
        p.fill(p.ellipse(32, 8.5, 9.6, 6.2), gold, radius=2.5)
        dm = p.ellipse(32, 8.5, 7.6, 4.6)
        p.fill(dm, disc, radius=4, bias=0.1 + 0.3 * glo, edge=False)
        p.fill(dm & ~p.ellipse(32, 8.5, 6.4, 3.6), disc, flat=1, edge=False)
        for k in range(5):
            a = 2 * math.pi * (t + k * 0.03)
            x, y = 32 + 6.9 * math.cos(a), 8.5 + 4.0 * math.sin(a)
            p.px(int(round(x)), int(round(y)), '#ffffff' if k == 4 else disc[4])
        p.glow(32, 8, '#ffffff', '#9af4ff', r=1, strength=0.35 + 0.55 * glo)
        for x in (22, 42):
            p.px(x, 9, '#e8ffff')
    p.rim('#b07aff', side=1, amount=0.5)
    p.rim('#3ae0ff', side=-1, amount=0.25)
    p.outline('#0a0818')
    # data motes rising from the disc
    for k, (x0, ph) in enumerate(((13, 0.0), (51, 0.3), (8, 0.55), (56, 0.8))):
        u = (t * 2 + ph) % 1.0
        if u < 0.75:
            p.px(x0 + sway(t, ph, 1, 2), int(round(44 - u * 26)), mix('#c8fcff', '#2a4a8a', u))
    p.backdrop = (18, 12, 44)
    return p


# ---------------------------------------------------------------- Celdauri Trade Confederation
def celdauri(t=0.0):
    """a grey-haired veteran trade-captain, three-quarter turned, in a high-collared slate jacket with yellow trim:
    composed, one brow raised, a measured half-smile for preferred clients.
    Moves: breathing, a blink, the earpiece light pulses, a glint runs over the rank badge."""
    p = Portrait()
    skin = ramp('#d09474', spread=0.6, hue_shift=0.05)
    hair = ramp('#cfd6e6', spread=0.55, hue_shift=0.04)
    jacket = ramp('#3a5868', spread=0.72)
    inner = ramp('#24323c', spread=0.7)
    yel = ramp('#ecb82a', spread=0.55, hue_shift=0.04)
    steel = ramp('#8a96a4', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # shoulders and the jacket
        p.fill(p.poly([(0, 66), (0, 50), (6, 45), (17, 41), (30, 44), (44, 41), (55, 44), (64, 50), (64, 66)]), jacket, radius=9,
               texture=0.04, seed=81)
        # a yellow stripe down the front, a pocket flap, badges
        p.fill(p.poly([(20, 50), (24, 50), (25, 66), (21, 66)]), yel, radius=1.5)
        p.fill(p.poly([(39, 54), (49, 53), (49, 57), (39, 58)]), jacket, radius=2, bias=-0.2)
        p.fill(p.poly([(8, 52), (16, 50), (17, 56), (9, 58)]), inner, radius=2)
        for k in range(3):
            p.stroke([(9, 53 + k * 2), (15, 51 + k * 2)], yel[2] if k != 1 else steel[3])
        p.fill(p.poly([(41, 60), (47, 59), (47, 63), (41, 64)]), steel, radius=1.5)
        g = int(round((t * 1.5 % 1) * 12)) - 3
        for k in range(2):
            x = 41 + g + k
            if 41 <= x <= 47:
                p.px(x, 61 - k, '#ffffff')
        # epaulettes
        p.fill(p.poly([(3, 47), (14, 42), (18, 44), (8, 50)]), jacket, radius=2, bias=0.15)
        p.stroke([(4, 47), (14, 43)], yel[3])
        p.fill(p.poly([(61, 47), (50, 42), (46, 44), (56, 50)]), jacket, radius=2, bias=-0.2)
        p.stroke([(60, 47), (50, 43)], yel[1])
        # the neck
        p.fill(p.poly([(27, 38), (40, 38), (40, 48), (26, 48)]), skin, radius=4, bias=-0.3)
        p.stroke([(38, 42), (37, 47)], skin[0])
        # the tall collar, open at the front like a funnel
        p.fill(p.poly([(14, 46), (17, 32), (21, 30), (27, 40), (30, 50), (24, 54)]), jacket, radius=3, bias=0.15)
        p.fill(p.poly([(17, 33), (21, 31), (27, 41), (29, 48), (26, 46)]), inner, radius=2)
        p.fill(p.poly([(50, 46), (47, 33), (43, 31), (38, 41), (36, 50), (42, 53)]), jacket, radius=3, bias=-0.25)
        p.fill(p.poly([(43, 32), (46, 34), (48, 45), (44, 41)]), yel, radius=2, bias=-0.1)
        p.stroke([(14, 46), (17, 32), (21, 30)], yel[3], width=2)
        p.stroke([(21, 31), (27, 41), (30, 50)], yel[2])
        p.stroke([(43, 31), (38, 41), (36, 50)], yel[1])
        for k in range(3):
            p.stroke([(45 + k, 37 + k), (47 + k, 44 + k)], yel[1] if k % 2 else yel[3])

    with p.at(0, head):
        # the head, three-quarter turned to the right
        hd = p.poly([(20, 15), (25, 9), (33, 7), (40, 9), (45, 14), (46, 20), (46, 24), (47, 27), (46, 31), (44, 36), (41, 41),
                     (37, 44), (34, 45), (30, 43), (26, 39), (22, 34), (20, 28), (19, 21)])
        p.fill(hd, skin, radius=9, texture=0.025, seed=82)
        # cropped hair over the ear; the ear with a comm-piece
        p.fill(p.poly([(19, 14), (27, 12), (27, 18), (25, 24), (21, 26), (18, 21)]), hair, flat=1, edge=False)
        for x in (20, 22, 24):
            p.stroke([(x, 15), (x, 24 - (x - 20) // 2)], hair[0])
        p.stroke([(26, 13), (26, 18), (24, 24)], hair[2])
        p.fill(p.ellipse(21.5, 28.5, 2.8, 4.2), skin, radius=2, bias=-0.15)
        p.stroke([(22, 26), (22, 30)], skin[1])
        p.fill(p.poly([(18, 26), (21, 25), (21, 31), (18, 31)]), steel, radius=1.5)
        lk = pulse(t, 0, 2)
        p.glow(19, 28, '#fff4c0', mix('#ff8a10', '#ffd060', lk), r=0, strength=0.4 + 0.5 * lk)
        p.pixels([(21, 33), (20, 34), (21, 35), (22, 34)], yel[3])
        p.px(21, 34, '#3a2010')
        # jaw shadow, cheekbones, the lines of a long career
        p.stroke([(25, 37), (30, 42), (34, 44)], skin[1])
        p.stroke([(46, 30), (44, 36), (41, 40)], skin[1])
        p.stroke([(29, 31), (33, 31)], skin[3])
        p.pixels([(30, 30), (31, 30)], skin[4])
        p.stroke([(34, 34), (33, 37), (33, 39)], skin[1])
        p.pixels([(27, 25), (27, 27), (26, 26)], skin[1])
        p.stroke([(30, 16), (36, 15)], skin[2])
        p.stroke([(31, 18), (34, 18)], skin[2])
        # brows: the far one raised
        p.stroke([(27, 22), (30, 21), (34, 22)], hair[1])
        p.stroke([(28, 23), (33, 23)], skin[1])
        p.stroke([(39, 20), (42, 19), (45, 21)], hair[1])
        # eyes, lined, the near one larger
        shut = blink(t, 0.4, 2)
        if shut:
            p.stroke([(28, 26), (34, 26)], '#3a1e1e')
            p.stroke([(39, 25), (43, 25)], '#3a1e1e')
        else:
            p.stroke([(28, 25), (34, 25)], '#24141a')
            p.px(35, 24, '#24141a')
            p.pixels([(29, 26), (30, 26), (31, 26), (32, 26), (33, 26)], '#efe6de')
            p.pixels([(31, 26), (32, 26)], '#4a86a8')
            p.px(32, 26, '#1c2a34')
            p.px(31, 26, '#9ad4f0')
            p.stroke([(29, 27), (33, 27)], skin[1])
            p.stroke([(39, 24), (43, 24)], '#24141a')
            p.px(44, 23, '#24141a')
            p.pixels([(40, 25), (41, 25), (42, 25)], '#efe6de')
            p.px(41, 25, '#4a86a8')
            p.px(42, 25, '#1c2a34')
            p.stroke([(40, 26), (42, 26)], skin[1])
        # nose
        p.stroke([(37, 24), (38, 28), (38, 31)], skin[4])
        p.stroke([(40, 26), (40, 31)], skin[1])
        p.pixels([(37, 32), (38, 32)], skin[0])
        p.px(40, 32, skin[1])
        # a touch of colour on the cheek
        p.pixels([(29, 31), (30, 32), (31, 32), (30, 31)], mix(skin[3], '#e07a7a', 0.35))
        # a measured half-smile
        lip = ramp('#c0606a', spread=0.5)
        p.stroke([(33, 37), (37, 37), (40, 36), (41, 35)], lip[0])
        p.stroke([(34, 38), (39, 38)], lip[2])
        p.stroke([(35, 39), (38, 39)], lip[3])
        p.px(36, 39, lip[4])
        p.px(42, 35, skin[1])
        p.stroke([(36, 42), (38, 42)], skin[3])
        # lashes
        p.pixels([(35, 24), (36, 23)], '#24141a')
        p.pixels([(44, 23), (45, 22)], '#24141a')
        # silver hair, swept up and back in spikes
        hr = [(18, 19), (17, 11), (13, 7), (20, 7), (18, 1), (25, 5), (25, -1), (30, 4), (33, 0), (36, 5), (40, 2), (41, 8),
              (46, 7), (45, 13), (46, 18), (42, 14), (36, 13), (30, 14), (25, 16), (21, 21)]
        p.fill(p.poly(hr), hair, radius=4, texture=0.1, seed=83)
        for seg in ([(21, 15), (19, 8)], [(25, 13), (23, 6)], [(30, 12), (28, 4)], [(35, 11), (34, 4)], [(40, 11), (40, 5)]):
            p.stroke(seg, hair[1])
        p.stroke([(23, 12), (21, 6)], hair[4])
        p.stroke([(28, 11), (26, 3)], hair[4])
    p.rim('#46e8ff', side=1, amount=0.5)
    p.rim('#ffb060', side=-1, amount=0.25)
    p.outline('#0c0a10')
    p.backdrop = (36, 22, 12)
    return p


# ---------------------------------------------------------------- Cheiran Hordes
def cheiran(t=0.0):
    """a hulking crab-thing: a riveted bronze carapace for a head, a slit of ember eyes under its lip, a beard of
    feeding tentacles, jointed legs arching overhead and sickle claws raised from below: the vanguard of a horde.
    Moves: tentacles writhe, the eyes blink out of step, the claws snap, the legs flex."""
    p = Portrait()
    shell = ramp('#b4462a', spread=0.85, hue_shift=0.05)
    body_c = ramp('#3e4a6e', spread=0.78)
    leg = ramp('#6a5a92', spread=0.75)
    flesh = ramp('#c8909a', spread=0.6)
    claw = ramp('#d8c098', spread=0.68)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # jointed legs arching overhead from behind the body
        for side in (-1, 1):
            for k, (base, knee, foot) in enumerate((((21, 22), (13, 1), (2, 13)), ((19, 30), (5, 14), (-1, 30)))):
                fx = sway(t, 0.3 * k + (0.5 if side > 0 else 0), 1)
                pts = [base, (knee[0] + fx, knee[1] - abs(fx)), foot]
                pts = [(32 + side * (32 - x), y) for x, y in pts]
                p.stroke(pts, leg[0], width=5)
                p.stroke(pts, leg[2], width=3)
                p.stroke(pts[:2], leg[4] if side < 0 else leg[3], width=1)
                p.fill(p.ellipse(pts[1][0] + 0.5, pts[1][1] + 0.5, 2.2, 2.2), leg, radius=1.5)
                p.px(int(pts[2][0]), int(pts[2][1]), '#ff8a3a')
        # the armoured body
        p.fill(p.poly([(0, 66), (0, 42), (10, 33), (22, 30), (32, 33), (42, 30), (54, 33), (64, 42), (64, 66)]), body_c, radius=8,
               texture=0.06, seed=91)
        for side in (-1, 1):
            p.fill(p.poly([(32 + side * 33, 44), (32 + side * 24, 33), (32 + side * 14, 33), (32 + side * 15, 44), (32 + side * 25, 50),
                           (32 + side * 33, 52)]), body_c, radius=4, bias=0.1 if side < 0 else -0.15)
            p.stroke([(32 + side * 31, 45), (32 + side * 22, 37), (32 + side * 15, 38)], body_c[3] if side < 0 else body_c[2])
        for y in (54, 59):
            p.stroke([(20, y), (32, y + 2), (44, y)], body_c[0])
            p.stroke([(21, y + 1), (32, y + 3), (43, y + 1)], body_c[3])
        # the gullet: a dark maw with teeth behind the tentacles
        p.fill(p.ellipse(32, 41, 8.5, 6.5), ramp('#2a0e18'), flat=1, edge=False)
        for x in range(25, 40, 2):
            p.px(x, 37, '#e8d8c0')
            p.px(x + 1, 45, '#c8b8a0')
    with p.at(0, head):
        # the carapace: a crab's shell, grooved and studded, an alloy keel riveted down the middle
        dome = p.ellipse(32, 19, 18.0, 15.5) & (p.yy < 31 + p.dy)
        p.fill(dome, shell, radius=8, texture=0.03, seed=92)
        for side in (-1, 1):
            p.stroke([(32 + side * 5, 7), (32 + side * 8, 13), (32 + side * 7, 19), (32 + side * 9, 26)], shell[0])
            p.stroke([(32 + side * 6, 7), (32 + side * 9, 13)], shell[3] if side < 0 else shell[2])
            p.stroke([(32 + side * 9, 16), (32 + side * 14, 19), (32 + side * 17, 25)], shell[1])
            for x, y in ((32 + side * 12, 10), (32 + side * 14, 14), (32 + side * 12, 24), (32 + side * 4, 23), (32 + side * 15, 22)):
                p.px(x, y, shell[4] if side < 0 else shell[3])
                p.px(x, y + 1, shell[0])
        p.fill(p.poly([(30, 4), (34, 4), (34, 28), (30, 28)]), ramp('#9aa0b4', spread=0.7), radius=2, bias=0.05)
        for y in (8, 14, 20, 26):
            p.px(31, y, '#ffffff')
            p.px(32, y + 1, '#3a3a50')
        p.pixels([(19, 13), (20, 11), (21, 9), (23, 7), (25, 6)], '#ffd8c0')
        p.pixels([(20, 13), (21, 11), (22, 9)], shell[4])
        # the overhanging lip, the eye-slit beneath
        p.fill(p.poly([(16, 26), (48, 26), (45, 35), (19, 35)]), ramp('#160c1c'), flat=2, edge=False)
        p.fill(p.poly([(14, 25), (50, 25), (48, 28), (40, 30), (32, 30), (24, 30), (16, 28)]), shell, radius=2, bias=0.1)
        for side in (-1, 1):
            p.fill(p.poly([(32 + side * 14, 26), (32 + side * 18, 26), (32 + side * 16, 35)]), shell, radius=1.5, bias=-0.1)
        for k, (ex, ey, r) in enumerate(((21, 32, 0), (26, 33, 1), (32, 33, 1), (38, 33, 1), (43, 32, 0))):
            if blink(t, (0.11 + k * 0.19) % 1, 1):
                p.px(ex, ey, '#6a1a0a')
                continue
            p.glow(ex, ey, '#fff0b0', mix('#ff4a10', '#ff8a2a', pulse(t, k * 0.3, 2)), r=r, strength=0.85)
        # feeding tentacles, writhing
        for k, (x0, ln, ph) in enumerate(((24, 14, 0.0), (27, 18, 0.3), (30, 21, 0.6), (33, 22, 0.15), (36, 19, 0.45),
                                          (39, 16, 0.75), (22, 10, 0.9), (41, 11, 0.2))):
            s1 = sway(t, ph, 1, cycles=1)
            s2 = sway(t, ph + 0.25, 2, cycles=1)
            curl = 1 if k % 2 else -1
            pts = [(x0, 35), (x0 + s1 * 0.5, 35 + ln * 0.4), (x0 + s1, 35 + ln * 0.75), (x0 + s2 + curl, 35 + ln)]
            p.stroke(pts, flesh[1], width=2)
            p.stroke(pts[:3], flesh[3] if x0 < 33 else flesh[2])
            p.pixels([(int(x0 + s2 + curl), 35 + ln), (int(x0 + s2 + curl * 2), 34 + ln)], '#e8789a')
    with p.at(0, body):
        # sickle claws raised from below, snapping now and then
        for side, at in ((-1, 0.45), (1, 0.62)):
            snap = blink(t, at, 3)
            q = lambda pts: [(32 + side * (32 - x), y) for x, y in pts]  # noqa: E731  (drawn for the left, mirrored for the right)
            bs = 0.05 if side < 0 else -0.18
            p.fill(p.poly(q([(-1, 66), (-1, 45), (4, 42), (11, 46), (13, 55), (10, 66)])), claw, radius=4, bias=bs, texture=0.04,
                   seed=93)
            p.stroke(q([(1, 47), (5, 44), (9, 46)]), claw[4] if side < 0 else claw[3])
            p.stroke(q([(11, 52), (10, 60)]), claw[1])
            # the lower, fixed blade
            p.fill(p.poly(q([(8, 60), (13, 54), (18, 50), (25, 47), (20, 53), (15, 58), (10, 63)])), claw, radius=2, bias=bs + 0.05)
            # the upper, moving blade
            if snap:
                mv = [(2, 46), (9, 43), (16, 43), (23, 46), (16, 47), (9, 48), (5, 51)]
            else:
                mv = [(2, 46), (6, 40), (12, 35), (20, 32), (15, 38), (10, 44), (5, 50)]
            p.fill(p.poly(q(mv)), claw, radius=2, bias=bs)
            # glowing inner edges, dark tips
            p.stroke(q([mv[6], mv[5], mv[4]]), '#ff6a2a')
            p.stroke(q([(11, 59), (15, 56), (20, 52)]), '#ff6a2a')
            p.pixels(q([mv[3], (25, 47), (24, 47)]), '#3a1a14')
    p.rim('#46e0ff', side=1, amount=0.5)
    p.rim('#ffb03a', side=-1, amount=0.3)
    p.outline('#0e0a0a')
    p.backdrop = (40, 18, 10)
    return p


# ---------------------------------------------------------------- Savages of Cymiae
def cymiae(t=0.0):
    """a feral brute with a mane of teal quills, short horns, coral war paint and a fanged snarl it can barely hold
    back; a spiked iron pauldron. Moves: the jaw trembles with rage, the eyes flare, the mane stirs, hot breath
    steams out, a vein throbs at the temple."""
    p = Portrait()
    mane = ramp('#21857e', spread=0.8, hue_shift=0.05)
    skin = ramp('#71906a', spread=0.7, hue_shift=0.05)
    horn = ramp('#d8c8a0', spread=0.6)
    iron = ramp('#5c5864', spread=0.75)
    paint = '#ff5a3c'
    body, head = breath(t, amp=1), breath(t, -0.1)
    rage = pulse(t, 0.0, 2)
    shake = sway(t, 0.0, 1, cycles=6) if blink(t, 0.25, 4) or blink(t, 0.7, 3) else 0
    with p.at(0, head):
        # the mane: a dark mass, thick quill-locks flung back and out
        p.fill(p.poly([(10, 40), (6, 26), (10, 12), (20, 4), (32, 2), (44, 4), (54, 12), (58, 26), (54, 40), (32, 30)]),
               ramp('#14504c', spread=0.7), radius=5, texture=0.1, seed=99)
        locks = ((23, 14, 3, 2, 5), (29, 10, 14, -3, 5), (35, 10, 50, -3, 5), (41, 14, 61, 2, 5), (20, 22, 0, 17, 5),
                 (44, 22, 64, 17, 5), (20, 30, 1, 33, 5), (44, 30, 63, 33, 5), (32, 9, 32, -4, 4), (26, 12, 20, -4, 4),
                 (38, 12, 44, -4, 4), (21, 26, 2, 26, 4), (43, 26, 62, 26, 4))
        for k, (bx, by, tx, ty, w) in enumerate(locks):
            s = sway(t, k * 0.13, 1)
            tip = (tx + s, ty + abs(s))
            dx, dy = tip[0] - bx, tip[1] - by
            ln = math.hypot(dx, dy)
            nx, ny = -dy / ln * w / 2, dx / ln * w / 2
            mid = (bx + dx * 0.55, by + dy * 0.55)
            p.fill(p.poly([(bx - nx, by - ny), (mid[0] - nx * 0.8, mid[1] - ny * 0.8), tip, (mid[0] + nx * 0.8, mid[1] + ny * 0.8),
                           (bx + nx, by + ny)]), mane, radius=2.5, bias=0.05 if bx < 32 else -0.1, texture=0.06, seed=100 + k)
            p.stroke([(bx + nx * 0.3, by + ny * 0.3), (mid[0], mid[1])], mane[4] if bx <= 32 else mane[3])
            p.px(int(round(mid[0] + dx * 0.15)), int(round(mid[1] + dy * 0.15)), mane[0])
    with p.at(0, body):
        # hunched shoulders: one under the falling mane, one in a spiked iron pauldron
        p.fill(p.poly([(0, 66), (0, 46), (8, 38), (18, 36), (32, 42), (46, 36), (56, 38), (64, 46), (64, 66)]), skin, radius=8,
               texture=0.07, seed=101)
        p.stroke([(10, 52), (16, 60)], skin[1])
        for k, (x0, y0, x1, y1) in enumerate(((3, 38, 1, 60), (8, 36, 6, 58), (13, 36, 12, 54), (18, 38, 18, 50))):
            s = sway(t, 0.2 + k * 0.15, 1)
            p.fill(p.poly([(x0 - 2, y0), (x0 + 3, y0), (x1 + 2 + s, y1 - 3), (x1 + s, y1), (x1 - 2 + s, y1 - 3)]), mane, radius=2,
                   bias=0.05, texture=0.06, seed=110 + k)
            p.stroke([(x0, y0 + 1), (x1 + s, y1 - 3)], mane[3])
        for sx, sy, tx, ty in ((48, 39, 49, 28), (56, 41, 61, 31), (42, 42, 38, 33)):
            p.fill(p.poly([(sx - 2.5, sy), (tx, ty), (sx + 2.5, sy)]), iron, radius=1.5, bias=0.1)
            p.px(tx, ty + 1, iron[4])
        p.fill(p.poly([(64, 66), (64, 46), (59, 40), (49, 37), (41, 41), (40, 50), (46, 58), (56, 62), (60, 66)]), iron, radius=6,
               texture=0.05, seed=103, bias=-0.05)
        p.stroke([(41, 48), (49, 44), (60, 46)], iron[1])
        p.stroke([(42, 47), (49, 43), (59, 44)], iron[3])
        p.pixels([(46, 51), (54, 53), (50, 58)], iron[4])
        p.pixels([(47, 52), (55, 54), (51, 59)], iron[0])
        p.stroke([(43, 55), (50, 60)], ramp('#8a3a20')[2])
        # neck, thick
        p.fill(p.poly([(23, 38), (41, 38), (42, 50), (22, 50)]), skin, radius=4, bias=-0.3)
        p.stroke([(26, 42), (28, 49)], skin[1])
        p.stroke([(38, 42), (36, 49)], skin[1])
        # a trophy necklace of fangs and a small skull
        p.stroke([(19, 47), (25, 53), (32, 55), (39, 53), (45, 47)], '#4a2a1a')
        for k, (x, y) in enumerate(((21, 49), (24, 52), (28, 54), (36, 54), (40, 52), (43, 49))):
            p.pixels([(x, y + 1), (x, y + 2)], '#e8dcc0')
            p.px(x, y + 3, '#b8a888')
        p.fill(p.ellipse(32, 57.5, 3.0, 3.2), ramp('#e0d4b8', spread=0.5), radius=2)
        p.pixels([(31, 57), (33, 57)], '#2a1010')
        p.pixels([(31, 60), (33, 60)], '#5a4a3a')
    with p.at(0, head):
        # horns
        for side in (-1, 1):
            hp = [(32 + side * 7, 13), (32 + side * 12, 8), (32 + side * 14, 2), (32 + side * 15, 7), (32 + side * 12, 14)]
            p.fill(p.poly(hp), horn, radius=2, bias=0.1 if side < 0 else -0.15)
            p.stroke([(32 + side * 9, 12), (32 + side * 12, 9)], horn[1])
            p.px(32 + side * 14, 3, horn[0])
        # the head: wide, heavy-jawed, lowered
        hd = p.poly([(21, 15), (26, 11), (38, 11), (43, 15), (46, 23), (45, 32), (42, 39), (37, 44), (32, 45), (27, 44), (22, 39),
                     (19, 32), (18, 23)])
        p.fill(hd, skin, radius=8, texture=0.09, seed=104)
        # war paint: a stripe down the brow, claw-marks on the cheeks
        p.stroke([(32, 12), (32, 20)], paint, width=2)
        for side in (-1, 1):
            for k in range(3):
                p.stroke([(32 + side * (8 + k * 2), 28 + k), (32 + side * (10 + k * 2), 33 + k)], mix(paint, '#a02010', 0.2 * k))
        # a heavy furrowed brow
        p.fill(p.poly([(19, 19), (25, 17), (31, 22), (33, 22), (39, 17), (45, 19), (44, 23), (33, 25), (31, 25), (20, 23)]), skin,
               radius=2, bias=0.15)
        p.stroke([(20, 23), (31, 25)], skin[0])
        p.stroke([(44, 23), (33, 25)], skin[0])
        p.stroke([(30, 17), (31, 21)], skin[1])
        p.stroke([(34, 17), (33, 21)], skin[1])
        # a throbbing vein at the temple
        vk = pulse(t, 0.1, 3)
        p.stroke([(21, 13), (22, 16), (20, 19)], mix(skin[2], '#c04a5a', vk))
        # deep-set burning eyes
        for ex, d in ((26, 1), (38, -1)):
            p.fill(p.ellipse(ex + 0.5, 25.5, 3.4, 1.8), skin, flat=0, edge=False)
            p.pixels([(ex - 2 * d, 25), (ex - d, 25), (ex, 26), (ex + d, 26)], mix('#ff9a10', '#ffd040', rage))
            p.pixels([(ex - d, 25), (ex, 26)], '#fff6c0')
            p.glow(ex, 26, '#ffffff', '#ffb020', r=1, strength=0.35 + 0.5 * rage)
        # a wrinkled snout, nostrils flared
        p.stroke([(30, 27), (31, 30)], skin[1])
        p.stroke([(34, 27), (33, 30)], skin[1])
        p.fill(p.poly([(28, 30), (36, 30), (37, 33), (27, 33)]), skin, radius=2, bias=0.1)
        p.pixels([(29, 32), (30, 32), (34, 32), (35, 32)], '#1a1410')
        p.stroke([(27, 29), (25, 31)], skin[1])
        p.stroke([(37, 29), (39, 31)], skin[1])
    with p.at(shake, head):
        # the snarl: gums, fangs, a trembling jaw
        p.fill(p.poly([(23, 35), (41, 35), (39, 42), (32, 44), (25, 42)]), ramp('#2a0a10'), flat=1, edge=False)
        p.stroke([(23, 35), (28, 34), (32, 35), (36, 34), (41, 35)], '#9a2030')
        for x in range(25, 40, 2):
            ln = 3 if x in (25, 37) else 1
            p.stroke([(x, 36), (x, 36 + ln - 1)], '#f0e6c8')
            p.stroke([(x + 1, 36), (x + 1, 36 + max(0, ln - 2))], '#c8b898')
        for x in range(26, 39, 2):
            ln = 3 if x in (26, 36) else 1
            p.stroke([(x, 42 - ln + 1), (x, 42)], '#e0d4b4')
        p.stroke([(25, 42), (32, 44), (39, 42)], '#7a1a24')
        p.stroke([(23, 35), (25, 42)], skin[0])
        p.stroke([(41, 35), (39, 42)], skin[0])
        # wiry beard-tendrils
        for k, (x0, ln) in enumerate(((26, 6), (29, 8), (32, 9), (35, 8), (38, 6))):
            s = sway(t, k * 0.2, 1)
            p.stroke([(x0, 44), (x0 + s, 44 + ln)], mane[1])
            p.px(x0 + s, 44 + ln, mane[3])
    p.rim('#ffb03a', side=1, amount=0.5)
    p.rim('#3af0d0', side=-1, amount=0.3)
    p.outline('#100606')
    # hot breath steaming from the snarl
    for k, ph in enumerate((0.0, 0.33, 0.66)):
        u = (t * 1.5 + ph) % 1.0
        if u < 0.7:
            x = 32 + int(round((k - 1) * 6 * u)) + sway(t, ph, 1, 2)
            y = int(round(46 + u * 10))
            c = mix('#e8e0d8', '#5a2020', u / 0.7)
            p.px(x, y, c)
            if u < 0.4:
                p.px(x + 1, y, c)
    p.backdrop = (58, 8, 10)
    return p


# ---------------------------------------------------------------- Dih-Mohn Flotilla
def dihmohn(t=0.0):
    """a sleek nomad-mystic in pale living armour, three-quarter turned: a long white skull trailing tubes, violet
    sinew glowing through the plates, calm, wary eyes. Moves: the tubes drift, chin-tendrils sway, the violet
    glow in the plates breathes, the eyes dim in a slow blink, motes of light float up."""
    p = Portrait()
    bone = ramp('#e2dcf0', spread=0.55, hue_shift=0.05)
    sinew = ramp('#8a3ad0', spread=0.7)
    deep = '#1a0a2a'
    glow1, glow2 = '#c070ff', '#f0d0ff'
    body, head = breath(t), breath(t, -0.1)
    gk = pulse(t, 0.0, 2)
    with p.at(0, head):
        # tubes trailing from the back of the skull
        for k, pts in enumerate(([(27, 4), (19, 0), (8, -2)], [(20, 6), (10, 7), (1, 13)], [(17, 12), (7, 18), (0, 29)],
                                 [(18, 18), (9, 27), (3, 41)], [(22, 24), (14, 34), (9, 50)], [(26, 28), (21, 38), (19, 47)])):
            s1, s2 = sway(t, k * 0.17, 1), sway(t, k * 0.17 + 0.2, 2)
            q = [pts[0], (pts[1][0] + s1, pts[1][1]), (pts[2][0] + s2, pts[2][1])]
            r_ = bone if k % 2 == 0 else sinew
            p.stroke(q, r_[0], width=4)
            p.stroke(q, r_[2] if k > 2 else r_[3], width=2)
            p.stroke(q[:2], r_[4] if k < 3 else r_[3], width=1)
            p.px(int(round(q[2][0])), int(round(q[2][1])), glow1 if k % 2 == 0 else glow2)
    with p.at(0, body):
        # violet sinew under the plates
        p.fill(p.poly([(0, 66), (0, 50), (10, 44), (24, 42), (34, 44), (46, 43), (56, 46), (64, 52), (64, 66)]), sinew, radius=6,
               texture=0.1, seed=121)
        for x in range(4, 62, 3):
            p.stroke([(x, 50), (x + 1, 66)], sinew[0] if x % 2 else sinew[3])
        # the neck: a column of fibres
        p.fill(p.poly([(29, 32), (39, 34), (42, 50), (26, 50)]), sinew, radius=4, bias=-0.1)
        for x in (30, 33, 36, 39):
            p.stroke([(x, 34), (x - 1, 50)], sinew[3] if x < 36 else sinew[1])
        # pale armour plates, layered like shell: the near shoulder, the chest keel, the far shoulder
        def vent(pts):
            p.stroke(pts, deep)
            mx, my = pts[len(pts) // 2]
            p.px(mx, my, mix(sinew[3], glow2, gk))
            p.px(pts[0][0], pts[0][1], mix(sinew[1], glow1, gk))
        p.fill(p.poly([(0, 66), (0, 54), (8, 49), (20, 48), (27, 52), (22, 59), (12, 63), (6, 66)]), bone, radius=5, bias=-0.1,
               texture=0.02, seed=124)
        p.fill(p.poly([(0, 52), (3, 44), (12, 39), (22, 40), (28, 45), (21, 48), (10, 49), (0, 57)]), bone, radius=5, texture=0.02,
               seed=122)
        p.stroke([(3, 46), (11, 41), (21, 41)], bone[4])
        vent([(7, 46), (11, 45), (15, 45), (19, 44)])
        vent([(6, 56), (11, 54), (16, 53), (21, 53)])
        p.fill(p.poly([(26, 66), (27, 55), (32, 50), (38, 50), (43, 55), (45, 66)]), bone, radius=4, bias=-0.05)
        p.stroke([(35, 51), (35, 66)], bone[1])
        p.stroke([(34, 52), (34, 66)], bone[4])
        for y in (57, 62):
            p.stroke([(28, y), (31, y - 2), (34, y - 1)], bone[1])
            p.stroke([(36, y - 1), (40, y - 2), (44, y)], bone[0])
        p.fill(p.poly([(64, 66), (64, 52), (58, 46), (49, 45), (45, 50), (48, 58), (56, 62)]), bone, radius=4, bias=-0.2)
        vent([(50, 51), (54, 51), (58, 52)])
    with p.at(0, head):
        # the long skull, three-quarter turned to the right
        hd = p.poly([(12, 10), (18, 4), (26, 1), (34, 2), (41, 6), (45, 11), (47, 16), (47, 20), (49, 25), (49, 30), (47, 35),
                     (44, 38), (40, 39), (36, 37), (31, 34), (25, 29), (18, 22), (13, 16)])
        p.fill(hd, bone, radius=10, texture=0.02, seed=123)
        # crest ridges running back over the skull
        p.stroke([(41, 7), (32, 4), (22, 5), (15, 10)], bone[4])
        p.stroke([(43, 11), (33, 8), (22, 10), (16, 15)], bone[1])
        p.stroke([(36, 13), (26, 14), (19, 19)], bone[1])
        # a split along the crest: the violet light inside
        p.stroke([(40, 6), (31, 6), (22, 8), (16, 13)], mix(sinew[2], glow1, gk))
        p.stroke([(33, 6), (26, 7)], mix(sinew[3], glow2, gk))
        # a brow ridge over the eye
        p.fill(p.poly([(32, 16), (40, 14), (47, 15), (47, 17), (40, 17), (33, 19)]), bone, radius=1.5, bias=0.2)
        # deep sockets, eyes of soft light
        shut = blink(t, 0.58, 2)
        p.fill(p.ellipse(38.5, 20, 4.2, 2.8), ramp(deep), flat=2, edge=False)
        p.fill(p.ellipse(46.2, 19.5, 1.4, 2.4), ramp(deep), flat=2, edge=False)
        p.stroke([(34, 17), (39, 16), (43, 18)], bone[0])
        if not shut:
            p.pixels([(37, 20), (38, 20), (39, 20)], mix('#6ac8ff', '#c8f6ff', gk))
            p.px(38, 20, '#ffffff')
            p.glow(38, 20, '#ffffff', '#8adcff', r=1, strength=0.35 + 0.3 * gk)
            p.px(46, 20, '#8adcff')
        else:
            p.stroke([(37, 20), (39, 20)], '#3a5a8a')
        # a sunken cheek below the ridge, glowing gill-slits in it, the muzzle with its breathing holes
        p.fill(p.poly([(30, 27), (40, 28), (45, 32), (44, 35), (38, 34), (31, 31)]), bone, flat=1, edge=False)
        p.stroke([(31, 31), (38, 34), (44, 35)], bone[0])
        p.stroke([(31, 26), (39, 26), (45, 29), (47, 33)], bone[4])
        for k in range(3):
            x = 31 + k * 3
            gc = mix(sinew[2], glow1, pulse(t, k * 0.2, 2))
            p.pixels([(x, 28 + k // 2), (x + 1, 29 + k // 2)], gc)
            p.px(x + 2, 30 + k // 2, mix(gc, '#ffffff', 0.3))
        for x, y in ((46, 30), (47, 32)):
            p.px(x, y, deep)
        # a lipless mouth with small fangs
        p.stroke([(38, 36), (43, 37), (47, 36)], deep)
        p.pixels([(41, 37), (44, 37)], '#ffffff')
        p.pixels([(39, 37)], bone[1])
        # violet sinew showing under the jaw
        p.stroke([(30, 33), (36, 36)], sinew[2], width=2)
        # chin tendrils
        for k, (x0, ln) in enumerate(((43, 9), (46, 7))):
            s = sway(t, 0.3 + k * 0.3, 1)
            p.stroke([(x0, 38), (x0 + s * 0.5, 38 + ln * 0.6), (x0 + s, 38 + ln)], bone[2])
            p.px(x0 + s, 38 + ln, glow1)
    p.rim('#ff5ad8', side=1, amount=0.45)
    p.rim('#5ae8ff', side=-1, amount=0.3)
    p.outline('#0e0618')
    # motes of light floating up
    for x0, ph in ((52, 0.0), (58, 0.4), (22, 0.7), (48, 0.25)):
        u = (t + ph) % 1.0
        if u < 0.8:
            p.px(x0 + sway(t, ph, 1, 2), int(round(44 - u * 34)), mix(glow2, '#3a1a6a', u))
    p.backdrop = (24, 10, 38)
    return p


ALL = {'crimson': crimson, 'firmament': firmament, 'obsidian': obsidian, 'axis': axis, 'bentor': bentor, 'celdauri': celdauri, 'cheiran': cheiran, 'cymiae': cymiae, 'dihmohn': dihmohn}

if __name__ == '__main__':
    from PIL import Image, ImageDraw
    D = __import__('os').path.dirname(__import__('os').path.abspath(__file__)) + '/'
    keys = list(ALL)
    cols = 3
    cw, ch = 2 * 256 + 30, 256 + 34
    rows = (len(keys) + cols - 1) // cols
    sheet = Image.new('RGBA', (cols * cw + 10, rows * ch + 10), (255, 255, 255, 255))
    dr = ImageDraw.Draw(sheet)
    for i, k in enumerate(keys):
        x0, y0 = 10 + (i % cols) * cw, 10 + (i // cols) * ch
        for j, t in enumerate((0.0, 0.5)):
            pp = ALL[k](t)
            sheet.alpha_composite(pp.image(4, bg=tuple(pp.backdrop) + (255,)), (x0 + j * 262, y0))
        dr.text((x0, y0 + 260), k, fill=(0, 0, 0, 255))
    sheet.save(D + 'sheet_faces_b4.png')
    for k, f in ALL.items():
        save_gif(f, D + f'anim_{k}.gif', scale=4)
