# card-journey (Game Boy Color, C / GBDK-2020)

A deck-building demo written in C and compiled to a real Game Boy Color ROM.

## What it does

- A 32-card deck. Each card has a **color** (red / yellow / blue) and a
  **shape** (square / circle / triangle), encoded as one value 0–8
  (`value = color*3 + shape`).
- You hold a **hand of 5** cards, shown as colored cards with a shape cut-out.
- **LEFT / RIGHT** move the cursor, **A** uses the selected card — it's
  immediately replaced by a fresh draw. The deck reshuffles when it runs out.
- Press **START** on the face-down deck to deal the opening hand (this also
  seeds the random number generator).

## Build

Requires [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020). This repo was
built against 4.5.0 installed at `~/gbdk/`.

```sh
make                      # uses GBDK_HOME=~/gbdk/ by default
make GBDK_HOME=/path/to/gbdk/   # if installed elsewhere
```

Output: `card-journey.gbc` (32 KB, flagged CGB-compatible — runs on GBC,
GBA, and original Game Boy).

## Run

Use any Game Boy Color emulator, e.g.:

```sh
mgba card-journey.gbc        # or: sameboy / bgb / openemu
```

Or flash it to a cartridge for real hardware.

## Where things are

- `src/main.c` — all game logic and graphics (tile/palette data at the top,
  deck/hand logic, rendering, input loop in `main`).
- `Makefile` — one-line GBDK build.

## Ideas for next steps

- Track score / mana and add a "play effect" where the `A` handler currently
  just redraws.
- Add a discard pile so used cards return to the deck on reshuffle.
- Bigger card art (multi-tile shapes) or a font for labels.
