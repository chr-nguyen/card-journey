# CARD JOURNEY — a GBC card-collecting mountain climb

A complete Game Boy Color game written in C with
[GBDK-2020](https://github.com/gbdk-2020/gbdk-2020). Climb a mountain of
ten stations; win a card duel at every station to keep going.

## The hook: every card is dual-use

Each card in your pack has three traits:

| Trait | Values |
|-------|--------|
| Color | Ruby, Amber, Jade, Azure |
| Shape (chip value) | Circle 2, Square 4, Triangle 6, Diamond 8 |
| Action verb | MOVE, FIGHT, SNEAK, SPEAK, TAKE, GIVE |

* **On the trail** between stations, obstacles demand action verbs — a
  chasm needs MOVE, a wolf needs FIGHT or SNEAK, a guard listens to
  SPEAK. Spending a card gets you past, but it is **removed from your
  pack for the rest of the run**. Refusing a hostile obstacle costs a
  heart.
* **At each station** you duel with that same pack, Balatro-style:
  from a hand of five, raise a set of cards and play it. Sets of
  matching *shapes* (pair, trio, full house, quad...) score
  `chips x mult`; playing all one *color* is a flush for bonus mult;
  FIGHT cards add chips and SPEAK cards add mult. Beat the station's
  goal within 3 plays (3 swaps to redraw).

Every card burned to travel safely weakens the deck you must win with —
that tension is the whole game.

* Win a duel → draft one of three new cards.
* Caches (spend TAKE) grant two random cards.
* Shrines (sacrifice a GIVE card) grant one of six permanent
  **talismans**: Ember Fang, Echo Bell, Prism Eye, Fourth Wind,
  Wool Charm, Lodestone.
* Travelers (spend SPEAK) restore a heart.
* Lose all hearts — or all cards — and the mountain wins.

## Controls

| Screen | Keys |
|--------|------|
| Map    | A climb, SELECT help, START pack view |
| Duel   | LEFT/RIGHT cursor, A raise, B lower, START play set, SELECT swap raised cards |
| Trail  | LEFT/RIGHT browse matching cards, A spend card, B refuse |

## Building

Needs GBDK-2020 (default path `~/gbdk`, override with
`make GBDK_HOME=/path/to/gbdk/`) and Python 3.

```sh
make            # -> card-journey.gbc
make assets     # regenerate src/assets.c|h from tools/gen_assets.py
```

The ROM is a 32 KB GBC-exclusive cartridge; it runs in mGBA, SameBoy,
BGB, or on real hardware via a flash cart:

```sh
mgba card-journey.gbc
```

## How it's put together

* `src/main.c` — the whole game: state machines for title, map, trail,
  duel, reward, endings; CGB palette fades; runtime 2x font scaling for
  the big title letters; channel-1/4 sound effects via direct register
  writes.
* `tools/gen_assets.py` — every tile (font, card frames, shape glyphs,
  verb icons, mountain scenery, UI borders, sprites) is drawn as ASCII
  art in this script and compiled to native 2bpp tile data in
  `src/assets.c|h` (generated, committed).
* Cards pack into a single byte (`ccss0vvv`), the whole collection is
  40 bytes, and the ROM uses only bank 0 of a 32 KB cart.
