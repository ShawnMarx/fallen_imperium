"""Tiny pixel-art portrait kit (64x64 busts): shapes are 'inflated' into soft 3D forms, lit from the upper left,
posterised into hue-shifted colour ramps, then outlined - the look of hand-made pixel portraits."""
import colorsys
import math

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

LIGHT = np.array([-0.55, -0.7, 0.6])
LIGHT = LIGHT / np.linalg.norm(LIGHT)


def hexrgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def ramp(base, n=5, spread=0.62, hue_shift=0.06, sat_boost=0.1):
    """darkest -> lightest; shadows lean cool (towards purple), lights warm (towards yellow)"""
    r, g, b = [c / 255 for c in (hexrgb(base) if isinstance(base, str) else base)]
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    out = []
    for i in range(n):
        t = i / (n - 1) - 0.5  # -0.5 .. 0.5 (base in the middle)
        hh = h
        # shift hue: shadows towards 0.72 (blue-violet), lights towards 0.14 (yellow)
        target = 0.72 if t < 0 else 0.14
        d = ((target - h + 0.5) % 1.0) - 0.5
        hh = (h + d * abs(t) * 2 * hue_shift * 3) % 1.0
        vv = min(1.0, max(0.0, v * (1 + t * spread * 2)))
        ss = min(1.0, max(0.0, s * (1 + sat_boost) - t * 0.25))
        out.append(tuple(int(c * 255) for c in colorsys.hsv_to_rgb(hh, ss, vv)))
    return out


# ---- animation helpers: t runs 0..1 over one loop (FRAMES frames of FRAME_MS each)
FRAMES = 24
FRAME_MS = 125


def wave(t, phase=0.0, cycles=1):
    """-1..1 sine over the loop"""
    return math.sin(2 * math.pi * (t * cycles + phase))


def breath(t, phase=0.0, amp=1):
    """whole-pixel rise and fall for breathing: 0 at rest, -amp when the chest is up"""
    return -int(round(amp * (0.5 - 0.5 * math.cos(2 * math.pi * (t + phase)))))


def sway(t, phase=0.0, amp=1, cycles=1):
    return int(round(amp * wave(t, phase, cycles)))


def pulse(t, phase=0.0, cycles=1):
    """0..1 glow strength"""
    return 0.5 + 0.5 * wave(t, phase, cycles)


def blink(t, at=0.62, frames=2):
    """True for a couple of frames once per loop (eyes shut, a light dims)"""
    f = int(t * FRAMES) % FRAMES
    a = int(at * FRAMES) % FRAMES
    return (f - a) % FRAMES < frames


def mix(c1, c2, k):
    c1 = hexrgb(c1) if isinstance(c1, str) else c1
    c2 = hexrgb(c2) if isinstance(c2, str) else c2
    return tuple(int(int(a) + (int(b) - int(a)) * k) for a, b in zip(c1, c2))


def bezier(pts, n=24):
    """points along a Bezier curve through control points pts (any degree)"""
    out = []
    for i in range(n + 1):
        t = i / n
        P = [np.array(q, float) for q in pts]
        while len(P) > 1:
            P = [(1 - t) * P[j] + t * P[j + 1] for j in range(len(P) - 1)]
        out.append((float(P[0][0]), float(P[0][1])))
    return out


def ribbon(path, w0, w1=None, w_mid=None):
    """a polygon around a path, width w0 at the start -> w1 at the end (optionally w_mid in the middle)"""
    w1 = w0 if w1 is None else w1
    left, right = [], []
    n = len(path)
    for i, (x, y) in enumerate(path):
        a, b = path[max(0, i - 1)], path[min(n - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / L, dx / L
        u = i / max(1, n - 1)
        if w_mid is None:
            w = w0 + (w1 - w0) * u
        else:
            w = (w0 + (w_mid - w0) * 2 * u) if u < 0.5 else (w_mid + (w1 - w_mid) * (2 * u - 1))
        left.append((x + nx * w / 2, y + ny * w / 2))
        right.append((x - nx * w / 2, y - ny * w / 2))
    return left + right[::-1]


def rot_ellipse(cx, cy, rx, ry, ang, n=40):
    c, s_ = math.cos(ang), math.sin(ang)
    return [(cx + rx * math.cos(2 * math.pi * i / n) * c - ry * math.sin(2 * math.pi * i / n) * s_,
             cy + rx * math.cos(2 * math.pi * i / n) * s_ + ry * math.sin(2 * math.pi * i / n) * c) for i in range(n)]


class Portrait:
    def __init__(self, w=64, h=64):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3), np.uint8)
        self.a = np.zeros((h, w), bool)
        self.yy, self.xx = np.mgrid[0:h, 0:w]
        self.dx = self.dy = 0  # current offset (moving parts of an animated portrait)
        self.bg = None  # optional painted background (h, w, 3) shown behind the figure instead of the flat backdrop

    class _At:
        def __init__(self, p, dx, dy):
            self.p, self.d = p, (dx, dy)

        def __enter__(self):
            self.old = (self.p.dx, self.p.dy)
            self.p.dx, self.p.dy = self.old[0] + self.d[0], self.old[1] + self.d[1]
            return self.p

        def __exit__(self, *a):
            self.p.dx, self.p.dy = self.old

    def at(self, dx=0, dy=0):
        """`with p.at(0, -1):` draws everything inside one pixel higher (offsets add up when nested)"""
        return Portrait._At(self, int(dx), int(dy))

    # ---- masks
    def poly(self, pts):
        im = Image.new('L', (self.w, self.h), 0)
        ImageDraw.Draw(im).polygon([(x + self.dx, y + self.dy) for x, y in pts], fill=255)
        return np.array(im) > 0

    def ellipse(self, cx, cy, rx, ry):
        cx, cy = cx + self.dx, cy + self.dy
        return ((self.xx + 0.5 - cx) / rx) ** 2 + ((self.yy + 0.5 - cy) / ry) ** 2 <= 1.0

    def line_mask(self, pts, width=1):
        im = Image.new('L', (self.w, self.h), 0)
        ImageDraw.Draw(im).line([(x + self.dx, y + self.dy) for x, y in pts], fill=255, width=width)
        return np.array(im) > 0

    # ---- painting
    def fill(self, mask, ramp_, radius=6.0, bias=0.0, edge=True, levels=(0.22, 0.46, 0.68, 0.88), flat=None, clip=None,
             tilt=(0.0, 0.0), texture=0.0, seed=1, depth=None, light=None):
        """inflate the mask into a rounded form and light it; flat = a fixed ramp index"""
        if clip is not None:
            mask = mask & clip
        if not mask.any():
            return mask
        if flat is not None:
            idx = np.full(mask.shape, flat)
        else:
            d = ndimage.distance_transform_edt(np.pad(mask, 1))[1:-1, 1:-1]
            hgt = np.sqrt(np.clip(d / radius, 0, 1))
            gy, gx = np.gradient(hgt)
            k = 2.2 if depth is None else depth  # the form's height in pixels (big rounded forms: ~radius)
            nx, ny, nz = -gx * k + tilt[0], -gy * k + tilt[1], np.ones_like(gx)
            ln = np.sqrt(nx * nx + ny * ny + nz * nz)
            L = LIGHT if light is None else light
            lam = (nx * L[0] + ny * L[1] + nz * L[2]) / ln + bias
            if texture:
                rng = np.random.default_rng(seed)
                lam = lam + (rng.random(lam.shape) - 0.5) * texture
            lam = 0.5 + (lam - 0.5) * 1.35  # more contrast: deep shadows, bright lights
            idx = np.digitize(lam, levels)
        idx = np.clip(idx, 0, len(ramp_) - 1)
        cols = np.array(ramp_, np.uint8)[idx]
        self.rgb[mask] = cols[mask]
        self.a |= mask
        if edge:  # a dark contour where this shape meets what is under it
            inner = ndimage.binary_erosion(mask)
            rim = mask & ~inner
            self.rgb[rim] = np.array(ramp_[0], np.uint8)
        return mask

    def px(self, x, y, col):
        x, y = x + self.dx, y + self.dy
        if 0 <= x < self.w and 0 <= y < self.h:
            self.rgb[y, x] = col if not isinstance(col, str) else hexrgb(col)
            self.a[y, x] = True

    def pixels(self, pts, col):
        for x, y in pts:
            self.px(x, y, col)

    def stroke(self, pts, col, width=1):
        m = self.line_mask(pts, width)
        self.rgb[m] = hexrgb(col) if isinstance(col, str) else col
        self.a |= m

    def glow(self, cx, cy, core, halo, r=1, strength=1.0):
        """a glowing dot (eyes, gems): bright core, a lighter halo blended over what is there"""
        hc = np.array(hexrgb(halo) if isinstance(halo, str) else halo, float)
        cx, cy = cx + self.dx, cy + self.dy
        for y in range(cy - r - 1, cy + r + 2):
            for x in range(cx - r - 1, cx + r + 2):
                if 0 <= x < self.w and 0 <= y < self.h:
                    d = math.hypot(x - cx, y - cy)
                    if d <= r + 1.2:
                        t = (0.55 if d > r + 0.3 else 0.85) * strength
                        self.rgb[y, x] = (self.rgb[y, x] * (1 - t) + hc * t).astype(np.uint8)
                        self.a[y, x] = True
        self.px(cx - self.dx, cy - self.dy, core)

    def rim(self, col, side=1, width=1, amount=0.7, clip=None):
        """neon rim light on the edge facing away from the key light (side 1 = right, -1 = left)"""
        c = np.array(hexrgb(col) if isinstance(col, str) else col, float)
        a = self.a.copy()
        hit = np.zeros_like(a)
        for k in range(1, width + 2):
            sh = np.zeros_like(a)
            if side > 0:
                sh[:, :-k] = ~a[:, k:]
                sh[:, -k:] = True
            else:
                sh[:, k:] = ~a[:, :-k]
                sh[:, :k] = True
            hit |= a & sh
        # keep it to the outer ring, not deep inside
        inner = ndimage.binary_erosion(a, iterations=width + 1)
        hit &= ~inner
        if clip is not None:
            hit &= clip
        self.rgb[hit] = (self.rgb[hit] * (1 - amount) + c * amount).astype(np.uint8)

    def outline(self, col='#0d0a14'):
        grown = ndimage.binary_dilation(self.a) & ~self.a
        self.rgb[grown] = hexrgb(col)
        self.a |= grown

    def image(self, scale=1, bg=None):
        im = np.zeros((self.h, self.w, 4), np.uint8)
        im[..., :3] = self.rgb
        im[..., 3] = np.where(self.a, 255, 0)
        img = Image.fromarray(im, 'RGBA')
        if bg is not None and self.bg is not None:
            b = Image.fromarray(np.dstack([self.bg, np.full((self.h, self.w), 255, np.uint8)]), 'RGBA')
            b.alpha_composite(img)
            img = b
        elif bg is not None:
            b = Image.new('RGBA', img.size, bg)
            b.alpha_composite(img)
            img = b
        if scale != 1:
            img = img.resize((self.w * scale, self.h * scale), Image.NEAREST)
        return img


def frames(face_fn, n=FRAMES):
    """every frame of an animated portrait (face_fn(t) with t = 0..1)"""
    return [face_fn(i / n) for i in range(n)]


def save_gif(face_fn, path, scale=4, n=FRAMES, ms=FRAME_MS):
    fr = frames(face_fn, n)
    ims = []
    for p in fr:
        bg = getattr(p, 'backdrop', (12, 14, 30))
        im = p.image(scale, bg=tuple(bg) + (255,)).convert('RGB')
        ims.append(im)
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=ms, loop=0, disposal=1)
    return fr
