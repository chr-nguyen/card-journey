#ifndef AI_H
#define AI_H
#include "battle.h"
typedef struct { uint8_t hand, lane, target; } BattleMove;
uint8_t ai_choose(const Battle *b, BattleMove *move);
uint8_t ai_keep(const Battle *b);
#endif
