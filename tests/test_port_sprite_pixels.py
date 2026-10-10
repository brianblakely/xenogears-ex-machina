"""The port's GTE pixel loops (port/sprite_pixels.c) against the contracts of
decomp/src/resident/sprite_darken_pixels.s and sprite_blend_pixels.s,
natively (tests/port_native.py), on invented pixels.

Expected pixels come from the contracts' arithmetic and psx-spx's GPF
formula (MAC = IR0 * IR >> 12 with sf=1, IR = MAC saturated to 16 bits,
colour FIFO = MAC >> 4 saturated to 0..255 under RGBC's code byte), worked
in Python here, not from the C.
"""

import ctypes
import random
import tempfile
import unittest

from tests import port_native as mem

LIB = None
BUILD = None

OUT = 0x80100000
SOURCE = 0x80110000
TARGET = 0x80120000

IR0, IR1, IR2, IR3 = 8, 9, 10, 11
RGB2, RGBC = 22, 6
MAC1, MAC2, MAC3 = 25, 26, 27


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/sprite_pixels.c"])
    LIB.sprite_darken_pixels.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
    LIB.sprite_blend_pixels.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def gpf(ir0, values):
    """(IR1-3, MAC1-3, colour FIFO bytes) of GPF sf=1 on IR1-3 = `values`."""
    macs = [(ir0 * s16(v)) >> 12 for v in values]
    irs = [max(-0x8000, min(0x7FFF, m)) for m in macs]
    colours = [max(0, min(0xFF, m >> 4)) for m in macs]
    return irs, macs, colours


def level_ir0(level, clamp_at, clamp_to):
    if level >= clamp_at:
        level = clamp_to
    return s16(level << 7)


def darken_expected(pixels, level):
    ir0 = level_ir0(level, 32, 32)
    out, last = [], None
    for p in pixels:
        last = gpf(ir0, [p & 0x1F, p & 0x3E0, p & 0x7C00])
        irs = last[0]
        colour = (irs[0] & 0x1F) | (irs[1] & 0x3E0) | (irs[2] & 0x7C00)
        if p != 0 and colour == 0:
            colour = 1
        out.append((p & 0x8000) | colour)
    return out, ir0, last


def blend_expected(bases, targets, level):
    ir0 = level_ir0(level, 33, 32)
    out, last = [], None
    for b, t in zip(bases, targets):
        fields = [b & 0x1F, b & 0x3E0, b & 0x7C00]
        diffs = [(t & m) - f for m, f in zip((0x1F, 0x3E0, 0x7C00), fields)]
        last = gpf(ir0, diffs)
        irs = last[0]
        value = ((fields[0] + (irs[0] & 0x1F)) | (fields[1] + (irs[1] & 0x3E0)) | (fields[2] + (irs[2] & 0x7C00)))
        out.append(value & 0xFFFF)
    return out, ir0, last


def halves(address, count):
    return [mem.u16(address + 2 * i) for i in range(count)]


class PixelLoops(unittest.TestCase):
    def setUp(self):
        mem.clear()
        LIB.xem_gte_reset()

    def check_gte(self, ir0, last, code=0):
        irs, macs, colours = last
        self.assertEqual(LIB.xem_gte_read_data(IR0), ir0 & 0xFFFFFFFF)
        self.assertEqual([LIB.xem_gte_read_data(r) for r in (IR1, IR2, IR3)], [v & 0xFFFFFFFF for v in irs])
        self.assertEqual([LIB.xem_gte_read_data(r) for r in (MAC1, MAC2, MAC3)], [v & 0xFFFFFFFF for v in macs])
        self.assertEqual(LIB.xem_gte_read_data(RGB2), code << 24 | colours[2] << 16 | colours[1] << 8 | colours[0])

    def darken(self, pixels, level):
        mem.put16(SOURCE, *pixels, 0x5A5A)
        mem.put16(OUT, *([0xBEEF] * (len(pixels) + 1)))
        LIB.sprite_darken_pixels(len(pixels), level, OUT, SOURCE)
        return halves(OUT, len(pixels) + 1)

    def test_darken_worked_pixels(self):
        # Level 16 halves each field in place: 0x0421's green and blue halves
        # fall below their fields and are masked off, so it would become 0 and
        # is kept opaque as colour 1; bit 15 is kept; a zero pixel stays 0.
        got = self.darken([0x0000, 0x8000, 0x7FFF, 0x0421, 0x8C63, 0x7C1F], 16)
        self.assertEqual(got, [0x0000, 0x8001, 0x3DEF, 0x0001, 0x8421, 0x3C0F, 0xBEEF])

    def test_darken_levels(self):
        rng = random.Random(1)
        pixels = [rng.randrange(0x10000) for _ in range(64)] + [0, 0x8000, 0x0001]
        for level in (0, 1, 7, 16, 31, 32, 33, 1000, -1, -5, 0x200):
            with self.subTest(level=level):
                LIB.xem_gte_write_data(RGBC, 0x2A000000)
                got = self.darken(pixels, level)
                want, ir0, last = darken_expected(pixels, level)
                self.assertEqual(got, want + [0xBEEF])
                self.check_gte(ir0, last, code=0x2A)
                if level >= 32:
                    # The colours are copied; a nonzero pixel without colour
                    # (bit 15 alone) still gets colour 1.
                    copies = [p | 1 if p == 0x8000 else p for p in pixels]
                    self.assertEqual(got[:-1], copies)

    def test_darken_count_zero_writes_nothing(self):
        self.assertEqual(self.darken([], 8), [0xBEEF])

    def blend(self, bases, targets, level):
        mem.put16(SOURCE, *bases, 0x1111)
        mem.put16(TARGET, *targets, 0x2222)
        mem.put16(OUT, *([0xBEEF] * (len(bases) + 1)))
        LIB.sprite_blend_pixels(len(bases), level, OUT, SOURCE, TARGET)
        return halves(OUT, len(bases) + 1)

    def test_blend_worked_pixels(self):
        # Half way: red 0 -> 31 gives 15; red 31 -> 0 gives 31 + (-16 & 0x1F)
        # = 47, carrying into green's bit 5; bit 15 of either input is dropped.
        got = self.blend([0x0000, 0x001F, 0x8000, 0x7FFF], [0x001F, 0x0000, 0x8000, 0x7FFF], 16)
        self.assertEqual(got, [0x000F, 0x002F, 0x0000, 0x7FFF, 0xBEEF])

    def test_blend_levels(self):
        rng = random.Random(2)
        bases = [rng.randrange(0x10000) for _ in range(64)]
        targets = [rng.randrange(0x10000) for _ in range(64)]
        for level in (0, 1, 8, 16, 31, 32, 33, 64, -1, -9):
            with self.subTest(level=level):
                got = self.blend(bases, targets, level)
                want, ir0, last = blend_expected(bases, targets, level)
                self.assertEqual(got, want + [0xBEEF])
                self.check_gte(ir0, last)

    def test_blend_level_zero_keeps_base_colour(self):
        bases = [0x7FFF, 0x8421, 0x1234]
        self.assertEqual(self.blend(bases, [0, 0x7FFF, 0x4321], 0), [0x7FFF, 0x0421, 0x1234, 0xBEEF])

    def test_blend_count_zero_writes_nothing(self):
        self.assertEqual(self.blend([], [], 8), [0xBEEF])


if __name__ == "__main__":
    unittest.main()
