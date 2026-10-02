# Card Journey: cosmic-coastal GBC art direction

The visual direction is an eerie coastal ascent through impossible ruins:
sea-green mist, asymmetric monoliths, suspended eyes, and curling tidal plants.
The mood is Lovecraftian mystery rather than cathedral-and-graveyard gothic.
The compact, readable adventure language of Game Boy Color Zelda remains the
presentation reference. The approved ivory card faces, jewel affinities, and
creature portraits are preserved. Artwork is original and defined in the native
tile generator; no external game sprites were copied. Game rules are unchanged.

## What changed

- Circle cards depict a hooded spirit, squares an armored sentinel, and triangles
  a bat-winged gargoyle.
  Each portrait is drawn at 16x16 pixels, centered inside the existing 24x32 card.
- Shape seals remain at the top of each card; verb badges move to the bottom.
  FIGHT uses a sword, GIVE a healing heart, and TAKE a pair of cards.
- Color palettes use aged ivory highlights, jewel-colored shadows, and a dark shared outline.
  The same portrait is recolored for its card's affinity.
- The board uses rounded tide-ring recesses and quiet currents rather than pointed
  arches and candles. Connected neighbors retain visible gold links.
- Enemy lanes occupy a violet band labeled ENEMY with downward chevrons; friendly
  lanes occupy a sea-green band labeled YOUR with upward chevrons. Ownership is
  conveyed with text and direction as well as color. Card affinities do not change.
- A continuous bright outline marks the selected hand card, destination, target,
  or reward choice. A separate hand arrow persists while choosing a lane or target;
  the footer explicitly says HIT ENEMY or HEAL ALLY. Selection stays visible when
  browsing a hand with no plays remaining, while the prompt says START TO ATTACK.
  Damaged card health is highlighted; action pips show remaining plays.
- Three-frame sword, healing, and draw effects give abilities visible feedback.
- Title and map screens share an original 48x32 cyclopean ruin with a floating
  eye. Waves, runic stones, tidal plants, and mist replace church towers and graves.

## Readability rules

- Dark scenery, light card faces: important game pieces must separate immediately.
- Keep the circle/square/triangle seal and verb badge explicit. Creature art is
  flavor, not an additional rule players must memorize.
- Reserve bright gold for selections and action feedback; do not
  use it across the whole background.
- Use hand-placed pixel clusters and native 8x8 tiles, with no smoothed scaling.
- Keep text and targeting unobstructed. Decoration never changes hit targets,
  card footprints, or the five-card hand window.

## Research and implementation choices

The [GB Studio sprite documentation](https://www.gbstudio.dev/docs/assets/sprites/)
describes 16x16 simple sprites and an 8-pixel composition grid that encourages
tile reuse. Its [palette documentation](https://www.gbstudio.dev/docs/assets/palettes/)
describes four-color palettes and conversion to the GBC's available colors.
Those constraints informed this art pass even though this game uses GBDK C.

The resulting design choices are specific to Card Journey: strong silhouettes
at native size, quiet surfaces behind the cards, consistent light direction,
and separate visual treatments for cards, text, and selection. Detailed card
illustrations use background tiles; movable cursors and short effects use hardware
sprites. This keeps the board readable without requiring many simultaneous sprites.

## Asset budget and maintenance

- 193 static background tiles, including the font.
- 44 additional tiles reserved for the largest title lettering composition.
- 44 sprite patterns; selection uses ten outline objects plus one pointer, with
  at most three selection objects on any scanline. Effects use four objects.
  There are four loaded sprite palettes; all eleven UI objects are hidden when
  leaving the board or entering an inspection screen.
- The title's dynamic tile base follows `BG_TILE_COUNT`. The generator rejects
  art that would overflow the 256 tile IDs or overlap the sprite/background ranges.
  This pass uses 237 tile IDs including the largest title, leaving 19 available.

Edit `tools/gen_assets.py`, then run `make assets` and `make`. Do not hand-edit
the generated `src/assets.c` or `src/assets.h`. Palette values are in `src/main.c`;
board decoration, targeting, and effects are in `src/duel_ui.h`.

Inspect the native 160x144 output as well as a nearest-neighbor enlargement.
Screenshots are emulator captures, not design mockups. The smoke test can export
them with `--screenshot-dir DIRECTORY --screenshot-scale 4` after its build step.

The emulator test checks all ten outline sprites, hand navigation, placement and
target framing, reward selection, ownership labels and direction markers, and
sprite cleanup during inspection. `--exercise-draw` additionally checks selection
after hand scrolling. Boot timing affects the game's RNG, so a coverage run may
need a different `--boot-frames` value to reach a six-card hand. The driver also
rechecks the active side after waiting for a turn transition.

This revision passed the ROM build, host battle rules, full win/loss runs, and
six-card scrolling with `--exercise-draw --boot-frames 440`. Standard emulator
coverage includes selection movement and cancellation, explicit enemy targeting,
reward outlines, and ownership markers on occupied boards.

The pass was checked in PyBoy. Color and contrast on an original unlit GBC screen
still need a physical-hardware playtest. `previews/` contains title, map, battle,
ownership, targeting, reward, and inspection captures enlarged 4x with
nearest-neighbor scaling.
