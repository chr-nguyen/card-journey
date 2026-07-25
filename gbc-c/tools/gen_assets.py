#!/usr/bin/env python3
"""Generate src/assets.c / src/assets.h for the Card Journey GBC ROM.

Every tile is defined here as 8x8 ASCII art using palette values:
    '.' = 0   (usually white / sky / transparent-for-sprites)
    '1' = 1   (main colour of whatever palette the tile is drawn with)
    '2' = 2   (shadow / secondary colour)
    '#' = 3   (ink / outline)
The art is encoded to native Game Boy 2bpp format at build time so the
C source stays readable and the ROM stays tiny.
"""

import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "src")


def tile_from_art(art, name):
    rows = [r for r in art.strip("\n").split("\n")]
    if len(rows) > 8:
        raise ValueError(f"{name}: expected <=8 rows, got {len(rows)}")
    rows += ["........"] * (8 - len(rows))
    data = []
    for r in rows:
        r = r.ljust(8, ".")
        if len(r) != 8:
            raise ValueError(f"{name}: row wider than 8: '{r}'")
        lo = hi = 0
        for x, ch in enumerate(r):
            v = {".": 0, "1": 1, "2": 2, "3": 3, "#": 3}[ch]
            if v & 1:
                lo |= 0x80 >> x
            if v & 2:
                hi |= 0x80 >> x
        data += [lo, hi]
    return data


def solid(v):
    row = str(v) * 8 if v else "." * 8
    return "\n".join([row] * 8)


def checker(a, b):
    ca = str(a) if a else "."
    cb = str(b) if b else "."
    r0 = (ca + cb) * 4
    r1 = (cb + ca) * 4
    return "\n".join([r0, r1] * 4)


def diag_edge(fill, rising):
    """Diagonal mountain-slope edge: sky (0) above, `fill` below.
    rising=True gives a '/' left slope, False gives a '\\' right slope."""
    rows = []
    for y in range(8):
        row = ""
        for x in range(8):
            below = (x >= 7 - y) if rising else (x <= y)
            on_edge = (x == 7 - y) if rising else (x == y)
            row += "#" if on_edge else (str(fill) if below else ".")
        rows.append(row)
    return "\n".join(rows)


def weave(phase):
    rows = []
    for y in range(8):
        rows.append("".join("1" if ((x + y + phase) % 4) < 2 else "2"
                            for x in range(8)))
    return "\n".join(rows)


# ---------------------------------------------------------------- font ----
FONT = {
"A": """
.###.
#...#
#...#
#####
#...#
#...#
#...#
""",
"B": """
####.
#...#
#...#
####.
#...#
#...#
####.
""",
"C": """
.####
#....
#....
#....
#....
#....
.####
""",
"D": """
####.
#...#
#...#
#...#
#...#
#...#
####.
""",
"E": """
#####
#....
#....
####.
#....
#....
#####
""",
"F": """
#####
#....
#....
####.
#....
#....
#....
""",
"G": """
.####
#....
#....
#..##
#...#
#...#
.####
""",
"H": """
#...#
#...#
#...#
#####
#...#
#...#
#...#
""",
"I": """
#####
..#..
..#..
..#..
..#..
..#..
#####
""",
"J": """
....#
....#
....#
....#
#...#
#...#
.###.
""",
"K": """
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
""",
"L": """
#....
#....
#....
#....
#....
#....
#####
""",
"M": """
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
""",
"N": """
#...#
##..#
#.#.#
#..##
#...#
#...#
#...#
""",
"O": """
.###.
#...#
#...#
#...#
#...#
#...#
.###.
""",
"P": """
####.
#...#
#...#
####.
#....
#....
#....
""",
"Q": """
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
""",
"R": """
####.
#...#
#...#
####.
#.#..
#..#.
#...#
""",
"S": """
.####
#....
#....
.###.
....#
....#
####.
""",
"T": """
#####
..#..
..#..
..#..
..#..
..#..
..#..
""",
"U": """
#...#
#...#
#...#
#...#
#...#
#...#
.###.
""",
"V": """
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
""",
"W": """
#...#
#...#
#...#
#.#.#
#.#.#
##.##
#...#
""",
"X": """
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
""",
"Y": """
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
""",
"Z": """
#####
....#
...#.
..#..
.#...
#....
#####
""",
"0": """
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
""",
"1": """
..#..
.##..
..#..
..#..
..#..
..#..
#####
""",
"2": """
.###.
#...#
....#
..##.
.#...
#....
#####
""",
"3": """
.###.
#...#
....#
..##.
....#
#...#
.###.
""",
"4": """
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
""",
"5": """
#####
#....
####.
....#
....#
#...#
.###.
""",
"6": """
.###.
#....
#....
####.
#...#
#...#
.###.
""",
"7": """
#####
....#
...#.
..#..
..#..
..#..
..#..
""",
"8": """
.###.
#...#
#...#
.###.
#...#
#...#
.###.
""",
"9": """
.###.
#...#
#...#
.####
....#
....#
.###.
""",
"!": """
..#..
..#..
..#..
..#..
..#..
.....
..#..
""",
"?": """
.###.
#...#
....#
..##.
..#..
.....
..#..
""",
".": """
.....
.....
.....
.....
.....
.##..
.##..
""",
",": """
.....
.....
.....
.....
..##.
..##.
.#...
""",
"-": """
.....
.....
.....
.###.
.....
.....
.....
""",
":": """
.....
.##..
.##..
.....
.##..
.##..
.....
""",
"/": """
....#
....#
...#.
..#..
.#...
#....
#....
""",
"+": """
.....
..#..
..#..
#####
..#..
..#..
.....
""",
"x": """
.....
#...#
.#.#.
..#..
.#.#.
#...#
.....
""",
"'": """
..#..
..#..
.....
.....
.....
.....
.....
""",
">": """
#....
##...
###..
####.
###..
##...
#....
""",
"=": """
.....
.....
#####
.....
#####
.....
.....
""",
}
FONT_ORDER = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?.,-:/+x'>=")

# ------------------------------------------------------------- bg tiles ----
BG = []          # list of (NAME, art)


def bg(name, art):
    BG.append((name, art))


bg("T_BLANK",  solid(0))
bg("T_SOLID1", solid(1))
bg("T_SOLID2", solid(2))
bg("T_SOLID3", solid(3))
bg("T_DITH01", checker(0, 1))
bg("T_DITH12", checker(1, 2))
bg("T_DITH23", checker(2, 3))

# --- card frame (3x4-tile card: black outline, colour-1 header/footer band,
#     1px colour accent down the sides, white face) ---
bg("T_CARD_TL", """
########
##111111
#1111111
#1111111
#1......
#1......
#1......
#1......
""")
bg("T_CARD_T", """
########
11111111
11111111
11111111
........
........
........
........
""")
bg("T_CARD_TR", """
########
111111##
1111111#
1111111#
......1#
......1#
......1#
......1#
""")
bg("T_CARD_L", """
#1......
#1......
#1......
#1......
#1......
#1......
#1......
#1......
""")
bg("T_CARD_FACE", solid(0))
bg("T_CARD_R", """
......1#
......1#
......1#
......1#
......1#
......1#
......1#
......1#
""")
bg("T_CARD_BL", """
#1......
#1......
#1......
#1......
#1111111
#1111111
##111111
########
""")
bg("T_CARD_B", """
........
........
........
........
11111111
11111111
11111111
########
""")
bg("T_CARD_BR", """
......1#
......1#
......1#
......1#
1111111#
1111111#
111111##
########
""")

# --- shape glyphs (drawn on the white card face) ---
bg("T_SHP_CIR", """
..####..
.#1111#.
#111111#
#111111#
#111111#
#111111#
.#1111#.
..####..
""")
bg("T_SHP_SQR", """
########
#111111#
#111111#
#111111#
#111111#
#111111#
#111111#
########
""")
bg("T_SHP_TRI", """
...##...
...##...
..#11#..
..#11#..
.#1111#.
.#1111#.
#111111#
########
""")
bg("T_SHP_DIA", """
...##...
..#11#..
.#1111#.
#111111#
#111111#
.#1111#.
..#11#..
...##...
""")

# --- action-verb icons ---
bg("T_ICO_MOVE", """
...##...
..####..
.##..##.
##....##
...##...
..####..
.##..##.
##....##
""")
bg("T_ICO_FIGHT", """
......##
.....###
....###.
#..###..
.####...
..##....
.#.##...
##..#...
""")
bg("T_ICO_SNEAK", """
........
..####..
.#....#.
#..##..#
#..##..#
.#....#.
..####..
........
""")
bg("T_ICO_SPEAK", """
.######.
#......#
#.#.#.##
#......#
.######.
..##....
..#.....
........
""")
bg("T_ICO_TAKE", """
...##...
..#..#..
..####..
.#....#.
.#.11.#.
.#.11.#.
.#....#.
..####..
""")
bg("T_ICO_GIVE", """
..#..#..
...##...
.######.
.#.##.#.
.######.
.#1##1#.
.#1##1#.
.######.
""")

# --- tiny value pips for the card corner (2 / 4 / 6 / 8) ---
bg("T_PIP2", """
###.....
..#.....
###.....
#.......
###.....
........
........
........
""")
bg("T_PIP4", """
#.#.....
#.#.....
###.....
..#.....
..#.....
........
........
........
""")
bg("T_PIP6", """
###.....
#.......
###.....
#.#.....
###.....
........
........
........
""")
bg("T_PIP8", """
###.....
#.#.....
###.....
#.#.....
###.....
........
........
........
""")

# --- card back weave ---
bg("T_WEAVE_A", weave(0))
bg("T_WEAVE_B", weave(2))

# --- dialog window border (thin rounded box, white inside) ---
bg("T_WIN_TL", """
..######
.#......
#.......
#.......
#.......
#.......
#.......
#.......
""")
bg("T_WIN_T", """
########
........
........
........
........
........
........
........
""")
bg("T_WIN_TR", """
######..
......#.
.......#
.......#
.......#
.......#
.......#
.......#
""")
bg("T_WIN_L", """
#.......
#.......
#.......
#.......
#.......
#.......
#.......
#.......
""")
bg("T_WIN_R", """
.......#
.......#
.......#
.......#
.......#
.......#
.......#
.......#
""")
bg("T_WIN_BL", """
#.......
#.......
#.......
#.......
#.......
#.......
.#......
..######
""")
bg("T_WIN_B", """
........
........
........
........
........
........
........
########
""")
bg("T_WIN_BR", """
.......#
.......#
.......#
.......#
.......#
.......#
......#.
######..
""")

# --- HUD icons ---
bg("T_HEART", """
.##..##.
#11##11#
#111111#
#111111#
.#1111#.
..#11#..
...##...
........
""")
bg("T_HEART_E", """
.##..##.
#..##..#
#......#
#......#
.#....#.
..#..#..
...##...
........
""")
bg("T_ICO_DECK", """
######..
#....#..
#.11.#..
#.11.#..
#....#..
######..
........
........
""")

# --- map / scenery ---
bg("T_FLAG", """
1111#...
111#....
11#.....
1#......
#.......
#.......
#.......
#.......
""")
bg("T_FLAG_DONE", """
2222#...
222#....
22#.....
2#......
#.......
#.......
#.......
#.......
""")
bg("T_STAR1", """
........
...#....
..###...
...#....
........
........
........
........
""")
bg("T_STAR2", """
......#.
........
........
..#.....
........
........
.....#..
........
""")
bg("T_MOON", """
...###..
..##....
.##.....
.##.....
.##.....
..##....
...###..
........
""")
bg("T_ROCK", """
22222222
22322222
22222232
22222222
23222222
22222322
22222222
22232222
""")
bg("T_ROCK_L", diag_edge(2, True))
bg("T_ROCK_R", diag_edge(2, False))
bg("T_SNOW", """
11111111
11121111
11111111
11111211
12111111
11111121
11111111
11211111
""")
bg("T_SNOW_L", diag_edge(1, True))
bg("T_SNOW_R", diag_edge(1, False))
bg("T_SNOWCAP", """
########
11111111
11211111
11111121
11111111
12111112
11111111
11111211
""")
bg("T_TREE_TOP", """
...#....
..#1#...
.#111#..
..#1#...
.#111#..
#11111#.
.#111#..
#11111#.
""")
bg("T_TREE_BOT", """
#11111#.
1111111#
########
...##...
...##...
...##...
..####..
........
""")
bg("T_PATHDOT", """
........
........
...##...
..#..#..
...##...
........
........
........
""")

# ------------------------------------------------------------ sprites ----
SPR = []


def spr(name, art):
    SPR.append((name, art))


spr("S_ARROW_UP", """
...##...
..#11#..
.#1111#.
#111111#
###11###
..#11#..
..#11#..
..####..
""")
spr("S_ARROW_RT", """
.#......
.##.....
.#1#....
.#11#...
.#11#...
.#1#....
.##.....
.#......
""")
spr("S_CLIMBER_A", """
..##....
..11....
.####...
#1##1#..
.####...
..##....
..#.#...
.#...#..
""")
spr("S_CLIMBER_B", """
..##....
..11....
.####...
.#1##1#.
.####...
..##....
..#.#...
..#..#..
""")
spr("S_SPARKLE", """
...#....
...#....
.#####..
...#....
...#....
........
........
........
""")

# ------------------------------------------------------------- output ----


def main():
    names = []
    data = []
    for name, art in BG:
        names.append(name)
        data.append(tile_from_art(art.replace("１", "1"), name))
    font_base = len(names)
    for ch in FONT_ORDER:
        data.append(tile_from_art(FONT[ch], f"font '{ch}'"))

    spr_names = []
    spr_data = []
    for name, art in SPR:
        spr_names.append(name)
        spr_data.append(tile_from_art(art, name))

    h = ["/* Generated by tools/gen_assets.py - do not edit by hand. */",
         "#ifndef ASSETS_H", "#define ASSETS_H", "#include <stdint.h>", ""]
    for i, n in enumerate(names):
        h.append(f"#define {n} {i}")
    h.append("")
    h.append(f"#define FONT_BASE {font_base}")
    h.append(f"#define FONT_COUNT {len(FONT_ORDER)}")
    h.append(f"#define BG_TILE_COUNT {font_base + len(FONT_ORDER)}")
    h.append("")
    for i, n in enumerate(spr_names):
        h.append(f"#define {n} {i}")
    h.append(f"#define SPR_TILE_COUNT {len(spr_names)}")
    h.append("")
    h.append("extern const uint8_t bg_tiles[];")
    h.append("extern const uint8_t spr_tiles[];")
    h.append("extern const uint8_t font_chars[];")
    h.append("#endif")

    c = ["/* Generated by tools/gen_assets.py - do not edit by hand. */",
         '#include "assets.h"', "",
         "const uint8_t bg_tiles[] = {"]
    all_names = names + [f"FONT '{ch}'" for ch in FONT_ORDER]
    for i, t in enumerate(data):
        c.append(f"    /* {i:3d} {all_names[i]} */")
        c.append("    " + ",".join(f"0x{b:02X}" for b in t) + ",")
    c.append("};")
    c.append("")
    c.append("const uint8_t spr_tiles[] = {")
    for i, t in enumerate(spr_data):
        c.append(f"    /* {i:3d} {spr_names[i]} */")
        c.append("    " + ",".join(f"0x{b:02X}" for b in t) + ",")
    c.append("};")
    c.append("")
    c.append("/* Characters in font order, used by the C side to map ASCII. */")
    c.append("const uint8_t font_chars[] = \"" +
             FONT_ORDER.replace("\\", "\\\\").replace('"', '\\"') + "\";")

    with open(os.path.join(OUT_DIR, "assets.h"), "w") as f:
        f.write("\n".join(h) + "\n")
    with open(os.path.join(OUT_DIR, "assets.c"), "w") as f:
        f.write("\n".join(c) + "\n")
    print(f"wrote {len(data)} bg tiles, {len(spr_data)} sprite tiles")


if __name__ == "__main__":
    main()
