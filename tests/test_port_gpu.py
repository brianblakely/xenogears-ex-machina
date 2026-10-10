"""The port's libgpu (port/libgpu.c) built natively against a recording GPU.

The C is built for the host as a freestanding shared library (no libc) and
driven through ctypes. libgpu's state lives at fixed addresses in an arena
below 16 MB that the test maps (the symbols are linked there), so the 24-bit
addresses libgpu stores in packets and gives to DMA are real host addresses,
as they are game addresses in the game module. tests/port_gpu/support.c
records every GPU access and stands in for the run loop's yields and the DMA
completion interrupt. Expected words follow the original SDK's instruction
sequences and psx-spx's GPU command encodings. Run inside `nix develop
path:./nix/runtime` (it provides $XEM_CLANG and ld.lld).
"""

import ctypes
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLANG = os.environ.get("XEM_CLANG") or shutil.which("clang")
LIB = None
BUILD = None
ARENA = 0x400000
ARENA_SIZE = 0x100000

# libgpu's data in the arena (host LP64 layout: wider than the PS1's, so
# spaced generously).
SYMBOLS = {
    "libgpu_graph_type": 0x400000,
    "libgpu_current_function_table": 0x400200,
    "libgpu_type_vram_widths": 0x400210,
    "libgpu_type_vram_heights": 0x400230,
    # Preceded by the SDK's MoveImage packet head (two words).
    "libgpu_move_image_source": 0x400258,
    "libgpu_move_image_destination": 0x40025C,
    "libgpu_move_image_size": 0x400260,
    "libgpu_clear_otag_terminator": 0x400280,
    "libgpu_clear_packet": 0x4002C0,
    "libgpu_control_words": 0x400300,
    "libgpu_last_call_function": 0x400400,
    "libgpu_last_call_argument": 0x400408,
    "libgpu_last_call_parameter": 0x400410,
    "libgpu_queue_write_index": 0x400420,
    "libgpu_queue_read_index": 0x400424,
    "libgpu_tim_cursor": 0x400430,
    "libetc_video_mode": 0x400440,
    "libgpu_queue_entry_function": 0x401000,
}
GENV = SYMBOLS["libgpu_graph_type"]
# Host offsets of GEnv's fields (the callback pointer is 8 bytes here).
GENV_QUEUE_MODE, GENV_VRAM_WIDTH, GENV_PENDING, GENV_CALLBACK, GENV_DRAW, GENV_DISP = 1, 4, 8, 0x10, 0x18, 0x74
DATA = 0x480000  # test buffers

LOG_GP0, LOG_GP1, LOG_DMA, LOG_LIST_GP0 = range(4)


def map_arena():
    libc = ctypes.CDLL(None, use_errno=True)
    libc.mmap.restype = ctypes.c_void_p
    libc.mmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_long]
    prot = 0x1 | 0x2
    flags = 0x02 | 0x20 | 0x100000  # MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE
    address = libc.mmap(ARENA, ARENA_SIZE, prot, flags, -1, 0)
    if address != ARENA:
        raise unittest.SkipTest(f"cannot map the test arena at {ARENA:#x}")


def build_library(out):
    common = [
        "-O2", "-fPIC", "-ffreestanding", "-fno-builtin", "-nostdinc", "-std=gnu89",
        "-funsigned-char", "-fwrapv", "-fno-strict-aliasing", "-Wall", "-Werror",
        # Host pointers are wider than the game's 32-bit addresses; the test's
        # addresses all fit.
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast",
        "-Iport/include", "-Idecomp/include",
    ]
    objects = []
    for source, extra in (
        ("port/libgpu.c", ["-Dlong=int"]),
        ("tests/port_gpu/support.c", []),
    ):
        obj = os.path.join(out, os.path.basename(source) + ".o")
        subprocess.run([CLANG, *common, *extra, "-c", source, "-o", obj], cwd=ROOT, check=True)
        objects.append(obj)
    library = os.path.join(out, "libxemgpu.so")
    defsyms = [f"-Wl,--defsym={name}={address:#x}" for name, address in SYMBOLS.items()]
    subprocess.run(
        [CLANG, "-shared", "-nostdlib", "-fuse-ld=lld", *defsyms, *objects, "-o", library],
        cwd=ROOT,
        check=True,
    )
    return ctypes.CDLL(library)


def setUpModule():
    global LIB, BUILD
    if not CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    map_arena()
    BUILD = tempfile.TemporaryDirectory()
    LIB = build_library(BUILD.name)
    LIB.NextPrim.restype = ctypes.c_uint32
    LIB.get_tim_addr.restype = ctypes.c_int
    LIB.ReadTIM.restype = ctypes.c_void_p
    LIB.ClearOTagR.restype = ctypes.c_void_p
    LIB.ClearOTag.restype = ctypes.c_void_p
    for name in ("GetTPage", "GetClut", "LoadClut", "LoadTPage"):
        getattr(LIB, name).restype = ctypes.c_uint16


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def word(address, value=None):
    cell = ctypes.c_uint32.from_address(address)
    if value is not None:
        cell.value = value & 0xFFFFFFFF
    return cell.value


def words(address, count):
    return list(struct.unpack(f"<{count}I", ctypes.string_at(address, 4 * count)))


def put(address, values):
    for i, value in enumerate(values):
        word(address + 4 * i, value)


def half(address, value=None):
    cell = ctypes.c_int16.from_address(address)
    if value is not None:
        cell.value = value
    return cell.value


def rect(address, x, y, w, h):
    for i, value in enumerate((x, y, w, h)):
        half(address + 2 * i, value)
    return address


def global_int(name):
    return ctypes.c_int.in_dll(LIB, name)


def log():
    n = global_int("test_log_length").value
    kinds = (ctypes.c_uint * 4096).in_dll(LIB, "test_log_kind")
    values = (ctypes.c_uint * 4096).in_dll(LIB, "test_log_value")
    return [(kinds[i], values[i]) for i in range(n)]


def sent(kind):
    return [value for k, value in log() if k == kind]


class GpuTest(unittest.TestCase):
    """Each test starts after ResetGraph(0) on the retail GPU (type 3)."""

    def setUp(self):
        ctypes.memset(ARENA, 0, ARENA_SIZE)
        word(SYMBOLS["libgpu_current_function_table"], 0x80056888)
        put(SYMBOLS["libgpu_type_vram_widths"], [0x400] * 5)
        put(SYMBOLS["libgpu_type_vram_heights"], [0x200, 0x400, 0x400, 0x200, 0x400])
        put(SYMBOLS["libgpu_move_image_source"] - 8, [0x04FFFFFF, 0x80000000])
        put(SYMBOLS["libgpu_clear_otag_terminator"], [0x04FFFFFF, 0, 0, 0, 0])
        params = (ctypes.c_uint * 8).in_dll(LIB, "test_param")
        for i in range(8):
            params[i] = 0
        params[7] = 2
        global_int("test_read_index").value = 0
        ctypes.c_uint.in_dll(LIB, "test_status").value = 0x14000000
        ctypes.c_uint.in_dll(LIB, "test_dma_control").value = 0
        callbacks = (ctypes.c_void_p * 7).in_dll(LIB, "test_dma_callbacks")
        for i in range(7):
            callbacks[i] = None
        for name in ("test_yields", "test_draw_sync_callbacks", "test_reset_callbacks", "test_log_length"):
            global_int(name).value = 0
        self.assertEqual(LIB.ResetGraph(0), 3)
        LIB.test_reset_log()

    def busy(self):
        return bool(ctypes.c_uint.in_dll(LIB, "test_dma_control").value & 0x01000000)


class ResetTests(GpuTest):
    def test_reset_graph_initialises_the_gpu_and_genv(self):
        ctypes.memset(GENV, 0x5A, 0x88)
        global_int("test_reset_callbacks").value = 0
        self.assertEqual(LIB.ResetGraph(0), 3)
        self.assertEqual(
            log(),
            [
                (LOG_GP0, 0x056888),  # GPU_cw(the function table's address)
                (LOG_DMA, 0), (LOG_DMA, 0), (LOG_DMA, 0x401),  # stop DMA2
                (LOG_GP1, 0x00000000),  # reset the GPU
                (LOG_GP1, 0x10000007),  # read the GPU version (2: retail)
            ],
        )
        self.assertEqual(global_int("test_reset_callbacks").value, 1)
        env = ctypes.string_at(GENV, 0x88)
        self.assertEqual(env[0], 3)
        self.assertEqual(env[GENV_QUEUE_MODE], 1)
        self.assertEqual(struct.unpack_from("<hh", env, GENV_VRAM_WIDTH), (0x400, 0x200))
        self.assertEqual(struct.unpack_from("<i", env, GENV_PENDING)[0], 0)
        self.assertEqual(struct.unpack_from("<Q", env, GENV_CALLBACK)[0], 0)
        self.assertEqual(env[GENV_DRAW:GENV_DRAW + 0x5C], b"\xff" * 0x5C)
        self.assertEqual(env[GENV_DISP:GENV_DISP + 0x14], b"\xff" * 0x14)

    def test_partial_reset_keeps_genv(self):
        LIB.SetGraphDebug(2)
        self.assertEqual(LIB.ResetGraph(1), 0)
        self.assertEqual(log(), [(LOG_DMA, 0), (LOG_DMA, 0), (LOG_DMA, 0x401), (LOG_GP1, 0x02000000), (LOG_GP1, 0x01000000)])
        self.assertEqual(LIB.GetGraphDebug(), 2)
        self.assertEqual(LIB.GetGraphType(), 3)

    def test_display_mask_and_video_mode(self):
        LIB.SetDispMask(1)
        LIB.SetDispMask(0)
        self.assertEqual(sent(LOG_GP1), [0x03000000, 0x03000001])
        self.assertEqual(LIB.SetVideoMode(1), 0)
        self.assertEqual(LIB.GetVideoMode(), 1)


class EnvironmentTests(GpuTest):
    def test_put_draw_env_sends_the_default_environment(self):
        env = DATA
        LIB.SetDefDrawEnv(ctypes.c_void_p(env), 0, 0, 320, 240)
        self.assertEqual(ctypes.string_at(env + 0x14, 5), bytes([10, 0, 1, 1, 0]))  # tpage, dtd, dfe, isbg
        self.assertEqual(LIB.PutDrawEnv(ctypes.c_void_p(env)), env)
        codes = [0xE3000000, 0xE403BD3F, 0xE5000000, 0xE100060A, 0xE2000000, 0xE6000000]
        self.assertEqual(word(env + 0x1C), 0x06FFFFFF)
        self.assertEqual(sent(LOG_DMA), [env + 0x1C, 0, 0x01000401])
        self.assertEqual(sent(LOG_LIST_GP0), codes)
        self.assertTrue(self.busy())
        self.assertEqual(ctypes.string_at(GENV + GENV_DRAW, 0x5C), ctypes.string_at(env, 0x5C))
        LIB.GetDrawEnv(ctypes.c_void_p(DATA + 0x100))
        self.assertEqual(ctypes.string_at(DATA + 0x100, 0x5C), ctypes.string_at(env, 0x5C))

    def test_background_fill_of_set_draw_env2(self):
        env = DATA
        LIB.SetDefDrawEnv(ctypes.c_void_p(env), 64, 8, 256, 240)
        half(env + 8, 64)  # offset (64, 8)
        ctypes.memmove(env + 0x18, bytes([1, 0x10, 0x20, 0x30]), 4)  # isbg, r, g, b
        LIB.SetDrawEnv2(ctypes.c_void_p(env + 0x1C), ctypes.c_void_p(env))
        self.assertEqual(ctypes.c_uint8.from_address(env + 0x1F).value, 9)
        self.assertEqual(words(env + 0x1C + 0x1C, 3), [0x02302010, 0x00080040, 0x00F00100])
        # Unaligned: a rectangle relative to the drawing offset.
        LIB.SetDrawEnv(ctypes.c_void_p(env + 0x1C), ctypes.c_void_p(env))
        self.assertEqual(words(env + 0x1C + 0x1C, 3), [0x60302010, 0x00000000, 0x00F00100])

    def test_draw_otag_env_links_the_environment_to_the_table(self):
        env, ot = DATA, DATA + 0x200
        LIB.SetDefDrawEnv(ctypes.c_void_p(env), 0, 0, 320, 240)
        LIB.ClearOTagR(ctypes.c_void_p(ot), 2)
        LIB.DrawOTagEnv(ctypes.c_void_p(ot + 4), ctypes.c_void_p(env))
        self.assertEqual(word(env + 0x1C), 0x06000000 | (ot + 4))
        self.assertEqual(sent(LOG_LIST_GP0), words(env + 0x20, 6) + [0, 0, 0, 0])

    def test_put_disp_env_programs_the_display(self):
        env = DATA
        LIB.SetDefDispEnv(ctypes.c_void_p(env), 0, 0, 320, 240)
        LIB.PutDispEnv(ctypes.c_void_p(env))
        self.assertEqual(sent(LOG_GP1), [0x05000000, 0x06C60260, 0x07040010, 0x08000001])
        LIB.test_reset_log()
        half(env + 2, 240)
        LIB.PutDispEnv(ctypes.c_void_p(env))
        self.assertEqual(sent(LOG_GP1), [0x0503C000, 0x08000001])
        LIB.test_reset_log()
        LIB.PutDispEnv(ctypes.c_void_p(env))
        self.assertEqual(sent(LOG_GP1), [0x0503C000])
        LIB.GetDispEnv(ctypes.c_void_p(DATA + 0x100))
        self.assertEqual(ctypes.string_at(DATA + 0x100, 0x14), ctypes.string_at(env, 0x14))

    def test_put_disp_env_pal_interlaced_640(self):
        env = DATA
        LIB.SetVideoMode(1)
        LIB.SetDefDispEnv(ctypes.c_void_p(env), 0, 0, 640, 480)
        ctypes.c_uint8.from_address(env + 0x10).value = 1  # isinter
        rect(env + 8, 0, 0, 256, 0)  # screen width 256 clocks*10, default height
        LIB.PutDispEnv(ctypes.c_void_p(env))
        # h: 0x260 to 0x260 + 2560; v: 0x13 to 0x13 + 0xF0; mode 640, PAL, interlaced, 480 lines.
        self.assertEqual(sent(LOG_GP1), [0x05000000, 0x06C60260, 0x07040C13, 0x0800002F])
        self.assertEqual(ctypes.c_uint8.from_address(env + 0x12).value, 1)


class TransferTests(GpuTest):
    def test_clear_image_aligned_fills(self):
        r = rect(DATA, 0, 0, 0x180, 0x1E0)
        self.assertEqual(LIB.ClearImage(ctypes.c_void_p(r), 0, 0, 0), 0)
        packet = SYMBOLS["libgpu_clear_packet"]
        self.assertEqual(words(packet, 6), [0x05FFFFFF, 0xE6000000, 0xE1000000, 0x02000000, 0, 0x01E00180])
        self.assertEqual(sent(LOG_DMA), [packet, 0, 0x01000401])
        self.assertEqual(sent(LOG_LIST_GP0), words(packet + 4, 5))

    def test_clear_image_unaligned_draws_and_restores_the_area(self):
        params = (ctypes.c_uint * 8).in_dll(LIB, "test_param")
        params[3], params[4], params[5] = 0x11, 0x22, 0x33
        ctypes.c_uint.in_dll(LIB, "test_status").value |= 0x0405
        r = rect(DATA, 8, 16, 0x7FF, 50)
        LIB.ClearImage2(ctypes.c_void_p(r), 1, 2, 3)
        self.assertEqual(half(r + 4), 0x3FF)  # clamped in place
        self.assertEqual(sent(LOG_GP1), [0x10000003, 0x10000004, 0x10000005, 0x04000002])
        self.assertEqual(
            sent(LOG_LIST_GP0),
            [0xE3000000, 0xE4FFFFFF, 0xE5000000, 0xE6000000, 0xE1000405, 0x60030201, 0x00100008, 0x003203FF,
             0xE3000011, 0xE4000022, 0xE5000033],
        )

    def test_load_image_sends_blocks_by_dma_and_the_rest_by_cpu(self):
        r, pixels = rect(DATA, 0x100, 0x20, 16, 2), DATA + 0x100
        LIB.LoadImage(ctypes.c_void_p(r), ctypes.c_void_p(pixels))
        self.assertEqual(
            log(),
            [(LOG_GP1, 0x04000000), (LOG_GP0, 0x01000000), (LOG_GP0, 0xA0000000), (LOG_GP0, 0x00200100),
             (LOG_GP0, 0x00020010), (LOG_GP1, 0x04000002), (LOG_DMA, pixels), (LOG_DMA, 0x00010010),
             (LOG_DMA, 0x01000201)],
        )
        LIB.DrawSync(0)
        LIB.test_reset_log()
        put(pixels, range(0x100, 0x108))
        # A 16-entry CLUT is 8 words, all by the CPU.
        self.assertEqual(LIB.LoadClut2(ctypes.c_void_p(pixels), 0, 0x1F0), 0x1F0 << 6)
        self.assertEqual(sent(LOG_GP0), [0x01000000, 0xA0000000, 0x01F00000, 0x00010010] + list(range(0x100, 0x108)))
        self.assertEqual(sent(LOG_DMA), [])

    def test_store_image_reads_into_memory(self):
        status = ctypes.c_uint.in_dll(LIB, "test_status")
        status.value |= 0x08000000
        reads = (ctypes.c_uint * 64).in_dll(LIB, "test_read_words")
        for i in range(64):
            reads[i] = 0x1000 + i
        r, out = rect(DATA, 0, 0, 2, 2), DATA + 0x100
        LIB.StoreImage(ctypes.c_void_p(r), ctypes.c_void_p(out))
        self.assertEqual(words(out, 2), [0x1000, 0x1001])
        self.assertEqual(sent(LOG_GP0), [0x01000000, 0xC0000000, 0, 0x00020002])

    def test_move_image_uses_the_sdk_packet(self):
        r = rect(DATA, 0, 0, 320, 240)
        self.assertEqual(LIB.MoveImage(ctypes.c_void_p(r), 320, 8), 0)
        head = SYMBOLS["libgpu_move_image_source"] - 8
        self.assertEqual(sent(LOG_DMA), [head, 0, 0x01000401])
        self.assertEqual(sent(LOG_LIST_GP0), [0x80000000, 0, 0x00080140, 0x00F00140])
        self.assertEqual(LIB.MoveImage(ctypes.c_void_p(rect(DATA, 0, 0, 0, 8)), 0, 0), -1)


class QueueTests(GpuTest):
    def test_draw_sync_callback_runs_from_the_completion_interrupt(self):
        callback = ctypes.cast(LIB.test_draw_sync_callback, ctypes.c_void_p).value
        LIB.DrawSyncCallback(ctypes.c_void_p(callback))
        ot, tile = DATA, DATA + 0x40
        LIB.ClearOTagR(ctypes.c_void_p(ot), 4)
        put(tile, [0, 0x60FF0000, 0x00100010, 0x00080008])
        LIB.SetTile(ctypes.c_void_p(tile))
        LIB.AddPrim(ctypes.c_void_p(ot + 8), ctypes.c_void_p(tile))
        # A callback waits: the work is queued and runs at once on a free DMA.
        LIB.DrawOTag(ctypes.c_void_p(ot + 12))
        self.assertTrue(self.busy())
        self.assertEqual(sent(LOG_LIST_GP0), [0x60FF0000, 0x00100010, 0x00080008, 0, 0, 0, 0])
        self.assertEqual(global_int("test_draw_sync_callbacks").value, 0)
        # Busy: the next transfer waits in the queue with a copy of its RECT.
        r = rect(DATA + 0x80, 0, 0, 3, 1)
        self.assertEqual(LIB.LoadImage(ctypes.c_void_p(r), ctypes.c_void_p(DATA + 0x100)), 1)
        self.assertEqual(sent(LOG_GP0), [])
        self.assertEqual(LIB.DrawSync(1), 1)
        self.assertEqual(LIB.DrawSync(0), 0)
        # One wait ended the transfer; its interrupt ran the queue, then the callback.
        self.assertEqual(global_int("test_yields").value, 1)
        self.assertEqual(sent(LOG_GP0)[:4], [0x01000000, 0xA0000000, 0, 0x00010003])
        self.assertEqual(global_int("test_draw_sync_callbacks").value, 1)
        self.assertEqual(word(SYMBOLS["libgpu_queue_read_index"]), word(SYMBOLS["libgpu_queue_write_index"]))
        self.assertEqual(LIB.DrawSync(1), 0)

    def test_without_a_callback_work_runs_at_once(self):
        ot = DATA
        LIB.ClearOTag(ctypes.c_void_p(ot), 2)
        self.assertEqual(words(ot, 2), [(ot + 4) & 0xFFFFFF, SYMBOLS["libgpu_clear_otag_terminator"]])
        LIB.DrawOTag(ctypes.c_void_p(ot))
        self.assertEqual(word(SYMBOLS["libgpu_queue_write_index"]), 0)
        self.assertTrue(self.busy())
        LIB.DrawSync(0)
        self.assertEqual(global_int("test_yields").value, 1)
        self.assertFalse(self.busy())

    def test_draw_prim_waits_and_sends_by_cpu(self):
        tile = DATA
        put(tile, [0, 0x60010203, 0x00200010, 0x00040004])
        LIB.SetTile(ctypes.c_void_p(tile))
        LIB.DrawPrim(ctypes.c_void_p(tile))
        self.assertEqual(log(), [(LOG_GP1, 0x04000000), (LOG_GP0, 0x60010203), (LOG_GP0, 0x00200010), (LOG_GP0, 0x00040004)])


class PacketTests(GpuTest):
    def test_primitive_headers(self):
        cases = {
            "SetPolyF3": (4, 0x20), "SetPolyFT3": (7, 0x24), "SetPolyG3": (6, 0x30), "SetPolyGT3": (9, 0x34),
            "SetPolyF4": (5, 0x28), "SetPolyFT4": (9, 0x2C), "SetPolyG4": (8, 0x38), "SetPolyGT4": (12, 0x3C),
            "SetSprt8": (3, 0x74), "SetSprt16": (3, 0x7C), "SetSprt": (4, 0x64), "SetTile1": (2, 0x68),
            "SetTile8": (2, 0x70), "SetTile16": (2, 0x78), "SetTile": (3, 0x60), "SetLineF2": (3, 0x40),
            "SetLineG2": (4, 0x50), "SetLineF3": (5, 0x48), "libgpu_set_line_g3": (7, 0x58),
            "SetLineF4": (6, 0x4C), "libgpu_set_line_g4": (9, 0x5C),
        }
        terminators = {"SetLineF3": 5, "libgpu_set_line_g3": 7, "SetLineF4": 6, "libgpu_set_line_g4": 9}
        for name, (length, code) in cases.items():
            ctypes.memset(DATA, 0xEE, 64)
            word(DATA, 0x00123456)
            getattr(LIB, name)(ctypes.c_void_p(DATA))
            self.assertEqual(word(DATA), length << 24 | 0x123456, name)
            self.assertEqual(ctypes.c_uint8.from_address(DATA + 7).value, code, name)
            if name in terminators:
                self.assertEqual(word(DATA + 4 * terminators[name]), 0x55555555, name)
        LIB.SetPolyFT4(ctypes.c_void_p(DATA))
        LIB.SetSemiTrans(ctypes.c_void_p(DATA), 1)
        LIB.SetShadeTex(ctypes.c_void_p(DATA), 1)
        self.assertEqual(ctypes.c_uint8.from_address(DATA + 7).value, 0x2F)
        LIB.SetSemiTrans(ctypes.c_void_p(DATA), 0)
        self.assertEqual(ctypes.c_uint8.from_address(DATA + 7).value, 0x2D)

    def test_ordering_table_links(self):
        ot, a, b = DATA, DATA + 0x40, DATA + 0x80
        LIB.ClearOTagR(ctypes.c_void_p(ot), 3)
        terminator = SYMBOLS["libgpu_clear_otag_terminator"]
        self.assertEqual(words(ot, 3), [terminator, ot, ot + 4])
        put(a, [0x05000000])
        LIB.AddPrim(ctypes.c_void_p(ot + 8), ctypes.c_void_p(a))
        self.assertEqual(words(ot + 8, 1), [a])
        self.assertEqual(word(a), 0x05000000 | (ot + 4))
        self.assertEqual(LIB.NextPrim(ctypes.c_void_p(a)), 0x80000000 | (ot + 4))
        LIB.AddPrims(ctypes.c_void_p(ot), ctypes.c_void_p(b), ctypes.c_void_p(a))
        self.assertEqual(word(ot), b)
        self.assertEqual(word(a), 0x05000000 | terminator)
        LIB.CatPrim(ctypes.c_void_p(b), ctypes.c_void_p(a))
        self.assertEqual(word(b) & 0xFFFFFF, a)
        self.assertFalse(LIB.IsEndPrim(ctypes.c_void_p(b)))
        LIB.TermPrim(ctypes.c_void_p(b))
        self.assertTrue(LIB.IsEndPrim(ctypes.c_void_p(b)))
        put(a, [0x03000000])
        put(b, [0x04000000])
        self.assertEqual(LIB.MargePrim(ctypes.c_void_p(a), ctypes.c_void_p(b)), 0)
        self.assertEqual(word(a) >> 24, 8)
        self.assertEqual(word(b), 0)
        put(b, [0x0C000000])
        self.assertEqual(LIB.MargePrim(ctypes.c_void_p(a), ctypes.c_void_p(b)), -1)

    def test_resident_ot_link_helpers(self):
        counts = {
            "gpu_ot_link_tile_1": 2, "gpu_ot_link_tile_8": 2, "gpu_ot_link_tile_16": 2, "gpu_ot_link_tile": 3,
            "gpu_ot_link_sprt_8": 3, "gpu_ot_link_sprt_16": 3, "gpu_ot_link_sprt": 4, "gpu_ot_link_line_f2": 3,
            "gpu_ot_link_line_g2": 4, "gpu_ot_link_line_f3": 5, "gpu_ot_link_line_g3": 7, "gpu_ot_link_line_f4": 6,
            "gpu_ot_link_line_g4": 9, "gpu_ot_link_poly_f3": 4, "gpu_ot_link_poly_ft3": 7, "gpu_ot_link_poly_g3": 6,
            "gpu_ot_link_poly_gt3": 9, "gpu_ot_link_poly_f4": 5, "gpu_ot_link_poly_ft4": 9, "gpu_ot_link_poly_g4": 8,
            "gpu_ot_link_11_words": 11,
        }
        ot, packet = DATA, DATA + 0x40
        for name, count in counts.items():
            # The entry's previous contents are not masked.
            put(ot, [0x01ABCDEF])
            put(packet, [0xFFFFFFFF])
            getattr(LIB, name)(ctypes.c_void_p(ot), ctypes.c_void_p(packet))
            self.assertEqual(word(ot), packet, name)
            self.assertEqual(word(packet), 0x01ABCDEF | count << 24, name)

    def test_drawing_mode_packets(self):
        p = DATA
        self.assertEqual(LIB.GetTPage(2, 1, 640, 256), 0x100 | 0x20 | 0x10 | 0xA)
        self.assertEqual(LIB.GetClut(256, 480), 480 << 6 | 16)
        tw = rect(DATA + 0x80, 64, 32, 32, 16)
        LIB.SetDrawMode(ctypes.c_void_p(p), 1, 0, 0x1A, ctypes.c_void_p(tw))
        self.assertEqual(words(p, 3), [0x02000000, 0xE100041A, 0xE2000000 | 4 << 15 | 8 << 10 | 30 << 5 | 28])
        LIB.SetDrawMode(ctypes.c_void_p(p), 0, 1, 0x1A, None)
        self.assertEqual(words(p + 4, 2), [0xE100021A, 0])
        LIB.SetTexWindow(ctypes.c_void_p(p), ctypes.c_void_p(tw))
        self.assertEqual(words(p + 4, 2), [0xE2000000 | 4 << 15 | 8 << 10 | 30 << 5 | 28, 0])
        LIB.SetDrawArea(ctypes.c_void_p(p), ctypes.c_void_p(rect(DATA + 0x90, 0, 256, 2000, 256)))
        self.assertEqual(words(p, 3), [0x02000000, 0xE3040000, 0xE407FFFF])
        offsets = DATA + 0xA0
        half(offsets, 160)
        half(offsets + 2, -8)
        LIB.SetDrawOffset(ctypes.c_void_p(p), ctypes.c_void_p(offsets))
        self.assertEqual(words(p + 4, 2), [0xE5000000 | (0x7F8 << 11) | 160, 0])
        LIB.SetPriority(ctypes.c_void_p(p), 1, 1)
        self.assertEqual(words(p + 4, 2), [0xE6000003, 0])
        LIB.SetDrawTPage(ctypes.c_void_p(p), 1, 1, 0xFFFF)
        self.assertEqual(word(p) >> 24, 1)
        self.assertEqual(word(p + 4), 0xE1000200 | 0x9FF | 0x400)

    def test_move_and_load_packets(self):
        p = DATA
        r = rect(DATA + 0x80, 16, 32, 64, 48)
        LIB.SetDrawMove(ctypes.c_void_p(p), ctypes.c_void_p(r), 100, 200)
        self.assertEqual(words(p, 6), [0x05000000, 0x01000000, 0x80000000, 0x00200010, 0x00C80064, 0x00300040])
        LIB.SetDrawMove(ctypes.c_void_p(p), ctypes.c_void_p(rect(r, 0, 0, 0, 4)), 0, 0)
        self.assertEqual(word(p) >> 24, 0)
        put(p, [0] * 20)
        LIB.SetDrawLoad(ctypes.c_void_p(p), ctypes.c_void_p(rect(r, 1, 2, 4, 3)))
        self.assertEqual(words(p, 4), [0x0A000000, 0xA0000000, 0x00020001, 0x00030004])
        self.assertEqual(word(p + 4 * 10), 0x01000000)
        LIB.SetDrawLoad(ctypes.c_void_p(p), ctypes.c_void_p(rect(r, 0, 0, 16, 16)))
        self.assertEqual(word(p), 0x01000000)  # too large: length 0, GP0(01h) over the tag

    def test_default_environments(self):
        env = DATA
        LIB.SetVideoMode(1)
        LIB.SetDefDrawEnv(ctypes.c_void_p(env), 0, 0, 512, 289)
        self.assertEqual(ctypes.c_uint8.from_address(env + 0x17).value, 0)
        LIB.SetDefDrawEnv(ctypes.c_void_p(env), 0, 0, 512, 288)
        self.assertEqual(ctypes.c_uint8.from_address(env + 0x17).value, 1)
        LIB.SetVideoMode(0)
        ctypes.memset(env, 0x77, 0x14)
        LIB.SetDefDispEnv(ctypes.c_void_p(env), 1, 2, 3, 4)
        self.assertEqual(ctypes.string_at(env, 0x14), struct.pack("<4h4h4B", 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0))


class TimTests(GpuTest):
    def test_read_tim_walks_clut_and_pixel_blocks(self):
        tim = DATA
        clut = [44, 0x01E00000, 0x00010010] + list(range(8))
        pixels = [28, 0x00000140, 0x00020002, 1, 2, 3, 4]
        put(tim, [0x10, 0x08] + clut + pixels)
        image = DATA + 0x100
        self.assertEqual(LIB.OpenTIM(ctypes.c_void_p(tim)), 0)
        self.assertEqual(LIB.ReadTIM(ctypes.c_void_p(image)), image)
        mode, crect, caddr, prect, paddr = struct.unpack("<I4xQQQQ", ctypes.string_at(image, 40))
        self.assertEqual(
            (mode, crect, caddr, prect, paddr), (8, tim + 12, tim + 20, tim + 56, tim + 64)
        )
        self.assertEqual(word(SYMBOLS["libgpu_tim_cursor"]), tim + 4 * 20)
        put(tim, [0x11])
        LIB.OpenTIM(ctypes.c_void_p(tim))
        self.assertIsNone(LIB.ReadTIM(ctypes.c_void_p(image)))
        put(tim, [0x10, 0x02] + pixels)
        self.assertEqual(LIB.get_tim_addr(ctypes.c_void_p(tim), ctypes.c_void_p(image)), 9)
        self.assertEqual(struct.unpack("<QQ", ctypes.string_at(image + 8, 16)), (0, 0))


if __name__ == "__main__":
    unittest.main()
