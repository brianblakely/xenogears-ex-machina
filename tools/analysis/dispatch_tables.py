"""Censuses of the data-indexed dispatch tables: battle formulas, model primitives.

Neither table has a program counter: one selector field of a fixed-layout record
picks the entry, and no code checks it against the table's length. The tables
(their lengths, handlers and row values) are parsed from the recovered C at run
time, and `--sweep` reads every record the game's loaders hand to them on both
discs and prints aggregate counts only.

Battle formulas (decomp/src/battle/battle_8008CCCC.c). For each target in the
mask, func_800941A4 calls D_800C348C[D_800C3DFC->formula](). An attacker whose
record has +0x15a bit 0x80 (fighting in a gear), or a descriptor with flagsA bit
0x10, goes to func_8009C198 instead: it selects a party member's gear descriptor
(an enemy keeps its own) and calls D_800C34DC[formula](). The 0x28-byte
descriptors come from the battle setup archive, directory (12, 0) file 3
(resident func_8001BBAC), an offset table of packed blocks that ovl2615
func_801E5384 unpacks: archive[4] holds the enemy commands, archive[5 + character]
a party member's and archive[0x11 + gear] its gear's (copies of 0x1f40, 0x5f0
and 0x690 bytes). An enemy's command is the arg1 byte of its AI's type-1
action-list entries (func_80078998 -> func_80085CCC), so the census also reads
which commands the enemy data files' AI scripts select (tools.analysis.battle_ai)
and whether each enemy fights in a gear (its record's +0x15a, copied by ovl2615
func_801E4870).

Model primitives (decomp/src/resident/main_8002C3E8.c). func_8002C8CC builds and
func_8002C700 draws a model's primitive groups, a header {u8 type, u8, s16 count}
and count records each, through D_8004FE50[type]: a prepare routine per record,
the record stride, the auxiliary bytes per record and the packet size, and six
draw routines by sort mode. Prepare routines that call func_8002CD64 first take
texture page (c4) and CLUT (c8) override words from the auxiliary data. The
models (0x38-byte headers; groups of them relocated by func_8002C3E8, single
ones by func_8002C59C) come from these loaders:

* field map bundles, directory (4, 0) file 0xb8 + 2 * map: every model group of
  the geometry component (component 2, field func_80070CC8);
* ovl2143 actors, the model file of each pair after ovl2143 in (4, 0), files
  0x6bb + 2k (field func_80077884, the gear shop's func_801CF9BC);
* battle objects (func_800A8BF0: the group between the model file's entries 2
  and 3): stage files (12, 3) 6 + 2s (resident func_800379D8), the model entries
  of enemy set files (12, 1) 2n + 3 (ovl2615 func_801E6314), object sets
  (0x28, 0) 2s + 1 (func_800A96B4), gears (0x28, 1) base + 1 and their part
  files base + 2 + v by D_800C3508 (func_800A9540, func_800A979C);
* arena model files (0x30, 1) id + 2 (menu func_8008509C; relocated against the
  base word at +0x1c, func_8008AF6C, and bound by func_8008B38C) and the menu
  overlay's own D_80091FB0 (func_800852C4);
* world map area files (0x24, 0) area + 1 for the area sets of D_8009B584
  (func_80071B9C, func_80073530: the group at header +8, func_80084580);
* sprite commands f5 (a model), f6 and f7 (a model group) in the sprite blocks
  tools.analysis.sprite_vm decodes (resident func_8001FBE4).

The census walks every model of each group a loader relocates, also those a
hierarchy or placement list never builds.
"""

from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import re
import struct
from collections import Counter
from collections.abc import Iterator, Sequence
from dataclasses import dataclass, field
from functools import cache
from pathlib import Path

from tools.analysis import battle_ai, sprite_vm
from tools.analysis.disc_index import Disc, discs
from tools.analysis.overlay_scripts import BASE, disc_image
from tools.analysis.packed import PackedError, decode_block

ROOT = Path(__file__).resolve().parents[2]
FORMULA_UNIT = "battle/battle_8008CCCC.c"  # D_800C348C, D_800C34DC
GEAR_FILE_UNIT = "battle/battle_8009E53C.c"  # D_800C3508
MODEL_UNIT = "resident/main_8002C3E8.c"  # D_8004FE50 and its prepare routines
AREA_UNIT = "worldmap/worldmap_80094A5C.c"  # D_8009B584
FORMULA_TABLES = (("D_800C348C", "func_800941A4"), ("D_800C34DC", "func_8009C198"))


class CensusError(ValueError):
    """Bytes the recovered loaders cannot hand to a table."""


# ---------------------------------------------------------------------------
# Tables read from the C
# ---------------------------------------------------------------------------


def _strip_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def initializer(text: str, name: str) -> tuple[int | None, str]:
    """The declared length (None for []) and the initializer text of a C array,
    comments removed."""
    text = _strip_comments(text)
    match = re.search(rf"\b{name}\[(\w*)\][^;={{]*=\s*\{{", text)
    if match is None:
        raise CensusError(f"{name} has no initializer")
    depth = 1
    for position in range(match.end(), len(text)):
        depth += {"{": 1, "}": -1}.get(text[position], 0)
        if depth == 0:
            body = text[match.end() : position]
            return (int(match.group(1), 0) if match.group(1) else None), body
    raise CensusError(f"{name}'s initializer is not closed")


def function_body(text: str, name: str) -> str:
    """The body of a C function definition (up to its closing brace in column 0),
    comments removed."""
    text = _strip_comments(text)
    match = re.search(rf"\n[^\n;]*\b{name}\([^;{{]*\)\s*\{{(.*?)\n\}}", text, re.S)
    if match is None:
        raise CensusError(f"{name} has no definition")
    return match.group(1)


def unit(path: str, root: Path = ROOT) -> str:
    return (root / "decomp/src" / path).read_text()


@dataclass(frozen=True)
class FormulaTable:
    name: str
    caller: str  # the function that indexes it
    handlers: tuple[str, ...]  # by formula id


@cache
def formula_tables(root: Path = ROOT) -> tuple[FormulaTable, ...]:
    text = unit(FORMULA_UNIT, root)
    tables = []
    for name, caller in FORMULA_TABLES:
        length, body = initializer(text, name)
        handlers = tuple(entry.strip() for entry in body.split(",") if entry.strip())
        if length not in (None, len(handlers)):
            raise CensusError(f"{name}[{length}] lists {len(handlers)} handlers")
        if f"{name}[D_800C3DFC->formula]()" not in function_body(text, caller):
            raise CensusError(f"{caller} does not index {name} by the formula")
        tables.append(FormulaTable(name, caller, handlers))
    return tuple(tables)


@dataclass(frozen=True)
class PrimitiveType:
    draw: tuple[str, ...]  # by sort mode 0-5
    prepare: str
    stride: int  # record bytes
    aux_stride: int  # auxiliary bytes per record
    packet_size: int
    overrides: bool  # prepare calls func_8002CD64 before the record


@dataclass(frozen=True)
class PrimitiveTable:
    types: tuple[PrimitiveType, ...]
    override_codes: frozenset[int]  # command[3] values func_8002CD64 consumes


@cache
def primitive_table(root: Path = ROOT) -> PrimitiveTable:
    text = unit(MODEL_UNIT, root)
    length, body = initializer(text, "D_8004FE50")
    rows = re.findall(r"\{\{([^}]*)\},\s*(\w+),\s*(\w+),\s*(\w+),\s*(\w+)\}", body)
    if length != len(rows):
        raise CensusError(f"D_8004FE50[{length}] has {len(rows)} rows")
    types = []
    for draw, prepare, stride, aux_stride, packet_size in rows:
        routines = tuple(name.strip() for name in draw.split(","))
        if len(routines) != 6:
            raise CensusError(f"a D_8004FE50 row lists {len(routines)} draw routines")
        overrides = "func_8002CD64(" in function_body(text, prepare)
        values = (int(stride, 0), int(aux_stride, 0), int(packet_size, 0))
        types.append(PrimitiveType(routines, prepare, *values, overrides))
    cases = re.findall(r"case (0x[0-9A-Fa-f]+):", function_body(text, "func_8002CD64"))
    return PrimitiveTable(tuple(types), frozenset(int(code, 16) for code in cases))


@cache
def gear_files(root: Path = ROOT) -> tuple[tuple[int, int], ...]:
    """D_800C3508: per gear its file base in (0x28, 1) and part file count."""
    _, body = initializer(unit(GEAR_FILE_UNIT, root), "D_800C3508")
    values = [int(v, 0) for v in body.replace("\n", " ").split(",") if v.strip()]
    return tuple(zip(values[::2], values[1::2], strict=True))


@cache
def area_files(root: Path = ROOT) -> tuple[int, ...]:
    """D_8009B584: each area set's first file (a record) in (0x24, 0)."""
    _, body = initializer(unit(AREA_UNIT, root), "D_8009B584")
    return tuple(int(row.split(",")[0], 0) for row in re.findall(r"\{([^}]*)\}", body))


# ---------------------------------------------------------------------------
# Battle formulas
# ---------------------------------------------------------------------------

DESCRIPTOR = 0x28
FORMULA = 0x16  # CommandDescriptor.formula
FLAGS_A = 0x0A  # CommandDescriptor.flagsA
GEAR_DESCRIPTOR = 0x10  # flagsA bit func_800941A4 hands to func_8009C198
SETUP_ARCHIVE = (12, 0, 3)  # func_8001BBAC: 80028470(12, 0), file 3 into D_800595A8
# func_801E5384: archive entry, bytes copied, and the first entry after the run
# (archive[0x10] and archive[0x24] are loaded for other uses).
ENEMY_COMMANDS = (4, 0x1F40, 5)
PARTY_COMMANDS = (5, 0x5F0, 0x10)  # archive[5 + character]
GEAR_COMMANDS = (0x11, 0x690, 0x24)  # archive[0x11 + gear]
ENEMY_RECORDS = 0x32  # 801e4870: records from +0x32, 0x170 bytes per enemy id
RECORD = 0x170
GEAR_FLAG = 0x15A  # Combatant.flags15A, bit 0x80
ACT = 1  # action-list entry type func_800793F0 hands to func_80078998


def archive_entries(data: bytes) -> list[bytes]:
    """Entries 1..count of the setup archive unpacked (func_8003342C turns the
    offsets into archive[1..count]); index 0 stands for the count word."""
    count = struct.unpack_from("<I", data, 0)[0]
    if not 0 < count < 0x100 or 4 + 4 * count > len(data):
        raise CensusError("no setup archive")
    entries = [b""]
    for offset in struct.unpack_from(f"<{count}I", data, 4):
        try:
            entries.append(decode_block(data[offset:]).data)
        except PackedError:
            entries.append(b"")  # read only for other uses (the last entry needs sector padding)
    return entries


def descriptors(block: bytes, copied: int) -> list[tuple[int, int]]:
    """(formula, flagsA) of the whole descriptors the copy takes from a block."""
    whole = min(len(block), copied) // DESCRIPTOR
    return [
        (
            block[DESCRIPTOR * i + FORMULA],
            struct.unpack_from("<H", block, DESCRIPTOR * i + FLAGS_A)[0],
        )
        for i in range(whole)
    ]


@dataclass
class FormulaFamily:
    """One descriptor family's formula ids against the table that dispatches them."""

    sets: int = 0
    records: int = 0
    short: int = 0  # descriptors the copy takes from past the unpacked block
    formulas: Counter = field(default_factory=Counter)
    gear_descriptors: int = 0  # flagsA bit 0x10
    outside: list[str] = field(default_factory=list)


@dataclass
class EnemyCommands:
    """Commands the enemy AI's act entries select, by attacker kind."""

    files: int = 0
    acts: int = 0
    unwritten: int = 0  # act entries closed with no arg1 write before them
    dynamic: int = 0  # act entries whose arg1 comes from a variable
    on_foot: Counter = field(default_factory=Counter)
    in_gear: Counter = field(default_factory=Counter)


@dataclass
class FormulaCensus:
    slot: int = 0
    archive: bytes = b""
    party: FormulaFamily = field(default_factory=FormulaFamily)
    gear: FormulaFamily = field(default_factory=FormulaFamily)
    enemy: FormulaFamily = field(default_factory=FormulaFamily)
    enemy_descriptors: list[tuple[int, int]] = field(default_factory=list)  # by command
    commands: EnemyCommands = field(default_factory=EnemyCommands)
    selected_outside: list[str] = field(default_factory=list)


def family_census(
    family: FormulaFamily,
    entries: list[bytes],
    first: int,
    copied: int,
    end: int,
    limit: int,
    label: str,
    redirect: bool = False,
) -> None:
    """Count one family's formula ids against `limit`; with `redirect` (the
    party), a descriptor with flagsA 0x10 is not dispatched: 8009c198 takes
    the gear descriptor at its index instead."""
    for index in range(first, end):
        block = entries[index]
        family.sets += 1
        records = descriptors(block, copied)
        family.short += copied // DESCRIPTOR - len(records)
        for number, (formula, flags) in enumerate(records):
            family.records += 1
            if flags & GEAR_DESCRIPTOR:
                family.gear_descriptors += 1
                if redirect:
                    continue
            family.formulas[formula] += 1
            if formula >= limit:
                family.outside.append(f"{label} {index - first} #{number}: formula {formula}")


def enemy_table(descriptor: tuple[int, int], in_gear: bool) -> int:
    """0 for D_800C348C, 1 for D_800C34DC: an enemy in a gear, or a descriptor
    with flagsA 0x10, goes through func_8009C198 with its own descriptor."""
    return int(in_gear or bool(descriptor[1] & GEAR_DESCRIPTOR))


VARIABLE = -1  # an arg1 taken from an AI variable


def act_commands(script: battle_ai.Script, data: bytes) -> Iterator[int | None]:
    """The arg1 byte of each act entry the script closes (None when nothing
    wrote it, VARIABLE when a variable did), following the reachable words in
    address order: a rule's writes are straight-line, and the census does not
    model a skipped rule's writes reaching a later rule's entry."""
    arg1: int | None = None
    for pc in sorted(script.reachable):
        op, offset, low, high = data[pc : pc + 4]
        if op == 0x01 and offset == 0:  # 8007a828: the type byte closes the entry
            if low == ACT:
                yield arg1
            arg1 = None
        elif op == 0x01 and offset == 1:
            arg1 = low
        elif op == 0x3D and offset in (0, 1):  # 8007bc40: bytes offset, offset + 1
            arg1 = high if offset == 0 else low
        elif (op == 0x02 and offset == 1) or (op == 0x52 and offset in (0, 1)):
            arg1 = VARIABLE  # 8007a874 (a byte variable), 8007d148 (a halfword)


def enemy_commands(census: FormulaCensus, root: Path, disc: int) -> None:
    extract = root / ".local/extract" / f"disc{disc}"
    track = root / ".local/discs" / f"disc{disc}.bin"
    commands = census.commands
    for _, path in battle_ai.enemy_files(extract, track):
        data = path.read_bytes()
        commands.files += 1
        for block in battle_ai.enemy_blocks(data):
            if block.malformed:
                continue
            record = ENEMY_RECORDS + RECORD * block.enemy
            in_gear = data[record + GEAR_FLAG] & 0x80 if record + RECORD <= len(data) else 0
            for script in block.scripts:
                for command in act_commands(script, data):
                    commands.acts += 1
                    if command is None:
                        commands.unwritten += 1
                    elif command == VARIABLE:
                        commands.dynamic += 1
                    else:
                        (commands.in_gear if in_gear else commands.on_foot)[command] += 1


def formula_census(disc: Disc, root: Path = ROOT) -> FormulaCensus:
    census = FormulaCensus()
    census.slot = disc.slot(*SETUP_ARCHIVE)
    census.archive = disc.sectors(census.slot)
    entries = archive_entries(census.archive)
    limits = [len(table.handlers) for table in formula_tables(root)]
    for family, (first, copied, end), limit, label, redirect in (
        (census.party, PARTY_COMMANDS, limits[0], "character", True),
        (census.gear, GEAR_COMMANDS, limits[1], "gear", False),
        (census.enemy, ENEMY_COMMANDS, limits[1], "enemy commands", False),
    ):
        family_census(family, entries, first, copied, end, limit, label, redirect)
    first, copied, _ = ENEMY_COMMANDS
    census.enemy_descriptors = descriptors(entries[first], copied)
    enemy_commands(census, root, disc.number)
    for kind, uses, in_gear in (
        ("on foot", census.commands.on_foot, False),
        ("in a gear", census.commands.in_gear, True),
    ):
        for command in sorted(uses):
            if command >= len(census.enemy_descriptors):
                census.selected_outside.append(f"{kind}: command {command} past the unpacked block")
                continue
            descriptor = census.enemy_descriptors[command]
            if descriptor[0] >= limits[enemy_table(descriptor, in_gear)]:
                census.selected_outside.append(f"{kind}: command {command} formula {descriptor[0]}")
    return census


# ---------------------------------------------------------------------------
# Model primitives
# ---------------------------------------------------------------------------

MODEL = 0x38  # model header bytes, also the stride in a group after 0x10 bytes
GROUP_HEADER = 0x10


@dataclass
class ModelWalk:
    groups: list[tuple[int, int]] = field(default_factory=list)  # (type, records)
    overrides: Counter = field(default_factory=Counter)
    other_cx: int = 0  # cx command bytes func_8002CD64 passes on as the record's own
    packet_bytes: int = 0
    errors: list[str] = field(default_factory=list)
    differences: list[str] = field(default_factory=list)  # header fields against the walk


def walk_model(data: bytes, model: int, base: int, table: PrimitiveTable) -> ModelWalk:
    """func_8002C8CC over the model header at data[model], whose offsets count
    from data[base] (its group, or the model itself after func_8002C59C)."""
    walk = ModelWalk()
    if not 0 <= model <= len(data) - MODEL:
        walk.errors.append("model header outside its data")
        return walk
    primitives, group_count = struct.unpack_from("<HH", data, model + 4)
    groups, aux = struct.unpack_from("<II", data, model + 0x10)
    packet_size = struct.unpack_from("<I", data, model + 0x34)[0]
    record, extra = base + groups, base + aux
    types = table.types
    for number in range(group_count):
        if not 0 <= record <= len(data) - 4:
            walk.errors.append(f"group {number} header outside its data")
            return walk
        kind, _, count = struct.unpack_from("<BBh", data, record)
        if kind >= len(types):
            walk.errors.append(f"group {number} type {kind} past the {len(types)}-entry table")
            return walk
        if count < 0:
            walk.errors.append(f"group {number} count {count}")
            return walk
        spec = types[kind]
        walk.groups.append((kind, count))
        record += 4 + count * spec.stride
        for _ in range(count):
            while spec.overrides and 0 <= extra <= len(data) - 4:
                code = data[extra + 3]
                if code not in table.override_codes:
                    walk.other_cx += code & 0xF0 == 0xC0
                    break
                walk.overrides[code] += 1
                extra += 4
            extra += spec.aux_stride
        walk.packet_bytes += count * spec.packet_size
    if record > len(data) or extra > len(data):
        walk.errors.append("records run past their data")
    if groups < aux and record > base + aux:  # other sections (morphs) may lie between
        walk.differences.append("the records run into the auxiliary data")
    if walk.packet_bytes != packet_size:
        walk.differences.append(
            f"packets need 0x{walk.packet_bytes:x}, header +0x34 0x{packet_size:x}"
        )
    if sum(count for _, count in walk.groups) != primitives:
        walk.differences.append("the primitive count at +4 differs from the records")
    return walk


def group_models(data: bytes, group: int) -> list[int]:
    """func_8002C3E8: the group's model count at +0, then models from +0x10."""
    if not 0 <= group <= len(data) - GROUP_HEADER:
        raise CensusError(f"model group at 0x{group:x} outside its data")
    count, flags = struct.unpack_from("<iI", data, group)
    if not 0 < count <= 0x400 or group + GROUP_HEADER + MODEL * count > len(data) or flags & 1:
        raise CensusError(f"no unrelocated model group at 0x{group:x}")
    return [group + GROUP_HEADER + MODEL * k for k in range(count)]


@dataclass(frozen=True)
class ModelRef:
    where: str
    data: bytes
    model: int
    base: int


def group_refs(where: str, data: bytes, group: int) -> list[ModelRef]:
    return [
        ModelRef(f"{where} model {k}", data, model, group)
        for k, model in enumerate(group_models(data, group))
    ]


def file_bytes(disc: Disc, slot: int) -> bytes:
    return (
        ROOT / ".local/extract" / f"disc{disc.number}" / "files" / f"{slot:04d}.bin"
    ).read_bytes()


def record_count(disc: Disc, group: int, index: int, file: int) -> int:
    """The file count of the directory record at (group, index) file `file`."""
    size = disc.entries[disc.slot(group, index, file)]["size"]
    if size >= 0:
        raise CensusError(f"({group:#x}, {index}) file {file} is not a directory record")
    return -size


def object_model_group(data: bytes, base: int = 0) -> tuple[int, int]:
    """func_800A8BF0: an object model file is an offset table (8003342c); its
    model group runs from entry 2 to entry 3 (the hierarchy)."""
    count = struct.unpack_from("<I", data, base)[0]
    if not 4 <= count < 0x40 or base + 4 + 4 * count > len(data):
        raise CensusError(f"no object model file at 0x{base:x}")
    models, hierarchy = struct.unpack_from("<II", data, base + 8)
    if not 0 < models < hierarchy <= len(data) - base:
        raise CensusError(f"object model file at 0x{base:x} has no model group")
    return base + models, base + hierarchy


def object_refs(where: str, data: bytes, base: int = 0) -> list[ModelRef]:
    group, end = object_model_group(data, base)
    view = data[group:end]  # the copy func_800A8BF0 relocates
    return group_refs(where, view, 0)


def field_models(disc: Disc) -> Iterator[ModelRef]:
    first = disc.slot(4, 0, 0xB8)  # 800777dc: map bundle 0xb8 + 2 * map
    for number in range(record_count(disc, 4, 0, 0xB7) // 2):
        data = file_bytes(disc, first + 2 * number)
        if len(data) < 0x154:
            continue  # a placeholder map
        size, offset = (struct.unpack_from("<I", data, at + 8)[0] for at in (0x10C, 0x130))
        geometry = decode_block(data[offset:], output_limit=size + 16).data[:size]
        count = struct.unpack_from("<I", geometry, 0)[0]
        for k, group in enumerate(struct.unpack_from(f"<{count}I", geometry, 4)):
            yield from group_refs(f"map {number} group {k}", geometry, group)


def actor_models(disc: Disc) -> Iterator[ModelRef]:
    for k in range((record_count(disc, 4, 0, 0x6B8) - 1) // 2):
        number = 0x6BB + 2 * k
        yield from object_refs(f"file {number:#x}", file_bytes(disc, disc.slot(4, 0, number)))


def battle_models(disc: Disc) -> Iterator[ModelRef]:
    for s in range(record_count(disc, 12, 3, 5) // 2):  # func_800379D8: scene < file 5's count / 2
        yield from object_refs(f"stage {s}", file_bytes(disc, disc.slot(12, 3, 6 + 2 * s)))
    for n in range((record_count(disc, 12, 1, 1) - 1) // 2):
        data = file_bytes(disc, disc.slot(12, 1, 2 * n + 3))
        for k in range(data[0]):  # 801e6314: 12-byte entries after an 8-byte header
            _, images, model = struct.unpack_from("<IIB", data, 8 + 12 * k)
            if model and images >= 8:
                yield from object_refs(f"enemy set {n} entry {k}", data, images)
    files = 1
    while disc.entries[disc.slot(0x28, 0, files + 1)]["size"] > 0:
        files += 1  # the object set pairs run up to the next directory record
    for s in range(files // 2):
        yield from object_refs(f"object set {s}", file_bytes(disc, disc.slot(0x28, 0, 2 * s + 1)))
    for gear, (base, variants) in enumerate(gear_files()):
        if not base:
            continue
        yield from object_refs(f"gear {gear}", file_bytes(disc, disc.slot(0x28, 1, base + 1)))
        for v in range(1, variants + 1):
            data = file_bytes(disc, disc.slot(0x28, 1, base + 2 + v))
            table, _, end = struct.unpack_from("<III", data, 4)  # GearPartFile
            if struct.unpack_from("<h", data, table)[0] > 0:  # only positive counts make parts
                yield from object_refs(f"gear {gear} part file {v}", data, end)


def menu_models(disc: Disc) -> Iterator[ModelRef]:
    for number in range(2, record_count(disc, 0x30, 1, 1) + 2):  # func_8008509C: id + 2
        data = decode_block(disc.sectors(disc.slot(0x30, 1, number))).data
        # func_8008AF6C relocates the pointers against the base word at +0x1c;
        # func_8008B38C relocates the model group at +4.
        models, base = struct.unpack_from("<I", data, 4)[0], struct.unpack_from("<I", data, 0x1C)[0]
        yield from group_refs(f"arena model {number - 2}", data, models - base)
    model = 0x80091FB0 - BASE  # func_800852C4: func_8002C59C(D_80091FB0)
    yield ModelRef("D_80091FB0", disc_image("menu", disc.number), model, model)


def worldmap_models(disc: Disc) -> Iterator[ModelRef]:
    for area in sorted(set(area_files())):
        data = decode_block(disc.sectors(disc.slot(0x24, 0, area + 1))).data
        group = struct.unpack_from("<I", data, 8)[0]  # AreaHeader.off8 (func_80073530)
        yield from group_refs(f"area {area}", data, group)


# func_8001FBE4: f5 binds the model at its target (relocated by func_8002C59C);
# f6 and f7 relocate the group there (func_8002C3E8) and build its first model.
SPRITE_MODEL_COMMANDS = {0xF5: 0, 0xF6: GROUP_HEADER, 0xF7: GROUP_HEADER}


def sprite_models(disc: Disc, root: Path = ROOT) -> Iterator[ModelRef]:
    """The models f5-f7 bind in every sprite block tools.analysis.sprite_vm finds."""
    extract = root / ".local/extract" / f"disc{disc.number}"
    manifest = json.loads((extract / "manifest.json").read_text())
    starts = sprite_vm.directory_starts(root / ".local/discs" / f"disc{disc.number}.bin")
    firsts = [slot for slot, _ in starts]
    seen = set()
    for entry in manifest["files"]:
        if entry["size"] <= 0:
            continue
        slot = entry["slot"]
        directory = starts[bisect.bisect_right(firsts, slot) - 1][1]
        dialect = sprite_vm.BATTLE if directory in sprite_vm.BATTLE_DIRECTORIES else sprite_vm.FIELD
        data = file_bytes(disc, slot)
        for view in sprite_vm.file_views(disc.number, slot, data, Counter()):
            for block in sprite_vm.resource_blocks(view.data):
                if not block.headers:
                    continue
                listing = sprite_vm.block_listing(view.data, block, dialect)
                for ins in listing.instructions:
                    skip = SPRITE_MODEL_COMMANDS.get(ins.opcode)
                    if skip is None:
                        continue
                    target = ins.data[0]
                    key = hashlib.sha256(view.data[target : target + skip + MODEL]).digest()
                    if key not in seen:
                        seen.add(key)
                        where = f"slot {slot} {view.name} +0x{ins.pc:x} {ins.opcode:02x}"
                        yield ModelRef(where, view.data, target + skip, target)


MODEL_SOURCES = (
    ("field maps", field_models),
    ("ovl2143 actors", actor_models),
    ("battle objects", battle_models),
    ("menu", menu_models),
    ("world map areas", worldmap_models),
    ("sprite commands", sprite_models),
)


@dataclass
class PrimitiveCensus:
    models: Counter = field(default_factory=Counter)  # source -> models walked
    distinct: dict[str, set] = field(default_factory=dict)  # source -> model digests
    groups: Counter = field(default_factory=Counter)  # type -> primitive groups
    records: Counter = field(default_factory=Counter)  # type -> records
    sources: dict[int, set] = field(default_factory=dict)  # type -> sources using it
    overrides: Counter = field(default_factory=Counter)
    other_cx: int = 0
    errors: list[str] = field(default_factory=list)
    differences: Counter = field(default_factory=Counter)
    difference_examples: list[str] = field(default_factory=list)


def primitive_census(disc: Disc) -> PrimitiveCensus:
    census = PrimitiveCensus()
    table = primitive_table()
    for name, source in MODEL_SOURCES:
        census.distinct.setdefault(name, set())
        try:
            refs = list(source(disc))
        except (CensusError, PackedError, struct.error) as error:
            census.errors.append(f"disc {disc.number} {name}: {error}")
            continue
        for ref in refs:
            walk = walk_model(ref.data, ref.model, ref.base, table)
            where = f"disc {disc.number} {name} {ref.where}"
            census.models[name] += 1
            census.errors += [f"{where}: {error}" for error in walk.errors]
            for difference in walk.differences:
                census.differences[difference.split(" 0x")[0]] += 1
                if len(census.difference_examples) < 8:
                    census.difference_examples.append(f"{where}: {difference}")
            digest = hashlib.sha256(
                repr((walk.groups, ref.data[ref.model : ref.model + MODEL])).encode()
            )
            census.distinct[name].add(digest.digest())
            for kind, count in walk.groups:
                census.groups[kind] += 1
                census.records[kind] += count
                census.sources.setdefault(kind, set()).add(name)
            census.overrides.update(walk.overrides)
            census.other_cx += walk.other_cx
    return census


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def _uses(counter: Counter) -> str:
    return " ".join(f"{key}:{counter[key]}" for key in sorted(counter)) or "none"


def dispatched(result: FormulaCensus) -> tuple[Counter, Counter]:
    """Formula ids per table: the party's and gears' own descriptors and each
    distinct enemy command an on-foot or a gear enemy selects."""
    counters = (Counter(result.party.formulas), Counter(result.gear.formulas))
    for uses, in_gear in ((result.commands.on_foot, False), (result.commands.in_gear, True)):
        for command in uses:
            if command < len(result.enemy_descriptors):
                descriptor = result.enemy_descriptors[command]
                counters[enemy_table(descriptor, in_gear)][descriptor[0]] += 1
    return counters


def formula_report(results: list[FormulaCensus]) -> list[str]:
    foot, gear = formula_tables()
    same = len({r.archive for r in results}) == 1
    out = [
        f"battle formulas: {foot.name} {len(foot.handlers)} handlers ({foot.caller}), "
        f"{gear.name} {len(gear.handlers)} handlers ({gear.caller})",
        "  setup archive (12, 0) file 3: "
        + ", ".join(f"disc {n} slot {r.slot}" for n, r in enumerate(results, 1))
        + (", identical" if same else ", different"),
    ]
    for n, result in enumerate(results, 1):
        out.append(f"  disc {n}:")
        for label, family, outside in (
            ("party (archive[5 + character], on foot)", result.party, foot.name),
            ("gears (archive[0x11 + gear])", result.gear, gear.name),
            ("enemies (archive[4])", result.enemy, "both tables"),
        ):
            past = f", {family.short} more copied from past the block" if family.short else ""
            out.append(
                f"    {label}: {family.sets} sets, {family.records} descriptors{past}; "
                f"flagsA 0x10 {family.gear_descriptors}; formulas {_uses(family.formulas)}"
            )
            out.append(f"      outside {outside}: {len(family.outside)}")
            out += [f"        {item}" for item in family.outside]
        commands = result.commands
        out.append(
            f"    enemy AI act entries ({commands.files} files): {commands.acts}, "
            f"{commands.dynamic} with a variable command, {commands.unwritten} with none written"
        )
        enemies = result.enemy_descriptors
        for kind, uses, table in (
            ("on foot", commands.on_foot, foot),
            ("in a gear", commands.in_gear, gear),
        ):
            formulas = Counter(enemies[c][0] for c in uses if c < len(enemies))
            out.append(
                f"      {kind} ({table.name}): {len(uses)} commands, {sum(uses.values())} entries; "
                f"formulas {_uses(formulas)}"
            )
        unselected = [
            c
            for c in range(len(enemies))
            if c not in commands.on_foot and c not in commands.in_gear
        ]
        high = [
            f"#{c} ({enemies[c][0]})" for c in unselected if enemies[c][0] >= len(foot.handlers)
        ]
        out.append(
            f"      descriptors no act entry selects: {len(unselected)}, of them with a "
            f"formula past {foot.name}: {' '.join(high) or 'none'}"
        )
        out.append(f"      selected outside their table: {len(result.selected_outside)}")
        out += [f"        {item}" for item in result.selected_outside]
    counts = [dispatched(result) for result in results]
    for index, table in enumerate((foot, gear)):
        columns = ", ".join(f"disc {n}" for n in range(1, len(results) + 1))
        out.append(f"  {table.name} (descriptors and selected enemy commands; {columns}):")
        for formula, handler in enumerate(table.handlers):
            uses = "  ".join(f"{c[index][formula]:4d}" for c in counts)
            out.append(f"    {formula:2d} {handler}  {uses}")
        used = [sum(1 for f in range(len(table.handlers)) if c[index][f]) for c in counts]
        out.append(f"    used: {' / '.join(map(str, used))} of {len(table.handlers)}")
    return out


def primitive_report(results: list[PrimitiveCensus]) -> list[str]:
    table = primitive_table()
    out = [
        f"model primitives: D_8004FE50 {len(table.types)} types (func_8002C8CC prepare, "
        "func_8002C700 draw by sort mode 0-5)",
    ]
    for n, result in enumerate(results, 1):
        sources = ", ".join(
            f"{name} {result.models[name]} ({len(result.distinct[name])} distinct)"
            for name, _ in MODEL_SOURCES
            if name in result.distinct
        )
        out.append(f"  disc {n} models: {sources}")
        out.append(
            "    override words before records: "
            + " ".join(
                f"{code:02x}:{result.overrides[code]}" for code in sorted(table.override_codes)
            )
            + f"; other cx command bytes: {result.other_cx}"
        )
        out.append(
            f"    header fields that disagree with the walk: {sum(result.differences.values())}"
        )
        out += [f"      {text}: {count}" for text, count in sorted(result.differences.items())]
        out += [f"      e.g. {example}" for example in result.difference_examples]
        out.append(
            f"    errors (type past the table, bad counts, data overruns): {len(result.errors)}"
        )
        out += [f"      {error}" for error in result.errors[:20]]
    out.append("  type prepare stride aux packet: groups/records disc 1, disc 2; sources")
    for kind, spec in enumerate(table.types):
        counts = "  ".join(f"{r.groups[kind]:6d} {r.records[kind]:7d}" for r in results)
        names = sorted(set().union(*(r.sources.get(kind, set()) for r in results)))
        sizes = f"{spec.stride} {spec.aux_stride:2d} 0x{spec.packet_size:02x}"
        overrides = " c4/c8" if spec.overrides else "      "
        out.append(f"    {kind:2d} {spec.prepare} {sizes}{overrides}  {counts}  {', '.join(names)}")
    used = {kind for r in results for kind in r.groups}
    unused = " ".join(str(kind) for kind in range(len(table.types)) if kind not in used)
    out.append(f"    used: {len(used)} of {len(table.types)} types; unused: {unused or 'none'}")
    return out


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="census of both discs (aggregates)")
    parser.add_argument("--only", choices=("formulas", "primitives"), help="one census")
    args = parser.parse_args(argv)
    if not args.sweep:
        parser.error("choose --sweep")
    both = discs()
    lines = ["dispatch tables (both discs; tables read from decomp/src)"]
    failures = 0
    if args.only in (None, "formulas"):
        formulas = [formula_census(disc) for disc in both]
        lines += formula_report(formulas)
        failures += sum(len(r.selected_outside) for r in formulas)
    if args.only in (None, "primitives"):
        primitives = [primitive_census(disc) for disc in both]
        lines += primitive_report(primitives)
        failures += sum(len(r.errors) for r in primitives)
    print("\n".join(lines))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
