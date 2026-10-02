#include "ai.h"

/* Scratch space is static to keep the Game Boy stack small. Evaluation uses
   only boards/health and the active side's hand count, never hidden enemy cards. */
static Battle trial;
static uint8_t keep_value(const Battle *b, uint8_t hand);
static int16_t evaluate(const Battle *b)
{
    const BattleSide *s = &b->side[b->active], *enemy = &b->side[1 - b->active];
    uint8_t lane, attack, enemy_attack;
    int16_t score = 0, damage = 0, threat = 0;
    for (lane = 0; lane < LANES; ++lane) {
        if (s->board[lane].card != CARD_NONE) {
            attack = card_attack(s->board[lane].card);
            score += attack * 3 + s->board[lane].hp;
            if (enemy->board[lane].card == CARD_NONE) damage += attack;
            else {
                score += attack >= enemy->board[lane].hp ? 9 : attack * 2;
                enemy_attack = card_attack(enemy->board[lane].card);
                if (s->board[lane].hp > enemy_attack) score += 3;
            }
        } else if (enemy->board[lane].card != CARD_NONE)
            threat += card_attack(enemy->board[lane].card);
        if (enemy->board[lane].card != CARD_NONE)
            score -= card_attack(enemy->board[lane].card) * 3 + enemy->board[lane].hp;
    }
    score += damage * 5 - threat * 5;
    if (damage >= enemy->hp) score += 1000;
    if (threat >= s->hp) score -= 500;
    if (b->plays) score += s->hand_n; /* Draws matter while a play remains. */
    else if (s->hand_n && ai_keep(b) != NO_TARGET)
        score += keep_value(b, ai_keep(b));
    return score;
}
/* A kept card trades one fresh draw for a known option next turn. Assess
   public board context without inspecting the opponent's hidden hand. */
static uint8_t keep_value(const Battle *b, uint8_t hand)
{
    const BattleSide *s = &b->side[b->active];
    const BattleSide *enemy = &b->side[1 - b->active];
    uint8_t card = s->hand[hand], lane, value;
    value = card_attack(card) + card_health(card);
    for (lane = 0; lane < LANES; ++lane) {
        if (s->board[lane].card != CARD_NONE &&
            C_COLOR(s->board[lane].card) == C_COLOR(card)) value += 2;
        if (C_VERB(card) == VERB_FIGHT && enemy->board[lane].card != CARD_NONE)
            value += 2;
        if (C_VERB(card) == VERB_GIVE && s->board[lane].card != CARD_NONE &&
            s->board[lane].hp < card_health(s->board[lane].card)) value += 2;
    }
    if (C_VERB(card) == VERB_TAKE && b->plays) ++value;
    return value;
}
uint8_t ai_keep(const Battle *b)
{
    const BattleSide *s = &b->side[b->active];
    uint8_t hand, best = NO_TARGET, value, best_value = 5;
    for (hand = 0; hand < s->hand_n; ++hand) {
        value = keep_value(b, hand);
        if (value > best_value) {
            best_value = value;
            best = hand;
        }
    }
    return best;
}

uint8_t ai_choose(const Battle *b, BattleMove *move)
{
    uint8_t hand, lane, target, found = 0;
    PlayPreview p;
    int16_t best = evaluate(b) + 1, score;
    for (hand = 0; hand < b->side[b->active].hand_n; ++hand)
        for (lane = 0; lane < LANES; ++lane) {
            if (!battle_preview(b, hand, lane, &p)) continue;
            for (target = 0; target < LANES; ++target) {
                if (p.targets && !(p.targets & (1 << target))) continue;
                if (!p.targets && target) break;
                trial = *b;
                if (!battle_play(&trial, hand, lane, p.targets ? target : NO_TARGET)) continue;
                score = evaluate(&trial);
                if (score > best) {
                    best = score;
                    move->hand = hand;
                    move->lane = lane;
                    move->target = p.targets ? target : NO_TARGET;
                    found = 1;
                }
            }
        }
    return found;
}
