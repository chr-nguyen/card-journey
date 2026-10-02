/* CARD JOURNEY - a three-lane deck-building battler for Game Boy Color. */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/hardware.h>
#include <rand.h>
#include <stdint.h>
#include <string.h>
#include "assets.h"
#include "cards.h"
#include "battle.h"
#include "ai.h"

/* ------------------------------------------------------------------ */
/*  Cards                                                              */
/* ------------------------------------------------------------------ */

#define MAXC DECK_MAX
#define N_STATIONS 3

/* ------------------------------------------------------------------ */
/*  Strings                                                            */
/* ------------------------------------------------------------------ */

static const char * const station_names[N_STATIONS] = {
    "BASE CAMP", "ICE CHASM", "ELDER GATE",
};
static const char * const color_names[4] = {
    "RUBY", "AMBER", "JADE", "AZURE",
};
static const char * const shape_names[4] = {
    "CIRCLE", "SQUARE", "TRIANGLE", "DIAMOND",
};


/* ------------------------------------------------------------------ */
/*  Game state                                                         */
/* ------------------------------------------------------------------ */

static uint8_t coll[MAXC];      /* your pack (whole collection)   */
static uint8_t coll_n;
static uint8_t station;         /* next station to challenge 0..9 */
static uint8_t hearts, hearts_max;
static uint8_t joy_prev;

/* ------------------------------------------------------------------ */
/*  Palettes                                                           */
/* ------------------------------------------------------------------ */

/* the card-table set: duels, trail, rewards, pack view, help */
#define P_UI    0
#define P_RUBY  1               /* P_RUBY + color = that card palette */
#define P_NEUT  5
#define P_FELT  6
#define P_GOLD  7
static const palette_color_t pals_table[32] = {
    RGB( 3, 4, 7), RGB( 9,10,14), RGB(16,17,20), RGB(28,27,22),
    RGB(26,25,19), RGB(24, 8,12), RGB(13, 5,10), RGB( 3, 3, 6),
    RGB(27,25,19), RGB(26,18, 8), RGB(14, 9, 7), RGB( 3, 3, 6),
    RGB(25,27,21), RGB(10,21,16), RGB( 4,11,12), RGB( 3, 3, 6),
    RGB(25,26,24), RGB(12,16,24), RGB( 7, 8,15), RGB( 3, 3, 6),
    RGB( 9, 6,13), RGB(10, 7,14), RGB(18,13,22), RGB(30,25,29), /* enemy */
    RGB( 3, 9,10), RGB( 4,10,11), RGB(10,19,18), RGB(25,29,25), /* yours */
    RGB( 3, 7, 9), RGB(25,22,12), RGB(10,16,17), RGB(31,30,19),
};

/* Antarctic night, blue ice, and pale green light in the elder masonry. */
#define P_SCENE 1
#define P_RED   4
static const palette_color_t pals_map[32] = {
    RGB( 3, 4, 7), RGB( 9,10,14), RGB(16,17,20), RGB(28,27,22),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 3, 4, 7), RGB(28, 8,10), RGB(15, 4, 8), RGB(29,23,18),
    RGB( 2, 4, 9), RGB(24,29,22), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 3, 4, 7), RGB(28,27,21), RGB( 8,15,17), RGB(28,27,21),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
};

#define TP_SKY0 0
#define TP_GND  0
#define TP_GOLD 7
static const palette_color_t pals_title[32] = {
    RGB( 2, 4, 9), RGB( 7,11,16), RGB(12,18,21), RGB(27,29,23),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(24,29,22), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(25,29,28), RGB(10,16,21), RGB( 1, 2, 5),
    RGB( 2, 4, 9), RGB(12,19,19), RGB( 7,12,16), RGB(24,29,22),
};

static const palette_color_t pals_spr[16] = {
    0, RGB(31,29,19), RGB(31,23, 8), RGB(17,10, 5), /* gold cursor */
    0, RGB(30,28,22), RGB(23, 8, 7), RGB( 2, 2, 5), /* explorer     */
    0, RGB(25,31,24), RGB( 9,24,14), RGB( 3,11,10), /* healing      */
    0, RGB(31,29,22), RGB(31,12, 8), RGB(16, 4, 7), /* sword impact */
};

/* live palette buffer so screens can fade in and out */
static palette_color_t pal_buf[32];
static const palette_color_t *pal_cur;

/* ------------------------------------------------------------------ */
/*  Low-level video helpers                                            */
/* ------------------------------------------------------------------ */

/* Compose a complete duel in RAM. Intermediate clears never reach VRAM.
 * Direct drawing also maintains the cache, including effects and inspection. */
static uint8_t video_tiles[360], video_attrs[360];
static uint8_t next_tiles[360], next_attrs[360];
static uint8_t video_buffered;

static void put(uint8_t x, uint8_t y, uint8_t t, uint8_t pal)
{
    uint16_t at = (uint16_t)y * 20 + x;
    if (video_buffered) {
        next_tiles[at] = t;
        next_attrs[at] = pal;
        return;
    }
    video_tiles[at] = t;
    video_attrs[at] = pal;
    set_bkg_tile_xy(x, y, t);
    VBK_REG = 1;
    set_bkg_tile_xy(x, y, pal);
    VBK_REG = 0;
}

static void video_present(void)
{
    uint8_t x, y, start, end, i;
    uint16_t row;
    video_buffered = 0;
    for (y = 0; y < 18; ++y) {
        row = (uint16_t)y * 20;
        start = 20;
        end = 0;
        for (x = 0; x < 20; ++x) {
            if (next_tiles[row+x] != video_tiles[row+x] ||
                next_attrs[row+x] != video_attrs[row+x]) {
                if (start == 20) start = x;
                end = x + 1;
            }
        }
        if (start == 20) continue;
        /* A maximum of twenty tile IDs and attributes per VBlank. */
        if (LCDC_REG & LCDCF_ON) wait_vbl_done();
        VBK_REG = 1;
        set_bkg_tiles(start, y, end-start, 1, next_attrs + row + start);
        VBK_REG = 0;
        set_bkg_tiles(start, y, end-start, 1, next_tiles + row + start);
        for (i = start; i < end; ++i) {
            video_tiles[row+i] = next_tiles[row+i];
            video_attrs[row+i] = next_attrs[row+i];
        }
    }
}

static void frect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                      uint8_t t, uint8_t pal)
{
    uint8_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            put(x + i, y + j, t, pal);
}

/* map an ASCII char to its font tile (uppercase text only) */
static uint8_t glyph(char c)
{
    uint8_t i;
    if (c == ' ') return T_BLANK;
    if (c >= 'A' && c <= 'Z') return FONT_BASE + (c - 'A');
    if (c >= '0' && c <= '9') return FONT_BASE + 26 + (c - '0');
    for (i = 36; i < FONT_COUNT; i++)
        if (font_chars[i] == (uint8_t)c) return FONT_BASE + i;
    return T_BLANK;
}

static void print(uint8_t x, uint8_t y, const char *s, uint8_t pal)
{
    while (*s) put(x++, y, glyph(*s++), pal);
}

/* unsigned number, left-aligned; returns x after the last digit */
static uint8_t print_u16(uint8_t x, uint8_t y, uint16_t v, uint8_t pal)
{
    char buf[6];
    uint8_t n = 0;
    if (!v) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (uint8_t)(v % 10); v /= 10; }
    while (n) put(x++, y, glyph(buf[--n]), pal);
    return x;
}

static uint8_t str_len(const char *s)
{
    uint8_t n = 0;
    while (*s++) n++;
    return n;
}

static void print_center(uint8_t y, const char *s, uint8_t pal)
{
    print((uint8_t)(20 - str_len(s)) >> 1, y, s, pal);
}

/* framed dark dialog box */
static void window(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t i;
    put(x, y, T_WIN_TL, P_UI);
    put(x + w - 1, y, T_WIN_TR, P_UI);
    put(x, y + h - 1, T_WIN_BL, P_UI);
    put(x + w - 1, y + h - 1, T_WIN_BR, P_UI);
    for (i = 1; i < w - 1; i++) {
        put(x + i, y, T_WIN_T, P_UI);
        put(x + i, y + h - 1, T_WIN_B, P_UI);
    }
    for (i = 1; i < h - 1; i++) {
        put(x, y + i, T_WIN_L, P_UI);
        put(x + w - 1, y + i, T_WIN_R, P_UI);
    }
    frect(x + 1, y + 1, w - 2, h - 2, T_BLANK, P_UI);
}

/* ------------------------------------------------------------------ */
/*  Palette fades (very 90s screen transitions)                        */
/* ------------------------------------------------------------------ */

static void pal_apply(void)
{
    set_bkg_palette(0, 8, pal_buf);
}

/* k = 0 (all white) .. 4 (true colors) */
static void pal_step(uint8_t k)
{
    uint8_t i;
    for (i = 0; i < 32; i++) {
        palette_color_t c = pal_cur[i];
        uint8_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        r = 31 - (uint8_t)(((31 - r) * k) >> 2);
        g = 31 - (uint8_t)(((31 - g) * k) >> 2);
        b = 31 - (uint8_t)(((31 - b) * k) >> 2);
        pal_buf[i] = RGB(r, g, b);
    }
    pal_apply();
}

static void delay_frames(uint8_t n)
{
    while (n--) wait_vbl_done();
}

static void fade_in(const palette_color_t *set)
{
    uint8_t k;
    pal_cur = set;
    for (k = 0; k <= 4; k++) { pal_step(k); delay_frames(3); }
}

static void fade_out(void)
{
    uint8_t k;
    for (k = 4; k != 0xFF; k--) { pal_step(k); delay_frames(3); }
}

/* ------------------------------------------------------------------ */
/*  Sound                                                              */
/* ------------------------------------------------------------------ */

static void snd_init(void)
{
    NR52_REG = 0x80;
    NR51_REG = 0xFF;
    NR50_REG = 0x77;
}

static void note(uint8_t duty_len, uint8_t env, uint16_t f)
{
    NR11_REG = duty_len;
    NR12_REG = env;
    NR13_REG = (uint8_t)f;
    NR14_REG = 0x80 | (uint8_t)(f >> 8);
}

static void thud(uint8_t env, uint8_t poly)
{
    NR41_REG = 0x00;
    NR42_REG = env;
    NR43_REG = poly;
    NR44_REG = 0x80;
}

static void sfx_cursor(void) { note(0x81, 0x41, 0x740); }
static void sfx_pick(void)   { note(0x81, 0x71, 0x783); }
static void sfx_play(void)   { NR10_REG = 0x17; note(0x81, 0x91, 0x680);
                               NR10_REG = 0x00; }
static void sfx_bad(void)    { note(0x81, 0x61, 0x400); }
static void sfx_hurt(void)   { thud(0xA2, 0x5C); }

static void sfx_win(void)
{
    note(0x81, 0x81, 0x6B0); delay_frames(6);
    note(0x81, 0x81, 0x716); delay_frames(6);
    note(0x81, 0x91, 0x763); delay_frames(10);
}

static void sfx_lose(void)
{
    thud(0xD3, 0x6E);
    note(0x81, 0xA3, 0x300); delay_frames(10);
    note(0x81, 0xA4, 0x240); delay_frames(12);
}

static void sfx_heart(void)
{
    note(0x81, 0x71, 0x700); delay_frames(5);
    note(0x81, 0x81, 0x76C); delay_frames(5);
}

/* ------------------------------------------------------------------ */
/*  Input                                                              */
/* ------------------------------------------------------------------ */

/* one frame tick; returns freshly pressed buttons */
static uint8_t tick(void)
{
    uint8_t k, n;
    wait_vbl_done();
    k = joypad();
    n = k & ~joy_prev;
    joy_prev = k;
    return n;
}

static void wait_press(uint8_t mask)
{
    while (!(tick() & mask)) ;
}

/* ------------------------------------------------------------------ */
/*  RNG / pack utilities                                               */
/* ------------------------------------------------------------------ */

static uint8_t rnd(uint8_t n)
{
    return (uint8_t)rand() % n;
}

static uint8_t random_card(void)
{
    return card_reward(rnd(REWARD_COUNT));
}


static uint8_t strongest_color(void)
{
    uint8_t counts[4] = { 0, 0, 0, 0 }, i, best = 0;
    for (i = 0; i < coll_n; i++) counts[C_COLOR(coll[i])]++;
    for (i = 1; i < 4; i++)
        if (counts[i] > counts[best]) best = i;
    return best;
}

/* ------------------------------------------------------------------ */
/*  Card drawing                                                       */
/* ------------------------------------------------------------------ */

static void draw_card(uint8_t x, uint8_t y, uint8_t card)
{
    uint8_t pal = P_RUBY + C_COLOR(card), i;
    uint8_t portrait = T_CARD_WISP_0 + C_SHAPE(card) * 6;
    put(x, y, T_CARD_TL, pal);
    put(x+1, y, T_SEAL_CIRCLE + C_SHAPE(card), pal);
    put(x+2, y, T_CARD_TR, pal);
    for (i = 0; i < 6; ++i)
        put(x + i % 3, y + 1 + i / 3, portrait + i, pal);
    put(x, y+3, T_CARD_BL, pal);
    put(x+1, y+3, T_ICO_MOVE + C_VERB(card), pal);
    put(x+2, y+3, T_CARD_BR, pal);
}


/* "RUBY TRIANGLE" style caption, centered on row y. */
static void print_card_name(uint8_t y, uint8_t card, uint8_t pal)
{
    uint8_t x = (uint8_t)(19 - (str_len(color_names[C_COLOR(card)]) +
                 str_len(shape_names[C_SHAPE(card)]))) >> 1;
    frect(1, y, 18, 1, T_BLANK, pal);
    print(x, y, color_names[C_COLOR(card)], pal);
    x += str_len(color_names[C_COLOR(card)]) + 1;
    print(x, y, shape_names[C_SHAPE(card)], pal);
}

/* quiet stone backdrop used by all card screens */
static void felt(void)
{
    frect(0, 0, 20, 18, T_TABLE, P_FELT);
}

static void hide_sprites(void)
{
    uint8_t i;
    for (i = 0; i < 11; i++) move_sprite(i, 0, 0);
}

static void screen_open(const palette_color_t *set)
{
    DISPLAY_ON;
    fade_in(set);
}

static void screen_close(void)
{
    fade_out();
    hide_sprites();
    DISPLAY_OFF;
}

/* ------------------------------------------------------------------ */
/*  Big 2x letters for the title (scaled from the font at runtime)     */
/* ------------------------------------------------------------------ */

#define DYN_BASE BG_TILE_COUNT
static const uint8_t exp4[16] = {
    0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,
    0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF,
};

/* returns next free dynamic tile index */
static uint8_t big_text(const char *s, uint8_t x, uint8_t y,
                        uint8_t pal, uint8_t dyn)
{
    static uint8_t buf[64];
    uint8_t r, lo, hi;
    const uint8_t *src;
    while (*s) {
        char c = *s++;
        if (c == ' ') { x += 2; continue; }
        src = bg_tiles + (uint16_t)glyph(c) * 16;
        for (r = 0; r < 8; r++) {
            uint8_t half = (r >= 4) ? 32 : 0;
            uint8_t row0 = (uint8_t)((r << 1) & 7) << 1;
            lo = src[r << 1];
            hi = src[(r << 1) + 1];
            buf[half + row0]      = exp4[lo >> 4];
            buf[half + row0 + 1]  = exp4[hi >> 4];
            buf[half + row0 + 2]  = exp4[lo >> 4];
            buf[half + row0 + 3]  = exp4[hi >> 4];
            buf[half + 16 + row0]     = exp4[lo & 15];
            buf[half + 16 + row0 + 1] = exp4[hi & 15];
            buf[half + 16 + row0 + 2] = exp4[lo & 15];
            buf[half + 16 + row0 + 3] = exp4[hi & 15];
        }
        set_bkg_data(dyn, 4, buf);
        put(x, y, dyn, pal);         put(x + 1, y, dyn + 1, pal);
        put(x, y + 1, dyn + 2, pal); put(x + 1, y + 1, dyn + 3, pal);
        dyn += 4;
        x += 2;
    }
    return dyn;
}

/* ------------------------------------------------------------------ */
/*  Title screen                                                       */
/* ------------------------------------------------------------------ */

/* Scene tiles live in CGB bank 1; card/font/sprite patterns stay in bank 0. */
static void load_scene(const uint8_t *tiles, uint8_t count,
                       const uint8_t *map, uint8_t y, uint8_t rows)
{
    uint8_t x, row;
    VBK_REG = 1;
    set_bkg_data(0, count, tiles);
    VBK_REG = 0;
    for (row = y; row < y + rows; ++row)
        for (x = 0; x < 20; ++x)
            put(x, row, *map++, 0x08 | P_SCENE);
}

static void explorer(uint8_t x, uint8_t y, uint8_t frame)
{
    uint8_t i;
    for (i = 0; i < 4; ++i) {
        set_sprite_tile(1+i, (frame ? S_EXPLORER_B_0 : S_EXPLORER_A_0)+i);
        set_sprite_prop(1+i, 1);
        move_sprite(1+i, x+(i&1)*8, y+(i/2)*8);
    }
    SHOW_SPRITES;
}

static void draw_night_scene(void)
{
    uint8_t dyn;
    load_scene(title_scene_tiles, TITLE_SCENE_COUNT, title_scene_map, 0, 18);
    dyn = big_text("CARD", 6, 1, TP_GOLD, DYN_BASE);
    big_text("JOURNEY", 3, 3, TP_GOLD, dyn);
    print_center(5, "BEYOND THE ICE", TP_SKY0);
    explorer(28, 116, 0);
}

static void title_screen(void)
{
    uint8_t t = 0;
    uint16_t seed = 0x1234;

    DISPLAY_OFF;
    draw_night_scene();
    print_center(16, "PRESS START", TP_GND);
    screen_open(pals_title);

    for (;;) {
        uint8_t n = tick();
        seed++;
        t++;
        if (n & J_START) break;
        /* Keep the call to action readable while the explorer's scarf moves. */
        explorer(28, 116, (t >> 5) & 1);
    }
    initrand(seed + DIV_REG);
    sfx_pick();
    screen_close();
}

/* ------------------------------------------------------------------ */
/*  Help screens (SELECT on the map)                                   */
/* ------------------------------------------------------------------ */

static void help_page(const char * const *lines, uint8_t n,
                      const char *heading)
{
    uint8_t i;
    DISPLAY_OFF;
    felt();
    window(0, 0, 20, 17);
    print_center(1, heading, P_GOLD);
    for (i = 0; i < n; i++)
        print(1, 3 + i + i / 4, lines[i], P_UI);
    print_center(17, "A:NEXT", P_FELT);
    screen_open(pals_table);
    wait_press(J_A | J_B | J_START);
    sfx_pick();
    screen_close();
}

static void help_screens(void)
{
    static const char * const p1[8] = {
        "PLAY UP TO 2 CARDS.", "FIRST TURN: 1 PLAY.",
        "START: END OR KEEP.", "CARDS ATTACK AHEAD.",
        "EMPTY LANE: HIT FOE.", "SHAPE SETS ATK / HP.",
        "CIR 2/2 SQR 1/4", "TRIANGLE 3/1",
    };
    static const char * const p2[8] = {
        "RUBY: FIGHT + TAKE", "JADE: GIVE + TAKE",
        "MATCH ADJACENT COLOR", "BOOSTS ENTRY VERBS.",
        "FIGHT: HIT A CARD.", "GIVE: HEAL AN ALLY.",
        "TAKE: DRAW CARDS.", "WIN: REPLACE A CARD.",
    };
    help_page(p1, 8, "THREE LANE DUELS");
    help_page(p2, 8, "COLOR CONNECTIONS");
}

/* ------------------------------------------------------------------ */
/*  Map screen                                                         */
/* ------------------------------------------------------------------ */

static const uint8_t flag_x[N_STATIONS + 1] = {
    4, 8, 14, 11
};
static const uint8_t flag_y[N_STATIONS + 1] = {
    13, 10, 7, 4
};

static void draw_hud(void)
{
    uint8_t i;
    frect(0, 0, 20, 1, T_BLANK, P_UI);
    for (i = 0; i < hearts_max; i++)
        put(i, 0, (i < hearts) ? T_HEART : T_HEART_E, P_RED);
    put(7, 0, T_ICO_DECK, P_UI);
    print_u16(8, 0, coll_n, P_UI);
    print(12, 0, "STN", P_UI);
    print_u16(16, 0, station + 1, P_UI);
    if (station + 1 < 10) { put(17, 0, glyph('/'), P_UI);
                            print(18, 0, "3", P_UI); }
    else print(18, 0, "  ", P_UI);
}

static void draw_map_scene(void)
{
    uint8_t i;
    load_scene(map_scene_tiles, MAP_SCENE_COUNT, map_scene_map, 1, 14);
    /* Transparent flags preserve the ice and trail beneath each landmark. */
    for (i = 0; i <= N_STATIONS; ++i) {
        set_sprite_tile(5+i, S_ROUTE_FLAG);
        set_sprite_prop(5+i, i < station ? 2 : 3);
        move_sprite(5+i, flag_x[i]*8+8, flag_y[i]*8+16);
    }
}

static void draw_map_footer(void)
{
    frect(0, 15, 20, 3, T_BLANK, P_UI);
    print(0, 15, "NEXT:", 6);
    print(6, 15, station_names[station], P_UI);
    print_center(16, "WIN A THREE LANE DUEL", P_UI);
    print_center(17, "12 HP / 2 PLAYS", P_UI);
}

static void draw_map_controls(void)
{
    frect(0, 15, 20, 1, T_BLANK, P_UI);
    print_center(15, "A:GO SEL:? ST:PACK", P_UI);
}

/* returns J_A (climb), J_SELECT (help) or J_START (pack view) */
static uint8_t map_screen(void)
{
    uint8_t t = 0, n;
    uint8_t px = flag_x[station] * 8 - 4;
    uint8_t py = flag_y[station] * 8 + 8;
    if (station == 0) { px = 24; py = 112; }

    DISPLAY_OFF;
    draw_hud();
    draw_map_scene();
    draw_map_footer();
    explorer(px, py, 0);
    SHOW_SPRITES;
    screen_open(pals_map);

    for (;;) {
        n = tick();
        t++;
        explorer(px, py, (t >> 5) & 1);
        if (t == 120) {
            draw_map_controls();
        } else if (t == 240) {
            t = 0;
            draw_map_footer();
        }
        if (n & (J_A | J_SELECT | J_START)) {
            sfx_pick();
            screen_close();
            return n;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Pack (collection) viewer                                           */
/* ------------------------------------------------------------------ */

static void pack_view(void)
{
    uint8_t cur = 0, n;
    for (;;) {
        uint8_t card = coll[cur];
        DISPLAY_OFF;
        felt();
        window(0, 0, 20, 17);
        print_center(1, "YOUR DECK", P_GOLD);
        print(2, 3, "CARD", P_UI);
        print_u16(7, 3, cur + 1, P_UI);
        put(9, 3, glyph('/'), P_UI);
        print_u16(10, 3, coll_n, P_UI);
        draw_card(8, 5, card);
        print_card_name(10, card, P_UI);
        print_center(12, verb_names[C_VERB(card)], P_GOLD);
        print(4, 14, "ATK", P_UI);
        print_u16(8, 14, card_attack(card), P_UI);
        print(10, 14, "HP", P_UI);
        print_u16(13, 14, card_health(card), P_UI);
        print_center(17, "L/R:CARD B:BACK", P_FELT);
        screen_open(pals_table);
        for (;;) {
            n = tick();
            if (n & (J_B | J_START)) { screen_close(); return; }
            if ((n & J_LEFT) && cur) { --cur; break; }
            if ((n & J_RIGHT) && cur + 1 < coll_n) { ++cur; break; }
        }
        sfx_cursor();
        screen_close();
    }
}

#include "duel_ui.h"

/* ------------------------------------------------------------------ */
/*  Reward: choose a card, then choose the card it replaces            */
/* ------------------------------------------------------------------ */

static const uint8_t reward_x[3] = { 2, 8, 14 };

static void reward_index(uint8_t index)
{
    uint8_t x;
    frect(0, 13, 20, 1, T_DITH12, P_FELT);
    print(6, 13, "CARD", P_UI);
    x = print_u16(11, 13, index + 1, P_GOLD);
    put(x, 13, glyph('/'), P_UI);
    print_u16(x + 1, 13, coll_n, P_UI);
}

static void reward_screen(void)
{
    uint8_t choices[3], chosen, cur = 0, i, n;

    choices[0] = card_reward((strongest_color() == COLOR_JADE ? 6 : 0) + rnd(6));
    choices[1] = card_reward((strongest_color() == COLOR_JADE ? 0 : 6) + rnd(6));
    do { choices[2] = random_card(); }
        while (choices[2] == choices[0] || choices[2] == choices[1]);

    DISPLAY_OFF;
    felt();
    frect(0, 0, 20, 3, T_BLANK, P_UI);
    print_center(0, "STATION CLEAR!", P_GOLD);
    print_center(2, "CHOOSE A REPLACEMENT", P_UI);
    for (i = 0; i < 3; i++) draw_card(reward_x[i] + 1, 6, choices[i]);
    print_card_name(12, choices[0], P_FELT);
    print_center(14, verb_names[C_VERB(choices[0])], P_FELT);
    print_center(16, "A:CHOOSE  B:SKIP", P_UI);
    set_sprite_tile(0, S_ARROW_UP);
    set_sprite_prop(0, 0);
    move_sprite(0, (reward_x[0] + 2) * 8 + 12, 10 * 8 + 16);
    duel_frame(reward_x[0] + 1, 6);
    SHOW_SPRITES;
    screen_open(pals_table);

    for (;;) {
        n = tick();
        if ((n & J_LEFT) && cur) cur--;
        else if ((n & J_RIGHT) && cur < 2) cur++;
        else if (n & J_A) break;
        else if (n & J_B) {
            sfx_cursor();
            screen_close();
            return;
        } else continue;
        sfx_cursor();
        move_sprite(0, (reward_x[cur] + 2) * 8 + 12, 10 * 8 + 16);
        duel_frame(reward_x[cur] + 1, 6);
        print_card_name(12, choices[cur], P_FELT);
        frect(0, 14, 20, 1, T_DITH12, P_FELT);
        print_center(14, verb_names[C_VERB(choices[cur])], P_FELT);
    }
    chosen = choices[cur];
    sfx_pick();
    screen_close();

    cur = 0;
    DISPLAY_OFF;
    felt();
    frect(0, 0, 20, 3, T_BLANK, P_UI);
    print_center(0, "REPLACE ONE CARD", P_GOLD);
    print(2, 4, "NEW", P_UI);
    print(13, 4, "OUT", P_UI);
    draw_card(2, 6, chosen);
    draw_card(14, 6, coll[0]);
    print_card_name(12, coll[0], P_FELT);
    reward_index(0);
    print_center(16, "L/R:PICK A:REPLACE", P_UI);
    print_center(17, "B:KEEP OLD DECK", P_UI);
    set_sprite_tile(0, S_ARROW_UP);
    move_sprite(0, (uint8_t)(15 * 8 + 12), (uint8_t)(10 * 8 + 16));
    duel_frame(14, 6);
    SHOW_SPRITES;
    screen_open(pals_table);

    for (;;) {
        n = tick();
        if ((n & J_LEFT) && cur) cur--;
        else if ((n & J_RIGHT) && cur < coll_n - 1) cur++;
        else if (n & J_A) {
            coll[cur] = chosen;
            sfx_win();
            break;
        } else if (n & J_B) {
            sfx_cursor();
            break;
        } else continue;
        sfx_cursor();
        frect(14, 6, 3, 4, T_DITH12, P_FELT);
        draw_card(14, 6, coll[cur]);
        print_card_name(12, coll[cur], P_FELT);
        reward_index(cur);
    }
    screen_close();
}
/* ------------------------------------------------------------------ */
/*  Endings                                                            */
/* ------------------------------------------------------------------ */

/* Wipe the title lettering while preserving the Antarctic panorama. */
static void clear_sky(void)
{
    uint8_t x, y;
    for (y = 0; y < 7; y++)
        for (x = 0; x < 20; x++)
            put(x, y, T_BLANK, TP_SKY0);
}

static void gameover_screen(void)
{
    DISPLAY_OFF;
    draw_night_scene();
    clear_sky();
    big_text("YOU FALL", 2, 2, TP_SKY0, DYN_BASE);
    print_center(5, "THE MOUNTAIN WINS", TP_GOLD);
    print(2, 16, "YOU REACHED STN", TP_GND);
    print_u16(18, 16, station + 1, TP_GND);
    print_center(17, "PRESS START", TP_GND);
    screen_open(pals_title);
    sfx_lose();
    wait_press(J_START);
    sfx_pick();
    screen_close();
}

static void summit_screen(void)
{
    uint8_t t = 0;
    DISPLAY_OFF;
    draw_night_scene();
    clear_sky();
    big_text("THE SUMMIT", 0, 2, TP_SKY0, DYN_BASE);
    print_center(5, "A TRUE CARD SAGE!", TP_GOLD);
    print_center(16, "THREE DUELS CLEARED", TP_GND);
    print_center(17, "PRESS START", TP_GND);
    screen_open(pals_title);
    sfx_win();
    for (;;) {
        uint8_t n = tick();
        t++;
        if (n & J_START) break;
        if ((t & 63) == 0)  { put(2, 1, T_STAR2, TP_SKY0);
                              put(18, 4, T_STAR2, TP_SKY0); }
        if ((t & 63) == 32) { put(2, 1, T_STAR1, TP_SKY0);
                              put(18, 4, T_STAR1, TP_SKY0); }
    }
    sfx_pick();
    screen_close();
}

/* ------------------------------------------------------------------ */
/*  A whole run of the mountain                                        */
/* ------------------------------------------------------------------ */

static void new_run(void)
{
    uint8_t i;
    for (i = 0; i < STARTER_SIZE; i++) coll[i] = starter_deck[i];
    coll_n = STARTER_SIZE;
    station = 0;
    hearts = hearts_max = 3;
}

static void play_run(void)
{
    new_run();
    for (;;) {
        uint8_t n = map_screen();
        if (n & J_SELECT) { help_screens(); continue; }
        if (n & J_START)  { pack_view(); continue; }

        if (battle_duel()) {
            station++;
            if (station >= N_STATIONS) { summit_screen(); return; }
            reward_screen();
        } else {
            hearts--;
            if (!hearts) { gameover_screen(); return; }
        }
    }
}

/* ------------------------------------------------------------------ */

void main(void)
{
    uint8_t i;
    DISPLAY_OFF;
    snd_init();
    set_bkg_data(0, BG_TILE_COUNT, bg_tiles);
    set_sprite_data(0, SPR_TILE_COUNT, spr_tiles);
    set_sprite_palette(0, 4, pals_spr);
    SPRITES_8x8;
    for (i = 0; i < 40; i++) move_sprite(i, 0, 0);
    /* wipe the map + attributes so no boot-logo tiles linger */
    VBK_REG = 0;
    fill_bkg_rect(0, 0, 32, 32, T_BLANK);
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, 0);
    VBK_REG = 0;
    /* start faded to white so the first fade_in works */
    pal_cur = pals_title;
    pal_step(0);
    SHOW_BKG;

    for (;;) {
        title_screen();
        play_run();
    }
}
