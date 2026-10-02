# Card Journey: three-lane battler implementation plan

Status: milestones 1-3 implemented as a playable prototype. The rules below
record the initial implementation; the current retention, color, and reward
rules are in [the GBC README](gbc-c/README.md), with rationale in
[the mechanics research](gbc-c/MECHANICS_RESEARCH.md). Numerical values
remain starting points for human playtesting; the full mountain and camps are pending.

## Goal

Build a short Game Boy Color deck-building adventure in which placement, color
connections, and entry abilities create satisfying combinations. The player
starts with a small deck, wins battles on the mountain, and acquires cards that
change how that deck plays.

Teaching sentence: "Play up to two cards, match neighboring colors to strengthen
their abilities, and attack through three lanes."

The first deliverable is one fully playable duel. Prove that duel before expanding
the campaign or introducing more card mechanics.

## Initial rules

### Cards and board

- Each side has three persistent card slots, facing the opponent's three slots.
- Each card has a color, shape, and verb. Its shape supplies attack and maximum
  health; its verb activates once when the card enters play.
- Start with Ruby and Jade; circle, square, and triangle; FIGHT, GIVE, and TAKE.
  This gives 18 possible definitions, but the starter and reward pools are curated.
- Starting shape stats: circle 2 attack / 2 health; square 1 / 4;
  triangle 3 / 1. Damage persists while a card is on the board.
- A friendly card immediately to the left or right with the same color boosts
  the incoming card's verb. One or two matching neighbors both give one boost.
  Slots do not wrap, and opposing cards do not create connections.
- Evaluate the connection after placement and removal of any replaced card.
  Connections do not change attack or health and do not retrigger old cards.

| Verb | Normal | Connected | Target |
| --- | --- | --- | --- |
| FIGHT | Deal 1 damage | Deal 2 damage | One enemy board card |
| GIVE | Heal 1 health | Heal 2 health | Another friendly board card |
| TAKE | Draw 1 card | Draw 2 cards | Own draw pile |

FIGHT cannot target the opponent directly. GIVE cannot exceed maximum health.
With no eligible target, the card can still be played and its verb has no effect;
the placement preview must make that clear. Verbs never activate on death or
replacement. Every play consumes one of the two available plays.

### Battle setup and turn sequence

- Both combatants start at 12 health with shuffled ten-card decks and empty boards.
- The player acts first with one play on the opening turn. Subsequent turns allow
  two plays. In 200 mirrored-deck AI battles, limiting the opening reduced first
  player wins from 177 to 116; this is preliminary simulation evidence.
- At the beginning of a turn, draw five cards, or as many as are available.
- Play zero, one, or two cards, then explicitly end the turn. Ending is explicit
  even after the second play so the player can inspect the resulting board.
- A card may enter an empty slot or replace an existing friendly card. Replacement
  sends the old card to its owner's discard pile. Preview this before committing.
- Resolve the active side's attacks from left to right. Each surviving card
  attacks the opposing slot, or the opponent's health if that slot is empty.
- Newly played cards can attack that turn. Defenders do not retaliate; they attack
  on their own side's turn. There is no excess-damage spillover from a destroyed
  card into opponent health.
- Remove defeated cards immediately. Stop the battle immediately when either
  combatant reaches zero health, before any later actions or attacks.
- Discard the active side's remaining hand and begin the other side's turn.
- Whenever a draw pile empties, shuffle its discard pile into it. Cards currently
  in hand or on the board remain out of the shuffle. If both piles are empty,
  stop drawing without inventing cards or applying a fatigue penalty.
- Seven hand slots accommodate the largest possible hand under these rules.
  The screen shows a scrolling window of five, with an indicator for hidden cards.
- A battle uses temporary card instances. Damage and battle-zone positions do not
  alter the permanent deck; every battle starts with that complete deck again.

These rules intentionally put the action limit, rather than mana or sacrifices,
at the center of the first prototype's cost decisions.

## Game Boy screen and controls

Use the existing 20-by-18 tile viewport. A candidate layout is:

| Tile rows | Content |
| --- | --- |
| 0 | Both combatants' health, active side, plays remaining |
| 1-4 | Three enemy cards |
| 5 | Enemy attack / current health |
| 6-9 | Three friendly cards |
| 10 | Friendly attack / current health |
| 11-14 | Five-card hand window |
| 15-16 | Selected card or placement/ability preview |
| 17 | Contextual controls |

Keep lane columns aligned. Reuse the 3-by-4 tile card frame, placing the verb icon
in the currently blank card-face row. Keep combat stats outside the card frame.
Show a small non-color indicator for a connected placement, plus the numeric
ability result. Card inspection supplies written color, shape, verb, and stats.

- D-pad: select hand card, lane, or ability target for the current phase.
- A: choose / confirm.
- B: cancel the pending choice without changing battle state.
- START: request end turn; confirm if there are unused plays.
- SELECT: inspect cards, draw/discard counts, and concise rules.

Choose hand card, destination, and any target before committing the play. Keep
the prospective state separate so cancellation never consumes a card or action.
Use the same rule calculation for preview and resolution.

## Implementation structure

The active game is `gbc-c/`; the GB Studio project is not the implementation target.

| File | Responsibility |
| --- | --- |
| `gbc-c/src/cards.h`, `cards.c` | Color/shape/verb definitions, stats, starter and reward pools |
| `gbc-c/src/battle.h`, `battle.c` | Hardware-independent battle state, legal plays, effects, combat, zone movement |
| `gbc-c/src/ai.h`, `ai.c` | Small opponent policy using legal actions and public information |
| `gbc-c/src/main.c` | Input phases, rendering, audio, map, rewards, campaign flow |
| `gbc-c/tools/gen_assets.py` | Any required icon and frame changes; regenerate `assets.c` and `assets.h` |
| `gbc-c/Makefile` | Additional compilation units and a focused host rules-test target |
| `gbc-c/tests/` | Deterministic tests of consequential battle rules |
| `README.md`, `gbc-c/README.md` | Updated game description, rules, controls, build instructions |

Reuse the packed one-byte card definition: color and shape already exist, and
the low three bits are available for verbs. Preserve existing verb-icon indices
through explicit constants or a lookup. Store current health separately in each
occupied board slot. Use bounded arrays and no dynamic allocation.

Inject a small deterministic random source into shuffle and AI tie-breaking for
tests. Rendering and the AI must share the engine's legality checks. The AI may
inspect its own hand and the public board, but not the player's hand or deck order.

Keep the existing title, mountain artwork, palettes, transitions, and sound
helpers. Replace the match-chain battle and its banking/push state. Adapt reward,
deck-view, help, and map text as campaign integration proceeds.

The current working tree already contains changes to both READMEs, `main.c`, and
the built ROM. Treat that working state as the baseline; do not reset it to HEAD.
Generated assets must be changed through their generator. Keep the 32 KB ROM
target initially and inspect linker size/headroom as features are integrated.

## Milestones and acceptance criteria

### 1. Battle rules foundation

Implement card definitions, bounded battle state, drawing/shuffling, deployment,
replacement, connections, the three verbs, attacks, and victory detection.

Add focused host tests for card conservation across zones; reshuffling with cards
on the board; connection edges and non-stacking boosts; target legality and heal
caps; replacement; action limits; attack order; and immediate lethal resolution.

Done when deterministic example battles resolve correctly without rendering and
the existing ROM still builds with the new modules linked.

### 2. One playable GBC duel

Implement the board layout, hand scrolling, selection phases, cancellable
previews, entry effects, attack feedback, and win/loss screen. Wire one practice
opponent into the run entry point. Use the same two-play rules for both sides.

Start the AI with a small scored policy: take a visible win, prevent an immediate
loss, favor useful ability effects and favorable lane trades, then improve its
board. Reevaluate after each play. Allow passing; do not replace a useful card
merely to consume the second play.

Done when a complete battle can be played and restarted in an emulator, all six
board cards and their stats are readable, and previews agree with actual effects.

### 3. Three-battle deck-building journey

Connect battles to three existing mountain stations. Win, choose one of three
cards or skip, then fight with the updated deck. Start at ten cards; retain the
existing twenty-card capacity. At capacity, gaining a card requires selecting a
replacement or skipping, without silently dropping a card.

Give the three opponents distinct decks: a straightforward attacker, a durable
support deck, and a deck that uses color connections. Differences should be
visible through the shared card rules rather than hidden bonuses.

Restore battle health between encounters. Reuse the three campaign hearts as
retry lives: defeat costs one heart and retries the same station; three defeats
end the run. Explain the distinction between battle health and campaign lives.

Done when a full short run reaches an ending, rewards affect subsequent draws,
skipping and replacement work, and the deck viewer explains the new attributes.

### 4. Mountain progression and tuning

Expand to ten opponents after the short run is enjoyable. Add occasional camp
visits offering recovery of one lost campaign heart or removal of one card, with
a minimum deck size of eight. Update map progress, help, tutorial, and endings.

Curate rewards to offer both useful connections and different combat roles.
Introduce mechanics through encounters: basic attacks, then connections, then
deliberate ability combinations. Add stronger opponents through deck composition
and better decisions before simply increasing health.

Done when the full mountain is completable, losses are understandable, and at
least two substantially different deck strategies are viable in playtests.

### 5. Verification and release candidate

Run the focused rules tests, rebuild assets if changed, and build the GBC ROM.
Exercise an emulator through a complete win and loss, reward skip/add/replace,
camp choices, inspection, seven-card hand scrolling, target cancellation, and
empty draw/discard cases. Inspect visual readability and animation pacing.

Check ROM/RAM and tile usage, update both READMEs, and summarize any remaining
balance limitations. Builds and smoke checks establish correctness; human
playtesting establishes whether the game is enjoyable.

## Early design risks to evaluate

- **TAKE under a two-play limit:** drawing on the second play has no future-hand
  benefit because unused cards are discarded. Test whether finding a better
  second play justifies TAKE; adjust hand retention or the verb if it does not,
  before creating more cards around it.
- **First-player snowball:** immediately attacking into an empty board may be too
  strong. Compare mirrored decks with each side starting and inspect opening
  damage before choosing an opening-turn restriction.
- **Healing stalemates:** square/GIVE loops may prevent progress. Record long
  battles and repetitive AI decisions. Tune damage, healing, or durability before
  introducing a timer, fatigue system, or additional player-facing rule.
- **One-color dominance:** color connections should reward planning while shape
  roles and useful verbs justify off-color picks. Measure actual reward choices
  and deck outcomes; do not assume variety will emerge automatically.
- **Free replacement loops:** repeatedly replacing cards to reuse entry abilities
  may outweigh maintaining a board. Check this explicitly alongside card-draw
  circulation and the action cap.
- **Small-screen comprehension:** validate the battle layout in milestone 2;
  avoid postponing readability until the campaign is built.

Defer MOVE/SNEAK/SPEAK, additional colors/shapes, permanent card upgrades,
mid-battle shops, relics, directional capture, mana/sacrifice systems, and save
support until the basic battle and short deck-building journey pass these checks.

## Immediate next implementation task

Playtest the three-battle prototype before extending the mountain. Evaluate
TAKE's value, reward choices, match length, and the opening restriction with people.
