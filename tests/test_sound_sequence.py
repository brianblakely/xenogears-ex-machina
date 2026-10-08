"""Synthetic sound sequence bytes exercise the recovered opcode table; they
describe no original data."""

import struct
import unittest

from tools.analysis.sound_sequence import (
    D_80050A94,
    D_800509B0,
    D_80050624,
    D_80050824,
    DURATIONS,
    END,
    JUMP,
    NOTE_KEYS,
    OPCODES,
    UNUSED_HANDLER,
    WAIT,
    SequenceError,
    Sweep,
    archive_offsets,
    check_driver_tables,
    containers,
    decode_channel,
    decode_instruction,
    locate,
    lookahead_length,
    parse_bank,
    parse_sequence,
    word_sum,
)


def sequence(channels: list[bytes | None]) -> bytes:
    """A sequence header (SoundSeqHeader) followed by the channels' data."""
    header = bytearray(0x22 + 2 * len(channels))
    header[0:4] = b"smds"
    header[0x14] = len(channels)
    data = bytearray()
    offsets = []
    for channel in channels:
        offsets.append(0 if channel is None else len(header) + len(data))
        data += channel or b""
    struct.pack_into(f"<{len(channels)}H", header, 0x22, *offsets)
    out = header + data
    struct.pack_into("<I", out, 8, len(out))
    return bytes(out)


def bank(effects: list[tuple[bytes | None, bytes | None]]) -> bytes:
    """An effect bank (SoundBank) whose words sum to zero, version 0x101."""
    header = bytearray(0x20 + 4 * len(effects))
    header[0:4] = b"seds"
    # version, flags, effect count, bank id, wave bank key
    struct.pack_into("<HHHHHH", header, 0xC, 0x101, 0, 0, len(effects), 7, 0)
    volumes = bytes([0x7F] * len(effects))
    data = bytearray()
    for effect, parts in enumerate(effects):
        for part, channel in enumerate(parts):
            offset = 0
            if channel is not None:
                offset = len(header) + len(volumes) + len(data)
                data += channel
            struct.pack_into("<H", header, 0x20 + 4 * effect + 2 * part, offset)
    struct.pack_into("<H", header, 0x18, len(header))
    out = header + volumes + data
    out += bytes(-len(out) % 4)
    struct.pack_into("<I", out, 8, len(out))
    struct.pack_into("<I", out, 4, (-word_sum(bytes(out), len(out))) & 0xFFFFFFFF)
    return bytes(out)


class OpcodeTableTests(unittest.TestCase):
    def test_every_slot_has_a_handler_or_the_unused_one(self):
        self.assertEqual(len(OPCODES), 97)
        self.assertEqual(len({op.handler for op in OPCODES.values()}), 97)
        self.assertNotIn(UNUSED_HANDLER, {op.handler for op in OPCODES.values()})
        self.assertTrue(all(0x80 <= code <= 0xFF for code in OPCODES))

    def test_lookahead_lengths(self):
        self.assertEqual(lookahead_length(0x80), 2)
        self.assertEqual((OPCODES[0x9D].length, lookahead_length(0x9D)), (3, 4))
        self.assertEqual((OPCODES[0xF5].length, lookahead_length(0xF5)), (1, 2))
        self.assertEqual(lookahead_length(0x82), 0)

    def test_driver_tables_check(self):
        exe = bytearray(0x800 + 0x41000)
        struct.pack_into("<I", exe, 0x18, 0x80010000)

        def put(address, data):
            exe[address - 0x80010000 + 0x800 : address - 0x80010000 + 0x800 + len(data)] = data

        handlers = [
            OPCODES[c].handler if c in OPCODES else UNUSED_HANDLER for c in range(0x80, 0x100)
        ]
        put(D_80050624, struct.pack("<128I", *handlers))
        put(D_80050824, bytes(lookahead_length(c) for c in range(0x80, 0x100)))
        put(D_800509B0, bytes(DURATIONS[k % 19] for k in range(NOTE_KEYS)))
        put(D_80050A94, bytes(k // 19 for k in range(NOTE_KEYS)))
        self.assertEqual(check_driver_tables(bytes(exe)), [])
        put(D_80050824 + 0x1D, b"\3")
        self.assertEqual(len(check_driver_tables(bytes(exe))), 1)


class DecodeTests(unittest.TestCase):
    def test_notes(self):
        note = decode_instruction(bytes([0x40, 19 * 5 + 6]), 0)
        self.assertEqual((note.mnemonic, note.length, note.flow), ("note", 2, WAIT))
        self.assertEqual(
            dict(note.operands), {"volume": 0x40, "key": 101, "semitone": 5, "ticks": 0x30}
        )
        explicit = decode_instruction(bytes([0x7F, 19 * 2, 0x99]), 0)
        self.assertEqual((explicit.length, dict(explicit.operands)["ticks"]), (3, 0x99))
        with self.assertRaises(SequenceError):
            decode_instruction(bytes([0x40, NOTE_KEYS]), 0)

    def test_operand_kinds(self):
        detune = decode_instruction(bytes([0xD3, 0xFF, 0xFE]), 0)
        self.assertEqual((detune.length, detune.operands), (3, (("delta", -2),)))
        play = decode_instruction(bytes([0x9C, 0x34, 0x12, 0xEE]), 0)
        self.assertEqual((play.length, dict(play.operands)["effect"]), (4, 0x1234))
        level = decode_instruction(bytes([0xE2, 0x10, 0xF0]), 0)
        self.assertEqual(level.operands, (("frames", 0x10), ("target", -16)))

    def test_channel_runs_to_its_end(self):
        data = bytes([0x94, 4, 0x98, 2, 0x60, 19 * 3 + 9, 0x99, 0x80, 0x18, 0x90, 0x55])
        channel = decode_channel(data, 0)
        self.assertEqual(
            [i.mnemonic for i in channel], ["octave", "repeat", "note", "repeat_end", "rest", "end"]
        )
        self.assertEqual(channel[-1].flow, END)
        self.assertEqual(channel[-1].offset + channel[-1].length, 10)

    def test_effect_jump_ends_the_channel(self):
        channel = decode_channel(bytes([0x91, 0x9E, 1, 0, 1, 0x60]), 0)
        self.assertEqual((channel[-1].mnemonic, channel[-1].flow), ("goto_effect", JUMP))
        self.assertEqual(dict(channel[-1].operands), {"effect": 1, "part": 1})

    def test_unused_slot_and_overrun_are_errors(self):
        with self.assertRaises(SequenceError) as caught:
            decode_channel(bytes([0x95, 0x82, 0x90]), 0)
        self.assertEqual(caught.exception.offset, 1)
        with self.assertRaises(SequenceError):
            decode_channel(bytes([0x95, 0xA2, 1]), 0)


class ScriptTests(unittest.TestCase):
    def test_sequence_channels(self):
        data = sequence([bytes([0xA0, 0x66, 0x90]), None, bytes([0x90])])
        script = parse_sequence(data)
        self.assertEqual([label for label, _ in script.entries], ["channel 0", "channel 2"])
        self.assertEqual(decode_channel(data, script.entries[1][1])[0].mnemonic, "end")

    def test_bank_effects_and_location(self):
        data = bank([(bytes([0xAC, 3, 0x40, 19, 0x90]), None), (None, bytes([0x90]))])
        script = parse_bank(data)
        self.assertEqual(
            [label for label, _ in script.entries], ["effect 0 part 0", "effect 1 part 1"]
        )
        container = bytes(8) + data + sequence([bytes([0x90])])
        found, rejected = locate(container)
        self.assertEqual(
            [(position, s.kind) for position, s in found], [(8 + len(data), "smds"), (8, "seds")]
        )
        self.assertEqual(rejected, {})
        broken = bytearray(container)
        broken[8 + 0x20] ^= 1  # the bank's word sum is no longer zero
        found, rejected = locate(bytes(broken))
        self.assertEqual([s.kind for _, s in found], ["smds"])
        self.assertEqual(sum(rejected.values()), 1)

    def test_archive_offsets(self):
        self.assertEqual(archive_offsets(struct.pack("<III", 2, 12, 14) + bytes(4)), (12, 14))
        self.assertEqual(archive_offsets(struct.pack("<III", 2, 14, 12) + bytes(4)), ())
        self.assertEqual(archive_offsets(struct.pack("<III", 2, 8, 12) + bytes(4)), ())
        self.assertEqual(archive_offsets(struct.pack("<III", 2, 12, 17) + bytes(4)), ())
        self.assertEqual(archive_offsets(struct.pack("<III", 2, 12, 16) + bytes(8), 16), (12, 16))

    def test_bank_in_a_packed_archive_entry_is_located(self):
        data = bank([(bytes([0x95, 0x95, 0x90]), None)])  # 40 bytes: five literal groups
        packed = len(data).to_bytes(4, "little")
        for start in range(0, len(data), 8):
            packed += b"\0" + data[start : start + 8]
        file = struct.pack("<III", 2, 12, 16) + bytes(4) + packed  # the final flag: padding

        class Disc:
            number, boot = 1, b""

            def files(self):
                return [{"slot": 5, "size": len(file)}]

            def sectors(self, slot):
                return file + bytes(2048 - len(file))

        found = [(label, s.kind) for label, c in containers(Disc()) for _, s in locate(c)[0]]
        self.assertEqual(found, [("disc1 slot 5 entry 1 unpacked", "seds")])

    def test_unreached_bytes_decode_on_their_own(self):
        # Each effect's channel ends at its first 90; 95 90 and the final 80
        # (a rest running into the padding) are reached by no channel.
        script = parse_bank(bank([(bytes([0x90, 0x95, 0x90]), None), (bytes([0x90, 0x80]), None)]))
        result = Sweep()
        result.add("bank", script)
        self.assertEqual((result.channels, result.instructions), (2, 2))
        self.assertEqual(result.unreferenced["seds"], 3)
        self.assertEqual(result.unreferenced["seds channels"], 1)
        self.assertEqual(len(result.unreferenced_errors), 1)
        self.assertNotIn(0x95, result.uses)


if __name__ == "__main__":
    unittest.main()
