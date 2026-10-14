"""Invented walkmeshes, scripts, area files and discs exercise the platforming classifier
(tools.analysis.platforming); they describe no original content."""

import contextlib
import io
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tests.test_formations import DiscImage, bundle, encounter_set, enemy_file, formation, packed
from tools.analysis import dispatch_tables
from tools.analysis import formations as fm
from tools.analysis import platforming as pf
from tools.analysis.field import CollisionLayer, CollisionTriangle

# Attribute indices into WORDS: floor, slide, a wall for walkers on foot, a wall for gears, a floor
# no party on foot may jump from or over, and one that blocks layer 0 only.
FLOOR, SLIDE, FOOT_WALL, GEAR_WALL, FOOT_NO_JUMP, LAYER0 = range(6)
WORDS = [0, pf.LEDGE, 0x80, 0x40, 0x10, pf.LAYER0_BLOCK]


def layer(points, faces) -> CollisionLayer:
    """Triangles (corner indices, attribute index) over points (x, y, z), each wound as
    field_collision_find_floor_triangle accepts its inside, with the neighbour across each edge."""
    wound = []
    for (a, b, c), attribute in faces:
        corners = [pf.gte_xy(points[i][0], points[i][2]) for i in (a, b, c)]
        wound.append(((a, b, c) if pf.nclip(*corners) >= 0 else (a, c, b), attribute))
    edges = {}
    for t, (corners, _) in enumerate(wound):
        for e in range(3):
            edges.setdefault(frozenset((corners[e], corners[(e + 1) % 3])), []).append(t)
    triangles = []
    for t, (corners, attribute) in enumerate(wound):
        across = []
        for e in range(3):
            others = [u for u in edges[frozenset((corners[e], corners[(e + 1) % 3]))] if u != t]
            across.append(others[0] if others else 0xFFFF)
        triangles.append(CollisionTriangle(corners, tuple(across), attribute))
    return CollisionLayer(tuple(triangles), tuple((x, y, z, 0) for x, y, z in points))


def strip(xs, heights, attributes, width=100) -> CollisionLayer:
    """Squares along x, z from 0 to `width`: square i spans xs[i] to xs[i + 1], its floor going
    from heights[i] to heights[i + 1] (y down), both its triangles of attributes[i]."""
    points = [(x, y, z) for x, y in zip(xs, heights, strict=True) for z in (0, width)]
    faces = []
    for i, attribute in enumerate(attributes):
        a, d, b, e = 2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3  # (x_i, 0), (x_i, w), (x_i+1, 0) ...
        faces += [((a, b, d), attribute), ((b, e, d), attribute)]
    return layer(points, faces)


def walkmesh(mesh: CollisionLayer, party=pf.FOOT, index=1) -> pf.Walkmesh:
    return pf.Walkmesh(mesh, WORDS, index, party)


def region_at(mesh: pf.Walkmesh, x: float, z: float = 50) -> int:
    return mesh.region[mesh.locate(x, z)]


def collision(layers, words=WORDS) -> bytes:
    """A collision component (field.c's layout): the header, each layer's 14-byte triangles and
    8-byte vertices, then the attribute words."""
    head = 0x18 + 8 * len(layers)
    body, pointers, sizes = bytearray(), [], []
    for mesh in layers:
        triangles = b"".join(
            struct.pack("<3h4H", *t.vertices, *t.adjacent_raw, t.attribute_raw)
            for t in mesh.triangles
        )
        pointers += [head + len(body), head + len(body) + len(triangles)]
        sizes.append(len(triangles))
        body += triangles + b"".join(struct.pack("<4h", *v) for v in mesh.vertices)
    header = struct.pack("<6I", len(layers), *(sizes + [0] * (4 - len(layers))), head + len(body))
    words = b"".join(struct.pack("<I", word) for word in words)
    return header + struct.pack(f"<{len(pointers)}I", *pointers) + bytes(body) + words


def events(records, code: bytes) -> bytes:
    """One actor whose event 0 runs `code`, after the entry records (x, z, layer)."""
    head = b"\xff" + b"".join(struct.pack("<hhBBB", x, z, n, 0, 0) for x, z, n in records)
    entries = struct.pack("<32H", len(head), *[0] * 31)
    return bytes(0x80) + struct.pack("<I", 1) + entries + head + code


def iv(value: int) -> bytes:
    return struct.pack("<H", 0x8000 | value)


ARM = bytes([0xF7, 0x2C, 0x81, 0x01, 0x80, 0xA7])  # draw period 300 count 1, player control
GEARS = bytes([0xFE, 0x1E, 0x01])


def door(field_id: int, entry: int) -> bytes:
    return b"\x98" + iv(field_id) + iv(entry)  # change_map_entry


CAMERA = strip([0, 1000], [0, 0], [FLOOR])  # every map's top layer: the camera's floor


class PartyTests(unittest.TestCase):
    def test_the_measured_jumps_rise_and_reach(self):
        self.assertEqual((pf.FOOT.rise, pf.FOOT.reach), (118.25, 240))
        self.assertEqual((pf.GEAR.rise, pf.GEAR.reach), (131.25, 204))
        self.assertEqual((pf.FOOT.walk_block, pf.FOOT.air_block), (0x80, 0x10))
        self.assertEqual((pf.GEAR.walk_block, pf.GEAR.air_block), (0x40, 0x08))

    def test_a_jump_carries_less_far_the_higher_it_lands(self):
        self.assertTrue(pf.FOOT.carries(240, 0))
        self.assertFalse(pf.FOOT.carries(241, 0))
        self.assertTrue(pf.FOOT.carries(108, 118.25))
        self.assertFalse(pf.FOOT.carries(109, 118.25))
        self.assertTrue(pf.FOOT.carries(0, 100))  # straight up, then over
        self.assertFalse(pf.FOOT.carries(0, 119))
        self.assertTrue(pf.GEAR.carries(72, 131))
        self.assertFalse(pf.GEAR.carries(73, 131))


class WalkmeshTests(unittest.TestCase):
    def test_a_slide_up_needs_a_jump_and_its_way_down_is_a_fall(self):
        # A floor, a slide rising 100 over 100, a floor above.
        mesh = walkmesh(strip([0, 100, 200, 300, 400], [0, 0, 0, -100, -100], [0, 0, 1, 0]))
        low, high = region_at(mesh, 50), region_at(mesh, 350)
        self.assertNotEqual(low, high)
        self.assertEqual(mesh.walk_links, {(high, low)}, "the slide only goes down")
        self.assertIn((low, high), mesh.jump_links)
        self.assertEqual(mesh.jump_links[(low, high)], 100)
        self.assertEqual(mesh.jumps_from([low])[high], 1)
        self.assertEqual(mesh.jumps_from([high])[low], 0)
        self.assertEqual(mesh.falls(), (1, 0))

    def test_floors_join_whatever_their_heights(self):
        mesh = walkmesh(strip([0, 100, 200, 300], [0, 0, -300, -300], [0, 0, 0]))
        self.assertEqual(mesh.regions, 1)
        self.assertFalse(mesh.walk_links or mesh.jump_links)

    def test_a_slide_takes_the_direction_away(self):
        # A slide entered level from the floor rises on: the leader slides back, it is no stair.
        mesh = walkmesh(strip([0, 100, 150, 200, 300], [0, 0, -6, -100, -100], [0, 1, 1, 0]))
        low, high = region_at(mesh, 50), region_at(mesh, 250)
        self.assertNotEqual(low, high)
        self.assertEqual(mesh.jumps_from([low])[high], 1)
        self.assertEqual(mesh.jumps_from([high])[low], 0)

    def test_a_jump_over_a_valley_of_slides_links_its_sides(self):
        mesh = walkmesh(strip([0, 100, 150, 200, 300], [0, 0, 60, 0, 0], [0, 1, 1, 0]))
        left, right = region_at(mesh, 50), region_at(mesh, 250)
        self.assertNotEqual(left, right, "nobody walks up out of the valley")
        self.assertEqual(mesh.walk_links, set(), "the valley has no floor to slide onto")
        self.assertEqual(set(mesh.jump_links), {(left, right), (right, left)})

    def test_the_arc_decides_which_party_lands(self):
        def up(xs, heights, party):
            mesh = walkmesh(strip(xs, heights, [0, 0, 1, 0, 0]), party)
            return mesh.jump_links.get((region_at(mesh, 50), region_at(mesh, xs[-1] - 50)))

        # A slide rising 125 over 50: above a jump on foot, within a gear's 131.25.
        xs, heights = [0, 100, 200, 250, 350, 450], [0, 0, 0, -125, -125, -125]
        self.assertIsNone(up(xs, heights, pf.FOOT))
        self.assertEqual(up(xs, heights, pf.GEAR), 125)
        # A slide 150 wide rising 100: a gear is above 100 only to 132 units, a jump on foot to 156.
        xs, heights = [0, 100, 200, 350, 450, 550], [0, 0, 0, -100, -100, -100]
        self.assertEqual(up(xs, heights, pf.FOOT), 100)
        self.assertIsNone(up(xs, heights, pf.GEAR))
        # Jumping down the same slides lands whichever the party.
        mesh = walkmesh(strip(xs, heights, [0, 0, 1, 0, 0]), pf.GEAR)
        self.assertEqual(mesh.jump_links[(region_at(mesh, 500), region_at(mesh, 50))], -100)

    def test_the_party_decides_what_blocks(self):
        xs, heights = [0, 100, 200, 300], [0, 0, 0, 0]
        for wall, foot_regions, gear_regions in ((FOOT_WALL, 2, 1), (GEAR_WALL, 1, 2)):
            mesh = strip(xs, heights, [FLOOR, wall, FLOOR])
            self.assertEqual(walkmesh(mesh).regions, foot_regions)
            self.assertEqual(walkmesh(mesh, pf.GEAR).regions, gear_regions)
        # A floor the party on foot walks but neither jumps from nor lands on in the air.
        mesh = strip([0, 100, 200, 300, 400], [0, 0, 0, -100, -100], [FOOT_NO_JUMP] * 2 + [1, 0])
        self.assertEqual(walkmesh(mesh).jump_links, {})
        self.assertEqual(len(walkmesh(mesh, pf.GEAR).jump_links), 2)

    def test_0x800000_blocks_layer_0_only(self):
        mesh = strip([0, 100, 200, 300], [0, 0, 0, 0], [FLOOR, LAYER0, FLOOR])
        self.assertEqual(walkmesh(mesh, index=0).regions, 2)
        self.assertEqual(walkmesh(mesh, index=1).regions, 1)

    def test_an_entrance_stands_where_the_game_finds_the_floor(self):
        self.assertEqual(pf.gte_xy(5, -3), (-3, 4), "a negative z borrows from x")
        mesh = walkmesh(strip([0, 100, 200, 300, 400], [0, 0, 0, -100, -100], [0, 0, 1, 0]))
        self.assertIsNone(mesh.locate(500, 50))
        self.assertFalse(mesh.ledge(mesh.locate(50, 50)))
        slide = mesh.locate(250, 50)
        self.assertTrue(mesh.ledge(slide))
        self.assertEqual(mesh.regions_at(slide), {region_at(mesh, 50)}, "it slides down")
        # On an edge two triangles share (square 0's second, square 1's first), the first has it.
        self.assertEqual(mesh.locate(100, 50), 1)


class MapTests(unittest.TestCase):
    def test_entry_records_follow_the_ff_marker(self):
        code = events([(10, -20, 1), (300, 40, 0)], b"\x00")[0x84 + 64 :]
        self.assertEqual(pf.entry_record(code, 0), (10, -20, 1))
        self.assertEqual(pf.entry_record(code, 1), (300, 40, 0))
        self.assertIsNone(pf.entry_record(code, 2))
        self.assertIsNone(pf.entry_record(code, -1))
        self.assertIsNone(pf.entry_record(b"\x00" + code[1:], 0))

    def test_scripts_name_doors_and_the_gears(self):
        code = GEARS + door(5, 2)
        code += b"\x12" + iv(6) + struct.pack("<H", 0x0400) + iv(0) + iv(0)  # entry from a variable
        code += b"\xfe\x84" + iv(1) + bytes(2) + iv(0x7FFF) + iv(0)  # a battle that sets no map
        code += b"\x00"
        package = events([], code)
        scripts = pf.map_scripts(bundle({5: (len(package), package)}))
        self.assertEqual(scripts.doors, ((5, 2), (6, None)))
        self.assertTrue(scripts.gears)
        package = events([], ARM + b"\x00")
        self.assertFalse(pf.map_scripts(bundle({5: (len(package), package)})).gears)

    def test_a_map_counts_the_jumps_between_its_entrances(self):
        mesh = strip([0, 100, 200, 300, 400], [0, 0, 0, -100, -100], [0, 0, 1, 0])
        package = events([(50, 50, 0), (350, 50, 0), (50, 50, 1), (50, 50, 0)], b"\x00")
        data = bundle({1: (len(col := collision([mesh, CAMERA])), col), 5: (len(package), package)})
        result, meshes = pf.analyse_map(7, data, pf.map_scripts(data), {0, 1, 2, 9})
        self.assertEqual(len(meshes), 1, "the top layer is the camera's")
        self.assertEqual(result.pairs, {(0, 1): 1, (1, 0): 0})
        self.assertEqual(result.traversal, 1)
        self.assertEqual((result.jump_links, result.falls), (2, 1), "up and down; down slides")
        self.assertEqual(result.entrances[2], "layer 1: not an actor layer")
        self.assertEqual(result.entrances[9], "no entry record")
        self.assertIn("1 traversal jumps", result.text())


def path_area(regions) -> bytes:
    """An area file whose header ends at 0x30 (no terrain tables) and whose spot block there
    holds one path table of `regions` (scene, entry, kind) and three empty ones."""
    header = struct.pack("<12I", 0, *[0x30] * 9, 0, 0)
    rows = b"".join(struct.pack("<8h", 0, 0, 9, 9, s, e, 0, kind) for s, e, kind in regions)
    first, empty = 24, 24 + len(rows)
    spot = struct.pack("<2I4i", 8, 8, first, empty, empty, empty)
    return header + spot + rows + struct.pack("<8h", 0, 0, 0, 0, -1, 0, 0, 0)


class WorldMapTests(unittest.TestCase):
    def test_path_regions_name_entrances_but_destinations(self):
        data = path_area([(-0x7FFB, 2, 0), (6, 1, 4), (7, 3, 2)])  # scene 0x8005: map 5, a flag
        self.assertEqual(pf.path_regions(data), [(0, -0x7FFB, 2, 0), (0, 7, 3, 0)])
        self.assertEqual(pf.path_regions(data[:0x30]), [], "a spot block past the file")

    def test_the_fixed_regions_come_from_the_world_map_table(self):
        self.assertEqual(pf.fixed_regions(), [(0x138, 1, 14), (0x1B8, 1, 29), (0x122, 3, -1)])


def result(map_id: int, traversal: int) -> pf.MapResult:
    return pf.MapResult(map_id, "foot", pairs={(0, 1): traversal} if traversal else {})


class AreaTests(unittest.TestCase):
    def test_encounters_ignore_stages_and_weights(self):
        def source(stage, weight):
            records = encounter_set(
                formation(battle=3, stage=stage, enemies=((1, 0, 0),)),
                formation(battle=4, stage=stage, enemies=((2, 0, 0),)),
            )
            weights = bytes([weight, 0] + [0] * 14)
            return fm.EncounterSource("field", 0, None, records + weights, (weights,))

        self.assertEqual(pf.encounters(source(1, 2)), pf.encounters(source(9, 5)))
        self.assertEqual(pf.encounters(source(1, 2)), {(3, (1,))})

    def test_areas_join_through_unarmed_maps_and_split_at_other_encounters(self):
        links = {1: {2}, 2: {1, 3, 5}, 3: {2, 4}, 4: {3}, 5: {2, 6}, 6: {5}}
        keys = {1: "a", 3: "b", 4: "a", 6: "a"}  # 2 and 5 are not armed
        self.assertEqual(pf.group_areas(keys, keys, links), [[1, 6], [3], [4]])

    def test_the_rule_needs_half_the_maps_and_three_jumps(self):
        heavy = pf.Area([1, 2], [result(1, 1), result(2, 2)])
        self.assertTrue(heavy.heavy)
        self.assertEqual(
            heavy.reason(), "platforming-heavy: 2 of 2 armed maps need a traversal jump,"
            " 3 traversal jumps in all"
        )
        few = pf.Area([1, 2, 3], [result(1, 4), result(2, 0), result(3, 0)])
        self.assertFalse(few.heavy)
        self.assertIn("only 1 of 3 armed maps", few.reason())
        low = pf.Area([1, 2], [result(1, 1), result(2, 1)])
        self.assertEqual(low.reason(), "not heavy: only 2 traversal jumps in all")
        self.assertTrue(pf.Area([1, 2], [result(1, 3), result(2, 0)]).heavy, "half is enough")

    def test_an_area_takes_the_nearest_world_map_name(self):
        links = {1: {2}, 2: {1, 3}, 3: {2, 4}, 4: {3}}
        named = {3: ["Far", "Tower", "Tower"], 4: ["Farther"]}
        self.assertEqual(
            pf.nearest_name([1], links, named),
            ("Tower", "the world map's name for map 3, 2 door steps away"),
        )
        self.assertEqual(pf.nearest_name([4], links, named)[0], "Farther")
        self.assertEqual(pf.nearest_name([1], links, {4: ["Farther"]})[0], "map 1")


class DiscTests(unittest.TestCase):
    """Two discs holding the same invented dungeon: maps 0 and 1 (gears, the same encounters on
    different stages) need one and two jumps, map 2 joins them to map 3 (another set)."""

    def build(self, root: Path) -> None:
        image = DiscImage()
        for group, index, first in ((4, 0, 100), (12, 1, 1000), (12, 3, 1200), (0x20, 0, 1400),
                                    (0x24, 0, 2000)):
            image.directory(group, index, first)
        one_jump = strip([0, 100, 200, 250, 350, 450], [0, 0, 0, -125, -125, -125], [0, 0, 1, 0, 0])
        two_jumps = strip(
            [0, 100, 150, 250, 300, 400], [0, 0, -50, -50, -100, -100], [0, 1, 0, 1, 0]
        )
        flat = strip([0, 100, 200], [0, 0, 0], [0, 0])
        weights = bytes([1] + [0] * 15)
        enemy = ((2, 0, 0),)
        dungeon = [encounter_set(formation(battle=1, stage=s, enemies=enemy)) for s in (0, 3)]
        other = encounter_set(formation(battle=1, enemies=((0, 0, 0),)))
        maps = [
            (one_jump, [(50, 50, 0), (400, 50, 0)], GEARS + ARM + door(1, 0), dungeon[0]),
            (two_jumps, [(50, 50, 0), (350, 50, 0)], GEARS + ARM + door(0, 1), dungeon[1]),
            (flat, [(50, 50, 0)], door(1, 1) + door(3, 0) + door(2, 0), dungeon[0]),
            (flat, [(50, 50, 0)], ARM, other),
        ]
        image.put(4, 0, 0xB7, -2 * len(maps))
        for n, (mesh, records, code, records6) in enumerate(maps):
            col, package = collision([mesh, CAMERA]), events(records, code + b"\x00")
            data = bundle({1: (len(col), col), 5: (len(package), package),
                           6: (fm.FIELD_SIZE, records6 + weights)})
            image.put(4, 0, 0xB8 + 2 * n, data)
        for n, area in enumerate(sorted(set(dispatch_tables.area_files()))):
            # The world map enters map 0 at entry 0; a destination names map 3's entry 5.
            regions = [(0, 0, 0), (3, 5, 4)] if n == 0 else []
            image.put(0x24, 0, area + 1, packed(path_area(regions)))
        image.put(12, 1, 1, -4)
        image.put(12, 1, 2, enemy_file())
        image.put(12, 1, 4, enemy_file())
        image.put(12, 3, 5, -4)
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

    def test_the_dungeon_is_platforming_heavy_and_the_table_says_so(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.build(root)
            analyses = [pf.analyse_disc(root, number, names=False) for number in (1, 2)]
        first = analyses[0]
        self.assertEqual(first.armed, [0, 1, 3])
        self.assertEqual(first.entrances[0], {0, 1})
        self.assertEqual(first.entrances[3], {0}, "a destination is no entrance")
        self.assertEqual(first.results[0].party, "gear")
        self.assertEqual(first.results[3].party, "foot")
        self.assertEqual(first.results[0].pairs, {(0, 1): 1, (1, 0): 0})
        self.assertEqual(first.results[1].pairs, {(0, 1): 2, (1, 0): 0})
        self.assertEqual([area.maps for area in first.areas], [[0, 1], [3]])
        self.assertEqual([area.heavy for area in first.areas], [True, False])
        rows = pf.table(analyses)
        self.assertEqual(rows, [("maps 0-1", [0, 1])])
        self.assertEqual(
            pf.rust_table(rows).splitlines(),
            [
                "pub const PLATFORMING_AREAS: &[(&str, &[u16])] = &[",
                '    ("maps 0-1", &[0, 1]),',
                "];",
            ],
        )
        drawn = [f"disc {d}: drawn on field map {m} formation 0" for d in (1, 2) for m in (0, 1)]
        self.assertEqual(pf.lost_enemies(analyses, [0, 1]), {(1, 2): drawn})
        lines = pf.report(analyses)
        self.assertIn("  maps 0-1: [0, 1]", lines)
        self.assertIn(
            "    map 1 (gear): 2 traversal jumps, 4 jump links, 2 falls; entrances 2;"
            " pairs 0->1:2 1->0:0",
            lines,
        )

    def test_the_command_line_prints_and_checks_the_table(self):
        def run(*argv):
            with contextlib.redirect_stdout(io.StringIO()) as out:
                status = pf.main([*argv, "--root", str(root)])
            return status, out.getvalue()

        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            pf, "area_names", lambda disc, root: {}
        ):
            root = Path(temporary)
            self.build(root)
            status, rust = run("--rust")
            self.assertEqual(status, 0)
            shipped = root / "encounters.rs"
            shipped.write_text("//! The table.\n" + rust)
            self.assertEqual(run("--check", str(shipped))[0], 0)
            shipped.write_text(rust.replace("&[0, 1]", "&[0]"))
            status, text = run("--check", str(shipped))
            self.assertEqual(status, 1)
            self.assertIn("differs from the analysis", text)
            status, text = run("--map", "1")
        self.assertEqual(status, 0)
        self.assertIn("map 1 (gear): 2 traversal jumps", text)
        self.assertIn("entrance 1: layer 0 triangle", text)


if __name__ == "__main__":
    unittest.main()
