# Fallen Imperium

An unofficial fan-made digital version of **Twilight Imperium 4th Edition** for Windows and the Steam Deck
(Linux), played against computer opponents (3 to 8 players, or hot-seat with friends).

**Current version: 0.58.2 (Pre-Beta).** Downloads are on the [Releases](../../releases) page.

> Unofficial fan project. Twilight Imperium is a trademark of Fantasy Flight Games; this project is not
> affiliated with or endorsed by Fantasy Flight Games or Asmodee. It is free and not for sale.

## What's in it
- The base game and **Prophecy of Kings**, with **Codex**, **Uncharted Space**, **Thunder's Edge** (incl. the
  Fracture, breakthroughs and galactic events) and **Discordant Stars** as modules you switch on.
- All factions: the 24 of the base game and Prophecy of Kings, the Council Keleres, the Thunder's Edge factions
  and the 34 Discordant Stars factions, with their abilities, leaders, technologies, units and promissory notes.
- Computer players with personalities (archetypes, grudges, betrayals), a strategy core per faction, plans over
  several turns, and four difficulty levels.
- Negotiations: trades, promises, loans, threats and vote deals - offered over a Metal Gear-style comms codec,
  with an animated portrait and a voice for every faction.
- Three tilesets: **Retrowave** (animated pixel art, the default), **Voidfall** (painted, high definition) and
  **Simplistic** (clean drawn shapes); animated space and ground battles; galaxy drafting; mouse, keyboard and
  controller support.

## Playing
- **Windows:** unzip `TI4-Windows-<version>.zip` and run `TwilightImperium.exe`.
- **Steam Deck / Linux:** unpack `TI4-SteamDeck-<version>.tar.gz` and run `TwilightImperium.sh` (on the Deck you
  can add it to Steam as a non-Steam game). See `README.txt` inside the download.
- `GUIDE.txt` (in the download, and `package/README.txt` here) explains the controls and lists what is new in
  each version.

The downloads include the soundtrack by **Michael Keane** (see Credits). The music files are not in this source
repository; a build from source plays without music unless you put the MP3s next to the game (any MP3 named
`music.mp3` works too).

## Building from source
Linux (x86-64) with a C++17 compiler and Python 3 (numpy, scipy, Pillow for the art tools):

```sh
make host            # the PC build for testing: build/host/ti4
make test            # automated screen tests and simulated games (screenshots in build/test)
make windows-toolchain && make windows-package   # dist/TI4-Windows-<version>.zip
make linux-package                                 # dist/TI4-SteamDeck-<version>.tar.gz
```

`ti4 --simulate N` plays N computer-only games and checks every answer against the rules.

## Credits and licences of included parts
- Music by **Michael Keane** - *The Council Keleres* and *Infiltration* (Embers of the Imperium), *Action*
  (Twilight Symposium). The composer made them for public use and asks only for credit; he is also named on
  the game's Credits screen.
- Game data (tiles, planets, units, factions, objectives) generated from the
  [AsyncTI4](https://github.com/AsyncTI4/TI4_map_generator_bot) bot's data set.
- Discordant Stars factions: the Discordant Stars fan expansion and its authors.
- DejaVu fonts (Bitstream Vera licence); the Lazaxian font supplied by the project owner.
- [stb](https://github.com/nothings/stb) image / truetype (public domain), [minimp3](https://github.com/lieff/minimp3)
  (CC0), SDL2 headers (zlib licence, `third_party/SDL2`), SDL 1.2 headers (LGPL 2.1, `third_party/SDL12`).
- Card wording in the game is paraphrased; no rulebook or card text is reproduced.
