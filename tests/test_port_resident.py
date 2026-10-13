"""The port's small resident routines (src/port/resident.c) against the contracts
of src/decomp/src/resident/mode_set_arena_task.s and console_report_printf.s,
natively (tests/port_native.py)."""

import ctypes
import struct
import tempfile
import unittest
from pathlib import Path

from tests import port_native as mem
from tests.test_sprite_vm import function_body

# Test addresses in the mapped RAM for the globals the routines use.
SYMBOLS = {"mode_arena_task": 0x80059000, "console_current": 0x80059004}

LIB = None
BUILD = None


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["src/port/resident.c", "tests/port_resident/support.c"], SYMBOLS)
    LIB.console_report_printf.argtypes = [ctypes.c_char_p]


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def counter(name):
    return ctypes.c_int.in_dll(LIB, name).value


class Resident(unittest.TestCase):
    def setUp(self):
        mem.clear()

    def test_set_arena_task_stores_the_word(self):
        mem.put32(SYMBOLS["mode_arena_task"] + 4, 0x11111111)
        LIB.mode_set_arena_task(-3)
        self.assertEqual(mem.u32(SYMBOLS["mode_arena_task"]), 0xFFFFFFFD)
        self.assertEqual(mem.u32(SYMBOLS["mode_arena_task"] + 4), 0x11111111)

    def test_report_printf_is_console_printf(self):
        # With a console: console_vprintf(0, format, the arguments).
        format = ctypes.c_char_p(b"ANG %x %d\n")
        mem.put32(SYMBOLS["console_current"], 0x80100000)
        calls = counter("test_vprintf_calls")
        LIB.console_report_printf(format, ctypes.c_int(0x123), ctypes.c_int(-7))
        self.assertEqual(counter("test_vprintf_calls"), calls + 1)
        self.assertEqual(counter("test_vprintf_target"), 0)
        self.assertEqual(ctypes.c_char_p.in_dll(LIB, "test_vprintf_format").value, b"ANG %x %d\n")
        args = (ctypes.c_int * 2).in_dll(LIB, "test_vprintf_args")
        self.assertEqual(list(args), [0x123, -7])
        # Without one, nothing.
        mem.put32(SYMBOLS["console_current"], 0)
        LIB.console_report_printf(format, ctypes.c_int(1), ctypes.c_int(2))
        self.assertEqual(counter("test_vprintf_calls"), calls + 1)


class SpriteFacingRequests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        source = (mem.ROOT / "tests/port_resident/sprite_facing.c").read_text()
        # Compile the actual port wrapper and the original request/control
        # functions. No original assets or modelled substitute are involved.
        for path, name, signature in (
            ("src/decomp/src/resident/sprite_construction.c", "sprite_clear_group_entries",
             "void sprite_clear_group_entries(Sprite *sprite)"),
            ("src/decomp/src/resident/sprite.c", "sprite_apply_cell_frame_controls",
             "void sprite_apply_cell_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source)"),
            ("src/decomp/src/resident/sprite.c", "sprite_apply_frame_controls",
             "void sprite_apply_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source)"),
            ("src/decomp/src/resident/sprite.c", "sprite_request_frame",
             "void xem_original_sprite_request_frame(Sprite *sprite, s32 frame)"),
            ("src/port/sprite_facing.c", "sprite_request_frame",
             "void sprite_request_frame(Sprite *sprite, s32 frame)"),
        ):
            source += "\n" + signature + " {" + function_body(path, name) + "\n"
        fixture = Path(cls.build.name) / "sprite_facing.c"
        fixture.write_text(source)
        cls.lib = mem.build(cls.build.name, [str(fixture)])
        cls.lib.test_request_setup.argtypes = [ctypes.c_void_p, ctypes.c_int]
        # A low mapping keeps the recovered source-directory arithmetic
        # positive when its pointer is cast to s32 in this native process.
        # MAP_FIXED_NOREPLACE follows port_native's game-memory mappings.
        cls.frames = mem._LIBC.mmap(ctypes.c_void_p(0x10000000), 4096, 3, 0x02 | 0x20 | 0x100000, -1, 0)
        if cls.frames != 0x10000000:
            raise RuntimeError("cannot map the invented 32-bit sprite directory")
        cls.addClassCleanup(lambda: mem._LIBC.munmap(ctypes.c_void_p(cls.frames), 4096))

    def test_replayed_requests_clear_and_inherit_groups_without_touching_the_live_pending_list(self):
        for cell_directory in (False, True):
            for clear in (False, True):
                with self.subTest(cell_directory=cell_directory, clear=clear):
                    data = bytearray(96)
                    struct.pack_into("<4H", data, 0, 3 | (0x8000 if cell_directory else 0), 16, 40, 64)
                    for offset, group, x, y, depth in ((16, 0, 9, -3, 6), (40, 1, -7, 8, 9)):
                        data[offset] = 1  # one part, preceded by group controls
                        start = offset + (6 if cell_directory else 10)
                        data[start:start + 7] = bytes((0xF0 | group, x & 255, y & 255, depth, 0, 0, 0))
                    ctypes.memmove(self.frames, bytes(data), len(data))
                    self.lib.test_request_setup(self.frames, clear)
                    self.lib.test_request(1)
                    self.assertTrue(self.lib.test_request_preserved_live_state())
                    self.assertEqual(self.lib.test_request_group(0, 0), 0 if clear else 41)
                    self.lib.test_request(2)
                    self.assertEqual([self.lib.test_request_group(0, i) for i in range(3)], [9, -3, 96])
                    self.lib.test_request(3)
                    self.assertEqual([self.lib.test_request_group(0, i) for i in range(3)], [9, -3, 96])
                    self.assertEqual([self.lib.test_request_group(1, i) for i in range(3)], [-7, 8, 144])
                    self.assertEqual(self.lib.test_request_frame(), 3)
                    self.assertTrue(self.lib.test_request_private_pending())
                    self.assertTrue(self.lib.test_request_preserved_live_state())


if __name__ == "__main__":
    unittest.main()
