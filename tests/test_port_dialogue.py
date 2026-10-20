"""The field's dialogue windows hand the host the run of packets they linked."""
import ctypes
import struct
import tempfile
import unittest
from pathlib import Path

from tests import port_native

# The ordering-table entry the windows link at, the stand-in's packets
# (0x100 apart, each linked as addPrim links it) and a packet the entry
# held before they drew.
OT = 0x80010000
PACKETS = 0x80020000
BEFORE = 0x80030000
END = 0xFFFFFF


class DialogueRun(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not port_native.CLANG:
            raise unittest.SkipTest("needs the pinned runtime clang")
        cls.build = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build.cleanup)
        support = Path(cls.build.name) / "dialogue_support.c"
        # A stand-in for the original that links `test_links` packets, and
        # a host that keeps what it was handed.
        support.write_text('''#include "common.h"
#include "psyq/libgpu.h"
#include "xem/memory.h"
u32 test_links;
u32 test_records;
u32 test_kind;
u32 test_length;
u32 test_run[2];
void xem_original_field_dialogue_draw_windows(u_long *ot, s32 buffer) {
    u32 i;

    for (i = 0; i < test_links; i++) {
        addPrim(ot, (u_long *)(XemUintptr)(0x80020000u + i * 0x100));
    }
}
void xem_host_present(u32 kind, void *data, u32 length) {
    test_records++;
    test_kind = kind;
    test_length = length;
    test_run[0] = ((u32 *)data)[0];
    test_run[1] = ((u32 *)data)[1];
}
''')
        cls.lib = port_native.build(cls.build.name, ["src/port/dialogue.c", str(support)])
        cls.lib.field_dialogue_draw_windows.argtypes = [ctypes.c_void_p, ctypes.c_int]

    def word(self, name):
        return ctypes.c_uint.in_dll(self.lib, name)

    def draw(self, links, before):
        """The windows linking `links` packets at an entry holding `before`:
        the records the host got, the last one's kind, length and run, and
        the entry's and packets' words after."""
        port_native.clear()
        port_native.write(OT, struct.pack("<I", before))
        for name in ("test_records", "test_kind", "test_length"):
            self.word(name).value = 0
        self.word("test_links").value = links
        self.lib.field_dialogue_draw_windows(OT, 0)
        run = (ctypes.c_uint * 2).in_dll(self.lib, "test_run")
        words = [struct.unpack("<I", port_native.read(address, 4))[0] for address in [OT] + [PACKETS + i * 0x100 for i in range(links)]]
        return self.word("test_records").value, self.word("test_kind").value, self.word("test_length").value, tuple(run), words

    def test_the_run_is_its_newest_packet_to_the_one_linked_to_what_the_entry_held(self):
        records, kind, length, run, words = self.draw(3, END)
        self.assertEqual((records, kind, length), (1, 6, 8), "one record of kind 6, two words")
        newest, oldest = (PACKETS + 0x200) & END, PACKETS & END
        self.assertEqual(run, (newest, oldest))
        # The links as the original made them: entry, newest, ..., oldest, end.
        self.assertEqual([word & END for word in words], [newest, END, PACKETS & END, (PACKETS + 0x100) & END])

    def test_packets_linked_there_before_stay_out_of_the_run(self):
        _, _, _, run, words = self.draw(2, BEFORE & END)
        self.assertEqual(run, ((PACKETS + 0x100) & END, PACKETS & END))
        self.assertEqual(words[1] & END, BEFORE & END, "the oldest links to what the entry held")

    def test_one_packet_is_a_run_of_one(self):
        _, _, _, run, _ = self.draw(1, END)
        self.assertEqual(run, (PACKETS & END, PACKETS & END))

    def test_nothing_linked_hands_the_host_nothing(self):
        records, _, _, _, words = self.draw(0, BEFORE & END)
        self.assertEqual(records, 0)
        self.assertEqual(words[0] & END, BEFORE & END)


if __name__ == "__main__":
    unittest.main()
