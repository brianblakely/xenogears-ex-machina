"""Sound sequence bytecode of the resident sound driver, from the recovered C.

The interpreter is sound_seq_interpret_channels (decomp/src/resident/sound.c). A byte below
0x80 is a note: it sets the channel volume (byte << 8) and is followed by an
encoded key byte k, indexing sound_note_semitones (semitone, k // 19) and sound_note_durations
(ticks, DURATIONS[k % 19]; 0 means a third byte holds the ticks). Bytes
0x80-0xFF call sound_seq_opcode_handlers[op - 0x80](position, sequence, channel), which
returns the next position. The look-ahead that decides whether the note before
the next one is released steps over opcodes by sound_seq_opcode_lengths instead of executing
them; LOOKAHEAD records that table where it differs from what the handler
consumes. Every entry of OPCODES names the handler it was read from.

Scripts: music sequences ("smds", SoundSeqHeader: channel data offsets at
+0x22, read by sound_start_seq_channels) and sound effect banks ("seds", SoundBank: two
channel offsets per effect at +0x20, read by sound_start_effect; sound_check_file
accepts a bank only with a zero word sum and version 0x101 at +0xC). The
driver never validates sequence headers (sound_check_seq_header returns 0).

The sweep also counts the operand-indexed tables of this machine: the
modulator waves sound_modulator_waves[mode & 0xF] that D9/E5/ED (with a nonzero rate and
depth) and F0 install, and F0's index into the channel's modulator[4].

    python3 -m tools.analysis.sound_sequence --sweep      # both discs, aggregate only
    python3 -m tools.analysis.sound_sequence FILE [--offset N]   # list one local script

Outputs describe the user's discs; keep listings under .local/.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field

from tools.analysis.disc_index import discs
from tools.analysis.packed import PackedError, decode_block

# Flow kinds.
NEXT = "next"  # continue with the following instruction
WAIT = "wait"  # ends this tick's decoding of the channel (note, rest, tie)
END = "end"  # 0x90: back to the loop point, or the channel stops
JUMP = "jump"  # 0x9E: continue in another effect's channel data
REPEAT = "repeat"  # 0x98: opens a repeat
REPEAT_END = "repeat_end"  # 0x99: back to the repeat start while passes remain
REPEAT_BREAK = "repeat_break"  # 0x9A: to the repeat end on the last pass

UNUSED_HANDLER = 0x8003CD00  # sound_seq_unused_opcode returns its argument; sound_seq_opcode_lengths gives 0
# Note ticks: sound_note_durations[k] = DURATIONS[k % 19] for k < 228 (0: a third byte).
DURATIONS = (0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2)
NOTE_KEYS = 12 * len(DURATIONS)  # 228 entries in sound_note_durations and sound_note_semitones

KINDS = {"u8": 1, "s8": 1, "x8": 1, "u16": 2, "s16be": 2}


@dataclass(frozen=True)
class Opcode:
    code: int
    mnemonic: str
    operands: tuple[str, ...]  # "name:kind"; u16 is little-endian, x8 is ignored
    handler: int  # sound_seq_opcode_handlers entry
    effect: str
    flow: str = NEXT

    @property
    def length(self) -> int:
        return 1 + sum(KINDS[spec.split(":")[1]] for spec in self.operands)


def _op(code, mnemonic, operands, handler, effect, flow=NEXT):
    return Opcode(code, mnemonic, tuple(operands), handler, effect, flow)


# Each entry: the handler (sound_seq_opcode_handlers[code - 0x80]) and what its C does.
OPCODES = {
    op.code: op
    for op in (
        _op(
            0x80,
            "rest",
            ["ticks:u8"],
            0x8003CD08,
            "wait `ticks` with the key released (unk5C; flags 0x400 and flags2 2, the key-off "
            "request of an expired gate)",
            WAIT,
        ),
        _op(
            0x81,
            "tie",
            ["ticks:u8"],
            0x8003CD30,
            "wait `ticks` holding the note (unk5C, flags 0x100)",
            WAIT,
        ),
        _op(0x8A, "nop_8a", [], 0x8003CD4C, "no effect"),
        _op(
            0x8D,
            "loop_point_if",
            ["selector:u8"],
            0x8003CD54,
            "mark the loop point after the operand (and the transpose) when `selector` equals "
            "sequence byte 0x1B (set by 8003abe8)",
        ),
        _op(
            0x8E,
            "skip3_8e",
            ["a:x8", "b:x8", "c:x8"],
            0x8003CD7C,
            "skip three operand bytes; no effect",
        ),
        _op(0x8F, "nop_8f", [], 0x8003CD84, "no effect"),
        _op(
            0x90,
            "end",
            [],
            0x8003CD8C,
            "continue at the loop point (counting the pass, restoring its transpose); without one "
            "release the voice and stop the channel",
            END,
        ),
        _op(0x91, "loop_point", [], 0x8003CE04, "mark the loop point here and save the transpose"),
        _op(0x94, "octave", ["octave:u8"], 0x8003CE18, "transpose = octave * 12"),
        _op(0x95, "octave_up", [], 0x8003CE38, "transpose += 12"),
        _op(0x96, "octave_down", [], 0x8003CE50, "transpose -= 12"),
        _op(
            0x97,
            "time_signature",
            ["beats:u8", "unit:u8"],
            0x8003CE68,
            "`beats` beats per bar of 0xC0 / unit ticks each (the bar and beat counters "
            "8003c020 advances); restart the current beat's ticks",
        ),
        _op(
            0x98,
            "repeat",
            ["count:u8"],
            0x8003CEF0,
            "open a repeat of `count` passes one level deeper, saving its start and transpose",
            REPEAT,
        ),
        _op(
            0x99,
            "repeat_end",
            [],
            0x8003CF38,
            "while passes remain go back to the repeat start (recording this end, restoring the "
            "start transpose); else close the repeat",
            REPEAT_END,
        ),
        _op(
            0x9A,
            "repeat_break",
            [],
            0x8003CFA4,
            "on the last pass jump to the recorded repeat end and close the repeat",
            REPEAT_BREAK,
        ),
        _op(
            0x9C,
            "play_effect",
            ["effect:u16", "unused:x8"],
            0x8003CFF0,
            "play effect `effect` of bank 0 at volume 0x7F, pan 0x40 (80039f18)",
        ),
        _op(
            0x9D,
            "stop_effect",
            ["effect:u16"],
            0x8003D034,
            "stop the effect channels playing id `effect` (8003a14c)",
        ),
        _op(
            0x9E,
            "goto_effect",
            ["effect:u16", "part:u8"],
            0x8003D070,
            "continue three bytes into channel `part` of effect `effect` of the channel's bank "
            "(the first bank when it has none); with no loaded bank of that id it returns the "
            "operand pointer, so the operands are executed as opcodes",
            JUMP,
        ),
        _op(
            0xA0,
            "rate",
            ["rate:u8"],
            0x8003D0E8,
            "tick rate = rate; ticks per frame = rate * tempo",
        ),
        _op(
            0xA1,
            "rate_add",
            ["delta:s8"],
            0x8003D110,
            "tick rate += delta; zero the ticks per frame until a rate or tempo opcode or slide "
            "recomputes them",
        ),
        _op(
            0xA2,
            "rate_slide",
            ["frames:u8", "target:u8"],
            0x8003D13C,
            "slide the tick rate to `target` over `frames`",
        ),
        _op(
            0xA4,
            "set_1a",
            ["value:u8"],
            0x8003CEC0,
            "sequence byte 0x1A = value (no recovered code reads it)",
        ),
        _op(0xA5, "add_1a", ["value:u8"], 0x8003CED4, "sequence byte 0x1A += value"),
        _op(
            0xA6,
            "fade_level",
            ["level:u8"],
            0x8003D17C,
            "sequence level = level << 24; flag every channel's volume",
        ),
        _op(
            0xA7,
            "fade_slide",
            ["units:u8", "target:u8"],
            0x8003D1BC,
            "slide the sequence level to `target` over units * 32 frames",
        ),
        _op(
            0xA9,
            "gate",
            ["sixteenths:u8"],
            0x8003D208,
            "release point of each note in sixteenths of its ticks (15: ticks - 1, 16: all)",
        ),
        _op(
            0xAA,
            "voice",
            ["voice:u8"],
            0x8003D21C,
            "move the channel to hardware voice `voice` (< 25)",
        ),
        _op(
            0xAC,
            "instrument",
            ["instrument:u8"],
            0x8003D298,
            "select an instrument of the wave bank (8003e5bc)",
        ),
        _op(
            0xAD,
            "duration_adjust",
            ["ticks:u8"],
            0x8003D2D0,
            "add to the signed note-length bias (0 resets it)",
        ),
        _op(
            0xAE,
            "drum_on",
            [],
            0x8003D300,
            "notes select entries of the sequence table (8003cc84), when the sequence has one",
        ),
        _op(0xAF, "drum_off", [], 0x8003D328, "notes play the channel's instrument"),
        _op(
            0xB0,
            "legato_on",
            [],
            0x8003D340,
            "channel flag 0x800: notes keep the key held (no gate release)",
        ),
        _op(0xB1, "legato_off", [], 0x8003D358, "clear channel flag 0x800"),
        _op(0xB2, "pitch_mod_on", [], 0x8003D370, "SPU pitch modulation on (odd hardware voices)"),
        _op(
            0xB3, "pitch_mod_off", [], 0x8003D3A4, "SPU pitch modulation off (odd hardware voices)"
        ),
        _op(0xB4, "noise_clock", ["clock:u8"], 0x8003D3D8, "noise on at SPU noise clock `clock`"),
        _op(
            0xB5,
            "noise_clock_add",
            ["delta:u8"],
            0x8003D438,
            "noise on, clock += delta (modulo 64)",
        ),
        _op(0xB6, "noise_on", [], 0x8003D4A4, "noise on"),
        _op(0xB7, "noise_off", [], 0x8003D4C4, "noise off"),
        _op(
            0xB8,
            "reverb_settings",
            ["depth:u8", "delay:s8", "feedback:s8"],
            0x8003D4E4,
            "reverb depth (<< 8), delay and feedback through 80038934, keeping the type",
        ),
        _op(
            0xBA,
            "reverb_on",
            [],
            0x8003D53C,
            "reverb on, unless an effect set's channel stays dry (driver flag 0x2000 clear or "
            "channel flag 2)",
        ),
        _op(0xBB, "reverb_off", [], 0x8003D59C, "reverb off"),
        _op(
            0xBC,
            "skip3_bc",
            ["a:x8", "b:x8", "c:x8"],
            0x8003D5BC,
            "skip three operand bytes; no effect",
        ),
        _op(0xBD, "nop_bd", [], 0x8003D5C4, "no effect"),
        _op(0xBE, "nop_be", [], 0x8003D5CC, "no effect"),
        _op(0xC0, "instrument_reload", [], 0x8003D5D4, "reselect the current instrument"),
        _op(
            0xC1,
            "envelope_modes",
            ["attack:u8", "sustain:u8", "release:u8"],
            0x8003D60C,
            "ADSR attack, sustain and release modes",
        ),
        _op(0xC2, "attack_rate", ["rate:u8"], 0x8003D640, "ADSR attack rate"),
        _op(0xC3, "decay_rate", ["rate:u8"], 0x8003D65C, "ADSR decay rate"),
        _op(0xC4, "sustain_rate", ["rate:u8"], 0x8003D678, "ADSR sustain rate"),
        _op(
            0xC5,
            "release_rate",
            ["rate:u8"],
            0x8003D694,
            "ADSR release rate, also each note's default",
        ),
        _op(0xC6, "sustain_level", ["level:u8"], 0x8003D6B4, "ADSR sustain level"),
        _op(
            0xC7,
            "decay_sustain",
            ["decay:u8", "level:u8"],
            0x8003D6D0,
            "ADSR decay rate and sustain level",
        ),
        _op(0xC8, "attack_mode", ["mode:u8"], 0x8003D6F8, "ADSR attack mode"),
        _op(0xC9, "sustain_mode", ["mode:u8"], 0x8003D714, "ADSR sustain mode"),
        _op(0xCA, "release_mode", ["mode:u8"], 0x8003D730, "ADSR release mode"),
        _op(0xD0, "detune", ["eighths:s8"], 0x8003D74C, "detune (1/256 semitones) = eighths << 5"),
        _op(0xD1, "detune_add", ["eighths:s8"], 0x8003D770, "detune += eighths << 5"),
        _op(0xD2, "detune_add_fine", ["steps:s8"], 0x8003D79C, "detune += steps << 3"),
        _op(
            0xD3,
            "detune_add_word",
            ["delta:s16be"],
            0x8003D7C8,
            "detune += a big-endian signed halfword",
        ),
        _op(
            0xD4,
            "pitch_slide",
            ["frames:u8", "semitones:s8"],
            0x8003D7FC,
            "slide the pitch by `semitones` over `frames` (either zero: stop the slide)",
        ),
        _op(
            0xD5,
            "pitch_slide_hold",
            [],
            0x8003D854,
            "toggle flags3 bit 1: the pitch slide keeps going without counting frames",
        ),
        _op(
            0xD6,
            "portamento",
            ["frames:u8"],
            0x8003D884,
            "each new note slides from the previous one over `frames` (0: off)",
        ),
        _op(
            0xD7,
            "vibrato_period",
            ["period:u8"],
            0x8003DAB0,
            "fade the pitch modulator in over (period + 1) * 4 frames (step 0x400 / that; 0xFF "
            "changes nothing)",
        ),
        _op(
            0xD8,
            "vibrato",
            ["rate:u8", "depth:s8", "delay:u8"],
            0x8003D8B8,
            "pitch modulator on: signed squared depth << 14, rate + rate*rate/64, delay * 4 "
            "frames, wave 3 (8003f2a0), restarted by each note; nothing when rate or depth is 0",
        ),
        _op(
            0xD9,
            "vibrato_wave",
            ["rate:u8", "depth:s8", "mode:u8"],
            0x8003D9A4,
            "as vibrato without delay, wave mode & 0xF (800508a4); mode bit 4 keeps it running "
            "across notes",
        ),
        _op(0xDA, "vibrato_on", [], 0x8003DAEC, "pitch modulator on"),
        _op(0xDB, "vibrato_off", [], 0x8003DB0C, "pitch modulator off"),
        _op(0xDC, "pitch_slide_off", [], 0x8003D86C, "stop the pitch slide"),
        _op(
            0xE0,
            "level",
            ["level:u8"],
            0x8003DB2C,
            "channel level = level << 24; stop level slides",
        ),
        _op(
            0xE1,
            "level_add",
            ["delta:s8"],
            0x8003DB58,
            "channel level += delta << 24; stop level slides",
        ),
        _op(
            0xE2,
            "level_slide",
            ["frames:u8", "target:s8"],
            0x8003DB98,
            "slide the channel level to `target` over `frames`",
        ),
        _op(
            0xE3,
            "tremolo_period",
            ["period:u8"],
            0x8003DE18,
            "fade the volume modulator in over (period + 1) * 4 frames (step 0x400 / that; 0xFF "
            "changes nothing)",
        ),
        _op(
            0xE4,
            "tremolo",
            ["rate:u8", "depth:s8", "delay:u8"],
            0x8003DC50,
            "volume modulator on: depth << 24, rate + rate*rate/64, delay * 4 frames, wave 2 "
            "(8003f240), restarted by each note; nothing when rate or depth is 0",
        ),
        _op(
            0xE5,
            "tremolo_wave",
            ["rate:u8", "depth:s8", "mode:u8"],
            0x8003DD24,
            "as tremolo without delay, wave mode & 0xF; mode bit 4 keeps it running across notes",
        ),
        _op(0xE6, "tremolo_on", [], 0x8003DE54, "volume modulator on"),
        _op(0xE7, "tremolo_off", [], 0x8003DE74, "volume modulator off"),
        _op(
            0xE8, "pan", ["pan:u8"], 0x8003DE94, "pan = pan << 8 (0 left, 0x40 centre, 0x7F right)"
        ),
        _op(0xE9, "pan_add", ["delta:s8"], 0x8003DEB4, "pan += delta << 8 (wrapping in 15 bits)"),
        _op(
            0xEA,
            "pan_slide",
            ["frames:u8", "target:s8"],
            0x8003DEE4,
            "step the pan toward `target` over `frames`; the last frame sets it to the stored "
            "difference (target - start) << 8",
        ),
        _op(
            0xEB,
            "autopan_period",
            ["period:u8"],
            0x8003DF3C,
            "fade the pan modulator in over (period + 1) * 4 frames (step 0x400 / that; 0xFF "
            "changes nothing)",
        ),
        _op(
            0xEC,
            "autopan",
            ["rate:u8", "depth:s8", "delay:u8"],
            0x8003DF78,
            "pan modulator on: depth << 24, rate + rate*rate/64, delay * 4 frames, wave 3, "
            "restarted by each note; nothing when rate or depth is 0",
        ),
        _op(
            0xED,
            "autopan_wave",
            ["rate:u8", "depth:s8", "mode:u8"],
            0x8003E04C,
            "as autopan without delay, wave mode & 0xF; mode bit 4 keeps it running across notes",
        ),
        _op(0xEE, "autopan_on", [], 0x8003E140, "pan modulator on"),
        _op(0xEF, "autopan_off", [], 0x8003E160, "pan modulator off"),
        _op(
            0xF0,
            "modulator_select",
            ["index:u8", "mode:u8", "target:u8"],
            0x8003E180,
            "select modulator `index` and set its wave (mode & 0xF), target (0 pitch, 1 level, "
            "2 pan), no delay; left off; mode bit 4 keeps it running across notes",
        ),
        _op(
            0xF1,
            "modulator_depth",
            ["rate:u8", "depth_high:s8", "depth_low:u8"],
            0x8003E1F8,
            "selected modulator: rate + rate*rate/64 and a 16-bit depth",
        ),
        _op(
            0xF2,
            "modulator_timing",
            ["delay:u8", "period:u8"],
            0x8003E308,
            "selected modulator: delay * 4 frames, fade-in over (period + 1) * 4 frames (period "
            "0xFF: neither changes)",
        ),
        _op(0xF5, "nop_f5", [], 0x8003E358, "no effect"),
        _op(
            0xF6,
            "modulator_on",
            ["index:u8"],
            0x8003E360,
            "restart modulator `index` and switch it on",
        ),
        _op(0xF7, "modulator_off", ["index:u8"], 0x8003E40C, "switch modulator `index` off"),
        _op(
            0xF8,
            "level_sweep",
            ["from:u8", "frames:u8", "to:u8"],
            0x8003DBE4,
            "each note sweeps the channel level from `from` to `to` over `frames` (equal levels "
            "or 0 frames: off)",
        ),
        _op(
            0xF9,
            "position",
            ["bar:u8", "beat:u8"],
            0x8003CE9C,
            "set the bar and beat counters and restart the beat's ticks",
        ),
        _op(
            0xFC,
            "wave_bank_instrument",
            ["key:u8", "instrument:u8"],
            0x8003E44C,
            "select the wave bank with `key` (the first loaded wave bank when none has it) and an "
            "instrument of it",
        ),
        _op(0xFD, "tempo", ["tempo:u8"], 0x8003E4BC, "tempo = tempo << 24 unless 0"),
        _op(
            0xFE,
            "wave_bank",
            ["key:u8"],
            0x8003E4F0,
            "select the wave bank with `key` (the first loaded wave bank when none has it)",
        ),
        _op(
            0xFF,
            "stop_when_silent",
            [],
            0x8003E54C,
            "stop the channel when its voice's envelope level is 0; else continue",
        ),
    )
}

# sound_seq_opcode_lengths where the look-ahead steps differently from the handler: the
# unused slots step 0 (the look-ahead would never leave them), 9D and F5 one
# byte more than they consume.
LOOKAHEAD = {0x9D: 4, 0xF5: 2}


def lookahead_length(code: int) -> int:
    if code not in OPCODES:
        return 0
    return LOOKAHEAD.get(code, OPCODES[code].length)


# sound_modulator_waves (sound.c): the modulator wave of each shape, 16 slots indexed by
# mode & 0xF (8-15 switch the modulator off). D9/E5/ED install one only with a
# nonzero rate and depth; F0 always, in modulator[index] of the channel
# (SoundSeqChannel.modulator[4], sound.h).
WAVE_SLOTS = 16
WAVE_OPCODES = (0xD9, 0xE5, 0xED, 0xF0)
MODULATORS = 4


def installed_wave(instruction) -> int | None:
    """The sound_modulator_waves index the instruction's handler installs, if any."""
    if instruction.code not in WAVE_OPCODES:
        return None
    values = dict(instruction.operands)
    if instruction.code != 0xF0 and not (values["rate"] and values["depth"]):
        return None
    return values["mode"] & 0xF


class SequenceError(ValueError):
    def __init__(self, offset: int, reason: str):
        self.offset, self.reason = offset, reason
        super().__init__(f"+0x{offset:x}: {reason}")


@dataclass(frozen=True)
class Instruction:
    offset: int
    code: int  # the first byte (a note's volume when below 0x80)
    mnemonic: str
    operands: tuple[tuple[str, int], ...]
    length: int
    flow: str


def _operand(data: bytes, offset: int, kind: str) -> int:
    if kind in ("u8", "x8"):
        return data[offset]
    if kind == "s8":
        return data[offset] - 0x100 if data[offset] & 0x80 else data[offset]
    if kind == "u16":
        return data[offset] | data[offset + 1] << 8
    value = data[offset] << 8 | data[offset + 1]  # s16be
    return value - 0x10000 if value & 0x8000 else value


def decode_instruction(data: bytes, offset: int) -> Instruction:
    if offset >= len(data):
        raise SequenceError(offset, "channel data runs past the end of the script")
    code = data[offset]
    if code < 0x80:
        if offset + 2 > len(data):
            raise SequenceError(offset, "note runs past the end of the script")
        key = data[offset + 1]
        if key >= NOTE_KEYS:
            raise SequenceError(
                offset, f"note key 0x{key:02x} is outside the 228-entry note tables"
            )
        ticks, length = DURATIONS[key % 19], 2
        if ticks == 0:
            if offset + 3 > len(data):
                raise SequenceError(offset, "note runs past the end of the script")
            ticks, length = data[offset + 2], 3
        operands = (("volume", code), ("key", key), ("semitone", key // 19), ("ticks", ticks))
        return Instruction(offset, code, "note", operands, length, WAIT)
    op = OPCODES.get(code)
    if op is None:
        raise SequenceError(offset, f"opcode 0x{code:02x} has no handler (unused slot 8003cd00)")
    if offset + op.length > len(data):
        raise SequenceError(offset, f"{op.mnemonic} runs past the end of the script")
    operands, position = [], offset + 1
    for spec in op.operands:
        name, kind = spec.split(":")
        operands.append((name, _operand(data, position, kind)))
        position += KINDS[kind]
    return Instruction(offset, code, op.mnemonic, tuple(operands), op.length, op.flow)


def decode_channel(data: bytes, start: int) -> tuple[Instruction, ...]:
    """Instructions from `start` through the end (0x90) or effect jump (0x9E)."""
    out, offset = [], start
    while True:
        instruction = decode_instruction(data, offset)
        out.append(instruction)
        offset += instruction.length
        if instruction.flow in (END, JUMP):
            return tuple(out)


@dataclass(frozen=True)
class Script:
    kind: str  # "smds" sequence or "seds" effect bank
    data: bytes
    entries: tuple[tuple[str, int], ...]  # (channel label, data offset)


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def word_sum(data: bytes, size: int) -> int:
    """sound_sum_file_words: the sum of the file's (size + 3) / 4 words."""
    count = (size + 3) // 4
    return sum(struct.unpack_from(f"<{count}I", data.ljust(count * 4, b"\0"))) & 0xFFFFFFFF


def parse_sequence(data: bytes) -> Script:
    """SoundSeqHeader: channel count +0x14, table entries +0x15, table +0x20,
    channel data offsets +0x22 (0: unused channel)."""
    if len(data) < 0x22 or data[:4] != b"smds":
        raise SequenceError(0, "not a sequence header")
    channels = data[0x14]
    if 0x22 + 2 * channels > len(data):
        raise SequenceError(0x22, "channel offsets run past the end of the script")
    entries = []
    for channel in range(channels):
        offset = _u16(data, 0x22 + 2 * channel)
        if offset:
            if offset >= len(data):
                raise SequenceError(0x22 + 2 * channel, "channel data offset outside the script")
            entries.append((f"channel {channel}", offset))
    if data[0x15] and _u16(data, 0x20) + 5 * data[0x15] > len(data):
        raise SequenceError(0x20, "sequence table runs past the end of the script")
    return Script("smds", data, tuple(entries))


def parse_bank(data: bytes) -> Script:
    """SoundBank: effect count +0x12, id +0x14, per-effect volumes at +0x18,
    two channel data offsets per effect from +0x20 (0: no channel)."""
    if len(data) < 0x20 or data[:4] != b"seds":
        raise SequenceError(0, "not an effect bank header")
    effects = _u16(data, 0x12)
    if 0x20 + 4 * effects > len(data) or _u16(data, 0x18) + effects > len(data):
        raise SequenceError(0x12, "effect table runs past the end of the bank")
    entries = []
    for effect in range(effects):
        for part in range(2):
            offset = _u16(data, 0x20 + 4 * effect + 2 * part)
            if offset:
                if offset >= len(data):
                    raise SequenceError(
                        0x20 + 4 * effect + 2 * part, "effect data offset outside the bank"
                    )
                entries.append((f"effect {effect} part {part}", offset))
    return Script("seds", data, tuple(entries))


def locate(container: bytes) -> tuple[list[tuple[int, Script]], Counter]:
    """Scripts at word-aligned magics: banks the driver would accept
    (sound_check_file), sequences whose header fits."""
    found, rejected = [], Counter()
    for magic in (b"smds", b"seds"):
        position = container.find(magic)
        while position >= 0:
            if position % 4 == 0 and position + 0x10 <= len(container):
                size = struct.unpack_from("<I", container, position + 8)[0]
                blob = container[position : position + size]
                if size < 0x20 or size > len(container) - position:
                    rejected[f"{magic.decode()} size outside its container"] += 1
                elif magic == b"seds" and (_u16(blob, 0xC) != 0x101 or word_sum(blob, size)):
                    rejected["seds failing the driver's checksum/version test"] += 1
                else:
                    try:
                        script = (parse_sequence if magic == b"smds" else parse_bank)(blob)
                    except SequenceError:
                        rejected[f"{magic.decode()} header inconsistent"] += 1
                    else:
                        found.append((position, script))
            position = container.find(magic, position + 1)
    return found, rejected


# Original tables in the boot program, checked against this module.
sound_seq_opcode_handlers = 0x80050624  # opcode handlers
sound_seq_opcode_lengths = 0x80050824  # look-ahead lengths
sound_note_durations = 0x800509B0  # key -> ticks
sound_note_semitones = 0x80050A94  # key -> semitone


def exe_bytes(exe: bytes, address: int, size: int) -> bytes:
    base = struct.unpack_from("<I", exe, 0x18)[0]  # PS-X EXE text address, after a 0x800 header
    return exe[address - base + 0x800 : address - base + 0x800 + size]


def check_driver_tables(exe: bytes) -> list[str]:
    """Differences between the boot program's tables and this module."""
    problems = []
    handlers = struct.unpack("<128I", exe_bytes(exe, sound_seq_opcode_handlers, 512))
    lengths = exe_bytes(exe, sound_seq_opcode_lengths, 128)
    for code in range(0x80, 0x100):
        expected = OPCODES[code].handler if code in OPCODES else UNUSED_HANDLER
        if handlers[code - 0x80] != expected:
            problems.append(
                f"sound_seq_opcode_handlers[{code:#x}] = {handlers[code - 0x80]:08x}, module {expected:08x}"
            )
        if lengths[code - 0x80] != lookahead_length(code):
            problems.append(
                f"sound_seq_opcode_lengths[{code:#x}] = {lengths[code - 0x80]}, module {lookahead_length(code)}"
            )
    ticks, semitones = exe_bytes(exe, sound_note_durations, NOTE_KEYS), exe_bytes(exe, sound_note_semitones, NOTE_KEYS)
    for key in range(NOTE_KEYS):
        if ticks[key] != DURATIONS[key % 19] or semitones[key] != key // 19:
            problems.append(f"note tables differ at key {key:#x}")
            break
    return problems


@dataclass
class Sweep:
    containers: int = 0
    located: Counter = field(default_factory=Counter)
    distinct: Counter = field(default_factory=Counter)
    rejected: Counter = field(default_factory=Counter)
    channels: int = 0
    instructions: int = 0
    uses: Counter = field(default_factory=Counter)
    unknown: list = field(default_factory=list)
    unreferenced: Counter = field(default_factory=Counter)
    unreferenced_errors: list = field(default_factory=list)
    table_problems: list = field(default_factory=list)
    disc_codes: dict = field(default_factory=dict)  # disc -> opcodes its scripts use
    waves: Counter = field(default_factory=Counter)  # (opcode, sound_modulator_waves index) installed
    modulators: Counter = field(default_factory=Counter)  # F0's modulator indices

    def add(self, where: str, script: Script) -> set:
        """Decode the script's channels into the counts; returns the opcodes
        (and "note") they use."""
        covered = bytearray(len(script.data))
        codes = set()
        for label, start in script.entries:
            self.channels += 1
            try:
                instructions = decode_channel(script.data, start)
            except SequenceError as error:
                self.unknown.append(f"{where} {label}: {error}")
                continue
            self.instructions += len(instructions)
            used = [i.code if i.code >= 0x80 else "note" for i in instructions]
            self.uses.update(used)
            codes.update(used)
            for instruction in instructions:
                wave = installed_wave(instruction)
                if wave is not None:
                    self.waves[(instruction.code, wave)] += 1
                if instruction.code == 0xF0:
                    self.modulators[dict(instruction.operands)["index"]] += 1
            end = instructions[-1].offset + instructions[-1].length
            covered[start:end] = b"\1" * (end - start)
        if script.entries:
            first = min(start for _, start in script.entries)
            self.unreached(where, script, covered, first)
        return codes

    def unreached(self, where: str, script: Script, covered: bytearray, first: int) -> None:
        """Count the nonzero bytes after the first channel that no channel
        reaches and decode each run of them as channels of its own (data the
        driver never reads, kept out of the use counts)."""
        data, position, dead = script.data, first, 0
        while position < len(data):
            if covered[position] or not data[position]:
                position += 1
                continue
            end = position
            while end < len(data) and not covered[end]:
                end += 1
            dead += sum(1 for byte in data[position:end] if byte)
            while position < end:
                if not data[position]:
                    position += 1
                    continue
                try:
                    instructions = decode_channel(data, position)
                except SequenceError as error:
                    self.unreferenced_errors.append(f"{where}: {error}")
                    break
                self.unreferenced[f"{script.kind} channels"] += 1
                position = instructions[-1].offset + instructions[-1].length
            position = max(position, end)
        if dead:
            self.unreferenced[script.kind] += dead
            self.unreferenced[f"{script.kind} scripts"] += 1


def archive_offsets(data: bytes, size: int | None = None) -> tuple[int, ...]:
    """Entry offsets when the first `size` bytes of `data` read as an offset
    archive (8003342c: a u32 count, then that many ascending u32 offsets past
    the table and inside it); () otherwise."""
    size = len(data) if size is None else size
    if size < 8:
        return ()
    count = struct.unpack_from("<I", data, 0)[0]
    if not 0 < count < 0x1000 or 4 + 4 * count > size:
        return ()
    offsets = struct.unpack_from(f"<{count}I", data, 4)
    if offsets[0] < 4 + 4 * count or offsets[-1] > size or list(offsets) != sorted(offsets):
        return ()
    return offsets


def containers(disc):
    """(label, bytes) of the boot program, every file, the files that unpack,
    and the archive entries (of a file or its unpacked form) that unpack:
    80032e88 unpacks an entry from its offset, its stream possibly reading
    the final flag byte past the entry."""
    yield f"disc{disc.number} boot", disc.boot
    for entry in disc.files():
        where = f"disc{disc.number} slot {entry['slot']}"
        sectors = disc.sectors(entry["slot"])
        yield where, sectors[: entry["size"]]
        try:
            unpacked = decode_block(sectors).data
        except PackedError:
            unpacked = None
        else:
            yield f"{where} unpacked", unpacked
        for label, data, size in (
            (where, sectors, entry["size"]),
            (f"{where} unpacked", unpacked, None),
        ):
            if data is None:
                continue
            view = memoryview(data)
            for index, offset in enumerate(archive_offsets(data, size)):
                try:
                    yield f"{label} entry {index} unpacked", decode_block(view[offset:]).data
                except PackedError:
                    pass


def sweep() -> Sweep:
    result, seen = Sweep(), {}  # digest -> opcodes the script uses
    for disc in discs():
        result.table_problems += [
            f"disc {disc.number}: {p}" for p in check_driver_tables(disc.boot)
        ]
        disc_codes = result.disc_codes.setdefault(disc.number, set())
        for where, container in containers(disc):
            result.containers += 1
            found, rejected = locate(container)
            result.rejected.update(rejected)
            for position, script in found:
                result.located[script.kind] += 1
                digest = hashlib.sha256(script.data).digest()
                if digest not in seen:
                    result.distinct[script.kind] += 1
                    seen[digest] = result.add(f"{where} +0x{position:x}", script)
                disc_codes.update(seen[digest])
    return result


def report(result: Sweep) -> str:
    lines = ["sound sequence sweep (both discs)"]
    lines.append(
        "  driver tables (sound_seq_opcode_handlers/sound_seq_opcode_lengths/sound_note_durations/sound_note_semitones) vs module: "
        + ("match" if not result.table_problems else f"{len(result.table_problems)} differences")
    )
    lines += [f"    {p}" for p in result.table_problems]
    lines.append(
        "  containers scanned (boot programs, files, unpacked files and archive entries): "
        f"{result.containers}"
    )
    for kind, name in (("smds", "sequences"), ("seds", "effect banks")):
        lines.append(
            f"  {name}: {result.located[kind]} located, {result.distinct[kind]} distinct decoded"
        )
    for reason, count in sorted(result.rejected.items()):
        lines.append(f"  magic not taken as a script ({reason}): {count}")
    lines.append(f"  channels: {result.channels}, instructions: {result.instructions}")
    used = sorted(code for code in result.uses if code != "note")
    lines.append(f"  opcodes defined: {len(OPCODES)} + note; used: {len(used)} + note")
    lines.append(
        "    used per disc: "
        + ", ".join(
            f"disc {disc} {len(codes - {'note'})} + note"
            for disc, codes in sorted(result.disc_codes.items())
        )
    )
    lines.append(f"    note: {result.uses['note']}")
    for code in used:
        lines.append(f"    {code:02x} {OPCODES[code].mnemonic}: {result.uses[code]}")
    unused = [f"{code:02x}" for code in sorted(OPCODES) if code not in result.uses]
    lines.append(f"  defined but unused: {' '.join(unused)}")
    lines.append(f"  modulator waves installed (sound_modulator_waves[mode & 0xf], {WAVE_SLOTS} slots):")
    for code in WAVE_OPCODES:
        shapes = {shape: n for (op, shape), n in sorted(result.waves.items()) if op == code}
        text = " ".join(f"{shape}:{n}" for shape, n in shapes.items()) or "none"
        lines.append(f"    {code:02x} {OPCODES[code].mnemonic}: {text}")
    indices = " ".join(f"{i}:{n}" for i, n in sorted(result.modulators.items())) or "none"
    past = sum(n for i, n in result.modulators.items() if i >= MODULATORS)
    lines.append(
        f"  f0 modulator indices (modulator[{MODULATORS}]): {indices}; past the array: {past}"
    )
    lines.append(f"  unknown/undecodable: {len(result.unknown)}")
    lines += [f"    {u}" for u in result.unknown]
    for kind in ("smds", "seds"):
        if result.unreferenced[kind]:
            lines.append(
                f"  nonzero bytes no channel reaches ({kind}): {result.unreferenced[kind]} in "
                f"{result.unreferenced[kind + ' scripts']} scripts, decoding as "
                f"{result.unreferenced[kind + ' channels']} whole channels"
            )
    if result.unreferenced_errors:
        lines.append(f"  unreached bytes that do not decode: {len(result.unreferenced_errors)}")
        lines += [f"    {u}" for u in result.unreferenced_errors]
    return "\n".join(lines)


def listing(script: Script) -> str:
    lines = []
    for label, start in script.entries:
        lines.append(f"{label} @ +0x{start:x}")
        try:
            instructions = decode_channel(script.data, start)
        except SequenceError as error:
            lines.append(f"  error {error}")
            continue
        for i in instructions:
            args = ", ".join(f"{name}={value}" for name, value in i.operands)
            lines.append(f"  {i.offset:05x}  {i.mnemonic} {args}".rstrip())
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("file", nargs="?", help="a local file holding a script (listing to stdout)")
    parser.add_argument("--offset", type=lambda v: int(v, 0), default=0)
    parser.add_argument("--sweep", action="store_true")
    args = parser.parse_args(argv)
    if args.sweep:
        result = sweep()
        print(report(result))
        return 1 if result.unknown or result.table_problems else 0
    if not args.file:
        parser.error("give --sweep or a file")
    with open(args.file, "rb") as handle:
        data = handle.read()[args.offset :]
    size = struct.unpack_from("<I", data, 8)[0]
    script = (parse_sequence if data[:4] == b"smds" else parse_bank)(data[:size])
    print(listing(script))
    return 0


if __name__ == "__main__":
    sys.exit(main())
