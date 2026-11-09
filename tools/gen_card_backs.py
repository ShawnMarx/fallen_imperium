#!/usr/bin/env python3
"""Card backs from the owner's pictures (0.33.1).

The action card back (docs/owner/action_card_back_reference.png, sent by the owner) is cut to the card
and its glow: inside the glowing frame the picture is kept as it is; outside it the glow fades out over
a few pixels, so the card sits on any background. Two sizes, like the campaign art:
  action_back-pc.png    (Windows / Steam Deck: drawn at up to 2.25x)
  action_back-miyoo.png (the Miyoo's 640 x 480)

usage: gen_card_backs.py OWNER_DIR OUT_DIR
"""
import os
import sys

import numpy as np
from PIL import Image

# the glowing frame of the card in the owner's 603 x 861 picture, and how much glow to keep round it
FRAME = (81, 87, 521, 804)   # x0, y0, x1, y1
MARGIN = 16
SIZES = {"pc": 360, "miyoo": 160}


def main(src_dir, out_dir):
    im = Image.open(os.path.join(src_dir, "action_card_back_reference.png")).convert("RGB")
    x0, y0, x1, y1 = FRAME
    crop = im.crop((x0 - MARGIN, y0 - MARGIN, x1 + 1 + MARGIN, y1 + 1 + MARGIN))
    a = np.asarray(crop).astype(np.float64) / 255.0
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    # distance outside the frame (0 inside), in source pixels
    dx = np.maximum(0, np.maximum(MARGIN - xx, xx - (w - 1 - MARGIN)))
    dy = np.maximum(0, np.maximum(MARGIN - yy, yy - (h - 1 - MARGIN)))
    d = np.hypot(dx, dy)
    fade = np.clip(1.0 - d / MARGIN, 0, 1) ** 1.5
    # outside the frame keep only what glows (the dark space round the card goes clear)
    lum = a.max(axis=2)
    alpha = np.where(d > 0, fade * np.clip((lum - 0.12) / 0.5, 0, 1), 1.0)
    rgba = np.dstack([a, alpha])
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")
    os.makedirs(out_dir, exist_ok=True)
    for name, width in SIZES.items():
        height = int(round(width * h / w))
        out = img.resize((width, height), Image.LANCZOS)
        path = os.path.join(out_dir, "action_back-%s.png" % name)
        out.save(path, optimize=True)
        print("wrote", path, out.size)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
