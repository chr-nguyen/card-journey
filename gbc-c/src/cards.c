#include "cards.h"

const char * const verb_names[6] = { "MOVE", "FIGHT", "SNEAK", "SPEAK", "TAKE", "GIVE" };
/* Ruby brings entry damage; Jade brings healing. Both colors can draw.
   Combat attack and health still come from shape, regardless of color. */
const uint8_t starter_deck[STARTER_SIZE] = {
    CARD(0,0,1), CARD(0,0,1), CARD(0,1,1), CARD(0,2,1), CARD(0,0,4),
    CARD(2,0,5), CARD(2,1,5), CARD(2,1,5), CARD(2,2,5), CARD(2,0,4)
};
const uint8_t opponent_decks[3][STARTER_SIZE] = {
    { CARD(0,0,1), CARD(0,0,1), CARD(0,1,1), CARD(0,0,4), CARD(0,1,4),
      CARD(0,2,4), CARD(2,0,5), CARD(2,1,5), CARD(2,0,4), CARD(2,2,4) },
    { CARD(0,0,1), CARD(0,0,1), CARD(0,1,1), CARD(0,2,1), CARD(2,0,5),
      CARD(2,1,5), CARD(2,1,5), CARD(2,2,5), CARD(2,0,4), CARD(2,1,4) },
    { CARD(0,0,1), CARD(0,0,1), CARD(0,2,1), CARD(0,2,1), CARD(2,0,5),
      CARD(2,1,5), CARD(2,1,5), CARD(2,2,5), CARD(2,0,4), CARD(2,1,4) }
};
uint8_t card_valid(uint8_t card)
{
    uint8_t v = C_VERB(card), c = C_COLOR(card);
    return card != CARD_NONE && !(card & 0x80) &&
        (c == COLOR_RUBY || c == COLOR_JADE) && C_SHAPE(card) < 3 &&
        (v == VERB_TAKE || (c == COLOR_RUBY && v == VERB_FIGHT) ||
         (c == COLOR_JADE && v == VERB_GIVE));
}
uint8_t card_attack(uint8_t card)
{
    static const uint8_t stats[3] = { 2, 1, 3 };
    return card_valid(card) ? stats[C_SHAPE(card)] : 0;
}
uint8_t card_health(uint8_t card)
{
    static const uint8_t stats[3] = { 2, 4, 1 };
    return card_valid(card) ? stats[C_SHAPE(card)] : 0;
}
uint8_t card_reward(uint8_t index)
{
    /* Six legal definitions per color: each shape with its role verb or TAKE. */
    static const uint8_t verbs[4] = {
        VERB_FIGHT, VERB_TAKE, VERB_GIVE, VERB_TAKE
    };
    index %= REWARD_COUNT;
    return CARD(index < 6 ? COLOR_RUBY : COLOR_JADE,
                (index % 6) / 2,
                verbs[(index < 6 ? 0 : 2) + (index & 1)]);
}
