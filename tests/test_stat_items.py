"""Synthetic records and text tables exercise the stat item reader; they describe no
original data. The source tests check its tables against the recovered field menu."""

import re
import struct
import unittest
from pathlib import Path

from tools.analysis.stat_items import (
    ACCESSORIES,
    ACCESSORY_BITS,
    ACCESSORY_NAMES,
    ITEM_BITS,
    ITEM_NAMES,
    ITEMS,
    MENU_DATA,
    RAISES_STATS,
    RECORD,
    names,
    raisers,
    report,
)

ROOT = Path(__file__).resolve().parents[1]


def item(amount: int, flags: int, stats: int) -> bytes:
    return bytes(8) + struct.pack("<BBHH", amount, 0, flags, stats) + bytes(2)


def accessory(stats: int) -> bytes:
    return bytes(0xC) + struct.pack("<H", stats) + bytes(2)


def table(*texts: bytes) -> bytes:
    """A text table: count, 0, count + 1 offsets, count (columns, rows) pairs, texts."""
    head = 4 + 2 * (len(texts) + 1) + 2 * len(texts)
    offsets, position = [], head
    for text in texts:
        offsets.append(position)
        position += len(text) + 1
    data = struct.pack(f"<HH{len(texts) + 1}H", len(texts), 0, *offsets, position)
    return data + bytes(2 * len(texts)) + b"".join(text + b"\0" for text in texts)


def function(path: str, head: str) -> str:
    text = (ROOT / path).read_text()
    body = text[text.index(head) :]
    return body[: body.index("\n}\n")]


class RaiserTests(unittest.TestCase):
    def test_items_raise_their_bits_only_with_flag_4(self):
        items = item(1, RAISES_STATS, 0x2000) + item(5, 0, 0x2000) + item(20, 0x8004, 0x8800)
        found = raisers(items, b"", ["a", "b", "c"], [])
        self.assertEqual(found["ether"], ["item 0 'a' +1"])
        self.assertEqual(found["attack"], ["item 2 'c' +20"])
        self.assertEqual(found["maxHp"], ["item 2 'c' +20"])
        self.assertNotIn("item 1", repr(found))

    def test_accessories_add_the_low_byte_to_each_bonus(self):
        found = raisers(b"", accessory(0x1005) + accessory(0x0802), [], ["x", "y"])
        self.assertEqual(found["equipEther"], ["accessory 0 'x' +5"])
        self.assertEqual(found["equipEtherDefense"], ["accessory 1 'y' +2"])

    def test_report_lists_every_member(self):
        text = report({1: raisers(item(1, RAISES_STATS, 0x2000), b"", ["n"], [])})
        self.assertIn("    ether: 1\n      item 0 'n' +1", text)
        self.assertIn("    equipEther: 0", text)

    def test_names_render_one_byte_glyphs_through_the_character_map(self):
        data = table(b"\x20\x21", b"\x20\x05")
        self.assertEqual(names(data, 0xFE, {0x20: "E", 0x21: "T"}), ["ET", "E{05}"])


class SourceTests(unittest.TestCase):
    """The bit tables and records follow the recovered field menu (slot39)."""

    def test_item_bits_follow_menu_use_item_on_character(self):
        body = function("decomp/src/slot39/menu_member_screens.c", "u8 menu_use_item_on_character(")
        self.assertIn(f"if (record->flags & {RAISES_STATS}) {{", body)
        pairs = re.findall(r"if \(record->stats & (0x[0-9A-F]+)\) \{\s+chara->(\w+) \+= record->amount;", body)
        self.assertEqual({int(bit, 16): member for bit, member in pairs}, ITEM_BITS)

    def test_accessory_bits_follow_menu_compute_character_equipment(self):
        body = function("decomp/src/slot39/menu_member_screens.c", "void menu_compute_character_equipment(")
        self.assertIn("amount = accessory->stats;", body)
        pairs = re.findall(r"if \(accessory->stats & (0x[0-9A-F]+)\) \{\s+chara->(\w+) \+= amount;", body)
        self.assertEqual({int(bit, 16): member for bit, member in pairs}, ACCESSORY_BITS)

    def test_tables_follow_the_loader_and_the_records(self):
        loader = function("decomp/src/slot39/menu_framework.c", "void menu_load_or_release_data_set(")
        self.assertIn(f"cd_select_directory(0x{MENU_DATA[0]:X}, {MENU_DATA[1]});", loader)
        self.assertIn(f"cd_read_file({MENU_DATA[2]}, archive, 0, 0x80);", loader)
        self.assertIn("tables->items = text_unpack_lzss_alloc(archive->items, 0);", loader)
        self.assertIn("tables->accessories = text_unpack_lzss_alloc(archive->accessories, 0);", loader)
        archive = (ROOT / "decomp/src/slot39/menu.h").read_text()
        self.assertIn(f"void *items; /* {4 * (ITEMS + 1):X} */", archive)
        self.assertIn(f"void *accessories; /* {4 * (ACCESSORIES + 1):X} */", archive)
        tables = (ROOT / "decomp/include/menu/tables.h").read_text()
        self.assertIn(f"sizeof(ItemInfo) == 0x{RECORD:X}", tables)
        self.assertIn(f"sizeof(AccessoryInfo) == 0x{RECORD:X}", tables)
        text = (ROOT / "decomp/src/resident/text_windows_and_pads.c").read_text()
        for name, index in (("item", ITEM_NAMES), ("accessory", ACCESSORY_NAMES)):
            self.assertIn(
                f"u8 *text_get_{name}_name(s32 index) {{\n"
                f"    return text_get_resource_entry(text_system_resources[{index}], index);",
                text,
            )


if __name__ == "__main__":
    unittest.main()
