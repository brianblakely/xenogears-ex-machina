"""Invented ordering tables and GP0 words exercise the packet decoder's bounds."""

import struct
import unittest
from types import SimpleNamespace

from tools.analysis.gpu_packets import (
    RAM_BYTES,
    Census,
    census_ram,
    command_words,
    decode_command,
    decode_words,
    display_environment,
    draw_environment,
    evaluate,
    gte_controls,
    oversized_triangles,
    texpage,
    walk,
)


def xy(x: int, y: int) -> int:
    return (x & 0xFFFF) | (y & 0xFFFF) << 16


def put(ram: bytearray, address: int, *words: int) -> None:
    for i, value in enumerate(words):
        struct.pack_into("<I", ram, (address & 0x1FFFFF) + 4 * i, value & 0xFFFFFFFF)


def node(ram: bytearray, address: int, following: int, body: list[int]) -> None:
    put(ram, address, len(body) << 24 | (following & 0xFFFFFF), *body)


# An invented textured quad: page (320, 0) 8-bit semi mode 1, CLUT (16, 480).
FT4 = [
    0x2E808080,
    xy(-5, 2),
    0x78010000 | 0x0000,
    xy(20, 2),
    (0x0A5 | 1 << 5 | 1 << 7) << 16 | 0x0010,
    xy(-5, 30),
    0x1000,
    xy(20, 30),
    0x1010,
]


class CommandTests(unittest.TestCase):
    def test_lengths(self):
        self.assertEqual(command_words(0x2C, FT4, 0), 9)
        self.assertEqual(command_words(0x3C, [], 0), 12)  # Gouraud textured quad
        self.assertEqual(command_words(0x30, [], 0), 6)  # Gouraud triangle
        self.assertEqual(command_words(0x28, [], 0), 5)  # flat quad
        self.assertEqual(command_words(0x64, [], 0), 4)  # variable textured rectangle
        self.assertEqual(command_words(0x78, [], 0), 2)  # 16x16 flat rectangle
        self.assertEqual(command_words(0x40, [], 0), 3)  # flat line
        self.assertEqual(command_words(0x02, [], 0), 3)
        self.assertEqual(command_words(0xE1, [], 0), 1)

    def test_polyline_needs_terminator(self):
        words = [0x48FFFFFF, xy(0, 0), xy(1, 1), xy(2, 2), 0x55555555]
        self.assertEqual(command_words(0x48, words, 0), 5)
        with self.assertRaisesRegex(ValueError, "terminator"):
            command_words(0x48, words[:4], 0)

    def test_textured_quad(self):
        item = decode_command(FT4)
        self.assertEqual(item["class"], "poly4-flat-textured-modulated-semi")
        self.assertEqual(item["points"], [(-5, 2), (20, 2), (-5, 30), (20, 30)])
        self.assertEqual(item["clut"], (16, 480))
        page = texpage(item["texpage"])
        self.assertEqual(page, {"x": 320, "y": 0, "semi": 1, "depth": 8})
        self.assertEqual(item["uv"][1], (16, 0))

    def test_state_commands(self):
        mode = decode_command([0xE1000000 | 1 << 9 | 2 << 5 | 0x1A])
        self.assertEqual(mode["dither"], 1)
        self.assertEqual(texpage(mode["texpage"])["semi"], 2)
        window = decode_command([0xE2000000 | 0x1E | 0x1E << 5 | 4 << 10 | 30 << 15])
        self.assertEqual(
            window["window"], {"mask_x": 240, "mask_y": 240, "offset_x": 32, "offset_y": 240}
        )
        offset = decode_command([0xE5000000 | (-3 & 0x7FF) | 256 << 11])
        self.assertEqual(offset["offset"], (-3, 256))
        mask = decode_command([0xE6000003])
        self.assertEqual((mask["set_mask"], mask["check_mask"]), (1, 1))

    def test_truncated_packet(self):
        with self.assertRaisesRegex(ValueError, "truncated"):
            decode_words(FT4[:5])


class ListTests(unittest.TestCase):
    def setUp(self):
        self.ram = bytearray(RAM_BYTES)

    def test_walk_in_draw_order(self):
        node(self.ram, 0x80001000, 0x80001100, [0xE1000200 | 0x1A])
        node(self.ram, 0x80001100, 0x80001200, [])
        node(self.ram, 0x80001200, 0x00FFFFFF, FT4)
        commands, stats = walk(bytes(self.ram), 0x80001000)
        self.assertEqual([c["class"] for c in commands], ["draw-mode", FT4_CLASS])
        self.assertEqual(stats, {"nodes": 3, "empty_nodes": 1, "packets": 2})
        self.assertEqual(commands[1]["packet"], 0x80001200)

    def test_loop_and_bounds(self):
        node(self.ram, 0x80001000, 0x80001000, [])
        with self.assertRaisesRegex(ValueError, "loops"):
            walk(bytes(self.ram), 0x80001000)
        with self.assertRaisesRegex(ValueError, "outside"):
            walk(bytes(self.ram[:0x1000]), 0x80001000)

    def test_census_tracks_draw_mode(self):
        node(self.ram, 0x80002000, 0x80002100, FT4)
        node(self.ram, 0x80002100, 0x00FFFFFF, [0xE1000000 | 2 << 5, 0x7A000010, xy(1, 1)])
        commands, _ = walk(bytes(self.ram), 0x80002000)
        census = Census()
        census.add(commands, {"texpage": 0x0A, "dither": 1})
        result = census.result()
        self.assertEqual(
            result["semi_transparency"],
            {
                FT4_CLASS + " mode=1": 1,
                "rect-16x16-semi mode=2": 1,
            },
        )
        self.assertEqual(result["dither"], {FT4_CLASS + " dither=1": 1})
        self.assertEqual(result["cluts"], 1)
        self.assertEqual(result["vertex_x"], [-5, 20])

    def test_environments_and_expressions(self):
        base = 0x80003000
        struct.pack_into("<10h", self.ram, base & 0x1FFFFF, 0, 256, 320, 224, 0, 256, 0, 0, 0, 0)
        struct.pack_into("<HBBBBBB", self.ram, (base & 0x1FFFFF) + 0x14, 0x0A, 1, 1, 0, 0, 0, 0)
        put(self.ram, base + 0x1C, 2 << 24 | 0xFFFFFF, 0xE1000600 | 0x0A, 0xE6000000)
        struct.pack_into(
            "<8hBB", self.ram, (base & 0x1FFFFF) + 0x5C, 0, 0, 320, 224, 0, 10, 256, 216, 0, 1
        )
        put(self.ram, 0x80000100, base)
        ram = bytes(self.ram)
        env = draw_environment(ram, evaluate("[0x80000100]", ram))
        self.assertEqual(env["clip"], (0, 256, 320, 224))
        self.assertEqual((env["dtd"], env["dfe"]), (1, 1))
        self.assertEqual(len(env["packet"]), 2)
        disp = display_environment(ram, evaluate("[0x80000100] + 0x5c", ram))
        self.assertEqual((disp["screen"], disp["isrgb24"]), ((0, 10, 256, 216), 1))
        self.assertEqual(evaluate("a0 - 4", ram, {"a0": 0x80000010}), 0x8000000C)
        for bad in ("[0x10", "a0", "0x10 0x20", "x9"):
            with self.assertRaises(ValueError):
                evaluate(bad, ram)
        args = SimpleNamespace(ot=[], drawenv="[0x80000100]", dispenv=None)
        self.assertEqual(census_ram(ram, args)["lists"], {})

    def test_size_limits_and_gte(self):
        self.assertEqual(oversized_triangles([(0, 0), (10, 0), (0, 600)]), ["0-1-2"])
        self.assertEqual(
            oversized_triangles([(0, 0), (1100, 0), (0, 10), (20, 10)]), ["0-1-2", "1-2-3"]
        )
        self.assertEqual(oversized_triangles([(0, 0), (1023, 0), (0, 511)]), [])
        cop2 = (
            [0] * 32
            + [0] * 21
            + [0, 0, 0, 160 << 16, 112 << 16, 640, 0xEF9E, 0x1400000, 0x155, 0x100, 0]
        )
        controls = gte_controls(cop2)
        self.assertEqual((controls["H"], controls["OFX"], controls["DQA"]), (640, 160.0, -4194))
        with self.assertRaises(ValueError):
            gte_controls([0] * 10)


FT4_CLASS = "poly4-flat-textured-modulated-semi"

if __name__ == "__main__":
    unittest.main()
