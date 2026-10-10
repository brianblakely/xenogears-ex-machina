"""The port's call adapters (port/adapters.c).

port/adapters.c is compiled for wasm32, the game module's target, with test
doubles of its callees and data (tests/port_adapters/support.c) that record
each call's arguments; Node runs the calls. Each test checks that an adapter
passes the value the matched PS1 code left for the missing argument and
returns what the original left in $v0. Run inside
`nix develop path:./nix/runtime` (it provides $XEM_CLANG, $XEM_WASM_LD and node).
"""

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLANG = os.environ.get("XEM_CLANG")
WASM_LD = os.environ.get("XEM_WASM_LD")
NODE = shutil.which("node")
BUILD = None

RUNNER = """
const fs = require("fs");
const [wasm, calls] = process.argv.slice(2);
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)), {});
const out = JSON.parse(calls).map(([name, ...args]) => instance.exports[name](...args));
console.log(JSON.stringify(out));
"""


def setUpModule():
    global BUILD
    if not (CLANG and WASM_LD and NODE):
        raise unittest.SkipTest("needs nix develop path:./nix/runtime ($XEM_CLANG, $XEM_WASM_LD, node)")
    BUILD = tempfile.TemporaryDirectory()
    flags = ["--target=wasm32-unknown-unknown", "-O2", "-std=gnu89", "-nostdinc", "-ffreestanding", "-fno-builtin",
             "-funsigned-char", "-fwrapv", "-fno-strict-aliasing", "-Wall", "-Werror", "-Wno-unused-function",
             "-Iport/include", "-Idecomp/include"]
    objects = []
    for source in ("port/adapters.c", "tests/port_adapters/support.c"):
        obj = os.path.join(BUILD.name, Path(source).stem + ".o")
        subprocess.run([CLANG, *flags, "-c", source, "-o", obj], cwd=ROOT, check=True)
        objects.append(obj)
    subprocess.run([WASM_LD, "--no-entry", "--export-all", *objects, "-o", os.path.join(BUILD.name, "adapters.wasm")],
                   check=True)
    Path(BUILD.name, "run.js").write_text(RUNNER)


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def run(*calls):
    """Reset the doubles, make the calls ([export, args...]) and return their results."""
    result = subprocess.run([NODE, os.path.join(BUILD.name, "run.js"), os.path.join(BUILD.name, "adapters.wasm"),
                             json.dumps([["test_reset"], *calls])], check=True, capture_output=True, text=True)
    return json.loads(result.stdout)[1:]


def recorded(count):
    return [["test_recorded", i] for i in range(count)]


def u32(value):
    return value & 0xFFFFFFFF


class Adapters(unittest.TestCase):
    def test_acting_slot_returns_the_new_acting_sprite(self):
        ret, slot, calls, sprite = run(["xem_adapt_battle_menu_set_acting_slot", 2], ["test_recorded", 0],
                                       ["test_calls"], ["test_sprite", 2])
        self.assertEqual((slot, calls), (2, 1))
        self.assertEqual(ret, sprite)

    def test_seal_deathblow_commands_is_checked(self):
        _, member, checked, calls = run(["xem_adapt_battle_seal_deathblow_commands", 0x105], *recorded(2), ["test_calls"])
        # The member byte is narrowed as 8009ac50 does; `checked` is nonzero.
        self.assertEqual((member, checked, calls), (5, 1, 1))

    def test_arrive_at_target_faces_the_partner(self):
        s0, s1 = run(["test_sprite", 0], ["test_sprite", 1])
        _, sprite, other = run(["xem_adapt_battle_sprite_arrive_at_target", s0], *recorded(2))
        # test_reset makes sprite 1 sprite 0's partner.
        self.assertEqual((sprite, other), (s0, s1))

    def test_update_position_gets_the_map_load_status(self):
        ret, *args = run(["xem_adapt_field_actor_update_position", 3, -7, 0x1000, 0x2000], *recorded(5))
        self.assertEqual(ret, -1)
        self.assertEqual(args[:4], [3, -7, 0x1000, 0x2000])
        self.assertEqual(u32(args[4]), 0x80077C78)

    def test_gear_model_step_gets_the_return_address(self):
        _, *args = run(["xem_adapt_gear_model_step_and_draw", 0x10, 0x20, 0x30, 1], *recorded(5))
        self.assertEqual(args[:4], [0x10, 0x20, 0x30, 1])
        self.assertEqual(u32(args[4]), 0x801CB480)

    def test_palette_bank_sprite_is_returned(self):
        ret, *args, bank_after, sprite = run(
            ["xem_adapt_sprite_create_with_palette_bank", 0x4000, 0x110, 0x1E3, -4, 5, 0x40, 1],
            *recorded(7), ["test_palette_bank"], ["test_sprite", 2])
        self.assertEqual(ret, sprite)
        # data, the four s16 coordinates, `unused` narrowed to s16, and the
        # bank in force during sprite_create, cleared afterwards.
        self.assertEqual(args, [0x4000, 0x110, 0x1E3, -4, 5, 0x40, 1])
        self.assertEqual(bank_after, 0)


if __name__ == "__main__":
    unittest.main()
