import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from pix import Portrait, ramp, hexrgb, breath, sway, pulse, blink, mix, wave  # noqa: E402


def mahact():
    p = Portrait()
    gold = ramp('#e0a82a', hue_shift=0.05)
    robe = ramp('#5a2580', spread=0.75)
    trim = ramp('#d39a24')
    skin = ramp('#b8a6c8', spread=0.6)
    gem = ramp('#ff5a1a')
    # tall crown blades behind the head
    for pts in ([(16, 20), (9, 1), (23, 13)], [(48, 20), (55, 1), (41, 13)], [(26, 10), (32, -2), (38, 10)],
                [(20, 14), (17, 3), (27, 9)], [(44, 14), (47, 3), (37, 9)]):
        p.fill(p.poly(pts), gold, radius=3)
    # the robe's great collar rises behind the head like wings
    p.fill(p.poly([(4, 40), (8, 22), (17, 30), (20, 48), (12, 52)]), robe, radius=4, bias=-0.1)
    p.fill(p.poly([(60, 40), (56, 22), (47, 30), (44, 48), (52, 52)]), robe, radius=4, bias=-0.2)
    p.stroke([(8, 23), (17, 31), (20, 47)], trim[3])
    p.stroke([(56, 23), (47, 31), (44, 47)], trim[1])
    p.fill(p.poly([(0, 64), (3, 53), (16, 47), (32, 56), (48, 47), (61, 53), (64, 64)]), robe, radius=9, texture=0.05)
    p.fill(p.poly([(20, 48), (32, 60), (44, 48), (40, 47), (32, 54), (24, 47)]), gold, radius=2)
    p.fill(p.ellipse(32, 58.5, 3.2, 3.2), gem, radius=2)
    p.glow(31, 57, '#fff4c8', '#ffb060', r=0)
    # neck
    p.fill(p.poly([(27, 40), (37, 40), (36, 52), (32, 55), (28, 52)]), skin, radius=4, bias=-0.25)
    # long gaunt head with a narrow chin
    head = p.ellipse(32, 26, 12.5, 18) & ~p.poly([(17, 34), (27, 46), (17, 46)]) & ~p.poly([(47, 34), (37, 46), (47, 46)])
    p.fill(head, skin, radius=9, texture=0.07, seed=3)
    # sunken temples and cheeks
    p.stroke([(21, 30), (24, 36), (28, 41)], skin[1])
    p.stroke([(43, 30), (40, 36), (36, 41)], skin[0])
    p.stroke([(22, 28), (25, 29)], skin[3])
    p.stroke([(42, 28), (39, 29)], skin[2])
    # deep sockets under a heavy brow
    for cx, s0 in ((26.5, 0), (37.5, 0)):
        p.fill(p.ellipse(cx, 24.5, 4.6, 2.6), skin, flat=s0, edge=False)
    p.stroke([(21, 21), (25, 20), (30, 22)], skin[4])
    p.stroke([(34, 22), (39, 20), (43, 21)], skin[3])
    p.stroke([(30, 21), (31, 23)], skin[1])
    p.stroke([(34, 21), (33, 23)], skin[1])
    # slit eyes burning amber
    for x0 in (24, 35):
        p.pixels([(x0, 24), (x0 + 1, 24), (x0 + 2, 24), (x0 + 3, 24), (x0 + 4, 24)], '#ff9a12')
        p.pixels([(x0 + 1, 24), (x0 + 2, 24), (x0 + 3, 24)], '#ffe27a')
        p.px(x0 + 2, 24, '#ffffff')
        p.pixels([(x0 + 1, 25), (x0 + 3, 25)], '#8a3c06')
    # nose: a long blade
    p.stroke([(32, 24), (32, 32)], skin[4])
    p.stroke([(33, 25), (33, 32)], skin[1])
    p.pixels([(30, 33), (34, 33)], skin[1])
    p.px(31, 34, skin[0])
    # a thin, contemptuous mouth
    p.stroke([(27, 38), (30, 37), (34, 37), (37, 38)], '#2e1636')
    p.pixels([(27, 39), (37, 39)], skin[1])
    p.stroke([(30, 41), (34, 41)], skin[1])
    # circlet with the gene-gem
    p.fill(p.poly([(19, 14), (32, 10), (45, 14), (45, 16), (32, 13), (19, 16)]), gold, radius=2)
    p.fill(p.ellipse(32, 14.5, 2.8, 3.2), gem, radius=2)
    p.glow(32, 14, '#fff8dc', '#ffc860', r=0)
    # golden ear-cuffs
    p.fill(p.poly([(19, 21), (17, 26), (19, 31), (21, 27)]), gold, radius=2)
    p.fill(p.poly([(45, 21), (47, 26), (45, 31), (43, 27)]), gold, radius=2)
    p.rim('#ff3fae', side=1, amount=0.55)
    p.rim('#46d8ff', side=-1, amount=0.3)
    p.outline('#10081a')
    return p


def nivyn():
    p = Portrait()
    void = ramp('#2c2f86', spread=0.8)
    robe = ramp('#151a42', spread=0.9)
    silver = ramp('#c3cff0', spread=0.5)
    neb = ramp('#b24ad6', spread=0.75)
    neb2 = ramp('#4a7af0', spread=0.7)
    crys = ramp('#86ecff', spread=0.45)
    # nebula hair: a back mass and long locks falling to the shoulders
    p.fill(p.poly([(15, 26), (14, 12), (22, 5), (32, 3), (42, 5), (50, 12), (49, 26)]), neb, radius=6, texture=0.08, seed=5)
    p.fill(p.poly([(17, 18), (10, 34), (7, 52), (14, 50), (18, 40), (21, 30)]), neb, radius=4, texture=0.08, seed=6)
    p.fill(p.poly([(47, 18), (54, 34), (57, 52), (50, 50), (46, 40), (43, 30)]), neb, radius=4, bias=-0.1, texture=0.08, seed=7)
    p.fill(p.poly([(12, 30), (9, 44), (12, 46), (15, 36)]), neb2, radius=2)
    p.fill(p.poly([(52, 30), (55, 44), (52, 46), (49, 36)]), neb2, radius=2)
    for x, y in [(16, 14), (24, 7), (44, 9), (11, 40), (53, 38), (48, 16), (13, 25), (52, 46)]:
        p.px(x, y, '#ffffff')
    # floating crown of shards with a light between them
    for pts in ([(23, 7), (25, -2), (28, 7)], [(29, 6), (32, -4), (35, 6)], [(36, 7), (39, -2), (41, 7)], [(18, 9), (18, 1), (22, 8)],
                [(46, 9), (46, 1), (42, 8)]):
        p.fill(p.poly(pts), crys, radius=2)
    # robe and silver gorget
    p.fill(p.poly([(0, 64), (4, 54), (19, 47), (32, 51), (45, 47), (60, 54), (64, 64)]), robe, radius=10, texture=0.04)
    p.fill(p.poly([(17, 48), (32, 53), (47, 48), (43, 57), (32, 60), (21, 57)]), silver, radius=3)
    p.glow(32, 56, '#ffffff', '#9fdcff', r=1)
    p.fill(p.poly([(28, 38), (36, 38), (35, 50), (29, 50)]), void, radius=3, bias=-0.25)
    # head: high cheekbones, serene
    head = p.ellipse(32, 25, 11.5, 15.5) & ~p.poly([(19, 33), (28, 42), (19, 42)]) & ~p.poly([(45, 33), (36, 42), (45, 42)])
    p.fill(head, void, radius=8, texture=0.05, seed=9)
    p.stroke([(23, 29), (26, 33)], void[1])
    p.stroke([(41, 29), (38, 33)], void[0])
    p.pixels([(23, 27), (24, 28)], void[3])
    # stars in the skin
    for x, y in [(26, 18), (37, 17), (29, 32), (39, 31), (24, 26), (34, 13), (41, 24), (28, 36)]:
        p.px(x, y, '#e8f0ff')
    # brows of light, eyes of white fire
    p.stroke([(23, 21), (29, 21)], '#9fb8ff')
    p.stroke([(35, 21), (41, 21)], '#7f98e8')
    for cx in (26, 38):
        p.fill(p.ellipse(cx + 0.5, 24, 3.2, 1.7), void, flat=0, edge=False)
        p.pixels([(cx - 1, 24), (cx, 24), (cx + 1, 24), (cx + 2, 24)], '#ffffff')
        p.pixels([(cx - 2, 24), (cx + 3, 24)], '#8fb4ff')
    # nose ridge, lips
    p.stroke([(32, 24), (32, 30)], void[3])
    p.pixels([(31, 31), (33, 31)], void[1])
    p.stroke([(29, 35), (35, 35)], void[0])
    p.stroke([(30, 36), (34, 36)], void[3])
    p.glow(32, 17, '#ffffff', '#b8f0ff', r=0)
    # constellations on the robe
    for (x0, y0), (x1, y1) in [((6, 59), (11, 56)), ((11, 56), (15, 61)), ((50, 57), (55, 60)), ((55, 60), (59, 56))]:
        p.stroke([(x0, y0), (x1, y1)], '#5a6ac0')
    for x, y in [(6, 59), (11, 56), (15, 61), (50, 57), (55, 60), (59, 56)]:
        p.px(x, y, '#eaf2ff')
    p.rim('#46f0ff', side=1, amount=0.5)
    p.rim('#ff4fd2', side=-1, amount=0.3)
    p.outline('#06061a')
    return p


def bastion():
    p = Portrait()
    skin = ramp('#c48a60', spread=0.62)
    olive = ramp('#5e6a38', spread=0.75)
    steel = ramp('#808894', spread=0.62)
    stub = ramp('#5c463a', spread=0.5)
    # armour shoulders with hazard stripes and a unit number
    p.fill(p.poly([(0, 64), (3, 51), (15, 45), (26, 48), (38, 48), (49, 45), (61, 51), (64, 64)]), olive, radius=8, texture=0.06)
    p.fill(p.poly([(3, 52), (15, 46), (20, 50), (9, 59)]), steel, radius=3)
    p.fill(p.poly([(61, 52), (49, 46), (44, 50), (55, 59)]), steel, radius=3)
    for k in range(3):
        p.stroke([(5 + k * 3, 61), (9 + k * 3, 57)], '#e8c34a')
    p.pixels([(48, 57), (49, 57), (50, 57), (48, 58), (50, 58), (48, 59), (49, 59), (50, 59)], '#d8d8c8')  # "0"
    p.pixels([(52, 57), (53, 57), (54, 57), (54, 58), (53, 59)], '#d8d8c8')  # "7"
    # neck, collar
    p.fill(p.poly([(26, 38), (38, 38), (39, 49), (25, 49)]), skin, radius=4, bias=-0.25)
    p.fill(p.poly([(21, 46), (43, 46), (45, 52), (19, 52)]), olive, radius=2)
    # broad, square jaw
    head = p.poly([(19, 22), (21, 12), (32, 8), (43, 12), (45, 22), (45, 33), (40, 42), (32, 45), (24, 42), (19, 33)])
    p.fill(head, skin, radius=8, texture=0.08, seed=11)
    jaw = p.poly([(20, 31), (25, 41), (32, 44), (39, 41), (44, 31), (39, 35), (32, 37), (25, 35)])
    p.fill(jaw, stub, radius=2, bias=0.05, edge=False, texture=0.25, seed=12)
    # dented helmet, visor pushed up
    p.fill(p.poly([(16, 21), (17, 9), (25, 2), (39, 2), (47, 9), (48, 21), (42, 16), (32, 14), (22, 16)]), olive, radius=6, texture=0.06)
    p.fill(p.poly([(18, 10), (46, 10), (44, 15), (20, 15)]), steel, radius=2, bias=0.15)
    p.pixels([(29, 5), (30, 5), (30, 6), (36, 4)], olive[0])
    # forehead lines, heavy brows
    p.stroke([(26, 17), (30, 17)], skin[1])
    p.stroke([(33, 18), (38, 17)], skin[1])
    p.stroke([(22, 21), (30, 22)], stub[0], width=2)
    p.stroke([(34, 22), (42, 21)], stub[0], width=2)
    # the human eye, tired
    p.pixels([(24, 25), (25, 25), (26, 25), (27, 25), (28, 25)], '#efe4d4')
    p.pixels([(26, 25), (27, 25)], '#355777')
    p.px(26, 24, skin[1])
    p.stroke([(24, 27), (28, 27)], skin[1])
    # the cybernetic eye
    p.fill(p.ellipse(37.5, 25.5, 3.6, 3.0), steel, radius=2)
    p.glow(37, 25, '#ffeaea', '#ff2020', r=1)
    p.stroke([(41, 25), (44, 28)], '#3a3f46')
    # scar with stitches across the brow and cheek
    p.stroke([(23, 18), (30, 31)], '#8c3c32')
    for x, y in [(24, 21), (26, 24), (28, 27)]:
        p.px(x - 1, y, '#d9a68a')
    # nose, hard mouth
    p.stroke([(32, 24), (31, 31)], skin[1])
    p.stroke([(33, 25), (33, 30)], skin[3])
    p.pixels([(30, 32), (33, 32)], skin[0])
    p.stroke([(28, 37), (36, 36)], '#4c2018')
    p.stroke([(27, 34), (28, 37)], skin[1])
    p.stroke([(37, 33), (36, 36)], skin[1])
    # headset and mic
    p.fill(p.ellipse(19.5, 27, 2.6, 3.6), steel, radius=2)
    p.stroke([(20, 30), (22, 36), (28, 39)], '#2a2f36')
    p.px(29, 39, '#d23a3a')
    p.rim('#ff8a2a', side=1, amount=0.55)
    p.rim('#5ad0ff', side=-1, amount=0.25)
    p.outline('#0b0c09')
    return p


def muaat():
    p = Portrait()
    rock = ramp('#4c3836', spread=0.6)
    suit = ramp('#3a3e4c', spread=0.72)
    fire = ramp('#ff6410', spread=0.5, hue_shift=0.1)
    yel = ramp('#ffd43a', spread=0.4)
    # a crown of flame, two layers
    p.fill(p.poly([(14, 28), (8, 8), (17, 15), (19, -1), (26, 10), (31, -4), (35, 8), (42, -1), (45, 13), (54, 5), (50, 29)]), fire,
           radius=3, bias=0.15, texture=0.12, seed=21)
    p.fill(p.poly([(19, 22), (18, 8), (24, 14), (29, 2), (33, 13), (39, 5), (42, 16), (46, 11), (45, 23)]), yel, radius=3, bias=0.2,
           texture=0.1, seed=22)
    # pressure suit with glowing vents
    p.fill(p.poly([(0, 64), (4, 51), (17, 45), (32, 49), (47, 45), (60, 51), (64, 64)]), suit, radius=9, texture=0.05)
    p.fill(p.poly([(18, 44), (46, 44), (49, 53), (15, 53)]), suit, radius=3, bias=0.1)
    for x in (21, 26, 38, 43):
        p.stroke([(x, 47), (x, 51)], fire[3])
        p.px(x, 49, '#fff0a0')
    p.stroke([(8, 57), (13, 53)], fire[2])
    p.stroke([(51, 53), (56, 57)], fire[2])
    # basalt head, heavy brow, wide jaw
    head = p.poly([(18, 25), (21, 14), (32, 10), (43, 14), (46, 25), (45, 35), (39, 43), (32, 46), (25, 43), (19, 35)])
    p.fill(head, rock, radius=8, texture=0.1, seed=23)
    # glowing cracks
    for seg in ([(22, 15), (26, 20), (24, 25)], [(41, 15), (37, 19), (40, 23)], [(21, 33), (26, 37), (24, 42)],
                [(43, 32), (39, 37), (41, 41)], [(31, 11), (32, 17), (30, 20)], [(35, 38), (37, 43)]):
        p.stroke(seg, fire[3])
    for x, y in [(26, 20), (37, 19), (26, 37), (39, 37)]:
        p.px(x, y, '#ffe680')
    # brow shadow, blazing eyes
    p.fill(p.poly([(20, 22), (31, 25), (33, 25), (44, 22), (44, 27), (20, 27)]), rock, flat=0, edge=False)
    for cx in (26, 38):
        p.pixels([(cx - 3, 26), (cx - 2, 26), (cx - 1, 26), (cx, 26), (cx + 1, 26), (cx + 2, 26)], fire[3])
        p.pixels([(cx - 2, 26), (cx - 1, 26), (cx, 26), (cx + 1, 26)], '#fff2a0')
        p.pixels([(cx - 1, 27), (cx, 27)], fire[2])
    # nostrils, a molten grin
    p.stroke([(31, 30), (32, 33), (33, 30)], rock[1])
    p.fill(p.poly([(25, 36), (39, 36), (37, 41), (27, 41)]), fire, flat=3)
    p.fill(p.poly([(27, 38), (37, 38), (36, 40), (28, 40)]), yel, flat=3)
    for x in (27, 29, 31, 33, 35, 37):
        p.px(x, 37, '#fff6c0')
    p.stroke([(25, 36), (39, 36)], rock[0])
    p.rim('#ffb21a', side=1, amount=0.6)
    p.rim('#ff3f2a', side=-1, amount=0.35)
    p.outline('#140707')
    return p


ALL = {'mahact': mahact, 'nivyn': nivyn, 'bastion': bastion, 'muaat': muaat}

if __name__ == '__main__':
    from PIL import Image
    out = sys.argv[1]
    sheet = Image.new('RGBA', (4 * 64 * 4 + 50, 64 * 4 + 20), (255, 255, 255, 255))
    for i, (k, f) in enumerate(ALL.items()):
        im = f().image(4)
        sheet.alpha_composite(im, (10 + i * (64 * 4 + 10), 10))
    sheet.save(out)


def mahact2(t=0.0):
    """after the owner's reference: a tall crested golden helm, a violet-white light in the seam of the mask.
    Moves: shoulders breathe, the helm follows a beat later, the light pulses, embers drift up."""
    p = Portrait()
    gold = ramp('#c9952e', spread=0.72, hue_shift=0.04)
    inlay = ramp('#3a3024', spread=0.55)
    silver = ramp('#c9ccd4', spread=0.5)
    dark = ramp('#1e1b18', spread=0.6)
    body, head = breath(t), breath(t, -0.1)
    glowk = pulse(t, cycles=2)
    with p.at(0, body):
        # shoulders: low silver plates in chevrons, gold edges
        p.fill(p.poly([(0, 66), (0, 53), (11, 47), (23, 51), (32, 58), (41, 51), (53, 47), (64, 53), (64, 66)]), silver,
               radius=6, texture=0.05)
        p.fill(p.poly([(0, 52), (4, 40), (10, 40), (12, 48)]), gold, radius=2)
        p.fill(p.poly([(64, 52), (60, 40), (54, 40), (52, 48)]), gold, radius=2, bias=-0.15)
        p.stroke([(1, 54), (11, 48), (23, 52), (32, 59), (41, 52), (53, 48), (63, 54)], gold[3], width=2)
        p.stroke([(6, 62), (15, 57), (24, 60), (32, 64), (40, 60), (49, 57), (58, 62)], gold[2])
        p.fill(p.poly([(14, 53), (22, 56), (18, 60)]), inlay, radius=1)
        p.fill(p.poly([(50, 53), (42, 56), (46, 60)]), inlay, radius=1)
        # neck: dark with gold scales
        p.fill(p.poly([(25, 42), (39, 42), (41, 54), (32, 58), (23, 54)]), dark, radius=4)
        for y in (46, 50, 54):
            for x in range(25, 40, 4):
                p.pixels([(x, y), (x + 1, y + 1), (x + 2, y)], gold[1])
    with p.at(0, head):
        # the crest: tall and narrow, with an inlaid spine
        p.fill(p.poly([(29, 24), (29, 5), (32, 0), (35, 5), (35, 24)]), gold, radius=3)
        p.fill(p.poly([(31, 6), (32, 4), (33, 6), (33, 20), (31, 20)]), inlay, radius=1)
        # fins sweeping up and out, three pairs
        for a, b, c, d in ([(28, 15), (22, 1), (25, 1), (30, 10)], [(25, 21), (13, 7), (17, 7), (28, 16)],
                           [(22, 29), (9, 21), (12, 19), (24, 24)]):
            p.fill(p.poly([a, b, c, d]), gold, radius=2)
            p.fill(p.poly([(64 - a[0], a[1]), (64 - b[0], b[1]), (64 - c[0], c[1]), (64 - d[0], d[1])]), gold, radius=2,
                   bias=-0.12)
        # the mask: two angular cheek plates meeting at the chin, a brow plate over them
        p.fill(p.poly([(20, 22), (31, 20), (31, 47), (26, 43), (21, 35)]), gold, radius=4, texture=0.04)
        p.fill(p.poly([(44, 22), (33, 20), (33, 47), (38, 43), (43, 35)]), gold, radius=4, bias=-0.12, texture=0.04)
        p.fill(p.poly([(23, 24), (29, 23), (29, 30), (24, 32)]), inlay, radius=2)
        p.fill(p.poly([(41, 24), (35, 23), (35, 30), (40, 32)]), inlay, radius=2)
        p.fill(p.poly([(24, 35), (29, 33), (29, 41)]), inlay, radius=1)
        p.fill(p.poly([(40, 35), (35, 33), (35, 41)]), inlay, radius=1)
        p.fill(p.poly([(19, 22), (32, 17), (45, 22), (44, 25), (32, 20), (20, 25)]), gold, radius=2, bias=0.15)
        p.stroke([(21, 36), (26, 43), (31, 47)], gold[4])
        p.stroke([(43, 36), (38, 43), (33, 47)], gold[1])
        # the seam and the light within it (it breathes too)
        p.fill(p.poly([(31, 21), (33, 21), (33, 47), (31, 47)]), dark, flat=0, edge=False)
        lo, hi = mix('#8a78e8', '#d8ccff', glowk), mix('#d8d0ff', '#ffffff', glowk)
        span = 2 + int(round(2 * glowk))
        for y in range(23 - span // 2, 42 + span // 2):
            p.px(32, y, hi if (y + int(t * 24)) % 3 else lo)
        for y in range(28 - span, 35 + span):
            p.px(31, y, mix('#5a40c0', '#9a80ff', glowk))
            p.px(33, y, mix('#5a40c0', '#9a80ff', glowk))
        p.glow(32, 30, '#ffffff', '#c0aaff', r=2, strength=0.6 + 0.4 * glowk)
        k = int(t * 4) % 4
        p.pixels([[(30, 29), (34, 31)], [(30, 31), (34, 29)], [(32, 26), (32, 35)], [(31, 27), (33, 33)]][k], '#e8e0ff')
    p.rim('#ff5a3a', side=1, amount=0.45)
    p.rim('#ff9a3a', side=-1, amount=0.25)
    p.outline('#140c08')
    # embers rising past the shoulders (after the outline: loose sparks, no edge)
    for i, (x0, ph) in enumerate(((6, 0.0), (17, 0.37), (47, 0.71), (58, 0.18), (39, 0.55))):
        u = (t * 2 + ph) % 1.0
        y = int(round(62 - u * 30))
        x = x0 + sway(t, ph, 1, cycles=2)
        if u < 0.85:
            p.px(x, y, mix('#ffb040', '#7a2010', u))
    p.backdrop = (70, 12, 14)
    return p


def nivyn2(t=0.0):
    """after the owner's reference: a raptor-helmed knight in gunmetal, gold scarf, cyan reactor light.
    Moves: armour breathes, the helm bobs a beat later, the reactor and the eye pulse, sparks rise."""
    p = Portrait()
    metal = ramp('#59606a', spread=0.78)
    dark = ramp('#2c3036', spread=0.7)
    gold = ramp('#c9a032', spread=0.6)
    scarf = ramp('#cfa628', spread=0.62, hue_shift=0.03)
    cyan = ramp('#3fe6e6', spread=0.4)
    body, head = breath(t), breath(t, -0.12)
    core = pulse(t, cycles=2)
    with p.at(0, head):
        # layered fin plates swept back from the helm
        for k, pts in enumerate(([(25, 12), (4, 5), (6, 10), (25, 17)], [(24, 17), (2, 14), (4, 19), (24, 22)],
                                 [(25, 22), (7, 23), (9, 27), (26, 26)])):
            with p.at(sway(t, 0.3 + 0.15 * k, 1) if k != 1 else 0, 0):
                p.fill(p.poly(pts), metal, radius=2, bias=-0.06 * k)
                a, b = pts[0], pts[1]
                p.stroke([(a[0] - 2, a[1] + 2), (b[0] + 3, b[1] + 2)], cyan[2] if k != 1 else gold[2])
        # the back horn and the cyan antenna
        p.fill(p.poly([(25, 11), (11, -1), (16, -1), (30, 8)]), metal, radius=2)
        p.fill(p.poly([(33, 8), (38, -2), (42, -1), (37, 9)]), cyan, radius=2)
        if blink(t, 0.1, 3) or blink(t, 0.6, 3):
            p.pixels([(39, 0), (40, 0), (39, 1)], '#e8ffff')
    with p.at(0, body):
        # body armour and gold trim
        p.fill(p.poly([(0, 66), (0, 37), (12, 32), (26, 36), (40, 44), (52, 49), (60, 66)]), dark, radius=7, texture=0.06)
        for k in range(3):
            p.stroke([(3, 40 + k * 3), (10, 38 + k * 3)], gold[2])
        # the shoulder and its reactor
        p.fill(p.ellipse(14, 48, 12.5, 11.5), metal, radius=6, texture=0.05)
        p.fill(p.ellipse(14, 48, 7.5, 7.5), cyan, radius=4, bias=0.1 + 0.3 * core)
        spoke = int(t * 12) % 3
        for a, b in (((10, 45), (18, 51)), ((14, 41), (13, 54)), ((8, 50), (20, 45)))[spoke:] + \
                (((10, 45), (18, 51)), ((14, 41), (13, 54)), ((8, 50), (20, 45)))[:spoke][:1]:
            p.stroke([a, b], mix('#9ff0f0', '#ffffff', core))
        p.glow(14, 48, '#ffffff', '#9ffcff', r=2, strength=0.55 + 0.45 * core)
        # gorget and the gold scarf in folds
        p.fill(p.poly([(26, 30), (40, 28), (44, 44), (30, 48)]), dark, radius=4)
        with p.at(sway(t, 0.2, 1), 0):
            p.fill(p.poly([(25, 32), (34, 27), (45, 29), (54, 40), (52, 52), (45, 56), (40, 47), (31, 41)]), scarf,
                   radius=5, texture=0.08)
            p.stroke([(33, 31), (44, 39), (47, 51)], scarf[1])
            p.stroke([(29, 35), (38, 42), (41, 49)], scarf[1])
            p.stroke([(40, 30), (50, 39)], scarf[4])
    with p.at(0, head):
        # the raptor helm
        p.fill(p.poly([(22, 20), (24, 10), (32, 5), (42, 6), (48, 11), (50, 18), (48, 26), (40, 32), (30, 33), (24, 28)]),
               metal, radius=7, texture=0.05)
        # the hooked beak
        p.fill(p.poly([(46, 12), (53, 15), (58, 20), (60, 27), (58, 34), (56, 33), (56, 28), (53, 24), (47, 23)]), metal,
               radius=3, bias=0.08)
        p.fill(p.poly([(46, 24), (53, 25), (54, 29), (48, 29)]), dark, radius=2)
        p.stroke([(47, 13), (54, 17), (58, 23)], metal[4])
        p.stroke([(56, 29), (57, 33)], metal[1])
        # engraved gold lines, a cyan seam
        p.stroke([(25, 15), (32, 9), (41, 9)], gold[3])
        p.stroke([(25, 22), (33, 18), (42, 19)], gold[2])
        p.stroke([(28, 27), (37, 26)], gold[1])
        p.stroke([(30, 31), (40, 29)], mix(cyan[1], cyan[3], core))
        # the burning eye (flickers now and then) and the sensor below it
        p.fill(p.ellipse(43.5, 16.5, 2.6, 2.2), dark, flat=0, edge=False)
        if blink(t, 0.78, 1):
            p.glow(43, 16, '#ffb040', '#a04010', r=1, strength=0.6)
        else:
            p.glow(43, 16, '#fff6c0', mix('#ff8a10', '#ffc040', pulse(t, 0.3, 3)), r=1)
        p.glow(46, 21, '#e8ffff', '#3fe6e6', r=0, strength=0.5 + 0.5 * core)
    p.rim('#3fe6e6', side=1, amount=0.45)
    p.rim('#ffb43a', side=-1, amount=0.2)
    p.outline('#07080a')
    # sparks from the reactor, drifting up
    for x0, ph in ((24, 0.0), (32, 0.4), (19, 0.75), (44, 0.2)):
        u = (t * 2 + ph) % 1.0
        if u < 0.8:
            p.px(x0 + sway(t, ph, 1, 2), int(round(63 - u * 22)), mix('#bffcff', '#1a6070', u))
    p.backdrop = (8, 10, 16)
    return p
