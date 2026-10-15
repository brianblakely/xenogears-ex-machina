"""Where the host lets movies be skipped, they take the original's early exit."""
import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native


class MovieSkip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "movie_skip_support.c"
        # An invented host choice, and stand-ins for the original functions
        # that keep what they saw.
        support.write_text('''#include "common.h"
s32 field_movie_mode;
s32 field_movie_fade_bits;
s32 movie_mode_unskippable;
u32 test_skip;
s32 test_fade_seen = -1;
s32 test_frames_seen;
s32 test_keep_seen = -1;
u32 xem_host_movie_skip(void) {
    return test_skip;
}
void xem_original_field_movie_run_frames(s32 frames) {
    test_fade_seen = field_movie_fade_bits;
    test_frames_seen = frames;
}
void xem_original_movie_mode_read_playback_input(void) {
    test_keep_seen = movie_mode_unskippable;
}
void field_movie_run_frames(s32 frames);
void movie_mode_read_playback_input(void);
s32 test_field(u32 skip, s32 mode, s32 fade) {
    test_skip = skip;
    field_movie_mode = mode;
    field_movie_fade_bits = fade;
    field_movie_run_frames(3);
    return test_fade_seen;
}
s32 test_mode6(u32 skip, s32 keep) {
    test_skip = skip;
    movie_mode_unskippable = keep;
    movie_mode_read_playback_input();
    return test_keep_seen;
}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/movie_skip.c", str(support)])

    def test_a_field_movie_gains_the_early_exit_only_when_armed(self):
        self.assertEqual(self.lib.test_field(0, 0, 0x40), 0x40, "the original's request")
        self.assertEqual(self.lib.test_field(1, 0, 0x40), 0xC0)
        self.assertEqual(self.lib.test_field(1, 1, 0), 0x80)
        self.assertEqual(self.lib.test_field(1, 0, 0x80), 0x80, "a skippable movie stays so")
        frames = ctypes.c_int.in_dll(self.lib, "test_frames_seen").value
        self.assertEqual(frames, 3, "the frames still run")

    def test_window_movies_stay_the_scripts(self):
        self.assertEqual(self.lib.test_field(1, 2, 0), 0)

    def test_the_movie_modes_keep_flag_clears_only_when_armed(self):
        self.assertEqual(self.lib.test_mode6(0, 1), 1, "the original's kept movie")
        self.assertEqual(self.lib.test_mode6(1, 1), 0)
        self.assertEqual(self.lib.test_mode6(1, 0), 0)


if __name__ == "__main__":
    unittest.main()
