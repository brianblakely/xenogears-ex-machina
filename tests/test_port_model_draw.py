"""The port's model renderers and environment-map patcher (port/model_draw.c)
against the contracts of decomp/src/resident/model_draw*.s, model_depth.s and
model_set_envmap_mapping.s, natively (tests/port_native.py), on invented
inputs."""

import struct
import tempfile
import unittest

from tests import port_native as mem

# The original addresses of the globals the renderers use.
SYMBOLS = {
    "model_envmap_patch_base": 0x800308D0,
}

LIB = None
BUILD = None


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/model_draw.c"], SYMBOLS)


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def srl(rd, rt, sa):
    return (rt << 16) | (rd << 11) | (sa << 6) | 0x02


def addiu(rt, rs, imm):
    return (0x09 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


T0, T1 = 8, 9
# (offset from the patch base, instruction word) of the image's mapping code.
U_SHIFTS = (0x30, 0x5C, 0x7C)
V_SHIFTS = (0x3C, 0x68, 0x88)


def image_mapping_code():
    base = SYMBOLS["model_envmap_patch_base"]
    for offset in U_SHIFTS:
        mem.put32(base + offset, srl(T0, T0, 6), addiu(T0, T0, 0x40))
    for offset in V_SHIFTS:
        mem.put32(base + offset, srl(T1, T1, 6), addiu(T1, T1, 0x40))


class EnvmapMapping(unittest.TestCase):
    def setUp(self):
        mem.clear()
        image_mapping_code()

    def test_patch_rewrites_shift_fields_and_immediates_only(self):
        base = SYMBOLS["model_envmap_patch_base"]
        before = mem.read(base, 0x90)
        LIB.model_set_envmap_mapping(5, 4, 0x41, 0x3F)
        for offset in U_SHIFTS:
            self.assertEqual(mem.u32(base + offset), srl(T0, T0, 5))
            self.assertEqual(mem.u32(base + offset + 4), addiu(T0, T0, 0x41))
        for offset in V_SHIFTS:
            self.assertEqual(mem.u32(base + offset), srl(T1, T1, 4))
            self.assertEqual(mem.u32(base + offset + 4), addiu(T1, T1, 0x3F))
        after = mem.read(base, 0x90)
        patched = {o + k for s in U_SHIFTS + V_SHIFTS for o in (s, s + 4) for k in range(4)}
        for i in range(0x90):
            if i not in patched:
                self.assertEqual(before[i], after[i], f"byte {i:#x}")

    def test_patch_writes_halfwords_like_the_original(self):
        # sh of the shifted count after `andi 0xF83F`: a count of 32 or more
        # spills into rd, as the original's or does; the upper halfword stays.
        base = SYMBOLS["model_envmap_patch_base"]
        LIB.model_set_envmap_mapping(33, 1, -2, 0x12345)
        word = mem.u32(base + U_SHIFTS[0])
        self.assertEqual(word & 0xFFFF0000, srl(T0, T0, 6) & 0xFFFF0000)
        self.assertEqual(word & 0xFFFF, ((srl(T0, T0, 6) & 0xF83F) | (33 << 6)) & 0xFFFF)
        self.assertEqual(mem.u32(base + U_SHIFTS[0] + 4), addiu(T0, T0, 0xFFFE))
        self.assertEqual(mem.u32(base + V_SHIFTS[0] + 4), addiu(T1, T1, 0x2345))


if __name__ == "__main__":
    unittest.main()
