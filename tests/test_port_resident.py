"""The port's small resident routines (port/resident.c) against the contracts
of decomp/src/resident/mode_set_arena_task.s and console_report_printf.s,
natively (tests/port_native.py)."""

import ctypes
import tempfile
import unittest

from tests import port_native as mem

# Test addresses in the mapped RAM for the globals the routines use.
SYMBOLS = {"mode_arena_task": 0x80059000, "console_current": 0x80059004}

LIB = None
BUILD = None


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/resident.c", "tests/port_resident/support.c"], SYMBOLS)
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


if __name__ == "__main__":
    unittest.main()
