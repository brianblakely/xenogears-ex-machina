"""The port's LZSS decoder (port/lzss.c) against the contract of
decomp/src/resident/text_unpack_lzss.s and text_unpack_lzss_alloc.s and the
format reconstruction tools/analysis/packed.py, natively
(tests/port_native.py), on invented streams.
"""

import ctypes
import os
import random
import signal
import tempfile
import unittest

from tests import port_native as mem
from tools.analysis.packed import decode_block

LIB = None
BUILD = None

PACKED = 0x80100000
DEST = 0x80140000
PAGE = 0x1000


def setUpModule():
    global LIB, BUILD
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/lzss.c", "tests/port_resident/support.c"])
    LIB.text_unpack_lzss.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    LIB.text_unpack_lzss.restype = ctypes.c_void_p
    LIB.text_unpack_lzss_alloc.argtypes = [ctypes.c_void_p, ctypes.c_int]
    LIB.text_unpack_lzss_alloc.restype = ctypes.c_void_p


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def pack(tokens, size=None):
    """A stream of the format: the u32 size, then per eight tokens a flag
    byte (bit i set: token i is a reference) and the tokens: a literal byte,
    or a reference (distance & 0xFF, length - 3 << 4 | distance >> 8)."""
    data = bytearray()
    produced = 0
    for start in range(0, len(tokens), 8):
        group = tokens[start:start + 8]
        flags = 0
        body = bytearray()
        for i, token in enumerate(group):
            if token[0] == "lit":
                body.append(token[1])
                produced += 1
            else:
                _, distance, length = token
                flags |= 1 << i
                body += bytes([distance & 0xFF, (length - 3) << 4 | distance >> 8])
                produced += length
        data.append(flags)
        data += body
    return (produced if size is None else size).to_bytes(4, "little") + bytes(data)


def random_tokens(rng, groups):
    tokens, produced = [], 0
    for _ in range(groups * 8):
        if produced and rng.random() < 0.5:
            tokens.append(("ref", rng.randint(1, min(produced, 0xFFF)), rng.randint(3, 18)))
            produced += tokens[-1][2]
        else:
            tokens.append(("lit", rng.randrange(256)))
            produced += 1
    return tokens


class Lzss(unittest.TestCase):
    def setUp(self):
        mem.clear()

    def unpack(self, stream, prefill=0xEE, room=0x2000):
        mem.write(PACKED, stream)
        mem.fill(DEST, room, prefill)
        result = LIB.text_unpack_lzss(PACKED, DEST)
        self.assertEqual(result, DEST)
        return mem.read(DEST, room)

    def test_literals_and_overlapping_references(self):
        # 'a', then 18 bytes from one back (a run), 'b', 'c', then 5 from
        # three back ("abc" overlapping into its own output), then literals.
        tokens = [("lit", 0x61), ("ref", 1, 18), ("lit", 0x62), ("lit", 0x63), ("ref", 3, 5),
                  ("lit", 1), ("lit", 2), ("lit", 3)]
        stream = pack(tokens)
        want = b"a" * 19 + b"bc" + b"abcab" + b"\x01\x02\x03"
        got = self.unpack(stream)
        self.assertEqual(got[:len(want)], want)
        self.assertEqual(got[len(want)], 0xEE)
        self.assertEqual(decode_block(stream + b"\0").data, want)

    def test_random_streams_match_the_format_decoder(self):
        rng = random.Random(3)
        for case in range(40):
            stream = pack(random_tokens(rng, rng.randint(1, 30)))
            block = decode_block(stream + b"\x5A")
            with self.subTest(case=case):
                got = self.unpack(stream, room=len(block.data) + 16)
                self.assertEqual(got[:len(block.data)], block.data)
                self.assertEqual(got[len(block.data):], b"\xEE" * 16)
                # The final flag read is the byte after the stream.
                self.assertEqual(block.source_bytes_read, len(stream) + 1)

    def test_zero_size_reads_one_flag_byte_and_writes_nothing(self):
        got = self.unpack((0).to_bytes(4, "little"), room=16)
        self.assertEqual(got, b"\xEE" * 16)

    def test_zero_distance_copies_the_output_onto_itself(self):
        # Outside the format tool's checks: the copy source is the output
        # position, so the destination's own bytes stay.
        tokens = [("lit", 7), ("ref", 0, 4), ("lit", 8)] + [("lit", 9)] * 5
        got = self.unpack(pack(tokens), prefill=0x33, room=16)
        self.assertEqual(got[:11], bytes([7, 0x33, 0x33, 0x33, 0x33, 8, 9, 9, 9, 9, 9]))

    def test_end_is_tested_between_groups_only(self):
        # A size reached inside a group: the group's remaining tokens are
        # still decoded (here past the size); a size equal to a later group
        # boundary stops there.
        tokens = [("lit", i) for i in range(16)]
        got = self.unpack(pack(tokens, size=16), room=32)
        self.assertEqual(got[:17], bytes(range(16)) + b"\xEE")

    def test_final_flag_read_lies_past_the_stream(self):
        # The byte after the stream is read: on a guard page it faults, one
        # byte further on it does not. Run in a child process.
        stream = pack(random_tokens(random.Random(4), 4))
        guard = mem.RAM + mem.RAM_BYTES - PAGE
        self.assertEqual(self.child_unpack(stream, guard - len(stream)), -signal.SIGSEGV)
        self.assertEqual(self.child_unpack(stream, guard - len(stream) - 1), 0)

    def child_unpack(self, stream, address):
        pid = os.fork()
        if pid == 0:
            try:
                mem.write(address, stream)
                libc = ctypes.CDLL(None)
                libc.mprotect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
                if libc.mprotect(mem.RAM + mem.RAM_BYTES - PAGE, PAGE, 0) != 0:
                    os._exit(2)
                LIB.text_unpack_lzss(address, DEST)
                os._exit(0)
            except BaseException:
                os._exit(3)
        _, status = os.waitpid(pid, 0)
        return -os.WTERMSIG(status) if os.WIFSIGNALED(status) else os.WEXITSTATUS(status)

    def test_alloc_decodes_into_the_heap_block(self):
        stream = pack([("lit", 0x41)] * 8)
        mem.write(PACKED, stream)
        block = 0x80150000
        mem.fill(block, 16, 0xEE)
        ctypes.c_uint.in_dll(LIB, "test_heap_block").value = block
        self.assertEqual(LIB.text_unpack_lzss_alloc(PACKED, 0x26), block)
        self.assertEqual(ctypes.c_int.in_dll(LIB, "test_heap_size").value, 8)
        self.assertEqual(ctypes.c_int.in_dll(LIB, "test_heap_mode").value, 0x26)
        self.assertEqual(mem.read(block, 9), b"A" * 8 + b"\xEE")

    def test_alloc_failure_returns_null_without_decoding(self):
        stream = pack([("lit", 0x41)] * 8)
        mem.write(PACKED, stream)
        ctypes.c_uint.in_dll(LIB, "test_heap_block").value = 0
        calls = ctypes.c_int.in_dll(LIB, "test_heap_calls").value
        self.assertIsNone(LIB.text_unpack_lzss_alloc(PACKED, 1))
        self.assertEqual(ctypes.c_int.in_dll(LIB, "test_heap_calls").value, calls + 1)


if __name__ == "__main__":
    unittest.main()
