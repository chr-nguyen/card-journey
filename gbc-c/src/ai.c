#include "ai.h"

/* Scratch space is static to keep the Game Boy stack small. Evaluation uses
   only boards/health and the active side's hand count, never hidden enemy cards. */
static Battle trial;
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
    return score;
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
