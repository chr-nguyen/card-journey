# Card Journey mechanics research and next prototype

Research date: 2026-10-02. Status: the first three rule experiments are
implemented. Their balance and enjoyment remain to be tested with people.

Recommendation: build around **three lanes, two plays, and one card prepared for
next turn**. Keep color connections as the central placement mechanic. Give
players more control over their next hand and their expedition deck before
expanding the card vocabulary or the mountain.

These changes remain hypotheses about fun and clarity, not demonstrated balance
improvements. The original prototype is available in the previous Git commit for
paired comparisons.

## What other card games teach us

This comparison includes TCGs and deck-building games. Both are relevant:
Card Journey has a persistent combat board and a deck that changes during a run.

| Reference | Relevant mechanism or design observation | Application to Card Journey |
| --- | --- | --- |
| Magic: The Gathering | Mark Rosewater distinguishes understanding a card, tracking the board, and mastering strategic uses. Simple cards can create deeper decisions through context. | Make the same small set of cards useful in different situations. Keep the existing shape, verb, and connection vocabulary readable. |
| Marvel Snap | The standard structure uses three locations and six turns. Location restrictions also require restraint: Second Dinner removed several overly restrictive locations from its pool. | Limited space can create depth. Start with positioning and opponent composition; introduce at most one clearly previewed encounter rule at a time. |
| Inscryption | Creatures can be sacrificed to play more powerful creatures, making an existing board piece part of the cost. | Our existing replacement rule already asks players to give up a body, health, or a connection to gain an entry effect. Make that opportunity cost meaningful before adding a sacrifice currency. |
| Dominion | Players cycle their decks through new five-card hands; cards such as Chapel remove cards and Remodel exchanges one card for another. | Improving a deck includes controlling its contents and frequency of draws. A reward can replace an awkward card instead of expanding the deck. |
| Slay the Spire | Its developers describe how readable enemy intent helped players make informed choices. Their balance talk combines iteration, metrics, and player feedback. | Show the consequences we can calculate reliably, then judge the design through both observed choices and player explanations. |

Primary sources:

- [Rosewater: Lenticular Design](https://magic.wizards.com/en/news/making-magic/lenticular-design-2014-03-31)
- [Second Dinner: Grand Arena, including its comparison with standard six-turn play](https://marvelsnap.com/new-limited-time-mode-grand-arena/)
- [Second Dinner: restrictive location changes](https://marvelsnap.com/may-25th-ota-balance-updates/)
- [Daniel Mullins describing Inscryption's sacrifice system](https://blog.playstation.com/2022/07/07/psychological-horrors-stack-in-devilish-deck-builder-inscryption/)
- [Rio Grande Games: Dominion second-edition rules](https://www.riograndegames.com/wp-content/uploads/2016/09/Dominion2nd.pdf)
- [Mega Crit founders: the development of enemy intent](https://arstechnica.com/video/watch/war-stories-slay-the-spire-war-stories/?c=series)
- [Anthony Giovannetti: Metrics Driven Design and Balance, GDC 2019](https://media.gdcvault.com/gdc2019/presentations/Giovannetti_Anthony_SlayTheSpire.pdf)

Applications in the table are our design inferences, not claims that those games
prove the proposed Card Journey rules will work.

## Findings in the earlier prototype

These observations describe the implementation before the three experiments
below. They explain the changes; the current rules are in [the README](README.md).

1. **The action budget already creates a useful cost.** Every deployment spends
   one of two plays, and three board slots force replacement decisions. A mana
   track would need to earn its additional complexity.
2. **TAKE had a timing problem.** On the final play, its newly drawn cards could
   not be deployed and were discarded before the next turn. It still deployed a
   body and affected deck circulation, but its advertised draw effect had little
   immediate value in that situation.
3. **Colors had no distinct capabilities.** Both colors had all
   three shapes and all three verbs. Recoloring a deck to a single color keeps
   its available stats and abilities while removing mismatched connections.
   This is a structural incentive toward one color, not a measured win-rate
   claim. The reward screen additionally guarantees an option in the deck's
   most common color.
4. **Healing has fewer opportunities than damage.** GIVE needs another friendly
   body that has survived damage. A one-health triangle cannot survive a positive
   damage hit, and a two-health circle dies to connected FIGHT. Squares have the
   largest healing window. Whether GIVE is too weak needs playtesting.
5. **Rewards usually expanded the deck.** Replacement was offered only at the
   twenty-card cap. An extra card may dilute an existing combination rather than
   improve the deck. There are only two reward stops before the current summit.
6. **Early deployment can compound its advantage.** Cards attack immediately
   and defenders do not retaliate. The existing one-play opening is useful,
   but the first-player test in the implementation plan uses one heuristic AI;
   it does not establish human balance.
7. **The AI needed to understand the planning rule.** It valued extra hand
   cards only while a play remained. Evaluating retention with that unchanged
   policy would have underrated the mechanic.

## First implemented experiment: prepare one card

Teaching sentence: **Play up to two cards, connect matching colors, and keep one
card for your next turn.**

Current rule:

- Start the duel with five cards and the existing one-play opening.
- On later turns, start with up to five cards total: a retained card plus four
  draws, or five draws if nothing was retained. Draw only what is available.
- At end turn, optionally select one unused hand card to keep; discard the rest.
  Resolve the existing left-to-right attacks. If the duel ends, retention has
  no effect outside that duel.
- A kept card remains in hand during the opponent's turn. It occupies one hand
  slot, cannot be drawn again, and can be kept again on a subsequent turn.
- Keeping a card costs no play and grants no extra play or bonus. The choice is
  which card deserves the single retained slot.
- TAKE keeps its current draw 1 / connected draw 2 behavior and seven-card cap.
  It can find a better second play or a card worth preparing for the next turn.
- Use identical retention and drawing rules for both sides.
- Clear retention at battle end. It does not alter the permanent deck.

This produces a short planning horizon without requiring players to track a
second resource. It also puts a useful limit on certainty: one planned card,
with the rest of the hand refreshed.

Example: a Ruby card occupies the left lane; a wounded Jade guardian occupies
the middle. A Ruby FIGHT card played in the empty right lane is unconnected.
Replacing the middle guardian with it creates a connection to the left card and
increases its entry damage, but gives up the guardian. A Jade GIVE card can
instead preserve the guardian. Retention lets the player keep the Ruby attacker
for a later opening rather than lose access to it at cleanup. Which line is
better still depends on the enemy board and available second play.

UI: offer the keep choice as part of ending the turn, with a small pin on the
chosen card and explicit KEEP / NONE options. Cancellation must restore the
uncommitted turn; attacks start only after confirmation. Preserve the familiar
card footprints. If the additional end-turn choice becomes tedious, test an
optional pin selected during normal hand browsing instead.

Risks: retaining the best card repeatedly could reduce hand variety; repeated
TAKE replacements could make key draws too reliable; the extra prompt could
slow the game. A positive result requires players to value the choice, not just
an increase in the heuristic AI's score.

## Second implemented experiment: make color choices have a cost

The implemented twelve-card pool replaces the earlier symmetric eighteen-card
pool, using the same stats and verbs:

| Color | Available verbs | Intended role and tradeoff |
| --- | --- | --- |
| Ruby | FIGHT, TAKE, on each of the three shapes | Direct entry damage and pressure; lacks healing. |
| Jade | GIVE, TAKE, on each of the three shapes | Sustaining bodies and preparing cards; lacks entry removal. |

All cards still attack normally. Jade can deal damage through its creatures;
Ruby can defend with a durable shape. These identities restrict available entry
effects, not which basic combat actions a player can take.

Now an off-color pick can provide something missing from a deck, at the cost of
less reliable connections. A single-color deck remains a legitimate option.
Do not force a mixed-color quota or add a color-damage chart.

This is deliberately a test pool. Ruby may dominate because it kills bodies
before healing becomes relevant. Measure that instead of assuming the two
colors are balanced. If necessary, test durability values separately so more
cards survive one hit; do not simultaneously change health, color pools, and
retention and then attribute the result to one mechanic.

Keep starters fixed during comparisons. Reward generation must draw from the
curated definitions rather than constructing any color/shape/verb combination.

## Third implemented experiment: a deck that improves without becoming larger

The short prototype now uses an expedition deck of exactly ten cards.
After a victory, choose one of three offered cards to replace a card in the deck,
or skip. Human tests can compare this with the earlier add-or-skip reward flow.

Replacement is valuable because the next battle contains the new card at the
same deck size. It can strengthen a connection, cover a weakness, or remove a
redundant role. It also creates a clear cost: the player must choose what to lose.

Offer three distinct, legal cards. Try to include a card supporting the current
plan, one covering a missing capability, and one suggesting a different plan;
show their ordinary card information rather than labeling a supposed best pick.
Avoid three automatic numerical upgrades and avoid guaranteeing an exact
combo piece. Start by comparing human choices from curated offers.

A camp can later offer **recover one lost expedition heart** or **draft another
replacement**. Both options have a clear effect in the existing systems. At full
hearts, disable recovery visibly. Persistent battle damage is unnecessary for
this version because expedition lives already carry risk between encounters.

## The engaging loop to aim for

**Read the threat → choose an order and placement → see the combination work →
prepare one card → win a useful replacement → test the revised deck.**

Keep the three-duel run while proving this loop. Give each encounter a purpose:

1. Base Camp: an opponent with a legible, straightforward attack plan.
2. Ice Chasm: an opponent that makes preserving a body or changing a lane matter.
3. Elder Gate: an opponent that tests both the player's chosen deck strength and
   its weakness, with no undisclosed combat bonuses.

After that loop succeeds, try six battles with two camp breaks and one early
route choice between a normal battle and a clearly described harder encounter
with an extra draft opportunity. Six is a pacing hypothesis, not a replacement
for the existing ten-station plan until human playtests support it. Show the next
opponent's broad deck identity before the reward choice when practical.

Target approximately 2–4 minutes for an ordinary duel and 15–25 minutes for the
expanded expedition. These are desired session lengths, not current measurements.
A longer mountain should introduce new decisions, not repeat the same duel.

## Readable information and atmosphere

Show a reliable preview of the player's imminent attack resolution: which
creatures die and whether damage reaches the opponent. This can use the existing
inspection/footer space. Do not label a forecast of the enemy's present board
as its guaranteed next turn: our AI can play cards first and change that result.

Full enemy intent would require a separate design experiment with committed,
visible enemy actions and defined behavior when those actions become illegal.
It should not be bolted onto the current adaptive AI as a misleading promise.

The Antarctic theme can strengthen the existing choices: creatures as unsettling
allies, connections as resonance, a retained card as a recurring dream, and
replacement as leaving an ally behind. Keep short, explicit mechanical labels.
A sanity meter, random curses, ritual currency, and additional status effects
would each need a compelling decision that the current rules cannot provide.

Later, a single optional run-wide artifact could alter one familiar rule and
suggest a deck direction. Test that only after the base choices hold up; adding
many modifiers now would hide the source of balance problems.

## Test order and acceptance evidence

1. Record the baseline: match length, opening advantage, card/verb choices,
   replacement frequency, and whether players can explain important outcomes.
2. Compare baseline with retention only using the same starting decks and paired
   random seeds. Update the AI to select retained cards, and include a policy
   capable of looking at both plays so combo value is not judged only greedily.
3. Compare color pools with the chosen hand rule held constant.
4. Compare add-or-skip and replace-or-skip rewards over complete short runs.
5. Introduce camps and a route choice only after players want another short run.

Use several human testers with differing card-game experience. Ask them to
explain a turn, a kept card, a skipped reward, and a loss. Observe whether they
change their plan when the board changes. Do not equate fast button presses or
AI win rates with enjoyment.

Record these questions rather than targeting equal pick rates:

- Does TAKE create useful options on both first and final plays?
- Do players retain different cards for different situations?
- Does a connection sometimes lose to a better defensive placement?
- Are there understandable reasons both to replace and to preserve a board card?
- Can a player describe how their reward changed the next duel?
- Are aggressive and defensive plans both useful across the encounters?
- Does a loss feel traceable to a decision or a visible risk?
- Do players want to try another build when a run ends?

A retained-card implementation also needs conservation, reshuffle, hand-cap,
empty-pile, cancellation, lethal-turn, and battle-reset checks. Automated
rules and emulator checks now cover these core paths; human testing remains.
Encounter and
balance conclusions must distinguish side-turns from full rounds and report
sample sizes and policies. The old 200-game result is a baseline observation,
not validation of the implemented rules.
