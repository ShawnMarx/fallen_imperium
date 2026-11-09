"""python3 deliver.py OUTNAME key:ref:oldmod ... -> comparison PNG + GIFs in /mnt/user-data/outputs"""
import sys, importlib
sys.path.insert(0, '.')
import faces_v2, pix
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]
rows = []
for a in sys.argv[2:]:
    k, ref, oldm = a.split(':')[:3]
    oldk = a.split(':')[3] if a.count(':') >= 3 else k
    rows.append((k, ref, getattr(importlib.import_module(oldm), oldk)))
F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 16)
W, H = 40 + 230 + 20 + 320 + 20 + 160, 30 + len(rows) * 350
sh = Image.new('RGB', (W, H), (18, 16, 34))
d = ImageDraw.Draw(sh)
d.text((20, 8), 'Your picture', font=F, fill=(200, 200, 230))
d.text((290, 8), 'New (round 2)', font=F, fill=(255, 90, 200))
d.text((630, 8), 'First try', font=F, fill=(150, 150, 180))
y = 34
for k, ref, old in rows:
    fn = getattr(faces_v2, k)
    pix.save_gif(fn, f'/mnt/user-data/outputs/portrait_{k}_v2.gif', scale=5)
    r = Image.open(ref).convert('RGB')
    r = r.resize((int(r.size[0] * 320 / r.size[1]), 320))
    sh.paste(r, (20, y))
    p = fn(0.0)
    sh.paste(p.image(5, bg=p.backdrop + (255,)).convert('RGB'), (290, y))
    o = old(0.0)
    sh.paste(o.image(2, bg=o.backdrop + (255,)).convert('RGB').resize((160, 160), Image.NEAREST), (630, y + 80))
    y += 350
sh.save(f'/mnt/user-data/outputs/{out}')
