#include "battle.h"
#include "ai.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define RF CARD(0,0,VERB_FIGHT)
#define RT CARD(0,0,VERB_TAKE)
#define RG CARD(0,1,VERB_GIVE)
#define JF CARD(2,0,VERB_FIGHT)

static Battle b;
static void empty_battle(void)
{
    uint8_t s, i;
    memset(&b, 0, sizeof(b));
    b.winner = NO_WINNER;
    b.plays = 2;
    b.rng = 123;
    for (s = 0; s < 2; ++s) {
        b.side[s].hp = BATTLE_HP;
        for (i = 0; i < LANES; ++i) b.side[s].board[i].card = CARD_NONE;
    }
}
static void slot(uint8_t side, uint8_t lane, uint8_t card, uint8_t hp)
{
    b.side[side].board[lane].card = card;
    b.side[side].board[lane].hp = hp;
}
static void test_definitions_and_initialization(void)
{
    Battle copy;
    uint8_t i, invalid = CARD(1,0,1);
    for (i = 0; i < 18; ++i) assert(card_valid(card_reward(i)));
    assert(!card_valid(CARD_NONE));
    assert(!card_valid(CARD(0,0,0)));
    assert(!card_valid(CARD(0,3,1)));
    assert(card_attack(CARD(0,2,1)) == 3);
    assert(card_health(CARD(2,1,5)) == 4);
    assert(battle_init(&b, starter_deck, 10, starter_deck, 10, 77));
    assert(battle_init(&copy, starter_deck, 10, starter_deck, 10, 77));
    assert(!memcmp(&b, &copy, sizeof(b)));
    assert(b.side[PLAYER].hand_n == 5 && b.side[PLAYER].deck_n == 5);
    assert(b.plays == 1);
    assert(b.side[ENEMY].hand_n == 0 && b.side[ENEMY].deck_n == 10);
    assert(!battle_init(&b, &invalid, 1, starter_deck, 10, 1));
    assert(!battle_init(&b, starter_deck, 0, starter_deck, 10, 1));
    assert(!memcmp(&b, &copy, sizeof(b))); /* Failed setup is non-mutating. */
}
static void test_connections(void)
{
    PlayPreview p;
    empty_battle();
    b.side[PLAYER].hand[0] = RF;
    b.side[PLAYER].hand_n = 1;
    slot(PLAYER, 2, RF, 2);
    slot(ENEMY, 0, RF, 2);
    assert(battle_preview(&b, 0, 0, &p) && p.amount == 1);
    slot(PLAYER, 0, RF, 2);
    assert(battle_preview(&b, 0, 1, &p) && p.amount == 2); /* Two neighbors, one boost. */
    slot(PLAYER, 1, JF, 2);
    assert(battle_preview(&b, 0, 1, &p) && p.replacing && p.amount == 2);
    slot(PLAYER, 0, JF, 2);
    slot(PLAYER, 2, JF, 2);
    slot(PLAYER, 1, RF, 2);
    assert(battle_preview(&b, 0, 1, &p) && p.amount == 1); /* Outgoing card cannot connect. */
}
static void test_targets_replacement_and_limits(void)
{
    Battle copy;
    PlayPreview p;
    empty_battle();
    b.side[PLAYER].hand[0] = RF;
    b.side[PLAYER].hand[1] = JF;
    b.side[PLAYER].hand_n = 2;
    slot(PLAYER, 0, RG, 3);
    slot(PLAYER, 1, RF, 2);
    slot(ENEMY, 2, JF, 2);
    assert(battle_preview(&b, 0, 0, &p) && p.targets == 4 && p.amount == 2);
    copy = b;
    assert(!battle_play(&b, 0, 0, 0));
    assert(!battle_play(&b, 0, 0, NO_TARGET));
    assert(!memcmp(&b, &copy, sizeof(b)));
    assert(battle_play(&b, 0, 0, 2));
    assert(b.side[PLAYER].discard_n == 1 && b.side[PLAYER].discard[0] == RG);
    assert(b.side[ENEMY].board[2].card == CARD_NONE);
    assert(b.side[ENEMY].discard_n == 1);
    assert(b.side[PLAYER].hand_n == 1 && b.side[PLAYER].hand[0] == JF);
    assert(b.plays == 1);
    assert(battle_play(&b, 0, 2, NO_TARGET));
    assert(b.plays == 0);
    copy = b;
    assert(!battle_play(&b, 0, 0, NO_TARGET));
    assert(!memcmp(&b, &copy, sizeof(b)));
}
static void test_healing(void)
{
    PlayPreview p;
    empty_battle();
    b.side[PLAYER].hand[0] = RG;
    b.side[PLAYER].hand_n = 1;
    slot(PLAYER, 0, RG, 3);
    slot(PLAYER, 1, RG, 1);
    assert(battle_preview(&b, 0, 1, &p) && p.targets == 1);
    assert(!battle_play(&b, 0, 1, 1)); /* Cannot heal self/outgoing card. */
    assert(battle_play(&b, 0, 1, 0));
    assert(b.side[PLAYER].board[0].hp == 4);
    assert(b.side[PLAYER].board[1].hp == 4);
    empty_battle();
    b.side[PLAYER].hand[0] = RG;
    b.side[PLAYER].hand_n = 1;
    assert(battle_preview(&b, 0, 0, &p) && p.targets == 0);
    assert(battle_play(&b, 0, 0, NO_TARGET));
}
static void test_draw_and_recycle(void)
{
    empty_battle();
    b.side[PLAYER].discard[0] = RF;
    b.side[PLAYER].discard[1] = JF;
    b.side[PLAYER].discard_n = 2;
    slot(PLAYER, 0, RG, 3);
    assert(battle_draw(&b, PLAYER, 5) == 2);
    assert(!b.side[PLAYER].deck_n && !b.side[PLAYER].discard_n);
    assert(b.side[PLAYER].board[0].card == RG && b.side[PLAYER].board[0].hp == 3);
    assert(battle_draw(&b, PLAYER, 1) == 0);
    empty_battle();
    b.side[PLAYER].hand[0] = RT;
    b.side[PLAYER].hand_n = 1;
    slot(PLAYER, 0, RF, 2);
    slot(PLAYER, 1, RG, 4);
    assert(battle_play(&b, 0, 1, NO_TARGET)); /* Replacement enters reshuffle. */
    assert(b.side[PLAYER].hand_n == 1 && b.side[PLAYER].hand[0] == RG);
    assert(b.side[PLAYER].board[1].card == RT);
    empty_battle();
    b.side[PLAYER].hand_n = HAND_MAX;
    b.side[PLAYER].deck_n = 1;
    b.side[PLAYER].deck[0] = RF;
    assert(!battle_draw(&b, PLAYER, 2));
    assert(b.side[PLAYER].deck_n == 1);
    empty_battle();
    b.side[PLAYER].hand[0] = RT;
    b.side[PLAYER].hand[1] = RT;
    b.side[PLAYER].hand[2] = b.side[PLAYER].hand[3] = b.side[PLAYER].hand[4] = RF;
    b.side[PLAYER].hand_n = 5;
    b.side[PLAYER].deck[0] = b.side[PLAYER].deck[1] =
        b.side[PLAYER].deck[2] = b.side[PLAYER].deck[3] = JF;
    b.side[PLAYER].deck_n = 4;
    slot(PLAYER, 0, RF, 2);
    assert(battle_play(&b, 0, 1, NO_TARGET));
    assert(b.side[PLAYER].hand_n == 6);
    assert(battle_play(&b, 0, 2, NO_TARGET));
    assert(b.side[PLAYER].hand_n == 7 && b.plays == 0);
}
static void test_ai_information_boundary(void)
{
    Battle copy;
    BattleMove first, second;
    assert(battle_init(&b, starter_deck, 10, starter_deck, 10, 77));
    copy = b;
    memset(copy.side[ENEMY].deck, RG, sizeof(copy.side[ENEMY].deck));
    memset(copy.side[ENEMY].hand, RG, sizeof(copy.side[ENEMY].hand));
    assert(ai_choose(&b, &first));
    assert(ai_choose(&copy, &second));
    assert(!memcmp(&first, &second, sizeof(first)));
}
static void test_combat_and_cleanup(void)
{
    empty_battle();
    slot(PLAYER, 0, CARD(0,2,1), 1);
    slot(ENEMY, 0, JF, 2);
    battle_attack_lane(&b, 0);
    assert(b.side[ENEMY].board[0].card == CARD_NONE);
    assert(b.side[ENEMY].hp == BATTLE_HP); /* No spillover. */
    assert(b.side[PLAYER].board[0].hp == 1); /* No retaliation. */
    b.side[PLAYER].hand[0] = RF;
    b.side[PLAYER].hand_n = 1;
    b.side[ENEMY].deck[0] = JF;
    b.side[ENEMY].deck_n = 1;
    battle_next_turn(&b);
    assert(b.active == ENEMY && b.plays == 2);
    assert(b.side[PLAYER].hand_n == 0 && b.side[PLAYER].discard_n == 1);
    assert(b.side[ENEMY].hand_n == 2); /* Draw pile plus the defeated card recycle. */
    empty_battle();
    slot(PLAYER, 0, RF, 2);
    slot(PLAYER, 1, RF, 2);
    slot(ENEMY, 1, RG, 4);
    b.side[ENEMY].hp = 1;
    battle_end_turn(&b);
    assert(b.winner == PLAYER && b.side[ENEMY].hp == 0);
    assert(b.side[ENEMY].board[1].hp == 4); /* Lethal stops later attacks. */
    assert(b.active == PLAYER);
}
static void check_zones(const uint8_t *deck0, const uint8_t *deck1)
{
    unsigned expected[256], actual[256];
    uint8_t side, i;
    for (side = 0; side < 2; ++side) {
        const uint8_t *deck = side ? deck1 : deck0;
        BattleSide *s = &b.side[side];
        memset(expected, 0, sizeof(expected));
        memset(actual, 0, sizeof(actual));
        for (i = 0; i < STARTER_SIZE; ++i) ++expected[deck[i]];
        assert(s->hand_n <= HAND_MAX && s->deck_n <= DECK_MAX && s->discard_n <= DECK_MAX);
        for (i = 0; i < s->hand_n; ++i) ++actual[s->hand[i]];
        for (i = 0; i < s->deck_n; ++i) ++actual[s->deck[i]];
        for (i = 0; i < s->discard_n; ++i) ++actual[s->discard[i]];
        for (i = 0; i < LANES; ++i) if (s->board[i].card != CARD_NONE) {
            ++actual[s->board[i].card];
            assert(s->board[i].hp && s->board[i].hp <= card_health(s->board[i].card));
        }
        assert(!memcmp(expected, actual, sizeof(expected)));
    }
}
static void test_seeded_battles(void)
{
    unsigned seed, wins = 0, losses = 0, stalls = 0, turns = 0;
    BattleMove move;
    for (seed = 1; seed <= 200; ++seed) {
        const uint8_t *enemy = opponent_decks[seed % 3];
        assert(battle_init(&b, starter_deck, 10, enemy, 10, (uint16_t)seed));
        while (b.winner == NO_WINNER && b.turn <= 120) {
            check_zones(starter_deck, enemy);
            while (b.plays && ai_choose(&b, &move)) {
                assert(battle_play(&b, move.hand, move.lane, move.target));
                check_zones(starter_deck, enemy);
            }
            battle_end_turn(&b);
            check_zones(starter_deck, enemy);
        }
        turns += b.turn;
        if (b.winner == PLAYER) ++wins;
        else if (b.winner == ENEMY) ++losses;
        else ++stalls;
    }
    printf("200 seeded AI battles: %u player wins, %u losses, %u stalls; %.1f turns average\n",
           wins, losses, stalls, turns / 200.0);
    assert(stalls == 0);
}
static void compare_openings(void)
{
    unsigned seed, opening, wins, turns;
    BattleMove move;
    for (opening = 1; opening <= 2; ++opening) {
        wins = turns = 0;
        for (seed = 1; seed <= 200; ++seed) {
            assert(battle_init(&b, starter_deck, 10, starter_deck, 10, (uint16_t)seed));
            b.plays = (uint8_t)opening;
            while (b.winner == NO_WINNER && b.turn <= 120) {
                while (b.plays && ai_choose(&b, &move))
                    assert(battle_play(&b, move.hand, move.lane, move.target));
                battle_end_turn(&b);
            }
            assert(b.winner != NO_WINNER);
            if (b.winner == PLAYER) ++wins;
            turns += b.turn;
        }
        printf("Mirrored decks, %u opening plays: first player won %u/200; %.1f turns average\n",
               opening, wins, turns / 200.0);
    }
}
int main(void)
{
    test_definitions_and_initialization();
    test_connections();
    test_targets_replacement_and_limits();
    test_healing();
    test_draw_and_recycle();
    test_combat_and_cleanup();
    test_ai_information_boundary();
    test_seeded_battles();
    compare_openings();
    puts("Battle rules tests passed.");
    return 0;
}
