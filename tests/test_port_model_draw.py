"""The port's model renderers and environment-map patcher (port/model_draw.c)
against the contracts of decomp/src/resident/model_draw*.s, model_depth.s and
model_set_envmap_mapping.s, natively (tests/port_native.py), on invented
inputs.

Expected packets, ordering tables, counters and pointers come from a Python
reading of the contracts below (culling, sorting, the masked slot pointer,
delay-slot stores), not from the C. Projections, lighting and depth cueing
come from the software GTE itself (tests/test_port_gte.py checks it), run
on the face's own inputs.
"""

import ctypes
import random
import struct
import tempfile
import unittest

from tests import port_native as mem

# The original addresses of the globals the renderers use.
SYMBOLS = {
    "model_envmap_patch_base": 0x800308D0,
    "model_screen_x_limit": 0x800500F8,
    "model_screen_y_limit": 0x800500FC,
    "model_ot_depth_shift": 0x80050100,
    "model_current_packet": 0x80059424,
    "model_lit_color_cache": 0x80059498,
    "model_current_normals": 0x8005952C,
    "model_current_vertices": 0x8005953C,
    "model_ot": 0x80059568,
    "model_drawn_primitive_count": 0x80059578,
    "model_color": 0x80059598,
}
G = SYMBOLS

VERTICES = 0x80100000
NORMALS = 0x80104000
RECORDS = 0x80108000
PACKETS = 0x80110000
OT = 0x80120000
OT_ENTRIES = 0x1000
CACHE = 0x80130000
SPILL = 0x1F800000

RTPS, RTPT = 0x0180001, 0x0280030
NCLIP, AVSZ3, AVSZ4 = 0x1400006, 0x158002D, 0x168002E
DPCS, NCS, NCT, NCCS, NCCT = 0x0780010, 0x0C8041E, 0x0D80420, 0x108041B, 0x118043F
MVMVA_RT_V0 = 0x0480012  # sf 1, RT, V0, no translation

# Data registers.
VXY0, VZ0, VXY1, VZ1, VXY2, VZ2, RGBC, OTZ = range(8)
IR0 = 8
SXY0, SXY1, SXY2 = 12, 13, 14
SZ0, SZ1, SZ2, SZ3 = 16, 17, 18, 19
RGB0, RGB1, RGB2 = 20, 21, 22
MAC0, MAC1, MAC2, MAC3 = 24, 25, 26, 27
LZCR = 31
FLAG = 31

LIB = None
BUILD = None
ENTRIES = (
    "model_draw_gt3_avg model_draw_g3_avg model_draw_f3_avg model_draw_ft3_avg model_draw_gt4_avg "
    "model_draw_g4_avg model_draw_f4_avg model_draw_ft4_avg model_draw_gt3_far model_draw_g3_far "
    "model_draw_f3_far model_draw_ft3_far model_draw_gt4_far model_draw_g4_far model_draw_f4_far "
    "model_draw_ft4_far model_draw_gt3_near model_draw_g3_near model_draw_f3_near model_draw_ft3_near "
    "model_draw_gt4_near model_draw_g4_near model_draw_f4_near model_draw_ft4_near model_draw_f3_lit "
    "model_draw_f3_cued model_draw_ft3_cued model_draw_ft3_cued_far model_draw_ft3_lit model_draw_gt3_lit "
    "model_draw_g3_lit model_draw_f4_lit model_draw_ft4_lit model_draw_ft4_cued model_draw_ft4_cued_far "
    "model_draw_ft3_envmap"
).split()

# Packet formats: XY word step, tag length (words), packet size (bytes).
FORMATS = {
    "gt3": (12, 9, 40), "g3": (8, 6, 28), "f3": (4, 4, 20), "ft3": (8, 7, 32),
    "gt4": (12, 12, 52), "g4": (8, 8, 36), "f4": (4, 5, 24), "ft4": (8, 9, 40),
}


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/model_draw.c"], SYMBOLS)
    for name in ENTRIES:
        getattr(LIB, name).argtypes = [ctypes.c_void_p, ctypes.c_int]
    LIB.model_set_envmap_mapping.argtypes = [ctypes.c_int] * 4


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def pair(low, high):
    return (low & 0xFFFF) | (high & 0xFFFF) << 16


# ---------------------------------------------------------------- the GTE


def wd(reg, value):
    LIB.xem_gte_write_data(reg, value & 0xFFFFFFFF)


def rd(reg):
    return LIB.xem_gte_read_data(reg)


def wc(reg, value):
    LIB.xem_gte_write_control(reg, value & 0xFFFFFFFF)


def run(command):
    LIB.xem_gte_execute(command)


def matrix(first, rows):
    flat = [v for row in rows for v in row]
    for i in range(4):
        wc(first + i, pair(flat[2 * i], flat[2 * i + 1]))
    wc(first + 4, flat[8] & 0xFFFF)


def setup_gte(zsf3=0x155, zsf4=0x100):
    """A view: identity rotation, the screen centre at (160, 120), H = 256,
    depth cueing from z 400 on, a light from the viewer, InitGeom's ZSF."""
    LIB.xem_gte_reset()
    matrix(0, ((0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000)))
    for i in range(3):
        wc(5 + i, 0)
    wc(24, 160 << 16)
    wc(25, 120 << 16)
    wc(26, 256)
    wc(27, 0x40)  # DQA: IR0 about 0x900 at z 1000, 0x9B0 at 600
    wc(28, 0x800000)  # DQB
    wc(29, zsf3)
    wc(30, zsf4)
    matrix(8, ((0, 0, -0x1000), (0x800, 0, 0), (0, 0x800, 0)))  # LLM
    matrix(16, ((0x1000, 0x400, 0), (0x800, 0x1000, 0), (0x400, 0, 0x1000)))  # LCM
    wc(13, 0x200)
    wc(14, 0x100)
    wc(15, 0x80)  # BK
    wc(21, 0x40)
    wc(22, 0x80)
    wc(23, 0xC0)  # FC


def project(vertex):
    """SXY and SZ of a vertex, from RTPS on the GTE as it stands (restored)."""
    saved = mem.gte_snapshot(LIB)
    wd(VXY0, pair(vertex[0], vertex[1]))
    wd(VZ0, vertex[2])
    run(RTPS)
    result = rd(SXY2), rd(SZ3)
    mem.gte_restore(LIB, saved)
    return result


def gte_result(steps, reads):
    """Run (('d', reg, value) | ('c', command)) steps on a copy of the GTE."""
    saved = mem.gte_snapshot(LIB)
    for step in steps:
        if step[0] == "d":
            wd(step[1], step[2])
        else:
            run(step[1])
    result = [rd(r) for r in reads]
    mem.gte_restore(LIB, saved)
    return result


def nclip(sxys):
    x = [s16(s) for s in sxys]
    y = [s16(s >> 16) for s in sxys]
    return x[0] * y[1] + x[1] * y[2] + x[2] * y[0] - x[0] * y[2] - x[1] * y[0] - x[2] * y[1]


def average(zsf, depths):
    return max(0, min(0xFFFF, (zsf * sum(depths)) >> 12))


# ---------------------------------------------------------------- scenes


class Scene:
    """Vertices, face records, packet slots and an OT in game memory."""

    def __init__(self, vertices, faces, size, extra=None, shift=0, x_limit=320, y_limit=240, normals=None):
        mem.clear()
        self.vertices = vertices
        self.faces = faces
        self.size = size
        self.extra = extra or (faces[0] if faces else (0, 0, 0, 0))
        for i, v in enumerate(vertices):
            mem.write(VERTICES + i * 8, struct.pack("<4h", v[0], v[1], v[2], 0x5A5A))
        if normals:
            for i, n in enumerate(normals):
                mem.write(NORMALS + i * 8, struct.pack("<4h", n[0], n[1], n[2], 0))
        records = list(faces) + [extra or faces[0] if faces else (0, 0, 0, 0)]
        for i, face in enumerate(records):
            indices = list(face) + [0] * (4 - len(face))
            mem.put16(RECORDS + i * 8, *indices)
        # Packet slots: a recognisable pattern every word the renderer must leave.
        for i in range(len(faces) + 1):
            for k in range(size // 4):
                mem.put32(PACKETS + i * size + k * 4, 0xC0DE0000 | i << 8 | k)
        for e in range(OT_ENTRIES):
            mem.put32(OT + e * 4, 0x00F00000 | e)
        mem.put32(G["model_current_vertices"], VERTICES)
        mem.put32(G["model_current_normals"], NORMALS)
        mem.put32(G["model_current_packet"], PACKETS)
        mem.put32(G["model_ot"], OT)
        mem.put32(G["model_drawn_primitive_count"], 7)
        mem.put32(G["model_screen_x_limit"], x_limit)
        mem.put32(G["model_screen_y_limit"], y_limit << 16)
        mem.put32(G["model_ot_depth_shift"], shift)
        self.x_limit = x_limit
        self.y_limit = y_limit << 16
        self.shift = shift

    def projections(self, face):
        return [project(self.vertices[i]) for i in face]

    def on_screen(self, sxys):
        return any(s < self.y_limit for s in sxys) and any(s & 0xFFFF < self.x_limit for s in sxys)

    def y_test(self, sxys):
        return any(s < self.y_limit for s in sxys)

    def x_test(self, sxys):
        return any(s & 0xFFFF < self.x_limit for s in sxys)

    def draw(self, name, count=None):
        getattr(LIB, name)(RECORDS, len(self.faces) if count is None else count)

    def slot(self, i):
        return PACKETS + i * self.size

    def words(self, i):
        return list(struct.unpack(f"<{self.size // 4}I", mem.read(self.slot(i), self.size)))

    def ot(self):
        return list(struct.unpack(f"<{OT_ENTRIES}I", mem.read(OT, OT_ENTRIES * 4)))


class Expected:
    """The contract's effects, accumulated face by face."""

    def __init__(self, scene, tag_words):
        self.scene = scene
        self.tag = tag_words << 24
        self.packets = {i: scene.words(i) for i in range(len(scene.faces) + 1)}
        self.ot = scene.ot()
        self.masked = False
        self.drawn = 7

    def store(self, i, offset, value):
        self.packets[i][offset // 4] = value & 0xFFFFFFFF

    def store_byte(self, i, offset, value):
        word = self.packets[i][offset // 4]
        shift = (offset % 4) * 8
        self.packets[i][offset // 4] = (word & ~(0xFF << shift)) | (value & 0xFF) << shift

    def address(self, i):
        address = self.scene.slot(i)
        return address & 0x00FFFFFF if self.masked else address

    def link(self, i, depth, shift):
        entry = depth >> shift
        old = self.ot[entry]
        self.ot[entry] = self.address(i)
        self.store(i, 0, old | self.tag)

    def check(self, test, count=None):
        scene = self.scene
        count = len(scene.faces) if count is None else count
        for i in range(len(scene.faces) + 1):
            test.assertEqual(scene.words(i), self.packets[i], f"packet {i}")
        test.assertEqual(scene.ot(), self.ot)
        test.assertEqual(mem.u32(G["model_drawn_primitive_count"]), self.drawn)
        end = PACKETS + count * scene.size
        test.assertEqual(mem.u32(G["model_current_packet"]), end & 0x00FFFFFF if self.masked else end)


def random_scene(rng, faces, quads, size, **kw):
    vertices = [(rng.randint(-900, 900), rng.randint(-700, 700), rng.randint(250, 3000)) for _ in range(40)]
    n = 4 if quads else 3
    records = [tuple(rng.randrange(len(vertices)) for _ in range(n)) for _ in range(faces)]
    return Scene(vertices, records, size, extra=tuple(rng.randrange(len(vertices)) for _ in range(n)), **kw)


def entry_names(sort, shapes):
    return [(f"model_draw_{shape}_{sort}", shape) for shape in shapes]


# ---------------------------------------------------------------- AVSZ sorts


def expect_avg(scene, shape, zsf3=0x155, zsf4=0x100):
    step, words, size = FORMATS[shape]
    e = Expected(scene, words)
    quad = shape.endswith("4")
    for i, face in enumerate(scene.faces):
        p = scene.projections(face)
        sxy = [s for s, _ in p]
        sz = [z for _, z in p]
        if quad:
            e.masked = True  # in the delay slot of the error test, before NCLIP's
            if nclip(sxy[:3]) <= 0 or not scene.on_screen(sxy):
                continue
            e.drawn += 1
            otz = average(zsf4, sz)
            if otz == 0:
                continue
            e.link(i, otz, scene.shift)
            for k in range(4):
                e.store(i, 8 + k * step, sxy[k])
        else:
            if not scene.on_screen(sxy):
                continue
            e.masked = True
            if nclip(sxy) <= 0:
                continue
            for k in range(3):
                e.store(i, 8 + k * step, sxy[k])
            e.drawn += 1
            otz = average(zsf3, sz)
            if otz == 0:
                continue
            e.link(i, otz, scene.shift)
    return e


class AverageSorts(unittest.TestCase):
    def test_random_scenes_every_format(self):
        rng = random.Random(1)
        for name, shape in entry_names("avg", FORMATS):
            for shift in (0, 2):
                with self.subTest(name=name, shift=shift):
                    setup_gte()
                    scene = random_scene(rng, 24, shape.endswith("4"), FORMATS[shape][2], shift=shift)
                    expected = expect_avg(scene, shape)
                    scene.draw(name)
                    expected.check(self)
                    self.assertGreater(expected.drawn, 7)

    def test_zero_otz_is_counted_but_not_linked(self):
        # ZSF3 = ZSF4 = 1: every average depth is 0. A triangle's SXY words
        # are still written; a quad's are not.
        for name, shape in (("model_draw_f3_avg", "f3"), ("model_draw_f4_avg", "f4")):
            with self.subTest(name=name):
                setup_gte(zsf3=1, zsf4=1)
                vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 1000), (120, 90, 1000)]
                face = (0, 1, 3, 2) if shape == "f4" else (0, 1, 2)
                scene = Scene(vertices, [face], FORMATS[shape][2])
                expected = expect_avg(scene, shape, zsf3=1, zsf4=1)
                scene.draw(name)
                expected.check(self)
                self.assertEqual(expected.drawn, 8)
                self.assertEqual(scene.ot(), [0x00F00000 | e for e in range(OT_ENTRIES)])

    def test_count_zero_projects_the_first_face_only(self):
        setup_gte()
        vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 1000)]
        scene = Scene(vertices, [(0, 2, 1)], 20)
        expected = Expected(scene, 4)
        scene.draw("model_draw_f3_avg", count=0)
        expected.check(self, count=0)
        self.assertEqual([rd(r) for r in (SXY0, SXY1, SXY2)], [project(vertices[i])[0] for i in (0, 2, 1)])

    def test_the_record_after_the_last_is_read_and_projected(self):
        setup_gte()
        vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 1000), (30, 30, 2000), (60, 0, 2500), (0, 60, 1500)]
        scene = Scene(vertices, [(0, 2, 1)], 40, extra=(3, 4, 5))
        scene.draw("model_draw_gt3_avg")
        self.assertEqual([rd(r) for r in (SXY0, SXY1, SXY2)], [project(vertices[i])[0] for i in (3, 4, 5)])
        self.assertEqual([rd(r) for r in (SZ1, SZ2, SZ3)], [project(vertices[i])[1] for i in (3, 4, 5)])
        self.assertEqual(rd(VXY0), pair(30, 30))
        self.assertEqual(rd(VZ2), 1500)
        # OTZ is the last face's AVSZ3, which the extra projection leaves.
        self.assertEqual(rd(OTZ), average(0x155, [1000, 1000, 1000]))

    def test_overflowed_projections_are_not_culled(self):
        # A vertex at z 1 overflows the division (FLAG bit 31): the renderers
        # read LZCR, not FLAG, so the face still reaches the OT.
        setup_gte()
        vertices = [(-100, -50, 1000), (1, 1, 1), (100, -50, 1000)]
        scene = Scene(vertices, [(0, 2, 1)], 20)
        saved = mem.gte_snapshot(LIB)
        wd(VXY0, pair(1, 1))
        wd(VZ0, 1)
        run(RTPS)
        self.assertTrue(LIB.xem_gte_read_control(FLAG) & 0x80000000)
        mem.gte_restore(LIB, saved)
        expected = expect_avg(scene, "f3")
        scene.draw("model_draw_f3_avg")
        expected.check(self)
        self.assertEqual(expected.drawn, 8)
        self.assertNotEqual(scene.ot(), [0x00F00000 | e for e in range(OT_ENTRIES)])

    def test_slot_pointer_stays_unmasked_when_no_face_passes_the_bounds(self):
        setup_gte()
        vertices = [(-900, -900, 300), (-890, -900, 300), (-900, -890, 300)]
        scene = Scene(vertices, [(0, 1, 2), (0, 2, 1)], 32)
        expected = expect_avg(scene, "ft3")
        self.assertFalse(expected.masked)
        scene.draw("model_draw_ft3_avg")
        expected.check(self)

    def test_bounds_take_y_and_x_from_different_vertices(self):
        # Vertex 0 is on screen in y only, vertex 1 in x only: the face is kept.
        setup_gte()
        vertices = [(-256 * 3, 0, 256), (0, -256 * 3, 256), (-256 * 3, -256 * 3, 256)]
        scene = Scene(vertices, [(0, 1, 2), (0, 2, 1)], 20)
        sxy = [project(v)[0] for v in vertices]
        self.assertTrue(sxy[0] < 240 << 16 and sxy[0] & 0xFFFF >= 320)
        self.assertTrue(sxy[1] >= 240 << 16 and sxy[1] & 0xFFFF < 320)
        expected = expect_avg(scene, "f3")
        scene.draw("model_draw_f3_avg")
        expected.check(self)
        self.assertEqual(expected.drawn, 8)


# ---------------------------------------------------------------- far and near sorts


def expect_depth(scene, shape, far):
    step, words, size = FORMATS[shape]
    e = Expected(scene, words)
    pick = max if far else min
    shift = scene.shift + 2
    for i, face in enumerate(scene.faces):
        p = scene.projections(face)
        sxy = [s for s, _ in p]
        sz = [z for _, z in p]
        if shape.endswith("4"):
            if nclip(sxy[:3]) <= 0 or not scene.y_test(sxy):
                continue
            e.store(i, 8, sxy[0])
            if not scene.x_test(sxy):
                continue
            e.store(i, 8 + step, sxy[1])
            e.store(i, 8 + 2 * step, sxy[2])
            if sz[0] == 0:
                continue
            e.store(i, 8 + 3 * step, sxy[3])
            if sz[1] == 0 or sz[2] == 0:
                continue
            e.masked = True
            if sz[3] == 0:
                continue
        else:
            if not scene.on_screen(sxy):
                continue
            e.store(i, 8, sxy[0])
            if nclip(sxy) <= 0:
                continue
            e.store(i, 8 + step, sxy[1])
            e.store(i, 8 + 2 * step, sxy[2])
            e.masked = True
        e.drawn += 1
        depth = pick(sz)
        if depth:
            e.link(i, depth, shift)
    return e


class DepthSorts(unittest.TestCase):
    def test_random_scenes_every_format(self):
        rng = random.Random(2)
        for sort, far in (("far", True), ("near", False)):
            for name, shape in entry_names(sort, FORMATS):
                with self.subTest(name=name):
                    setup_gte()
                    scene = random_scene(rng, 24, shape.endswith("4"), FORMATS[shape][2], shift=1)
                    expected = expect_depth(scene, shape, far)
                    scene.draw(name)
                    expected.check(self)
                    self.assertGreater(expected.drawn, 7)

    def test_back_face_gets_its_first_sxy_word(self):
        setup_gte()
        vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 1000)]
        scene = Scene(vertices, [(0, 2, 1)], 28)
        self.assertLessEqual(nclip([project(vertices[i])[0] for i in (0, 2, 1)]), 0)
        before = scene.words(0)
        scene.draw("model_draw_g3_far")
        after = scene.words(0)
        self.assertEqual(after[2], project(vertices[0])[0])
        self.assertEqual(after[:2] + after[3:], before[:2] + before[3:])
        self.assertEqual(mem.u32(G["model_current_packet"]), PACKETS + 28)

    def test_quad_rejected_by_x_still_gets_its_first_sxy_word(self):
        setup_gte()
        # y on screen, x right of the limit for all four points.
        vertices = [(300, -50, 300), (400, -50, 300), (300, 50, 300), (400, 50, 300)]
        scene = Scene(vertices, [(0, 1, 2, 3)], 24)
        sxy = [project(v)[0] for v in vertices]
        self.assertTrue(scene.y_test(sxy) and not scene.x_test(sxy))
        self.assertGreater(nclip(sxy[:3]), 0)
        before = scene.words(0)
        scene.draw("model_draw_f4_near")
        after = scene.words(0)
        self.assertEqual(after[2], sxy[0])
        self.assertEqual(after[:2] + after[3:], before[:2] + before[3:])
        self.assertEqual(mem.u32(G["model_drawn_primitive_count"]), 7)

    def test_quad_with_a_zero_depth_point_is_written_but_not_counted(self):
        # The fourth point behind the eye: SZ3 saturates to 0. Its SXY words
        # are written and the slot pointer masked, but it is not counted.
        for name in ("model_draw_g4_far", "model_draw_g4_near"):
            with self.subTest(name=name):
                setup_gte()
                vertices = [(-100, -50, 1000), (100, -50, 1000), (-100, 60, 1000), (100, 60, -100)]
                scene = Scene(vertices, [(0, 1, 2, 3)], 36)
                self.assertEqual(project(vertices[3])[1], 0)
                expected = expect_depth(scene, "g4", name.endswith("far"))
                self.assertTrue(expected.masked)
                self.assertEqual(expected.drawn, 7)
                scene.draw(name)
                expected.check(self)

    def test_depth_is_shifted_by_the_ot_shift_plus_two(self):
        setup_gte()
        vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 3000)]
        scene = Scene(vertices, [(0, 1, 2)], 20, shift=3)
        scene.draw("model_draw_f3_far")
        entry = project(vertices[2])[1] >> 5
        self.assertEqual(scene.ot()[entry], PACKETS & 0x00FFFFFF)
        self.assertEqual(scene.words(0)[0], (0x00F00000 | entry) | 0x04000000)


# ---------------------------------------------------------------- lit and depth-cued


class LitAndCued(unittest.TestCase):
    def face_scene(self, size, shape_faces, normals=None, vertices=None):
        vertices = vertices or [
            (-100, -50, 1000), (100, -50, 1000), (0, 80, 1000), (120, 90, 1200),
            (-100, -50, 600), (100, -50, 600), (0, 80, 600), (120, 90, 600),
        ]
        return Scene(vertices, shape_faces, size, normals=normals)

    def test_f3_lit_lights_from_the_cache_and_always_links(self):
        setup_gte(zsf3=1)  # every OTZ 0: f3_lit links anyway
        scene = self.face_scene(20, [(0, 1, 2), (0, 2, 1), (4, 5, 6)])
        records = [(0x11223344, (0x100, -0x200, 0x300)), (0x55667788, (0, 0, 0x1000)), (0x99AABBCC, (-0x800, 0x800, 0))]
        for i, (colour, normal) in enumerate(records):
            mem.put32(CACHE + i * 12, colour)
            mem.write(CACHE + i * 12 + 4, struct.pack("<4h", *normal, 0))
        mem.put32(G["model_lit_color_cache"], CACHE)
        e = Expected(scene, 4)
        for i, face in enumerate(scene.faces):
            sxy = [s for s, _ in scene.projections(face)]
            for k in range(3):
                e.store(i, 8 + 4 * k, sxy[k])
            e.masked = e.masked or nclip(sxy) > 0
            if nclip(sxy) <= 0:
                continue
            e.drawn += 1
            colour, normal = records[i]
            rgb = gte_result([("d", VXY0, pair(normal[0], normal[1])), ("d", VZ0, normal[2]),
                              ("d", RGBC, colour), ("c", NCCS)], [RGB2])[0]
            e.store(i, 4, (colour & 0xFF000000) | (rgb & 0xFFFFFF))
            e.link(i, 0, 0)
        scene.draw("model_draw_f3_lit")
        e.check(self)
        self.assertEqual(e.drawn, 9)
        self.assertEqual(mem.u32(G["model_lit_color_cache"]), CACHE + 36)

    def test_ft3_lit_consumes_a_normal_per_face_and_keeps_the_code(self):
        setup_gte()
        scene = self.face_scene(32, [(0, 1, 2), (0, 2, 1), (4, 6, 5)])
        normals = [(0x100, -0x200, 0x300), (0, 0, 0x1000), (-0x800, 0x800, 0)]
        for i, n in enumerate(normals):
            mem.write(CACHE + i * 8, struct.pack("<4h", *n, 0))
        mem.put32(G["model_lit_color_cache"], CACHE)
        for i in range(3):
            mem.write(scene.slot(i) + 4, bytes([1, 2, 3, 0x26]))
        e = Expected(scene, 7)
        for i, face in enumerate(scene.faces):
            p = scene.projections(face)
            sxy = [s for s, _ in p]
            e.store(i, 8, sxy[0])
            e.store(i, 16, sxy[1])
            if nclip(sxy) <= 0:
                continue
            e.store(i, 24, sxy[2])
            e.masked = True
            e.drawn += 1
            rgb = gte_result([("d", VXY0, pair(normals[i][0], normals[i][1])), ("d", VZ0, normals[i][2]),
                              ("c", NCS)], [RGB2])[0]
            e.store(i, 4, 0x26000000 | (rgb & 0xFFFFFF))
            e.link(i, average(0x155, [z for _, z in p]), 0)
        scene.draw("model_draw_ft3_lit")
        e.check(self)
        self.assertEqual(mem.u32(G["model_lit_color_cache"]), CACHE + 24)

    def test_cued_triangles_modulate_and_issue_dpcs_for_unlinked_faces(self):
        for name, shape in (("model_draw_ft3_cued", "ft3"), ("model_draw_f3_cued", "f3")):
            for zsf in (0x155, 1):
                with self.subTest(name=name, zsf=zsf):
                    setup_gte(zsf3=zsf)
                    step, words, size = FORMATS[shape]
                    scene = self.face_scene(size, [(0, 1, 2), (4, 5, 6)])
                    mem.put32(G["model_color"], 0x30405060)
                    for i in range(2):
                        mem.write(scene.slot(i) + 4, bytes([9, 9, 9, 0x25]))
                    e = Expected(scene, words)
                    last = None
                    for i, face in enumerate(scene.faces):
                        p = scene.projections(face)
                        sxy = [s for s, _ in p]
                        e.masked = True
                        for k in range(3):
                            e.store(i, 8 + k * step, sxy[k])
                        e.drawn += 1
                        steps = [("d", RGBC, 0x30405060)]
                        for k, v in enumerate(face):
                            vx = scene.vertices[v]
                            steps += [("d", 2 * k, pair(vx[0], vx[1])), ("d", 2 * k + 1, vx[2])]
                        steps += [("c", RTPT), ("c", DPCS)]
                        last = gte_result(steps, [RGB2, RGB1])
                        otz = average(zsf, [z for _, z in p])
                        if otz:
                            e.store(i, 4, 0x24000000 | (last[0] & 0xFFFFFF))
                            e.link(i, otz, 0)
                    scene.draw(name)
                    e.check(self)
                    # The last face's DPCS is the last colour pushed, linked or not.
                    self.assertEqual(rd(RGB2), last[0])

    def test_ft3_cued_far_issues_dpcs_once_the_y_test_is_reached(self):
        setup_gte()
        # Entirely below the screen: the y test fails, DPCS is still issued.
        vertices = [(-100, 2000, 1000), (100, 2000, 1000), (0, 2100, 1000)]
        scene = Scene(vertices, [(0, 2, 1)], 32)
        mem.put32(G["model_color"], 0x00808080)
        wd(RGB0, 0x11)
        wd(RGB1, 0x22)
        wd(RGB2, 0x33)
        scene.draw("model_draw_ft3_cued_far")
        self.assertEqual(rd(RGB1), 0x33)
        self.assertNotEqual(rd(RGB2), 0x33)
        self.assertEqual(mem.u32(G["model_drawn_primitive_count"]), 7)

    def test_ft3_cued_far_links_the_farthest_vertex(self):
        setup_gte()
        scene = self.face_scene(32, [(0, 1, 3), (4, 5, 6)])
        mem.put32(G["model_color"], 0x00808080)
        for i in range(2):
            mem.write(scene.slot(i) + 4, bytes([9, 9, 9, 0x25]))
        e = Expected(scene, 7)
        for i, face in enumerate(scene.faces):
            p = scene.projections(face)
            sxy = [s for s, _ in p]
            e.masked = True
            for k in range(3):
                e.store(i, 8 + 8 * k, sxy[k])
            e.drawn += 1
            steps = [("d", RGBC, 0x00808080)]
            for k, v in enumerate(face):
                vx = scene.vertices[v]
                steps += [("d", 2 * k, pair(vx[0], vx[1])), ("d", 2 * k + 1, vx[2])]
            rgb = gte_result(steps + [("c", RTPT), ("c", DPCS)], [RGB2])[0]
            e.store(i, 4, 0x24000000 | (rgb & 0xFFFFFF))
            e.link(i, max(z for _, z in p), 2)
        scene.draw("model_draw_ft3_cued_far")
        e.check(self)

    def vertex_lit(self, name, cached):
        setup_gte()
        normals = [(0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000), (0x900, 0x900, 0x900),
                   (-0x400, 0, 0xC00), (0, -0x400, 0xC00), (0x400, 0x400, 0), (0, 0, -0x1000)]
        size = 28 if cached else 40
        scene = self.face_scene(size, [(0, 2, 1), (4, 6, 5), (0, 1, 2)], normals=normals)
        colours = [0x20406080, 0x31507090, 0x42618100]
        for i, c in enumerate(colours):
            mem.put32(CACHE + i * 4, c)
        mem.put32(G["model_lit_color_cache"], CACHE)
        for i in range(3):
            mem.write(scene.slot(i) + 4, bytes([1, 2, 3, 0x34]))
        e = Expected(scene, 6 if cached else 9)
        o1, o2 = (16, 24) if cached else (20, 32)
        for i, face in enumerate(scene.faces):
            p = scene.projections(face)
            sxy = [s for s, _ in p]
            e.store(i, 8, sxy[0])
            e.store(i, o1, sxy[1])
            if nclip(sxy) <= 0:
                continue
            e.store(i, o2, sxy[2])
            e.masked = True
            e.drawn += 1
            steps = [("d", RGBC, colours[i])] if cached else []
            for k, v in enumerate(face):
                n = normals[v]
                steps += [("d", 2 * k, pair(n[0], n[1])), ("d", 2 * k + 1, n[2])]
            rgb = gte_result(steps + [("c", NCCT if cached else NCT)], [RGB0, RGB1, RGB2])
            code = colours[i] & 0xFF000000 if cached else 0x34000000
            e.store(i, 4, code | (rgb[0] & 0xFFFFFF))
            e.store(i, 12 if cached else 16, rgb[1])
            e.store(i, 20 if cached else 28, rgb[2])
            e.link(i, average(0x155, [z for _, z in p]), 0)
        scene.draw(name)
        e.check(self)
        # The last face's normal addresses spilled to the scratchpad.
        self.assertEqual([mem.u32(SPILL + 4 * k) for k in range(3)], [NORMALS + 8 * v for v in (0, 1, 2)])
        if cached:
            self.assertEqual(mem.u32(G["model_lit_color_cache"]), CACHE + 12)

    def test_gt3_lit(self):
        self.vertex_lit("model_draw_gt3_lit", False)

    def test_g3_lit(self):
        self.vertex_lit("model_draw_g3_lit", True)

    def quad_lit(self, name, textured):
        setup_gte()
        size = 40 if textured else 24
        step = 8 if textured else 4
        # Face 1's index 0 is 0x2001: the loop masks it to 13 bits (vertex 1).
        vertices = [(-100, -50, 1000), (100, -50, 1000), (-100, 60, 1000), (100, 60, 1000),
                    (-100, -50, 600), (100, -50, 600), (-100, 60, 600), (100, 60, 600)]
        scene = Scene(vertices, [(0, 1, 2, 3), (0x2004, 5, 6, 7), (0, 2, 1, 3)], size)
        cache_records = [(0x51000000 | 0x203040, (0x100, 0x200, 0xF00)),
                         (0x62000000 | 0x405060, (0, 0, 0x1000)),
                         (0x73000000 | 0x607080, (0x800, 0, 0x800))]
        record = 8 if textured else 12
        for i, (colour, normal) in enumerate(cache_records):
            if textured:
                mem.write(CACHE + i * 8, struct.pack("<4h", *normal, 0))
            else:
                mem.put32(CACHE + i * 12, colour)
                mem.write(CACHE + i * 12 + 4, struct.pack("<4h", *normal, 0))
        mem.put32(G["model_lit_color_cache"], CACHE)
        for i in range(3):
            mem.write(scene.slot(i) + 4, bytes([1, 2, 3, 0x2C]))
        e = Expected(scene, 9 if textured else 5)
        for i, face in enumerate(scene.faces):
            face = [face[0] & 0x1FFF if i else face[0]] + list(face[1:])
            p = [project(vertices[v]) for v in face]
            sxy = [s for s, _ in p]
            e.masked = True
            if nclip(sxy[:3]) <= 0:
                continue
            e.drawn += 1
            colour, normal = cache_records[i]
            steps = [("d", VXY0, pair(normal[0], normal[1])), ("d", VZ0, normal[2])]
            if not textured:
                steps.append(("d", RGBC, colour))
            rgb = gte_result(steps + [("c", NCS if textured else NCCS)], [RGB2])[0]
            for k in range(4):
                e.store(i, 8 + k * step, sxy[k])
            code = 0x2C000000 if textured else colour & 0xFF000000
            e.store(i, 4, code | (rgb & 0xFFFFFF))
            e.link(i, average(0x100, [z for _, z in p]), 0)
        self.assertEqual(e.drawn, 9)
        scene.draw(name)
        e.check(self)
        self.assertEqual(mem.u32(G["model_lit_color_cache"]), CACHE + 3 * record)

    def test_f4_lit(self):
        self.quad_lit("model_draw_f4_lit", False)

    def test_ft4_lit(self):
        self.quad_lit("model_draw_ft4_lit", True)

    def test_first_face_index_0_keeps_sixteen_bits(self):
        # Only faces after the first mask index 0 to thirteen bits.
        setup_gte()
        vertices = [(-100, -50, 1000)] * 0x2001 + [(-100, -50, 600)]
        vertices[1] = (5000, 5000, 300)  # 0x2001 & 0x1FFF: off screen
        vertices[2], vertices[3], vertices[4] = (100, -50, 1000), (-100, 60, 1000), (100, 60, 1000)
        scene = Scene(vertices, [(0x2001, 2, 3, 4)], 24)
        mem.put32(G["model_lit_color_cache"], CACHE)
        scene.draw("model_draw_f4_lit")
        self.assertEqual(mem.u32(G["model_drawn_primitive_count"]), 8)

    def cued_quads(self, name, far):
        setup_gte()
        vertices = [(-100, -50, 1000), (100, -50, 1000), (-100, 60, 1000), (100, 60, 1400),
                    (300, -50, 300), (400, -50, 300), (300, 50, 300), (400, 50, 300)]
        # Face 1 is below the x limit test: rejected by x, its SXY0 written.
        scene = Scene(vertices, [(0, 1, 2, 3), (4, 5, 6, 7), (0, 2, 1, 3)], 40)
        mem.put32(G["model_color"], 0x00406080)
        for i in range(3):
            mem.write(scene.slot(i) + 4, bytes([1, 2, 3, 0x2D]))
        e = Expected(scene, 9)
        for i, face in enumerate(scene.faces):
            p = scene.projections(face)
            sxy = [s for s, _ in p]
            e.masked = True
            if nclip(sxy[:3]) <= 0 or not scene.y_test(sxy):
                continue
            e.store(i, 8, sxy[0])
            if not scene.x_test(sxy):
                continue
            for k in range(1, 4):
                e.store(i, 8 + 8 * k, sxy[k])
            e.drawn += 1
            steps = [("d", RGBC, 0x00406080)]
            for k, v in enumerate(face[:3]):
                vx = vertices[v]
                steps += [("d", 2 * k, pair(vx[0], vx[1])), ("d", 2 * k + 1, vx[2])]
            v3 = vertices[face[3]]
            steps += [("c", RTPT), ("d", VXY0, pair(v3[0], v3[1])), ("d", VZ0, v3[2]), ("c", RTPS), ("c", DPCS)]
            rgb = gte_result(steps, [RGB2])[0]
            e.store(i, 4, 0x2C000000 | (rgb & 0xFFFFFF))
            depth = max(z for _, z in p) if far else average(0x100, [z for _, z in p])
            e.link(i, depth, 2 if far else 0)
        self.assertEqual(e.drawn, 8)
        self.assertEqual(e.packets[1][2], project(vertices[4])[0])
        scene.draw(name)
        e.check(self)

    def test_ft4_cued(self):
        self.cued_quads("model_draw_ft4_cued", False)

    def test_ft4_cued_far(self):
        self.cued_quads("model_draw_ft4_cued_far", True)


# ---------------------------------------------------------------- GTE state left behind


def load_vertex(scene, slot, index):
    v = scene.vertices[index]
    wd(2 * slot, pair(v[0], v[1]))
    wd(2 * slot + 1, v[2])


def replay_triangles(scene, dpcs_at):
    """The GTE commands of an AVSZ3 triangle renderer, in the contract's
    order: RTPT of each face and of the record after the last, NCLIP once
    the error test passes, AVSZ3 on screen, and the depth cue (dpcs_at:
    None, "counted" or "y"). Returns the whole GTE state."""
    records = list(scene.faces) + [scene.extra]
    for k in range(3):
        load_vertex(scene, k, records[0][k])
    for i in range(len(scene.faces) + 1):
        run(RTPT)
        if i == len(scene.faces):
            break
        sxy = [rd(SXY0), rd(SXY1), rd(SXY2)]
        for k in range(3):
            load_vertex(scene, k, records[i + 1][k])
        run(NCLIP)
        if dpcs_at == "y" and scene.y_test(sxy):
            run(DPCS)
            if not scene.x_test(sxy):
                continue
        elif dpcs_at == "y":
            run(DPCS)
            continue
        if not scene.on_screen(sxy):
            continue
        front = s32(rd(MAC0)) > 0
        if dpcs_at != "y":
            run(AVSZ3)
        if front and dpcs_at == "counted":
            run(DPCS)
    return mem.gte_snapshot(LIB)


class GteState(unittest.TestCase):
    def check_replay(self, name, dpcs_at):
        rng = random.Random(3)
        setup_gte()
        scene = random_scene(rng, 30, False, 32)
        mem.put32(G["model_color"], 0x00406080)
        wd(RGBC, 0x00406080)
        start = mem.gte_snapshot(LIB)
        expected = replay_triangles(scene, dpcs_at)
        mem.gte_restore(LIB, start)
        scene.draw(name)
        self.assertEqual(mem.gte_snapshot(LIB), expected)

    def test_avg_triangles(self):
        self.check_replay("model_draw_ft3_avg", None)

    def test_cued_triangles(self):
        self.check_replay("model_draw_ft3_cued", "counted")

    def test_cued_far_triangles(self):
        self.check_replay("model_draw_ft3_cued_far", "y")


# ---------------------------------------------------------------- environment map


def srl(rd_, rt, sa):
    return (rt << 16) | (rd_ << 11) | (sa << 6) | 0x02


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

    def envmap_scene(self):
        setup_gte()
        matrix(0, ((0xE00, 0x300, -0x200), (-0x300, 0xE00, 0x100), (0x200, -0x100, 0xF00)))
        normals = [(0x1000, 0, 0), (0, 0x1000, 0), (0, 0, 0x1000), (0x900, -0x900, 0x900),
                   (-0x400, 0x200, 0xC00), (0, -0x400, -0xC00)]
        vertices = [(-100, -50, 1000), (100, -50, 1000), (0, 80, 1000), (-90, -40, 700), (90, -40, 700), (0, 70, 700)]
        scene = Scene(vertices, [(0, 1, 2), (3, 5, 4), (3, 4, 5)], 32, normals=normals)
        image_mapping_code()
        return scene, normals

    def expect_envmap(self, scene, normals, u_shift, v_shift, u_offset, v_offset):
        e = Expected(scene, 7)
        for i, face in enumerate(scene.faces):
            p = scene.projections(face)
            sxy = [s for s, _ in p]
            e.store(i, 8, sxy[0])
            e.store(i, 16, sxy[1])
            if nclip(sxy) <= 0:
                continue
            e.store(i, 24, sxy[2])
            e.masked = True
            otz = average(0x155, [z for _, z in p])
            if otz == 0:
                continue
            e.drawn += 1
            for k, v in enumerate(face):
                n = normals[v]
                mac = gte_result([("d", VXY0, pair(n[0], n[1])), ("d", VZ0, n[2]), ("c", MVMVA_RT_V0)], [MAC1, MAC2])
                e.store_byte(i, 12 + 8 * k, (mac[0] >> u_shift) + u_offset)
                e.store_byte(i, 13 + 8 * k, (mac[1] >> v_shift) + v_offset)
            e.link(i, otz, 0)
        return e

    def test_envmap_reads_the_mapping_from_its_code(self):
        scene, normals = self.envmap_scene()
        e = self.expect_envmap(scene, normals, 6, 6, 0x40, 0x40)
        scene.draw("model_draw_ft3_envmap")
        e.check(self)
        self.assertEqual(e.drawn, 9)

    def test_envmap_mapping_persists_once_patched(self):
        scene, normals = self.envmap_scene()
        LIB.model_set_envmap_mapping(2, 1, 0x30, -0x10)
        e = self.expect_envmap(scene, normals, 2, 1, 0x30, -0x10)
        scene.draw("model_draw_ft3_envmap")
        e.check(self)
        # A second scene draws with the same patched code.
        scene2, normals2 = self.envmap_scene_keep_code()
        e = self.expect_envmap(scene2, normals2, 2, 1, 0x30, -0x10)
        scene2.draw("model_draw_ft3_envmap")
        e.check(self)

    def envmap_scene_keep_code(self):
        base = SYMBOLS["model_envmap_patch_base"]
        code = mem.read(base, 0x90)
        scene, normals = self.envmap_scene()
        mem.write(base, code)
        return scene, normals


if __name__ == "__main__":
    unittest.main()
