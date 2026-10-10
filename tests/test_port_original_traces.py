"""The port's handwritten-routine replacements against the ORIGINAL routines,
from a private instruction trace of the original game (reference emulator,
tools/reference/scenario.py --trace-instructions).

The trace hooks each routine's entry and its return with a snapshot of RAM
and the scratchpad and the 64 GTE words. For every call, this test loads the
entry state into the native game memory (tests/port_native.py) and the
software GTE, runs the port's routine with the original's arguments, and
compares RAM, the scratchpad and every GTE register with the original's
state at the return. The original's saves of s registers below its unchanged
stack pointer are dead stack bytes the port does not write; they are the
only bytes left out.

The captures are private: XEM_HANDWRITTEN_TRACE lists capture directories
(os.pathsep-separated; default .local/scenarios/p2-handwritten-routines-
{battle,wide}/capture), and the test skips without one.
tools/reference/handwritten_trace.py writes the trace specification and
says how the captures were made. A word only the original changed, outside
the packets, OT and globals the routine writes, is an interrupt the emulator
took during the call; such calls are counted, not failed.
"""

import ctypes
import json
import os
import struct
import tempfile
import unittest
from pathlib import Path

from tests import port_native as mem
from tests.test_port_model_draw import SYMBOLS
from tools.reference.instruction_trace import SnapshotReader

ROOT = Path(__file__).resolve().parents[1]
CAPTURES = [Path(p) for p in os.environ.get("XEM_HANDWRITTEN_TRACE", os.pathsep.join(
    str(ROOT / f".local/scenarios/p2-handwritten-routines-{name}/capture") for name in ("battle", "wide"))).split(os.pathsep)]
CAPTURES = [c for c in CAPTURES if (c / "instruction-trace.jsonl").exists()]

LIB = None
BUILD = None
# The routine owning each return hook.
RETURNS = {"model_draw_exit": "model_draw_", "sprite_darken_pixels_return": "sprite_darken_pixels",
           "sprite_blend_pixels_return": "sprite_blend_pixels"}
# The original's register saves below sp (model_draw_save: sp-4..sp-36).
STACK_SAVES = 64


def setUpModule():
    global LIB, BUILD
    if not CAPTURES:
        raise unittest.SkipTest("no private instruction trace (XEM_HANDWRITTEN_TRACE)")
    if not mem.CLANG:
        raise unittest.SkipTest("no clang (run inside nix develop path:./nix/runtime)")
    BUILD = tempfile.TemporaryDirectory()
    LIB = mem.build(BUILD.name, ["port/model_draw.c", "port/sprite_pixels.c"], SYMBOLS)


def tearDownModule():
    if BUILD:
        BUILD.cleanup()


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def read_data(raw, reg):
    """What mfc2 returns from the emulator's raw data register storage."""
    value = raw[reg]
    if reg in (1, 3, 5, 8, 9, 10, 11):
        return s16(value) & 0xFFFFFFFF
    if reg in (7, 16, 17, 18, 19):
        return value & 0xFFFF
    if reg == 15:
        return raw[14]
    if reg in (28, 29):
        colour = 0
        for i, ir in enumerate(raw[9:12]):
            colour |= min(0x1F, max(0, s16(ir) >> 7)) << (5 * i)
        return colour
    return value


def load_gte(raw):
    LIB.xem_gte_reset()
    for reg in range(32):
        LIB.xem_gte_write_control(reg, raw[32 + reg])
    for reg in list(range(15)) + list(range(16, 28)) + [30]:
        LIB.xem_gte_write_data(reg, raw[reg])


def pairs(records):
    """(entry, return) record pairs, in capture order."""
    entry = None
    for record in records:
        owner = RETURNS.get(record["hook"])
        if owner is None:
            entry = record
        elif entry is not None and entry["hook"].startswith(owner):
            yield entry, record
            entry = None
        else:
            entry = None


class OriginalTraces(unittest.TestCase):
    def test_every_traced_call_matches_the_original(self):
        calls, interrupted, by_routine, failures = 0, 0, {}, []
        for capture in CAPTURES:
            lines = (capture / "instruction-trace.jsonl").read_text().splitlines()
            records = [json.loads(line) for line in lines if line]
            reader = SnapshotReader(capture / "instruction-trace-snapshots.bin")
            for entry, done in pairs(records):
                calls, interrupted = self.replay(reader, entry, done, calls, interrupted, by_routine, failures)
        print(f"\n{calls} original calls compared: {json.dumps(by_routine, sort_keys=True)}; "
              f"{interrupted} with interrupt writes outside the routine's outputs")
        self.assertGreater(calls, 0)
        self.assertEqual(failures[:20], [], f"{len(failures)} of {calls} calls differ")

    def replay(self, reader, entry, done, calls, interrupted, by_routine, failures):
        ram, scratchpad, _ = reader.read(entry)
        gpr = entry["gpr_u32"]
        mem.write(mem.RAM, ram)
        mem.write(mem.SCRATCHPAD, scratchpad[:1024])
        load_gte(entry["cop2_u32"])
        name = entry["hook"]
        function = getattr(LIB, name)
        if name == "sprite_blend_pixels":
            fifth = struct.unpack_from("<I", ram, (gpr[29] & 0x1FFFFF) + 16)[0]
            function.argtypes = [ctypes.c_uint] * 5
            function(gpr[4], gpr[5], gpr[6], gpr[7], fifth)
        elif name == "sprite_darken_pixels":
            function.argtypes = [ctypes.c_uint] * 4
            function(gpr[4], gpr[5], gpr[6], gpr[7])
        else:
            function.argtypes = [ctypes.c_uint, ctypes.c_uint]
            function(gpr[4], gpr[5])
        expected_ram, expected_scratchpad, _ = reader.read(done)
        # The dead stack bytes below sp take the original's values (the
        # field runs its frame with sp in the scratchpad).
        sp = gpr[29] & 0x1FFFFFFF
        if 0x1F800000 <= sp <= 0x1F800400:
            sp -= 0x1F800000
            mem.write(mem.SCRATCHPAD + sp - STACK_SAVES, expected_scratchpad[sp - STACK_SAVES:sp])
        else:
            sp &= 0x1FFFFF
            mem.write(mem.RAM + sp - STACK_SAVES, expected_ram[sp - STACK_SAVES:sp])
        actual_ram = mem.read(mem.RAM, mem.RAM_BYTES)
        ram_diff = [] if actual_ram == expected_ram else [
            i for i in range(0, mem.RAM_BYTES, 4) if actual_ram[i:i + 4] != expected_ram[i:i + 4]]
        # A word the port changed must hold the original's value. A word
        # only the original changed is the work of an interrupt taken
        # during the call (the emulator runs the game's handlers in
        # between) when it lies outside everything the routine can
        # write; inside, it is a write the port missed.
        wrong = [i for i in ram_diff if actual_ram[i:i + 4] != ram[i:i + 4]]
        missed = [i for i in ram_diff if actual_ram[i:i + 4] == ram[i:i + 4]]
        outputs = self.outputs(name, gpr, ram, expected_ram)
        missed_outputs = [i for i in missed if outputs is None or any(a <= i < b for a, b in outputs)]
        if missed and not missed_outputs and not wrong:
            interrupted += 1
            ram_diff = []
        scratch_diff = mem.read(mem.SCRATCHPAD, 1024) != expected_scratchpad[:1024]
        gte = done["cop2_u32"]
        gte_diff = [r for r in range(32) if LIB.xem_gte_read_data(r) != read_data(gte, r)]
        gte_diff += [32 + r for r in range(32) if LIB.xem_gte_read_control(r) != gte[32 + r]]
        calls += 1
        by_routine[name] = by_routine.get(name, 0) + 1
        if ram_diff or scratch_diff or gte_diff:
            failures.append(
                f"{name} call {calls} (frame {entry['frontend_run']}): RAM words "
                f"{[hex(0x80000000 + i) for i in ram_diff[:8]]}{'...' if len(ram_diff) > 8 else ''} "
                f"scratchpad {scratch_diff} GTE {gte_diff}")
        return calls, interrupted

    @staticmethod
    def outputs(name, gpr, ram, expected_ram):
        """RAM offset ranges the routine may write, or None to allow none."""
        if not name.startswith("model_draw_"):
            return None

        def word(data, address):
            return struct.unpack_from("<I", data, address & 0x1FFFFF)[0]

        globals_ = [(SYMBOLS[n] & 0x1FFFFF, (SYMBOLS[n] & 0x1FFFFF) + 4) for n in (
            "model_current_packet", "model_drawn_primitive_count", "model_lit_color_cache")]
        start = word(ram, SYMBOLS["model_current_packet"]) & 0x1FFFFF
        end = word(expected_ram, SYMBOLS["model_current_packet"]) & 0x1FFFFF
        ot = word(ram, SYMBOLS["model_ot"]) & 0x1FFFFF
        return globals_ + [(start, end), (ot, ot + 0x10000)]


if __name__ == "__main__":
    unittest.main()
