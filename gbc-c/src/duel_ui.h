/* Included by main.c after its video/input helpers. Rules live in battle.c. */
static Battle duel_state;
static uint8_t duel_hand, duel_lane, duel_target, duel_phase, duel_scroll;
static const uint8_t lane_x[LANES] = { 3, 8, 13 };

static void duel_frame(uint8_t x, uint8_t y)
{
    uint8_t row, col, i = 1, edge;
    /* Ten tiles form a continuous outline, without covering the portrait or seals. */
    for (row = 0; row < 4; ++row)
        for (col = 0; col < 3; ++col) {
            if (col == 1 && row != 0 && row != 3) continue;
            edge = row == 0 || row == 3;
            set_sprite_tile(i, edge ? (col == 1 ? S_SELECT_H : S_SELECT_CORNER) : S_SELECT_V);
            set_sprite_prop(i, (col == 2 ? S_FLIPX : 0) | (row == 3 ? S_FLIPY : 0));
            move_sprite(i++, (x + col) * 8 + 8, (y + row) * 8 + 16);
        }
    SHOW_SPRITES;
}

static void duel_avatar(uint8_t y, uint8_t tile)
{
    uint8_t i;
    for (i = 0; i < 4; ++i) put(i & 1, y + i / 2, tile + i, P_UI);
}

static void duel_effect(uint8_t lane, uint8_t side, uint8_t verb)
{
    uint8_t frame, i, tile, pal;
    tile = verb == VERB_FIGHT ? S_SLASH_0_0 :
        (verb == VERB_GIVE ? S_HEAL_0_0 : S_DRAW_0_0);
    pal = verb == VERB_FIGHT ? 3 : (verb == VERB_GIVE ? 2 : 0);
    hide_sprites();
    for (frame = 0; frame < 3; ++frame) {
        for (i = 0; i < 4; ++i) {
            set_sprite_tile(i + 1, tile + frame * 4 + i);
            set_sprite_prop(i + 1, pal);
            move_sprite(i + 1, lane_x[lane] * 8 + 12 + (i & 1) * 8,
                        (side == ENEMY ? 2 : 7) * 8 + 16 + (i / 2) * 8);
        }
        SHOW_SPRITES;
        delay_frames(4);
    }
    hide_sprites();
}

static void duel_card_caption(uint8_t card)
{
    uint8_t x;
    frect(0, 15, 20, 2, T_BLANK, P_UI);
    if (card == CARD_NONE) return;
    print(0, 15, color_names[C_COLOR(card)], P_UI);
    x = str_len(color_names[C_COLOR(card)]) + 1;
    print(x, 15, shape_names[C_SHAPE(card)], P_UI);
    x += str_len(shape_names[C_SHAPE(card)]) + 1;
    print(x, 15, verb_names[C_VERB(card)], P_UI);
}

static void duel_stats(uint8_t x, uint8_t y, uint8_t card, uint8_t hp, uint8_t pal)
{
    print_u16(x, y, card_attack(card), pal);
    put(x + 1, y, glyph('/'), pal);
    print_u16(x + 2, y, hp, hp < card_health(card) ? P_GOLD : pal);
}

static void duel_render(void)
{
    uint8_t side, lane, i, card, x, amount;
    BattleSide *s = &duel_state.side[PLAYER];
    PlayPreview p;
    felt();
    frect(0, 1, 20, 5, T_TABLE, P_NEUT);
    frect(0, 6, 20, 5, T_TABLE, P_FELT);
    frect(0, 0, 20, 1, T_BLANK, P_UI);
    print(0, 0, "YOU", P_UI);
    print_u16(4, 0, s->hp, P_GOLD);
    print(7, 0, "FOE", P_UI);
    print_u16(11, 0, duel_state.side[ENEMY].hp, P_GOLD);
    print(14, 0, "ACT", P_UI);
    put(17, 0, duel_state.plays ? T_AP_FULL : T_AP_EMPTY, P_GOLD);
    put(18, 0, duel_state.plays > 1 ? T_AP_FULL : T_AP_EMPTY, P_GOLD);
    print(19, 0, duel_state.active == PLAYER ? "P" : "E", P_GOLD);
    duel_avatar(2, T_AVATAR_RIVAL_0 + station * 4);
    duel_avatar(7, T_AVATAR_HERO_0);
    print(0, 1, "FOE", P_NEUT);
    print(0, 6, "YOU", P_FELT);
    for (i = 0; i < 5; ++i) put(18, 1 + i, glyph("ENEMY"[i]), P_NEUT);
    for (i = 0; i < 4; ++i) put(18, 6 + i, glyph("YOUR"[i]), P_FELT);
    put(0, 4, T_SIGIL_0, P_NEUT);
    put(0, 5, T_SIGIL_1, P_NEUT);
    put(0, 9, T_ICO_DECK, P_FELT);
    print_u16(1, 9, s->deck_n, P_FELT);
    frect(0, 10, 3, 1, T_TABLE_TRIM, P_FELT);
    for (side = 0; side < 2; ++side)
        for (lane = 0; lane < LANES; ++lane) {
            uint8_t y = side == ENEMY ? 1 : 6;
            uint8_t pal = side == ENEMY ? P_NEUT : P_FELT;
            BattleSlot *slot = &duel_state.side[side].board[lane];
            if (slot->card != CARD_NONE) {
                draw_card(lane_x[lane], y, slot->card);
                duel_stats(lane_x[lane], y + 4, slot->card, slot->hp, pal);
            } else {
                for (i = 0; i < 12; ++i)
                    put(lane_x[lane] + i % 3, y + i / 3, T_SLOT_0 + i, pal);
                print_u16(lane_x[lane] + 1, y + 4, lane + 1, pal);
            }
            put(lane_x[lane] + 3, y + 4, side == ENEMY ? T_OWNER_DOWN : T_OWNER_UP, pal);
        }
    for (side = 0; side < 2; ++side)
        for (lane = 0; lane < LANES - 1; ++lane) {
            BattleSlot *left = &duel_state.side[side].board[lane];
            BattleSlot *right = &duel_state.side[side].board[lane + 1];
            if (left->card != CARD_NONE && right->card != CARD_NONE &&
                C_COLOR(left->card) == C_COLOR(right->card)) {
                put(lane_x[lane] + 3, side == ENEMY ? 3 : 8, T_LINK, P_GOLD);
                put(lane_x[lane] + 4, side == ENEMY ? 3 : 8, T_LINK, P_GOLD);
            }
        }
    if (duel_hand >= s->hand_n) duel_hand = s->hand_n ? s->hand_n - 1 : 0;
    if (duel_hand < duel_scroll) duel_scroll = duel_hand;
    if (duel_hand >= duel_scroll + 5) duel_scroll = duel_hand - 4;
    frect(0, 11, 20, 1, T_TABLE_TRIM, P_FELT);
    for (i = 0; i < 5; ++i)
        if (duel_scroll + i < s->hand_n)
            draw_card(i * 4, 11, s->hand[duel_scroll + i]);
    if (s->hand_n > 5) {
        print_u16(19, 11, duel_hand + 1, P_FELT);
        put(19, 12, glyph('/'), P_FELT);
        print_u16(19, 13, s->hand_n, P_FELT);
    }
    frect(0, 15, 20, 3, T_BLANK, P_UI);
    hide_sprites();
    if (duel_state.active == ENEMY) {
        print_center(15, "OPPONENT THINKING", P_UI);
        return;
    }
    card = s->hand_n ? s->hand[duel_hand] : CARD_NONE;
    if (card != CARD_NONE)
        put((duel_hand - duel_scroll) * 4 + 3, 14, T_HAND_PICK, P_GOLD);
    duel_card_caption(card);
    if (duel_phase == 3) {
        print_center(16, "END TURN?", P_GOLD);
        print_center(17, "A:YES B:BACK", P_UI);
        return;
    }
    if (duel_phase == 0) {
        if (card != CARD_NONE) duel_frame((duel_hand - duel_scroll) * 4, 11);
        if (!duel_state.plays) print_center(16, "START TO ATTACK", P_GOLD);
        else if (!s->hand_n) print_center(16, "NO CARDS: START", P_GOLD);
        else {
            print(0, 16, "ATK", P_UI);
            print_u16(4, 16, card_attack(card), P_UI);
            print(6, 16, "HP", P_UI);
            print_u16(9, 16, card_health(card), P_UI);
            print(12, 16, "SELECTED", P_GOLD);
        }
        print_center(17, "A:PLAY ST:END SEL:?", P_UI);
    } else if (battle_preview(&duel_state, duel_hand, duel_lane, &p)) {
        duel_frame(lane_x[duel_phase == 2 ? duel_target : duel_lane],
                   duel_phase == 2 && C_VERB(card) == VERB_FIGHT ? 1 : 6);
        set_sprite_tile(0, S_ARROW_RT);
        set_sprite_prop(0, 0);
        x = lane_x[duel_phase == 2 ? duel_target : duel_lane];
        move_sprite(0, x * 8, (duel_phase == 2 && C_VERB(card) == VERB_FIGHT ? 3 : 8) * 8 + 16);
        amount = p.amount;
        if (duel_phase == 2 && C_VERB(card) == VERB_GIVE) {
            BattleSlot *slot = &s->board[duel_target];
            uint8_t missing = card_health(slot->card) - slot->hp;
            if (amount > missing) amount = missing;
        }
        if (C_VERB(card) == VERB_TAKE) {
            uint8_t available = s->deck_n + s->discard_n + p.replacing;
            if (amount > available) amount = available;
            if (amount > HAND_MAX - s->hand_n + 1) amount = HAND_MAX - s->hand_n + 1;
        }
        print(0, 16, p.replacing ? "REPLACE" : "PLACE", P_UI);
        print_u16(p.replacing ? 8 : 6, 16, duel_lane + 1, P_UI);
        if (p.amount == 2) put(10, 16, T_STAR1, P_GOLD);
        print(12, 16, verb_names[C_VERB(card)], P_UI);
        if (!p.targets && C_VERB(card) != VERB_TAKE) amount = 0;
        print_u16(18, 16, amount, P_GOLD);
        print_center(17, duel_phase == 2 ?
            (C_VERB(card) == VERB_FIGHT ? "A:HIT ENEMY B:BACK" : "A:HEAL ALLY B:BACK") :
            "A:PLACE B:BACK", P_UI);
    }
    SHOW_SPRITES;
}

static void duel_inspect(void)
{
    uint8_t group = 0, cur = duel_hand, n, count, card, hp;
    for (;;) {
        BattleSide *s = &duel_state.side[group == 2 ? ENEMY : PLAYER];
        count = group ? LANES : s->hand_n;
        if (cur >= count) cur = 0;
        card = count ? (group ? s->board[cur].card : s->hand[cur]) : CARD_NONE;
        hp = group ? s->board[cur].hp : card_health(card);
        DISPLAY_OFF;
        hide_sprites();
        felt();
        window(0, 0, 20, 17);
        print_center(1, group == 0 ? "HAND" : (group == 1 ? "YOUR BOARD" : "ENEMY BOARD"), P_GOLD);
        print_u16(17, 1, cur + 1, P_UI);
        if (card != CARD_NONE) {
            draw_card(2, 4, card);
            print(7, 4, color_names[C_COLOR(card)], P_UI);
            print(7, 6, shape_names[C_SHAPE(card)], P_UI);
            print(7, 8, verb_names[C_VERB(card)], P_UI);
            print(2, 10, "ATK", P_UI);
            print_u16(6, 10, card_attack(card), P_UI);
            print(9, 10, "HP", P_UI);
            print_u16(12, 10, hp, P_UI);
            put(13, 10, glyph('/'), P_UI);
            print_u16(14, 10, card_health(card), P_UI);
        } else print_center(6, "EMPTY", P_UI);
        print(2, 12, "DECK", P_UI);
        print_u16(7, 12, s->deck_n, P_UI);
        print(10, 12, "DISCARD", P_UI);
        print_u16(18, 12, s->discard_n, P_UI);
        print_center(14, "UP/DOWN:AREA", P_UI);
        print_center(15, "LEFT/RIGHT:CARD", P_UI);
        print_center(17, "B OR SELECT:BACK", P_FELT);
        DISPLAY_ON;
        for (;;) {
            n = tick();
            if (n & (J_B | J_SELECT)) { duel_render(); return; }
            if ((n & J_UP) && group) { --group; cur = 0; break; }
            if ((n & J_DOWN) && group < 2) { ++group; cur = 0; break; }
            if ((n & J_LEFT) && cur) { --cur; break; }
            if ((n & J_RIGHT) && cur + 1 < count) { ++cur; break; }
        }
    }
}

static void duel_commit(uint8_t target)
{
    uint8_t verb = C_VERB(duel_state.side[PLAYER].hand[duel_hand]);
    if (!battle_play(&duel_state, duel_hand, duel_lane, target)) { sfx_bad(); return; }
    duel_phase = 0;
    sfx_play();
    duel_render();
    if (target != NO_TARGET)
        duel_effect(target, verb == VERB_FIGHT ? ENEMY : PLAYER, verb);
    else if (verb == VERB_TAKE) duel_effect(duel_lane, PLAYER, verb);
    if (duel_state.side[PLAYER].hand_n)
        duel_frame((duel_hand - duel_scroll) * 4, 11);
}

static void duel_attack_animation(void)
{
    uint8_t lane;
    for (lane = 0; lane < LANES && duel_state.winner == NO_WINNER; ++lane) {
        if (duel_state.side[duel_state.active].board[lane].card == CARD_NONE) continue;
        duel_render();
        hide_sprites();
        frect(0, 15, 20, 3, T_BLANK, P_UI);
        print_center(15, duel_state.active == PLAYER ? "YOUR ATTACK" : "ENEMY ATTACK", P_GOLD);
        print(6, 16, "LANE", P_UI);
        print_u16(11, 16, lane + 1, P_GOLD);
        put(lane_x[lane] + 1, 5, T_STAR1, P_GOLD);
        delay_frames(15);
        battle_attack_lane(&duel_state, lane);
        sfx_hurt();
        duel_render();
        duel_effect(lane, 1 - duel_state.active, VERB_FIGHT);
        delay_frames(3);
    }
    battle_next_turn(&duel_state);
    duel_phase = duel_hand = duel_scroll = 0;
    duel_render();
}

static uint8_t battle_duel(void)
{
    uint8_t n, i, card;
    PlayPreview p;
    BattleMove move;
    battle_init(&duel_state, coll, coll_n, opponent_decks[station], STARTER_SIZE,
                ((uint16_t)rand() << 8) | (uint8_t)rand());
    duel_hand = duel_lane = duel_target = duel_phase = duel_scroll = 0;
    DISPLAY_OFF;
    duel_render();
    screen_open(pals_table);
    while (duel_state.winner == NO_WINNER) {
        if (duel_state.active == ENEMY) {
            delay_frames(25);
            if (duel_state.plays && ai_choose(&duel_state, &move)) {
                battle_play(&duel_state, move.hand, move.lane, move.target);
                sfx_play();
                duel_render();
                frect(0, 15, 20, 2, T_BLANK, P_UI);
                card = duel_state.side[ENEMY].board[move.lane].card;
                print(0, 15, "FOE PLAYS", P_UI);
                print(10, 15, verb_names[C_VERB(card)], P_GOLD);
                put(lane_x[move.lane] + 1, 5, T_STAR1, P_GOLD);
                if (move.target != NO_TARGET)
                    duel_effect(move.target, C_VERB(card) == VERB_FIGHT ? PLAYER : ENEMY, C_VERB(card));
                else if (C_VERB(card) == VERB_TAKE) duel_effect(move.lane, ENEMY, VERB_TAKE);
                delay_frames(30);
            } else duel_attack_animation();
            continue;
        }
        n = tick();
        if (n & J_SELECT) { duel_inspect(); continue; }
        if (n & J_B) {
            if (duel_phase == 2) duel_phase = 1;
            else duel_phase = 0;
            duel_render();
            continue;
        }
        if ((n & J_START) && duel_phase == 0) {
            if (duel_state.plays) { duel_phase = 3; duel_render(); }
            else duel_attack_animation();
            continue;
        }
        if (n & (J_LEFT | J_RIGHT)) {
            if (duel_phase == 0) {
                if ((n & J_LEFT) && duel_hand) --duel_hand;
                if ((n & J_RIGHT) && duel_hand + 1 < duel_state.side[PLAYER].hand_n) ++duel_hand;
            } else if (duel_phase == 1) {
                if ((n & J_LEFT) && duel_lane) --duel_lane;
                if ((n & J_RIGHT) && duel_lane < LANES - 1) ++duel_lane;
            } else if (duel_phase == 2) {
                battle_preview(&duel_state, duel_hand, duel_lane, &p);
                i = duel_target;
                do {
                    i = (n & J_LEFT) ? (i ? i - 1 : LANES - 1) : (i + 1) % LANES;
                } while (!(p.targets & (1 << i)));
                duel_target = i;
            }
            sfx_cursor();
            duel_render();
        }
        if (n & J_A) {
            if (duel_phase == 3) { duel_attack_animation(); continue; }
            if (duel_phase == 0) {
                if (!duel_state.plays || !duel_state.side[PLAYER].hand_n) { sfx_bad(); continue; }
                duel_phase = 1;
                duel_render();
            } else if (duel_phase == 1 && battle_preview(&duel_state, duel_hand, duel_lane, &p)) {
                if (!p.targets) duel_commit(NO_TARGET);
                else {
                    duel_phase = 2;
                    for (i = 0; i < LANES; ++i) if (p.targets & (1 << i)) break;
                    duel_target = i;
                    duel_render();
                }
            } else if (duel_phase == 2) duel_commit(duel_target);
        }
    }
    hide_sprites();
    frect(0, 15, 20, 3, T_BLANK, P_UI);
    print_center(15, duel_state.winner == PLAYER ? "DUEL WON!" : "DUEL LOST", P_GOLD);
    print_center(17, "A:CONTINUE", P_UI);
    if (duel_state.winner == PLAYER) sfx_win(); else sfx_lose();
    wait_press(J_A);
    screen_close();
    return duel_state.winner == PLAYER;
}
