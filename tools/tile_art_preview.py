#!/usr/bin/env python3
"""Preview clips for the pixel-art tiles made by gen_tile_art.py.

usage: tile_art_preview.py ARTDIR OUTDIR
  OUTDIR/tiles-map.gif     the tiles placed together around Mecatol (2x, animated)
  OUTDIR/tiles-sheet.png   every tile and token with its name (2x, first frame)
  OUTDIR/tiles-preview.mp4 the map, then the labelled sheet, two loops each
"""
import json
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ART, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
meta = json.load(open(os.path.join(ART, "pieces.json")))
by = {m["name"]: m for m in meta}
strips = {m["name"]: Image.open(os.path.join(ART, m["name"] + ".png")).convert("RGBA") for m in meta}
F = meta[0]["frames"]
BG = (10, 13, 25, 255)
FG = (223, 230, 247)
MUTED = (141, 154, 187)
RED = (236, 58, 52)
AMBER = (244, 176, 74)
ANOM = {"41", "42", "43", "44", "45", "67", "68", "79", "80", "81", "56"}


def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    for d in ("/usr/share/fonts/truetype/dejavu",):
        p = os.path.join(d, name)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def frame_of(name, f):
    m = by[name]
    return strips[name].crop((f * m["w"], 0, f * m["w"] + m["w"], m["h"]))


# ---- map: Mecatol in the middle, two rings around it (flat-top hexes)
MAP = ["18_mecatol_rex",
       "42_nebula", "39_alpha_wormhole", "43_supernova", "41_gravity_rift", "40_beta_wormhole", "44_asteroid_field",
       "25_quann", "67_cormund", "82b_mallice_active", "26_lodor", "80_supernova", "68_everra",
       "64_atlas", "79_asteroids_alpha", "81_muaat_supernova", "45_asteroid_field", "56_the_dark", "17_creuss_gate"]
DIRS = [(1, 0), (1, -1), (0, -1), (-1, 0), (-1, 1), (0, 1)]
cells = [(0, 0)]
for k in (1, 2):
    q, r = DIRS[4][0] * k, DIRS[4][1] * k
    for d in range(6):
        for _ in range(k):
            cells.append((q, r))
            q += DIRS[d][0]
            r += DIRS[d][1]


def map_frame(f):
    im = Image.new("RGBA", (384, 420), (0, 0, 0, 0))
    for (q, r), name in zip(cells, MAP):
        x = round(192 + 72 * q - 48)
        y = round(210 + 83.14 * (r + q / 2) - 42)
        im.alpha_composite(frame_of(name, f), (x, y))
    return im


# ---- labelled sheet
tiles = [m for m in meta if m["kind"] == "tile"]
toks = [m for m in meta if m["kind"] == "token"]
COLS, CW, CH, LAB = 5, 210, 168, 40
SW = COLS * CW + 40
TROW = 80 + 52
SH = 70 + ((len(tiles) + COLS - 1) // COLS) * (CH + LAB) + 40 + TROW + 20
f_title, f_lab, f_no = font(22, True), font(12), font(12, True)


def wrap(draw, text, fnt, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if draw.textlength(t, font=fnt) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    lines.append(cur)
    return lines[:2]


def sheet_frame(f):
    im = Image.new("RGBA", (SW, SH), BG)
    d = ImageDraw.Draw(im)
    d.text((20, 18), "Pixel anomaly tiles - batch 1 (not in the game yet)", font=f_title, fill=FG)
    d.text((20, 46), "96x84 px tiles shown 2x  ·  red number = anomaly  ·  6.4 s seamless loops", font=f_lab, fill=MUTED)
    y0 = 70
    for i, m in enumerate(tiles):
        cx = 20 + (i % COLS) * CW
        cy = y0 + (i // COLS) * (CH + LAB)
        fr = frame_of(m["name"], f).resize((m["w"] * 2, m["h"] * 2), Image.NEAREST)
        im.alpha_composite(fr, (cx + (CW - 10 - fr.width) // 2, cy))
        no, _, title = m["label"].partition(" · ")
        col = RED if no in ANOM else MUTED
        tw = d.textlength(no, font=f_no)
        d.rounded_rectangle((cx + 4, cy + CH + 4, cx + 12 + tw, cy + CH + 20), 3, fill=col)
        d.text((cx + 8, cy + CH + 5), no, font=f_no, fill=(255, 255, 255) if col == RED else BG)
        for k, line in enumerate(wrap(d, title, f_lab, CW - 30 - tw)):
            d.text((cx + 18 + tw, cy + CH + 5 + k * 15), line, font=f_lab, fill=FG)
    ty = y0 + ((len(tiles) + COLS - 1) // COLS) * (CH + LAB) + 10
    d.text((20, ty), "Tokens", font=f_no, fill=AMBER)
    for i, m in enumerate(toks):
        cx = 20 + i * ((SW - 40) // len(toks))
        fr = frame_of(m["name"], f).resize((80, 80), Image.NEAREST)
        im.alpha_composite(fr, (cx + 40, ty + 22))
        lab = m["label"].replace(" token", "").replace(" — ", " ")
        for k, line in enumerate(wrap(d, lab, f_lab, (SW - 40) // len(toks) - 8)):
            d.text((cx, ty + 106 + k * 14), line, font=f_lab, fill=FG)
    return im


# map GIF (2x)
frames = [map_frame(f).resize((768, 840), Image.NEAREST) for f in range(F)]
pal_frames = []
for fr in frames:
    bg = Image.new("RGBA", fr.size, BG)
    bg.alpha_composite(fr)
    pal_frames.append(bg.convert("RGB").quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
pal_frames[0].save(os.path.join(OUT, "tiles-map.gif"), save_all=True, append_images=pal_frames[1:], duration=100, loop=0)

# labelled still
sheet_frame(0).convert("RGB").save(os.path.join(OUT, "tiles-sheet.png"))

# MP4: map (2 loops) then sheet (2 loops), on one canvas
VW, VH = 1080, max(SH, 900) + (max(SH, 900) % 2)
tmp = os.path.join(OUT, "_frames")
os.makedirs(tmp, exist_ok=True)
n = 0
f_cap = font(26, True)
for part in ("map", "sheet"):
    for loop in range(2):
        for f in range(F):
            can = Image.new("RGBA", (VW, VH), BG)
            if part == "map":
                d = ImageDraw.Draw(can)
                d.text((40, 30), "On the map", font=f_cap, fill=FG)
                d.text((40, 64), "Mecatol Rex with the new anomaly and wormhole tiles around it", font=font(16), fill=MUTED)
                can.alpha_composite(frames[f], ((VW - 768) // 2, 110))
            else:
                s = sheet_frame(f)
                can.alpha_composite(s, ((VW - s.width) // 2, 0))
            can.convert("RGB").save(os.path.join(tmp, f"{n:05d}.png"))
            n += 1
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "10", "-i", os.path.join(tmp, "%05d.png"),
                "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "16", "-preset", "slow",
                "-vf", "scale=iw:ih:flags=neighbor", os.path.join(OUT, "tiles-preview.mp4")], check=True)
for fn in os.listdir(tmp):
    os.remove(os.path.join(tmp, fn))
os.rmdir(tmp)
print("ok", n, "video frames")
