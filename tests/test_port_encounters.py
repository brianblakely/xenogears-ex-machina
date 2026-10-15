"""The field's random draw runs unless the host turns its map's random battles off, and every
other field frame where the host halves them."""
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
        # An invented host that turns maps 7 and 9 off and halves map 11's,
        # and a stand-in draw that counts its runs.
        support.write_text('''#include "common.h"
s32 mode_field_map_id;
s32 field_dialogue_tick_count;
u32 test_asked = 0xFFFFFFFF;
s32 test_draws;
u32 xem_host_random_battles(u32 map) {
    test_asked = map;
    if (map == 7 || map == 9) {
        return 0;
    }
    return map == 11 ? 2 : 1;
}
s32 xem_original_field_encounter_count_down(void) {
    test_draws++;
    return 0;
}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/encounters.c", str(support)])

    def global_(self, name, kind=ctypes.c_int):
        return kind.in_dll(self.lib, name)

    def draws(self, map_id, ticks):
        """The draw's runs over field frames `ticks` on field map `map_id`."""
        self.global_("mode_field_map_id").value = map_id
        before = self.global_("test_draws").value
        for tick in ticks:
            self.global_("field_dialogue_tick_count").value = tick
            self.lib.field_encounter_count_down()
            asked = self.global_("test_asked", ctypes.c_uint).value
            self.assertEqual(asked, map_id & 0xFFF, "the map, not its flag bits")
        return self.global_("test_draws").value - before

    def test_the_draw_runs_unless_the_host_turns_its_map_off(self):
        maps = ((7, False), (8, True), (0x4007, False), (0xC009, False), (0xC00A, True))
        for map_id, runs in maps:
            self.assertEqual(self.draws(map_id, [0]), int(runs), f"map {map_id:#x}")

    def test_halved_the_draw_runs_on_every_other_field_frame(self):
        self.assertEqual(self.draws(11, range(10)), 5)
        self.assertEqual(self.draws(0x800B, range(1, 8, 2)), 0, "the odd frames, never")
        self.assertEqual(self.draws(8, range(10)), 10, "the original's, every frame")


if __name__ == "__main__":
    unittest.main()
