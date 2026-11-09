#!/usr/bin/env python3
"""Thunder's Edge / Discordant Stars faction emblems (0.29.0, owner: "draw them to
match the base and PoK icons"): each emblem is lifted off its banner in the owner's
faction label sheet (TI4FactionLabels_v5_wTE.pdf, pages 3-5 rendered at 600 dpi as
x6-<page>.png) - the banner's glow behind it is painted out (inpainting from the
ring around the emblem), the emblem kept where it differs from that background,
cleaned (holes, specks) and refined with GrabCut, then saved as a transparent
128 px icon like the other faction icons.
Usage: tools/gen_soon_emblems.py PAGES_DIR LABELS_PDF OUT_DIR"""
import sys, os
import numpy as np
import cv2
import pdfplumber

NAMES = {2: ["axis", "celdauri", "cymiae", "florzen", "freesystems", "ghoti", "ilyxum", "kollecc", "kortali", "lanefir",
             "bentor", "cheiran", "dihmohn", "edyn", "ghemina", "gledge", "kjalengard", "kolume", "kyro", "lizho"],
         3: ["ltokk", "mortheus", "nivyn", "olradin", "rohdhna", "vaylerian", "veldyr",
             "mirveda", "myko", "nokar", "rhodun", "tnelis", "vaden", "zelian"],
         4: ["lastbastion", "deepwrought", "obsidian", None, None, None, None, "ralnel", "crimson", None, None, None, None]}


def emblem(img):
    """img: BGR uint8 square crop with the emblem in the middle -> BGRA (128 px) or None"""
    S = img.shape[0]
    yy, xx = np.mgrid[0:S, 0:S]
    rr = np.hypot(xx - S / 2, yy - S / 2) / (S / 2)
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    # the banner around the emblem's panel: its blue starfield and the white bevel
    banner = ((hsv[..., 0] > 95) & (hsv[..., 0] < 125) & (hsv[..., 1] > 90)) | ((hsv[..., 1] < 40) & (hsv[..., 2] > 200) & (rr > 0.7))
    # the panel's own glow, modelled smoothly (a wide median ignores the emblem's detail)
    small = cv2.resize(img, (64, 64), interpolation=cv2.INTER_AREA)
    bgm = cv2.medianBlur(small, 31)
    bgm = cv2.resize(cv2.GaussianBlur(bgm, (0, 0), 3), (S, S), interpolation=cv2.INTER_CUBIC)
    lab_i = cv2.cvtColor(img, cv2.COLOR_BGR2LAB).astype(np.float32)
    lab_b = cv2.cvtColor(bgm, cv2.COLOR_BGR2LAB).astype(np.float32)
    diff = np.sqrt(((lab_i - lab_b) ** 2).sum(axis=2))
    gc = np.full((S, S), cv2.GC_PR_BGD, np.uint8)
    gc[(diff > 26) & (rr < 0.8)] = cv2.GC_PR_FGD
    gc[(diff > 45) & (rr < 0.6)] = cv2.GC_FGD
    gc[(rr > 0.94) | banner] = cv2.GC_BGD
    gc[(yy < S * 0.06)] = cv2.GC_BGD
    bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    try:
        cv2.grabCut(img, gc, None, bgd, fgd, 5, cv2.GC_INIT_WITH_MASK)
    except cv2.error:
        return None
    fg = ((gc == cv2.GC_FGD) | (gc == cv2.GC_PR_FGD)).astype(np.uint8)
    fg = cv2.morphologyEx(fg, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, stats, cent = cv2.connectedComponentsWithStats(fg, 8)
    out = np.zeros_like(fg)
    for k in range(1, n):
        if stats[k, cv2.CC_STAT_AREA] > S * S * 0.0015 and np.hypot(cent[k][0] - S / 2, cent[k][1] - S / 2) < S * 0.38:
            out[lab == k] = 1
    flood = out.copy() * 255  # fill holes the emblem encloses
    ff = np.zeros((S + 2, S + 2), np.uint8)
    cv2.floodFill(flood, ff, (0, 0), 128)
    hole = flood == 0
    # only fill holes that look like the emblem (not the panel showing through)
    fill = hole & (diff > 18)
    out = (out | fill).astype(np.uint8)
    alpha = cv2.GaussianBlur(out.astype(np.float32), (0, 0), 1.0)
    alpha = np.clip(alpha * 1.2, 0, 1)
    ys, xs = np.where(out > 0)
    if len(xs) == 0:
        return None
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    side = int(max(x1 - x0, y1 - y0) * 1.06) + 4
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    rgba = np.dstack([img, (alpha * 255).astype(np.uint8)])
    pad = side
    big = cv2.copyMakeBorder(rgba, pad, pad, pad, pad, cv2.BORDER_CONSTANT, value=(0, 0, 0, 0))
    crop = big[cy + pad - side // 2: cy + pad + side // 2, cx + pad - side // 2: cx + pad + side // 2]
    return cv2.resize(crop, (128, 128), interpolation=cv2.INTER_AREA)


def emblem_a(img):
    """the other way: paint the middle out from the ring around it, keep what differs"""
    S = img.shape[0]
    yy, xx = np.mgrid[0:S, 0:S]
    rr = np.hypot(xx - S / 2, yy - S / 2) / (S / 2)
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    # the label's starry blue band runs along the top and the bevel down the bottom-left: not panel, not emblem
    edge = (yy < S * 0.2) | ((xx < S * 0.3) & (yy > S * 0.6))
    banner = edge & (((hsv[..., 0] > 95) & (hsv[..., 0] < 130) & (hsv[..., 1] > 70)) | ((hsv[..., 1] < 50) & (hsv[..., 2] > 170)))
    banner = cv2.dilate(banner.astype(np.uint8), np.ones((9, 9), np.uint8)) > 0
    hole = ((rr < 0.86) | banner).astype(np.uint8) * 255
    bg = cv2.inpaint(img, hole, 9, cv2.INPAINT_TELEA)
    bg = cv2.GaussianBlur(bg, (0, 0), S * 0.03)
    lab_i = cv2.cvtColor(img, cv2.COLOR_BGR2LAB).astype(np.float32)
    lab_b = cv2.cvtColor(bg, cv2.COLOR_BGR2LAB).astype(np.float32)
    diff = np.sqrt(((lab_i - lab_b) ** 2).sum(axis=2))
    m = (diff > 22).astype(np.uint8)
    m[rr > 0.92] = 0
    m[banner] = 0
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, stats, cent = cv2.connectedComponentsWithStats(m, 8)
    keep = np.zeros_like(m)
    for k in range(1, n):
        if stats[k, cv2.CC_STAT_AREA] > S * S * 0.002 and np.hypot(cent[k][0] - S / 2, cent[k][1] - S / 2) < S * 0.36:
            keep[lab == k] = 1
    flood = keep.copy() * 255
    ff = np.zeros((S + 2, S + 2), np.uint8)
    cv2.floodFill(flood, ff, (0, 0), 128)
    out = ((flood == 255) | (flood == 0)).astype(np.uint8)
    return finish(img, out)


def finish(img, out):
    S = img.shape[0]
    alpha = cv2.GaussianBlur(out.astype(np.float32), (0, 0), 1.0)
    alpha = np.clip(alpha * 1.2, 0, 1)
    ys, xs = np.where(out > 0)
    if len(xs) == 0:
        return None
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    side = int(max(x1 - x0, y1 - y0) * 1.06) + 4
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    rgba = np.dstack([img, (alpha * 255).astype(np.uint8)])
    pad = side
    big = cv2.copyMakeBorder(rgba, pad, pad, pad, pad, cv2.BORDER_CONSTANT, value=(0, 0, 0, 0))
    crop = big[cy + pad - side // 2: cy + pad + side // 2, cx + pad - side // 2: cx + pad + side // 2]
    return cv2.resize(crop, (128, 128), interpolation=cv2.INTER_AREA)


def main():
    pages, pdf_path, out = sys.argv[1], sys.argv[2], sys.argv[3]
    pdf = pdfplumber.open(pdf_path)
    k = 600 / 72
    for pi, names in NAMES.items():
        page = cv2.imread(os.path.join(pages, "x6-%d.png" % (pi + 1)))
        ims = sorted(pdf.pages[pi].images, key=lambda im: (round(im["x0"]), round(im["top"])))
        for im, nm in zip(ims, names):
            if not nm:
                continue
            x0, t, w, h = im["x0"] * k, im["top"] * k, im["width"] * k, im["height"] * k
            cx, cy, half = x0 + 0.163 * w, t + 0.5 * h, 0.52 * h
            side = int(2 * half)
            X0, Y0 = int(cx - half), int(cy - half)
            crop = page[Y0:Y0 + side, X0:X0 + side]
            method = os.environ.get("EMBLEM_METHOD", "a")  # "a" (inpainted panel) gave the cleaner set
            e = emblem_a(crop) if method == "a" else emblem(crop)
            grp = "factions_te" if pi == 4 else "factions_ds"
            os.makedirs(os.path.join(out, grp), exist_ok=True)
            if e is not None:
                cv2.imwrite(os.path.join(out, grp, nm + ".png"), e)
            print(grp, nm, "ok" if e is not None else "EMPTY")


if __name__ == "__main__":
    main()
