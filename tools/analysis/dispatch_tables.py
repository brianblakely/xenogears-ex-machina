"""Censuses of the data-indexed dispatch tables: battle formulas (with the gear
techniques that select them), model primitives, world map arrival modes, sprite
kinds and TMD primitives.

None of these tables has a program counter: one selector field of a
fixed-layout record picks the entry, and no code checks it against the table's
length. The tables (their lengths, handlers and row values) are parsed from the
recovered C at run time, and `--sweep` reads every record the game's loaders
hand to them on both discs and prints aggregate counts only. The world map
modes, sprite kinds and TMD primitives are described with their sections below.

Battle formulas (decomp/src/battle/battle_menus_and_resolver.c). For each target in the mask,
battle_resolve_action calls battle_formula_table[battle_current_command->formula](). An
attacker whose record has +0x15a bit 0x80 (fighting in a gear), or a descriptor with flagsA
bit 0x10, goes to battle_resolve_gear_action instead: it selects a party member's gear
descriptor (an enemy keeps its own) and calls battle_gear_formula_table[formula](). The
0x28-byte descriptors come from the battle setup archive, directory (12, 0) file 3 (resident
mode_battle_load_files), an offset table of packed blocks that ovl2615
battle_setup_load_party_and_enemy_files unpacks: archive[4] holds the enemy commands,
archive[5 + character] a party member's and archive[0x11 + gear] its gear's (copies of 0x1f40,
0x5f0 and 0x690 bytes). An enemy's command is the operand byte of its AI's type-1 action-list
entries (battle_action_list_act -> battle_commit_action), so the census also reads which
commands the enemy data files' AI scripts select (tools.analysis.battle_ai) and whether each
enemy fights in a gear (its record's +0x15a, copied by ovl2615
battle_setup_copy_enemy_records_and_ai).

Model primitives (decomp/src/resident/model_renderer.c). model_build_packets builds and
model_draw_sprite_model draws a model's primitive groups, a header {u8 type, u8, s16 count}
and count records each, through model_primitive_types[type]: a prepare routine per record,
the record stride, the auxiliary bytes per record and the packet size, and six
draw routines by sort mode. Prepare routines that call model_apply_override_command first take
texture page (c4) and CLUT (c8) override words from the auxiliary data. The
models (0x38-byte headers; groups of them relocated by model_relocate_group, single
ones by model_relocate_sprite_model) come from these loaders:

* field map bundles, directory (4, 0) file 0xb8 + 2 * map: every model group of
  the geometry component (component 2, field field_load_from_bundle);
* ovl2143 actors, the model file of each pair after ovl2143 in (4, 0), files
  0x6bb + 2k (field field_layer_load, the gear shop's gear_shop_model_read_files);
* battle objects (battle_create_object: the group between the model file's entries 2 and 3):
  stage files (12, 3) 6 + 2s (resident mode_load_battle_stage), the model entries of enemy set
  files (12, 1) 2n + 3 (ovl2615 battle_setup_build_enemy_sources_and_models), object sets (0x28,
  0) 2s + 1 (battle_read_object_set_files), gears (0x28, 1) base + 1 and their part files base +
  2 + v by battle_gear_file_table (battle_read_gear_files, battle_create_object_from_files);
* arena model files (0x30, 1) id + 2 (menu arena_actor_load_model; relocated against the base
  word at +0x1c, arena_node_relocate_model_file, and bound by arena_node_build_model_set) and
  the menu overlay's own arena_actor_extra_model (arena_mode_task);
* world map area files (0x24, 0) area + 1 for the area sets of worldmap_area_file_sets
  (worldmap_select_area_files, worldmap_unpack_area_data: the group at header +8,
  worldmap_objects_build);
* sprite commands f5 (a model), f6 and f7 (a model group) in the sprite blocks
  tools.analysis.sprite_vm decodes (resident sprite_vm_run_generic_command).

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

from tools.analysis import battle_ai, battle_event_vm, events, sprite_vm
from tools.analysis.disc_index import Disc, discs
from tools.analysis.overlay_scripts import BASE, disc_image
from tools.analysis.packed import PackedError, decode_block

ROOT = Path(__file__).resolve().parents[2]
# battle_formula_table, battle_gear_formula_table
FORMULA_UNIT = "battle/battle_menus_and_resolver.c"
GEAR_FILE_UNIT = "battle/battle_scene.c"  # battle_gear_file_table
MODEL_UNIT = "resident/model_renderer.c"  # model_primitive_types and its prepare routines
AREA_UNIT = "worldmap/worldmap_movement_terrain.c"  # worldmap_area_file_sets
MODE_UNIT = "worldmap/worldmap_open_map.c"  # worldmap_mode_handlers, the world map modes
SPRITE_UNIT = "resident/sprite_vm_draw.c"  # sprite_draw_callbacks, the sprite task callbacks
# battle_tmd_build_packets packets, battle_tmd_draw_object draws
TMD_UNIT = "battle/battle_tmd_screen_effects.c"
FORMULA_TABLES = (
    ("battle_formula_table", "battle_resolve_action"),
    ("battle_gear_formula_table", "battle_resolve_gear_action"),
)


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
        if f"{name}[battle_current_command->formula]()" not in function_body(text, caller):
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
    overrides: bool  # prepare calls model_apply_override_command before the record


@dataclass(frozen=True)
class PrimitiveTable:
    types: tuple[PrimitiveType, ...]
    override_codes: frozenset[int]  # command[3] values model_apply_override_command consumes


@cache
def primitive_table(root: Path = ROOT) -> PrimitiveTable:
    text = unit(MODEL_UNIT, root)
    length, body = initializer(text, "model_primitive_types")
    rows = re.findall(r"\{\{([^}]*)\},\s*(\w+),\s*(\w+),\s*(\w+),\s*(\w+)\}", body)
    if length != len(rows):
        raise CensusError(f"model_primitive_types[{length}] has {len(rows)} rows")
    types = []
    for draw, prepare, stride, aux_stride, packet_size in rows:
        routines = tuple(name.strip() for name in draw.split(","))
        if len(routines) != 6:
            raise CensusError(f"a model_primitive_types row lists {len(routines)} draw routines")
        overrides = "model_apply_override_command(" in function_body(text, prepare)
        values = (int(stride, 0), int(aux_stride, 0), int(packet_size, 0))
        types.append(PrimitiveType(routines, prepare, *values, overrides))
    cases = re.findall(
        r"case (0x[0-9A-Fa-f]+):", function_body(text, "model_apply_override_command")
    )
    return PrimitiveTable(tuple(types), frozenset(int(code, 16) for code in cases))


@cache
def gear_files(root: Path = ROOT) -> tuple[tuple[int, int], ...]:
    """battle_gear_file_table: per gear its file base in (0x28, 1) and part file count."""
    _, body = initializer(unit(GEAR_FILE_UNIT, root), "battle_gear_file_table")
    values = [int(v, 0) for v in body.replace("\n", " ").split(",") if v.strip()]
    return tuple(zip(values[::2], values[1::2], strict=True))


@cache
def area_files(root: Path = ROOT) -> tuple[int, ...]:
    """worldmap_area_file_sets: each area set's first file (a record) in (0x24, 0)."""
    _, body = initializer(unit(AREA_UNIT, root), "worldmap_area_file_sets")
    return tuple(int(row.split(",")[0], 0) for row in re.findall(r"\{([^}]*)\}", body))


# ---------------------------------------------------------------------------
# Battle formulas
# ---------------------------------------------------------------------------

DESCRIPTOR = 0x28
FORMULA = 0x16  # CommandDescriptor.formula
FLAGS_A = 0x0A  # CommandDescriptor.flagsA
GEAR_DESCRIPTOR = 0x10  # flagsA bit battle_resolve_action hands to battle_resolve_gear_action
# mode_battle_load_files: 80028470(12, 0), file 3 into mode_battle_setup_archive
SETUP_ARCHIVE = (12, 0, 3)
# battle_setup_load_party_and_enemy_files: archive entry, bytes copied, and the first entry after
# the run (archive[0x10] and archive[0x24] are loaded for other uses).
ENEMY_COMMANDS = (4, 0x1F40, 5)
PARTY_COMMANDS = (5, 0x5F0, 0x10)  # archive[5 + character]
GEAR_COMMANDS = (0x11, 0x690, 0x24)  # archive[0x11 + gear]
ENEMY_RECORDS = 0x32  # 801e4870: records from +0x32, 0x170 bytes per enemy id
RECORD = 0x170
GEAR_FLAG = 0x15A  # Combatant.flags15A, bit 0x80
ACT = 1  # action-list entry type battle_action_list_execute hands to battle_action_list_act


def archive_entries(data: bytes) -> list[bytes]:
    """Entries 1..count of the setup archive unpacked (text_relocate_offset_table turns the
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
    outside_records: list[tuple[int, int, int]] = field(default_factory=list)  # set, #, formula


@dataclass
class EnemyCommands:
    """Commands the enemy AI's act entries select, by attacker kind."""

    files: int = 0
    acts: int = 0
    unwritten: int = 0  # act entries closed with no operand write before them
    dynamic: int = 0  # act entries whose operand comes from a variable
    joins: int = 0  # act entries that paths reach with different commands
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
    techniques: Techniques | None = None


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
                family.outside_records.append((index - first, number, formula))


def enemy_table(descriptor: tuple[int, int], in_gear: bool) -> int:
    """0 for battle_formula_table, 1 for battle_gear_formula_table: an enemy in a gear, or a
    descriptor with flagsA 0x10, goes through battle_resolve_gear_action with its own
    descriptor."""
    return int(in_gear or bool(descriptor[1] & GEAR_DESCRIPTOR))


VARIABLE = -1  # an operand taken from an AI variable


def act_commands(script: battle_ai.Script, data: bytes) -> Iterator[frozenset[int | None]]:
    """The operand bytes each act entry the script closes can carry (None when
    nothing wrote it, VARIABLE when a variable did), along the runner's paths:
    an action steps to the next word; a condition (with its 99 chain) goes on
    when it holds and, unless it always holds, skips its rule when it does
    not (battle_ai.analyse_script's branches), so a skipped rule's writes
    never reach a later entry. Every edge goes forward, so one pass in
    address order meets each word after all of its predecessors."""
    states: dict[int, set[int | None]] = {script.entry: {None}}

    def reach(pc: int, state: set[int | None]) -> None:
        if script.entry <= pc < script.end:
            states.setdefault(pc, set()).update(state)

    for pc in sorted(script.reachable - script.chained):
        state = states.pop(pc, set())
        op, offset, low, high = data[pc : pc + 4]
        if not state or op in battle_ai.ENDS:
            continue
        if op >= 0x80:
            reach(battle_ai.condition_end(data, pc, script.end), state)
            if script.branches.get(pc) is not None:
                reach(script.branches[pc], state)
            continue
        if op == 0x01 and offset == 0:  # 8007a828: the type byte closes the entry
            if low == ACT:
                yield frozenset(state)
            state = {None}
        elif op == 0x01 and offset == 1:
            state = {low}
        elif op == 0x3D and offset in (0, 1):  # 8007bc40: bytes offset, offset + 1
            state = {high if offset == 0 else low}
        elif (op == 0x02 and offset == 1) or (op == 0x52 and offset in (0, 1)):
            state = {VARIABLE}  # 8007a874 (a byte variable), 8007d148 (a halfword)
        reach(pc + 4, state)


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
                for possible in act_commands(script, data):
                    commands.acts += 1
                    commands.joins += len(possible) > 1
                    commands.unwritten += None in possible
                    commands.dynamic += VARIABLE in possible
                    for command in possible - {None, VARIABLE}:
                        (commands.in_gear if in_gear else commands.on_foot)[command] += 1


# A gear's technique slot k is its descriptor 21 + k (battle.c battle_confirm_art
# offers it, battle_execute_chosen_art commits it), offered only while bit 0x8000 >> k of
# the pilot's CharacterBattleData.mask6 is set (battle_is_flag_in_mask reads bits 0-15).
TECHNIQUE_BASE = 21
NEW_GAME = (0x10, 0, 3)  # mode_load_initial_game_data: the game data a new game starts from
CHARACTER, CHARACTERS, GEAR_ID = 0x26C, 11, 0xA0  # 0xa4-byte records, +0xa0 the gear
SKILLS, MASK6 = 0x16C0, 6  # 0x20-byte CharacterBattleData per character
# ovl2596 battle_results_load_resources: item 0 is the growth table (+0x5f20)
RESULTS = (0x10, 2, 2)
# battle_results_unlock_by_known_arts: k < 13, 0 ends
GROWTH, UNLOCKS_B, LEARNED_SLOTS = 0x110, 0xE0, 13
# battle_setup_load_party_and_enemy_files: formation flag 0x10 puts character 10 in gear 17
FORCED_GEAR = (17, 10)


@dataclass
class Techniques:
    """Which characters can pilot each gear and which technique bits their
    mask6 can hold: the new-game state, battle-results learning (ovl2596
    battle_results_unlock_by_known_arts), field ext d0 (a character's records copied over another's,
    gear and masks) and ext a1 (set_gear). Only the debug battle selector
    (ovl2606 battle_grant_debug_items_and_skills) writes the masks otherwise."""

    pilots: dict[int, set[int]] = field(default_factory=dict)  # gear -> characters
    bits: dict[int, int] = field(default_factory=dict)  # character -> possible mask6
    set_gear: set[tuple[int, int]] = field(default_factory=set)
    copies: set[tuple[int, int]] = field(default_factory=set)
    variable_operands: int = 0  # ext a1/d0 operands read from variables
    records: list[tuple[int, int, int, int, bool]] = field(default_factory=list)


def field_character_changes(disc: Disc, root: Path = ROOT) -> tuple[set, set, int]:
    """The (character, gear) pairs ext a1 sets and the (from, to) pairs ext d0
    copies in every reachable field script, and how many operands are variables."""
    extract = root / ".local/extract" / f"disc{disc.number}"
    track = root / ".local/discs" / f"disc{disc.number}.bin"
    pairs: dict[str, set] = {"fe a1": set(), "fe d0": set()}
    variables = 0
    for _, path in events.map_files(extract, track):
        package = events.map_events(path.read_bytes())
        if package is None:
            continue
        starts, _ = events.script_entries(package)
        walk = events.walk(package.bytecode, [pc for _, _, pc in starts])
        for ins in walk.instructions.values():
            if ins.key in pairs:
                if all(ins.immediate):
                    pairs[ins.key].add(tuple(value & 0x7FFF for value in ins.operands))
                else:
                    variables += 1
    return pairs["fe a1"], pairs["fe d0"], variables


def technique_census(
    disc: Disc, outside: list[tuple[int, int, int]], root: Path = ROOT
) -> Techniques:
    """The new-game state, the growth table and the field scripts of `disc`
    against the gear descriptors past battle_gear_formula_table."""
    game = disc.data(disc.slot(*NEW_GAME))
    results = disc.sectors(disc.slot(*RESULTS))
    growth = decode_block(results[struct.unpack_from("<I", results, 4)[0] :]).data
    gears, masks, learned = {}, {}, {}
    for c in range(CHARACTERS):
        gears[c] = game[CHARACTER + 0xA4 * c + GEAR_ID]
        masks[c] = struct.unpack_from("<H", game, SKILLS + 0x20 * c + MASK6)[0]
        entries = growth[GROWTH * c + UNLOCKS_B : GROWTH * c + UNLOCKS_B + 16]
        learned[c] = next((k for k, entry in enumerate(entries) if entry == 0), len(entries))
    return techniques(gears, masks, learned, *field_character_changes(disc, root), outside)


def techniques(
    gears: dict[int, int],
    masks: dict[int, int],
    learned: dict[int, int],
    set_gear: set[tuple[int, int]],
    copies: set[tuple[int, int]],
    variables: int,
    outside: list[tuple[int, int, int]],
) -> Techniques:
    """Each gear's possible pilots and each character's possible mask6 (its
    new-game mask, the slots below min(unlocksB entries, 13) that battle
    results can teach, and what ext d0 copies from another character), and
    whether a pilot can be offered each (gear, descriptor, formula)."""
    result = Techniques(set_gear=set(set_gear), copies=set(copies), variable_operands=variables)
    for c, gear in gears.items():
        result.bits[c] = masks[c]
        for k in range(min(learned[c], LEARNED_SLOTS)):
            result.bits[c] |= 0x8000 >> k
        result.pilots.setdefault(gear, set()).add(c)
    for character, gear in result.set_gear:
        result.pilots.setdefault(gear, set()).add(character)
    gear, character = FORCED_GEAR
    result.pilots.setdefault(gear, set()).add(character)
    changed = True
    while changed:  # a copy takes the gear and the masks along
        changed = False
        for source, target in result.copies:
            if result.bits[target] | result.bits[source] != result.bits[target]:
                result.bits[target] |= result.bits[source]
                changed = True
            for pilots in result.pilots.values():
                if source in pilots and target not in pilots:
                    pilots.add(target)
                    changed = True
    for gear, number, formula in outside:
        slot = number - TECHNIQUE_BASE
        bit = 0x8000 >> slot if 0 <= slot < 16 else 0
        pilots = result.pilots.get(gear, set())
        selectable = result.variable_operands > 0 or any(result.bits[c] & bit for c in pilots)
        result.records.append((gear, number, formula, slot, selectable))
    return result


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
    census.techniques = technique_census(disc, census.gear.outside_records, root)
    for gear, number, formula, slot, selectable in census.techniques.records:
        if selectable:
            census.selected_outside.append(
                f"gear {gear} #{number} (slot {slot}): formula {formula}"
            )
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
    other_cx: int = 0  # cx command bytes model_apply_override_command passes on as the record's own
    packet_bytes: int = 0
    errors: list[str] = field(default_factory=list)
    differences: list[str] = field(default_factory=list)  # header fields against the walk


def walk_model(data: bytes, model: int, base: int, table: PrimitiveTable) -> ModelWalk:
    """model_build_packets over the model header at data[model], whose offsets count
    from data[base] (its group, or the model itself after model_relocate_sprite_model)."""
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
    """model_relocate_group: the group's model count at +0, then models from +0x10."""
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
    """battle_create_object: an object model file is an offset table (8003342c); its
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
    view = data[group:end]  # the copy battle_create_object relocates
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
    # mode_load_battle_stage: scene < file 5's count / 2
    for s in range(record_count(disc, 12, 3, 5) // 2):
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
    for number in range(2, record_count(disc, 0x30, 1, 1) + 2):  # arena_actor_load_model: id + 2
        data = decode_block(disc.sectors(disc.slot(0x30, 1, number))).data
        # arena_node_relocate_model_file relocates the pointers against the base word at +0x1c;
        # arena_node_build_model_set relocates the model group at +4.
        models, base = struct.unpack_from("<I", data, 4)[0], struct.unpack_from("<I", data, 0x1C)[0]
        yield from group_refs(f"arena model {number - 2}", data, models - base)
    # arena_mode_task: model_relocate_sprite_model(arena_actor_extra_model)
    model = 0x80091FB0 - BASE
    yield ModelRef("arena_actor_extra_model", disc_image("menu", disc.number), model, model)


def worldmap_models(disc: Disc) -> Iterator[ModelRef]:
    for area in sorted(set(area_files())):
        data = decode_block(disc.sectors(disc.slot(0x24, 0, area + 1))).data
        group = struct.unpack_from("<I", data, 8)[0]  # AreaHeader.off8 (worldmap_unpack_area_data)
        yield from group_refs(f"area {area}", data, group)


# sprite_vm_run_generic_command: f5 binds the model at its target (relocated by
# model_relocate_sprite_model); f6 and f7 relocate the group there (model_relocate_group) and build
# its first model.
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
# World map arrival modes
# ---------------------------------------------------------------------------
#
# The world map overlay's entry (worldmap.c worldmap_main) runs mode game_data_worldmap_flag_word[0]
# & 0x7fff of worldmap_mode_handlers (its enter, then start and leave each frame) without a bound
# check. The word is the game data's +0x2320. The world map sets it to 1 for a new world state and
# keeps it across its own battles (bit 0x8000 marks the return); its exits store a field's entry
# there. Field 56 (change_map, field_event_change_map: operand 7) sets it as the field leaves for
# the world map (exit kind 1, mode 3). Battle event opcode 26 (ovl3087
# battle_event_script_set_saved_map) sets the scene (a) and the word (d); after the battle the world
# map runs only when the scene & 0x7ff is 0x400 or more (ovl2596 battle_results_leave_battle),
# otherwise d is a field's entry.

MODE_FIELD_OPERAND = 3  # 56's operands: scene 1, +231e 3, heading 5, arrival 7
MODE_EVENT_OPERAND = 3  # 26's operands: scene a, heading b, area c, arrival d
WORLD_SCENES = 0x400  # scene & 0x7ff from here on is the world map's
EVENT_ARCHIVE = (0x20, 0, 2)  # ovl3087 801e5160: the battle event script archive


@cache
def world_modes(root: Path = ROOT) -> tuple[str, ...]:
    """worldmap_mode_handlers's rows (each enter, start, leave), by mode."""
    length, body = initializer(unit(MODE_UNIT, root), "worldmap_mode_handlers")
    rows = tuple(re.findall(r"\{(\w+), (\w+), (\w+)\}", body))
    if length != len(rows):
        raise CensusError(f"worldmap_mode_handlers[{length}] lists {len(rows)} modes")
    return tuple(start for _, start, _ in rows)


# The world map leaves for a field (exit 0, worldmap.c worldmap_main) with the scene and entry of
# the current path region (worldmap_current_path, a PathRegion) or, from a scripted mode, constants
# its code stores in the game data's map (game_data.map, 0x8006f94e). worldmap_path_select_region
# makes a region current for a path table (it tests every region with a link; kind 4 regions only
# record a destination) and worldmap_path_select_region_of_kind for table 3. The area files (0x24,
# 0) area + 1 of worldmap_area_file_sets hold the four path tables (worldmap_unpack_area_data:
# AreaHeader.spots, then SpotHeader.table, offsets from the spot block).
PATH_TABLES = 4
REGION = 16  # PathRegion: x, z, w, h, id, entry, link, kind
DESTINATION = 4  # a kind-4 region records a destination, not a current path


@cache
def scripted_exits(root: Path = ROOT) -> tuple[int, ...]:
    """The scenes the world map's scripted modes store before leaving."""
    text = "".join(path.read_text() for path in sorted((root / "decomp/src/worldmap").glob("*.c")))
    values = re.findall(r"game_data\.map = (0x[0-9A-Fa-f]+|\d+);", _strip_comments(text))
    return tuple(sorted({int(value, 0) for value in values}))


def world_exits(disc: Disc, root: Path = ROOT) -> tuple[Counter, list[str]]:
    """The field (scene & 0xfff) of every path region a world-map exit can use,
    and the path tables that point outside their area file."""
    fields, anomalies = Counter(), []
    for area in sorted(set(area_files(root))):
        data = decode_block(disc.sectors(disc.slot(0x24, 0, area + 1))).data
        spots = struct.unpack_from("<i", data, 4)[0]
        table = spots + struct.unpack_from("<i", data, spots + 4)[0]
        for number in range(PATH_TABLES):
            region = spots + struct.unpack_from("<i", data, table + 4 * number)[0]
            if not 0 <= region <= len(data) - REGION:
                anomalies.append(f"area file {area + 1} path table {number}: outside the file")
                continue
            while (scene := struct.unpack_from("<h", data, region + 8)[0]) != -1:
                link, kind = struct.unpack_from("<hh", data, region + 12)
                if link != -1 and kind != DESTINATION and scene & 0x7FF < WORLD_SCENES:
                    fields[scene & 0xFFF] += 1
                region += REGION
                if region > len(data) - REGION:
                    anomalies.append(f"area file {area + 1} path table {number}: runs off the file")
                    break
    for scene in scripted_exits(root):
        if scene & 0x7FF < WORLD_SCENES:  # 0x400 is the new world state's place
            fields[scene & 0xFFF] += 1
    return fields, anomalies


@dataclass
class ModeCensus:
    exits: Counter = field(default_factory=Counter)  # field -> world-map exits naming it
    exit_anomalies: list[str] = field(default_factory=list)
    field_uses: Counter = field(default_factory=Counter)  # mode -> field 56 instructions
    event_uses: Counter = field(default_factory=Counter)  # mode -> battle event 26 instructions
    field_entries: int = 0  # battle event 26 instructions naming a field scene
    variables: list[str] = field(default_factory=list)  # operands read from variables
    outside: list[str] = field(default_factory=list)


def mode_census(disc: Disc, root: Path = ROOT) -> ModeCensus:
    census = ModeCensus()
    census.exits, census.exit_anomalies = world_exits(disc, root)
    limit = len(world_modes(root))
    extract = root / ".local/extract" / f"disc{disc.number}"
    track = root / ".local/discs" / f"disc{disc.number}.bin"
    for map_id, path in events.map_files(extract, track):
        package = events.map_events(path.read_bytes())
        if package is None:
            continue
        starts, _ = events.script_entries(package)
        walk = events.walk(package.bytecode, [pc for _, _, pc in starts])
        for pc, ins in sorted(walk.instructions.items()):
            if ins.key != "56":
                continue
            where = f"map {map_id} +0x{pc:04x}"
            if not ins.immediate[MODE_FIELD_OPERAND]:
                census.variables.append(f"{where} {ins.text()}")
                continue
            mode = ins.operands[MODE_FIELD_OPERAND] & 0x7FFF
            census.field_uses[mode] += 1
            if mode >= limit:
                census.outside.append(f"{where}: mode {mode}")
    archive = disc.sectors(disc.slot(*EVENT_ARCHIVE))
    for number, raw in battle_event_vm.archive_scripts(archive):
        script = battle_event_vm.parse_script(raw)
        listing = battle_event_vm.disassemble(script.code, battle_event_vm.entry_points(script))
        for ins in listing.instructions.values():
            if ins.opcode != 0x26:
                continue
            where = f"event set {number} +0x{ins.offset:04x}"
            scene, text = ins.operands[0][1], ins.operands[MODE_EVENT_OPERAND][1]
            if not scene.startswith("#") or not text.startswith("#"):
                census.variables.append(f"{where} {ins.text()}")
                continue
            if int(scene[1:], 16) & 0x7FF < WORLD_SCENES:
                census.field_entries += 1
                continue
            mode = int(text[1:], 16) & 0x7FFF
            census.event_uses[mode] += 1
            if mode >= limit:
                census.outside.append(f"{where}: mode {mode}")
    return census


# ---------------------------------------------------------------------------
# Sprite kinds
# ---------------------------------------------------------------------------
#
# A new sprite's kind is bits 8-10 of its animation header's flags plus 8 for bit 14
# (sprite_get_header_kind): effect sprites take it from a directory animation
# (sprite_create_effect), children from the header a command spawns (sprite_create_child; kind 3
# takes the parent's). sprite_task_init_by_kind rewrites camera markers 12 and 13 to 10 and 11 and
# hands the auxiliary task sprite_draw_callbacks[kind] (sprite_task_set_draw_by_kind); the task loop
# skips a NULL update (task_run_main_list).


@cache
def sprite_callbacks(root: Path = ROOT) -> tuple[str, ...]:
    length, body = initializer(unit(SPRITE_UNIT, root), "sprite_draw_callbacks")
    entries = tuple(entry.strip() for entry in body.split(",") if entry.strip())
    if length != len(entries):
        raise CensusError(f"sprite_draw_callbacks[{length}] lists {len(entries)} callbacks")
    return entries


def sprite_kind(flags: int) -> int:
    """sprite_get_header_kind: header flags bits 8-10, plus 8 for bit 14."""
    return ((flags >> 8) & 7) + (8 if flags >> 14 & 1 else 0)


@dataclass
class KindCensus:
    blocks: int = 0  # distinct blocks
    directory: Counter = field(default_factory=Counter)  # kind -> directory headers
    spawned: Counter = field(default_factory=Counter)  # kind -> headers commands spawn


def sprite_blocks(
    disc: Disc, root: Path = ROOT
) -> Iterator[tuple[bytes, sprite_vm.ResourceBlock, str]]:
    """(view, block, dialect) of every distinct sprite resource block of the disc."""
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
                key = (
                    dialect,
                    hashlib.sha256(view.data[block.offset : block.offset + block.size]).digest(),
                )
                if block.headers and key not in seen:
                    seen.add(key)
                    yield view.data, block, dialect


def kind_census(disc: Disc, root: Path = ROOT) -> KindCensus:
    census = KindCensus()
    for data, block, dialect in sprite_blocks(disc, root):
        census.blocks += 1
        for header in block.headers:
            census.directory[sprite_kind(sprite_vm.u16(data, header))] += 1
        listing = sprite_vm.block_listing(data, block, dialect)
        for ins in listing.instructions:
            for header in ins.headers:
                census.spawned[sprite_kind(sprite_vm.u16(data, header))] += 1
    return census


# ---------------------------------------------------------------------------
# TMD primitives
# ---------------------------------------------------------------------------
#
# Battle draws PlayStation TMD models (battle/effect_script.h's "effect script file"): the resident
# model_slot_ring_tmd (the slot-highlight ring, battle_slot_ring_create) and the model a battle
# sprite command f3 binds as its parts (battle_sprite_vm_run), which ovl3384
# battle_module_debris_start can also break into pieces. Both read object 0 (battle_tmd_get_object:
# 0x1c-byte entries after a 0xc-byte header); each primitive is olen (packet words - 1), ilen (data
# words - 1), flag and mode bytes and its data. battle_tmd_build_packets builds a packet and
# battle_tmd_draw_object draws it by kind (mode & 0x1c, plus 0x100 when flag bit 0, no lighting, is
# clear); ovl3384 switches on the same kinds. battle_tmd_get_packet_size sizes the packets by olen.

TMD_OBJECT = 0xC
RESIDENT_TMD = 0x8001C76C
POLYGON = 0x20  # GPU command codes 0x20-0x3f draw polygons
# libgpu.h packet sizes of the types battle_tmd_build_packets writes
POLY_SIZES = {
    "POLY_F3": 0x14,
    "POLY_G3": 0x1C,
    "POLY_FT3": 0x20,
    "POLY_GT3": 0x28,
    "POLY_F4": 0x18,
    "POLY_G4": 0x24,
    "POLY_FT4": 0x28,
    "POLY_GT4": 0x34,
}


def tmd_kind(flag: int, mode: int) -> int:
    return (mode & 0x1C) | ((flag ^ 1) & 1) << 8


def kr_body(text: str, name: str) -> str:
    """The body of a function defined K&R (parameter declarations before the
    brace), comments removed."""
    text = _strip_comments(text)
    match = re.search(rf"\n[^\n;]*\b{name}\([\w, ]*\)\n(?:[^{{}}]*;\n)*\{{(.*?)\n\}}", text, re.S)
    if match is None:
        raise CensusError(f"{name} has no K&R definition")
    return match.group(1)


def case_groups(body: str) -> list[tuple[tuple[int, ...], str]]:
    """The labels and statements of each case group of the first switch in body."""
    start = body.index("switch (kind) {")
    depth, end = 0, start
    for end in range(start, len(body)):
        depth += {"{": 1, "}": -1}.get(body[end], 0)
        if depth == 0 and body[end] == "}":
            break
    parts = re.split(r"\n\s*case (0x[0-9A-Fa-f]+):", body[start:end])
    groups, labels = [], []
    for label, text in zip(parts[1::2], parts[2::2], strict=True):
        labels.append(int(label, 16))
        if text.strip():
            groups.append((tuple(labels), text))
            labels = []
    return groups


@dataclass(frozen=True)
class TmdKind:
    packet: str  # the POLY type battle_tmd_draw_object draws for the kind's mode bits
    reads: int  # primitive bytes battle_tmd_build_packets and battle_tmd_draw_object read


@cache
def tmd_kinds(root: Path = ROOT) -> dict[int, TmdKind]:
    """Each kind battle_tmd_build_packets and battle_tmd_draw_object handle:
    battle_tmd_draw_object's second switch (mode & 0x1c) gives the packet each mode draws (the
    builder writes the colour bytes of the flat quads through POLY_F3), and the bytes read are
    the furthest cmd[] byte (builder) and vertex or normal index (both switches of the drawer)
    of the kind's cases."""
    text = unit(TMD_UNIT, root)
    reads: dict[int, int] = {}
    for labels, statements in case_groups(kr_body(text, "battle_tmd_build_packets")):
        offsets = [int(o, 0) + 1 for o in re.findall(r"cmd\[(0x[0-9A-Fa-f]+|\d+)\]", statements)]
        for label in labels:
            reads[label] = max([reads.get(label, 0), *offsets])
    draw = kr_body(text, "battle_tmd_draw_object")
    split = draw.index("kind = cmd[3] & 0x1C;\n        switch")
    for labels, statements in case_groups(draw[:split]):
        halves = re.findall(r"SET_VERTICES[34]\(([^)]*)\)|cmd, (0x[0-9A-Fa-f]+)\)", statements)
        offsets = []
        for group, single in halves:
            offsets += [int(o, 16) + 2 for o in (group.split(",") if group else [single])]
        for label in labels:
            reads[label] = max([reads.get(label, 0), *offsets])
    packets: dict[int, str] = {}
    for labels, statements in case_groups(draw[split:]):
        types = set(re.findall(r"\((POLY_\w+) \*\)prims", statements))
        if len(types) != 1:
            raise CensusError(f"battle_tmd_draw_object mode cases {labels} draw {sorted(types)}")
        packet = types.pop()
        for label in labels:
            packets[label] = packet
    if any(kind & 0x1C not in packets for kind in reads):
        raise CensusError(
            "battle_tmd_draw_object draws no packet for a kind battle_tmd_build_packets builds"
        )
    return {kind: TmdKind(packets[kind & 0x1C], reads[kind]) for kind in sorted(reads)}


@dataclass
class TmdCensus:
    models: Counter = field(default_factory=Counter)  # source -> distinct TMDs
    binds: int = 0  # f3 commands with a model in the distinct blocks
    primitives: Counter = field(default_factory=Counter)  # kind -> primitives
    modes: Counter = field(default_factory=Counter)  # mode byte -> primitives
    headers: Counter = field(default_factory=Counter)  # (id, flags, objects)
    errors: list[str] = field(default_factory=list)


def walk_tmd(
    census: TmdCensus, where: str, data: bytes, base: int, kinds: dict[int, TmdKind]
) -> None:
    if not 0 <= base <= len(data) - TMD_OBJECT - 0x1C:
        census.errors.append(f"{where}: TMD header outside its data")
        return
    census.headers[struct.unpack_from("<3I", data, base)] += 1
    entry = base + TMD_OBJECT
    commands, count = struct.unpack_from("<Ii", data, entry + 0x10)
    command = entry + commands
    for number in range(count):
        if not 0 <= command <= len(data) - 4:
            census.errors.append(f"{where}: primitive {number} outside its data")
            return
        olen, ilen, flag, mode = data[command : command + 4]
        kind = tmd_kind(flag, mode)
        census.primitives[kind] += 1
        census.modes[mode] += 1
        spec = kinds.get(kind)
        # battle_tmd_build_packets writes the mode as the packet's GPU code
        if mode & 0xE0 != POLYGON:
            census.errors.append(f"{where}: primitive {number} mode {mode:#x} is not a polygon")
        if spec is None:
            census.errors.append(f"{where}: primitive {number} kind {kind:#x} has no case")
        else:
            if 4 * (olen + 1) != POLY_SIZES[spec.packet]:
                census.errors.append(
                    f"{where}: primitive {number} kind {kind:#x} sizes {4 * (olen + 1)}"
                    f" packet bytes for a {spec.packet}"
                )
            if 4 * (ilen + 1) < spec.reads:
                census.errors.append(
                    f"{where}: primitive {number} kind {kind:#x} has {4 * (ilen + 1)} bytes,"
                    f" {spec.reads} read"
                )
        command += 4 * (ilen + 1)


def tmd_census(disc: Disc, root: Path = ROOT) -> TmdCensus:
    census = TmdCensus()
    kinds = tmd_kinds(root)
    boot = disc.boot
    text = struct.unpack_from("<I", boot, 0x18)[0]  # PS-X EXE text address, after a 0x800 header
    walk_tmd(census, "model_slot_ring_tmd", boot, RESIDENT_TMD - text + 0x800, kinds)
    census.models["resident"] += 1
    for data, block, dialect in sprite_blocks(disc, root):
        if dialect != sprite_vm.BATTLE:
            continue
        targets = set()
        for ins in sprite_vm.block_listing(data, block, dialect).instructions:
            if ins.opcode == 0xF3 and ins.data:
                census.binds += 1
                if ins.data[0] not in targets:  # each model of a distinct block once
                    targets.add(ins.data[0])
                    census.models["sprite f3"] += 1
                    where = f"f3 +0x{ins.pc:x} -> +0x{ins.data[0]:x}"
                    walk_tmd(census, where, data, ins.data[0], kinds)
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
            f"{commands.dynamic} with a variable command, {commands.unwritten} with none written,"
            f" {commands.joins} reached with different commands"
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
        techniques = result.techniques
        if techniques is not None:
            pilots = "; ".join(
                f"gear {g} " + ",".join(map(str, sorted(techniques.pilots.get(g, ()))))
                for g in sorted({record[0] for record in techniques.records})
            )
            out.append(
                f"    gear techniques past {gear.name}: pilots {pilots or 'none'};"
                f" ext a1 {len(techniques.set_gear)} pairs, ext d0 {len(techniques.copies)},"
                f" {techniques.variable_operands} variable operands"
            )
            for g, number, formula, slot, selectable in techniques.records:
                bits = " ".join(
                    f"{c}:{techniques.bits[c]:04x}" for c in sorted(techniques.pilots.get(g, ()))
                )
                out.append(
                    f"      gear {g} #{number} (formula {formula}) slot {slot}, mask6 bit"
                    f" {0x8000 >> slot:#06x}: pilots' possible mask6 {bits};"
                    f" {'selectable' if selectable else 'never offered'}"
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
        f"model primitives: model_primitive_types {len(table.types)} types"
        " (model_build_packets prepare, model_draw_sprite_model draw by sort mode 0-5)",
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


def mode_report(results: list[ModeCensus]) -> list[str]:
    modes = world_modes()
    out = [
        f"world map arrival modes: worldmap_mode_handlers {len(modes)} modes"
        " (worldmap.c worldmap_main, game_data_worldmap_flag_word[0] & 0x7fff)"
    ]
    for n, result in enumerate(results, 1):
        out.append(
            f"  disc {n}: field 56 operand 7 {_uses(result.field_uses)}; battle event 26"
            f" operand d {_uses(result.event_uses)} ({result.field_entries} name a field scene);"
            f" from variables {len(result.variables)}"
        )
        out += [f"    {item}" for item in result.variables]
        out.append(f"    past the table: {len(result.outside)}")
        out += [f"      {item}" for item in result.outside]
        out.append(
            f"    world-map exits name {len(result.exits)} fields:"
            f" {' '.join(map(str, sorted(result.exits)))}"
        )
        out += [f"      {item}" for item in result.exit_anomalies]
    used = {m for r in results for m in (*r.field_uses, *r.event_uses)}
    unused = " ".join(str(m) for m in range(len(modes)) if m not in used)
    out.append(f"  modes the data select: {len(used)} of {len(modes)}; others: {unused or 'none'}")
    return out


def kind_report(results: list[KindCensus]) -> list[str]:
    callbacks = sprite_callbacks()
    empty = [k for k, name in enumerate(callbacks) if name == "NULL"]
    out = [
        f"sprite kinds: sprite_draw_callbacks {len(callbacks)} callbacks"
        f" (sprite_task_set_draw_by_kind), NULL for {' '.join(map(str, empty))};"
        " header bits 8-10 and 14 (sprite_get_header_kind)"
    ]
    for n, result in enumerate(results, 1):
        out.append(
            f"  disc {n}: {result.blocks} distinct blocks; directory headers"
            f" {_uses(result.directory)}; spawned headers {_uses(result.spawned)}"
        )
    return out


def tmd_report(results: list[TmdCensus]) -> list[str]:
    kinds = tmd_kinds()
    out = [
        f"TMD primitives: {len(kinds)} kinds (battle_tmd_build_packets builds,"
        " battle_tmd_draw_object draws; mode & 0x1c, 0x100 lit)"
    ]
    for n, result in enumerate(results, 1):
        sources = ", ".join(f"{name} {count}" for name, count in sorted(result.models.items()))
        sources += f" (from {result.binds} f3 commands)"
        headers = ", ".join(
            f"id {i:#x} flags {f} objects {o}: {c}"
            for (i, f, o), c in sorted(result.headers.items())
        )
        out.append(f"  disc {n} models: {sources}; headers {headers}")
        out.append(
            "    primitives by kind: "
            + " ".join(f"{kind:#x}:{result.primitives[kind]}" for kind in sorted(result.primitives))
        )
        out.append(
            "    mode bytes: " + " ".join(f"{m:02x}:{c}" for m, c in sorted(result.modes.items()))
        )
        out.append(
            f"    errors (no case, not a polygon, packet or read size): {len(result.errors)}"
        )
        out += [f"      {error}" for error in result.errors[:20]]
    out.append("  kind packet bytes-read: primitives disc 1, disc 2")
    for kind, spec in kinds.items():
        counts = "  ".join(f"{r.primitives[kind]:6d}" for r in results)
        out.append(f"    {kind:#05x} {spec.packet:<8} {spec.reads:2d}  {counts}")
    used = {kind for r in results for kind in r.primitives}
    out.append(f"    used: {len(used & set(kinds))} of {len(kinds)} kinds")
    return out


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="census of both discs (aggregates)")
    parser.add_argument(
        "--only", choices=("formulas", "primitives", "modes", "kinds", "tmd"), help="one census"
    )
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
    if args.only in (None, "modes"):
        modes = [mode_census(disc) for disc in both]
        lines += mode_report(modes)
        failures += sum(len(r.outside) for r in modes)
    if args.only in (None, "kinds"):
        lines += kind_report([kind_census(disc) for disc in both])
    if args.only in (None, "tmd"):
        tmds = [tmd_census(disc) for disc in both]
        lines += tmd_report(tmds)
        failures += sum(len(r.errors) for r in tmds)
    print("\n".join(lines))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
