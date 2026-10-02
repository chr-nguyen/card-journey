#ifndef AI_H
#define AI_H
#include "battle.h"
typedef struct { uint8_t hand, lane, target; } BattleMove;
uint8_t ai_choose(const Battle *b, BattleMove *move);
#endif
