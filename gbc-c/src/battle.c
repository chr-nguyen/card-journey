#include "battle.h"
#include <string.h>

uint8_t battle_random(Battle *b, uint8_t count)
{
    uint16_t x = b->rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    b->rng = x ? x : 1;
    return count ? (uint8_t)(b->rng % count) : 0;
}
static void shuffle(Battle *b, BattleSide *s)
{
    uint8_t i, j, card;
    for (i = s->deck_n; i > 1; --i) {
        j = battle_random(b, i);
        card = s->deck[i - 1];
        s->deck[i - 1] = s->deck[j];
        s->deck[j] = card;
    }
}
uint8_t battle_draw(Battle *b, uint8_t side, uint8_t count)
{
    BattleSide *s;
    uint8_t drawn = 0, i;
    if (side > 1 || b->winner != NO_WINNER) return 0;
    s = &b->side[side];
    while (count && s->hand_n < HAND_MAX) {
        if (!s->deck_n) {
            if (!s->discard_n) break;
            for (i = 0; i < s->discard_n; ++i) s->deck[i] = s->discard[i];
            s->deck_n = s->discard_n;
            s->discard_n = 0;
            shuffle(b, s);
        }
        s->hand[s->hand_n++] = s->deck[--s->deck_n];
        ++drawn;
        --count;
    }
    return drawn;
}
uint8_t battle_init(Battle *b, const uint8_t *player, uint8_t player_n,
                    const uint8_t *enemy, uint8_t enemy_n, uint16_t seed)
{
    uint8_t i, side;
    if (!player_n || !enemy_n || player_n > DECK_MAX || enemy_n > DECK_MAX)
        return 0;
    for (i = 0; i < player_n; ++i) if (!card_valid(player[i])) return 0;
    for (i = 0; i < enemy_n; ++i) if (!card_valid(enemy[i])) return 0;
    memset(b, 0, sizeof(*b));
    b->rng = seed ? seed : 1;
    b->winner = NO_WINNER;
    b->plays = 1; /* A single opening deployment limits first-player snowball. */
    b->turn = 1;
    for (side = 0; side < 2; ++side) {
        BattleSide *s = &b->side[side];
        s->hp = BATTLE_HP;
        s->deck_n = side ? enemy_n : player_n;
        for (i = 0; i < s->deck_n; ++i) s->deck[i] = side ? enemy[i] : player[i];
        for (i = 0; i < LANES; ++i) s->board[i].card = CARD_NONE;
        shuffle(b, s);
    }
    battle_draw(b, PLAYER, 5);
    return 1;
}
uint8_t battle_preview(const Battle *b, uint8_t hand, uint8_t lane, PlayPreview *p)
{
    const BattleSide *s, *enemy;
    uint8_t card, i, verb;
    if (b->winner != NO_WINNER || !b->plays || b->active > 1 || lane >= LANES)
        return 0;
    s = &b->side[b->active];
    enemy = &b->side[1 - b->active];
    if (hand >= s->hand_n) return 0;
    card = s->hand[hand];
    if (!card_valid(card)) return 0;
    p->amount = 1;
    p->targets = 0;
    p->replacing = s->board[lane].card != CARD_NONE;
    if ((lane && s->board[lane - 1].card != CARD_NONE &&
         C_COLOR(s->board[lane - 1].card) == C_COLOR(card)) ||
        (lane < LANES - 1 && s->board[lane + 1].card != CARD_NONE &&
         C_COLOR(s->board[lane + 1].card) == C_COLOR(card))) p->amount = 2;
    verb = C_VERB(card);
    for (i = 0; i < LANES; ++i) {
        if ((verb == VERB_FIGHT && enemy->board[i].card != CARD_NONE) ||
            (verb == VERB_GIVE && i != lane && s->board[i].card != CARD_NONE))
            p->targets |= (uint8_t)(1 << i);
    }
    return 1;
}
static void remove_slot(BattleSide *s, uint8_t lane)
{
    s->discard[s->discard_n++] = s->board[lane].card;
    s->board[lane].card = CARD_NONE;
    s->board[lane].hp = 0;
}
static void damage_slot(BattleSide *s, uint8_t lane, uint8_t amount)
{
    if (amount >= s->board[lane].hp) remove_slot(s, lane);
    else s->board[lane].hp -= amount;
}
uint8_t battle_play(Battle *b, uint8_t hand, uint8_t lane, uint8_t target)
{
    PlayPreview p;
    BattleSide *s;
    uint8_t card, verb, i, max;
    if (!battle_preview(b, hand, lane, &p)) return 0;
    if (p.targets) {
        if (target >= LANES || !(p.targets & (1 << target))) return 0;
    } else if (target != NO_TARGET) return 0;
    s = &b->side[b->active];
    card = s->hand[hand];
    verb = C_VERB(card);
    if (p.replacing) remove_slot(s, lane);
    s->board[lane].card = card;
    s->board[lane].hp = card_health(card);
    for (i = hand + 1; i < s->hand_n; ++i) s->hand[i - 1] = s->hand[i];
    --s->hand_n;
    --b->plays;
    if (verb == VERB_FIGHT && p.targets)
        damage_slot(&b->side[1 - b->active], target, p.amount);
    if (verb == VERB_GIVE && p.targets) {
        max = card_health(s->board[target].card);
        s->board[target].hp += p.amount;
        if (s->board[target].hp > max) s->board[target].hp = max;
    }
    if (verb == VERB_TAKE) battle_draw(b, b->active, p.amount);
    return 1;
}
void battle_attack_lane(Battle *b, uint8_t lane)
{
    BattleSide *s, *enemy;
    uint8_t amount;
    if (b->winner != NO_WINNER || lane >= LANES) return;
    s = &b->side[b->active];
    enemy = &b->side[1 - b->active];
    if (s->board[lane].card == CARD_NONE) return;
    amount = card_attack(s->board[lane].card);
    if (enemy->board[lane].card != CARD_NONE) damage_slot(enemy, lane, amount);
    else if (amount >= enemy->hp) {
        enemy->hp = 0;
        b->winner = b->active;
    } else enemy->hp -= amount;
}
void battle_next_turn(Battle *b)
{
    BattleSide *s;
    if (b->winner != NO_WINNER) return;
    s = &b->side[b->active];
    while (s->hand_n) s->discard[s->discard_n++] = s->hand[--s->hand_n];
    b->active = 1 - b->active;
    b->plays = 2;
    ++b->turn;
    battle_draw(b, b->active, 5);
}
void battle_end_turn(Battle *b)
{
    uint8_t lane;
    for (lane = 0; lane < LANES; ++lane) battle_attack_lane(b, lane);
    battle_next_turn(b);
}
