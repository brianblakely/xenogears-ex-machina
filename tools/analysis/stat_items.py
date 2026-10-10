"""The items and accessories that raise a character's stats, with their names.

The field menu (slot39) loads its item and accessory records from the menu data archive,
directory (0x10, 0) file 2 (menu_load_or_release_data_set): entry 0 holds the 16-byte item
records and entry 2 the 16-byte accessory records (ItemInfo and AccessoryInfo,
decomp/include/menu/tables.h). An item whose flags (+0x0a) have bit 4 raises the
CharacterRecord member of each bit of its +0x0c by its +0x08 (menu_use_item_on_character);
an accessory adds the low byte of its +0x0c to the equipment bonus of each of its bits
(menu_compute_character_equipment). System texts 22 and 17 name them (text_get_item_name,
text_get_accessory_name), with the characters text_control --chars reads from each disc's
own data. The names are the game's own labels for the stats the records raise.

    python3 -m tools.analysis.stat_items --sweep   # both discs, each member's raisers
"""

from __future__ import annotations

import argparse
import struct
from collections import defaultdict

from tools.analysis.disc_index import Disc, discs
from tools.analysis.text_control import (
    TextError,
    archive_entry,
    character_map,
    decode_text,
    font_threshold,
    render,
    system_data,
    text_table,
    unpack,
)

MENU_DATA = (0x10, 0, 2)  # menu_load_or_release_data_set: the MenuDataArchive
ITEMS, ACCESSORIES = 0, 2  # its +4 and +0xc entries (tables->items, tables->accessories)
RECORD = 0x10  # sizeof(ItemInfo), sizeof(AccessoryInfo)
ITEM_NAMES, ACCESSORY_NAMES = 22, 17  # text_system_resources[22] and [17]
RAISES_STATS = 4  # ItemInfo.flags bit 4
# The CharacterRecord member (decomp/include/resident/gamedata.h) each bit of +0x0c
# raises: menu_use_item_on_character for an item, menu_compute_character_equipment for an
# accessory.
ITEM_BITS = {
    0x8000: "attack",
    0x4000: "defense",
    0x2000: "ether",
    0x1000: "etherDefense",
    0x800: "maxHp",
    0x400: "maxEp",
}
ACCESSORY_BITS = {
    0x8000: "equipAttack",
    0x4000: "equipDefense",
    0x2000: "equipSpeed",
    0x1000: "equipEther",
    0x800: "equipEtherDefense",
    0x400: "equip5E",
    0x200: "equip5F",
    0x100: "bodyDefense",
}


def names(table: bytes, threshold: int, chars: dict[int, str]) -> list[str]:
    """Each entry of a text table without its 00, read as text_control --chars renders it."""
    offsets, _ = text_table(table)
    out = []
    for offset in offsets:
        try:
            tokens = decode_text(table, offset, threshold)
        except TextError as error:
            out.append(f"(undecodable: {error})")
            continue
        out.append(render(tuple(t for t in tokens if t.mnemonic != "end"), chars))
    return out


def raisers(items: bytes, accessories: bytes, item_names, accessory_names) -> dict[str, list[str]]:
    """member -> the items and accessories raising it: "item N name +amount" and
    "accessory N name +amount"."""
    out = defaultdict(list)
    for index in range(len(items) // RECORD):
        amount, _, flags, stats = struct.unpack_from("<BBHH", items, index * RECORD + 8)
        if flags & RAISES_STATS:
            for bit, member in ITEM_BITS.items():
                if stats & bit:
                    out[member].append(f"item {index} {item_names[index]!r} +{amount}")
    for index in range(len(accessories) // RECORD):
        (stats,) = struct.unpack_from("<H", accessories, index * RECORD + 0xC)
        for bit, member in ACCESSORY_BITS.items():
            if stats & bit:
                out[member].append(f"accessory {index} {accessory_names[index]!r} +{stats & 0xFF}")
    return out


def disc_raisers(disc: Disc) -> dict[str, list[str]]:
    menu = disc.sectors(disc.slot(*MENU_DATA))
    system = system_data(disc)
    threshold = font_threshold(unpack(disc.sectors(disc.slot(0, 1, 6))))
    chars = character_map(disc, threshold).glyphs
    return raisers(
        archive_entry(menu, ITEMS, packed=True),
        archive_entry(menu, ACCESSORIES, packed=True),
        names(archive_entry(system, ITEM_NAMES), threshold, chars),
        names(archive_entry(system, ACCESSORY_NAMES), threshold, chars),
    )


def report(results: dict[int, dict[str, list[str]]]) -> str:
    lines = [
        "stat items: menu data (0x10, 0) file 2 entries 0 (items, flag 4) and 2 (accessories);"
        " names system texts 22 and 17"
    ]
    for disc, found in sorted(results.items()):
        lines.append(f"  disc {disc}:")
        for member in (*ITEM_BITS.values(), *ACCESSORY_BITS.values()):
            sources = found.get(member, [])
            lines.append(f"    {member}: {len(sources)}")
            lines += [f"      {source}" for source in sources]
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--sweep", action="store_true", help="both discs")
    args = parser.parse_args(argv)
    if not args.sweep:
        parser.error("choose --sweep")
    print(report({disc.number: disc_raisers(disc) for disc in discs()}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
