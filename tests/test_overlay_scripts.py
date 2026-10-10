"""Synthetic scripts exercise the recovered overlay script decoders."""

import re
import struct
import unittest
from collections import Counter

from tools.analysis.events import EXTENDED
from tools.analysis.overlay_scripts import (
    ARENA,
    BASE,
    DIRECTORS,
    EVENT_KINDS,
    EVENT_TABLE,
    FRAME_EVENT,
    HIT_SPEC,
    MOVIE_SOUND_DIRECTORY,
    MOVIE_SOUND_END,
    MOVIE_SOUND_ENTRIES,
    MOVIE_SOUND_FILE,
    MOVIE_SOUND_REQUEST,
    MOVIE_SOUNDS,
    ROOT,
    SPARKLE_SIZES,
    TEXTURE_SLOT_BYTES,
    TEXTURE_SLOTS,
    WORLDMAP,
    WORLDMAP_HANDLER_ADDRESSES,
    EventSweep,
    MovieSound,
    ScriptError,
    UnknownKind,
    UnknownOpcode,
    decode,
    disassemble,
    effect_form,
    event_sweep_file,
    expected_sha256,
    frame_events,
    hit_form,
    loaded,
    model_file,
    movie_sound_bank_file,
    movie_sound_banks,
    movie_sound_run,
    movie_sound_seek,
    movie_sound_step,
    movie_sound_sweep,
    run,
    scene_sweep,
    sequence_tables,
    spans_text,
    sweep,
    texture_frames,
    texture_slots,
    texture_sweep,
    texture_uploads,
    unpack,
)


def halfwords(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}h", *values)


def image(size: int, placed: dict[int, bytes]) -> bytearray:
    """A mode overlay image of `size` bytes holding each value at its address."""
    data = bytearray(size)
    for address, raw in placed.items():
        offset = address - BASE
        data[offset : offset + len(raw)] = raw
    return data


def handler_table() -> bytes:
    return b"".join(address.to_bytes(4, "little") for address in WORLDMAP_HANDLER_ADDRESSES)


class WorldmapScriptTests(unittest.TestCase):
    def test_lengths_signed_operands_and_stop(self):
        data = halfwords(1, 10, 2, 0, 13, 0, 3, 4985, -288, 19242, 9, 40, 0, 0)
        listing = disassemble(WORLDMAP, data, BASE)
        self.assertEqual([i.opcode for i in listing], [1, 2, 3, 9, 0])
        self.assertEqual([i.size for i in listing], [4, 8, 8, 4, 4])
        self.assertEqual([i.address - BASE for i in listing], [0, 4, 12, 20, 24])
        self.assertEqual(listing[2].operands, (4985, -288, 19242))
        self.assertEqual(listing[1].text(), "send_actor actor=0, argument=13")
        self.assertEqual(listing[-1].spec.flow, "stop")

    def test_unused_halfwords_are_read_and_shown_when_set(self):
        instruction = decode(WORLDMAP, halfwords(11, 2, 8, 7), BASE)
        self.assertEqual(instruction.text(), "set_fade rate=2, step=8, unused=7")

    def test_opcode_is_the_unsigned_low_halfword_of_the_word(self):
        with self.assertRaises(UnknownOpcode) as caught:
            decode(WORLDMAP, halfwords(-1, 0), BASE)
        self.assertEqual(caught.exception.opcode, 0xFFFF)

    def test_unknown_opcode_misalignment_and_truncation_fail(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(WORLDMAP, halfwords(1, 5, 12, 0), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 4, 12))
        with self.assertRaises(ScriptError):
            disassemble(WORLDMAP, halfwords(3, 1, 2), BASE)
        with self.assertRaises(ScriptError):
            decode(WORLDMAP, halfwords(0, 0, 0, 0), BASE + 2)

    def test_sweep_checks_the_dispatch_table_and_reads_both_scripts(self):
        first = halfwords(1, 60, 5, 44, 8, 0, 240, 0, 0, 0)
        second = halfwords(12, 0)
        data = image(0x2C0C6, {0x8009A3C0: handler_table(), 0x8009A758: first, 0x8009AC60: second})
        result = sweep(WORLDMAP, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (2, 4))
        self.assertEqual(result.uses, {1: 1, 5: 1, 8: 1, 0: 1})
        self.assertEqual(result.failures[0][:2], ("worldmap_scene18_script_start", 0x8009AC60))
        data[0x8009A3C0 - BASE] ^= 4
        with self.assertRaises(ScriptError):
            sweep(WORLDMAP, bytes(data))


class ArenaScriptTests(unittest.TestCase):
    def test_one_to_three_byte_commands_until_end(self):
        data = bytes([0x02, 0x12, 0x2B, 0x15, 0x07, 0x40, 0x1A, 0x01, 0xFF, 0x00, 0x99])
        listing = disassemble(ARENA, data, BASE)
        self.assertEqual([i.size for i in listing], [1, 2, 3, 1, 2, 1])
        self.assertEqual(listing[2].text(), "show_marker x/2=7, y=64")
        self.assertEqual(listing[4].operands, (255,))
        self.assertEqual(listing[-1].spec.mnemonic, "end")

    def test_values_without_a_case_fail_and_spinning_cases_end_the_listing(self):
        with self.assertRaises(UnknownOpcode) as caught:
            disassemble(ARENA, bytes([0x02, 0x23]), BASE)
        self.assertEqual((caught.exception.address, caught.exception.opcode), (BASE + 1, 0x23))
        for code in (16, 17):
            self.assertEqual(disassemble(ARENA, bytes([code, 0x12]), BASE)[-1].spec.flow, "hang")
        with self.assertRaises(ScriptError):
            disassemble(ARENA, bytes([0x15, 0x07]), BASE)

    def test_sweep_follows_the_scene_table_and_direct_starts(self):
        scene = bytes([0x03, 0x0E, 0x2D, 0x19, 0x00])
        table = struct.pack("<10I", *([0x80090F40] * 9 + [0x80090F48]))
        data = image(0x22E69, {0x8009105C: table, 0x80090F40: scene, 0x80090F48: b"\x24"})
        result = sweep(ARENA, bytes(data))
        self.assertEqual((result.scripts, result.instructions), (4, 6))
        self.assertEqual(result.uses, {0: 3, 3: 1, 14: 1, 25: 1})
        self.assertEqual(result.failures[0][:2], ("arena_scene_scripts[9]", 0x80090F48))


def director(name: str):
    return next(d for d in DIRECTORS if d.interpreter == name)


def u16s(*values: int) -> bytes:
    return struct.pack(f"<{len(values)}H", *values)


class SceneDirectorTests(unittest.TestCase):
    def test_a_starter_that_does_not_step_fetches_entry_0_twice(self):
        flight = director("worldmap_scene13_director_update")
        outcome = run(flight, [1, 2, 0x40], [3, 5, 0])
        self.assertEqual(
            [(s.index, s.state, s.wait, s.frame) for s in outcome.steps],
            [(0, 1, 3, 1), (0, 1, 3, 5), (1, 2, 5, 9), (2, 0x40, 0, 16)],
        )
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("exit", 16, 3))

    def test_a_stepping_starter_runs_entry_0_on_the_first_update(self):
        vehicle = director("worldmap_scene12_director_update")
        outcome = run(vehicle, [2, 0x40], [10, 0])
        self.assertEqual([(s.index, s.frame) for s in outcome.steps], [(0, 1), (1, 13)])
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("exit", 13, 2))
        self.assertEqual(vehicle.cues[0x40].state, 1)  # a further update would fetch entry 2

    def test_states_without_a_case_idle_and_fetches_past_the_table_stop(self):
        vehicle = director("worldmap_scene12_director_update")
        self.assertEqual(run(vehicle, [1, 5], [0, 0]).stop, "no case")
        self.assertEqual(run(vehicle, [1, 0], [0, 0]).stop, "idle")
        outcome = run(director("worldmap_scene14_director_update"), [1], [0])
        self.assertEqual((outcome.stop, outcome.frame, outcome.next), ("past table", 2, 1))

    def test_waits_are_signed_like_the_actor_field(self):
        outcome = run(director("worldmap_scene16_director_update"), [1, 0x40], [0xFFFF, 0])
        self.assertEqual([(s.index, s.frame) for s in outcome.steps], [(0, 1), (1, 2)])

    def test_cues_name_their_actions(self):
        cues = director("worldmap_scene12_director_update").cues
        self.assertEqual(cues[0x11].mnemonic, "requests+fade_out+sounds")
        self.assertEqual(cues[0x40].flow, "exit")
        self.assertEqual(cues[1].flow, "wait")
        self.assertEqual(cues[0].flow, "idle")
        self.assertEqual(cues[2].text(), "slot 2 request 1, sound 0xd, sound 0xe, sound 0xf")
        slots = director("worldmap_scene12_director_update").slots
        self.assertEqual(
            cues[0x16].text(slots),
            "slot 0 (worldmap_screen_fade_update) request 13, fade_rate 0x2, fade_step 0x4",
        )
        self.assertEqual(director("worldmap_scene13_director_update").cues[0x40].text(), "exit_worldmap, state 0")

    def test_the_sweep_reads_every_sequence_and_checks_the_picker(self):
        placed = {}
        for each in DIRECTORS:
            for sequence in each.sequences:
                states = [1] * (sequence.entries - 1) + [0x40 if 0x40 in each.cues else 11]
                placed[sequence.states] = u16s(*states)
                placed[sequence.durations] = u16s(*[0] * sequence.entries)
        scene = director("worldmap_scene15_director_update")
        pointers = [a for s in scene.sequences for a in (s.states, s.durations)]
        placed[0x8009A65C] = struct.pack("<6I", *pointers)
        data = image(0x2AC70, placed)
        result = scene_sweep(bytes(data))
        self.assertEqual((result.sequences, len(result.failures)), (7, 0))
        self.assertEqual(
            result.instructions, sum(s.entries for d in DIRECTORS for s in d.sequences)
        )
        data[0x8009A65C - BASE] ^= 4
        with self.assertRaises(ScriptError):
            sequence_tables(scene, bytes(data))


END = (MOVIE_SOUND_END, 0)


def movie_table(*runs: list) -> list[tuple[int, int]]:
    """A timeline table: a leading end, then each run and its end."""
    table = [END]
    for each in runs:
        table += [*each, END]
    return table


def movie_bytes(*runs: list) -> bytes:
    """field_movie_sound_timelines holding movie_table(*runs), the last run repeating its last
    entry to fill the table's 96 entries."""
    table = movie_table(*runs)
    filler = [table[-2]] * (MOVIE_SOUND_ENTRIES - len(table))
    table = table[:-1] + filler + [END]
    return b"".join(struct.pack("<HH", frame, sound) for frame, sound in table)


def seds(effects: int, size: int = 0x40) -> bytes:
    """An effect bank sound_check_file accepts: magic, version 0x101 at +0xC,
    the effect count at +0x12, and a last word that zeroes the word sum."""
    data = bytearray(size)
    data[:4] = b"seds"
    struct.pack_into("<IHxxxxH", data, 8, size, 0x101, effects)
    struct.pack_into("<H", data, 0x18, 0x20 + 4 * effects)
    total = sum(struct.unpack_from(f"<{size // 4}I", data))
    struct.pack_into("<I", data, size - 4, -total & 0xFFFFFFFF)
    return bytes(data)


class BankFiles:
    """Directory (0x1C, 0) of a disc index whose slots are the file numbers."""

    def __init__(self, banks: dict):
        self.blobs = {MOVIE_SOUND_FILE + bank: blob for bank, blob in banks.items()}
        self.entries = {slot: {"size": len(blob)} for slot, blob in self.blobs.items()}

    def slot(self, group: int, index: int, file: int) -> int:
        assert (group, index) == MOVIE_SOUND_DIRECTORY
        return file

    def data(self, slot: int) -> bytes:
        return self.blobs[slot]


class MovieSoundTests(unittest.TestCase):
    def test_a_bank_seeks_past_bank_plus_one_ends(self):
        table = movie_table([(1, 0x301), (41, 0xB)], [], [(0, 0x202)])
        self.assertEqual(movie_sound_banks(table), 3)
        self.assertEqual([movie_sound_seek(table, bank) for bank in range(3)], [1, 4, 5])
        self.assertEqual(
            [(s.frame, s.effect, s.pair) for s in movie_sound_run(table, 0)],
            [(1, 1, 3), (41, 0xB, 0)],
        )
        self.assertEqual(movie_sound_run(table, 1), [])
        self.assertEqual(movie_sound_run(table, 2)[0].index, 5)
        self.assertEqual(movie_sound_seek(table, 3), len(table))  # past the last end
        with self.assertRaises(ScriptError):
            movie_sound_seek(table, 4)
        with self.assertRaises(ScriptError):
            movie_sound_run(table, 3)
        with self.assertRaises(ScriptError):
            movie_sound_run(table[:-1], 2)

    def test_a_sound_holds_the_effect_and_the_voice_pair(self):
        sound = MovieSound(0, 360, 0x70D)
        self.assertEqual((sound.effect, sound.pair, sound.unread), (0xD, 7, 0))
        high = MovieSound(0, 0, 0xF80D)
        self.assertEqual((high.pair, high.unread), (0, 0x1F))
        self.assertEqual(sound.text(), "frame  360: effect 0x0d voice pair 7")

    def test_a_step_plays_every_entry_whose_frame_has_come(self):
        table = movie_table([(1, 0x301), (1, 0x202), (60, 0x11)])
        played, position = movie_sound_step(table, movie_sound_seek(table, 0), 3, 2)
        self.assertEqual(([s.index for s in played], position), ([1, 2], 3))
        self.assertEqual(movie_sound_step(table, position, 61, 2), ([], 3))  # 61 < 60 + 2
        played, position = movie_sound_step(table, position, 62, 2)
        self.assertEqual(([s.effect for s in played], position), ([0x11], 4))
        self.assertEqual(movie_sound_step(table, position, 4000, 2), ([], 4))  # the end waits
        with self.assertRaises(ScriptError):
            movie_sound_step(table, 5, 0, 0)

    def test_the_sweep_checks_each_bank_file_and_the_requested_banks(self):
        table = movie_bytes([(1, 0x301), (5, 0x202)], [(0, 0x101)])
        data = bytes(image(0x3FAFE, {0x800AE060: table}))
        files = BankFiles({0: seds(3), 1: seds(2)})
        requests = Counter({0: 1, 1: 2, 0xFF: 3, "variable": 1})
        result = movie_sound_sweep(data, files, requests)
        self.assertEqual((result.banks, result.entries, result.failures), (2, 2 + 91, []))
        self.assertEqual(result.files, [(0, 0x115, 3, [0], []), (1, 0x116, 2, [0], [1])])
        self.assertEqual(result.pairs, Counter({3: 1, 2: 1, 1: 91}))
        self.assertEqual(movie_sound_bank_file(files, 1), (0x116, 2))
        result = movie_sound_sweep(data, BankFiles({0: seds(2)}), requests + Counter({2: 1}))
        self.assertEqual(
            [where for where, _ in result.failures], ["bank 0", "bank 1", "bank 2"]
        )  # effect 2 past 2 effects, no file, a request without a run
        broken = bytearray(seds(3))
        broken[0x20] ^= 1
        with self.assertRaises(ScriptError):
            movie_sound_bank_file(BankFiles({0: bytes(broken)}), 0)
        tail = bytearray(data)
        tail[MOVIE_SOUNDS - BASE + 4 * (MOVIE_SOUND_ENTRIES - 1)] = 1
        self.assertEqual(movie_sound_sweep(bytes(tail)).failures[0][0], "table")


def slot_rows(*rows: tuple) -> bytes:
    """TexAnimSlot rows (rect, the s32, frames address)."""
    return b"".join(struct.pack("<4hiI", *rect, value, frames) for rect, value, frames in rows)


def frames_bytes(*frames: tuple[int, int]) -> bytes:
    return b"".join(struct.pack("<2h", picture, duration) for picture, duration in frames)


def texture_image(first: bytes, second: bytes) -> bytes:
    """Both slot tables pointing at the runs `first` (at 8009a1a0) and
    `second` (at 8009a208)."""
    rows = [((0xF8, 0x1B0 + 0x20 * i, 8, 1), i, 0x8009A1A0) for i in range(2)]
    more = [((0x280 + 0x20 * i, 0xC0, 0x10, 0x20), i, 0x8009A208) for i in range(3)]
    return bytes(
        image(
            0x2C0C6,
            {
                0x8009A1A0: first,
                0x8009A1E8: slot_rows(*rows),
                0x8009A208: second,
                0x8009A250: slot_rows(*more),
            },
        )
    )


class TextureAnimationTests(unittest.TestCase):
    def test_slots_point_at_runs_that_end_at_a_negative_duration(self):
        data = texture_image(frames_bytes((0, 5), (1, 5), (0, -1)), frames_bytes((3, 8), (0, -1)))
        slots = texture_slots(data)
        self.assertEqual(
            [(s.table, s.index, s.stepper) for s in slots],
            [("worldmap_texture_anim_slots", i, "worldmap_texture_anim_advance") for i in range(2)]
            + [("worldmap_texture_anim2_slots", i, "worldmap_texture_anim2_advance") for i in range(3)],
        )
        self.assertEqual((slots[1].rect, slots[1].frames), ((0xF8, 0x1D0, 8, 1), 0x8009A1A0))
        self.assertEqual(texture_frames(data, slots[0].frames), [(0, 5), (1, 5), (0, -1)])
        result = texture_sweep(data)
        self.assertEqual((result.slots, result.frames, result.failures), (5, 2 * 2 + 3, []))
        self.assertEqual(result.cycles[0], ("worldmap_texture_anim_slots[0]", 10))
        self.assertEqual(result.cycles[-1], ("worldmap_texture_anim2_slots[2]", 8))

    def test_the_stepper_shows_frame_1_first_and_restarts_at_frame_0(self):
        run = [(0, 5), (1, 5), (2, 3), (0, -1)]
        self.assertEqual(
            texture_uploads(run, 20), [(1, 1, 1), (6, 2, 2), (9, 0, 0), (14, 1, 1), (19, 2, 2)]
        )

    def test_runs_without_an_end_or_a_restart_fail(self):
        good = frames_bytes((3, 8), (0, -1))
        empty = texture_sweep(texture_image(frames_bytes((0, -1)), good))  # nothing to restart at
        self.assertEqual([where for where, _ in empty.failures], ["worldmap_texture_anim_slots[0]", "worldmap_texture_anim_slots[1]"])
        zero = texture_sweep(texture_image(good, frames_bytes((0, 4), (1, 0), (0, -1))))
        self.assertEqual(
            [where for where, _ in zero.failures], [f"worldmap_texture_anim2_slots[{i}]" for i in range(3)]
        )
        data = bytearray(texture_image(good, good))
        struct.pack_into("<I", data, 0x8009A250 + 12 - BASE, BASE + len(data) - 2)
        # no end
        self.assertEqual(texture_sweep(bytes(data)).failures[0][0], "worldmap_texture_anim2_slots[0]")
        with self.assertRaises(ScriptError):
            texture_frames(bytes(8), BASE + 4)


def frame_event(first: int, last: int, spec: int) -> bytes:
    return struct.pack("<BBh", first, last, spec)


def hit_spec(kind: int, type_: int = 0, a: int = 0, b: int = 0, va: int = 0, vb: int = 0) -> bytes:
    return struct.pack("<BBBBhh", kind, type_, a, b, va, vb)


def model_bytes(lists: list[int], body: dict[int, bytes], base: int = 0x80100000) -> bytes:
    """A relocatable model file: animation table at 0x40, header at 0x80,
    header-relative event lists and HitSpecs from body."""
    data = bytearray(0x180)
    struct.pack_into("<I", data, 0x08, base + 0x40)
    struct.pack_into("<I", data, 0x10, base + 0x80)
    struct.pack_into("<I", data, 0x1C, base)
    struct.pack_into(f"<I{len(lists)}I", data, 0x40, len(lists), *[base + 0x100] * len(lists))
    struct.pack_into(f"<{len(lists)}h", data, 0x80 + EVENT_TABLE, *lists)
    for offset, raw in body.items():
        data[0x80 + offset : 0x80 + offset + len(raw)] = raw
    return bytes(data)


class FrameEventTests(unittest.TestCase):
    def test_records_run_their_specs_by_kind_until_the_ff_record(self):
        body = {
            0x40: hit_spec(0, 0x21, 3, 4, 7, -1),
            0x48: hit_spec(1, 0x99, 5, 6),
            0x4C: bytes([4, 9]),
            0x50: frame_event(0, 3, 0x40) + frame_event(5, 5, 0x48) + frame_event(2, 1, 0x4C),
            0x5C: b"\xff\x00\x00\x00",
        }
        data = model_bytes([0x50], body)
        events = frame_events(data, 0x80, 0x50)
        self.assertEqual(
            [(e.first, e.last, e.kind) for e in events], [(0, 3, 0), (5, 5, 1), (2, 1, 4)]
        )
        self.assertEqual(events[0].values, (0x21, 3, 7, 4, -1))
        self.assertEqual(
            events[0].text(), "frames 0-3 hit shot_1 part_a=3, vertex_a=7, part_b=4, vertex_b=-1"
        )
        self.assertEqual(events[1].values, (5, 6))  # byte 1 is not read
        self.assertEqual(events[2].text(), "frames 2-1 hide_part part=9")

    def test_kinds_without_a_case_and_reads_past_the_file_fail(self):
        body = {0x40: hit_spec(6), 0x50: frame_event(0, 0, 0x40) + b"\xff"}
        with self.assertRaises(UnknownKind) as caught:
            frame_events(model_bytes([0x50], body), 0x80, 0x50)
        self.assertEqual((caught.exception.offset, caught.exception.kind), (0x40, 6))
        with self.assertRaises(ScriptError):
            frame_events(model_bytes([0x50], {0x50: frame_event(0, 0, 0x7FF0)}), 0x80, 0x50)
        with self.assertRaises(ScriptError):
            frame_events(model_bytes([0x50], {0x50: frame_event(0, 0, 0x40)})[:0xD4], 0x80, 0x50)

    def test_kind_sizes_are_the_bytes_their_cases_read(self):
        self.assertEqual(
            {k: s.size() for k, s in EVENT_KINDS.items()}, {0: 8, 1: 4, 2: 8, 3: 1, 4: 2, 5: 2}
        )

    def test_hit_and_effect_types_pick_their_forms(self):
        self.assertEqual(
            [hit_form(t) for t in (0x20, 4, 0x21, 0x26, 0x27, 0x00, 0x10, 0x41, 0x60)],
            ["charged_shot", "shot_1_at_opponent", "shot_1", "shot_6", "trail", "trail", "trail"]
            + ["trail_no_sparkle"] * 2,
        )
        self.assertEqual(
            [effect_form(t) for t in (0, 4, 5, 8, 0xC, 0xD, 0x10, 0x11, 0x13, 0x14, 0x22, 0x23)],
            ["sparkle_0", "sparkle_4", "none", "sparkle_0_jittered", "sparkle_4_jittered", "none"]
            + ["line", "bolt_0", "bolt_2", "none", "sparkle_trail_2", "past_table"],
        )

    def test_model_files_relocate_against_their_build_address(self):
        model = model_file(model_bytes([0x50, 0, 0x50], {}))
        self.assertEqual((model.header, model.lists), (0x80, (0x50, 0, 0x50)))
        without_table = bytearray(model_bytes([0x50], {}))
        without_table[0x08:0x0C] = bytes(4)
        self.assertEqual(model_file(bytes(without_table)).lists, ())

    def test_a_file_sweep_counts_shared_lists_once_and_reports_unread_bytes(self):
        body = {
            0x40: hit_spec(0, 0x41, 1, 1),  # 0x3C-0x3F and 0x48-0x4B are unread
            0x4C: frame_event(0, 2, 0x40) + frame_event(3, 1, 0x40) + b"\xff\x00\x00\x00",
            0x58: b"\x00" * 4,  # between the lists
            0x5C: b"\xff\x00\x00\x00",
        }
        result = EventSweep()
        event_sweep_file(result, "t", model_bytes([0x4C, 0, 0x4C, 0x5C], body))
        self.assertEqual((result.animations, result.with_list, result.lists), (4, 3, 2))
        self.assertEqual((result.events, result.uses[0], result.never), (2, 2, 1))
        self.assertEqual((result.forms[0, "trail_no_sparkle"], result.types[0, 0x41]), (2, 2))
        self.assertEqual((result.tails, result.unread, result.unlisted), (1, 6, 4))
        self.assertEqual(
            spans_text([0, 1, 2, 3, 4, 0x10, 0x20, 0x21]), "0x00-0x04, 0x10, 0x20-0x21"
        )

    def test_unpack_keeps_only_output_decoded_before_a_read_past_the_source(self):
        stream = struct.pack("<I", 8) + b"\x00ABCDEFGH"
        self.assertEqual(unpack(stream + b"\x00"), (b"ABCDEFGH", 0))
        self.assertEqual(unpack(stream), (b"ABCDEFGH", 0))  # only the final flag read
        longer = struct.pack("<I", 16) + b"\x00ABCDEFGH\x00IJK"
        self.assertEqual(unpack(longer), (b"ABCDEFGHIJK", 5))

    def test_a_model_file_is_loaded_to_its_size_rounded_up_to_words(self):
        class Sectors:
            entries = {7: {"size": 5}, 8: {"size": 8}}

            def sectors(self, slot):
                return bytes(range(16))

        self.assertEqual(loaded(Sectors(), 7), bytes(range(8)))
        self.assertEqual(loaded(Sectors(), 8), bytes(range(8)))


class SourceTests(unittest.TestCase):
    """The opcode tables follow the recovered interpreters in decomp/src."""

    def test_worldmap_entries_follow_the_handler_table_and_returns(self):
        text = (ROOT / "decomp/src/worldmap/worldmap_open_map.c").read_text()
        table = re.search(r"ScriptOp worldmap_actor_script_handlers\[12\] = \{(.*?)\};", text, re.S).group(1)
        handlers = [spec.handler for spec in WORLDMAP.opcodes.values()]
        named = [word for word in re.findall(r"\w+", code(table)) if word != "ScriptOp"]
        self.assertEqual(named, handlers)
        self.assertEqual([defined_at(text, a) for a in WORLDMAP_HANDLER_ADDRESSES], handlers)
        for code_, spec in WORLDMAP.opcodes.items():
            body = re.search(rf"\ns32 {spec.handler}\([^)]*\) \{{.*?\n\}}", text, re.S).group(0)
            advances = {int(n) for n in re.findall(r"return (\d+);", body)} - {0}
            expected = set() if spec.flow == "stop" else {WORLDMAP.size(spec) // 2}
            self.assertEqual(advances, expected, code_)

    def test_arena_entries_follow_the_switch_cases_and_their_advances(self):
        text = (ROOT / "decomp/src/menu/menu2.c").read_text()
        body = text[text.index("s32 arena_scene_run_script(void) {") : text.index("void arena_scene_draw_marker(")]
        parts = re.split(r"\n\s*case (\d+):", body)
        advances = {}
        for number, case in zip(parts[1::2], parts[2::2], strict=True):
            steps = case.count("arena_scene_script_pc++")
            steps += sum(int(n) for n in re.findall(r"arena_scene_script_pc \+= (\d+);", case))
            advances[int(number)] = steps
        self.assertEqual(sorted(advances), sorted(ARENA.opcodes))
        for code, spec in ARENA.opcodes.items():
            self.assertEqual(spec.handler, f"case {code}")
            expected = 0 if spec.flow in ("stop", "hang") else ARENA.size(spec)
            self.assertEqual(advances[code], expected, code)


def code(text: str) -> str:
    """C source without its comments."""
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def asset(text: str, address: int) -> tuple[str, int]:
    """The name and size of the INCLUDE_ASSET that links `address` from the
    user's image."""
    pattern = rf'INCLUDE_ASSET\("\.data", (\w+), 0x{address:08X}, (0x[0-9A-F]+)\);'
    found = re.search(pattern, code(text))
    return found.group(1), int(found.group(2), 16)


def asset_size(text: str, address: int) -> int:
    """The size of the INCLUDE_ASSET that links `address` from the user's image."""
    return asset(text, address)[1]


def asset_addresses(text: str) -> dict[str, int]:
    """Each INCLUDE_ASSET's name and the address it links."""
    pattern = r'INCLUDE_ASSET\("\.data", (\w+), 0x([0-9A-F]{8}), 0x[0-9A-F]+\);'
    return {name: int(address, 16) for name, address in re.findall(pattern, code(text))}


def defined_at(text: str, address: int) -> str:
    """The name defined where a comment gives `address` (a function's leading
    comment, a variable's trailing one)."""
    function = re.search(
        rf"/\* {address:08X}\b(?:[^*]|\*(?!/))*\*/\n(?:[^\n(]*?[ *])?(\w+)\(", text
    )
    variable = re.search(rf"^[^\n/]*?\b(\w+)(?:\[[^\]]*\])*(?: = [^\n]*?)? /\* {address:08X}\b", text, re.M)
    return (function or variable).group(1)


def asset_ranges(overlay: str) -> list[tuple[int, int]]:
    """The `asset` ranges of an overlay's (or the resident's) classification file."""
    name = "resident/classification.txt" if overlay == "resident" else f"overlays/{overlay}.classification.txt"
    lines = (ROOT / "decomp/targets" / name).read_text()
    return [
        (int(start, 16), int(end, 16))
        for start, end, kind in re.findall(r"^(\w+) (\w+) (\w+) ", lines, re.M)
        if kind == "asset"
    ]


class AssetTests(unittest.TestCase):
    """The scripts the decoders read and the media embedded in data stay
    user-supplied: each is linked from the user's image with INCLUDE_ASSET
    inside an `asset` range, never written as a C initializer
    (docs/matching.md)."""

    def check(self, overlay: str, addresses: list[int]) -> None:
        sources = "".join(
            path.read_text() for path in sorted((ROOT / f"decomp/src/{overlay}").glob("*.c"))
        )
        ranges = asset_ranges(overlay)
        for address in addresses:
            with self.subTest(overlay=overlay, address=f"{address:08x}"):
                name, size = asset(sources, address)
                self.assertTrue(any(s <= address and address + size <= e for s, e in ranges))
                self.assertNotRegex(code(sources), rf"\b{name}(\[\w*\])+ = ")

    def test_world_map_scripts_and_cue_tables(self):
        tables = [a for d in DIRECTORS for s in d.sequences for a in (s.states, s.durations)]
        self.check("worldmap", [0x8009A758, 0x8009AC60, *tables])

    def test_arena_scripts(self):
        text = code((ROOT / "decomp/src/menu/menu2.c").read_text())
        table = re.search(r"u8 \*arena_scene_scripts\[\] = \{(.*?)\};", text, re.S).group(1)
        linked = asset_addresses(text)
        scenes = [linked[name] for name in re.findall(r"\w+", table)]
        self.assertEqual(len(scenes), 10)
        self.check("menu", [0x80090F38, 0x800910C4, 0x80091050, *scenes])

    def test_field_movie_sound_timelines(self):
        self.check("field", [MOVIE_SOUNDS])

    def test_world_map_texture_animations(self):
        self.check("worldmap", texture_runs())

    def test_resident_media(self):
        # The packed boot logo, sprite image and console font, the 0xFFFF
        # font glyph and the error sound's effect and wave banks.
        self.check("resident", [0x8004EABC, 0x8004FBD8, 0x800501D0, 0x80050240, 0x80050910, 0x80050940])


def function(text: str, name: str) -> str:
    """The definition of `name`, without comments."""
    text = code(text)
    start = re.search(rf"\n\w[\w *]* {name}\([^)]*\) \{{", text).start()
    return text[start : text.index("\n}\n", start) + 2]


def cases(body: str, switch: str) -> dict[int, str]:
    """The case bodies of `switch (switch)`, up to its closing brace."""
    start = body.index(f"switch ({switch}) {{")
    depth, end = 0, start
    for end in range(start, len(body)):
        depth += {"{": 1, "}": -1}.get(body[end], 0)
        if depth == 0 and body[end] == "}":
            break
    parts = re.split(r"\n\s*(?:case (\w+)|(default)):", body[start:end])
    found = {}
    for label, default, case in zip(parts[1::3], parts[2::3], parts[3::3], strict=True):
        found["default" if default else int(label, 0)] = case
    return found


# A director case's statements, as the cue actions of overlay_scripts.py.
ACTIONS = (
    (r"worldmap_actor_request\((\w+), (\w+)\);", "request"),
    (r"sound_play_effect\(\(sound_effect_bank->id << 16\) \| worldmap_scene15_ambient_sounds\[worldmap_entry_index\]\[(\d)\]\);", "ambient"),
    (r"sound_play_effect\(\(sound_effect_bank->id << 16\) \| (\w+)\);", "sound"),
    (r"sound_play_effect_on_channels_12_13\(\(sound_effect_bank->id << 16\) \| (\w+)\);", "sound_12"),
    (r"worldmap_effects_start_emitters\((\w+), NULL, NULL\);", "emitters"),
    (r"worldmap_effects_start_emitters\((\w+), &scratch->position, NULL\);", "emitters_at_target"),
    (r"worldmap_effects_stop_particles\((\w+)\);", "stop_effects"),
    (r"sound_set_seq_fade\(\(SoundSeq \*\)mode_music_seq, (\w+), (\w+)\);", "music_fade"),
    (r"worldmap_screen_fade_rate = (\w+);", "fade_rate"),
    (r"worldmap_screen_fade_step = (\w+);", "fade_step"),
    (r"worldmap_loop_running = 0;", "exit_worldmap"),
)
QUIET = (r"break;", r"worldmap_loop_result = 0;", r"scratch->position\.v[xyz] = .*;")


def case_actions(case: str) -> tuple[tuple, int | None]:
    actions, state = [], None
    for line in (line.strip() for line in case.splitlines()):
        if not line:
            continue
        if found := re.fullmatch(r"actor->state = (\d+);", line):
            state = int(found.group(1))
            continue
        for pattern, verb in ACTIONS:
            if found := re.fullmatch(pattern, line):
                actions.append((verb, *(int(value, 0) for value in found.groups())))
                break
        else:
            if not any(re.fullmatch(pattern, line) for pattern in QUIET):
                raise AssertionError(f"statement without a cue action: {line}")
    return tuple(actions), state


def requests_taken(text: str, name: str, argument: int) -> tuple[bool, bool]:
    """Whether handler `name` acts on request `argument` (a case of `switch
    (actor->unk4)`, an `actor->unk4 == n` test, or `actor->unk4 != 0` for any
    nonzero one), and whether it reads unk4 at all."""
    body = function(text, name)
    taken = {int(n, 0) for n in re.findall(r"actor->unk4 == (\w+)\)", body)}
    if "switch (actor->unk4) {" in body:
        taken |= {label for label in cases(body, "actor->unk4") if label != "default"}
    anything = "actor->unk4 != 0" in body and argument != 0
    return argument in taken or anything, "actor->unk4" in body


class SceneSourceTests(unittest.TestCase):
    def test_requests_wake_their_slot_and_name_a_state_it_takes(self):
        """worldmap_actor_request sets the slot's command to 1 (the actor pass, 80097800,
        runs its update; a start handler returning 3 leaves it idle until then)
        and, unless one is pending, unk4 to the argument. Every request names an
        argument its receiver takes, except two."""
        sources = "".join(
            path.read_text() for path in sorted((ROOT / "decomp/src/worldmap").glob("*.c"))
        )
        sender = function(sources, "worldmap_actor_request")
        self.assertIn("actor->command = 1;\n        actor->unk4 = arg;", sender)
        actor_pass = function(sources, "worldmap_actor_run_all")
        self.assertIn(
            "case 1:\n                actor->command = ((ActorFunc)actor->update)(i);", actor_pass
        )
        untaken = []
        for each in DIRECTORS:
            for state, cue in each.cues.items():
                for verb, *args in cue.actions:
                    if verb == "request":
                        slot, argument = args
                        taken, reads = requests_taken(sources, each.slots[slot], argument)
                        if not taken:
                            untaken.append(
                                (each.interpreter, state, each.slots[slot], argument, reads)
                            )
        self.assertEqual(
            untaken,
            [
                # wakes the rig's flight, idle since worldmap_scene14_rig_flight_start returned 3;
                # never read
                ("worldmap_scene14_director_update", 9, "worldmap_scene14_rig_flight_update", 1, False),
                # 0 is "none pending": the heat haze keeps its state
                ("worldmap_scene16_director_update", 3, "worldmap_scene16_haze_strength_update", 0, True),
            ],
        )
        self.assertIn("return 3;", function(sources, "worldmap_scene14_rig_flight_start"))

    def test_every_case_of_each_director_is_its_cue(self):
        sources = "".join(
            path.read_text() for path in sorted((ROOT / "decomp/src/worldmap").glob("*.c"))
        )
        named = {address: name for name, address in asset_addresses(sources).items()}
        for each in DIRECTORS:
            body = function((ROOT / each.source).read_text(), each.interpreter)
            found = cases(body, "actor->state")
            self.assertEqual(sorted(found), sorted(each.cues), each.interpreter)
            for state, case in found.items():
                with self.subTest(director=each.interpreter, case=state):
                    if state == 1:
                        self.assertIn("if (--actor->wait < 0) {", case)
                        self.assertIn("actor->u.step++;", case)
                        if each.picker is None:
                            sequence = each.sequences[0]
                            self.assertIn(f"{named[sequence.states]}[actor->u.step]", case)
                            self.assertIn(f"{named[sequence.durations]}[actor->u.step]", case)
                        else:
                            self.assertIn("((u16 *)actor->unk54)[actor->u.step]", case)
                            self.assertIn("((u16 *)actor->unk58)[actor->u.step]", case)
                        continue
                    cue = each.cues[state]
                    self.assertEqual(case_actions(case), (cue.actions, cue.state))

    def test_starters_tables_and_setups_follow_their_definitions(self):
        sources = "".join(
            path.read_text() for path in sorted((ROOT / "decomp/src/worldmap").glob("*.c"))
        )
        modes = re.search(r"WorldmapMode worldmap_mode_handlers\[19\] = \{(.*?)\n\};", sources, re.S).group(1)
        modes = re.findall(r"\{(\w+), (\w+), (\w+)\}", modes)
        named = {address: name for name, address in asset_addresses(sources).items()}
        for each in DIRECTORS:
            with self.subTest(director=each.interpreter):
                starter = function(sources, each.starter)
                self.assertEqual("actor->u.step++;" in starter, each.advances)
                if each.picker is None:
                    self.assertIn(f"actor->state = {named[each.sequences[0].states]}[0];", starter)
                else:
                    picker = defined_at(sources, each.picker)
                    self.assertIn(f"{picker}[sequence].states", starter)
                    pairs = re.search(rf"Sequence {picker}\[3\] = \{{(.*?)\}};", code(sources), re.S)
                    names = [(named[s.states], named[s.durations]) for s in each.sequences]
                    self.assertEqual(re.findall(r"\{(\w+), (\w+)\}", pairs.group(1)), names)
                for sequence in each.sequences:
                    # u16 entries linked from the user's image (INCLUDE_ASSET sizes);
                    # a wait table may end with its padding's stray halfword
                    states = asset_size(sources, sequence.states)
                    durations = asset_size(sources, sequence.durations)
                    self.assertEqual(states, 2 * sequence.entries)
                    self.assertIn(durations - 2 * sequence.entries, (0, 2))
                self.assertEqual(modes[each.mode][1], each.setup)
                setup = function(sources, each.setup)
                calls = re.findall(r"worldmap_actor_spawn\(\(s32\)(\w+), \(s32\)(\w+)\);", setup)
                self.assertEqual(
                    calls[:2],
                    [("worldmap_screen_fade_start", "worldmap_screen_fade_update"), (each.starter, each.interpreter)],
                )
                # fresh slots (worldmap_actor_alloc_slots), so slot n is the setup's n-th actor
                self.assertLess(setup.index("worldmap_actor_alloc_slots();"), setup.index("worldmap_actor_spawn("))
                self.assertEqual(tuple(update for _, update in calls), each.slots)
                targets = {
                    a[1] for cue in each.cues.values() for a in cue.actions if a[0] == "request"
                }
                self.assertLess(max(targets), len(each.slots))


def struct_fields(text: str, name: str) -> dict:
    body = re.search(rf"typedef struct \{{([^}}]*)\}} {name};", text).group(1)
    layout, offset = {}, 0
    for kind, field_name in re.findall(r"\b([us](?:8|16|32)) (\w+);", body):
        size = int(kind[1:]) // 8
        layout[field_name] = (offset, size, kind[0] == "s")
        offset += size
    return layout


class FrameEventSourceTests(unittest.TestCase):
    def setUp(self):
        self.menu3 = (ROOT / "decomp/src/menu/menu3.c").read_text()

    def test_layouts_follow_actor_h(self):
        header = (ROOT / "decomp/src/menu/actor.h").read_text()
        self.assertEqual(struct_fields(header, "FrameEvent"), FRAME_EVENT)
        self.assertEqual(struct_fields(header, "HitSpec"), HIT_SPEC)

    def test_kinds_follow_the_cases_of_func_80074678(self):
        body = function(self.menu3, "arena_frame_event_run")
        self.assertIn("while (event->first != 0xFF) {", body)
        self.assertIn(
            "event = (FrameEvent *)((u8 *)actor->header + ", body.replace("events", "event")
        )
        self.assertIn("spec = (HitSpec *)((u8 *)actor->header + event->spec);", body)
        found = cases(body, "spec->unk0")
        self.assertEqual(found.pop("default").split(), ["continue;"])  # stays on the record
        self.assertEqual(sorted(found), sorted(EVENT_KINDS))
        callees = {name: function(self.menu3, name) for name in ("arena_frame_event_hit", "arena_frame_event_effect")}
        for kind, case in found.items():
            spec = EVENT_KINDS[kind]
            callee = spec.handler.partition(": ")[2]
            self.assertEqual(spec.handler.partition(":")[0], f"arena_frame_event_run case {kind}")
            read = set(re.findall(r"spec->(\w+)", case)) - {"unk0"}
            if callee in callees:
                self.assertIn(f"{callee}(actor, spec", case)
                read |= set(re.findall(r"hit->(\w+)", callees[callee]))
            elif callee:
                self.assertIn(f"{callee}(actor", case)
            self.assertEqual(read, {name for name, _ in spec.fields}, kind)
        self.assertIn("flags |= 1;", found[4])
        self.assertIn("flags &= ~1;", found[5])
        self.assertIn("if (!sounded) {", found[1])
        self.assertIn("if (trail_count < 20) {", found[2])

    def test_forms_follow_the_type_dispatch_of_the_callees(self):
        hit = function(self.menu3, "arena_frame_event_hit")
        self.assertIn("if (hit->type == 0x20) {", hit)
        self.assertEqual(hit.count("!(hit->type & 0x40)"), 2)
        shots = set(cases(hit, "hit->type")) - {"default"}
        self.assertEqual(shots, {t for t in range(256) if hit_form(t).startswith("shot")})
        effect = function(self.menu3, "arena_frame_event_effect")
        for text in ("arena_effect_is_two_point_type(hit->type) != 0", "hit->type == 0x10", "hit->type >= 0x20"):
            self.assertIn(text, effect)
        self.assertIn(
            "if (code < 0x10) {\n        return 0;\n    }\n    return code < 0x20;",
            function(self.menu3, "arena_effect_is_two_point_type"),
        )
        bolts = set(cases(function(self.menu3, "arena_effect_queue_bolt_by_type"), "code"))
        self.assertEqual(bolts, {t for t in range(0x10, 0x20) if effect_form(t).startswith("bolt")})
        sparkles = set(cases(function(self.menu3, "arena_effect_spawn_sparkle"), "kind"))
        self.assertEqual(sparkles, {t for t in range(0x10) if effect_form(t).startswith("sparkle")})
        sizes = re.search(r"s16 arena_effect_trail_sizes\[\] = \{([^}]*)\};", self.menu3).group(1)
        self.assertEqual(len(sizes.split(",")), SPARKLE_SIZES)


class MovieSoundSourceTests(unittest.TestCase):
    """The timeline decoder follows field_movie_load_sound_bank, field_movie_play_due_sounds and event fe a0."""

    def setUp(self):
        self.text = (ROOT / "decomp/src/field/field_event.c").read_text()

    def test_the_seek_loads_file_0x115_plus_bank_and_skips_bank_plus_one_ends(self):
        body = function(self.text, "field_movie_load_sound_bank")
        group, index = MOVIE_SOUND_DIRECTORY
        self.assertIn(f"cd_select_directory(0x{group:X}, {index});", body)
        self.assertIn(f"file = bank + 0x{MOVIE_SOUND_FILE:X};", body)
        self.assertIn("for (i = 0; i < bank + 1; i++) {", body)
        self.assertIn(f"[pos * 2] == 0x{MOVIE_SOUND_END:X}) {{", body)
        self.assertIn("pos++;\n            field_movie_sound_timeline_index = pos;", body)

    def test_the_player_plays_the_low_byte_on_the_pair_in_bits_8_to_10(self):
        body = function(self.text, "field_movie_play_due_sounds")
        self.assertIn("if (field_movie_frame < times[field_movie_sound_timeline_index * 2] + FIELD_MOVIE.sound_start) {", body)
        self.assertIn(
            "sound_play_effect_on_channel((sound & 0xFF) | (field_movie_sound_bank->id << 16), ((sound >> 8) & 7) * 2);", body
        )
        self.assertIn("field_movie_sound_timeline_index++;", body)
        full = MovieSound(0, 0, 0xFFFF)
        self.assertEqual((full.effect, full.pair), (0xFF, 7))

    def test_event_fe_a0_names_the_bank_in_operand_9(self):
        prefix, extended, offset = MOVIE_SOUND_REQUEST
        body = function(self.text, "field_event_play_movie_sound")
        self.assertIn(
            f"FIELD_MOVIE.sound_bank = field_event_read_selected_operand_08({offset}, EVENT_OPERAND_BYTE(0xB));", body
        )
        self.assertEqual(prefix, 0xFE)
        forms = EXTENDED[extended]
        self.assertEqual(
            [(form.mnemonic, form.handler) for form in forms],
            [("play_movie_sound", "field_event_play_movie_sound")],
        )
        self.assertIn(offset, [operand.offset for operand in forms[0].operands])

    def test_the_whole_table_is_linked(self):
        self.assertEqual(asset_size(self.text, MOVIE_SOUNDS), 4 * MOVIE_SOUND_ENTRIES)


TEXTURE_SOURCE = ROOT / "decomp/src/worldmap/worldmap_open_map.c"


def texture_runs() -> list[int]:
    """The runs the slot tables name, in slot order."""
    text = code(TEXTURE_SOURCE.read_text())
    linked = asset_addresses(text)
    runs = []
    for table, _, count, _ in TEXTURE_SLOTS:
        rows = re.search(rf"TexAnimSlot {table}\[{count}\] = \{{(.*?)\n\}};", text, re.S).group(1)
        runs += [linked[name] for name in re.findall(r", ([A-Za-z_]\w*)\}", rows)]
    return runs


class TextureSourceTests(unittest.TestCase):
    """The texture animation decoder follows the slot tables and steppers."""

    def setUp(self):
        self.text = TEXTURE_SOURCE.read_text()

    def test_each_slot_names_a_linked_run_of_whole_entries(self):
        runs = texture_runs()
        self.assertEqual(len(runs), sum(count for _, _, count, _ in TEXTURE_SLOTS))
        for start in runs:
            self.assertEqual(asset_size(self.text, start) % 4, 0, f"{start:08x}")

    def test_the_steppers_count_down_step_and_restart_at_frame_0(self):
        creators = {"worldmap_texture_anim_slots": "worldmap_texture_anim_create", "worldmap_texture_anim2_slots": "worldmap_texture_anim2_create"}
        for table, _, _, stepper in TEXTURE_SLOTS:
            with self.subTest(table=table):
                body = function(self.text, creators[table])
                self.assertIn(f"anim->slot = &{table}[i];", body)
                self.assertIn("anim->frame = 0;\n        anim->timer = 1;", body)
                body = function(self.text, stepper)
                for line in (
                    "if (--anim->timer == 0) {",
                    "anim->frame++;",
                    "anim->timer = anim->slot->frames[anim->frame].duration;",
                    "if (anim->timer < 0) {",
                    "anim->frame = 0;",
                    "anim->timer = anim->slot->frames[0].duration;",
                ):
                    self.assertIn(line, body)

    def test_layouts_follow_worldmap_h(self):
        header = (ROOT / "decomp/src/worldmap/worldmap.h").read_text()
        self.assertEqual(
            struct_fields(header, "TexAnimFrame"),
            {"image": (0, 2, True), "duration": (2, 2, True)},
        )
        slot = re.search(r"typedef struct \{([^}]*)\} TexAnimSlot;", header).group(1)
        self.assertEqual(
            slot.split(), ["RECT", "rect;", "s32", "unk8;", "TexAnimFrame", "*frames;"]
        )
        self.assertEqual(TEXTURE_SLOT_BYTES, 8 + 4 + 4)


class TargetTests(unittest.TestCase):
    def test_overlays_name_their_recovered_images(self):
        for overlay in (WORLDMAP.overlay, ARENA.overlay, "field"):
            self.assertRegex(expected_sha256(overlay), "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
