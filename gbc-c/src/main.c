/*
 * CARD JOURNEY - a Game Boy Color card-collecting mountain climb.
 * Built with GBDK-2020.
 *
 * THE GAME
 *   Climb a mountain of ten stations. Every card in your pack has a
 *   COLOR (ruby/amber/jade/azure), a SHAPE with a chip value
 *   (circle 2, square 4, triangle 6, diamond 8) and an ACTION verb
 *   (move/fight/sneak/speak/take/give).
 *
 *   Cards are DUAL-USE, and that is the whole game:
 *     - On the TRAIL, obstacles demand verbs. Spending a card gets you
 *       past, but the card is gone from your pack forever.
 *     - At each STATION you duel with that same pack: pick cards from a
 *       hand of five and play shape-sets (pair, trio, full house...)
 *       scoring chips x mult, Balatro-style, until you beat the goal.
 *   Every card burned on the trail weakens the deck you must win with.
 *
 *   Win a duel, draft one of three new cards. Sacrifice cards at
 *   shrines for permanent talismans. Run out of hearts and you fall.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/hardware.h>
#include <rand.h>
#include <stdint.h>
#include "assets.h"

/* ------------------------------------------------------------------ */
/*  Cards                                                              */
/* ------------------------------------------------------------------ */

/* A card packs into one byte: ccss0vvv */
#define CARD(c, s, v)   (uint8_t)(((c) << 5) | ((s) << 3) | (v))
#define C_COLOR(k)      ((uint8_t)((k) >> 5) & 3)
#define C_SHAPE(k)      ((uint8_t)((k) >> 3) & 3)
#define C_VERB(k)       ((uint8_t)(k) & 7)
#define CARD_NONE       0xFF
#define CHIPVAL(k)      ((uint8_t)((C_SHAPE(k) + 1) << 1))   /* 2/4/6/8 */

enum { V_MOVE, V_FIGHT, V_SNEAK, V_SPEAK, V_TAKE, V_GIVE };

#define MAXC        40      /* pack size limit */
#define HANDN       5
#define N_STATIONS  10

/* Talisman bit flags */
#define TAL_EMBER   0x01    /* FIGHT adds +9 chips instead of +4  */
#define TAL_ECHO    0x02    /* SPEAK adds +2 mult instead of +1   */
#define TAL_PRISM   0x04    /* flush adds +4 mult instead of +2   */
#define TAL_WIND    0x08    /* +1 play each duel                  */
#define TAL_WOOL    0x10    /* +1 max heart                       */
#define TAL_LODE    0x20    /* +6 chips on every play             */
#define N_TALS      6

/* ------------------------------------------------------------------ */
/*  Strings                                                            */
/* ------------------------------------------------------------------ */

static const char * const station_names[N_STATIONS] = {
    "TRAILHEAD", "MOSS HOLLOW", "OLD BRIDGE", "PINE GATE", "HALF CAMP",
    "CRAG POINT", "ICE WALL", "WIND SHELF", "STAR LEDGE", "LAST GATE",
};
static const uint16_t station_goal[N_STATIONS] = {
    35, 60, 90, 130, 180, 240, 310, 390, 480, 600,
};
enum {
    MOD_PAIR, MOD_RUBY, MOD_FIGHT, MOD_FLUSH, MOD_SQUARE,
    MOD_SPEAK, MOD_TRIO, MOD_JADE, MOD_DIAMOND, MOD_FULL,
};
static const uint8_t station_mod[N_STATIONS] = {
    MOD_PAIR, MOD_RUBY, MOD_FIGHT, MOD_FLUSH, MOD_SQUARE,
    MOD_SPEAK, MOD_TRIO, MOD_JADE, MOD_DIAMOND, MOD_FULL,
};
static const char * const mod_descs[10] = {
    "PAIR +10 CHIPS", "RUBY +2 EACH", "FIGHT +3 EACH",
    "FLUSH +1 MULT", "SQR +3 EACH", "SPEAK +1 EACH",
    "TRIO +20 CHIPS", "JADE +2 EACH", "DIA +4 EACH",
    "FULL +25 CHIPS",
};
static const char * const color_names[4] = {
    "RUBY", "AMBER", "JADE", "AZURE",
};
static const char * const shape_names[4] = { "CIR", "SQR", "TRI", "DIA" };
static const char * const verb_names[6] = {
    "MOVE", "FIGHT", "SNEAK", "SPEAK", "TAKE", "GIVE",
};
static const char * const tal_names[N_TALS] = {
    "EMBER FANG", "ECHO BELL", "PRISM EYE",
    "FOURTH WIND", "WOOL CHARM", "LODESTONE",
};
static const char * const tal_descs[N_TALS] = {
    "FIGHT ADDS +9 CHIPS", "SPEAK ADDS +2 MULT", "FLUSH ADDS +4 MULT",
    "+1 PLAY EACH DUEL", "+1 MAX HEART", "+6 CHIPS EVERY PLAY",
};
static const char * const hand_names[7] = {
    "HIGH CARD", "PAIR", "TWO PAIR", "TRIO", "FULL HOUSE", "QUAD", "FIVE!",
};

/* Trail encounters */
enum { E_CHASM, E_WOLF, E_GUARD, E_ICEFALL, E_CACHE, E_SHRINE, E_TRAVELER };
static const char * const enc_names[7] = {
    "A WIDE CHASM!", "A HUNGRY WOLF!", "A SURLY GUARD!", "FALLING ICE!",
    "A LOST CACHE!", "A QUIET SHRINE.", "A KIND TRAVELER.",
};
/* verb bitmask a card must match to answer the encounter */
static const uint8_t enc_verbs[7] = {
    (1 << V_MOVE),
    (1 << V_FIGHT) | (1 << V_SNEAK),
    (1 << V_SPEAK) | (1 << V_FIGHT),
    (1 << V_MOVE) | (1 << V_SNEAK),
    (1 << V_TAKE),
    (1 << V_GIVE),      /* the shrine wants an offering */
    (1 << V_SPEAK),
};
static const uint8_t enc_hostile[7] = { 1, 1, 1, 1, 0, 0, 0 };

/* ------------------------------------------------------------------ */
/*  Game state                                                         */
/* ------------------------------------------------------------------ */

static uint8_t coll[MAXC];      /* your pack (whole collection)   */
static uint8_t coll_n;
static uint8_t station;         /* next station to challenge 0..9 */
static uint8_t hearts, hearts_max;
static uint8_t tals;            /* talisman bit flags             */
static uint8_t joy_prev;

/* the opening pack: 20 cards, verb- and shape-balanced */
static const uint8_t start_pack[20] = {
    CARD(0,0,V_MOVE),  CARD(1,0,V_MOVE),  CARD(2,1,V_MOVE),  CARD(3,0,V_MOVE),
    CARD(0,1,V_FIGHT), CARD(1,0,V_FIGHT), CARD(2,0,V_FIGHT), CARD(3,2,V_FIGHT),
    CARD(1,1,V_SNEAK), CARD(2,0,V_SNEAK), CARD(3,1,V_SNEAK),
    CARD(0,0,V_SPEAK), CARD(2,2,V_SPEAK), CARD(3,0,V_SPEAK),
    CARD(0,2,V_TAKE),  CARD(1,1,V_TAKE),  CARD(2,3,V_TAKE),
    CARD(0,1,V_GIVE),  CARD(1,2,V_GIVE),  CARD(3,3,V_GIVE),
};

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
    RGB_WHITE, RGB(21,21,23), RGB(12,12,14), RGB_BLACK,
    RGB_WHITE, RGB(29, 5, 7), RGB(16, 2, 4), RGB_BLACK,
    RGB_WHITE, RGB(30,20, 2), RGB(21,12, 0), RGB_BLACK,
    RGB_WHITE, RGB( 4,22, 9), RGB( 2,12, 5), RGB_BLACK,
    RGB_WHITE, RGB( 6,11,29), RGB( 3, 6,17), RGB_BLACK,
    RGB_WHITE, RGB(19,17,24), RGB(11,10,15), RGB_BLACK,
    RGB( 3,11, 7), RGB( 4,14, 9), RGB( 2, 8, 5), RGB_WHITE,
    RGB_WHITE, RGB(31,24, 6), RGB(24,15, 0), RGB(20,12, 0),
};

/* the map set: day-lit mountain */
#define P_SCENE 1
#define P_FLAGR 2               /* flag on rock  */
#define P_TREE  3
#define P_RED   4
#define P_FLAGS 7               /* flag on snow  */
static const palette_color_t pals_map[32] = {
    RGB_WHITE, RGB(21,21,23), RGB(12,12,14), RGB_BLACK,
    RGB(17,24,31), RGB(30,31,31), RGB(14,11, 9), RGB( 4, 3, 3),
    RGB(14,11, 9), RGB(30, 4, 5), RGB(22,22,22), RGB( 4, 3, 3),
    RGB(14,11, 9), RGB( 6,18, 8), RGB( 3,11, 5), RGB( 7, 5, 3),
    RGB_WHITE, RGB(30, 4, 5), RGB(18, 2, 3), RGB_BLACK,
    RGB_WHITE, RGB(31,24, 6), RGB(22,15, 0), RGB_BLACK,
    RGB_WHITE, RGB(19,17,24), RGB(11,10,15), RGB_BLACK,
    RGB(30,31,31), RGB(30, 4, 5), RGB(22,22,22), RGB( 4, 3, 3),
};

/* the title set: night sky bands, moonlit silhouette */
#define TP_SKY0 0
#define TP_SKY1 1
#define TP_SKY2 2
#define TP_SKY3 3
#define TP_MTN  4
#define TP_MOON 5
#define TP_GND  6
#define TP_GOLD 7
static const palette_color_t pals_title[32] = {
    RGB( 1, 1, 7), RGB( 4, 4,12), RGB( 2, 2, 9), RGB_WHITE,
    RGB( 2, 2,11), RGB( 5, 5,16), RGB( 3, 3,13), RGB_WHITE,
    RGB( 4, 4,16), RGB( 8, 7,21), RGB( 6, 5,18), RGB_WHITE,
    RGB( 7, 6,21), RGB(11,10,26), RGB( 9, 8,23), RGB_WHITE,
    RGB( 7, 6,21), RGB(26,27,31), RGB( 3, 2, 6), RGB_WHITE,
    RGB( 1, 1, 7), RGB(30,29,18), RGB( 2, 2, 9), RGB_WHITE,
    RGB( 2, 2, 4), RGB( 4, 4, 8), RGB( 1, 1, 2), RGB_WHITE,
    RGB( 4, 4,16), RGB( 8, 7,21), RGB( 6, 5,18), RGB(31,26,10),
};

static const palette_color_t pals_spr[8] = {
    0, RGB_WHITE, RGB(31,10,10), RGB_BLACK,          /* cursor arrow */
    0, RGB(30,22,16), RGB(28, 4, 6), RGB_BLACK,      /* climber      */
};

/* live palette buffer so screens can fade in and out */
static palette_color_t pal_buf[32];
static const palette_color_t *pal_cur;

/* ------------------------------------------------------------------ */
/*  Low-level video helpers                                            */
/* ------------------------------------------------------------------ */

static void put(uint8_t x, uint8_t y, uint8_t t, uint8_t pal)
{
    set_bkg_tile_xy(x, y, t);
    VBK_REG = 1;
    set_bkg_tile_xy(x, y, pal);
    VBK_REG = 0;
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

/* rounded white dialog box */
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
    static const uint8_t shape_w[10] = { 0,0,0,0, 1,1,1, 2,2, 3 };
    return CARD(rnd(4), shape_w[rnd(10)], rnd(6));
}

static uint8_t weighted_shape(void)
{
    static const uint8_t shape_w[10] = { 0,0,0,0, 1,1,1, 2,2, 3 };
    return shape_w[rnd(10)];
}

/* Reward options should answer three different player needs. */
static uint8_t least_common_verb(void)
{
    uint8_t counts[6] = { 0, 0, 0, 0, 0, 0 }, i, best = 0;
    for (i = 0; i < coll_n; i++) counts[C_VERB(coll[i])]++;
    for (i = 1; i < 6; i++)
        if (counts[i] < counts[best]) best = i;
    return best;
}

static uint8_t strongest_color(void)
{
    uint8_t counts[4] = { 0, 0, 0, 0 }, i, best = 0;
    for (i = 0; i < coll_n; i++) counts[C_COLOR(coll[i])]++;
    for (i = 1; i < 4; i++)
        if (counts[i] > counts[best]) best = i;
    return best;
}

static void shuffle(uint8_t *a, uint8_t n)
{
    uint8_t i, j, t;
    for (i = n - 1; i > 0; i--) {
        j = rnd(i + 1);
        t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void coll_remove(uint8_t idx)
{
    coll[idx] = coll[--coll_n];
}

static uint8_t coll_add(uint8_t card)   /* 0 if the pack is full */
{
    if (coll_n >= MAXC) return 0;
    coll[coll_n++] = card;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Card drawing                                                       */
/* ------------------------------------------------------------------ */

static void draw_card(uint8_t x, uint8_t y, uint8_t card)
{
    uint8_t pal = P_RUBY + C_COLOR(card);
    put(x, y, T_CARD_TL, pal);  put(x+1, y, T_CARD_T, pal);
    put(x+2, y, T_CARD_TR, pal);
    put(x, y+1, T_CARD_L, pal);
    put(x+1, y+1, T_SHP_CIR + C_SHAPE(card), pal);
    put(x+2, y+1, T_CARD_R, pal);
    put(x, y+2, T_CARD_L, pal);
    put(x+1, y+2, T_ICO_MOVE + C_VERB(card), pal);
    put(x+2, y+2, T_CARD_R, pal);
    put(x, y+3, T_CARD_BL, pal); put(x+1, y+3, T_CARD_B, pal);
    put(x+2, y+3, T_CARD_BR, pal);
}

static void draw_cardback(uint8_t x, uint8_t y)
{
    put(x, y, T_CARD_TL, P_NEUT);  put(x+1, y, T_CARD_T, P_NEUT);
    put(x+2, y, T_CARD_TR, P_NEUT);
    put(x, y+1, T_CARD_L, P_NEUT); put(x+1, y+1, T_WEAVE_A, P_NEUT);
    put(x+2, y+1, T_CARD_R, P_NEUT);
    put(x, y+2, T_CARD_L, P_NEUT); put(x+1, y+2, T_SHP_DIA, P_NEUT);
    put(x+2, y+2, T_CARD_R, P_NEUT);
    put(x, y+3, T_CARD_BL, P_NEUT); put(x+1, y+3, T_CARD_B, P_NEUT);
    put(x+2, y+3, T_CARD_BR, P_NEUT);
}

/* "RUBY TRI-6 FIGHT" style caption, centered on row y */
static void print_card_name(uint8_t y, uint8_t card, uint8_t pal)
{
    uint8_t x = (uint8_t)(20 - (str_len(color_names[C_COLOR(card)]) + 7
                 + str_len(verb_names[C_VERB(card)]))) >> 1;
    frect(1, y, 18, 1, T_BLANK, pal);
    print(x, y, color_names[C_COLOR(card)], pal);
    x += str_len(color_names[C_COLOR(card)]) + 1;
    print(x, y, shape_names[C_SHAPE(card)], pal);
    x += 3;
    put(x++, y, glyph('-'), pal);
    x = print_u16(x, y, CHIPVAL(card), pal);
    x++;
    print(x, y, verb_names[C_VERB(card)], pal);
}

/* felt table backdrop used by all card screens */
static void felt(void)
{
    frect(0, 0, 20, 18, T_DITH12, P_FELT);
}

static void hide_sprites(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++) move_sprite(i, 0, 0);
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

#define DYN_BASE 112
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

static uint8_t sky_pal(uint8_t y)
{
    if (y < 4) return TP_SKY0;
    if (y < 7) return TP_SKY1;
    if (y < 9) return TP_SKY2;
    return TP_SKY3;
}

static void draw_night_scene(void)
{
    uint8_t x, y, dyn;

    /* banded night sky */
    for (y = 0; y < 16; y++)
        for (x = 0; x < 20; x++)
            put(x, y, T_BLANK, sky_pal(y));
    /* ground band */
    frect(0, 16, 20, 2, T_DITH12, TP_GND);

    /* stars and moon */
    put(2, 1, T_STAR1, TP_SKY0);  put(16, 0, T_STAR2, TP_SKY0);
    put(6, 2, T_STAR2, TP_SKY0);  put(12, 1, T_STAR2, TP_SKY0);
    put(1, 5, T_STAR2, TP_SKY1);  put(18, 4, T_STAR1, TP_SKY1);
    put(4, 8, T_STAR1, TP_SKY2);  put(15, 7, T_STAR2, TP_SKY2);
    put(17, 2, T_MOON, TP_MOON);

    /* moonlit mountain silhouette, apex at (10,10) */
    for (y = 10; y < 16; y++) {
        uint8_t hw = (uint8_t)(y - 10) * 2;
        uint8_t on_l = (10 >= hw + 1);
        uint8_t on_r = (10 + hw + 1 <= 19);
        uint8_t lx = on_l ? 10 - hw - 1 : 0;
        uint8_t rx = on_r ? 10 + hw + 1 : 19;
        uint8_t fill_t = (y < 12) ? T_SNOW : T_ROCK;
        uint8_t f;
        if (on_l) put(lx, y, (y < 12) ? T_SNOW_L : T_ROCK_L, TP_MTN);
        if (on_r) put(rx, y, (y < 12) ? T_SNOW_R : T_ROCK_R, TP_MTN);
        for (f = lx + on_l; f <= rx - on_r; f++)
            put(f, y, (y == 12) ? T_SNOWCAP : fill_t, TP_MTN);
    }

    /* the big logo (band palettes keep the sky gradient behind it) */
    dyn = big_text("CARD", 6, 2, sky_pal(2), DYN_BASE);
    big_text("JOURNEY", 3, 5, sky_pal(5), dyn);
    print_center(8, "THE TEN STATIONS", TP_GOLD);
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
        /* twinkle: blink PRESS START, swap a star */
        if ((t & 31) == 0)
            print_center(16, "PRESS START", TP_GND);
        else if ((t & 31) == 16)
            frect(4, 16, 12, 1, T_DITH12, TP_GND);
        if ((t & 63) == 0)  put(2, 1, T_STAR2, TP_SKY0);
        if ((t & 63) == 32) put(2, 1, T_STAR1, TP_SKY0);
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
        "PLAY SHAPE SETS:", "PAIR TRIO QUAD...",
        "PTS = CHIPS x MULT", "CIR2 SQR4",
        "TRI6 DIA8", "ONE COLOR = FLUSH!",
        "FIGHT ADDS CHIPS,", "SPEAK ADDS MULT.",
    };
    static const char * const p2[8] = {
        "OBSTACLES NEED", "ACTION CARDS. SPENT",
        "CARDS ARE GONE", "FOR GOOD!",
        "CACHE: TAKE=CARDS", "SHRINE: GIVE=CHARM",
        "TALK: SPEAK=HEART", "REACH STATION TEN!",
    };
    help_page(p1, 8, "THE DUEL");
    help_page(p2, 8, "THE TRAIL");
}

/* ------------------------------------------------------------------ */
/*  Map screen                                                         */
/* ------------------------------------------------------------------ */

static const uint8_t flag_x[N_STATIONS + 1] = {
    4, 15, 7, 13, 6, 12, 8, 12, 9, 11, 10
};
static const uint8_t flag_y[N_STATIONS + 1] = {
    13, 13, 12, 11, 10, 9, 8, 8, 5, 4, 3
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
                            print(18, 0, "10", P_UI); }
    else print(18, 0, "  ", P_UI);
}

static void draw_map_scene(void)
{
    uint8_t x, y, i;

    /* sky, then the day-lit mountain: apex (10,3), snow to row 6 */
    frect(0, 1, 20, 14, T_BLANK, P_SCENE);
    for (y = 3; y < 15; y++) {
        uint8_t hw = y - 3;
        uint8_t on_l = (10 >= hw + 1);
        uint8_t on_r = (10 + hw + 1 <= 19);
        uint8_t lx = on_l ? 10 - hw - 1 : 0;
        uint8_t rx = on_r ? 10 + hw + 1 : 19;
        uint8_t snow = (y <= 6);
        if (on_l) put(lx, y, snow ? T_SNOW_L : T_ROCK_L, P_SCENE);
        if (on_r) put(rx, y, snow ? T_SNOW_R : T_ROCK_R, P_SCENE);
        for (x = lx + on_l; x <= rx - on_r; x++)
            put(x, y, (y == 7) ? T_SNOWCAP : (snow ? T_SNOW : T_ROCK),
                P_SCENE);
    }
    /* foothill pines */
    put(1, 13, T_TREE_TOP, P_TREE); put(1, 14, T_TREE_BOT, P_TREE);
    put(18, 12, T_TREE_TOP, P_TREE); put(18, 13, T_TREE_BOT, P_TREE);

    /* path dots + station flags */
    for (i = 0; i < N_STATIONS; i++) {
        uint8_t mx = (uint8_t)(flag_x[i] + flag_x[i + 1]) >> 1;
        uint8_t my = (uint8_t)(flag_y[i] + flag_y[i + 1]) >> 1;
        uint8_t p = (my <= 6) ? P_FLAGS : P_FLAGR;
        put(mx, my, T_PATHDOT, p);
    }
    for (i = 0; i <= N_STATIONS; i++) {
        uint8_t p = (flag_y[i] <= 6) ? P_FLAGS : P_FLAGR;
        uint8_t done = (i < station);
        put(flag_x[i], flag_y[i], done ? T_FLAG_DONE : T_FLAG, p);
    }
}

static void draw_map_footer(void)
{
    const char *trail;
    frect(0, 15, 20, 3, T_BLANK, P_UI);
    print(0, 15, "MOD:", P_GOLD);
    print(4, 15, mod_descs[station_mod[station]], P_UI);
    print(0, 16, "NEXT:", P_UI);
    print(6, 16, station_names[station], P_UI);
    print(0, 17, "GOAL", P_GOLD);
    print_u16(5, 17, station_goal[station], P_GOLD);
    if (station == 0) trail = "CALM START";
    else if (station == 1 || station == 4 || station == 7)
        trail = "CACHE: TAKE";
    else if (station == 2 || station == 5 || station == 8)
        trail = "SHRINE:GIVE";
    else trail = "DANGER";
    print(9, 17, trail, P_UI);
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
    uint8_t px = flag_x[station] * 8 + 8;
    uint8_t py = flag_y[station] * 8 + 14;
    if (station == 0) { px = 2 * 8 + 8; py = 14 * 8 + 14; }

    DISPLAY_OFF;
    draw_hud();
    draw_map_scene();
    draw_map_footer();
    set_sprite_tile(1, S_CLIMBER_A);
    set_sprite_prop(1, 1);
    move_sprite(1, px, py);
    SHOW_SPRITES;
    screen_open(pals_map);

    for (;;) {
        n = tick();
        t++;
        set_sprite_tile(1, (t & 16) ? S_CLIMBER_B : S_CLIMBER_A);
        if (t == 120) {
            draw_map_controls();
        } else if (t == 240) {
            t = 0;
            frect(0, 15, 20, 1, T_BLANK, P_UI);
            print(0, 15, "MOD:", P_GOLD);
            print(4, 15, mod_descs[station_mod[station]], P_UI);
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
    uint8_t i, v, counts[6] = { 0, 0, 0, 0, 0, 0 };
    DISPLAY_OFF;
    felt();
    window(0, 0, 20, 18);
    print(1, 1, "YOUR PACK:", P_UI);
    print_u16(11, 1, coll_n, P_UI);
    for (i = 0; i < coll_n; i++) {
        uint8_t x = 1 + (i % 6) * 3;
        uint8_t y = 3 + (i / 6) * 2;
        uint8_t p = P_RUBY + C_COLOR(coll[i]);
        put(x, y, T_SHP_CIR + C_SHAPE(coll[i]), p);
        put(x + 1, y, T_ICO_MOVE + C_VERB(coll[i]), p);
        counts[C_VERB(coll[i])]++;
    }
    /* A quick answer to the important trail question: what can I spend? */
    for (v = 0; v < 6; v++) {
        put(v * 3, 17, T_ICO_MOVE + v, P_UI);
        print_u16(v * 3 + 1, 17, counts[v], P_UI);
    }
    print_center(16, "B:BACK", P_UI);
    screen_open(pals_table);
    wait_press(J_B | J_A | J_START);
    sfx_pick();
    screen_close();
}

/* ------------------------------------------------------------------ */
/*  The duel                                                           */
/* ------------------------------------------------------------------ */

static uint8_t duel_deck[MAXC], dd_n;
static uint8_t discards[MAXC], dis_n;
static uint8_t hand[HANDN];
static uint8_t sel;                     /* bitmask of raised cards */
static uint16_t score, goal;
static uint8_t plays, swaps;

#define SLOT_X(i)  ((i) * 4)
#define HAND_Y     10
#define PLAY_Y     3

static void duel_refill_deck(void)
{
    uint8_t i;
    for (i = 0; i < dis_n; i++) duel_deck[i] = discards[i];
    dd_n = dis_n;
    dis_n = 0;
    if (dd_n > 1) shuffle(duel_deck, dd_n);
}

static uint8_t duel_draw(void)
{
    if (!dd_n) duel_refill_deck();
    if (!dd_n) return CARD_NONE;
    return duel_deck[--dd_n];
}

static void draw_slot(uint8_t i)
{
    frect(SLOT_X(i), HAND_Y - 1, 3, 5, T_DITH12, P_FELT);
    if (hand[i] == CARD_NONE) return;
    draw_card(SLOT_X(i), (sel & (1 << i)) ? HAND_Y - 1 : HAND_Y, hand[i]);
}

static void duel_hud(void)
{
    frect(0, 0, 20, 3, T_BLANK, P_UI);
    print(0, 0, "STN", P_UI);
    print_u16(4, 0, station + 1, P_UI);
    print(6, 0, station_names[station], P_UI);
    print(0, 1, "GOAL", P_UI);
    print_u16(5, 1, goal, P_UI);
    print(11, 1, "PTS", P_GOLD);
    print_u16(15, 1, score, P_GOLD);
    print(0, 2, "PLAY", P_UI);
    print_u16(5, 2, plays, P_UI);
    print(7, 2, "SWAP", P_UI);
    print_u16(12, 2, swaps, P_UI);
    put(15, 2, T_ICO_DECK, P_UI);
    print_u16(16, 2, dd_n, P_UI);
}

static void duel_msg(const char *s)
{
    frect(0, 7, 20, 2, T_BLANK, P_UI);
    print_center(7, s, P_UI);
}

static void move_cursor_to(uint8_t i)
{
    move_sprite(0, (uint8_t)(SLOT_X(i) * 8 + 20), (HAND_Y + 4) * 8 + 16);
}

/* Score the raised cards. With show set, also explain the result. */
static uint16_t score_selection(uint8_t show)
{
    uint8_t cnt[4] = { 0, 0, 0, 0 };
    uint8_t i, n = 0, c0 = 0, c1 = 0, htype;
    uint8_t first_col = 0xFF, flush = 1;
    uint16_t chips;
    uint8_t mult;

    chips = (tals & TAL_LODE) ? 6 : 0;
    mult = 0;
    for (i = 0; i < HANDN; i++) {
        uint8_t k;
        if (!(sel & (1 << i))) continue;
        k = hand[i];
        n++;
        cnt[C_SHAPE(k)]++;
        chips += CHIPVAL(k);
        if (C_VERB(k) == V_FIGHT) chips += (tals & TAL_EMBER) ? 9 : 4;
        if (C_VERB(k) == V_SPEAK) mult += (tals & TAL_ECHO) ? 2 : 1;
        if (station_mod[station] == MOD_RUBY && C_COLOR(k) == 0) chips += 2;
        if (station_mod[station] == MOD_FIGHT && C_VERB(k) == V_FIGHT)
            chips += 3;
        if (station_mod[station] == MOD_SQUARE && C_SHAPE(k) == 1) chips += 3;
        if (station_mod[station] == MOD_SPEAK && C_VERB(k) == V_SPEAK)
            mult++;
        if (station_mod[station] == MOD_JADE && C_COLOR(k) == 2) chips += 2;
        if (station_mod[station] == MOD_DIAMOND && C_SHAPE(k) == 3) chips += 4;
        if (first_col == 0xFF) first_col = C_COLOR(k);
        else if (C_COLOR(k) != first_col) flush = 0;
    }
    for (i = 0; i < 4; i++) {
        if (cnt[i] >= c0) { c1 = c0; c0 = cnt[i]; }
        else if (cnt[i] > c1) c1 = cnt[i];
    }
    if      (c0 >= 5)           { htype = 6; chips += 70; mult += 7; }
    else if (c0 == 4)           { htype = 5; chips += 50; mult += 5; }
    else if (c0 == 3 && c1 >= 2){ htype = 4; chips += 40; mult += 4; }
    else if (c0 == 3)           { htype = 3; chips += 30; mult += 3; }
    else if (c0 == 2 && c1 == 2){ htype = 2; chips += 20; mult += 2; }
    else if (c0 == 2)           { htype = 1; chips += 10; mult += 2; }
    else                        { htype = 0; chips += 5;  mult += 1; }
    if (flush && n >= 3) mult += (tals & TAL_PRISM) ? 4 : 2;
    if (station_mod[station] == MOD_PAIR && c0 >= 2) chips += 10;
    if (station_mod[station] == MOD_FLUSH && flush && n >= 3) mult++;
    if (station_mod[station] == MOD_TRIO && c0 >= 3) chips += 20;
    if (station_mod[station] == MOD_FULL && c0 >= 3 && c1 >= 2) chips += 25;

    if (show) {
        frect(0, 7, 20, 2, T_BLANK, P_UI);
        print(0, 7, hand_names[htype], P_UI);
        if (flush && n >= 3) print(str_len(hand_names[htype]) + 1, 7,
                                   "FLUSH!", P_GOLD);
        i = print_u16(0, 8, chips, P_UI);
        put(i, 8, glyph('x'), P_UI);
        i = print_u16(i + 1, 8, mult, P_UI);
        print(i + 1, 8, "= +", P_GOLD);
        print_u16(i + 4, 8, chips * mult, P_GOLD);
    }
    return chips * mult;
}

static void duel_preview(void)
{
    if (sel) score_selection(1);
    else duel_msg("RAISE TO PREVIEW");
}

/* returns 1 = station won, 0 = lost */
static uint8_t duel(void)
{
    uint8_t i, cur = 0, n;

    /* build the duel deck from the whole pack */
    for (i = 0; i < coll_n; i++) duel_deck[i] = coll[i];
    dd_n = coll_n;
    dis_n = 0;
    if (dd_n > 1) shuffle(duel_deck, dd_n);

    score = 0;
    goal = station_goal[station];
    plays = 3 + ((tals & TAL_WIND) ? 1 : 0);
    swaps = 3;
    sel = 0;

    DISPLAY_OFF;
    felt();
    frect(0, 0, 20, 3, T_BLANK, P_UI);
    frect(0, 7, 20, 2, T_BLANK, P_UI);
    frect(0, 16, 20, 2, T_BLANK, P_UI);
    print(0, 16, "A:RAISE B:LOWER", P_UI);
    print(0, 17, "START:PLAY SEL:SWAP", P_UI);
    for (i = 0; i < HANDN; i++) hand[i] = CARD_NONE;
    duel_hud();
    duel_msg("SHUFFLING...");
    set_sprite_tile(0, S_ARROW_UP);
    set_sprite_prop(0, 0);
    move_cursor_to(0);
    SHOW_SPRITES;
    screen_open(pals_table);

    /* deal */
    for (i = 0; i < HANDN; i++) {
        hand[i] = duel_draw();
        draw_slot(i);
        sfx_cursor();
        delay_frames(5);
    }
    if (station == 0) duel_msg("MATCH SHAPES: PAIR");
    else duel_msg(mod_descs[station_mod[station]]);
    duel_hud();

    for (;;) {
        n = tick();

        if ((n & J_LEFT) && cur) {
            cur--; move_cursor_to(cur); sfx_cursor();
        }
        if ((n & J_RIGHT) && cur < HANDN - 1) {
            cur++; move_cursor_to(cur); sfx_cursor();
        }
        if ((n & J_A) && hand[cur] != CARD_NONE && !(sel & (1 << cur))) {
            sel |= 1 << cur;
            draw_slot(cur);
            print_card_name(15, hand[cur], P_FELT);
            duel_preview();
            sfx_pick();
        }
        if ((n & J_B) && (sel & (1 << cur))) {
            sel &= ~(1 << cur);
            draw_slot(cur);
            frect(0, 15, 20, 1, T_DITH12, P_FELT);
            duel_preview();
            sfx_cursor();
        }

        if ((n & J_SELECT) && sel && swaps) {          /* swap cards */
            swaps--;
            for (i = 0; i < HANDN; i++) {
                if (!(sel & (1 << i))) continue;
                if (hand[i] != CARD_NONE) discards[dis_n++] = hand[i];
                hand[i] = duel_draw();
            }
            sel = 0;
            for (i = 0; i < HANDN; i++) draw_slot(i);
            frect(0, 15, 20, 1, T_DITH12, P_FELT);
            duel_preview();
            duel_hud();
            sfx_play();
        }

        if ((n & J_START) && sel && plays) {           /* play! */
            uint16_t pts;
            plays--;
            /* show the played set on the table row */
            frect(0, PLAY_Y, 20, 4, T_DITH12, P_FELT);
            {
                uint8_t px = 0;
                for (i = 0; i < HANDN; i++) {
                    if (!(sel & (1 << i))) continue;
                    draw_card(SLOT_X(px), PLAY_Y, hand[i]);
                    px++;
                }
            }
            sfx_play();
            pts = score_selection(1);
            score += pts;
            for (i = 0; i < HANDN; i++) {
                if (!(sel & (1 << i))) continue;
                if (hand[i] != CARD_NONE) discards[dis_n++] = hand[i];
                hand[i] = duel_draw();
            }
            sel = 0;
            for (i = 0; i < HANDN; i++) draw_slot(i);
            frect(0, 15, 20, 1, T_DITH12, P_FELT);
            duel_hud();
            delay_frames(30);

            if (score >= goal) {
                duel_msg("STAGE CLEAR!");
                sfx_win();
                delay_frames(40);
                screen_close();
                return 1;
            }
            if (!plays) {
                duel_msg("OUT OF PLAYS...");
                sfx_lose();
                delay_frames(50);
                screen_close();
                return 0;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Reward draft: pick one of three cards                              */
/* ------------------------------------------------------------------ */

static const uint8_t pick3_x[3] = { 2, 8, 14 };

static void reward_screen(void)
{
    uint8_t c[3], cur = 0, i, n;

    /* One answer for the trail, one for a flush build, one raw scorer. */
    c[0] = CARD(rnd(4), weighted_shape(), least_common_verb());
    c[1] = CARD(strongest_color(), weighted_shape(), rnd(6));
    c[2] = CARD(rnd(4), 2 + rnd(2), rnd(6));

    DISPLAY_OFF;
    felt();
    frect(0, 0, 20, 3, T_BLANK, P_UI);
    print_center(0, "STATION CLEAR!", P_GOLD);
    print_center(2, "TAKE ONE CARD:", P_UI);
    print(1, 4, "TRAIL", P_UI);
    print(8, 4, "FLUSH", P_UI);
    print(14, 4, "POWER", P_UI);
    for (i = 0; i < 3; i++) draw_card(pick3_x[i] + 1, 6, c[i]);
    frect(0, 12, 20, 1, T_BLANK, P_UI);
    print_card_name(12, c[0], P_UI);
    frect(0, 16, 20, 2, T_BLANK, P_UI);
    print_center(16, "A:TAKE B:SKIP", P_UI);
    set_sprite_tile(0, S_ARROW_UP);
    set_sprite_prop(0, 0);
    move_sprite(0, (pick3_x[0] + 2) * 8 + 12, 10 * 8 + 16);
    SHOW_SPRITES;
    screen_open(pals_table);

    for (;;) {
        n = tick();
        if ((n & J_LEFT) && cur) cur--;
        else if ((n & J_RIGHT) && cur < 2) cur++;
        else if (n & J_A) {
            if (!coll_add(c[cur])) {
                print_card_name(12, c[cur], P_UI);
                print_center(14, "PACK FULL!", P_UI);
                sfx_bad();
                delay_frames(40);
            } else sfx_win();
            break;
        }
        else if (n & J_B) { sfx_cursor(); break; }
        else continue;
        sfx_cursor();
        move_sprite(0, (pick3_x[cur] + 2) * 8 + 12, 10 * 8 + 16);
        frect(0, 12, 20, 1, T_BLANK, P_UI);
        print_card_name(12, c[cur], P_UI);
    }
    screen_close();
}

/* ------------------------------------------------------------------ */
/*  Trail encounters                                                   */
/* ------------------------------------------------------------------ */

static uint8_t match_list[MAXC], match_n;

static void trail_prompt_verbs(uint8_t mask)
{
    uint8_t v, x, w = 0, first = 1;
    for (v = 0; v < 6; v++)
        if (mask & (1 << v)) {
            if (!first) w += 4;             /* " OR " */
            w += 1 + str_len(verb_names[v]);
            first = 0;
        }
    x = (uint8_t)(20 - w) >> 1;
    first = 1;
    for (v = 0; v < 6; v++) {
        if (!(mask & (1 << v))) continue;
        if (!first) { print(x + 1, 3, "OR", P_UI); x += 4; }
        put(x, 3, T_ICO_MOVE + v, P_UI);
        print(x + 1, 3, verb_names[v], P_UI);
        x += 1 + str_len(verb_names[v]);
        first = 0;
    }
}

static void trail_show_card(uint8_t idx)
{
    frect(6, 7, 8, 4, T_DITH12, P_FELT);
    draw_card(8, 7, coll[match_list[idx]]);
    print_card_name(12, coll[match_list[idx]], P_FELT);
    frect(0, 13, 20, 1, T_DITH12, P_FELT);
    if (match_n > 1) {
        uint8_t x = print_u16(8, 13, idx + 1, P_FELT);
        put(x, 13, glyph('/'), P_FELT);
        print_u16(x + 1, 13, match_n, P_FELT);
    }
}

static void msg_wait(const char *s)
{
    frect(0, 15, 20, 1, T_BLANK, P_UI);
    print_center(15, s, P_UI);
    frect(0, 16, 20, 2, T_BLANK, P_UI);
    print_center(16, "A:OK", P_UI);
    delay_frames(8);
    wait_press(J_A | J_B | J_START);
}

/* one encounter; may change hearts/pack. */
static void encounter(uint8_t e)
{
    uint8_t i, cur = 0, n, spent;

    match_n = 0;
    for (i = 0; i < coll_n; i++)
        if (enc_verbs[e] & (1 << C_VERB(coll[i])))
            match_list[match_n++] = i;

    DISPLAY_OFF;
    hide_sprites();
    felt();
    window(0, 0, 20, 6);
    print_center(1, enc_names[e], P_GOLD);
    trail_prompt_verbs(enc_verbs[e]);
    print_center(4, enc_hostile[e] ? "OR LOSE A HEART!" : "OR WALK ON BY.",
                 P_UI);
    frect(0, 15, 20, 1, T_BLANK, P_UI);
    frect(0, 16, 20, 2, T_BLANK, P_UI);
    if (match_n) {
        print_center(16, "L/R:PICK A:USE", P_UI);
        print_center(17, "B:REFUSE", P_UI);
        trail_show_card(0);
    } else {
        print_center(9, "NO MATCHING CARD!", P_FELT);
        print_center(17, "B:GO ON", P_UI);
    }
    screen_open(pals_table);

    spent = 0;
    for (;;) {
        n = tick();
        if (match_n) {
            if ((n & J_LEFT) && cur) {
                cur--; trail_show_card(cur); sfx_cursor();
            }
            if ((n & J_RIGHT) && cur < match_n - 1) {
                cur++; trail_show_card(cur); sfx_cursor();
            }
            if (n & J_A) { spent = 1; break; }
        }
        if (n & J_B) break;
    }

    if (spent) {
        coll_remove(match_list[cur]);
        sfx_play();
        frect(0, 6, 20, 9, T_DITH12, P_FELT);
        switch (e) {
        case E_CACHE: {
            uint8_t got = 0;
            for (i = 0; i < 2; i++)
                if (coll_add(random_card())) {
                    draw_card(3 + got * 5, 7, coll[coll_n - 1]);
                    got++;
                }
            msg_wait(got ? "YOU FOUND CARDS!" : "PACK FULL...");
            break;
        }
        case E_SHRINE: {
            uint8_t free_t = 0xFF;
            for (i = 0; i < N_TALS; i++)
                if (!(tals & (1 << i))) { free_t = i; break; }
            /* pick a random unowned talisman */
            if (free_t != 0xFF) {
                do { i = rnd(N_TALS); } while (tals & (1 << i));
                tals |= 1 << i;
                if ((1 << i) == TAL_WOOL) { hearts_max++; hearts++; }
                print_center(8, tal_names[i], P_FELT);
                print_center(10, tal_descs[i], P_FELT);
                sfx_win();
                msg_wait("A TALISMAN!");
            } else if (hearts < hearts_max) {
                hearts++;
                sfx_heart();
                msg_wait("THE SHRINE HEALS YOU");
            } else {
                msg_wait("THE SHRINE IS QUIET.");
            }
            break;
        }
        case E_TRAVELER:
            if (hearts < hearts_max) { hearts++; sfx_heart(); }
            msg_wait("A WARM MEAL. +HEART");
            break;
        default:
            msg_wait("YOU PRESS ON!");
        }
    } else if (enc_hostile[e]) {
        hearts--;
        sfx_hurt();
        msg_wait("OUCH! -1 HEART");
    } else {
        msg_wait("YOU WALK ON.");
    }
    sfx_cursor();
    screen_close();
}

/* pick the encounters guarding station st */
static void run_trail(uint8_t st)
{
    static const uint8_t hostile_pool[8] = {
        E_CHASM, E_WOLF, E_GUARD, E_ICEFALL,
        E_CHASM, E_WOLF, E_ICEFALL, E_TRAVELER,
    };
    uint8_t e0;

    if (st == 0) return;                    /* a gentle first walk */
    if (st == 2 || st == 5 || st == 8)      e0 = E_SHRINE;
    else if (st == 1 || st == 4 || st == 7) e0 = E_CACHE;
    else                                    e0 = hostile_pool[rnd(8)];
    encounter(e0);
    if (!hearts) return;
    encounter(hostile_pool[rnd(8)]);
}

/* ------------------------------------------------------------------ */
/*  Endings                                                            */
/* ------------------------------------------------------------------ */

/* wipe the logo/subtitle area back to banded sky */
static void clear_sky(void)
{
    uint8_t x, y;
    for (y = 2; y < 9; y++)
        for (x = 0; x < 20; x++)
            put(x, y, T_BLANK, sky_pal(y));
}

static void gameover_screen(void)
{
    DISPLAY_OFF;
    draw_night_scene();
    clear_sky();
    big_text("YOU FALL", 2, 4, TP_SKY1, DYN_BASE);
    print_center(8, "THE MOUNTAIN WINS", TP_GOLD);
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
    big_text("THE SUMMIT", 0, 4, TP_SKY1, DYN_BASE);
    print_center(8, "A TRUE CARD SAGE!", TP_GOLD);
    print(3, 16, "CARDS KEPT:", TP_GND);
    print_u16(15, 16, coll_n, TP_GND);
    print_center(17, "PRESS START", TP_GND);
    screen_open(pals_title);
    sfx_win();
    for (;;) {
        uint8_t n = tick();
        t++;
        if (n & J_START) break;
        if ((t & 63) == 0)  { put(2, 1, T_STAR2, TP_SKY0);
                              put(18, 4, T_STAR2, TP_SKY1); }
        if ((t & 63) == 32) { put(2, 1, T_STAR1, TP_SKY0);
                              put(18, 4, T_STAR1, TP_SKY1); }
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
    for (i = 0; i < 20; i++) coll[i] = start_pack[i];
    coll_n = 20;
    station = 0;
    hearts = hearts_max = 3;
    tals = 0;
}

static void play_run(void)
{
    new_run();
    for (;;) {
        uint8_t n = map_screen();
        if (n & J_SELECT) { help_screens(); continue; }
        if (n & J_START)  { pack_view(); continue; }

        run_trail(station);
        if (!hearts) { gameover_screen(); return; }
        if (!coll_n) { gameover_screen(); return; }

        if (duel()) {
            reward_screen();
            station++;
            if (station >= N_STATIONS) { summit_screen(); return; }
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
    set_sprite_palette(0, 2, pals_spr);
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
