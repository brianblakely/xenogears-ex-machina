"""Invented encounter sets, bundles, area files and discs exercise the formation decoder."""

import contextlib
import io
import json
import re
import struct
import tempfile
import unittest
from pathlib import Path

from tools.analysis import battle_ai, dispatch_tables
from tools.analysis import formations as fm

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "decomp/include/resident/formation.h"


def packed(data: bytes) -> bytes:
    """Literal groups that tools.analysis.packed decodes to `data`, padded with
    zeros to whole groups as the original packer pads, and the flag byte the
    decoder reads before it stops."""
    data = data + bytes(-len(data) % 8)
    out = bytearray(struct.pack("<I", len(data)))
    for n in range(0, len(data), 8):
        out += b"\x00" + data[n : n + 8]
    return bytes(out + b"\x00")


def formation(battle=1, flags=0, stage=0, script=0, party=(0, 0, 0), enemies=()):
    """enemies: (id byte, flags byte, group byte) per enemy, the rest absent."""
    ids, flag_bytes, groups = bytearray([0xFF] * 8), bytearray([0xFF] * 8), bytearray([0xFF] * 8)
    for n, (ident, flag, group) in enumerate(enemies):
        ids[n], flag_bytes[n], groups[n] = ident, flag, group
    return bytes([battle, flags, stage, script, *party, 0]) + ids + flag_bytes + groups


def encounter_set(*records) -> bytes:
    records = list(records) + [formation()] * (fm.FORMATIONS - len(records))
    return b"".join(records)


def bundle(components: dict[int, tuple[int, bytes]]) -> bytes:
    """A map bundle: sizes at +0x10c and offsets at +0x130 (events.HEADER bytes),
    then each component's packed stream; `components` maps a component to its
    declared size and decoded bytes."""
    header = bytearray(0x154)
    body = bytearray()
    for component, (size, data) in components.items():
        struct.pack_into("<I", header, fm.SIZES + 4 * component, size)
        struct.pack_into("<I", header, fm.OFFSETS + 4 * component, len(header) + len(body))
        body += packed(data)
    return bytes(header + body)


def event_package(bytecode: bytes) -> bytes:
    """One actor whose 32 event entries all start at +0 (tools.analysis.field)."""
    return bytes(0x80) + struct.pack("<I", 1) + bytes(64) + bytecode


def area_file(tables: bytes | None) -> bytes:
    """An area file with every terrain kind on one table, or (None) with a
    header that ends at 0x30, before words 12-26."""
    if tables is None:
        return struct.pack("<10I", 10, *[0x30] * 9) + struct.pack("<6I", *[0xFFFFFFF0] * 6)
    header = struct.pack("<11I", 26, *[0x70] * 10) + struct.pack("<17I", *[0x70] * 16, 0)
    return header + tables


def enemy_file(templates=(), unique=()) -> bytes:
    """Eight enemy blocks with the same turn script (`fd`): ids in `unique`
    add a word of their own, ids in `templates` have a table that names offsets
    outside their block (no script table)."""
    blocks = []
    for enemy in range(8):
        if enemy in templates:
            blocks.append(struct.pack("<4H", 0, 0, 0xBB, 0) + bytes(4))
        else:
            own = bytes([0xFD, 0, 0, enemy]) if enemy in unique else b""
            blocks.append(
                struct.pack("<4H", 8, 0xFFFF, 0xFFFF, 0xFFFF) + bytes([0xFD, 0, 0, 0]) + own
            )
    offsets, position = [], 0x32
    for block in blocks:
        offsets.append(position)
        position += len(block)
    header = struct.pack("<25H", *offsets, *[0] * 16, position)
    return header + b"".join(blocks) + bytes(4)


class LayoutTests(unittest.TestCase):
    def test_fields_follow_the_shared_header(self):
        text = HEADER.read_text()
        body = re.search(
            r"typedef struct BattleFormation \{(.*?)\} BattleFormation;", text, re.S
        ).group(1)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        members, offset = [], 0
        for name, count in re.findall(r"\bu8 (\w+)(?:\[(\w+)\])?;", body):
            size = int(count, 0) if count else 1
            members.append((name, offset, size))
            offset += size
        self.assertEqual(tuple(members), fm.FIELDS)
        self.assertEqual(offset, fm.FORMATION)
        checks = re.findall(r"OFFSET_OF\(BattleFormation, (\w+)\) == (0x[0-9a-fA-F]+)", text)
        offsets = {name: place for name, place, _ in fm.FIELDS}
        self.assertTrue(checks)
        for name, place in checks:
            self.assertEqual(offsets[name], int(place, 16), name)
        self.assertIn("sizeof(BattleFormation) == 0x20", text)
        self.assertIn(f"sizeof(EncounterSet) == 0x{fm.SET:x}", text)

    def test_brackets_come_from_the_world_map_table(self):
        self.assertEqual(fm.brackets(), (0, 54, 201, 340, 0xFFFF))
        self.assertEqual(fm.bracket_text(), "0-53 54-200 201-339 340-65534")


class FormationTests(unittest.TestCase):
    def test_members_enemies_and_text(self):
        raw = formation(
            battle=23,
            flags=0xA3,
            stage=7,
            script=5,
            party=(1, 0x82, 0),
            enemies=(
                (0x01, 0x00, 0x00),
                (0x83, 0x80, 0x81),
                (0x7F, 0x80, 0x00),
                (0x02, 0x01, 0x03),
            ),
        )
        f = fm.decode_set(encounter_set(raw))[0]
        self.assertEqual((f.battle, f.flags, f.stage, f.script_set), (23, 0xA3, 7, 5))
        self.assertEqual(f.party_groups, bytes([1, 0x82, 0]))
        self.assertEqual([e.slot for e in f.enemies], [3, 4, 6])
        self.assertEqual(f.pairs, ((23, 1), (23, 3), (23, 2)))
        second = f.enemies[1]
        self.assertTrue(second.gear)
        self.assertEqual((second.flags, second.group & 0x7F), (0x80, 1))
        self.assertEqual([fm.enemy_text(e) for e in f.enemies], ["1/0", "3g/1h^", "2/3."])
        self.assertEqual(fm.flag_text(0xA3), "a3 event,cmd8,+03")
        self.assertEqual(fm.flag_text(0), "00")
        text = fm.formation_text(f)
        self.assertIn("party 1 2^ 0", text)
        self.assertIn("enemies 1/0 3g/1h^ 2/3.", text)

    def test_other_enemy_flag_bits_and_byte_7_are_shown(self):
        raw = bytearray(formation(enemies=((4, 0x42, 0),)))
        raw[7] = 9
        f = fm.Formation(bytes(raw))
        self.assertEqual(fm.enemy_text(f.enemies[0]), "4/0!42")
        self.assertTrue(fm.formation_text(f).endswith("unk7 09"))

    def test_a_set_needs_sixteen_formations(self):
        with self.assertRaises(fm.FormationError):
            fm.decode_set(bytes(fm.SET - 1))


class FieldTests(unittest.TestCase):
    def test_component_6_is_the_set_then_the_weights(self):
        weights = bytes(range(1, 17))
        data = bundle(
            {6: (fm.FIELD_SIZE, encounter_set(formation(battle=9)) + weights + b"\x11\x22")}
        )
        size, stream = fm.field_component(data)
        self.assertEqual((size, len(stream)), (0x210, 0x218))
        source = fm.field_source(4, size, stream)
        self.assertEqual(source.label, "field map 4")
        self.assertEqual(source.rows, (weights,))
        self.assertEqual(source.weight(2), (3,))
        self.assertEqual(source.formations[0].battle, 9)
        self.assertIn("writes 8 bytes more", source.note)

    def test_empty_short_and_odd_components(self):
        size, stream = fm.field_component(bundle({6: (0, b"")}))
        self.assertIsNone(fm.field_source(1, size, stream))
        self.assertIsNone(fm.field_component(bytes(0x100)))
        size, stream = fm.field_component(bundle({6: (0x100, bytes(0x100))}))
        with self.assertRaises(fm.FormationError):
            fm.field_source(1, size, stream)

    def test_scripts_name_formations_and_arm_the_draw(self):
        code = bytes([0x71, 0x05, 0x80])  # request_battle formation 5
        code += bytes([0xFE, 0x84, 0x03, 0x00, 0, 0, 0xFF, 0x7F, 0, 0])  # variable 3, no map
        code += bytes([0xF7, 0x2C, 0x81, 0x01, 0x80])  # period 300, count 1
        code += bytes([0xF7, 0x2C, 0x81, 0x00, 0x80])  # count 0
        code += bytes([0xF7, 0x2C, 0x81, 0x07, 0x00])  # count from variable 7
        code += bytes([0x00])  # end_slot
        package = event_package(code)
        data = bundle({5: (len(package), package), 6: (0, b"")})
        requests, armings, control = fm.field_scripts(data)
        self.assertEqual(requests, [(0, "71", 5, None), (3, "fe 84", None, 3)])
        self.assertEqual(armings, [(13, 300, 1), (18, 300, 0), (23, 300, None)])
        self.assertEqual([a.arms for a in armings], [True, False, False])
        self.assertEqual([a.text for a in armings], ["300/1", "300/0", "300/var"])
        self.assertFalse(control)
        package = event_package(bytes([0xA7, 0x00]))  # request_player_control
        self.assertTrue(fm.field_scripts(bundle({5: (len(package), package)})).control)


class TerrainTests(unittest.TestCase):
    def test_tables_lie_past_the_header_with_their_weight_rows(self):
        table = encounter_set(formation(battle=3)) + bytes(
            [1] * 16 + [0] * 16 + [2] * 16 + [0] * 16
        )
        data = area_file(table + bytes(0x20))
        self.assertEqual(fm.terrain_tables(data, 4), (0x70,) * 16)
        sources = fm.terrain_sources(44, data, 4)
        self.assertEqual(len(sources), 16)
        self.assertEqual(sources[5].label, "area file 44 kind 5")
        self.assertEqual(sources[0].weight(0), (1, 0, 2, 0))
        self.assertEqual(sources[0].formations[0].battle, 3)
        self.assertIn("32 bytes before the next table", sources[15].note)
        self.assertEqual(fm.terrain_tables(data[:-0x40], 4), (None,) * 16)

    def test_words_past_a_short_header_name_no_table(self):
        data = area_file(None) + bytes(0x400)
        self.assertEqual(fm.header_end(data), 0x30)
        self.assertEqual(fm.terrain_tables(data, 4), (None,) * 16)
        self.assertEqual(fm.terrain_sources(143, data, 4), [])


def census_with(
    sources, requests=None, chains=None, templates=(), unique=(), battles=2, armings=None
):
    census = fm.Census(1, sources=sources, requests=requests or {}, chains=chains or {})
    census.armings = armings or {}
    census.controls = set(census.armings)
    census.script_sets, census.stages, census.battles = 2, 4, battles
    for battle in range(battles):
        data = enemy_file(templates if battle == 0 else (), unique if battle == 1 else ())
        for block in battle_ai.enemy_blocks(data):
            census.blocks[(battle, block.enemy)] = block
            census.block_images[(battle, block.enemy)] = data[block.offset : block.end]
    return census


class CrossCheckTests(unittest.TestCase):
    def test_reach_pairs_and_bounds(self):
        records = encounter_set(
            formation(battle=1, flags=fm.EVENT, script=1, enemies=((0, 0, 0), (1, 0, 0))),
            formation(battle=0, enemies=((2, 0, 0),)),
            formation(battle=0, enemies=((3, 0, 0),)),
            formation(battle=1, stage=9, enemies=((4, 0, 0),)),
            formation(battle=5, enemies=((0, 0, 0),)),
            formation(battle=0, flags=fm.EVENT, script=7, enemies=((7, 0, 0),)),
        )
        weights = bytes([1] + [0] * 15)
        source = fm.EncounterSource("field", 3, None, records + weights, (weights,))
        debug = fm.EncounterSource(
            "debug", 4, None, encounter_set(formation(battle=1, enemies=((6, 0, 0),))), ()
        )
        requests = {3: [fm.Request(0, "71", 2, None), fm.Request(9, "fe 84", None, 0x400)]}
        armings = {3: [fm.Arming(12, 300, 0), fm.Arming(17, 300, 1)]}
        census = census_with(
            [source, debug], requests, {1: [3, None]}, templates=(7,), unique=(5,), armings=armings
        )
        ways = fm.reach(census)
        self.assertEqual(ways[("field", 3, None, 0)], {"drawn"})
        self.assertEqual(ways[("field", 3, None, 2)], {"named"})
        self.assertEqual(ways[("field", 3, None, 3)], {"chained"})
        self.assertEqual(ways[("field", 3, None, 1)], {"set"})
        check = fm.cross_check(census)
        self.assertEqual(check.pairs[(1, 0)], {"drawn"})
        self.assertEqual(check.pairs[(1, 4)], {"chained"})
        self.assertEqual(check.debug_pairs, {(1, 6)})
        self.assertEqual(check.outside, ["field map 3 formation 4: battle 5 id 0"])
        self.assertEqual(check.templates, [(0, 7)])
        self.assertIn((1, 5), check.unplaced)
        self.assertEqual(check.unique_unplaced, [(1, 5)])  # the other images are placed elsewhere
        self.assertEqual(check.bad_script_sets, ["field map 3 formation 5: set 7"])
        self.assertEqual(check.bad_stages, ["field map 3 formation 3: stage 9"])
        self.assertEqual(check.battles_unnamed, [])

    def test_a_field_draw_needs_an_arming_f7(self):
        records = encounter_set(
            formation(battle=1, flags=fm.EVENT, script=1, enemies=((0, 0, 0),)),
            formation(battle=1, enemies=((1, 0, 0),)),
        )
        weights = bytes([1] + [0] * 15)
        rows = (weights, bytes(16), bytes(16), bytes(16))
        sources = [
            fm.EncounterSource("field", map_id, None, records + weights, (weights,))
            for map_id in (3, 4, 5, 6)
        ] + [fm.EncounterSource("worldmap", 9, 0, records + b"".join(rows), rows)]
        armings = {  # map 3 has no f7
            4: [fm.Arming(0, 0, 1), fm.Arming(5, 900, 0)],
            5: [fm.Arming(0, 900, 1)],
            6: [fm.Arming(0, 900, 1)],
        }
        census = census_with(sources, chains={1: [1]}, armings=armings)
        census.controls.discard(5)  # no player control runs the draw
        ways = fm.reach(census)
        for map_id in (3, 4, 5):
            self.assertFalse(fm.armed(census, map_id))
            self.assertEqual(ways[("field", map_id, None, 0)], {"set"})
            self.assertEqual(ways[("field", map_id, None, 1)], {"set"})  # no chain either
        self.assertTrue(fm.armed(census, 6))
        self.assertEqual(ways[("field", 6, None, 0)], {"drawn"})
        self.assertEqual(ways[("field", 6, None, 1)], {"chained"})
        self.assertEqual(ways[("worldmap", 9, 0, 0)], {"drawn"})
        self.assertEqual(ways[("worldmap", 9, 0, 1)], {"chained"})
        text = " ".join(fm.disc_report(census))
        self.assertIn(
            "control 0c or a7): 1; such an f7 but no player control: [5];"
            " only f7s that do not arm: [4]; no f7: 1;",
            text,
        )
        self.assertIn("weighted formations never armed: 3", text)


class DiscImage:
    """A raw track (the directory table at sector 40, then each file from its
    own sector) and the extraction tools/extraction/disc_files.py writes."""

    def __init__(self):
        self.table = [0xFFFF] * 61
        self.files: dict[int, bytes | int] = {}

    def directory(self, group: int, index: int, first: int) -> None:
        self.table[group + index] = first

    def put(self, group: int, index: int, file: int, content: bytes | int) -> None:
        self.files[file + self.table[group + index] - 2] = content

    def write(self, root: Path, number: int) -> None:
        track = bytearray(41 * fm.SECTOR)
        start = 40 * fm.SECTOR + fm.USER
        track[start : start + 0x7A] = struct.pack("<61H", *self.table)
        extract = root / "extract" / f"disc{number}"
        (extract / "files").mkdir(parents=True)
        entries, lba = [], 41
        for slot, content in sorted(self.files.items()):
            if isinstance(content, int):
                entries.append({"slot": slot, "lba": lba, "size": content})
                continue
            entries.append({"slot": slot, "lba": lba, "size": len(content)})
            (extract / "files" / f"{slot:04d}.bin").write_bytes(content)
            for n in range(0, len(content), 2048):
                sector = bytearray(fm.SECTOR)
                chunk = content[n : n + 2048]
                sector[fm.USER : fm.USER + len(chunk)] = chunk
                track += sector
                lba += 1
        (root / "discs").mkdir(exist_ok=True)
        (root / "discs" / f"disc{number}.bin").write_bytes(track)
        (extract / "manifest.json").write_text(json.dumps({"files": entries}))


class SweepTests(unittest.TestCase):
    def build(self, root: Path) -> None:
        image = DiscImage()
        for group, index, first in (
            (4, 0, 100),
            (12, 1, 1000),
            (12, 3, 1200),
            (0x20, 0, 1400),
            (0x20, 3, 1500),
            (0x24, 0, 2000),
        ):
            image.directory(group, index, first)
        # Field: map 0 holds a set and requests formation 2, map 1 an empty component.
        records = encounter_set(
            formation(
                battle=1, flags=fm.EVENT, script=0, stage=1, enemies=((0, 0, 0), (1, 0x80, 1))
            ),
            formation(battle=0, enemies=((0, 0, 0),)),
            formation(battle=0, enemies=((3, 0, 0),)),
            formation(battle=1, enemies=((2, 0, 0),)),
            formation(battle=0, enemies=((7, 0, 0),)),
        )
        weights = bytes([5, 1] + [0] * 14)
        # f7 period 300 count 1 arms the draw, request_battle formation 2, player control.
        package = event_package(bytes([0xF7, 0x2C, 0x81, 0x01, 0x80, 0x71, 0x02, 0x80, 0xA7, 0x00]))
        image.put(4, 0, 0xB7, -4)
        image.put(
            4, 0, 0xB8, bundle({5: (len(package), package), 6: (fm.FIELD_SIZE, records + weights)})
        )
        image.put(4, 0, 0xBA, bundle({5: (len(package), package), 6: (0, b"")}))
        # World map: the first area file has tables, the others none.
        areas = sorted(set(dispatch_tables.area_files()))
        table = (
            encounter_set(formation(battle=1, enemies=((6, 0, 0),))) + bytes([1] * 16) + bytes(0x50)
        )
        for n, area in enumerate(areas):
            image.put(0x24, 0, area + 1, packed(area_file(table if n == 0 else None)))
        # Debug selector: one set, then a sub-directory.
        image.put(0x20, 3, 4, encounter_set(formation(battle=1, enemies=((5, 0, 0),))) + bytes(16))
        image.put(0x20, 3, 5, -3)
        # Enemy files of battles 0 and 1 (battle 0's id 7 without a script table).
        image.put(12, 1, 1, -4)
        image.put(12, 1, 2, enemy_file(templates=(7,)))
        image.put(12, 1, 4, enemy_file())
        image.put(12, 3, 5, -4)  # two stage pairs
        # The event archive: one set whose opcode 24 makes formation 3 the next battle.
        script = (
            bytes(0x40)
            + struct.pack("<I", 1)
            + bytes(16)
            + bytes([0x24, 0x03, 0x80, 0x04, 0x80, 0x00])
        )
        first, second = packed(script), packed(b"messages")
        image.put(0x20, 0, 2, struct.pack("<3I", 2, 12, 12 + len(first)) + first + second)
        for number in (1, 2):
            image.write(root, number)

    def test_sweep_lists_every_set_and_cross_checks_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.build(root)
            results = fm.sweep(root)
            lines = fm.report(results)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = fm.main(["--sweep", "--root", str(root)])
            with contextlib.redirect_stdout(io.StringIO()) as listed:
                fm.main(["--list", "field", "--item", "0", "--root", str(root)])
        census = results[0]
        self.assertEqual(status, 0)
        self.assertEqual([s.label for s in census.of("field")], ["field map 0"])
        self.assertEqual(census.empty_maps, [1])
        self.assertEqual(len(census.of("worldmap")), 16)
        self.assertEqual(len(census.tableless), len(set(dispatch_tables.area_files())) - 1)
        self.assertEqual([s.label for s in census.of("debug")], ["debug file 4"])
        self.assertEqual(census.requests[0], [(5, "71", 2, None)])
        self.assertEqual(census.armings[0], [(0, 300, 1)])
        self.assertEqual(census.chains, {0: [3]})
        self.assertEqual((census.script_sets, census.stages, census.battles), (1, 2, 2))
        check = fm.cross_check(census)
        self.assertEqual(check.pairs[(1, 0)], {"drawn"})
        self.assertEqual(check.pairs[(0, 3)], {"named"})
        self.assertEqual(check.pairs[(1, 2)], {"chained"})
        self.assertEqual(check.pairs[(0, 7)], {"set"})
        self.assertEqual(check.templates, [(0, 7)])
        self.assertEqual(check.debug_pairs, {(1, 5)})
        self.assertIn(
            "  field maps: 2 bundles (0 placeholder files), 1 with an encounter set,"
            " 1 with an empty component 6",
            lines,
        )
        self.assertIn("    placed pairs on blocks without a script table: 1", lines)
        self.assertIn("sets on both discs: 18, identical: 18", lines)
        # Map 1 runs the same script on whatever set was loaded before it.
        self.assertIn("with an empty component 6 and armed: [1];", " ".join(lines))
        text = listed.getvalue()
        self.assertIn("battle requests: +0x5 71 2", text)
        self.assertIn("draw: armed; f7 period/count: +0x0 300/1; player control: yes", text)
        self.assertIn("   0    5 battle   1", text)


if __name__ == "__main__":
    unittest.main()
