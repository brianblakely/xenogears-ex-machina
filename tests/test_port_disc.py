"""The game's disc check tells the host which disc it waits for, and whether
the story's disc change asks."""
import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native


class DiscCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "disc_support.c"
        # The host's import and the original check, recording their calls.
        support.write_text('''#include "common.h"
u8 menu_state_screen;
s32 test_wanted[2] = { -1, -1 };
s32 test_checked = -1;
void xem_host_cd_wanted(u32 disc, u32 story) { test_wanted[0] = disc; test_wanted[1] = story; }
s32 xem_original_menu_cd_check_disc(s32 disc) { test_checked = disc; return 0; }
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/disc.c", str(support)])
        cls.lib.menu_cd_check_disc.argtypes = [ctypes.c_int]
        cls.lib.menu_cd_check_disc.restype = ctypes.c_int

    def check(self, screen, disc):
        ctypes.c_ubyte.in_dll(self.lib, "menu_state_screen").value = screen
        self.assertEqual(self.lib.menu_cd_check_disc(disc), 0)
        wanted = (ctypes.c_int * 2).in_dll(self.lib, "test_wanted")
        return (wanted[0], wanted[1], ctypes.c_int.in_dll(self.lib, "test_checked").value)

    def test_the_story_change_asks_with_its_wait(self):
        self.assertEqual(self.check(6, 2), (2, 1, 2), "menu kind 6: disc 2, the story's change, then the check")

    def test_the_title_screen_asks_at_once(self):
        self.assertEqual(self.check(2, 1), (1, 0, 1), "the file screen: disc 1 for a new game, no wait")


if __name__ == "__main__":
    unittest.main()
