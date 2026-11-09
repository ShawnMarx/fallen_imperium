#!/usr/bin/env python3
"""Draw the 74x74 OnionOS app icon (package/icon.png): a gold-edged hex
system tile with a ringed planet. Drawn at 4x and scaled down for smooth edges.
Also writes the Windows icon (package/windows/ti4.ico, 16 to 256 pixels)."""
import math
import os
from PIL import Image, ImageDraw


def render(S):
    W = 74 * S
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = W / 2

    def hexagon(r):
        return [(cx + r * math.cos(math.pi / 3 * i), cy + r * math.sin(math.pi / 3 * i)) for i in range(6)]

    d.polygon(hexagon(36 * S), fill=(232, 182, 76, 255))
    d.polygon(hexagon(32.5 * S), fill=(18, 26, 50, 255))
    # stars
    for x, y, r in [(16, 30, 1.2), (52, 22, 1.0), (58, 46, 1.3), (22, 52, 1.0), (40, 14, 0.9), (30, 60, 0.8)]:
        d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=(220, 225, 240, 255))
    # planet ring (back half), planet, ring (front half)
    ring_box = [cx - 26 * S, cy - 8 * S, cx + 26 * S, cy + 8 * S]
    d.arc(ring_box, 180, 360, fill=(240, 200, 110, 255), width=3 * S)
    pr = 14 * S
    d.ellipse([cx - pr, cy - pr, cx + pr, cy + pr], fill=(214, 150, 60, 255))
    d.ellipse([cx - pr * 0.75, cy - pr * 0.85, cx + pr * 0.35, cy + pr * 0.2], fill=(238, 190, 100, 255))
    d.arc(ring_box, 0, 180, fill=(240, 200, 110, 255), width=3 * S)
    return img


img = render(4).resize((74, 74), Image.LANCZOS)
img.save("package/icon.png")
print("wrote package/icon.png", img.size)

os.makedirs("package/windows", exist_ok=True)
big = render(14)  # 1036 px, scaled down for each size
sizes = [16, 24, 32, 48, 64, 128, 256]
frames = [big.resize((n, n), Image.LANCZOS) for n in sizes]
frames[-1].save("package/windows/ti4.ico", format="ICO", sizes=[(n, n) for n in sizes], append_images=frames[:-1])
print("wrote package/windows/ti4.ico", sizes)
