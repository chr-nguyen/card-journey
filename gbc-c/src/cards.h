#ifndef CARDS_H
#define CARDS_H
#include <stdint.h>

#define CARD(c,s,v) ((uint8_t)(((c) << 5) | ((s) << 3) | (v)))
#define C_COLOR(k) (((uint8_t)(k) >> 5) & 3)
#define C_SHAPE(k) (((uint8_t)(k) >> 3) & 3)
#define C_VERB(k) ((uint8_t)(k) & 7)
#define CARD_NONE 255
#define COLOR_RUBY 0
#define COLOR_JADE 2
#define VERB_FIGHT 1
#define VERB_TAKE 4
#define VERB_GIVE 5
#define DECK_MAX 20
#define STARTER_SIZE 10

extern const uint8_t starter_deck[STARTER_SIZE];
extern const uint8_t opponent_decks[3][STARTER_SIZE];
extern const char * const verb_names[6];
uint8_t card_valid(uint8_t card);
uint8_t card_attack(uint8_t card);
uint8_t card_health(uint8_t card);
uint8_t card_reward(uint8_t index);
#endif
