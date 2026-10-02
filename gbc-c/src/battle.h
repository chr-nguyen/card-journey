#ifndef BATTLE_H
#define BATTLE_H
#include "cards.h"

#define LANES 3
#define HAND_MAX 7
#define BATTLE_HP 12
#define PLAYER 0
#define ENEMY 1
#define NO_TARGET 255
#define NO_WINNER 255

typedef struct { uint8_t card, hp; } BattleSlot;
typedef struct {
    uint8_t deck[DECK_MAX], discard[DECK_MAX], hand[HAND_MAX];
    uint8_t deck_n, discard_n, hand_n, hp;
    BattleSlot board[LANES];
} BattleSide;
typedef struct {
    BattleSide side[2];
    uint16_t rng, turn;
    uint8_t active, plays, winner;
} Battle;
typedef struct {
    uint8_t amount, targets, replacing;
} PlayPreview;

uint8_t battle_init(Battle *b, const uint8_t *player, uint8_t player_n,
                    const uint8_t *enemy, uint8_t enemy_n, uint16_t seed);
uint8_t battle_random(Battle *b, uint8_t count);
uint8_t battle_draw(Battle *b, uint8_t side, uint8_t count);
uint8_t battle_preview(const Battle *b, uint8_t hand, uint8_t lane, PlayPreview *p);
uint8_t battle_play(Battle *b, uint8_t hand, uint8_t lane, uint8_t target);
void battle_attack_lane(Battle *b, uint8_t lane);
void battle_next_turn(Battle *b, uint8_t keep);
void battle_end_turn(Battle *b, uint8_t keep);
#endif
