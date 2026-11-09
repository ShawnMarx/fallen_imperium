FALLEN IMPERIUM - Steam Deck / Linux version
(an unofficial fan project of Twilight Imperium)
Version 0.57.2 - Pre-Beta (what is new in each version: GUIDE.txt)

The same game as the Miyoo Mini Plus version (Fourth Edition + Prophecy of
Kings, Codex, Uncharted Space, Thunder's Edge and Discordant Stars, 3 to 8
players against the computer, every faction). Unofficial fan
project. GUIDE.txt is the full guide (written for the Miyoo - the button
names are the same on the Deck, except: START = View, SELECT = click a stick).

NEW IN 0.57.2
  - Retrowave (the animated pixel art) is now the tileset a new install
    starts in. Tilesets on the title screen goes round in the order
    Retrowave, Voidfall, Simplistic. (A tileset you chose before is kept.)
  - With Cards set to timed, the betrayal codec stays up until both
    lines have typed out.

NEW IN 0.57.1 - your tester's reports
  - Fixed: the Xxcha's Peace Accords offered Thunder's Edge space stations
    as if they were empty planets (checked over 40 games: 15 of 31 Peace
    Accords choices included a station; now none). A station is only
    taken by being the one player with ships in its system. The Ghemina
    Lord's hero (Kantrus) had the same slip and is fixed too.
  - Fixed: with Thunder's Edge on, the Xxcha hero was still the Codex
    version (every planet the Xxcha spend paying its resources AND its
    influence - what your tester saw; it wasn't the breakthrough).
    Thunder's Edge gives the Xxcha a new hero: Xxekir Grom, ACTION: place
    up to 4 PDS or mechs, in any mix, on planets you control, ready each
    planet you put one on, then purge him. That is the hero now in
    Thunder's Edge games (the Codex one stays for games without it). The
    breakthrough, Archon's Gift, is unchanged: each planet you spend counts
    its resources or its influence, whichever you need.
  - The map: a flag in its holder's colour stands on every controlled
    planet (from the second zoom level), and mechs are drawn standing on
    their planets (from the third zoom level; the number badge then counts
    the infantry and structures).
  - Both fixes apply to games started with 0.57.1; older saves play on
    exactly as before.

NEW IN 0.57.0 - the comms codec: every faction talks
  - When a computer player offers you a deal, the offer arrives over a
    Metal Gear-style comms codec in front of a space backdrop: their
    animated portrait on the left (breathing, swaying, glowing - the 65
    pictures you approved), yours on the right, the subspace signal
    jumping while they talk, and their words typing out in their own
    voice. The deal itself (what each side gives, the promises, a
    threat) sits underneath with Accept / Counter / Decline as before.
  - Every faction has its own voice, from your gist notes: offers, an
    offer to a friend, threats, vote deals, accepting, refusing,
    counter-offers, and the betrayal exchange (the betrayer's words, then
    the victim's answer, naming each other). About 900 lines, plus lines
    one faction says only to another (Sol and the Barony, the Hacan and
    the Mentak, the Ral Nel and the Hacan, the Jol-Nar and their
    Deepwrought cousins, the Creuss and the Crimson, the Keleres and the
    pirates and the Syndicate, the Mahact and the heirs of the Lazax...),
    used first, with the faction's usual lines as the second choice.
  - Ways of speaking: the L1Z1X's words come wrapped in machine junk
    with Lazaxian glyphs at both ends; the Nekro say only two readable
    words buried in junk; the Obsidian speak entirely in the Lazaxian
    alphabet (your font); the Glimmer of Mortheus fades in and out; the
    Ghemina's Lord starts a sentence and the Lady finishes it; the
    Arborec's words sway like leaves; the Creuss and the Crimson whisper;
    the Zelian's growl grinds before the [translation].
  - Deal banners (right of the map) became a comms strip: the speaking
    faction's portrait and line - an offer, accepting, refusing, a
    counter-offer - over the deal's summary.
  - Betrayals: when a computer player turns on someone, the red betrayal
    codec opens - "BETRAYAL!", the channel lost, both portraits, the
    betrayer's words and then the victim's answer. A promise broken to
    you (or by you) now opens it too, and waits for A like a betrayal
    (at Instant speed you get the short banner as before).
  - Display only: the lines are chosen from the game's seed, never from
    the dice, so games and older saves play on exactly as before.

NEW IN 0.56.0 - Waypoint Mode: choose your ships' route
  - When you move ships, a crosshair button sits at the bottom right of
    the map, just above the game log (pointing at it says "Waypoint
    Mode"). Click it, or press X, to plot the route on the map yourself
    instead of letting the game choose it:
      1. If ships move from more than one system, first pick the system
         whose ships you are routing (highlighted).
      2. The systems the ships can step into next are highlighted: click
         them (or move the cursor and press A) one by one. A numbered line
         shows the route; a dashed red ring marks a gravity rift.
      3. Click the system you are moving into to finish (or press
         START). B takes the last stop back; X goes back to the automatic
         route.
    The move list comes back with the route listed under it. Ground
    forces and fighters waiting at a stop can now be picked up there -
    including inside a gravity rift (your RULES-CHECK M2 question).
  - A route can go through a gravity rift on purpose: each rift gives
    +1 move, and each ship rolls a die for every rift it leaves (1-3 and
    it is lost, with what it carries). The game shows how many rolls the
    route needs before you confirm.
  - Without Waypoint Mode nothing changes: the game picks the route as
    before (rift-free whenever it can). The computer players keep using
    the automatic routes. Older saves play on exactly as before.
  - Tested: thousands of computer moves given explicit routes (through
    rifts too), each checked, played and replayed from the save.

NEW IN 0.55.0 - the final bot pass
  - Stop the winner: every bot works out, from what anyone at the table
    can see (points, the public objectives each player meets right now,
    Imperial in hand with Mecatol Rex, unscored secrets), who could reach
    the winning score at the coming status phase. It then tries taking
    each of that player's planets and fleets away and sees which ones
    would break the win - those become worth a great deal to attack, and
    Imperial is taken just to keep it from them.
  - The last round: when someone will very likely win at this round's
    scoring, nothing is kept for later - the bots fight at shorter odds,
    play the cards they were saving, use every tactical action and stop
    gathering for next round.
  - Cards and abilities the bots never used (found by a report over 140
    games with everything on): the Dominus Orb is now used when it opens
    a real move, the Crimson Rebellion's hero (Homesick Phantom) stores
    new ships, the Mahact breakthrough is played more readily, and The
    Silver Flame is gambled on when a bot can't otherwise win. Many other
    "never" answers are deliberate (bad trades) and stay.
  - Fixed: the Nomad's agent Field Marshal Mercer was never used by the
    bots (they looked for the system after the tactical action had
    ended). Now they use him to defend a planet or to bring spare troops
    to the front for free.
  - Fixed: in rare votes with no "abstain" choice (Special Session and
    similar) a bot gave an answer the game refused; it now always picks
    one of the outcomes on offer.
  - The Creuss and the Crimson Rebellion (homes beside the board): the
    bots now treat the systems round their gate / the Sorrow as their
    home slice, as players do (before, they saw those planets as far
    away and valued them at a quarter).
  - Rules fixes found by the big test run (only in new games): a ship the
    Vuil'raith capture can no longer also be rebuilt at home by the
    Crimson's Subatomic Splicer (the same carrier ended up in two
    places), and ships waiting on the Homesick Phantom card count toward
    the Crimson's unit limits. See RULES-CHECK N1-N2.
  - Tested: over two thousand computer games over every player count, module
    mix, difficulty and galactic event; scripted "human habit" players
    (a promise breaker, a Mecatol camper, a deal spammer, a turtle, an
    all-out attacker) - none of them could farm the bots: the breaker's
    promises are discounted once it is known for breaking them and it won
    no more than anyone else, the turtle lost ground. Older saves play
    on exactly as before.

NEW IN 0.54.0 - Mecatol Rex as king of the hill, bots that learn your habits
  - New Game has a "Mecatol Rex" row: Contested (the default) or Calm.
    Contested: the computer players fight over Mecatol Rex once the
    custodians are gone. They value it by Imperial's points - this
    round's point if its holder has Imperial and hasn't played it yet,
    the leader on it, their own Imperial in hand - not for the planet
    itself. They only attack with a clear win in sight (warlike
    personalities go in sooner, cautious ones later), first check
    whether a third player could take it from their battered fleet,
    and send only spare ships (home guards and the guards of Styx and
    floating factories stay). In the test games Mecatol Rex changed
    hands more often and the fights there went up by about half,
    without the bots getting weaker. Calm: they only take it when its
    holder leaves it open (as in 0.53.0).
  - Planning ahead, continued: bots gather before fighting a big fleet
    (4 or more ships) next to their home, and build what their goal
    still lacks at docks near it (warships for the fight, infantry and a
    carrier for Styx's landing) - after the ships and troops their
    expansion needs.
  - Bots learn your habits: from your earlier games they remember how
    often you attack, raid home systems, sit on Mecatol Rex and break or
    keep promises. A known promise breaker gets fewer deals, a raider's
    neighbours keep a ship more at home, someone who sits on Mecatol Rex
    is contested harder. The profile is fixed when a game starts (saved
    with it). Options > "Bots learn your habits" switches it on or off
    and shows how many games they remember.
  - Fixed (tester): retreating ships may now take ground forces from
    your planets in the system along (the rules' FAQ), as many as they
    have room for - you choose how many.
  - Fixed (tester): a Vuil'raith carrier passing through its own home
    (a Dimensional Tear - a gravity rift they don't roll for) can now pick
    up units there on the way.
  - Checked (tester): the Dimensional Tears' production is fixed - 5,
    and 7 with Dimensional Tear II - whatever planet they are on; the
    game already works that way (at the Vuil'raith home it is 5, not
    5 + Acheron's resources).

NEW IN 0.53.0 - bots that plan ahead (first step)
  - The computer players can now work toward a goal over several turns
    and rounds instead of looking at one turn only: they pick a target out
    of this turn's reach, gather their fleet (and carriers with ground
    forces) closer to it turn by turn, and strike when the gathered fleet
    is strong enough. Ships already gathered make the bot stick to the
    same goal, and moving them away for something else has to be worth it.
  - Targets: Styx (Thunder's Edge) - before this no bot ever attacked its
    neutral dreadnoughts; in 40 test games it was attacked 5 times and
    taken and held in about 1 of 9 games where it was on the board - and
    another player's home system when a bot has a reason (Kingslay, a
    secret objective won there, an objective that wants a planet there).
  - Retaking Mecatol Rex from a player's fleet was tried as well and made
    the bots weaker in the test games (both fleets wear down and a third
    player takes the prize), so that one stays off; bots still take Mecatol
    Rex when its holder leaves it open, as before.
  - Easy bots don't do this; Normal ones less than Hard and Competitive.

NEW IN 0.52.2
  - Campaigns: Trade Wars has a new picture (a ground war seen from the mouth
    of a cave).

NEW IN 0.52.1 - the Celagrom
  - The Celdauri's second flagship from their Trade Protectorate
    breakthrough, the Celagrom, is its own unit now with its own card:
    cost 5, move 1, capacity 2, combat 5, Bombardment 5, Sustain Damage.
    At the start of a space combat it may produce 1 hit on one of the
    opponent's ships (you pick the type); it rolls 1 more die while the
    Supremacy is on the board. It was a copy of the Supremacy before. If
    it is destroyed it can be built again. Games saved before 0.52.1 keep
    the old way.

NEW IN 0.52.0 - the Lady and the Lord, ships on the map
  - The Ghemina Raiders' two flagships are two separate units now: The Lord
    (capacity 7; explorers in its system gain an infantry) and The Lady
    (capacity 3, Bombardment 5 x4; a trade good when someone else's
    structure in her system is destroyed). You build, move and lose each
    one by name - no more "which flagship moved / was built / was
    destroyed?" questions. Both count as flagships for everything that
    looks for one (objectives, secrets, the heroes, Rapid Mobilization).
    Games saved before 0.52.0 keep playing the old way.
  - Fixed: the Ghemina heroes Korela and Kantrus were offered even when
    there was no planet they could act on (picking one then did nothing).
  - Ships are drawn on the map tiles from the second zoom level: a small
    picture of each ship along the bottom of its tile (biggest first),
    fighters as one with their count, "+N" for ground forces carried and
    for ships that don't fit. The farthest zoom keeps the numbers.

NEW IN 0.51.1 - painted Voidfall troops, bigger armies
  - Voidfall ground combat: every unit on the ground is painted the way the
    Voidfall tiles are lit - a warm light from the upper left, the lower
    right in violet shadow, a thin edge of violet light along the outline
    (the void glowing behind them) and soft violet shadows under their feet.
    The default soldier has a glossy shaded helmet, a glowing visor (a small
    helmet light from behind), a glowing power cell on his rifle and a soft
    flash when he fires; the default mech gets a glowing visor and chest
    core. Every faction's own troops, mechs, PDS, space docks, fighters and
    the fallen get the same light. Team colours are a little brighter.
  - Voidfall only: each infantry is 10 soldiers instead of 6, and they are
    spread over their side of the battlefield instead of standing in knots.
    (Mortheus stays one nebula per infantry.)
  - Simplistic and Retrowave battles are unchanged.

NEW IN 0.50.0 - pixel-art faction troops
  - In Retrowave ground combat every faction's own troops are proper pixel
    art now: the Arborec plants and their tree mech, the Sardakk bugs and
    beetle, the Florzen jellyfish, the Ghoti fish, the Vuil'raith horrors,
    the Nekro drones, Mortheus nebulae, Zelian rocks, Cheiran crabs, Khrask
    golems, Titans and Roh'Dhna robots and the rest - crisp outlines,
    posterised colours, see-through glows and shadows, animated in eight
    frames a second (stride, wings, tentacles, pulsing lights), and their
    fallen too. Creuss / Mahact seams and the Vuil'raith growths on the
    mechs, and the Naalu crystal fighters, are on the same pixel grid.

NEW IN 0.49.0 - full-detail Retrowave battles
  - Every ship in a Retrowave battle is real pixel art, made from the unit
    designs: a crisp outline, the hull shaded in six tones of the player's
    colour, a stripe of the full colour along its spine, lit windows that
    blink and engine flames that flicker.
  - Damage you can see: a hit a ship can take makes its shield flare and
    leaves a burning breach; explosions boil out as fireballs that break up
    into smoke, sparks fly, big ships go with a shock ring, and a war sun's
    end is a blinding flash that hollows out into a ring.
  - Ground combat: mechs, PDS and space docks are pixel art too, with the
    same explosions, sparks and smoke. The expanded tile shows the same
    pixel-art ships.
  - Simplistic and Voidfall battles are unchanged.

NEW IN 0.48.0 - sharper Retrowave
  - On the PC and the Steam Deck every Retrowave tile, planet, wormhole and
    token is drawn on a pixel grid twice as fine: the same designs and
    colours, still pixel art, but planets, rocks, wormholes and their
    letters are much more defined (Mecatol Rex shows its ruined avenues,
    stars come in two sizes). Options > Picture > Original 640x480 keeps
    the old art.
  - The expanded tile, its ships and the battles use the finer pixels too
    (the full-detail pixel-art battles are the next build).
  - The map's tiles take turns animating, so the closest zoom no longer
    hitches every tenth of a second.

NEW IN 0.47.0 - the SCREENS panel on the map
  - Once you use the mouse, a small SCREENS panel sits on the map with
    buttons for Objectives, Laws, Your cards, Faction sheet, Units,
    Technology, Planets, Players and the Game log. Each one opens that
    screen as a window over the map, so the galaxy stays in view.
  - Drag the panel by its top strip; "-" folds it to a small tab and "+"
    opens it again (it starts folded on a 4:3 picture). Where it sits and
    whether it is folded is remembered.
  - Drag a window by its title strip. Close it with its x, its button
    again, B / Esc or a right click; another button swaps the screen in
    it. Beside the window the wheel still zooms the map and a right-button
    drag moves it; the keys go to the window.
  - With a controller only nothing changes: MENU has the same screens.

NEW IN 0.46.2
  - Galactic Events button on the top bar (when events are in play): click
    it for the list, point at an event to read what it does.
  - Retrowave New Game screen: more rifts, and far off on the right a war
    sun firing missiles at a Vuil'raith void dragon that rises from below
    and sinks back.
  - The 1s-and-0s game start covers the whole widescreen.
  - Simplistic draws its own Fracture and Thunder's Edge tiles again (the
    painted skies stay in Voidfall and Retrowave).

NEW IN 0.46.1
  - The fleet pool and ship capacity are checked at all times: whenever a
    system holds more ships than your fleet pool allows, or more fighters /
    ground forces than your ships can carry, you choose what to remove -
    whatever caused it (the Roh'Dhna turning a space dock into a war sun,
    a hero or agent placing ships, a fleet token lost, a space dock
    destroyed by a card...). Not while ships are moving, during an
    invasion's landing or in a space combat (checked when those end).
  - Industrious (Roh'Dhna) warns when the fleet pool is already full.
  - An extra action (Fleet Logistics, Thunder's Edge, Master Plan, Minister
    of War) can be used to Pass - shown apart from End turn.
  - Proxima Targeting VI says its dice are the technology's own (no X-89
    doubling, no Plasma Scoring die).

NEW IN 0.46.0 - widescreen (the first Windows / Steam Deck edition step)
  - The picture takes your screen's shape: 16:9 on a PC monitor, 16:10 on
    the Steam Deck (a window takes whatever shape you give it). The galaxy
    map gets the extra room; the side panel sits at the right edge.
  - Menus, cards and other screens not widened yet stay centred, with their
    backgrounds carried out to the edges.
  - Options > Picture: Sharp widescreen (new), Sharp 4:3 (the old shape) or
    Original 640x480.
  - The Miyoo version stays at 0.45.0.

NEW IN 0.29.0
  - The mouse wheel scrolls the lists in the question panel (moving and
    landing units, paying, producing...) and the research cards.
  - Space confirms every editor (move, land, produce, pay, hits, tokens);
    Q / E (or the arrows) change the numbers; a click picks a row, a second
    click on it adds one (or switches a planet on / off).
  - Deal offers: clicking Accept / Decline / Counter-offer picks that
    button (a click used to answer with whichever was selected).
  - The Continue buttons on large cards show your key (SPACE) and can be
    clicked.
  - Make a bug report also writes game_log.txt: what happened in your saved
    game, turn by turn.

NEW IN 0.28.0
  - Options > Make a bug report: copies ti4.log, your save and settings into
    a folder called bug-report-<date>-<time> in the game's folder - send me
    that folder when something goes wrong.
  - Options > Control layout shows every key and button.
  - Keyboard: Space moves ships (Q / E fewer / more); the mouse works on the
    objectives board and the negotiations web; the camera no longer jumps
    when the mouse is over the side panel or the log.
  - Everything else new in the game: see GUIDE.txt.

NEW IN 0.27.0
  - A sharp picture drawn at the Deck's own resolution (1067x800, black
    bars at the sides keep the 4:3 shape; 1440x1080 on a 1080p monitor).
    Options > Picture: Original goes back to the old 640x480 picture.
  - Sound: music and sound effects (Options > Music / Sounds, or the game
    menu during a game).
  - Mouse and keyboard controls like the Windows version (see below); the
    right stick moves the map camera.
  - Starts in full screen (F11 for a window on a desktop).

INSTALL (Desktop Mode)
  1. Hold the power button and choose "Switch to Desktop".
  2. Copy TI4-SteamDeck-0.29.0.tar.gz to the Deck (Downloads is fine).
  3. Right-click it > "Extract" > "Extract archive here". You get a folder
     called TwilightImperium (move it anywhere you like, e.g. a "Games"
     folder in your home folder). If you had an older version, you can
     extract this one over it - your save and settings stay.
  4. To try it in Desktop Mode: open the folder and double-click
     TwilightImperium.sh (if asked, choose "Execute").

PLAY IN GAME MODE
  1. In Desktop Mode, open Steam.
  2. Games menu (top left) > "Add a Non-Steam Game to My Library..." >
     "Browse..." > find the TwilightImperium folder > choose
     TwilightImperium.sh (set the file type filter to "All files" if it
     isn't listed) > "Add Selected Programs".
  3. Go back to Game Mode ("Return to Gaming Mode" on the desktop). The game
     is in your Library under "Non-Steam".
  4. If the buttons do nothing: in the game's page, controller icon >
     choose the "Gamepad" layout.

CONTROLS (Options > Controls: Controller - the Deck's default)
  Steam Deck         Game (Miyoo) button   What it does
  ----------------   -------------------   ---------------------------------
  D-pad / left stick D-pad                 move the map cursor / lists
  right stick        -                     move the map camera
  A                  A                     choose / confirm (hold while the
                                           computer plays to fast-forward)
  B                  B                     back, skip or clear
  X / Y              X / Y                 as in the guide
  L1 / R1            L1 / R1               zoom the map; previous / next page
  L2 / R2            L2 / R2               side panel / look at the map
  View (two squares) START                 confirm in editors, send offers
  Menu (three lines) MENU                  the game menu and Negotiations
  Click a stick      SELECT                players overview (B goes back /
                                           undoes)
  Touch screen       tap = A on what you touch (menu rows, map tiles,
                     the buttons in the hint bar)
  The hints along the bottom show the Deck's names (VIEW, MENU, L3...).

KEYBOARD AND MOUSE (Options > Controls: Keyboard & mouse; or a dock)
  WASD moves the map camera, the arrows move the cursor, the mouse points
  at tiles and rows (the camera stays put), left click / Space = A, right
  click / Backspace = B, a right-button drag moves the camera, the wheel
  zooms smoothly, Esc = the game menu, Tab = view the map, Enter = START,
  R = SELECT, Q / E = L1 / R1, Z = L2, X, V = Y. F11 = full screen on / off.
  In Controller mode WASD is the D-pad instead; the mouse always works.

FILES
  Everything stays in the TwilightImperium folder: savegame.ti4 (your game
  in progress), settings.cfg, ti4.log (send it if something goes wrong),
  "Action - Twilight Symposium.mp3" (the music), icons.dat, retro.dat,
  and bug-report-... folders made with Options > Make a bug report.
  Saves are the same as the Miyoo's - copy savegame.ti4 between the Deck
  and App/TwilightImperium on the SD card to carry a game over.

NEEDS
  SteamOS 3.5 or newer (any Deck updated in the last two years), or another
  64-bit Linux with SDL2 installed. If the game doesn't start, look in
  ti4.log in the game's folder.
