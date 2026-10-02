# Card Journey — a three-lane GBC deck battler

A Game Boy Color deck-building adventure written in C with GBDK-2020.
The current prototype is a three-opponent mountain journey: build a deck,
connect matching colors, and battle through three lanes.

## Cards

Each card has a **color**, **shape**, and **verb**. The prototype uses Ruby and
Jade. Shape determines combat stats:

| Shape | Attack | Health |
| --- | --- | --- |
| Circle | 2 | 2 |
| Square | 1 | 4 |
| Triangle | 3 | 1 |

A verb activates once when you play the card. If a friendly card immediately
beside its destination shares its color, the verb is stronger. Two matching
neighbors still provide just one boost; the left and right edges do not connect.

| Verb | Normal | Connected |
| --- | --- | --- |
| FIGHT | Deal 1 damage to an enemy card | Deal 2 |
| GIVE | Heal another friendly card by 1 | Heal by 2 |
| TAKE | Draw 1 card | Draw 2 |

Healing stops at a card's maximum health. FIGHT cannot damage the opponent
directly. A card can be deployed without a verb target, but that ability then
has no effect. A star in the placement preview identifies a color connection.

## Battle loop

Both sides start at 12 health. Enemy cards occupy the top three slots; your
cards occupy the middle three slots, with your hand below. The numbers under
each board card are **attack / current health**. Inspection explains each card.

1. Draw five cards at the beginning of your turn.
2. Play up to two cards. The player starts first and gets **one play on the
   opening turn**. Choose a card, choose its lane, then choose any ability target.
3. Press START to end your turn. Your cards attack their opposing lanes from
   left to right. An empty enemy lane lets damage reach the opponent.
4. Discard unused hand cards. The opponent takes its turn using the same rules.

Cards stay on the board, and damage persists. Newly deployed cards attack that
turn; defenders attack on their own turn and do not retaliate. Excess damage
to a defeated card does not spill over to opponent health. Reduce the opponent
to zero health to win.

You can replace one of your board cards with a new one; the old card goes to
your discard pile. Defeated cards also enter their owner's discard pile. When
the draw pile runs out, reshuffle the discard pile. Cards in hand or on the board
remain outside that shuffle. TAKE can grow your hand to seven cards; the visible
five-card window scrolls as you select cards.

The cosmic-coastal GBC setting uses sea mist, impossible ruins, and strange eyes.
The original hooded-spirit, armored-sentinel, and gargoyle card portraits remain;
shape seals are at the top and verb badges at the bottom. The enemy board has a
violet ENEMY band and downward markers; yours has a sea-green YOUR band and upward
markers. Card affinity colors stay identical on both sides. A full bright outline
marks the selected card, lane, or target, including reward choices. A small hand
arrow keeps track of the source card during placement and targeting; prompts name
enemy and ally targets explicitly. Gold links mark matching neighbors, and the
HUD's action pips show remaining plays. See [the art direction and asset notes](ART_DIRECTION.md).

## Building your deck

Begin with ten cards. After each of the first two wins, choose one of three
cards to add to your deck, or skip. Rewards include a card matching your most
common color. The deck supports twenty cards; at capacity a reward requires
replacing a card or skipping.

The three opponents favor attacks, durable support, and color connections.
Battle health resets for every duel. The three hearts on the mountain map are
**retry lives**: losing a duel costs a life and retries the same station. Cards
defeated in battle remain in your permanent deck. Clear all three opponents to
reach the summit; lose all three lives to end the run.

## Controls

| Screen / phase | Controls |
| --- | --- |
| Map | A battle, SELECT rules, START deck |
| Hand | LEFT/RIGHT select, A choose, START end turn, SELECT inspect |
| Placement | LEFT/RIGHT lane, A confirm / choose target, B cancel |
| Ability target | LEFT/RIGHT target, A commit, B return to placement |
| End-turn prompt | A confirm, B cancel |
| Inspection | UP/DOWN hand / friendly board / enemy board; LEFT/RIGHT card; B or SELECT back |
| Reward | LEFT/RIGHT offer, A acquire, B skip |
| Deck | LEFT/RIGHT card, B or START back |

Choosing a placement or target is a preview; backing out consumes no card or
play. START asks for confirmation while unused plays remain.

## Building and verification

Needs GBDK-2020 (default path `~/gbdk`; override with
`make GBDK_HOME=/path/to/gbdk/`). Asset regeneration needs Python 3.

```sh
make           # card-journey.gbc, 32 KB GBC-exclusive ROM
make test      # host C compiler: rules, card conservation, AI, seeded battles
make assets    # regenerate src/assets.c and src/assets.h
mgba card-journey.gbc
```

Optional headless verification requires PyBoy installed for the selected Python:

```sh
make emulator-test PYBOY_PYTHON=/path/to/python-with-pyboy
```

The emulator test uses normal buttons, reads RAM for assertions, compares moves
with the host rules engine, and writes screenshots to `/tmp/card-journey-*.png`.
It exercises a complete run, placement/target cancellation, inspection, retries,
and card acquisition. The script also accepts `--lose-run` and
`--skip-first-reward` to exercise the loss ending and reward skipping.

## Source layout and prototype limits

- `src/cards.c|h`: packed definitions, shape stats, starter and opponent decks.
- `src/battle.c|h`: bounded battle state, deck circulation, legal plays, verbs,
  connections, combat, and victory detection; independent of Game Boy hardware.
- `src/ai.c|h`: opponent decisions based on its hand and the public board.
- `src/duel_ui.h`: battle presentation and input, included by `main.c` after the
  shared rendering helpers.
- `src/main.c`: title, mountain, deck view, rewards, endings, palettes, and audio.
- `tools/gen_assets.py`: source of generated native 2bpp tile graphics.
- `tests/`: deterministic rules tests and emulator smoke driver.

This is the first playable prototype. Full ten-station progression, camps,
permanent upgrades, more colors/shapes/verbs, and saving are pending. Balance
and TAKE's usefulness with a two-play limit still need human playtesting.
See [the implementation plan](../IMPLEMENTATION_PLAN.md).
