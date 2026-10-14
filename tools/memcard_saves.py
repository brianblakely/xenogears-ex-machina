#!/usr/bin/env python3
"""Build memory card saves by playing routes on the native runtime.

    python3 tools/memcard_saves.py ROUTES.json --base CARD.mcd --card OUT.mcd
        [--only NAME ...] [--work DIR] [--headless BIN] [--discs DIR]

A route starts from the base card's first Xenogears save with the story
value (vars[0]), place, roster, party and (optionally) maximum stats the route
names, loads it through the title screen's Continue on `xem-headless
--control` and plays on: Cross through dialogue and choices, Circle out of
menus, battles fought with Cross, cutscenes left to run, until the story
value and field control the step names. `place` steps and the final save point re-enter the
field at a map entry through a save and a load, in place of walking there
(events on the way do not run).
The state the game leaves becomes a save as the save menu writes one
(slot39 menu_save_build_payload and menu_copy_game_data_to_save; the event
variables from the field's live bank, which the game copies into game_data
when it leaves the field), at the route's file number, and is loaded once
more to check it reaches field control with the same story value and party.

The runtime is deterministic, so a route gives the same bytes every time.
What the game's scripts do along the route is the game's own state; what
the route sets at its start is not, and scripts check flags that earlier
chapters' events set, so start a route where the game enters the chapter
it saves in (a chapter's opening event, before its own flags are set).

Routes (JSON): {"saves": [{"name", "file" (No.), "disc", "start": {"story",
"map", "entry", "roster", "party", "gears", "set_bits", "max"}, "steps":
[...], "save_point":
{"map", "entry"}, "rest" (default true: HP, EP, gear HP and fuel restored
before the save, as the restoring field events do)}]}. Steps: {"advance": {"story": N, "map": M},
"max_frames": F} (until field control with those values), {"interact":
ACTOR}, {"navigate": [X, Z]} and {"place": {"map", "entry"}}.
"""

import argparse
import hashlib
import json
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "reference"))
import native_scenario as ns  # noqa: E402
from scenario import compile_scenario  # noqa: E402

from tools.analysis.disc_index import discs  # noqa: E402
from tools.analysis.text_control import archive_entry  # noqa: E402

KSEG0 = 0x80000000
BLOCK, FRAME, HEADER = 0x2000, 0x80, 0x100
PREFIX = b"BASLUS-00664"  # slot39 menu_load_card_resources: the prefix on both discs
GAME_DATA_SIZE = 0x2358

# Game data (resident/gamedata.h) and save payload (slot39/file.h) offsets.
CHARACTERS, GEARS, SKILLS = 0x26C, 0x978, 0x16C0
RECORD = 0xA4
VARS, JOINED, PARTY, MAP, ENTRY, FLAG_WORDS = 0x1930, 0x1D30, 0x1D34, 0x231A, 0x231C, 0x2322
SAVE_NAMES, SAVE_CHARACTERS, SAVE_GEARS, SAVE_GEAR = 0x24, 0x290, 0x99C, 0x3C
SAVE_BLOCKS = (
    (0xE4C, 0x1648, 0x78),
    (0xEC4, 0x16C0, 0x160),
    (0x1024, 0x1820, 0x100),
    (0x1124, 0x1920, 0xA38),
)
SAVE_SUM = 0x1EFF  # the payload's eight-bit sum of its first 0x1eff bytes, stored after them
DISC_VARIABLE = 82  # vars[82]: the disc less one (menu_save_build_payload)
SAVE_ENTRY_VARIABLE = 0x2A  # vars[0x2a]: the save point's entry, loads enter there (field.c)

# menu_debug_set_skill_masks (slot39): every character's skills; tier 7.
SKILL_MASKS = (
    (0xFFF8, 0xFF00, 0xFFF0, 0xFE00, 0xE000),
    (0xFFE0, 0xFFF0, 0xFFF0, 0xFFF0, 0xC000),
    (0xFFE0, 0xFFE0, 0xFFF0, 0xFF00, 0x8000),
    (0xFFE0, 0xFFC0, 0xFFF0, 0xFF00, 0xF000),
    (0xFFC0, 0xFFC0, 0xFFF0, 0xFFC0, 0xE000),
    (0xFFC0, 0xF000, 0xFFF0, 0xF000, 0x8000),
    (0xFFC0, 0xFF00, 0xFFF0, 0xFF00, 0x8000),
    (0x0000, 0xFF00, 0x0000, 0xFF00, 0x0000),
    (0x0000, 0xF800, 0xFFF0, 0x0000, 0xE000),
    (0xFFE0, 0xFFE0, 0xFFF0, 0xFFE0, 0x8000),
    (0xFFC0, 0xFF00, 0xFFF0, 0xFF00, 0x8000),
)

# Title screen to a loaded save in card 1's first file: the Kernel MENU's
# Field, Start, Continue, Load, card 1 file 1, yes (the recovered load route).
LOAD = {
    "schema_version": 1,
    "name": "memcard-load",
    "source_profile": "na-slus-00664-39c547a9afc6",
    "entry": {"type": "kernel"},
    "launch": {"settle_frames": 120},
    "steps": [
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 600},
        {"wait_frames": 1200},
        {"buttons": ["start"], "hold_frames": 8, "after_frames": 300},
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 200},
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 120},
        {"wait_frames": 121},
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 90},
        {"wait_frames": 91},
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 90},
        {"buttons": ["left"], "hold_frames": 6, "after_frames": 30},
        {"buttons": ["cross"], "hold_frames": 8, "after_frames": 120},
    ],
}


# Field control counts once the player has walked this many checks (8 frames
# apart) in a row: a field fading in reads as walking before its opening
# event takes control.
STEADY_CHECKS = 16


class RouteError(RuntimeError):
    pass


def schema_addresses():
    names = (
        "game_data",
        "field_event_variables",
        "pad_vblank_count",
        "mode_battle_ai_variables",
        "mode_field_map_id",
        "mode_music_selected_track",
    )
    globals_ = json.loads((ROOT / "build/game/schema.json").read_text())["globals"]
    return {name: globals_[name]["address"] - KSEG0 for name in names}


def title_lines(number):
    """The save title of each story value: line vars[0] of directory (0x10, 1)
    file 1, 30 bytes (slot39 menu_card_init_and_read_title)."""
    disc = next(d for d in discs() if d.number == number)
    raw = disc.data(disc.slot(0x10, 1, 1))
    starts, i = [0], 0
    while i < len(raw):
        if raw[i] >= 0x80:
            i += 2
            continue
        if raw[i] == 0x0A:
            starts.append(i + 1)
        i += 1
    return [raw[s : s + 30] for s in starts]


def experience_to_99(number):
    """The total experience of a character just at level 99 and its experience
    to the next level: the growth file (directory (0x10, 2) file 2, packed
    item 0; ovl2596 GrowthFile) gives each level's experience to the next, and
    a level-up sets that as the record's own (battle_results_add_exp_and_level_up).
    The level-up repeats while it is not positive, so a record at 99 needs it."""
    disc = next(d for d in discs() if d.number == number)
    growth = archive_entry(disc.data(disc.slot(0x10, 2, 2)), 0, packed=True)
    experience = struct.unpack_from("<99i", growth, 0xBB0)
    return sum(experience[:98]), experience[98]


def checksum(block):
    block[HEADER + SAVE_SUM] = sum(block[HEADER : HEADER + SAVE_SUM]) & 0xFF


def set_title(block, digit, line):
    """slot39 menu_save_build_header_title: "XENOGEARS/No." in full width,
    the file number as two digits, a space and the story value's line."""
    head = (
        "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．".encode("shift_jis")
        + bytes([0x82, (digit + 1) // 10 + 0x4F, 0x82, (digit + 1) % 10 + 0x4F, 0x81, 0x40])
        + line
    )
    title = bytearray(0x5C)
    title[: len(head)] = head
    block[4 : 4 + 0x5C] = title


def summarize(block, characters, party, play_time, digit):
    """The SaveSummary (slot39/file.h) the file screen shows."""
    p = HEADER
    struct.pack_into("<i", block, p, play_time)
    for slot, who in enumerate(party):
        if who == 0xFF:
            block[p + 0x1C + slot] = 0xFF
            continue
        record = characters[who * RECORD : (who + 1) * RECORD]
        hp, max_hp, ep, max_ep = struct.unpack_from("<4H", record, 0x4C)
        struct.pack_into("<H", block, p + 4 + 2 * slot, hp)
        struct.pack_into("<H", block, p + 0xA + 2 * slot, max_hp)
        block[p + 0x10 + slot] = ep & 0xFF
        block[p + 0x13 + slot] = max_ep & 0xFF
        block[p + 0x16 + slot] = record[0x62]
        block[p + 0x19 + slot] = record[0x63]
        block[p + 0x1C + slot] = who
    block[p + 0x1F] = 0
    block[p + 0x23] = digit


def first_save(card):
    for frame in range(1, 16):
        entry = card[frame * FRAME : (frame + 1) * FRAME]
        if entry[0] == 0x51 and entry[10:22] == PREFIX:
            return bytes(card[frame * BLOCK : (frame + 1) * BLOCK])
    raise RouteError("no Xenogears save on the base card")


def maximize(block, experience):
    """Every character at the growth caps (ovl2596 battle_results_grow_*: level
    99, HP 999, EP 99, stats 200, hit and evade 99) with the experience of
    level 99 and every skill (slot39 menu_debug_set_skill_masks)."""
    p = HEADER
    for i in range(11):
        r = p + SAVE_CHARACTERS + i * RECORD
        total, following = experience
        struct.pack_into("<4I", block, r + 0x3C, total, total, following, following)
        struct.pack_into("<4H", block, r + 0x4C, 999, 999, 99, 99)
        for offset in (0x58, 0x59, 0x5B, 0x5C):
            block[r + offset] = 200
        block[r + 0x5E] = block[r + 0x5F] = 99
        block[r + 0x62] = block[r + 0x63] = 99
    for i, (counter, level, a, b, fuel) in enumerate(SKILL_MASKS):
        s = p + 0xEC4 + i * 0x20
        struct.pack_into("<4H", block, s, counter, level, a, b)
        block[s + 0x17] = 7
        struct.pack_into("<H", block, s + 0x1A, fuel)


def start_block(base, start, disc, digit, titles, experience):
    """The base save with the route's start: story value, place, disc, roster,
    party, the gears the story has given ("gears": character to gear, the
    CharacterRecord +0xa0 field event fe a1 sets), flags an event the route
    starts after would have set ("set_bits": [variable, mask] pairs) and, with
    "max", every
    character at the growth caps
    (ovl2596 battle_results_grow_*: level 99, HP 999, EP 99, stats 200, hit and
    evade 99) with the experience of level 99 and every skill."""
    block = bytearray(base)
    p = HEADER
    data = p + 0x1124 - 0x1920  # game data from +0x1920 in the save

    def put(offset, value):
        struct.pack_into("<H", block, data + offset, value)

    if start.get("max"):
        maximize(block, experience)
    put(VARS, start["story"])
    put(VARS + 2 * 1, start["entry"])
    put(VARS + 2 * 2, start["map"])
    put(VARS + 2 * 0x1E, start["map"])
    put(VARS + 2 * SAVE_ENTRY_VARIABLE, start["entry"])
    put(VARS + 2 * DISC_VARIABLE, disc - 1)
    put(MAP, start["map"])
    put(ENTRY + 4, start["entry"])
    for index, mask in start.get("set_bits", []):
        variable = data + VARS + 2 * index
        struct.pack_into("<H", block, variable, struct.unpack_from("<H", block, variable)[0] | mask)
    for character, gear in start.get("gears", {}).items():
        block[p + SAVE_CHARACTERS + int(character) * RECORD + 0xA0] = gear
    put(JOINED, sum(1 << c for c in start["roster"]))
    party = list(start["party"]) + [0xFF] * (3 - len(start["party"]))
    block[data + PARTY : data + PARTY + 3] = bytes(party)
    characters = block[p + SAVE_CHARACTERS : p + SAVE_CHARACTERS + 11 * RECORD]
    summarize(block, characters, party, struct.unpack_from("<i", block, p)[0], digit)
    set_title(block, digit, titles[start["story"]])
    checksum(block)
    return bytes(block)


def rest(data):
    """What the restoring field events do (restore_all_hp, restore_all_ep,
    restore_gears): every character's HP and EP and every gear's HP and fuel
    at their maximum."""
    for i in range(11):
        r = CHARACTERS + i * RECORD
        data[r + 0x4C : r + 0x4E] = data[r + 0x4E : r + 0x50]
        data[r + 0x50 : r + 0x52] = data[r + 0x52 : r + 0x54]
    for i in range(20):
        g = GEARS + i * RECORD
        data[g + 0x60 : g + 0x64] = data[g + 0x64 : g + 0x68]
        data[g + 0x38 : g + 0x3A] = data[g + 0x3A : g + 0x3C]


def ram_block(ram, addresses, template, map_id, entry, disc, digit, titles, rested=False):
    """The save the menu would write from RAM, entered at `map_id`'s save point
    `entry`: menu_save_build_payload (play time, party summary, flag words 1-16
    from the battle AI variables, vars[82] the disc) and
    menu_copy_game_data_to_save, with the field's live variables as
    field_event_save_map_and_variables copies them. The names come from
    `template` (the menu encodes them; RAM holds them decoded)."""
    data = bytearray(ram[addresses["game_data"] : addresses["game_data"] + GAME_DATA_SIZE])
    data[VARS : VARS + 0x400] = ram[
        addresses["field_event_variables"] : addresses["field_event_variables"] + 0x400
    ]
    data[FLAG_WORDS + 2 : FLAG_WORDS + 34] = ram[
        addresses["mode_battle_ai_variables"] : addresses["mode_battle_ai_variables"] + 32
    ]
    data[FLAG_WORDS : FLAG_WORDS + 2] = ram[
        addresses["mode_music_selected_track"] : addresses["mode_music_selected_track"] + 2
    ]
    vars4 = struct.unpack_from("<H", data, VARS + 8)[0]
    struct.pack_into("<H", data, ENTRY, (vars4 << 9) & 0xFFFF)
    for index, value in (
        (1, entry),
        (0x1E, map_id),
        (SAVE_ENTRY_VARIABLE, entry),
        (DISC_VARIABLE, disc - 1),
    ):
        struct.pack_into("<H", data, VARS + 2 * index, value)
    struct.pack_into("<H", data, MAP, map_id)
    struct.pack_into("<H", data, ENTRY + 4, entry)
    if rested:
        rest(data)

    block = bytearray(BLOCK)
    block[:HEADER] = template[:HEADER]
    p = HEADER
    block[p + SAVE_NAMES : p + SAVE_CHARACTERS] = template[p + SAVE_NAMES : p + SAVE_CHARACTERS]
    block[p + SAVE_CHARACTERS : p + SAVE_CHARACTERS + 11 * RECORD] = data[
        CHARACTERS : CHARACTERS + 11 * RECORD
    ]
    for i in range(20):
        gear = data[GEARS + i * RECORD : GEARS + (i + 1) * RECORD]
        g = p + SAVE_GEARS + i * SAVE_GEAR
        block[g : g + 0x10] = gear[0:0x10]
        block[g + 0x10 : g + 0x28] = gear[0x10:0x28]
        block[g + 0x28 : g + 0x2C] = gear[0x5C:0x60]
        block[g + 0x2C : g + 0x30] = gear[0x60:0x64]
        block[g + 0x34 : g + 0x36] = gear[0x38:0x3A]
        block[g + 0x38] = gear[0x99]
        block[g + 0x39] = gear[0x74]
        block[g + 0x3A] = gear[0x75]
    for save, game, length in SAVE_BLOCKS:
        block[p + save : p + save + length] = data[game : game + length]
    party = list(data[PARTY : PARTY + 3])
    play_time = struct.unpack_from("<i", ram, addresses["pad_vblank_count"])[0]
    summarize(block, data[CHARACTERS : CHARACTERS + 11 * RECORD], party, play_time, digit)
    set_title(block, digit, titles[struct.unpack_from("<H", data, VARS)[0]])
    checksum(block)
    return bytes(block)


def directory_entry(name):
    entry = bytearray(FRAME)
    struct.pack_into("<IIH", entry, 0, 0x51, BLOCK, 0xFFFF)
    entry[10 : 10 + len(name)] = name
    entry[FRAME - 1] = 0
    for b in entry[: FRAME - 1]:
        entry[FRAME - 1] ^= b
    return bytes(entry)


def put_save(card, digit, block):
    """Write `block` as file BASLUS-00664 + digit: over that file, else in the
    first free block."""
    name = PREFIX + bytes([0x30 + digit])
    frames = [
        f
        for f in range(1, 16)
        if card[f * FRAME] == 0x51 and card[f * FRAME + 10 : f * FRAME + 23] == name
    ]
    if not frames:
        frames = [f for f in range(1, 16) if card[f * FRAME] == 0xA0]
        if not frames:
            raise RouteError("the card has no free block")
    frame = frames[0]
    card[frame * FRAME : (frame + 1) * FRAME] = directory_entry(name)
    card[frame * BLOCK : (frame + 1) * BLOCK] = block


def free_entry():
    entry = bytearray(FRAME)
    struct.pack_into("<IIH", entry, 0, 0xA0, 0, 0xFFFF)
    for b in entry[: FRAME - 1]:
        entry[FRAME - 1] ^= b
    return bytes(entry)


def single_save_card(blank, block):
    """`blank` (a card with every file freed) holding `block` as file 1."""
    card = bytearray(blank)
    put_save(card, 0, block)
    return bytes(card)


class Session:
    """xem-headless --control with a card holding one save, loaded through the
    title screen."""

    def __init__(self, args, disc, block, work):
        cards = work / "cards"
        cards.mkdir(parents=True, exist_ok=True)
        (cards / "card1.mcd").write_bytes(single_save_card(args.blank, block))
        disc_path = args.discs / f"Xenogears disc {disc}.chd"
        self.headless = ns.Headless(
            [str(args.headless), "--disc", str(disc_path), "--control", "--cards", str(cards)]
        )
        self.work = work
        availability = json.loads((ROOT / "analysis/coverage/field-pairs.json").read_text())
        program, _ = compile_scenario(LOAD, availability)
        ns.run(program, self.headless, work / "load")

    def close(self):
        self.headless.close()

    def context(self):
        """The interface's context (xem_core::context): its kind and fields."""
        reply = self.headless({"cmd": "context"})
        return reply["kind"], reply

    def run(self, count):
        self.headless({"cmd": "frames", "count": count})

    def press(self, buttons, frames, after):
        self.headless({"cmd": "press", "buttons": buttons, "frames": frames})
        self.run(frames + after)

    def wait_assist(self, max_frames):
        """Run until the assisted walk a navigate or interact started ends."""
        for _ in range(0, max_frames, 8):
            status = self.headless({"cmd": "frames", "count": 8})
            if status.get("assist") is None:
                outcome = status.get("assist_outcome")
                # Interrupted: the interface changed (an event the walk set off).
                if outcome not in ("arrived", "confirmed", "interrupted"):
                    raise RouteError(f"the assisted walk ended: {outcome}")
                return
        raise RouteError(f"the assisted walk did not end in {max_frames} frames")

    def u16(self, offset):
        return struct.unpack("<H", self.headless.read(offset, 2))[0]

    def screenshot(self, name):
        self.headless({"cmd": "screenshot", "path": str(self.work / f"{name}.png")})


def advance(session, addresses, until, max_frames, log):
    """Play until the player walks in a field with the story value and map
    `until` names: Cross through dialogue and choices, Circle out of menus,
    battles fought with Cross, events left to run (Cross when their scripts
    read the pad)."""
    start = session.headless.frames
    last_story, last_kind = None, None
    steady = 0
    menu_choice = answered = False
    while session.headless.frames - start < max_frames:
        kind, detail = session.context()
        if kind != last_kind:
            log.append({"frames": session.headless.frames, "context": kind})
            last_kind = kind
        if kind != "walk":
            steady = 0
        if kind == "menu" and answered:
            menu_choice = True  # the choice just made opened a menu
        if kind != "dialogue":
            answered = False
        if kind in ("walk", "event", "dialogue"):
            story = session.u16(addresses["field_event_variables"])
            if story != last_story:
                log.append(
                    {
                        "frames": session.headless.frames,
                        "story": story,
                        "map": session.u16(addresses["mode_field_map_id"]) & 0xFFF,
                    }
                )
                last_story = story
        if kind == "walk":
            story = session.u16(addresses["field_event_variables"])
            field_map = session.u16(addresses["mode_field_map_id"]) & 0xFFF
            if story == until.get("story", story) and field_map == until.get("map", field_map):
                steady += 1
                if steady >= STEADY_CHECKS:
                    log.append(
                        {
                            "frames": session.headless.frames,
                            "stop": True,
                            "story": story,
                            "map": field_map,
                        }
                    )
                    return
            else:
                steady = 0
            session.run(8)
            continue
        elif kind in ("battle", "arena"):
            session.press(["cross"], 8, 56)
        elif kind == "dialogue":
            choice = detail.get("choice")
            if choice and menu_choice and choice["index"] != choice["count"] - 1:
                # A choice that opened a menu ("Open menu / Member change /
                # Resume game"): take its last option.
                session.press(["down"], 4, 8)
                continue
            answered = bool(choice)
            session.press(["cross"], 4, 16)
        elif kind == "menu":  # a save point's or a shop's menu: leave it
            session.press(["circle"], 4, 16)
        elif kind == "event" and detail.get("buttons"):
            session.press(["cross"], 4, 28)
        else:
            session.run(8)
    session.screenshot("stuck")
    story = session.u16(addresses["field_event_variables"])
    field_map = session.u16(addresses["mode_field_map_id"]) & 0xFFF
    raise RouteError(
        f"not at {until} with field control after {max_frames} frames "
        f"(context {kind}, story {story}, map {field_map}; "
        f"see {session.work / 'stuck.png'})"
    )


def play(args, route, base, addresses, titles):
    work = args.work / route["name"]
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    disc, digit = route["disc"], route["file"] - 1
    titles = titles[disc]
    block = start_block(base, route["start"], disc, digit, titles, experience_to_99(disc))
    template = block
    log = []
    session = Session(args, disc, block, work / "0")
    try:
        return finish(args, route, session, addresses, template, disc, digit, titles, log, work)
    finally:
        (work / "route-log.json").write_text(json.dumps(log, indent=1) + "\n")


def finish(args, route, session, addresses, template, disc, digit, titles, log, work):
    try:
        for number, step in enumerate(route.get("steps", []), 1):
            if "advance" in step:
                advance(session, addresses, step["advance"], step.get("max_frames", 30000), log)
            elif "interact" in step:
                session.headless({"cmd": "interact", "actor": step["interact"]})
                session.wait_assist(step.get("max_frames", 30000))
                advance(
                    session, addresses, step.get("until", {}), step.get("max_frames", 30000), log
                )
            elif "navigate" in step:
                session.headless({"cmd": "navigate", "to": step["navigate"]})
                session.wait_assist(step.get("max_frames", 30000))
                advance(
                    session, addresses, step.get("until", {}), step.get("max_frames", 30000), log
                )
            elif "place" in step:
                ram = session.headless.read(0, ns.RAM)
                block = ram_block(
                    ram,
                    addresses,
                    template,
                    step["place"]["map"],
                    step["place"]["entry"],
                    disc,
                    digit,
                    titles,
                )
                session.close()
                session = Session(args, disc, block, work / str(number))
                advance(
                    session, addresses, step.get("until", {}), step.get("max_frames", 6000), log
                )
            else:
                raise RouteError(f"unknown step {step}")
        session.screenshot("end")
        ram = session.headless.read(0, ns.RAM)
    finally:
        session.close()
    point = route["save_point"]
    block = ram_block(
        ram,
        addresses,
        template,
        point["map"],
        point["entry"],
        disc,
        digit,
        titles,
        route.get("rest", True),
    )
    story = struct.unpack_from("<H", block, HEADER + 0x1124 + VARS - 0x1920)[0]
    party = list(block[HEADER + 0x1124 + PARTY - 0x1920 :][:3])
    # Check: the save loads into field control, unchanged.
    check = Session(args, disc, block, work / "check")
    try:
        advance(check, addresses, {"story": story, "map": point["map"]}, 30000, [])
        check.screenshot("loaded")
        loaded_party = list(check.headless.read(addresses["game_data"] + PARTY, 3))
    finally:
        check.close()
    if loaded_party != party:
        raise RouteError(f"loaded party {loaded_party}, saved {party}")
    return block, story, party


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("routes", type=Path)
    parser.add_argument(
        "--base",
        type=Path,
        required=True,
        help="the card whose first Xenogears save routes start from",
    )
    parser.add_argument(
        "--card",
        type=Path,
        required=True,
        help="the card image to write the saves into (new: the base save alone)",
    )
    parser.add_argument("--only", nargs="*", help="route names to build")
    parser.add_argument("--work", type=Path, default=ROOT / ".local" / "memcard-saves")
    parser.add_argument(
        "--headless", type=Path, default=ROOT / "src/runtime/target/release/xem-headless"
    )
    parser.add_argument("--discs", type=Path, default=ROOT / "discs")
    args = parser.parse_args()

    routes = json.loads(args.routes.read_text())["saves"]
    if args.only:
        routes = [r for r in routes if r["name"] in args.only]
    base_card = bytearray(args.base.read_bytes())
    base = first_save(base_card)
    for frame in range(1, 16):
        base_card[frame * FRAME : (frame + 1) * FRAME] = free_entry()
    args.blank = bytes(base_card)
    addresses = schema_addresses()
    titles = {number: title_lines(number) for number in (1, 2)}
    built = []
    for route in routes:
        block, story, party = play(args, route, base, addresses, titles)
        built.append((route, block))
        point = route["save_point"]["map"]
        print(
            f"No.{route['file']:02d} {route['name']}: story {story}, map {point}, "
            f"party {party}, sha256 {hashlib.sha256(block).hexdigest()[:16]}"
        )
    card = bytearray(
        args.card.read_bytes() if args.card.exists() else single_save_card(args.blank, base)
    )
    for route, block in built:
        put_save(card, route["file"] - 1, block)
    args.card.write_bytes(card)
    print(f"wrote {len(built)} saves to {args.card}")


if __name__ == "__main__":
    main()
