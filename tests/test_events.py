"""Invented field event scripts exercise the opcode table, forms and flow; the
source tests check every table entry against the recovered handlers."""

import contextlib
import io
import json
import re
import struct
import tempfile
import unittest
from pathlib import Path

from tools.analysis import events as ev
from tools.analysis.field import FieldError

ROOT = Path(__file__).resolve().parents[1]
FIELD = ROOT / "decomp/src/field"
DEFINITION = re.compile(
    r"^(?:s32|void|u32|s16|u16|u8|int) ([A-Za-z_]\w*)\([^;]*?\)\s*(?:\n[^{]*)?\{(.*?)\n\}",
    re.M | re.S,
)
# The selected-operand readers by flag bit (8009cf78-8009d154).
READERS = {
    bit: f"field_event_read_selected_operand_{bit:02x}" for bit in (1, 2, 4, 8, 16, 32, 64, 128)
}


def number(value: int) -> str:
    return rf"(?:{value}|0x0*{value:X}|0x0*{value:x})"


class Source:
    def __init__(self) -> None:
        text = "\n".join(path.read_text() for path in sorted(FIELD.glob("*.c")))
        self.text = text
        self.bodies = {name: body for name, body in DEFINITION.findall(text)}

    def dispatch(self, table: str) -> list[str]:
        block = re.search(rf"void \(\*{table}\[\d+\]\)\(void\) = \{{(.*?)\}};", self.text, re.S)
        return re.findall(r"[A-Za-z_]\w*", re.sub(r"/\*.*?\*/", "", block.group(1), flags=re.S))

    def reach(self, name: str, depth: int = 3) -> str:
        """The handler's body and those of the field functions it calls."""
        seen, pending, parts = set(), [(name, 0)], []
        while pending:
            current, level = pending.pop()
            if current in seen or current not in self.bodies:
                continue
            seen.add(current)
            body = self.bodies[current]
            parts.append(body)
            if level < depth:
                pending += [(c, level + 1) for c in re.findall(r"([A-Za-z_]\w*)\(", body)]
        return "\n".join(parts)

    def advances(self, name: str) -> set[int]:
        body = self.reach(name)
        found = {int(n, 0) for n in re.findall(r"pc \+= (0x[0-9A-Fa-f]+|\d+);", body)}
        if re.search(r"pc\+\+|\+\+field_current_event_actor->pc", body):
            found.add(1)
        return found


SOURCE = Source()

# Handlers whose completing advance is a sum the source spells in parts.
COMPOSITE = {
    ("primary", 0x12): (5, 4),  # 800932d0 advances 5, then the handler 4
    ("primary", 0x5C): (1, 2),  # without a member: bc advances 1, then 2
    ("primary", 0xD4): (1, 4),  # pc++ before 8009c5a8's 4
    ("primary", 0xFC): (1, 4),  # through d4
    ("primary", 0x9A): (3, 3),  # the +6 path adds 3 twice
}
# Reads that come after an advance inside the handler, by how far they move,
# and 11's step limit, which its set-up reads 9 bytes earlier at pc + 11.
SHIFTED = {("primary", 0x12): {5: 5, 7: 5}, ("primary", 0xD4): {2: 1, 4: 1}}
SHIFTED[("primary", 0xFC)] = SHIFTED[("primary", 0xD4)]
SHIFTED[("primary", 0x11, "walk_step_limited")] = {2: -9}


def read_patterns(operand: ev.Operand, offset: int) -> list[str]:
    k = number(offset)
    if operand.kind in ("u8", "flags", "cond"):
        return [rf"EVENT_OPERAND_BYTE\({k}\)", rf"pc \+ {k}\]", rf"code\[{k}\]"]
    if operand.kind == "actor":
        return [rf"field_event_read_actor_index(?:_or_leader)?\({k}\)"]
    if operand.kind == "s16":
        return [rf"field_event_read_s16\({k}\)", rf"\(s16\)field_event_read_u16\({k}\)"]
    if operand.kind == "iv":
        return [rf"field_event_read_imm_or_var\({k}\)"]
    if operand.kind == "sel":
        flags = number(operand.flags)
        reader = READERS[operand.bit]
        selected = rf"(?:EVENT_OPERAND_BYTE\({flags}\)|[^)]*pc \+ {flags}\]|code\[{flags}\])"
        # 02 reads its two sides directly: 800acd7c or a variable by mode.
        immediate, variable = rf"field_event_read_s16\({k}\)", rf"field_event_read_u16\({k}\)"
        direct = rf"{immediate}(?s:.*){variable}|{variable}(?s:.*){immediate}"
        return [rf"{reader}\({k}, {selected}", direct]
    if operand.kind == "u32":
        return [rf"operand\[{k}\]"]
    return [rf"field_event_read_u16\({k}\)"]  # u16, var, addr, data, bit, msg


class SourceTests(unittest.TestCase):
    """The tables follow field_event_primary_handlers, field_event_extended_handlers and their handlers."""

    def test_entries_name_the_dispatch_table_handlers(self):
        primary = SOURCE.dispatch("field_event_primary_handlers")
        extended = SOURCE.dispatch("field_event_extended_handlers")
        self.assertEqual((len(primary), len(extended)), (256, 227))
        self.assertEqual(primary[ev.PREFIX], ev.PREFIX_HANDLER)
        self.assertEqual(sorted(ev.PRIMARY), [c for c in range(256) if c != ev.PREFIX])
        self.assertEqual(sorted(ev.EXTENDED), list(range(227)))
        for table, handlers in ((ev.PRIMARY, primary), (ev.EXTENDED, extended)):
            for code, forms in table.items():
                with self.subTest(code=f"{code:02x}"):
                    self.assertEqual({f.handler for f in forms}, {handlers[code]})
                    default = forms[-1].select
                    self.assertTrue(default is None or default[2] is None)  # ends with a default
                    self.assertTrue(all(f.effect for f in forms))

    def test_sizes_are_the_handlers_pc_advances(self):
        for space, table in (("primary", ev.PRIMARY), ("extended", ev.EXTENDED)):
            for code, forms in table.items():
                handler = forms[0].handler
                found = SOURCE.advances(handler)
                parts = COMPOSITE.get((space, code))
                for form in forms:
                    with self.subTest(space=space, code=f"{code:02x}", form=form.mnemonic):
                        if form.flow in ("end", "return", "hang", "rerun", "jump", "table"):
                            continue
                        if form.flow == "call":  # the return pc it pushes
                            self.assertRegex(SOURCE.reach(handler), rf"pc \+ {form.size};")
                        elif form.setup is not None:  # pc = pc - setup, then setup + size
                            self.assertIn(f"pc = pc - {form.setup}", SOURCE.reach(handler))
                            self.assertIn(form.setup + form.size, found)
                        elif parts and form.size == sum(parts):
                            self.assertTrue(set(parts) <= found)
                        else:
                            self.assertIn(form.size, found)
                        for advance, _ in form.also:
                            if not (parts and advance == sum(parts)):
                                self.assertIn(advance, found)

    def test_every_advance_in_a_handler_is_in_the_table(self):
        other_walk_mode = {("primary", 0x10): {13}, ("primary", 0x11): {11}}  # 80098cac
        for space, table in (("primary", ev.PRIMARY), ("extended", ev.EXTENDED)):
            for code, forms in table.items():
                explained = set(COMPOSITE.get((space, code), ())) | other_walk_mode.get(
                    (space, code), set()
                )
                for form in forms:
                    explained |= {form.size, *(advance for advance, _ in form.also)}
                    if form.setup is not None:
                        explained.add(form.setup + form.size)
                with self.subTest(space=space, code=f"{code:02x}"):
                    self.assertLessEqual(SOURCE.advances(forms[0].handler), explained)

    def test_jumps_store_their_address_operand(self):
        for space, table in (("primary", ev.PRIMARY), ("extended", ev.EXTENDED)):
            for code, forms in table.items():
                for form in forms:
                    if form.flow not in ("jump", "branch", "call"):
                        continue
                    with self.subTest(space=space, code=f"{code:02x}"):
                        (address,) = [o.offset for o in form.operands if o.kind == "addr"]
                        body = SOURCE.reach(form.handler)
                        self.assertRegex(body, rf"pc = field_event_read_u16\({number(address)}\)")

    def test_operands_are_read_where_the_table_says(self):
        for space, table in (("primary", ev.PRIMARY), ("extended", ev.EXTENDED)):
            for code, forms in table.items():
                body = SOURCE.reach(forms[0].handler)
                for form in forms:
                    shift = SHIFTED.get(
                        (space, code, form.mnemonic), SHIFTED.get((space, code), {})
                    )
                    for operand in form.operands:
                        offset = operand.offset - shift.get(operand.offset, 0)
                        with self.subTest(space=space, code=f"{code:02x}", operand=operand):
                            patterns = read_patterns(operand, offset)
                            self.assertTrue(any(re.search(p, body) for p in patterns))

    def test_waiting_extended_handlers_step_back_onto_the_prefix(self):
        for code, forms in ev.EXTENDED.items():
            if any(f.flow == "wait" for f in forms):
                with self.subTest(code=f"{code:02x}"):
                    self.assertRegex(SOURCE.reach(forms[0].handler), r"pc--|pc -= 1;")


def ext(*values):
    return bytes((ev.PREFIX, *values))


class DecodeTests(unittest.TestCase):
    def test_lengths_and_operands_follow_the_table(self):
        cases = {
            bytes((0x35, 0x10, 0, 0xFE, 0xFF, 0x40)): ("set_variable v0010, #-2", 6),
            bytes((0x35, 0x10, 0, 0x22, 0, 0)): ("set_variable v0010, v0022", 6),
            bytes((0x26, 0x05, 0x80)): ("wait_countdown #5", 3),
            bytes((0x26, 0x24, 0x00)): ("wait_countdown v0024", 3),
            bytes((0x02, 1, 0, 2, 0, 0x45, 9, 0)): ("branch_if_false v0001, #2, <=, @0009", 8),
            ext(0x0A, 0x2A, 0x08): ("set_variable_bit v0082.10", 4),
            bytes((0xD4, 0xFE, 0x21, 0, 1)): ("message_actor party1, msg0021, 0x01", 5),
            ext(0x75, 3, 0x40, 0): ("store_actor_facing actor3, v0040", 5),
            bytes((0x8E, 0x10, 0x27, 0, 0, 0x20, 0)): ("branch_unless_gold 10000, @0020", 7),
        }
        for code, (text, size) in cases.items():
            with self.subTest(code=code.hex(" ")):
                ins = ev.decode_instruction(code + bytes(40), 0)
                self.assertEqual((ins.text(), ins.size), (text, size))

    def test_extended_operands_are_relative_to_the_extended_byte(self):
        ins = ev.decode_instruction(ext(0x23, *range(1, 20)) + bytes(4), 0)
        self.assertEqual(
            (ins.key, ins.size, ins.operands[0], ins.operands[6]), ("fe 23", 21, 0x0201, 13)
        )
        self.assertEqual(ins.successors, (0, 21))  # waits by stepping back onto fe

    def test_forms_pick_by_their_selector_byte(self):
        setup = bytes((0x10, 0, 1, 0, 2, 0, 3, 0, 0xE0))
        walk = setup + bytes((0x10, 1, 0x00))
        first, step = ev.decode_instruction(walk, 0), ev.decode_instruction(walk, 9)
        self.assertEqual(
            (first.name, first.size, step.name, step.size), ("walk_to", 9, "walk_step", 2)
        )
        self.assertEqual(ev.decode_instruction(bytes((0x57, 0x0F, 0)), 0).name, "arc_floor")
        self.assertEqual(ev.decode_instruction(bytes((0x57, 0x8F, 0)), 0).name, "arc_step")
        self.assertEqual(ev.decode_instruction(bytes((0x57, 0x81)) + bytes(12), 0).size, 11)
        halt = ev.decode_instruction(bytes((0x73, 7)), 0)
        self.assertEqual((halt.name, halt.successors), ("emitter_halt", (0,)))
        rerun = ev.decode_instruction(ext(0x27, 9), 0)
        self.assertEqual((rerun.size, rerun.successors), (1, (1,)))  # 27 then runs as primary

    def test_flows_give_their_successors(self):
        def successors(code, pc=0):
            return ev.decode_instruction(code + bytes(16), pc).successors

        self.assertEqual(successors(b"\x00"), ())
        self.assertEqual(successors(b"\x01\x08\x00"), (8,))
        self.assertEqual(successors(b"\x05\x0a\x00"), (10, 3))
        self.assertEqual(successors(b"\x0d"), ())
        self.assertEqual(successors(b"\x0c"), (0,))
        self.assertEqual(successors(b"\xd1"), (0,))
        self.assertEqual(successors(b"\x84\x03\x80\x0c\x00"), (5, 12))
        self.assertEqual(successors(ext(0x00)), (1,))
        self.assertEqual(successors(ext(0x79)), (1,))
        self.assertEqual(successors(b"\xa6\x02\x80"), (9,))
        self.assertEqual(successors(b"\xa6\x10\x00"), (3,))
        self.assertEqual(
            ev.decode_instruction(bytes((0x12, *range(1, 9))) + bytes(9), 0).inner, (4,)
        )

    def test_alternative_advances_need_their_operands(self):
        self.assertEqual(ev.decode_instruction(b"\x9a\x00\x80" + bytes(6), 0).skips, (6,))
        self.assertEqual(ev.decode_instruction(b"\x9a\x10\x00" + bytes(6), 0).skips, (6,))
        self.assertEqual(ev.decode_instruction(b"\x9a\x14\x80" + bytes(6), 0).skips, ())
        self.assertEqual(ev.decode_instruction(b"\xd4\xff\x01\x00\x00" + bytes(2), 0).skips, (6,))
        self.assertEqual(ev.decode_instruction(b"\xd4\x05\x01\x00\x00" + bytes(2), 0).skips, ())
        self.assertEqual(ev.decode_instruction(ext(0x18, 2) + bytes(3), 0).skips, (5,))

    def test_unknown_extended_opcodes_and_truncation_fail_with_their_location(self):
        with self.assertRaises(ev.UnknownInstruction) as caught:
            ev.decode_instruction(ext(0xE3), 0)
        self.assertEqual((caught.exception.pc, caught.exception.opcode), (1, 0xE3))
        with self.assertRaises(FieldError):
            ev.decode_instruction(bytes((0x35, 0, 0, 0)), 0)
        with self.assertRaises(ev.EventError):
            ev.decode_instruction(b"\x01\xff\x00", 0)


class WalkTests(unittest.TestCase):
    def test_skips_are_followed_only_over_a_whole_instruction(self):
        code = b"\xd4\xff\x01\x00\x00" + b"\x9c" + b"\x00"
        result = ev.walk(code, [0])
        self.assertEqual(
            (sorted(result.instructions), result.skips, result.into), ([0, 5, 6], 1, [])
        )
        code = b"\xd4\xff\x01\x00\x00" + b"\xa9\x23" + b"\x00"
        result = ev.walk(code, [0])
        self.assertEqual((sorted(result.instructions), result.into), ([0, 5, 7], [(0, 6)]))

    def test_variable_tables_follow_consecutive_jump_slots(self):
        code = b"\xa6\x10\x00" + b"\x01\x0c\x00" * 2 + b"\x01\x0d\x00" + b"\x00\x00"
        result = ev.walk(code, [0])
        self.assertEqual(result.tables, [(0, 3)])
        self.assertEqual(sorted(result.instructions), [0, 3, 6, 9, 12, 13])

    def test_step_forms_need_their_set_up(self):
        setup = bytes((0x10, 0, 1, 0, 2, 0, 3, 0, 0xE0))
        self.assertEqual(ev.walk(setup + b"\x10\x01\x00", [0]).unpaired, [])
        self.assertEqual(ev.walk(b"\x00" + b"\x10\x01\x00", [1]).unpaired, [1])

    def test_unknown_and_undecodable_paths_keep_their_pc(self):
        result = ev.walk(b"\x02\x00\x00\x00\x00\xc0\x0a\x00" + ext(0xE3) + b"\x01", [0])
        self.assertEqual(result.unknown, [(9, 0xE3)])
        self.assertEqual(result.undecodable[0][0], 10)


def events(code: bytes, entries: list[list[int]]) -> bytes:
    """An event component: type bits, actor count, entry rows and bytecode."""
    rows = b"".join(struct.pack("<32H", *row, *[0] * (32 - len(row))) for row in entries)
    return bytes(0x80) + struct.pack("<I", len(entries)) + rows + code


def bundle(code: bytes, entries: list[list[int]], *, short: int = 0) -> bytes:
    """A map bundle whose event component is stored as literal groups; the
    header gives the component `short` bytes fewer than the stream holds."""
    component = events(code, entries)
    padded = component + bytes(-len(component) % 8)
    packed = bytearray(struct.pack("<I", len(padded)))
    for start in range(0, len(padded), 8):
        packed += b"\x00" + padded[start : start + 8]
    packed += b"\x00"  # the decoder reads the next flag before it returns
    header = bytearray(ev.HEADER)
    struct.pack_into("<I", header, 0x10C + 4 * ev.EVENTS, len(component) - short)
    struct.pack_into("<I", header, 0x130 + 4 * ev.EVENTS, ev.HEADER)
    return bytes(header) + bytes(packed)


class SweepTests(unittest.TestCase):
    def test_entries_skip_zero_events_beside_map_entry_records(self):
        records = ev.event_package(events(b"\xff" + bytes(7) + b"\x00", [[8]]))
        self.assertEqual(ev.script_entries(records), ([(0, 0, 8)], 31))
        plain = ev.event_package(events(b"\x00", [[]]))
        self.assertEqual(ev.script_entries(plain), ([(0, e, 0) for e in range(32)], 0))
        self.assertEqual(
            ev.map_events(bundle(b"\x26\x02\x80\x00", [[0]])).bytecode, b"\x26\x02\x80\x00"
        )
        self.assertIsNone(ev.map_events(bytes(24)))

    def test_the_package_is_the_whole_decoded_stream(self):
        # As map 489: ext a0 (fe + 12 bytes) ends 2 bytes past the header's
        # size, in the stream's tail, and its successor lies past the stream.
        code = bytes(7) + ext(0xA0) + bytes(10) + b"\x17"
        self.assertEqual((len(code), len(events(code, [[7]])) % 8), (20, 0))
        package = ev.map_events(bundle(code, [[7]], short=2))
        self.assertEqual((package.bytecode, package.component_end), (code, 18))
        self.assertEqual(package.declared, code[:18])
        result = ev.walk(package.bytecode, [7])
        self.assertEqual((sorted(result.instructions), result.undecodable), ([7], []))
        self.assertEqual(result.outside, [(7, 20)])
        with self.assertRaises(ev.EventError):
            ev.disassemble_reachable(package.bytecode, 7)
        totals = ev.Totals()
        ev.add_map(totals, "map 0", 0, package)
        self.assertEqual((totals.bytecode, totals.covered), (18, 12))  # with end_slot at 0
        self.assertEqual(len(totals.beyond), 1)
        self.assertIn("+0x0014, past the 20-byte stream", totals.outside[0])
        # a stream longer than the allocation (size + 0x10) would overrun it
        with self.assertRaises(ev.PackedError):
            ev.map_events(bundle(code, [[7]], short=0x11))

    def test_field_changes_count_immediate_fields_and_list_variables(self):
        code = b"\x98\xe9\xc1\x00\x80" + b"\x98\x04\x00\x01\x80" + ext(0x84)
        code += b"\x05\x80\x00\x00\xff\xff\x00\x80" + b"\x00"
        totals = ev.Totals()
        ev.add_map(totals, "map 3", 3, ev.event_package(events(code, [[0, 5, 10]])))
        self.assertEqual(totals.field_changes, 3)
        self.assertEqual(dict(totals.targets), {489: 1})  # 0x41e9: flags above bit 12
        self.assertEqual(totals.variable_targets, ["map 3 +0x0005 change_map_entry v0004, #1"])

    def build(self, root: Path, maps: dict[int, bytes]) -> None:
        directory = 0xC0  # the disc index's entry before the first bundle
        for disc in (1, 2):
            sectors = bytearray(41 * 2352)
            table = [0xFFFF] * 61
            table[4] = directory + 3 - ev.MAP_FILE  # 80028738: file 0xb8 at directory + 1
            start = 40 * 2352 + 24
            sectors[start : start + 0x7A] = struct.pack("<61H", *table)
            (root / "discs").mkdir(exist_ok=True)
            (root / "discs" / f"disc{disc}.bin").write_bytes(sectors)
            extract = root / "extract" / f"disc{disc}"
            (extract / "files").mkdir(parents=True)
            manifest = [{"slot": directory, "lba": 0, "size": -2 * len(maps)}]
            for map_id, data in maps.items():
                for slot, content in (
                    (directory + 1 + 2 * map_id, data),
                    (directory + 2 + 2 * map_id, b"x"),
                ):
                    if disc == 2 and map_id == 1:
                        content = bytes(24)  # a placeholder
                    manifest.append({"slot": slot, "lba": 0, "size": len(content)})
                    (extract / "files" / f"{slot:04d}.bin").write_bytes(content)
            (extract / "manifest.json").write_text(json.dumps({"files": manifest}))

    def test_sweep_reports_aggregates_and_unknown_locations(self):
        clean = bundle(b"\x26\x02\x80\x00", [[0]])
        unknown = bundle(b"\x35\x00\x00\x01\x00\x40" + ext(0xF0), [[0, 0]])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.build(root, {0: clean, 1: unknown})
            results = ev.sweep(root)
            lines = ev.report(results)
            with contextlib.redirect_stdout(io.StringIO()):
                status = ev.main(["--sweep", "--root", str(root)])
        self.assertEqual(status, 1)
        self.assertEqual([(t.maps, t.placeholders) for _, t in results], [(2, 0), (1, 1)])
        self.assertEqual(results[0][1].instructions, 3)
        self.assertIn("  unknown opcodes: 1", lines)
        self.assertTrue(any("disc1 map 1 +0x0007: extended f0" in line for line in lines))
        self.assertIn("maps on both discs: 1, identical: 1", lines)
        self.assertTrue(any(line.split()[:2] == ["26", "wait_countdown"] for line in lines))


if __name__ == "__main__":
    unittest.main()
