"""The port's software GTE and libgte (port/gte.c, port/gte_asm.c, port/libgte.c).

The C is built for the host as a freestanding shared library (no libc) and
driven through ctypes. Expected values are worked by hand from psx-spx's GTE
formulas and the original libgte instruction sequences; the invented tables
below describe no original content. Run inside `nix develop path:./nix/runtime`
(it provides $XEM_CLANG and ld.lld). The original-table checks read the
user's resident executable when present (XEM_RESIDENT_IMAGE, default
.local/extract/disc1/SLUS_006.64) and skip otherwise.
"""

import ctypes
import json
import math
import os
import random
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

from tools.analysis.sprite_matrix import rotation, scale_matrix

ROOT = Path(__file__).resolve().parents[1]
CLANG = os.environ.get("XEM_CLANG") or shutil.which("clang")
LIB = None
BUILD = None
RESIDENT_IMAGE = Path(
    os.environ.get("XEM_RESIDENT_IMAGE", ROOT / ".local/extract/disc1/SLUS_006.64")
)

# Data registers.
VXY0, VZ0, VXY1, VZ1, VXY2, VZ2, RGBC, OTZ = range(8)
IR0, IR1, IR2, IR3, SXY0, SXY1, SXY2, SXYP = range(8, 16)
SZ0, SZ1, SZ2, SZ3, RGB0, RGB1, RGB2, RES1 = range(16, 24)
MAC0, MAC1, MAC2, MAC3, IRGB, ORGB, LZCS, LZCR = range(24, 32)
# Control registers.
RT, TRX, TRY, TRZ, LLM = 0, 5, 6, 7, 8
RBK, GBK, BBK, LCM = 13, 14, 15, 16
RFC, GFC, BFC = 21, 22, 23
OFX, OFY, H, DQA, DQB, ZSF3, ZSF4, FLAG = range(24, 32)


def u32(value):
    return value & 0xFFFFFFFF


def s32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def pair(low, high):
    return (low & 0xFFFF) | (high & 0xFFFF) << 16


def unr_reference(h, sz3):
    """psx-spx's RTPS/RTPT division pseudo code, transcribed."""
    if h >= sz3 * 2:
        return 0x1FFFF, True
    z = 16 - sz3.bit_length()
    n = h << z
    d = sz3 << z
    u = max(0, (0x40000 // (((d - 0x7FC0) >> 7) + 0x100) + 1) // 2 - 0x101) + 0x101
    d = (0x2000080 - d * u) >> 8
    d = (0x80 + d * u) >> 8
    return min(0x1FFFF, (n * d + 0x8000) >> 16), False


def build_library(out):
    common = [
        "-O2",
        "-fPIC",
        "-ffreestanding",
        "-fno-builtin",
        "-nostdinc",
        "-std=gnu89",
        "-funsigned-char",
        "-fwrapv",
        "-fno-strict-aliasing",
        "-Wall",
        "-Werror",
        "-Iport/include",
        "-Idecomp/include",
    ]
    objects = []
    for source, extra in (
        ("port/gte.c", []),
        ("port/gte_asm.c", []),
        # libgte's long is the PS1's 32-bit long.
        ("port/libgte.c", ["-Dlong=int"]),
        ("tests/port_gte/support.c", []),
    ):
        obj = os.path.join(out, os.path.basename(source) + ".o")
        subprocess.run([CLANG, *common, *extra, "-c", source, "-o", obj], cwd=ROOT, check=True)
        objects.append(obj)
    library = os.path.join(out, "libxemgte.so")
    subprocess.run(
        [CLANG, "-shared", "-nostdlib", "-fuse-ld=lld", *objects, "-o", library],
        cwd=ROOT,
        check=True,
    )
    return ctypes.CDLL(library)


def setUpModule():
    global LIB, BUILD
    if not CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = build_library(BUILD.name)
    LIB.xem_gte_write_data.argtypes = [ctypes.c_int, ctypes.c_uint]
    LIB.xem_gte_write_control.argtypes = [ctypes.c_int, ctypes.c_uint]
    LIB.xem_gte_read_data.restype = ctypes.c_uint
    LIB.xem_gte_read_control.restype = ctypes.c_uint
    LIB.xem_gte_execute.argtypes = [ctypes.c_uint]
    LIB.xem_gte_state.restype = ctypes.c_void_p
    LIB.xem_gte_state_size.restype = ctypes.c_uint


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


class Gte:
    """Register access in the order a test writes it."""

    def reset(self):
        LIB.xem_gte_reset()

    def d(self, reg, value=None):
        if value is not None:
            LIB.xem_gte_write_data(reg, u32(value))
        return LIB.xem_gte_read_data(reg)

    def c(self, reg, value=None):
        if value is not None:
            LIB.xem_gte_write_control(reg, u32(value))
        return LIB.xem_gte_read_control(reg)

    def run(self, command):
        LIB.xem_gte_execute(command)
        return self.c(FLAG)

    def matrix(self, base, rows):
        e = [v for row in rows for v in row]
        for i in range(4):
            self.c(base + i, pair(e[i * 2], e[i * 2 + 1]))
        self.c(base + 4, e[8])

    def vector(self, base, values):
        for i, v in enumerate(values):
            self.c(base + i, v)

    def v(self, n, x, y, z):
        self.d(VXY0 + n * 2, pair(x, y))
        self.d(VZ0 + n * 2, z)

    def ir(self, x, y, z):
        self.d(IR1, x)
        self.d(IR2, y)
        self.d(IR3, z)

    def mac(self):
        return [s32(self.d(MAC1 + i)) for i in range(3)]

    def irs(self):
        return [s32(self.d(IR1 + i)) for i in range(3)]


IDENTITY = ((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000))


class GteTest(unittest.TestCase):
    def setUp(self):
        self.g = Gte()
        self.g.reset()


class RegisterTests(GteTest):
    def test_state_is_the_64_register_words(self):
        self.assertEqual(LIB.xem_gte_state_size(), 256)
        self.g.d(MAC2, 0x12345678)
        self.g.c(DQB, 0x9ABCDEF0)
        words = struct.unpack("<64I", ctypes.string_at(LIB.xem_gte_state(), 256))
        self.assertEqual(words[MAC2], 0x12345678)
        self.assertEqual(words[32 + DQB], 0x9ABCDEF0)
        self.assertEqual(words[LZCR], 32)

    def test_16_bit_registers_read_back_sign_or_zero_extended(self):
        g = self.g
        for reg in (VZ0, VZ1, VZ2, IR0, IR1, IR2, IR3):
            self.assertEqual(g.d(reg, 0x12348900), 0xFFFF8900, reg)
            self.assertEqual(g.d(reg, 0x56781234), 0x1234, reg)
        for reg in (OTZ, SZ0, SZ1, SZ2, SZ3):
            self.assertEqual(g.d(reg, 0x12348900), 0x8900, reg)
        for reg in (VXY0, RGBC, SXY0, SXY1, RGB0, RGB1, RGB2, RES1, MAC0, MAC1, MAC3):
            self.assertEqual(g.d(reg, 0x89ABCDEF), 0x89ABCDEF, reg)
        for reg in (RT + 4, LLM + 4, LCM + 4, H, DQA, ZSF3, ZSF4):
            self.assertEqual(g.c(reg, 0x00018000), 0xFFFF8000, reg)
        for reg in (RT, TRX, RBK, RFC, OFX, DQB):
            self.assertEqual(g.c(reg, 0x89ABCDEF), 0x89ABCDEF, reg)

    def test_sxyp_pushes_the_screen_fifo_and_mirrors_sxy2(self):
        g = self.g
        g.d(SXY0, 1)
        g.d(SXY1, 2)
        g.d(SXY2, 3)
        self.assertEqual(g.d(SXYP), 3)
        g.d(SXYP, 4)
        self.assertEqual([g.d(r) for r in (SXY0, SXY1, SXY2, SXYP)], [2, 3, 4, 4])
        g.d(SXY2, 7)  # a direct write changes only SXY2
        self.assertEqual([g.d(r) for r in (SXY0, SXY1, SXY2, SXYP)], [2, 3, 7, 7])

    def test_irgb_expands_and_orgb_collects_ir(self):
        g = self.g
        g.d(IRGB, 0xFFFF1234)
        self.assertEqual(g.irs(), [0x14 << 7, 0x11 << 7, 0x04 << 7])
        self.assertEqual(g.d(IRGB), 0x1234)
        self.assertEqual(g.d(ORGB), 0x1234)
        g.ir(-5, 0x7FFF, 0x100)  # saturated per component, no flags
        self.assertEqual(g.d(ORGB), 0x1F << 5 | 2 << 10)
        self.assertEqual(g.d(IRGB), 0x1F << 5 | 2 << 10)
        self.assertEqual(g.c(FLAG), 0)
        g.d(ORGB, 0)  # read only
        self.assertEqual(g.d(ORGB), 0x1F << 5 | 2 << 10)

    def test_lzcr_counts_leading_sign_bits(self):
        g = self.g
        for value, count in (
            (0, 32),
            (1, 31),
            (0x10000, 15),
            (0x7FFFFFFF, 1),
            (-1, 32),
            (0x80000000, 1),
            (0xFFFF0000, 16),
            (0xFFFFFFFE, 31),
        ):
            g.d(LZCS, value)
            self.assertEqual(g.d(LZCR), count, hex(value))
            self.assertEqual(g.d(LZCS), u32(value))
        g.d(LZCR, 5)  # read only
        self.assertEqual(g.d(LZCR), 31)

    def test_flag_keeps_bits_30_to_12_and_sums_the_error_bits(self):
        g = self.g
        self.assertEqual(g.c(FLAG, 0xFFFFFFFF), 0xFFFFF000)
        self.assertEqual(g.c(FLAG, 0x00001000), 0x00001000)  # IR0: not an error bit
        self.assertEqual(g.c(FLAG, 0x00080000), 0x00080000)  # colour B: not an error bit
        self.assertEqual(g.c(FLAG, 0x00800000), 0x80800000)  # IR2
        self.assertEqual(g.c(FLAG, 0x00002000), 0x80002000)  # SY2
        g.run(0x3F7)  # an unassigned opcode only clears FLAG
        self.assertEqual(g.c(FLAG), 0)


class PerspectiveTests(GteTest):
    def setUp(self):
        super().setUp()
        g = self.g
        g.matrix(RT, IDENTITY)
        g.c(H, 1000)
        g.c(DQA, -0x1062)
        g.c(DQB, 0x1400000)
        g.c(OFX, 160 << 16)
        g.c(OFY, 120 << 16)

    def test_rtps_projects_one_vertex(self):
        g = self.g
        g.v(0, 100, 50, 1000)
        flag = g.run(0x4A180001)
        self.assertEqual(g.mac(), [100, 50, 1000])
        self.assertEqual(g.irs(), [100, 50, 1000])
        self.assertEqual([g.d(r) for r in (SZ0, SZ1, SZ2, SZ3)], [0, 0, 0, 1000])
        # H / SZ3 = 1.0 (10000h): SX = 100 + 160, SY = 50 + 120.
        self.assertEqual(g.d(SXY2), pair(260, 170))
        self.assertEqual(g.d(SXYP), pair(260, 170))
        # MAC0 = 10000h * -1062h + 1400000h, negative: IR0 saturates to 0.
        self.assertEqual(s32(g.d(MAC0)), 0x10000 * -0x1062 + 0x1400000)
        self.assertEqual(g.d(IR0), 0)
        self.assertEqual(flag, 0x1000)

    def test_rtps_division_and_screen_overflows(self):
        g = self.g
        g.c(OFX, 0)
        g.c(OFY, 0)
        g.c(DQA, 0)
        g.c(DQB, 0)
        g.v(0, 0x7FFF, -0x8000, 1)
        flag = g.run(0x4A180001)
        # SZ3 = 1 <= H / 2: n = 1FFFFh (bit 17). 1FFFFh * 7FFFh overflows MAC0
        # positively (16), SX saturates to 3FFh (14); 1FFFFh * -8000h
        # negatively (15), SY to -400h (13).
        self.assertEqual(g.d(SXY2), pair(0x3FF, -0x400))
        self.assertEqual(flag, 0x80000000 | 0x20000 | 0x10000 | 0x8000 | 0x4000 | 0x2000)
        self.assertEqual(g.d(MAC0), 0)

    def test_rtps_sf0_tests_ir3_flag_on_mac3_shifted(self):
        g = self.g
        g.c(H, 0)
        g.v(0, 0, 0, 0x7FFF)
        flag = g.run(0x00000001)  # sf = 0: MAC3 = 7FFF000h, IR3 clamps without a flag
        self.assertEqual(g.mac(), [0, 0, 0x7FFF000])
        self.assertEqual(g.d(IR3), 0x7FFF)
        self.assertEqual(g.d(SZ3), 0x7FFF)
        self.assertEqual(flag, 0x1000)  # only IR0 (DQB / 1000h > 1000h)
        g.c(TRZ, 0x10)  # MAC3 >> 12 = 800Fh: now the flag
        flag = g.run(0x00000001)
        self.assertEqual(g.d(IR3), 0x7FFF)
        self.assertEqual(g.d(SZ3), 0x800F)
        self.assertEqual(flag & 0x80400000, 0x00400000)  # IR3 is outside the error sum
        g.c(TRZ, -0x20)  # negative: SZ3 saturates to 0 (bit 18), IR3 by lm
        g.v(0, 0, 0, 0x10)
        flag = g.run(0x00000401)
        self.assertEqual(g.d(IR3), 0)
        self.assertEqual(g.d(SZ3), 0)
        self.assertEqual(flag & 0x80440000, 0x80040000)

    def test_rtpt_projects_three_and_depth_cues_the_last(self):
        g = self.g
        g.c(OFX, 0)
        g.c(OFY, 0)
        g.c(DQA, -1)
        g.c(DQB, 0x8000)
        g.v(0, 200, 0, 1000)
        g.v(1, 200, 0, 2000)
        g.v(2, 200, 0, 4000)
        flag = g.run(0x4A280030)
        n = [unr_reference(1000, z)[0] for z in (1000, 2000, 4000)]
        self.assertEqual(n[0], 0x10000)
        self.assertEqual([g.d(r) for r in (SZ0, SZ1, SZ2, SZ3)], [0, 1000, 2000, 4000])
        self.assertEqual(
            [g.d(r) for r in (SXY0, SXY1, SXY2)], [pair((k * 200) >> 16, 0) for k in n]
        )
        # IR0 from the last vertex only: (-n + 8000h) >> 12; the first's
        # would be negative and flag IR0.
        self.assertEqual(g.d(IR0), (0x8000 - n[2]) >> 12)
        self.assertEqual(s32(g.d(MAC0)), 0x8000 - n[2])
        self.assertEqual(flag, 0)


class DivisionTests(GteTest):
    def divide(self, h, sz3):
        g = self.g
        g.c(H, h)
        g.c(TRZ, sz3)
        g.c(DQA, 1)
        flag = g.run(0x4A180001)
        return g.d(MAC0), flag & 0x20000

    def setUp(self):
        super().setUp()
        self.g.matrix(RT, IDENTITY)

    def test_hand_worked_quotients(self):
        # SZ3 = 8000h, H = 8000h: z = 0, u = 200h, d = 10000h then 20000h.
        self.assertEqual(self.divide(0x8000, 0x8000), (0x10000, 0))
        self.assertEqual(self.divide(0xFFFF, 0x8000), (0x1FFFE, 0))
        # z = 6: n = d = FA00h, u = 5 + 101h, d = 65572 then 67109.
        self.assertEqual(self.divide(1000, 1000), (0x10000, 0))
        self.assertEqual(self.divide(1, 1), (0x10000, 0))
        self.assertEqual(self.divide(0, 1), (0, 0))
        self.assertEqual(self.divide(2, 1), (0x1FFFF, 0x20000))
        self.assertEqual(self.divide(5, 0), (0x1FFFF, 0x20000))
        self.assertEqual(self.divide(0, 0), (0x1FFFF, 0x20000))

    def test_every_depth_matches_the_documented_unr_steps(self):
        for h in (1, 0x155, 1000, 0x7FFF, 0xFFFF):
            for sz3 in range(1, 0x10000):
                expected, overflow = unr_reference(h, sz3)
                quotient, flag = self.divide(h, sz3)
                self.assertEqual(quotient, expected, (h, sz3))
                self.assertEqual(bool(flag & 0x20000), overflow, (h, sz3))
                if not overflow:
                    self.assertLess(abs(quotient - Fraction(h * 0x10000, sz3)), 3, (h, sz3))


class CommandTests(GteTest):
    def test_nclip(self):
        g = self.g
        g.d(SXY0, pair(0, 0))
        g.d(SXY1, pair(10, 0))
        g.d(SXY2, pair(0, 10))
        self.assertEqual(g.run(0x4B400006), 0)
        self.assertEqual(g.d(MAC0), 100)
        g.d(SXY0, pair(-0x8000, -0x8000))
        g.d(SXY1, pair(0x7FFF, -0x8000))
        g.d(SXY2, pair(-0x8000, 0x7FFF))
        self.assertEqual(g.run(0x4B400006), 0x80010000)  # MAC0 overflow, an error bit
        self.assertEqual(g.d(MAC0), u32(4294836225))

    def test_average_depths(self):
        g = self.g
        g.c(ZSF3, 0x155)
        g.c(ZSF4, 0x100)
        for reg, z in zip((SZ0, SZ1, SZ2, SZ3), (100, 200, 300, 400), strict=True):
            g.d(reg, z)
        g.d(SZ0, 100)
        self.assertEqual(g.run(0x4B58002D), 0)
        self.assertEqual((g.d(MAC0), g.d(OTZ)), (900 * 0x155, (900 * 0x155) >> 12))
        self.assertEqual(g.run(0x4B68002E), 0)
        self.assertEqual((g.d(MAC0), g.d(OTZ)), (1000 * 0x100, 62))
        g.c(ZSF3, -1)
        self.assertEqual(g.run(0x4B58002D), 0x80040000)
        self.assertEqual((s32(g.d(MAC0)), g.d(OTZ)), (-900, 0))
        g.c(ZSF4, 0x7FFF)
        for reg in (SZ0, SZ1, SZ2, SZ3):
            g.d(reg, 0xFFFF)
        self.assertEqual(g.run(0x4B68002E), 0x80050000)
        self.assertEqual((g.d(MAC0), g.d(OTZ)), (0xFFFA0004, 0xFFFF))

    def test_outer_product(self):
        g = self.g
        g.matrix(RT, ((1, 0, 0), (0, 2, 0), (0, 0, 3)))
        g.ir(4, 5, 6)
        self.assertEqual(g.run(0x4B70000C), 0)
        self.assertEqual(g.mac(), [-3, 6, -3])  # D x IR
        self.assertEqual(g.irs(), [-3, 6, -3])
        g.ir(4, 5, 6)
        g.run(0x4B78000C)  # sf = 1: arithmetic shifts
        self.assertEqual(g.mac(), [-1, 0, -1])

    def test_square(self):
        g = self.g
        g.ir(-3, 300, 0x7FFF)
        self.assertEqual(g.run(0x4AA00428), 0x80C00000)
        self.assertEqual(g.mac(), [9, 90000, 0x3FFF0001])
        self.assertEqual(g.irs(), [9, 0x7FFF, 0x7FFF])
        g.ir(-0x40, 0x40, 0)
        self.assertEqual(g.run(0x4AA80428), 0)  # sf = 1: 1000h >> 12
        self.assertEqual(g.mac(), [1, 1, 0])

    def test_mvmva_selectors(self):
        g = self.g
        g.matrix(RT, ((1, 2, 3), (4, 5, 6), (7, 8, 9)))
        g.vector(TRX, (1, 2, 3))
        g.v(1, 10, 20, 30)
        self.assertEqual(g.run(0x00008012), 0)  # sf 0, RT, V1, TR
        self.assertEqual(g.mac(), [4096 + 140, 8192 + 320, 12288 + 500])
        self.assertEqual(g.irs(), [4096 + 140, 8192 + 320, 12288 + 500])
        g.matrix(LLM, ((1, 0, 0), (0, -1, 0), (0, 0, 2)))
        g.vector(RBK, (100, 0, 0))
        g.ir(7, 8, 9)
        self.assertEqual(g.run(0x0003A012), 0x81000000)  # sf 0, LLM, IR, BK
        self.assertEqual(g.mac(), [100 * 4096 + 7, -8, 18])
        self.assertEqual(g.irs(), [0x7FFF, -8, 18])
        # The garbage matrix: -R*10h, R*10h, IR0 / RT13 x3 / RT22 x3.
        g.d(RGBC, 0x00000002)
        g.d(IR0, 5)
        self.assertEqual(g.run(0x0006E012), 0)  # sf 0, mx 3, V1, none
        self.assertEqual(g.mac(), [-320 + 640 + 150, 3 * 60, 5 * 60])

    def test_mvmva_lm_saturates_at_zero(self):
        g = self.g
        g.matrix(RT, ((-0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
        g.v(0, 5, 6, 0x9000 - 0x10000)
        self.assertEqual(g.run(0x00086412), 0x81400000)  # sf 1, RT, V0, none, lm
        self.assertEqual(g.mac(), [-5, 6, -0x7000])
        self.assertEqual(g.irs(), [0, 6, 0])

    def test_mvmva_far_colour_bug(self):
        g = self.g
        g.matrix(RT, ((1, 2, 3), (4, 5, 6), (7, 8, 9)))
        g.vector(RFC, (0x10000000, 0, 0))
        g.v(1, 10, 20, 30)
        # FC*1000h + RT11*V1x is computed and flagged (IR1), then dropped.
        self.assertEqual(g.run(0x0000C012), 0x81000000)
        self.assertEqual(g.mac(), [40 + 90, 100 + 180, 160 + 270])

    def test_mac_44_bit_overflow_flags_and_wrap(self):
        g = self.g
        g.matrix(RT, ((0x7FFF, 0, 0), (0, 0, 0), (0, 0, 0)))
        g.vector(TRX, (0x7FFFFFFF, 0, 0))
        g.v(0, 0x7FFF, 0, 0)
        self.assertEqual(g.run(0x00080012), 0xC1000000)  # MAC1 > 43 bits, IR1 low
        wrapped = 0x7FFFFFFF * 4096 + 0x7FFF * 0x7FFF - (1 << 44)
        self.assertEqual(s32(g.d(MAC1)), s32(wrapped >> 12))
        self.assertEqual(g.irs()[0], -0x8000)
        g.vector(TRX, (-0x80000000, 0, 0))
        g.v(0, -0x8000, 0, 0)
        self.assertEqual(g.run(0x00080012), 0x89000000)  # MAC1 < -2^43, IR1 high
        self.assertEqual(s32(g.d(MAC1)), 0x80000000 - 0x7FFF * 8)
        self.assertEqual(g.irs()[0], 0x7FFF)

    def lights(self):
        g = self.g
        g.matrix(LLM, IDENTITY)
        g.matrix(LCM, IDENTITY)
        g.vector(RBK, (0x100, 0x200, 0x300))
        g.v(0, 0x800, 0x400, 0)
        g.d(RGBC, 0x34FF4080)

    def test_ncs(self):
        g = self.g
        self.lights()
        self.assertEqual(g.run(0x4AC8041E), 0)
        self.assertEqual(g.mac(), [0x900, 0x600, 0x300])
        self.assertEqual(g.irs(), [0x900, 0x600, 0x300])
        self.assertEqual(g.d(RGB2), 0x34306090)

    def test_nccs(self):
        g = self.g
        self.lights()
        self.assertEqual(g.run(0x4B08041B), 0)
        self.assertEqual(g.mac(), [0x480, 0x180, 0x2FD])
        self.assertEqual(g.irs(), [0x480, 0x180, 0x2FD])
        self.assertEqual(g.d(RGB2), 0x342F1848)

    def test_ncds(self):
        g = self.g
        self.lights()
        g.vector(RFC, (0xFF0, 0, 0))
        g.d(IR0, 0x800)
        self.assertEqual(g.run(0x4AE80413), 0)
        self.assertEqual(g.mac(), [0xA38, 0xC0, 0x17E])
        self.assertEqual(g.irs(), [0xA38, 0xC0, 0x17E])
        self.assertEqual(g.d(RGB2), 0x34170CA3)

    def test_cc_and_cdp(self):
        g = self.g
        g.matrix(LCM, IDENTITY)
        g.d(RGBC, 0x77204080)
        g.ir(0x1000, 0x800, 0)
        self.assertEqual(g.run(0x4B38041C), 0)
        self.assertEqual(g.mac(), [0x800, 0x200, 0])
        self.assertEqual(g.d(RGB2), 0x77002080)
        g.ir(0x1000, 0x800, 0)
        g.vector(RFC, (0x123, 0x456, 0x789))
        g.d(IR0, 0)
        self.assertEqual(g.run(0x4B280414), 0)
        self.assertEqual(g.mac(), [0x800, 0x200, 0])
        self.assertEqual(g.d(RGB1), 0x77002080)
        self.assertEqual(g.d(RGB2), 0x77002080)

    def test_dpcs_and_dcpl(self):
        g = self.g
        g.d(RGBC, 0x2C204080)
        g.vector(RFC, (0xFF0, 0, 0x100))
        g.d(IR0, 0x400)
        self.assertEqual(g.run(0x4A780010), 0)
        self.assertEqual(g.mac(), [0x9FC, 0x300, 0x1C0])
        self.assertEqual(g.d(RGB2), 0x2C1C309F)
        g.ir(0x1000, 0x1000, 0x1000)  # R * 1.0 << 4 = R << 16, as DPCS
        self.assertEqual(g.run(0x4A680029), 0)
        self.assertEqual(g.mac(), [0x9FC, 0x300, 0x1C0])
        self.assertEqual(g.d(RGB1), 0x2C1C309F)

    def test_intpl(self):
        g = self.g
        g.d(RGBC, 0x11000000)
        g.ir(0x100, 0x200, 0x300)
        g.vector(RFC, (0x1000, 0x1000, 0x1000))
        g.d(IR0, 0x800)
        self.assertEqual(g.run(0x4A980011), 0)
        self.assertEqual(g.mac(), [0x880, 0x900, 0x980])
        self.assertEqual(g.d(RGB2), 0x11989088)

    def test_far_colour_difference_saturates_as_lm_0(self):
        g = self.g
        g.d(RGBC, 0x000000FF)
        g.vector(RFC, (0, 0, 0))
        g.d(IR0, 0)
        # FC - R<<4 = -FF0h stays negative in IR1 without a flag although lm = 1 ...
        self.assertEqual(g.run(0x4A780410), 0)
        # ... and the final IR1 = MAC1 (FF0h) is clamped by lm only at the end.
        self.assertEqual(g.mac(), [0xFF0, 0, 0])
        self.assertEqual(g.irs(), [0xFF0, 0, 0])
        g.d(RGBC, 0)
        g.vector(RFC, (0x8000, 0, 0))
        self.assertEqual(g.run(0x4A780410), 0x81000000)  # 8000h > 7FFFh in IR1

    def test_triples_push_three_colours(self):
        g = self.g
        g.matrix(LLM, IDENTITY)
        g.matrix(LCM, IDENTITY)
        g.vector(RBK, (0x10, 0x20, 0x30))
        g.d(RGBC, 0x99000000)
        g.v(0, 0, 0, 0)
        g.v(1, 0x800, 0, 0)
        g.v(2, 0, 0x800, 0)
        self.assertEqual(g.run(0x4AD80420), 0)
        self.assertEqual([g.d(r) for r in (RGB0, RGB1, RGB2)], [0x99030201, 0x99030281, 0x99038201])
        # DPCT works on RGB0 each time as the FIFO advances; FC = 0, IR0 = .5.
        g.d(RGB0, 0x00302010)
        g.d(RGB1, 0x00605040)
        g.d(RGB2, 0x00908070)
        g.vector(RFC, (0, 0, 0))
        g.d(IR0, 0x800)
        self.assertEqual(g.run(0x4AF8002A), 0)
        self.assertEqual([g.d(r) for r in (RGB0, RGB1, RGB2)], [0x99181008, 0x99302820, 0x99484038])

    def test_gpf_gpl_and_colour_saturation(self):
        g = self.g
        g.d(RGBC, 0x5A000000)
        g.d(IR0, 0x1000)
        g.ir(0x7FFF, -16, 0x100)
        self.assertEqual(g.run(0x4B98003D), 0x300000)  # colour R and G saturate
        self.assertEqual(g.mac(), [0x7FFF, -16, 0x100])
        self.assertEqual(g.d(RGB2), 0x5A1000FF)
        g.d(MAC1, 1)
        g.d(MAC2, 2)
        g.d(MAC3, 3)
        g.d(IR0, 2)
        g.ir(10, 20, 30)
        self.assertEqual(g.run(0x4BA0003E), 0)
        self.assertEqual(g.mac(), [21, 42, 63])
        g.d(MAC1, 1)
        g.d(MAC2, 2)
        g.d(MAC3, 3)
        g.d(IR0, 0x1000)
        g.ir(10, 20, 30)
        self.assertEqual(g.run(0x4BA8003E), 0)
        self.assertEqual(g.mac(), [11, 22, 33])


def buffer(data):
    return ctypes.create_string_buffer(bytes(data), len(data))


def address(buf):
    return ctypes.c_void_p(ctypes.addressof(buf))


def matrix_bytes(rows, translation=(0, 0, 0), pad=0):
    return struct.pack("<9hH3i", *[v for row in rows for v in row], pad, *translation)


class AsmStatementTests(GteTest):
    """port/gte_asm.c, the inline statements' functions."""

    def call(self, name, *args, restype=None):
        function = getattr(LIB, name)
        function.restype = restype
        return function(*args)

    def test_loads(self):
        g = self.g
        vectors = buffer(struct.pack("<12h", *range(1, 13)))
        self.call("xem_gte_ldv3c", address(vectors))
        self.assertEqual(
            [g.d(r) for r in range(6)], [pair(1, 2), 3, pair(5, 6), 7, pair(9, 10), 11]
        )
        long_vector = buffer(struct.pack("<3i", 0x11112222, 0x33334444, -5))
        self.call("xem_gte_ldlv0", address(long_vector))
        self.assertEqual((g.d(VXY0), s32(g.d(VZ0))), (0x44442222, -5))
        self.call("xem_gte_ldsxy3", ctypes.c_uint(1), ctypes.c_uint(2), ctypes.c_uint(3))
        self.assertEqual([g.d(r) for r in (SXY0, SXY1, SXY2)], [1, 2, 3])
        self.call(
            "xem_gte_SetBackColor", ctypes.c_ubyte(1), ctypes.c_ubyte(2), ctypes.c_ubyte(0xFF)
        )
        self.assertEqual([g.c(r) for r in (RBK, GBK, BBK)], [0x10, 0x20, 0xFF0])
        column = buffer(struct.pack("<9h", 1, 0, 0, -2, 0, 0, 3, 0, 0))
        self.call("xem_gte_ldclmv", address(column))
        self.assertEqual(g.irs(), [1, -2, 3])

    def test_stores(self):
        g = self.g
        g.d(SXY0, 0x10)
        g.d(SXY1, 0x11)
        g.d(SXY2, 0x12)
        poly = buffer(bytes(40))
        self.call("xem_gte_stsxy3_ft4", address(poly))
        self.assertEqual(struct.unpack_from("<I", poly, 8)[0], 0x10)
        self.assertEqual(struct.unpack_from("<I", poly, 16)[0], 0x11)
        self.assertEqual(struct.unpack_from("<I", poly, 24)[0], 0x12)
        g.d(SZ3, 0x403)
        otz = buffer(bytes(4))
        self.call("xem_gte_stszotz", address(otz))
        self.assertEqual(struct.unpack("<i", otz.raw)[0], 0x100)
        out = (ctypes.c_uint * 3)()
        self.call("xem_gte_getsxy3", out)
        self.assertEqual(list(out), [0x10, 0x11, 0x12])
        self.assertEqual(self.call("xem_gte_getsxy2", restype=ctypes.c_uint), 0x12)
        g.c(FLAG, 0x00800000)
        flag = buffer(bytes(4))
        self.call("xem_gte_stflg", address(flag))
        self.assertEqual(struct.unpack("<I", flag.raw)[0], 0x80800000)

    def test_gtemac_matrix_product_sequence(self):
        # gte_MulMatrix0's three column steps through rtir and the column moves.
        m0 = buffer(matrix_bytes(((0x2000, 0, 0), (0, 0x2000, 0), (0, 0, 0x2000))))
        m1 = buffer(matrix_bytes(((16, 32, 48), (64, 80, 96), (112, 128, -144))))
        m2 = buffer(bytes(32))
        self.call("xem_gte_SetRotMatrix", address(m0))
        for offset in (0, 2, 4):
            self.call("xem_gte_ldclmv", ctypes.c_void_p(ctypes.addressof(m1) + offset))
            self.call("xem_gte_rtir")
            self.call("xem_gte_stclmv", ctypes.c_void_p(ctypes.addressof(m2) + offset))
        self.assertEqual(struct.unpack_from("<9h", m2), (32, 64, 96, 128, 160, 192, 224, 256, -288))


def fill(name, values):
    data = struct.pack(f"<{len(values)}h", *values)
    ctypes.memmove(ctypes.addressof(ctypes.c_short.in_dll(LIB, name)), data, len(data))


def load_rcossin():
    """rcossin_tbl from the resident's C (decomp/src) into the test object."""
    source = (ROOT / "decomp/src/resident/gpu_rotation_and_psyq_libraries.c").read_text()
    start = source.index("rcossin_tbl[4096][2] = {")
    body = source[source.index("{", start + 20) + 1 : source.index("};", start)]
    values = [int(v) for v in re.findall(r"-?\d+", re.sub(r"/\*.*?\*/", "", body, flags=re.S))]
    assert len(values) == 8192
    fill("rcossin_tbl", values)
    return struct.pack("<8192h", *values)


class LibgteCase(GteTest):
    def call(self, name, *args, restype=ctypes.c_int):
        function = getattr(LIB, name)
        function.restype = restype
        return function(*[ctypes.c_int(a) if isinstance(a, int) else a for a in args])


class LibgteTests(LibgteCase):
    @classmethod
    def setUpClass(cls):
        cls.table = load_rcossin()
        # Invented model tables: sqrt and 1/sqrt of 1.0-4.0 in 64ths, atan of 0-1 in 1024ths.
        fill(
            "libgte_square_root_table", [round(4096 * math.sqrt((i + 64) / 64)) for i in range(192)]
        )
        fill(
            "libgte_inverse_square_root_table",
            [round(4096 / math.sqrt((i + 64) / 64)) for i in range(198)],
        )
        fill(
            "libgte_arctangent_table",
            [round(math.atan(i / 1024) * 2048 / math.pi) for i in range(1026)],
        )

    def test_init_geom(self):
        self.call("InitGeom")
        g = self.g
        self.assertEqual(
            [g.c(r) for r in (ZSF3, ZSF4, H, DQA, DQB, OFX, OFY)],
            [0x155, 0x100, 1000, u32(-0x1062), 0x1400000, 0, 0],
        )

    def test_sine_cosine_and_rotation_matrices(self):
        self.assertEqual(self.call("gpu_get_sin", 1024), 4096)
        self.assertEqual(self.call("gpu_get_cos", 1024 + 4096), 0)
        self.assertEqual(self.call("gpu_get_sin", 3), struct.unpack_from("<h", self.table, 12)[0])
        identity = matrix_bytes(IDENTITY, (7, 8, 9), 0x5555)
        m = buffer(identity)
        self.call("RotMatrixZ", 1024, address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (0, -4096, 0, 4096, 0, 0, 0, 0, 4096))
        self.assertEqual(m.raw[18:], identity[18:])
        m = buffer(identity)
        self.call("RotMatrixZ", -1024, address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (0, 4096, 0, -4096, 0, 0, 0, 0, 4096))
        m = buffer(identity)
        self.call("RotMatrixX", 1024, address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (4096, 0, 0, 0, 0, -4096, 0, 4096, 0))
        m = buffer(identity)
        self.call("RotMatrixY", 1024, address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (0, 0, 4096, 0, 4096, 0, -4096, 0, 0))
        angles = buffer(struct.pack("<4h", 1024, 0, 0, 0))
        m = buffer(identity)
        self.call("RotMatrix", address(angles), address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (4096, 0, 0, 0, 0, -4096, 0, 4096, 0))
        m = buffer(identity)
        self.call("RotMatrixYXZ", address(angles), address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (4096, 0, 0, 0, 0, -4096, 0, 4096, 0))

    def test_composed_rotations_match_single_axis_products(self):
        # RotMatrix is Rz * Ry * Rx and RotMatrixYXZ is Ry * Rx * Rz, within
        # the rounding of their intermediate shifts.
        rng = random.Random(7)
        for _ in range(50):
            x, y, z = (rng.randrange(-4096, 4096) for _ in range(3))

            def axis(name, angle):
                m = buffer(matrix_bytes(IDENTITY))
                self.call(name, angle, address(m))
                return [list(struct.unpack_from("<9h", m)[i * 3 : i * 3 + 3]) for i in range(3)]

            def product(a, b):
                return [
                    [sum(a[i][k] * b[k][j] for k in range(3)) / 4096 for j in range(3)]
                    for i in range(3)
                ]

            angles = buffer(struct.pack("<4h", x, y, z, 0))
            for name, order in (("RotMatrix", ("Z", "Y", "X")), ("RotMatrixYXZ", ("Y", "X", "Z"))):
                m = buffer(matrix_bytes(IDENTITY))
                self.call(name, address(angles), address(m))
                got = struct.unpack_from("<9h", m)
                values = dict(zip("XYZ", (x, y, z), strict=True))
                want = product(
                    product(
                        axis("RotMatrix" + order[0], values[order[0]]),
                        axis("RotMatrix" + order[1], values[order[1]]),
                    ),
                    axis("RotMatrix" + order[2], values[order[2]]),
                )
                for i in range(9):
                    self.assertLessEqual(abs(got[i] - want[i // 3][i % 3]), 4, (name, x, y, z))

    def test_handwritten_rotation_matches_the_reviewed_reconstruction(self):
        rng = random.Random(1)
        for _ in range(500):
            angles = [rng.randrange(-0x8000, 0x8000) for _ in range(3)]
            source = buffer(struct.pack("<4h", *angles, 0x1234))
            m = buffer(matrix_bytes(IDENTITY, (1, 2, 3), 0xBEEF))
            result = self.call(
                "gpu_build_rotation_matrix", address(source), address(m), restype=ctypes.c_void_p
            )
            self.assertEqual(result, ctypes.addressof(m))
            self.assertEqual(struct.unpack_from("<9h", m), rotation(angles, self.table), angles)
            self.assertEqual(m.raw[18:], matrix_bytes(IDENTITY, (1, 2, 3), 0xBEEF)[18:])

    def test_scale_matrices_match_the_reviewed_reconstruction(self):
        rng = random.Random(2)
        for _ in range(200):
            original = struct.pack(
                "<9hH3i", *(rng.randrange(-0x8000, 0x8000) for _ in range(9)), 0xABCD, 1, 2, 3
            )
            scales = [rng.randrange(-0x20000, 0x20000) for _ in range(3)]
            v = buffer(struct.pack("<4i", *scales, 0))
            for name, columns in (("ScaleMatrixL", False), ("ScaleMatrix", True)):
                m = buffer(original)
                self.call(name, address(m), address(v), restype=ctypes.c_void_p)
                self.assertEqual(m.raw, scale_matrix(original, scales, columns), name)

    def test_matrix_products(self):
        g = self.g
        m0 = buffer(matrix_bytes(((0x2000, 0, 0), (0, 0x2000, 0), (0, 0, 0x2000)), (100, 200, 300)))
        m1 = buffer(matrix_bytes(((16, 32, 48), (64, 80, 96), (112, 128, -144)), (0x12345, -1, 2)))
        m2 = buffer(bytes(32))
        self.call("MulMatrix0", address(m0), address(m1), address(m2), restype=ctypes.c_void_p)
        self.assertEqual(
            struct.unpack_from("<9hH", m2), (32, 64, 96, 128, 160, 192, 224, 256, -288, 0xFFFF)
        )
        # CompMatrix rotates only the translation's low halves.
        m2 = buffer(bytes(32))
        self.call("CompMatrix", address(m0), address(m1), address(m2), restype=ctypes.c_void_p)
        self.assertEqual(struct.unpack_from("<3i", m2, 20), (0x2345 * 2 + 100, -2 + 200, 4 + 300))
        # MulMatrix2 into m1, SetMulMatrix into the rotation registers.
        self.call("SetMulMatrix", address(m0), address(m1), restype=ctypes.c_void_p)
        self.assertEqual(g.c(RT), pair(32, 64))
        self.assertEqual(s32(g.c(RT + 4)), -288)
        self.call("MulMatrix2", address(m0), address(m1), restype=ctypes.c_void_p)
        self.assertEqual(struct.unpack_from("<9h", m1), (32, 64, 96, 128, 160, 192, 224, 256, -288))
        self.call(
            "libgte_multiply_matrix_in_place", address(m0), address(m1), restype=ctypes.c_void_p
        )
        self.assertEqual(
            struct.unpack_from("<9h", m0), (64, 128, 192, 256, 320, 384, 448, 512, -576)
        )

    def test_apply_matrix_long_vector_splits_components(self):
        m = buffer(matrix_bytes(IDENTITY))
        for value in (0x12345678, -0x12345678, 100, -1, 0x3FFFFFFF):
            v0 = buffer(struct.pack("<4i", value, -value, 7, 0))
            v1 = buffer(bytes(16))
            self.call(
                "ApplyMatrixLV", address(m), address(v0), address(v1), restype=ctypes.c_void_p
            )
            self.assertEqual(struct.unpack_from("<3i", v1), (value, -value, 7))

    def test_transpose_including_in_place(self):
        values = (1, 2, 3, 4, 5, 6, 7, 8, 9)
        m = buffer(matrix_bytes((values[0:3], values[3:6], values[6:9]), (5, 6, 7), 0x7777))
        out = buffer(bytes(32))
        self.call("libgte_transpose_matrix", address(m), address(out))
        self.assertEqual(struct.unpack_from("<9h", out), (1, 4, 7, 2, 5, 8, 3, 6, 9))
        self.call("libgte_transpose_matrix", address(m), address(m))
        self.assertEqual(struct.unpack_from("<9h", m), (1, 4, 7, 2, 5, 8, 3, 6, 9))
        self.assertEqual(struct.unpack_from("<H3i", m, 18), (0x7777, 5, 6, 7))

    def test_projection_wrappers(self):
        g = self.g
        self.call("InitGeom")
        m = buffer(matrix_bytes(IDENTITY))
        self.call("SetRotMatrix", address(m))
        self.call("SetTransMatrix", address(m))
        self.call("SetGeomOffset", 160, 120)
        out = [buffer(bytes(4)) for _ in range(3)]
        v = buffer(struct.pack("<4h", 100, 50, 1000, 0))
        otz = self.call("RotTransPers", address(v), *(address(b) for b in out))
        self.assertEqual(otz, 250)
        self.assertEqual([struct.unpack("<I", b.raw)[0] for b in out], [pair(260, 170), 0, 0x1000])
        ofx, ofy = buffer(bytes(4)), buffer(bytes(4))
        self.call("ReadGeomOffset", address(ofx), address(ofy))
        self.assertEqual(
            (struct.unpack("<i", ofx.raw)[0], struct.unpack("<i", ofy.raw)[0]), (160, 120)
        )
        self.call("SetGeomScreen", 0x8000)
        self.assertEqual(self.call("ReadGeomScreen"), -0x8000)  # read back sign extended
        self.assertEqual(self.call("NormalClip", pair(0, 0), pair(10, 0), pair(0, 10)), 100)
        self.assertEqual(self.call("libgte_average_z4", 100, 200, 300, 400), 62)
        self.assertEqual(g.d(SZ0), 100)

    def test_front_quad_returns_nclip_and_skips_back_faces(self):
        self.call("InitGeom")
        m = buffer(matrix_bytes(IDENTITY))
        self.call("SetRotMatrix", address(m))
        self.call("SetTransMatrix", address(m))
        corners = [
            buffer(struct.pack("<4h", x, y, 1000, 0))
            for x, y in ((0, 0), (100, 0), (0, 100), (100, 100))
        ]
        outs = [buffer(struct.pack("<i", -1)) for _ in range(7)]
        clip = self.call(
            "libgte_project_front_quad", *(address(c) for c in corners), *(address(o) for o in outs)
        )
        self.assertEqual(clip, 100 * 100)
        values = [struct.unpack("<i", o.raw)[0] for o in outs]
        self.assertEqual(values[:4], [pair(0, 0), pair(100, 0), pair(0, 100), pair(100, 100)])
        self.assertEqual(values[5], (4000 * 0x100) >> 12)  # AVSZ4 with InitGeom's ZSF4
        self.assertEqual(values[6], 0x1000)
        corners[1], corners[2] = corners[2], corners[1]
        outs = [buffer(struct.pack("<i", -1)) for _ in range(7)]
        clip = self.call(
            "libgte_project_front_quad", *(address(c) for c in corners), *(address(o) for o in outs)
        )
        self.assertEqual(clip, -100 * 100)
        self.assertEqual([struct.unpack("<i", o.raw)[0] for o in outs], [-1] * 6 + [0x1000])

    def test_matrix_stack(self):
        g = self.g
        depth = ctypes.c_int.in_dll(LIB, "libgte_matrix_stack_depth")
        calls = ctypes.c_int.in_dll(LIB, "test_debug_print_calls")
        depth.value = 0
        calls.value = 0
        for i in range(8):
            g.c(i, 0x1000 + i)
        self.call("PushMatrix")
        self.assertEqual(depth.value, 0x20)
        for i in range(8):
            g.c(i, 0)
        self.call("PopMatrix")
        self.assertEqual([g.c(i) for i in range(8)], [0x1000 + i for i in range(8)])
        self.assertEqual(depth.value, 0)
        self.call("PopMatrix")  # underflow: the message, nothing popped
        self.assertEqual((depth.value, calls.value), (0, 1))
        depth.value = 0x280
        self.call("PushMatrix")  # overflow
        self.assertEqual((depth.value, calls.value), (0x280, 2))

    def test_square_root_and_vector_normalization(self):
        for value, root in ((0, 0), (100, 10), (1 << 24, 4096), (1 << 26, 8192), (-1, 0)):
            self.assertEqual(self.call("SquareRoot0", value), root, value)
        v0 = buffer(struct.pack("<4i", 4096, 0, 0, 0))
        v1 = buffer(bytes(16))
        self.assertEqual(self.call("VectorNormal", address(v0), address(v1)), 1 << 24)
        self.assertEqual(struct.unpack_from("<3i", v1), (4096, 0, 0))
        # 10000: LZCR 18, shift 6, mantissa 156, 1/sqrt(156/64) = 2624 here.
        v0 = buffer(struct.pack("<4h", 0, 0, 100, 0))
        v1 = buffer(bytes(8))
        self.call("VectorNormalSS", address(v0), address(v1))
        self.assertEqual(struct.unpack_from("<3h", v1), (0, 0, 2624 * 100 >> 6))

    def test_ratan2_quadrants(self):
        for y, x, angle in (
            (0, 0, 0),
            (0, 1, 0),
            (1, 0, 1024),
            (1, 1, 512),
            (0, -1, 2048),
            (-1, 0, -1024),
            (-1, -1, -1536),
            (0x40000000, 0x40000000, 512),
            (0x200000, 0x100000, 1024 - 302),
        ):
            self.assertEqual(self.call("ratan2", y, x), angle, (y, x))

    def test_fog_and_colours(self):
        g = self.g
        self.call("SetFogNearFar", 100, 1100, 1000)
        self.assertEqual((s32(g.c(DQA)), g.c(DQB)), (-28, 4505 << 12))
        self.call("SetFogNearFar", 100, 199, 1000)  # range < 100: unchanged
        self.assertEqual((s32(g.c(DQA)), g.c(DQB)), (-28, 4505 << 12))
        self.call("SetBackColor", 1, 2, 3)
        self.call("SetFarColor", 4, 5, 6)
        self.assertEqual([g.c(r) for r in (RBK, GBK, BBK, RFC, GFC, BFC)], [16, 32, 48, 64, 80, 96])
        light = buffer(matrix_bytes(IDENTITY))
        self.call("SetLightMatrix", address(light))
        self.call("SetColorMatrix", address(light))
        self.call("SetBackColor", 0x10, 0x20, 0x30)
        normal = buffer(struct.pack("<4h", 0x800, 0x400, 0, 0))
        colour = buffer(bytes(4))
        self.call("NormalColor", address(normal), address(colour))
        self.assertEqual(struct.unpack("<I", colour.raw)[0], g.d(RGB2))
        self.assertEqual(g.mac(), [0x900, 0x600, 0x300])

    def test_returns_left_in_v0_and_pad_words(self):
        g = self.g
        g.d(LZCS, 0x10000)
        v0 = buffer(struct.pack("<4i", 0x1000, 0x2000, 0x3000, 0))
        v2 = buffer(bytes(16))
        # LoadAverage12 returns LZCR, read where the SDK meant FLAG.
        result = self.call(
            "libgte_weighted_sum_vector12", address(v0), address(v0), 0x800, 0x800, address(v2)
        )
        self.assertEqual(result, 15)
        self.assertEqual(struct.unpack_from("<3i", v2), (0x1000, 0x2000, 0x3000))
        # RotTransSV writes IR3 as a word over the pad.
        m = buffer(matrix_bytes(IDENTITY, (0, 0, -10)))
        self.call("SetRotMatrix", address(m))
        self.call("SetTransMatrix", address(m))
        sv = buffer(struct.pack("<4h", 1, 2, 3, 0x7777))
        out = buffer(struct.pack("<4h", 9, 9, 9, 0x7777))
        flag = buffer(bytes(4))
        self.call("RotTransSV", address(sv), address(out), address(flag))
        self.assertEqual(struct.unpack("<4h", out.raw[:8]), (1, 2, -7, -1))


@unittest.skipUnless(
    RESIDENT_IMAGE.is_file(),
    "the user's resident executable (XEM_RESIDENT_IMAGE) is absent",
)
class OriginalTableTests(LibgteCase):
    """The SDK's own tables from the user's image, against floating point."""

    @classmethod
    def setUpClass(cls):
        load_rcossin()
        image = RESIDENT_IMAGE.read_bytes()
        load = struct.unpack_from("<I", image, 0x18)[0]

        def halves(name, start, count):
            fill(name, struct.unpack_from(f"<{count}h", image, start - load + 0x800))

        halves("libgte_square_root_table", 0x80056A00, 192)
        halves("libgte_inverse_square_root_table", 0x80056B94, 198)
        halves("libgte_arctangent_table", 0x80057030, 1025)

    def test_original_square_root(self):
        rng = random.Random(3)
        for value in [0, 1, 4, 100, 0x7FFFFFFF] + [rng.randrange(1, 1 << 31) for _ in range(2000)]:
            got = self.call("SquareRoot0", value)
            self.assertLessEqual(
                abs(got - math.isqrt(value)), max(1, math.isqrt(value) >> 6), value
            )

    def test_original_arctangent(self):
        rng = random.Random(4)
        for _ in range(2000):
            y, x = rng.randrange(-1 << 20, 1 << 20), rng.randrange(-1 << 20, 1 << 20)
            if not (y or x):
                continue
            expected = math.atan2(y, x) * 2048 / math.pi
            self.assertLessEqual(abs(self.call("ratan2", y, x) - expected), 2, (y, x))

    def test_original_normalization(self):
        rng = random.Random(5)
        for _ in range(500):
            # The squared length must fit 31 bits: the original sums with a trapping add.
            v = [rng.randrange(-0x6000, 0x6000) for _ in range(3)]
            if not any(v):
                continue
            out = buffer(bytes(8))
            source = buffer(struct.pack("<4h", *v, 0))
            self.call("VectorNormalSS", address(source), address(out))
            length = math.sqrt(sum(c * c for c in v))
            for got, c in zip(struct.unpack_from("<3h", out), v, strict=True):
                # The table steps by 1/64 to 1/256.
                self.assertLessEqual(abs(got - 4096 * c / length), 32, v)


class AsmMapTests(unittest.TestCase):
    COP2 = re.compile(r"\b(lwc2|swc2|mtc2|mfc2|ctc2|cfc2|cop2)\b|\.word 0x4[AB]")
    CALL = re.compile(r'asm sideeffect "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"')

    def test_every_mapped_function_is_defined(self):
        entries = json.loads((ROOT / "port/asm_map.json").read_text())
        source = (ROOT / "port/gte_asm.c").read_text()
        defined = set(re.findall(r"^\w[\w ]*?\b(xem_gte_\w+)\(", source, re.M))
        self.assertEqual({e["function"] for e in entries}, defined)
        keys = [(e["template"], e["constraints"]) for e in entries]
        self.assertEqual(len(keys), len(set(keys)))

    def test_every_gte_statement_in_the_decomp_is_mapped(self):
        entries = json.loads((ROOT / "port/asm_map.json").read_text())
        mapped = {(e["template"], e["constraints"]) for e in entries}
        users = sorted(
            path
            for path in (ROOT / "decomp/src").rglob("*.c")
            if re.search(r'#include "(psyq/inline_c|gte)\.h"', path.read_text(errors="replace"))
        )
        self.assertTrue(users)
        found = set()
        with tempfile.TemporaryDirectory() as out:
            for path in users:
                ir = os.path.join(out, path.stem + ".ll")
                subprocess.run(
                    [
                        CLANG,
                        "--target=mipsel-unknown-unknown",
                        "-S",
                        "-emit-llvm",
                        "-O0",
                        "-std=gnu89",
                        "-undef",
                        "-nostdinc",
                        "-ffreestanding",
                        "-Iport/include",
                        "-Idecomp/include",
                        "-Dmips",
                        "-D__mips__",
                        "-D__mips",
                        "-Dpsx",
                        "-D__psx__",
                        "-D__psx",
                        "-D_PSYQ",
                        "-D_MIPSEL",
                        "-D__CHAR_UNSIGNED__",
                        "-D_LANGUAGE_C",
                        "-DLANGUAGE_C",
                        "-funsigned-char",
                        "-fwrapv",
                        "-fno-strict-aliasing",
                        "-fno-builtin",
                        "-w",
                        "-Wno-return-mismatch",
                        '-DORIGINAL_IMAGE="x"',
                        "-DORIGINAL_BASE=0",
                        str(path),
                        "-o",
                        ir,
                    ],
                    cwd=ROOT,
                    check=True,
                )
                for template, constraints in self.CALL.findall(Path(ir).read_text()):
                    if self.COP2.search(template):
                        found.add((template, constraints))
        self.assertEqual(found - mapped, set())
        self.assertEqual(mapped - found, set())


if __name__ == "__main__":
    unittest.main()
