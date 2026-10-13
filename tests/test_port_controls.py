"""The hosts' control queries read the original game's state where it is meaningful."""
import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native

WALKING = 1
PICTURE = 2
MENU = 4


class FieldControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "controls_support.c"
        # Two invented pictures (map 7 with item 0x40, map 9 with 0x41), room
        # for eleven, and the original's inventory check over one held item.
        support.write_text('''#include "common.h"
#include "battle/turn.h"
TurnState *battle_turn_state;
s32 mode_field_map_id;
s32 field_player_control_polled;
s32 field_menu_request;
s32 field_picture_table[11 * 8 + 1] = {
    7, 0, 0, 0, 0x40, 0, 0, 0,
    9, 0, 0, 0, 0x41, 0, 0, 0,
    0xFFFF,
};
s32 test_held = -1;
s32 field_inventory_is_item_not_held(s32 item) { return item == test_held ? 0 : -1; }
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/controls.c", str(support)])
        cls.lib.xem_game_field_controls.argtypes = [ctypes.c_uint]
        cls.lib.xem_game_field_controls.restype = ctypes.c_uint

    def set(self, name, value):
        ctypes.c_int.in_dll(self.lib, name).value = value

    def setUp(self):
        self.set("mode_field_map_id", 0x4007)
        self.set("test_held", -1)
        self.set("field_player_control_polled", 1)
        self.set("field_menu_request", 0xFF)

    def test_select_shows_the_maps_picture_only_with_its_item_held(self):
        controls = self.lib.xem_game_field_controls
        self.assertEqual(controls(1), WALKING, "the picture's item is not held")
        self.set("test_held", 0x41)
        self.assertEqual(controls(1), WALKING, "another map's item")
        self.set("test_held", 0x40)
        self.assertEqual(controls(1), WALKING | PICTURE, "the map's id below its flag bits")
        self.set("mode_field_map_id", 8)
        self.assertEqual(controls(1), WALKING, "a map without a picture")

    def test_the_player_walks_while_its_control_reads_the_pad_and_no_menu_runs(self):
        controls = self.lib.xem_game_field_controls
        for request in (0x80, 0):
            self.set("field_menu_request", request)
            self.assertEqual(controls(1), MENU, f"the menu runs ({request:#x}: the button's, a script's)")
        self.set("field_menu_request", 0xFF)
        self.set("field_player_control_polled", 0)
        self.assertEqual(controls(1), 0, "a dialogue, an event")

    def test_a_table_without_its_end_is_read_no_further_than_its_pictures(self):
        table = (ctypes.c_int * (11 * 8 + 1)).in_dll(self.lib, "field_picture_table")
        saved = list(table)
        self.addCleanup(lambda: [table.__setitem__(i, v) for i, v in enumerate(saved)])
        # Memory the loading field has not filled yet: no end within ten
        # pictures, and an eleventh that would match.
        for picture in range(11):
            table[picture * 8] = 0x100 + picture
            table[picture * 8 + 4] = 0x40
        self.set("test_held", 0x40)
        self.set("mode_field_map_id", 0x109)
        self.assertEqual(self.lib.xem_game_field_controls(1), WALKING | PICTURE, "the tenth is read")
        self.set("mode_field_map_id", 0x10A)
        self.assertEqual(self.lib.xem_game_field_controls(1), WALKING, "the eleventh is not")

    def test_field_overlay_memory_is_read_only_while_the_field_runs(self):
        self.set("test_held", 0x40)
        for mode in (0, 2, 3, 4, 5, 6, 0xFFFFFFFF):
            self.assertEqual(self.lib.xem_game_field_controls(mode), 0, f"mode {mode:#x} has no field overlay")


if __name__ == "__main__":
    unittest.main()
