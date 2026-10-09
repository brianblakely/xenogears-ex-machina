"""The dispatch-table censuses follow the recovered C; synthetic records
exercise the walks (no original data)."""

import re
import struct
import unittest

from tools.analysis import battle_ai
from tools.analysis.dispatch_tables import (
    ACT,
    ENEMY_COMMANDS,
    GEAR_COMMANDS,
    PARTY_COMMANDS,
    ROOT,
    VARIABLE,
    CensusError,
    FormulaFamily,
    act_commands,
    archive_entries,
    area_files,
    descriptors,
    family_census,
    formula_tables,
    function_body,
    gear_files,
    group_models,
    initializer,
    object_model_group,
    primitive_table,
    walk_model,
)


def source(path: str) -> str:
    return (ROOT / "decomp/src" / path).read_text()


def packed(data: bytes) -> bytes:
    """A literal-only packed block (80032e88) with its final flag byte."""
    data += bytes(-len(data) % 8)
    out = len(data).to_bytes(4, "little")
    for start in range(0, len(data), 8):
        out += b"\0" + data[start : start + 8]
    return out + b"\0"


def descriptor(formula: int, flags: int = 0) -> bytes:
    record = bytearray(0x28)
    struct.pack_into("<H", record, 0x0A, flags)
    record[0x16] = formula
    return bytes(record)


def model_header(groups: int, aux: int, primitives: int, group_count: int, packets: int) -> bytes:
    header = bytearray(0x38)
    struct.pack_into("<HHHH", header, 0, 0, 3, primitives, group_count)
    struct.pack_into("<II", header, 0x10, groups, aux)
    struct.pack_into("<I", header, 0x34, packets)
    return bytes(header)


class SourceTests(unittest.TestCase):
    """Every table, length and selector the census uses is read from decomp/src."""

    def test_formula_tables_and_their_callers(self):
        foot, gear = formula_tables()
        self.assertEqual(
            (foot.name, foot.caller, len(foot.handlers)), ("D_800C348C", "func_800941A4", 8)
        )
        self.assertEqual(
            (gear.name, gear.caller, len(gear.handlers)), ("D_800C34DC", "func_8009C198", 11)
        )
        self.assertEqual(foot.handlers[4], gear.handlers[9])  # func_80096018 sits in both
        text = source("battle/battle_8008CCCC.c")
        for table in (foot, gear):
            for formula, handler in enumerate(table.handlers):
                comment = text[: text.index(f"\nvoid {handler}(void) {{")].rsplit("/*", 1)[1]
                self.assertIn(f"{table.name}[{formula}]", comment, handler)
        caller = function_body(text, "func_800941A4")
        self.assertIn("flags15A & 0x80", caller)  # gear attackers go to 8009c198
        self.assertIn("D_800C3DFC->flagsA & 0x10", caller)

    def test_descriptor_fields_and_archive_layout(self):
        header = source("battle/combatant.h")
        self.assertIn("u8 formula; /* 0x16", header)
        self.assertIn("u16 flagsA;   /* 0x0A */", header)
        loader = " ".join(function_body(source("ovl2615/ovl2615.c"), "func_801E5384").split())
        for entry, member, size in (
            ("5 + D_800CCCE8.party_ids[i]", "member_data[i]", 0x5F0),
            ("0x11 + gear", "member_gear[i]", 0x690),
            ("4", "data35D8", 0x1F40),
        ):
            copy = f"archive[{entry}], 1); memmove(D_800CCCE8.{member}, block, 0x{size:X});"
            self.assertIn(copy, loader)
        self.assertIn("archive[0x10]", loader)  # the runs end at entries loaded otherwise
        self.assertIn("archive[0x24]", loader)
        self.assertEqual(
            (PARTY_COMMANDS, GEAR_COMMANDS, ENEMY_COMMANDS),
            ((5, 0x5F0, 0x10), (0x11, 0x690, 0x24), (4, 0x1F40, 5)),
        )
        entry = source("resident/main_8001B6C4.c")
        self.assertIn("func_80028470(12, 0);", entry)
        self.assertIn("file = func_80031BDC(func_800288EC(3), 1);", entry)

    def test_enemy_command_is_the_act_entrys_arg1(self):
        executor = function_body(source("battle/battle_800792F8.c"), "func_800793F0")
        self.assertIn(f"case {ACT}:\n            func_80078998(actor, i, target);", executor)
        act = function_body(source("battle/battle_80070E2C.c"), "func_80078998")
        self.assertIn("D_800C3EAC->unk2DC = D_800D2E5C[index].arg1 + 1;", act)
        commit = function_body(source("battle/battle.c"), "func_80085CCC")
        self.assertIn("D_800D2C94.action = action - 1;", commit)

    def test_primitive_table(self):
        table = primitive_table()
        self.assertEqual(len(table.types), 17)
        self.assertEqual(table.override_codes, {0xC4, 0xC8})
        self.assertEqual([t.stride for t in table.types], [8] * 17)
        self.assertEqual(
            [t.packet_size for t in table.types],
            [0x14, 0x20, 0x1C, 0x28] * 2 + [0x18, 0x28, 0x24, 0x34] * 2 + [0x20],
        )
        # The textured types (odd, not 16) check 8002cd64 before their record.
        self.assertEqual(
            [k for k, t in enumerate(table.types) if t.overrides], list(range(1, 16, 2))
        )
        self.assertEqual([len(t.draw) for t in table.types], [6] * 17)
        renderer = function_body(source("resident/main_8002C3E8.c"), "func_8002C700")
        self.assertIn("type = &D_8004FE50[group->type];", renderer)

    def test_loader_tables(self):
        gears = gear_files()
        self.assertEqual(len(gears), 20)
        self.assertEqual((gears[0], gears[2], gears[19]), ((1, 0), (5, 6), (0, 0)))
        areas = area_files()
        self.assertEqual((len(areas), areas[0], areas[-1]), (20, 43, 219))

    def test_initializer_reads_lengths_and_strips_comments(self):
        text = "int D_1[3] = { /* 0 */ 1, /* 1 */ {2, 3}, 4 };\nvoid (*D_2[])(void) = {f, g};"
        self.assertEqual(initializer(text, "D_1"), (3, "  1,  {2, 3}, 4 "))
        self.assertEqual(initializer(text, "D_2"), (None, "f, g"))
        with self.assertRaises(CensusError):
            initializer(text, "D_3")


class FormulaTests(unittest.TestCase):
    def test_archive_entries_follow_8003342c(self):
        blocks = [packed(b"A" * 8), packed(descriptor(3) + descriptor(9, 0x10))]
        offsets = [12, 12 + len(blocks[0])]
        archive = struct.pack("<III", 2, *offsets) + b"".join(blocks)
        entries = archive_entries(archive)
        self.assertEqual(entries[1], b"A" * 8)
        self.assertEqual(descriptors(entries[2], 0x50), [(3, 0), (9, 0x10)])
        with self.assertRaises(CensusError):
            archive_entries(struct.pack("<I", 0))

    def test_family_census_counts_ids_past_the_table(self):
        family = FormulaFamily()
        entries = [b"", descriptor(0) + descriptor(7) + descriptor(8), descriptor(2, 0x10)]
        family_census(family, entries, 1, 0x78, 3, 8, "set")
        self.assertEqual((family.sets, family.records, family.short), (2, 4, 2))
        self.assertEqual(family.formulas, {0: 1, 7: 1, 8: 1})
        self.assertEqual(family.gear_descriptors, 1)  # dispatched through another descriptor
        self.assertEqual(family.outside, ["set 0 #2: formula 8"])

    def test_act_commands_follow_the_list_writes(self):
        words = [
            (0x80, 0, 0, 0),  # always
            (0x01, 1, 31, 0),  # arg1 = 31
            (0x01, 2, 5, 0),  # animation
            (0x01, 0, ACT, 0),  # closes an act entry
            (0x01, 0, 2, 0),  # an approach entry without arg1
            (0x01, 0, ACT, 0),  # an act entry nothing wrote arg1 for
            (0x3D, 1, 9, 0),  # arg1 = 9 by the halfword writer
            (0x01, 0, ACT, 0),
            (0x52, 0, 3, 0),  # type and arg1 from a variable
            (0x01, 0, ACT, 0),
            (0xFD, 0, 0, 0),
        ]
        data = b"".join(bytes(w) for w in words)
        script = battle_ai.analyse_script(data, 0, len(data))
        self.assertEqual(list(act_commands(script, data)), [31, None, 9, VARIABLE])


class PrimitiveTests(unittest.TestCase):
    def test_walk_counts_groups_overrides_and_packets(self):
        table = primitive_table()
        groups = (
            struct.pack("<BBh", 13, 0x77, 2)
            + bytes(16)
            + struct.pack("<BBh", 4, 0x77, 1)
            + bytes(8)
        )
        aux = (
            struct.pack("<I", 0xC4000000)  # texture page override
            + struct.pack("<I", 0xC8000000)  # CLUT override
            + bytes(12)
            + bytes(12)  # the two textured records
            + bytes(4)  # the flat record's colour
        )
        packets = 2 * 0x28 + 0x14
        model = model_header(0x38, 0x38 + len(groups), 3, 2, packets)
        data = model + groups + aux
        walk = walk_model(data, 0, 0, table)
        self.assertEqual(walk.groups, [(13, 2), (4, 1)])
        self.assertEqual(walk.overrides, {0xC4: 1, 0xC8: 1})
        self.assertEqual((walk.packet_bytes, walk.errors, walk.differences), (packets, [], []))

    def test_walk_reports_types_past_the_table_and_bad_headers(self):
        table = primitive_table()
        past = model_header(0x38, 0x40, 1, 1, 0x14) + struct.pack("<BBh", 17, 0, 1) + bytes(12)
        self.assertEqual(
            walk_model(past, 0, 0, table).errors, ["group 0 type 17 past the 17-entry table"]
        )
        negative = model_header(0x38, 0x3C, 0, 1, 0) + struct.pack("<BBh", 0, 0, -1)
        self.assertEqual(walk_model(negative, 0, 0, table).errors, ["group 0 count -1"])
        wrong = model_header(0x38, 0x44, 1, 1, 0x18) + struct.pack("<BBh", 0, 0, 1) + bytes(12)
        self.assertEqual(
            walk_model(wrong, 0, 0, table).differences, ["packets need 0x14, header +0x34 0x18"]
        )
        self.assertEqual(
            walk_model(bytes(8), 0, 0, table).errors, ["model header outside its data"]
        )

    def test_cx_bytes_other_than_c4_c8_are_the_records_own(self):
        table = primitive_table()
        groups = struct.pack("<BBh", 5, 0, 1) + bytes(8)
        aux = struct.pack("<I", 0xC0000000) + bytes(4)
        data = model_header(0x38, 0x38 + len(groups), 1, 1, 0x20) + groups + aux
        walk = walk_model(data, 0, 0, table)
        self.assertEqual((walk.overrides, walk.other_cx, walk.errors), ({}, 1, []))

    def test_groups_and_object_model_files(self):
        group = struct.pack("<iI", 2, 0) + bytes(8) + bytes(0x70)
        self.assertEqual(group_models(group, 0), [0x10, 0x48])
        with self.assertRaises(CensusError):
            group_models(struct.pack("<iI", 2, 1) + bytes(0x78), 0)  # already relocated
        # 8003342c table: images, the model group, the hierarchy, the header.
        file = struct.pack("<5I", 4, 0x14, 0x18, 0x18 + len(group), 0x18 + len(group))
        file += bytes(4) + group
        self.assertEqual(object_model_group(file), (0x18, 0x18 + len(group)))
        with self.assertRaises(CensusError):
            object_model_group(struct.pack("<5I", 4, 0x14, 0x20, 0x18, 0x18) + bytes(16))


class LayoutTests(unittest.TestCase):
    def test_model_header_fields_follow_the_c(self):
        header = source("resident/model.h")
        fields = header[: header.index("} SpriteModel;")].rsplit("typedef struct {", 1)[1]
        declarations = re.findall(r"(\w+) \**(\w+);", re.sub(r"/\*.*?\*/", "", fields))
        # u16 counts at +0, +2, +4 and +6, six offsets from +8, the box from +0x20,
        # then aux_size at +0x30 and packet_size at +0x34.
        self.assertEqual(
            declarations,
            [
                ("u16", "flags"),
                ("u16", "vertex_count"),
                ("u16", "primitive_count"),
                ("u16", "group_count"),
                ("SVECTOR", "vertices"),
                ("SVECTOR", "normals"),
                ("u8", "unk10"),
                ("u8", "unk14"),
                ("u8", "unk18"),
                ("MorphTable", "morphs"),
                ("SVECTOR", "box_min"),
                ("SVECTOR", "box_max"),
                ("s32", "aux_size"),
                ("s32", "packet_size"),
            ],
        )


if __name__ == "__main__":
    unittest.main()
