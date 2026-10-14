"""The field's random draw runs unless the host turns its map's random battles off."""
import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native


class RandomBattles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "encounters_support.c"
        # An invented host that turns maps 7 and 9 off, and a stand-in draw
        # that counts its runs.
        support.write_text('''#include "common.h"
s32 mode_field_map_id;
u32 test_asked = 0xFFFFFFFF;
s32 test_draws;
u32 xem_host_random_battles(u32 map) {
    test_asked = map;
    return map != 7 && map != 9;
}
s32 xem_original_field_encounter_count_down(void) {
    test_draws++;
    return 0;
}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/encounters.c", str(support)])

    def global_(self, name, kind=ctypes.c_int):
        return kind.in_dll(self.lib, name)

    def test_the_draw_runs_unless_the_host_turns_its_map_off(self):
        maps = ((7, False), (8, True), (0x4007, False), (0xC009, False), (0xC00A, True))
        for map_id, runs in maps:
            self.global_("mode_field_map_id").value = map_id
            before = self.global_("test_draws").value
            self.lib.field_encounter_count_down()
            asked = self.global_("test_asked", ctypes.c_uint).value
            self.assertEqual(asked, map_id & 0xFFF, "the map, not its flag bits")
            ran = self.global_("test_draws").value - before
            self.assertEqual(ran, int(runs), f"map {map_id:#x}")


if __name__ == "__main__":
    unittest.main()
