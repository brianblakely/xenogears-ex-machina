"""The game's DMA addresses must access the RAM the GPU and CPU share."""

import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.game_module import CFLAGS, ROOT, WASM_LAYOUT, WASM_TRIPLE, inlinable, ram_mirrors


class RamMirrors(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("XEM_CLANG") and shutil.which("node"), "needs the pinned runtime shell")
    def test_packet_links_alias_loads_and_bulk_copies_in_real_wasm(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, ir, obj, wasm = (directory / name for name in ("memory.c", "memory.ll", "memory.o", "memory.wasm"))
            source.write_text('''
typedef unsigned int u32;
u32 link(u32 *ot, u32 *prim) {
    u32 previous = *ot;
    u32 address = (u32)prim & 0xFFFFFF;
    *ot = address;
    *(u32 *)address = previous | 0x09000000;
    return address;
}
u32 read_word(u32 *address) { return *address; }
void *copy(void *to, void *from) { __builtin_memcpy(to, from, 4); return to; }
void *fill(void *to) { __builtin_memset(to, 0xAB, 4); return to; }
u32 local(u32 value) { volatile u32 stack[2]; stack[1] = value; return stack[1]; }
''')
            subprocess.run([os.environ["XEM_CLANG"], *CFLAGS, "-O0", "-Xclang", "-disable-O0-optnone",
                            "-S", "-emit-llvm", str(source), "-o", str(ir)], cwd=ROOT, check=True)
            lines = ir.read_text().splitlines()
            lines = [WASM_LAYOUT if line.startswith("target datalayout") else
                     WASM_TRIPLE if line.startswith("target triple") else line for line in lines]
            lines = [re.sub(r' "target-(cpu|features)"="[^\"]*"', "", line) if line.startswith("attributes ") else line
                     for line in lines]
            ir.write_text("\n".join(ram_mirrors(lines)) + "\n")
            subprocess.run([os.environ["XEM_CLANG"], "--target=wasm32-unknown-unknown", "-O2", "-w", "-c",
                            str(ir), "-o", str(obj)], check=True)
            subprocess.run([os.environ["XEM_WASM_LD"], "--no-entry", "--global-base=268435456",
                            "--initial-memory=2149580800", "--export=link", "--export=read_word",
                            "--export=copy", "--export=fill", "--export=local", str(obj), "-o", str(wasm)], check=True)
            script = '''const fs = require('fs');
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(process.argv[1]));
  const e = instance.exports, m = new DataView(e.memory.buffer);
  m.setUint32(0x80001000, 0xFFFFFF, true);
  m.setUint32(0x80001200, 0x09000000, true);
  const address = e.link(0x80001000, 0x80001200) >>> 0;
  const linked = [address, m.getUint32(0x80001000, true), m.getUint32(0x80001200, true), m.getUint32(0x1200, true)];
  const aliases = [0x1200, 0x201200, 0x601200, 0x80001200, 0x80601200, 0xA0001200, 0xA0601200].map(a => e.read_word(a) >>> 0);
  const copied = e.copy(0xA0001228, 0x201200) >>> 0;
  const filled = e.fill(0x601230) >>> 0;
  m.setUint32(0x1F800010, 123, true);
  m.setUint32(0x10000010, 456, true);
  // Presentation rings are host memory, outside the PS1 address segments.
  // Their high bits must survive the same rewrite as game RAM accesses.
  m.setUint32(0x7050000C, 2, true);
  m.setUint32(0x70300010, 987, true);
  e.copy(0x70000020, 0x70300010);
  console.log(JSON.stringify({linked, aliases, copied, copy: m.getUint32(0x80001228, true),
    filled, fill: m.getUint32(0x80001230, true), scratch: e.read_word(0xBF800010),
    port: e.read_word(0x10000010), local: e.local(789),
    around: e.read_word(0x7050000C), projection: m.getUint32(0x70000020, true)}));
})();'''
            result = subprocess.run(["node", "-e", script, str(wasm)], check=True, capture_output=True, text=True)
            self.assertEqual(json.loads(result.stdout), {
                "linked": [0x1200, 0x1200, 0x09FFFFFF, 0], "aliases": [0x09FFFFFF] * 7,
                "copied": 0xA0001228, "copy": 0x09FFFFFF, "filled": 0x601230, "fill": 0xABABABAB,
                "scratch": 123, "port": 456, "local": 789, "around": 2, "projection": 987,
            })


def wasm_ir(source, directory):
    """A C unit's IR as the game module's front end gives it, retargeted."""
    path, ir = directory / "unit.c", directory / "unit.ll"
    path.write_text(source)
    subprocess.run([os.environ["XEM_CLANG"], *CFLAGS, "-O0", "-Xclang", "-disable-O0-optnone",
                    "-S", "-emit-llvm", str(path), "-o", str(ir)], cwd=ROOT, check=True)
    lines = [WASM_LAYOUT if line.startswith("target datalayout") else
             WASM_TRIPLE if line.startswith("target triple") else line for line in ir.read_text().splitlines()]
    return [re.sub(r' "target-(cpu|features)"="[^\"]*"', "", line) if line.startswith("attributes ") else line
            for line in lines]


class PortUnits(unittest.TestCase):
    """The port's own units: their globals' constant addresses stay as they
    are (the optimizer then knows them apart), and their functions may be
    inlined; the game's units keep every access translated and every call."""

    SOURCE = '''
typedef unsigned int u32;
struct Registers { u32 data[4]; u32 flags; } registers;
/* A step as the software GTE's: a register, the flags, the register again. */
u32 step(u32 *game) {
    registers.flags |= 1;
    registers.data[2] += *game;
    registers.flags |= 2;
    *game = registers.data[2];
    registers.flags |= 4;
    return registers.flags;
}
u32 flags(void) { return registers.flags; }
'''

    @unittest.skipUnless(os.environ.get("XEM_CLANG"), "needs the pinned runtime shell")
    def test_a_port_units_own_globals_are_addressed_as_they_are_and_game_pointers_translated(self):
        with tempfile.TemporaryDirectory() as directory:
            lines = wasm_ir(self.SOURCE, Path(directory))
        calls = lambda out: [line for line in out if "call ptr @xem.memory.address(" in line]
        game, port = ram_mirrors(list(lines)), ram_mirrors(list(lines), True)
        # A game unit: every constant address within the global is translated.
        self.assertTrue(any("@registers" in call for call in calls(game)))
        # A port unit: none of them is, and the game's pointer still is.
        self.assertFalse(any("@registers" in call for call in calls(port)), calls(port))
        self.assertEqual(len(calls(port)), 2, calls(port))
        self.assertLess(len(calls(port)), len(calls(game)))

    @unittest.skipUnless(os.environ.get("XEM_CLANG") and shutil.which("node"), "needs the pinned runtime shell")
    def test_a_port_unit_runs_the_same_with_its_globals_known_and_reaches_ram_through_any_alias(self):
        results = []
        for port in (False, True):
            with tempfile.TemporaryDirectory() as directory:
                directory = Path(directory)
                ir, obj, wasm = (directory / name for name in ("unit.x.ll", "unit.o", "unit.wasm"))
                rewritten = "\n".join(ram_mirrors(wasm_ir(self.SOURCE, directory), port)) + "\n"
                ir.write_text(inlinable(rewritten) if port else rewritten)
                subprocess.run([os.environ["XEM_CLANG"], "--target=wasm32-unknown-unknown", "-O2", "-w", "-c",
                                str(ir), "-o", str(obj)], check=True)
                subprocess.run([os.environ["XEM_WASM_LD"], "--no-entry", "--global-base=268435456",
                                "--initial-memory=2149580800", "--export=step", "--export=flags", str(obj), "-o", str(wasm)],
                               check=True)
                script = '''const fs = require('fs');
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(process.argv[1]));
  const e = instance.exports, m = new DataView(e.memory.buffer);
  m.setUint32(0x80001200, 5, true);
  // The same word of RAM through its cached, uncached and physical addresses.
  const steps = [0x80001200, 0xA0001200, 0x1200, 0x201200].map(a => e.step(a) >>> 0);
  console.log(JSON.stringify({steps, ram: m.getUint32(0x80001200, true), flags: e.flags() >>> 0}));
})();'''
                result = subprocess.run(["node", "-e", script, str(wasm)], check=True, capture_output=True, text=True)
                results.append(json.loads(result.stdout))
        # The register takes the word (5), which then holds the register:
        # each step through another alias of the word doubles both.
        self.assertEqual(results[0], {"steps": [7, 7, 7, 7], "ram": 40, "flags": 7})
        self.assertEqual(results[1], results[0])

    def test_inlinable_frees_the_functions_and_nothing_else(self):
        ir = '''define dso_local void @f() #0 {
  ret void
}
attributes #0 = { noinline nounwind null_pointer_is_valid "no-builtins" }
attributes #1 = { nocallback nofree nounwind willreturn }
attributes #2 = { alwaysinline noinline_marker "noinline"="kept" }
'''
        self.assertEqual(inlinable(ir), ir.replace("{ noinline nounwind", "{ nounwind"))


if __name__ == "__main__":
    unittest.main()
