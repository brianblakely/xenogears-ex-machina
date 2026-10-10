"""The port's menu overlay routines (port/arena.c) against the contracts of
decomp/src/menu/*.s (all but the task switch), natively (tests/port_native.py),
on invented inputs.

Two kinds of checks: values worked by hand from the contracts, and, for the
packet builders, a register-level transliteration of the original assembly
below (`Asm*`), which drives the same software GTE through the original's
transfer and command sequence; the port and the transliteration start from
the same memory and GTE state and must leave the same bytes and the same GTE
registers. The GTE arithmetic itself is tested in tests/test_port_gte.py.
"""

import ctypes
import random
import struct
import tempfile
import unittest

from tests import port_native as mem

# Original addresses (the menu link's symbols).
SYMBOLS = {
    "model_screen_x_limit": 0x800500F8,
    "model_screen_y_limit": 0x800500FC,
    "model_current_packet": 0x80059424,
    "model_current_vertices": 0x8005953C,
    "model_ot": 0x80059568,
    "model_drawn_primitive_count": 0x80059578,
    "model_submitted_primitive_count": 0x800595C0,
    "arena_stage_ground_triangles": 0x80092854,
    "arena_draw_buffer_index": 0x800928A0,
    "arena_stage_height_map": 0x800928DC,
    "arena_mesh_light_direction": 0x8009A2C8,
}
S = SYMBOLS

# GTE commands (decomp/include/macro.inc).
RTPS, RTPT, NCLIP, DPCS = 0x0180001, 0x0280030, 0x1400006, 0x0780010


def gpf(sf):
    return 0x190003D | sf << 19


def mvmva(sf, mx, v, cv, lm):
    return 0x0400012 | sf << 19 | mx << 17 | v << 15 | cv << 13 | lm << 10


M32 = 0xFFFFFFFF
LIB = None
BUILD = None


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/arena.c", "tests/port_arena/support.c"], SYMBOLS)
    for name in ("arena_gte_scale_svector", "arena_gte_multiply_svector", "arena_gte_scale_vector_low_halves",
                 "arena_gte_rotate_scale_svector"):
        getattr(LIB, name).argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    LIB.arena_gte_scale_matrix_columns.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    LIB.arena_box_filter_rgb555.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    LIB.arena_copy_words.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    LIB.arena_stage_draw_ground_cells.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
    LIB.arena_stage_draw_ground_cells.restype = ctypes.c_uint
    LIB.arena_mesh_project_shadow.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    LIB.arena_mesh_draw_flat_triangles.argtypes = [ctypes.c_void_p, ctypes.c_int]
    LIB.arena_mesh_draw_flat_quads.argtypes = [ctypes.c_void_p, ctypes.c_int]
    for name in ("arena_gte_scale_svector", "arena_gte_multiply_svector", "arena_gte_scale_vector_low_halves",
                 "arena_gte_rotate_scale_svector", "arena_gte_scale_matrix_columns", "arena_box_filter_rgb555",
                 "arena_copy_words", "arena_mesh_project_shadow", "arena_mesh_draw_flat_triangles",
                 "arena_mesh_draw_flat_quads"):
        getattr(LIB, name).restype = None


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def s16v(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def u8(address):
    return mem.read(address, 1)[0]


def svec(x, y, z, pad=0):
    return struct.pack("<4h", x, y, z, pad)


def breaks():
    count = ctypes.c_int.in_dll(LIB, "test_break_count").value
    codes = (ctypes.c_uint * 8).in_dll(LIB, "test_break_codes")
    return [codes[i] for i in range(min(count, 8))]


def reset_breaks():
    ctypes.c_int.in_dll(LIB, "test_break_count").value = 0


# ---------------------------------------------------------------- GTE set-up

def gte_clear():
    mem.gte_restore(LIB, [0] * 64)


def set_rotation(m):
    """Rotation matrix rows into control registers 0-4."""
    (a, b, c), (d, e, f), (g, h, i) = m
    words = [pair(a, b), pair(c, d), pair(e, f), pair(g, h), i & 0xFFFF]
    for reg, word in enumerate(words):
        LIB.xem_gte_write_control(reg, word)


def pair(low, high):
    return (low & 0xFFFF) | (high & 0xFFFF) << 16


def set_projection(tr=(0, 0, 0), ofx=160, ofy=120, h=256, dqa=-0x100, dqb=0x1000000, far=(0x10, 0x20, 0x30)):
    set_rotation(((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
    for reg, value in zip((5, 6, 7), tr):
        LIB.xem_gte_write_control(reg, value & M32)
    LIB.xem_gte_write_control(24, (ofx << 16) & M32)
    LIB.xem_gte_write_control(25, (ofy << 16) & M32)
    LIB.xem_gte_write_control(26, h)
    LIB.xem_gte_write_control(27, dqa & 0xFFFF)
    LIB.xem_gte_write_control(28, dqb & M32)
    for reg, value in zip((21, 22, 23), far):
        LIB.xem_gte_write_control(reg, value)


# ---------------------------------------------------------------- the transliteration's machine

class Asm:
    """Memory and GTE operations as the original instructions make them."""

    @staticmethod
    def lw(a):
        return mem.u32(xa(a))

    @staticmethod
    def lhu(a):
        return mem.u16(xa(a))

    @staticmethod
    def lh(a):
        return mem.s16(xa(a)) & M32

    @staticmethod
    def lbu(a):
        return u8(xa(a))

    @staticmethod
    def sw(a, v):
        mem.put32(xa(a), v)

    @staticmethod
    def sh(a, v):
        mem.put16(xa(a), v)

    @staticmethod
    def lwc2(reg, a):
        LIB.xem_gte_write_data(reg, mem.u32(xa(a)))

    @staticmethod
    def swc2(reg, a):
        mem.put32(xa(a), LIB.xem_gte_read_data(reg))

    @staticmethod
    def mtc2(reg, v):
        LIB.xem_gte_write_data(reg, v & M32)

    @staticmethod
    def mfc2(reg):
        return LIB.xem_gte_read_data(reg)

    @staticmethod
    def ctc2(reg, v):
        LIB.xem_gte_write_control(reg, v & M32)

    @staticmethod
    def cop2(word):
        LIB.xem_gte_execute(word)


def xa(address):
    """The PS1 address's location in the mapped game memory."""
    physical = address & 0x1FFFFFFF
    if physical < 0x800000:
        return 0x80000000 | (physical & 0x1FFFFF)
    return physical


A = Asm


def slt(a, b):
    return int(s32(a) < s32(b))


def sltu(a, b):
    return int((a & M32) < (b & M32))


def asm_mesh_triangles(a0, a1):
    a2 = A.lw(S["model_current_vertices"])
    a3 = A.lw(S["model_drawn_primitive_count"])
    t7 = A.lw(S["model_current_packet"])
    t8 = A.lw(S["model_ot"])
    v0 = A.lw(S["model_screen_y_limit"])
    v1 = A.lw(S["model_screen_x_limit"])
    t9 = 4 << 24

    def face_vectors(packet_bytes):
        nonlocal t7
        t4 = A.lw(a0)
        t5 = A.lhu(a0 + 4)
        if packet_bytes:
            t7 = (t7 + packet_bytes) & M32
        t0 = ((t4 & 0xFFFF) << 3) + a2
        A.lwc2(0, t0)
        A.lwc2(1, t0 + 4)
        t0 = ((t4 >> 13) & 0xFFF8) + a2
        A.lwc2(2, t0)
        A.lwc2(3, t0 + 4)
        t0 = ((t5 << 3) + a2) & M32
        A.lwc2(4, t0)
        A.lwc2(5, t0 + 4)

    face_vectors(0)
    t7 = ((t7 - 20) & M32) & 0x00FFFFFF
    while True:
        A.cop2(RTPT)
        if a1 == 0:
            break
        a1 = (a1 - 1) & M32
        a0 += 8
        face_vectors(20)
        t1 = A.mfc2(12)
        t2 = A.mfc2(13)
        t0 = sltu(t1, v0)
        t3 = A.mfc2(14)
        A.cop2(NCLIP)
        if not t0:
            t0 = sltu(t2, v0)
            if not t0:
                t0 = sltu(t3, v0)
                if not t0:
                    continue
        t0 = A.mfc2(24)
        a3 = (a3 + 1) & M32
        if s32(t0) <= 0:
            continue
        if not (sltu(t1 & 0xFFFF, v1) or sltu(t2 & 0xFFFF, v1) or sltu(t3 & 0xFFFF, v1)):
            continue
        A.sw(t7 + 8, t1)
        A.sw(t7 + 12, t2)
        A.sw(t7 + 16, t3)
        t1 = A.lw(t8)
        A.sw(t8, t7)
        A.sw(t7, t1 | t9)
    t7 = (t7 + 20) & M32
    A.sw(S["model_drawn_primitive_count"], a3)
    A.sw(S["model_current_packet"], t7)


def asm_mesh_quads(a0, a1):
    a2 = A.lw(S["model_current_vertices"])
    a3 = A.lw(S["model_drawn_primitive_count"])
    t7 = A.lw(S["model_current_packet"])
    t8 = A.lw(S["model_ot"])
    v0 = A.lw(S["model_screen_y_limit"])
    v1 = A.lw(S["model_screen_x_limit"])
    t9 = 5 << 24
    t6 = 0

    def face_vectors(packet_bytes):
        nonlocal t7, t6
        t4 = A.lw(a0)
        t5 = A.lhu(a0 + 4)
        if packet_bytes:
            t7 = (t7 + packet_bytes) & M32
        t6 = (((t4 & 0xFFFF) << 3) + a2) & M32
        t0 = ((t4 >> 13) & 0xFFF8) + a2
        A.lwc2(2, t0)
        A.lwc2(3, t0 + 4)
        t0 = ((t5 << 3) + a2) & M32
        A.lwc2(4, t0)
        A.lwc2(5, t0 + 4)

    face_vectors(0)
    t7 = ((t7 - 24) & M32) & 0x00FFFFFF
    while True:
        A.lwc2(0, t6)
        A.lwc2(1, t6 + 4)
        A.cop2(RTPT)  # the branch's delay slot
        if a1 == 0:
            break
        a1 = (a1 - 1) & M32
        a0 += 8
        face_vectors(24)
        t1, t2, t3 = A.mfc2(12), A.mfc2(13), A.mfc2(14)
        A.cop2(NCLIP)
        t0 = ((A.lhu(a0 - 2) << 3) + a2) & M32
        t4 = A.mfc2(24)
        a3 = (a3 + 1) & M32
        A.lwc2(0, t0)  # delay slot
        if s32(t4) <= 0:
            continue
        A.lwc2(1, t0 + 4)
        A.cop2(RTPS)
        t4 = A.mfc2(14)
        if not any(sltu(t, v0) for t in (t1, t2, t3, t4)):
            continue
        if not any(sltu(t & 0xFFFF, v1) for t in (t1, t2, t3, t4)):
            continue
        A.sw(t7 + 8, t1)
        A.sw(t7 + 12, t2)
        A.sw(t7 + 16, t3)
        A.sw(t7 + 20, t4)
        t1 = A.lw(t8)
        A.sw(t8, t7)
        A.sw(t7, t1 | t9)
    t7 = (t7 + 24) & M32
    A.sw(S["model_drawn_primitive_count"], a3)
    A.sw(S["model_current_packet"], t7)


def asm_ground(a0, a1, a2):
    s5, s6, s7 = 0x00FFFFFF, 0xFFFF0000, 0x07000000
    t8, t6, a3, t5 = 0x1F800000, 0x1F800080, 0x1F800100, 0x1F800120
    s4 = A.lw(S["arena_stage_ground_triangles"] + 4 * A.lbu(S["arena_draw_buffer_index"]))

    def packet(second, third):
        nonlocal s4
        A.cop2(DPCS)
        t0, t1, t2 = A.lhu(s2), A.lhu(s2 + second), A.lhu(s2 + third)
        t3 = s3 & s6
        A.sw(s4 + 12, s1 | t0 | t3)
        t3 = (s3 << 16) & M32
        A.sw(s4 + 20, s1 | t1 | t3)
        A.sh(s4 + 28, s1 | t2)
        t0, t1, t2 = A.mfc2(17), A.mfc2(18), A.mfc2(19)
        t3 = slt(t1, t0)
        A.swc2(22, s4 + 4)
        if not t3:
            t0 = t1
        t3 = slt(t2, t0)
        s4 &= s5
        if not t3:
            t0 = t2
        t0 = ((t0 >> 4) << 2) + a0
        t1 = A.lw(t0)
        A.swc2(14, s4 + 24)
        A.swc2(12, s4 + 8)
        A.swc2(13, s4 + 16)
        A.sw(t0, s4)
        t1 |= s7
        s4 = (s4 + 32) & M32
        A.sw(s4 - 32, t1)

    submitted = A.lw(S["model_submitted_primitive_count"])
    for t7 in range(0x7F):
        t2 = A.lbu(t8 + t7)
        if t2 == 0:
            continue
        t1 = A.lbu(t6 + t7)
        if t1 == 0xFF or not sltu(t1, t2):
            continue
        s1 = t2 - t1
        v1 = ((t7 << 8) - a2) & M32
        v0 = ((t1 << 8) - a1) & M32
        submitted = (submitted + s1) & M32
        A.sw(S["model_submitted_primitive_count"], submitted)
        t4 = (A.lw(S["arena_stage_height_map"]) + (((t7 << 7) + t1) << 2)) & M32
        t9 = (t4 + (s1 << 2)) & M32
        while True:
            A.sh(a3 + 24, v0)
            A.sh(a3 + 0, v0)
            A.sh(a3 + 20, v1)
            A.sh(a3 + 4, v1)
            v0 = (v0 + 0x100) & M32
            A.sh(a3 + 16, v0)
            A.sh(a3 + 8, v0)
            t0 = (v1 + 0x100) & M32
            A.sh(a3 + 28, t0)
            A.sh(a3 + 12, t0)
            t0 = A.lw(t4)
            A.sh(a3 + 2, t0)
            A.sh(a3 + 10, A.lhu(t4 + 0x204))
            A.sh(a3 + 18, A.lhu(t4 + 4))
            A.sh(a3 + 26, A.lhu(t4 + 0x200))
            for reg in range(6):
                A.lwc2(reg, a3 + 4 * reg)
            t1 = t0 >> 16
            s1 = t1 & 0xF0F0
            A.cop2(RTPT)
            s2 = t5 + ((t1 & 3) << 3)
            s3 = A.lw((t1 & 12) + t5 + 32)
            A.cop2(NCLIP)
            A.lwc2(2, a3 + 24)
            A.lwc2(3, a3 + 28)
            A.lwc2(4, a3 + 8)
            t0 = A.mfc2(24)
            A.lwc2(5, a3 + 12)
            if s32(t0) > 0:
                packet(6, 2)
            A.cop2(RTPT)
            A.cop2(NCLIP)
            t0 = A.mfc2(24)
            t4 = (t4 + 4) & M32
            if s32(t0) > 0:
                packet(4, 6)
            if t4 == t9:
                break
    t0 = A.lw(S["arena_stage_ground_triangles"] + 4 * A.lbu(S["arena_draw_buffer_index"])) & s5
    count = (((s4 & s5) - t0) & M32) >> 5
    A.sw(S["model_drawn_primitive_count"], A.lw(S["model_drawn_primitive_count"]) + count)
    return count


# ---------------------------------------------------------------- comparison

def snapshot():
    return (mem.read(mem.RAM, mem.RAM_BYTES), mem.read(mem.SCRATCHPAD, mem.SCRATCHPAD_BYTES), mem.gte_snapshot(LIB))


def restore(state):
    ram, pad, gte = state
    mem.write(mem.RAM, ram)
    mem.write(mem.SCRATCHPAD, pad)
    mem.gte_restore(LIB, gte)


class Compare(unittest.TestCase):
    def assertSameEffects(self, port, original):
        """Run both from the same state; same bytes, same GTE registers."""
        before = snapshot()
        port_result = port()
        port_state = snapshot()
        restore(before)
        original_result = original()
        original_state = snapshot()
        self.assertEqual(port_result, original_result)
        for name, a, b in (("RAM", port_state[0], original_state[0]), ("scratchpad", port_state[1], original_state[1])):
            if a != b:
                first = next(i for i in range(len(a)) if a[i] != b[i])
                self.fail(f"{name} differs first at +{first:#x}: port {a[first:first + 8].hex()}, "
                          f"original {b[first:first + 8].hex()}")
        self.assertEqual(port_state[2], original_state[2], "GTE registers")
        restore(port_state)
        return port_result


# ---------------------------------------------------------------- tests

BUF = 0x80100000


class VectorScale(unittest.TestCase):
    def setUp(self):
        mem.clear()
        gte_clear()

    def test_scale_svector_is_4_12_and_saturates(self):
        mem.write(BUF, svec(0x1000, -0x800, 0x7FFF, 0x1234))
        mem.write(BUF + 8, svec(0, 0, 0, 0x5A5A))
        LIB.arena_gte_scale_svector(BUF, BUF + 8, 0x800)
        self.assertEqual(struct.unpack("<4h", mem.read(BUF + 8, 8)), (0x800, -0x400, 0x3FFF, 0x5A5A))
        LIB.arena_gte_scale_svector(BUF, BUF + 8, 0x4000)
        self.assertEqual(struct.unpack("<4h", mem.read(BUF + 8, 8)), (0x4000, -0x2000, 0x7FFF, 0x5A5A))
        self.assertEqual(LIB.xem_gte_read_data(8), 0x4000)  # IR0 stays the scale

    def test_multiply_svector_has_no_fraction(self):
        mem.write(BUF, svec(3, -4, 0x4000))
        LIB.arena_gte_multiply_svector(BUF, BUF + 8, 7)
        self.assertEqual(struct.unpack("<3h", mem.read(BUF + 8, 6)), (21, -28, 0x7FFF))
        LIB.arena_gte_multiply_svector(BUF, BUF + 8, -3)
        self.assertEqual(struct.unpack("<3h", mem.read(BUF + 8, 6)), (-9, 12, -0x8000))

    def test_vector_low_halves_ignore_upper_halves(self):
        mem.put32(BUF, 0xABCD0010, 0x1234FFF0, 0xFFFF0400)
        LIB.arena_gte_scale_vector_low_halves(BUF, BUF + 16, 0x2000)
        self.assertEqual(struct.unpack("<3h", mem.read(BUF + 16, 6)), (0x20, -0x20, 0x800))

    def test_output_may_be_the_input(self):
        mem.write(BUF, svec(100, 200, -300, 9))
        LIB.arena_gte_scale_svector(BUF, BUF, 0x3000)
        self.assertEqual(struct.unpack("<4h", mem.read(BUF, 8)), (300, 600, -900, 9))

    def test_rotate_scale_ignores_translation(self):
        set_rotation(((0, 0x1000, 0), (0x1000, 0, 0), (0, 0, -0x1000)))
        for reg in (5, 6, 7):
            LIB.xem_gte_write_control(reg, 0x10000)
        mem.write(BUF, svec(100, 200, 300, 0x77))
        mem.write(BUF + 8, svec(0, 0, 0, 0x66))
        LIB.arena_gte_rotate_scale_svector(BUF, BUF + 8, 0x2000)
        self.assertEqual(struct.unpack("<4h", mem.read(BUF + 8, 8)), (400, 200, -600, 0x66))
        # MAC1-3 hold the scaled results of GPF, IR0 the scale.
        self.assertEqual([s32(LIB.xem_gte_read_data(r)) for r in (25, 26, 27)], [400, 200, -600])
        self.assertEqual(LIB.xem_gte_read_data(8), 0x2000)

    def test_rotate_scale_saturates_the_rotation_first(self):
        set_rotation(((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
        LIB.xem_gte_write_control(0, pair(0x2000, 0))  # R11 = 2.0
        mem.write(BUF, svec(0x6000, 1, 1))
        LIB.arena_gte_rotate_scale_svector(BUF, BUF + 8, 0x800)
        # MAC1 = 0xC000 goes back into IR1 as its low halfword (-0x4000),
        # then GPF halves it.
        self.assertEqual(struct.unpack("<3h", mem.read(BUF + 8, 6)), (-0x2000, 0, 0))


class MatrixColumns(unittest.TestCase):
    def setUp(self):
        mem.clear()
        gte_clear()

    def test_columns_scale_in_place(self):
        # Needs the GTE to latch the IR vector before an MVMVA with v=3
        # (each row from the inputs, not the rows already written to IR).
        rows = ((0x1000, 0x200, -0x300), (0x400, 0x7000, 0x10), (-0x1000, 0x20, 0x1800))
        words = struct.pack("<9hh3i", *(v for r in rows for v in r), 0x4242, 11, 22, 33)
        mem.write(BUF, words)
        mem.write(BUF + 0x40, struct.pack("<3h", 0x800, 0x2000, -0x1000))
        LIB.arena_gte_scale_matrix_columns(BUF, BUF + 0x40)
        scales = (0x800, 0x2000, -0x1000)
        expect = []
        for r in rows:
            for c, v in enumerate(r):
                expect.append(max(-0x8000, min(0x7FFF, (v * scales[c]) >> 12)))
        got = struct.unpack("<9hh3i", mem.read(BUF, 32))
        self.assertEqual(list(got[:9]), expect)
        self.assertEqual(got[9:], (0x4242, 11, 22, 33))
        # The GTE rotation is the unscaled matrix (R33 reads back sign
        # extended, without the padding); IR1-3 the last column.
        original = struct.unpack("<5I", words[:20])
        self.assertEqual([LIB.xem_gte_read_control(r) for r in range(5)],
                         list(original[:4]) + [s16v(original[4]) & M32])
        self.assertEqual([s16v(LIB.xem_gte_read_data(r)) for r in (9, 10, 11)], expect[2::3])

    def test_scales_inside_the_matrix_are_read_before_stores(self):
        rows = ((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000))
        mem.write(BUF, struct.pack("<9h", *(v for r in rows for v in r)))
        # The scales alias m[0][0..2]: (0x1000, 0, 0) read before any store.
        LIB.arena_gte_scale_matrix_columns(BUF, BUF)
        self.assertEqual(struct.unpack("<9h", mem.read(BUF, 18)), (0x1000, 0, 0, 0, 0, 0, 0, 0, 0))


def rgb(r, g, b, high=0):
    return r | g << 5 | b << 10 | high << 15


class BoxFilter(unittest.TestCase):
    def setUp(self):
        mem.clear()
        gte_clear()

    def test_quarter_sums_in_place(self):
        random.seed(5)
        upper = [rgb(random.randrange(32), random.randrange(32), random.randrange(32), random.randrange(2))
                 for _ in range(6)]
        lower = [rgb(random.randrange(32), random.randrange(32), random.randrange(32), 1) for _ in range(6)]
        mem.put16(BUF, *upper)
        mem.put16(BUF + 0x280, *lower)
        LIB.arena_box_filter_rgb555(BUF, BUF + 10)

        def channel(c, shift):
            return (c >> shift) & 31

        expect = []
        for i in range(4):
            px = 0
            for shift in (0, 5, 10):
                total = sum(channel(c, shift) for c in (upper[i], upper[i + 1], lower[i], lower[i + 1]))
                px |= (total >> 2) << shift
            expect.append(px)
        got = [mem.u16(BUF + 2 * i) for i in range(6)]
        self.assertEqual(got[:4], expect)
        self.assertEqual(got[4:], upper[4:])
        self.assertEqual([mem.u16(BUF + 0x280 + 2 * i) for i in range(6)], lower)
        # IR1-3 hold the last quarter sums (5-bit values << 7, >> 2 after adding four).
        last = [sum(channel(c, s) << 7 for c in (upper[3], upper[4], lower[3], lower[4])) >> 2 for s in (0, 5, 10)]
        self.assertEqual([LIB.xem_gte_read_data(r) for r in (9, 10, 11)], last)

    def test_single_window(self):
        mem.put16(BUF, rgb(31, 0, 4), rgb(31, 1, 4), 0x7FFF)
        mem.put16(BUF + 0x280, rgb(31, 2, 4), rgb(30, 3, 5))
        LIB.arena_box_filter_rgb555(BUF, BUF + 4)
        self.assertEqual(mem.u16(BUF), rgb(30, 1, 4))
        self.assertEqual(mem.u16(BUF + 2), rgb(31, 1, 4))


class CopyWords(unittest.TestCase):
    def setUp(self):
        mem.clear()

    def test_forward_word_copy(self):
        mem.put32(BUF, 1, 2, 3, 4, 5)
        LIB.arena_copy_words(BUF + 0x100, BUF, 16)
        self.assertEqual([mem.u32(BUF + 0x100 + 4 * i) for i in range(5)], [1, 2, 3, 4, 0])

    def test_overlap_copies_forward(self):
        mem.put32(BUF, 1, 2, 3, 4)
        LIB.arena_copy_words(BUF + 4, BUF, 12)
        self.assertEqual([mem.u32(BUF + 4 * i) for i in range(4)], [1, 1, 1, 1])

    def test_scratchpad_destination(self):
        mem.put32(BUF, 0xDEADBEEF, 0x01020304)
        LIB.arena_copy_words(0x1F800120, BUF, 8)
        self.assertEqual(mem.u32(0x1F800120), 0xDEADBEEF)
        self.assertEqual(mem.u32(0x1F800124), 0x01020304)


# Mesh fixtures.
VERTS = 0x80120000
RECORDS = 0x80130000
PACKETS = 0x80140000
OT = 0x80150000


def mesh_state(packet=PACKETS, drawn=7, y_limit=(240 - 1) << 16, x_limit=320):
    mem.put32(S["model_current_vertices"], VERTS)
    mem.put32(S["model_drawn_primitive_count"], drawn)
    mem.put32(S["model_current_packet"], packet)
    mem.put32(S["model_ot"], OT)
    mem.put32(S["model_screen_y_limit"], y_limit)
    mem.put32(S["model_screen_x_limit"], x_limit)
    mem.put32(OT, 0x00FFFFFF)


def records(faces):
    data = b""
    for face in faces:
        a, b, c, d = (list(face) + [0])[:4]
        data += struct.pack("<4H", a, b, c, d)
    mem.write(RECORDS, data)


class MeshPackets(Compare):
    def setUp(self):
        mem.clear()
        gte_clear()
        set_projection(tr=(0, 0, 0x200))
        random.seed(11)
        verts = b"".join(svec(random.randrange(-300, 300), random.randrange(-300, 300), random.randrange(-200, 200))
                         for _ in range(64))
        mem.write(VERTS, verts)

    def random_faces(self, n, quads):
        return [tuple(random.randrange(64) for _ in range(4 if quads else 3)) for _ in range(n + 1)]

    def test_triangles_match_the_original(self):
        mesh_state()
        records(self.random_faces(40, False))
        self.assertSameEffects(lambda: LIB.arena_mesh_draw_flat_triangles(RECORDS, 40),
                               lambda: asm_mesh_triangles(RECORDS, 40))
        self.assertEqual(mem.u32(S["model_current_packet"]), (PACKETS + 40 * 20) & 0x00FFFFFF)

    def test_quads_match_the_original(self):
        mesh_state()
        records(self.random_faces(40, True))
        self.assertSameEffects(lambda: LIB.arena_mesh_draw_flat_quads(RECORDS, 40),
                               lambda: asm_mesh_quads(RECORDS, 40))
        # Every quad is counted, culled or not.
        self.assertEqual(mem.u32(S["model_drawn_primitive_count"]), 7 + 40)
        self.assertEqual(mem.u32(S["model_current_packet"]), (PACKETS + 40 * 24) & 0x00FFFFFF)

    def test_tight_bounds_match_the_original(self):
        for limits in (((60 << 16), 40), (0, 320), ((240 << 16), 0), (0xFFFF0000, 0x10000)):
            for quads in (False, True):
                with self.subTest(limits=limits, quads=quads):
                    mem.clear()
                    self.setUp()
                    mesh_state(y_limit=limits[0], x_limit=limits[1])
                    records(self.random_faces(30, quads))
                    if quads:
                        self.assertSameEffects(lambda: LIB.arena_mesh_draw_flat_quads(RECORDS, 30),
                                               lambda: asm_mesh_quads(RECORDS, 30))
                    else:
                        self.assertSameEffects(lambda: LIB.arena_mesh_draw_flat_triangles(RECORDS, 30),
                                               lambda: asm_mesh_triangles(RECORDS, 30))

    def test_count_zero_projects_the_first_face(self):
        mesh_state()
        records([(1, 2, 3)])
        LIB.arena_mesh_draw_flat_triangles(RECORDS, 0)
        self.assertEqual(mem.u32(S["model_current_packet"]), PACKETS & 0x00FFFFFF)
        self.assertEqual(mem.u32(S["model_drawn_primitive_count"]), 7)
        self.assertEqual(mem.u32(OT), 0x00FFFFFF)
        # V0-V2 hold face 0's vertices; SZ3 its third vertex's depth.
        for i, reg in enumerate((0, 2, 4)):
            self.assertEqual(LIB.xem_gte_read_data(reg), mem.u32(VERTS + 8 * (i + 1)))
        z = mem.s16(VERTS + 8 * 3 + 4)
        self.assertEqual(LIB.xem_gte_read_data(19), max(0, z + 0x200))

    def test_hand_built_triangle_and_counter(self):
        # Vertices at depth 0x200 project 1:2 around (160, 120) with H = 256.
        mem.write(VERTS, svec(0, 0, 0) + svec(100, 0, 0) + svec(0, 100, 0) + svec(-100, 0, 0))
        mesh_state(packet=PACKETS)
        # Face 0 front-facing, face 1 its reverse (back face), then the extra.
        records([(0, 1, 2), (0, 2, 1), (0, 1, 2)])
        LIB.arena_mesh_draw_flat_triangles(RECORDS, 2)
        dma = PACKETS & 0x00FFFFFF
        self.assertEqual(mem.u32(PACKETS + 8), pair(160, 120))
        self.assertEqual(mem.u32(PACKETS + 12), pair(210, 120))
        self.assertEqual(mem.u32(PACKETS + 16), pair(160, 170))
        self.assertEqual(mem.u32(OT), dma)
        self.assertEqual(mem.u32(PACKETS), 0x04FFFFFF)
        # The back face is counted (after the y test) but neither written nor linked.
        self.assertEqual(mem.u32(S["model_drawn_primitive_count"]), 9)
        self.assertEqual(mem.read(PACKETS + 20, 20), bytes(20))
        self.assertEqual(mem.u32(S["model_current_packet"]), dma + 40)

    def test_quad_fourth_point_and_link_order(self):
        mem.write(VERTS, svec(0, 0, 0) + svec(100, 0, 0) + svec(0, 100, 0) + svec(100, 100, 0))
        mesh_state()
        records([(0, 1, 2, 3), (0, 1, 2, 3), (0, 1, 2, 3)])
        LIB.arena_mesh_draw_flat_quads(RECORDS, 2)
        dma = PACKETS & 0x00FFFFFF
        self.assertEqual([mem.u32(PACKETS + 8 + 4 * i) for i in range(4)],
                         [pair(160, 120), pair(210, 120), pair(160, 170), pair(210, 170)])
        # The second quad was prepended at the one OT slot ahead of the first.
        self.assertEqual(mem.u32(OT), dma + 24)
        self.assertEqual(mem.u32(PACKETS + 24), dma | 0x05000000)
        self.assertEqual(mem.u32(PACKETS), 0x05FFFFFF)


GROUND_POOL = 0x80160000
HEIGHTS = 0x80170000
GROUND_OT = 0x80180000


class Ground(Compare):
    def setUp(self):
        mem.clear()
        gte_clear()
        set_rotation(((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
        # A camera looking down +z; heights move y.
        for reg, value in zip((5, 6, 7), (0, 0x40, 0x600)):
            LIB.xem_gte_write_control(reg, value)
        LIB.xem_gte_write_control(24, 160 << 16)
        LIB.xem_gte_write_control(25, 120 << 16)
        LIB.xem_gte_write_control(26, 0x100)
        LIB.xem_gte_write_control(27, (-0x80) & 0xFFFF)
        LIB.xem_gte_write_control(28, 0x1400000)
        for reg, value in zip((21, 22, 23), (0x100, 0x200, 0x300)):
            LIB.xem_gte_write_control(reg, value)
        LIB.xem_gte_write_data(6, 0x24806040)  # RGBC: code 0x24
        mem.put32(S["arena_stage_ground_triangles"], GROUND_POOL, GROUND_POOL + 0xE100)
        mem.write(S["arena_draw_buffer_index"], b"\x01")
        mem.put32(S["arena_stage_height_map"], HEIGHTS)
        mem.put32(S["model_submitted_primitive_count"], 100)
        mem.put32(S["model_drawn_primitive_count"], 50)
        # MapTable: four UV orientations of four halfwords, four tpage/CLUT words.
        mem.put16(0x1F800120, *[0x0000 + i for i in range(16)])
        mem.put32(0x1F800140, 0x7A000011, 0x7B000022, 0x7C000033, 0x7D000044)
        mem.fill(0x1F800080, 0x80, 0xFF)
        random.seed(3)
        heights = b"".join(struct.pack("<hH", random.randrange(-0x80, 0x80), random.randrange(0x10000))
                           for _ in range(128 * 128))
        mem.write(HEIGHTS, heights)
        for i in range(0x1000):
            mem.put32(GROUND_OT + 4 * i, 0x00FFFFFF)

    def spans(self, rows):
        for row, (left, right) in rows.items():
            mem.write(0x1F800000 + row, bytes([right]))
            mem.write(0x1F800080 + row, bytes([left]))

    def test_cells_match_the_original(self):
        self.spans({0: (2, 6), 1: (0, 3), 2: (5, 5), 3: (7, 4), 4: (1, 0), 5: (0xFF, 9), 126: (60, 64), 127: (0, 9)})
        count = self.assertSameEffects(lambda: LIB.arena_stage_draw_ground_cells(GROUND_OT, 0x300, 0x80),
                                       lambda: asm_ground(GROUND_OT, 0x300, 0x80))
        self.assertGreater(count, 0)
        # Spans of rows 0, 1 and 126 count their cells (4 + 3 + 4); row 127 is never read.
        self.assertEqual(mem.u32(S["model_submitted_primitive_count"]), 111)
        self.assertEqual(mem.u32(S["model_drawn_primitive_count"]), 50 + count)
        # The pool's stored base is kept.
        self.assertEqual(mem.u32(S["arena_stage_ground_triangles"] + 4), GROUND_POOL + 0xE100)

    def test_first_packet_layout(self):
        # One cell, flat, front-facing for both triangles.
        mem.write(HEIGHTS, struct.pack("<hH", 0, 0x12F5) + struct.pack("<hH", 0, 0))
        mem.write(HEIGHTS + 0x200, struct.pack("<hH", 0, 0) + struct.pack("<hH", 0, 0))
        self.spans({0: (0, 1)})
        count = LIB.arena_stage_draw_ground_cells(GROUND_OT, 0, 0)
        pool = GROUND_POOL + 0xE100
        corners = [mem.s16(0x1F800100 + 2 * i) for i in range(16)]
        # Upper left, lower right, upper right, lower left; pads untouched.
        self.assertEqual(corners, [0, 0, 0, 0, 0x100, 0, 0x100, 0, 0x100, 0, 0, 0, 0, 0, 0x100, 0])
        flags = 0x12F5
        nibbles = flags & 0xF0F0
        uv = 0x1F800120 + (flags & 3) * 8
        tpage_clut = mem.u32(0x1F800140 + (flags & 12))
        packets = [p for p in (pool, pool + 32) if mem.u32(p) != 0]
        self.assertEqual(len(packets), count)
        if count:
            first = pool
            self.assertEqual(mem.u32(first + 12), nibbles | mem.u16(uv) | (tpage_clut & 0xFFFF0000))
            self.assertEqual(mem.u32(first + 20), nibbles | mem.u16(uv + 6) | ((tpage_clut << 16) & M32))
            self.assertEqual(mem.u16(first + 28), nibbles | mem.u16(uv + 2))
            self.assertEqual(mem.u32(first) >> 24, 7)
            self.assertEqual(mem.u32(first + 4) >> 24, 0x24)
        # The OT holds 24-bit packet addresses.
        linked = [mem.u32(GROUND_OT + 4 * i) for i in range(0x1000) if mem.u32(GROUND_OT + 4 * i) != 0x00FFFFFF]
        self.assertTrue(all(v < 0x01000000 for v in linked))

    def test_empty_spans_draw_nothing(self):
        self.spans({0: (0, 0), 1: (0xFF, 5), 2: (4, 4), 3: (5, 2)})
        self.assertEqual(LIB.arena_stage_draw_ground_cells(GROUND_OT, 0, 0), 0)
        self.assertEqual(mem.u32(S["model_submitted_primitive_count"]), 100)
        self.assertEqual(mem.u32(S["model_drawn_primitive_count"]), 50)


WORK = 0x80190000


class Shadow(unittest.TestCase):
    def setUp(self):
        mem.clear()
        gte_clear()
        set_rotation(((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
        reset_breaks()

    def project(self, vertices, light, work=None):
        mem.write(VERTS, b"".join(svec(*v) for v in vertices) + svec(1, 2, 3))
        mem.put32(S["arena_mesh_light_direction"], *light)
        if work is None:
            work = [(0x1111, 0x2222, 0x3333, 0x4444)] * len(vertices)
        mem.write(WORK, b"".join(struct.pack("<4h", *w) for w in work))
        LIB.arena_mesh_project_shadow(VERTS, WORK, len(vertices))
        return [struct.unpack("<4h", mem.read(WORK + 8 * i, 8)) for i in range(len(vertices))]

    def expected(self, v, light, keep):
        x, y, z = v
        lx, ly, lz = light
        d = s32(ly - y)
        if d == 0:
            return keep

        def div(n):
            n = s32(n)
            q = abs(n) // abs(d)
            return q if (n < 0) == (d < 0) else -q

        vx = div((x * ly - lx * y) & M32)
        vz = div((-((-z * ly) + lz * y)) & M32)
        return (s16v(vx), keep[1], s16v(vz), keep[3])

    def test_projection_and_parallel_rays(self):
        light = (0x100, -0x1000, 0x80)
        verts = [(100, -200, 300), (-50, 0, 20), (7, -0x1000, 9), (0x7FFF, 0x7FFF, -0x8000)]
        got = self.project(verts, light)
        keep = (0x1111, 0x2222, 0x3333, 0x4444)
        self.assertEqual(got, [self.expected(v, light, keep) for v in verts])
        # The third vertex lies on L.y: its record is untouched.
        self.assertEqual(got[2], keep)
        # V0 holds the input after the last.
        self.assertEqual(LIB.xem_gte_read_data(0), pair(1, 2))
        self.assertEqual(breaks(), [])

    def test_products_wrap_to_32_bits(self):
        light = (0x7FFFFFFF, 0x40000, -0x7FFFFFFF)
        verts = [(0x7FFF, -0x8000, 0x7FFF), (-0x8000, 0x7FFF, -0x8000)]
        got = self.project(verts, light)
        keep = (0x1111, 0x2222, 0x3333, 0x4444)
        self.assertEqual(got, [self.expected(v, light, keep) for v in verts])

    def test_division_overflow_traps_and_continues(self):
        # L.y - y = -1 and x*L.y - L.x*y wraps to 0x80000000: break 6, and
        # the quotient is the dividend.
        got = self.project([(0, 2, 0)], (0x40000000, 1, 0))
        self.assertEqual(breaks(), [6])
        self.assertEqual(got[0][0], 0)
        self.assertEqual(got[0][2], 0)


if __name__ == "__main__":
    unittest.main()
