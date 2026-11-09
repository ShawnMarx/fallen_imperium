#!/usr/bin/env python3
"""Pack the owner-supplied icons (assets/icons/**.png) into assets/icons.dat,
the file the game loads at start-up.

Every icon is cropped to its visible area and scaled (Lanczos) so its longer
side is MASTER pixels; the game scales these masters down to whatever size a
screen needs. Pixels are stored premultiplied (A, R, G, B) and run-length
packed, so the loader needs no image library.

Icons that are plain black shapes (they were drawn for light card faces) are
flagged "mono": the game paints them in any colour, using only their shape.
The unit pictures (assets/icons/units, cut from the owner's unit reference
card by gen_unit_art.py) are flagged "shaded": grey pictures the game paints
in a player's colour, keeping their light and dark parts.

  usage: gen_icons.py [assets/icons] [assets/icons.dat]
"""
import os
import struct
import sys

from PIL import Image, ImageChops

MASTER = 64
MASTER_UNITS = 128  # unit pictures (gen_unit_art.py) are drawn larger, in the tile view
MONO = {"action_card", "agenda", "codex", "pok", "promissory", "legendary_black"}


def pack_rle(pixels):
    """PackBits on 32-bit pixels: header h < 128 -> h+1 copies of one pixel;
    h >= 128 -> h-127 literal pixels follow."""
    out = bytearray()
    i, n = 0, len(pixels)
    while i < n:
        j = i
        while j + 1 < n and pixels[j + 1] == pixels[i] and j - i < 127:
            j += 1
        if j > i:
            out.append(j - i)
            out += pixels[i]
            i = j + 1
            continue
        lit = [pixels[i]]
        i += 1
        while i < n and len(lit) < 128 and not (i + 1 < n and pixels[i + 1] == pixels[i]):
            lit.append(pixels[i])
            i += 1
        out.append(127 + len(lit))
        for p in lit:
            out += p
    return bytes(out)


def load(path, mono, master=MASTER):
    im = Image.open(path).convert("RGBA")
    box = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    if box:
        im = im.crop(box)
    k = master / max(im.size)
    w, h = max(1, round(im.width * k)), max(1, round(im.height * k))
    # Premultiply before resampling so transparent edges don't bleed dark fringes.
    r, g, b, a = im.split()
    r, g, b = (ImageChops.multiply(ch, a) for ch in (r, g, b))
    pm = Image.merge("RGBA", (r, g, b, a)).resize((w, h), Image.LANCZOS)
    px = []
    raw = pm.tobytes()
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        if mono:
            r = g = b = a  # shape only: white, premultiplied
        r, g, b = min(r, a), min(g, a), min(b, a)
        px.append(struct.pack("<I", (a << 24) | (r << 16) | (g << 8) | b))
    return w, h, px


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "assets/icons"
    dst = sys.argv[2] if len(sys.argv) > 2 else "assets/icons.dat"
    entries = []
    for group in sorted(os.listdir(src)):
        gdir = os.path.join(src, group)
        if not os.path.isdir(gdir):
            continue
        for f in sorted(os.listdir(gdir)):
            if not f.lower().endswith(".png"):
                continue
            key = f[:-4]
            if group == "units":  # grey pictures painted in a player's colour (flag 2)
                w, h, px = load(os.path.join(gdir, f), False, MASTER_UNITS)
                entries.append(("unit/" + key, w, h, 2, pack_rle(px)))
                continue
            name = ("faction/" if group.startswith("factions") else "") + key  # factions_ds / factions_te too (0.28.2)
            mono = not group.startswith("factions") and key in MONO
            w, h, px = load(os.path.join(gdir, f), mono)
            entries.append((name, w, h, 1 if mono else 0, pack_rle(px)))
    out = bytearray(b"TI4I")
    out += struct.pack("<HH", 1, len(entries))
    for name, w, h, flags, data in entries:
        nb = name.encode()
        out += struct.pack("<B", len(nb)) + nb
        out += struct.pack("<HHBI", w, h, flags, len(data)) + data
    with open(dst, "wb") as fh:
        fh.write(out)
    print(f"{dst}: {len(entries)} icons, {len(out)} bytes")


if __name__ == "__main__":
    main()
