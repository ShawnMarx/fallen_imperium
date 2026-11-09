# Fallen Imperium - codec portrait style guide

Portraits for the in-game negotiation "comms codec" (a space version of the Metal Gear Solid codec, full
colour, Retrowave). One 64x64 pixel-art bust per faction, drawn IN CODE with the kit in `pix.py`, animated as
a 24-frame loop. The owner approved the look of `nivyn2` and `mahact2` in `faces.py` ("I love it") - that is
the standard every portrait must meet. Study, before drawing anything:

- `faces.py` -> `nivyn2(t)` and `mahact2(t)` (code), `sheet4.png` (both, still), `frames_nivyn.png` /
  `frames_mahact.png` (4 frames of the loop), `anim_nivyn.gif` / `anim_mahact.gif`.
- `codec_neg.png` - how they sit in the game's negotiation screen (shown at 4x, 256 px, behind CRT scanlines).
- `pix.py` - the kit (read it all; it is short).

## The look
- Bust: head and shoulders filling the frame, head large (the face/helm takes roughly the upper 2/3), shoulders
  run off the bottom edge. A strong, readable silhouette at 64 px - a viewer must recognise the species at a
  glance. Frontal, or 3/4 turned toward the viewer's RIGHT (the game mirrors the right-hand speaker).
- Shapes are `p.fill(mask, ramp(...))`: inflated forms lit from the upper left, posterised into 5-step
  hue-shifted ramps (cool shadows, warm lights), dark contour where shapes overlap. Add detail with
  `p.stroke` (engraved lines, folds, seams), `p.pixels` (highlights, specks), `p.glow` (eyes, gems, lights),
  `texture=` for skin / stone / fabric grain.
- Finish with `p.rim(col, side=1)` - a neon rim light in a faction-appropriate colour on the right edge (and
  optionally a weaker warm/cool one on the left, side=-1) - then `p.outline(dark)`.
- Set `p.backdrop = (r, g, b)`: a DARK colour behind the portrait that suits the faction (Mahact dark red
  (70,12,14), Nivyn near-black (8,10,16)). Keep it dark (each channel <= ~80) so the figure pops.
- Full, saturated Retrowave colour: neon accents (cyan, magenta, violet, amber) on darker forms. Glowing eyes
  and lights read best. Avoid muddy mid-greys everywhere - give every portrait one or two accent colours.
- Expressions: the owner defines each faction's emotion (2026-11-08). Until he does, keep faces NEUTRAL - no
  smiles, no frowns, level brows, a plain straight mouth (Letnev and Celdauri: neutral).
- Round 2 (owner, 2026-11-08): portraits are redrawn two at a time from HIS pictures (folder
  the owner's "Use for Faction Portrait" folder), closely following them
  (pose, costume, colours), in faces_v2.py. Humans look like regular humans.
- Never copy or trace a reference picture. The references show what the species looks like (anatomy,
  costume, colours); draw your own pixel portrait of that species.

## Animation (required)
Every portrait function is `def key(t=0.0):` with t = 0..1 over the loop (24 frames x 125 ms = 3 s).
The loop must be seamless (t=0 and t=1 look the same: use the helpers, which are periodic).
Small, slow, living movement - the codec feel, never cartoon bouncing:
- `breath(t)`: whole-pixel rise of the chest/shoulders (draw them inside `with p.at(0, breath(t)):`).
- head follows a beat later: `with p.at(0, breath(t, -0.1)):` around the head parts.
- `sway(t, phase, amp, cycles)` for loose parts (tendrils, cloth, hair, leaves, antennae, flames, fins).
- `pulse(t, phase, cycles)` 0..1 for glow strength / colour (`mix(c1, c2, k)`, `p.glow(..., strength=)`).
- `blink(t, at, frames)` for an eye blink, a light flicker, a glitch frame.
- particles after the outline (sparks, embers, spores, bubbles, motes) drifting on `(t * n + phase) % 1`.
Movement stays within 1-2 px. Body polygons must extend below the frame (y = 66) so a breath never opens a gap
at the bottom edge. Pick 2-4 motions per portrait that suit the species (a fish-scientist's bubbles, a fire
being's flicker, a machine's scanning eye, a hivemind's swaying leaves...).

## Files and checks
- Write your portraits in your OWN file (named in your task), e.g. `faces_b1.py`, starting with
  `import sys; sys.path.insert(0, '<this folder>')` then `from pix import *` (or the names you need).
  Do NOT edit `pix.py`, `faces.py`, `codec.py` or other agents' files; put any helper you need in your file.
- For review, write `sheet_<yourfile>.png`: every portrait at 4x on its backdrop, frames t=0 and t=0.5 side
  by side, the faction key under each (PIL text). And `anim_<key>.gif` per portrait via
  `pix.save_gif(fn, path, scale=4)`.
- LOOK at your sheet (Read the PNG) and iterate until each portrait reads clearly and matches the standard of
  `sheet4.png`. Check a few frames for glitches (gaps, parts tearing apart, outline holes). Expect several
  passes per portrait; quality matters more than speed.
- Run python with plain `python3` from this folder (numpy, scipy, PIL are installed).

## Round 2 decisions (owner)
- Kept from the first round: cymiae (faces_b4.cymiae - "keep the first try Cymiae design").
- Approved round 2: arborec, axis, letnev (front-on, neutral), celdauri (neutral), saar (neutral, shaggy side fur).
- Florzen: space jellyfish, NO face (no eye-like lights). Muaat: a fire being inside a forge-like metal egg; the face
  is a solid cast-metal mask (not a real face); the orange fire is its real body, seen only through the portholes.
- Ghosts of Creuss: the blue aetheric energy IS the creature; the metal suit only houses it; streams leave through
  ports in the suit. Mirveda: a pale-white human in a suit joined to the headpiece. Hacan: front-on, thinner cowl.
- Naalu: mythological naga with a web-array fan as part of the head; pompous - grins while looking down at the viewer.
  Zelian: a sentient asteroid - a giant space rock in a storm of smaller orbiting rocks. Mentak: a human woman with
  cybernetic implants (pirate liberties OK). Tnelis: the seated boss (straight-faced) and two enforcers.
  L1Z1X: cybernetic Lazax, keep the first design's glitches, the headdress is a big dome (not a hat). Mortheus: a nebula.
- Nekro: pure AI machine horror - a jagged spiked drone, limbs = talons / a mechanical hand / a red sensor eye; the
  central eye flashes as if firing (no laser drawn); glitches encouraged. Berserkers (kjalengard): futuristic lizard
  warriors, dark blue / red space armour.
- Sardakk: an evil bug - a flat dish head with black dot eyes, a red face with hooked mandibles beneath.
  Monks of Kolume: robed AI robots surrounded by smoke (smoke() helper in faces_v2.py).
- Winnu: tall slender pale-gold humanoids, regal. Jol-Nar: aquatic aliens in a glass dome on a mechanical spider walker.
- Yin: grey humans with greyfire (space leprosy: burn-victim scarring). Xxcha: turtle-like, here in a high-tech tank suit.
- Xxcha: front-on (owner: "make it look straight"). Yin: keep as approved (eyes as first drawn).
- Mahact: regal, emotionless; the face is a cosmic void; broad prominent crest fins. Empyrean: slender bat-like
  beings in robes, no face.
- Naaz-Rokha: a panther-like alien with a small monkey-like Rokha on its back. Nomad: a robed figure, armoured
  space suit under the robes, a mask hiding the face.
- Titans of Ul: large humanoid robots, silver body, purple glow pulsing at the seams and the central eye.
  Crimson Rebellion: like the Creuss but chaotic - the metal body falling apart, red energy bursting out.
- Last Bastion: a soldier in sleek silver-white armour, a blue-visored helmet. Deepwrought: the Jol-Nar's own species,
  living in water (orange striped head, tendrils, fins).
- Firmament / Obsidian (two factions, one group): the Firmament = a council of five robed beings of different races (a bird,
  a toad, the fan-crested one in front, a salamander, a cat) facing the screen before a great star map in a golden hall;
  neutral. The Obsidian = the same council possessed by eldritch things from beyond the void - EVIL: near-black, glowing
  slanted eyes, burning cracks, tentacles, a black void orb wreathed in fire. Shared drawing: faces_v2._council(evil).
- Ral Nel: a gecko-like alien, head turned right: yellow-green bumpy head, dark bristle mane and jaw fringe, big orange
  slit-pupil eye, long pink-lipped mouth (closed, neutral); blue jumpsuit with a teal front panel, belt, arm bracer.
- 2026-11-08 23:20: owner "Then I think we should be finished" - all 65 portraits final (the 6 Thunder's Edge ones
  redrawn after the first approval: lastbastion, deepwrought, firmament, obsidian, ralnel; crimson earlier).
