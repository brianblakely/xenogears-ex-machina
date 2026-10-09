"""Synthetic tables, records and bytecode exercise the special part census;
they describe no original data."""

import re
import struct
import unittest
from collections import defaultdict
from pathlib import Path

from tools.analysis import events
from tools.analysis.special_parts import (
    BATTLE_END,
    CHARACTER_WEAPONS,
    COUNT,
    EMPTIED,
    FIRST,
    GAME_DATA_SIZE,
    GEAR_BASE,
    GEAR_PARTS,
    GEAR_RECORD_WEAPONS,
    GEAR_SHOP,
    ITEM_BASE,
    LAYOUT,
    LOCAL_VARIABLES,
    LOCKED,
    RECORD,
    RECORDS,
    SHOP,
    SHOP_WEAPONS,
    TABLES,
    Census,
    alias,
    drops,
    in_lists,
    item_codes,
    listed,
    report,
    shop_ids,
    specials,
    written,
)

ROOT = Path(__file__).resolve().parents[1]


def function(path: str, head: str) -> str:
    text = (ROOT / path).read_text()
    body = text[text.index(head) :]
    return body[: body.index("\n}\n")]


class LayoutTests(unittest.TestCase):
    def test_layout_follows_gamedata_h(self):
        text = (ROOT / "decomp/include/resident/gamedata.h").read_text()
        found = re.findall(r"OFFSET_OF\(GameData, (\w+)\) == (0x[0-9A-F]+)", text)
        offsets = {name: int(value, 16) for name, value in found}
        for start, _, name in LAYOUT:
            if name in offsets:
                self.assertEqual(offsets[name], start, name)
        for name in ("ammo", "gearAmmo"):
            self.assertRegex(text, rf"u8 {name}\[{COUNT}\];")
        self.assertEqual([start for start, _, _ in LAYOUT[1:]], [s + n for s, n, _ in LAYOUT[:-1]])
        self.assertIn(f"sizeof(GameData) == 0x{GAME_DATA_SIZE:X}", text)
        self.assertIn(f"OFFSET_OF(CharacterRecord, weapons) == 0x{CHARACTER_WEAPONS:X}", text)
        self.assertRegex(text, rf"u8 weapons\[4\]; +/\* 0x{GEAR_RECORD_WEAPONS:02X}:")

    def test_the_arrays_lie_from_fifty_bytes_past_each_base(self):
        arrays = {name: start for start, _, name in LAYOUT}
        self.assertEqual(ITEM_BASE + FIRST, arrays["ammo"])
        self.assertEqual(GEAR_BASE + FIRST, arrays["gearAmmo"])
        self.assertEqual(GEAR_BASE, arrays["flags"])
        self.assertEqual(GEAR_BASE + FIRST + COUNT, LOCKED)

    def test_constants_follow_the_c(self):
        loader = function("decomp/src/ovl2615/ovl2615.c", "void func_801E5384(void) {")
        record = TABLES["weapon"][1]
        self.assertIn("archive[0x25]", loader)
        self.assertIn(f"(u8 *)block + 0x{record * FIRST:x}, 0x{record * COUNT:x})", loader)
        lists = function("decomp/src/ovl2615/ovl2615.c", "void func_801E4CD0(void) {")
        self.assertIn(f"D_8006D634.weaponIds[i] >= {FIRST} && D_8006D634.weaponIds[i] < {BATTLE_END}", lists)
        offered = function("decomp/src/slot39/slot39_801DBE54.c", "s32 func_801DE5CC(")
        self.assertIn(f"D_8006D634.gearPartIds[i] >= {FIRST}", offered)
        self.assertIn(f"D_8006D634.weaponIds[i] >= {FIRST}", offered)
        reset = function("decomp/src/field/field.c", "void func_800705DC(void) {")
        self.assertIn(f"D_800C3A68[i + 0x{LOCAL_VARIABLES // 2:x}] = 0;", reset)
        stock = function("decomp/src/ovl2602/ovl2602.c", "void func_801C6E74(void) {")
        self.assertIn(f"D_80059171 * 0x{GEAR_SHOP:x}", stock)
        self.assertIn(f"stock[4][i] = *(entry + i + 0x{GEAR_PARTS[0]:X});", stock)
        shop = function("decomp/src/ovl2601/ovl2601.c", "void func_801C6A6C(void) {")
        self.assertIn(f"D_80059171 * 0x{SHOP:X}", shop)
        self.assertIn(f"shop_kinds[j] = i / {SHOP_WEAPONS};", shop)
        copy = function("decomp/src/ovl2615/ovl2615.c", "void func_801E4870(void) {")
        self.assertIn(f"D_800C3DD0 + 0x{RECORDS:x}", copy)
        take = function("decomp/src/field/field_800854D0.c", "void func_8009640C(void) {")
        self.assertRegex(take, rf"if \(--counts\[slot\] == 0\) \{{\s+ids\[slot\] = 0x{EMPTIED:X};")

    def test_alias_names_what_each_index_reaches(self):
        self.assertEqual(alias(ITEM_BASE, 0), "gearAccessoryIds[108]")
        self.assertEqual(alias(ITEM_BASE, 50), "ammo[0]")
        self.assertEqual(alias(ITEM_BASE, 97), "ammo[47]")
        self.assertEqual(alias(ITEM_BASE, 98), "gearAmmo[0]")
        self.assertEqual(alias(GEAR_BASE, 0), "flags byte 0")
        self.assertEqual(alias(GEAR_BASE, 1), "flags byte 1")
        self.assertEqual(alias(GEAR_BASE, 72), "gearAmmo[22]")
        self.assertEqual(alias(GEAR_BASE, 98), "locked byte 0")
        self.assertEqual(alias(GEAR_BASE, 101), "map byte 1")
        self.assertEqual(alias(GEAR_BASE, 0x88), "+0x233e")  # inside the game data, not in LAYOUT
        self.assertEqual(alias(GEAR_BASE, EMPTIED), "+0x23b5, 0x8006f9e9 past the game data")


class TableTests(unittest.TestCase):
    def test_specials_take_the_records_from_fifty_with_a_kind(self):
        _, size, users, width, load, kind = TABLES["weapon"]
        table = bytearray(size * 100)
        rows = {10: (0x10, 1, 100), 50: (0x10, 1, 100), 67: (0x10, 5, 255)}
        for index, (mask, value, rounds) in rows.items():
            row = size * index
            table[row + users : row + users + width] = mask.to_bytes(width, "little")
            table[row + kind], table[row + load] = value, rounds
        table[size * 73 + users] = 0x10  # users but no kind: an empty record
        found, empty = specials(bytes(table), size, users, width, load, kind)
        self.assertEqual(found, {50: (0x10, 1, 100), 67: (0x10, 5, 255)})
        self.assertEqual(empty, [i for i in range(FIRST, 100) if i not in (50, 67)])

    def test_shop_ids_read_each_whole_record(self):
        table = bytearray(SHOP * 2 + 4)
        table[0:3] = bytes([1, 50, 0])
        table[SHOP_WEAPONS] = 90  # an accessory
        table[SHOP + 5] = 61
        self.assertEqual(shop_ids(bytes(table), SHOP, (0, SHOP_WEAPONS)), {0: {1, 50}, 1: {61}})
        gear = bytearray(GEAR_SHOP)
        gear[GEAR_PARTS[0]] = 55
        gear[GEAR_PARTS[1] - 1] = 101
        gear[GEAR_PARTS[1]] = 47  # a gear accessory (stock[3])
        self.assertEqual(shop_ids(bytes(gear), GEAR_SHOP, GEAR_PARTS), {0: {55, 101}})

    def test_drops_read_both_drops_of_whole_records(self):
        data = bytearray(RECORDS + RECORD + 0x20)
        data[RECORDS + 0x152 : RECORDS + 0x156] = bytes([50, 60, 0, 3])
        self.assertEqual(drops(bytes(data)), [(0, 50), (3, 60)])


class FieldTests(unittest.TestCase):
    def decode(self, code: bytes) -> list:
        """Every instruction of `code`, which an end_slot (00) closes."""
        out, pc, code = [], 0, code + b"\x00"
        while pc < len(code):
            out.append(events.decode_instruction(code, pc))
            pc += out[-1].size
        return out

    def test_a_local_variable_set_by_immediates_resolves_give_item(self):
        set_weapon = bytes([0x35, 0x10, 0x05]) + struct.pack("<H", 0x135) + b"\x40"
        set_part = bytes([0x35, 0x10, 0x05]) + struct.pack("<H", 0x338) + b"\x40"
        give = bytes([0x8C, 0x10, 0x05])
        ins = self.decode(set_weapon + set_part + give + bytes([0x8C, 0x35, 0x83]))
        writes = written(ins)
        self.assertEqual(writes[0x510], [0x135, 0x338])
        self.assertEqual(item_codes(ins[2], writes), [0x135, 0x338])
        self.assertEqual(item_codes(ins[3], writes), [0x335])
        self.assertEqual(
            [listed(c) for c in (0x135, 0x338, 0x131, 0x235)],
            [("weapon list", 53), ("gear part list", 56), None, None],
        )

    def test_other_writers_and_saved_variables_stay_unresolved(self):
        set_local = bytes([0x35, 0x10, 0x05]) + struct.pack("<H", 0x135) + b"\x40"
        add = bytes([0x38, 0x10, 0x05]) + struct.pack("<H", 1) + b"\x40"
        set_saved = bytes([0x35, 0x16, 0x00]) + struct.pack("<H", 0x135) + b"\x40"
        gives = bytes([0x8C, 0x10, 0x05, 0x8C, 0x16, 0x00, 0x8C, 0x12, 0x05])
        ins = self.decode(set_local + add + set_saved + gives)
        writes = written(ins)
        self.assertIsNone(item_codes(ins[3], writes))  # v0510 also added to
        self.assertIsNone(item_codes(ins[4], writes))  # v0016 is saved, set elsewhere too
        self.assertIsNone(item_codes(ins[5], writes))  # v0512 never written

    def test_take_item_resolves_as_give_item_and_names_any_id(self):
        set_part = bytes([0x35, 0x10, 0x05]) + struct.pack("<H", 0x32B) + b"\x40"
        take = bytes([0x8D, 0x10, 0x05, 0x8D, 0x2B, 0x81, 0x8D, 0x05, 0x80])
        ins = self.decode(set_part + take)
        writes = written(ins)
        self.assertEqual([i.key for i in ins[1:4]], ["8d"] * 3)
        self.assertEqual(item_codes(ins[1], writes), [0x32B])
        self.assertEqual(item_codes(ins[2], writes), [0x12B])
        self.assertEqual(
            [in_lists(c) for c in (0x32B, 0x12B, 0x005, 0x22B)],
            [("gear part list", 43), ("weapon list", 43), None, None],
        )
        self.assertIsNone(listed(0x32B))  # below FIRST: no special part

    def test_bit_operands_and_camera_stores_write_variables(self):
        set_local = bytes([0x35, 0x10, 0x05]) + struct.pack("<H", 0x135) + b"\x40"
        set_bit = bytes([0xFE, 0x0A]) + struct.pack("<H", 0x510 << 4 | 3)
        store_heading = bytes([0xAF, 0x12, 0x05, 0x00])
        set_heading = bytes([0xAF, 0x14, 0x05, 0x01])  # a raw heading, no variable
        set_other = bytes([0x35, 0x14, 0x05]) + struct.pack("<H", 0x135) + b"\x40"
        ins = self.decode(set_local + set_bit + store_heading + set_heading + set_other)
        writes = written(ins)
        self.assertEqual(writes[0x510], [0x135, None])
        self.assertEqual(writes[0x512], [None])
        self.assertEqual(writes[0x514], [0x135])


class ReportTests(unittest.TestCase):
    def test_report_names_the_word_an_id_past_the_arrays_reaches(self):
        census = Census(battle_records=True)
        census.tables = {"weapon": ({50: (0x10, 1, 100)}, list(range(51, 100)))}
        census.new_game = {
            "ammo": bytes([100] * COUNT),
            "gearAmmo": bytes([100] * COUNT),
            "flags": 0x8000,
            "locked": 0,
            "gearAccessoryIds[108]": 0,
            "slots": [("character 4", (31, 0, 0, 37, 0), (50, 0, 0, 57, 0))],
        }
        census.sources["gear part list"] = defaultdict(set, {55: {"gear shop 14"}})
        census.unopened["gear part list"] = defaultdict(set, {101: {"gear shop 15"}})
        census.taken["gear part list"] = defaultdict(set, {43: {"take_item map 1 +0x10"}})
        census.shop_tables = {"shop": (2, SHOP * 2 + 4), "gear shop": (1, GEAR_SHOP + 5)}
        text = report({1: census})
        self.assertIn("weapon table: ids 50 users [0x10] kinds [1] load byte [100]", text)
        self.assertIn("gear part list, ids only in tables no immediate opens: 101", text)
        self.assertIn(
            "gear part list ids past 97: 101 -> map byte 1,"
            " 255 (take_item) -> +0x23b5, 0x8006f9e9 past the game data",
            text,
        )
        self.assertIn("weapon list ids past 97: none", text)
        self.assertIn("take_item from the gear part list (its last copy leaves id 0xff): 43", text)
        self.assertIn("    43: take_item map 1 +0x10", text)
        self.assertIn("take_item from the weapon list (its last copy leaves id 0xff): none", text)
        tables = "shop tables, whole records: shops 2 (188 bytes), gear shops 1 (105 bytes)"
        self.assertIn(tables, text)
        self.assertIn("new game flags 0x8000, locked 0x0000, gearAccessoryIds[108] 0", text)
        self.assertIn("character 4 weapons [31, 0, 0, 37, 0] ammo [50, 0, 0, 57, 0]", text)


if __name__ == "__main__":
    unittest.main()
