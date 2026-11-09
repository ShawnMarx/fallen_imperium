#!/usr/bin/env python3
"""Discordant Stars faction emblems, take 2 (0.29.1). The owner pointed at the DS tech
card backs: every back is the same card template with the faction's emblem sitting on a
black disc in the middle. So the template itself is recovered as the per-pixel median of
all the backs (the emblems differ, the frame and starfield don't), and each emblem is
whatever differs from that template - with a soft alpha edge, so glows and
anti-aliasing survive - turned upright (the backs show them a quarter turn
clockwise). The result is trimmed and saved as a transparent PNG next to the
base / PoK faction icons (tools/gen_icons.py scales them down when packing).
The backs come from the owner's "D:\\TI4 Fan Project" (DS Files/2 Technology Cards/back).

Usage: tools/gen_ds_emblems.py BACKS_DIR OUT_DIR [REVIEW_PNG]
BACKS_DIR holds one back per faction, named by the DS abbreviation (Axis.png, Bent.png...)."""
import sys, os, glob
import numpy as np
import cv2

# DS file abbreviation -> faction key in the game
KEYS = {"Axis": "axis", "Bent": "bentor", "Celd": "celdauri", "Chei": "cheiran", "Cymi": "cymiae",
        "DihM": "dihmohn", "Edyn": "edyn", "Flor": "florzen", "Free": "freesystems", "Ghem": "ghemina",
        "Ghot": "ghoti", "Gled": "gledge", "Ilyx": "ilyxum", "Kjal": "kjalengard", "Koll": "kollecc",
        "Kolu": "kolume", "Kort": "kortali", "Kyro": "kyro", "Lane": "lanefir", "LihZ": "lizho",
        "Ltok": "ltokk", "Mirv": "mirveda", "Mort": "mortheus", "Myko": "myko", "Nivy": "nivyn",
        "Noka": "nokar", "Olra": "olradin", "Rhod": "rhodun", "RohD": "rohdhna", "Tnel": "tnelis",
        "Vade": "vaden", "Vayl": "vaylerian", "Veldyr": "veldyr", "Zeli": "zelian"}

SPECKLED = {"kyro"}            # emblems whose design is a cloud of specks
CX, CY, R = 298.5, 414.0, 121.0   # the black disc on the 596x821 card back
LO, HI = 12.0, 40.0                # colour difference: fully clear below LO, solid above HI
OUT = 256                          # longest side of the saved emblem


def extract(img, bg, specks=False):
    H, W = img.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W]
    rr = np.hypot(xx - CX, yy - CY)
    bgf = bg.astype(np.float32)
    f = img.astype(np.float32)
    diff = np.abs(f - bgf).max(axis=2)
    strong = (diff > HI).astype(np.uint8)
    # keep only shapes that reach into the disc (drops the tech-colour badge, star tints)
    n, lab, stats, _ = cv2.connectedComponentsWithStats(strong, 8)
    inner = np.unique(lab[(rr < R - 10) & (strong > 0)])
    keep = np.isin(lab, inner[inner > 0])
    # drop lone specks; big emblem parts stay
    sizes = np.bincount(lab.ravel(), minlength=n)
    keep &= sizes[lab] >= 12
    near = cv2.dilate(keep.astype(np.uint8), np.ones((9, 9), np.uint8)) > 0
    near |= (rr < R - 8)  # soft glows inside the disc are part of the emblem too
    a = np.clip((diff - LO) / (HI - LO), 0, 1) * near
    a[rr > R + 60] = 0
    if not specks:
        # lone dust (stray stars, faint texture) away from the emblem goes
        n, lab, stats, _ = cv2.connectedComponentsWithStats((a > 0.04).astype(np.uint8), 8)
        area = stats[:, cv2.CC_STAT_AREA]
        big = np.isin(lab, np.nonzero(area >= max(40, area[1:].max() * 0.004))[0]) & (lab > 0)
        close = cv2.dilate(big.astype(np.uint8), np.ones((5, 5), np.uint8)) > 0
        touching = np.unique(lab[close & (lab > 0)])
        a[~np.isin(lab, touching)] = 0
    # un-mix the background out of semi-transparent pixels
    ae = np.maximum(a, 1e-3)[..., None]
    col = np.clip((f - (1 - a[..., None]) * bgf) / ae, 0, 255)
    ys, xs = np.nonzero(a > 0.04)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    out = np.dstack([col, a[..., None] * 255])[y0:y1, x0:x1]
    out = out.astype(np.uint8)
    h, w = out.shape[:2]
    k = OUT / max(h, w)
    # premultiplied resize so edges don't pick up dark fringes
    pm = out.astype(np.float32)
    pm[..., :3] *= pm[..., 3:4] / 255
    pm = cv2.resize(pm, (max(1, round(w * k)), max(1, round(h * k))), interpolation=cv2.INTER_AREA if k < 1 else cv2.INTER_CUBIC)
    pm = np.clip(pm, 0, 255)
    al = np.maximum(pm[..., 3:4], 1e-3)
    pm[..., :3] = np.clip(pm[..., :3] * 255 / al, 0, 255)
    # the backs print every emblem turned a quarter clockwise (compare the faction tokens and
    # labels: flames and tridents point up there), so turn it back
    return np.ascontiguousarray(np.rot90(pm.astype(np.uint8), 1))  # BGRA


def main():
    src, dst = sys.argv[1], sys.argv[2]
    review = sys.argv[3] if len(sys.argv) > 3 else None
    files = sorted(glob.glob(os.path.join(src, "*.png")))
    imgs = {os.path.basename(p)[:-4]: cv2.imread(p, cv2.IMREAD_COLOR) for p in files}
    stack = np.stack(list(imgs.values()))
    # the template: the median card, except on the disc, where most cards carry an
    # emblem - there the darkest card at each pixel shows the bare disc (and its few stars)
    bg = np.median(stack, axis=0).astype(np.uint8)
    H, W = bg.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W]
    disc = np.hypot(xx - CX, yy - CY) < R + 4
    # (the median of just the cards that are dark at that pixel, so one card printed from
    # a slightly different template can't leave its stars on every other emblem)
    srt = np.sort(stack, axis=0)
    dark = (stack < 40).sum(axis=0)
    idx = np.clip(dark // 2, 0, len(stack) - 1)
    low = np.take_along_axis(srt, idx[None], axis=0)[0]
    nodark = dark == 0  # no card is dark here: a star or the disc's glowing rim
    low[nodark] = bg[nodark]
    bg[disc] = low[disc]
    os.makedirs(dst, exist_ok=True)
    tiles = []
    for ab, key in KEYS.items():
        if ab not in imgs:
            print("missing", ab)
            continue
        e = extract(imgs[ab], bg, key in SPECKLED)
        cv2.imwrite(os.path.join(dst, key + ".png"), e)
        tiles.append((key, imgs[ab], e))
        print(key, e.shape[1], "x", e.shape[0])
    if review:
        T = 150
        cols = 6
        rows = (len(tiles) + cols - 1) // cols
        sheet = np.zeros((rows * (T * 2 + 22), cols * T, 3), np.uint8)
        for i, (key, card, e) in enumerate(tiles):
            r, c = divmod(i, cols)
            y = r * (T * 2 + 22)
            x = c * T
            disc = np.rot90(card[int(CY - R - 12):int(CY + R + 12), int(CX - R - 12):int(CX + R + 12)], 1)
            sheet[y:y + T, x:x + T] = cv2.resize(np.ascontiguousarray(disc), (T, T), interpolation=cv2.INTER_AREA)
            # the cut-out on a mid-grey checker so holes and halos show
            chk = ((np.indices((T, T)).sum(axis=0) // 10) % 2) * 30 + 70
            panel = np.dstack([chk] * 3).astype(np.float32)
            h, w = e.shape[:2]
            k = (T - 12) / max(h, w)
            s = cv2.resize(e, (max(1, int(w * k)), max(1, int(h * k))), interpolation=cv2.INTER_AREA).astype(np.float32)
            oy, ox = (T - s.shape[0]) // 2, (T - s.shape[1]) // 2
            al = s[..., 3:4] / 255
            reg = panel[oy:oy + s.shape[0], ox:ox + s.shape[1]]
            panel[oy:oy + s.shape[0], ox:ox + s.shape[1]] = reg * (1 - al) + s[..., :3] * al
            sheet[y + T:y + 2 * T, x:x + T] = panel.astype(np.uint8)
            cv2.putText(sheet, key, (x + 4, y + 2 * T + 15), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (220, 220, 220), 1, cv2.LINE_AA)
        cv2.imwrite(review, sheet)


if __name__ == "__main__":
    main()
