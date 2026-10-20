"""Presentation-only seams: cloud corner provenance, battle glyph class, the
camera record and a model's or sprite's placement.

Invented inputs exercise the actual port functions without original assets.
"""

import ctypes
import tempfile
import unittest
from pathlib import Path

from tests import port_native as mem
from tests.test_sprite_vm import function_body


class PresentationSeams(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not mem.CLANG:
            raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        source = '''
#include "common.h"
#include "xem/gte.h"
#include "xem/memory.h"
#include "resident/sprite.h"
unsigned int test_glyph_class, test_glyph_colour;
SpritePart *test_glyph;
void xem_original_battle_popup_draw_glyph(SpritePart *glyph, s32 colour) {
    test_glyph = glyph;
    test_glyph_colour = colour;
    test_glyph_class = xem_gte_ring->class_;
}
void xem_present_camera(MATRIX *m, s32 x, s32 y, s32 z, u32 mode);
/* A camera turned 30 degrees about y (rows in 4.12), its translation, and
 * the point a battle's looks at. */
void test_record_camera(void) {
    MATRIX m = {{{3547, 0, -2048}, {0, 4096, 0}, {2048, 0, 3547}}, {120, -40, 2600}};
    xem_present_camera(&m, 300, -200, 500, XEM_CAMERA_BATTLE);
}
'''
        for path, name, signature in (
            ("src/port/asm.c", "xem_asm_link_9_words", "void xem_asm_link_9_words(u32 *entry, u32 *packet)"),
            ("src/port/present.c", "battle_popup_draw_glyph", "void battle_popup_draw_glyph(SpritePart *glyph, s32 colour)"),
            ("src/port/present.c", "xem_record_entry",
             "void xem_record_entry(u32 class_, s32 sz, u32 owner, s32 x, s32 y, s32 z)"),
            ("src/port/present.c", "xem_present_camera", "void xem_present_camera(MATRIX *m, s32 x, s32 y, s32 z, u32 mode)"),
            ("src/port/present.c", "xem_record_placement", "void xem_record_placement(u32 owner, u32 packets, u32 size)"),
        ):
            source += "\n" + signature + " {" + function_body(path, name) + "\n"
        fixture = Path(cls.build.name) / "presentation.c"
        fixture.write_text(source)
        cls.lib = mem.build(cls.build.name, [str(fixture)])
        cls.lib.xem_asm_link_9_words.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.battle_popup_draw_glyph.argtypes = [ctypes.c_void_p, ctypes.c_int]
        cls.ring = (ctypes.c_uint * (8 + 65536 * 10)).in_dll(cls.lib, "xem_gte_ring_storage")
        cls.stores = (ctypes.c_uint * (4 + 65536 * 4)).in_dll(cls.lib, "xem_present_store_storage")
        cls.reveals = (ctypes.c_uint * 4).in_dll(cls.lib, "xem_present_reveal_storage")
        cls.capture = (ctypes.c_uint * 2).in_dll(cls.lib, "xem_present_capture_storage")

    def setUp(self):
        mem.clear()
        ctypes.memset(ctypes.addressof(self.ring), 0, ctypes.sizeof(self.ring))
        ctypes.memset(ctypes.addressof(self.stores), 0, ctypes.sizeof(self.stores))
        ctypes.memset(ctypes.addressof(self.reveals), 0, ctypes.sizeof(self.reveals))
        ctypes.memset(ctypes.addressof(self.capture), 0, ctypes.sizeof(self.capture))

    def test_cloud_links_keep_each_corner_source_when_screen_points_collide(self):
        self.ring[6] = 1  # recording
        entry, packet = 0x80100000, 0x80101000
        mem.put32(entry, 0x00112233)
        expected = []
        # Cross the projection ring's boundary. Every corner has the same
        # rounded SXY, at different depths, including another cloud later.
        for cloud in range(2):
            first = 65534 + cloud * 4
            for corner in range(4):
                projection = 8 + ((first + corner) % 65536) * 10
                self.ring[projection] = 0x00400080
                self.ring[projection + 1] = 300 + cloud * 1000 + corner * 10
                expected.append([(packet + 8 + corner * 8) & 0xFFFFFF,
                                 0x00400080, first + 4, first + corner + 1])
            self.ring[0] = first + 4
            self.lib.xem_asm_link_9_words(entry, packet)
            self.assertEqual(mem.u32(packet), (0x00112233 if cloud == 0 else 0x00101000) | 0x09000000)
            self.assertEqual(mem.u32(entry), packet & 0xFFFFFF)
            # Linking does not write the packet's corners ahead of the game.
            self.assertEqual(mem.u32(packet + 8), 0)
            packet += 40
        self.assertEqual(self.stores[0], 8)
        self.assertEqual([list(self.stores[4 + i * 4:8 + i * 4]) for i in range(8)], expected)

    def test_cloud_link_without_recording_only_links_the_original_packet(self):
        entry, packet = 0x80100000, 0x80101000
        mem.put32(entry, 0x00ABCDEF)
        self.lib.xem_asm_link_9_words(entry, packet)
        self.assertEqual(mem.u32(packet), 0x09ABCDEF)
        self.assertEqual(mem.u32(entry), 0x00101000)
        self.assertEqual(self.stores[0], 0)

    def test_a_camera_is_recorded_as_its_axes_translation_and_the_point_it_looks_at(self):
        self.ring[0] = 7  # entries written so far
        self.lib.test_record_camera()
        self.assertEqual(self.ring[0], 12, "five entries")

        def entry(i):
            # sxy, sz | class << 16, x, y, offset, h, owner, view[3]
            words = list(self.ring[8 + (7 + i) * 10:8 + (8 + i) * 10])
            view = [w - (1 << 32) if w >= 1 << 31 else w for w in words[7:10]]
            return words[1] & 0xFFFF, words[1] >> 16, words[6], view

        # The matrix's columns (axes 0-2), its translation (axis 3), then
        # the target with its depth through the camera, owned by the mode.
        columns = [[3547, 0, 2048], [0, 4096, 0], [-2048, 0, 3547]]
        for k in range(3):
            self.assertEqual(entry(k), (0, 6, k, columns[k]))
        self.assertEqual(entry(3), (0, 6, 3, [120, -40, 2600]))
        depth = ((2048 * 300 + 3547 * 500) >> 12) + 2600
        self.assertEqual(entry(4), (depth, 8, 2, [300, -200, 500]))
        # Made while the host looks from anywhere around the scene: the
        # mode with the host's `around` bits.
        self.reveals[3] = 2
        self.lib.test_record_camera()
        self.assertEqual(self.ring[8 + 16 * 10 + 6], 2 | 2 << 8)

    def test_a_placement_is_recorded_as_its_axes_translation_and_buffer_while_a_trace_takes_it(self):
        # The GTE's rotation (rows in 4.12: 30 degrees about y, packed two
        # halfwords a register) and translation, a model's buffer and its size.
        for register, word in enumerate([3547, (-2048 & 0xFFFF), 4096, 2048, 3547, 120, -40 & 0xFFFFFFFF, 2600]):
            self.lib.xem_gte_write_control(register, word)
        place = lambda: self.lib.xem_record_placement(0x80123450, 0x80140000, 0x200)
        place()
        self.assertEqual(self.ring[0], 0, "nothing while no trace takes the records")
        self.ring[6] = 1  # recording
        self.capture[0] = 1
        place()
        self.assertEqual(self.ring[0], 0, "nothing for a model drawn again for the record")
        self.capture[0] = 0
        place()
        self.assertEqual(self.ring[0], 4, "four entries")

        def entry(i):
            # sxy, sz | class << 16, x, y, offset, h, owner, view[3]
            words = list(self.ring[8 + i * 10:8 + (i + 1) * 10])
            view = [w - (1 << 32) if w >= 1 << 31 else w for w in words[7:10]]
            return words[1] & 0xFFFF, words[1] >> 16, words[6], view, words[2], words[3]

        # The rotation's columns (axes 0-2), then its translation with the
        # buffer and its size, owned by the model.
        columns = [[3547, 0, 2048], [0, 4096, 0], [-2048, 0, 3547]]
        for k in range(3):
            self.assertEqual(entry(k)[:4], (k, 11, 0x80123450, columns[k]))
        self.assertEqual(entry(3), (3, 11, 0x80123450, [120, -40, 2600], 0x80140000, 0x200))

    def test_battle_glyphs_are_interface_and_restore_the_callers_class(self):
        self.ring[1] = 2  # a surrounding sprite draw
        self.lib.battle_popup_draw_glyph(0x80100000, 0x2C808080)
        self.assertEqual(ctypes.c_uint.in_dll(self.lib, "test_glyph_class").value, 5)
        self.assertEqual(ctypes.c_uint.in_dll(self.lib, "test_glyph_colour").value, 0x2C808080)
        self.assertEqual(ctypes.c_void_p.in_dll(self.lib, "test_glyph").value, 0x80100000)
        self.assertEqual(self.ring[1], 2)
