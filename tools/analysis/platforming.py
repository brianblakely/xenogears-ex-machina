"""Platforming-heavy areas: where the field's random battles meet jumps (plan.md, Phase 7).

A field map arms its random battles when its scripts reach event f7 with a nonzero period and count
and player control (tools.analysis.formations). This tool measures from the walkmesh and the scripts
how much of crossing each such map needs jumps, groups the maps into areas and applies the rule of
docs/runtime.md ("Random battles in platforming areas"):

- **Party.** The party crosses a map in its gears when the map's scripts reach fe 1e
  (field_event_switch_party_to_gears), else on foot. Its leader's flag bits 8-10 decide what blocks
  it (field_event_become_party_character): on foot attribute bit 7 blocks walking
  (field_collision_walk_attribute_aware: bits 5-7 against flags 8-10) and bit 4 airborne moves and
  jump starts (field_collision_walk, field_actor_is_jump_blocked_by_floor: bits 3-4 against flags
  9-10), in a gear bits 6 and 3; 0x800000 blocks layer 0. PARTIES holds each party's running jump as
  measured on the game (docs/runtime.md).
- **Layers.** Actors stand on layers 0 to count - 2 (field_event_place_actor_on_floor,
  field_collision_find_actor_floor); the top layer is the camera's floor
  (field_camera_walk_top_layer), which nobody walks.
- **Walk graph.** The leader walks from a floor (a triangle without 0x400000) to any neighbouring
  floor and onto a ledge (0x400000) that is not more than CLIMB above it
  (field_collision_walk_attribute_aware refuses a ledge whose floor is above the actor). A ledge
  takes the leader's direction away and slides it down (field_actor_apply_additive_motion), so from
  a ledge it only goes on to neighbours not more than CLIMB above. Heights are the triangles'
  centroids, as xem_core::field compares them.
- **Regions** are the strongly connected components of the floors, a slide down ledges joining the
  floor it starts from to the floors it ends on. A region link that leads down a slide is a walk
  link; a **fall** is one whose way back needs a jump (or has none on the layer).
- **Jump links** join two regions when a jump from a floor's edge into a ledge carries the leader
  over ledges to a floor of the other region: every edge it crosses within the jump's arc
  (Party.carries: the distance from the takeoff and the height above it, edge midpoints), and no
  triangle it may not cross in the air. No jump starts on a ledge
  (field_event_request_player_control).
- **Entrances** are the entry records (the event bytecode opens with ff; record n at 1 + 7n holds
  s16 x, s16 z and the layer: field_event_place_at_map_entry) that other maps' map changes name (12,
  47, 98, fe 84, fe cf with immediate field and entry operands; debug maps 720-729 aside) and the
  world map's path regions (the area files' four path tables and worldmap_fixed_path_regions; a
  region of kind 4 only names a destination). An entrance stands on the first triangle whose edges
  wind around it as field_collision_find_floor_triangle tests them.
- **Traversal jumps** of a map: between each ordered pair of its entrances on one layer, the fewest
  jump links on the way (0-1 breadth-first search, walk links free); the map's figure is the most
  any pair needs. A pair the layer does not join counts nothing.
- **Areas** are the armed maps whose draws start the same encounters (the battle and enemy ids of
  each formation with a weight: sets differing only in a stage, a weight or an undrawn formation
  are one area's) that doors join, through maps not armed with others. An area is
  **platforming-heavy** when at least half of its armed maps need at least one traversal jump and
  its maps' traversal jumps sum to at least 3. Both discs are read; an area heavy on either disc is
  listed, the maps of overlapping areas together. Its name is the nearest world-map entrance's (at
  most NAME_STEPS door steps, then the lowest map) from the area file's name table, read with
  tools.analysis.text_control's characters (the later areas' Japanese tables name nothing).

`--table` (the default) prints every area and its maps' metrics with the reason, and the (battle,
enemy id) pairs only met in random battles of the listed maps (formations.reach without their draw).
`--rust` prints PLATFORMING_AREAS for src/runtime/crates/xem-core/src/encounters.rs; `--check
[FILE]` fails unless FILE (that one) holds exactly that. `--map N [--disc D]` prints one map's
layers, regions, links and entrances. Inputs: .local/extract and .local/discs (`--root`); keep
listings under .local/.
"""

from __future__ import annotations

import argparse
import dataclasses
import math
import re
import struct
import sys
from collections import Counter, deque
from dataclasses import dataclass, field
from pathlib import Path

from tools.analysis import dispatch_tables, events, formations, text_control
from tools.analysis.arithmetic import signed16, signed32
from tools.analysis.disc_index import Disc
from tools.analysis.field import CollisionLayer, FieldError, collision_package
from tools.analysis.packed import PackedError, decode_block

ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / "src/runtime/crates/xem-core/src/encounters.rs"

COLLISION = 1  # BUNDLE_COLLISION (src/decomp/src/field/field_load.h)
SIZES, OFFSETS = 0x10C, 0x130  # FieldBundle.sizes, .offsets
SLACK = 0x10  # field_load_from_bundle decodes the component's size + 0x10
LEDGE = 0x400000  # a slide: not walked onto from below, no jump starts (field_motion.c)
LAYER0_BLOCK = 0x800000
CLIMB = 8  # a ledge more than this above a floor is not walked onto (xem_core::field)
GEARS = "fe 1e"  # switch_party_to_gears
DEBUG_MAPS = frozenset(range(720, 730))  # the development menu's maps, whose doors go everywhere
# Each map change: its field operand (events.FIELD_OPERAND) and the entry operand after it.
DOORS = {key: (operand, operand + 1) for key, operand in events.FIELD_OPERAND.items()}
ENTRY_MARKER = 0xFF  # field_event_place_at_map_entry: the bytecode opens with entry records
ENTRY_RECORD = 7
AREA_DIRECTORY = formations.AREA_DIRECTORY
SPOTS, NAMES = 0x04, 0x1C  # AreaHeader.spots, .names
PATH_TABLES = 4  # SpotHeader.table: four offsets, relative to the spot block
PATH_REGION = struct.Struct("<8h")  # PathRegion: x, z, w, h, scene, entry, link, kind
DESTINATION = 4  # a kind that only records a destination (worldmap_path_select_region)
FIXED_UNIT = "worldmap/worldmap_movement_terrain.c"  # worldmap_fixed_path_regions
HEAVY_SHARE, HEAVY_JUMPS = 2, 3  # half the armed maps need a jump; at least 3 in all
NAME_STEPS = 2  # how many door steps away an area's world-map name may come from


# ---------------------------------------------------------------------------
# The party
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Party:
    """The party leader as the collision code sees it, and its running jump: `arc` holds the
    distance from the takeoff and the height above it after each field update that moved it."""

    name: str
    walk_block: int
    air_block: int
    arc: tuple[tuple[float, float], ...]

    @property
    def rise(self) -> float:
        return max(height for _, height in self.arc)

    @property
    def reach(self) -> float:
        """How far a running jump goes before it touches down at its takeoff's height."""
        return max(distance for distance, height in self.arc if height >= 0)

    def carries(self, distance: float, rise: float) -> bool:
        """Whether a jump passes `distance` from its takeoff at `rise` above it or higher. In the
        air the leader may go slower than it runs, so any update as far or farther counts."""
        return any(d >= distance and h >= rise for d, h in self.arc)


# Measured headless (docs/runtime.md): the leader's FieldActor position (16.16) every vertical
# blank of a jump with Run and a direction held over open floor, from where it left the floor.
# Fei on foot (map 22): the impulse -20.75 and gravity 2.0 of analysis/formats/jump-physics.md,
# the first two updates straight up, then 12 units an update (16 running on the floor).
FOOT = Party(
    "foot",
    0x80,
    0x10,
    (
        (0, 20.75), (0, 39.5), (12, 56.25), (24, 71.0), (36, 83.75), (48, 94.5), (60, 103.25),
        (72, 110.0), (84, 114.75), (96, 117.5), (108, 118.25), (120, 117.0), (132, 113.75),
        (144, 108.5), (156, 101.25), (168, 92.0), (180, 80.75), (192, 67.5), (204, 52.25),
        (216, 35.0), (228, 15.75), (240, 0.0),
    ),
)  # fmt: skip
# The party in its gears (map 414 with game_data.inGear set): the impulse -24.75 and gravity 2.0
# for seven updates, one still update at the top, then a fall from rest; 12 units an update (as
# running on the floor).
GEAR = Party(
    "gear",
    0x40,
    0x08,
    (
        (0, 24.75), (0, 47.5), (12, 68.25), (24, 87.0), (36, 103.75), (48, 118.5), (60, 131.25),
        (72, 131.25), (84, 129.25), (96, 125.25), (108, 119.25), (120, 111.25), (132, 101.25),
        (144, 89.25), (156, 75.25), (168, 59.25), (180, 41.25), (192, 21.25), (204, 0.0),
    ),
)  # fmt: skip
PARTIES = {party.name: party for party in (FOOT, GEAR)}


# ---------------------------------------------------------------------------
# One layer
# ---------------------------------------------------------------------------


def gte_xy(x: int, z: int) -> tuple[int, int]:
    """A point as field_collision_find_floor_triangle loads it into the GTE: (x << 16) + z, read
    back as the two signed halves (a negative z borrows from x)."""
    packed = ((x << 16) + z) & 0xFFFFFFFF
    return signed16(packed & 0xFFFF), signed16(packed >> 16)


def nclip(a: tuple[int, int], b: tuple[int, int], c: tuple[int, int]) -> int:
    """The GTE's NCLIP of three screen points (MAC0, 32 bits)."""
    (x0, y0), (x1, y1), (x2, y2) = a, b, c
    return signed32(x0 * y1 + x1 * y2 + x2 * y0 - x0 * y2 - x1 * y0 - x2 * y1)


class Walkmesh:
    """One collision layer as one party crosses it (module notes)."""

    def __init__(self, layer: CollisionLayer, words: list[int], index: int, party: Party):
        count = len(layer.triangles)
        self.index, self.party = index, party
        self.points = [(x, y, z) for x, y, z, _ in layer.vertices]
        self.corners = [triangle.vertices for triangle in layer.triangles]
        self.word = []
        for triangle in layer.triangles:
            attribute = triangle.attribute_raw & 0xFF
            self.word.append(words[attribute] if attribute < len(words) else 0)
        self.neighbours = [
            tuple(n if 0 <= n < count else -1 for n in map(signed16, triangle.adjacent_raw))
            for triangle in layer.triangles
        ]
        self.floor = [sum(self.points[i][1] for i in corners) / 3 for corners in self.corners]
        self._slides: dict[int, frozenset[int]] = {}
        self._exits = {
            t: sorted(self.exits(t))
            for t in range(count)
            if not self.blocked(t) and not self.ledge(t)
        }
        self.region, self.regions = self._components()
        self.walk_links = {
            (self.region[t], self.region[u])
            for t, exits in self._exits.items()
            for u in exits
            if self.region[u] != self.region[t]
        }
        self.jump_links = self._jump_links()
        self._adjacent: dict[int, list[tuple[int, int]]] = {}
        for a, b in self.walk_links:
            self._adjacent.setdefault(a, []).append((b, 0))
        for a, b in self.jump_links:
            self._adjacent.setdefault(a, []).append((b, 1))

    def __len__(self) -> int:
        return len(self.corners)

    def blocked(self, t: int) -> bool:
        word = self.word[t]
        return bool(word & self.party.walk_block) or (self.index == 0 and bool(word & LAYER0_BLOCK))

    def air_blocked(self, t: int) -> bool:
        return self.blocked(t) or bool(self.word[t] & self.party.air_block)

    def ledge(self, t: int) -> bool:
        return bool(self.word[t] & LEDGE)

    def above(self, n: int, t: int) -> bool:
        """Whether n's floor is more than CLIMB above t's (y grows down)."""
        return self.floor[n] < self.floor[t] - CLIMB

    def moves(self, t: int):
        """Where the leader goes on from t without jumping."""
        if self.blocked(t):
            return
        for n in self.neighbours[t]:
            if n < 0 or self.blocked(n):
                continue
            if (self.ledge(n) or self.ledge(t)) and self.above(n, t):
                continue
            yield n

    def slide(self, t: int) -> frozenset[int]:
        """The floors a slide from ledge t ends on."""
        if t not in self._slides:
            ends, seen, queue = set(), {t}, deque([t])
            while queue:
                for n in self.moves(queue.popleft()):
                    if n in seen:
                        continue
                    seen.add(n)
                    if self.ledge(n):
                        queue.append(n)
                    else:
                        ends.add(n)
            self._slides[t] = frozenset(ends)
        return self._slides[t]

    def exits(self, t: int) -> set[int]:
        """The floors a walk from floor t reaches next: its floor neighbours and the slides'
        ends."""
        out = set()
        for n in self.moves(t):
            out |= self.slide(n) if self.ledge(n) else {n}
        out.discard(t)
        return out

    def _components(self) -> tuple[list[int], int]:
        """Strongly connected components of the floors under exits (Tarjan, iterative)."""
        successors = self._exits
        index, low, on, stack = {}, {}, set(), []
        region, regions = [-1] * len(self), 0
        for root in successors:
            if root in index:
                continue
            work = [(root, 0)]
            while work:
                node, i = work.pop()
                if i == 0:
                    index[node] = low[node] = len(index)
                    stack.append(node)
                    on.add(node)
                following = successors[node]
                if i < len(following):
                    work.append((node, i + 1))
                    nxt = following[i]
                    if nxt not in index:
                        work.append((nxt, 0))
                    elif nxt in on:
                        low[node] = min(low[node], index[nxt])
                    continue
                for nxt in following:
                    if nxt in on:
                        low[node] = min(low[node], low[nxt])
                if low[node] == index[node]:
                    while True:
                        member = stack.pop()
                        on.discard(member)
                        region[member] = regions
                        if member == node:
                            break
                    regions += 1
        return region, regions

    def midpoint(self, t: int, e: int) -> tuple[float, float, float]:
        """The middle of edge e of t (v0-v1, v1-v2, v2-v0), across which neighbour e lies."""
        a = self.points[self.corners[t][e]]
        b = self.points[self.corners[t][(e + 1) % 3]]
        return ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2)

    def _jump_links(self) -> dict[tuple[int, int], float]:
        """(region, region) -> the lowest rise of a jump between them."""
        links: dict[tuple[int, int], float] = {}
        for t in range(len(self)):
            if self.region[t] < 0 or self.air_blocked(t):
                continue
            for e, n in enumerate(self.neighbours[t]):
                if n < 0 or not self.ledge(n) or self.air_blocked(n):
                    continue
                tx, ty, tz = self.midpoint(t, e)
                seen, queue = {n}, deque([n])
                while queue:
                    over = queue.popleft()
                    for e2, u in enumerate(self.neighbours[over]):
                        if u < 0 or u in seen or self.air_blocked(u):
                            continue
                        ux, uy, uz = self.midpoint(over, e2)
                        rise = ty - uy
                        if not self.party.carries(math.hypot(ux - tx, uz - tz), rise):
                            continue
                        seen.add(u)
                        if self.ledge(u):
                            queue.append(u)
                        elif self.region[u] not in (-1, self.region[t]):
                            key = (self.region[t], self.region[u])
                            links[key] = min(links.get(key, rise), rise)
        return links

    def jumps_from(self, starts) -> dict[int, int]:
        """The fewest jump links from any of the regions `starts` to each region (0-1 BFS)."""
        cost = {start: 0 for start in starts}
        queue = deque(cost)
        while queue:
            a = queue.popleft()
            for b, weight in self._adjacent.get(a, ()):
                if cost[a] + weight < cost.get(b, math.inf):
                    cost[b] = cost[a] + weight
                    (queue.appendleft if weight == 0 else queue.append)(b)
        return cost

    def falls(self) -> tuple[int, int]:
        """Walk links whose way back needs a jump, and those with no way back on the layer."""
        need = none = 0
        for a, b in self.walk_links:
            back = self.jumps_from([b]).get(a)
            if back is None:
                none += 1
            elif back:
                need += 1
        return need, none

    def locate(self, x: int, z: int) -> int | None:
        """The first triangle whose edges wind around (x, z) as field_collision_find_floor_triangle
        tests them: each GTE normal clip at least 0."""
        p = gte_xy(x, z)
        for t, corners in enumerate(self.corners):
            a, b, c = (gte_xy(self.points[i][0], self.points[i][2]) for i in corners)
            if nclip(a, b, p) >= 0 and nclip(b, c, p) >= 0 and nclip(c, a, p) >= 0:
                return t
        return None

    def regions_at(self, t: int) -> frozenset[int]:
        """The regions a leader placed on triangle t is in: its own, or a slide's ends'."""
        if self.blocked(t):
            return frozenset()
        if self.ledge(t):
            return frozenset(self.region[u] for u in self.slide(t))
        return frozenset([self.region[t]])


# ---------------------------------------------------------------------------
# One map
# ---------------------------------------------------------------------------


def component(data: bytes, index: int) -> bytes:
    """A bundle component as field_load_from_bundle decodes it, cut to its size."""
    size = struct.unpack_from("<I", data, SIZES + 4 * index)[0]
    offset = struct.unpack_from("<I", data, OFFSETS + 4 * index)[0]
    return decode_block(data[offset:], output_limit=size + SLACK).data[:size]


def entry_record(bytecode: bytes, entry: int) -> tuple[int, int, int] | None:
    """(x, z, layer) of entry record `entry`, None when the bytecode holds none there."""
    at = 1 + ENTRY_RECORD * entry
    if bytecode[:1] != bytes([ENTRY_MARKER]) or entry < 0 or at + ENTRY_RECORD > len(bytecode):
        return None
    x, z = struct.unpack_from("<hh", bytecode, at)
    return x, z, bytecode[at + 4]


@dataclass(frozen=True)
class Scripts:
    """What a map's reachable scripts say about getting around it (events.walk)."""

    bytecode: bytes
    doors: tuple[tuple[int, int | None], ...]  # (map, entry: None for a variable) of map changes
    gears: bool  # fe 1e: the party crosses it in its gears


def map_scripts(data: bytes) -> Scripts:
    try:
        package = events.map_events(data)
    except (FieldError, PackedError):
        package = None
    if package is None:
        return Scripts(b"", (), False)
    starts, _ = events.script_entries(package)
    walked = events.walk(package.bytecode, [pc for _, _, pc in starts])
    doors, gears = [], False
    for _, ins in sorted(walked.instructions.items()):
        gears |= ins.key == GEARS
        if ins.key not in DOORS:
            continue
        target, entry = DOORS[ins.key]
        if not ins.immediate[target]:
            continue
        field_id = ins.operands[target] & 0x7FFF
        if field_id == events.NO_FIELD:
            continue
        index = ins.operands[entry] & 0x7FFF if ins.immediate[entry] else None
        doors.append((field_id & 0xFFF, index))
    return Scripts(package.bytecode, tuple(doors), gears)


@dataclass
class LayerMetrics:
    layer: int
    triangles: int
    ledges: int
    regions: int
    jump_links: int
    walk_links: int
    falls: int  # walk links whose way back needs a jump
    one_way: int  # walk links with no way back on the layer


@dataclass
class MapResult:
    map: int
    party: str
    layers: list[LayerMetrics] = field(default_factory=list)
    entrances: dict[int, str] = field(default_factory=dict)  # entry -> where it stands
    pairs: dict[tuple[int, int], int | None] = field(default_factory=dict)  # fewest jumps
    error: str = ""

    @property
    def traversal(self) -> int:
        """The most jumps a walk between two of its entrances needs."""
        return max((jumps for jumps in self.pairs.values() if jumps is not None), default=0)

    @property
    def jump_links(self) -> int:
        return sum(layer.jump_links for layer in self.layers)

    @property
    def falls(self) -> int:
        return sum(layer.falls for layer in self.layers)

    def text(self) -> str:
        if self.error:
            return f"map {self.map}: {self.error}"
        pairs = " ".join(
            f"{a}->{b}:{'-' if jumps is None else jumps}"
            for (a, b), jumps in sorted(self.pairs.items())
        )
        return (
            f"map {self.map} ({self.party}): {self.traversal} traversal jumps, {self.jump_links}"
            f" jump links, {self.falls} falls; entrances {len(self.entrances)}"
            + (f"; pairs {pairs}" if pairs else "")
        )


def analyse_map(map_id: int, data: bytes, scripts: Scripts, entries) -> tuple[MapResult, list]:
    """The map's metrics for its party, and its actor layers' walkmeshes."""
    party = GEAR if scripts.gears else FOOT
    result = MapResult(map_id, party.name)
    try:
        package = collision_package(component(data, COLLISION))
    except (FieldError, PackedError, struct.error) as error:
        result.error = f"collision: {error}"
        return result, []
    raw = package.attributes_raw
    words = [struct.unpack_from("<I", raw, at)[0] for at in range(0, len(raw) - 3, 4)]
    meshes = [Walkmesh(layer, words, n, party) for n, layer in enumerate(package.layers[:-1])]
    for mesh in meshes:
        falls, one_way = mesh.falls()
        result.layers.append(
            LayerMetrics(
                mesh.index,
                len(mesh),
                sum(mesh.ledge(t) for t in range(len(mesh))),
                mesh.regions,
                len(mesh.jump_links),
                len(mesh.walk_links),
                falls,
                one_way,
            )
        )
    placed: dict[int, list[tuple[int, frozenset[int]]]] = {}
    for entry in sorted(entries):
        record = entry_record(scripts.bytecode, entry)
        if record is None:
            result.entrances[entry] = "no entry record"
            continue
        x, z, layer = record
        if layer >= len(meshes):
            result.entrances[entry] = f"layer {layer}: not an actor layer"
            continue
        t = meshes[layer].locate(x, z)
        regions = frozenset() if t is None else meshes[layer].regions_at(t)
        if not regions:
            result.entrances[entry] = f"layer {layer} ({x}, {z}): no floor"
            continue
        result.entrances[entry] = f"layer {layer} triangle {t} regions {sorted(regions)}"
        placed.setdefault(layer, []).append((entry, regions))
    for layer, here in placed.items():
        for entry, regions in here:
            cost = meshes[layer].jumps_from(regions)
            for other, targets in here:
                if other != entry:
                    jumps = [cost[r] for r in targets if r in cost]
                    result.pairs[(entry, other)] = min(jumps) if jumps else None
    return result, meshes


# ---------------------------------------------------------------------------
# One disc
# ---------------------------------------------------------------------------


def path_regions(data: bytes) -> list[tuple[int, int, int, int]]:
    """(table, scene, entry, link) of an area file's path regions; a table outside the file
    (area file 198's fourth) holds none."""
    spots = struct.unpack_from("<I", data, SPOTS)[0]
    if spots + 8 > len(data):
        return []
    table = spots + struct.unpack_from("<I", data, spots + 4)[0]
    if table + 4 * PATH_TABLES > len(data):
        return []
    out = []
    for number in range(PATH_TABLES):
        at = spots + struct.unpack_from("<i", data, table + 4 * number)[0]
        while 0 <= at <= len(data) - PATH_REGION.size:
            _, _, _, _, scene, entry, link, kind = PATH_REGION.unpack_from(data, at)
            if scene == -1:
                break
            if kind != DESTINATION:
                out.append((number, scene, entry, link))
            at += PATH_REGION.size
    return out


def fixed_regions(root: Path = ROOT) -> list[tuple[int, int, int]]:
    """(scene, entry, link) of worldmap_fixed_path_regions."""
    text = dispatch_tables.unit(FIXED_UNIT, root)
    _, body = dispatch_tables.initializer(text, "worldmap_fixed_path_regions")
    rows = [[int(value, 0) for value in row.split(",")] for row in re.findall(r"\{([^}]*)\}", body)]
    return [(row[4], row[5], row[6]) for row in rows]


def area_names(disc: int, root: Path) -> dict[int, list[str]]:
    """Area file -> its name table's texts, in the characters text_control's map gives; a table
    most of whose texts hold more glyphs the map does not give than it does (the Japanese ones of
    the later areas) names nothing."""
    image = Disc(disc, root.parent)
    threshold = text_control.font_threshold(text_control.unpack(image.sectors(image.slot(0, 1, 6))))
    chars = text_control.character_map(image, threshold).glyphs
    names = {}
    for area in sorted(set(dispatch_tables.area_files())):
        data = text_control.unpack(image.sectors(image.slot(*AREA_DIRECTORY, area + 1)))
        table = data[struct.unpack_from("<I", data, NAMES)[0] :]
        offsets, _ = text_control.text_table(table)
        texts, readable = [], 0
        for offset in offsets:
            try:
                tokens = text_control.decode_text(table, offset, threshold)
            except text_control.TextError:
                texts.append("")
                continue
            glyphs = tuple(token for token in tokens if token.mnemonic == "glyph")
            known = sum(1 for g in glyphs if g.length == 1 and g.code in chars)
            readable += known > len(glyphs) - known
            texts.append(text_control.render(glyphs, chars).strip())
        names[area + 1] = texts if 2 * readable > len(texts) else []
    return names


@dataclass
class Area:
    maps: list[int]
    results: list[MapResult]
    name: str = ""
    named: str = ""  # where the name comes from

    @property
    def jumping(self) -> list[int]:
        return [result.map for result in self.results if result.traversal >= 1]

    @property
    def traversal(self) -> int:
        return sum(result.traversal for result in self.results)

    @property
    def heavy(self) -> bool:
        return (
            HEAVY_SHARE * len(self.jumping) >= len(self.maps) and self.traversal >= HEAVY_JUMPS
        )

    def reason(self) -> str:
        share = f"{len(self.jumping)} of {len(self.maps)} armed maps need a traversal jump"
        jumps = f"{self.traversal} traversal jumps in all"
        if self.heavy:
            return f"platforming-heavy: {share}, {jumps}"
        missing = []
        if HEAVY_SHARE * len(self.jumping) < len(self.maps):
            missing.append(f"only {share}")
        if self.traversal < HEAVY_JUMPS:
            missing.append(f"only {jumps}")
        return "not heavy: " + " and ".join(missing)


@dataclass
class DiscAnalysis:
    number: int
    census: formations.Census
    armed: list[int]
    scripts: dict[int, Scripts]
    entrances: dict[int, set[int]]
    links: dict[int, set[int]]  # door graph, both ways
    results: dict[int, MapResult]
    areas: list[Area]


def encounters(source: formations.EncounterSource) -> frozenset:
    """What a map's draw can start: (battle, enemy ids) of each formation with a weight. Sets that
    differ in nothing else (a stage, a weight, a formation no weight draws) are one area's."""
    return frozenset(
        (formation.battle, tuple(enemy.id for enemy in formation.enemies))
        for n, formation in enumerate(source.formations)
        if any(source.weight(n))
    )


def group_areas(armed, keys: dict[int, frozenset], links: dict[int, set[int]]) -> list[list[int]]:
    """Armed maps with the same encounters that doors join, through maps not armed with others."""
    groups, left = [], set(armed)
    while left:
        start = min(left)
        key, members, seen, stack = keys[start], {start}, {start}, [start]
        while stack:
            for other in links.get(stack.pop(), ()):
                if other in seen or (other in keys and keys[other] != key):
                    continue
                seen.add(other)
                if other in keys:
                    members.add(other)
                stack.append(other)
        left -= members
        groups.append(sorted(members))
    return sorted(groups)


def nearest_name(maps, links: dict[int, set[int]], named: dict[int, list[str]]) -> tuple[str, str]:
    """The name of the world-map entrance fewest door steps away, at most NAME_STEPS (then the
    lowest map; the name most of its path regions give, then the first in order), and whence."""
    seen, frontier = set(maps), sorted(maps)
    for steps in range(NAME_STEPS + 1):
        found = [m for m in frontier if named.get(m)]
        if found:
            counts = Counter(named[min(found)])
            name = min(counts, key=lambda text: (-counts[text], text))
            away = "its own" if steps == 0 else f"{steps} door step{'s' * (steps > 1)} away"
            return name, f"the world map's name for map {min(found)}, {away}"
        frontier = sorted({o for m in frontier for o in links.get(m, ()) if o not in seen})
        seen.update(frontier)
    name = f"map {maps[0]}" if len(maps) == 1 else f"maps {min(maps)}-{max(maps)}"
    return name, f"no world-map name within {NAME_STEPS} door steps"


def analyse_disc(root: Path, number: int, names: bool = True) -> DiscAnalysis:
    census = formations.read_disc(root, number)
    files = formations.DiscFiles(root, number)
    bundles, scripts = {}, {}
    for map_id, path in events.map_files(files.extract, files.track):
        data = path.read_bytes()
        if len(data) >= events.HEADER:
            bundles[map_id] = data
            scripts[map_id] = map_scripts(data)
    entrances: dict[int, set[int]] = {}
    links: dict[int, set[int]] = {}
    for map_id, found in scripts.items():
        if map_id in DEBUG_MAPS:
            continue
        for target, entry in found.doors:
            if entry is not None:
                entrances.setdefault(target, set()).add(entry)
            if target != map_id and target not in DEBUG_MAPS:
                links.setdefault(map_id, set()).add(target)
                links.setdefault(target, set()).add(map_id)
    named: dict[int, list[str]] = {}
    texts = area_names(number, root) if names else {}
    for area in sorted(set(dispatch_tables.area_files())):
        data = decode_block(files.sectors(files.slot(AREA_DIRECTORY, area + 1))).data
        for _, scene, entry, link in path_regions(data):
            entrances.setdefault(scene & 0xFFF, set()).add(entry)
            name = texts.get(area + 1, [])[link] if 0 <= link < len(texts.get(area + 1, [])) else ""
            if name:
                named.setdefault(scene & 0xFFF, []).append(name)
    for scene, entry, _ in fixed_regions():
        entrances.setdefault(scene & 0xFFF, set()).add(entry)
    armed = sorted(m for m in census.armings if formations.armed(census, m))
    results = {}
    for map_id in armed:
        results[map_id], _ = analyse_map(map_id, bundles[map_id], scripts[map_id],
                                         entrances.get(map_id, set()))
    keys = {m: frozenset() for m in armed}
    keys.update({s.item: encounters(s) for s in census.of("field") if s.item in results})
    areas = [
        Area(maps, [results[m] for m in maps], *nearest_name(maps, links, named))
        for maps in group_areas(armed, keys, links)
    ]
    return DiscAnalysis(number, census, armed, scripts, entrances, links, results, areas)


# ---------------------------------------------------------------------------
# The table
# ---------------------------------------------------------------------------


def table(analyses: list[DiscAnalysis]) -> list[tuple[str, list[int]]]:
    """(name, maps) of every area heavy on either disc, overlapping ones together, by first map."""
    merged: list[tuple[str, set[int]]] = []
    for analysis in analyses:
        for area in analysis.areas:
            if not area.heavy:
                continue
            maps = set(area.maps)
            for n, (name, others) in enumerate(merged):
                if others & maps:
                    merged[n] = (name, others | maps)
                    break
            else:
                merged.append((area.name, maps))
    return sorted(((name, sorted(maps)) for name, maps in merged), key=lambda row: row[1][0])


def rust_table(rows: list[tuple[str, list[int]]]) -> str:
    lines = ["pub const PLATFORMING_AREAS: &[(&str, &[u16])] = &["]
    lines += [f'    ("{name}", &[{", ".join(map(str, maps))}]),' for name, maps in rows]
    return "\n".join(lines + ["];"])


def met(census: formations.Census) -> dict[tuple[int, int], set[str]]:
    """(battle, enemy id) -> how and where the field and world map formations start it."""
    ways = formations.reach(census)
    out: dict[tuple[int, int], set[str]] = {}
    for source in census.sources:
        if source.kind == "debug":
            continue
        for n, formation in enumerate(source.formations):
            for way in ways[(source.kind, source.item, source.sub, n)] - {"set"}:
                for pair in formation.pairs:
                    out.setdefault(pair, set()).add(f"{way} on {source.label} formation {n}")
    return out


def lost_enemies(analyses: list[DiscAnalysis], maps) -> dict[tuple[int, int], list[str]]:
    """Pairs some formation starts on a disc that none starts on either disc once the listed
    maps' draws stop (their player control no longer runs it), with where they were met."""
    before: dict[tuple[int, int], set[str]] = {}
    after: set[tuple[int, int]] = set()
    for analysis in analyses:
        census = analysis.census
        for pair, how in met(census).items():
            before.setdefault(pair, set()).update(f"disc {census.disc}: {w}" for w in how)
        after |= set(met(dataclasses.replace(census, controls=census.controls - set(maps))))
    return {pair: sorted(how) for pair, how in sorted(before.items()) if pair not in after}


# ---------------------------------------------------------------------------
# Reports
# ---------------------------------------------------------------------------


def report(analyses: list[DiscAnalysis]) -> list[str]:
    rows = table(analyses)
    lines = [
        f"rule: an area is platforming-heavy when at least 1/{HEAVY_SHARE} of its armed maps need a"
        f" traversal jump and its maps' traversal jumps sum to at least {HEAVY_JUMPS}",
        "jumps: " + "; ".join(
            f"{p.name} rise {p.rise:g}, flat reach {p.reach:g}" for p in PARTIES.values()
        ),
    ]
    for analysis in analyses:
        armed, areas = len(analysis.armed), len(analysis.areas)
        lines.append(f"disc {analysis.number}: {armed} armed maps in {areas} areas")
        for area in analysis.areas:
            lines.append(f"  {area.name} {area.maps} ({area.named}): {area.reason()}")
            if area.traversal or any(result.jump_links for result in area.results):
                lines += [f"    {result.text()}" for result in area.results]
    lines.append("table (PLATFORMING_AREAS):")
    lines += [f"  {name}: {maps}" for name, maps in rows]
    lost = lost_enemies(analyses, [m for _, maps in rows for m in maps])
    lines.append(f"(battle, enemy id) met only in random battles of these maps: {len(lost)}")
    for pair, how in lost.items():
        lines.append(f"  {pair}: {'; '.join(how)}")
    return lines


def map_listing(analysis: DiscAnalysis, root: Path, map_id: int) -> list[str]:
    files = formations.DiscFiles(root, analysis.number)
    data = dict(events.map_files(files.extract, files.track))[map_id].read_bytes()
    scripts = analysis.scripts[map_id]
    result, meshes = analyse_map(map_id, data, scripts, analysis.entrances.get(map_id, set()))
    lines = [result.text(), f"  doors: {sorted(set(scripts.doors), key=str)}"]
    for mesh, metrics in zip(meshes, result.layers, strict=True):
        lines.append(f"  {metrics}")
        sizes: dict[int, int] = {}
        for t in range(len(mesh)):
            if mesh.region[t] >= 0:
                sizes[mesh.region[t]] = sizes.get(mesh.region[t], 0) + 1
        lines.append(f"    regions (triangles): {dict(sorted(sizes.items()))}")
        lines.append(f"    walk links: {sorted(mesh.walk_links)}")
        lines.append(
            "    jump links (rise): "
            + ", ".join(f"{a}->{b} ({rise:g})" for (a, b), rise in sorted(mesh.jump_links.items()))
        )
    lines += [f"  entrance {entry}: {where}" for entry, where in sorted(result.entrances.items())]
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--table", action="store_true", help="every area's metrics (the default)")
    parser.add_argument("--rust", action="store_true", help="print PLATFORMING_AREAS")
    parser.add_argument(
        "--check", type=Path, nargs="?", const=TABLE, metavar="FILE", help="FILE holds that table"
    )
    parser.add_argument("--map", type=int, help="one map's layers, regions, links, entrances")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1, help="with --map")
    parser.add_argument("--root", type=Path, default=ROOT / ".local", help="extract/ and discs/")
    args = parser.parse_args(argv)
    if args.map is not None:
        analysis = analyse_disc(args.root, args.disc, names=False)
        print("\n".join(map_listing(analysis, args.root, args.map)))
        return 0
    analyses = [analyse_disc(args.root, number) for number in (1, 2)]
    generated = rust_table(table(analyses))
    if args.rust:
        print(generated)
        return 0
    if args.check is not None:
        if generated in args.check.read_text():
            print(f"{args.check}: PLATFORMING_AREAS matches the analysis")
            return 0
        print(f"{args.check}: PLATFORMING_AREAS differs from the analysis, which gives:")
        print(generated)
        return 1
    print("\n".join(report(analyses)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
