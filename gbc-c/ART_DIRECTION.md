# Card Journey: Antarctic GBC art direction

The title and mountain map now depict a doomed polar expedition: fractured blue
ice, enormous black masonry, a pale auroral ribbon, and a small explorer in a
rust-red coat. The visual reference is *At the Mountains of Madness*: Antarctic
scale and unfamiliar architecture. Native 160x144 silhouettes and restrained
four-color tile palettes keep the route readable. All artwork is original and
built on the pixel grid; no external game sprites were copied.

The approved card portraits, palettes, and battle layout remain the visual
foundation. The battle change is to rendering: moving a selection no longer
clears the visible board.

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
- The title now has a full-screen Antarctic panorama: a crescent moon, aurora,
  layered snow ridges, distant ruins, glacier fissures, and a foreground explorer.
  The start prompt stays visible.
- The map uses asymmetric mountain faces, a switchback trail, a base-camp tent,
  and carved summit masonry. Transparent sprite flags retain the terrain below
  them. The three stops are Base Camp, Ice Chasm, and Elder Gate.
- The explorer is a 16x16, four-sprite character with a fur hood, expedition coat,
  backpack, and two wind-tossed scarf frames, used on both title and map screens.

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

[Yacht Club Games: The Art of the Game](https://www.yachtclubgames.com/blog/the-art-of-the-game/)
explains its GBC-inspired approach: four colors per tile, restrained environment
colors, and stronger color separation for characters and interactive objects.
Its discussion of weathered textures in *Dragon Quest III* and *Dragon Quest
Monsters* informed the broken ice strata and carved stone marks here. The rust
coat and bright route flags separate the explorer and route from the cold terrain.

[Lovecraft's original text](https://www.hplovecraft.com/writings/texts/fiction/mm.aspx)
provided the landscape reference: Antarctic peaks, vast dark ruins, and an
architecture whose scale overwhelms the expedition. Those descriptions informed
our angular ridges, stepped masonry, narrow doorway, and small human silhouette.

The [GB Studio sprite documentation](https://www.gbstudio.dev/docs/assets/sprites/)
and [palette documentation](https://www.gbstudio.dev/docs/assets/palettes/)
provide useful native-grid and color constraints. The scenery is authored as
integer pixel clusters, deduplicated into 8x8 tiles, and encoded directly to 2bpp.

[GBDK's rendering documentation](https://gbdk.org/docs/api/docs_using_gbdk.html)
describes VRAM access restrictions and the automatic VBlank copy of shadow OAM.
The game now composes duels in RAM, compares final tiles and attributes against
the displayed state, and sends changed row spans during VBlank. Cursor movement
uses a smaller selection-only update; scrolling and played cards use the complete
composition. Sprite outlines are updated together in a critical section.

## Flicker verification

The emulator regression examines every displayed frame during left/right hand
selection. It checks the board and card interiors for any changed pixels and
asserts that the LCD stays enabled. The previous committed ROM disturbed the
board for 17 frames during the measured right move; the updated ROM disturbed it
for zero frames. The cursor reached its next card at frame 8 instead of frame 22
in the same boot/input sequence. These timings describe that emulator scenario,
not all possible actions.

## Asset budget and maintenance

- 193 static background tiles, including the font.
- 44 additional tiles reserved for the largest title lettering composition.
- 53 sprite patterns; selection uses ten outline objects plus one pointer, with
  at most three selection objects on any scanline. Effects use four objects.
  There are four loaded sprite palettes; all eleven UI objects are hidden when
  leaving the board or entering an inspection screen.
- The title's dynamic tile base follows `BG_TILE_COUNT`. The generator rejects
  art that would overflow the 256 tile IDs or overlap the sprite/background ranges.
  Bank 0 uses 237 tile IDs including the largest title, leaving 19 available.
- Scenery is loaded separately into CGB VRAM bank 1: 173 unique title tiles or
  193 map tiles, each below the 256-tile limit. These screens never replace the
  bank-0 card patterns. The scene generator rejects tile-budget overflow.
- The explorer uses four objects; four route flags bring the map to eight.
  Background composition and displayed-state caches use 1,440 bytes of RAM.
  The ROM remains a 32 KB, GBC-exclusive cartridge.

Edit `tools/scenes.py` for landscape pixels and `tools/gen_assets.py` for the
explorer and shared tiles, then run `make assets` and `make`. Do not hand-edit
`src/assets.c` or `src/assets.h`. Palette values and screen composition are in
`src/main.c`; board decoration, targeting, and effects are in `src/duel_ui.h`.

Inspect the native 160x144 output as well as a nearest-neighbor enlargement.
Screenshots are emulator captures, not design mockups. The smoke test can export
them with `--screenshot-dir DIRECTORY --screenshot-scale 4` after its build step.

The emulator test checks all ten outline sprites, hand navigation, placement and
target framing, reward selection, ownership labels and direction markers, and
sprite cleanup during inspection. `--exercise-draw` additionally checks selection
after hand scrolling. Boot timing affects the game's RNG, so a coverage run may
need a different `--boot-frames` value to reach a six-card hand. The driver also
rechecks the active side after waiting for a turn transition.

This revision passed the ROM build, host battle rules, and emulator coverage of
both endings: `--boot-frames 441` reached three wins; the standard run reached
three defeats. Six-card scrolling passed with `--exercise-draw --boot-frames 420`.
The driver also covers movement without flicker, cancellation, enemy targeting,
reward outlines, inspection, ownership, and agreement with the host rules.

The pass was checked in PyBoy. Color and contrast on an original unlit GBC screen
still need a physical-hardware playtest. `previews/` contains title, map, battle,
ownership, targeting, reward, and inspection captures enlarged 4x with
nearest-neighbor scaling.
