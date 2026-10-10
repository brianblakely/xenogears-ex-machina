"""The port's libspu, libcard and BIOS calls (port/libspu.c, port/libcard.c,
port/bios.c).

The C is built for the host as a freestanding shared library and driven
through ctypes against tests/port_spu_card/support.c, which stands in for the
game's data at its original addresses, the register window and the host
imports. Expected register values are worked from the original libspu code
(the resident's generated assembly, read privately) and psx-spx; the reverb
presets below are invented. Run inside `nix develop path:./nix/runtime`.
"""

import ctypes
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLANG = os.environ.get("XEM_CLANG") or shutil.which("clang")
LIB = None
BUILD = None

WAIT_ENTRY = 0x8004CB3C
KANJI_FONT = 0x1FC66000


def build_library(out):
    common = [
        "-O2", "-fPIC", "-ffreestanding", "-fno-builtin", "-nostdinc", "-std=gnu89",
        "-funsigned-char", "-fwrapv", "-fno-strict-aliasing", "-Wall", "-Werror",
        "-Wno-unused-function", "-Iport/include", "-Idecomp/include",
    ]
    objects = []
    for source, extra in (
        # The PS1's long is 32 bits (SpuCommonAttr, DIRENTRY).
        ("port/libspu.c", ["-Dlong=int"]),
        ("port/libcard.c", ["-Dlong=int"]),
        ("port/bios.c", ["-Dlong=int"]),
        ("tests/port_spu_card/support.c", []),
    ):
        obj = os.path.join(out, os.path.basename(source) + ".o")
        subprocess.run([CLANG, *common, *extra, "-c", source, "-o", obj], cwd=ROOT, check=True)
        objects.append(obj)
    library = os.path.join(out, "libxemspucard.so")
    subprocess.run([CLANG, "-shared", "-nostdlib", "-fuse-ld=lld", *objects, "-o", library], cwd=ROOT, check=True)
    return ctypes.CDLL(library)


def setUpModule():
    global LIB, BUILD
    if not CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = build_library(BUILD.name)
    for name in ("SpuSetTransferStartAddr", "SpuWrite", "SpuRead", "Krom2RawAdd", "SpuSetTransferCallback",
                 "SpuSetIRQCallback"):
        getattr(LIB, name).restype = ctypes.c_uint32
    LIB.SpuSetTransferStartAddr.argtypes = [ctypes.c_uint32]
    LIB.Krom2RawAdd.argtypes = [ctypes.c_uint32]
    LIB.SpuSetTransferCallback.argtypes = [ctypes.c_void_p]


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def var(ctype, name):
    return ctype.in_dll(LIB, name)


def window():
    return (ctypes.c_uint16 * 0x200).in_dll(LIB, "test_window")


def reg(offset):
    return window()[offset // 2]


def set_reg(offset, value):
    window()[offset // 2] = value


def reset():
    ctypes.memset(ctypes.addressof(window()), 0, 0x400)
    for name in ("test_sync_count", "test_transfer_count", "test_write_io_count", "test_event_count",
                 "test_debug_break_count", "test_callback_calls", "libspu_started", "test_yields"):
        var(ctypes.c_int, name).value = 0
    var(ctypes.c_void_p, "libspu_transfer_callback").value = None


def syncs():
    count = min(var(ctypes.c_int, "test_sync_count").value, 64)
    table = (ctypes.c_uint16 * 0x200 * 64).in_dll(LIB, "test_syncs")
    return [list(table[i]) for i in range(count)]


def transfers():
    count = var(ctypes.c_int, "test_transfer_count").value
    table = (ctypes.c_uint32 * 4 * 16).in_dll(LIB, "test_transfers")
    return [list(table[i]) for i in range(count)]


class SpuInit(unittest.TestCase):
    def setUp(self):
        reset()
        starts = (ctypes.c_int32 * 10).in_dll(LIB, "libspu_reverb_work_area_starts")
        starts[0] = 0xFFFE
        LIB.SpuInit()

    def test_registers_end_as_libspu_leaves_them(self):
        self.assertEqual(reg(0x1AA), 0xC000)  # SPU on, unmuted
        self.assertEqual(reg(0x1AC), 4)  # normal transfers
        self.assertEqual(reg(0x1A2), 0xFFFE)  # mBASE of reverb mode 0
        for voice in range(24):
            self.assertEqual(reg(voice * 16 + 4), 0x3FFF)
            self.assertEqual(reg(voice * 16 + 6), 0x200)
        self.assertEqual(var(ctypes.c_int, "libspu_memory_shift").value, 3)
        self.assertEqual(var(ctypes.c_int, "libspu_transfer_idle").value, 1)
        attributes = (ctypes.c_uint16 * 26).in_dll(LIB, "test_voice_attributes")
        self.assertEqual(list(attributes[2:]), [0xC000] * 24)

    def test_the_spu_sees_key_on_then_key_off_on_the_silent_block(self):
        seen = syncs()
        key_on = [i for i, w in enumerate(seen) if w[0x188 // 2] == 0xFFFF and w[0x18A // 2] == 0xFF]
        key_off = [i for i, w in enumerate(seen) if w[0x18C // 2] == 0xFFFF and w[0x18E // 2] == 0xFF]
        self.assertEqual(len(key_on), 1)
        self.assertTrue(any(i > key_on[0] for i in key_off))
        # The SPU was off (SPUCNT 0) while the voices were reset.
        self.assertEqual(seen[0][0x1AA // 2], 0)
        io = (ctypes.c_uint32 * 3 * 16).in_dll(LIB, "test_write_io")
        self.assertEqual(var(ctypes.c_int, "test_write_io_count").value, 1)
        self.assertEqual(list(io[0]), [0x1000, 1, 16])

    def test_spu_start_installs_the_transfer_entry_and_event(self):
        callbacks = (ctypes.c_void_p * 8).in_dll(LIB, "test_dma_callback")
        self.assertEqual(callbacks[4], WAIT_ENTRY)
        events = (ctypes.c_long * 4).in_dll(LIB, "test_open_event")
        self.assertEqual([events[0] & 0xFFFFFFFF, events[1], events[2]], [0xF0000009, 0x20, 0x2000])
        self.assertEqual(var(ctypes.c_int, "libspu_started").value, 1)


class Transfers(unittest.TestCase):
    def setUp(self):
        reset()
        LIB.SpuInit()
        var(ctypes.c_int, "test_transfer_count").value = 0
        self.buffer = (ctypes.c_uint8 * 0x1000)()

    def test_start_address_rounds_up_to_eight_bytes_within_spu_ram(self):
        self.assertEqual(LIB.SpuSetTransferStartAddr(0x1010), 0x1010)
        self.assertEqual(LIB.SpuSetTransferStartAddr(0x1011), 0x1018)
        self.assertEqual(var(ctypes.c_uint16, "libspu_transfer_start_address").value, 0x1018 >> 3)
        self.assertEqual(LIB.SpuSetTransferStartAddr(0x1000), 0)
        self.assertEqual(LIB.SpuSetTransferStartAddr(0x80000), 0)

    def test_a_write_moves_whole_blocks_and_completes_through_the_callback(self):
        LIB.SpuSetTransferMode(0)
        LIB.SpuSetTransferStartAddr(0x2000)
        callback = ctypes.cast(LIB.test_callback, ctypes.c_void_p).value
        self.assertIsNone(LIB.SpuSetTransferCallback(callback) or None)
        self.assertEqual(LIB.SpuWrite(self.buffer, 0x41), 0x41)
        address = ctypes.addressof(self.buffer) & 0xFFFFFFFF
        self.assertEqual(transfers(), [[0x2000, address, 0x80, 0]])
        self.assertEqual(reg(0x1AA) & 0x30, 0x20)  # DMA write mode
        self.assertEqual(reg(0x1A6), 0x2000 >> 3)
        LIB.xem_spu_transfer_done()
        self.assertEqual(reg(0x1AA) & 0x30, 0)
        self.assertEqual(var(ctypes.c_int, "test_callback_calls").value, 1)
        self.assertEqual(var(ctypes.c_int, "test_event_count").value, 0)

    def test_without_a_callback_the_transfer_event_is_delivered(self):
        LIB.SpuSetTransferStartAddr(0x2000)
        LIB.SpuRead(self.buffer, 0x40)
        self.assertEqual(transfers()[0][3], 1)
        self.assertEqual(reg(0x1AA) & 0x30, 0x30)
        self.assertEqual(var(ctypes.c_int, "libspu_transfer_idle").value, 0)
        LIB.xem_spu_transfer_done()
        events = (ctypes.c_uint32 * 2 * 16).in_dll(LIB, "test_events")
        self.assertEqual(list(events[0]), [0xF0000009, 0x20])

    def test_sizes_clamp_to_the_library_limit(self):
        LIB.SpuSetTransferStartAddr(0x1010)
        self.assertEqual(LIB.SpuWrite(self.buffer, 0x90000), 0x7EFF0)
        self.assertEqual(transfers()[0][2], 0x7F000)

    def test_io_mode_uses_the_fifo(self):
        LIB.SpuSetTransferMode(1)
        LIB.SpuSetTransferStartAddr(0x3000)
        var(ctypes.c_int, "test_write_io_count").value = 0
        LIB.SpuWrite(self.buffer, 0x20)
        self.assertEqual(transfers(), [])
        io = (ctypes.c_uint32 * 3 * 16).in_dll(LIB, "test_write_io")
        self.assertEqual([io[0][0], io[0][2]], [0x3000, 0x20])
        self.assertEqual(reg(0x1AA) & 0x30, 0)

    def test_decoded_data_reads_the_cd_capture_halves(self):
        LIB.SpuReadDecodedData(self.buffer, 5)
        LIB.SpuReadDecodedData(self.buffer, 6)
        LIB.SpuReadDecodedData(self.buffer, 0)
        base = ctypes.addressof(self.buffer) & 0xFFFFFFFF
        self.assertEqual(transfers(), [[0, base, 0x800, 1], [0x800, base + 0x800, 0x800, 1], [0, base, 0x1000, 1]])


class Attributes(unittest.TestCase):
    def setUp(self):
        reset()
        LIB.SpuInit()

    def common(self, mask, mvol=(0, 0), mode=(0, 0), cd=(0, 0), cd_reverb=0, cd_mix=0):
        attr = (ctypes.c_int16 * 20)()
        words = ctypes.cast(attr, ctypes.POINTER(ctypes.c_uint32))
        words[0] = mask
        attr[2], attr[3] = mvol
        attr[4], attr[5] = mode
        attr[8], attr[9] = cd
        words[5] = cd_reverb
        words[6] = cd_mix
        LIB.SpuSetCommonAttr(attr)

    def test_master_volume_fixed_and_sweep(self):
        self.common(0x3, mvol=(0x3FFF, -0x3FFF))
        self.assertEqual((reg(0x180), reg(0x182)), (0x3FFF, 0x4001))  # Wide's inverted right
        self.common(0xF, mvol=(0x200, -5), mode=(1, 7))
        self.assertEqual((reg(0x180), reg(0x182)), (0x807F, 0xE000))
        self.common(0x5, mvol=(0x1234, 0), mode=(9, 0))
        self.assertEqual(reg(0x180), 0x1234)

    def test_cd_input_volume_and_bits(self):
        self.common(0x3C0, cd=(0x7FFF, 0x7FFF), cd_reverb=0, cd_mix=1)
        self.assertEqual((reg(0x1B0), reg(0x1B2)), (0x7FFF, 0x7FFF))
        self.assertEqual(reg(0x1AA) & 0x5, 0x1)

    def test_envelope_status(self):
        status = ctypes.c_int32()
        level = ctypes.c_int16()
        set_reg(5 * 16 + 0xC, 0x1234)
        LIB.SpuGetVoiceEnvelopeAttr(5, ctypes.byref(status), ctypes.byref(level))
        self.assertEqual((status.value, level.value), (2, 0x1234))
        set_reg(5 * 16 + 0xC, 0)
        LIB.SpuGetVoiceEnvelopeAttr(5, ctypes.byref(status), ctypes.byref(level))
        self.assertEqual(status.value, 0)

    def test_irq_enable_and_acknowledge_reach_the_spu(self):
        var(ctypes.c_int, "test_sync_count").value = 0
        self.assertEqual(LIB.SpuSetIRQ(1), 1)
        self.assertEqual(LIB.SpuSetIRQ(0), 0)
        seen = syncs()
        self.assertEqual([w[0x1AA // 2] & 0x40 for w in seen], [0x40, 0])
        handler = ctypes.cast(LIB.test_callback, ctypes.c_void_p).value
        LIB.SpuSetIRQCallback(ctypes.c_void_p(handler))
        self.assertEqual((ctypes.c_void_p * 16).in_dll(LIB, "test_interrupt_callback")[9], handler)

    def test_noise_clock_clamps(self):
        self.assertEqual(LIB.SpuSetNoiseClock(0x50), 0x3F)
        self.assertEqual(reg(0x1AA) & 0x3F00, 0x3F00)
        self.assertEqual(LIB.SpuSetNoiseClock(-1), 0)


class Reverb(unittest.TestCase):
    def setUp(self):
        reset()
        LIB.SpuInit()
        params = (ctypes.c_uint8 * (68 * 10)).in_dll(LIB, "libspu_reverb_mode_parameters")
        for mode in range(10):
            entry = ctypes.cast(ctypes.addressof(params) + 68 * mode, ctypes.POINTER(ctypes.c_uint16))
            entry[0] = entry[1] = 0xFFFF  # a mask the type setter replaces with 0 (all)
            for i in range(32):
                entry[2 + i] = mode * 0x100 + i
        starts = (ctypes.c_int32 * 10).in_dll(LIB, "libspu_reverb_work_area_starts")
        for mode in range(10):
            starts[mode] = 0xF000 - mode * 0x100

    def test_mode_type_writes_the_preset_with_reverb_off_between(self):
        set_reg(0x1AA, reg(0x1AA) | 0x80)
        set_reg(0x184, 0x1111)
        var(ctypes.c_int, "test_sync_count").value = 0
        self.assertEqual(LIB.SpuSetReverbModeType(3), 0)
        self.assertEqual([reg(0x1C0 + 2 * i) for i in range(32)], [0x300 + i for i in range(32)])
        self.assertEqual(reg(0x1A2), 0xF000 - 0x300)
        self.assertEqual(reg(0x184), 0)
        self.assertEqual(reg(0x1AA) & 0x80, 0x80)
        self.assertEqual(syncs()[0][0x1AA // 2] & 0x80, 0)
        mode = ctypes.c_int32()
        LIB.SpuGetReverbModeType(ctypes.byref(mode))
        self.assertEqual(mode.value, 3)
        self.assertEqual(LIB.SpuSetReverbModeType(10), -1)

    def test_echo_delay_and_feedback(self):
        LIB.SpuSetReverbModeType(7)
        self.assertEqual(var(ctypes.c_int, "libspu_reverb_delay").value, 0x7F)
        LIB.SpuSetReverbModeDelayTime(0x40)
        d13, d12 = (0x40 << 13) // 127, (0x40 << 12) // 127
        base = 0x700
        expect = {10: d13 - base, 11: d12 - (base + 1), 12: base + 13 + d12, 16: base + 17 + d12,
                  26: base + 28 + d12, 27: base + 29 + d12}
        for i, value in expect.items():
            self.assertEqual(reg(0x1C0 + 2 * i), value & 0xFFFF, i)
        LIB.SpuSetReverbModeFeedback(0x7F)
        self.assertEqual(reg(0x1C0 + 14), 0x8100)
        # Other modes ignore both.
        LIB.SpuSetReverbModeType(2)
        LIB.SpuSetReverbModeFeedback(1)
        self.assertEqual(reg(0x1C0 + 14), 0x207)

    def test_depth(self):
        LIB.SpuSetReverbModeDepth(ctypes.c_short(-0x2800), ctypes.c_short(0x2800))
        self.assertEqual((reg(0x184), reg(0x186)), (0xD800, 0x2800))


class Card(unittest.TestCase):
    def setUp(self):
        reset()

    def test_file_calls_wait_out_the_cards_access_time(self):
        var(ctypes.c_int, "test_card_result").value = 7
        var(ctypes.c_int, "test_yields").value = 0
        name = ctypes.create_string_buffer(b"bu00:BASLUS-00664=")
        self.assertEqual(LIB.open(name, 1), 7)
        self.assertEqual(var(ctypes.c_int, "test_yields").value, 2)
        self.assertEqual(LIB.open(ctypes.create_string_buffer(b"cdrom:\\X;1"), 1), -1)
        self.assertEqual(LIB.delete(ctypes.create_string_buffer(b"tty:")), 0)

    def test_directory_calls_return_the_entry(self):
        LIB.firstfile.restype = ctypes.c_void_p
        entry = (ctypes.c_uint8 * 40)()
        var(ctypes.c_int, "test_card_result").value = 1
        self.assertEqual(LIB.firstfile(ctypes.create_string_buffer(b"bu10:"), entry), ctypes.addressof(entry))
        var(ctypes.c_int, "test_card_result").value = 0
        self.assertIsNone(LIB.firstfile(ctypes.create_string_buffer(b"bu10:"), entry))

    def test_card_events_reach_both_classes(self):
        self.assertEqual(LIB._card_info(0), 1)
        self.assertEqual(LIB._card_info(0x10), 0)
        LIB.xem_card_interrupt(0x2000)
        events = (ctypes.c_uint32 * 2 * 16).in_dll(LIB, "test_events")
        self.assertEqual([list(events[0]), list(events[1])], [[0xF0000011, 0x2000], [0xF4000001, 0x2000]])

    def test_wait_event_waits_for_the_event(self):
        var(ctypes.c_int, "test_yields").value = 0
        var(ctypes.c_int, "test_test_event_ready").value = 0
        self.assertEqual(LIB.WaitEvent(0xF1000003), 1)
        self.assertEqual(var(ctypes.c_int, "test_yields").value, 1)

    def test_get_gp_is_the_linkers_gp(self):
        LIB.GetGp.restype = ctypes.c_uint32
        self.assertEqual(LIB.GetGp(), ctypes.addressof(var(ctypes.c_uint8, "_gp")) & 0xFFFFFFFF)

    def test_pc_file_server_is_absent(self):
        name = ctypes.create_string_buffer(b"core")
        self.assertEqual(LIB.PCinit(), -1)
        self.assertEqual(LIB.PCcreat(name, 0), -1)
        self.assertEqual(LIB.PCwrite(3, name, 4), -1)
        codes = (ctypes.c_uint32 * 16).in_dll(LIB, "test_debug_breaks")
        self.assertEqual(list(codes[:3]), [0x101, 0x102, 0x106])


class Kanji(unittest.TestCase):
    """Glyph addresses worked from the JIS X 0208 assignment: row 1 has 94
    characters, row 2 53 (cells 1-14, 26-33, 42-48, 60-74, 82-89, 94), row 3
    62 (digits 16-25, A-Z 33-58, a-z 65-90), rows 4-8 83, 86, 48, 66 and 32;
    the first-level kanji follow from row 16 to row 47 cell 51."""

    def glyph(self, code):
        address = LIB.Krom2RawAdd(code)
        return None if address == 0xFFFFFFFF else (address - KANJI_FONT) // 30

    def test_non_kanji(self):
        self.assertEqual(self.glyph(0x8140), 0)  # ideographic space
        self.assertEqual(self.glyph(0x817E), 62)
        self.assertEqual(self.glyph(0x8180), 63)  # 0x7F is no trail byte
        self.assertEqual(self.glyph(0x819E), 93)
        self.assertEqual(self.glyph(0x819F), 94)  # row 2
        self.assertEqual(self.glyph(0x81AC), 107)
        self.assertIsNone(self.glyph(0x81AD))  # unassigned
        self.assertEqual(self.glyph(0x81B8), 108)
        self.assertEqual(self.glyph(0x81FC), 146)  # row 2 cell 94
        self.assertEqual(self.glyph(0x824F), 147)  # fullwidth 0
        self.assertEqual(self.glyph(0x8260), 157)  # fullwidth A
        self.assertEqual(self.glyph(0x8281), 183)  # fullwidth a
        self.assertEqual(self.glyph(0x829F), 209)  # hiragana small a
        self.assertEqual(self.glyph(0x84BE), 523)  # row 8 cell 32
        self.assertIsNone(self.glyph(0x84BF))
        self.assertIsNone(self.glyph(0x8540))

    def test_kanji(self):
        self.assertEqual(self.glyph(0x889F), 524)
        self.assertEqual(self.glyph(0x88FC), 524 + 93)
        self.assertEqual(self.glyph(0x8940), 524 + 94)
        self.assertEqual(self.glyph(0x9872), 3488)  # the last glyph, ending at ROM offset 0x7F8DE
        self.assertEqual(0x66000 + 3489 * 30, 0x7F8DE)
        self.assertIsNone(self.glyph(0x9873))
        self.assertIsNone(self.glyph(0x889E))
        self.assertIsNone(self.glyph(0x41))


if __name__ == "__main__":
    unittest.main()
