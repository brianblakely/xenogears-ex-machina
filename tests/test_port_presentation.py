"""Presentation-only seams: cloud corner provenance and battle glyph class.

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
'''
        for path, name, signature in (
            ("src/port/asm.c", "xem_asm_link_9_words", "void xem_asm_link_9_words(u32 *entry, u32 *packet)"),
            ("src/port/present.c", "battle_popup_draw_glyph", "void battle_popup_draw_glyph(SpritePart *glyph, s32 colour)"),
        ):
            source += "\n" + signature + " {" + function_body(path, name) + "\n"
        fixture = Path(cls.build.name) / "presentation.c"
        fixture.write_text(source)
        cls.lib = mem.build(cls.build.name, [str(fixture)])
        cls.lib.xem_asm_link_9_words.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.battle_popup_draw_glyph.argtypes = [ctypes.c_void_p, ctypes.c_int]
        cls.ring = (ctypes.c_uint * (8 + 65536 * 10)).in_dll(cls.lib, "xem_gte_ring_storage")
        cls.stores = (ctypes.c_uint * (4 + 65536 * 4)).in_dll(cls.lib, "xem_present_store_storage")

    def setUp(self):
        mem.clear()
        ctypes.memset(ctypes.addressof(self.ring), 0, ctypes.sizeof(self.ring))
        ctypes.memset(ctypes.addressof(self.stores), 0, ctypes.sizeof(self.stores))

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

    def test_battle_glyphs_are_interface_and_restore_the_callers_class(self):
        self.ring[1] = 2  # a surrounding sprite draw
        self.lib.battle_popup_draw_glyph(0x80100000, 0x2C808080)
        self.assertEqual(ctypes.c_uint.in_dll(self.lib, "test_glyph_class").value, 5)
        self.assertEqual(ctypes.c_uint.in_dll(self.lib, "test_glyph_colour").value, 0x2C808080)
        self.assertEqual(ctypes.c_void_p.in_dll(self.lib, "test_glyph").value, 0x80100000)
        self.assertEqual(self.ring[1], 2)
