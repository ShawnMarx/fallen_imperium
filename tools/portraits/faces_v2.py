"""Round 2 portraits: drawn after the owner's own reference pictures (the owner's picture folder, 
New Assets/Use for Faction Portrait), two at a time."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from pix import (Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave, bezier, ribbon,  # noqa: E402
                 rot_ellipse, FRAMES)

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0


def painted_bg(p, base, glow, cx, cy, r, steps=4, extra=None):
    """a dark backdrop with a soft dithered glow (cx, cy, r) - posterised into a few steps like the figures"""
    yy, xx = np.mgrid[0:p.h, 0:p.w]
    d = np.sqrt((xx + 0.5 - cx) ** 2 + (yy + 0.5 - cy) ** 2) / r
    k = np.clip(1 - d, 0, 1) ** 1.4
    if extra is not None:
        k = np.clip(k + extra, 0, 1)
    k = np.floor(k * steps + BAYER[yy % 4, xx % 4]) / steps
    b, g = np.array(hexrgb(base) if isinstance(base, str) else base, float), np.array(hexrgb(glow) if isinstance(glow, str) else glow, float)
    p.bg = (b + (g - b) * k[..., None]).clip(0, 255).astype(np.uint8)
    p.backdrop = tuple(int(v) for v in (hexrgb(base) if isinstance(base, str) else base))


def shade(p, mask, ramp_, lam, levels=(0.2, 0.42, 0.62, 0.82), edge=True, dither=0.0):
    """paint mask from a custom light value array lam (0..1) through a ramp"""
    mask = mask.copy()
    if not mask.any():
        return mask
    if dither:
        n = len(ramp_)
        yy, xx = np.mgrid[0:p.h, 0:p.w]
        idx = np.clip(np.floor(lam * n + (BAYER[yy % 4, xx % 4] - 0.5) * dither), 0, n - 1).astype(int)
    else:
        idx = np.clip(np.digitize(lam, levels), 0, len(ramp_) - 1)
    cols = np.array(ramp_, np.uint8)[idx]
    p.rgb[mask] = cols[mask]
    p.a |= mask
    if edge:
        from scipy import ndimage
        rim = mask & ~ndimage.binary_erosion(mask)
        p.rgb[rim] = np.array(ramp_[0], np.uint8)
    return mask


def ribbed_bud(p, cx, cy, rx, ry, ang, stem, ribs, ramp_, rib_amp=0.45, seed=3, levels=None):
    """an egg-shaped bud lit like a sphere, with ribs fanning from the stem point"""
    from pix import LIGHT
    cx, cy = cx + p.dx, cy + p.dy
    sx, sy = stem[0] + p.dx, stem[1] + p.dy
    yy, xx = p.yy + 0.5, p.xx + 0.5
    c, s_ = math.cos(ang), math.sin(ang)
    lx, ly = (xx - cx) * c + (yy - cy) * s_, -(xx - cx) * s_ + (yy - cy) * c
    u, v = lx / rx, ly / ry
    r2 = u * u + v * v
    mask = r2 <= 1.0
    nz = np.sqrt(np.clip(1 - r2, 0, 1))
    nx, ny = u * c - v * s_, u * s_ + v * c
    phi = np.arctan2(yy - sy, xx - sx)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    lam = (nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]) / ln
    lam = 0.5 + (lam - 0.45) * 0.95
    # ribs: thin dark grooves with a lit edge beside each, fading toward the stem
    dist = np.sqrt((xx - sx) ** 2 + (yy - sy) ** 2)
    fade = np.clip((dist - 3) / 8, 0, 1)
    g = np.cos(phi * ribs)
    lam = lam - rib_amp * 0.5 * np.clip((g - 0.55) / 0.45, 0, 1) * fade
    lam = lam + rib_amp * 0.3 * np.clip((np.cos(phi * ribs - 0.9) - 0.7) / 0.3, 0, 1) * fade
    return shade(p, mask, ramp_, np.clip(lam, 0, 0.999), dither=0.8)


def bg_px(p, x, y, col):
    if p.bg is not None and 0 <= x < p.w and 0 <= y < p.h:
        p.bg[int(y), int(x)] = hexrgb(col) if isinstance(col, str) else col


# ---------------------------------------------------------------------------------------------------------------
def two_tone_leaf(p, t, base, ctrl, tip, hook, w, outer, inner, ph, amp=1, bias=0.0, side=1, dark_tip=None):
    """a broad twisting leaf: narrow at the base, wide in the middle, hooked at the tip; the outer (back) face in
    `outer`, a lighter inner face along one side in `inner`; the tip sways"""
    s = sway(t, ph, amp) if amp else 0
    tipx = (tip[0] + s, tip[1] + s * 0.5)
    hk = (hook[0] + s, hook[1] + s * 0.5)
    path = bezier([base, ctrl, tipx, hk], 30)
    p.fill(p.poly(ribbon(path, w * 0.3, 0.7, w)), outer, radius=2.5, bias=bias - 0.05, texture=0.03,
           seed=int(base[0] * 7 + base[1]), depth=3.2)
    # the inner face: a narrower band shifted to one side over the first 3/4
    n = len(path)
    inner_path = []
    for i, (x, y) in enumerate(path[:int(n * 0.8)]):
        a, b2 = path[max(0, i - 1)], path[min(n - 1, i + 1)]
        dx, dy = b2[0] - a[0], b2[1] - a[1]
        L = math.hypot(dx, dy) or 1
        u = i / n
        off = side * w * 0.22 * math.sin(math.pi * min(1, u * 1.25))
        inner_path.append((x - dy / L * off, y + dx / L * off))
    if len(inner_path) > 3:
        p.fill(p.poly(ribbon(inner_path, w * 0.15, 0.6, w * 0.6)), inner, radius=2.0, bias=bias + 0.12, edge=False,
               depth=3.0)
        # streaks along the inner face
        for j in range(2, len(inner_path) - 3, 4):
            x, y = inner_path[j]
            p.px(round(x), round(y), inner[1])
    if dark_tip is not None:
        p.fill(p.poly(ribbon(path[int(n * 0.72):], w * 0.55, 0.6)), dark_tip, radius=1.2, edge=False)


def arborec(t=0.0):
    """after the owner's picture: a veined pod head ringed by curling leaves, a crinkled snout with drooping
    tendrils, on a twisted stalk wrapped in vines. Moves: leaves sway, tendrils curl, the pod breathes, spores."""
    p = Portrait()
    painted_bg(p, (16, 32, 22), (92, 120, 74), 16, 10, 52, steps=6)
    for x0, ph in ((7, 0.0), (59, 0.4), (52, 0.7)):
        for y in range(28, 64):
            x = x0 + int(round(2.2 * math.sin(y / 6.0 + ph * 6))) + sway(t, ph, 1)
            bg_px(p, x, y, (34, 60, 38))
    pod_g = ramp('#7d9032', spread=0.85, hue_shift=0.06)
    yel_o = ramp('#6a4622', spread=0.7, hue_shift=0.05)
    yel_i = ramp('#a88a3a', spread=0.55, hue_shift=0.05)
    dk_o = ramp('#2e1e22', spread=0.7)
    dk_i = ramp('#5a3e34', spread=0.6)
    tipc = ramp('#3a2420', spread=0.5)
    skin = ramp('#b58466', spread=0.62, hue_shift=0.04)
    stalk = ramp('#6a6a36', spread=0.75)
    vine = ramp('#3f6a2c', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the twisted stalk rising from the lower left, wrapped in vines
        st = bezier([(18, 68), (16, 58), (34, 60), (30, 46)], 22)
        p.fill(p.poly(ribbon(st, 13, 8)), stalk, radius=5, texture=0.05, depth=5)
        for k in range(5):
            y0 = 48 + k * 4
            p.stroke([(19 + k, y0 + 4), (27, y0 + 1), (35, y0 - 1)], vine[1 + (k % 2)])
        two_tone_leaf(p, t, (30, 52), (40, 54), (52, 56), (47, 62), 6, dk_o, dk_i, 0.55, side=-1)
        for x0, ph in ((5, 0.1), (48, 0.3), (57, 0.8)):
            pts = [(x0, 66), (x0 + 2 + sway(t, ph, 1), 58), (x0 + sway(t, ph + 0.2, 1), 51)]
            p.stroke(pts, vine[2])
    with p.at(0, head):
        # leaves behind the pod
        two_tone_leaf(p, t, (45, 17), (55, 6), (60, 1), (52, 1), 10, dk_o, dk_i, 0.0, bias=-0.05, side=-1, dark_tip=tipc)
        two_tone_leaf(p, t, (48, 26), (61, 24), (64, 37), (56, 41), 9, dk_o, dk_i, 0.3, bias=-0.1, side=-1, dark_tip=tipc)
        two_tone_leaf(p, t, (30, 12), (22, 1), (9, 3), (8, 12), 6, yel_o, yel_i, 0.15, dark_tip=tipc)
        two_tone_leaf(p, t, (38, 8), (35, 0), (44, -2), (48, 3), 5, yel_o, yel_i, 0.45, bias=0.1)
        two_tone_leaf(p, t, (26, 26), (11, 21), (1, 29), (6, 39), 6.5, yel_o, yel_i, 0.65, dark_tip=tipc)
        # the pod: a tilted bud, ribbed
        cx, cy = 37, 21
        pod_r = [hexrgb(c) for c in ('#1c2210', '#313c18', '#4a5820', '#66762a', '#859434', '#a8ae44', '#ccc662')]
        ribbed_bud(p, cx, cy, 16.5, 14.5, -0.5, (25, 34), 26, pod_r)
        # crown light: a faint seam that pulses
        k = pulse(t, 0.0, 1)
        for x, y in ((40, 9), (42, 10), (44, 11), (46, 13)):
            p.px(x, y, mix(pod_g[3], '#f0ff9a', 0.25 + 0.55 * k))
        # a dark leaf folding over the pod's lower right (in front)
        two_tone_leaf(p, t, (44, 28), (54, 32), (54, 42), (47, 44), 7, dk_o, dk_i, 0.8, side=-1, dark_tip=tipc)
        # the snout: a crinkled trunk hanging from the stem end, flaring into a lip
        sn = [(23, 28), (31, 28), (35, 32), (33, 37), (29, 41), (27, 45), (21, 48), (16, 46), (18, 40), (20, 33)]
        p.fill(p.poly(sn), skin, radius=4.5, texture=0.03, depth=5)
        # crinkles: short uneven folds across the trunk
        for (x0, y0, x1, y1, c) in ((22, 31, 30, 30, 1), (24, 33, 33, 33, 2), (21, 35, 27, 35, 1), (28, 36, 32, 35, 1),
                                    (20, 38, 26, 37, 2), (27, 39, 30, 38, 1), (19, 41, 24, 40, 1), (24, 42, 28, 42, 2),
                                    (18, 44, 23, 43, 1)):
            p.stroke([(x0, y0), ((x0 + x1) // 2, y0 - 1), (x1, y1)], skin[c])
            p.px((x0 + x1) // 2, y0 - 2, skin[4])
        # the lip and its dark mouth
        p.fill(p.poly([(14, 45), (20, 47), (27, 44), (27, 48), (21, 51), (14, 49)]), skin, radius=1.5, bias=0.1, depth=2)
        p.stroke([(16, 48), (21, 49), (26, 47)], dk_o[0])
        # drooping tendrils from the lip, curling
        c1, c2, c3 = sway(t, 0.25, 1), sway(t, 0.7, 1), sway(t, 0.45, 1)
        t1 = bezier([(15, 47), (9, 48), (5 + c1, 44), (6 + c1, 39), (10 + c1, 41)], 24)
        p.fill(p.poly(ribbon(t1, 1.9, 0.7)), skin, radius=1, bias=0.05)
        t2 = bezier([(19, 50), (17, 58), (9 + c2, 60), (6 + c2, 55), (10 + c2, 54)], 24)
        p.fill(p.poly(ribbon(t2, 2.0, 0.7)), skin, radius=1, bias=-0.05)
    p.rim('#e0ff90', side=-1, amount=0.35)
    p.rim('#70ffd0', side=1, amount=0.3)
    p.outline('#0a120c')
    for x0, ph in ((10, 0.0), (20, 0.35), (50, 0.6), (58, 0.15), (44, 0.85), (6, 0.5)):
        u = (t + ph) % 1.0
        x = x0 + sway(t, ph, 1, 2)
        y = int(round(62 - u * 50))
        if u < 0.9:
            p.px(x, y, mix('#f0ffb0', '#6a8a40', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def axis(t=0.0):
    """after the owner's picture: an insect shipwright-lord with long antennae, burning amber eyes at the top of a
    tapering plated head, a ribbed throat, white collar plates and gold-trimmed dark armour, hands clasped, before
    a great machine ring. Moves: antennae sway, eyes glow, mandibles twitch, lights run round the ring, breathing."""
    p = Portrait()
    painted_bg(p, (8, 20, 24), (40, 84, 90), 32, 26, 36, steps=6)
    chitin = ramp('#a8642a', spread=0.8, hue_shift=0.06)
    gold = ramp('#c08a2c', spread=0.7, hue_shift=0.05)
    bone = ramp('#cdc8bc', spread=0.5)
    armour = ramp('#3c3634', spread=0.7)
    steel = ramp('#4a7880', spread=0.65)
    neck = ramp('#5a4434', spread=0.65)
    body, head = breath(t), breath(t, -0.1)
    # the ring throne behind: two thin concentric rings tied by blocky joints
    for r_, w_ in ((26.5, 1.2), (23.6, 1.0)):
        o = p.poly([(32 + (r_ + w_) * math.cos(2 * math.pi * i / 96), 33 + (r_ + w_) * math.sin(2 * math.pi * i / 96)) for i in range(96)])
        n = p.poly([(32 + (r_ - w_) * math.cos(2 * math.pi * i / 96), 33 + (r_ - w_) * math.sin(2 * math.pi * i / 96)) for i in range(96)])
        p.fill(o & ~n, steel, radius=1, bias=0.12, depth=1.5)
    for a in (math.pi, 0.0, -math.pi / 2, math.pi * 0.78, math.pi * 0.22, -math.pi * 0.82, -math.pi * 0.18, -math.pi * 0.62,
              -math.pi * 0.38):
        cx, cy = 32 + 25 * math.cos(a), 33 + 25 * math.sin(a)
        ca, sa = math.cos(a), math.sin(a)
        blk = [(cx + 3 * ca - 1.6 * -sa, cy + 3 * sa - 1.6 * ca), (cx + 3 * ca + 1.6 * -sa, cy + 3 * sa + 1.6 * ca),
               (cx - 3 * ca + 1.6 * -sa, cy - 3 * sa + 1.6 * ca), (cx - 3 * ca - 1.6 * -sa, cy - 3 * sa - 1.6 * ca)]
        p.fill(p.poly(blk), steel, radius=1.2, bias=0.05, depth=2)
    for j in range(2):  # lights running round the ring
        a = 2 * math.pi * (t + j * 0.5) - math.pi / 2
        for k, f in ((0, 1.0), (1, 0.55), (2, 0.25)):
            aa = a - k * 0.07
            p.px(round(32 + 26.5 * math.cos(aa)), round(33 + 26.5 * math.sin(aa)), mix(steel[3], '#d8ffff', f))
    with p.at(0, body):
        # shoulders, torso: dark plate, bronze trim, a gold chest plate
        p.fill(p.poly([(0, 66), (2, 47), (12, 41), (22, 41), (32, 45), (42, 41), (52, 41), (62, 47), (64, 66)]), armour,
               radius=6, texture=0.04, depth=5)
        for x0, d in ((5, 1), (55, -1)):
            p.stroke([(x0, 46), (x0 + 5 * d, 44)], gold[2])
            p.stroke([(x0, 49), (x0 + 5 * d, 47)], gold[1])
            p.stroke([(x0 - d, 52), (x0 + 4 * d, 50)], gold[1])
        p.fill(p.poly([(25, 45), (32, 48), (39, 45), (39, 56), (32, 60), (25, 56)]), gold, radius=4, depth=4)
        p.stroke([(32, 49), (32, 59)], gold[1])
        p.stroke([(27, 50), (32, 53), (37, 50)], gold[4])
        # forearms with segmented gauntlets, meeting at the clasped hands
        for side in (-1, 1):
            arm = bezier([(32 + side * 25, 62), (32 + side * 17, 56), (32 + side * 8, 57)], 12)
            p.fill(p.poly(ribbon(arm, 7, 6)), armour, radius=3, bias=0.05 if side < 0 else -0.1, depth=3)
            for k in range(3):
                x = 32 + side * (12 + k * 4)
                p.stroke([(x, 54), (x + side, 60)], armour[0])
                p.px(x - side, 55, gold[2])
        # the clasped hands: long knuckled fingers interlaced
        p.fill(p.ellipse(32, 58, 7, 4.6), neck, radius=3, texture=0.05, depth=3)
        for k, x in enumerate((27, 29, 31, 33, 35, 37)):
            p.stroke([(x, 55 + (k % 2)), (x, 61)], neck[0] if k % 2 else neck[1])
            p.px(x, 55 + (k % 2), neck[4] if k % 2 == 0 else gold[3])
        # the white collar plates: flared, spiked, standing up beside the throat
        p.fill(p.poly([(13, 44), (15, 34), (17, 30), (19, 33), (22, 26), (24, 32), (27, 36), (28, 43), (21, 47)]), bone,
               radius=3, texture=0.04, depth=3)
        p.fill(p.poly([(51, 44), (49, 34), (47, 30), (45, 33), (42, 26), (40, 32), (37, 36), (36, 43), (43, 47)]), bone,
               radius=3, bias=-0.12, texture=0.04, depth=3)
        p.stroke([(19, 35), (21, 44)], bone[1])
        p.stroke([(45, 35), (43, 44)], bone[1])
    with p.at(0, head + 3):
        # the ribbed throat: segments widening downward
        p.fill(p.poly([(28, 26), (36, 26), (38, 42), (26, 42)]), neck, radius=3, depth=3)
        for k, y in enumerate(range(28, 42, 2)):
            w = 4 + k // 2
            p.stroke([(32 - w, y), (32 + w, y)], chitin[2] if k % 2 == 0 else neck[0])
            if k % 2 == 0:
                p.px(32 - w + 1, y, chitin[4])
        # antennae from the brow: long, thin, curving out of the frame, swaying
        for side, ph in ((-1, 0.0), (1, 0.33)):
            sx_ = sway(t, ph, 2)
            path = bezier([(32 + side * 2, 7), (32 + side * 4, -1), (32 + side * 9 + sx_, -6), (32 + side * 16 + sx_, -12)], 24)
            pts = [(round(x), round(y)) for x, y in path]
            p.stroke(pts, chitin[1])
            p.pixels([(x, y) for x, y in pts[1:8]], chitin[3])
            p.pixels(pts[3::4], gold[4])
        # the head: wide across the eyes at the top, tapering in plates to the mouthparts
        hd = [(21, 11), (25, 6), (32, 5), (39, 6), (43, 11), (42, 17), (39, 22), (36, 28), (34, 32), (32, 33), (30, 32),
              (28, 28), (25, 22), (22, 17)]
        p.fill(p.poly(hd), chitin, radius=6, texture=0.04, depth=6)
        # plate seams down the face, brow ridge
        p.stroke([(32, 7), (32, 30)], chitin[1])
        p.pixels([(33, y) for y in range(8, 29, 2)], chitin[4])
        for y, w in ((15, 7), (19, 6), (23, 4), (27, 3)):
            p.stroke([(32 - w, y), (32 + w, y)], chitin[1])
            p.pixels([(32 - w + 1, y - 1), (32 + w - 2, y - 1)], chitin[4])
        # mouthparts: two small mandibles that twitch
        mo = 1 if (blink(t, 0.3, 2) or blink(t, 0.42, 1)) else 0
        p.fill(p.poly([(30, 30), (32, 32), (31 - mo, 35), (29 - mo, 33)]), gold, radius=1, bias=0.1)
        p.fill(p.poly([(34, 30), (32, 32), (33 + mo, 35), (35 + mo, 33)]), gold, radius=1, bias=-0.1)
        # the eyes: big glowing amber lenses at the top corners, in brass cups
        g = pulse(t, 0.0, 2)
        for cx in (23.5, 40.5):
            p.fill(p.ellipse(cx, 11, 5.3, 5.0), gold, radius=2.5, bias=-0.15, depth=3)
            lens = ramp(mix('#d07a10', '#ffa828', g), spread=0.55)
            p.fill(p.ellipse(cx, 11, 3.9, 3.7), lens, radius=3, bias=0.3, depth=4)
            ix = int(cx)
            p.glow(ix, 10, '#fffadc', mix('#ffb030', '#ffe080', g), r=1, strength=0.75 + 0.25 * g)
            p.px(ix - 1, 9, '#ffffff')
            p.px(ix + 2, 13, mix('#ff9a20', '#ffd060', g))
    p.rim('#5ae8f0', side=1, amount=0.45)
    p.rim('#ffb048', side=-1, amount=0.22)
    p.outline('#05090b')
    for x0, ph in ((12, 0.0), (52, 0.4), (19, 0.7), (46, 0.2)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(50 - u * 40)), mix('#c0ffff', '#1a5058', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def scaler(k, ax, ay):
    """point transform: scale by k about (ax, ay) - for enlarging a part drawn in older coordinates"""
    def f(x, y):
        return (ax + (x - ax) * k, ay + (y - ay) * k)

    def pts(lst):
        return [f(x, y) for x, y in lst]

    def ipts(lst):
        return [(int(round(ax + (x - ax) * k)), int(round(ay + (y - ay) * k))) for x, y in lst]
    return f, pts, ipts


def letnev(t=0.0):
    """after the owner's picture, front-on (owner: "a regular human with silver long hair and light grey-blue skin",
    "have him look straight"): a Letnev lord, ornate gold pauldrons, a tall gold crest rising behind him, gold rope
    braids on navy, a red nebula behind. Moves: breathing, hair stirs, the shoulder gem glows, a glint climbs the
    crest, a slow cold blink."""
    p = Portrait()
    painted_bg(p, (52, 8, 10), (214, 96, 34), 4, 0, 46, steps=6)
    rng = np.random.default_rng(5)
    for _ in range(18):
        x, y = int(rng.integers(0, 64)), int(rng.integers(0, 40))
        ph = float(rng.random())
        bg_px(p, x, y, mix((120, 40, 30), (255, 220, 180), pulse(t, ph, 1) if rng.random() < 0.5 else 0.5))
    skin = ramp('#91a5bd', spread=0.66, hue_shift=0.03)
    hair = ramp('#d2d8e2', spread=0.58, hue_shift=0.02)
    gold = ramp('#c38a2a', spread=0.8, hue_shift=0.06)
    navy = ramp('#22306a', spread=0.7)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the tall crest behind the left shoulder: two leaning gold bars with cut-outs, braced
        for x0, x1 in ((1, 7), (8, 14)):
            p.fill(p.poly([(x0, 50), (x0 + 5, 3), (x0 + 8, 0), (x1 + 1, 4), (x1 - 1, 50)]), gold, radius=2, depth=2.5)
            p.stroke([(x0 + 3, 46), (x0 + 7, 8)], gold[1])
        for y in (13, 26):
            p.fill(p.poly([(4 + y // 9, y), (16 + y // 9, y - 1), (16 + y // 9, y + 2), (4 + y // 9, y + 3)]), gold, radius=1,
                   depth=1.5, bias=0.1)
        gy = int(46 - (t % 1.0) * 44)
        for k in range(3):
            p.px(7 + (48 - gy) // 8 + k % 2, gy + k, mix(gold[4], '#fff6d0', 1 - k * 0.3))
        # the hair mass behind the head
        p.fill(p.poly([(21, 8), (24, 2), (32, -1), (40, 2), (43, 8), (45, 24), (41, 36), (23, 36), (19, 24)]), hair,
               radius=4, depth=3, bias=-0.4, texture=0.04)
        # pauldrons: curved overlapping gold scallops on both shoulders
        for side in (-1, 1):
            for k in range(3):
                x0, x1 = (0, 28) if side < 0 else (64, 36)
                arc = bezier([(x0, 46 + k * 5), (x0 + side * -9, 34 + k * 5), (x1 + side * 6, 35 + k * 5), (x1, 44 + k * 5)], 20)
                p.fill(p.poly(ribbon(arc, 4, 5, 8)), gold, radius=2.8, depth=3.5, bias=(-0.04 if side < 0 else -0.1) * (k + 1),
                       texture=0.03)
                p.stroke([(round(x), round(y) - 2) for x, y in arc[3:-3:2]], gold[4] if side < 0 else gold[3])
        # the navy tunic between them, gold rope braids and tassels
        p.fill(p.poly([(20, 66), (22, 50), (32, 47), (42, 50), (44, 66)]), navy, radius=5, depth=4)
        for (a2, c, b2) in (((14, 54), (32, 63), (50, 54)), ((16, 59), (32, 68), (48, 59))):
            rope = bezier([a2, c, b2], 30)
            for j, (x, y) in enumerate(rope):
                p.px(round(x), round(y), gold[3] if j % 2 == 0 else gold[1])
                p.px(round(x), round(y) - 1, gold[2] if j % 2 else gold[4])
        for tx, ty, ph in ((25, 53, 0.1), (39, 53, 0.6)):
            sw = sway(t, ph, 1)
            p.fill(p.poly([(tx - 1, ty), (tx + 1, ty), (tx + 2 + sw, ty + 7), (tx - 2 + sw, ty + 7)]), gold, radius=1.2, depth=2)
            p.stroke([(tx - 1 + sw, ty + 7), (tx + 1 + sw, ty + 7)], gold[4])
        gk = pulse(t, 0.2, 1)
        p.glow(50, 42, mix('#bfffb0', '#ffffff', gk), mix('#2a9a3a', '#7aff6a', gk), r=1, strength=0.6 + 0.4 * gk)
        arc = bezier([(64, 20), (57, 30), (58, 44), (64, 56)], 18)
        p.fill(p.poly(ribbon(arc, 2.4, 1.2, 3.0)), gold, radius=1.2, depth=2, bias=0.05)
    with p.at(0, head):
        # neck
        p.fill(p.poly([(28, 25), (36, 25), (37, 38), (27, 38)]), skin, flat=1, edge=True)
        p.fill(p.poly([(28, 25), (36, 25), (36, 31), (28, 31)]), skin, flat=0, edge=False)
        p.stroke([(34, 32), (35, 37)], skin[0])
        # ears, half hidden by the hair
        for ex in (23.2, 40.8):
            p.fill(p.ellipse(ex, 17, 1.6, 3), skin, radius=1, depth=1.5, bias=-0.1)
        # the face, front-on
        face = [(24, 10), (26, 5), (32, 3), (38, 5), (40, 10), (40, 17), (40, 22), (38, 26), (35, 29), (29, 29),
                (26, 26), (24, 22), (24, 17)]
        p.fill(p.poly(face), skin, radius=7, depth=6, texture=0.02)
        # the shadow side (right) and under the cheekbones
        p.fill(p.poly([(35, 16), (40, 16), (39, 22), (37, 26), (34, 29), (33, 26), (34, 20)]), skin, flat=1, edge=False)
        p.stroke([(26, 21), (28, 23)], skin[1])
        p.stroke([(38, 21), (36, 23)], skin[0])
        # brows (set hard), eyes (a slow blink), nose, mouth turned down, chin
        p.stroke([(25, 12), (29, 12)], hair[0])
        p.stroke([(35, 12), (39, 12)], hair[0])
        if blink(t, 0.55, 2):
            p.stroke([(26, 15), (29, 15)], skin[1])
            p.stroke([(35, 15), (38, 15)], skin[0])
        else:
            p.stroke([(26, 14), (29, 14)], skin[0])
            p.stroke([(35, 14), (38, 14)], skin[0])
            for ex in (26, 36):
                p.px(ex, 15, (214, 220, 230)), p.px(ex + 1, 15, (70, 96, 136)), p.px(ex + 2, 15, (200, 206, 218))
            p.px(27, 15, (54, 74, 110)), p.px(37, 15, (48, 64, 98))
        p.pixels([(26, 16), (27, 16)], skin[1])
        p.pixels([(36, 16), (37, 16)], skin[0])
        p.stroke([(31, 15), (31, 20)], skin[4])
        p.stroke([(33, 16), (33, 20)], skin[1])
        p.pixels([(31, 21), (33, 21)], skin[0])
        p.px(32, 21, skin[3])
        p.pixels([(26, 18), (27, 18)], skin[4])
        p.stroke([(29, 24), (35, 24)], (96, 78, 100))
        p.pixels([(31, 25), (32, 25), (33, 25)], skin[1])
        p.pixels([(31, 28), (32, 28), (33, 28)], skin[3])
        p.stroke([(29, 9), (35, 9)], skin[3])
        # silver hair: swept back from a high forehead, long locks falling either side of the face
        cap = [(22, 12), (23, 5), (28, 1), (32, 0), (36, 1), (41, 5), (42, 12), (40, 9), (37, 6), (32, 5), (27, 6), (24, 9)]
        p.fill(p.poly(cap), hair, radius=2.5, depth=3, texture=0.04)
        for k, x in enumerate((27, 32, 37)):
            p.stroke([(x, 6), (x + (x - 32) // 4, 3), (x + (x - 32) // 2, 1)], hair[2])
        p.pixels([(29, 2), (30, 3), (34, 2)], hair[4])
        for side, ph in ((-1, 0.0), (1, 0.5)):
            sw = sway(t, ph, 1)
            path = bezier([(32 + side * 9.5, 8), (32 + side * 9, 22), (32 + side * 11, 36), (32 + side * 17 + sw, 56)], 26)
            p.fill(p.poly(ribbon(path, 4, 3, 5)), hair, radius=2, depth=2.5, bias=0.0 if side < 0 else -0.12, texture=0.05)
            for k, off in enumerate((-1, 1)):
                pts = [(round(x + off), round(y)) for x, y in path[4:-2:3]]
                p.pixels(pts, hair[1] if k else hair[4])
    p.rim('#ff8a4a', side=-1, amount=0.35)
    p.rim('#ffd08a', side=1, amount=0.3)
    p.outline('#140608')
    for x0, ph in ((20, 0.1), (58, 0.5), (30, 0.8)):
        u = (t + ph) % 1.0
        if u < 0.8:
            p.px(x0 + sway(t, ph, 1, 2), int(round(40 - u * 36)), mix('#ffd090', '#6a1a10', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def celdauri(t=0.0):
    """after the owner's picture: a weathered Celdauri naval officer - peaked cap with a gold badge, grey stubble, a
    high tan collar, gold fringed epaulettes and a navy double-breasted coat - in a ship's corridor lit by orange
    windows. Moves: breathing, a slow blink, light drifts past the windows, the tags and badge glint."""
    p = Portrait()
    painted_bg(p, (12, 16, 24), (40, 50, 62), 32, 26, 40, steps=5)
    for x0, x1, y0, y1 in ((1, 12, 8, 30), (53, 63, 4, 24)):
        for y in range(y0, y1):
            for x in range(x0, x1):
                k = 1 - abs((x - (x0 + x1) / 2) / ((x1 - x0) / 2)) * 0.5 - abs((y - (y0 + y1) / 2) / ((y1 - y0) / 2)) * 0.35
                bg_px(p, x, y, mix((90, 40, 20), (255, 176, 96), max(0.0, min(1.0, k + BAYER[y % 4, x % 4] * 0.2 - 0.1))))
        for y in range(y0 - 1, y1 + 1):
            bg_px(p, x0 - 1, y, (30, 34, 44)), bg_px(p, x1, y, (30, 34, 44))
        for x in range(x0 - 1, x1 + 1):
            bg_px(p, x, y0 - 1, (30, 34, 44)), bg_px(p, x, y1, (52, 58, 70))
        sy = y0 + int(((t * 2) % 1.0) * (y1 - y0))
        for x in range(x0, x1):
            bg_px(p, x, sy, (255, 236, 200))
    for x in range(14, 51, 9):
        for y in range(0, 64):
            bg_px(p, x, y, (22, 26, 36) if (y // 3) % 2 else (28, 32, 44))
    skin = ramp('#b88c70', spread=0.75, hue_shift=0.05)
    navy = ramp('#26345a', spread=0.72)
    tan = ramp('#b49c6a', spread=0.6)
    gold = ramp('#c8962e', spread=0.75, hue_shift=0.05)
    capr = ramp('#22304e', spread=0.6)
    grey = ramp('#8a8a8a', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(0, 66), (2, 46), (12, 40), (24, 38), (32, 42), (40, 38), (52, 40), (62, 46), (64, 66)]), navy,
               radius=7, depth=5, texture=0.03)
        p.stroke([(30, 44), (26, 66)], navy[0])
        p.stroke([(34, 44), (38, 66)], navy[1])
        for y in (48, 53, 58, 63):
            p.px(27 - (y - 48) // 8, y, gold[3])
            p.px(37 + (y - 48) // 8, y, gold[3])
        for k, c in enumerate(((180, 40, 40), (230, 220, 210), (40, 80, 170), (220, 170, 40))):
            p.px(15 + k, 47, c), p.px(15 + k, 48, c)
        p.fill(p.poly([(17, 51), (20, 51), (20, 54), (18.5, 56), (17, 54)]), gold, radius=1, depth=1.5)
        for side in (-1, 1):
            cx = 32 + side * 21
            p.fill(p.poly([(cx - 9, 41), (cx + 9, 41), (cx + 9, 46), (cx - 9, 46)]), gold, radius=1.6, depth=2)
            for k in range(4):
                p.stroke([(cx - 8, 42 + k), (cx + 8, 42 + k)], gold[4 - k])
            for x in range(cx - 8, cx + 9, 2):
                p.stroke([(x, 47), (x, 50)], gold[2])
        p.stroke([(29, 40), (31, 47), (33, 40)], grey[3])
        gl = blink(t, 0.2, 2)
        p.fill(p.poly([(30, 47), (33, 47), (33, 51), (30, 51)]), grey, radius=1, depth=1.5, bias=0.3 if gl else 0.0)
        # the high stiff tan collar, open at the throat
        p.fill(p.poly([(18, 43), (19, 31), (22, 27), (28, 33), (30, 43)]), tan, radius=1.5, depth=2, bias=0.05)
        p.fill(p.poly([(46, 43), (45, 31), (42, 27), (36, 33), (34, 43)]), tan, radius=1.5, depth=2, bias=-0.15)
        p.fill(p.poly([(23, 29), (28, 33), (29, 40), (26, 36)]), ramp('#5a4a32', spread=0.4), flat=1, edge=False)
        p.fill(p.poly([(41, 29), (36, 33), (35, 40), (38, 36)]), ramp('#4a3c2a', spread=0.4), flat=1, edge=False)
        p.stroke([(19, 32), (22, 28)], tan[4])
        p.stroke([(45, 32), (42, 28)], tan[3])
    with p.at(0, head):
        p.fill(p.poly([(27, 26), (37, 26), (37, 38), (27, 38)]), skin, radius=2, depth=2, bias=-0.3)
        face = [(21, 12), (43, 12), (44, 20), (42, 28), (37, 33), (32, 35), (27, 33), (22, 28), (20, 20)]
        p.fill(p.poly(face), skin, radius=7, depth=6, texture=0.03)
        # the shadow side of the face (right), the visor's shadow over the eyes
        p.fill(p.poly([(34, 15), (44, 15), (44, 20), (42, 28), (37, 33), (33, 34), (34, 26), (33, 20)]), skin, flat=1,
               edge=False)
        p.fill(p.poly([(21, 15), (43, 15), (43, 18), (21, 18)]), skin, flat=0, edge=False)
        for ex, d in ((20, -1), (44, 1)):
            p.fill(p.ellipse(ex + d * 0.3, 21, 1.7, 3.2), skin, radius=1, depth=1.5, bias=-0.15)
        # brows, eyes, nose, cheekbones, moustache, set mouth, a hint of stubble on the jaw
        p.stroke([(24, 18), (30, 18)], (110, 104, 100))
        p.stroke([(34, 18), (40, 18)], (90, 86, 84))
        shut = blink(t, 0.7, 2)
        for ex in (26, 36):
            if shut:
                p.stroke([(ex, 20), (ex + 2, 20)], skin[0])
            else:
                p.px(ex, 20, (40, 30, 30)), p.px(ex + 1, 20, (130, 156, 170)), p.px(ex + 2, 20, (40, 30, 30))
        p.stroke([(32, 20), (31, 25)], skin[4])
        p.stroke([(33, 21), (33, 25)], skin[1])
        p.pixels([(30, 26), (34, 26)], skin[0])
        p.pixels([(24, 23), (25, 23)], skin[4])
        for y in range(28, 35):
            for x in range(23, 42):
                if (x * 3 + y * 5) % 7 == 0 and p.a[y + p.dy, x] and abs(x - 32) <= (36 - y) * 1.5:
                    p.px(x, y, mix(skin[1], (110, 110, 108), 0.45))
        p.stroke([(28, 28), (36, 28)], (150, 146, 140))
        p.stroke([(29, 29), (35, 29)], (120, 116, 112))
        p.stroke([(30, 31), (34, 31)], (92, 54, 48))
        p.pixels([(31, 32), (32, 32), (33, 32)], skin[1])
        p.px(32, 34, skin[1])
        # the peaked cap
        p.fill(p.poly([(18, 9), (23, 3), (32, 1), (41, 2), (47, 6), (46, 11), (32, 12), (19, 12)]), capr, radius=3, depth=3)
        p.stroke([(20, 10), (45, 10)], gold[2])
        p.stroke([(20, 11), (45, 11)], gold[1])
        p.fill(p.poly([(19, 12), (45, 12), (44, 15), (34, 16), (27, 16), (20, 15)]), ramp('#141414', spread=0.5),
               radius=1.5, depth=2, bias=0.15)
        p.stroke([(23, 13), (41, 13)], (74, 74, 80))
        bk = pulse(t, 0.4, 1)
        p.fill(p.poly([(30, 4), (34, 4), (35, 7), (32, 9), (29, 7)]), gold, radius=1.2, depth=2, bias=0.1 + 0.2 * bk)
        p.px(32, 6, mix(gold[4], '#ffffff', bk))
    p.rim('#ffa860', side=-1, amount=0.32)
    p.rim('#ffb070', side=1, amount=0.3)
    p.outline('#06080c')
    return p


# ---------------------------------------------------------------------------------------------------------------
def saar(t=0.0):
    """after the owner's picture: a stocky Saar nomad - broad furred head, pointed ears, an upturned pink snout,
    an olive cloak bunched round the neck, a necklace of fangs, a long rifle held across the body, grey mist
    behind. Neutral. Moves: breathing, ears twitch, a blink, the scope light pulses, mist drifts."""
    p = Portrait()
    painted_bg(p, (34, 40, 52), (124, 132, 146), 44, 4, 52, steps=6)
    for k in range(3):  # drifting mist bands
        y0 = 44 + k * 7
        off = int(((t + k * 0.33) % 1.0) * 64)
        for x in range(64):
            if (x + off) % 64 < 26 and (x + y0) % 2 == 0:
                bg_px(p, x, y0 + (x // 9) % 2, (96, 104, 118))
    fur = ramp('#9c8e7e', spread=0.78, hue_shift=0.05)
    olive = ramp('#5b6131', spread=0.7, hue_shift=0.05)
    vest = ramp('#3a3530', spread=0.6)
    pink = ramp('#c88a84', spread=0.55)
    gun = ramp('#4a4e54', spread=0.7)
    bone = ramp('#e4dccb', spread=0.4)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the cloak's bunched cowl behind the head
        p.fill(p.poly([(10, 34), (15, 24), (24, 22), (31, 27), (39, 22), (48, 24), (54, 34), (50, 40), (31, 42), (13, 40)]),
               olive, radius=5, depth=4, bias=-0.15, texture=0.05)
        # body: the vest, the cloak draped over both shoulders
        p.fill(p.poly([(20, 66), (21, 40), (31, 38), (42, 40), (43, 66)]), vest, radius=4, depth=3)
        p.fill(p.poly([(0, 66), (0, 42), (6, 33), (16, 30), (24, 36), (22, 52), (17, 66)]), olive, radius=5, depth=4,
               texture=0.05)
        p.fill(p.poly([(64, 66), (64, 42), (58, 33), (47, 30), (39, 36), (42, 52), (47, 66)]), olive, radius=5, depth=4,
               bias=-0.12, texture=0.05)
        p.stroke([(8, 38), (14, 50), (13, 62)], olive[1])
        p.stroke([(56, 38), (50, 50), (52, 62)], olive[1])
        p.fill(p.ellipse(44, 36, 1.8, 1.8), bone, radius=1, depth=1.5)  # the cloak's clasp
        # the necklace of fangs
        neck = bezier([(23, 35), (31, 41), (40, 35)], 16)
        p.stroke([(round(x), round(y)) for x, y in neck], (74, 52, 36))
        for x, y in neck[2:-1:2]:
            p.stroke([(round(x), round(y) + 1), (round(x), round(y) + 3)], bone[3])
            p.px(round(x), round(y) + 4, bone[1])
        # the arm holding the rifle (furred) and the rifle across the body
        p.fill(p.poly(ribbon(bezier([(8, 44), (8, 52), (14, 58)], 10), 7, 6)), fur, radius=3, depth=3, texture=0.06)
        rifle = bezier([(-2, 63), (30, 52), (66, 39)], 20)
        p.fill(p.poly(ribbon(rifle, 4.2, 3.0)), gun, radius=1.6, depth=2, bias=0.05)
        p.fill(p.poly(ribbon(bezier([(24, 50), (34, 46.5), (42, 43.5)], 10), 3.0, 2.6)), gun, radius=1.2, depth=1.5, bias=0.2)
        sk = pulse(t, 0.0, 2)
        p.px(37, 46, mix('#2aa040', '#9affa0', sk)), p.px(38, 45, mix('#1a7030', '#6aff80', sk))
        p.fill(p.ellipse(15, 57, 3.6, 3.0), fur, radius=2, depth=2)  # the hand on the stock
        p.stroke([(13, 56), (17, 57)], fur[1])
        p.fill(p.ellipse(53, 43, 3.2, 2.8), gun, radius=1.6, depth=2, bias=0.2)  # gloved hand on the barrel
        p.stroke([(51, 42), (55, 44)], gun[1])
    with p.at(0, head):
        # ears: broad, pointed, set out to the sides (one twitches)
        tw = 1 if blink(t, 0.3, 2) else 0
        p.fill(p.poly([(21, 11), (12, 4 - tw), (15, 1 - tw), (25, 7)]), fur, radius=2, depth=2.5)
        p.fill(p.poly([(39, 7), (48, 1), (50, 5), (42, 12)]), fur, radius=2, depth=2.5, bias=-0.1)
        p.fill(p.poly([(20, 9), (15, 4 - tw), (22, 7)]), pink, radius=1, depth=1, bias=-0.2)
        p.fill(p.poly([(41, 8), (47, 3), (42, 10)]), pink, radius=1, depth=1, bias=-0.25)
        # shaggy fur framing the sides of the face, from the ears down past the jaw (behind the head)
        ruff = [(20, 8), (15, 12), (14, 16), (12, 20), (14, 22), (12, 26), (15, 28), (14, 32), (19, 31), (21, 34),
                (26, 32), (32, 33), (38, 32), (43, 34), (45, 31), (50, 32), (49, 28), (52, 26), (50, 22), (52, 20),
                (49, 16), (48, 12), (43, 8)]
        p.fill(p.poly(ruff), fur, radius=4, depth=4, texture=0.1, bias=-0.05)
        for x0, y0, d in ((15, 14, -1), (13, 20, -1), (14, 26, -1), (47, 14, 1), (48, 20, 1), (46, 26, 1)):
            p.stroke([(x0 - d * 3, y0 - 1), (x0, y0 + 2)], fur[1])
            p.px(x0 - d * 2, y0, fur[4])
        # the head: broad and heavy
        hd = [(18, 13), (21, 6), (30, 2), (39, 5), (43, 11), (44, 19), (41, 26), (32, 31), (23, 28), (19, 21)]
        p.fill(p.poly(hd), fur, radius=9, depth=9, texture=0.08, bias=-0.14)
        # darker stripes over the brow, furred cheeks
        for a2, b2 in (((26, 5), (27, 11)), ((30, 4), (30, 11)), ((34, 5), (33, 11))):
            p.stroke([a2, b2], fur[1])
        p.pixels([(19, 18), (20, 21), (42, 18), (42, 21)], fur[3])
        p.fill(p.poly([(34, 15), (42, 14), (42, 21), (38, 26), (33, 24)]), fur, flat=1, edge=False)
        # heavy brow ridge with deep-set eyes in its shadow (blink)
        p.stroke([(23, 12), (29, 12)], fur[3])
        p.stroke([(32, 12), (38, 12)], fur[2])
        p.stroke([(23, 13), (29, 13)], fur[1])
        p.stroke([(32, 13), (38, 13)], fur[0])
        p.stroke([(24, 17), (28, 17)], fur[1])
        p.stroke([(33, 17), (37, 17)], fur[0])
        if blink(t, 0.72, 2):
            p.stroke([(24, 15), (27, 15)], fur[0]), p.stroke([(33, 15), (36, 15)], fur[0])
        else:
            for ex in (25, 34):
                p.px(ex, 15, (20, 14, 16)), p.px(ex + 1, 15, (20, 14, 16))
                p.px(ex, 14, (250, 244, 232))
                p.px(ex - 1, 15, fur[3]), p.px(ex + 2, 15, fur[2])
        # brow wrinkles
        p.stroke([(25, 10), (29, 9)], fur[1])
        p.stroke([(33, 9), (37, 10)], fur[1])
        # the upturned pink snout and the mouth (neutral)
        p.fill(p.ellipse(30.5, 19.5, 3.6, 2.7), pink, radius=2, depth=2.5)
        p.px(29, 20, (70, 40, 44)), p.px(32, 20, (70, 40, 44))
        p.px(29, 18, pink[4])
        # long fur on the cheeks falling over the head's edge
        for x0, d in ((20, -1), (42, 1)):
            for k in range(3):
                p.stroke([(x0 + d * k, 16 + k * 3), (x0 + d * (k + 2), 20 + k * 3)], fur[3] if k % 2 == 0 else fur[2])
        # the muzzle, lit, and a closed neutral mouth
        p.fill(p.ellipse(30.5, 24.5, 6, 3.6), fur, radius=2.5, depth=2.5, bias=0.12, edge=False)
        p.fill(p.ellipse(30.5, 19.5, 3.6, 2.7), pink, radius=2, depth=2.5)
        p.px(29, 20, (70, 40, 44)), p.px(32, 20, (70, 40, 44))
        p.px(29, 18, pink[4])
        p.stroke([(27, 25), (34, 25)], (74, 56, 52))
        p.pixels([(24, 22), (37, 22)], fur[1])
    p.rim('#c8d8ff', side=-1, amount=0.3)
    p.rim('#9ad0ff', side=1, amount=0.25)
    p.outline('#0c0e12')
    return p


# ---------------------------------------------------------------------------------------------------------------
def cymiae(t=0.0):
    """after the owner's picture: a hulking Cymiae ape, maddeningly angry - glowing green eyes under a heavy brow,
    a roaring fanged mouth, dark green-black fur, a machine harness with exhaust pipes and green-lit bands, a
    mechanical claw raised, a red haze with orange lights behind. Moves: heaving breath, the jaw trembles,
    the head shakes, the eyes flare, the lights pulse, steam bursts from the pipes."""
    p = Portrait()
    painted_bg(p, (74, 6, 26), (236, 66, 96), 30, 26, 46, steps=6)
    for x, y, ph in ((5, 30, 0.0), (9, 18, 0.3), (58, 14, 0.6), (57, 44, 0.15), (3, 46, 0.8), (50, 4, 0.45)):
        k = pulse(t, ph, 1)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                bg_px(p, x + dx, y + dy, mix((200, 80, 60), (255, 200, 120), k * (0.6 if dx or dy else 1.0)))
    fur = ramp('#2e4032', n=6, spread=0.95, hue_shift=0.06)
    face = ramp('#3c4440', spread=0.8)
    metal = ramp('#8a843c', spread=0.72, hue_shift=0.05)
    claw = ramp('#9aa0a4', spread=0.7)
    mouth = ramp('#a01a2a', spread=0.6)
    bone = ramp('#e8e0cc', spread=0.4)
    green = '#3aff7a'
    body = breath(t, 0.0, 2) if False else -int(round(1 + wave(t, 0, 2)))  # heaving, twice per loop
    head = -int(round(0.5 + 0.5 * wave(t, -0.08, 2)))
    shake = sway(t, 0.0, 1, cycles=6) if (t * 24) % 8 < 3 else 0
    flare = pulse(t, 0.1, 3)
    # the harness behind: exhaust stacks with green-lit rings, a pipe off to the right
    with p.at(0, body):
        for x0, x1, top in ((15, 21, -2), (40, 45, 2)):
            p.fill(p.poly([(x0, top), (x1, top), (x1, 26), (x0, 26)]), metal, radius=2, depth=3, texture=0.04)
            p.fill(p.poly([(x0 - 1, top), (x1 + 1, top), (x1 + 1, top + 3), (x0 - 1, top + 3)]), metal, radius=1, depth=2, bias=0.1)
            for y in range(top + 6, 24, 5):
                p.stroke([(x0, y), (x1, y)], mix('#1a8a40', green, flare))
        p.fill(p.poly(ribbon(bezier([(46, 24), (54, 14), (62, 6)], 12), 4, 3)), metal, radius=1.5, depth=2, bias=-0.05)
        p.stroke([(55, 12), (58, 15)], mix('#1a8a40', green, flare))
        # the massive hunched shoulders and arms
        p.fill(p.poly([(0, 66), (0, 34), (8, 24), (20, 20), (32, 26), (44, 20), (56, 24), (64, 34), (64, 66)]), fur,
               radius=8, depth=7, texture=0.1)
        rng = np.random.default_rng(11)
        for _ in range(40):  # fur tufts catching the red light
            x, y = int(rng.integers(2, 62)), int(rng.integers(24, 64))
            if p.a[min(63, y + body), x] and not (16 < x < 48 and y < 44):
                p.pixels([(x, y), (x + 1, y + 1)], fur[4] if rng.random() < 0.5 else fur[3])
        # machine plates on the right shoulder with glowing bars
        p.fill(p.poly([(44, 26), (58, 28), (62, 40), (52, 44), (44, 38)]), metal, radius=3, depth=3, bias=-0.05)
        for k in range(4):
            p.stroke([(48, 31 + k * 3), (56, 32 + k * 3)], mix('#147a34', green, flare))
        # the raised mechanical claw on the left, green lights on the forearm
        p.fill(p.poly(ribbon(bezier([(2, 66), (6, 54), (12, 46)], 12), 9, 7)), fur, radius=4, depth=4, texture=0.08)
        for k in range(3):
            p.stroke([(3 + k, 60 - k * 3), (9 + k, 58 - k * 3)], mix('#147a34', green, flare))
        p.fill(p.ellipse(13, 47, 6, 4.5), claw, radius=3, depth=3, bias=-0.05)  # the mechanical hand
        p.stroke([(9, 46), (17, 45)], mix('#147a34', green, flare))
        for k, (bx, by) in enumerate(((8, 44), (12, 43), (16, 43), (19, 45))):
            path = bezier([(bx, by), (bx - 2 + k, by - 6), (bx + 1 + k, by - 11)], 10)
            p.fill(p.poly(ribbon(path, 2.4, 0.8)), claw, radius=1, depth=1.5, bias=0.1)
            p.px(round(path[-1][0]), round(path[-1][1]), claw[4])
    f, P, IP = scaler(1.3, 32, 26)
    with p.at(shake, head + body):
        # the head: low, heavy, thrust forward
        hd = [(18, 17), (23, 9), (32, 6), (41, 9), (46, 17), (48, 27), (44, 36), (37, 43), (27, 43), (20, 36), (16, 27)]
        p.fill(p.poly(hd), fur, radius=7, depth=6, texture=0.08)
        p.fill(p.poly([(22, 18), (42, 18), (44, 29), (38, 39), (26, 39), (20, 29)]), face, radius=4, depth=4, texture=0.05)
        # the brow ridge, crushed down into a V
        p.fill(p.poly([(19, 14), (32, 20), (45, 14), (45, 18), (32, 23), (19, 18)]), face, radius=2, depth=3, bias=0.1)
        p.stroke([(20, 14), (31, 20)], face[4])
        # glowing, narrowed eyes
        for xs in (((24, 20), (26, 21), (27, 21)), ((37, 21), (38, 21), (40, 20))):
            p.pixels(list(xs), mix('#2ae070', '#d8ffe0', flare))
        p.glow(*[int(v) for v in IP([(27, 22)])[0]], '#eaffef', mix('#1aa050', green, flare), r=0, strength=0.5 + 0.5 * flare)
        p.glow(*[int(v) for v in IP([(37, 22)])[0]], '#eaffef', mix('#1aa050', green, flare), r=0, strength=0.5 + 0.5 * flare)
        # flared nostrils, snarl creases
        p.fill(p.poly([(28, 23), (36, 23), (37, 27), (27, 27)]), face, radius=1.5, depth=2, bias=0.05)
        p.px(30, 26, (14, 14, 14)), p.px(34, 26, (14, 14, 14))
        p.stroke([(23, 25), (26, 29)], face[0])
        p.stroke([(41, 25), (38, 29)], face[0])
        # the roaring mouth: jaw trembling, fangs bared
        jaw = 1 if blink(t, 0.2, 1) or blink(t, 0.45, 1) or blink(t, 0.7, 1) or blink(t, 0.9, 1) else 0
        p.fill(p.poly([(24, 29), (40, 29), (38, 38 + jaw), (32, 40 + jaw), (26, 38 + jaw)]), mouth, radius=2.5, depth=3,
               bias=-0.1)
        p.fill(p.poly([(28, 32), (36, 32), (35, 38 + jaw), (29, 38 + jaw)]), ramp('#3a0610', spread=0.4), flat=0, edge=False)
        p.stroke([(26, 29), (38, 29)], bone[2])
        for fx in (26, 40):
            p.stroke([(fx, 28), (fx, 31)], bone[3])
            p.px(fx, 32, bone[1])
        for fx in (27, 39):
            p.stroke([(fx, 35 + jaw), (fx, 33 + jaw)], bone[3])
        p.stroke([(28, 39 + jaw), (36, 39 + jaw)], bone[1])
    p.rim('#ff5a7a', side=-1, amount=0.4)
    p.rim('#ffa060', side=1, amount=0.35)
    p.outline('#100408')
    # steam bursting from the stacks
    for x0, top, ph in ((18, -2, 0.0), (42, 2, 0.5)):
        u = (t * 2 + ph) % 1.0
        for k in range(3):
            y = top - 1 - int(u * 6) - k
            x = x0 + sway(t, ph + k * 0.1, 1, 2)
            if 0 <= y < 64 and u < 0.7:
                p.px(x, y, mix('#ffd0d8', '#a04060', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def florzen(t=0.0):
    """after the owner's picture: a Florzen - a space jellyfish, no face - a translucent purple-blue bell veined in
    pale light with a glowing orange core, ribbed tentacles hanging below, thin ones curling out, teal water with
    light shafts and bubbles. Moves: the bell pulses, the core glows, tentacles ripple, bubbles rise."""
    p = Portrait()
    painted_bg(p, (6, 26, 30), (40, 96, 92), 30, 4, 50, steps=6)
    for x0, w in ((6, 5), (46, 7)):  # light shafts from above
        for y in range(64):
            for dx in range(w):
                x = x0 + dx + y // 6
                if 0 <= x < 64 and (x + y) % 2 == 0:
                    bg_px(p, x, y, (34 + 8 * pulse(t, x0 * 0.02, 1), 86, 84))
    for dx in range(-5, 6):  # a glowing orb at the top left
        for dy in range(-5, 6):
            d = math.hypot(dx, dy)
            if d <= 5:
                bg_px(p, 2 + dx, 2 + dy, mix((60, 200, 180), (190, 255, 236), max(0.0, 1 - d / 5)))
    memb = ramp('#5e5a9c', n=6, spread=0.8, hue_shift=0.06)
    core = ramp('#e8902a', spread=0.6, hue_shift=0.05)
    tent = ramp('#5a3a6e', spread=0.75, hue_shift=0.04)
    beat = 0.5 + 0.5 * wave(t, 0.0, 1)  # bell contraction
    bob = -int(round(beat))
    with p.at(0, bob):
        # a few thin trailing tentacles curling out (behind)
        for k, (x0, side, ph) in enumerate(((16, -1, 0.0), (48, 1, 0.3), (22, -1, 0.6), (42, 1, 0.8))):
            pts = []
            for j in range(12):
                y = 27 + j * 3.4
                pts.append((x0 + side * j * 0.9 + 2.2 * wave(t, ph - j * 0.06, 1) + side * (j > 8) * (j - 8) * 1.5, y))
            p.stroke([(round(x), round(y)) for x, y in pts], tent[3] if k % 2 else tent[2])
        # the ribbed oral arms: tapering, rippling
        for k, (x0, ph, w) in enumerate(((22, 0.0, 3.6), (27, 0.2, 4.4), (32, 0.45, 4.6), (37, 0.65, 4.4), (42, 0.85, 3.6))):
            pts = []
            for j in range(10):
                y = 24 + j * 4.6
                pts.append((x0 + (x0 - 32) * 0.08 * j + 3.2 * wave(t, ph - j * 0.11, 1) * min(1, j / 3), y))
            p.fill(p.poly(ribbon(pts, w, 1.4)), tent, radius=1.8, depth=2.5, bias=0.05 - 0.05 * (k % 2), texture=0.04)
            for j in range(1, len(pts) - 1):
                x, y = pts[j]
                p.px(round(x - 0.5), round(y), tent[4] if j % 2 else tent[1])
        # the bell: a tall translucent dome with a scalloped skirt, a glowing core inside
        rx, ry = 23 - beat, 18 + beat * 0.5
        top = [(32 + rx * math.cos(a2), 21 - ry * math.sin(a2)) for a2 in [math.pi * i / 32 for i in range(33)]]
        skirt = [(32 - rx + 0.5, 24), (32 - rx + 2, 29), (25, 27), (32, 24), (39, 27), (32 + rx - 2, 29), (32 + rx - 0.5, 24)]
        bell = p.poly(top + skirt)
        p.fill(bell, memb, radius=9, depth=8, texture=0.03, bias=0.12)
        g = pulse(t, 0.1, 1)
        p.fill(p.ellipse(32, 15, 13.5 - beat * 0.5, 10), core, radius=6, depth=6, bias=0.12 + 0.12 * g, edge=False)
        p.fill(p.ellipse(32, 12, 7.5, 5), ramp(mix('#ffb040', '#ffe486', g), spread=0.4), radius=4, depth=3, bias=0.25,
               edge=False)
        p.stroke([(32, 8), (32, 15)], core[1])
        p.stroke([(32, 15), (27, 21)], core[1])
        p.stroke([(32, 15), (37, 21)], core[1])
        # the translucent lower rim: lighter membrane curling under
        p.stroke([(round(32 - rx + 2), 25), (24, 26), (32, 24), (40, 26), (round(32 + rx - 2), 25)], memb[3])
        # pale veins netting the dome
        rng = np.random.default_rng(4)
        for _ in range(10):
            a0 = float(rng.uniform(0.25, math.pi - 0.25))
            r0 = float(rng.uniform(0.6, 0.97))
            pts = []
            for j in range(4):
                a2 = a0 + (j - 1.5) * 0.16 + float(rng.uniform(-0.05, 0.05))
                rr = r0 - j * 0.06
                pts.append((round(32 + rx * rr * math.cos(a2)), round(21 - ry * rr * math.sin(a2))))
            p.stroke(pts, mix(memb[4], '#c8f6ff', 0.35 + 0.35 * pulse(t, a0 * 0.2, 1)))
        # small lights rippling round the skirt (not eyes: many, evenly spaced)
        for i in range(9):
            x = round(32 - rx + 3 + i * (2 * rx - 6) / 8)
            y = 27 if i in (0, 8) else (26 if i in (1, 7) else 25)
            p.px(x, y, mix(memb[2], '#9af0ff', pulse(t, i / 9, 1)))
    p.rim('#7affe8', side=-1, amount=0.4)
    p.rim('#c890ff', side=1, amount=0.35)
    p.outline('#04100f')
    for x0, ph in ((12, 0.0), (50, 0.3), (8, 0.55), (56, 0.8), (30, 0.65)):  # bubbles
        u = (t + ph) % 1.0
        y = int(round(64 - u * 64))
        x = x0 + sway(t, ph, 1, 2)
        p.px(x, y, (190, 250, 240))
        if u > 0.4:
            p.px(x + 1, y, (120, 200, 196))
    return p


# ---------------------------------------------------------------------------------------------------------------
def muaat(t=0.0):
    """after the owner's picture: the Muaat forge-egg - a riveted bronze shell with a cast metal face (not a real
    face) in a round frame at the top, porthole windows where the fire elemental inside shows through, hoses and
    cables, a segmented crest. Moves: the fire churns in the portholes, glow pulses, steam and embers escape,
    the machine hums (a slight bob), a glint crosses the mask."""
    p = Portrait()
    painted_bg(p, (14, 20, 18), (60, 74, 58), 32, 30, 44, steps=6)
    bronze = ramp('#8c7440', n=6, spread=0.9, hue_shift=0.06)
    dark = ramp('#3a3026', spread=0.7)
    hose = ramp('#5c4a36', spread=0.75)
    mask = ramp('#b08a4a', spread=0.75, hue_shift=0.05)
    hum = -1 if blink(t, 0.25, 3) or blink(t, 0.75, 3) else 0
    fire = pulse(t, 0.0, 2)

    def porthole(cx, cy, r, ph):
        """a riveted round window onto the fire inside: churning flame colours, brightest at the centre"""
        p.fill(p.ellipse(cx, cy, r + 1.6, r + 1.6), bronze, radius=1.5, depth=2, bias=0.1)
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy) / r
                if d <= 1.0:
                    a = math.atan2(y + 0.5 - cy, x + 0.5 - cx)
                    swirl = 0.5 + 0.5 * math.sin(a * 3 + d * 6 - t * 2 * math.pi * 2 + ph * 6)
                    k = (1 - d) * 0.9 + swirl * 0.25 * (1 - d * 0.4) + 0.15 * fire + 0.1
                    col = mix((150, 40, 10), (255, 150, 30), min(1, k * 1.4)) if k < 0.6 else \
                        mix((255, 150, 30), (255, 250, 200), min(1, (k - 0.6) * 2.5))
                    p.px(x, y, col)
        for i in range(8):
            a = i * math.pi / 4
            p.px(round(cx + (r + 1) * math.cos(a)), round(cy + (r + 1) * math.sin(a)), bronze[5])

    with p.at(0, hum):
        # the segmented crest arching over the top of the egg, plates with gaps between
        for i in range(9):
            a0 = math.pi * (1.1 + i * 0.092)
            a1 = a0 + math.pi * 0.078
            seg_ = [(32 + (23 + r) * math.cos(a), 45 + (37 + r) * math.sin(a)) for r, a in ((1.0, a0), (6.0, a0), (6.0, a1), (1.0, a1))]
            p.fill(p.poly(seg_), dark, radius=1.5, depth=2, bias=0.15)
        # the egg: riveted bronze plates in horizontal bands
        egg = p.poly(rot_ellipse(32, 45, 23, 37, 0.0, n=64))
        p.fill(egg, bronze, radius=12, depth=12, texture=0.05)
        for y in (31, 44, 54, 62):
            p.stroke([(12, y), (52, y)], bronze[1])
            p.stroke([(12, y + 1), (52, y + 1)], bronze[4])
        for x in (22, 42):
            p.stroke([(x, 29), (x, 66)], bronze[1])
        for x, y in ((16, 33), (48, 33), (15, 46), (49, 46), (18, 56), (46, 56), (26, 33), (38, 33)):
            p.px(x, y, bronze[5])
        # the round frame and the cast face at the top
        p.fill(p.ellipse(32, 19, 10.5, 10.5), dark, radius=4, depth=4, bias=0.05)
        p.fill(p.ellipse(32, 19, 8.5, 8.5), bronze, radius=3, depth=3, bias=-0.15)
        face = [(27, 13), (32, 11), (37, 13), (38, 18), (37, 23), (34, 27), (32, 28), (30, 27), (27, 23), (26, 18)]
        p.fill(p.poly(face), mask, radius=4, depth=4, texture=0.02)
        p.stroke([(28, 18), (30, 18)], mask[0])  # closed, cast eyes
        p.stroke([(34, 18), (36, 18)], mask[0])
        p.stroke([(32, 18), (32, 22)], mask[4])
        p.px(33, 22, mask[1])
        p.stroke([(30, 24), (34, 24)], mask[1])
        gl = int((t * 2 % 1.0) * 20) - 4  # a glint sweeping across the mask
        for k in range(3):
            x = 26 + gl + k
            for y in range(12, 28):
                if 0 <= x < 64 and p.a[y + hum, x] and (x - 26 - gl) == (y - 12) // 6:
                    p.px(x, y, mask[5] if hasattr(mask, '__len__') and len(mask) > 5 else mask[4])
        # hoses curving from the frame down the sides; a tank on the right; cables
        for side in (-1, 1):
            hp = bezier([(32 + side * 8, 25), (32 + side * 18, 28), (32 + side * 22, 40), (32 + side * 20, 56)], 20)
            p.fill(p.poly(ribbon(hp, 3.6, 3.0)), hose, radius=1.6, depth=2.5, bias=0.05 if side < 0 else -0.1)
            for j in range(2, len(hp) - 1, 3):
                x, y = hp[j]
                p.px(round(x), round(y), hose[4])
            cab = bezier([(32 + side * 6, 27), (32 + side * 12, 34), (32 + side * 9, 44), (32 + side * 14, 52)], 16)
            p.stroke([(round(x), round(y)) for x, y in cab], dark[0])
        p.fill(p.ellipse(51, 47, 5.5, 5.5), bronze, radius=3, depth=4, bias=-0.1)
        p.fill(p.poly([(50, 52), (53, 52), (53, 57), (50, 57)]), bronze, radius=1, depth=1.5, bias=-0.2)
        p.fill(p.ellipse(12, 49, 4, 3.4), dark, radius=2, depth=2.5)
        # the portholes: the elemental's fire seen through the shell
        for cx, cy, r, R in ((32, 37, 5.2, 11), (32, 62, 4.0, 8)):  # the shell lit around each window
            for y in range(int(cy - R), int(cy + R) + 1):
                for x in range(int(cx - R), int(cx + R) + 1):
                    d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                    if r + 1.5 < d <= R and 0 <= y + hum < 64 and 0 <= x < 64 and p.a[y + hum, x]:
                        k = (1 - d / R) * (0.55 + 0.2 * fire)
                        p.rgb[y + hum, x] = (p.rgb[y + hum, x] * (1 - k) + np.array((255, 176, 70)) * k).astype(np.uint8)
        porthole(32, 37, 5.2, 0.0)
        porthole(32, 62, 4.0, 0.4)
    p.rim('#ffb050', side=-1, amount=0.3)
    p.rim('#9ad0a0', side=1, amount=0.25)
    p.outline('#060806')
    # embers and steam escaping the seams
    for x0, y0, ph, col in ((14, 42, 0.0, '#ffa040'), (50, 42, 0.4, '#ffa040'), (42, 22, 0.7, '#d8e0d0'), (22, 22, 0.2, '#d8e0d0')):
        u = (t + ph) % 1.0
        if u < 0.6:
            p.px(x0 + sway(t, ph, 1, 2), int(round(y0 - u * 18)), mix(col, '#203020', u / 0.6))
    return p


# ---------------------------------------------------------------------------------------------------------------
def hacan(t=0.0):
    """after the owner's picture, front-on (owner: "have him look straight"): a regal Hacan lion in a red cowl
    latticed with gold thread and edged with gold fringe, the mane framing his face inside the hood, a jewelled
    circlet on the brow, a gold medallion and an arm guard, warm golden haze behind. Neutral, half-lidded.
    Moves: breathing, the mane and fringe stir, a slow blink, the gem and medallion glint, motes drift."""
    p = Portrait()
    painted_bg(p, (84, 44, 18), (236, 176, 96), 52, 6, 54, steps=6)
    HL = np.array([0.0, -0.6, 0.8]) / np.linalg.norm([0.0, -0.6, 0.8])
    fur = ramp('#c48e3c', spread=0.8, hue_shift=0.07)
    mane = ramp('#a86a28', spread=0.8, hue_shift=0.06)
    cream = ramp('#dcc8a4', spread=0.45)
    red = ramp('#a8202a', spread=0.85, hue_shift=0.05)
    gold = ramp('#d0a03a', spread=0.75, hue_shift=0.06)
    rose = ramp('#9a5a58', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the cowl: red cloth over the head and shoulders, latticed with gold thread
        hood = [(13, 10), (18, 3), (32, 0), (46, 3), (51, 10), (54, 26), (60, 42), (64, 66), (0, 66), (4, 42), (10, 26)]
        hm = p.fill(p.poly(hood), red, radius=9, depth=7, texture=0.04)
        for c in range(-80, 120, 9):
            for x in range(64):
                for y in (c + x // 2, c - x // 2 + 40):
                    if 0 <= y < 64 and hm[y, x] and x % 3 != 0:
                        p.rgb[y, x] = mix(p.rgb[y, x], gold[2], 0.3)
        for pts in (((10, 30), (8, 46), (6, 64)), ((54, 30), (56, 46), (58, 64)), ((18, 44), (20, 56), (18, 64)),
                    ((46, 44), (44, 56), (46, 64))):
            p.stroke(list(pts), red[1])
        # the garment between the cowl's edges, the medallion, the arm guard
        p.fill(p.poly([(20, 40), (44, 40), (46, 66), (18, 66)]), red, radius=4, depth=3, bias=-0.25)
        mk = blink(t, 0.4, 2)
        p.fill(p.ellipse(32, 47, 4.5, 4.5), gold, radius=2.5, depth=3, bias=0.3 if mk else 0.05)
        p.stroke([(30, 46), (32, 48), (34, 46)], gold[1])
        p.px(32, 45, gold[4])
        p.fill(p.ellipse(8, 54, 8, 6.5), gold, radius=4, depth=4, texture=0.05)
        for k in range(3):
            p.stroke([(2 + k, 50 + k * 3), (14 + k, 49 + k * 3)], gold[1])
        p.px(8, 52, (190, 40, 40)), p.px(9, 52, (240, 90, 80))
    with p.at(0, head):
        # the mane, framing the face inside the hood
        mn = [(17, 10), (24, 5), (32, 4), (40, 5), (47, 10), (50, 20), (48, 32), (42, 40), (32, 42), (22, 40), (16, 32), (14, 20)]
        p.fill(p.poly(mn), mane, radius=6, depth=5, texture=0.12, light=HL)
        for k in range(6):
            side = -1 if k % 2 == 0 else 1
            y0 = 14 + (k // 2) * 8
            sw = sway(t, k * 0.15, 1)
            x0 = 32 + side * 15
            p.stroke([(x0, y0), (x0 + side * 2 + sw, y0 + 5)], mane[1])
            p.px(x0 + side + sw, y0 + 2, mane[4])
        # the face: broad brow, wide cheeks, a long nose bridge to a broad muzzle
        face = [(24, 8), (32, 6), (40, 8), (43, 14), (44, 21), (41, 28), (37, 33), (32, 35), (27, 33), (23, 28), (20, 21), (21, 14)]
        p.fill(p.poly(face), fur, radius=8, depth=6, texture=0.05, light=HL, edge=False)
        # brow ridges and the furrow between them
        p.stroke([(24, 12), (29, 12)], fur[4])
        p.stroke([(35, 12), (40, 12)], fur[4])
        p.stroke([(32, 9), (32, 12)], fur[1])
        # half-lidded, regal eyes (a slow blink)
        for ex, d in ((24, 1), (40, -1)):
            if blink(t, 0.66, 2):
                p.stroke([(ex, 15), (ex + d * 5, 15)], fur[0])
            else:
                p.stroke([(ex, 14), (ex + d * 5, 14)], fur[0])
                p.px(ex, 15, fur[0])
                p.px(ex + d, 15, (236, 186, 80)), p.px(ex + d * 2, 15, (40, 24, 14)), p.px(ex + d * 3, 15, (40, 24, 14))
                p.px(ex + d * 4, 15, (200, 140, 54)), p.px(ex + d * 5, 15, fur[0])
            p.stroke([(ex + d, 16), (ex + d * 4, 16)], fur[1])
        # dark lines from the inner corners down beside the nose
        p.stroke([(29, 16), (30, 20)], fur[1])
        p.stroke([(35, 16), (34, 20)], fur[1])
        # the broad nose bridge, lit, and the rose leather nose
        p.fill(p.poly([(30, 13), (34, 13), (35, 20), (29, 20)]), fur, radius=2, depth=2, bias=0.2, light=HL, edge=False)
        p.stroke([(32, 13), (32, 19)], fur[4])
        p.fill(p.poly([(27, 19), (37, 19), (36, 22), (32, 24), (28, 22)]), rose, radius=1.5, depth=2, light=HL)
        p.px(29, 21, (60, 28, 28)), p.px(35, 21, (60, 28, 28)), p.pixels([(31, 19), (32, 19), (33, 19)], rose[4])
        # the white muzzle pads with whisker spots, the neutral mouth, a white chin
        for cx, sgn in ((29.5, -1), (34.5, 1)):
            p.fill(p.ellipse(cx, 25.5, 3.6, 2.8), cream, radius=2, depth=2, light=HL, edge=False)
        for x, y in ((28, 25), (36, 25)):
            p.px(x, y, (170, 140, 104))
        p.stroke([(32, 24), (32, 26)], (110, 76, 60))
        p.stroke([(32, 27), (29, 28)], (90, 56, 44))
        p.stroke([(32, 27), (35, 28)], (90, 56, 44))
        p.fill(p.poly([(29, 29), (35, 29), (34, 32), (32, 34), (30, 32)]), cream, radius=2, depth=2, light=HL, texture=0.06,
               edge=False)
        # the jewelled circlet on the brow, under the hood's edge
        p.fill(p.poly([(23, 7), (32, 4), (41, 7), (41, 9), (32, 7), (23, 9)]), gold, radius=1.5, depth=2, bias=0.1, light=HL)
        p.fill(p.poly([(30, 1), (34, 1), (35, 5), (32, 7), (29, 5)]), gold, radius=1.5, depth=2, bias=0.15, light=HL)
        gk = pulse(t, 0.2, 1)
        p.px(32, 3, mix('#a01830', '#ff5070', gk)), p.px(32, 4, (120, 20, 30))
    with p.at(0, body):
        # the gold fringe along the cowl's inner edge, framing the mane, down to the hem (sways a little)
        for side in (-1, 1):
            path = bezier([(32 + side * 4, 2), (32 + side * 18, 4), (32 + side * 19, 26), (32 + side * 14, 66)], 40)
            for j, (x, y) in enumerate(path):
                sw = sway(t, j * 0.03, 1) if j > 20 else 0
                p.px(round(x), round(y), gold[3] if j % 2 else gold[1])
                p.px(round(x) + side + sw, round(y) + 1, gold[2] if j % 2 else gold[4])
    p.rim('#ffe0a0', side=1, amount=0.32)
    p.rim('#ff9a50', side=-1, amount=0.25)
    p.outline('#1a0c06')
    for x0, ph in ((58, 0.0), (6, 0.4), (54, 0.75)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(40 - u * 36)), mix('#fff0c0', '#8a5a20', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def kortali(t=0.0):
    """after the owner's picture: a Kortali warrior sealed in futuristic armour - no face, an angular crested helm with
    purple eye slits, huge layered pauldrons edged in gold, a sword behind the shoulder, slate dusk with a purple
    glow below and falling sparks. Moves: breathing, the eye slits pulse, sparks fall, a glint runs the gold."""
    p = Portrait()
    painted_bg(p, (22, 26, 36), (52, 58, 74), 30, 18, 40, steps=6)
    for y in range(40, 64):  # the purple glow rising from below
        for x in range(64):
            k = (y - 40) / 24 * (1 - abs(x - 30) / 44)
            if k > 0 and (k * 4 + BAYER[y % 4, x % 4]) > 1.2:
                bg_px(p, x, y, mix((40, 30, 60), (110, 60, 140), min(1, k)))
    for x0, ph in ((52, 0.0), (58, 0.35), (47, 0.7), (61, 0.15), (55, 0.55)):  # sparks falling on the right
        u = (t * 1.5 + ph) % 1.0
        y = int(u * 70) - 6
        for k in range(4):
            bg_px(p, x0, y - k, mix((255, 170, 90), (90, 50, 50), k / 4))
    armour = ramp('#4c505c', n=6, spread=0.85, hue_shift=0.05)
    dark = ramp('#24262e', spread=0.6)
    gold = ramp('#c8a050', spread=0.7, hue_shift=0.05)
    steel = ramp('#8a92a0', spread=0.6)
    body, head = breath(t), breath(t, -0.12)
    eye = pulse(t, 0.0, 1)
    with p.at(0, body):
        # the sword behind the left shoulder
        p.fill(p.poly(ribbon([(1, 6), (12, 28), (20, 44)], 3.2, 3.6)), steel, radius=1.2, depth=1.5, bias=0.1)
        p.fill(p.poly([(-1, 2), (5, 0), (6, 4), (0, 6)]), dark, radius=1, depth=1.5)
        # the body under the plates: dark mail
        p.fill(p.poly([(6, 66), (10, 36), (32, 30), (54, 36), (58, 66)]), dark, radius=5, depth=4, texture=0.12)
        # the breastplate: angular, layered, a central ridge
        p.fill(p.poly([(20, 36), (32, 33), (44, 36), (42, 52), (32, 62), (22, 52)]), armour, radius=5, depth=5, texture=0.03)
        p.stroke([(32, 34), (32, 61)], armour[5])
        p.stroke([(33, 35), (33, 60)], armour[1])
        p.stroke([(23, 44), (32, 48), (41, 44)], armour[1])
        p.stroke([(25, 52), (32, 56), (39, 52)], armour[1])
        p.pixels([(27, 40), (37, 40), (28, 49), (36, 49)], armour[5])
        # huge layered pauldrons with gold edges, the left one spiked upward
        p.fill(p.poly([(2, 46), (4, 30), (12, 20), (16, 12), (20, 22), (28, 28), (26, 40), (14, 48)]), armour, radius=5,
               depth=5, texture=0.03)
        p.fill(p.poly([(4, 40), (12, 34), (24, 36), (22, 44), (10, 50), (2, 52)]), armour, radius=3, depth=3, bias=-0.1)
        p.fill(p.poly([(62, 46), (60, 30), (54, 23), (44, 26), (38, 32), (42, 42), (52, 48)]), armour, radius=5, depth=5,
               bias=-0.12, texture=0.03)
        p.fill(p.poly([(60, 40), (52, 35), (42, 37), (44, 44), (54, 50), (62, 52)]), armour, radius=3, depth=3, bias=-0.2)
        gl = int((t % 1.0) * 30)
        for pts in ([(4, 31), (12, 21), (16, 13)], [(4, 41), (12, 35), (24, 37)], [(60, 31), (54, 24), (44, 27)],
                    [(60, 41), (52, 36), (42, 38)]):
            seg = bezier(pts, 14)
            for j, (x, y) in enumerate(seg):
                p.px(round(x), round(y), gold[4] if abs(j - gl % 15) < 1 else (gold[2] if j % 3 else gold[3]))
    with p.at(1, head):
        # the helm: angular, a central crest, cheek plates, a pointed chin guard - no face
        hm = [(24, 14), (27, 6), (32, 1), (37, 6), (40, 14), (40, 22), (36, 29), (32, 31), (28, 29), (24, 22)]
        p.fill(p.poly(hm), armour, radius=6, depth=6, texture=0.03)
        p.fill(p.poly([(30, 1), (32, -2), (34, 1), (33, 10), (31, 10)]), armour, radius=1.5, depth=2, bias=0.2)
        p.fill(p.poly([(22, 12), (26, 8), (27, 16), (24, 22)]), armour, radius=2, depth=2, bias=0.05)
        p.fill(p.poly([(42, 12), (38, 8), (37, 16), (40, 22)]), armour, radius=2, depth=2, bias=-0.15)
        p.stroke([(31, 4), (31, 9)], gold[3]), p.stroke([(33, 4), (33, 9)], gold[1])
        # the dark visor band and the purple eye slits (they pulse)
        p.fill(p.poly([(25, 14), (39, 14), (38, 19), (33, 21), (31, 21), (26, 19)]), dark, radius=1.5, depth=1.5, bias=-0.2)
        ec, eh = mix('#7a3ad0', '#e0b0ff', eye), mix('#4a1a90', '#b070ff', eye)
        p.pixels([(27, 16), (28, 17), (29, 17)], ec)
        p.pixels([(37, 16), (36, 17), (35, 17)], ec)
        p.px(28, 16, eh), p.px(36, 16, eh)
        p.stroke([(28, 22), (32, 26), (36, 22)], armour[1])
        p.stroke([(32, 21), (32, 29)], armour[5])
    p.rim('#a070ff', side=-1, amount=0.3)
    p.rim('#9ab0d0', side=1, amount=0.35)
    p.outline('#06070a')
    return p


# ---------------------------------------------------------------------------------------------------------------
def stream(p, path, t, speed=1.0, ph=0.0, width=2, core='#e8ffff', glow='#4ac8ff', fade=0.0):
    """an aetheric stream: glowing strands along a path, light flowing along it (blended over whatever is there)"""
    n = len(path)
    gc, cc = np.array(hexrgb(glow), float), np.array(hexrgb(core), float)
    for j, (x, y) in enumerate(path):
        u = j / max(1, n - 1)
        flow = 0.5 + 0.5 * math.cos(2 * math.pi * (u * 3 - t * speed * 2 - ph))
        k = (0.45 + 0.55 * flow) * (1 - fade * u)
        for w in range(-(width // 2), width - width // 2):
            xi, yi = int(round(x)), int(round(y + w))
            if 0 <= xi < p.w and 0 <= yi < p.h:
                base = p.rgb[yi, xi].astype(float) if p.a[yi, xi] else (p.bg[yi, xi].astype(float) if p.bg is not None else gc * 0)
                c = cc if (w == 0 and flow > 0.75) else gc
                a = k * (0.9 if w == 0 else 0.5)
                p.rgb[yi, xi] = (base * (1 - a) + c * a).clip(0, 255).astype(np.uint8)
                p.a[yi, xi] = True


def ghost(t=0.0):
    """after the owner's picture: a Ghost of Creuss - the blue aetheric energy is the creature; the tall bronze mitre
    helm, filigreed collar plate and ribbed dark armour only house it, and its body pours out of the suit's ports
    in long glowing streams. Moves: the streams flow, the ports pulse, the suit hovers gently, the gem glows."""
    p = Portrait()
    painted_bg(p, (24, 22, 28), (64, 58, 66), 30, 22, 42, steps=6)
    rng = np.random.default_rng(9)
    for _ in range(26):  # motes of aether
        x, y = int(rng.integers(0, 64)), int(rng.integers(0, 64))
        bg_px(p, x, y, mix((60, 70, 90), (170, 220, 255), pulse(t, float(rng.random()), 1)))
    bronze = ramp('#7a5a54', spread=0.75, hue_shift=0.04)
    gold = ramp('#c8a058', spread=0.65, hue_shift=0.05)
    dark = ramp('#3a2c2c', spread=0.7)
    cloak = ramp('#3e2e38', spread=0.7)
    port = pulse(t, 0.0, 2)
    hover = -int(round(0.5 + 0.5 * wave(t, 0.0, 1)))
    with p.at(0, hover):
        # the cloak hanging from the shoulders, the dark ribbed armour
        p.fill(p.poly([(0, 66), (2, 44), (8, 34), (18, 32), (22, 50), (16, 66)]), cloak, radius=5, depth=4, texture=0.05)
        p.fill(p.poly([(64, 66), (62, 44), (56, 34), (46, 32), (42, 50), (48, 66)]), cloak, radius=5, depth=4, bias=-0.15,
               texture=0.05)
        p.fill(p.poly([(14, 66), (16, 40), (24, 36), (32, 38), (40, 36), (48, 40), (50, 66)]), dark, radius=6, depth=5,
               texture=0.04)
        for y in (44, 49, 54, 59):
            p.stroke([(20, y), (26, y + 2), (32, y + 1), (38, y + 2), (44, y)], dark[1])
            p.stroke([(20, y + 1), (26, y + 3), (32, y + 2), (38, y + 3), (44, y + 1)], dark[3])
        p.stroke([(32, 40), (32, 64)], dark[0])
        # the collar plate: bronze, gold filigree
        p.fill(p.poly([(14, 38), (20, 31), (32, 29), (44, 31), (50, 38), (40, 40), (32, 38), (24, 40)]), bronze, radius=3,
               depth=3, texture=0.04)
        for pts in ([(18, 35), (22, 33), (25, 35)], [(46, 35), (42, 33), (39, 35)], [(28, 32), (32, 31), (36, 32)]):
            p.stroke(pts, gold[3])
        # the neck: a dark cylinder with a ring
        p.fill(p.poly([(28, 24), (36, 24), (36, 31), (28, 31)]), dark, radius=2, depth=2, bias=-0.1)
        p.stroke([(28, 28), (36, 28)], bronze[2])
        # the helm: a tall narrow mitre leaning back to a point, filigreed, a ridge at the brow, a gem near the tip
        hm = [(26, 29), (22, 25), (22, 18), (25, 12), (30, 6), (36, 2), (41, 0), (44, 4), (45, 12), (44, 20), (41, 29)]
        p.fill(p.poly(hm), bronze, radius=6, depth=7, texture=0.05)
        p.stroke([(23, 17), (33, 16), (44, 14)], bronze[4])
        p.stroke([(23, 18), (33, 17), (44, 15)], bronze[1])
        for pts in ([(27, 12), (30, 9), (33, 10), (35, 7)], [(31, 14), (35, 12), (39, 13), (42, 10)], [(35, 5), (38, 3), (41, 3)],
                    [(38, 8), (41, 7), (43, 9)]):
            p.stroke(pts, gold[3])
        p.pixels([(29, 11), (40, 12), (37, 5)], gold[4])
        p.glow(41, 5, '#e8ffff', mix('#2ab0e8', '#8af0ff', port), r=1, strength=0.6 + 0.4 * port)
        # the face piece: a jutting vent below the ridge (no face), a port on its side
        p.fill(p.poly([(19, 21), (31, 20), (34, 24), (30, 29), (22, 28), (18, 24)]), bronze, radius=2.5, depth=3, bias=-0.08)
        for y in (23, 25, 27):
            p.stroke([(21, y), (30, y)], bronze[1])
        p.stroke([(21, 22), (30, 21)], bronze[4])
        # the ports where the aether leaks out
        for x, y in ((19, 24), (24, 36), (40, 36), (21, 46), (43, 48)):
            p.fill(p.ellipse(x, y, 2.2, 2.2), bronze, radius=1.2, depth=1.5, bias=0.1)
            p.glow(x, y, '#f0ffff', mix('#3ab8f0', '#a8f4ff', port), r=1, strength=0.7 + 0.3 * port)
    p.rim('#7ad8ff', side=1, amount=0.35)
    p.rim('#7ad8ff', side=-1, amount=0.25)
    p.outline('#0a0a0e')
    # the creature itself: aetheric streams pouring out of the ports, flowing
    h = hover
    for k, (ctrl, ph) in enumerate((([(19, 24 + h), (12, 22), (6, 12), (-4, 8)], 0.0), ([(19, 24 + h), (12, 28), (6, 20), (-4, 24)], 0.3),
                                     ([(44, 20 + h), (50, 22), (58, 18), (68, 20)], 0.15),
                                     ([(24, 36 + h), (16, 44), (10, 56), (6, 68)], 0.45), ([(24, 36 + h), (12, 38), (4, 46), (-4, 44)], 0.6),
                                     ([(40, 36 + h), (48, 42), (52, 54), (50, 68)], 0.7), ([(40, 36 + h), (52, 36), (60, 44), (68, 44)], 0.85),
                                     ([(21, 46 + h), (14, 52), (12, 60), (16, 68)], 0.2), ([(43, 48 + h), (52, 54), (58, 62), (68, 60)], 0.5))):
        sw = sway(t, ph, 2)
        ctrl = [ctrl[0], (ctrl[1][0], ctrl[1][1] + sw * 0.5), (ctrl[2][0] + sw, ctrl[2][1]), ctrl[3]]
        stream(p, bezier(ctrl, 48), t, ph=ph * 3, width=3 if k % 2 else 2, fade=0.3)
    return p


# ---------------------------------------------------------------------------------------------------------------
def mirveda(t=0.0):
    """after the owner's picture: a Mirveda - a pale-white human (neutral) in a white suit joined to a smooth white
    headpiece that covers the upper face: a V-shaped visor, glowing lenses at the temples, thin antennae sweeping up
    from them; a hooded high collar, a dark undersuit. Moves: breathing, the lenses pulse and scan, the antennae
    sway, a slow blink is hidden by the visor - the mouth stays still."""
    p = Portrait()
    painted_bg(p, (46, 52, 60), (128, 136, 146), 32, 12, 40, steps=6)
    white = ramp('#d6d8d8', spread=0.5, hue_shift=0.03)
    coat = ramp('#b4b8b6', spread=0.7, hue_shift=0.03)
    skin = ramp('#c8c4cc', spread=0.6, hue_shift=0.03)
    under = ramp('#3a3e44', spread=0.6)
    mech = ramp('#9aa0a6', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    lens = pulse(t, 0.0, 1)
    with p.at(0, body):
        # the coat: pale, a hooded high collar folded round the neck, a dark undersuit and a zip down the middle
        p.fill(p.poly([(0, 66), (2, 46), (12, 38), (22, 36), (32, 40), (42, 36), (52, 38), (62, 46), (64, 66)]), coat,
               radius=7, depth=7, texture=0.04)
        for pts in ([(14, 46), (17, 64)], [(50, 46), (47, 64)]):
            p.stroke(pts, coat[1])
        p.fill(p.poly([(24, 40), (40, 40), (42, 66), (22, 66)]), under, radius=3, depth=3)
        p.stroke([(32, 41), (32, 64)], mech[3])
        for y in range(43, 64, 3):
            p.px(31, y, mech[1]), p.px(33, y, mech[1])
        p.fill(p.poly([(16, 40), (19, 30), (24, 26), (27, 34), (26, 44), (21, 46)]), coat, radius=3, depth=3, bias=0.05)
        p.fill(p.poly([(48, 40), (45, 30), (40, 26), (37, 34), (38, 44), (43, 46)]), coat, radius=3, depth=3, bias=-0.12)
        p.stroke([(20, 32), (22, 42)], coat[1])
        p.stroke([(44, 32), (42, 42)], coat[1])
        for x, y in ((8, 48), (11, 47), (54, 49), (52, 51), (14, 56)):
            p.px(x, y, under[1])
        p.stroke([(18, 46), (22, 58)], under[2])
        p.stroke([(46, 46), (42, 58)], under[2])
    with p.at(0, head):
        # neck in the dark undersuit
        p.fill(p.poly([(27, 26), (37, 26), (37, 38), (27, 38)]), under, radius=2, depth=2, bias=-0.1)
        for y in (30, 33, 36):
            p.stroke([(28, y), (36, y)], under[3])
        # the pale lower face, in the visor's shadow at the top
        face = [(25, 15), (39, 15), (40, 22), (37, 28), (32, 30), (27, 28), (24, 22)]
        p.fill(p.poly(face), skin, radius=5, depth=4, texture=0.02)
        p.fill(p.poly([(25, 17), (39, 17), (38, 20), (26, 20)]), skin, flat=1, edge=False)
        p.stroke([(32, 21), (32, 24)], skin[3])
        p.px(31, 24, skin[1]), p.px(33, 24, skin[1])
        p.stroke([(30, 26), (34, 26)], (130, 110, 124))
        p.pixels([(31, 27), (32, 27), (33, 27)], skin[3])
        p.stroke([(26, 24), (28, 28)], skin[1])
        p.stroke([(38, 24), (36, 28)], skin[1])
        # the headpiece: a smooth white dome; a V-shaped visor down over the eyes, edged dark; blade-fins sweeping out
        p.fill(p.poly([(22, 13), (23, 6), (27, 2), (32, 1), (37, 2), (41, 6), (42, 13), (39, 17), (32, 20), (25, 17)]), white,
               radius=7, depth=6)
        p.stroke([(25, 17), (32, 20), (39, 17)], (40, 44, 50))
        p.stroke([(25, 18), (32, 21), (39, 18)], (70, 76, 84))
        p.stroke([(26, 15), (32, 18), (38, 15)], white[1])
        for side in (-1, 1):
            fin = [(32 + side * 3, 9), (32 + side * 13, 6), (32 + side * 20, 2), (32 + side * 12, 11)]
            p.fill(p.poly(fin), white, radius=1.5, depth=2, bias=0.1 if side < 0 else -0.08)
        # the temple modules with glowing lenses; thin antennae curving up and out (they don't meet)
        for side, ph in ((-1, 0.0), (1, 0.5)):
            cx = 32 + side * 11
            p.fill(p.ellipse(cx, 17, 3.4, 2.9), mech, radius=2, depth=2.5, bias=0.05 if side < 0 else -0.1)
            p.glow(cx, 17, '#e8ffff', mix('#2ab8d0', '#8af8ff', lens), r=1, strength=0.6 + 0.4 * lens)
            sw = sway(t, ph, 1)
            ant = bezier([(cx + side * 3, 15), (cx + side * 8, 12), (cx + side * 11 + sw, 6), (cx + side * 14 + sw, 1)], 16)
            p.stroke([(round(x), round(y)) for x, y in ant], mech[3])
            p.px(round(ant[-1][0]), round(ant[-1][1]), white[4])
        if blink(t, 0.3, 2):
            p.px(20, 17, '#ffffff'), p.px(44, 17, '#ffffff')
    p.rim('#a8e8ff', side=1, amount=0.3)
    p.rim('#ffffff', side=-1, amount=0.2)
    p.outline('#14181e')
    return p


# ---------------------------------------------------------------------------------------------------------------
def glitch(p, y0, y1, shift, tint=None, k=0.35):
    """a codec glitch (as in the first L1Z1X): one band of rows torn sideways and tinted - backdrop included"""
    y0, y1 = max(0, y0), min(p.h, y1)
    p.rgb[y0:y1] = np.roll(p.rgb[y0:y1], shift, axis=1)
    p.a[y0:y1] = np.roll(p.a[y0:y1], shift, axis=1)
    if p.bg is not None:
        p.bg[y0:y1] = np.roll(p.bg[y0:y1], shift, axis=1)
    if tint is not None:
        c = np.array(hexrgb(tint) if isinstance(tint, str) else tint, float)
        band = p.rgb[y0:y1].astype(float)
        m = p.a[y0:y1]
        band[m] = band[m] * (1 - k) + c * k
        p.rgb[y0:y1] = band.astype(np.uint8)
        if p.bg is not None:
            p.bg[y0:y1] = (p.bg[y0:y1].astype(float) * (1 - k * 0.6) + c * k * 0.6).astype(np.uint8)


def l1z1x(t=0.0):
    """after the owner's picture: a cybernetic Lazax - a gaunt grey-green Lazax face, skull-like, red eyes, under a
    wide dome headpiece ringed with red lights; cables hanging round the face; olive-gold mechanical plating, a
    mechanical arm across the chest, red tissue glowing in an open cavity; green mist. Moves: breathing, the eyes and
    lights pulse, the cables sway, and the first design's codec glitch tears the image once a loop."""
    t = t % 1.0
    p = Portrait()
    painted_bg(p, (12, 26, 16), (64, 104, 62), 22, 20, 46, steps=6)
    metal = ramp('#64623c', spread=0.75, hue_shift=0.03, sat_boost=-0.15)
    skin = ramp('#7c8c72', spread=0.75, hue_shift=0.04)
    dark = ramp('#262a22', spread=0.6)
    cloak = ramp('#24282a', spread=0.6)
    flesh = ramp('#a8243a', spread=0.6)
    cable = ramp('#a89a68', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    eye = pulse(t, 0.0, 2)
    red = mix('#c01020', '#ff5060', eye)
    with p.at(0, body):
        # the cloak behind
        p.fill(p.poly([(0, 66), (2, 40), (12, 30), (52, 30), (62, 40), (64, 66)]), cloak, radius=5, depth=4)
        # the chest cavity: red tissue and tubes, glowing
        p.fill(p.ellipse(32, 55, 9, 8), dark, radius=3, depth=2)
        p.fill(p.ellipse(32, 55, 7.5, 6.5), flesh, radius=4, depth=4, texture=0.2, bias=-0.1)
        for y in (51, 55, 59):
            p.stroke([(26, y), (32, y + 1), (38, y)], dark[1])
        p.stroke([(32, 49), (32, 61)], flesh[4])
        p.glow(32, 54, '#ffc0c8', mix('#c02030', '#ff6070', eye), r=1, strength=0.5 + 0.3 * eye)
        # olive-gold plating: wide shoulders, a chest plate split over the cavity
        p.fill(p.poly([(2, 50), (6, 38), (16, 34), (28, 36), (30, 44), (20, 48), (8, 52)]), metal, radius=4, depth=4, texture=0.04)
        p.fill(p.poly([(62, 50), (58, 38), (48, 34), (36, 36), (34, 44), (44, 48), (56, 52)]), metal, radius=4, depth=4,
               bias=-0.12, texture=0.04)
        p.fill(p.poly([(20, 38), (30, 36), (32, 46), (22, 47)]), metal, radius=2, depth=2, bias=0.1)
        p.fill(p.poly([(44, 38), (34, 36), (32, 46), (42, 47)]), metal, radius=2, depth=2, bias=-0.15)
        for pts in ([(8, 42), (16, 39)], [(48, 39), (56, 42)], [(22, 41), (29, 40)], [(35, 40), (42, 41)]):
            p.stroke(pts, metal[1])
        p.px(52, 44, red), p.px(12, 44, red)
        # the mechanical arm across the chest: forearm from the right, a clawed hand on the left breast
        arm = bezier([(60, 56), (48, 50), (34, 46), (24, 44)], 16)
        p.fill(p.poly(ribbon(arm, 5, 4)), metal, radius=2, depth=2.5, bias=0.05)
        for j in range(3, len(arm) - 2, 4):
            x, y = arm[j]
            p.stroke([(round(x), round(y) - 2), (round(x), round(y) + 2)], metal[1])
        for k in range(4):
            p.stroke([(25 - k, 42 + k), (19 - k, 40 + k * 2)], dark[3])
            p.px(19 - k, 40 + k * 2, metal[4])
        p.glow(40, 48, '#ffe0a0', '#ffa020', r=0, strength=0.6)
    with p.at(0, head):
        # the neck: mechanical vertebrae
        p.fill(p.poly([(28, 28), (36, 28), (36, 38), (28, 38)]), dark, radius=2, depth=2)
        for y in (30, 33, 36):
            p.stroke([(28, y), (36, y)], metal[2])
        # tubes hanging from under the headdress's rim, down to the shoulders
        for k, (x0, x1, ph) in enumerate(((10, 6, 0.0), (14, 12, 0.3), (19, 18, 0.55), (54, 58, 0.15), (50, 52, 0.45),
                                          (45, 46, 0.7))):
            sw = sway(t, ph, 1)
            path = bezier([(x0, 20), (x0 + sw, 28), (x1 + sw, 38)], 12)
            p.stroke([(round(x), round(y)) for x, y in path], cable[1 + k % 3])
        # the gaunt Lazax face, fused under the headdress: hollow cheeks, deep red-lit sockets, a lipless mouth
        face = [(24, 15), (40, 15), (42, 21), (40, 29), (37, 35), (32, 37), (27, 35), (24, 29), (22, 21)]
        p.fill(p.poly(face), skin, radius=6, depth=6, texture=0.05)
        p.fill(p.poly([(24, 22), (28, 25), (28, 31), (25, 29)]), skin, flat=1, edge=False)
        p.fill(p.poly([(40, 22), (36, 25), (36, 31), (39, 29)]), skin, flat=0, edge=False)
        for ex in (27.5, 36.5):
            p.fill(p.poly([(ex - 3, 19), (ex + 3, 19), (ex + 2.5, 23), (ex - 2.5, 23)]), dark, flat=0, edge=False)
            p.glow(int(ex), 21, '#ffd0d0', red, r=0, strength=0.9)
        p.stroke([(32, 22), (32, 27)], skin[3])
        p.px(31, 28, dark[0]), p.px(33, 28, dark[0])
        p.fill(p.poly([(28, 30), (36, 30), (35, 34), (29, 34)]), dark, flat=0, edge=False)
        for x in range(28, 37):
            p.px(x, 31, skin[4] if x % 2 == 0 else skin[2])
            p.px(x, 33, skin[3] if x % 2 == 0 else skin[1])
        p.stroke([(26, 33), (29, 36)], skin[1])
        p.stroke([(38, 33), (35, 36)], skin[1])
        # the headdress: a great segmented dome fused to the skull, flaring into a wide rim round the face
        dome = ramp('#5a5638', spread=0.8, hue_shift=0.03, sat_boost=-0.1)
        top = bezier([(3, 23), (4, 4), (18, -8), (32, -9)], 18) + bezier([(32, -9), (46, -8), (60, 4), (61, 23)], 18)[1:]
        rim = [(55, 23), (46, 19), (38, 17), (32, 17), (26, 17), (18, 19), (9, 23)]
        p.fill(p.poly(top + rim), dome, radius=13, depth=12, texture=0.04, bias=-0.22)
        # its dark underside where it meets the head, ports along the rim
        p.fill(p.poly([(3, 21), (10, 21), (20, 18), (32, 16), (44, 18), (54, 21), (61, 21), (56, 24), (46, 20), (38, 18),
                       (26, 18), (18, 20), (8, 24)]), dark, flat=1, edge=False)
        # seams curving from the crown to the rim, a central ridge, rivets
        for x0 in (-16, -9, 9, 16):
            seam = bezier([(32 + x0 * 0.3, -4), (32 + x0 * 0.8, 4), (32 + x0 * 1.45, 18)], 12)
            p.stroke([(round(x), round(y)) for x, y in seam], dome[0])
            p.pixels([(round(x) + (1 if x0 < 0 else -1), round(y)) for x, y in seam[2::3]], dome[3])
        p.stroke([(32, -3), (32, 15)], dome[4])
        p.stroke([(33, -3), (33, 15)], dome[1])
        for x, y in ((9, 16), (55, 16), (14, 8), (50, 8), (24, 3), (40, 3)):
            p.px(x, y, dome[4])
        # the forehead plate where dome and skull join
        p.fill(p.poly([(25, 15), (39, 15), (38, 19), (26, 19)]), metal, radius=1.5, depth=2, bias=-0.05)
        p.stroke([(26, 18), (38, 18)], metal[0])
        # red lenses: two big ones flanking the face on the rim, a ring of small lights over the dome
        for x, y in ((15, 18), (49, 18)):
            p.fill(p.ellipse(x, y, 2.8, 2.8), dome, radius=1.5, depth=2, bias=0.1)
            p.glow(x, y, mix('#ff6060', '#ffc0c0', eye), red, r=1, strength=0.75 + 0.25 * eye)
        for i, (x, y) in enumerate(((8, 14), (13, 7), (20, 2), (27, 0), (37, 0), (44, 2), (51, 7), (56, 14))):
            k = pulse(t, i / 8, 1)
            p.glow(x, y, mix('#c02030', '#ff6a6a', k), mix('#600810', '#e02030', k), r=0, strength=0.55 + 0.4 * k)
    p.rim('#ff2a3a', side=1, amount=0.4)
    p.rim('#9ad870', side=-1, amount=0.25)
    p.outline('#060806')
    # the first design's glitches: a tear across the face and the chest, then an aftershock
    if blink(t, 0.42, 1):
        glitch(p, 19 + head, 26 + head, 2, '#ff2040', 0.3)
        glitch(p, 48, 53, -2, '#40e0ff', 0.25)
    elif blink(t, 0.46, 1):
        glitch(p, 6 + head, 10 + head, -1, '#40e0ff', 0.2)
    return p


# ---------------------------------------------------------------------------------------------------------------
_NOISE = np.random.default_rng(21).random((64, 64))


def _vnoise(x, y):
    """smooth value noise on a 64x64 wrapping lattice"""
    xi, yi = np.floor(x).astype(int), np.floor(y).astype(int)
    xf, yf = x - xi, y - yi
    xf, yf = xf * xf * (3 - 2 * xf), yf * yf * (3 - 2 * yf)
    def g(i, j):
        return _NOISE[(yi + j) % 64, (xi + i) % 64]
    return (g(0, 0) * (1 - xf) + g(1, 0) * xf) * (1 - yf) + (g(0, 1) * (1 - xf) + g(1, 1) * xf) * yf


def mortheus(t=0.0):
    """after the owner's picture: the Glimmer of Mortheus - simply a nebula: billowing violet and blue-grey clouds
    with golden edges, a glowing cyan-green heart, twinkling stars inside. Moves: the clouds drift and turn
    slowly, the heart breathes light, the stars twinkle."""
    p = Portrait()
    yy, xx = np.mgrid[0:64, 0:64].astype(float)
    a = 2 * math.pi * t
    ox, oy = 3.0 * math.cos(a), 3.0 * math.sin(a)  # a circle through the noise: a seamless loop

    def fbm(x, y):
        v, amp, f = 0.0, 0.62, 1.0
        for o in range(3):
            v = v + amp * _vnoise(x * f / 16 + ox * f / 10 + o * 7.3, y * f / 16 + oy * f / 10 + o * 3.1)
            amp *= 0.42
            f *= 2.1
        return v
    # swirl the sample space round the heart, slowly
    cx, cy = 26.0, 28.0
    dx, dy = xx - cx, yy - cy
    r = np.sqrt(dx * dx + dy * dy)
    ang = 0.9 * np.exp(-r / 26) + 0.12 * math.sin(a)
    sx = cx + dx * np.cos(ang) - dy * np.sin(ang)
    sy = cy + dx * np.sin(ang) + dy * np.cos(ang)
    h = fbm(sx, sy)
    # a band of cloud from lower left to upper right, thinning into dark space at the corners
    band = 1 - np.clip(np.abs((yy - 64 + xx * 0.9) / 30.0 + 0.55), 0, 1)
    d = np.clip(h * 1.6 - 0.62 + band * 0.55, 0, 1)
    # light the billows from the upper right: the golden rims
    hx = np.roll(h, 1, axis=1) - h
    hy = np.roll(h, -1, axis=0) - h
    rimk = np.clip((hx + hy) * 16 - 0.1, 0, 1) * (d > 0.32)
    space, violet, blue, pale, gold = (np.array(c, float) for c in ((6, 6, 18), (70, 46, 120), (86, 100, 140), (196, 186, 170),
                                                                      (232, 178, 80)))
    col = np.where(d[..., None] < 0.35, space + (violet - space) * (d[..., None] / 0.35),
                   np.where(d[..., None] < 0.65, violet + (blue - violet) * ((d[..., None] - 0.35) / 0.3),
                            blue + (pale - blue) * ((d[..., None] - 0.65) / 0.35)))
    col = col + (gold - col) * (rimk[..., None] * 0.7)
    # the heart: a cyan-green glow that breathes
    g = pulse(t, 0.0, 1)
    hk = np.clip(1 - r / (13 + 2 * g), 0, 1) ** 1.4
    col = col + (np.array((110, 255, 200)) - col) * (hk[..., None] * (0.75 + 0.2 * g))
    # posterise with an ordered dither for the pixel look
    q = 22.0
    col = np.floor(col / q + (BAYER[yy.astype(int) % 4, xx.astype(int) % 4][..., None] - 0.5) * 0.6 + 0.5) * q
    p.bg = col.clip(0, 255).astype(np.uint8)
    p.backdrop = (6, 6, 18)
    # twinkling stars inside the cloud and in the dark around it
    rng = np.random.default_rng(13)
    for _ in range(22):
        x, y = int(rng.integers(0, 64)), int(rng.integers(0, 64))
        ph, big = float(rng.random()), rng.random() < 0.3
        k = pulse(t, ph, 2 if big else 1)
        if k > 0.25:
            p.px(x, y, mix((120, 130, 170), (255, 255, 240), k))
            if big and k > 0.7:
                for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    p.px(x + ddx, y + ddy, mix((80, 90, 130), (220, 230, 255), k * 0.7))
    return p


# ---------------------------------------------------------------------------------------------------------------
def mentak(t=0.0):
    """after the owner's picture: a Mentak pirate captain - a woman, pale lavender skin, dark hair slicked back, a
    bronze cybernetic plate on the shaved side of her scalp (and, as a pirate liberty, a small cyber-lens over one
    eye), a dark high collar, a mustard jumpsuit with navy panels, a massive mechanical arm glowing green, a rifle
    over her shoulder and a ragged cape; lilac haze with masked crew behind. Neutral.
    Moves: breathing, a blink, the implant and lens pulse, the arm's green glow throbs, the cape stirs."""
    p = Portrait()
    painted_bg(p, (58, 44, 76), (176, 156, 196), 12, 6, 50, steps=6)
    for cx, cy in ((7, 34), (15, 30)):  # masked crew in the haze behind
        for y in range(cy - 6, 64):
            for x in range(cx - 6, cx + 7):
                d = ((x - cx) / 5.5) ** 2 + ((y - cy) / 6.0) ** 2
                if d <= 1 or (y > cy + 4 and abs(x - cx) < 7):
                    bg_px(p, x, y, (74, 58, 92) if (x + y) % 5 else (64, 50, 82))
        bg_px(p, cx - 2, cy, (110, 90, 130)), bg_px(p, cx + 2, cy, (110, 90, 130))
    skin = ramp('#a898ba', spread=0.68, hue_shift=0.04)
    hair = ramp('#2c2430', spread=0.6)
    suit = ramp('#9c8440', spread=0.68, hue_shift=0.04, sat_boost=-0.1)
    navy = ramp('#2a2c4a', spread=0.65)
    metal = ramp('#4a4c54', spread=0.75)
    bronze = ramp('#a07840', spread=0.7, hue_shift=0.05)
    cape = ramp('#7a6040', spread=0.7)
    green = pulse(t, 0.0, 2)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the rifle resting on her shoulder, pointing up behind her
        gun = bezier([(38, 30), (48, 18), (60, 6)], 12)
        p.fill(p.poly(ribbon(gun, 6, 5)), metal, radius=2, depth=2.5, bias=0.05)
        p.fill(p.poly([(50, 8), (56, 4), (60, 7), (54, 12)]), metal, radius=1.5, depth=2, bias=0.15)
        p.px(52, 14, mix('#2a8a40', '#8aff9a', green))
        # the ragged cape over her right shoulder (viewer's right)
        cp = [(40, 34), (52, 32), (60, 40), (62, 66), (58, 60), (55, 66), (51, 58), (47, 66), (44, 50)]
        p.fill(p.poly([(x + (sway(t, 0.2, 1) if y > 50 else 0), y) for x, y in cp]), cape, radius=4, depth=3, texture=0.08)
        # the jumpsuit: mustard with navy side panels, a belt
        p.fill(p.poly([(12, 66), (14, 42), (24, 36), (32, 38), (40, 36), (50, 42), (52, 66)]), suit, radius=6, depth=5,
               texture=0.04)
        p.fill(p.poly([(14, 46), (22, 42), (24, 66), (14, 66)]), navy, radius=2, depth=2)
        p.fill(p.poly([(50, 46), (42, 42), (40, 66), (50, 66)]), navy, radius=2, depth=2, bias=-0.1)
        p.stroke([(24, 60), (40, 60)], navy[0])
        p.stroke([(24, 61), (40, 61)], suit[1])
        # the massive mechanical arm on her left (viewer's left): pauldron, pistons, cables, a green glow
        p.fill(p.poly([(0, 66), (0, 40), (6, 33), (16, 31), (24, 36), (24, 48), (16, 56), (10, 66)]), metal, radius=5, depth=5,
               texture=0.05)
        p.fill(p.ellipse(13, 38, 7, 5.5), metal, radius=3, depth=4, bias=0.1)
        p.stroke([(6, 42), (4, 60)], metal[4])
        p.stroke([(18, 44), (14, 62)], metal[1])
        for y in (46, 52):
            p.stroke([(4, y), (18, y - 1)], metal[0])
        p.stroke([(20, 34), (24, 28), (26, 24)], navy[2])  # cables up to the collar
        p.glow(12, 50, '#e8ffe8', mix('#1a8a30', '#5aff7a', green), r=1, strength=0.6 + 0.4 * green)
        p.glow(8, 58, '#e8ffe8', mix('#1a8a30', '#5aff7a', green), r=0, strength=0.6)
    with p.at(0, head):
        # the dark high collar wrapped round her neck
        p.fill(p.poly([(23, 24), (37, 24), (40, 36), (32, 39), (22, 36)]), navy, radius=3, depth=3)
        p.stroke([(24, 29), (38, 29)], navy[3])
        p.stroke([(24, 33), (38, 33)], navy[1])
        # her face: pale lavender, strong cheekbones, dark-lined eyes, plum lips (neutral)
        face = [(24, 9), (30, 6), (37, 8), (39, 14), (38, 21), (35, 26), (31, 28), (27, 26), (24, 21), (23, 15)]
        p.fill(p.poly(face), skin, radius=6, depth=5, texture=0.02)
        p.fill(p.poly([(34, 15), (39, 14), (38, 21), (35, 26), (33, 25), (35, 19)]), skin, flat=1, edge=False)
        if blink(t, 0.6, 2):
            p.stroke([(26, 15), (29, 15)], hair[1]), p.stroke([(33, 15), (36, 15)], hair[1])
        else:
            p.stroke([(25, 14), (29, 14)], hair[0]), p.stroke([(33, 14), (37, 14)], hair[0])
            p.px(26, 15, (230, 224, 236)), p.px(27, 15, (60, 40, 70)), p.px(28, 15, (40, 30, 46))
            p.px(34, 15, (40, 30, 46)), p.px(35, 15, (60, 40, 70)), p.px(36, 15, (230, 224, 236))
        p.stroke([(25, 12), (29, 12)], hair[1]), p.stroke([(33, 12), (37, 12)], hair[1])
        p.stroke([(31, 15), (31, 20)], skin[3])
        p.px(30, 21, skin[1]), p.px(32, 21, skin[1])
        p.stroke([(29, 24), (33, 24)], (110, 50, 90))
        p.pixels([(30, 25), (31, 25), (32, 25)], (140, 76, 116))
        p.pixels([(25, 18), (26, 19)], skin[4])
        # the cyber-lens over her right eye (viewer's right): a small amber ring
        lk = pulse(t, 0.3, 1)
        p.fill(p.ellipse(35.5, 15, 2.6, 2.4), metal, radius=1, depth=1.5, edge=False, bias=0.1)
        p.px(35, 15, mix('#c07010', '#ffd060', lk)), p.px(36, 15, mix('#804008', '#ffb030', lk))
        p.stroke([(38, 14), (40, 12)], metal[3])
        # dark hair slicked back, the left side shaved under a bronze cyber-plate with a light
        p.fill(p.poly([(24, 9), (27, 3), (33, 1), (39, 3), (41, 9), (40, 16), (38, 9), (31, 7), (26, 10)]), hair, radius=3,
               depth=3, texture=0.05)
        for x in (29, 32, 35, 38):
            p.stroke([(x - 1, 3), (x + 1, 8)], hair[2])
        p.fill(p.poly([(21, 8), (25, 4), (28, 6), (26, 12), (23, 16), (21, 13)]), bronze, radius=1.5, depth=2, bias=0.1)
        p.stroke([(23, 8), (25, 6)], bronze[4])
        p.glow(23, 11, '#e8ffe8', mix('#1a8a30', '#5aff7a', green), r=0, strength=0.7)
        p.fill(p.ellipse(23.5, 17, 1.5, 2.6), skin, radius=1, depth=1.5, bias=-0.1)  # ear
    p.rim('#e0c8ff', side=-1, amount=0.3)
    p.rim('#9affb0', side=1, amount=0.25)
    p.outline('#100c16')
    return p


# ---------------------------------------------------------------------------------------------------------------
def tnelis(t=0.0):
    """after the owner's picture: a Tnelis crime family - the boss seated in front, straight-faced (he means
    business): short grey hair, a blue suit with gold epaulettes, white collar, a red tie; behind him his two
    enforcers - a fleshy tendril-mouthed alien in a trench coat (left) and a green reptile with glowing eyes in a
    fur-collared jacket (right) - under a pale moon in the dark and mist. Moves: the boss breathes and blinks
    slowly, the enforcers' eyes glow, the tendrils stir, mist drifts, the moon glows."""
    p = Portrait()
    painted_bg(p, (10, 12, 18), (46, 52, 64), 40, 8, 36, steps=6)
    mk = pulse(t, 0.0, 1)
    for dx in range(-3, 4):  # the moon
        for dy in range(-3, 4):
            d = math.hypot(dx, dy)
            if d <= 3.2:
                bg_px(p, 44 + dx, 7 + dy, mix((180, 170, 140), (255, 246, 214), max(0.0, 1 - d / 3.2) * (0.7 + 0.3 * mk)))
    flesh = ramp('#9a7478', spread=0.7, hue_shift=0.04)
    coat = ramp('#6a5a46', spread=0.7)
    rept = ramp('#4c6a3c', spread=0.75, hue_shift=0.05)
    jacket = ramp('#2e2a2c', spread=0.6)
    furc = ramp('#c8c0b4', spread=0.5)
    skin = ramp('#c4a490', spread=0.65, hue_shift=0.04)
    suit = ramp('#25365c', spread=0.75)
    gold = ramp('#c89a3a', spread=0.7, hue_shift=0.05)
    grey = ramp('#9c9a98', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    glow = pulse(t, 0.3, 2)
    # --- the left enforcer: fleshy ridged head, red eyes, a beard of tendrils; a trench coat with the collar up
    with p.at(0, breath(t, 0.4)):
        p.fill(p.poly([(-2, 66), (0, 32), (8, 26), (20, 28), (26, 38), (22, 66)]), coat, radius=5, depth=4, texture=0.05)
        p.fill(p.poly([(2, 30), (8, 22), (12, 30), (8, 40)]), coat, radius=2, depth=2, bias=0.1)
        p.fill(p.poly([(22, 30), (18, 22), (14, 30), (18, 40)]), coat, radius=2, depth=2, bias=-0.1)
        hd = [(6, 12), (10, 6), (16, 4), (22, 7), (24, 13), (22, 20), (16, 22), (10, 20)]
        p.fill(p.poly(hd), flesh, radius=5, depth=5, texture=0.1)
        for k in range(4):
            p.stroke([(9 + k * 3, 6 + abs(k - 1.5)), (10 + k * 3, 12)], flesh[1])
        for ex in (12, 18):
            p.glow(ex, 14, '#ffd0d0', mix('#a01020', '#ff3040', glow), r=0, strength=0.9)
        for k, x0 in enumerate((11, 13, 15, 17, 19)):
            sw = sway(t, k * 0.2, 1)
            p.stroke([(x0, 19), (x0 + sw, 24), (x0, 27)], flesh[1 + k % 2])
    # --- the right enforcer: a green reptile, glowing yellow eyes, a dark jacket with a white fur collar
    with p.at(0, breath(t, 0.7)):
        p.fill(p.poly([(42, 66), (40, 34), (48, 28), (60, 30), (66, 40), (66, 66)]), jacket, radius=5, depth=4)
        p.fill(p.poly([(40, 32), (46, 26), (54, 28), (62, 26), (64, 34), (56, 36), (46, 36)]), furc, radius=3, depth=3,
               texture=0.12)
        hd = [(43, 15), (45, 9), (51, 6), (57, 8), (60, 13), (59, 19), (56, 24), (51, 27), (47, 25), (44, 21)]
        p.fill(p.poly(hd), rept, radius=4, depth=5, texture=0.08, bias=-0.05)
        p.fill(p.poly([(44, 14), (51, 16), (59, 13), (59, 16), (51, 19), (44, 17)]), rept, radius=1.5, depth=2, bias=0.2)
        p.stroke([(45, 13), (51, 15), (58, 12)], rept[0])
        for ex in (48, 55):
            p.px(ex - 1, 17, rept[0])
            p.glow(ex, 17, '#f8ffd0', mix('#8ac020', '#e8ff40', glow), r=0, strength=0.9)
        p.pixels([(50, 22), (53, 22)], rept[0])
        p.stroke([(48, 24), (55, 24)], rept[0])
        p.pixels([(46, 10), (49, 8), (54, 8)], rept[4])
    # --- the boss, seated in front: blue suit, gold epaulettes, white collar, red tie, medals
    with p.at(0, body):
        p.fill(p.poly([(6, 66), (8, 48), (18, 42), (28, 40), (32, 44), (36, 40), (46, 42), (56, 48), (58, 66)]), suit, radius=7,
               depth=6, texture=0.04)
        for side in (-1, 1):
            cx = 32 + side * 17
            p.fill(p.poly([(cx - 6, 43), (cx + 6, 43 - side), (cx + 6, 46), (cx - 6, 46)]), gold, radius=1.2, depth=1.5)
            for x in range(cx - 5, cx + 6, 2):
                p.stroke([(x, 47), (x, 48)], gold[2])
        p.fill(p.poly([(27, 40), (32, 46), (37, 40), (36, 52), (32, 58), (28, 52)]), ramp('#e4e2dc', spread=0.4), radius=2,
               depth=2)
        p.fill(p.poly([(31, 45), (33, 45), (34, 55), (32, 58), (30, 55)]), ramp('#8a1a24', spread=0.5), radius=1.5, depth=2)
        p.stroke([(27, 41), (24, 58)], suit[0])
        p.stroke([(37, 41), (40, 58)], suit[0])
        for x, y, c in ((18, 52, (200, 60, 60)), (20, 52, (60, 120, 200)), (44, 52, (220, 190, 70)), (46, 52, (200, 60, 60))):
            p.px(x, y, c), p.px(x, y + 1, c)
        p.fill(p.ellipse(19, 56, 1.8, 1.8), gold, radius=1, depth=1)
    with p.at(0, head):
        p.fill(p.poly([(28, 34), (36, 34), (36, 42), (28, 42)]), skin, radius=2, depth=2, bias=-0.25)
        # the boss's face: older, hard, straight-faced
        face = [(25, 22), (32, 19), (39, 22), (40, 29), (38, 35), (34, 39), (30, 39), (26, 35), (24, 29)]
        p.fill(p.poly(face), skin, radius=6, depth=5, texture=0.04)
        p.fill(p.poly([(34, 27), (40, 27), (38, 35), (34, 38), (33, 33)]), skin, flat=1, edge=False)
        # short grey hair, receding
        p.fill(p.poly([(24, 24), (26, 20), (32, 18), (38, 20), (40, 24), (38, 23), (32, 21), (26, 23)]), grey, radius=1.5,
               depth=2, texture=0.1)
        # heavy level brows, eyes, the nose, a hard straight mouth
        p.stroke([(26, 26), (30, 26)], grey[0])
        p.stroke([(34, 26), (38, 26)], grey[0])
        if blink(t, 0.7, 2):
            p.stroke([(27, 28), (29, 28)], skin[1]), p.stroke([(35, 28), (37, 28)], skin[0])
        else:
            for ex in (27, 35):
                p.px(ex, 28, (230, 226, 220)), p.px(ex + 1, 28, (50, 60, 70)), p.px(ex + 2, 28, (230, 226, 220))
        p.stroke([(32, 28), (32, 32)], skin[3])
        p.px(31, 33, skin[1]), p.px(33, 33, skin[1])
        p.stroke([(29, 35), (35, 35)], (110, 64, 60))
        p.stroke([(27, 31), (28, 34)], skin[1])
        p.stroke([(37, 31), (36, 34)], skin[0])
        for ex in (24.5, 39.5):
            p.fill(p.ellipse(ex, 29, 1.4, 2.4), skin, radius=1, depth=1, bias=-0.1)
    p.rim('#c8d8ff', side=1, amount=0.35)
    p.rim('#ff7a6a', side=-1, amount=0.2)
    p.outline('#050608')
    # mist drifting along the bottom
    for k in range(2):
        y0 = 58 + k * 3
        off = int(((t + k * 0.5) % 1.0) * 64)
        for x in range(64):
            if (x + off) % 64 < 30 and (x + y0) % 2 == 0:
                c = (150, 156, 170)
                p.rgb[y0, x] = (p.rgb[y0, x] * 0.6 + np.array(c) * 0.4).astype(np.uint8)
                p.a[y0, x] = True
    return p


# ---------------------------------------------------------------------------------------------------------------
def naalu(t=0.0):
    """after the owner's picture: a Naalu naga - a great fan-shaped web array rising from her head, tan scaled skin,
    a long ridged throat, long dark claws raised beside her, misty green ruins behind. Pompous: head tilted down,
    half-lidded eyes looking down at the viewer, a superior grin. Moves: the web array ripples with light,
    breathing, the claws flex, the eyes glow, a forked tongue flicks once."""
    p = Portrait()
    painted_bg(p, (30, 40, 34), (126, 144, 124), 32, 10, 46, steps=6)
    for x0, w in ((4, 4), (16, 3), (50, 4), (60, 3)):  # columns in the mist
        for y in range(64):
            for dx in range(w):
                if (y + dx) % 3:
                    bg_px(p, x0 + dx, y, (58 + dx * 6, 72 + dx * 6, 62 + dx * 5))
    skin = ramp('#9e7656', spread=0.72, hue_shift=0.05)
    fan = ramp('#d6a83a', spread=0.6, hue_shift=0.05)
    rib = ramp('#3e2c1e', spread=0.5)
    claw = ramp('#2e2624', spread=0.6)
    gold = ramp('#d0a03a', spread=0.7, hue_shift=0.05)
    body, head = breath(t), breath(t, -0.1)
    ripple = t
    with p.at(0, head):
        # the web array: a fan of translucent membrane on dark ribs, rising behind the head
        fanpts = [(32 + 22 * math.cos(a), 17 - 21 * math.sin(a)) for a in [math.pi * i / 30 for i in range(31)]]
        fm = p.fill(p.poly(fanpts + [(32, 19)]), fan, radius=6, depth=4, bias=0.1, texture=0.03)
        for i in range(9):
            a = math.pi * (0.06 + i * 0.11)
            ex, ey = 32 + 21 * math.cos(a), 17 - 20 * math.sin(a)
            p.stroke([(32, 17), (round(ex), round(ey))], rib[1])
            k = pulse(ripple, i / 9, 1)  # light runs across the membrane rib by rib
            mx, my = 32 + 14 * math.cos(a + 0.055), 17 - 13 * math.sin(a + 0.055)
            p.px(round(mx), round(my), mix(fan[3], '#fff4b0', k))
        arc = [(32 + 21.5 * math.cos(a), 17 - 20.5 * math.sin(a)) for a in [math.pi * i / 40 for i in range(41)]]
        p.stroke([(round(x), round(y)) for x, y in arc], rib[0])
    with p.at(0, body):
        # shoulders and the upper chest, a gold collar of plates
        p.fill(p.poly([(4, 66), (6, 50), (16, 42), (26, 40), (38, 40), (48, 42), (58, 50), (60, 66)]), skin, radius=7, depth=6,
               texture=0.06)
        p.fill(p.poly([(16, 50), (24, 44), (32, 46), (40, 44), (48, 50), (44, 58), (32, 62), (20, 58)]), gold, radius=3, depth=3,
               texture=0.04)
        for x in range(20, 46, 4):
            p.stroke([(x, 50), (x + (x - 32) // 8, 57)], gold[1])
        p.glow(32, 55, '#d0ffe0', '#30b070', r=1, strength=0.7)
        # scales over the shoulders
        for y in range(46, 64, 3):
            for x in range(6 + (y % 2) * 2, 60, 4):
                if p.a[min(63, y + body), x] and not (16 <= x <= 48 and y < 60):
                    p.px(x, y, skin[1])
    with p.at(0, head):
        # the neck with ridged throat scales
        p.fill(p.poly([(27, 30), (37, 30), (39, 44), (25, 44)]), skin, radius=3, depth=3, bias=0.1)
        for y in range(31, 44, 3):
            p.stroke([(29, y), (35, y)], skin[1])
        # the head, tilted down: a narrow skull, high cheekbones, a pointed chin
        hd = [(24, 12), (28, 7), (36, 7), (40, 12), (41, 19), (39, 26), (35, 31), (32, 33), (29, 31), (25, 26), (23, 19)]
        p.fill(p.poly(hd), skin, radius=6, depth=6, texture=0.04)
        p.fill(p.poly([(36, 18), (41, 18), (39, 26), (35, 31), (34, 26)]), skin, flat=1, edge=False)
        p.stroke([(26, 9), (32, 8), (38, 9)], skin[4])
        p.pixels([(26, 20), (27, 21)], skin[4])
        # arched scaled brows, half-lidded eyes looking down (pupils low), glowing
        g = pulse(t, 0.2, 2)
        iris = mix('#a0c020', '#e8ff60', g)
        for ex, d in ((25, 1), (39, -1)):
            p.stroke([(ex, 14), (ex + d * 2, 13), (ex + d * 5, 14)], skin[0])
            p.stroke([(ex, 16), (ex + d * 5, 16)], skin[0])
            p.px(ex + d, 17, iris), p.px(ex + d * 2, 17, (24, 22, 8)), p.px(ex + d * 3, 17, (24, 22, 8)), p.px(ex + d * 4, 17, iris)
            p.stroke([(ex + d, 18), (ex + d * 4, 18)], skin[1])
        p.stroke([(32, 18), (32, 22)], skin[3])
        p.px(31, 23, skin[0]), p.px(33, 23, skin[0])
        # the superior grin: wide, corners drawn up, a sliver of small sharp teeth
        p.stroke([(27, 25), (37, 25)], (64, 30, 30))
        p.px(26, 24, (64, 30, 30)), p.px(38, 24, (64, 30, 30))
        p.pixels([(x, 26) for x in range(28, 37)], (236, 228, 210))
        p.stroke([(28, 27), (36, 27)], (64, 30, 30))
        p.fill(p.ellipse(32, 31.0, 3.4, 2.4), skin, flat=2, edge=False)
        p.pixels([(31, 30), (32, 30)], skin[3])
        p.stroke([(30, 33), (34, 33)], skin[1])
        p.pixels([(29, 28), (32, 28), (35, 28)], skin[3])
        p.pixels([(25, 23), (39, 23)], skin[1])
        if blink(t, 0.5, 2):  # a forked tongue flicks out
            p.px(32, 28, (200, 60, 90)), p.px(32, 29, (200, 60, 90)), p.px(31, 30, (200, 60, 90)), p.px(33, 30, (200, 60, 90))
    p.rim('#f0ffb0', side=-1, amount=0.3)
    p.rim('#9ad8a0', side=1, amount=0.3)
    with p.at(0, body):
        # the raised hand beside her: long fingers ending in long curved claws (they flex)
        claw = ramp('#2a2422', spread=0.75)
        p.fill(p.poly(ribbon(bezier([(2, 66), (4, 54), (9, 46)], 10), 7, 6)), skin, radius=2.5, depth=3, texture=0.05)
        p.fill(p.ellipse(10, 44, 4.5, 3.8), skin, radius=2, depth=2.5)
        for k in range(4):
            fl = sway(t, k * 0.12, 1)
            bx = 5 + k * 3.4
            path = bezier([(bx, 42), (bx - 1 + k * 0.5, 34), (bx + k * 0.6 + fl, 26), (bx + 4 + k + fl, 21)], 14)
            p.fill(p.poly(ribbon(path[:7], 2.4, 1.8)), skin, radius=1, depth=1.4, bias=0.25, edge=False)
            p.fill(p.poly(ribbon(path[6:], 2.2, 0.8)), claw, radius=1, depth=1.6, bias=0.1)
            p.px(round(path[-1][0]), round(path[-1][1]), claw[4])
    p.outline('#0a0e0a')
    return p


# ---------------------------------------------------------------------------------------------------------------
def zelian(t=0.0):
    """after the owner's picture: a Zelian Purifier - a sentient asteroid: one giant rough space rock, cratered and
    craggy, in a storm of smaller rocks orbiting it, a red nebula swirling behind. Moves: the storm of rocks orbits
    (passing in front of and behind it), dust drifts, the nebula glows."""
    p = Portrait()
    painted_bg(p, (36, 8, 8), (176, 64, 44), 34, 30, 40, steps=7)
    yy, xx = np.mgrid[0:64, 0:64]
    for k in range(3):  # a swirl band of red dust behind
        for x in range(64):
            y = int(round(34 + (x - 32) * 0.28 + 6 * math.sin(x / 9 + k * 2 + t * 2 * math.pi) + (k - 1) * 4))
            if 0 <= y < 64 and (x + k) % 2 == 0:
                bg_px(p, x, y, (150 + k * 20, 60 + k * 10, 40))
    cx, cy, R = 32.0, 31.0, 18.0
    # the rocks of the storm: an inclined ring; each rock (angle, radius, size, speed)
    rng = np.random.default_rng(17)
    rocks = []
    for i in range(34):
        a0 = rng.uniform(0, 2 * math.pi)
        rr = rng.uniform(R + 3, R + 15)
        sz = rng.uniform(1.3, 2.6) if i % 4 else rng.uniform(2.8, 4.0)
        rocks.append((a0, rr, sz, 1 if i % 3 else 2, int(rng.integers(0, 1000)), rng.uniform(-9, 9)))
    rock_r = ramp('#4c423e', spread=0.85, hue_shift=0.04)
    tilt = 0.78

    def draw_rocks(front):
        for a0, rr, sz, sp, seed, lift in rocks:
            a = a0 + 2 * math.pi * t * sp * (0.5 if rr > R + 9 else 1)
            x = cx + rr * math.cos(a)
            y = cy + rr * math.sin(a) * tilt - (x - cx) * 0.18 + lift * math.cos(a)
            infront = math.sin(a) > 0
            if infront != front:
                continue
            if not front and math.hypot(x - cx, y - cy) < R - 1:
                continue
            rr_ = np.random.default_rng(seed)
            pts = [(x + sz * (0.7 + 0.5 * rr_.random()) * math.cos(b), y + sz * (0.7 + 0.5 * rr_.random()) * math.sin(b))
                   for b in [2 * math.pi * j / 6 + a for j in range(6)]]
            p.fill(p.poly(pts), rock_r, radius=1.2, depth=1.8, bias=-0.05 if front else -0.15)
    draw_rocks(False)
    # the giant rock: a craggy sphere, lit from the upper left, crevices and craters
    from pix import LIGHT
    ang = np.arctan2(yy + 0.5 - cy, xx + 0.5 - cx)
    rad = R + 1.6 * np.sin(ang * 5 + 1.0) + 1.1 * np.sin(ang * 9 + 2.0) + 0.8 * np.sin(ang * 14)
    d = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy)
    mask = d <= rad
    u, v = (xx + 0.5 - cx) / R, (yy + 0.5 - cy) / R
    nz = np.sqrt(np.clip(1 - u * u - v * v, 0.05, 1))
    bump = (_vnoise(xx / 2.6, yy / 2.6) - 0.5) * 1.4 + (_vnoise(xx / 1.2 + 9, yy / 1.2) - 0.5) * 0.7
    bx = np.roll(bump, -1, axis=1) - bump
    by = np.roll(bump, -1, axis=0) - bump
    nx, ny = u + bx * 2.2, v + by * 2.2
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    lam = (nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]) / ln
    lam = np.clip(0.42 + (lam - 0.42) * 1.0, 0, 0.999)
    big = ramp('#4a403c', n=6, spread=0.85, hue_shift=0.05)
    shade(p, mask, big, lam, dither=0.6)
    # deep crevices and a few craters
    for (x0, y0, x1, y1) in ((22, 24, 30, 28), (35, 20, 43, 24), (26, 37, 33, 40), (38, 33, 45, 37), (29, 15, 34, 18)):
        p.stroke([(x0, y0), ((x0 + x1) // 2, (y0 + y1) // 2 + 1), (x1, y1)], big[0])
    for (x, y, r) in ((25, 30, 2.2), (39, 28, 1.8), (32, 40, 1.6), (42, 21, 1.5)):
        p.fill(p.ellipse(x, y, r, r * 0.8), big, flat=0, edge=False)
        p.px(int(x + r * 0.6), int(y + r * 0.5), big[4])
    draw_rocks(True)
    p.rim('#ff7a4a', side=1, amount=0.4)
    p.rim('#ffb070', side=-1, amount=0.2)
    p.outline('#0c0404')
    # dust motes in the storm
    for x0, ph in ((8, 0.0), (56, 0.3), (14, 0.6), (50, 0.8), (30, 0.45)):
        u_ = (t + ph) % 1.0
        p.px((x0 + int(u_ * 20)) % 64, int(10 + 44 * ph + 6 * math.sin(u_ * 6.28)) % 64, (255, 170, 120))
    return p


# ---------------------------------------------------------------------------------------------------------------
def nekro(t=0.0):
    """after the owner's picture: a Nekro drone - pure machine horror: a dark armoured sphere with a red-lit core and a
    great central eye sensor, limbs radiating from it - scythe talons, a grasping mechanical hand, a red sensor eye on
    a stalk - and a spike on top; grey murk with red lights and wreckage. Moves: the drone hovers, the limbs twitch,
    the sensors pulse, the central eye charges and flashes (as if firing), the codec glitches tear the image."""
    t = t % 1.0
    p = Portrait()
    painted_bg(p, (26, 26, 30), (126, 122, 126), 30, 12, 40, steps=6)
    for x0, y0, ph in ((10, 8, 0.0), (54, 10, 0.4), (5, 34, 0.7), (60, 46, 0.2), (40, 3, 0.55)):  # red lights in the murk
        k = pulse(t, ph, 1)
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                d = math.hypot(dx, dy)
                if d <= 2.2:
                    bg_px(p, x0 + dx, y0 + dy, mix((80, 40, 44), (230, 70, 70), (1 - d / 2.2) * (0.5 + 0.5 * k)))
    for x in range(64):  # wreckage along the bottom
        h = int(4 + 3 * math.sin(x * 0.7) + 2 * math.sin(x * 1.9))
        for y in range(64 - h, 64):
            bg_px(p, x, y, (30, 28, 30) if (x + y) % 3 else (44, 40, 42))
    metal = ramp('#3e3c44', spread=0.85, hue_shift=0.04)
    red = ramp('#c01020', spread=0.6)
    blade = ramp('#7a7880', spread=0.8)
    hover = -int(round(0.5 + 0.5 * wave(t, 0.0, 1)))
    ph = t % 0.5 / 0.5  # the central eye: charges, flashes, cools (twice a loop)
    flash = 1.0 if 0.55 <= ph < 0.64 else (ph / 0.55 if ph < 0.55 else max(0.0, 1 - (ph - 0.64) / 0.36))
    cx, cy = 32, 30

    def limb(joints, w, ph_):
        """a segmented machine limb through the joint points; twitches now and then; returns the end point"""
        tw = 1 if blink(t, ph_, 2) else 0
        js = [joints[0]] + [(x + tw * (1 if x > cx else -1), y - tw) for x, y in joints[1:]]
        for (x0, y0), (x1, y1) in zip(js, js[1:]):
            p.fill(p.poly(ribbon([(x0, y0), (x1, y1)], w, w * 0.8)), metal, radius=1.5, depth=2.5, bias=-0.05)
            mx, my = (x0 + x1) / 2, (y0 + y1) / 2
            p.px(round(mx), round(my), mix(red[2], '#ff5060', pulse(t, ph_, 2)))
        for (x0, y0), (x1, y1) in zip(js, js[1:]):  # a spur on each segment
            mx, my = (x0 + x1) / 2, (y0 + y1) / 2
            dx, dy = x1 - x0, y1 - y0
            L = math.hypot(dx, dy) or 1
            nx_, ny_ = -dy / L, dx / L
            if ny_ > 0:
                nx_, ny_ = -nx_, -ny_
            p.fill(p.poly([(mx - dx / L * 1.5, my - dy / L * 1.5), (mx + dx / L * 1.5, my + dy / L * 1.5),
                           (mx + nx_ * (w * 0.5 + 3) + dx / L * 2, my + ny_ * (w * 0.5 + 3) + dy / L * 2)]), metal, radius=1,
                   depth=1.2, bias=0.1)
        for x, y in js[1:-1]:
            p.fill(p.poly([(x - w * 0.6, y), (x, y - w * 0.6), (x + w * 0.6, y), (x, y + w * 0.6)]), metal, radius=1, depth=1.5,
                   bias=0.2)
        return js[-1]

    with p.at(0, hover):
        # upper left: a scythe talon
        ex, ey = limb([(24, 24), (16, 18), (11, 12)], 5.5, 0.1)
        tal = bezier([(ex, ey), (4, 6), (0, 14), (3, 24)], 16)
        p.fill(p.poly(ribbon(tal, 5.0, 0.6)), blade, radius=1.2, depth=1.8, bias=0.1)
        for x, y in tal[4:13:3]:
            p.px(round(x) + 2, round(y), blade[4]), p.px(round(x) + 3, round(y) + 1, blade[2])
        # upper right: a great hooked talon
        ex, ey = limb([(40, 23), (48, 17), (53, 14)], 5.5, 0.35)
        tal = bezier([(ex, ey), (61, 8), (65, 18), (59, 28)], 18)
        p.fill(p.poly(ribbon(tal, 5.6, 0.6)), blade, radius=1.2, depth=1.8, bias=0.05)
        for x, y in tal[5:15:3]:
            p.px(round(x) - 2, round(y), blade[4]), p.px(round(x) - 3, round(y) + 1, blade[2])
        # left: a grasping mechanical hand
        ex, ey = limb([(22, 33), (14, 38), (9, 43)], 5.5, 0.6)
        p.fill(p.ellipse(ex, ey, 3.2, 2.8), metal, radius=1.5, depth=2, bias=0.1)
        for k, (fx, fy) in enumerate(((-4, -4), (-6, 0), (-5, 4), (-1, 6))):
            p.fill(p.poly(ribbon([(ex, ey), (ex + fx, ey + fy), (ex + fx - 1, ey + fy + (2 if k > 1 else -2))], 1.8, 1.0)), blade,
                   radius=0.8, depth=1.2, bias=0.1)
        # right: a blade
        ex, ey = limb([(42, 33), (50, 37), (55, 38)], 5.5, 0.8)
        p.fill(p.poly(ribbon([(ex, ey), (60, 36), (64, 32)], 3.6, 0.6)), blade, radius=1, depth=1.5, bias=0.15)
        # lower right: a red sensor eye on a stalk
        ex, ey = limb([(38, 39), (42, 46), (44, 51)], 3.6, 0.25)
        p.fill(p.ellipse(ex, ey + 1, 3.6, 3.4), metal, radius=1.5, depth=2, bias=0.05)
        sk = pulse(t, 0.15, 3)
        p.glow(int(ex), int(ey) + 1, mix('#ff8080', '#ffffff', sk), mix('#a00818', '#ff2a3a', sk), r=1, strength=0.85)
        # below: two manipulator legs with pincer tips
        for js, ph_ in (([(28, 40), (25, 48), (24, 55)], 0.45), ([(34, 41), (33, 49), (30, 56)], 0.9)):
            ex, ey = limb(js, 3.6, ph_)
            p.stroke([(ex - 1, ey), (ex - 2, ey + 3)], blade[3]), p.stroke([(ex + 1, ey), (ex + 2, ey + 3)], blade[3])
        # the spike on top
        p.fill(p.poly([(29, 21), (35, 21), (34, 12), (32, 6), (30, 12)]), metal, radius=1.5, depth=2, bias=0.1)
        p.fill(p.poly([(26, 10), (38, 10), (37, 6), (35, 8), (34, 3), (32, 6), (30, 3), (29, 8), (27, 6)]), metal, radius=1.2,
               depth=1.5, bias=0.2)
        p.px(32, 14, red[3]), p.px(32, 17, red[2])
        # the sphere: dark armoured plates
        # the hull: angular armour plates with serrated edges and spikes jutting out between them
        rngj = np.random.default_rng(29)
        hull = []
        for i in range(28):
            a = 2 * math.pi * i / 28
            r = 12.5 if i % 2 == 0 else 10.5
            if i % 4 == 0:
                r = 16.5 + rngj.uniform(-1.0, 1.5)  # a spike
            hull.append((cx + r * math.cos(a), cy + r * math.sin(a) * 0.96))
        p.fill(p.poly(hull), metal, radius=8, depth=10, texture=0.05, bias=-0.08)
        # plate seams: straight angular cuts radiating from the eye, serrated rings
        for i in range(7):
            a = 2 * math.pi * i / 7 + 0.3
            p.stroke([(round(cx + 6 * math.cos(a)), round(cy + 6 * math.sin(a))),
                      (round(cx + 11 * math.cos(a + 0.12)), round(cy + 11 * math.sin(a + 0.12)))], metal[0])
        for r_ in (8.5,):
            ring = [(cx + r_ * math.cos(2 * math.pi * i / 18) * (1.0 if i % 2 else 0.86),
                     cy + r_ * math.sin(2 * math.pi * i / 18) * (1.0 if i % 2 else 0.86)) for i in range(19)]
            p.stroke([(round(x), round(y)) for x, y in ring], metal[1])
        # bright edges on the upper-left spikes
        for i in (16, 20, 24):
            a = 2 * math.pi * i / 28
            p.px(round(cx + 13.5 * math.cos(a)), round(cy + 13.5 * math.sin(a)), metal[4])
        # the red-lit core band round the middle, glowing with the charge
        for x in range(cx - 12, cx + 13):
            d = abs(x - cx) / 12
            k = (0.35 + 0.65 * flash) * (1 - d * 0.6)
            p.px(x, cy, mix(red[1], '#ff7080', k)), p.px(x, cy + 1, mix(red[0], red[3], k))
        # the great central eye
        p.fill(p.ellipse(cx, cy, 5.2, 4.8), metal, radius=2, depth=2.5, bias=0.25)
        p.fill(p.ellipse(cx, cy, 3.8, 3.5), ramp(mix('#8a0818', '#ff3040', flash), spread=0.5), radius=2, depth=2.5, bias=0.25,
               edge=False)
        p.glow(cx, cy, '#ffffff' if flash > 0.95 else mix('#ff8080', '#fff0f0', flash), mix('#c01020', '#ff6070', flash), r=1,
               strength=0.6 + 0.4 * flash)
        if flash > 0.95:  # the flash bursts from the lens
            for dx, dy in ((-5, 0), (5, 0), (0, -5), (0, 5), (-4, -4), (4, 4), (-4, 4), (4, -4)):
                p.px(cx + dx, cy + dy, (255, 140, 140))
    p.rim('#ff3a4a', side=1, amount=0.2)
    p.outline('#08060a')
    if blink(t, 0.2, 1):  # codec glitches
        glitch(p, 27 + hover, 33 + hover, 3, '#ff2040', 0.35)
        glitch(p, 44, 48, -2, '#40e0ff', 0.25)
    elif blink(t, 0.24, 1):
        glitch(p, 16, 19, -2, '#ff2040', 0.2)
    elif blink(t, 0.7, 1):
        glitch(p, 35 + hover, 39 + hover, -3, '#40e0ff', 0.3)
    return p


# ---------------------------------------------------------------------------------------------------------------
def kjalengard(t=0.0):
    """after the owner's picture: a Berserker of Kjalengard - a lizard warrior, green scaled, in dark blue space armour
    edged in rust-red, a helm of rust plates and swept-back blade crests, spiked pauldrons, a coil of rope round the
    neck, a great blade at his side, warm hazy light behind. Proud, neutral. Moves: breathing, a blink, the helm
    glints, dust drifts in the light."""
    p = Portrait()
    painted_bg(p, (38, 34, 38), (232, 204, 158), 62, 30, 48, steps=6)
    for y in range(0, 44):  # a dark structure on the left
        for x in range(0, 9 - y // 8):
            bg_px(p, x, y, (30, 30, 36) if (x + y) % 4 else (40, 40, 46))
    scale = ramp('#3e5c30', spread=0.8, hue_shift=0.05, sat_boost=-0.1)
    armour = ramp('#1c2842', spread=0.62, hue_shift=0.04)
    trim = ramp('#b8562a', spread=0.65, hue_shift=0.05)
    rope = ramp('#8a7656', spread=0.6)
    blade = ramp('#5a6070', spread=0.8)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        p.fill(p.poly([(50, 66), (54, 66), (61, 42), (59, 38), (56, 42)]), blade, radius=1.5, depth=2, bias=0.1)
        p.fill(p.poly([(4, 66), (6, 48), (16, 40), (28, 38), (40, 40), (52, 42), (58, 50), (58, 66)]), armour, radius=7, depth=6,
               texture=0.04)
        p.stroke([(18, 47), (28, 52), (40, 48)], trim[2])
        p.stroke([(20, 55), (28, 58), (38, 56)], trim[1])
        p.fill(p.poly([(26, 48), (36, 46), (37, 62), (31, 66), (25, 62)]), armour, radius=3, depth=3, bias=0.12)
        p.stroke([(31, 48), (31, 64)], trim[3])
        # spiked pauldrons
        p.fill(p.poly([(0, 52), (1, 40), (9, 34), (21, 34), (25, 42), (13, 48)]), armour, radius=4, depth=4, bias=0.05)
        for k in range(3):
            p.stroke([(2, 42 + k * 3), (11, 37 + k * 3), (21, 37 + k * 3)], trim[2] if k == 0 else trim[1])
        for x0, h in ((3, 7), (8, 9), (13, 10), (18, 8)):
            p.fill(p.poly([(x0 - 2, 35), (x0 + 2, 35), (x0 + 1, 35 - h)]), blade, radius=1, depth=1.5, bias=0.15)
        p.fill(p.poly([(64, 52), (63, 40), (55, 35), (45, 35), (41, 42), (51, 48)]), armour, radius=4, depth=4, bias=-0.12)
        for k in range(3):
            p.stroke([(62, 42 + k * 3), (54, 37 + k * 3), (46, 37 + k * 3)], trim[1])
        for x0, h in ((49, 7), (55, 9), (61, 7)):
            p.fill(p.poly([(x0 - 2, 36), (x0 + 2, 36), (x0 + 1, 36 - h)]), blade, radius=1, depth=1.5, bias=-0.05)
    with p.at(0, head):
        # the scaled neck and the coil of rope
        p.fill(p.poly([(22, 24), (36, 24), (38, 40), (22, 40)]), scale, radius=3, depth=3, texture=0.12, bias=-0.15)
        for k in range(3):
            y = 34 + k * 2
            for x in range(20, 40):
                p.px(x, y, rope[3] if (x + k) % 2 else rope[1])
        # the lizard head, three-quarters to the right: heavy brow, long blunt snout, a yellow eye
        hd = [(19, 16), (24, 10), (33, 8), (42, 10), (50, 14), (56, 17), (58, 21), (56, 25), (48, 28), (38, 30), (27, 28), (21, 23)]
        p.fill(p.poly(hd), scale, radius=6, depth=6, texture=0.2)
        for y in range(10, 30, 2):  # scales
            for x in range(20 + (y % 4), 58, 4):
                if p.a[y + p.dy, x] and not (30 <= x <= 44 and 12 <= y <= 18):
                    p.px(x, y, scale[1])
                    p.px(x + 1, y - 1, scale[3])
        p.fill(p.poly([(36, 23), (57, 22), (56, 25), (48, 28), (38, 30)]), scale, flat=1, edge=False)
        p.stroke([(36, 23), (47, 23), (57, 22)], scale[0])
        for x in (40, 44, 48, 52):
            p.px(x, 24, (220, 214, 196))
        p.px(56, 19, scale[0])
        p.stroke([(24, 21), (30, 25), (36, 23)], scale[0])  # the cheek ridge
        p.pixels([(25, 20), (30, 24)], scale[4])
        if blink(t, 0.6, 2):
            p.stroke([(36, 18), (40, 18)], scale[0])
        else:
            p.stroke([(35, 17), (41, 17)], scale[0])
            p.pixels([(36, 18), (37, 18), (39, 18), (40, 18)], (240, 176, 32))
            p.px(38, 18, (20, 14, 8)), p.px(37, 19, (200, 130, 20)), p.px(39, 19, (200, 130, 20))
            p.stroke([(36, 20), (40, 20)], scale[0])
        # the helm: a dark blue skull plate; the rust-red brow ridge over the eye runs straight back into a horn
        p.fill(p.poly([(18, 17), (20, 11), (26, 8), (34, 8), (30, 12), (25, 15), (24, 21), (20, 24)]), armour, radius=2, depth=2.5,
               bias=0.05)
        p.stroke([(20, 14), (26, 10), (32, 9)], trim[2])
        # dark blade crests behind the horn, swept back
        for (bx, by, tx, ty) in ((22, 12, 12, 8), (25, 10, 16, 3)):
            p.fill(p.poly([(bx - 1, by + 2), (bx + 3, by), (tx, ty)]), armour, radius=1, depth=1.5, bias=0.12)
        # the horn: thick at the brow, angled down toward the snout, tapering straight back past the skull to a point
        horn = [(45, 15), (44, 12), (36, 10), (26, 6), (16, 2), (6, -2), (14, 4), (24, 9), (31, 13), (38, 15)]
        p.fill(p.poly(horn), trim, radius=2.5, depth=3, texture=0.04)
        p.stroke([(43, 12), (34, 9), (24, 5), (12, 0)], trim[4])
        p.stroke([(42, 15), (32, 13), (22, 9)], trim[0])
        p.pixels([(30, 9), (20, 4)], trim[1])
        # the brow ridge pressing down over the eye (menacing)
        p.fill(p.poly([(33, 14), (45, 15), (44, 16), (34, 15)]), trim, radius=1, depth=1.5, bias=-0.1)
        gl = int((t % 1.0) * 36)  # a glint running back along the horn
        for k in range(2):
            x = 44 - gl + k
            if 8 <= x <= 44:
                y = round(12 - (44 - x) * 0.33)
                p.px(x, y, (255, 230, 190))
    p.rim('#ffd8a0', side=1, amount=0.4)
    p.rim('#6a8aff', side=-1, amount=0.18)
    p.outline('#0a0a0e')
    for x0, ph in ((50, 0.0), (58, 0.4), (44, 0.75)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(60 - u * 50)), mix('#fff0d0', '#806040', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def sardakk(t=0.0):
    """after the owner's picture: a Sardakk N'orr warrior - an evil bug. Its head is a broad V: a flat plate of olive
    chitin, wide across the top, narrowing down to the red face at its point, rows of black dot eyes across it; hooked
    red mandibles tipped pale. Below it the body: a hunched, segmented insect torso with broad plated shoulders, red
    flesh at the neck and joints, thin forelimbs, and heavy plated arms (one banded with metal); golden haze.
    Moves: the mandibles clack, the head lunges a little, the throat pulses, drool drips, the forelimbs twitch."""
    p = Portrait()
    painted_bg(p, (96, 82, 34), (240, 224, 146), 56, 4, 52, steps=6)
    shell = ramp('#94a046', spread=0.72, hue_shift=0.06, sat_boost=-0.12)
    under = ramp('#5e6a2c', spread=0.72, hue_shift=0.05)
    flesh = ramp('#a02a36', spread=0.72, hue_shift=0.04)
    sinew = ramp('#5a3428', spread=0.7)
    claw = ramp('#c42c3c', spread=0.6)
    body, head = breath(t), breath(t, -0.15)
    lunge = 1 if blink(t, 0.45, 4) else 0
    with p.at(0, body):
        # the torso: hunched, broad plated shoulders, a segmented thorax narrowing down
        torso = [(2, 66), (2, 44), (8, 34), (18, 30), (30, 32), (40, 30), (50, 32), (58, 38), (62, 48), (60, 66)]
        p.fill(p.poly(torso), under, radius=8, depth=7, texture=0.08)
        # shoulder plates: overlapping chitin with lit edges
        for side in (-1, 1):
            cx = 32 + side * 18
            for k in range(2):
                arc = bezier([(cx - 11, 41 + k * 6), (cx, 32 + k * 6), (cx + 11, 41 + k * 6)], 12)
                p.fill(p.poly(ribbon(arc, 5, 6, 8)), under, radius=3, depth=3.5, bias=(0.08 if side < 0 else -0.05) - 0.08 * k,
                       texture=0.06)
                p.stroke([(round(x), round(y) - 3) for x, y in arc[2:-2:2]], under[4] if side < 0 else under[3])
        # the thorax down the middle: segmented plates with red flesh between them
        p.fill(p.poly([(24, 40), (40, 40), (42, 52), (38, 66), (26, 66), (22, 52)]), under, radius=4, depth=4, bias=0.1,
               texture=0.06)
        for y in (47, 54, 61):
            p.stroke([(23, y), (32, y + 2), (41, y)], flesh[0])
            p.stroke([(24, y - 1), (32, y + 1), (40, y - 1)], under[4])
        # red flesh at the neck behind the head
        p.fill(p.poly([(24, 30), (40, 30), (42, 38), (32, 42), (22, 38)]), flesh, radius=3, depth=3, texture=0.18)
        # thin spindly forelimbs folded at the chest (they twitch)
        for k, (x0, y0, side, ph) in enumerate(((26, 42, -1, 0.1), (38, 42, 1, 0.6))):
            tw = sway(t, ph, 1)
            leg = [(x0, y0), (x0 + side * 7, y0 + 4 + tw), (x0 + side * 4, y0 + 11 + tw), (x0 + side * 8, y0 + 15)]
            for (xa, ya), (xb, yb) in zip(leg, leg[1:]):
                p.fill(p.poly(ribbon([(xa, ya), (xb, yb)], 2.0, 1.4)), sinew, radius=0.8, depth=1.2, bias=0.1)
            p.px(leg[1][0], leg[1][1], sinew[4])
        # heavy plated arms in front: the left raised with a metal band, the right reaching across
        p.fill(p.poly(ribbon(bezier([(-2, 66), (6, 56), (16, 50)], 12), 11, 9)), shell, radius=4, depth=4, texture=0.08,
               bias=-0.02)
        for k in range(3):
            p.stroke([(1 + k * 4, 62 - k * 3), (4 + k * 4, 54 - k * 3)], under[0])
        p.stroke([(3, 58), (10, 54)], (204, 204, 196)), p.stroke([(4, 59), (11, 55)], (140, 140, 136))
        p.fill(p.poly(ribbon(bezier([(66, 66), (58, 58), (48, 56)], 12), 10, 8)), shell, radius=4, depth=4, texture=0.08,
               bias=-0.15)
        p.stroke([(58, 62), (61, 57)], under[0])
    with p.at(lunge, head + lunge):
        # the red face at the point of the V
        face = [(29, 28), (36, 26), (45, 28), (46, 34), (43, 39), (37, 42), (31, 39), (28, 34)]
        p.fill(p.poly(face), flesh, radius=5, depth=5, texture=0.18, bias=0.05 + 0.12 * pulse(t, 0.0, 2), edge=False)
        # the head: a broad flat V of chitin - wide across the top, narrowing down to the face; a slight tilt
        v = [(2, 6), (14, 2), (30, 2), (46, 4), (60, 8), (63, 13), (56, 17), (46, 25), (40, 34), (34, 34), (26, 24), (14, 15),
             (4, 11)]
        vm = p.poly(v)
        fm = p.poly(face)  # the face
        pm = p.fill(vm & ~fm, shell, radius=5, depth=4, texture=0.06, bias=0.05, edge=False)
        pm = vm
        # the underside showing along the outer lower edges (not where the head turns into the face)
        p.fill(p.poly([(4, 11), (14, 15), (24, 21), (22, 23), (12, 17), (3, 13)]), under, radius=1.5, depth=2, bias=-0.1)
        p.fill(p.poly([(63, 13), (56, 17), (48, 22), (50, 23), (58, 19), (62, 16)]), under, radius=1.5, depth=2, bias=-0.1)
        # the lower part of the V turns into the red face: chitin blending into flesh
        cxp, cyp = 37.0 + p.dx, 33.0 + p.dy
        for y in range(64):
            for x in range(64):
                if vm[y, x] or fm[y, x]:
                    d = math.hypot((x - cxp) / 1.6, y - cyp)
                    k = max(0.0, min(1.0, 1.3 - d / 12.0))
                    if k > BAYER[y % 4, x % 4] * 0.94 + 0.03:  # dithered: chitin pixels give way to flesh pixels
                        lum = p.rgb[y, x].astype(float).mean() / 255
                        p.rgb[y, x] = flesh[min(4, max(0, int(lum * 5.2)))]
                        p.a[y, x] = True
        p.stroke([(4, 5), (14, 3), (30, 3), (46, 5), (59, 9)], shell[4])
        p.stroke([(32, 6), (36, 14), (37, 20)], shell[1])
        # rows of black dot eyes across the V, following its edges, denser toward the right
        for r, (y0, x0, x1, step) in enumerate(((7, 16, 56, 5), (10, 18, 58, 4), (13, 22, 56, 4), (16, 28, 52, 3),
                                                (19, 46, 52, 3))):
            for x in range(x0 + (r % 2) * 2, x1 + 1, step):
                y = y0 + round((x - x0) * 0.04)
                yy_ = y + p.dy
                if 0 <= yy_ < 64 and 0 <= x - p.dx < 64 and pm[yy_, x + p.dx if x + p.dx < 64 else x]:
                    p.px(x, y, (16, 16, 10))
        for x, y in ((30, 30), (33, 34), (40, 34), (43, 30), (36, 28)):
            p.stroke([(x, y), (x + 1, y + 2)], flesh[4])
            p.px(x + 1, y + 3, flesh[0])
        p.fill(p.poly([(33, 34), (41, 34), (40, 38), (34, 38)]), ramp('#2a0a10', spread=0.4), flat=0, edge=False)
        p.pixels([(34, 34), (36, 35), (38, 34), (40, 35)], (236, 226, 210))
        mo = 0 if blink(t, 0.2, 2) or blink(t, 0.35, 2) else 2
        for side, bx in ((-1, 32), (1, 42)):
            path = bezier([(bx, 36), (bx + side * (5 + mo), 39), (bx + side * (5 + mo), 45), (bx - side * 1, 47)], 14)
            p.fill(p.poly(ribbon(path, 3.2, 0.8)), claw, radius=1.2, depth=1.8, bias=0.1)
            p.px(round(path[-1][0]), round(path[-1][1]), (244, 236, 222))
            p.px(round(path[-2][0]), round(path[-2][1]), (232, 214, 204))
        for side, bx in ((-1, 35), (1, 39)):
            p.stroke([(bx, 38), (bx + side, 42), (bx, 44)], flesh[3])
        u = (t * 2) % 1.0
        if u < 0.7:
            p.stroke([(37, 39), (37, 39 + int(u * 9))], (236, 206, 196))
    p.rim('#fff4c0', side=1, amount=0.35)
    p.rim('#c8d868', side=-1, amount=0.2)
    p.outline('#14100a')
    for x0, ph in ((50, 0.0), (60, 0.4), (14, 0.7)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(62 - u * 30)), mix('#fff8d0', '#806a30', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def smoke(p, t, cx, cy, w, h, strength=0.45, col=(210, 200, 186), speed=1.0, seed=0.0):
    """soft drifting smoke blended over everything in a box: noise rising and curling, a seamless loop"""
    a = 2 * math.pi * t * speed
    yy, xx = np.mgrid[0:p.h, 0:p.w].astype(float)
    nx = (xx - cx) / 7.0 + 1.5 * math.cos(a) + seed
    ny = (yy - cy) / 7.0 + 1.5 * math.sin(a) + seed * 0.7
    n = _vnoise(nx + 0.6 * _vnoise(nx * 0.5, ny * 0.5), ny)
    box = np.clip(1 - ((xx - cx) / w) ** 2 - ((yy - cy) / h) ** 2, 0, 1) ** 0.5
    k = np.clip((n - 0.36) * 3.0, 0, 1) * box
    k = np.floor(k * 3 + BAYER[yy.astype(int) % 4, xx.astype(int) % 4] * 0.9) / 3 * strength
    k = np.clip(k, 0, strength)
    base = np.where(p.a[..., None], p.rgb.astype(float), p.bg.astype(float) if p.bg is not None else 0.0)
    out = base * (1 - k[..., None]) + np.array(col, float) * k[..., None]
    m = k > 0
    p.rgb[m] = out[m].astype(np.uint8)
    p.a |= m


def kolume(t=0.0):
    """after the owner's picture: a Monk of Kolume - a robed AI robot: a deep rust-orange hood and robe, a silver mask
    with dark eye sockets and a vertical seam, blue lights glowing on its chest panels, a metal hand holding a
    smouldering incense stick, smoke curling all round it; a teal hall with a brazier burning behind. Calm.
    Moves: the smoke curls and drifts, the brazier flame flickers, the lights glow and dim, slow breathing."""
    p = Portrait()
    painted_bg(p, (14, 36, 44), (70, 120, 126), 50, 10, 46, steps=6)
    for y in range(0, 40):  # a tall window of light on the left
        for x in range(6, 12):
            if (x + y) % 2 == 0:
                bg_px(p, x, y, (90, 140, 140))
    for y in range(14, 64):  # the brazier on the right: a column, a bowl
        for x in range(52, 60):
            bg_px(p, x, y, (40, 42, 44) if (x - 52) < 5 else (28, 30, 32))
    for x in range(50, 62):
        bg_px(p, x, 13, (70, 66, 60)), bg_px(p, x, 14, (56, 52, 48))
    robe = ramp('#86401e', spread=0.72, hue_shift=0.05, sat_boost=-0.1)
    mask = ramp('#a8aeb4', spread=0.65)
    dark = ramp('#1a1e24', spread=0.6)
    plate = ramp('#3a424c', spread=0.6)
    glow = pulse(t, 0.0, 1)
    blue = mix('#2a7ab0', '#8ae0ff', glow)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the robe: draped wide, folds falling
        p.fill(p.poly([(0, 66), (2, 44), (12, 34), (24, 30), (40, 30), (52, 34), (62, 44), (64, 66)]), robe, radius=8, depth=6,
               texture=0.05)
        for pts in ([(8, 44), (6, 58), (8, 66)], [(16, 40), (14, 54), (18, 66)], [(48, 40), (50, 54), (46, 66)],
                    [(56, 46), (58, 60)]):
            p.stroke(pts, robe[1])
        for pts in ([(10, 42), (9, 54)], [(50, 42), (52, 52)]):
            p.stroke(pts, robe[3])
        # the chest: dark plates with glowing blue panels, between the robe's edges
        p.fill(p.poly([(24, 34), (40, 34), (42, 66), (22, 66)]), plate, radius=3, depth=3)
        for (x0, y0) in ((26, 38), (34, 38), (26, 46), (34, 46)):
            p.fill(p.poly([(x0, y0), (x0 + 5, y0), (x0 + 5, y0 + 5), (x0, y0 + 5)]), dark, flat=1, edge=False)
            p.stroke([(x0 + 1, y0 + 2), (x0 + 4, y0 + 2)], blue)
            p.px(x0 + 2, y0 + 3, blue)
        p.stroke([(32, 36), (32, 64)], dark[0])
        p.fill(p.poly([(22, 34), (26, 32), (26, 66), (20, 66)]), robe, radius=2, depth=2, bias=0.1)
        p.fill(p.poly([(42, 34), (38, 32), (38, 66), (44, 66)]), robe, radius=2, depth=2, bias=-0.1)
        # the raised hand holding the incense stick (left)
        p.fill(p.poly(ribbon(bezier([(10, 60), (8, 50), (10, 42)], 10), 7, 6)), robe, radius=3, depth=3, bias=0.05)
        p.fill(p.ellipse(10, 40, 3.4, 3), plate, radius=1.5, depth=2, bias=0.15)
        for k in range(3):
            p.stroke([(8 + k * 2, 38), (8 + k * 2, 36)], mask[2])
        p.stroke([(11, 38), (13, 30)], (120, 90, 60))
        p.px(13, 29, mix('#ff8030', '#ffe080', pulse(t, 0.3, 3)))
    with p.at(0, head):
        # the hood: deep, pointed, a dark interior
        p.fill(p.poly([(18, 32), (18, 18), (22, 8), (30, 2), (36, 2), (44, 8), (46, 18), (46, 32), (40, 34), (24, 34)]), robe,
               radius=6, depth=6, texture=0.05)
        p.fill(p.poly([(22, 30), (22, 18), (26, 10), (32, 7), (38, 10), (42, 18), (42, 30), (32, 33)]), dark, radius=3, depth=2)
        # the silver mask: a long faceplate, dark round eye sockets, a vertical seam, a blue light below
        p.fill(p.poly([(26, 14), (32, 11), (38, 14), (38, 22), (35, 26), (29, 26), (26, 22)]), mask, radius=4, depth=4, bias=-0.1)
        # dark angled eye sockets with small blue lights
        p.fill(p.poly([(27, 16), (31, 17), (31, 20), (27, 19)]), dark, flat=0, edge=False)
        p.fill(p.poly([(37, 16), (33, 17), (33, 20), (37, 19)]), dark, flat=0, edge=False)
        p.px(29, 18, mix('#1a4060', '#5ab8ff', glow)), p.px(35, 18, mix('#1a4060', '#5ab8ff', glow))
        p.stroke([(32, 12), (32, 21)], mask[4])
        # the snout: a ribbed filter running down from the mask, a blue light at its end
        p.fill(p.poly([(30, 21), (34, 21), (35, 29), (32, 31), (29, 29)]), plate, radius=1.5, depth=2, bias=0.1)
        for y in (23, 25, 27):
            p.stroke([(30, y), (34, y)], dark[1])
        p.px(32, 29, blue), p.px(32, 30, mix(blue, '#ffffff', 0.3))
        p.pixels([(27, 15), (28, 14)], mask[4])
    p.rim('#ffb070', side=1, amount=0.3)
    p.rim('#7ad8e0', side=-1, amount=0.25)
    p.outline('#0a0c10')
    # the brazier's flame, flickering
    for k in range(10):
        ph = k / 10
        u = (t * 3 + ph) % 1.0
        x = 56 + round(3 * math.sin(ph * 9)) + sway(t, ph, 1, 3)
        y = 12 - int(u * 9)
        if 0 <= y < 64:
            c = mix((255, 240, 160), (220, 70, 20), u)
            p.px(x, y, c)
            if u < 0.4:
                p.px(x + 1, y, c)
    # smoke: from the incense stick curling up, and a haze drifting round the monk
    # the incense trail: a curling ribbon of smoke rising from the stick
    trail = []
    for j in range(40):
        u = j / 39
        y = 28 - u * 30
        x = 13 + 4 * math.sin(u * 7 - 2 * math.pi * t) * u + u * 2
        trail.append((x, y))
    for j, (x, y) in enumerate(trail):
        k = 0.75 * (1 - j / 45)
        for dx in range(-1 - j // 14, 2 + j // 14):
            xi, yi = int(round(x + dx)), int(round(y))
            if 0 <= xi < 64 and 0 <= yi < 64:
                base = p.rgb[yi, xi].astype(float) if p.a[yi, xi] else p.bg[yi, xi].astype(float)
                kk = k * (1 - abs(dx) / (2.5 + j / 14))
                p.rgb[yi, xi] = (base * (1 - kk) + np.array((232, 222, 204)) * kk).astype(np.uint8)
                p.a[yi, xi] = True
    smoke(p, t, 16, 10, 14, 14, strength=0.5, col=(226, 216, 198), speed=1.0, seed=0.0)
    smoke(p, t, 32, 56, 40, 14, strength=0.4, speed=-1.0, seed=3.0)
    smoke(p, t, 54, 30, 14, 22, strength=0.35, speed=1.0, seed=6.0)
    smoke(p, t, 6, 44, 10, 18, strength=0.35, speed=-1.0, seed=9.0)
    return p


# ---------------------------------------------------------------------------------------------------------------
def winnu(t=0.0):
    """after the owner's picture: a Winnu noble - tall, slender, pale-gold skin, a tall bald skull, a jewelled gold
    circlet with a red gem, red drop earrings, a high dark collar to the jaw, a heavy gold chain with a red gem
    pendant, a dark slate coat trimmed in gold; golden spires in the haze behind. Regal, neutral.
    Moves: breathing, a slow regal blink, the gems glint, the haze drifts."""
    p = Portrait()
    painted_bg(p, (120, 104, 60), (240, 226, 176), 32, 6, 50, steps=6)
    for x0, w, h in ((3, 3, 34), (8, 2, 24), (50, 3, 40), (56, 2, 30), (60, 3, 22), (14, 2, 18)):  # spires
        for y in range(64 - h - 20, 64):
            for dx in range(w):
                bg_px(p, x0 + dx, y, (168, 150, 104) if dx == 0 else (150, 132, 90))
        bg_px(p, x0 + w // 2, 64 - h - 21, (190, 174, 128))
    skin = ramp('#c4b880', spread=0.6, hue_shift=0.04)
    gold = ramp('#d0a03a', spread=0.75, hue_shift=0.06)
    coat = ramp('#3c4440', spread=0.65)
    dark = ramp('#24222a', spread=0.6)
    red = ramp('#b01828', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    gem = pulse(t, 0.0, 1)
    with p.at(0, body):
        # the coat: dark slate, gold trim along the edges, a high shoulder line
        p.fill(p.poly([(4, 66), (6, 46), (14, 38), (24, 36), (32, 38), (40, 36), (50, 38), (58, 46), (60, 66)]), coat, radius=7,
               depth=6, texture=0.04)
        p.stroke([(14, 38), (6, 46), (5, 66)], gold[2])
        p.stroke([(50, 38), (58, 46), (59, 66)], gold[1])
        p.stroke([(28, 44), (24, 66)], gold[2]), p.stroke([(36, 44), (40, 66)], gold[1])
        p.fill(p.poly([(24, 40), (40, 40), (38, 66), (26, 66)]), dark, radius=3, depth=3)  # the dark tunic beneath
        for y in (52, 58):
            p.px(27, y, gold[3]), p.px(37, y, gold[3])
        # the heavy gold chain across the chest, a red gem pendant
        for k, (a2, c, b2) in enumerate((((16, 40), (32, 50), (48, 40)), ((18, 42), (32, 53), (46, 42)))):
            for j, (x, y) in enumerate(bezier([a2, c, b2], 26)):
                p.px(round(x), round(y), gold[3] if (j + k) % 2 else gold[1])
        p.fill(p.poly([(29, 52), (35, 52), (36, 56), (32, 60), (28, 56)]), gold, radius=1.5, depth=2, bias=0.1)
        p.fill(p.ellipse(32, 55.5, 2.2, 2.2), red, radius=1.2, depth=1.5, bias=0.1 + 0.2 * gem)
        p.px(31, 54, mix(red[4], '#ffffff', gem))
    with p.at(0, head):
        # the high dark collar wrapped to the jaw
        p.fill(p.poly([(23, 28), (41, 28), (43, 42), (32, 45), (21, 42)]), dark, radius=3, depth=3)
        for y in (32, 35, 38, 41):
            p.stroke([(23, y), (41, y)], dark[3])
        # the tall bald head: long skull, narrow face, high cheekbones
        hd = [(25, 10), (27, 3), (32, 0), (37, 3), (39, 10), (39, 18), (38, 24), (35, 29), (32, 31), (29, 29), (26, 24), (25, 18)]
        p.fill(p.poly(hd), skin, radius=7, depth=4, texture=0.02, bias=0.12)
        p.fill(p.poly([(36, 17), (39, 17), (38, 24), (35, 28), (35, 23)]), skin, flat=2, edge=False)
        p.stroke([(27, 21), (28, 24)], skin[2]), p.stroke([(37, 21), (36, 24)], skin[1])
        # brows, heavy-lidded eyes (a slow blink), a long nose, thin lips (neutral)
        p.stroke([(26, 14), (30, 14)], skin[1]), p.stroke([(34, 14), (38, 14)], skin[0])
        if blink(t, 0.65, 2):
            p.stroke([(27, 16), (30, 16)], skin[0]), p.stroke([(34, 16), (37, 16)], skin[0])
        else:
            p.stroke([(27, 15), (30, 15)], skin[0]), p.stroke([(34, 15), (37, 15)], skin[0])
            p.px(28, 16, (60, 40, 30)), p.px(29, 16, (30, 20, 16)), p.px(35, 16, (30, 20, 16)), p.px(36, 16, (60, 40, 30))
        p.stroke([(32, 16), (32, 22)], skin[3])
        p.px(31, 23, skin[1]), p.px(33, 23, skin[1])
        p.stroke([(30, 26), (34, 26)], (120, 80, 70))
        p.px(32, 27, skin[3])
        # ears with red drop earrings
        for ex, d in ((23.5, -1), (40.5, 1)):
            p.fill(p.ellipse(ex, 18, 1.5, 3), skin, radius=1, depth=1.5, bias=-0.1)
            ix = int(ex)
            p.px(ix, 22, gold[3]), p.px(ix, 23, red[3]), p.px(ix, 24, red[2]), p.px(ix, 25, mix(red[1], '#ff8080', gem))
        # the jewelled circlet: a gold band over the brow rising to a peak with a red gem
        p.fill(p.poly([(25, 8), (29, 6), (32, 3), (35, 6), (39, 8), (39, 10), (32, 8), (25, 10)]), gold, radius=1.5, depth=2,
               bias=0.1)
        p.fill(p.ellipse(32, 6, 1.6, 1.8), red, radius=1, depth=1.5, bias=0.15)
        p.px(32, 5, mix(red[4], '#ffffff', gem))
        p.stroke([(32, 0), (32, 2)], gold[3])
    p.rim('#fff0c0', side=1, amount=0.35)
    p.rim('#ffe0a0', side=-1, amount=0.25)
    p.outline('#1a1408')
    for x0, ph in ((10, 0.0), (54, 0.4), (46, 0.75)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(50 - u * 44)), mix('#fff8e0', '#a08850', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def jolnar(t=0.0):
    """after the owner's picture: a Jol-Nar scholar - an aquatic alien seen through a green glass dome, mounted on a
    mechanical spider walker: a pale hull plate with the Jol-Nar mark, a lens eye, pipes, jointed legs; green
    haze. Moves: bubbles rise in the dome, the alien sways and its fins flutter, the lens glows, the walker
    shifts its legs."""
    p = Portrait()
    painted_bg(p, (40, 60, 30), (190, 210, 120), 34, 14, 46, steps=6)
    hull = ramp('#948c78', spread=0.72, hue_shift=0.04)
    metal = ramp('#5e5a50', spread=0.75, hue_shift=0.04)
    alien = ramp('#8a6a3a', spread=0.75, hue_shift=0.06)
    glass = ramp('#5aa848', spread=0.7, hue_shift=0.05)
    body = breath(t)
    shift = sway(t, 0.0, 1)
    with p.at(0, body):
        rust = (122, 70, 40)
        grime = (70, 66, 50)
        # the walker's legs: jointed, with hydraulic pistons and hoses (they shift)
        for k, (pts, ph) in enumerate((([(36, 50), (44, 57), (44, 66)], 0.25), ([(26, 50), (18, 57), (18, 66)], 0.75),
                                       ([(22, 44), (10, 48), (5, 66)], 0.0), ([(44, 46), (56, 46), (60, 66)], 0.5))):
            s_ = sway(t, ph, 1)
            pts = [pts[0], (pts[1][0] + s_, pts[1][1]), pts[2]]
            for j, ((xa, ya), (xb, yb)) in enumerate(zip(pts, pts[1:])):
                p.fill(p.poly(ribbon([(xa, ya), (xb, yb)], 5, 4)), metal, radius=1.8, depth=2.5, bias=0.0 - 0.06 * (k % 2),
                       texture=0.06, seed=k * 3 + j)
                if j == 1:
                    continue
                # a piston rod alongside the segment, bright where it slides
                ox, oy = (yb - ya), -(xb - xa)
                L = math.hypot(ox, oy) or 1
                ox, oy = ox / L * 2.6, oy / L * 2.6
                p.stroke([(round(xa + ox + (xb - xa) * 0.2), round(ya + oy + (yb - ya) * 0.2)),
                          (round(xa + ox + (xb - xa) * 0.75), round(ya + oy + (yb - ya) * 0.75))], metal[3])
                p.px(round(xa + (xb - xa) * 0.5), round(ya + (yb - ya) * 0.5), grime)
            # the knee joint: a bolted hub
            kx, ky = pts[1]
            p.fill(p.ellipse(kx, ky, 3.6, 3.6), metal, radius=1.5, depth=2, bias=0.25, texture=0.04)
            p.fill(p.ellipse(kx, ky, 1.6, 1.6), metal, flat=0, edge=False)
            p.px(round(kx), round(ky), hull[3])
            for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2)):
                p.px(round(kx) + dx, round(ky) + dy, metal[1])
            # a hose looping from the hull to the knee
            if k < 2:
                hose = bezier([pts[0], ((pts[0][0] + kx) / 2, max(pts[0][1], ky) + 4), (kx, ky + 1)], 10)
                p.stroke([(round(x), round(y)) for x, y in hose], (40, 36, 30))
            # the foot: a clawed pad
            fx, fy = pts[2]
            p.stroke([(fx - 2, 63), (fx + 2, 63)], metal[0])
        # an arm with pipes reaching up on the left, a clamp at its end
        p.fill(p.poly(ribbon([(20, 40), (8, 34), (2, 22)], 5, 4)), metal, radius=2, depth=2.5, texture=0.12)
        p.fill(p.ellipse(8, 34, 3, 3), hull, radius=1.5, depth=2, bias=0.1)
        p.stroke([(18, 38), (8, 31), (4, 22)], metal[4])
        p.stroke([(19, 42), (9, 37), (5, 26)], (52, 48, 40))
        p.fill(p.poly([(0, 20), (5, 18), (6, 23), (1, 24)]), metal, radius=1, depth=1.5, bias=0.15)
        # the hull: a weathered armoured plate under the dome
        p.fill(p.poly([(16, 34), (28, 32), (48, 32), (54, 38), (52, 48), (40, 52), (24, 52), (16, 46)]), hull, radius=5, depth=5,
               texture=0.14)
        # panel seams, rivets, a vent grille, dents and scratches
        p.stroke([(18, 38), (52, 38)], hull[1])
        p.stroke([(18, 39), (52, 39)], hull[4])
        p.stroke([(34, 39), (34, 51)], hull[1])
        p.stroke([(18, 46), (34, 46)], hull[1])
        for x in range(19, 53, 4):
            p.px(x, 36, metal[1])
        for x, y in ((20, 41), (32, 41), (36, 41), (50, 41), (20, 49), (32, 49)):
            p.px(x, y, metal[2]), p.px(x, y - 1, hull[4])
        for y in (47, 49):
            p.stroke([(37, y), (40, y)], metal[0])
        rng = np.random.default_rng(31)
        for _ in range(14):  # grime and scratches
            x, y = int(rng.integers(18, 53)), int(rng.integers(34, 51))
            if p.a[y + p.dy, x]:
                p.px(x, y, grime if rng.random() < 0.6 else hull[4])
        for x0, y0 in ((22, 40), (45, 40), (29, 47)):  # rust streaks running down
            for k in range(4):
                if p.a[y0 + k + p.dy, x0]:
                    p.px(x0, y0 + k, mix(rust, hull[2], k / 5))
        p.fill(p.poly([(42, 41), (48, 41), (48, 47), (42, 47)]), ramp('#d8d0bc', spread=0.3), radius=1, depth=1, texture=0.1)
        p.pixels([(44, 43), (45, 43), (46, 44), (44, 45)], (190, 50, 40))
        p.pixels([(45, 44), (46, 45)], (220, 170, 40))
        p.px(47, 46, grime), p.px(42, 41, grime)
        lens = pulse(t, 0.0, 2)
        p.fill(p.ellipse(25, 44, 3.2, 3.2), metal, radius=1.5, depth=2, bias=0.1)
        p.glow(25, 44, '#f0ffe0', mix('#4a9a30', '#b0ff80', lens), r=1, strength=0.7 + 0.3 * lens)
        # the glass dome: green-tinted, the alien inside
        dome = p.ellipse(33, 20, 17, 16)
        p.fill(dome & (p.yy < 34), glass, radius=10, depth=6, bias=-0.15)
        # the alien: a broad scaly amphibian head - a frilled crest, big eyes on top, a wide mouth, barbels,
        # gill slits - swaying in the water
        alien = ramp('#b07a32', spread=0.8, hue_shift=0.06)
        with p.at(shift, 0):
            for k in range(5):  # the crest frills (they flutter)
                fl = sway(t, k * 0.2, 1, 2)
                p.fill(p.poly([(25 + k * 4, 12), (29 + k * 4, 12), (27 + k * 4 + fl, 5 + abs(k - 2))]) & dome, alien, radius=1,
                       depth=1.2, bias=0.1)
            ah = [(21, 18), (24, 12), (33, 10), (42, 12), (45, 18), (44, 25), (38, 30), (28, 30), (22, 25)]
            am = p.poly(ah) & dome
            p.fill(am, alien, radius=6, depth=6, texture=0.16)
            p.fill(p.poly([(45, 17), (50, 13 + sway(t, 0.3, 1)), (48, 22)]) & dome, alien, radius=1, depth=1.5, bias=0.05)
            p.fill(p.poly([(21, 17), (16, 13 + sway(t, 0.7, 1)), (18, 22)]) & dome, alien, radius=1, depth=1.5, bias=0.05)
            # big eyes on top of the head
            for ex in (27.5, 38.5):
                p.fill(p.ellipse(ex, 16, 2.8, 2.4), alien, radius=1.5, depth=2, bias=0.2)
                ix = int(ex)
                p.px(ix, 16, (240, 190, 60)), p.px(ix + 1, 16, (20, 14, 8)), p.px(ix, 15, (255, 240, 180))
            # the wide mouth, a lower lip, barbels hanging; gill slits on the cheeks
            p.stroke([(26, 24), (33, 25), (40, 24)], alien[0])
            p.stroke([(27, 26), (33, 27), (39, 26)], alien[3])
            for bx, ph in ((27, 0.1), (39, 0.6)):
                bs = sway(t, ph, 1)
                p.stroke([(bx, 26), (bx - 1 + bs, 29), (bx + bs, 31)], alien[3])
            for gx in (23, 24, 42, 43):
                p.stroke([(gx, 19), (gx, 22)], alien[0])
            p.pixels([(30, 13), (34, 12), (37, 14), (31, 20), (35, 21), (25, 21), (41, 21)], alien[4])
        # the glass over it: a tint, a curved highlight, the rim
        gm = dome & (p.yy < 34)
        tint = np.array(glass[3], float)
        p.rgb[gm] = (p.rgb[gm].astype(float) * 0.82 + tint * 0.18).astype(np.uint8)
        p.stroke([(round(33 + 14 * math.cos(a)), round(20 - 13 * math.sin(a))) for a in [math.pi * (0.55 + i * 0.05) for i in range(7)]],
                 (230, 255, 220))
        p.stroke([(17, 32), (49, 32)], metal[1])
        p.stroke([(17, 33), (49, 33)], hull[3])
    p.rim('#e0ffb0', side=-1, amount=0.3)
    p.rim('#b0e070', side=1, amount=0.3)
    p.outline('#0c1208')
    for x0, ph in ((28, 0.0), (36, 0.4), (40, 0.75), (25, 0.55)):  # bubbles in the dome
        u = (t * 1.5 + ph) % 1.0
        y = int(round(30 - u * 24)) + body
        x = x0 + sway(t, ph, 1, 2)
        if 6 < y < 32:
            p.px(x, y, (220, 255, 210))
    return p


# ---------------------------------------------------------------------------------------------------------------
def yin(t=0.0):
    """after the owner's picture: a Yin brother - a grey-skinned bald human ravaged by greyfire (a space leprosy):
    mottled, scarred, burn-like skin over the scalp, temples and cheeks, pale glowing eyes, a stern set mouth; a
    high-collared quilted ochre robe; misty grey spires behind. Neutral, stern.
    Moves: breathing, a slow blink, the pale eyes glow, the mist drifts."""
    p = Portrait()
    painted_bg(p, (54, 52, 54), (150, 146, 140), 30, 10, 46, steps=6)
    for x0, w, h in ((2, 5, 40), (10, 3, 30), (52, 6, 44), (60, 3, 34)):  # spires in the mist
        for y in range(64 - h - 10, 64):
            for dx in range(w):
                if (dx + y) % 3:
                    bg_px(p, x0 + dx, y, (88, 86, 86) if dx else (100, 98, 96))
    skin = ramp('#827c84', spread=0.75, hue_shift=0.03)
    robe = ramp('#86682c', spread=0.7, hue_shift=0.05, sat_boost=-0.1)
    body, head = breath(t), breath(t, -0.1)
    eye = pulse(t, 0.0, 1)
    with p.at(0, body):
        # the quilted robe: padded ribs, a high stiff collar, a front panel
        p.fill(p.poly([(0, 66), (2, 50), (10, 42), (22, 38), (32, 40), (42, 38), (54, 42), (62, 50), (64, 66)]), robe, radius=7,
               depth=6, texture=0.05)
        for y in range(46, 66, 3):
            for x0, x1 in ((2, 24), (40, 62)):
                p.stroke([(x0, y), (x1, y)], robe[1])
        p.fill(p.poly([(25, 44), (39, 44), (41, 66), (23, 66)]), robe, radius=3, depth=3, bias=0.08)
        p.stroke([(32, 46), (32, 66)], robe[1])
        p.fill(p.poly([(12, 44), (15, 30), (21, 25), (24, 36), (27, 44), (20, 48)]), robe, radius=3, depth=3, bias=0.05)
        p.fill(p.poly([(52, 44), (49, 30), (43, 25), (40, 36), (37, 44), (44, 48)]), robe, radius=3, depth=3, bias=-0.15)
        p.stroke([(16, 31), (21, 44)], robe[3]), p.stroke([(48, 31), (43, 44)], robe[1])
    with p.at(0, head):
        p.fill(p.poly([(26, 30), (38, 30), (38, 42), (26, 42)]), skin, radius=2, depth=2, bias=-0.3)
        # the bald head: a broad skull, hollow cheeks, a heavy brow
        hd = [(20, 10), (24, 3), (32, 0), (40, 3), (44, 10), (44, 20), (41, 28), (36, 34), (32, 35), (28, 34), (23, 28), (20, 20)]
        p.fill(p.poly(hd), skin, radius=8, depth=7, texture=0.05)
        p.fill(p.poly([(38, 17), (44, 17), (41, 28), (36, 34), (36, 26)]), skin, flat=1, edge=False)
        # greyfire: burn-like scarring - pale, puckered blotches ringed in raw purple, over the scalp, temple and cheek
        rng = np.random.default_rng(47)
        yy0 = p.dy
        for cx_, cy_, rx, ry in ((27, 5, 5.5, 3.2), (37, 4, 3.0, 2.2), (22, 15, 2.4, 4.5), (24, 24, 2.6, 2.4), (41, 21, 2.0, 3.6),
                                 (33, 9, 2.0, 1.6)):
            for y in range(int(cy_ - ry) - 2, int(cy_ + ry) + 3):
                for x in range(int(cx_ - rx) - 2, int(cx_ + rx) + 3):
                    if not (0 <= y + yy0 < 64 and 0 <= x < 64) or not p.a[y + yy0, x]:
                        continue
                    d = math.hypot((x - cx_) / rx, (y - cy_) / ry) + rng.uniform(-0.25, 0.25)
                    base = p.rgb[y + yy0, x].astype(float)
                    if d < 0.7:
                        c = base * 0.35 + np.array((200, 188, 194)) * 0.65 if (x + y) % 3 else base * 0.5 + np.array((150, 126, 140)) * 0.5
                    elif d < 1.15:
                        c = base * 0.45 + np.array((92, 52, 68)) * 0.55
                    else:
                        continue
                    p.px(x, y, tuple(int(v) for v in c))
        # heavy brow, deep sockets, pale eyes (a slow blink), bags beneath
        p.stroke([(23, 13), (30, 14)], skin[0]), p.stroke([(34, 14), (41, 13)], skin[0])
        p.fill(p.poly([(24, 15), (30, 15), (30, 18), (24, 18)]), skin, flat=1, edge=False)
        p.fill(p.poly([(34, 15), (40, 15), (40, 18), (34, 18)]), skin, flat=0, edge=False)
        if blink(t, 0.62, 2):
            p.stroke([(25, 16), (29, 16)], skin[0]), p.stroke([(35, 16), (39, 16)], skin[0])
        else:
            for ex in (25, 35):
                p.px(ex, 16, (200, 196, 200)), p.px(ex + 1, 16, mix('#8ab0c8', '#d8f4ff', eye)), p.px(ex + 2, 16, (24, 22, 28))
                p.px(ex + 3, 16, (186, 182, 188))
        p.stroke([(25, 18), (29, 18)], skin[1]), p.stroke([(35, 18), (39, 18)], skin[1])
        # the nose, its wings, deep lines to the mouth, purplish thin lips set hard, a heavy chin
        p.stroke([(32, 17), (32, 23)], skin[4])
        p.stroke([(33, 18), (33, 23)], skin[1])
        p.pixels([(30, 24), (34, 24)], skin[0]), p.px(31, 25, skin[1]), p.px(33, 25, skin[1])
        p.stroke([(28, 23), (27, 28)], skin[1]), p.stroke([(37, 23), (38, 28)], skin[0])
        p.stroke([(29, 28), (36, 28)], (78, 52, 64))
        p.stroke([(30, 29), (35, 29)], (128, 96, 110))
        p.stroke([(30, 32), (35, 32)], skin[1])
        p.pixels([(31, 31), (32, 31), (33, 31)], skin[3])
        for ex in (19.5, 44.5):
            p.fill(p.ellipse(ex, 18, 1.5, 3), skin, radius=1, depth=1.5, bias=-0.1)
    p.rim('#d8e0ec', side=1, amount=0.3)
    p.rim('#ffd890', side=-1, amount=0.18)
    p.outline('#121012')
    for k in range(2):  # mist drifting along the bottom
        y0 = 60 + k * 2
        off = int(((t + k * 0.5) % 1.0) * 64)
        for x in range(64):
            if (x + off) % 64 < 28 and (x + y0) % 2 == 0:
                p.rgb[y0, x] = (p.rgb[y0, x] * 0.65 + np.array((170, 166, 160)) * 0.35).astype(np.uint8)
                p.a[y0, x] = True
    return p


# ---------------------------------------------------------------------------------------------------------------
def xxcha(t=0.0):
    """after the owner's picture, front-on (owner: "make it look straight"): an Xxcha in a high-tech tank suit - heavy
    yellow armour, a cylindrical tank on the back capped with a glowing green dome (left), a box module with vents
    (right), ribbed hoses; the turtle head pushing out of the suit's collar facing us - lilac-green scaled skin, a
    heavy brow, eyes set wide, a blunt beaked snout with pink nostrils; a scaly clawed hand; olive haze. Neutral.
    Moves: breathing, the green cap glows, a slow blink, the head sways a little."""
    p = Portrait()
    painted_bg(p, (98, 90, 46), (206, 196, 132), 32, 12, 50, steps=6)
    yel = ramp('#c8a42a', spread=0.75, hue_shift=0.06)
    dark = ramp('#2a2620', spread=0.6)
    hose = ramp('#a88a2a', spread=0.7)
    skin = ramp('#8a9a86', spread=0.72, hue_shift=0.08)
    lilac = ramp('#a894b0', spread=0.6, hue_shift=0.04)
    green = ramp('#5aa850', spread=0.6)
    body, head = breath(t), breath(t, -0.12)
    cap = pulse(t, 0.0, 1)
    with p.at(0, body):
        # ribbed hoses down the left side, from the tank
        for k, x0 in enumerate((2, 7)):
            path = bezier([(x0 + 8, 16), (x0, 28), (x0 + 1, 46), (x0 + 4, 66)], 30)
            p.fill(p.poly(ribbon(path, 4, 4)), hose, radius=1.5, depth=2, bias=0.05 - 0.05 * k)
            for j in range(1, len(path) - 1, 2):
                x, y = path[j]
                p.px(round(x) - 1, round(y), hose[0]), p.px(round(x), round(y), hose[0]), p.px(round(x) + 1, round(y), hose[3])
        # the tank on the back (left) capped with a glowing green dome
        p.fill(p.poly([(6, 4), (22, 4), (22, 26), (6, 26)]), yel, radius=6, depth=5, texture=0.04)
        p.fill(p.ellipse(14, 4, 8, 3), yel, radius=2, depth=2, bias=0.2)
        p.fill(p.ellipse(14, 3, 5.5, 2.1), green, radius=1.5, depth=2, bias=0.1 + 0.25 * cap)
        p.px(12, 2, mix(green[4], '#eaffd0', cap)), p.px(13, 2, mix(green[3], '#eaffd0', cap))
        for y in (9, 18):
            p.stroke([(6, y), (22, y)], yel[1])
        for x in (8, 20):
            p.fill(p.poly([(x - 1, 6), (x + 1, 6), (x + 1, 24), (x - 1, 24)]), dark, radius=1, depth=1, bias=0.2)
        p.fill(p.ellipse(14, 13, 1.6, 1.6), dark, radius=1, depth=1, bias=0.2)
        # the box module with vents (right)
        p.fill(p.poly([(42, 6), (56, 4), (62, 10), (62, 24), (50, 26), (42, 22)]), yel, radius=4, depth=4, bias=-0.05, texture=0.04)
        p.fill(p.poly([(52, 12), (60, 11), (60, 18), (52, 19)]), dark, flat=0, edge=False)
        for y in (13, 15, 17):
            p.stroke([(53, y), (59, y - 1)], dark[3])
        p.pixels([(45, 9), (48, 8)], dark[1])
        # the suit's body: heavy yellow plates, rounded shoulders on both sides
        p.fill(p.poly([(2, 66), (4, 40), (12, 30), (24, 26), (40, 26), (52, 30), (60, 40), (62, 66)]), yel, radius=8, depth=7,
               texture=0.05)
        for cx_, bias_ in ((13, 0.05), (51, -0.12)):
            p.fill(p.ellipse(cx_, 40, 10, 8.5), yel, radius=6, depth=6, texture=0.05, bias=bias_)
        p.stroke([(5, 38), (13, 33), (21, 36)], yel[4]), p.stroke([(43, 36), (51, 33), (59, 38)], yel[3])
        for y in range(52, 64, 2):  # ribbed sleeves
            p.stroke([(4, y), (16, y + 1)], yel[1]), p.stroke([(48, y + 1), (60, y)], yel[1])
        # the collar the head pushes out of
        p.fill(p.poly([(18, 30), (46, 30), (48, 40), (40, 46), (24, 46), (16, 40)]), dark, radius=2, depth=2)
    with p.at(sway(t, 0.2, 1), head):
        # the turtle head facing us: a broad flat skull, eyes set wide under a heavy brow, a blunt beaked snout
        hd = [(19, 26), (22, 19), (28, 15), (36, 15), (42, 19), (45, 26), (44, 33), (40, 39), (32, 42), (24, 39), (20, 33)]
        p.fill(p.poly(hd), skin, radius=7, depth=6, texture=0.05)
        p.fill(p.poly([(24, 18), (32, 15), (40, 18), (38, 21), (32, 19), (26, 21)]), lilac, radius=2, depth=2, bias=0.15,
               edge=False)
        for y in range(23, 40, 3):  # scales
            for x in range(21 + (y % 2) * 2, 44, 4):
                yy_ = y + p.dy
                if 0 <= yy_ < 64 and 0 <= x + p.dx < 64 and p.a[yy_, x + p.dx] and not (27 <= x <= 37 and 26 <= y <= 36):
                    p.px(x, y, skin[1])
        p.fill(p.poly([(22, 36), (32, 40), (42, 36), (40, 39), (32, 42), (24, 39)]), skin, flat=0, edge=False)  # the jaw shadow
        # heavy brow ridges over deep sockets, eyes set wide (a slow blink)
        for ex, d in ((22, 1), (38, -1)):
            p.fill(p.poly([(ex, 24), (ex + 5, 24), (ex + 5, 28), (ex, 28)]), skin, flat=0, edge=False)
            if blink(t, 0.6, 2):
                p.stroke([(ex, 26), (ex + 4, 26)], skin[1])
            else:
                p.px(ex + 1, 25, (236, 214, 120)), p.px(ex + 2, 25, (236, 214, 120)), p.px(ex + 3, 25, (200, 170, 80))
                p.px(ex + 1, 26, (200, 170, 80)), p.px(ex + 2, 26, (20, 16, 10)), p.px(ex + 3, 26, (200, 170, 80))
                p.px(ex + 2, 27, (160, 130, 60))
        p.stroke([(20, 23), (27, 22), (29, 24)], skin[4]), p.stroke([(35, 24), (37, 22), (44, 23)], skin[3])
        p.stroke([(21, 24), (28, 24)], skin[0]), p.stroke([(36, 24), (43, 24)], skin[0])
        # the hooked turtle beak down the middle of the face: a ridge, small nostrils, the hook over the lip
        p.fill(p.poly([(29, 24), (35, 24), (36, 30), (34, 35), (32, 37), (30, 35), (28, 30)]), lilac, radius=2, depth=2.5,
               bias=0.0)
        p.stroke([(32, 25), (32, 33)], lilac[4])
        p.px(30, 28, (70, 50, 66)), p.px(34, 28, (70, 50, 66))
        p.px(32, 36, lilac[0])
        # the wide mouth line running back from the beak, a pink inner lip
        p.stroke([(22, 33), (26, 34), (30, 35)], (66, 56, 66)), p.stroke([(34, 35), (38, 34), (42, 33)], (66, 56, 66))
        p.stroke([(24, 35), (29, 36)], (196, 120, 144)), p.stroke([(35, 36), (40, 35)], (196, 120, 144))
    with p.at(0, body):
        # the scaly clawed hand in front
        p.fill(p.poly([(25, 50), (38, 48), (42, 54), (38, 60), (27, 60), (23, 55)]), skin, radius=4, depth=4, texture=0.06)
        for fx in (27, 32, 37):
            p.fill(p.poly(ribbon([(fx, 58), (fx + 1, 62), (fx, 66)], 3.4, 2.6)), skin, radius=1.2, depth=1.5, bias=0.05)
            p.px(fx, 64, (232, 228, 214)), p.px(fx + 1, 64, (200, 196, 186))
        for x, y in ((28, 52), (32, 51), (36, 52), (30, 55), (34, 55)):
            p.px(x, y, skin[1])
        p.stroke([(21, 52), (25, 50)], (220, 220, 214)), p.stroke([(22, 53), (26, 51)], (60, 60, 70))
    p.rim('#fff0a0', side=1, amount=0.3)
    p.rim('#c0ff90', side=-1, amount=0.15)
    p.outline('#14120a')
    return p


# ---------------------------------------------------------------------------------------------------------------
def mahact(t=0.0):
    """after the owner's picture: a Mahact Gene-Sorcerer - regal and emotionless: a tall crested gold helm whose face
    is a cosmic void (deep blue, swirling, full of stars), ornate gold armour plates with bright inlay, a deep violet
    cloak, a gold staff; a starfield behind. Moves: the stars in the void drift and twinkle, the inlay glows, the
    cloak stirs, breathing."""
    p = Portrait()
    painted_bg(p, (8, 10, 30), (40, 54, 110), 30, 18, 44, steps=6)
    rng = np.random.default_rng(53)
    for _ in range(26):
        x, y = int(rng.integers(0, 64)), int(rng.integers(0, 64))
        bg_px(p, x, y, mix((60, 70, 120), (230, 236, 255), pulse(t, float(rng.random()), 1)))
    gold = ramp('#c89a3c', spread=0.8, hue_shift=0.06)
    pale = ramp('#e8d8a8', spread=0.4)
    cloak = ramp('#3c2a64', spread=0.7, hue_shift=0.04)
    dark = ramp('#1c1a24', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    glow = pulse(t, 0.0, 1)
    with p.at(0, body):
        # the violet cloak behind and around
        p.fill(p.poly([(-2, 66), (0, 44), (8, 34), (18, 32), (46, 32), (56, 34), (64, 44), (66, 66)]), cloak, radius=7, depth=5,
               texture=0.04)
        for pts in ([(6, 44), (4, 66)], [(58, 44), (60, 66)], [(12, 40), (10, 60)]):
            p.stroke(pts, cloak[1])
        # the breastplate: ornate gold plates, bright inlay, a central keel
        p.fill(p.poly([(16, 40), (24, 36), (32, 38), (40, 36), (48, 40), (46, 56), (38, 64), (26, 64), (18, 56)]), gold,
               radius=6, depth=6, texture=0.04)
        inlay = mix(pale[3], '#fffbe8', glow)
        for pts in ([(20, 44), (26, 42), (31, 46)], [(44, 44), (38, 42), (33, 46)], [(22, 50), (28, 49), (31, 54)],
                    [(42, 50), (36, 49), (33, 54)]):
            p.stroke(pts, inlay)
        p.stroke([(32, 39), (32, 62)], gold[0])
        p.stroke([(33, 40), (33, 61)], pale[4])
        p.fill(p.ellipse(32, 56, 2.4, 2.4), pale, radius=1, depth=1.5, bias=0.2 + 0.2 * glow)
        # great pauldrons: layered plates with flared points
        for side in (-1, 1):
            cx = 32 + side * 19
            p.fill(p.poly([(cx - side * 8, 44), (cx - side * 9, 36), (cx, 31), (cx + side * 9, 34), (cx + side * 12, 26),
                           (cx + side * 11, 40), (cx + side * 4, 46)]), gold, radius=4, depth=4,
                   bias=0.05 if side < 0 else -0.12, texture=0.04)
            p.stroke([(cx - side * 7, 38), (cx, 34), (cx + side * 8, 37)], pale[3] if side < 0 else gold[3])
        # the gold sceptre in front of the armour, held in a gauntlet, its forked head rising out of frame
        p.fill(p.poly([(51, 66), (54, 66), (57, 10), (55, 10)]), gold, radius=1, depth=1.5, bias=0.12)
        p.stroke([(53, 64), (56, 12)], pale[4])
        for dx in (-3, 0, 3):
            p.stroke([(56, 12), (56 + dx, 4), (round(56 + dx * 1.3), 0)], gold[3])
        p.fill(p.ellipse(56, 12, 2.4, 2.4), gold, radius=1.2, depth=1.5, bias=0.2)
        p.px(56, 12, mix(pale[3], '#fffbe8', glow))
        p.fill(p.ellipse(54, 46, 4, 3.4), gold, radius=2, depth=2.5, bias=-0.05, texture=0.04)  # the gauntlet's grip
        for y in (44, 46, 48):
            p.stroke([(51, y), (56, y)], gold[1])
    with p.at(0, head):
        # the gorget
        p.fill(p.poly([(25, 32), (39, 32), (42, 40), (22, 40)]), gold, radius=2.5, depth=3)
        for y in (34, 37):
            p.stroke([(25, y), (39, y)], gold[1])
        # the crown: a fan of tall, narrow gold plates rising behind the face, the middle ones tallest
        plates = [(23, 6, 0.05), (26, 1, 0.12), (29, -3, 0.18), (32, -6, 0.15), (35, -3, 0.0), (38, 1, -0.08), (41, 6, -0.15)]
        for k, (x0, top, bias_) in enumerate(plates):
            lean = (x0 - 32) * 0.25
            p.fill(p.poly([(x0 - 1.6, 22), (x0 + 1.6, 22), (x0 + 1.4 + lean, top + 2), (x0 + lean, top), (x0 - 1.4 + lean, top + 2)]),
                   gold, radius=1.2, depth=2, bias=bias_, texture=0.03)
            p.stroke([(x0 - 1, 20), (round(x0 - 0.6 + lean), top + 2)], pale[3] if x0 <= 32 else gold[3])
        # the helm shell round the void
        hm = [(25, 26), (24, 16), (27, 9), (32, 7), (37, 9), (40, 16), (39, 26), (36, 31), (28, 31)]
        p.fill(p.poly(hm), gold, radius=4, depth=5, texture=0.03)
        # the edge wings: from the jaw they sweep out, then bend sharply up to tall points
        for side in (-1, 1):
            w = [(32 + side * 6, 29), (32 + side * 10, 27), (32 + side * 17, 23), (32 + side * 19, 8), (32 + side * 21, 20),
                 (32 + side * 19, 26), (32 + side * 10, 30)]
            p.fill(p.poly(w), gold, radius=1.8, depth=2.5, bias=0.12 if side < 0 else -0.1, texture=0.03)
            p.stroke([(32 + side * 10, 28), (32 + side * 17, 24), (32 + side * 19, 10)], pale[3] if side < 0 else gold[3])
        # the void: a tall oval face full of deep space, stars drifting and twinkling inside
        vm = p.ellipse(32, 19 + p.dy * 0, 4.6, 8.4) & (p.yy >= 11 + p.dy)
        vm = p.ellipse(32, 19, 4.6, 8.4)
        yy, xx = p.yy, p.xx
        a = 2 * math.pi * t
        n = _vnoise((xx + 4 * math.cos(a)) / 3.0, (yy + 4 * math.sin(a)) / 3.0)
        lo, hi = np.array((4, 6, 26), float), np.array((70, 90, 200), float)
        vcol = lo + (hi - lo) * (n[..., None] ** 1.5)
        p.rgb[vm] = vcol[vm].astype(np.uint8)
        p.a |= vm
        rng2 = np.random.default_rng(67)
        for _ in range(8):
            sx_, sy_ = float(rng2.uniform(0, 9)), float(rng2.uniform(12, 27))
            ph = float(rng2.random())
            x = int(round(28 + ((sx_ + t * 3) % 9)))
            y = int(round(sy_)) + p.dy
            if 0 <= y < 64 and vm[y, x]:
                p.rgb[y, x] = mix((110, 130, 220), (255, 255, 255), pulse(t, ph, 2))
        # the void's rim
        rim_ = vm & ~np.roll(vm, 1, 0) | vm & ~np.roll(vm, -1, 0) | vm & ~np.roll(vm, 1, 1) | vm & ~np.roll(vm, -1, 1)
        p.rgb[rim_] = np.array(gold[0], np.uint8)
    p.rim('#8ab0ff', side=1, amount=0.35)
    p.rim('#ffe0a0', side=-1, amount=0.2)
    p.outline('#05060e')
    return p


# ---------------------------------------------------------------------------------------------------------------
def empyrean(t=0.0):
    """after the owner's picture: an Empyrean - a slender bat-like being with no face: a tall, flared, hood-like head
    split by a dark seam, great membranous wings spread behind it, olive robes trimmed in teal with a green pendant,
    long thin arms crossed with clawed fingers; a pale cathedral arch behind. Moves: the wings breathe open and closed
    a little, the pendant glows, the claws flex, motes drift in the light."""
    p = Portrait()
    painted_bg(p, (64, 70, 74), (226, 226, 214), 32, 4, 46, steps=6)
    for x0 in (4, 56):  # columns
        for y in range(64):
            for dx in range(4):
                bg_px(p, x0 + dx, y, (110, 112, 112) if dx else (140, 140, 136))
    wing = ramp('#9a6a62', spread=0.7, hue_shift=0.05)
    bone = ramp('#6a4a46', spread=0.7)
    robe = ramp('#5a5a2e', spread=0.72, hue_shift=0.05)
    teal = ramp('#2e7a66', spread=0.6)
    skin = ramp('#6a4048', spread=0.72, hue_shift=0.04)
    headc = ramp('#4a3036', spread=0.7, hue_shift=0.04)
    body, head = breath(t), breath(t, -0.1)
    spread_ = sway(t, 0.0, 1)
    with p.at(0, body):
        # the wings: membranes between long finger bones, spread behind
        for side in (-1, 1):
            sx = 32 + side * 10
            tip = (32 + side * (27 + spread_), 2)
            elbow = (32 + side * (22 + spread_), 14)
            mem = [(sx, 22), elbow, tip, (32 + side * (30 + spread_), 30), (32 + side * (26 + spread_), 50),
                   (32 + side * 22, 40), (32 + side * 18, 56), (32 + side * 14, 44)]
            p.fill(p.poly(mem), wing, radius=4, depth=3, texture=0.08, bias=0.05 if side < 0 else -0.08)
            for end in (tip, (32 + side * (30 + spread_), 30), (32 + side * (26 + spread_), 50), (32 + side * 18, 56)):
                p.stroke([(sx, 22), elbow if end == tip else (int((sx + end[0]) / 2), int((22 + end[1]) / 2)), end], bone[1])
            p.stroke([(sx, 22), elbow, tip], bone[3])
        # the robe: olive, a broad collar trimmed in teal, falling straight
        p.fill(p.poly([(14, 66), (16, 40), (22, 32), (42, 32), (48, 40), (50, 66)]), robe, radius=6, depth=5, texture=0.05)
        p.fill(p.poly([(16, 40), (22, 32), (32, 36), (42, 32), (48, 40), (40, 44), (32, 46), (24, 44)]), robe, radius=3,
               depth=3, bias=0.12)
        p.stroke([(17, 40), (24, 44), (32, 46), (40, 44), (47, 40)], teal[3])
        p.stroke([(18, 41), (24, 45), (32, 47), (40, 45), (46, 41)], teal[1])
        for pts in ([(22, 48), (20, 66)], [(42, 48), (44, 66)], [(32, 48), (32, 66)]):
            p.stroke(pts, robe[1])
        g = pulse(t, 0.0, 1)
        p.stroke([(32, 46), (32, 49)], (200, 190, 120))
        p.fill(p.poly([(30, 49), (34, 49), (32, 53)]), teal, radius=1, depth=1, bias=0.2 + 0.2 * g)
        p.px(32, 50, mix('#60ffc0', '#e0fff0', g))
        # long thin arms crossed before the robe, clawed fingers hanging
        for side in (-1, 1):
            arm = [(32 + side * 14, 44), (32 + side * 6, 54), (32 - side * 4, 58)]
            p.fill(p.poly(ribbon(arm, 3.2, 2.4)), skin, radius=1.2, depth=1.5, bias=0.05 - 0.1 * (side > 0))
            fl = sway(t, 0.3 if side < 0 else 0.7, 1)
            hx, hy = arm[-1]
            for k in range(3):
                p.stroke([(hx, hy), (hx - side * (1 + k) + fl, hy + 4 + k), (hx - side * (2 + k) + fl, hy + 7 + k)], skin[1 + k % 2])
    with p.at(0, head):
        # the faceless head: a tall narrow hood flaring wide at the top, a dark seam down its middle
        hd = [(28, 34), (27, 22), (26, 14), (22, 9), (18, 6), (24, 5), (32, 4), (40, 5), (46, 6), (42, 9), (38, 14), (37, 22),
              (36, 34)]
        p.fill(p.poly(hd), headc, radius=4, depth=4, texture=0.08)
        p.fill(p.poly([(26, 10), (32, 6), (38, 10), (36, 14), (32, 12), (28, 14)]), headc, radius=1.5, depth=2, bias=0.2)
        p.stroke([(32, 8), (32, 33)], (20, 10, 14))
        p.stroke([(31, 14), (31, 30)], headc[3])
        p.stroke([(33, 14), (33, 30)], headc[1])
        p.stroke([(19, 6), (32, 5), (45, 6)], headc[4])
        p.pixels([(30, 18), (30, 24), (34, 21)], headc[0])
        # the raised collar behind the head
        p.fill(p.poly([(20, 36), (23, 24), (27, 28), (29, 36)]), robe, radius=2, depth=2.5, bias=0.1)
        p.fill(p.poly([(44, 36), (41, 24), (37, 28), (35, 36)]), robe, radius=2, depth=2.5, bias=-0.12)
        p.stroke([(21, 35), (24, 26)], teal[2]), p.stroke([(43, 35), (40, 26)], teal[1])
    p.rim('#fff8e8', side=1, amount=0.3)
    p.rim('#fff0d8', side=-1, amount=0.25)
    p.outline('#14100e')
    for x0, ph in ((12, 0.0), (52, 0.4), (30, 0.7)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(40 - u * 36)), mix('#ffffff', '#a0a0a0', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def naaz(t=0.0):
    """after the owner's picture: the Naaz-Rokha Alliance - two species: a black panther-like alien with golden eyes and
    pointed ears in a dark suit with a rust-leather chest harness, a heavy rifle held across the body; on its back a
    small monkey-like Rokha with big ears, a bald pink head and red goggle-eyes, peering over its shoulder; jungle
    behind. Neutral. Moves: both breathe on different beats, the panther blinks and its ear flicks, the Rokha looks
    around and its goggles glint, leaves sway."""
    p = Portrait()
    painted_bg(p, (34, 54, 30), (150, 176, 110), 50, 4, 50, steps=6)
    for x0, ph in ((4, 0.0), (10, 0.3), (58, 0.6), (54, 0.8)):  # hanging vines and fronds
        for y in range(0, 50):
            x = x0 + round(2 * math.sin(y / 7 + ph * 6)) + (sway(t, ph, 1) if y > 25 else 0)
            bg_px(p, x, y, (52, 84, 44) if y % 5 else (70, 108, 56))
    fur = ramp('#32323e', spread=0.85, hue_shift=0.06)
    suit = ramp('#2a2830', spread=0.6)
    leather = ramp('#7a3a26', spread=0.7, hue_shift=0.05)
    gun = ramp('#5a5a5c', spread=0.75)
    pink = ramp('#c89a88', spread=0.6, hue_shift=0.04)
    olive = ramp('#5a6230', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    small = breath(t, 0.45)
    with p.at(0, small):
        # the Rokha on its back, peering over the right shoulder: big round ears, a bald pink head, red goggle eyes
        p.fill(p.poly([(42, 30), (44, 22), (52, 20), (58, 24), (58, 32)]), olive, radius=3, depth=3, texture=0.06)
        look = sway(t, 0.2, 1)
        with p.at(look, 0):
            for ex in (42.5, 57.5):
                p.fill(p.ellipse(ex, 12, 3.2, 3.6), pink, radius=2, depth=2, bias=-0.05)
                p.fill(p.ellipse(ex, 12, 1.6, 2.2), pink, flat=1, edge=False)
            p.fill(p.ellipse(50, 13, 7, 7.5), pink, radius=5, depth=5, texture=0.05)
            p.stroke([(46, 8), (49, 7)], pink[3])
            gl = blink(t, 0.3, 2)
            for ex in (47, 53):
                p.fill(p.ellipse(ex, 14, 2.2, 2), gun, radius=1, depth=1.5, bias=0.1)
                p.px(ex, 14, (255, 220, 220) if gl else (210, 40, 40)), p.px(ex + 1, 14, (140, 20, 20))
            p.stroke([(48, 18), (52, 18)], pink[0])
            p.px(50, 16, pink[1])
        # its little hands gripping the panther's shoulder
        p.fill(p.ellipse(45, 26, 2.2, 1.8), pink, radius=1, depth=1.2)
        p.fill(p.ellipse(56, 26, 2.2, 1.8), pink, radius=1, depth=1.2, bias=-0.1)
    with p.at(0, body):
        # the panther's body: dark suit, a rust-leather chest harness with straps, a dark sleeve and gauntlet
        p.fill(p.poly([(2, 66), (4, 44), (12, 34), (24, 30), (40, 30), (52, 34), (60, 44), (62, 66)]), suit, radius=7, depth=6,
               texture=0.05)
        p.fill(p.poly([(16, 38), (26, 34), (40, 34), (48, 38), (44, 52), (32, 54), (20, 52)]), leather, radius=4, depth=4,
               texture=0.08)
        p.stroke([(24, 34), (30, 54)], leather[0]), p.stroke([(40, 34), (36, 54)], leather[0])
        p.stroke([(44, 32), (52, 44)], suit[3])
        p.fill(p.ellipse(32, 40, 1.6, 1.6), gun, radius=1, depth=1, bias=0.2)
        p.fill(p.poly([(4, 46), (12, 42), (14, 50), (6, 54)]), gun, radius=1.5, depth=2, bias=0.05)  # arm band
        p.px(8, 48, (210, 40, 40))
        # the heavy rifle across the body, wrapped in cord
        p.fill(p.poly([(-2, 60), (40, 50), (52, 48), (54, 52), (42, 56), (-2, 66)]), gun, radius=2, depth=2.5, texture=0.05)
        for x in range(4, 24, 3):
            p.stroke([(x, 58 - x // 5), (x + 1, 62 - x // 5)], (110, 90, 60))
        p.fill(p.poly([(36, 52), (46, 49), (48, 56), (38, 58)]), gun, radius=1.5, depth=2, bias=0.2)
        p.fill(p.ellipse(50, 56, 4, 3.2), fur, radius=2, depth=2, bias=0.05)  # the gloved hand
        p.fill(p.ellipse(14, 60, 3.6, 3), fur, radius=2, depth=2, bias=0.05)
    with p.at(0, head):
        # the collar of the suit
        p.fill(p.poly([(16, 32), (34, 30), (38, 38), (20, 40)]), suit, radius=2, depth=2, bias=0.1)
        # the panther head, turned a little to the left: tall pointed ears, broad cheeks, a lighter muzzle
        p.fill(p.poly([(14, 14), (16, 2), (23, 9)]), fur, radius=1.5, depth=2, bias=0.1)
        tw = 1 if blink(t, 0.7, 2) else 0
        p.fill(p.poly([(30, 9), (37 + tw, 1), (38, 13)]), fur, radius=1.5, depth=2, bias=-0.05)
        p.fill(p.poly([(16, 6), (21, 10), (17, 11)]), ramp('#5a4a50', spread=0.4), flat=1, edge=False)
        hd = [(11, 18), (14, 10), (22, 7), (31, 8), (37, 12), (40, 19), (38, 26), (33, 31), (24, 33), (15, 31), (9, 25)]
        p.fill(p.poly(hd), fur, radius=8, depth=7, texture=0.05, bias=0.05)
        with p.at(-2, 0):  # the face, turned a little more to the left
            # the muzzle: a lighter, rounded pad toward the left, a dark nose, the mouth line
            mz = ramp('#4a4852', spread=0.6)
            p.fill(p.poly([(11, 22), (16, 20), (24, 21), (27, 26), (24, 31), (16, 31), (11, 27)]), mz, radius=3, depth=3, bias=0.1)
            p.fill(p.poly([(13, 21), (18, 21), (17, 24), (15, 24)]), ramp('#141418', spread=0.4), flat=1, edge=False)
            p.px(14, 21, (80, 78, 90))
            p.stroke([(16, 25), (16, 27)], fur[0])
            p.stroke([(13, 28), (16, 27), (20, 28), (23, 27)], fur[0])
            p.pixels([(19, 25), (21, 26), (20, 24)], mz[0])
            # the brow and almond-shaped golden eyes (a slow blink)
            p.stroke([(17, 15), (22, 14)], fur[0]), p.stroke([(26, 14), (32, 15)], fur[0])
            if blink(t, 0.55, 2):
                p.stroke([(18, 17), (22, 17)], fur[0]), p.stroke([(27, 17), (31, 17)], fur[0])
            else:
                for ex in (18, 27):
                    p.px(ex, 17, fur[0]), p.px(ex + 1, 16, (236, 200, 50)), p.px(ex + 1, 17, (236, 200, 50))
                    p.px(ex + 2, 16, (20, 16, 8)), p.px(ex + 2, 17, (20, 16, 8)), p.px(ex + 3, 16, (210, 168, 36))
                    p.px(ex + 3, 17, (190, 150, 30)), p.px(ex + 4, 17, fur[0])
            p.stroke([(17, 18), (17, 20)], fur[0])
        p.pixels([(22, 9), (27, 9), (36, 20), (35, 15), (30, 11)], fur[4])
    p.rim('#d8ffb0', side=-1, amount=0.3)
    p.rim('#a0e070', side=1, amount=0.25)
    p.outline('#0a0e08')
    return p


# ---------------------------------------------------------------------------------------------------------------
def nomad(t=0.0):
    """after the owner's picture: the Nomad - a robed figure: a dark green hooded cloak, a flat rust-red cap over the hood,
    the face hidden behind a wrapped mask with three glowing green lenses and a rust respirator; under the cloak an
    armoured rust-red space suit with diagonal straps, a green light clasp on the shoulder, a pistol at the hip; a
    dark neon city behind. Moves: the lenses pulse, the cloak stirs, the clasp blinks, breathing, rain of light."""
    p = Portrait()
    painted_bg(p, (8, 22, 26), (40, 90, 92), 40, 16, 44, steps=6)
    for x0, w, h in ((0, 6, 50), (8, 4, 40), (50, 5, 54), (58, 6, 44)):  # towers with lit windows
        for y in range(64 - h, 64):
            for dx in range(w):
                bg_px(p, x0 + dx, y, (16, 34, 38))
        for y in range(64 - h + 2, 64, 4):
            k = pulse(t, (x0 + y) * 0.07, 1)
            bg_px(p, x0 + 1 + (y // 4) % max(1, w - 1), y, mix((60, 120, 110), (180, 255, 200), k) if (x0 + y) % 3 else (200, 120, 60))
    cloak = ramp('#2a3c32', spread=0.7, hue_shift=0.05)
    suit = ramp('#8a3a24', spread=0.72, hue_shift=0.05)
    wrap = ramp('#24262a', spread=0.6)
    hat = ramp('#7a3022', spread=0.65)
    metal = ramp('#4a4c50', spread=0.7)
    body, head = breath(t), breath(t, -0.1)
    lens = pulse(t, 0.0, 1)
    green = mix('#40c040', '#c8ffa0', lens)
    with p.at(0, body):
        # the cloak: wide, falling in heavy folds
        cl = [(0, 66), (2, 44), (10, 32), (22, 26), (42, 26), (54, 32), (62, 44), (64, 66)]
        p.fill(p.poly([(x + (sway(t, 0.1, 1) if y > 50 else 0), y) for x, y in cl]), cloak, radius=8, depth=6, texture=0.05)
        for pts in ([(8, 40), (6, 66)], [(14, 36), (12, 62)], [(56, 40), (58, 66)], [(50, 36), (52, 60)]):
            p.stroke(pts, cloak[1])
        p.stroke([(10, 38), (8, 60)], cloak[3])
        # the armoured suit showing at the front: rust-red plates, diagonal straps
        p.fill(p.poly([(22, 34), (40, 34), (44, 66), (20, 66)]), suit, radius=4, depth=4, texture=0.05)
        for y0 in (40, 46):
            p.stroke([(22, y0 + 4), (40, y0 - 2)], suit[0])
            p.stroke([(22, y0 + 5), (40, y0 - 1)], suit[3])
        p.stroke([(31, 52), (31, 66)], suit[1])
        # the pistol at the hip, its green light
        p.fill(p.poly([(16, 52), (28, 52), (28, 56), (20, 58), (16, 56)]), metal, radius=1.5, depth=2, bias=0.1)
        p.px(26, 53, green)
        # the cloak's edges drawn over the suit
        p.fill(p.poly([(14, 34), (22, 30), (24, 66), (12, 66)]), cloak, radius=3, depth=3, bias=0.05)
        p.fill(p.poly([(50, 34), (42, 30), (40, 66), (52, 66)]), cloak, radius=3, depth=3, bias=-0.1)
        # the green light clasp on the shoulder
        p.fill(p.ellipse(46, 30, 3, 2.4), metal, radius=1.2, depth=1.5, bias=0.15)
        p.glow(46, 30, '#f0ffe0', mix('#40c040', '#b0ff90', pulse(t, 0.3, 2)), r=1, strength=0.8)
    with p.at(0, head):
        # the hood and the cowl wrapped round the head and shoulders
        p.fill(p.poly([(18, 34), (19, 18), (23, 10), (32, 7), (41, 10), (45, 18), (46, 34), (32, 38)]), cloak, radius=6, depth=5,
               texture=0.05)
        # the face wrap: dark cloth, the mask beneath
        p.fill(p.poly([(23, 16), (32, 13), (41, 16), (41, 28), (36, 33), (28, 33), (23, 28)]), hat, radius=4, depth=4,
               texture=0.08)
        for y in (20, 25, 30):
            p.stroke([(24, y), (40, y - 1)], hat[0])
        # three glowing green lenses: two eyes and one above between them
        for ex, ey in ((28, 21), (36, 21), (32, 17)):
            p.fill(p.ellipse(ex, ey, 2.2, 2.2), metal, radius=1, depth=1.5, bias=0.1)
            p.glow(ex, ey, '#f4ffe8', green, r=1, strength=0.75 + 0.25 * lens)
        # the rust respirator below
        p.fill(p.poly([(29, 25), (35, 25), (36, 29), (32, 32), (28, 29)]), wrap, radius=1.5, depth=2, bias=0.15)
        p.stroke([(30, 28), (34, 28)], wrap[0])
        p.stroke([(30, 27), (34, 27)], wrap[3])
        # the flat rust cap over the hood
        p.fill(p.poly([(20, 10), (24, 4), (40, 4), (44, 10), (42, 12), (22, 12)]), hat, radius=2.5, depth=3, texture=0.05)
        p.stroke([(22, 11), (42, 11)], hat[0])
        p.stroke([(24, 5), (40, 5)], hat[4])
    p.rim('#7affd0', side=1, amount=0.3)
    p.rim('#ffb070', side=-1, amount=0.15)
    p.outline('#040a0a')
    for x0, ph in ((14, 0.0), (48, 0.35), (30, 0.7), (60, 0.5)):  # falling motes of light
        u = (t * 2 + ph) % 1.0
        p.px(x0, int(u * 64), (120, 200, 180))
    return p


# ---------------------------------------------------------------------------------------------------------------
def titans(t=0.0):
    """after the owner's picture: a Titan of Ul - a huge humanoid robot with a silver body: a small domed head with one
    central eye sunk between massive shoulders, heavy segmented chest plating, purple light pulsing in every seam, the
    eye and a ring on the chest, one fist wreathed in purple energy; misty jungle haze behind. Moves: the purple glow
    pulses through the seams, the eye throbs, the energy round the fist flickers, a slow heavy breath."""
    p = Portrait()
    painted_bg(p, (60, 64, 58), (196, 196, 180), 40, 6, 52, steps=6)
    for x in range(64):  # treetops in the haze
        h = int(8 + 3 * math.sin(x * 0.4) + 2 * math.sin(x * 1.3))
        for y in range(64 - h, 64):
            bg_px(p, x, y, (52, 70, 50) if (x + y) % 3 else (64, 84, 58))
    silver = ramp('#6c7876', spread=0.66, hue_shift=0.05)
    dark = ramp('#2e3434', spread=0.6)
    g = pulse(t, 0.0, 1)
    seam = mix('#8a30c8', '#f0a8ff', g)
    seam_dim = mix('#3a1060', '#8a40c0', g)
    body = breath(t, 0.0, 1)

    def glowline(pts):
        p.stroke(pts, seam_dim)
        p.stroke([(x, y + 1) for x, y in pts], dark[0])
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            p.px((x0 + x1) // 2, (y0 + y1) // 2, seam)

    with p.at(0, body):
        # the massive torso
        p.fill(p.poly([(0, 66), (0, 40), (8, 30), (20, 26), (44, 26), (56, 30), (64, 40), (64, 66)]), silver, radius=9, depth=8,
               texture=0.06)
        # chest plates: two broad pectoral plates, segmented abdomen
        for side in (-1, 1):
            p.fill(p.poly([(32 + side * 2, 30), (32 + side * 14, 30), (32 + side * 16, 40), (32 + side * 8, 46), (32 + side * 2, 44)]),
                   silver, radius=4, depth=4, bias=0.08 if side < 0 else -0.08, texture=0.05)
        for k in range(3):
            y = 48 + k * 6
            p.fill(p.poly([(22 + k, y), (42 - k, y), (41 - k, y + 5), (23 + k, y + 5)]), silver, radius=2, depth=2.5,
                   bias=0.05 - 0.05 * k, texture=0.05)
        glowline([(18, 30), (24, 30), (30, 30)]), glowline([(34, 30), (40, 30), (46, 30)])
        glowline([(17, 41), (24, 46), (30, 45)]), glowline([(34, 45), (40, 46), (47, 41)])
        for y in (47, 53, 59):
            glowline([(22, y), (32, y + 1), (42, y)])
        glowline([(32, 31), (32, 44)])
        # the glowing ring on the chest
        p.fill(p.ellipse(32, 38, 3.4, 3.4), dark, radius=1.5, depth=2, bias=0.1)
        p.glow(32, 38, '#fff0ff', seam, r=1, strength=0.75 + 0.25 * g)
        # the shoulders: great rounded plates, glowing joints
        for side in (-1, 1):
            cx = 32 + side * 23
            p.fill(p.ellipse(cx, 34, 11, 9.5), silver, radius=7, depth=7, bias=0.05 if side < 0 else -0.14, texture=0.06)
            p.fill(p.ellipse(cx, 30, 7, 4), silver, radius=3, depth=3, bias=0.2 if side < 0 else 0.0)
            glowline([(cx - 8, 38), (cx, 41), (cx + 8, 38)])
            p.glow(cx + side * -6, 44, '#f8e0ff', seam, r=0, strength=0.8)
        # the fist on the left, wreathed in purple energy that flickers
        p.fill(p.ellipse(7, 57, 7, 6), silver, radius=3.5, depth=4, bias=0.05, texture=0.05)
        for k in range(3):
            p.stroke([(3 + k * 3, 53), (3 + k * 3, 60)], dark[1])
        for k in range(20):
            ph = k / 20
            u = (t * 2 + ph) % 1.0
            x = 7 + round(7 * math.cos(ph * 6.28 + t * 6.28) * (1 - u * 0.4)) + sway(t, ph, 1, 3)
            y = 58 - int(u * 18) + round(3 * math.sin(ph * 6.28))
            if 0 <= y < 64 and 0 <= x < 64:
                c = mix((250, 200, 255), (140, 50, 190), u)
                p.px(x, y, c)
                if u < 0.4:
                    p.px(x + 1, y, c)
    with p.at(0, breath(t, -0.1)):
        # the small domed head sunk between the shoulders, a single central eye
        p.fill(p.poly([(26, 28), (26, 18), (28, 12), (32, 10), (36, 12), (38, 18), (38, 28), (32, 30)]), silver, radius=4, depth=5,
               texture=0.05)
        p.stroke([(32, 11), (32, 15)], silver[4])
        p.fill(p.poly([(28, 17), (36, 17), (35, 24), (32, 26), (29, 24)]), dark, radius=1.5, depth=2, bias=-0.05)
        p.fill(p.ellipse(32, 20, 2.4, 2.4), dark, radius=1, depth=1.5, bias=0.2)
        p.glow(32, 20, '#ffffff' if g > 0.8 else '#ffe8ff', seam, r=1, strength=0.8 + 0.2 * g)
        p.stroke([(29, 24), (35, 24)], seam_dim)
        p.pixels([(27, 16), (37, 16)], seam)
    p.rim('#f0e0ff', side=1, amount=0.3)
    p.rim('#d8c0ff', side=-1, amount=0.2)
    p.outline('#0c0e0e')
    return p


# ---------------------------------------------------------------------------------------------------------------
def crimson(t=0.0):
    """after the owner's picture: the Crimson Rebellion - the Creuss vessel gone chaotic: a tall leaning steel-blue helm
    with red-lit seams, armour plates separating and drifting apart, red energy (its body) bursting out of every gap in
    wild, tangled streams; purple space with drifting rocks and a bright star. Moves: the plates jitter and drift
    apart and back, the red streams writhe, the seams flare, a glitch now and then."""
    t = t % 1.0
    p = Portrait()
    painted_bg(p, (34, 10, 34), (120, 60, 120), 14, 12, 44, steps=6)
    for dx in range(-3, 4):  # the bright star at the upper left
        for dy in range(-3, 4):
            d = math.hypot(dx, dy)
            if d <= 3.2:
                bg_px(p, 12 + dx, 12 + dy, mix((150, 90, 180), (255, 240, 255), 1 - d / 3.2))
    rng = np.random.default_rng(71)
    for _ in range(9):  # drifting rocks
        x, y = int(rng.integers(0, 64)), int(rng.integers(0, 64))
        for ddx, ddy in ((0, 0), (1, 0), (0, 1)):
            bg_px(p, (x + ddx + int(t * 4)) % 64, y + ddy if y + ddy < 64 else y, (60, 40, 60))
    steel = ramp('#3e5464', spread=0.72, hue_shift=0.04)
    red = mix('#c01020', '#ff4050', pulse(t, 0.0, 2))
    sep = 0.5 + 0.5 * wave(t, 0.0, 1)  # how far the plates have drifted apart

    def plate(pts, ox, oy, bias=0.0, seed=0):
        jit = 1 if blink(t, (seed * 0.13) % 1, 1) else 0
        dx, dy = ox * sep + jit, oy * sep
        p.fill(p.poly([(x + dx, y + dy) for x, y in pts]), steel, radius=2.5, depth=3, bias=bias, texture=0.05, seed=seed)
        return dx, dy

    # the red core - the creature itself - glowing between the plates
    for cx_, cy_, r_ in ((32, 34, 9), (30, 48, 7), (22, 40, 4), (42, 42, 4)):
        for y in range(cy_ - r_, cy_ + r_ + 1):
            for x in range(cx_ - r_, cx_ + r_ + 1):
                d = math.hypot(x - cx_, y - cy_) / r_
                if d <= 1 and 0 <= x < 64 and 0 <= y < 64:
                    p.px(x, y, mix((90, 0, 20), (255, 70, 80), (1 - d) * (0.6 + 0.4 * pulse(t, 0.2, 2))))
    # armour plates, separating
    plate([(8, 40), (14, 30), (24, 28), (26, 38), (18, 44)], -3, -2, 0.05, 1)            # left shoulder
    plate([(56, 42), (52, 32), (42, 30), (40, 38), (48, 46)], 3, -1, -0.12, 2)           # right shoulder
    plate([(24, 38), (32, 36), (40, 38), (38, 46), (26, 46)], 0, 2, 0.05, 3)              # chest
    plate([(22, 48), (30, 47), (31, 56), (23, 58)], -2, 2, 0.0, 4)                        # belly left
    plate([(34, 47), (42, 48), (41, 58), (33, 56)], 2, 3, -0.1, 5)                        # belly right
    plate([(4, 46), (12, 46), (12, 58), (4, 60)], -3, 1, 0.0, 6)                          # left arm
    plate([(50, 48), (58, 48), (60, 60), (52, 60)], 3, 2, -0.15, 7)                       # right arm
    plate([(18, 58), (28, 60), (26, 66), (16, 66)], -1, 1, 0.0, 8)
    plate([(36, 60), (46, 58), (48, 66), (38, 66)], 1, 1, -0.1, 9)
    # the tall helm, leaning, a red-lit seam across it (drifts up and away a little)
    hx, hy = 2 * sep, -2 * sep
    with p.at(round(hx), round(hy)):
        hm = [(28, 28), (26, 18), (28, 8), (34, 2), (40, 2), (42, 10), (40, 20), (36, 28)]
        p.fill(p.poly(hm), steel, radius=5, depth=6, texture=0.04)
        p.stroke([(27, 16), (34, 12), (41, 12)], red)
        p.stroke([(28, 20), (35, 16), (40, 17)], mix(red, '#400008', 0.5))
        p.stroke([(30, 6), (36, 3)], steel[4])
    p.rim('#ff6080', side=1, amount=0.3)
    p.rim('#b070d0', side=-1, amount=0.2)
    p.outline('#0e040c')
    # the red streams bursting from every gap, writhing
    for k, (ctrl, ph) in enumerate((([(30, 30), (20, 22), (10, 26), (2, 18)], 0.0), ([(34, 30), (46, 22), (56, 26), (66, 16)], 0.2),
                                     ([(26, 44), (14, 50), (8, 40), (-2, 44)], 0.4), ([(38, 44), (50, 52), (58, 44), (66, 50)], 0.6),
                                     ([(32, 50), (28, 58), (36, 62), (30, 68)], 0.8), ([(28, 36), (18, 34), (16, 44), (6, 52)], 0.1),
                                     ([(36, 36), (46, 34), (48, 24), (58, 6)], 0.5), ([(32, 28), (26, 18), (20, 8), (22, -2)], 0.7))):
        s1, s2 = sway(t, ph, 3), sway(t, ph + 0.3, 3, cycles=2)
        ctrl = [ctrl[0], (ctrl[1][0] + s1, ctrl[1][1] - s2), (ctrl[2][0] - s2, ctrl[2][1] + s1), ctrl[3]]
        path = bezier(ctrl, 90)
        # tangle: a slow curl along the stream that writhes
        path = [(x + 2.0 * math.sin(j * 0.16 + 2 * math.pi * t + k), y + 2.0 * math.cos(j * 0.14 + 2 * math.pi * t + k))
                for j, (x, y) in enumerate(path)]
        stream(p, path, t, speed=1.5, ph=ph * 4, width=3 if k % 3 else 2, core='#ffe0e0', glow='#ff2030', fade=0.35)
    if blink(t, 0.3, 1):
        glitch(p, 24, 30, 2, '#ff2040', 0.3)
    elif blink(t, 0.75, 1):
        glitch(p, 44, 48, -2, '#ff60a0', 0.25)
    return p


# ---------------------------------------------------------------------------------------------------------------
def lastbastion(t=0.0):
    """after the owner's picture: a Last Bastion soldier - sleek silver-white armour plates over a black bodysuit, a
    white helmet with a big rounded blue visor (no face showing), orange accents at the collar, heavy shoulder rails
    with rods; behind, a ship descending on pillars of thruster fire over a burning sky. Neutral.
    Moves: breathing, light slides across the visor, the thruster flames flicker, embers drift."""
    p = Portrait()
    painted_bg(p, (96, 60, 40), (250, 200, 130), 14, 22, 46, steps=6)
    # the descending ship and its thruster fire (background)
    for x0 in (4, 12, 20):
        for y in range(10, 64):
            k = pulse(t, x0 * 0.05 + y * 0.03, 3)
            w = 2 + (y - 10) // 14
            for dx in range(-w, w + 1):
                bg_px(p, x0 + dx, y, mix((255, 170, 60), (255, 240, 200), k * (1 - abs(dx) / (w + 1))))
    for y in range(0, 11):
        for x in range(0, 28):
            if abs(x - 12) < 4 + y * 0.9 and y > 2 or abs(x - 12) < 3:
                bg_px(p, x, y, (70, 66, 70) if (x + y) % 4 else (96, 92, 96))
    for x in range(0, 28, 2):
        bg_px(p, x, 10, (120, 114, 116))
    for x in range(64):  # rocky ground
        h = int(6 + 2 * math.sin(x * 0.3) + 1.5 * math.sin(x * 1.1))
        for y in range(64 - h, 64):
            bg_px(p, x, y, (70, 56, 50))
    white = ramp('#c8ccd2', spread=0.55, hue_shift=0.03)
    black = ramp('#24262c', spread=0.6)
    visor = ramp('#2a6ab0', spread=0.6, hue_shift=0.04)
    orange = ramp('#e06a2a', spread=0.5)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # the black bodysuit, slim
        p.fill(p.poly([(12, 66), (14, 46), (22, 38), (28, 36), (36, 36), (42, 38), (50, 46), (52, 66)]), black, radius=6, depth=5,
               texture=0.05)
        for y in range(50, 66, 3):
            p.stroke([(18, y), (46, y)], black[1])
        # sculpted chest armour: two curved plates meeting at a V, a ridged abdomen piece
        for side in (-1, 1):
            p.fill(p.poly([(32, 40), (32 + side * 10, 38), (32 + side * 12, 44), (32 + side * 8, 50), (32 + side * 1, 50)]), white,
                   radius=3.5, depth=4, bias=0.08 if side < 0 else -0.1, texture=0.02)
        p.fill(p.poly([(28, 50), (36, 50), (35, 58), (32, 61), (29, 58)]), white, radius=2, depth=2, bias=0.05)
        p.stroke([(29, 53), (35, 53)], white[1]), p.stroke([(30, 56), (34, 56)], white[1])
        p.stroke([(27, 38), (32, 40), (37, 38)], orange[2])
        # the shoulder rails: narrow angled plates rising above the shoulders, rods across them, gauntlet lights
        for side in (-1, 1):
            rail = [(32 + side * 11, 40), (32 + side * 14, 30), (32 + side * 19, 28), (32 + side * 22, 32), (32 + side * 20, 64),
                    (32 + side * 15, 66), (32 + side * 13, 46)]
            p.fill(p.poly(rail), white, radius=2.5, depth=3, bias=0.08 if side < 0 else -0.14, texture=0.02)
            for y in (34, 38):
                p.stroke([(32 + side * 10, y + 1), (32 + side * 24, y - 1)], black[3])
                p.px(32 + side * 24, y - 1, white[4])
            for y in (48, 52, 56):
                p.stroke([(32 + side * 16, y), (32 + side * 20, y)], black[1])
            p.px(32 + side * 18, 61, (240, 200, 60))
            p.stroke([(32 + side * 15, 31), (32 + side * 20, 30)], orange[2])
    with p.at(0, head):
        # the neck seal, the white helmet, the big rounded blue visor
        p.fill(p.poly([(27, 28), (37, 28), (38, 36), (26, 36)]), black, radius=2, depth=2)
        hm = [(22, 18), (24, 9), (30, 4), (36, 4), (41, 9), (43, 18), (41, 26), (36, 30), (28, 30), (23, 26)]
        p.fill(p.poly(hm), white, radius=6, depth=6, texture=0.02)
        vz = [(25, 14), (30, 10), (38, 10), (42, 14), (42, 22), (38, 27), (30, 27), (25, 22)]
        vm = p.fill(p.poly(vz), visor, radius=4, depth=4, bias=0.05)
        # a reflection sliding across the visor
        gx = int(24 + ((t * 1.0) % 1.0) * 24)
        for k in range(3):
            for y in range(10, 28):
                x = gx + k - (y - 10) // 3
                if 0 <= x < 64 and 0 <= y + p.dy < 64 and vm[y + p.dy, x]:
                    p.rgb[y + p.dy, x] = np.array(mix(visor[3], (220, 240, 255), 0.5 - k * 0.15), np.uint8)
        p.stroke([(27, 12), (32, 11)], visor[4])
        p.fill(p.ellipse(23, 18, 2.2, 3.4), white, radius=1.2, depth=1.5, bias=0.1)  # the ear piece
        p.px(23, 18, orange[3])
    p.rim('#ffc070', side=-1, amount=0.4)
    p.rim('#ffe0b0', side=1, amount=0.25)
    p.outline('#140c08')
    for x0, ph in ((48, 0.0), (56, 0.35), (40, 0.7), (60, 0.55)):
        u = (t + ph) % 1.0
        if u < 0.85:
            p.px(x0 + sway(t, ph, 1, 2), int(round(56 - u * 50)), mix('#ffe0a0', '#802010', u))
    return p


# ---------------------------------------------------------------------------------------------------------------
def deepwrought(t=0.0):
    """after the owner's picture: a Deepwrought scholar - the Jol-Nar's own species, living in water: an orange-striped
    domed head crowned with long dark tendrils that drift, small dark eyes with glowing studs above, violet cheek fins,
    a pale body with a studded leather strap, a webbed clawed hand; teal water with light rays, kelp, and small
    glowing drones. Neutral. Moves: the tendrils drift and curl, the fins ripple, bubbles rise, the drones bob."""
    p = Portrait()
    painted_bg(p, (14, 60, 70), (100, 190, 190), 34, 0, 56, steps=6)
    for x0, w in ((12, 4), (34, 3), (50, 5)):  # light rays from above
        for y in range(64):
            for dx in range(w):
                x = x0 + dx + y // 5
                if 0 <= x < 64 and (x + y) % 2 == 0:
                    bg_px(p, x, y, (60 + 10 * pulse(t, x0 * 0.03, 1), 150, 150))
    for x0, ph in ((4, 0.0), (8, 0.4), (58, 0.2), (61, 0.7)):  # kelp
        for y in range(36, 64):
            x = x0 + round(2 * math.sin(y / 5 + ph * 6) + sway(t, ph, 1) * (64 - y) / 28)
            bg_px(p, x, y, (40, 110, 70) if y % 3 else (60, 140, 90))
    for cx_, cy_, ph in ((54, 10, 0.1), (8, 22, 0.6)):  # little drones with pink lights
        cy_ += round(1.5 * wave(t, ph, 1))
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                if dx * dx + dy * dy <= 5:
                    bg_px(p, cx_ + dx, cy_ + dy, (190, 200, 220))
        bg_px(p, cx_ + 1, cy_, mix((220, 60, 160), (255, 180, 230), pulse(t, ph, 2)))
        bg_px(p, cx_, cy_ + 3, (150, 160, 180)), bg_px(p, cx_, cy_ + 4, (150, 160, 180))
    skin = ramp('#d0b8a4', spread=0.6, hue_shift=0.04)
    orange = ramp('#d07030', spread=0.75, hue_shift=0.06)
    tend = ramp('#3a3446', spread=0.6)
    fin = ramp('#7a50b0', spread=0.6, hue_shift=0.04)
    strap = ramp('#5a3a28', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, head):
        # the tendrils: a crown of long dark tendrils drifting up and back in the water (behind the head)
        for k in range(10):
            a0 = math.pi * (0.95 + k * 0.11)
            bx, by = 32 + 11 * math.cos(a0), 16 + 9 * math.sin(a0)
            pts = []
            for j in range(14):
                u = j / 13
                ang = a0 + 0.25 * math.sin(2 * math.pi * (t + k * 0.13) + u * 3)
                pts.append((bx + math.cos(ang) * u * 16 + 2 * math.sin(u * 4 + k), by + math.sin(ang) * u * 16 - u * 4))
            p.fill(p.poly(ribbon(pts, 2.4, 1.0)), tend, radius=1, depth=1.5, bias=0.1 - 0.05 * (k % 2))
    with p.at(0, body):
        # the slim pale body, the studded strap across it, a webbed clawed hand raised
        p.fill(p.poly([(28, 30), (36, 30), (37, 40), (27, 40)]), skin, radius=2, depth=2, bias=-0.1)  # the neck
        p.fill(p.poly([(16, 66), (18, 48), (24, 40), (40, 40), (46, 48), (48, 66)]), skin, radius=6, depth=5, texture=0.05)
        for y in range(46, 64, 3):
            p.stroke([(26, y), (38, y)], skin[1])
        p.fill(p.poly(ribbon([(16, 42), (32, 52), (50, 60)], 3.4, 3.4)), strap, radius=1.2, depth=1.5, bias=0.1)
        for x, y in ((22, 45), (28, 49), (34, 53), (40, 56), (46, 59)):
            p.px(x, y, (220, 190, 120))
        p.fill(p.ellipse(52, 46, 4, 3.2), skin, radius=2, depth=2, bias=-0.05)
        for k in range(4):
            fl = sway(t, k * 0.15, 1)
            p.stroke([(50 + k * 2, 44), (51 + k * 2 + fl, 39), (53 + k * 2 + fl, 37)], skin[1])
            p.px(53 + k * 2 + fl, 37, orange[0])
        p.glow(54, 40, '#fff0c0', '#ffa040', r=0, strength=0.7)  # a little light held in its claws
    with p.at(0, head):
        # the domed head: orange with dark stripes, a heavy brow, small dark eyes, a wide mouth
        hd = [(20, 18), (22, 10), (28, 6), (36, 6), (42, 10), (44, 18), (42, 26), (38, 32), (32, 34), (26, 32), (22, 26)]
        p.fill(p.poly(hd), orange, radius=7, depth=6, texture=0.04)
        for k in range(5):  # stripes over the dome
            y = 8 + k * 3
            p.stroke([(23, y + 2), (32, y), (41, y + 2)], orange[1])
        # violet cheek fins (they ripple)
        for side in (-1, 1):
            r_ = sway(t, 0.3 if side < 0 else 0.8, 1)
            p.fill(p.poly([(32 + side * 11, 18), (32 + side * (18 + r_), 14), (32 + side * (19 + r_), 22), (32 + side * 11, 26)]),
                   fin, radius=1.5, depth=1.5, bias=0.05 if side < 0 else -0.1)
            for k in range(3):
                p.stroke([(32 + side * 12, 19 + k * 2), (32 + side * (17 + r_), 16 + k * 2)], fin[3])
        # the face: fish-like - large dark eyes set wide with glowing studs above, a wide down-turned mouth, gill lines
        for ex in (26, 38):
            p.fill(p.ellipse(ex, 19, 2.6, 2.4), ramp('#141418', spread=0.4), flat=1, edge=False)
            p.px(ex - 1, 18, (230, 240, 250)), p.px(ex, 18, (120, 130, 150))
            p.stroke([(ex - 3, 16), (ex + 3, 16)], orange[0])
            p.glow(ex, 13, '#fff0c0', '#ffb040', r=0, strength=0.6 + 0.3 * pulse(t, ex * 0.1, 2))
        p.fill(p.poly([(29, 21), (35, 21), (34, 25), (30, 25)]), orange, radius=1.5, depth=2, bias=0.2, edge=False)  # the snout
        p.px(31, 24, orange[0]), p.px(33, 24, orange[0])
        p.stroke([(25, 28), (28, 27), (32, 28), (36, 27), (39, 28)], (70, 30, 24))
        p.stroke([(27, 29), (37, 29)], orange[3])
        p.pixels([(26, 29), (38, 29)], (70, 30, 24))
        for k in range(3):
            p.stroke([(22 + k, 22 + k * 2), (23 + k, 24 + k * 2)], orange[0])
            p.stroke([(42 - k, 22 + k * 2), (41 - k, 24 + k * 2)], orange[0])
        p.fill(p.poly([(26, 31), (38, 31), (36, 34), (28, 34)]), skin, radius=1.5, depth=1.5, bias=0.1, edge=False)  # pale throat
    p.rim('#a0fff0', side=-1, amount=0.3)
    p.rim('#70d0ff', side=1, amount=0.25)
    p.outline('#06141a')
    for x0, ph in ((20, 0.0), (44, 0.3), (30, 0.6), (12, 0.8)):  # bubbles
        u = (t + ph) % 1.0
        x = x0 + sway(t, ph, 1, 2)
        y = int(round(62 - u * 62))
        p.px(x, y, (200, 250, 250))
        if u > 0.5:
            p.px(x + 1, y - 1, (140, 210, 220))
    return p


# ---------------------------------------------------------------------------------------------------------------
def _hall_and_starmap(p, t):
    """the Firmament's backdrop: a golden hall with a great swirling star map (blue / violet, white core) ringed with
    gold astrolabe lines; the light flows inward along the arms, stars twinkle, a marker orbits the ring"""
    yy, xx = np.mgrid[0:64, 0:64].astype(float)
    bg = np.zeros((64, 64, 3), float)
    bg[:] = (16, 12, 38)
    # golden pillars and arches at both sides
    for side in (-1, 1):
        for y in range(64):
            for k in range(7):
                x = k if side < 0 else 63 - k
                c = (150, 108, 56) if k in (2, 3) else (196, 150, 84) if k == 4 else (110, 76, 40)
                if k == 6:
                    c = (226, 186, 112)
                bg[y, x] = c
        for y in (7, 8):
            for k in range(9):
                x = k if side < 0 else 63 - k
                bg[y, x] = (214, 170, 96) if y == 7 else (120, 84, 44)
    for i in range(40):  # the arch: a gold band curving in from each pillar top
        a = math.pi * i / 39 * 0.5
        for side in (-1, 1):
            x = (7 + 12 * (1 - math.cos(a))) if side < 0 else (56 - 12 * (1 - math.cos(a)))
            y = 7 - 7 * math.sin(a)
            for w in (0, 1):
                xi, yi = int(round(x)), int(round(y)) + w
                if 0 <= xi < 64 and 0 <= yi < 64:
                    bg[yi, xi] = (220, 178, 100) if w == 0 else (130, 90, 46)
    # the star map
    cx, cy = 32, 11
    dx, dy = (xx + 0.5 - cx) / 23.0, (yy + 0.5 - cy) / 18.0
    r = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dy, dx)
    lr = np.log(r + 0.06)
    arm = 0.5 + 0.5 * np.cos(2 * th - 5.5 * lr)
    flow = 0.5 + 0.5 * np.cos(8 * lr + 2 * math.pi * t)
    fall = np.clip(1 - r, 0, 1)
    tex = _vnoise(xx / 4.0 + 3, yy / 4.0 + 1)
    k = fall ** 0.8 * (0.25 + 0.55 * arm + 0.18 * flow * arm + 0.12 * tex)
    k = k + np.exp(-(r / 0.13) ** 2) * (0.8 + 0.2 * pulse(t, 0.0, 1))
    cols = np.array([(16, 12, 38), (34, 22, 84), (52, 44, 150), (90, 70, 200), (150, 90, 220), (196, 160, 240),
                     (236, 230, 255), (255, 255, 255)], float)
    n = len(cols) - 1
    idx = np.clip(np.floor(k * n * 1.15 + (BAYER[yy.astype(int) % 4, xx.astype(int) % 4] - 0.5) * 0.9), 0, n).astype(int)
    m = (idx > 0) & (r < 1.05)
    bg[m] = cols[idx][m]
    # gold astrolabe lines (drawn over the map, faint where it is bright)
    def gold(x, y, c=(232, 200, 120), a=0.75):
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < 64 and 0 <= yi < 64:
            bg[yi, xi] = bg[yi, xi] * (1 - a) + np.array(c) * a
    ring = rot_ellipse(cx, cy + 3, 20, 6.5, -0.14, n=160)
    for x, y in ring:
        gold(x, y)
    for i in range(0, 120, 2):  # a dotted circle
        a = 2 * math.pi * i / 120
        gold(cx + 12 * math.cos(a), cy + 10 * math.sin(a), a=0.55)
    for (ox, oy, rr) in ((45, 6, 2.5), (19, 16, 2.0), (40, 18, 1.6)):
        for i in range(24):
            a = 2 * math.pi * i / 24
            gold(ox + rr * math.cos(a), oy + rr * math.sin(a), a=0.65)
    for (x0, y0, x1, y1) in ((cx, cy, 45, 6), (cx, cy, 19, 16), (24, 2, 40, 20)):
        for i in range(30):
            u = i / 29
            if i % 3:
                gold(x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, a=0.45)
    # a marker orbiting the ring once per loop
    a = 2 * math.pi * t
    c, s_ = math.cos(-0.14), math.sin(-0.14)
    mx, my = 20 * math.cos(a), 6.5 * math.sin(a)
    ox, oy = cx + mx * c - my * s_, cy + 3 + mx * s_ + my * c
    gold(ox, oy, (255, 250, 220), 1.0), gold(ox + 1, oy, (255, 230, 160), 0.8), gold(ox, oy + 1, (255, 230, 160), 0.6)
    # twinkling stars in the map and the dark
    for i, (sx, sy) in enumerate(((14, 4), (50, 3), (24, 22), (44, 24), (12, 12), (52, 15), (37, 2), (27, 6))):
        kk = pulse(t, i * 0.137, 1)
        if kk > 0.3:
            bg[sy, sx] = mix((180, 190, 255), (255, 255, 255), kk)
        if kk > 0.8:
            for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                bg[sy + ddy, sx + ddx] = bg[sy + ddy, sx + ddx] * 0.4 + np.array((200, 210, 255)) * 0.6
    p.bg = bg.clip(0, 255).astype(np.uint8)
    p.backdrop = (16, 12, 38)


def _void_eye(p, t):
    """the Obsidian's backdrop: a dusky violet sky between dark rock, a black void orb wreathed in boiling fire and
    burning orange astrolabe lines; a violet glint stirs in the void like a pupil"""
    yy, xx = np.mgrid[0:64, 0:64].astype(float)
    top, bot = np.array((84, 50, 74), float), np.array((26, 16, 30), float)
    v = np.clip(yy / 63.0, 0, 1)[..., None]
    bg = top * (1 - v) + bot * v
    q = np.floor((1 - v[..., 0]) * 4 + BAYER[yy.astype(int) % 4, xx.astype(int) % 4]) / 4
    bg = bot + (top - bot) * q[..., None]
    cx, cy = 32, 12
    dx, dy = xx + 0.5 - cx, (yy + 0.5 - cy) * 1.05
    r = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dy, dx)
    a = 2 * math.pi * t
    n1 = _vnoise(np.cos(th) * 2.4 + 1.3 * math.cos(a) + 5, np.sin(th) * 2.4 + 1.3 * math.sin(a) + 5)
    n2 = _vnoise(np.cos(th) * 5.0 - 1.1 * math.sin(a) + 9, np.sin(th) * 5.0 + 1.1 * math.cos(a) + 2)
    outer = 13.0 + 5.0 * n1 + 2.5 * n2
    halo = np.clip(1 - (r - 9) / 22.0, 0, 1) ** 1.6
    hq = np.floor(halo * 4 + BAYER[yy.astype(int) % 4, xx.astype(int) % 4] * 0.9) / 4
    bg = bg * (1 - hq[..., None] * 0.8) + np.array((190, 54, 30), float) * hq[..., None] * 0.8
    fire = (r >= 8.5) & (r <= outer)
    u = np.clip((r - 8.5) / np.maximum(outer - 8.5, 1), 0, 1)
    fcols = np.array([(255, 246, 190), (255, 196, 80), (250, 120, 40), (210, 56, 30), (130, 24, 26)], float)
    fi = np.clip(np.floor(u * 5 + (BAYER[yy.astype(int) % 4, xx.astype(int) % 4] - 0.5) * 0.8), 0, 4).astype(int)
    bg[fire] = fcols[fi][fire]
    void = r < 8.5
    bg[void] = (8, 4, 12)
    rimv = (r >= 7.8) & (r < 8.6)
    bg[rimv] = (60, 30, 50)
    # the glint in the void: a violet pupil that swells and fades
    gk = pulse(t, 0.25, 1)
    pr = np.sqrt((xx + 0.5 - 33) ** 2 + (yy + 0.5 - 11) ** 2)
    gm = pr < 1.0 + 1.6 * gk
    bg[gm] = bg[gm] * 0.3 + np.array(mix((90, 40, 140), (220, 170, 255), gk), float) * 0.7
    bg[11, 33] = mix((200, 160, 255), (255, 255, 255), gk)
    # dark rock walls at both sides
    for y in range(64):
        wl = 5 + int(2.5 * math.sin(y / 5.0) + 1.5 * math.sin(y / 2.3))
        wr = 5 + int(2.5 * math.sin(y / 4.0 + 2) + 1.5 * math.sin(y / 1.9))
        bg[y, :max(0, wl)] = (22, 14, 24)
        bg[y, 64 - max(0, wr):] = (22, 14, 24)
        if wl > 0:
            bg[y, wl - 1] = (70, 36, 44)
        if wr > 0:
            bg[y, 64 - wr] = (70, 36, 44)
    # burning astrolabe lines
    def line_px(x, y, c=(255, 170, 70), al=0.8):
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < 64 and 0 <= yi < 64 and not void[yi, xi]:
            bg[yi, xi] = bg[yi, xi] * (1 - al) + np.array(c) * al
    for x, y in rot_ellipse(cx, cy + 2, 22, 7, -0.12, n=170):
        line_px(x, y)
    for x, y in rot_ellipse(cx, cy, 17, 15, 0.3, n=150)[::2]:
        line_px(x, y, (255, 200, 110), 0.6)
    for (ox, oy, rr) in ((48, 5, 2.5), (14, 17, 2.2)):
        for i in range(24):
            aa = 2 * math.pi * i / 24
            line_px(ox + rr * math.cos(aa), oy + rr * math.sin(aa), (255, 190, 90), 0.7)
    mx, my = 22 * math.cos(-a), 7 * math.sin(-a)
    c, s_ = math.cos(-0.12), math.sin(-0.12)
    line_px(cx + mx * c - my * s_, cy + 2 + mx * s_ + my * c, (255, 250, 210), 1.0)
    p.bg = bg.clip(0, 255).astype(np.uint8)
    p.backdrop = (26, 16, 30)


def _council(p, t, evil):
    """the five robed beings of the Firmament, facing the screen: a bird (far left), a toad (left), the fan-crested one
    (centre, in front), a salamander (right), a cat (far right); hoods down, bunched behind their heads. evil = the
    Obsidian: the same beings possessed - near-black, glowing slanted eyes, burning cracks"""
    def dark(hexcol, spread=0.6, k=0.8):
        c = mix(hexrgb(hexcol), (10, 6, 14), k) if evil else hexrgb(hexcol)
        return ramp('#%02x%02x%02x' % tuple(c), spread=spread)
    robe = ramp('#18121e', spread=0.45) if evil else ramp('#283260', spread=0.6)
    robe_b = ramp('#130e17', spread=0.4) if evil else ramp('#202850', spread=0.55)
    eyec = [(255, 90, 40), (255, 200, 80), (255, 60, 60), (230, 120, 255), (255, 150, 40)]
    glowk = pulse(t, 0.0, 2)

    def eyes(i, pts, normal_col):
        """a pair of eyes; evil: glowing, slanted inward (an angry V)"""
        if not evil:
            for (x, y) in pts:
                p.px(x, y, normal_col)
            return
        col = eyec[i]
        for j, (x, y) in enumerate(pts):
            p.glow(x, y, (255, 240, 220), mix(col, (255, 230, 200), 0.3 * glowk), r=0, strength=0.75)
            p.px(x + (1 if j == 0 else -1), y - 1, mix(col, (40, 10, 10), 0.35))

    def cracks(pts):
        if evil:
            for (x, y) in pts:
                p.px(x, y, mix((190, 50, 20), (255, 170, 60), pulse(t, x * 0.05, 1)))

    def robed(cx, sy, w, ph, rb, hood_y):
        """a robe from the shoulders (sy) down, and the hood bunched behind the head"""
        p.fill(p.poly([(cx - w - 1, 66), (cx - w, sy + 4), (cx - w + 4, sy), (cx + w - 4, sy), (cx + w, sy + 4),
                       (cx + w + 1, 66)]), rb, radius=4, depth=3, texture=0.04, seed=cx)
        p.stroke([(cx - 2, sy + 6), (cx - 3, 64)], rb[1]), p.stroke([(cx + 3, sy + 7), (cx + 4, 64)], rb[1])
        p.fill(p.ellipse(cx, hood_y, w - 1.5, 4.2), rb, radius=2.5, depth=2.5, bias=0.05)

    # ---- far left: the bird
    with p.at(0, breath(t, 0.55)):
        robed(7, 35, 8, 0.55, robe_b, 33)
        with p.at(0, breath(t, 0.45)):
            cx, y0 = 7, 24
            sk = dark('#5a6684')
            p.stroke([(cx, y0 + 1), (cx - 2, y0 - 2), (cx - 4, y0 - 3)], sk[2])  # head feathers
            p.stroke([(cx + 1, y0 + 1), (cx + 1, y0 - 2)], sk[1])
            p.fill(p.poly([(cx - 5, y0 + 6), (cx - 4, y0 + 2), (cx, y0), (cx + 4, y0 + 2), (cx + 5, y0 + 6), (cx + 4, y0 + 10),
                           (cx, y0 + 11), (cx - 4, y0 + 10)]), sk, radius=4, depth=4, texture=0.1, seed=3)
            beak = dark('#e8a838', 0.6, 0.6)
            # the head turned a little toward the centre: a hooked beak jutting to the right
            p.fill(p.poly([(cx + 1, y0 + 4), (cx + 4, y0 + 4), (cx + 8, y0 + 6), (cx + 8, y0 + 9), (cx + 6, y0 + 8),
                           (cx + 2, y0 + 8)]), beak, radius=1.5, depth=1.5)
            p.px(cx + 7, y0 + 9, beak[0]), p.px(cx + 8, y0 + 9, beak[0])
            p.stroke([(cx + 2, y0 + 6), (cx + 6, y0 + 7)], beak[0])
            if evil or not blink(t, 0.3, 2):
                eyes(0, [(cx - 2, y0 + 4), (cx + 2, y0 + 3)], (255, 214, 60))
                if not evil:
                    p.px(cx - 3, y0 + 4, (190, 50, 40)), p.px(cx + 3, y0 + 3, (190, 50, 40))
            cracks([(cx + 2, y0 + 1), (cx + 3, y0 + 2), (cx - 4, y0 + 7), (cx - 3, y0 + 8)])
    # ---- far right: the cat
    with p.at(0, breath(t, 0.15)):
        robed(57, 35, 8, 0.15, robe_b, 33)
        with p.at(0, breath(t, 0.05)):
            cx, y0 = 57, 24
            fur = dark('#46445a')
            p.fill(p.poly([(cx - 5, y0 + 7), (cx - 5, y0 + 3), (cx - 5, y0 - 2), (cx - 2, y0 + 1), (cx + 2, y0 + 1),
                           (cx + 5, y0 - 2), (cx + 5, y0 + 3), (cx + 5, y0 + 7), (cx + 3, y0 + 10), (cx, y0 + 11),
                           (cx - 3, y0 + 10)]), fur, radius=4, depth=4, texture=0.12, seed=5)
            p.px(cx - 4, y0, dark('#b06878', 0.5)[2]), p.px(cx + 4, y0, dark('#b06878', 0.5)[2])
            p.fill(p.ellipse(cx, y0 + 8, 2.6, 2.0), dark('#8e8a9e', 0.5), radius=1.5, depth=1, edge=False)
            p.px(cx, y0 + 7, fur[0])
            p.pixels([(cx - 1, y0 + 9), (cx + 1, y0 + 9)], fur[0])
            if evil:
                p.pixels([(cx - 1, y0 + 10), (cx + 1, y0 + 10)], (240, 230, 220))  # bared fangs
            if evil or not blink(t, 0.8, 2):
                eyes(4, [(cx - 2, y0 + 4), (cx + 2, y0 + 4)], (200, 236, 90))
                if not evil:
                    p.px(cx - 2, y0 + 4, (200, 236, 90)), p.px(cx - 3, y0 + 4, (120, 150, 50))
                    p.px(cx + 2, y0 + 4, (200, 236, 90)), p.px(cx + 3, y0 + 4, (120, 150, 50))
            cracks([(cx - 3, y0 + 1), (cx - 2, y0 + 2), (cx + 3, y0 + 6), (cx + 4, y0 + 7)])
    # ---- left: the toad
    with p.at(0, breath(t, 0.3)):
        robed(19, 38, 9, 0.3, robe, 36)
        with p.at(0, breath(t, 0.2)):
            cx, y0 = 19, 28
            sk = dark('#b08a5c', 0.7)
            for ex in (cx - 3, cx + 3):  # bulging eyes on top
                p.fill(p.ellipse(ex, y0 + 1, 2.2, 2.0), sk, radius=1.5, depth=2, bias=0.1)
            p.fill(p.poly([(cx - 6, y0 + 5), (cx - 5, y0 + 2), (cx - 2, y0 + 1), (cx + 2, y0 + 1), (cx + 5, y0 + 2),
                           (cx + 6, y0 + 5), (cx + 5, y0 + 8), (cx, y0 + 10), (cx - 5, y0 + 8)]), sk, radius=4, depth=4,
                   texture=0.12, seed=4)
            for ex in (cx - 3, cx + 3):
                p.fill(p.ellipse(ex, y0 + 1, 2.2, 2.0), sk, radius=1.5, depth=2, bias=0.1)
            p.pixels([(cx - 4, y0 + 4), (cx + 1, y0 + 3), (cx + 4, y0 + 5), (cx - 1, y0 + 5), (cx - 3, y0 + 8),
                      (cx + 3, y0 + 8)], sk[0])  # warts
            p.pixels([(cx - 4, y0 + 3), (cx + 4, y0 + 4)], sk[4])
            p.stroke([(cx - 5, y0 + 6), (cx - 2, y0 + 7), (cx + 2, y0 + 7), (cx + 5, y0 + 6)], sk[0])  # wide mouth
            p.fill(p.poly([(cx - 3, y0 + 8), (cx + 3, y0 + 8), (cx, y0 + 10)]), dark('#d8c08a', 0.5), radius=1, flat=2,
                   edge=False)
            if evil or not blink(t, 0.55, 2):
                if evil:
                    eyes(1, [(cx - 3, y0 + 1), (cx + 3, y0 + 1)], None)
                else:
                    for ex in (cx - 3, cx + 3):
                        p.pixels([(ex - 1, y0 + 1), (ex + 1, y0 + 1)], (226, 176, 60))
                        p.px(ex, y0 + 1, (24, 16, 12))
            cracks([(cx - 1, y0 + 2), (cx, y0 + 3), (cx, y0 + 4), (cx + 2, y0 + 5)])
    # ---- right: the salamander
    with p.at(0, breath(t, 0.8)):
        robed(45, 38, 9, 0.8, robe, 36)
        with p.at(0, breath(t, 0.7)):
            cx, y0 = 45, 28
            sk = dark('#7a8c6c', 0.62)
            p.fill(p.poly([(cx - 7, y0 + 4), (cx - 5, y0 + 1), (cx, y0), (cx + 5, y0 + 1), (cx + 7, y0 + 4), (cx + 5, y0 + 8),
                           (cx, y0 + 10), (cx - 5, y0 + 8)]), sk, radius=4, depth=4, texture=0.08, seed=7)
            lip = dark('#c83c44', 0.55, 0.7)
            p.fill(p.poly([(cx - 5, y0 + 5), (cx, y0 + 6), (cx + 5, y0 + 5), (cx + 4, y0 + 8), (cx, y0 + 9), (cx - 4, y0 + 8)]),
                   lip, radius=1.5, depth=1.5, bias=0.05)
            p.stroke([(cx - 4, y0 + 6), (cx, y0 + 7), (cx + 4, y0 + 6)], lip[0])
            p.pixels([(cx - 2, y0 + 2), (cx + 2, y0 + 1), (cx + 1, y0 + 3), (cx - 4, y0 + 3)], sk[1])
            if evil or not blink(t, 0.1, 2):
                if evil:
                    eyes(3, [(cx - 4, y0 + 3), (cx + 4, y0 + 3)], None)
                else:
                    for ex in (cx - 4, cx + 4):
                        p.px(ex, y0 + 3, (244, 206, 50)), p.px(ex, y0 + 2, sk[0])
            cracks([(cx - 1, y0 + 1), (cx, y0 + 2), (cx, y0 + 3), (cx + 2, y0 + 4)])
    # ---- centre, in front: the fan-crested one
    with p.at(0, breath(t)):
        robed(32, 42, 11, 0.0, robe, 40)
        with p.at(0, breath(t, -0.1)):
            cx = 32
            sk = dark('#7a4658', 0.55)
            p.fill(p.poly([(cx - 2, 35), (cx + 2, 35), (cx + 2, 41), (cx - 2, 41)]), sk, radius=1.5, depth=1.5, bias=-0.2)
            s = sway(t, 0.2, 1)
            fan_c = dark('#d8407a', 0.72, 0.82)
            fan = [(cx - 1, 33), (cx - 5, 30), (cx - 7 - s, 27), (cx - 6 - s, 25), (cx - 4, 24), (cx - 2, 22), (cx, 20),
                   (cx + 2, 22), (cx + 4, 24), (cx + 6 + s, 25), (cx + 7 + s, 27), (cx + 5, 30), (cx + 1, 33)]
            p.fill(p.poly(fan), fan_c, radius=2.5, depth=2.5, texture=0.05)
            vein = mix((170, 40, 20), (255, 170, 70), pulse(t, 0.1, 1)) if evil else fan_c[1]
            for (tx, ty) in ((cx - 6 - s, 26), (cx - 3, 23), (cx, 21), (cx + 3, 23), (cx + 6 + s, 26)):
                p.stroke([(cx, 31), (tx, ty)], vein)
            p.fill(p.ellipse(cx, 33.5, 2.9, 3.6), sk, radius=2.5, depth=2.5, texture=0.04)
            if evil or not blink(t, 0.9, 2):
                eyes(2, [(cx - 1, 33), (cx + 1, 33)], (170, 240, 250))
            p.px(cx, 36, sk[0])


def _tentacle(p, t, base, ctrl, tip, curl, ph, w=3.4):
    s = sway(t, ph, 1)
    path = bezier([base, ctrl, (tip[0] + s, tip[1]), (curl[0] + s, curl[1]), (curl[0] + s * 0.5 - 2, curl[1] + 3)], 40)
    p.fill(p.poly(ribbon(path, w, 0.7)), ramp('#3a2240', spread=0.55), radius=1.6, depth=2, bias=0.05)
    for i in range(3, len(path) - 6, 4):  # suckers
        x, y = path[i]
        p.px(round(x) + 1, round(y), (150, 70, 110))


def firmament(t=0.0):
    """after the owner's picture: the Firmament - a council of five robed beings of different races (a bird, a toad,
    the fan-crested one in front, a salamander, a cat) facing the screen before a great swirling star map ringed with
    gold astrolabe lines, in a golden hall. Neutral. Moves: the map's light flows inward, the core pulses, a marker
    orbits the ring, stars twinkle, each of them breathes and blinks in their own time, the fan crest flexes."""
    p = Portrait()
    _hall_and_starmap(p, t)
    _council(p, t, False)
    p.rim('#a090ff', side=1, amount=0.35)
    p.rim('#ffd090', side=-1, amount=0.3)
    p.outline('#06060e')
    return p


def obsidian(t=0.0):
    """after the owner's picture: the Obsidian - the same council possessed by what lies beyond the void: near-black
    figures with glowing slanted eyes and burning cracks, a black tentacle curling between them, before a black void
    orb wreathed in boiling fire and burning astrolabe lines, between dark rock; smoke rolls along the ground. Evil.
    Moves: the fire boils round the void, a violet pupil swells in it, the eyes and cracks pulse, the tentacle sways,
    smoke drifts, a marker orbits the burning ring."""
    p = Portrait()
    _void_eye(p, t)
    # tentacles rising behind them against the fire, curling in
    _tentacle(p, t, (12, 44), (6, 30), (12, 14), (18, 12), 0.1)
    _tentacle(p, t, (52, 44), (60, 30), (52, 14), (46, 12), 0.6)
    _council(p, t, True)
    p.rim('#ff7a3a', side=1, amount=0.5)
    p.rim('#ff4a3a', side=-1, amount=0.4)
    p.outline('#060308')
    smoke(p, t, 32, 63, 44, 8, strength=0.6, col=(120, 86, 108), speed=1.0, seed=2.0)
    return p


def ralnel(t=0.0):
    """after the owner's picture: a Ral Nel - a gecko-like alien, the head turned to the right: a yellow-green bumpy
    head with a fringe of dark bristles over the crown and under the jaw, a big orange eye with a slit pupil, a long
    pink-lipped mouth (closed: neutral), a little red crown spot; a blue jumpsuit with a teal front panel, a grey
    badge, a brown belt, a metal bracer on the arm; a rusty bulkhead with pipes and green lights behind. Neutral.
    Moves: slow breathing, the throat pumps, the eye blinks, a quick tongue flick, the bristles stir, lights blink."""
    p = Portrait()
    painted_bg(p, (54, 26, 24), (150, 86, 62), 30, 20, 50, steps=6)
    for y in range(64):  # pipes and panel seams on the bulkhead
        for x, c in ((3, (40, 22, 20)), (4, (96, 54, 40)), (5, (40, 22, 20)), (15, (70, 36, 30)), (58, (70, 36, 30))):
            bg_px(p, x, y, c)
    for y in range(6, 14):
        for x in range(8, 14):
            bg_px(p, x, y, (36, 24, 24) if (x in (8, 13) or y in (6, 13)) else (58, 40, 36))
    for i in range(30):  # a cable hanging in a loop
        u = i / 29
        bg_px(p, 18 + int(u * 14), int(2 + 8 * math.sin(u * math.pi)), (30, 18, 18))
    for (x, y, ph) in ((10, 9, 0.0), (12, 9, 0.5), (56, 4, 0.25)):
        bg_px(p, x, y, mix((40, 90, 50), (140, 255, 150), pulse(t, ph, 2)))
    head_c = ramp('#c4bc4c', spread=0.6, hue_shift=0.09)
    lip = ramp('#e2907e', spread=0.55, hue_shift=0.04)
    pale = ramp('#e0d488', spread=0.45)
    suit = ramp('#24427e', spread=0.65)
    teal = ramp('#3a8a86', spread=0.6)
    belt = ramp('#6a4228', spread=0.6)
    steel = ramp('#9aa2ae', spread=0.6)
    brist = (40, 26, 26)
    body, head = breath(t), breath(t, -0.1)
    with p.at(0, body):
        # a lanky body in a blue jumpsuit with a teal front panel, a badge, a belt; the left arm with a metal bracer
        p.fill(p.poly([(12, 66), (14, 48), (20, 41), (28, 39), (38, 40), (45, 44), (48, 52), (48, 66)]), suit, radius=6,
               depth=3.5, texture=0.03)
        p.fill(p.poly([(25, 42), (33, 42), (35, 66), (23, 66)]), teal, radius=3, depth=2.5, bias=0.05)
        p.stroke([(25, 42), (23, 66)], suit[0]), p.stroke([(33, 42), (35, 66)], suit[0])
        p.fill(p.ellipse(19, 48, 2.0, 2.0), steel, radius=1.5, depth=2)
        p.fill(p.poly([(13, 58), (48, 58), (48, 61), (13, 61)]), belt, radius=1.5, depth=1.5)
        p.fill(p.poly([(27, 57), (31, 57), (31, 62), (27, 62)]), steel, radius=1, depth=1.5)
        p.px(29, 59, steel[0])
        p.fill(p.poly([(5, 66), (7, 50), (12, 44), (16, 50), (13, 66)]), suit, radius=3, depth=2.5, bias=-0.05)
        p.fill(p.poly([(5, 53), (14, 53), (14, 61), (5, 61)]), steel, radius=2, depth=2.5)
        p.stroke([(6, 57), (13, 57)], steel[1])
        p.px(9, 59, mix((150, 20, 20), (255, 80, 60), pulse(t, 0.6, 2)))
    with p.at(0, head):
        # the long neck rising from the collar
        p.fill(p.poly([(22, 26), (33, 29), (35, 43), (23, 43), (20, 34)]), head_c, radius=3, depth=2.5, bias=-0.1, texture=0.08)
        p.fill(p.poly([(30, 30), (34, 30), (35, 43), (31, 43)]), pale, radius=2, depth=1.5, edge=False)
    with p.at(0, body):
        p.fill(p.poly([(18, 41), (28, 39), (38, 40), (40, 44), (17, 44)]), suit, radius=2, depth=2, bias=0.1)  # the collar
    with p.at(0, head):
        # the throat: pumps in and out under the jaw
        k = pulse(t, 0.3, 2)
        p.fill(p.ellipse(38, 31, 8, 2.2 + 1.3 * k), pale, radius=2, depth=1.5, bias=0.05)
        # dark bristles: a mane over the crown and down the back of the head (they stir), a fringe under the jaw
        mane = [(21, 27), (19, 21), (21, 15), (25, 10), (31, 6), (38, 5), (45, 7), (50, 10)]
        n_sp = 0
        for i in range(len(mane) - 1):
            for j in range(2):
                u = j / 2
                bx = mane[i][0] + (mane[i + 1][0] - mane[i][0]) * u
                by = mane[i][1] + (mane[i + 1][1] - mane[i][1]) * u
                ang = math.atan2(by - 18, bx - 34)
                s = sway(t, n_sp * 0.09, 1) * 0.6
                ex, ey = bx + math.cos(ang - 0.5) * 4 + s, by + math.sin(ang - 0.5) * 4
                p.stroke([(bx, by), (ex, ey)], brist)
                p.px(round(ex), round(ey), (120, 96, 80))
                n_sp += 1
        for x in range(32, 54, 3):
            yb = 32 if x < 46 else 30
            p.stroke([(x, yb), (x - 1 + sway(t, x * 0.04, 1), yb + 3)], brist)
        # the cranium and the long snout (to the right)
        p.fill(p.poly([(20, 20), (22, 13), (28, 8), (36, 6), (44, 7), (50, 10), (55, 13), (59, 17), (61, 21), (59, 24),
                       (48, 26), (38, 27), (28, 29), (22, 26)]), head_c, radius=6, depth=4, texture=0.1, seed=11, bias=0.06)
        p.stroke([(32, 25), (44, 24), (55, 21)], head_c[2])  # a soft shade above the lip
        # the raised brow over the eye
        p.fill(p.ellipse(45, 15, 5, 4.4), head_c, radius=3, depth=3, bias=0.12, edge=False, texture=0.06, seed=12)
        for (x, y) in ((26, 13), (30, 10), (34, 9), (27, 18), (23, 21), (38, 10), (31, 15), (53, 15), (56, 18), (35, 21)):
            p.px(x, y, head_c[4])  # bumps catching the light
            p.px(x + 1, y + 1, head_c[1])
        # the pink lower jaw, mouth closed (neutral)
        p.fill(p.poly([(30, 28), (40, 27), (50, 26), (60, 23), (59, 26), (52, 29), (42, 31), (32, 31)]), lip, radius=1.6,
               depth=1.5, bias=0.1)
        p.stroke([(30, 28), (40, 27), (50, 26), (60, 23)], (96, 34, 36))
        p.stroke([(42, 26), (50, 25), (58, 22)], lip[3])  # the pink upper lip
        if 0.40 <= t % 1.0 < 0.48:  # a quick tongue flick
            p.stroke([(59, 24), (62, 26)], (220, 70, 90))
            p.px(63, 26, (220, 70, 90)), p.px(62, 27, (220, 70, 90))
        p.px(58, 19, head_c[0]), p.px(57, 19, head_c[1])  # the nostril
        p.pixels([(36, 11), (37, 11), (36, 12)], (220, 60, 60))  # the red crown spot
        # the big orange eye with a slit pupil
        p.stroke([(40, 12), (44, 10), (49, 11)], head_c[0])
        if blink(t, 0.62, 2):
            p.fill(p.ellipse(45, 16, 3.4, 3.0), head_c, radius=2, depth=2, bias=0.05)
            p.stroke([(42, 16), (48, 16)], head_c[0])
        else:
            p.fill(p.ellipse(45, 16, 3.4, 3.0), ramp('#f08a1e', spread=0.6, hue_shift=0.05), radius=2.5, depth=2.5,
                   bias=0.1)
            p.stroke([(45, 14), (45, 18)], (24, 12, 10))
            p.px(43, 14, (255, 250, 220))
    p.rim('#7ae0ff', side=1, amount=0.35)
    p.rim('#ffb070', side=-1, amount=0.25)
    p.outline('#140a0a')
    return p
