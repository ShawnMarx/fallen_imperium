#!/usr/bin/env python3
"""Cut the unit pictures out of the owner's "Neutral Unit Reference" card
(docs/owner/neutral_unit_reference.webp) into grey-scale shapes with alpha,
assets/icons/units/<unit>.png. gen_icons.py packs them as "unit/<unit>"
icons that the game paints in each player's colour (light parts of the
picture stay light, dark parts dark).

Ships are turned to face right (on the card they point up); the war sun
(standing, dome up), ground units, PDS and space dock keep their pose.

  usage: gen_unit_art.py [card] [out dir]
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

# rough boxes (x0, y0, x1, y1) around each picture on the 826 x 1417 card
BOXES = {
    "flagship": (18, 100, 104, 252),
    "warsun": (18, 262, 112, 372),
    "dreadnought": (18, 405, 104, 566),
    "carrier": (28, 566, 94, 694),
    "cruiser": (32, 708, 90, 856),
    "destroyer": (22, 856, 106, 980),
    "fighter": (26, 1016, 100, 1096),
    "mech": (424, 1056, 530, 1150),
    "infantry": (424, 1190, 522, 1302),
    "pds": (22, 1212, 104, 1296),
    "spacedock": (424, 1302, 518, 1400),
}
SHIPS = {"flagship", "warsun", "dreadnought", "carrier", "cruiser", "destroyer", "fighter"}


def cut(img, box):
    a = np.asarray(img.crop(box).convert("L"), dtype=np.float32)
    solid = a > 78
    lab, n = ndimage.label(solid)
    if n == 0:
        raise SystemExit("nothing found in %r" % (box,))
    sizes = ndimage.sum(solid, lab, range(1, n + 1))
    main = int(np.argmax(sizes)) + 1
    ys, xs = np.where(lab == main)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    keep = lab == main
    # small pieces touching the main shape belong to it (engines, antennas); specks of starfield don't
    near = ndimage.binary_dilation(keep, iterations=3)
    for k in range(1, n + 1):
        if k == main or sizes[k - 1] < 12:
            continue
        if (near & (lab == k)).any():
            keep |= lab == k
    closed = ndimage.binary_closing(keep, iterations=2)
    # fill the small dark gaps inside the picture, but big openings that show
    # the card's background (the space dock's ring) stay see-through
    holes, nh = ndimage.label(ndimage.binary_fill_holes(closed) & ~closed)
    shape = closed.copy()
    for k in range(1, nh + 1):
        h = holes == k
        if h.sum() < 40 or a[h].mean() > 52:
            shape |= h
    # soft edge: one pixel of anti-aliasing from the original brightness
    edge = ndimage.binary_dilation(shape) & ~shape
    alpha = np.where(shape, 1.0, np.where(edge, np.clip((a - 40.0) / 50.0, 0.0, 1.0), 0.0))
    grey = np.clip((a - 55.0) / (235.0 - 55.0), 0.0, 1.0)
    grey = np.where(shape, np.maximum(grey, 0.12), grey)
    ys, xs = np.where(alpha > 0)
    sl = (slice(ys.min(), ys.max() + 1), slice(xs.min(), xs.max() + 1))
    g = (grey[sl] * 255).astype(np.uint8)
    al = (alpha[sl] * 255).astype(np.uint8)
    return Image.merge("RGBA", [Image.fromarray(g)] * 3 + [Image.fromarray(al)])


def main():
    card = sys.argv[1] if len(sys.argv) > 1 else "docs/owner/neutral_unit_reference.webp"
    out = sys.argv[2] if len(sys.argv) > 2 else "assets/icons/units"
    os.makedirs(out, exist_ok=True)
    img = Image.open(card).convert("RGB")
    if img.size != (826, 1417):
        img = img.resize((826, 1417), Image.LANCZOS)
    for name, box in BOXES.items():
        im = cut(img, box)
        if name in SHIPS and name != "warsun":  # the war sun stands upright, dome up (owner, 0.21.1)
            im = im.transpose(Image.ROTATE_270)  # nose up -> nose right
        im.save(os.path.join(out, name + ".png"))
        print(f"{name}: {im.width} x {im.height}")


if __name__ == "__main__":
    main()
