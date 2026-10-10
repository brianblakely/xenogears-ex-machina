"""Battle formation and encounter set decoder (resident/formation.h).

The battle overlay copies formation formation_selected_index of the encounter set formation_encounter_set
into formation_active as it starts (battle battle_main). A set is 16 formations of
0x20 bytes, BattleFormation in decomp/include/resident/formation.h, whose
member comments name their readers; FIELDS follows that struct (the tests
compare the two) and FLAGS names the flag bits its readers test. Sets come
from:

- a field map: map bundle component 6 (sizes at +0x10c, offsets at +0x130),
  which field_load_from_bundle decodes into formation_encounter_set itself: the set, then the 16
  weights formation_encounter_weights of the random draw (field_encounter_count_down). The packed stream may
  end a few bytes later, in commons_unused_4_words_b; an empty component leaves the set in
  place. Player control (events 0c, a7: field_event_request_player_control) runs the draw, which
  waits until a script of the map runs event f7 with a nonzero period and count
  (field_event_draw_random_picks; every map load clears both).
- the world map: area files (0x24, 0) area + 1 of worldmap_area_file_sets. worldmap_unpack_area_data
  points worldmap_encounter_sets[kind] at the offsets in header words 11-26; the roll
  (worldmap_encounter_roll) draws by the 16 weights at +0x200 + 16 * bracket, the bracket
  from the scene id (variable 0) against worldmap_encounter_level_brackets, and copies the kind's set.
- the debug battle selector: the first 0x200 bytes of (0x20, 3) file 4-6 or
  7 + n (ovl2606 battle_scene_select_main).

A field script names formation n of its map's set for a battle (events 71 and
fe 84, operand 1, an immediate when bit 15 is set: field_event_request_battle,
field_event_request_battle_field). A battle event script's opcode 24 makes formation n of the same
set the next battle (ovl3087 battle_event_script_next_battle, taken by 80070f40).

`--sweep` decodes every set on both discs, prints aggregate counts and
cross-checks the (battle, enemy id) pairs the formations place against the
enemy AI blocks (tools.analysis.battle_ai), the event script sets against the
battle event archive (tools.analysis.battle_event_vm) and the stages against
directory (12, 3). `--list field|worldmap|debug [--item N] [--disc D]`
prints the formations of every map, area file or debug file (or one); keep
listings under .local/.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field
from functools import cache
from pathlib import Path
from typing import NamedTuple

from tools.analysis import battle_ai, battle_event_vm, dispatch_tables, events
from tools.analysis.field import FieldError
from tools.analysis.packed import PackedError, decode_block

ROOT = Path(__file__).resolve().parents[2]
SECTOR, USER = 2352, 24

FORMATION = 0x20
FORMATIONS = 16
SET = FORMATION * FORMATIONS  # what the battle indexes and the world map copies
WEIGHTS = FORMATIONS  # one weight per formation
FIELD_SIZE = SET + WEIGHTS  # formation_encounter_set then formation_encounter_weights
# BattleFormation (resident/formation.h): member, offset, bytes.
FIELDS = (
    ("battle", 0x00, 1),
    ("flags", 0x01, 1),
    ("stage", 0x02, 1),
    ("scriptSet", 0x03, 1),
    ("partyGroups", 0x04, 3),
    ("unk7", 0x07, 1),
    ("enemyIds", 0x08, 8),
    ("enemyFlags", 0x10, 8),
    ("enemyGroups", 0x18, 8),
)
# The flag bits a reader tests (formation.h); no reader tests 0x01-0x04.
FLAGS = {
    0x08: "noresults",  # ovl2596 battle_results_grant_rewards, battle battle_main
    0x10: "party10",  # ovl2615 battle_setup_load_party_and_enemy_files (battle_uses_fixed_party), battle battle_upload_party_portraits, ovl2596
    0x20: "event",  # ovl2615 battle_setup_init_command_menus (battle_uses_event_script), ovl3087 battle_event_script_load
    0x40: "cmd7",  # ovl2615 battle_setup_init_command_menus
    0x80: "cmd8",  # ovl2615 battle_setup_init_command_menus
}
EVENT = 0x20
NO_ENEMY = 0x7F  # enemyIds & 0x7f (ovl2615 battle_setup_place_formation)
GEAR = 0x80  # enemyIds: slot byte 4
HIDDEN, BYTE5 = 0x80, 0x01  # enemyFlags: slot bytes 3 and 5
FIRST_ENEMY_SLOT = 3

FIELD_COMPONENT = 6  # field_load_from_bundle decodes it into formation_encounter_set
SIZES, OFFSETS = 0x10C, 0x130  # FieldBundle.sizes, .offsets
SLACK = 0x10  # field_load_from_bundle passes the component's size + 0x10
AREA_DIRECTORY = (0x24, 0)  # the world map's (80028470(0x24, 0))
AREA_TABLES = 0x2C  # AreaHeader word 11: the 16 terrain tables (worldmap_unpack_area_data)
KINDS = 16
BRACKET_UNIT = "worldmap/worldmap_movement_terrain.c"  # worldmap_encounter_level_brackets
DEBUG_DIRECTORY = (0x20, 3)  # ovl2606 battle_scene_select_main: 80028470(0x20, 3)
DEBUG_FIRST = 4  # Event1-3 are files 4-6, FileNo n file 7 + n
STAGE_DIRECTORY = (12, 3)  # mode_load_battle_stage: 80028470(12, 3)
STAGE_COUNT = 5  # mode_load_battle_stage: scene < the file count of entry 5 / 2
SCRIPT_DIRECTORY = (0x20, 0)  # ovl3087's
SCRIPT_ARCHIVE = 2  # ovl3087 battle_event_script_load
REQUESTS = ("71", "fe 84")  # field events that set formation_selected_index from operand 1
ARMING = "f7"  # field_event_draw_random_picks: the field draw's period and count from operands 1 and 3
CONTROL = ("0c", "a7")  # player control (field_event_request_player_control), which runs the draw field_encounter_count_down
CHAIN = 0x24  # battle event opcode next_battle (battle_event_script_next_battle)


class FormationError(ValueError):
    """Bytes the recovered readers cannot take as encounter sets."""


class Request(NamedTuple):
    """A field event that sets formation_selected_index from operand 1 (field_event_read_imm_or_var)."""

    pc: int
    event: str  # 71 or fe 84
    formation: int | None  # an immediate operand's value (formation_selected_index is a u8)
    variable: int | None  # the variable it names otherwise


class Arming(NamedTuple):
    """A field event f7 (field_event_draw_random_picks): it sets the draw's period
    field_work.unk2298 from operand 1 and its count unk229C from operand 3 (the
    debug monitor's TIME and ENCOUNT), then field_encounter_draw_steps draws that many
    distinct countdowns. field_encounter_count_down returns while the period is 0, and
    field_encounter_draw_steps clears the period when the count is 0."""

    pc: int
    period: int | None  # an immediate operand's value; None for a variable
    count: int | None

    @property
    def arms(self) -> bool:
        """Both operands are nonzero immediates (a variable's value is not known)."""
        return bool(self.period) and bool(self.count)

    @property
    def text(self) -> str:
        return "/".join("var" if v is None else str(v) for v in (self.period, self.count))


class Scripts(NamedTuple):
    """What a map's scripts can reach (events.walk)."""

    requests: list[Request]
    armings: list[Arming]
    control: bool  # player control, 0c or a7


# ---------------------------------------------------------------------------
# Formations
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Enemy:
    slot: int  # battle slot 3-10
    id: int  # its id in the battle's enemy file
    gear: bool  # fights in a gear (slot byte 4)
    flags: int  # the enemyFlags byte: HIDDEN to slot byte 3, BYTE5 to slot byte 5
    group: int  # the enemyGroups byte; & 0x7f is its formation group


MEMBERS = {name: (offset, size) for name, offset, size in FIELDS}


@dataclass(frozen=True)
class Formation:
    raw: bytes

    def member(self, name: str) -> bytes:
        offset, size = MEMBERS[name]
        return self.raw[offset : offset + size]

    @property
    def battle(self) -> int:
        return self.member("battle")[0]

    @property
    def flags(self) -> int:
        return self.member("flags")[0]

    @property
    def stage(self) -> int:
        return self.member("stage")[0]

    @property
    def script_set(self) -> int:
        return self.member("scriptSet")[0]

    @property
    def unk7(self) -> int:
        return self.member("unk7")[0]

    @property
    def party_groups(self) -> bytes:
        return self.member("partyGroups")

    @property
    def enemies(self) -> tuple[Enemy, ...]:
        ids, flags, groups = (self.member(m) for m in ("enemyIds", "enemyFlags", "enemyGroups"))
        return tuple(
            Enemy(FIRST_ENEMY_SLOT + n, ids[n] & 0x7F, bool(ids[n] & GEAR), flags[n], groups[n])
            for n in range(len(ids))
            if ids[n] & 0x7F != NO_ENEMY
        )

    @property
    def pairs(self) -> tuple[tuple[int, int], ...]:
        """The (battle, enemy id) pairs it places."""
        return tuple((self.battle, enemy.id) for enemy in self.enemies)


def decode_set(data: bytes) -> tuple[Formation, ...]:
    if len(data) < SET:
        raise FormationError(f"an encounter set needs 0x{SET:x} bytes, not 0x{len(data):x}")
    return tuple(
        Formation(bytes(data[n * FORMATION : (n + 1) * FORMATION])) for n in range(FORMATIONS)
    )


def flag_text(flags: int) -> str:
    names = [name for bit, name in FLAGS.items() if flags & bit]
    unread = flags & ~sum(FLAGS) & 0xFF
    if unread:
        names.append(f"+{unread:02x}")
    return f"{flags:02x}" + (f" {','.join(names)}" if names else "")


def enemy_text(enemy: Enemy) -> str:
    """id[g]/group[h][.][^]: g in a gear, h hidden, . slot byte 5, ^ group bit 7;
    other enemyFlags bits as !xx."""
    text = f"{enemy.id}{'g' if enemy.gear else ''}/{enemy.group & 0x7F}"
    text += "h" if enemy.flags & HIDDEN else ""
    text += "." if enemy.flags & BYTE5 else ""
    text += "^" if enemy.group & 0x80 else ""
    other = enemy.flags & ~(HIDDEN | BYTE5) & 0xFF
    return text + (f"!{other:02x}" if other else "")


def formation_text(formation: Formation) -> str:
    party = " ".join(f"{g & 0x7F}{'^' if g & 0x80 else ''}" for g in formation.party_groups)
    enemies = " ".join(enemy_text(e) for e in formation.enemies) or "-"
    unk7 = f"  unk7 {formation.unk7:02x}" if formation.unk7 else ""
    return (
        f"battle {formation.battle:3d}  stage {formation.stage:3d}"
        f"  script {formation.script_set:3d}  flags {flag_text(formation.flags):<21}"
        f" party {party:<8} enemies {enemies}{unk7}"
    )


@cache
def brackets(root: Path = ROOT) -> tuple[int, ...]:
    """worldmap_encounter_level_brackets: the scene ids that start each weight row (worldmap_encounter_roll
    stops its search at the last entry, so it starts none)."""
    text = dispatch_tables.unit(BRACKET_UNIT, root)
    _, body = dispatch_tables.initializer(text, "worldmap_encounter_level_brackets")
    return tuple(int(value, 0) for value in body.split(",") if value.strip())


# ---------------------------------------------------------------------------
# The sets on a disc
# ---------------------------------------------------------------------------


class DiscFiles:
    """One disc's files as the resident addresses them (tools.analysis.disc_index):
    directory (group, index) file f is index entry f + table[group + index] - 2."""

    def __init__(self, root: Path, number: int):
        self.number = number
        self.track = root / "discs" / f"disc{number}.bin"
        self.extract = root / "extract" / f"disc{number}"
        manifest = json.loads((self.extract / "manifest.json").read_text())
        self.entries = {entry["slot"]: entry for entry in manifest["files"]}
        self.directories = battle_ai.directory_table(self.track)

    def slot(self, directory: tuple[int, int], file: int) -> int:
        group, index = directory
        return file + self.directories[group + index] - 2

    def size(self, slot: int) -> int | None:
        entry = self.entries.get(slot)
        return None if entry is None else entry["size"]

    def data(self, slot: int) -> bytes:
        return (self.extract / "files" / f"{slot:04d}.bin").read_bytes()

    def sectors(self, slot: int) -> bytes:
        """The file's whole sectors, as the drive reads them: a packed area
        file's stream goes on past the file's size."""
        entry = self.entries[slot]
        out = bytearray()
        with self.track.open("rb") as track:
            for n in range((entry["size"] + 2047) // 2048):
                track.seek((entry["lba"] + n) * SECTOR + USER)
                out += track.read(2048)
        return bytes(out)


@dataclass(frozen=True)
class EncounterSource:
    kind: str  # field, worldmap, debug
    item: int  # map, area file, debug file
    sub: int | None  # the world map's terrain kind
    data: bytes  # what the readers take: the set and its weights
    rows: tuple[bytes, ...]  # weight rows: the field's one, the world map's per bracket
    note: str = ""

    @property
    def label(self) -> str:
        names = {"field": "field map", "worldmap": "area file", "debug": "debug file"}
        kind = f" kind {self.sub}" if self.sub is not None else ""
        return f"{names[self.kind]} {self.item}{kind}"

    @property
    def formations(self) -> tuple[Formation, ...]:
        return decode_set(self.data)

    def weight(self, n: int) -> tuple[int, ...]:
        return tuple(row[n] for row in self.rows)


def field_component(data: bytes) -> tuple[int, bytes] | None:
    """Map bundle component 6 as field_load_from_bundle writes it at formation_encounter_set: the
    header's size and the whole decoded stream (empty for an empty component);
    None for a file too short to be a bundle (events.map_events)."""
    if len(data) < events.HEADER:
        return None
    size = struct.unpack_from("<I", data, SIZES + 4 * FIELD_COMPONENT)[0]
    offset = struct.unpack_from("<I", data, OFFSETS + 4 * FIELD_COMPONENT)[0]
    return size, decode_block(data[offset:], output_limit=size + SLACK).data


def field_source(map_id: int, size: int, stream: bytes) -> EncounterSource | None:
    if size == 0 and not stream:
        return None
    if size < FIELD_SIZE or len(stream) < FIELD_SIZE:
        raise FormationError(f"component 6 of 0x{size:x} bytes is not a set and its weights")
    tail = len(stream) - FIELD_SIZE
    note = f"component 6, 0x{size:x} bytes; its stream writes {tail} bytes more, into commons_unused_4_words_b"
    return EncounterSource(
        "field", map_id, None, stream[:FIELD_SIZE], (stream[SET:FIELD_SIZE],), note
    )


def header_end(data: bytes) -> int:
    """Where an area file's header ends: at the first of the sections
    worldmap_unpack_area_data resolves from header words 1-9 (the spot block, +8 ... +0x24)."""
    return min(struct.unpack_from("<9I", data, 4))


def terrain_tables(data: bytes, rows: int) -> tuple[int | None, ...]:
    """worldmap_unpack_area_data: the offsets in header words 11-26 (AreaHeader +0x2c). A
    word past the header's end is section data, not an offset, and a table must
    hold the set and weight rows worldmap_encounter_roll reads between the header's end
    and the file's; None otherwise."""
    read = SET + rows * WEIGHTS
    if len(data) < AREA_TABLES + 4 * KINDS:
        return (None,) * KINDS
    end = header_end(data)
    return tuple(
        offset
        if AREA_TABLES + 4 * (kind + 1) <= end and end <= offset and offset + read <= len(data)
        else None
        for kind, offset in enumerate(struct.unpack_from(f"<{KINDS}I", data, AREA_TABLES))
    )


def terrain_sources(area_file: int, data: bytes, rows: int) -> list[EncounterSource]:
    sources = []
    offsets = terrain_tables(data, rows)
    valid = sorted(o for o in offsets if o is not None)
    for kind, offset in enumerate(offsets):
        if offset is None:
            continue
        read = SET + rows * WEIGHTS
        after = min([o for o in valid if o > offset] + [len(data)])
        weights = tuple(
            data[offset + SET + WEIGHTS * r : offset + SET + WEIGHTS * (r + 1)] for r in range(rows)
        )
        note = f"+0x{offset:x}; {after - offset - read} bytes before the next table no reader reads"
        sources.append(
            EncounterSource(
                "worldmap", area_file, kind, data[offset : offset + read], weights, note
            )
        )
    return sources


@dataclass
class Census:
    disc: int
    sources: list[EncounterSource] = field(default_factory=list)
    maps: int = 0
    empty_maps: list[int] = field(default_factory=list)
    placeholders: int = 0
    component_sizes: Counter = field(default_factory=Counter)
    stream_tails: Counter = field(default_factory=Counter)
    area_files: list[int] = field(default_factory=list)
    tableless: list[int] = field(default_factory=list)  # area files without terrain tables
    table_gaps: Counter = field(default_factory=Counter)
    debug_files: list[tuple[int, int]] = field(default_factory=list)  # (file, size)
    debug_end: str = ""
    requests: dict[int, list[Request]] = field(default_factory=dict)
    armings: dict[int, list[Arming]] = field(default_factory=dict)  # map -> its f7s
    controls: set[int] = field(default_factory=set)  # maps whose scripts reach 0c or a7
    chains: dict[int, list[int | None]] = field(default_factory=dict)  # script set -> formations
    script_sets: int = 0
    stages: int = 0
    blocks: dict[tuple[int, int], battle_ai.Block] = field(default_factory=dict)
    block_images: dict[tuple[int, int], bytes] = field(default_factory=dict)
    battles: int = 0
    undecodable: list[str] = field(default_factory=list)

    def of(self, kind: str) -> list[EncounterSource]:
        return [s for s in self.sources if s.kind == kind]


def read_field(census: Census, files: DiscFiles) -> None:
    for map_id, path in events.map_files(files.extract, files.track):
        data = path.read_bytes()
        try:
            component = field_component(data)
        except PackedError as error:
            census.undecodable.append(f"field map {map_id}: {error}")
            continue
        if component is None:
            census.placeholders += 1
            continue
        census.maps += 1
        size, stream = component
        census.component_sizes[size] += 1
        try:
            source = field_source(map_id, size, stream)
        except FormationError as error:
            census.undecodable.append(f"field map {map_id}: {error}")
            continue
        if source is None:
            census.empty_maps.append(map_id)
        else:
            census.sources.append(source)
            census.stream_tails[len(stream) - FIELD_SIZE] += 1
        scripts = field_scripts(data)
        census.requests[map_id], census.armings[map_id] = scripts.requests, scripts.armings
        if scripts.control:
            census.controls.add(map_id)


def field_scripts(data: bytes) -> Scripts:
    """Every 71, fe 84 and f7 a script can reach, and whether it reaches player
    control (events.walk)."""
    try:
        package = events.map_events(data)
    except (FieldError, PackedError):
        return Scripts([], [], False)
    if package is None:
        return Scripts([], [], False)
    starts, _ = events.script_entries(package)
    result = events.walk(package.bytecode, [pc for _, _, pc in starts])
    requests, armings, control = [], [], False
    for pc, ins in sorted(result.instructions.items()):
        if ins.key in REQUESTS:
            value = immediate(ins, 0)
            if value is None:
                requests.append(Request(pc, ins.key, None, ins.operands[0]))
            else:
                requests.append(Request(pc, ins.key, value & 0xFF, None))
        elif ins.key == ARMING:
            armings.append(Arming(pc, immediate(ins, 0), immediate(ins, 1)))
        control |= ins.key in CONTROL
    return Scripts(requests, armings, control)


def immediate(ins: events.Instruction, n: int) -> int | None:
    """Operand n's 15-bit immediate, None when it names a variable (field_event_read_imm_or_var)."""
    return ins.operands[n] & 0x7FFF if ins.immediate[n] else None


def arming_f7(census: Census, map_id: int) -> bool:
    """Whether the map's scripts reach an f7 that arms the field's draw."""
    return any(arming.arms for arming in census.armings.get(map_id, []))


def armed(census: Census, map_id: int) -> bool:
    """Whether the map's scripts reach an f7 that arms the field's draw and the
    player control that runs it."""
    return map_id in census.controls and arming_f7(census, map_id)


def read_worldmap(census: Census, files: DiscFiles, root: Path) -> None:
    rows = len(brackets(root)) - 1
    for area in sorted(set(dispatch_tables.area_files(root))):
        number = area + 1  # worldmap_select_area_files: worldmap_area_data_file = file + 1
        try:
            data = decode_block(files.sectors(files.slot(AREA_DIRECTORY, number))).data
        except PackedError as error:
            census.undecodable.append(f"area file {number}: {error}")
            continue
        census.area_files.append(number)
        sources = terrain_sources(number, data, rows)
        if not sources:
            census.tableless.append(number)
        census.sources += sources
        offsets = sorted(s for s in terrain_tables(data, rows) if s is not None)
        read = SET + rows * WEIGHTS
        for before, after in zip(offsets, offsets[1:] + [len(data)], strict=False):
            census.table_gaps[after - before - read] += 1


def read_debug(census: Census, files: DiscFiles) -> None:
    """The selector's files from 4 up to the first entry that is not a file
    holding a whole set."""
    number = DEBUG_FIRST
    while True:
        size = files.size(files.slot(DEBUG_DIRECTORY, number))
        if size is None or size < SET:
            census.debug_end = f"file {number}: " + (
                "none"
                if size is None
                else f"a sub-directory of {-size}"
                if size < 0
                else f"{size} bytes"
            )
            return
        data = files.data(files.slot(DEBUG_DIRECTORY, number))
        census.debug_files.append((number, size))
        census.sources.append(
            EncounterSource("debug", number, None, data[:SET], (), f"{size} bytes")
        )
        number += 1


def read_cross(census: Census, files: DiscFiles) -> None:
    for battle, path in battle_ai.enemy_files(files.extract, files.track):
        census.battles = max(census.battles, battle + 1)
        data = path.read_bytes()
        try:
            for block in battle_ai.enemy_blocks(data):
                census.blocks[(battle, block.enemy)] = block
                census.block_images[(battle, block.enemy)] = data[block.offset : block.end]
        except ValueError as error:
            census.undecodable.append(f"battle {battle} enemy file: {error}")
    archive = files.data(files.slot(SCRIPT_DIRECTORY, SCRIPT_ARCHIVE))
    for n, raw in battle_event_vm.archive_scripts(archive):
        census.script_sets += 1
        script = battle_event_vm.parse_script(raw)
        listing = battle_event_vm.disassemble(script.code, battle_event_vm.entry_points(script))
        targets = []
        for insn in sorted(listing.instructions.values(), key=lambda i: i.offset):
            if insn.opcode == CHAIN:
                text = dict(insn.operands)["formation"]
                targets.append(int(text[1:], 16) & 0xFF if text.startswith("#") else None)
        census.chains[n] = targets
    size = files.size(files.slot(STAGE_DIRECTORY, STAGE_COUNT))
    census.stages = -size // 2 if size is not None and size < 0 else 0


def read_disc(root: Path, number: int, repository: Path = ROOT) -> Census:
    files = DiscFiles(root, number)
    census = Census(number)
    read_field(census, files)
    read_worldmap(census, files, repository)
    read_debug(census, files)
    read_cross(census, files)
    return census


def sweep(root: Path, repository: Path = ROOT) -> list[Census]:
    return [read_disc(root, number, repository) for number in (1, 2)]


# ---------------------------------------------------------------------------
# Cross-checks
# ---------------------------------------------------------------------------


def reach(census: Census) -> dict[tuple[str, int, int | None, int], set[str]]:
    """How each formation of a field or world map set can start a battle:
    drawn (a nonzero weight; on a field map also scripts that arm the draw),
    named (a field script's immediate request on its map), chained (opcode 24
    of the script set of a reachable event formation of the same set), or none
    of these ('set' only). The draws' other gates (field_encounter_count_down, worldmap_run_frame_loop)
    are not traced."""
    result = {}
    for source in census.sources:
        if source.kind == "debug":
            continue
        field_map = source.kind == "field"
        found = census.requests.get(source.item, []) if field_map else []
        named = {r.formation for r in found if r.formation is not None}
        formations = source.formations
        drawable = not field_map or armed(census, source.item)
        drawn = {n for n in range(FORMATIONS) if drawable and any(source.weight(n))}
        reached = drawn | named
        chained = set()
        pending = list(reached)
        while pending:
            n = pending.pop()
            if formations[n].flags & EVENT:
                for target in census.chains.get(formations[n].script_set, []):
                    if (
                        target is not None
                        and target < FORMATIONS
                        and target not in reached | chained
                    ):
                        chained.add(target)
                        pending.append(target)
        for n in range(FORMATIONS):
            ways = set()
            if n in drawn:
                ways.add("drawn")
            if n in named:
                ways.add("named")
            if n in chained:
                ways.add("chained")
            result[(source.kind, source.item, source.sub, n)] = ways or {"set"}
    return result


@dataclass
class CrossCheck:
    pairs: dict[tuple[int, int], set[str]] = field(default_factory=dict)
    debug_pairs: set[tuple[int, int]] = field(default_factory=set)
    outside: list[str] = field(default_factory=list)  # pairs no enemy file holds
    templates: list[tuple[int, int]] = field(default_factory=list)  # placed, no script table
    unplaced: list[tuple[int, int]] = field(default_factory=list)  # script tables never placed
    unplaced_images: Counter = field(default_factory=Counter)
    unique_unplaced: list[tuple[int, int]] = field(default_factory=list)
    battles_unnamed: list[int] = field(default_factory=list)
    script_sets: Counter = field(default_factory=Counter)
    debug_script_sets: set[int] = field(default_factory=set)
    debug_battles: set[int] = field(default_factory=set)
    bad_script_sets: list[str] = field(default_factory=list)
    stages: Counter = field(default_factory=Counter)
    bad_stages: list[str] = field(default_factory=list)


def cross_check(census: Census) -> CrossCheck:
    check = CrossCheck()
    ways = reach(census)
    for source in census.sources:
        for n, formation in enumerate(source.formations):
            where = f"{source.label} formation {n}"
            if source.kind == "debug":
                check.debug_pairs.update(formation.pairs)
                check.debug_battles.add(formation.battle)
                if formation.flags & EVENT:
                    check.debug_script_sets.add(formation.script_set)
            else:
                for pair in formation.pairs:
                    check.pairs.setdefault(pair, set()).update(
                        ways[(source.kind, source.item, source.sub, n)]
                    )
                check.stages[formation.stage] += 1
                if formation.flags & EVENT:
                    check.script_sets[formation.script_set] += 1
            if formation.flags & EVENT and formation.script_set >= census.script_sets:
                check.bad_script_sets.append(f"{where}: set {formation.script_set}")
            if formation.stage >= census.stages:
                check.bad_stages.append(f"{where}: stage {formation.stage}")
            for battle, enemy in formation.pairs:
                if (battle, enemy) not in census.blocks:
                    check.outside.append(f"{where}: battle {battle} id {enemy}")
    placed = set(check.pairs)
    for pair in sorted(placed | check.debug_pairs):
        block = census.blocks.get(pair)
        if block is not None and block.malformed:
            check.templates.append(pair)
    placed_images = {census.block_images[p] for p in placed if p in census.block_images}
    for pair, block in sorted(census.blocks.items()):
        if block.malformed or pair in placed:
            continue
        check.unplaced.append(pair)
        image = census.block_images[pair]
        check.unplaced_images[image] += 1
        if image not in placed_images:
            check.unique_unplaced.append(pair)
    check.battles_unnamed = sorted(set(range(census.battles)) - {b for b, _ in placed})
    return check


# ---------------------------------------------------------------------------
# Reports
# ---------------------------------------------------------------------------


def _listed(items, limit: int = 12) -> list[str]:
    items = list(items)
    lines = [f"    {item}" for item in items[:limit]]
    if len(items) > limit:
        lines.append(f"    ... {len(items) - limit} more")
    return lines


def label(key: tuple[str, int, int | None, int]) -> str:
    """The source label of a reach() key."""
    kind, item, sub, _ = key
    return EncounterSource(kind, item, sub, b"", ()).label


def _counts(counter: Counter, form="{:x}") -> str:
    return " ".join(f"{form.format(key)}:{counter[key]}" for key in sorted(counter))


def report(results: list[Census]) -> list[str]:
    out = []
    for census in results:
        out += disc_report(census)
    first, second = ({(s.kind, s.item, s.sub): s.data for s in c.sources} for c in results[:2])
    shared = first.keys() & second.keys()
    same = sum(1 for key in shared if first[key] == second[key])
    out.append(f"sets on both discs: {len(shared)}, identical: {same}")
    checks = [cross_check(c).pairs for c in results]
    placed = [set(pairs) for pairs in checks]
    ways = [{(p, w) for p, found in pairs.items() for w in found - {"set"}} for pairs in checks]
    out.append(
        f"pairs placed on either disc: {len(set().union(*placed))};"
        f" placed on disc 2 only: {len(placed[1] - placed[0])};"
        f" drawn, named or chained on disc 2 only: {len(ways[1] - ways[0])}"
    )
    return out


def disc_report(census: Census) -> list[str]:
    field_sets, terrain, debug = census.of("field"), census.of("worldmap"), census.of("debug")
    formations = [f for s in field_sets + terrain for f in s.formations]
    field_data = {s.data[:SET] for s in field_sets}
    terrain_data = {s.data[:SET] for s in terrain}
    ways_of = reach(census)
    starts = " ".join(
        f"{way} {n}" for way, n in sorted(Counter(w for f in ways_of.values() for w in f).items())
    )
    chained = sorted(key for key, found in ways_of.items() if "chained" in found)
    flags = Counter(bit for f in formations for bit in range(8) if f.flags & 1 << bit)
    placed = [e for f in formations for e in f.enemies]
    requests = [r for found in census.requests.values() for r in found]
    immediate = Counter(r.formation for r in requests if r.formation is not None)
    variable = [
        f"map {m} +0x{r.pc:x} {r.event} v{r.variable:04x}"
        for m, found in census.requests.items()
        for r in found
        if r.formation is None
    ]
    request_maps = sum(1 for found in census.requests.values() if found)
    setless = sorted({m for m, found in census.requests.items() if found} & set(census.empty_maps))
    sizes = ", ".join(
        f"{n} of {size} bytes"
        for size, n in sorted(Counter(s for _, s in census.debug_files).items())
    )
    debug_last = DEBUG_FIRST + len(debug) - 1
    tables = len(census.area_files) - len(census.tableless)
    tableless = " ".join(map(str, census.tableless)) or "none"
    chains = ", ".join(f"set {n} -> {t}" for n, t in census.chains.items() if t) or "none"
    chain_targets = ", ".join(f"{label(key)} formation {key[3]}" for key in chained) or "none"
    party_bits = sum(g >> 7 for f in formations for g in f.party_groups)
    armings = Counter(a.text for found in census.armings.values() for a in found)
    weighted = [s.item for s in field_sets if any(map(any, s.rows))]
    arming = [m for m in weighted if armed(census, m)]
    uncontrolled = [m for m in weighted if arming_f7(census, m) and m not in census.controls]
    clearing = [m for m in weighted if census.armings.get(m) and not arming_f7(census, m)]
    empty_armed = [m for m in census.empty_maps if armed(census, m)] or "none"
    unarmed = sum(
        1
        for s in field_sets
        if not armed(census, s.item)
        for n in range(FORMATIONS)
        if any(s.weight(n))
    )
    out = [
        f"disc {census.disc}:",
        f"  field maps: {census.maps} bundles ({census.placeholders} placeholder files),"
        f" {len(field_sets)} with an encounter set,"
        f" {len(census.empty_maps)} with an empty component 6",
        f"    component 6 sizes: {_counts(census.component_sizes)};"
        f" stream bytes past 0x{FIELD_SIZE:x} (into commons_unused_4_words_b):"
        f" {_counts(census.stream_tails, '{}')}",
        f"    f7 reached (field_event_draw_random_picks, period/count): {sum(armings.values())},"
        f" {_counts(armings, '{}')}",
        f"    maps with a weight: {len(weighted)}; armed (an f7 with a nonzero period and"
        f" count, and player control 0c or a7): {len(arming)}; such an f7 but no player"
        f" control: {uncontrolled or 'none'}; only f7s that do not arm:"
        f" {clearing or 'none'}; no f7:"
        f" {len(weighted) - len(arming) - len(uncontrolled) - len(clearing)}; with an empty"
        f" component 6 and armed: {empty_armed}; weighted formations never armed: {unarmed}",
        f"  world map area files: {len(census.area_files)}, {tables} with {KINDS} terrain"
        f" tables ({len(terrain)} tables); without: {tableless}",
        f"    weight rows: {len(brackets()) - 1} (worldmap_encounter_level_brackets {list(brackets())});"
        f" bytes after each table's weights no reader reads: {_counts(census.table_gaps, '{}')}",
        f"  debug selector files {DEBUG_FIRST}-{debug_last}: {len(debug)} ({sizes});"
        f" then {census.debug_end}",
        f"    their sets equal to a field map's: {sum(s.data in field_data for s in debug)},"
        f" to a terrain table's: {sum(s.data in terrain_data for s in debug)}",
        f"  formations in field and world map sets: {len(formations)},"
        f" {len({f.raw for f in formations})} distinct; by how they start: {starts}",
        "    flag bits: " + " ".join(f"{1 << b:02x}:{flags[b]}" for b in range(8)),
        f"    byte 7 values: {_counts(Counter(f.unk7 for f in formations))}",
        f"    party group bytes with bit 7: {party_bits}",
        f"    placed enemies: {len(placed)}; in a gear {sum(e.gear for e in placed)};"
        f" enemyFlags bytes {_counts(Counter(e.flags for e in placed))};"
        f" group bytes with bit 7 {sum(e.group >> 7 for e in placed)}",
        f"  field script battle requests (71, fe 84): {len(requests)} in {request_maps} maps;"
        f" immediate formations {_counts(immediate, '{}')}",
        f"    from variables: {len(variable)};"
        f" in maps with an empty component 6: {setless or 'none'}",
        *_listed(variable, 6),
        f"  battle event chains (opcode 24, {census.script_sets} script sets): {chains}",
        f"    formations they chain to: {chain_targets}",
    ]
    check = cross_check(census)
    ways = Counter(w for found in check.pairs.values() for w in found)
    only_set = sorted(p for p, found in check.pairs.items() if found == {"set"})
    debug_only = len(check.debug_pairs - set(check.pairs))
    common = check.unplaced_images.most_common(1)
    common_text = f"({len(common[0][0])} bytes) {common[0][1]} times" if common else "none"
    unnamed_sets = sorted(set(range(census.script_sets)) - set(check.script_sets))
    debug_sets = sorted(set(unnamed_sets) & check.debug_script_sets)
    debug_battles = sorted(set(check.battles_unnamed) & check.debug_battles) or "none"
    highest = max(check.stages, default=0)
    out += [
        f"  cross-check, {census.battles} enemy files (tools.analysis.battle_ai):",
        f"    battles no field or world map formation names: {check.battles_unnamed or 'none'};"
        f" of these the debug sets name: {debug_battles}",
        f"    (battle, enemy id) pairs placed: {len(check.pairs)} (drawn {ways['drawn']},"
        f" named {ways['named']}, chained {ways['chained']};"
        f" only by formations nothing starts: {len(only_set)}); by debug sets only: {debug_only}",
        f"    pairs no enemy file holds: {len(check.outside)}",
        *_listed(check.outside, 6),
        f"    placed pairs on blocks without a script table: {len(check.templates)}",
        *_listed(check.templates, 6),
        f"    script tables no field or world map formation places: {len(check.unplaced)},"
        f" {len(check.unplaced_images)} distinct block images, the most common {common_text};"
        f" not identical to a placed block: {len(check.unique_unplaced)}",
        *_listed((f"battle {b} id {e}" for b, e in check.unique_unplaced), 12),
        f"    event script sets named (flag 0x20): {len(check.script_sets)} of"
        f" {census.script_sets} (none names {unnamed_sets}, debug sets name {debug_sets});"
        f" outside the archive: {len(check.bad_script_sets)}",
        *_listed(check.bad_script_sets, 6),
        f"    stages used: {len(check.stages)}, highest {highest};"
        f" at or past the {census.stages} stage pairs of (12, 3): {len(check.bad_stages)}",
        *_listed(check.bad_stages, 6),
        f"  undecodable: {len(census.undecodable)}",
        *_listed(census.undecodable, 6),
    ]
    return out


def failures(results: list[Census]) -> bool:
    for census in results:
        check = cross_check(census)
        if census.undecodable or check.outside or check.bad_script_sets or check.bad_stages:
            return True
    return False


def bracket_text(root: Path = ROOT) -> str:
    """The scene ids of each world map weight row (worldmap_encounter_roll)."""
    starts = brackets(root)
    return " ".join(f"{low}-{high - 1}" for low, high in zip(starts, starts[1:], strict=False))


def listing(census: Census, kind: str, item: int | None) -> list[str]:
    lines = []
    if kind == "field" and item is not None and item in census.empty_maps:
        lines.append(f"; field map {item}: component 6 is empty (the set loaded before stays)")
    if kind == "worldmap":
        lines.append(f"; weight rows by the scene id (variable 0, worldmap_encounter_level_brackets): {bracket_text()}")
        if item is not None and item in census.tableless:
            lines.append(
                f"; area file {item}: no terrain tables (words 11-26 name none past its header)"
            )
    for source in census.of(kind):
        if item is not None and source.item != item:
            continue
        lines.append(f"; disc {census.disc} {source.label}: {source.note}")
        if kind == "field":
            requests = census.requests.get(source.item, [])
            named = " ".join(
                f"+0x{r.pc:x} {r.event} "
                + (str(r.formation) if r.formation is not None else f"v{r.variable:04x}")
                for r in requests
            )
            lines.append(f";   battle requests: {named or 'none'}")
            draws = " ".join(f"+0x{a.pc:x} {a.text}" for a in census.armings.get(source.item, []))
            arms = "armed" if armed(census, source.item) else "never armed"
            control = "yes" if source.item in census.controls else "no"
            lines.append(
                f";   draw: {arms}; f7 period/count: {draws or 'none'}; player control: {control}"
            )
        width = 4 * len(source.rows)
        weights = f"{'weights':<{width}} " if width else ""
        lines.append(f";  #  {weights}formation (enemies id[g]/group[h][.][^])")
        for n, formation in enumerate(source.formations):
            weights = "".join(f"{w:3d} " for w in source.weight(n))
            lines.append(f"  {n:2d}  {weights}{formation_text(formation)}")
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="decode every set, both discs")
    parser.add_argument(
        "--list", choices=("field", "worldmap", "debug"), help="print the formations"
    )
    parser.add_argument("--item", type=int, help="one map, area file or debug file number")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    parser.add_argument("--root", type=Path, default=ROOT / ".local", help="extract/ and discs/")
    args = parser.parse_args(argv)
    if args.sweep:
        results = sweep(args.root)
        print("\n".join(report(results)))
        return 1 if failures(results) else 0
    if args.list is None:
        parser.error("choose --sweep or --list")
    census = read_disc(args.root, args.disc)
    print("\n".join(listing(census, args.list, args.item)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
