"""A message window's step reveals at least the host's text speed, and only that."""
import tempfile
import unittest
from pathlib import Path

from tests import port_native


class TextSpeed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "text_speed_support.c"
        # An invented host speed and a stand-in reveal that keeps the budget
        # it was given.
        support.write_text('''#include "common.h"
#include "resident/window.h"
Window window_single_line_window;
Window test_window;
u32 test_speed;
u32 test_seen = 0xFFFFFFFF;
u32 xem_host_text_speed(void) {
    return test_speed;
}
void xem_original_window_reveal_text(Window *window) {
    test_seen = window->unk69;
}
u32 test_step(s32 single, u32 speed, u32 budget, s32 slow) {
    Window *window = single ? &window_single_line_window : &test_window;
    test_speed = speed;
    window->unk69 = budget;
    window->unk88 = slow;
    window_reveal_text(window);
    return test_seen;
}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/text_speed.c", str(support)])

    def step(self, speed, budget, slow=0, single=False):
        return self.lib.test_step(int(single), speed, budget, slow)

    def test_the_original_reveals_its_own_budget(self):
        for budget in (1, 2, 255):
            self.assertEqual(self.step(0, budget), budget)

    def test_a_speed_raises_a_smaller_budget_and_never_lowers_one(self):
        self.assertEqual(self.step(4, 1), 4)
        self.assertEqual(self.step(4, 2), 4)
        self.assertEqual(self.step(255, 1), 255)
        self.assertEqual(self.step(4, 255), 255, "authored instant text stays instant")

    def test_slow_text_and_the_one_line_layout_window_keep_their_own(self):
        self.assertEqual(self.step(255, 1, slow=3), 1, "0F 0E's glyph every few frames")
        self.assertEqual(self.step(4, 1, slow=3), 1, "with Fast text too")
        self.assertEqual(self.step(255, 100, single=True), 100, "window_render_text_line's budget")


if __name__ == "__main__":
    unittest.main()
