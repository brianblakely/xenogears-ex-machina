"""Focus requests must reach only the original game's legal pause checks."""
import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native
from tools.game_module import focus_pause_checks


class PauseChecks(unittest.TestCase):
    def test_pause_checks_keep_original_locks_and_leave_movies_alone(self):
        lines = [
            "define void @field_main() {",
            "  %start = and i32 %buttons, 2048",
            "  %locked = icmp ne i8 %pause_lock, 0",
            "  br i1 %locked, label %play, label %pause",
            "  %resume = and i32 %resume_buttons, 2048",
            "}",
            "define void @movie_play() {",
            "  %skip = and i32 %buttons, 2048",
            "}",
        ]
        rewritten, helpers = focus_pause_checks(lines)
        self.assertEqual(helpers, {"xem_field_pause_start"})
        self.assertEqual(rewritten[1], "  %start = call i32 @xem_field_pause_start(i32 %buttons)")
        self.assertEqual(rewritten[4], "  %resume = call i32 @xem_field_pause_start(i32 %resume_buttons)")
        self.assertEqual(rewritten[2:4], lines[2:4], "the original legality branches must stay intact")
        self.assertEqual(rewritten[6:], lines[6:], "Start in a movie skips it; never inject a pause there")

    def test_changed_pause_sites_fail_the_build(self):
        with self.assertRaisesRegex(ValueError, "field_main: expected 2 pause Start checks, found 1"):
            focus_pause_checks(["define void @field_main() {", "  %start = and i32 %buttons, 2048", "}"])


class PauseEligibility(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "pause_support.c"
        support.write_text(f'''#include "{port_native.ROOT}/src/decomp/src/field/field.h"
FieldWork field_work;
u16 field_pad_port0_allowed_mask;
s32 field_camera_cut_timer;
u8 battle_turns_active;
u8 arena_retreat_rule_enabled;
u16 sound_driver_flags;
u8 arena_menu_paused;
u32 test_requested, test_reached, test_silenced;
s32 test_arena_mode;
u32 xem_host_pause_requested(void) {{ return test_requested; }}
void xem_host_pause_reached(void) {{ test_requested = 0; test_reached++; }}
void xem_original_sound_silence_voices(void) {{ test_silenced++; }}
void xem_original_arena_menu_open_pause(s32 mode) {{ test_arena_mode = mode; }}
void test_field_masks(u16 input, u16 allowed, s32 cut) {{
    field_work.input_mask = input;
    field_pad_port0_allowed_mask = allowed;
    field_camera_cut_timer = cut;
}}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/pause.c", str(support)])
        for name in ("xem_pause_start", "xem_field_pause_start", "xem_battle_pause_start", "xem_arena_pause_start"):
            function = getattr(cls.lib, name)
            function.argtypes = [ctypes.c_uint]
            function.restype = ctypes.c_uint

    def setUp(self):
        ctypes.c_ushort.in_dll(self.lib, "sound_driver_flags").value = 0
        ctypes.c_ubyte.in_dll(self.lib, "arena_menu_paused").value = 0
        for name in ("test_requested", "test_reached", "test_silenced"):
            ctypes.c_uint.in_dll(self.lib, name).value = 0
        ctypes.c_ubyte.in_dll(self.lib, "battle_turns_active").value = 1
        ctypes.c_ubyte.in_dll(self.lib, "arena_retreat_rule_enabled").value = 0
        self.lib.test_field_masks(0xFFFF, 0xFFFF, 0)

    def test_pause_query_reads_original_flags_and_guards_overlay_memory(self):
        self.assertFalse(self.lib.xem_game_paused(1))
        ctypes.c_ubyte.in_dll(self.lib, "arena_menu_paused").value = 1
        self.assertFalse(self.lib.xem_game_paused(1))
        self.assertTrue(self.lib.xem_game_paused(4))
        ctypes.c_ubyte.in_dll(self.lib, "arena_menu_paused").value = 0
        ctypes.c_ushort.in_dll(self.lib, "sound_driver_flags").value = 0x40
        for mode in (1, 2, 3, 4):
            self.assertTrue(self.lib.xem_game_paused(mode))

    def test_requests_respect_original_input_locks_and_preserve_player_start(self):
        helpers = (self.lib.xem_pause_start, self.lib.xem_field_pause_start,
                   self.lib.xem_battle_pause_start, self.lib.xem_arena_pause_start)
        for helper in helpers:
            self.assertEqual(helper(0x40), 0)
            self.assertEqual(helper(0x840), 0x800, "real Start must remain unchanged")
        ctypes.c_uint.in_dll(self.lib, "test_requested").value = 1
        for helper in helpers:
            self.assertEqual(helper(0x40), 0x800)
        for masks in ((0xF7FF, 0xFFFF, 0), (0xFFFF, 0xF7FF, 0), (0xFFFF, 0xFFFF, 1)):
            self.lib.test_field_masks(*masks)
            self.assertEqual(self.lib.xem_field_pause_start(0), 0, "do not interrupt camera cuts or script input locks")
        ctypes.c_ubyte.in_dll(self.lib, "battle_turns_active").value = 0
        self.assertEqual(self.lib.xem_battle_pause_start(0), 0, "do not send battle input code 0xE before pause is legal")
        ctypes.c_ubyte.in_dll(self.lib, "arena_retreat_rule_enabled").value = 1
        self.assertEqual(self.lib.xem_arena_pause_start(0), 0, "do not change the arena menu's driving pad before pause is legal")
        for helper in helpers:
            self.assertEqual(helper(0x800), 0x800, "focus guards never swallow player Start")

    def test_only_reaching_a_real_pause_consumes_the_request(self):
        requested = ctypes.c_uint.in_dll(self.lib, "test_requested")
        reached = ctypes.c_uint.in_dll(self.lib, "test_reached")
        requested.value = 1
        self.lib.arena_menu_open_pause(0)
        self.assertEqual((requested.value, reached.value), (1, 0), "closing an arena menu is not a pause")
        self.lib.arena_menu_open_pause(1)
        self.assertEqual((requested.value, reached.value), (0, 1))
        self.assertEqual(ctypes.c_int.in_dll(self.lib, "test_arena_mode").value, 1)
        requested.value = 1
        self.lib.sound_silence_voices()
        self.assertEqual((requested.value, reached.value), (0, 2))
        self.assertEqual(ctypes.c_uint.in_dll(self.lib, "test_silenced").value, 1, "the original audio pause still runs")


if __name__ == "__main__":
    unittest.main()
