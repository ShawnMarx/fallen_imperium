"""Mock-ups of the comms 'codec' (Metal Gear Solid style, full colour, Retrowave) for the owner's review."""
import math
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import faces  # noqa: E402

ICONS = '/home/claude/ti4-miyoo/assets/icons/'
FB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
FR = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
FM = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'
FC = '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf'
W, H = 1280, 720


def font(path, size):
    return ImageFont.truetype(path, size)


def background(img, tint=(10, 8, 24), grid=(40, 30, 80), neb=((150, 40, 170), (40, 110, 190)), planet=(70, 60, 150),
               seed=7):
    """deep space behind the codec: nebula clouds, a dense starfield, a distant ringed planet with a neon rim"""
    import numpy as np
    W, H = img.size
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:H, 0:W].astype(float)
    base = np.array(tint, float) + (yy / H)[..., None] * np.array((8, 4, 16), float)

    def noise(scale, oct_=4):
        out = np.zeros((H, W))
        amp, tot = 1.0, 0.0
        for o in range(oct_):
            gh, gw = max(2, int(H / scale) + 2), max(2, int(W / scale) + 2)
            g = Image.fromarray((rng.random((gh, gw)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)
            out += amp * np.asarray(g, float) / 255
            tot += amp
            amp *= 0.5
            scale /= 2
        return out / tot
    n1, n2 = noise(260), noise(200)
    band = np.clip(1 - np.abs((yy - H * 0.45 - (xx - W / 2) * 0.35) / (H * 0.42)), 0, 1)
    c1 = np.clip((n1 - 0.45) * 2.4, 0, 1) * band
    c2 = np.clip((n2 - 0.5) * 2.2, 0, 1) * (1 - band * 0.5)
    col = base + c1[..., None] * np.array(neb[0], float) * 0.9 + c2[..., None] * np.array(neb[1], float) * 0.55
    # dust lanes: darker veins through the nebula
    col *= (1 - 0.45 * np.clip((noise(90, 3) - 0.62) * 4, 0, 1))[..., None]
    arr = np.clip(col, 0, 255).astype(np.uint8)
    im = Image.fromarray(arr, 'RGB').convert('RGBA')
    # the distant planet, lower right, lit from the left with a neon rim, a thin ring
    pl = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    pd = ImageDraw.Draw(pl)
    px_, py_, pr = int(W * 0.5), int(H * 1.55), int(H * 0.95)
    for k in range(pr, 0, -2):
        t = k / pr
        c = tuple(int(planet[i] * (0.25 + 0.75 * (1 - t) ** 0.6)) for i in range(3))
        pd.ellipse((px_ - k - (pr - k) * 0.35, py_ - k - (pr - k) * 0.35, px_ + k - (pr - k) * 0.35, py_ + k - (pr - k) * 0.35),
                   fill=c + (255,))
    pd.arc((px_ - pr, py_ - pr, px_ + pr, py_ + pr), 200, 340, fill=(255, 120, 220, 255), width=4)
    for k, a_ in enumerate((90, 60, 30)):  # an atmosphere glow along the horizon
        pd.arc((px_ - pr - 4 - k * 4, py_ - pr - 4 - k * 4, px_ + pr + 4 + k * 4, py_ + pr + 4 + k * 4), 205, 335,
               fill=(255, 110, 220, a_), width=4)
    pl = pl.filter(ImageFilter.GaussianBlur(0.6))
    im.alpha_composite(pl)
    # stars: many faint, some bright with glow and cross flares
    d = ImageDraw.Draw(im)
    rnd = random.Random(seed)
    for _ in range(900):
        x, y = rnd.randrange(W), rnd.randrange(H)
        v = rnd.random()
        c = (int(150 + 100 * v), int(150 + 100 * v), 255) if rnd.random() < 0.7 else (255, int(170 + 80 * v), int(220 + 30 * v))
        im.putpixel((x, y), c + (255,))
    glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for _ in range(40):
        x, y = rnd.randrange(W), rnd.randrange(H)
        r = rnd.choice([2, 3, 3, 4, 6])
        c = rnd.choice([(255, 255, 255), (180, 220, 255), (255, 190, 240)])
        gd.ellipse((x - r * 2, y - r * 2, x + r * 2, y + r * 2), fill=c + (70,))
        if r >= 4:
            gd.line([(x - r * 4, y), (x + r * 4, y)], fill=c + (160,), width=1)
            gd.line([(x, y - r * 4), (x, y + r * 4)], fill=c + (160,), width=1)
        gd.ellipse((x - 1, y - 1, x + 1, y + 1), fill=c + (255,))
    glow = glow.filter(ImageFilter.GaussianBlur(1.2))
    im.alpha_composite(glow)
    img.paste(im, (0, 0))


def glow_rect(img, box, col, width=2, rad=10, glow=6):
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.rounded_rectangle(box, rad, outline=col + (170,), width=width + 4)
    layer = layer.filter(ImageFilter.GaussianBlur(glow))
    img.alpha_composite(layer)
    ImageDraw.Draw(img).rounded_rectangle(box, rad, outline=col + (255,), width=width)


def panel(img, box, fill=(8, 10, 26, 225), rad=10):
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).rounded_rectangle(box, rad, fill=fill)
    img.alpha_composite(layer)


def portrait_window(img, x, y, face, frame_col, name, icon, sub, scale=4, mirror=False, alert=False):
    s = 64 * scale
    pad = 10
    box = (x, y, x + s + 2 * pad, y + s + 2 * pad)
    panel(img, box, fill=(6, 8, 20, 240))
    # the codec backdrop behind the face: a dark coloured wash with fine scanlines
    back = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    bd = ImageDraw.Draw(back)
    base = getattr(face, 'backdrop', None) or ((60, 10, 18) if alert else (14, 34, 60))
    for yy in range(s):
        t = yy / s
        k = 1.25 - t * 0.55
        c = (min(255, int(base[0] * k) + 4), min(255, int(base[1] * k) + 4), min(255, int(base[2] * k) + 6), 255)
        bd.line([(0, yy), (s, yy)], fill=c)
    for yy in range(0, s, 12):  # faint horizontal bars
        bd.line([(0, yy), (s, yy)], fill=(255, 255, 255, 14))
    im = face.image(scale, bg=tuple(face.backdrop) + (255,)) if getattr(face, 'bg', None) is not None else face.image(scale)
    if mirror:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    back.alpha_composite(im)
    # CRT scanlines over the portrait
    sl = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sl)
    for yy in range(0, s, 3):
        sd.line([(0, yy), (s, yy)], fill=(0, 0, 0, 70))
    back.alpha_composite(sl)
    img.alpha_composite(back, (x + pad, y + pad))
    glow_rect(img, box, frame_col, width=3, rad=8)
    # name plate
    ny = box[3] + 8
    panel(img, (x, ny, box[2], ny + 46), fill=(10, 12, 30, 235), rad=6)
    ic = Image.open(icon).convert('RGBA').resize((34, 34), Image.LANCZOS)
    img.alpha_composite(ic, (x + 8, ny + 6))
    d = ImageDraw.Draw(img)
    fs = 17
    while fs > 11 and d.textlength(name, font=font(FC, fs)) > box[2] - x - 58:
        fs -= 1
    d.text((x + 50, ny + 5), name, font=font(FC, fs), fill=frame_col)
    d.text((x + 50, ny + 26), sub, font=font(FR, 13), fill=(170, 180, 210))
    return box


def wrap(text, f, width, d):
    words, lines, cur = text.split(), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if d.textlength(t, font=f) > width and cur:
            lines.append(cur)
            cur = w
        else:
            cur = t
    if cur:
        lines.append(cur)
    return lines


def speech_box(img, box, who, col, text, typed=1.0, alert=False):
    panel(img, box, fill=(8, 6, 22, 235) if not alert else (26, 4, 10, 235), rad=10)
    glow_rect(img, box, col, width=2, rad=10, glow=5)
    d = ImageDraw.Draw(img)
    d.text((box[0] + 18, box[1] + 12), who, font=font(FC, 18), fill=col)
    f = font(FR, 20)
    shown = text[:int(len(text) * typed)]
    y = box[1] + 42
    for ln in wrap(shown, f, box[2] - box[0] - 36, d):
        d.text((box[0] + 18, y), ln, font=f, fill=(236, 238, 250))
        y += 28
    if typed < 1.0:
        d.rectangle((box[0] + 18 + d.textlength(wrap(shown, f, box[2] - box[0] - 36, d)[-1], font=f) + 3, y - 26,
                     box[0] + 18 + d.textlength(wrap(shown, f, box[2] - box[0] - 36, d)[-1], font=f) + 13, y - 6), fill=col)


def signal(img, cx, y, col, alert=False):
    d = ImageDraw.Draw(img)
    f = font(FM, 30)
    label = 'CHANNEL LOST' if alert else 'SUBSPACE'
    d.text((cx - d.textlength(label, font=font(FC, 15)) / 2, y), label, font=font(FC, 15), fill=(160, 170, 220))
    freq = '--.--' if alert else '7.21'
    tw = d.textlength(freq, font=f)
    d.text((cx - tw / 2, y + 22), freq, font=f, fill=col)
    # signal bars
    rnd = random.Random(3 if not alert else 9)
    for i in range(12):
        h = rnd.randint(6, 40) if not alert else rnd.choice([2, 4, 30, 44, 6])
        x = cx - 72 + i * 12
        for k in range(0, h, 4):
            t = k / 44
            c = (int(70 + 185 * t), int(220 - 150 * t), int(255 - 60 * t)) if not alert else (255, int(60 + 90 * (1 - t)), 60)
            d.rectangle((x, y + 120 - k, x + 8, y + 122 - k), fill=c)


def negotiation(out):
    img = Image.new('RGBA', (W, H))
    background(img)
    d = ImageDraw.Draw(img)
    d.text((40, 22), 'NEGOTIATIONS', font=font(FC, 26), fill=(255, 80, 190))
    d.text((40, 54), 'An offer arrives over the subspace channel', font=font(FR, 15), fill=(160, 170, 220))
    import faces_v2
    mah = faces_v2.mahact()
    niv = faces.nivyn2()
    portrait_window(img, 40, 84, mah, (255, 70, 200), 'MAHACT GENE-SORCERERS', ICONS + 'factions/mahact.png', 'Speaking  ·  Hostile')
    portrait_window(img, W - 40 - 276, 84, niv, (70, 230, 255), 'NIVYN STAR KINGS', ICONS + 'factions_ds/nivyn.png', 'You',
                    mirror=True)
    signal(img, W // 2, 84, (90, 255, 200))
    speech_box(img, (346, 236, W - 346, 404), 'MAHACT', (255, 70, 200),
               'Star Kings. You claim the spaces between the stars? We ruled the stars themselves. Your votes on this '
               'agenda, little king - and perhaps you keep your throne.')
    # the deal itself, as on today's deal screen
    box = (40, 470, W - 40, 700)
    panel(img, box, fill=(8, 10, 28, 235))
    glow_rect(img, box, (120, 130, 200), width=1, rad=10, glow=3)
    d = ImageDraw.Draw(img)
    d.text((64, 484), 'Mahact gives you', font=font(FC, 18), fill=(255, 110, 210))
    d.text((84, 512), '3 trade goods', font=font(FR, 17), fill=(230, 232, 245))
    d.text((84, 538), 'Promise: no attacks on your home for 2 rounds', font=font(FR, 17), fill=(230, 232, 245))
    d.text((W // 2 + 20, 484), 'You give the Mahact', font=font(FC, 18), fill=(230, 232, 245))
    d.text((W // 2 + 40, 512), 'All your votes FOR on "Mutiny"', font=font(FR, 17), fill=(230, 232, 245))
    d.line([(W // 2, 484), (W // 2, 576)], fill=(70, 80, 130), width=1)
    for i, (lab, col) in enumerate((('Accept', (60, 200, 110)), ('Counter', (70, 200, 240)), ('Decline', (230, 70, 80)))):
        bx = W // 2 - 260 + i * 180
        b = (bx, 626, bx + 160, 668)
        panel(img, b, fill=(16, 20, 40, 255), rad=8)
        glow_rect(img, b, col, width=2 if i == 0 else 1, rad=8, glow=3 if i == 0 else 1)
        tw = d.textlength(lab, font=font(FC, 19))
        d.text((bx + 80 - tw / 2, 635), lab, font=font(FC, 19), fill=(240, 240, 250))
    img.convert('RGB').save(out)


def betrayal(out):
    img = Image.new('RGBA', (W, H))
    background(img, tint=(22, 4, 10), neb=((200, 30, 50), (120, 20, 90)), planet=(110, 40, 40), seed=11)
    d = ImageDraw.Draw(img)
    # glitched BETRAYAL header
    f = font(FC, 54)
    title = 'BETRAYAL'
    tw = d.textlength(title, font=f)
    for dx, col in ((-4, (0, 220, 255)), (4, (255, 40, 120)), (0, (255, 235, 235))):
        d.text((W / 2 - tw / 2 + dx, 18), title, font=f, fill=col)
    for y in (34, 52, 61):
        d.rectangle((W / 2 - tw / 2 - 20, y, W / 2 + tw / 2 + 20, y + 2), fill=(26, 4, 10))
    sub = 'The Embers of Muaat break their promise to the Last Bastion'
    d.text((W / 2 - d.textlength(sub, font=font(FR, 16)) / 2, 84), sub, font=font(FR, 16), fill=(255, 170, 170))
    import faces_v2
    mu = faces_v2.muaat()
    ba = faces_v2.lastbastion()
    portrait_window(img, 40, 116, mu, (255, 120, 30), 'EMBERS OF MUAAT', ICONS + 'factions/muaat.png', 'Betrayer', alert=True)
    portrait_window(img, W - 40 - 276, 116, ba, (90, 220, 255), 'LAST BASTION', ICONS + 'factions_te/lastbastion.png', 'Betrayed',
                    mirror=True, alert=True)
    signal(img, W // 2, 120, (255, 90, 90), alert=True)
    # the broken promise between them
    t1, t2 = 'Promise broken:', '"No attacks on the Bastion\'s home"'
    d.text((W / 2 - d.textlength(t1, font=font(FC, 15)) / 2, 262), t1, font=font(FC, 15), fill=(255, 150, 150))
    d.text((W / 2 - d.textlength(t2, font=font(FR, 15)) / 2, 284), t2, font=font(FR, 15), fill=(240, 220, 220))
    speech_box(img, (40, 470, W // 2 - 10, 586), 'MUAAT', (255, 120, 30),
               'Your walls held through a hundred wars, Bastion. Let\'s see how they hold when they\'re molten.', alert=True)
    speech_box(img, (W // 2 + 10, 560, W - 40, 676), 'LAST BASTION', (90, 220, 255),
               'Muaat fire inbound! Seal the bulkheads, all hands to stations! We\'ve outlived worse than a tantrum!', alert=True)
    img.convert('RGB').save(out)


if __name__ == '__main__':
    negotiation(sys.argv[1])
    betrayal(sys.argv[2])


LAZ = '/home/claude/ti4-miyoo/assets/fonts/Lazaxian-Bold.ttf'


def mixed_line(d, x, y, parts, size=20, col=(236, 238, 250), lazcol=(255, 90, 90)):
    """parts: list of (text, kind) - kind 'n' normal, 'm' machine noise (mono), 'l' Lazaxian glyphs"""
    fn, fm, fl = font(FR, size), font(FM, size - 2), ImageFont.truetype(LAZ, size)
    for text, kind in parts:
        f = fn if kind == 'n' else fm if kind == 'm' else fl
        c = col if kind == 'n' else (140, 160, 190) if kind == 'm' else lazcol
        d.text((x, y), text, font=f, fill=c)
        x += d.textlength(text, font=f)
    return x


def line_samples(out):
    img = Image.new('RGBA', (W, 250))
    background(img)
    d = ImageDraw.Draw(img)
    b1 = (40, 20, W - 40, 112)
    panel(img, b1, fill=(8, 6, 22, 235))
    glow_rect(img, b1, (255, 60, 60), width=2, rad=10, glow=5)
    d = ImageDraw.Draw(img)
    d.text((58, 30), 'L1Z1X MINDNET', font=font(FC, 18), fill=(255, 60, 60))
    mixed_line(d, 58, 62, [('zz8', 'm'), ('QX', 'l'), ('19d2', 'm'), ('V', 'l'), ('… ', 'm'),
                           ('Another subjugated pawn of the empire comes to grovel. ', 'n'),
                           ('…23', 'm'), ('KRZ', 'l'), ('zZZz', 'm'), ('T', 'l'), ('t19', 'm')], size=19)
    b2 = (40, 134, W - 40, 226)
    panel(img, b2, fill=(8, 6, 22, 235))
    glow_rect(img, b2, (120, 255, 120), width=2, rad=10, glow=5)
    d = ImageDraw.Draw(img)
    d.text((58, 144), 'NEKRO VIRUS', font=font(FC, 18), fill=(120, 255, 120))
    mixed_line(d, 58, 176, [('7aa', 'm'), ('SX', 'l'), ('9f5a8', 'm'), ('…', 'm'), ('Kill', 'n'), ('…19d', 'm'), ('VG', 'l'),
                            ('6s9s56', 'm'), ('Q', 'l'), ('… ', 'm'), ('Life', 'n'), ('…39D', 'm'), ('FR', 'l'), ('0ds0a', 'm')],
               size=19, lazcol=(120, 255, 120))
    img.convert('RGB').save(out)


if __name__ == '__main__':
    line_samples(sys.argv[3] if len(sys.argv) > 3 else 'samples.png')
