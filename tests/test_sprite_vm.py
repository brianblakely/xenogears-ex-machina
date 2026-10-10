"""Invented streams distinguish ordinary sprite time from facing-only replay.

The disassembler tests use invented byte strings and read the recovered
handlers' case labels from decomp/src; no original data is involved.
"""

import re
import struct
import unittest
from collections import Counter
from pathlib import Path

from tests.sprite_fixtures import ADDRESS, RESOURCE, forbidden_read, invented_sprite, reader
from tools.analysis import sprite_vm as vm
from tools.analysis.sprite_state import put, u16, u32
from tools.analysis.sprite_vm import (
    SpriteEnvironment,
    UnsupportedSpriteCommand,
    advance_sprite_timer,
    execute_sprite_commands,
)
from tools.packed_container import encode

ROOT = Path(__file__).resolve().parents[1]


class SpriteVmTests(unittest.TestCase):
    def setUp(self):
        self.sprite, self.resource, _ = invented_sprite()
        self.start = RESOURCE + 0x1A0
        put(self.sprite, 0x64, self.start)
        self.widths = bytearray(256)
        self.widths[0xB3] = 2
        self.environment = SpriteEnvironment(0, 0, 0)

    def execute(self, **kwargs):
        return execute_sprite_commands(
            self.sprite,
            ADDRESS,
            self.environment,
            self.widths,
            reader(RESOURCE, self.resource),
            forbidden_read,
            **kwargs,
        )

    def advance(self, **kwargs):
        return advance_sprite_timer(
            self.sprite,
            ADDRESS,
            self.environment,
            self.widths,
            reader(RESOURCE, self.resource),
            forbidden_read,
            **kwargs,
        )

    def test_expiring_timer_executes_and_scales_the_new_duration(self):
        self.resource[0x1A0] = 0x33
        put(self.sprite, 0x9E, 1, 2)
        put(self.sprite, 0xAC, 128 << 7)
        out = self.advance()
        self.assertEqual((u16(out.sprite, 0x9E), u32(out.sprite, 0x64)), (2, self.start + 1))
        self.assertEqual(out.commands, 1)
        self.assertEqual(u16(self.sprite, 0x9E), 1)

    def test_zero_negative_and_disabled_timers_preserve_original_width_rules(self):
        for rate, initial, expected in ((0, 0, 0), (0, 0x8000, 0x7FFF), (-1, 1, 1)):
            put(self.sprite, 0x9E, initial, 2)
            out = advance_sprite_timer(
                self.sprite,
                ADDRESS,
                SpriteEnvironment(rate, 1, 17),
                self.widths,
                forbidden_read,
                forbidden_read,
            )
            self.assertEqual((u16(out.sprite, 0x9E), out.commands), (expected, 0))
            self.assertEqual(out.environment.frame_head, 17)

    def test_rate_one_can_expire_two_consecutive_waits_in_one_timer_call(self):
        self.environment = SpriteEnvironment(1, 0, 0)
        self.resource[0x1A0:0x1A2] = bytes((0x30, 0x3F))
        put(self.sprite, 0x9E, 1, 2)
        out = self.advance()
        self.assertEqual((u16(out.sprite, 0x9E), out.commands), (16, 2))
        self.assertEqual(u32(out.sprite, 0x64), self.start + 2)

    def test_jump_and_index_store_feed_actual_frame_lookup_with_saturated_step(self):
        self.resource[0x1A0:0x1A8] = bytes((0xE1, 5, 0, 0xFE, 0x80, 0xB3, 0xFF, 0x13))
        put(self.sprite, 0x54, RESOURCE + 0x160)
        put(self.sprite, 0xA8, 63 << 22)
        out = self.execute()
        self.assertEqual(out.commands, 3)
        self.assertEqual((u16(out.sprite, 0x34), u16(out.sprite, 0x9E)), (1, 4))
        self.assertEqual((u32(out.sprite, 0xA8) >> 11) & 63, 0)
        self.assertEqual((u32(out.sprite, 0xA8) >> 22) & 63, 63)
        self.assertEqual(
            (u32(out.sprite, 0x64), out.environment.frame_head), (self.start + 8, ADDRESS)
        )

    def test_frame_increment_and_decrement_preserve_halfword_wrap(self):
        for opcode, frame, expected in ((0x00, 65535, 0), (0x20, 0, 65535)):
            self.resource[0x1A0] = opcode
            put(self.sprite, 0x34, frame, 2)
            out = self.execute()
            self.assertEqual((u16(out.sprite, 0x34), u16(out.sprite, 0x9E)), (expected, 1))

    def test_uninitialized_duration_is_required_and_signed_scaling_is_explicit(self):
        self.resource[0x1A0] = 0x40
        with self.assertRaisesRegex(ValueError, "incoming S3"):
            self.execute()
        out = self.execute(incoming_duration=-257)
        self.assertEqual(u16(out.sprite, 0x9E), 65536 - 257)
        put(self.sprite, 0xAC, 0)
        self.assertEqual(u16(self.execute(incoming_duration=0).sprite, 0x9E), 1)

    def test_backward_jump_uses_signed_operand_relative_to_current_pc(self):
        self.resource[0x1A0:0x1A6] = bytes((0x32, 0x80, 0x80, 0xE1, 0xFD, 0xFF))
        put(self.sprite, 0x64, self.start + 3)
        out = self.execute()
        self.assertEqual(
            (out.commands, u16(out.sprite, 0x9E), u32(out.sprite, 0x64)), (2, 3, self.start + 1)
        )

    def test_unknown_command_reports_exact_pc_and_never_mutates_caller_state(self):
        self.resource[0x1A0:0x1A3] = bytes((0xB3, 17, 0x90))
        old = bytes(self.sprite)
        with self.assertRaises(UnsupportedSpriteCommand) as caught:
            self.execute()
        self.assertEqual(
            (caught.exception.pointer, caught.exception.opcode), (self.start + 2, 0x90)
        )
        self.assertEqual(self.sprite, old)

    def test_zero_width_and_unreachable_timer_rates_fail_instead_of_yielding(self):
        self.resource[0x1A0:0x1A2] = bytes((0xB3, 17))
        self.widths[0xB3] = 0
        with self.assertRaisesRegex(ValueError, "command execution exceeded"):
            self.execute(inspection_limit=3)
        self.environment = SpriteEnvironment(-2, 0, 0)
        with self.assertRaisesRegex(ValueError, "timer exceeded"):
            self.advance(inspection_limit=3)


def function_body(path: str, name: str) -> str:
    text = (ROOT / path).read_text()
    start = re.search(r"^[^\n;]*\b" + name + r"\([^;{]*\)\s*\{", text, re.M).end()
    depth, end = 1, start
    while depth:
        depth += {"{": 1, "}": -1}.get(text[end], 0)
        end += 1
    return text[start:end]


def hex_cases(text: str) -> set[int]:
    return {int(value, 16) for value in re.findall(r"case 0x([0-9A-Fa-f]+):", text)}


def block(*scripts: bytes, directory_high: int = 0, sections: int = 3) -> bytes:
    """An invented resource block: one six-byte header per script."""
    first = 8 + 4 * sections  # after the count, the section offsets and the size
    count = len(scripts)
    headers = first + 2 + 2 * count
    table = headers + 6 * count  # an empty frame table
    at = table + 2
    section = bytearray(struct.pack("<H", count | directory_high << 6))
    starts = []
    for script in scripts:
        starts.append(at)
        at += len(script)
    for index in range(count):
        section += struct.pack("<H", headers + 6 * index - first)
    for index, start in enumerate(starts):
        header = headers + 6 * index
        section += struct.pack("<3H", 0, start - (header + 2), table - (header + 4))
    section += bytes(2) + b"".join(scripts)
    end = first + len(section)
    return struct.pack(f"<{sections + 2}I", sections, first, *[end] * sections) + bytes(section)


class SpriteDisassemblyTests(unittest.TestCase):
    def test_tables_hold_exactly_the_recovered_switch_cases(self):
        generic = function_body("decomp/src/resident/sprite.c", "sprite_vm_run_generic_command")
        self.assertEqual(hex_cases(generic), set(vm.GENERIC))
        shared = set(vm._interpreter("x"))
        resident = "decomp/src/resident/sprite_vm_draw.c"
        self.assertEqual(hex_cases(function_body(resident, "sprite_vm_run")), shared)
        battle = function_body("decomp/src/battle/battle_sprite_vm.c", "battle_sprite_vm_run")
        own = {op for op, spec in vm.BATTLE_SPECS.items() if spec.handler[:8] == "800c11cc"}
        self.assertEqual(hex_cases(battle), own)
        commands = function_body(
            "decomp/src/battle/battle_sprite_commands.c", "battle_sprite_command_run"
        )
        self.assertEqual(hex_cases(commands), set(vm.BATTLE_COMMANDS))
        place = generic[generic.index("case 0xBC:") : generic.index("case 0xD1:")]
        selectors = {int(value) for value in re.findall(r"case (\d+):", place)}
        self.assertEqual(selectors, set(vm.PLACE_SELECTORS))

    def test_lengths_follow_the_width_rule_except_be(self):
        self.assertEqual([vm.table_width(op) for op in (0x80, 0x9F, 0xA0, 0xC7)], [1, 1, 2, 2])
        self.assertEqual([vm.table_width(op) for op in (0xC8, 0xF0, 0xF1, 0xFF)], [3, 3, 4, 4])
        source = re.sub(
            r"/\*.*?\*/", "", (ROOT / "decomp/src/resident/sprite_construction.c").read_text()
        )
        table = re.search(r"u8 sprite_vm_command_lengths\[0x80\] = \{([^}]*)\}", source).group(1)
        lengths = [int(value) for value in re.findall(r"\d+", table)]
        self.assertEqual(lengths, [vm.table_width(op) for op in range(0x80, 0x100)])
        for dialect in vm.DIALECTS:
            for opcode in range(0x100):
                spec = vm.spec_for(opcode, dialect)
                if opcode < 0x80:
                    self.assertEqual(spec.length, 1)
                else:
                    expected = 3 if opcode == 0xBE else vm.table_width(opcode)
                    self.assertEqual(spec.length, expected, f"{dialect} {opcode:02x}")
        self.assertEqual(vm.defined_opcodes(vm.FIELD), 0x80 + 94)
        self.assertEqual(vm.defined_opcodes(vm.BATTLE), 0x80 + 118)

    def test_timed_families_and_stale_duration(self):
        names = [vm.decode(bytes([op]), 0).name for op in (0x0F, 0x10, 0x2F, 0x3F, 0x40, 0x7F)]
        self.assertEqual(
            names, ["frame_next", "frame_step", "frame_back", "wait", "wait_stale", "wait_stale"]
        )
        self.assertEqual(vm.decode(bytes([0x35]), 0).successors, (1,))

    def test_operands_are_signed_relative_and_little_endian(self):
        code = bytes([0x30, 0xE1, 0xFF, 0xFF, 0xA0, 0xF0, 0xFA, 0x83, 0x04, 0x00])
        jump = vm.decode(code, 1)
        self.assertEqual((jump.name, jump.operands, jump.successors), ("jump", (-1,), (0,)))
        speed = vm.decode(code, 4)
        self.assertEqual((speed.name, speed.operands, speed.length), ("set_speed", (-16,), 2))
        branch = vm.decode(code, 6)
        self.assertEqual((branch.name, branch.operands), ("branch_nonzero", (0x83, 4)))
        self.assertEqual(branch.successors, (10, 10))
        self.assertIn("table[3]", vm.render(branch))

    def test_frame_show_takes_three_bytes_with_its_fields(self):
        value = 5 | 1 << 9 | 3 << 11 | 1 << 15
        ins = vm.decode(bytes([0xBE]) + struct.pack("<H", value), 0)
        self.assertEqual((ins.length, ins.flow, ins.operands), (3, "wait", (value - 0x10000,)))
        self.assertIn("frame=5 time=4 flip remap", vm.render(ins))

    def test_flow_follows_calls_loops_and_returns_and_skips_dead_bytes(self):
        script = bytes(
            [0xB4, 0x02, 0x13, 0xE4, 0xFF, 0xFF, 0xE2, 0x05, 0x00, 0x80, 0x82, 0x31, 0x85]
        )
        listing = vm.disassemble(script, [0])
        self.assertEqual([i.pc for i in listing.instructions], [0, 2, 3, 6, 9, 11, 12])
        self.assertEqual(listing.errors, ())
        self.assertEqual(listing.instructions[2].successors, (6, 2))  # loop: on, or back
        self.assertEqual(listing.instructions[3].successors, (9, 11))  # call: return, target

    def test_overlaps_and_runaway_commands_are_errors(self):
        listing = vm.disassemble(bytes([0xE1, 0x01, 0x00, 0x80]), [0])
        self.assertEqual(listing.errors, ((1, "command inside the command at +0x0"),))
        listing = vm.disassemble(bytes([0x31, 0xED, 0x00]), [0])
        self.assertIn("runs past", listing.errors[0][1])
        listing = vm.disassemble(bytes([0xE1, 0x10, 0x00]), [0], bounds=(0, 3))
        self.assertEqual(listing.errors, ((16, "command outside the animation section"),))

    def test_dialects_differ_only_in_handlers(self):
        code = bytes([0xF8, 0x04, 0x00, 0x83, 0x8F])
        field = vm.decode(code, 0, vm.FIELD)
        battle = vm.decode(code, 0, vm.BATTLE)
        self.assertEqual((field.name, field.length, field.flow), ("unhandled", 4, "next"))
        self.assertIn("no case f8", field.unhandled)
        self.assertEqual(
            (battle.name, battle.length, battle.successors), ("branch_event", 4, (4, 4))
        )
        self.assertEqual(vm.decode(code, 4, vm.FIELD).name, "unhandled")
        self.assertEqual(vm.decode(code, 4, vm.BATTLE).flow, "end")
        self.assertEqual(vm.decode(bytes([0xA4, 0x02]), 0, vm.BATTLE).name, "partner_animation")
        self.assertEqual(vm.decode(bytes([0xA4, 0x02]), 0).name, "target_animation")
        self.assertEqual(vm.decode(bytes([0x84]), 0, vm.BATTLE).unhandled[:8], "800c11cc")

    def test_spawns_start_scripts_through_their_headers(self):
        # e0's header offset counts from its operand bytes, e3's from the command;
        # a header's command offset counts from its own halfword.
        code = bytes([0xE0, 6, 0, 0xE3, 10, 0, 0x80])
        code += struct.pack("<3H", 0, 10, 0) + struct.pack("<3H", 0, 6, 0)
        code += bytes([0x33, 0x80, 0x31, 0x80])
        listing = vm.disassemble(code, [0], vm.BATTLE)
        self.assertEqual(listing.instructions[0].headers, (7,))
        self.assertEqual(listing.instructions[1].headers, (13,))
        self.assertEqual((vm.script_start(code, 7), vm.script_start(code, 13)), (19, 21))
        self.assertEqual(listing.scripts, (0, 19, 21))
        self.assertEqual([i.pc for i in listing.instructions], [0, 3, 6, 19, 20, 21, 22])
        self.assertEqual(listing.errors, ())

    def test_battle_commands_take_their_arguments_from_their_form(self):
        f9 = vm.decode(bytes([0xF9, 0x2D, 0x10, 0x00]), 0, vm.BATTLE)
        self.assertEqual((f9.command, f9.data, f9.overread), ("set_position", (0x12,), 0))
        ec = vm.decode(bytes([0xEC, 0x0E, 0xF0]), 0, vm.BATTLE)
        self.assertEqual((ec.command, ec.overread), ("set_direction", 0))
        c3 = vm.decode(bytes([0xC3, 0x0E]), 0, vm.BATTLE)
        self.assertEqual((c3.command, c3.overread), ("set_direction", 1))
        retry = vm.disassemble(bytes([0xEC, 0x50, 0x00, 0x80]), [0], vm.BATTLE)
        self.assertEqual(retry.instructions[0].reentry, 1)
        self.assertIn("wait_hit resumes inside its command at +0x1", retry.errors[0][1])
        coherent = vm.decode(bytes([0xC3, 0x67]), 0, vm.BATTLE)
        self.assertEqual((coherent.command, coherent.reentry), ("wait_camera", None))
        e8 = vm.decode(bytes([0xE8, 0x17, 0x81]), 0, vm.BATTLE)
        self.assertEqual(
            (e8.command, e8.overread, vm.render(e8)[-8:]), ("scale_velocity", 0, "table[1]")
        )
        missing = vm.decode(bytes([0xC3, 0x39]), 0, vm.BATTLE)
        self.assertIn("800b3f04 has no case 39", missing.unhandled)

    def test_indirect_commands_and_place_selectors(self):
        c8 = vm.decode(bytes([0xC8, 0xA0, 0x7E]), 0)
        self.assertEqual((c8.command, c8.unhandled), ("set_speed", ""))
        self.assertIn("stack[126]", vm.render(c8))
        self.assertIn("no case c8", vm.decode(bytes([0xC8, 0xC8, 0x00]), 0).unhandled)
        names = [vm.decode(bytes([0xBC, value]), 0).command for value in (0x98, 0xC0, 0x05)]
        self.assertEqual(names, ["self", "target_target", "creator_offset_5"])
        self.assertIn("no selector 50", vm.decode(bytes([0xBC, 0xB2]), 0).unhandled)

    def test_data_offsets_count_from_the_operand_bytes(self):
        self.assertEqual(vm.decode(bytes([0xFC, 0x10, 0x00, 0x00]), 0).data, (0x11,))
        self.assertEqual(vm.decode(bytes([0xCC, 0x10, 0x00]), 0).data, (0x10,))
        self.assertEqual(vm.decode(bytes([0xCA, 0xFE, 0xFF]), 0, vm.BATTLE).data, (-1,))
        self.assertEqual(vm.decode(bytes([0xF3, 0, 0, 0]), 0, vm.BATTLE).data, ())

    def test_resource_blocks_are_found_and_their_animations_decoded(self):
        data = bytes(8) + block(bytes([0xA0, 0x10, 0x13, 0x81]), bytes([0x33, 0xE1, 0xFF, 0xFF]))
        found = list(vm.resource_blocks(data))
        self.assertEqual(len(found), 1)
        listing = vm.block_listing(data, found[0], vm.FIELD)
        self.assertEqual(
            [i.name for i in listing.instructions][:3], ["set_speed", "frame_step", "hold"]
        )
        self.assertEqual(len(listing.scripts), 2)
        self.assertEqual(listing.errors, ())
        high = block(bytes([0x80]), directory_high=3)
        self.assertEqual(len(vm.resource_block(high, 0).headers), 1)
        broken = bytearray(block(bytes([0x80])))
        broken[0x16] = 0xFF  # a header offset past the section
        self.assertIsNone(vm.resource_block(bytes(broken), 0))
        self.assertIsNone(vm.resource_block(bytes(4) + block(bytes([0x80])), 2))

    def test_headers_no_directory_lists_are_reported_apart(self):
        data = bytearray(block(bytes([0x31, 0x80]), bytes([0x32, 0x80]), bytes([0x33, 0x80])))
        data[0x14:0x16] = struct.pack("<H", 2)  # the directory lists headers 0 and 2 only
        data[0x18:0x1A] = struct.pack("<H", 0x28 - 0x14)
        found = vm.resource_block(bytes(data), 0)
        self.assertEqual(found.headers, (0x1C, 0x28))
        listing = vm.block_listing(bytes(data), found, vm.BATTLE)
        self.assertEqual([i.pc for i in listing.instructions], [0x30, 0x31, 0x34, 0x35])
        self.assertEqual(vm.unlisted_headers(bytes(data), found, listing), (0x22,))
        self.assertEqual(vm.script_start(bytes(data), 0x22), 0x32)

    def test_blocks_take_any_section_count_and_headers_point_past_themselves(self):
        # 80022224 reads only the first three section offsets; battle sprite
        # files carry more sections, so section 1 starts later.
        data = bytes(4) + block(bytes([0x31, 0x80]), sections=5)
        found = list(vm.resource_blocks(data))
        self.assertEqual([(b.offset, b.count, b.sections[0]) for b in found], [(4, 5, 0x1C)])
        listing = vm.block_listing(data, found[0], vm.BATTLE)
        self.assertEqual([i.name for i in listing.instructions], ["wait", "end"])
        inside = bytearray(block(bytes([0x80])))
        inside[0x1A:0x1C] = bytes(2)  # commands from the header's own second halfword
        self.assertIsNone(vm.resource_block(bytes(inside), 0))

    def test_offset_tables_and_packed_files_become_views(self):
        table = struct.pack("<4I", 2, 16, 20, 24) + b"abcdwxyz"
        self.assertEqual(vm._container(table), [(16, 20), (20, 24)])
        self.assertIsNone(vm._container(struct.pack("<3I", 2, 12, 4) + bytes(8)))
        sprite = block(bytes([0x31, 0x80]))
        # The decoder reads one flag byte past the last group, as the original does.
        packed = encode(sprite) + bytes(1)
        views = list(vm.file_views(1, 5, packed, Counter()))
        self.assertEqual([view.name for view in views], ["raw", "unpacked"])
        # The packer's zero literals that complete its last group follow.
        self.assertEqual(views[1].data[: len(sprite)], sprite)
        self.assertFalse(any(views[1].data[len(sprite) :]))
        archive = struct.pack("<4I", 2, 16, 16 + len(packed), 16 + 2 * len(packed))
        names = [view.name for view in vm.file_views(1, 5, archive + packed * 2, Counter())]
        self.assertEqual(names, ["raw", "raw/0p", "raw/1p"])


if __name__ == "__main__":
    unittest.main()
