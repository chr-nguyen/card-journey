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
import scenes

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


def tiled_art(add, name, rows):
    """Split deliberately drawn pixel clusters into native 8x8 tiles."""
    rows = rows.strip("\n").splitlines() if isinstance(rows, str) else rows
    width = len(rows[0])
    if width % 8 or len(rows) % 8 or any(len(row) != width for row in rows):
        raise ValueError(f"{name}: inconsistent or non-tile-aligned artwork")
    index = 0
    for y in range(0, len(rows), 8):
        for x in range(0, width, 8):
            add(f"{name}_{index}", "\n".join(row[x:x + 8] for row in rows[y:y + 8]))
            index += 1


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
.....##.
....#1#.
...#1#..
..#1#...
.#2#....
..##2...
.#..#...
22222222
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
.#####..
.#111#..
.##22##.
..#..1#.
..#.21#.
..#111#.
..#####.
22222222
""")
bg("T_ICO_GIVE", """
.##.##..
#11#11#.
#1.111#.
.11111..
..111...
...1....
........
22222222
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
22222222
22222222
2222##22
222#2222
22222222
22222222
22222222
""")
bg("T_ROCK_L", diag_edge(2, True))
bg("T_ROCK_R", diag_edge(2, False))
bg("T_SNOW", """
11111111
11111111
11111111
11112211
11111111
11111111
11111111
11111111
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

# Original shrine creatures. Portraits occupy 16x16 pixels, centered in the
# 24-pixel card body. One four-color affinity palette supplies paper, light,
# shadow, and ink, so the same silhouette reads in both Ruby and Jade.
PORTRAITS = {
    "WISP": """
.......##.......
......#112#.....
.....#1.112#....
....#1.11122#...
...#11####122#..
...#1######2#...
..#11##..##22#..
..#12######22#..
..#112####222#..
.#11.112222222#.
.#1..11#222#22#.
#11.11#1222#222#
##111#112222#2##
.#11#11222222#..
..##11##22##2#..
...###..##..##..
""",
    "GOLEM": """
.....######.....
....#..1122#....
....#.11#22#....
....#11##22#....
....########....
....#.#..#2#....
....#11##22#....
..###11##22###..
.#.11######122#.
#.112#11112#222#
#1122#11#12#222#
.####211#122###.
..#.#2111122#.#.
..###22##222###.
...#112##222#...
...####..####...
""",
    "GARGOYLE": """
.....#....#.....
....#1#..#2#....
#...#11##22#...#
##..#1.11.2#..##
#1#..#1##2#..#2#
#11#.#1112#.#22#
#121##1##2##212#
#1221#1122#1222#
#1#2211111222#2#
##.#21111222#.##
#...#11##22#...#
....#112222#....
...#11#22#22#...
..#112####222#..
..####....####..
................
""",
}
for name, art in PORTRAITS.items():
    rows = art.strip().splitlines()
    if len(rows) != 16 or any(len(row) != 16 for row in rows):
        raise ValueError(f"{name}: portrait must be 16x16")
    tiled_art(bg, f"T_CARD_{name}", ["#2.." + row + "..2#" for row in rows])

# Shape seals remain visible above the illustration.
for name, seal in (
    ("CIRCLE", [".###.", "#...#", "#...#", "#...#", ".###."]),
    ("SQUARE", ["#####", "#...#", "#...#", "#...#", "#####"]),
    ("TRIANGLE", ["..#..", ".###.", ".#.#.", "##.##", "#####"]),
):
    rows = ["########"]
    rows += ["1" + row.replace(".", "1").replace("#", ".") + "11" for row in seal]
    rows += ["11111111", "22222222"]
    bg(f"T_SEAL_{name}", "\n".join(rows))

# Low-contrast surfaces keep the card silhouettes and lettering dominant.
bg("T_TABLE", """
........
........
..11....
....1...
........
......11
........
........
""")
bg("T_TABLE_TRIM", """
22222222
11111111
........
........
........
........
........
........
""")
bg("T_LINK", """
........
........
...11...
22211222
22211222
...11...
........
........
""")
bg("T_AP_FULL", """
........
..###...
.#...#..
.#.1.#..
.#111#..
..###...
........
........
""")
bg("T_AP_EMPTY", """
........
..222...
.2...2..
.2...2..
.2...2..
..222...
........
........
""")
slot = [["."] * 24 for _ in range(32)]
# Rounded, interrupted tide-ring rather than a cathedral arch.
for y in range(2, 9):
    left = (8, 5, 4, 3, 2, 2, 2)[y - 2]
    for x in (left, 23 - left):
        slot[y][x] = "2"
        slot[y][x - 1 if x < 12 else x + 1] = "1"
for x in range(8, 16):
    slot[1][x] = "2"
for x in range(2, 22):
    slot[29][x] = "1"
    slot[30][x] = "2"
for y in range(9, 29):
    slot[y][1] = "1"
    slot[y][2] = "1" if y % 6 == 0 else "2"
    slot[y][21] = "1" if y % 6 == 0 else "2"
    slot[y][22] = "1"
for x, y in ((11, 13), (10, 14), (12, 14), (9, 15), (13, 15),
             (10, 16), (12, 16), (11, 17)):
    slot[y][x] = "1"
tiled_art(bg, "T_SLOT", ["".join(row) for row in slot])

# Small duelist portraits give the two sides a face, without consuming OAM.
AVATARS = {
    "HERO": """
.....######.....
....#222222#....
...#22111122#...
..#2211111122#..
..#21#111#122#..
..#2111111122#..
...#11111112#...
....#11#112#....
...##111122##...
..#222####222#..
.#222211112222#.
.#22#211112#22#.
.#22#222222#22#.
..##22222222##..
....########....
................
""",
    "RIVAL": """
....###..##.....
...#222##22#....
..#222222222#...
..#222222222#...
...#1111111#....
...#1#111#1#....
...#1111111#....
....#11#11#.....
....#11111#.....
...#2#####2#....
..#222111222#...
.#22221112222#..
.#22#22222#22#..
..##2222222##...
....#######.....
................
""",
    "SAGE": """
.......##.......
......#22#......
.....#2222#.....
....#222222#....
...#22222222#...
..############..
...#1#111#1#....
...#1111111#....
....#11111#.....
...#2111112#....
..#222111222#...
.#22222122222#..
.#22#22222#22#..
..##2222222##...
....#######.....
................
""",
    "KEEPER": """
....########....
...#22222222#...
..#2111111122#..
..#21######12#..
..#21#1111#12#..
..#21#1##1#12#..
..#21######12#..
...#22222222#...
....###22###....
...#222##222#...
..#2211111122#..
.#222211112222#.
.#22#221122#22#.
..##22222222##..
....########....
................
""",
}
for name, art in AVATARS.items():
    tiled_art(bg, f"T_AVATAR_{name}", art)

# Asymmetric cyclopean stones, a floating eye, and a submerged portal.
# 48x32 scene; all pixels are authored on the native grid, never resampled.
ruin = [["."] * 48 for _ in range(32)]
for y in range(10, 32):
    for left, right, top in ((3 + (31 - y) // 8, 10, 14), (36, 43 - (31 - y) // 9, 10)):
        if y < top:
            continue
        for x in range(left, right + 1):
            ruin[y][x] = "#" if x in (left, right) or y == top else "2"
    if y >= 18:
        left, right = (15, 32) if y < 22 else (17, 30)
        for x in range(left, right + 1):
            ruin[y][x] = "2" if x in (left, left + 1, right - 1, right) or y < 21 else "#"
for y in range(4, 11):
    half = (2, 6, 9, 11, 9, 6, 2)[y - 4]
    for x in range(24 - half, 25 + half):
        ruin[y][x] = "1" if abs(x - 24) == half else "2"
    for x in range(23, 26):
        ruin[y][x] = "#" if y not in (4, 10) else "1"
for x, y in ((7, 18), (6, 21), (7, 24), (39, 14), (38, 17), (39, 20), (19, 19), (28, 19)):
    ruin[y][x] = "1"
    ruin[y + 1][x] = "1"
for x in range(48):
    if x % 7 < 4:
        ruin[30][x] = "1"
tiled_art(bg, "T_RUIN", ["".join(row) for row in ruin])
tiled_art(bg, "T_SIGIL", """
........
...22...
..2##2..
.2#11#2.
..2##2..
...22...
........
..2..2..
..2..2..
.2....2.
.2....2.
..2..2..
...22...
........
........
........
""")
bg("T_MENHIR", """
...22...
..222#..
..212#..
.2212#..
.21222#.
.22212#.
.22222#.
########
""")
tiled_art(bg, "T_KELP", """
...222..
..2...2.
..2..2..
..22....
...22...
....22..
.....2..
..2..2..
.2...2..
.2..22..
..222...
...22...
...22...
..22....
.2222...
........
""")
bg("T_WAVE", """
........
..111...
11...111
........
........
.....22.
22222..2
........
""")
bg("T_OWNER_DOWN", """
........
.#...#..
..#.#...
...#....
.#...#..
..#.#...
...#....
........
""")
bg("T_OWNER_UP", """
........
...#....
..#.#...
.#...#..
...#....
..#.#...
.#...#..
........
""")
bg("T_HAND_PICK", """
........
...#....
..##....
.#####..
..##....
...#....
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

spr("S_SELECT_CORNER", """
11111111
12222222
12......
12......
12......
12......
12......
12......
""")
spr("S_SELECT_H", """
11111111
........
........
........
........
........
........
........
""")
spr("S_SELECT_V", """
12......
12......
12......
12......
12......
12......
12......
12......
""")

# Three short 16x16 effect frames. Generated on the same pixel grid as the art;
# no scaling, filtering, or large opaque overlays are involved.
for effect in ("SLASH", "HEAL", "DRAW"):
    for frame in range(3):
        canvas = [["."] * 16 for _ in range(16)]
        def pixel(x, y, color):
            if 0 <= x < 16 and 0 <= y < 16:
                canvas[y][x] = color
        if effect == "SLASH":
            for step in range(5 + frame * 5):
                x, y = 14 - step, 1 + step
                pixel(x + 1, y, "2")
                pixel(x, y, "1")
                pixel(x, y + 1, "1")
        else:
            radius = 2 + frame * 3
            for x, y in ((8 - radius, 8), (8 + radius, 8),
                         (8, 8 - radius), (8, 8 + radius)):
                pixel(x, y, "1")
                pixel(x - 1, y, "2")
                pixel(x + 1, y, "2")
                pixel(x, y - 1, "2")
                pixel(x, y + 1, "2")
            if effect == "HEAL":
                for step in range(-2, 3):
                    pixel(8 + step, 8, "1")
                    pixel(8, 8 + step, "1")
            else:
                for step in range(5):
                    pixel(6 + step, 5 - frame, "1")
                    pixel(6 + step, 10 - frame, "2")
                    pixel(6, 5 + step - frame, "1")
                    pixel(10, 5 + step - frame, "2")
        tiled_art(spr, f"S_{effect}_{frame}", ["".join(row) for row in canvas])

# Fur hood, rust expedition coat, backpack, and a wind-tossed scarf.
# Four 8x8 hardware sprites form each 16x16 explorer frame.
EXPLORER = """
.....######.....
....#111111#....
...#11222211#...
...#12####21#...
...#11#11#11#...
....#111111#....
..###222222###..
.#22#222222#11#.
.#22#221122#11#.
..###222222###..
...#22222222#...
...#22222222#...
....###22###....
....#11##11#....
....####.###....
................
"""
tiled_art(spr, "S_EXPLORER_A", EXPLORER)
explorer_b = EXPLORER.strip().splitlines()
explorer_b[6] = "..###222222###.."
explorer_b[7] = ".#22#222222#11#."
explorer_b[8] = ".#22#221122#11#."
explorer_b[10] = "...#22222222##.."
explorer_b[11] = "...#2222222222#."
explorer_b[12] = "....###22#####.."
tiled_art(spr, "S_EXPLORER_B", explorer_b)
spr("S_ROUTE_FLAG", """
.111111.
.12222..
.1222...
.1......
.1......
.1......
.1......
.1......
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
    # Title text uses 44 additional tiles. It must never overwrite static art.
    if len(data) + 44 > 256:
        raise ValueError("Static art plus title lettering exceeds the tile bank")

    spr_names = []
    spr_data = []
    for name, art in SPR:
        spr_names.append(name)
        spr_data.append(tile_from_art(art, name))
    if len(spr_data) > 128:
        raise ValueError("Sprite patterns would overlap this renderer's background tiles")

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
    scene_h, scene_c = scenes.export()
    h.extend(scene_h)
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

    c.extend(scene_c)

    with open(os.path.join(OUT_DIR, "assets.h"), "w") as f:
        f.write("\n".join(h) + "\n")
    with open(os.path.join(OUT_DIR, "assets.c"), "w") as f:
        f.write("\n".join(c) + "\n")
    print(f"wrote {len(data)} bg tiles, {len(spr_data)} sprite tiles")


if __name__ == "__main__":
    main()
