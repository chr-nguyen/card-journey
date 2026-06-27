/*
 * card-journey — Game Boy Color deck-building demo (GBDK-2020)
 *
 * A 32-card deck. Each card has a COLOR (red/yellow/blue) and a SHAPE
 * (square/circle/triangle), encoded as a single value 0..8:
 *     value = color * 3 + shape      (color = value/3, shape = value%3)
 *
 * Screen layout:
 *     - top:    the last card you used
 *     - middle: your hand of 5 (cursor underneath)
 *     - bottom: the face-down deck pile + number of cards remaining
 *
 * Controls:
 *     LEFT / RIGHT  move the cursor
 *     A             use the selected card (it moves to the "used" slot and is
 *                   replaced by a fresh draw); the deck reshuffles when empty
 *     SELECT        reset: new shuffled deck + new hand
 *     START         (on the face-down deck) deal the opening hand
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <stdint.h>

/* ---- gameplay constants ---- */
#define DECK_SIZE 32
#define HAND_SIZE 5
#define NUM_CARDS 9          /* 3 colors x 3 shapes */

/* ---- layout (in 8x8 tiles) ---- */
#define CARD_W 3
#define CARD_H 4
#define USED_X 8             /* used-card slot, top-center */
#define USED_Y 1
#define CARD_Y 7             /* hand row */
#define SLOT_X(s) (1 + (s) * 4)   /* hand slots at columns 1,5,9,13,17 */
#define CURSOR_ROW (CARD_Y + CARD_H)
#define DECK_X 2             /* deck pile, bottom-left */
#define DECK_Y 13
#define COUNT_X (DECK_X + CARD_W + 1)
#define COUNT_Y (DECK_Y + 1)

/* ---- tile indices ---- */
#define T_BLANK    0
#define T_SOLID    1
#define T_SQUARE   2   /* T_SQUARE + shape gives the glyph tile */
#define T_CIRCLE   3
#define T_TRIANGLE 4
#define T_DIGIT0   5   /* T_DIGIT0 + n gives digit n */

/* ---- background palette indices ---- */
#define PAL_RED  0
#define PAL_YEL  1
#define PAL_BLU  2
#define PAL_BACK 3   /* face-down card back */

/* Tile pixel data, 2bpp, 16 bytes per tile (low,high byte per row).
 * Card body pixels = colour (value 1); shape cut-out = white (value 0).
 * Digits use ink value 3, so they read black-on-white under any palette. */
const uint8_t card_tiles[] = {
    /* 0 BLANK */
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    /* 1 SOLID */
    0xFF,0x00, 0xFF,0x00, 0xFF,0x00, 0xFF,0x00,
    0xFF,0x00, 0xFF,0x00, 0xFF,0x00, 0xFF,0x00,
    /* 2 SQUARE */
    0xFF,0x00, 0xFF,0x00, 0xC3,0x00, 0xC3,0x00,
    0xC3,0x00, 0xC3,0x00, 0xFF,0x00, 0xFF,0x00,
    /* 3 CIRCLE */
    0xFF,0x00, 0xE7,0x00, 0xC3,0x00, 0x81,0x00,
    0x81,0x00, 0xC3,0x00, 0xE7,0x00, 0xFF,0x00,
    /* 4 TRIANGLE */
    0xFF,0x00, 0xFF,0x00, 0xE7,0x00, 0xC3,0x00,
    0x81,0x00, 0x00,0x00, 0xFF,0x00, 0xFF,0x00,
    /* 5 '0' */
    0x38,0x38, 0x44,0x44, 0x44,0x44, 0x44,0x44,
    0x44,0x44, 0x44,0x44, 0x38,0x38, 0x00,0x00,
    /* 6 '1' */
    0x10,0x10, 0x30,0x30, 0x10,0x10, 0x10,0x10,
    0x10,0x10, 0x10,0x10, 0x38,0x38, 0x00,0x00,
    /* 7 '2' */
    0x38,0x38, 0x44,0x44, 0x04,0x04, 0x18,0x18,
    0x20,0x20, 0x40,0x40, 0x7C,0x7C, 0x00,0x00,
    /* 8 '3' */
    0x38,0x38, 0x44,0x44, 0x04,0x04, 0x18,0x18,
    0x04,0x04, 0x44,0x44, 0x38,0x38, 0x00,0x00,
    /* 9 '4' */
    0x08,0x08, 0x18,0x18, 0x28,0x28, 0x48,0x48,
    0x7C,0x7C, 0x08,0x08, 0x08,0x08, 0x00,0x00,
    /* 10 '5' */
    0x7C,0x7C, 0x40,0x40, 0x78,0x78, 0x04,0x04,
    0x04,0x04, 0x44,0x44, 0x38,0x38, 0x00,0x00,
    /* 11 '6' */
    0x38,0x38, 0x40,0x40, 0x40,0x40, 0x78,0x78,
    0x44,0x44, 0x44,0x44, 0x38,0x38, 0x00,0x00,
    /* 12 '7' */
    0x7C,0x7C, 0x04,0x04, 0x08,0x08, 0x10,0x10,
    0x20,0x20, 0x20,0x20, 0x20,0x20, 0x00,0x00,
    /* 13 '8' */
    0x38,0x38, 0x44,0x44, 0x44,0x44, 0x38,0x38,
    0x44,0x44, 0x44,0x44, 0x38,0x38, 0x00,0x00,
    /* 14 '9' */
    0x38,0x38, 0x44,0x44, 0x44,0x44, 0x3C,0x3C,
    0x04,0x04, 0x04,0x04, 0x38,0x38, 0x00,0x00,
};
#define NUM_TILES 15

/* Cursor sprite (an up-pointing arrow). */
const uint8_t cursor_tile[] = {
    0x18,0x00, 0x3C,0x00, 0x7E,0x00, 0xFF,0x00,
    0x18,0x00, 0x18,0x00, 0x18,0x00, 0x00,0x00,
};

/* Background palettes: {white, main colour, shadow, black} */
const palette_color_t bkg_pals[] = {
    RGB_WHITE, RGB(31, 0, 0),  RGB(15, 0, 0),  RGB_BLACK,   /* red    */
    RGB_WHITE, RGB(31,31, 0),  RGB(20,16, 0),  RGB_BLACK,   /* yellow */
    RGB_WHITE, RGB( 0, 0,31),  RGB( 0, 0,15),  RGB_BLACK,   /* blue   */
    RGB_WHITE, RGB(18,18,18),  RGB(10,10,10),  RGB_BLACK,   /* back   */
};

/* Sprite palette: index 0 is transparent. */
const palette_color_t spr_pals[] = {
    RGB_WHITE, RGB_BLACK, RGB_BLACK, RGB_BLACK,
};

/* ---- game state ---- */
uint8_t deck[DECK_SIZE];
uint8_t deck_pos;
int8_t  hand[HAND_SIZE];
int8_t  last_used;
uint8_t cursor;

/* Rebuild the deck with random cards and shuffle (Fisher-Yates). */
void build_deck(void) {
    uint8_t i, j, t;
    for (i = 0; i < DECK_SIZE; i++) {
        deck[i] = (uint8_t)rand() % NUM_CARDS;
    }
    for (i = DECK_SIZE - 1; i > 0; i--) {
        j = (uint8_t)rand() % (i + 1);
        t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
    deck_pos = 0;
}

/* Draw the next card value off the top of the deck. */
int8_t draw_card_value(void) {
    if (deck_pos >= DECK_SIZE) build_deck();
    return (int8_t)deck[deck_pos++];
}

/* Write one BG tile plus its CGB palette attribute. */
void put_tile(uint8_t x, uint8_t y, uint8_t tile, uint8_t pal) {
    set_bkg_tile_xy(x, y, tile);
    VBK_REG = 1;
    set_bkg_tile_xy(x, y, pal);
    VBK_REG = 0;
}

/* Render a card (face-up) at a tile position; card < 0 clears the slot. */
void draw_card_at(uint8_t bx, uint8_t by, int8_t card) {
    uint8_t pal   = (card < 0) ? PAL_RED : (uint8_t)card / 3;
    uint8_t shape = (card < 0) ? 0       : (uint8_t)card % 3;
    uint8_t r, c, tile;
    for (r = 0; r < CARD_H; r++) {
        for (c = 0; c < CARD_W; c++) {
            if (card < 0)                 tile = T_BLANK;
            else if (r == 1 && c == 1)    tile = T_SQUARE + shape;
            else                          tile = T_SOLID;
            put_tile(bx + c, by + r, tile, pal);
        }
    }
}

/* Render a face-down (gray) card pile at a tile position. */
void draw_back_at(uint8_t bx, uint8_t by) {
    uint8_t r, c;
    for (r = 0; r < CARD_H; r++)
        for (c = 0; c < CARD_W; c++)
            put_tile(bx + c, by + r, T_SOLID, PAL_BACK);
}

/* Show how many cards are left in the deck (two digits). */
void draw_count(void) {
    uint8_t rem  = DECK_SIZE - deck_pos;
    uint8_t tens = rem / 10;
    uint8_t ones = rem % 10;
    put_tile(COUNT_X,     COUNT_Y, tens ? (T_DIGIT0 + tens) : T_BLANK, PAL_RED);
    put_tile(COUNT_X + 1, COUNT_Y, T_DIGIT0 + ones, PAL_RED);
}

/* Move the cursor sprite under the selected hand slot. */
void update_cursor(void) {
    uint8_t cx = SLOT_X(cursor) + 1;             /* centre column */
    move_sprite(0, cx * 8 + 8, CURSOR_ROW * 8 + 16);
}

/* Fresh deck + new hand. LCD is briefly turned off to avoid tearing. */
void new_game(void) {
    uint8_t i;
    build_deck();
    wait_vbl_done();
    DISPLAY_OFF;
    for (i = 0; i < HAND_SIZE; i++) {
        hand[i] = draw_card_value();
        draw_card_at(SLOT_X(i), CARD_Y, hand[i]);
    }
    last_used = -1;
    draw_card_at(USED_X, USED_Y, last_used);   /* empty used slot */
    draw_back_at(DECK_X, DECK_Y);              /* the deck pile    */
    draw_count();
    cursor = 0;
    update_cursor();
    DISPLAY_ON;
}

void main(void) {
    uint16_t seed = 1;
    uint8_t i, k, p, prev;

    /* Turn the LCD off so we can freely rewrite VRAM. */
    DISPLAY_OFF;

    /* Load graphics. */
    set_bkg_data(0, NUM_TILES, card_tiles);
    set_bkg_palette(0, 4, bkg_pals);
    set_sprite_data(0, 1, cursor_tile);
    set_sprite_palette(0, 1, spr_pals);
    set_sprite_tile(0, 0);

    /* Clear the whole 32x32 background map AND its colour attributes,
     * otherwise the boot ROM's Nintendo logo tiles linger in the gaps. */
    VBK_REG = 0;
    fill_bkg_rect(0, 0, 32, 32, T_BLANK);   /* tiles      -> blank      */
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, PAL_RED);   /* attributes -> palette 0  */
    VBK_REG = 0;

    /* Show a face-down deck and wait for START (also gathers RNG entropy). */
    for (i = 0; i < HAND_SIZE; i++) draw_back_at(SLOT_X(i), CARD_Y);
    SHOW_BKG;
    DISPLAY_ON;
    while (!(joypad() & J_START)) seed++;
    initrand(seed);

    new_game();
    SHOW_SPRITES;

    prev = J_START;   /* ignore the START still being held */
    for (;;) {
        wait_vbl_done();
        k = joypad();
        p = k & ~prev;

        if ((p & J_LEFT)  && cursor > 0)             { cursor--; update_cursor(); }
        if ((p & J_RIGHT) && cursor < HAND_SIZE - 1) { cursor++; update_cursor(); }
        if (p & J_A) {
            last_used = hand[cursor];                       /* card you played */
            draw_card_at(USED_X, USED_Y, last_used);
            hand[cursor] = draw_card_value();              /* replacement      */
            draw_card_at(SLOT_X(cursor), CARD_Y, hand[cursor]);
            draw_count();
        }
        if (p & J_SELECT) {
            new_game();
        }
        prev = k;
    }
}
