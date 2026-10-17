"""The game's DMA addresses must access the RAM the GPU and CPU share."""

import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.game_module import CFLAGS, ROOT, WASM_LAYOUT, WASM_TRIPLE, ram_mirrors


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
  console.log(JSON.stringify({linked, aliases, copied, copy: m.getUint32(0x80001228, true),
    filled, fill: m.getUint32(0x80001230, true), scratch: e.read_word(0xBF800010),
    port: e.read_word(0x10000010), local: e.local(789)}));
})();'''
            result = subprocess.run(["node", "-e", script, str(wasm)], check=True, capture_output=True, text=True)
            self.assertEqual(json.loads(result.stdout), {
                "linked": [0x1200, 0x1200, 0x09FFFFFF, 0], "aliases": [0x09FFFFFF] * 7,
                "copied": 0xA0001228, "copy": 0x09FFFFFF, "filled": 0x601230, "fill": 0xABABABAB,
                "scratch": 123, "port": 456, "local": 789,
            })


if __name__ == "__main__":
    unittest.main()
