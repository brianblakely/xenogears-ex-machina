"""The dispatch-table censuses follow the recovered C; synthetic records
exercise the walks (no original data)."""

import re
import struct
import unittest

from tools.analysis import battle_ai
from tools.analysis.dispatch_tables import (
    ACT,
    ENEMY_COMMANDS,
    FORCED_GEAR,
    GEAR_COMMANDS,
    GROWTH,
    LEARNED_SLOTS,
    PARTY_COMMANDS,
    POLY_SIZES,
    ROOT,
    TECHNIQUE_BASE,
    UNLOCKS_B,
    VARIABLE,
    CensusError,
    FormulaFamily,
    TmdCensus,
    act_commands,
    archive_entries,
    area_files,
    descriptors,
    enemy_table,
    family_census,
    formula_tables,
    function_body,
    gear_files,
    group_models,
    initializer,
    kr_body,
    object_model_group,
    primitive_table,
    sprite_callbacks,
    sprite_kind,
    techniques,
    tmd_kind,
    tmd_kinds,
    walk_model,
    walk_tmd,
    world_modes,
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

    def test_gear_techniques_follow_the_menu_learning_and_loader(self):
        menu = function_body(source("battle/battle.c"), "func_8008B224")
        self.assertIn(f"gearCommands[member][row * 2 + column + {TECHNIQUE_BASE}]", menu)
        self.assertIn("func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, column + row * 2)", menu)
        battle = source("battle/battle.c")  # func_8008ADD0 is defined K&R
        commit = battle[battle.index("void func_8008ADD0(member)") :]
        commit = commit[: commit.index("\n}\n")]
        self.assertIn(f"gearCommands[member][D_800C3EAC->unk2E6 + {TECHNIQUE_BASE}]", commit)
        bits = initializer(source("battle/battle_80070E2C.c"), "D_800C3468")[1]
        values = [int(v, 0) for v in bits.split(",") if v.strip()]
        self.assertEqual(values, [0x8000 >> i for i in range(16)])
        learn = function_body(source("ovl2596/ovl2596.c"), "func_801E3F28")
        self.assertIn(f"for (k = 0; k < {LEARNED_SLOTS}; k++)", learn)
        self.assertIn("skills[id].unlocksB |= 0x8000 >> k;", learn)
        growth = source("ovl2596/battle_results.h")
        self.assertIn(f"u8 unlocksB[16];          /* 0x{UNLOCKS_B:X}: 0 ends */", growth)
        self.assertIn("Growth characters[11];", growth)
        self.assertIn(f"(0x{GROWTH:x} each;", growth)
        loader = function_body(source("ovl2615/ovl2615.c"), "func_801E5384")
        gear, character = FORCED_GEAR
        self.assertIn(f"D_800CCCE8.party_ids[1] = {character};", loader)
        self.assertIn(f"D_800CCCE8.record[i].bA0 = 0x{gear:X};", loader)
        results = function_body(source("ovl2596/ovl2596.c"), "func_801E211C")
        self.assertIn("func_80028470(0x10, 2);", results)
        self.assertIn("D_800D2C08[0] = func_80032E88(archive->items[0], 0);", results)

    def test_world_modes_follow_the_entry_and_their_writers(self):
        self.assertEqual(len(world_modes()), 19)
        entry = function_body(source("worldmap/worldmap.c"), "func_80070CFC")
        self.assertIn("mode = D_8006F954[0] & 0x7FFF;", entry)
        self.assertIn("step = D_8009A058[D_8009C5A8].enter;", entry)
        leave = function_body(source("field/field_800854D0.c"), "func_80093014")
        self.assertIn("D_8005A39C->unk2320 = func_8009D044(7, EVENT_OPERAND_BYTE(9));", leave)
        self.assertIn("D_8006F94E[3] = D_800D3278->operands[3];", source("ovl3087/ovl3087.c"))
        results = function_body(source("ovl2596/ovl2596.c"), "func_801E252C")
        self.assertIn(
            "} else if ((D_8006F94E & 0x7FF) >= 0x400) {\n            func_800199CC(3);", results
        )

    def test_sprite_kinds_follow_the_header_and_the_callback_table(self):
        callbacks = sprite_callbacks()
        self.assertEqual(len(callbacks), 16)
        self.assertEqual(
            [k for k, c in enumerate(callbacks) if c == "NULL"], [3, 4, 10, 11, 12, 13]
        )
        kind = function_body(source("resident/sprite_80022090.c"), "func_80023440")
        self.assertIn("s32 index = (*entry >> 8) & 7;", kind)
        self.assertIn("if ((*entry >> 14) & 1) {\n        index += 8;", kind)
        self.assertEqual([sprite_kind(f) for f in (0x0700, 0x4100, 0x47FF)], [7, 9, 15])
        dispatch = function_body(source("resident/sprite_800248D4.c"), "func_80025224")
        self.assertIn("func_8001CD64(task, D_8004FD40[kind]);", dispatch)
        loop = function_body(source("resident/sprite.c"), "func_8001C964")
        self.assertIn("if (task->update != NULL) {", loop)

    def test_tmd_kinds_follow_both_switches(self):
        kinds = tmd_kinds()
        self.assertEqual(sorted(kinds), [m | lit for lit in (0, 0x100) for m in range(0, 0x20, 4)])
        self.assertEqual((kinds[0].packet, kinds[0].reads), ("POLY_F3", 14))  # cmd[4..6], v at 8-c
        self.assertEqual((kinds[8].packet, kinds[0x108].packet), ("POLY_F4", "POLY_F4"))
        self.assertEqual((kinds[0x1C].packet, kinds[0x1C].reads), ("POLY_GT4", 44))
        self.assertEqual(tmd_kind(1, 0x21), 0x0)  # flag bit 0 set: no lighting
        self.assertEqual(tmd_kind(0, 0x3C), 0x11C)
        builder = kr_body(source("battle/battle_800B15D8.c"), "func_800B1720")
        self.assertIn("prims += (cmd[0] + 1) * 4;", builder)
        self.assertIn("cmd += (cmd[1] + 1) * 4;", builder)

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


class InventoryTests(unittest.TestCase):
    def test_every_function_pointer_table_is_in_the_inventory(self):
        """docs/scripts/interpreters.md lists every file-scope initializer that
        names functions (a table no code bounds)."""
        inventory = (ROOT / "docs/scripts/interpreters.md").read_text()
        definition = re.compile(
            r"\b(D_[0-9A-F]{8}|[a-z]\w*)\s*(?:\[[^\]=]*\])*\s*(?:\)\s*\([^)]*\))?"
            r"\s*(?:__attribute__\(\([^)]*\)\)\)\s*)?=\s*\{"
        )
        tables = set()
        for path in sorted((ROOT / "decomp/src").rglob("*.c")):
            text = re.sub(r"/\*.*?\*/", "", path.read_text(), flags=re.S)
            for match in definition.finditer(text):
                depth, end = 0, match.end() - 1
                for end in range(match.end() - 1, len(text)):
                    depth += {"{": 1, "}": -1}.get(text[end], 0)
                    if depth == 0:
                        break
                if re.search(r"\bfunc_[0-9A-F]{8}\b", text[match.end() : end]):
                    tables.add(match.group(1))
        self.assertIn("D_80088BFC", tables)  # the .text table menu6 places by attribute
        self.assertGreaterEqual(len(tables), 31)
        self.assertEqual(sorted(name for name in tables if f"`{name}`" not in inventory), [])


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
        entries = [b"", descriptor(0) + descriptor(7) + descriptor(8), descriptor(9, 0x10)]
        party = FormulaFamily()  # a flagged party descriptor hands over to its gear's
        family_census(party, entries, 1, 0x78, 3, 8, "set", redirect=True)
        self.assertEqual((party.sets, party.records, party.short), (2, 4, 2))
        self.assertEqual((party.formulas, party.gear_descriptors), ({0: 1, 7: 1, 8: 1}, 1))
        self.assertEqual(party.outside, ["set 0 #2: formula 8"])
        enemy = FormulaFamily()  # a flagged enemy descriptor keeps its own formula
        family_census(enemy, entries, 1, 0x78, 3, 8, "set")
        self.assertEqual((enemy.formulas[9], enemy.gear_descriptors), (1, 1))
        self.assertEqual(enemy.outside, ["set 0 #2: formula 8", "set 1 #0: formula 9"])

    def test_enemy_table_follows_func_800941a4(self):
        self.assertEqual(enemy_table((8, 0), in_gear=False), 0)
        self.assertEqual(enemy_table((8, 0), in_gear=True), 1)
        self.assertEqual(enemy_table((8, 0x10), in_gear=False), 1)

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
        self.assertEqual(list(act_commands(script, data)), [{31}, {None}, {9}, {VARIABLE}])

    def test_act_commands_follow_the_runners_paths(self):
        def commands(*words):
            data = b"".join(bytes(w) for w in words)
            return list(act_commands(battle_ai.analyse_script(data, 0, len(data)), data))

        # A skipped rule's write never reaches the next rule (a true one ends at fd).
        self.assertEqual(
            commands(
                (0x81, 0, 0, 0),
                (0x01, 1, 40, 0),
                (0xFD, 0, 0, 0),
                (0x80, 0, 0, 0),
                (0x01, 0, ACT, 0),
                (0xFD, 0, 0, 0),
            ),
            [{None}],
        )
        # Without the fd the true path runs on into the next rule: both arrive.
        self.assertEqual(
            commands(
                (0x81, 0, 0, 0),
                (0x01, 1, 40, 0),
                (0x80, 0, 0, 0),
                (0x01, 0, ACT, 0),
                (0xFD, 0, 0, 0),
            ),
            [{None, 40}],
        )


class TechniqueTests(unittest.TestCase):
    def test_pilots_and_masks_decide_which_slots_are_offered(self):
        gears, masks = {0: 18, 1: 2, 10: 18}, {0: 0x8000, 1: 0xFFF0, 10: 0}
        learned = {0: 6, 1: 15, 10: 0}
        outside = [(18, TECHNIQUE_BASE + 5, 90), (18, TECHNIQUE_BASE + 13, 91)]
        outside += [(17, TECHNIQUE_BASE + 12, 92), (2, TECHNIQUE_BASE + 12, 93)]
        result = techniques(gears, masks, learned, {(1, 3)}, {(1, 10)}, 0, outside)
        # learning reaches slot 5 for character 0 and stops at 13 slots for 1
        self.assertEqual(result.bits[0], 0xFC00)
        self.assertEqual(result.bits[1], 0xFFF8)
        # the copy gives character 10 character 1's masks and gears 2 and 3
        self.assertEqual(result.bits[10], 0xFFF8)
        self.assertEqual(result.pilots[2], {1, 10})
        self.assertEqual(result.pilots[17], {10})  # the formation flag's forced gear
        self.assertEqual(
            [selectable for *_, selectable in result.records], [True, False, True, True]
        )
        # a variable operand could name any character or gear
        self.assertTrue(
            techniques(gears, masks, learned, set(), set(), 1, outside[1:2]).records[0][4]
        )


class TmdTests(unittest.TestCase):
    def test_walk_checks_codes_packet_and_read_sizes(self):
        kinds = tmd_kinds()

        def model(*primitives: bytes) -> bytes:
            entry = struct.pack("<7I", 0, 0, 0, 0, 0x1C, len(primitives), 0)
            return struct.pack("<3I", 0x41, 0, 1) + entry + b"".join(primitives)

        good = bytes((4, 3, 1, 0x21)) + bytes(12)  # flat triangle: a POLY_F3, 14 bytes read
        short = bytes((4, 2, 1, 0x21)) + bytes(8)  # 12 data bytes
        wrong = bytes((5, 3, 1, 0x21)) + bytes(12)  # a 24-byte packet
        line = bytes((4, 3, 1, 0x41)) + bytes(12)  # a line code
        census = TmdCensus()
        walk_tmd(census, "m", model(good, short, wrong, line), 0, kinds)
        self.assertEqual(POLY_SIZES["POLY_F3"], 0x14)
        self.assertEqual(census.primitives[0], 4)
        self.assertEqual(len(census.errors), 3)
        self.assertIn("primitive 1 kind 0x0 has 12 bytes, 14 read", census.errors[0])
        self.assertIn("primitive 2 kind 0x0 sizes 24 packet bytes for a POLY_F3", census.errors[1])
        self.assertIn("primitive 3 mode 0x41 is not a polygon", census.errors[2])


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
