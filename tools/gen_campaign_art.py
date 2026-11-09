#!/usr/bin/env python3
"""Campaign backdrops (0.30.0, owner's Big Push list).

  * Eve of the Fall - the owner's picture of the Lazax emperor (0.31.0). My own
    earlier painting of a regal Lazax is kept as a spare (eve_of_the_fall_pixel):, in the Retrowave look:
    the last emperor before a synthwave sun that doubles as his halo, Mecatol
    Rex's towers on the horizon, a neon grid floor; four-armed, crowned with a
    sun-disc circlet (after the owner's reference picture). Drawn in pixels at
    320 x 150 with ordered dithering, then enlarged without smoothing.
  * Fated Expedition and Trade Wars - the owner's pictures, cut above the white
    circuit band they carry and fitted to the same banner shape.

Every backdrop is a 32:15 banner: 1280 x 600 for the PC versions, 640 x 300 for
the Miyoo.

usage: gen_campaign_art.py OWNER_DIR OUT_DIR [PREVIEW.png]
"""
import math
import os
import sys

import numpy as np
from PIL import Image

W, H = 320, 150
BAYER = np.array([[0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26], [12, 44, 4, 36, 14, 46, 6, 38],
                  [60, 28, 52, 20, 62, 30, 54, 22], [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
                  [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]], float) / 64.0
YY, XX = np.mgrid[0:H, 0:W].astype(float)
TH = np.tile(BAYER, (H // 8 + 1, W // 8 + 1))[:H, :W]


def hexc(s):
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], float)


def ramp(cols):
    return np.stack([hexc(c) for c in cols])


def shade(img, mask, rmp, v):
    """paint `mask` from ramp `rmp` at levels v (0..1), ordered-dithered between steps"""
    v = np.broadcast_to(np.clip(v, 0, 1), (H, W))
    n = len(rmp)
    x = v * (n - 1) + (TH - 0.5)
    k = np.clip(np.rint(x), 0, n - 1).astype(int)
    img[mask] = rmp[k][mask]


def ellipse(cx, cy, rx, ry):
    return ((XX + 0.5 - cx) / rx) ** 2 + ((YY + 0.5 - cy) / ry) ** 2 <= 1.0


def poly(pts):
    """inside a convex or simple polygon (even-odd)"""
    x, y = XX + 0.5, YY + 0.5
    inside = np.zeros((H, W), bool)
    n = len(pts)
    for i in range(n):
        x0, y0 = pts[i]
        x1, y1 = pts[(i + 1) % n]
        cond = ((y0 > y) != (y1 > y))
        with np.errstate(divide="ignore", invalid="ignore"):
            xi = (x1 - x0) * (y - y0) / (y1 - y0 + 1e-9) + x0
        inside ^= cond & (x < xi)
    return inside


def thick_line(x0, y0, x1, y1, w):
    px, py = XX + 0.5, YY + 0.5
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy + 1e-9
    t = np.clip(((px - x0) * dx + (py - y0) * dy) / L2, 0, 1)
    d = np.hypot(px - (x0 + t * dx), py - (y0 + t * dy))
    return d <= w


def outline(mask):
    m = mask
    edge = m & ~(np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1))
    return edge


SKY = ramp(["0c0420", "1a0838", "2e0c52", "4a1068", "6e1478", "961c7c", "c02a78", "e6466c", "f8705c", "ffa058", "ffd27a"])
SUN = ramp(["ff2e88", "ff4a7c", "ff6c6c", "ff925a", "ffb64e", "ffd65a", "fff08c"])
CITY = ramp(["0a0418", "140828", "1e0c38", "2a1048"])
FLOOR = ramp(["06020e", "0e0420", "160630", "1e0840"])
GRID = ramp(["5a0c6a", "9c1488", "e0289c", "ff60c0"])
SKIN = ramp(["1e3a4c", "2c5468", "3e7286", "5492a4", "70b0c0", "92cad6", "b8e2ea", "e2f6fa"])
ROBE = ramp(["040c18", "081a2c", "0e2a42", "163c58", "205270", "2e6a8a", "4486a6"])
GOLD = ramp(["3a2006", "6a400c", "a06814", "d09424", "f0bc40", "ffde7a", "fff4c4"])
WHITE = ramp(["4a4e66", "7a7e98", "a8acc4", "d4d6e6", "f6f6fc"])
DARK = hexc("06030c")


def eve_of_the_fall():
    img = np.zeros((H, W, 3))
    horizon = 102.0
    # the sky: night above, the dying light of an empire at the horizon
    shade(img, YY < horizon, SKY, np.clip((YY / horizon) ** 1.6, 0, 1))
    # stars high up
    rng = np.random.default_rng(7)
    for _ in range(90):
        x, y = rng.integers(0, W), rng.integers(0, 50)
        img[y, x] = np.array([255, 220, 250]) if rng.random() < 0.3 else np.array([200, 180, 255])
    # the sun, banded below its middle - the emperor's halo
    sx, sy, sr = 160.0, 60.0, 50.0
    sun = ellipse(sx, sy, sr, sr) & (YY < horizon)
    band = (YY > sy - 4) & (np.mod(YY - sy, 7.0 + (YY - sy) * 0.06) < (1.0 + (YY - sy) * 0.09))
    shade(img, sun & ~band, SUN, 1.0 - np.clip((YY - (sy - sr)) / (2 * sr), 0, 1))
    glow = (np.hypot(XX - sx, YY - sy) < sr + 7) & ~sun & (YY < horizon)
    img[glow] = img[glow] * 0.6 + np.array([255, 90, 160]) * 0.4
    # Mecatol Rex on the horizon: towers and spires, windows lit
    city = np.zeros((H, W), bool)
    rng = np.random.default_rng(11)
    x = 0
    while x < W:
        w = int(rng.integers(5, 14))
        h = float(rng.integers(6, 22))
        if abs(x - 160) < 70:
            h *= 0.65
        city |= (XX >= x) & (XX < x + w) & (YY >= horizon - h) & (YY < horizon + 2)
        if rng.random() < 0.45:  # a spire
            sxp = x + w // 2
            city |= thick_line(sxp, horizon - h - float(rng.integers(5, 14)), sxp, horizon - h, 0.6)
        x += w + int(rng.integers(0, 3))
    shade(img, city, CITY, 0.3 + 0.4 * ((YY - horizon + 25) / 25.0))
    win = city & (rng.random((H, W)) < 0.10) & (YY < horizon - 1)
    img[win] = np.where(rng.random((H, W, 1)) < 0.5, [120, 230, 255], [255, 120, 210])[win]
    # the grid floor
    fl = YY >= horizon + 2
    shade(img, fl, FLOOR, np.clip((YY - horizon) / 48.0, 0, 1))
    vx, vy = 160.0, horizon - 8
    gl = np.zeros((H, W), bool)
    for k in range(-14, 15):  # rays from the vanishing point
        bx = 160 + k * 26.0
        gl |= thick_line(vx, vy, bx, H + 20, 0.45)
    zz = 0
    for k in range(1, 9):  # rows, closer together near the horizon
        y = horizon + 2 + 46.0 * (k / 8.0) ** 2.2
        gl |= np.abs(YY + 0.5 - y) < 0.55
        zz += 1
    gl &= fl
    shade(img, gl, GRID, np.clip((YY - horizon) / 48.0, 0.15, 1))

    # ---- the Lazax
    cx = 160.0
    rim_m = np.array([255, 70, 190])   # magenta rim light (right)
    rim_c = np.array([90, 230, 255])   # cyan rim light (left)
    # a royal cape behind him, sweeping out to the floor, gold-edged
    cape = poly([(118, 92), (202, 92), (240, 150), (80, 150)])
    CAPE = ramp(["10041c", "1e0830", "300c48", "461462", "5e1c7c"])
    shade(img, cape, CAPE, 0.3 + 0.5 * np.clip(1 - np.abs(XX - cx) / 80.0, 0, 1) - 0.15 * ((YY - 92) / 58.0))
    shade(img, outline(cape) & (YY > 93), GOLD, 0.55)
    # robe: shoulders and a long fall to the floor
    robe = poly([(122, 104), (136, 94), (184, 94), (198, 104), (214, 150), (106, 150)])
    shade(img, robe, ROBE, 0.35 + 0.35 * np.clip((XX - 106) / 108.0, 0, 1) * 0 + 0.25 * (1 - np.abs(XX - cx) / 55.0))
    # gold trim down the front and along the hem of the shoulders
    trim = robe & ((np.abs(XX - cx) < 1.2) | (np.abs((YY - 94) - 0.55 * np.abs(XX - cx) - 2) < 0.9))
    shade(img, trim, GOLD, 0.55 + 0.3 * (1 - np.abs(XX - cx) / 60.0))
    # embroidery: gold chevrons down the robe
    for k, y0 in enumerate((112.0, 124.0, 136.0)):
        chev = robe & (np.abs((YY - y0) - 0.42 * np.abs(XX - cx)) < 0.7) & (np.abs(XX - cx) > 3) & (np.abs(XX - cx) < 26 + k * 6)
        shade(img, chev, GOLD, 0.45)
    # pauldrons: layered gold plates on the shoulders
    for s_ in (-1, 1):
        for k in range(3):
            pl = ellipse(cx + s_ * (25 + k * 2.5), 97 + k * 3.2, 12 - k * 1.5, 5.2 - k * 0.8)
            shade(img, pl, GOLD, 0.75 - 0.15 * k + 0.2 * np.clip((cx - XX) / 40.0, -1, 1))
            shade(img, outline(pl), GOLD, 0.25)
    # the high collar fanning up behind the neck, white and gold
    collar = poly([(140, 96), (132, 70), (146, 80), (160, 74), (174, 80), (188, 70), (180, 96)]) & ~ellipse(cx, 98, 13, 10)
    shade(img, collar, WHITE, 0.55 + 0.4 * np.clip((96 - YY) / 26.0, 0, 1))
    shade(img, collar & outline(collar), GOLD, 0.7)
    # lower arms: sleeves folding in, hands holding the sceptre before the chest
    for s in (-1, 1):
        sl = thick_line(cx + s * 22, 100, cx + s * 7, 116, 4.2)
        shade(img, sl, ROBE, 0.5)
        cuff = thick_line(cx + s * 10, 112, cx + s * 7, 116, 3.0) & ~thick_line(cx + s * 22, 100, cx + s * 11, 110, 4.4)
        shade(img, cuff, GOLD, 0.6)
    # the sceptre: a gold staff with a glowing orb
    staff = thick_line(cx, 104, cx, 150, 1.0)
    shade(img, staff, GOLD, 0.65 + 0.25 * np.clip((XX - cx + 1) / 2.0, 0, 1))
    orb = ellipse(cx, 100, 4.5, 4.5)
    shade(img, orb, ramp(["14405a", "2a86a8", "5ed0e8", "c8f6ff", "ffffff"]),
          1.0 - np.clip(np.hypot(XX - cx + 1.5, YY - 98.5) / 5.5, 0, 1))
    hands_low = ellipse(cx - 5, 117, 4, 3) | ellipse(cx + 5, 117, 4, 3)
    shade(img, hands_low, SKIN, 0.55 + 0.3 * np.clip((cx - XX) / 8.0, -1, 1))
    # upper arms: raised, palms open, long fingers - the gesture of the reference
    for s in (-1, 1):
        sh = (cx + s * 26, 98)
        el = (cx + s * 44, 86)
        wr = (cx + s * 50, 70)
        sleeve = thick_line(sh[0], sh[1], el[0], el[1], 6.5) | thick_line(el[0], el[1], wr[0], wr[1] + 3, 5.2)
        # the sleeve hangs open below the forearm, as a robe would
        drape = poly([(el[0] - s * 1, el[1] + 2), (wr[0] + s * 4, wr[1] + 4), (wr[0] + s * 9, wr[1] + 22), (el[0] + s * 4, el[1] + 14)])
        shade(img, sleeve | drape, ROBE, 0.45 + 0.2 * s)
        shade(img, outline(drape) & ~sleeve, GOLD, 0.5)
        cuff = thick_line(wr[0], wr[1] + 6, wr[0], wr[1] + 2, 4.2)
        shade(img, cuff, GOLD, 0.7)
        palm = ellipse(wr[0], wr[1] - 1, 3.6, 4.4)
        fingers = np.zeros((H, W), bool)
        for f, ang in enumerate((-0.55, -0.2, 0.15, 0.5)):
            a = -math.pi / 2 + s * ang
            fx, fy = wr[0] + math.cos(a) * 2.5, wr[1] - 3 + math.sin(a) * 1.5
            fingers |= thick_line(fx, fy, fx + math.cos(a) * (8.5 - abs(ang) * 4), fy + math.sin(a) * (8.5 - abs(ang) * 4), 0.75)
        thumb = thick_line(wr[0], wr[1], wr[0] - s * 6, wr[1] - 3, 0.8)
        hand = palm | fingers | thumb
        shade(img, hand, SKIN, 0.55 + 0.3 * (-s * (XX - wr[0]) / 8.0))
        # a little light of power over each palm
        spark = ellipse(wr[0], wr[1] - 14, 2.2, 2.2)
        shade(img, spark, ramp(["ff60c0", "ffb0e8", "ffffff"]), 1.0 - np.hypot(XX - wr[0], YY - wr[1] + 14) / 2.5)
    # neck
    neck = poly([(153, 96), (155, 74), (165, 74), (167, 96)])
    shade(img, neck, SKIN, 0.35 + 0.35 * np.clip((cx - XX) / 8.0, -1, 1))
    # the head: long and high-domed, a narrow chin
    head = ellipse(cx, 52, 14.5, 22) | poly([(147, 56), (173, 56), (166, 78), (154, 78)])
    light = np.clip(0.5 + (cx - XX) / 30.0 + (52 - YY) / 80.0, 0, 1)
    shade(img, head, SKIN, 0.25 + 0.65 * light)
    # cheek hollows
    for s in (-1, 1):
        hol = ellipse(cx + s * 8, 66, 2.5, 5.5) & head
        img[hol] = img[hol] * 0.78
    # eyes: dark almonds with a gold glow at the heart
    for s in (-1, 1):
        ex = cx + s * 6.5
        eye = ellipse(ex, 58.5, 4.2, 1.9) & ~ellipse(ex - s * 1.0, 60.6, 4.6, 1.6)
        img[eye] = DARK
        img[ellipse(ex + s * 0.6, 58.6, 1.2, 1.0)] = np.array([255, 226, 140])
        brow = thick_line(ex - 4, 55.5 - s * 0.3, ex + 4, 55.0 + s * 0.3, 0.5) & head
        img[brow] = img[brow] * 0.7
    # a thin, stern mouth
    mouth = thick_line(cx - 3.5, 71.5, cx + 3.5, 71.5, 0.45)
    img[mouth] = np.array([24, 40, 56])
    # the crown: a gold circlet across the brow with a sun disc, and a high golden ring above the dome
    circ = (np.abs(((XX + 0.5 - cx) / 15.5) ** 2 + ((YY + 0.5 - 46) / 5.0) ** 2 - 1.0) < 0.22) & (YY > 44.5) & head
    shade(img, circ, GOLD, 0.6 + 0.3 * light)
    disc = ellipse(cx, 47.5, 3.6, 3.6)
    shade(img, disc, GOLD, 0.9 - np.hypot(XX - cx, YY - 47.5) / 6.0)
    img[ellipse(cx, 47.5, 1.4, 1.4)] = np.array([60, 210, 240])
    halo = np.abs(np.hypot((XX + 0.5 - cx) / 1.0, (YY + 0.5 - 36) / 0.32) - 22.0) < 1.1
    halo &= ~head | (YY < 36)
    shade(img, halo, GOLD, 0.7 + 0.25 * np.clip((cx - XX) / 22.0, -1, 1))
    # rim lights: cyan from the left, magenta from the right, on the figure's edges
    fig = robe | collar | head | neck | cape
    edge = outline(fig)
    left = edge & (XX < cx)
    right = edge & (XX >= cx)
    img[left] = img[left] * 0.45 + rim_c * 0.55
    img[right] = img[right] * 0.45 + rim_m * 0.55
    # a vignette so the menu text below reads
    vig = np.clip((YY - 118) / 32.0, 0, 1)[..., None]
    img = img * (1 - 0.55 * vig)
    return np.clip(img, 0, 255).astype(np.uint8)


def banner_from(path, band_y, focus=0.5, top=0):
    im = Image.open(path).convert("RGB")
    im = im.crop((0, top, im.width, band_y))
    aspect = W / H
    w, h = im.size
    if w / h > aspect:  # too wide: cut the sides around the focus
        nw = int(round(h * aspect))
        x0 = int(round((w - nw) * focus))
        im = im.crop((x0, 0, x0 + nw, h))
    else:
        nh = int(round(w / aspect))
        y0 = int(round((h - nh) * focus))
        im = im.crop((0, y0, w, y0 + nh))
    return im


def save_pair(img, out_dir, name, pixel_art):
    os.makedirs(out_dir, exist_ok=True)
    big = img.resize((1280, 600), Image.NEAREST if pixel_art else Image.LANCZOS)
    small = img.resize((640, 300), Image.NEAREST if pixel_art else Image.LANCZOS)
    big.save(os.path.join(out_dir, name + "-pc.jpg"), quality=90, optimize=True)
    small.save(os.path.join(out_dir, name + "-miyoo.jpg"), quality=86, optimize=True)
    return small


def main():
    owner, out = sys.argv[1], sys.argv[2]
    prev = sys.argv[3] if len(sys.argv) > 3 else None
    shots = []
    # 0.31.0 (owner): Eve of the Fall now uses his picture of the Lazax emperor; my pixel painting stays
    # here (eve_of_the_fall()) as a spare, written to eve_of_the_fall_pixel-*.jpg but not used by the game
    save_pair(Image.fromarray(eve_of_the_fall(), "RGB"), out, "eve_of_the_fall_pixel", True)
    shots.append(save_pair(banner_from(os.path.join(owner, "eve_of_the_fall_reference.jpg"), 1024, focus=0.2), out,
                           "eve_of_the_fall", False))
    shots.append(save_pair(banner_from(os.path.join(owner, "fated_expedition_reference.jpg"), 470, focus=0.6), out,
                           "fated_expedition", False))
    # 0.52.2 (owner): Trade Wars is his new picture - the ground war seen from a cave mouth (the old one stays in docs/owner);
    # cut to the banner shape around the middle of the battle
    shots.append(save_pair(banner_from(os.path.join(owner, "trade_wars_reference2.jpg"), 368, focus=0.5), out, "trade_wars",
                           False))
    # 0.32.0 (owner): Twilight's Fall - his picture of the masked emperor in shattered crystal; cut so the crown
    # and the mask sit in the banner, the shoulders falling into the dark foot where the title is written
    shots.append(save_pair(banner_from(os.path.join(owner, "twilights_fall_reference.jpg"), 1024, focus=0.17), out,
                           "twilights_fall", False))
    if prev:
        sheet = Image.new("RGB", (640, 300 * len(shots)))
        for i, s in enumerate(shots):
            sheet.paste(s, (0, i * 300))
        sheet.save(prev)
    print("wrote", out)


if __name__ == "__main__":
    main()
