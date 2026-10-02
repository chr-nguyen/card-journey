"""Exercise the ROM through normal buttons using the host AI as a test driver.

Requires PyBoy, the matching main.c assembly listing, and a host rules library.
RAM is read for assertions; the test never writes game memory.
"""
import argparse
import ctypes as ct
import re
import struct
import zlib
from pathlib import Path
from pyboy import PyBoy


class Slot(ct.Structure):
    _fields_ = [("card", ct.c_uint8), ("hp", ct.c_uint8)]


class Side(ct.Structure):
    _fields_ = [("deck", ct.c_uint8 * 20), ("discard", ct.c_uint8 * 20),
                ("hand", ct.c_uint8 * 7), ("deck_n", ct.c_uint8),
                ("discard_n", ct.c_uint8), ("hand_n", ct.c_uint8),
                ("hp", ct.c_uint8), ("board", Slot * 3)]


class Battle(ct.Structure):
    _fields_ = [("side", Side * 2), ("rng", ct.c_uint16), ("turn", ct.c_uint16),
                ("active", ct.c_uint8), ("plays", ct.c_uint8), ("winner", ct.c_uint8)]


class Move(ct.Structure):
    _fields_ = [("hand", ct.c_uint8), ("lane", ct.c_uint8), ("target", ct.c_uint8)]


class Preview(ct.Structure):
    _fields_ = [("amount", ct.c_uint8), ("targets", ct.c_uint8), ("replacing", ct.c_uint8)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", default="card-journey.gbc")
    parser.add_argument("--asm", default="/tmp/card-journey-main.asm")
    parser.add_argument("--rules", default="/tmp/card-journey-rules.so")
    parser.add_argument("--boot-frames", type=int, default=300)
    parser.add_argument("--lose-run", action="store_true", help="Pass every turn to exercise the loss ending")
    parser.add_argument("--skip-first-reward", action="store_true")
    parser.add_argument("--exercise-draw", action="store_true", help="Prefer connected TAKE cards to exercise hand scrolling")
    parser.add_argument("--screenshot-dir", type=Path, default=Path("/tmp"))
    parser.add_argument("--screenshot-scale", type=int, choices=(1, 2, 4), default=1)
    args = parser.parse_args()
    args.screenshot_dir.mkdir(parents=True, exist_ok=True)
    assets = {name: int(value) for name, value in re.findall(
        r"#define (\w+) (\d+)", Path("src/assets.h").read_text())}
    rules = ct.CDLL(args.rules)
    rules.ai_choose.argtypes = [ct.POINTER(Battle), ct.POINTER(Move)]
    rules.ai_choose.restype = ct.c_uint8
    rules.ai_keep.argtypes = [ct.POINTER(Battle)]
    rules.ai_keep.restype = ct.c_uint8
    rules.card_valid.argtypes = [ct.c_uint8]
    rules.card_valid.restype = ct.c_uint8
    rules.battle_play.argtypes = [ct.POINTER(Battle), ct.c_uint8, ct.c_uint8, ct.c_uint8]
    rules.battle_play.restype = ct.c_uint8
    rules.battle_preview.argtypes = [ct.POINTER(Battle), ct.c_uint8, ct.c_uint8, ct.POINTER(Preview)]
    rules.battle_preview.restype = ct.c_uint8
    asm = Path(args.asm).read_text().split("\t.area _DATA", 1)[1].split("\t.area _INITIALIZED", 1)[0]
    offsets, offset = {}, 0
    for name, size in re.findall(r"(_\w+):\s*\.ds (\d+)", asm):
        offsets[name] = offset
        offset += int(size)
    gb = PyBoy(args.rom, window="null", sound_emulated=False)
    gb.set_emulation_speed(0)

    def tick(frames=30):
        gb.tick(frames, True)

    def press(key, frames=40):
        gb.button_press(key)
        tick(6)
        gb.button_release(key)
        tick(frames)

    def screenshot(name):
        pixels = gb.screen.ndarray
        pixels = pixels.repeat(args.screenshot_scale, axis=0).repeat(args.screenshot_scale, axis=1)
        height, width, _ = pixels.shape
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        raw = b"".join(b"\0" + pixels[row].tobytes() for row in range(height))
        png = b"\x89PNG\r\n\x1a\n"
        png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
        (args.screenshot_dir / ("card-journey-" + name + ".png")).write_bytes(png)

    tick(args.boot_frames)
    screenshot("title")
    press("start", 90)
    screenshot("map")
    # Locate the initialized permanent starter deck, then derive local symbol
    # addresses from SDCC's listing rather than hardcoding linker addresses.
    starter = bytes(ct.cast(rules.starter_deck, ct.POINTER(ct.c_uint8 * 10)).contents)
    ram = bytes(gb.memory[0xC000:0xD000])
    candidates = [i for i in range(len(ram) - 25)
                  if ram[i:i + 10] == starter and ram[i + 20:i + 24] == bytes([10, 0, 3, 3])]
    assert len(candidates) == 1, "Cannot locate initialized run state"
    base = 0xC000 + candidates[0]

    def addr(name):
        return base + offsets["_" + name]

    def read(name):
        return gb.memory[addr(name)]

    def battle():
        raw = bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121])
        value = Battle()
        # Side is 57 bytes in both ABIs. SDCC omits the two-byte alignment gap
        # before uint16 fields; reconstruct explicitly for host AI calls.
        ct.memmove(ct.addressof(value), raw[:114], 114)
        value.rng = int.from_bytes(raw[114:116], "little")
        value.turn = int.from_bytes(raw[116:118], "little")
        value.active, value.plays, value.winner = raw[118:121]
        return value

    def assert_frame(x, y):
        # Check all ten hardware sprites, not just the original four corners.
        sprite = 1
        for row in range(4):
            for col in range(3):
                if col == 1 and row not in (0, 3):
                    continue
                tile = ("S_SELECT_H" if col == 1 else "S_SELECT_CORNER") if row in (0, 3) else "S_SELECT_V"
                prop = (0x20 if col == 2 else 0) | (0x40 if row == 3 else 0)
                expected = [(y + row) * 8 + 16, (x + col) * 8 + 8, assets[tile], prop]
                actual = list(gb.memory[0xFE00 + sprite * 4:0xFE04 + sprite * 4])
                assert actual == expected, ("Selection outline", sprite, actual, expected)
                sprite += 1

    def assert_ownership():
        font = assets["FONT_BASE"]
        for index, char in enumerate("ENEMY"):
            assert gb.memory[0x9800 + (1 + index) * 32 + 18] == font + ord(char) - ord("A")
        for x in (6, 11, 16):
            assert gb.memory[0x9800 + 5 * 32 + x] == assets["T_OWNER_DOWN"]
            assert gb.memory[0x9800 + 10 * 32 + x] == assets["T_OWNER_UP"]

    press("a", 90)
    screenshot("battle")
    assert_frame(0, 11)
    assert_ownership()
    def stable_navigation(key):
        # Capture EVERY displayed frame, not only the settled result. Moving a
        # hand selection must never clear the board or blank the LCD. Card
        # interiors exclude the two-pixel sprite outline that intentionally moves.
        board = gb.screen.ndarray[:88].copy()
        interiors = [gb.screen.ndarray[90:118, col * 32 + 2:col * 32 + 22].copy()
                     for col in range(5)]
        gb.button_press(key)
        for frame in range(46):
            if frame == 6:
                gb.button_release(key)
            tick(1)
            assert gb.memory[0xFF40] & 0x80, "LCD blanked during selection movement"
            assert (gb.screen.ndarray[:88] == board).all(), ("Board flicker", key, frame)
            for col, interior in enumerate(interiors):
                assert (gb.screen.ndarray[90:118, col * 32 + 2:col * 32 + 22] == interior).all(), (
                    "Hand card flicker", key, frame, col)

    stable_navigation("right")
    assert_frame(4, 11)
    stable_navigation("left")
    assert_frame(0, 11)
    initial = bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121])
    press("a")  # Hand -> placement, cancel without consuming anything.
    assert read("duel_phase") == 1
    assert_frame(3, 6)
    assert gb.memory[0x9800 + 14 * 32 + 3] == assets["T_HAND_PICK"]
    screenshot("placement")
    press("b")
    assert_frame(0, 11)
    assert bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121]) == initial
    press("select")
    for sprite in range(11):
        assert gb.memory[0xFE00 + sprite * 4] == 0, "Selection leaked into inspection"
    screenshot("inspection")
    press("down")
    press("down")
    press("b")
    assert bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121]) == initial
    press("start")
    assert read("duel_phase") == 3
    press("b")
    assert bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121]) == initial
    press("start")
    press("a")
    assert read("duel_phase") == 4
    screenshot("keep")
    assert_frame(0, 11)
    press("right")
    assert_frame(4, 11)
    press("b")
    assert read("duel_phase") == 0
    assert bytes(gb.memory[addr("duel_state"):addr("duel_state") + 121]) == initial

    victories = defeats = actions = max_hand = 0
    saved_target = False
    while victories < 3 and defeats < 3 and actions < 400:
        value = battle()
        if value.winner != 255:
            tick(90)  # Let lethal attack feedback and the result jingle finish.
            old_station, old_lives = read("station"), read("hearts")
            won = value.winner == 0
            screenshot("won" if won else "lost")
            press("a", 100)
            if won:
                victories += 1
                assert read("station") == old_station + 1
                if victories == 3:
                    screenshot("summit")
                    break
                old_count = read("coll_n")
                old_deck = bytes(gb.memory[addr("coll"):addr("coll") + old_count])
                tick(30)
                screenshot("reward")
                assert_frame(3, 6)
                press("right")
                assert_frame(9, 6)
                press("left")
                assert_frame(3, 6)
                for x in (3, 9, 15):
                    assert gb.memory[0x9800 + 6 * 32 + x] == assets["T_CARD_TL"], "Reward card frame missing"
                skip = args.skip_first_reward and victories == 1
                if args.exercise_draw and not skip:
                    for offer, x in enumerate((4, 10, 16)):
                        if gb.memory[0x9800 + 9 * 32 + x] == assets["T_ICO_TAKE"]:
                            for _ in range(offer):
                                press("right")
                            break
                press("b" if skip else "a", 100)
                if not skip:
                    screenshot("replace")
                    assert_frame(14, 6)
                    press("right")
                    assert_frame(14, 6)
                    press("left")
                    press("a", 100)
                assert read("coll_n") == old_count == 10
                new_deck = bytes(gb.memory[addr("coll"):addr("coll") + old_count])
                assert new_deck == old_deck if skip else new_deck[1:] == old_deck[1:]
            else:
                defeats += 1
                assert read("hearts") == old_lives - 1, (old_lives, read("hearts"))
                if defeats == 3:
                    screenshot("gameover")
                    break
            press("a", 100)  # Map -> next battle / retry.
            continue
        if value.active == 1:
            tick(120)
            continue
        tick(30)  # Finish the board transition before issuing the next input.
        value = battle()
        # The transition can advance into an enemy turn or a result screen.
        # Never choose a player move from a state read before that transition.
        if value.active != 0 or value.winner != 255:
            continue
        max_hand = max(max_hand, value.side[0].hand_n)
        if value.side[0].hand_n > 5:
            old_hand = read("duel_hand")
            state_before = bytes(value)
            for _ in range(value.side[0].hand_n - 1 - old_hand):
                press("right")
            assert read("duel_scroll") > 0
            assert_frame((read("duel_hand") - read("duel_scroll")) * 4, 11)
            screenshot("scrolling-hand")
            for _ in range(value.side[0].hand_n - 1 - old_hand):
                press("left")
            assert bytes(battle()) == state_before
        move = Move()
        chosen = rules.ai_choose(ct.byref(value), ct.byref(move)) if value.plays else False
        if args.exercise_draw and value.plays:
            best_draw = 0
            for hand in range(value.side[0].hand_n):
                if value.side[0].hand[hand] & 7 != 4:
                    continue
                for lane in range(3):
                    preview = Preview()
                    if (rules.battle_preview(ct.byref(value), hand, lane, ct.byref(preview))
                            and preview.amount > best_draw):
                        move = Move(hand, lane, 255)
                        chosen = True
                        best_draw = preview.amount
            if value.turn == 1:
                for hand in range(value.side[0].hand_n):
                    if (value.side[0].hand[hand] >> 3) & 3 == 1:
                        move = Move(hand, 1, 255)
                        chosen = True
                        break
        if args.lose_run or not value.plays or not chosen:
            press("start")
            if read("duel_phase") == 3:
                press("a")
            if read("duel_phase") == 4:
                state = battle()
                kept = 255 if args.lose_run else rules.ai_keep(ct.byref(state))
                kept_card = state.side[0].hand[kept] if kept != 255 else None
                if kept == 255:
                    press("start", 40)
                else:
                    while read("duel_hand") != kept:
                        press("right" if read("duel_hand") < kept else "left")
                    press("a", 40)
                # Wait for the player's attack to finish, then check the hand
                # before the opponent can finish its own turn or play our card.
                for _ in range(180):
                    after = battle()
                    if after.active == 1 or after.winner != 255:
                        break
                    tick(1)
                else:
                    raise AssertionError(("Player attack did not end", after.active,
                                          after.turn, read("duel_phase"), kept))
                if after.winner == 255:
                    assert after.side[0].hand_n == (1 if kept_card is not None else 0)
                    if kept_card is not None:
                        assert after.side[0].hand[0] == kept_card
                tick(150)
            else:
                tick(150)
            actions += 1
            continue
        # Navigate to a legal host-selected move using the game controls.
        while read("duel_hand") != move.hand:
            press("right" if read("duel_hand") < move.hand else "left")
        press("a")
        assert read("duel_phase") == 1, (read("duel_phase"), value.active, value.plays, value.turn)
        while read("duel_lane") != move.lane:
            press("right" if read("duel_lane") < move.lane else "left")
        expected = battle()
        assert rules.battle_play(ct.byref(expected), move.hand, move.lane, move.target), (
            "Invalid driver move", move.hand, move.lane, move.target,
            "before", value.turn, value.active, value.plays, list(value.side[0].hand),
            "after", expected.turn, expected.active, expected.plays, list(expected.side[0].hand))
        press("a")
        if move.target != 255:
            assert read("duel_phase") == 2
            if not saved_target:
                before = battle()
                screenshot("target")
                assert_frame((3, 8, 13)[read("duel_target")],
                             1 if value.side[0].hand[move.hand] & 7 == 1 else 6)
                press("b")
                assert read("duel_phase") == 1
                after = battle()
                assert bytes(before) == bytes(after)
                press("a")
                saved_target = True
            for _ in range(3):
                if read("duel_target") == move.target:
                    break
                press("right")
            assert read("duel_target") == move.target
            press("a")
        actual = battle()
        assert bytes(actual) == bytes(expected), "Emulated placement differs from host rules"
        screenshot("board")
        assert_ownership()
        if any(slot.card != 255 for slot in actual.side[1].board):
            screenshot("ownership")
        for lane in range(2):
            a, b = actual.side[0].board[lane].card, actual.side[0].board[lane + 1].card
            if a != 255 and b != 255 and (a >> 5) == (b >> 5):
                screenshot("connected")
        actions += 1
    assert victories == 3 or defeats == 3, "Run did not reach an ending"
    if not args.lose_run:
        assert saved_target, "Target selection was not exercised"
    else:
        assert defeats == 3 and victories == 0
    if args.exercise_draw:
        assert max_hand > 5, "Connected TAKE did not exercise the scrolling hand"
    print(f"Emulator smoke passed: {victories} wins, {defeats} defeats, {actions} actions; "
          f"largest hand {max_hand}; cancellation and applicable host/ROM rule agreement checked.")
    gb.stop(save=False)


if __name__ == "__main__":
    main()
