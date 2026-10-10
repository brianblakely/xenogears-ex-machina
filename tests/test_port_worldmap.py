"""The port's world map terrain renderers (port/worldmap_terrain.c) against
the contracts of decomp/src/worldmap/worldmap_terrain_draw_quarter_block.s,
worldmap_billboards_draw_block.s and screen_bounds.s, natively
(tests/port_native.py), on invented inputs.

Two kinds of checks: hand-worked scenes whose projections are exact (identity
rotation, H equal to every depth, so SX = x + OFX and SY = y + OFY) with the
expected packet words written out, and random scenes compared with a model
of the .s contracts written here in Python. The model makes the original's
GTE transfers and commands in its order on the library's software GTE (which
tests/test_port_gte.py checks on its own) and its memory accesses on the
mapped game memory; the C then runs from the same starting state, and game
memory and the whole GTE state must come out identical.
"""

import random
import struct
import tempfile
import unittest

from tests import port_native as mem

SCRATCHPAD = mem.SCRATCHPAD
# The original addresses of the globals the renderers use.
SYMBOLS = {
    "worldmap_terrain_packet_count": 0x8009D7DC,
    "worldmap_billboard_quad_count": 0x8009BE04,
    "worldmap_camera": 0x8009BE28,
    "worldmap_area_blocks_x": 0x8009D160,
    "worldmap_area_blocks_z": 0x8009D2B4,
}
PACKET_COUNT = SYMBOLS["worldmap_terrain_packet_count"]
QUAD_COUNT = SYMBOLS["worldmap_billboard_quad_count"]
CAMERA = SYMBOLS["worldmap_camera"]

# Invented buffers in the mapped RAM.
CELLS = 0x80100000
OT = 0x80110000
PACKETS = 0x80120000
POSITIONS = 0x80180000

LIB = None
BUILD = None

# Data and control register numbers used here.
VXY0, SXY0, SXY1, SXY2, IR0, SZ1, SZ2, SZ3, MAC0, MAC1, MAC2, MAC3 = 0, 12, 13, 14, 8, 17, 18, 19, 24, 25, 26, 27
TRX, TRY, TRZ, OFX, OFY, H, DQA, DQB, FLAG = 5, 6, 7, 24, 25, 26, 27, 28, 31
RTPT, RTPS, NCLIP = 0x0280030, 0x0180001, 0x1400006
MVMVA_RT_V0_NONE = 0x0400012 | 1 << 19 | 3 << 13


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/worldmap_terrain.c"], SYMBOLS)


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def s32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def xy(x, y):
    return (x & 0xFFFF) | (y & 0xFFFF) << 16


def svector(x, y, z):
    return struct.pack("<hhhh", s16(x), s16(y), s16(z), 0)


def d(reg, value=None):
    if value is None:
        return LIB.xem_gte_read_data(reg)
    LIB.xem_gte_write_data(reg, value & 0xFFFFFFFF)


def c(reg, value=None):
    if value is None:
        return LIB.xem_gte_read_control(reg)
    LIB.xem_gte_write_control(reg, value & 0xFFFFFFFF)


def matrix_words(m):
    """The five rotation words of a 3x3 matrix of s16."""
    flat = [v & 0xFFFF for row in m for v in row]
    return [flat[0] | flat[1] << 16, flat[2] | flat[3] << 16, flat[4] | flat[5] << 16, flat[6] | flat[7] << 16, flat[8]]


IDENTITY = [[0x1000, 0, 0], [0, 0x1000, 0], [0, 0, 0x1000]]


def exact_projection(h=0x100, ofx=160, ofy=8, dqb=0x300):
    """Identity rotation, no translation, H = h, depth cue IR0 = dqb."""
    LIB.xem_gte_reset()
    for i, w in enumerate(matrix_words(IDENTITY)):
        c(i, w)
    c(OFX, ofx << 16)
    c(OFY, ofy << 16)
    c(H, h)
    c(DQA, 0)
    c(DQB, dqb << 12)


def ram(address):
    """A PS1 RAM address in any segment or mirror, at its KSEG0 address."""
    return 0x80000000 | (address & 0x1FFFFF)


def on_screen(sxy):
    """screen_bounds_test."""
    if all(v & 0xFFFF >= 320 for v in sxy):
        return False
    if all((s32(v) >> 16) & 0xFFFFFFFF >= 216 for v in sxy):
        return False
    return True


# ------------------------------------------------------------ terrain model


def terrain_model(cells, ot, packets):
    """worldmap_terrain_draw_quarter_block.s, step by step."""
    count = mem.u32(PACKET_COUNT)
    vertex = SCRATCHPAD
    packet = packets

    def load(first, address):
        d(first, mem.u32(address))
        d(first + 1, mem.u32(address + 4))

    def triangle(word, uv0, uv1, uv2):
        nonlocal packet, count
        LIB.xem_gte_execute(RTPT)
        if c(FLAG) & 0x80000000:
            return "flag"
        sxy = [d(SXY0), d(SXY1), d(SXY2)]
        if not on_screen(sxy):
            return "reject"
        depth = max(d(SZ1), d(SZ2), d(SZ3))
        if depth >= 0xF00:
            return "reject"
        LIB.xem_gte_execute(NCLIP)
        if s32(d(MAC0)) <= 0:
            return "reject"
        cue = d(IR0)
        if cue >= 0x1000:
            cue = 0xFFF
        page = mem.u16(SCRATCHPAD + 0x308 + ((word >> 8) & 7) * 2)
        bank = 32 if word & 0x800 else 0
        clut = mem.u16(SCRATCHPAD + 0x288 + ((cue >> 7) + bank) * 2)
        entry = ot + (depth >> 4) * 4
        previous = mem.u32(entry)
        count += 1
        mem.put32(ram(packet + 8), sxy[0])
        mem.put32(ram(packet + 16), sxy[1])
        mem.put32(ram(packet + 24), sxy[2])
        mem.put16(ram(packet + 28), uv2)
        mem.put32(ram(packet + 12), clut << 16 | uv0)
        mem.put32(ram(packet + 20), page << 16 | uv1)
        mem.put32(ram(packet), 0x07000000 | previous)
        mem.put32(entry, packet & 0xFFFFFF)
        packet += 32
        return "drawn"

    for row in range(8):
        for column in range(8):
            word = mem.u32(cells + (row * 9 + column) * 4)
            base = ((word >> 16) & 0xF) * 16 | ((word >> 20) & 0xF) * 16 << 8
            uv = {"tl": base, "tr": base + 0xF, "bl": base + 0xF00, "br": base + 0xF0F}
            if word & 0x2000:  # flip u
                uv = {"tl": uv["tr"], "tr": uv["tl"], "bl": uv["br"], "br": uv["bl"]}
            if word & 0x4000:  # flip v
                uv = {"tl": uv["bl"], "tr": uv["br"], "bl": uv["tl"], "br": uv["tr"]}
            if count >= 0x7FE:
                mem.put32(PACKET_COUNT, count)
                return
            at = vertex + (row * 9 + column) * 8
            tl, tr, bl, br = at, at + 8, at + 0x48, at + 0x50
            load(0, tl)
            load(4, bl)
            diagonal = word & 0x8000
            load(2, br if diagonal else tr)
            if triangle(word, uv["tl"], uv["br"] if diagonal else uv["tr"], uv["bl"]) != "flag":
                load(0, tr)
                load(2, br)
                load(4, tl if diagonal else bl)
                triangle(word, uv["tr"], uv["br"], uv["tl"] if diagonal else uv["bl"])
    mem.put32(PACKET_COUNT, count)


# ------------------------------------------------------------ billboard model


def billboards_model(data, count, ot, quads):
    """worldmap_billboards_draw_block.s, step by step."""
    drawn = mem.u32(QUAD_COUNT)
    camera_x = s32(mem.u32(CAMERA)) >> 12
    camera_z = s32(mem.u32(CAMERA + 8)) >> 12
    width = s32(mem.u32(SYMBOLS["worldmap_area_blocks_x"]) << 11)
    height = s32(mem.u32(SYMBOLS["worldmap_area_blocks_z"]) << 11)
    left = count & 0xFFFFFFFF
    while s32(drawn) < 512:
        word = mem.u32(data)
        y = s32(word) >> 16
        dx = s32(s16(word) - camera_x)
        dz = s32(mem.s16(data + 4) - camera_z)
        if dx < -0x4000:
            dx = s32(dx + width)
        if dx >= 0x4000:
            dx = s32(dx - width)
        if dz < -0x4000:
            dz = s32(dz + height)
        if dz >= 0x4000:
            dz = s32(dz - height)
        for i in range(5):
            c(i, mem.u32(SCRATCHPAD + 0x28 + i * 4))
        d(0, xy(dx, y))
        d(1, -dz & 0xFFFF)
        LIB.xem_gte_execute(MVMVA_RT_V0_NONE)
        t = [d(MAC1 + i) + mem.u32(SCRATCHPAD + 0x3C + i * 4) for i in range(3)]
        for i in range(5):
            c(i, mem.u32(SCRATCHPAD + 0x48 + i * 4))
        for i in range(3):
            c(TRX + i, t[i])
        for i in range(6):
            d(i, mem.u32(SCRATCHPAD + i * 4))
        LIB.xem_gte_execute(RTPT)
        if not c(FLAG) & 0x80000000:
            sxy = [d(SXY0), d(SXY1), d(SXY2)]
            depth = d(SZ3)
            if on_screen(sxy) and depth < 0xE00:
                d(0, mem.u32(SCRATCHPAD + 24))
                d(1, mem.u32(SCRATCHPAD + 28))
                LIB.xem_gte_execute(RTPS)
                cue = min(d(IR0), 0xFFF) if d(IR0) < 0x80000000 else 0xFFF
                entry = ot + (depth >> 4) * 4
                previous = mem.u32(entry)
                clut = mem.u16(SCRATCHPAD + 0x68 + (cue >> 8) * 2)
                for i, v in enumerate(sxy + [d(SXY2)]):
                    mem.put32(quads + 8 + i * 8, v)
                mem.put32(entry, quads & 0xFFFFFF)
                mem.put32(quads, previous | 0x09000000)
                mem.put16(quads + 14, clut)
                quads += 40
                drawn = (drawn + 1) & 0xFFFFFFFF
        data += 8
        left = (left - 1) & 0xFFFFFFFF
        if left == 0:
            break
    mem.put32(QUAD_COUNT, drawn)


# ------------------------------------------------------------ helpers


def state():
    return mem.read(mem.RAM, mem.RAM_BYTES), mem.read(SCRATCHPAD, mem.SCRATCHPAD_BYTES), mem.gte_snapshot(LIB)


def restore(saved):
    ram, scratch, gte = saved
    mem.write(mem.RAM, ram)
    mem.write(SCRATCHPAD, scratch)
    mem.gte_restore(LIB, gte)


class Fixture(unittest.TestCase):
    def setUp(self):
        mem.clear()
        LIB.xem_gte_reset()

    def assertSameAsModel(self, model, run):
        """Run the model, then the C from the same state; whether RAM changed."""
        start = state()
        model()
        expected = state()
        restore(start)
        run()
        actual = state()
        self.assertEqual(expected[2], actual[2], "GTE state")
        for name, base, want, got in (("scratchpad", SCRATCHPAD, expected[1], actual[1]),
                                      ("RAM", mem.RAM, expected[0], actual[0])):
            if want != got:
                first = next(i for i in range(len(got)) if got[i] != want[i])
                self.fail(f"{name} differs first at {base + first:#x}: "
                          f"{want[first:first + 16].hex()} expected, {got[first:first + 16].hex()} written")
        return start[0] != actual[0]


# ------------------------------------------------------------ terrain tests


def terrain_tables():
    for i in range(64):
        mem.put16(SCRATCHPAD + 0x288 + i * 2, 0x7000 + i)
    for i in range(8):
        mem.put16(SCRATCHPAD + 0x308 + i * 2, 0x0100 + i)


def terrain_grid(x0=0, y0=0, z=0x100, x_step=16):
    for i in range(9):
        for j in range(9):
            mem.write(SCRATCHPAD + (i * 9 + j) * 8, svector(x0 + j * x_step, y0 + i * 16, z))


def cell_word(column, row, page=0, bank=0, flip_u=0, flip_v=0, diagonal=0, low=0):
    return column << 16 | row << 20 | page << 8 | bank << 11 | flip_u << 13 | flip_v << 14 | diagonal << 15 | low


class TerrainHandWorked(Fixture):
    def setUp(self):
        super().setUp()
        terrain_tables()
        terrain_grid()
        exact_projection()
        # Preset colour/code words and a marker in each uv2 word's upper half.
        for k in range(130):
            mem.put32(PACKETS + k * 32 + 4, 0x24808080)
            mem.put32(PACKETS + k * 32 + 28, 0xABCD0000)
        mem.put32(OT + 0x10 * 4, 0x00FFFFFF)

    def draw(self, words):
        for i, w in enumerate(words):
            mem.put32(CELLS + i * 4, w)
        LIB.worldmap_terrain_draw_quarter_block(CELLS, OT, PACKETS)

    def packet(self, k):
        return struct.unpack("<8I", mem.read(PACKETS + k * 32, 32))

    def test_every_cell_on_screen_draws_two_triangles_linked_at_depth(self):
        words = [cell_word(3, 5, page=2)] * 72
        self.draw(words)
        self.assertEqual(mem.u32(PACKET_COUNT), 128)
        # Cell (0, 0): (TL, TR, BL), then (TR, BR, BL); SX = x + 160, SY = y + 8.
        uv = 0x30 | 0x50 << 8
        first = self.packet(0)
        self.assertEqual(first, (0x07FFFFFF, 0x24808080, xy(160, 8), 0x7006 << 16 | uv, xy(176, 8),
                                 0x0102 << 16 | uv + 0xF, xy(160, 24), 0xABCD0000 | uv + 0xF00))
        second = self.packet(1)
        self.assertEqual(second, (0x07000000 | (PACKETS & 0xFFFFFF), 0x24808080, xy(176, 8), 0x7006 << 16 | uv + 0xF,
                                  xy(176, 24), 0x0102 << 16 | uv + 0xF0F, xy(160, 24), 0xABCD0000 | uv + 0xF00))
        # Cell (7, 7) is the last; the ninth cell word and vertex are skipped.
        last = self.packet(127)
        self.assertEqual(last[2], xy(160 + 7 * 16 + 16, 8 + 7 * 16))
        self.assertEqual(mem.u32(OT + 0x10 * 4), (PACKETS + 127 * 32) & 0xFFFFFF)
        self.assertEqual(mem.u32(PACKETS + 128 * 32 + 4), 0x24808080)

    def test_flips_diagonal_page_and_clut_bank(self):
        words = [cell_word(1, 2, page=7, bank=1, flip_u=1, flip_v=1, diagonal=1)] + [cell_word(0, 0)] * 71
        self.draw(words)
        uv = 0x10 | 0x20 << 8
        tl, tr, bl, br = uv + 0xF0F, uv + 0xF00, uv + 0xF, uv
        first = self.packet(0)  # (TL, BR, BL)
        self.assertEqual(first[2:], (xy(160, 8), 0x7026 << 16 | tl, xy(176, 24), 0x0107 << 16 | br,
                                     xy(160, 24), 0xABCD0000 | bl))
        second = self.packet(1)  # (TR, BR, TL)
        self.assertEqual(second[2:], (xy(176, 8), 0x7026 << 16 | tr, xy(176, 24), 0x0107 << 16 | br,
                                      xy(160, 8), 0xABCD0000 | tl))

    def test_packet_limit_is_checked_before_each_cell(self):
        mem.put32(PACKET_COUNT, 0x7FE - 3)
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(mem.u32(PACKET_COUNT), 0x7FF)  # two cells, the second passing 0x7FE
        self.assertEqual(self.packet(4)[0], 0)
        self.assertEqual(self.packet(2)[2], xy(176, 8))  # cell (1, 0)
        self.assertEqual(self.packet(3)[2], xy(192, 8))

    def test_depth_cue_limit_and_depth_limit(self):
        # IR0 0x1000 is limited to 0xFFF: CLUT 31.
        exact_projection(dqb=0x1000)
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(self.packet(0)[3] >> 16, 0x7000 + 31)
        mem.clear()
        terrain_tables()
        # Every vertex at depth 0xF00 (H too, so the screen stays exact): rejected.
        terrain_grid(z=0xF00)
        exact_projection(h=0xF00)
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(mem.u32(PACKET_COUNT), 0)
        terrain_grid(z=0xEFF)
        exact_projection(h=0xEFF)
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(mem.u32(PACKET_COUNT), 128)
        self.assertEqual(mem.u32(OT + 0xEF * 4), (PACKETS + 127 * 32) & 0xFFFFFF)

    def test_bounds_take_any_vertex_x_and_any_vertex_y(self):
        # Shift the grid so the first column's x is 304..320 and y runs from
        # -8: cell (0, 0)'s first triangle keeps x 304 and y 0 at TL.
        terrain_grid(x0=144, y0=-8)
        self.draw([cell_word(0, 0)] * 72)
        count = mem.u32(PACKET_COUNT)
        # Columns 0 (x 304, 320) only: the other columns lie at x >= 320.
        self.assertEqual(count, 16)
        self.assertEqual(self.packet(0)[2], xy(304, 0))

    def test_back_faces_are_culled(self):
        # Mirror x: every triangle winds the other way.
        terrain_grid(x0=128, x_step=-16)
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(mem.u32(PACKET_COUNT), 0)
        self.assertEqual(self.packet(0), (0, 0x24808080, 0, 0, 0, 0, 0, 0xABCD0000))

    def test_flag_error_on_the_first_triangle_skips_the_second(self):
        # Cell (0, 0)'s TL behind the projection plane: the first triangle
        # (TL, TR, BL) has a divide overflow, so (TR, BR, BL) is not drawn.
        mem.write(SCRATCHPAD, svector(0, 0, 0x10))
        self.draw([cell_word(0, 0)] * 72)
        self.assertEqual(mem.u32(PACKET_COUNT), 126)
        self.assertEqual(self.packet(0)[2], xy(176, 8))  # cell (1, 0)


class TerrainAgainstModel(Fixture):
    def test_random_scenes(self):
        rng = random.Random(1)
        drew = 0
        for scene in range(40):
            with self.subTest(scene=scene):
                mem.clear()
                LIB.xem_gte_reset()
                for i in range(64):
                    mem.put16(SCRATCHPAD + 0x288 + i * 2, rng.getrandbits(16))
                for i in range(8):
                    mem.put16(SCRATCHPAD + 0x308 + i * 2, rng.getrandbits(16))
                for i in range(81):
                    x = rng.randint(-200, 200) + (i % 9) * 30
                    y = rng.randint(-150, 150) + (i // 9) * 25
                    mem.write(SCRATCHPAD + i * 8, svector(x, y, rng.choice([rng.randint(-50, 0x1200), 0x200])))
                for i in range(72):
                    mem.put32(CELLS + i * 4, rng.getrandbits(32))
                for i in range(0x100):
                    mem.put32(OT + i * 4, rng.getrandbits(32) if rng.random() < 0.2 else 0)
                angle = rng.uniform(-0.5, 0.5)
                import math
                ca, sa = int(math.cos(angle) * 4096), int(math.sin(angle) * 4096)
                for i, w in enumerate(matrix_words([[ca, 0, sa], [0, 0x1000, 0], [-sa, 0, ca]])):
                    c(i, w)
                c(TRX, rng.randint(-100, 100))
                c(TRY, rng.randint(-100, 100))
                c(TRZ, rng.randint(0, 0x300))
                c(OFX, 160 << 16)
                c(OFY, 112 << 16)
                c(H, rng.choice([0x100, 0x200, 0x300]))
                c(DQA, rng.randint(-0x400, 0))
                c(DQB, rng.randint(0, 0x1400000))
                mem.put32(PACKET_COUNT, rng.choice([0, 5, 0x7F0, 0x7FD, 0x7FE, 0x900]))
                # KSEG0, KUSEG (the DMA address) and KSEG1 buffers.
                packets = rng.choice([PACKETS, PACKETS + 0x20, PACKETS & 0xFFFFFF, PACKETS | 0x20000000])
                drew += self.assertSameAsModel(
                    lambda: terrain_model(CELLS, OT, packets),
                    lambda: LIB.worldmap_terrain_draw_quarter_block(CELLS, OT, packets),
                )
        self.assertGreater(drew, 25)


# ------------------------------------------------------------ billboard tests


CORNERS = [(-0x18, -0x48), (0x18, -0x48), (-0x18, 0), (0x18, 0)]


def billboard_scratch(view=IDENTITY, view_t=(0, 0, 0), roll=IDENTITY):
    for i, (x, y) in enumerate(CORNERS):
        mem.write(SCRATCHPAD + i * 8, svector(x, y, 0))
    mem.put32(SCRATCHPAD + 0x28, *matrix_words(view))
    mem.write(SCRATCHPAD + 0x3C, struct.pack("<3i", *view_t))
    mem.put32(SCRATCHPAD + 0x48, *matrix_words(roll))
    for i in range(16):
        mem.put16(SCRATCHPAD + 0x68 + i * 2, 0x7100 + i)


def camera(x, z):
    mem.write(CAMERA, struct.pack("<3i", x << 12, 0, z << 12))


def position(index, x, y, z):
    mem.write(POSITIONS + index * 8, svector(x, y, z))


class BillboardsHandWorked(Fixture):
    def setUp(self):
        super().setUp()
        billboard_scratch()
        exact_projection(h=0x100, ofx=160, ofy=120, dqb=0x300)
        camera(0x100, 0x200)
        mem.put32(SYMBOLS["worldmap_area_blocks_x"], 16)  # 0x8000 wide
        mem.put32(SYMBOLS["worldmap_area_blocks_z"], 8)  # 0x4000 high
        for k in range(8):
            mem.put32(QUADS_AT(k) + 4, 0x2C808080)
            mem.put32(QUADS_AT(k) + 12, 0x11220000 | 0x3344)

    def draw(self, count):
        LIB.worldmap_billboards_draw_block(POSITIONS, count, OT, PACKETS)

    def quad(self, k):
        return struct.unpack("<10I", mem.read(QUADS_AT(k), 40))

    def test_one_quad_projected_about_its_position(self):
        # dx = 0x10, y = 0x20, dz = -0x100: translation (0x10, 0x20, 0x100).
        position(0, 0x110, 0x20, 0x100)
        mem.put32(OT + 0x10 * 4, 0x00ABCDEF)
        self.draw(1)
        self.assertEqual(mem.u32(QUAD_COUNT), 1)
        q = self.quad(0)
        self.assertEqual(q[0], 0x09ABCDEF)
        self.assertEqual(q[1], 0x2C808080)
        self.assertEqual(q[2], xy(160 + 0x10 - 0x18, 120 + 0x20 - 0x48))
        self.assertEqual(q[3], 0x7103 << 16 | 0x3344)  # IR0 0x300: CLUT 3; uv kept
        self.assertEqual(q[4], xy(160 + 0x10 + 0x18, 120 + 0x20 - 0x48))
        self.assertEqual(q[6], xy(160 + 0x10 - 0x18, 120 + 0x20))
        self.assertEqual(q[8], xy(160 + 0x10 + 0x18, 120 + 0x20))
        self.assertEqual(mem.u32(OT + 0x10 * 4), PACKETS & 0xFFFFFF)
        # The roll matrix and the translation stay in the GTE.
        self.assertEqual([c(i) for i in range(8)], matrix_words(IDENTITY) + [0x10, 0x20, 0x100])

    def test_positions_wrap_once_around_the_map(self):
        for x, expected in ((0x100 - 0x5000, -0x5000 + 0x8000), (0x100 + 0x4000, 0x4000 - 0x8000),
                            (0x100 - 0x4000, -0x4000), (0x100 + 0x3FFF, 0x3FFF)):
            position(0, x, 0, 0x100)
            self.draw(1)
            self.assertEqual(s32(c(TRX)), expected, hex(x))
        # The bounds with a one-block map: -0x4000 is kept, below it gains.
        mem.put32(SYMBOLS["worldmap_area_blocks_x"], 1)
        for x, expected in ((0x100 - 0x4000, -0x4000), (0x100 - 0x4001, -0x4001 + 0x800),
                            (0x100 + 0x4000, 0x4000 - 0x800)):
            position(0, x, 0, 0x100)
            self.draw(1)
            self.assertEqual(s32(c(TRX)), expected, hex(x))
        mem.put32(SYMBOLS["worldmap_area_blocks_x"], 16)
        # Only once: dx = -0xE000 gains one width and stays below -0x4000.
        camera(0x7000, 0x200)
        position(0, -0x7000, 0, 0x100)
        self.draw(1)
        self.assertEqual(s32(c(TRX)), -0xE000 + 0x8000)
        camera(0x100, 0x200)
        for z, expected in ((0x200 + 0x5000, -(0x5000 - 0x4000)), (0x200 - 0x4000, 0x4000),
                            (0x200 - 0x4001, -(-0x4001 + 0x4000))):
            position(0, 0x100, 0, z)
            self.draw(1)
            self.assertEqual(s32(c(TRZ)), expected, hex(z))

    def test_depth_limit_and_cue_limit(self):
        # Corner depth = translation z = -dz.
        exact_projection(h=0xDFF, dqb=0x1000)
        position(0, 0x100, 0, 0x200 - 0xDFF)
        position(1, 0x100, 0, 0x200 - 0xE00)
        self.draw(2)
        self.assertEqual(mem.u32(QUAD_COUNT), 1)
        self.assertEqual(self.quad(0)[3] >> 16, 0x710F)  # IR0 0x1000 limited to 0xFFF
        self.assertEqual(mem.u32(OT + 0xDF * 4), PACKETS & 0xFFFFFF)
        self.assertEqual(self.quad(1)[0], 0)

    def test_quad_limit_and_its_word(self):
        for i in range(4):
            position(i, 0x100, 0, 0x100)
        mem.put32(QUAD_COUNT, 510)
        self.draw(4)
        self.assertEqual(mem.u32(QUAD_COUNT), 512)
        self.assertEqual(self.quad(2)[0], 0)
        # The count is a word: a nonzero halfword after the s16 stops it.
        mem.put32(QUAD_COUNT, 0x00010000)
        mem.put32(PACKETS, 0)
        self.draw(4)
        self.assertEqual(mem.u32(QUAD_COUNT), 0x00010000)
        self.assertEqual(self.quad(0)[0], 0)
        # ... and a negative word draws on and is stored back whole.
        mem.put32(QUAD_COUNT, 0xFFFF0000)
        self.draw(2)
        self.assertEqual(mem.u32(QUAD_COUNT), 0xFFFF0002)

    def test_a_count_of_zero_counts_down_from_two_to_the_32(self):
        for i in range(4):
            position(i, 0x100, 0, 0x100)
        mem.put32(QUAD_COUNT, 509)
        self.draw(0)
        self.assertEqual(mem.u32(QUAD_COUNT), 512)

    def test_skipped_positions_take_no_quad(self):
        position(0, 0x100 + 0x200, 0, 0x100)  # x off screen
        position(1, 0x100, 0, 0x200 + 0x10)  # behind: divide overflow
        position(2, 0x100, 0, 0x100)
        self.draw(3)
        self.assertEqual(mem.u32(QUAD_COUNT), 1)
        self.assertEqual(self.quad(0)[2], xy(160 - 0x18, 120 - 0x48))


def QUADS_AT(k):
    return PACKETS + k * 40


class BillboardsAgainstModel(Fixture):
    def test_random_scenes(self):
        import math

        rng = random.Random(2)
        drew = 0
        for scene in range(60):
            with self.subTest(scene=scene):
                mem.clear()
                LIB.xem_gte_reset()
                a, b = rng.uniform(-3.2, 3.2), rng.uniform(-0.6, 0.6)
                ca, sa, cb, sb = (int(f * 4096) for f in (math.cos(a), math.sin(a), math.cos(b), math.sin(b)))
                view = [[ca, 0, -sa], [sa * sb >> 12, cb, ca * sb >> 12], [sa * cb >> 12, -sb, ca * cb >> 12]]
                r = rng.uniform(-0.4, 0.4)
                cr, sr = int(math.cos(r) * 4096), int(math.sin(r) * 4096)
                billboard_scratch(view, (rng.randint(-50, 50), rng.randint(-50, 50), rng.randint(0, 0x400)),
                                  [[cr, -sr, 0], [sr, cr, 0], [0, 0, 0x1000]])
                for i in range(16):
                    mem.put16(SCRATCHPAD + 0x68 + i * 2, rng.getrandbits(16))
                c(OFX, 160 << 16)
                c(OFY, 112 << 16)
                c(H, 0x200)
                c(DQA, rng.randint(-0x300, 0))
                c(DQB, rng.randint(0, 0x1400000))
                mem.write(CAMERA, struct.pack("<3i", rng.randint(-1 << 27, 1 << 27), 0, rng.randint(-1 << 27, 1 << 27)))
                mem.put32(SYMBOLS["worldmap_area_blocks_x"], rng.randint(1, 40))
                mem.put32(SYMBOLS["worldmap_area_blocks_z"], rng.randint(1, 40))
                cx, cz = s32(mem.u32(CAMERA)) >> 12, s32(mem.u32(CAMERA + 8)) >> 12
                count = rng.randint(1, 120)
                for i in range(count):
                    position(i, (cx + rng.randint(-0x900, 0x900)) & 0xFFFF, rng.randint(-0x200, 0x200),
                             (cz + rng.randint(-0x900, 0x900)) & 0xFFFF)
                for i in range(0x100):
                    mem.put32(OT + i * 4, rng.getrandbits(32) if rng.random() < 0.2 else 0)
                mem.put32(QUAD_COUNT, rng.choice([0, 0, 100, 500, 511, 0x10000]))
                drew += self.assertSameAsModel(
                    lambda: billboards_model(POSITIONS, count, OT, PACKETS),
                    lambda: LIB.worldmap_billboards_draw_block(POSITIONS, count, OT, PACKETS),
                )
        self.assertGreater(drew, 35)


if __name__ == "__main__":
    unittest.main()
