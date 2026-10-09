"""Special parts: the ids from 50 that index the game data's durability bytes.

Character 4 equips special parts beside its weapons (CharacterRecord.entryItems)
and so does the gear it pilots (GearRecord.partItems): weapon and gear part ids
from 50, 0 for an empty slot. The field menu offers the inventory's ids from 50
of the slot's weapon kind (slot39 func_801DE5CC); the battle lists weapon ids
50-72 (ovl2615 func_801E4CD0) and copies the 48 weapon records 50-97 (0x300
bytes from +0x320 of the setup archive's entry 0x25, func_801E5384). Each id's
durability byte is GameData.specialDurability[id - 50] (+0x22B8, characters)
or gearSpecialDurability[id - 50] (+0x22E8, gears), 48 bytes each
(decomp/include/resident/gamedata.h): the code forms its address from 50 bytes
before the array (0x8006F8BA; 0x8006F8EA, the flag word +0x22B6), so an index
outside 50-97 reaches other game data, which `alias` names.

    python3 -m tools.analysis.special_parts --sweep   # both discs, aggregates

The sweep reads the user's discs and prints the special records of the item
tables, the new-game file's arrays, flag word and special slots, the enemies'
gear special slots, and every id from 50 a source can put into the weapon list
or the gear part list:

* field give_item (8c) with the list in the item code's high byte (1 weapons,
  3 gear parts; field func_800951B8); a variable operand from v0400 is resolved
  from the same map's set_variable (35) immediates, since field func_800705DC
  clears those variables for each map;
* the shops the field opens: fe 59 (menu kind 4, ovl2601) with the first 30 ids
  of each 0x5c-byte record of the menu resources' file 6 as weapons
  (func_801C6A6C, func_801CF2A0), fe 5a (kind 5, ovl2602) with bytes
  0x3c-0x4f of each 0x64-byte record of file 7 as gear parts (func_801C6E74,
  func_801D44FC), and every record of both tables;
* the enemies' two drops (combatant record +0x150 chances, +0x152 ids, +0x154
  categories, 0 weapons and 3 gear parts; ovl2596 func_801E42C4,
  func_801E1444) and the AI's set_drop (3c, the first) and set_own_155 (3b, the
  second: +0x155, +0x153, +0x151).

The field menu's debug fill (slot39 func_801E5058) adds ids 1-71 to both lists.
"""

from __future__ import annotations

import argparse
import struct
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

from tools.analysis import battle_ai, events
from tools.analysis.disc_index import Disc, discs
from tools.analysis.dispatch_tables import SETUP_ARCHIVE, archive_entries
from tools.analysis.text_control import archive_entry

ROOT = Path(__file__).resolve().parents[2]
FIRST, COUNT = 50, 48  # slot39 func_801DE5CC ids >= 50; ovl2615 func_801E5384 records 50-97
BATTLE_END = 73  # ovl2615 func_801E4CD0: weapon ids 50..72 enter the battle's list
BATTLE_RECORDS = 0x25  # func_801E5384: archive[0x25] + 0x320, 0x300 bytes, to +0x5818
ITEM_BASE, GEAR_BASE = 0x2286, 0x22B6  # D_8006F8BA, D_8006F8EA as game data offsets
# The game data around the arrays (gamedata.h): (offset, size, member).
LAYOUT = (
    (0x221A, 150, "gearAccessoryIds"),
    (0x22B0, 1, "unk22B0"),
    (0x22B1, 3, "inGear"),
    (0x22B4, 2, "unk22B4"),
    (0x22B6, 2, "flags"),
    (0x22B8, COUNT, "specialDurability"),
    (0x22E8, COUNT, "gearSpecialDurability"),
    (0x2318, 2, "locked"),
    (0x231A, 2, "map"),
    (0x231C, 6, "entry"),
)
FLAGS, LOCKED = 0x22B6, 0x2318
NEW_GAME = (0x10, 0, 3)  # func_8001B970 loads it whole into D_8006D634
CHARACTERS, CHARACTER, ENTRY_ITEMS = 0x26C, 0xA4, 0x6F  # 11 records
GEARS, GEAR, PART_ITEMS = 0x978, 0xA4, 0x04  # 20 records
MENU_DATA = (0x10, 0, 2)  # slot39 func_801C72BC: the MenuDataArchive
WEAPONS, GEAR_WEAPONS = 1, 42  # its +8 and +0xAC entries (tables->weapons, ->gearWeapons)
# (entry, record size, users offset and width, durability offset, kind offset):
# slot39 MenuWeapon / GearWeapon; battle BattleItem / BattlePart durability.
TABLES = {
    "weapon": (WEAPONS, 0x10, 0x0, 2, 0x3, 0x6),
    "gear weapon": (GEAR_WEAPONS, 0x14, 0x4, 4, 0xC, 0xF),
}
# D_8005945C, the shops' resource archive: field func_800799D4 loads file 1 of
# directory 0x10 with the menu overlay, and so does the resident's debug start
# (func_8001C1A8).
RESOURCES = (0x10, 0, 1)
SHOPS, SHOP, SHOP_WEAPONS = 6, 0x5C, 30  # ovl2601: files[6], kind 0 (ids 0-29) weapons
GEAR_SHOPS, GEAR_SHOP, GEAR_PARTS = 7, 0x64, (0x3C, 0x50)  # ovl2602: files[7], stock[4]
OPEN_SHOP, OPEN_GEAR_SHOP = "fe 59", "fe 5a"  # field func_800939A0, func_80093A04
GIVE_ITEM, SET_VARIABLE = "8c", "35"
WEAPON_LIST, GEAR_PART_LIST = 1, 3  # field item code lists
LOCAL_VARIABLES = 0x400  # byte offsets of D_800C3A68[0x200..0x3ff]
RECORDS, RECORD, ENEMIES = 0x32, 0x170, 8  # ovl2615 func_801E4870
DROPS = ((0x150, 0x152, 0x154), (0x151, 0x153, 0x155))  # (chance, id, category)
DROP_LISTS = {0: "weapon list", 3: "gear part list"}  # ovl2596 func_801E1444
AI_DROPS = {0x3C: 0, 0x3B: 1}  # set_drop, set_own_155: b1 category, b2 id
ENEMY_PART_ITEMS = 0xA4 + PART_ITEMS  # the combatant's gear record


def alias(base: int, index: int) -> str:
    """The game data member byte that `index` reaches from `base`."""
    offset = base + index
    for start, size, name in LAYOUT:
        if start <= offset < start + size:
            return f"{name}[{offset - start}]" if size > 2 else f"{name} byte {offset - start}"
    return f"+0x{offset:x}"


def specials(table: bytes, record: int, users: int, width: int, durability: int, kind: int):
    """{id: (users, kind, durability)} of the records from FIRST with a kind,
    and the ids from FIRST whose record has none."""
    found, empty = {}, []
    for index in range(FIRST, len(table) // record):
        row = table[record * index : record * (index + 1)]
        mask = int.from_bytes(row[users : users + width], "little")
        if row[kind]:
            found[index] = (mask, row[kind], row[durability])
        else:
            empty.append(index)
    return found, empty


def shop_ids(table: bytes, size: int, span: tuple[int, int]) -> dict[int, set[int]]:
    """{shop: ids} of bytes span[0]..span[1] of each whole record."""
    first, end = span
    return {
        shop: set(table[size * shop + first : size * shop + end]) - {0}
        for shop in range(len(table) // size)
    }


def drops(data: bytes) -> list[tuple[int, int]]:
    """(category, id) of both drops of every whole enemy record."""
    out = []
    for enemy in range(ENEMIES):
        record = RECORDS + RECORD * enemy
        if record + RECORD <= len(data):
            out += [(data[record + c], data[record + i]) for _, i, c in DROPS]
    return out


def ranges(ids) -> str:
    ids, parts = sorted(set(ids)), []
    for value in ids:
        if parts and parts[-1][1] == value - 1:
            parts[-1][1] = value
        else:
            parts.append([value, value])
    return ", ".join(f"{a}" if a == b else f"{a}-{b}" for a, b in parts) or "none"


def lists() -> dict[str, dict[int, set[str]]]:
    return {"weapon list": defaultdict(set), "gear part list": defaultdict(set)}


@dataclass
class Census:
    tables: dict[str, tuple[dict, list]] = field(default_factory=dict)
    battle_records: bool = False  # the battle's copy is the weapon table's records 50-97
    new_game: dict[str, object] = field(default_factory=dict)
    # list -> {id: sources}: ids from FIRST a source puts into the list, and
    # those only a table no immediate operand opens holds
    sources: dict[str, dict[int, set[str]]] = field(default_factory=lists)
    unopened: dict[str, dict[int, set[str]]] = field(default_factory=lists)
    variable_gives: int = 0
    unresolved: list[str] = field(default_factory=list)
    variable_shops: list[str] = field(default_factory=list)
    enemy_slots: Counter = field(default_factory=Counter)


def written(instructions) -> dict[int, list[int | None]]:
    """variable -> what each instruction naming it as a `var` operand stores:
    set_variable's immediate, else None (unknown)."""
    writes = defaultdict(list)
    for ins in instructions:
        for operand, value in zip(ins.spec.operands, ins.operands, strict=True):
            if operand.kind == "var":
                immediate = ins.key == SET_VARIABLE and ins.immediate[1]
                writes[value].append(ins.operands[1] & 0xFFFF if immediate else None)
    return writes


def item_codes(ins, writes: dict[int, list[int | None]]) -> list[int] | None:
    """The item codes give_item `ins` may add; None when they are not known."""
    if ins.immediate[0]:
        return [ins.operands[0] & 0x7FFF]
    values = writes.get(ins.operands[0], [])
    if ins.operands[0] < LOCAL_VARIABLES or not values or None in values:
        return None
    return values


def listed(code: int) -> tuple[str, int] | None:
    """(list, id) when item code `code` adds an id from FIRST to the weapon or
    gear part list (the list in the high byte, field func_800951B8)."""
    kind = {WEAPON_LIST: "weapon list", GEAR_PART_LIST: "gear part list"}.get(code >> 8)
    return (kind, code & 0xFF) if kind and code & 0xFF >= FIRST else None


def field_sources(census: Census, disc: Disc, root: Path) -> tuple[set[int], set[int]]:
    """Field give_item ids and the shop numbers fe 59 and fe 5a name."""
    extract = root / ".local/extract" / f"disc{disc.number}"
    track = root / ".local/discs" / f"disc{disc.number}.bin"
    shops, gear_shops = set(), set()
    for map_id, path in events.map_files(extract, track):
        package = events.map_events(path.read_bytes())
        if package is None:
            continue
        starts, _ = events.script_entries(package)
        walk = events.walk(package.bytecode, [pc for _, _, pc in starts])
        writes = written(walk.instructions.values())
        for ins in sorted(walk.instructions.values(), key=lambda i: i.pc):
            if ins.key in (OPEN_SHOP, OPEN_GEAR_SHOP):
                if ins.immediate[0]:
                    (shops if ins.key == OPEN_SHOP else gear_shops).add(ins.operands[0] & 0x7FFF)
                else:
                    census.variable_shops.append(f"map {map_id} {ins.text()}")
            if ins.key != GIVE_ITEM:
                continue
            census.variable_gives += not ins.immediate[0]
            codes = item_codes(ins, writes)
            if codes is None:
                census.unresolved.append(f"map {map_id} +0x{ins.pc:x} {ins.text()}")
                continue
            for kind, item in filter(None, map(listed, codes)):
                census.sources[kind][item].add(f"give_item map {map_id}")
    return shops, gear_shops


def census_disc(disc: Disc, root: Path = ROOT) -> Census:
    census = Census()
    menu = disc.sectors(disc.slot(*MENU_DATA))
    raw = {}
    for name, (entry, *layout) in TABLES.items():
        raw[name] = archive_entry(menu, entry, packed=True)
        census.tables[name] = specials(raw[name], *layout)
    copied = slice(TABLES["weapon"][1] * FIRST, TABLES["weapon"][1] * (FIRST + COUNT))
    setup = archive_entries(disc.sectors(disc.slot(*SETUP_ARCHIVE)))
    census.battle_records = setup[BATTLE_RECORDS][copied] == raw["weapon"][copied]
    game = disc.data(disc.slot(*NEW_GAME))
    slots = []
    for c in range(11):
        row = tuple(game[CHARACTERS + CHARACTER * c + ENTRY_ITEMS :][:5])
        if any(row):
            slots.append((f"character {c}", row))
    for g in range(20):
        row = tuple(game[GEARS + GEAR * g + PART_ITEMS :][:4])
        if any(row):
            slots.append((f"gear {g}", row))
    census.new_game = {
        "specialDurability": game[0x22B8 : 0x22B8 + COUNT],
        "gearSpecialDurability": game[0x22E8 : 0x22E8 + COUNT],
        "flags": struct.unpack_from("<H", game, FLAGS)[0],
        "locked": struct.unpack_from("<H", game, LOCKED)[0],
        "gearAccessoryIds[108]": game[ITEM_BASE],
        "slots": slots,
    }
    shops, gear_shops = field_sources(census, disc, root)
    archive = disc.sectors(disc.slot(*RESOURCES))
    for table, size, span, opened, kind, name in (
        (SHOPS, SHOP, (0, SHOP_WEAPONS), shops, "weapon list", "shop"),
        (GEAR_SHOPS, GEAR_SHOP, GEAR_PARTS, gear_shops, "gear part list", "gear shop"),
    ):
        for shop, ids in shop_ids(archive_entry(archive, table, packed=True), size, span).items():
            target = census.sources if shop in opened else census.unopened
            for item in ids:
                if item >= FIRST:
                    target[kind][item].add(f"{name} {shop}")
    extract = root / ".local/extract" / f"disc{disc.number}"
    track = root / ".local/discs" / f"disc{disc.number}.bin"
    for n, path in battle_ai.enemy_files(extract, track):
        data = path.read_bytes()
        for category, item in drops(data):
            if category in DROP_LISTS and item >= FIRST:
                census.sources[DROP_LISTS[category]][item].add(f"drop battle {n}")
        for enemy in range(ENEMIES):
            record = RECORDS + RECORD * enemy
            if record + RECORD <= len(data):
                census.enemy_slots[tuple(data[record + ENEMY_PART_ITEMS :][:4])] += 1
        try:
            blocks = battle_ai.enemy_blocks(data)
        except ValueError:
            continue
        for block in blocks:
            for script in block.scripts:
                for ins in script.instructions:
                    if ins.opcode in AI_DROPS and ins.pc in script.reachable:
                        category, item = ins.raw[1], ins.raw[2]
                        if category in DROP_LISTS and item >= FIRST:
                            census.sources[DROP_LISTS[category]][item].add(f"AI drop battle {n}")
    return census


def report(results: dict[int, Census]) -> str:
    out = []
    for number, census in results.items():
        out.append(f"disc {number}")
        for name, (found, empty) in census.tables.items():
            kinds = sorted({kind for _, kind, _ in found.values()})
            durability = sorted({value for _, _, value in found.values()})
            out.append(
                f"  {name} table: ids {ranges(found)} kinds {kinds} durability {durability};"
                f" ids {ranges(empty)} no kind"
            )
        out.append(
            f"  setup archive entry 0x{BATTLE_RECORDS:x} +0x320-0x61f (the battle's records"
            f" {FIRST}-{FIRST + COUNT - 1}) equals the weapon table's: {census.battle_records}"
        )
        game = census.new_game
        for name in ("specialDurability", "gearSpecialDurability"):
            out.append(f"  new game {name}: {dict(sorted(Counter(game[name]).items()))}")
        out.append(
            f"  new game flags 0x{game['flags']:04x}, locked 0x{game['locked']:04x},"
            f" gearAccessoryIds[108] {game['gearAccessoryIds[108]']}"
        )
        slots = "; ".join(f"{who} {list(row)}" for who, row in game["slots"])
        out.append(f"  new game special slots: {slots}")
        for kind in ("weapon list", "gear part list"):
            ids = census.sources[kind]
            out.append(f"  {kind}, ids from {FIRST} a source supplies: {ranges(ids)}")
            for item in sorted(ids):
                names = sorted(ids[item])
                out.append(f"    {item}: {', '.join(names[:4])}{' ...' if len(names) > 4 else ''}")
            unopened = {i: s for i, s in census.unopened[kind].items() if i not in ids}
            if unopened:
                out.append(f"  {kind}, ids only in tables no immediate opens: {ranges(unopened)}")
                for item in sorted(unopened):
                    out.append(f"    {item}: {', '.join(sorted(unopened[item]))}")
        out.append(
            f"  give_item variable operands: {census.variable_gives},"
            f" unresolved {len(census.unresolved)}"
        )
        out += [f"    {line}" for line in census.unresolved[:8]]
        out.append(f"  shops opened by a variable number: {census.variable_shops or 'none'}")
        out.append(f"  enemy gear special slots: {dict(census.enemy_slots)}")
        for kind, base in (("weapon list", ITEM_BASE), ("gear part list", GEAR_BASE)):
            ids = set(census.sources[kind]) | set(census.unopened[kind])
            past = sorted(i for i in ids if i >= FIRST + COUNT)
            listed = ", ".join(f"{i} -> {alias(base, i)}" for i in past) or "none"
            out.append(f"  {kind} ids past {FIRST + COUNT - 1}: {listed}")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--sweep", action="store_true", help="census of both discs (aggregates)")
    args = parser.parse_args(argv)
    if not args.sweep:
        parser.error("choose --sweep")
    print(report({disc.number: census_disc(disc) for disc in discs()}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
