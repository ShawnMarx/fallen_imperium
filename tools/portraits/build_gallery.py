"""Write every faction portrait as an animated GIF (4x) plus one labelled still sheet.
Usage: python3 build_gallery.py OUTDIR"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pix  # noqa: E402
import portraits_all as pa  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402


def main(out):
    os.makedirs(out, exist_ok=True)
    for k, name, g, f in pa.ALL:
        pix.save_gif(f, os.path.join(out, k + '.gif'), scale=4)
    bold = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
    cell, cols, pad, lab = 128, 10, 14, 30
    rows = []
    for group, _ in pa.ORDER:
        rows.append(('H', group))
        keys = [x for x in pa.ALL if x[2] == group]
        for i in range(0, len(keys), cols):
            rows.append(('R', keys[i:i + cols]))
    h = sum(36 if r[0] == 'H' else cell + lab + 6 for r in rows) + pad * 2
    w = cols * (cell + 10) + pad * 2 - 10
    sh = Image.new('RGB', (w, h), (14, 12, 28))
    d = ImageDraw.Draw(sh)
    y = pad
    for r in rows:
        if r[0] == 'H':
            d.text((pad, y + 8), r[1].upper(), font=bold, fill=(255, 90, 200))
            y += 36
            continue
        for i, (k, name, g, f) in enumerate(r[1]):
            p = f(0.0)
            x = pad + i * (cell + 10)
            sh.paste(p.image(2, bg=tuple(p.backdrop) + (255,)).convert('RGB'), (x, y))
            short = name.replace('The ', '')
            fs = 12
            while fs > 8 and d.textlength(short, font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', fs)) > cell:
                fs -= 1
            fnt = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', fs)
            d.text((x + cell / 2 - d.textlength(short, font=fnt) / 2, y + cell + 6), short, font=fnt, fill=(220, 224, 240))
        y += cell + lab + 6
    sh.save(os.path.join(out, 'all_portraits.png'))


if __name__ == '__main__':
    main(sys.argv[1])
