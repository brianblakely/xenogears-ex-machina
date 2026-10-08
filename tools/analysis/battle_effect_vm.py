"""Battle effect-script VM: opcode tables, decoder and both-disc sweep.

Battle objects (the stage model, stage object sets, gears and their parts, 3D
enemies, and the copies scripts create) run effect scripts in the battle
overlay's 800aad54 (decomp/src/battle/battle_8009E53C.c, switch on
jtbl_8007056c, opcodes 00-75; still a NON_MATCHING C draft whose operand reads
match the original's pc increments case by case). ovl2143's 801e39f0
(decomp/src/ovl2143/ovl2143.c, matching, opcodes 00-70) runs the same format
for the field and menu gear models with a reduced command set: its no-op
cases consume no parameter words, so the two dialects decode differently.

A command is a signed 16-bit word: opcode in the low byte, argument in the
high byte, then the opcode's u16 parameter words (a word named a_b holds a in
its high byte and b in its low byte). Jump offsets are signed and count bytes
from the command's start. Unknown opcodes (default case) stop the script on
themselves; they are reported, never skipped.

Scripts live in relocatable blocks (8003342c: a u32 count, then that many
offsets from the block's start): a script table's first offset is its
animation table, the others its scripts (offset 0 marks an empty id).
"""

from __future__ import annotations

import argparse
import glob
import hashlib
import json
import struct
from collections import Counter
from dataclasses import dataclass, field, replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Control flow kinds.
NEXT = "next"  # continues with the following command
END = "end"  # 00: the script stays on this command
RETURN = "return"  # the interpreter returns without storing the position
HANDOVER = "handover"  # 14: returns when the argument is 0xFD, else NEXT
JUMP = "jump"  # to start + offset
BRANCH = "branch"  # to start + offset or the following command
LOOP = "loop"  # 31: back past the 30 command at start + offset, or on
DEFER = "defer"  # a later jump to start + offset (argument nonzero), then on
EVENTS = "events"  # 63: animation events follow; jump to start + offset


@dataclass(frozen=True)
class Opcode:
    name: str
    operands: tuple[str, ...] = ()
    flow: str = NEXT
    effect: str = ""
    handler: str = ""  # the switch case it was read from

    @property
    def length(self) -> int:
        return 2 + 2 * len(self.operands)


def _op(name, effect, operands=(), flow=NEXT):
    return Opcode(name, tuple(operands), flow, effect)


XYZ = ("x", "y", "z")

# 800aad54 (battle overlay), read from its switch cases.
BATTLE = {
    0x00: _op("end", "stop; the script stays on this command", flow=END),
    0x01: _op(
        "wait", "wait `frames` frames (counter at 0x40; just started: one frame)", ["frames"]
    ),
    0x02: _op(
        "popup_wait_or_menu_step",
        "byte 0x35 set (acting for an event script, 800aa384): wait while a number popup shows "
        "(800bf6f8), clear 80059464, 800591ac and 0x35, report done to the event thread "
        "(80080c6c); else count a battle menu step (800b9258) after the run",
    ),
    0x03: _op("popup_wait", "as 02 without the menu step"),
    0x04: _op(
        "load_extra",
        "no extra file yet: read file arg + 800c3530[file] of directory 0x28/2 as the extra file",
        ["file"],
    ),
    0x05: _op(
        "setup_extra",
        "wait for the disc, relocate the extra file, upload its images and sound bank and take "
        "its animations (ids from 64); byte 0x23 set: wait for the disc and clear it",
    ),
    0x06: _op("free_extra", "free the extra file and its sound bank (800b0060)"),
    0x07: _op("stop_disc_read", "request a disc read stop (8002a498(0))"),
    0x08: _op(
        "stop_effects", "release the parts' non-persistent effects (800a2acc), stop the animation"
    ),
    0x09: _op("release_kind", "release every part's effects of kind arg (800a2bb8)"),
    0x0A: _op("release_part", "release part arg's attached effects 0-2 (800a216c mask 7)"),
    0x0B: _op(
        "reset_parts", "release the parts' effects, zero each part's rotation and translation"
    ),
    0x0C: _op("stop_motion", "clear spin, spin acceleration, drift and drift acceleration"),
    0x0D: _op("release_part_rotation", "release part arg's attached effect 0 (mask 1)"),
    0x0E: _op("release_part_translation", "release part arg's attached effect 1 (mask 2)"),
    0x0F: _op(
        "stop_camera_effects", "camera mode 1 (800bcad0), release the effect entries (800b00f4)"
    ),
    0x10: _op("pose", "apply animation arg (800af518) as a frame (800a1b50)"),
    0x11: _op(
        "animate",
        "start animation arg (800a2434; mode high byte, tag low byte), take its span, start its "
        "events (800ae1bc)",
        ["mode_tag"],
    ),
    0x12: _op(
        "animate_from_start",
        "as 11 through 800a2704 (tracks from their start values), no events",
        ["mode_tag"],
    ),
    0x13: _op(
        "tween_to_frame",
        "tween the parts toward animation (low byte of tag_index) as a frame in mode arg "
        "(800a1cf4)",
        ["tag_index", "duration_smooth"],
    ),
    0x14: _op(
        "call",
        "start script (high byte) of the source (low byte; 0xFF each object) on the objects of "
        "target code arg; arg 0xFD hands over and returns",
        ["script_source"],
        HANDOVER,
    ),
    0x15: _op(
        "clone",
        "copy the object into a free slot 13-1e with its own hierarchy; part `part` and its "
        "descendants show on the copy only (800af180); start script arg on it (0xFF none)",
        ["part"],
    ),
    0x16: _op(
        "merge_back",
        "show the parts on the original object again (800af270), free this one and start "
        "script arg on the original (0xFF none); returns",
        flow=RETURN,
    ),
    0x17: _op("free_self", "free this object (800a9ff0); returns", flow=RETURN),
    0x18: _op(
        "object_animation",
        "start the object's animation arg (800ae1bc), looping when `loop` is set",
        ["loop"],
    ),
    0x19: _op("stop_animation", "stop the object's animation (800aeeec)"),
    0x1A: _op(
        "move_image",
        "MoveImage the rectangle (width rounded to even) to (dst_x, dst_y); arg bit 0: relative "
        "to the object's image placement",
        ["x", "y", "dst_x", "dst_y", "w", "h"],
    ),
    0x1B: _op(
        "image_animation",
        "start image animation arg (800a3640) from a source animation, mode, step handler "
        "(800aa820), rectangles and parameters (15 words read even when arg is out of range)",
        [
            "mode_source",
            "step_flags",
            "x",
            "y",
            "z",
            "x2",
            "y2",
            "z2",
            "x3",
            "y3",
            "p0",
            "p1",
            "p2",
            "p3",
            "p4",
        ],
    ),
    0x1C: _op("stop_image_animation", "stop image animation arg (800a429c)"),
    0x1D: _op(
        "tween_part",
        "tween part arg from start to end over `duration` (800af678; flags/mode and "
        "kind/field1 bytes)",
        [
            "mode_flags",
            "field1_kind",
            "start_x",
            "start_y",
            "start_z",
            "end_x",
            "end_y",
            "end_z",
            "duration",
        ],
    ),
    0x1E: _op("scaled_hierarchy", "byte 0x37 = arg: pose with per-part scales (8009f1c4)"),
    0x1F: _op(
        "select_object",
        "continue with the object of target code arg (800af438) when present; it receives the "
        "script position",
    ),
    0x20: _op("wait_tweens", "wait while a tween runs (step flag 0x100)"),
    0x21: _op("wait_tag", "tag (0x3c) = arg; wait while a tween with the tag runs (step flag 1)"),
    0x22: _op(
        "wait_loops",
        "wait for `count` tween loops (flag 0x400; arg != 0xFF: tag = arg, flag 4)",
        ["count"],
    ),
    0x23: _op(
        "show_part",
        "show or hide part `part` by arg bit 0 (bit 7: its descendants, 800afa98)",
        ["part"],
    ),
    0x24: _op("show", "active (0x34) = arg bit 0"),
    0x25: _op(
        "attach",
        "attach the objects of the target code (low byte) to part (high byte) at the offset, or "
        "where they are (arg bit 0); arg bit 1 turns them with the part",
        ["part_code", *XYZ],
    ),
    0x26: _op("detach", "detach the objects of target code arg"),
    0x27: _op("compose", "recompose the hierarchy (8009f1c4 or 8009ef3c by byte 0x37)"),
    0x28: _op(
        "wait_effect_near",
        "wait until part (low byte)'s effect with time (high byte) runs and distance / 0x8e < arg",
        ["time_part"],
    ),
    0x29: _op("wait_effect_far", "as 28 until distance / 0x8e >= arg", ["time_part"]),
    0x2A: _op("wait_near", "wait while the distance to the position is >= 0x8e"),
    0x2B: _op("wait_far", "wait while the distance to the position is <= 0x8e"),
    0x2C: _op("wait_point_near", "wait while the distance to the point is >= 0x8e", XYZ),
    0x2D: _op("wait_point_far", "wait while the distance to the point is <= 0x8e", XYZ),
    0x2E: _op(
        "on_near", "jump once within 0x8e of the position (arg 0: cancel)", ["offset"], DEFER
    ),
    0x2F: _op("wait_popup", "wait while a number popup shows (800bf6f8)"),
    0x30: _op(
        "loop_start", "loop counter (the operand word) = 0; arg is the loop count", ["counter"]
    ),
    0x31: _op(
        "loop",
        "count the 30 command at start + offset; back past it while below its count",
        ["offset"],
        LOOP,
    ),
    0x32: _op("jump", "jump", ["offset"], JUMP),
    0x33: _op(
        "jump_if_start_mode",
        "jump when the battle start mode (800d36b8) is nonzero",
        ["offset"],
        BRANCH,
    ),
    0x34: _op("jump_if_byte22_clear", "jump when byte 0x22 is 0", ["offset"], BRANCH),
    0x35: _op("jump_random", "jump when rand() >= 0x4000", ["offset"], BRANCH),
    0x36: _op(
        "on_timer", "jump after `frames` frames (arg 0: cancel)", ["frames", "offset"], DEFER
    ),
    0x37: _op("on_ground", "jump on reaching the ground (arg 0: cancel)", ["offset"], DEFER),
    0x38: _op(
        "move_to_position",
        "movement tween (800ae098 kind 7; values arg and the low byte, duration the high byte) "
        "of the root toward the position",
        ["duration_value"],
    ),
    0x39: _op("move_to_position_8", "as 38 with kind 8", ["duration_value"]),
    0x3A: _op(
        "wait_popup_when_idle",
        "wait while a number popup shows when the next event is the end, byte 0x22 is 0 or "
        "0x35 is set",
    ),
    0x3B: _op(
        "jump_if_slot_flag",
        "jump when the object's slot bit of 800c48e8 is set",
        ["offset"],
        BRANCH,
    ),
    0x3C: _op(
        "sound",
        "play sound (low byte) of bank source arg (800ae220), parameter (high byte)",
        ["param_sound"],
    ),
    0x3D: _op("replay_queue_if", "when arg is among the queued scripts: run the queue and return"),
    0x3E: _op(
        "jump_if_all_hit",
        "jump when every selected target's event code class (0: codes 0-1, 5: 2, 3, 5, 4: others) "
        "is arg; arg 8: each target object has flag 2",
        ["offset"],
        BRANCH,
    ),
    0x3F: _op("menu_update", "update the battle menu when open (800bf6cc)"),
    0x40: _op("turn_to", "turn the root to the angles over arg frames (800adf1c)", XYZ),
    0x41: _op("turn_by", "turn the root by the angles over arg frames", XYZ),
    0x42: _op("face_position", "turn the root toward the position over arg frames"),
    0x43: _op("face_position_yaw", "as 42, heading only"),
    0x44: _op("set_spin", "spin = (x, y, z)", XYZ),
    0x45: _op("add_spin", "spin += (x, y, z)", XYZ),
    0x46: _op("set_spin_acceleration", "spin acceleration = (x, y, z)", XYZ),
    0x47: _op("add_spin_acceleration", "spin acceleration += (x, y, z)", XYZ),
    0x48: _op("set_byte36", "byte 0x36 = arg"),
    0x49: _op("place", "put the root at (x, y, z)", XYZ),
    0x4A: _op(
        "place_at",
        "arg 0xFB: put the root at distance 0x8e from the position; else at x, z of target "
        "code arg",
    ),
    0x4B: _op("set_drift", "drift = (x, y, z)", XYZ),
    0x4C: _op("add_drift", "drift += (x, y, z)", XYZ),
    0x4D: _op("set_drift_acceleration", "drift acceleration = (x, y, z)", XYZ),
    0x4E: _op("add_drift_acceleration", "drift acceleration += (x, y, z)", XYZ),
    0x4F: _op("drift_to_position", "drift speed that reaches the position in arg frames"),
    0x50: _op("set_position", "position = (x, y, z); stop following", XYZ),
    0x51: _op("position_at_slot", "position = the battle position of target code arg"),
    0x52: _op(
        "follow", "follow target code arg at part `part` and offset (800aef68)", ["part", *XYZ]
    ),
    0x53: _op("position_on_ground", "put the position on the scene ground"),
    0x54: _op("set_distance", "0x8e = distance scaled by the object's scale", ["distance"]),
    0x55: _op("add_distance", "0x8e += distance scaled by the object's scale", ["distance"]),
    0x56: _op("add_distance_raw", "0x8e += distance", ["distance"]),
    0x57: _op(
        "add_slot_size", "0x8e += the scaled size of the object of target code arg (800aa650)"
    ),
    0x58: _op("follow_target", "follow target code arg at its root"),
    0x59: _op("position_at_camera_eye", "position = camera preset arg's eye"),
    0x5A: _op("position_at_camera_target", "position = camera preset arg's look-at point"),
    0x5B: _op("queue_mode", "queue state (0x2b) = arg; 2 runs the queued scripts and returns"),
    0x5C: _op("jump_if_at_position", "jump when the root is at the position", ["offset"], BRANCH),
    0x5D: _op("billboard", "part `part`'s billboard mode (0x52) = arg", ["part"]),
    0x5E: _op("set_scale", "object scale (0x1c) = scale", ["scale"]),
    0x5F: _op("set_flags", "object flags (0x4a) = flags", ["flags"]),
    0x60: _op("position_at_area", "position = the centre of the slot's formation area"),
    0x61: _op(
        "jump_if_group",
        "jump when the slot's group has members among flagged groups (800885d0)",
        ["offset"],
        BRANCH,
    ),
    0x62: _op(
        "set_part_transform",
        "set or add part `part`'s rotation, translation or scale (800afd98, mode arg)",
        ["part", *XYZ],
    ),
    0x63: _op(
        "object_events",
        "run arg animation events from the following bytes (800ae2a4); jump past them",
        ["offset"],
        EVENTS,
    ),
    0x64: _op("set_word3e", "halfword 0x3e = value", ["value"]),
    0x65: _op(
        "camera_from_target",
        "start effect entry 7 (800b0164) from the camera look-at point toward the position "
        "(code f6), a camera preset (f5 look-at, f4 eye) or the object of a target code",
        ["code_param", "field12_camera", "value"],
    ),
    0x66: _op(
        "camera_from_eye",
        "as 65 with entry 8 from the camera eye",
        ["code_param", "field12_camera", "value"],
    ),
    0x67: _op(
        "camera_turn",
        "start camera entry (high byte: 0 orbit yaw, 1 look-at yaw, 2 orbit pitch, 3 orbit "
        "distance, 4 look-at distance, 5 look-at height, 6 orbit height) from its value or the "
        "given angle to the end value (800b0164)",
        ["channel_param", "field12_flags", "angle", "end"],
    ),
    0x68: _op(
        "camera_start", "release the effect entries, camera mode 4, effects run, reset the orbit"
    ),
    0x69: _op("camera_wait", "post camera request arg (800c3b84), then wait until it completes"),
    0x6A: _op("camera_snap", "camera channels 7 and 8 start at their targets (800c3b8c = 1)"),
    0x6B: _op("rotation_order", "part `part` uses RotMatrixYXZ (byte 6) = arg", ["part"]),
    0x6C: _op("wait_disc", "wait while the disc is busy (800286cc)"),
    0x6D: _op("set_byte38", "byte 0x38 = arg bit 0"),
    0x6E: _op(
        "wait_object_byte38",
        "wait while the object of target code arg has byte 0x38 == value bit 0",
        ["value"],
    ),
    0x6F: _op("mark_targets", "halfword 0x3a = the selected targets (800c3e30), or -1 for arg 0"),
    0x70: _op(
        "jump_if_same_targets",
        "jump and yield when 0x3a equals the selected targets",
        ["offset"],
        BRANCH,
    ),
    0x71: _op("set_sound_request", "arg 0: the sound request (800d39e4) = sound", ["sound"]),
    0x72: _op("wait_sound_request", "yield; repeat until the sound request is done (800591b1)"),
    0x73: _op("play_sound_request", "yield after requesting sound 800d39e4 (800b8054)"),
    0x74: _op("count_hit", "count an effect hit (800bf998); returns", flow=RETURN),
    0x75: _op(
        "jump_if_facing", "jump when the root's yaw is the heading 43 turns to", ["offset"], BRANCH
    ),
}

# 801e39f0 (ovl2143): the battle commands it keeps, with these differences.
# Its no-op cases (no effect, no parameter words).
_NOOP = bytes.fromhex(
    "04 05 06 07 09 0f 12 1b 1c 2c 2d 2f 3a 3e 3f 51 52 53 58 59 5a 60 61 65 66 67 68 69 6a"
)
MODEL_VIEWER = {code: op for code, op in BATTLE.items() if code <= 0x70}
MODEL_VIEWER.update({code: _op("nop", "no effect; no parameter words") for code in _NOOP})
MODEL_VIEWER.update(
    {
        0x02: _op("call_800796f4", "call 800796f4 after the run (empty in the field overlay)"),
        0x03: _op("call_800796f4", "call 800796f4 after the run (empty in the field overlay)"),
        0x13: _op(
            "tween_to_frame",
            "tween toward animation (low byte) as a frame in mode arg; 801e85cc set: pose at once",
            ["tag_index", "duration_smooth"],
        ),
        0x14: _op(
            "call",
            "start script (high byte) of the source (low byte; 0xFF each actor) on the actors "
            "of code arg (eight); arg 0xFD hands over and returns",
            ["script_source"],
            HANDOVER,
        ),
        0x15: _op(
            "clone",
            "copy the actor into free slot 8 or 9 and start script arg on it (0xFF none)",
            ["part"],
        ),
        0x33: _op("skip_word", "consume one word", ["unused"]),
        0x34: _op("skip_word", "consume one word", ["unused"]),
        0x3B: _op("skip_word", "consume one word", ["unused"]),
        0x4A: _op("place_at", "arg 0xFB: put the root at distance 0x8e from the target"),
    }
)

HANDLERS = {"battle": "800aad54", "ovl2143": "801e39f0"}
BATTLE = {code: replace(op, handler=f"800aad54 case {code:02x}") for code, op in BATTLE.items()}
MODEL_VIEWER = {
    code: replace(op, handler=f"801e39f0 case {code:02x}") for code, op in MODEL_VIEWER.items()
}
DIALECTS = {"battle": BATTLE, "ovl2143": MODEL_VIEWER}


class EffectError(ValueError):
    """A script does not decode under the recovered handlers."""


@dataclass(frozen=True)
class Instruction:
    offset: int
    opcode: int
    argument: int
    operands: tuple[int, ...]
    name: str
    length: int
    successors: tuple[int, ...]
    data: tuple[int, int] | None = None  # event bytes of 63

    def text(self) -> str:
        words = " ".join(f"{w:04x}" for w in self.operands)
        return (
            f"{self.offset:06x}: {self.opcode:02x} {self.argument:02x} {self.name} {words}".rstrip()
        )


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def decode(code: bytes, offset: int, dialect: str = "battle") -> Instruction:
    """Decode the command at offset; raise EffectError for unknown or truncated ones."""
    table = DIALECTS[dialect]
    if offset & 1 or offset < 0 or offset + 2 > len(code):
        raise EffectError(f"command offset 0x{offset:x} misaligned or outside the code")
    word = code[offset] | code[offset + 1] << 8
    op, arg = word & 0xFF, word >> 8
    if op not in table:
        raise EffectError(f"unknown {dialect} opcode 0x{op:02x} at 0x{offset:x}")
    spec = table[op]
    end = offset + spec.length
    if end > len(code):
        raise EffectError(f"opcode 0x{op:02x} at 0x{offset:x} runs past the code")
    operands = struct.unpack_from(f"<{len(spec.operands)}H", code, offset + 2)
    target = (
        offset + signed16(operands[-1])
        if spec.flow in (JUMP, BRANCH, LOOP, DEFER, EVENTS)
        else None
    )
    data = None
    if spec.flow == NEXT:
        successors = (end,)
    elif spec.flow in (END, RETURN):
        successors = ()
    elif spec.flow == HANDOVER:
        successors = () if arg == 0xFD else (end,)
    elif spec.flow == JUMP:
        successors = (target,)
    elif spec.flow == BRANCH:
        successors = (end, target)
    elif spec.flow == LOOP:
        successors = (end, target + 4)
    elif spec.flow == DEFER:
        successors = (end, target) if arg else (end,)
    else:  # EVENTS
        if target < end:
            raise EffectError(f"events of 0x{offset:x} end before the command")
        successors, data = (target,), (end, target)
    return Instruction(offset, op, arg, operands, spec.name, spec.length, successors, data)


@dataclass
class Listing:
    instructions: dict[int, Instruction] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)


def disassemble(code: bytes, entries, dialect: str = "battle") -> Listing:
    """Follow every path from the entry offsets; report conflicts instead of guessing."""
    listing = Listing()
    owner: dict[int, int] = {}
    pending = sorted(set(entries))
    while pending:
        offset = pending.pop()
        if offset in listing.instructions:
            continue
        if offset in owner:
            listing.errors.append(f"0x{offset:x} starts inside the command at 0x{owner[offset]:x}")
            continue
        try:
            insn = decode(code, offset, dialect)
        except EffectError as error:
            listing.errors.append(str(error))
            continue
        span = range(offset, offset + insn.length)
        if insn.data:
            span = range(offset, insn.data[1])
        clash = [b for b in span if b in owner]
        if clash:
            listing.errors.append(f"0x{offset:x} overlaps the command at 0x{owner[clash[0]]:x}")
            continue
        for b in span:
            owner[b] = offset
        listing.instructions[offset] = insn
        if insn.opcode == 0x31:
            counter = offset + signed16(insn.operands[0])
            if counter + 4 > len(code) or code[counter] != 0x30:
                listing.errors.append(f"loop at 0x{offset:x} does not count a 30 command")
        pending.extend(s for s in insn.successors if s not in listing.instructions)
    return listing


def script_table(data: bytes, base: int) -> tuple[int, list[int]]:
    """A relocatable script table at base: its animation table and script offsets (0: empty)."""
    count = struct.unpack_from("<I", data, base)[0]
    if not 1 <= count <= 0x100 or base + 4 + 4 * count > len(data):
        raise EffectError(f"no script table at 0x{base:x}")
    offsets = struct.unpack_from(f"<{count}I", data, base + 4)
    scripts = [base + o if o else 0 for o in offsets[1:]]
    if any(not base < s < len(data) for s in scripts if s):
        raise EffectError(f"script table at 0x{base:x} points outside its file")
    return base + offsets[0], scripts


def script_file(data: bytes, base: int = 0) -> tuple[int, list[int]]:
    """An object script file (count, scripts table, data) at base: its script table."""
    count = struct.unpack_from("<I", data, base)[0]
    if count < 2 or base + 12 > len(data):
        raise EffectError(f"no object script file at 0x{base:x}")
    return script_table(data, base + struct.unpack_from("<I", data, base + 4)[0])


class Disc:
    """Numbered files of one extracted disc, addressed as the resident loader does.

    80028470 selects directory group + index of the table at 80018004 (the boot
    executable's copy of sector 40); 80028738 reads index entry file + entry - 2.
    """

    def __init__(self, root: Path, name: str):
        self.root = root / ".local/extract" / name
        manifest = json.loads((self.root / "manifest.json").read_text())
        self.entries = {entry["slot"]: entry for entry in manifest["files"]}
        exe = Path(glob.glob(str(self.root / "SLUS_*"))[0]).read_bytes()
        self.directories = struct.unpack_from("<64H", exe, 0x800 + 0x8004)

    def slot(self, directory: int, file: int) -> int:
        return file + self.directories[directory] - 2

    def size(self, directory: int, file: int) -> int:
        return self.entries[self.slot(directory, file)]["size"]

    def read(self, directory: int, file: int) -> bytes:
        return (self.root / "files" / f"{self.slot(directory, file):04d}.bin").read_bytes()


# Directory table indices (group + index of 80028470).
ENEMY_SETS = 0x0C + 1  # 2n + 3: enemy set file (ovl2615 801e6314), n = formation byte 0
STAGES = 0x0C + 3  # 7 + 2s: stage scene data (ovl2615 801e7210, the motion block)
OBJECT_SETS = 0x28 + 0  # 2s + 2: stage object set scripts (800a96b4 / 800a979c)
GEARS = 0x28 + 1  # base + 2: gear scripts, base + 2 + v: part files (800a9540 / 800a979c)
EXTRAS = 0x28 + 2  # every file: extra script files (opcode 04)
MODELS = 0x04 + 0  # 0x6ba + 2k: field/menu gear model scripts run by ovl2143
# 800c3508: per gear the base file and variant count (the last gear has none).
GEAR_FILES = bytes.fromhex(
    "0100 0300 0506 0d00 0f03 1404 1a00 1c00 1e00 2000 2200 2404 2a03 2f04 3500 3700 3900 3b00"
    " 3d00 0000"
)


def file_tables(kind: str, data: bytes):
    """Yield (label, script table) for one file of the given kind."""
    if kind == "script":
        yield "", script_file(data)
    elif kind == "gear_part":
        # GearPartFile: part table (count first) and the parts' script file.
        table, model = struct.unpack_from("<II", data, 4)
        if struct.unpack_from("<h", data, table)[0] > 0:
            yield "", script_file(data, model)
    elif kind == "enemy":
        # 801e6314: count, 8-byte header, 12-byte entries; a model entry's own
        # script file sits at its offset inside the copied data.
        entries, end = data[0], struct.unpack_from("<I", data, 4)[0]
        for k in range(entries):
            offset, images, model = struct.unpack_from("<IIB", data, 8 + 12 * k)
            if model and images >= 8:
                if not 8 + 12 * entries <= offset < end:
                    raise EffectError(f"model entry {k} script outside the copied data")
                yield f".{k}", script_file(data, offset)
    elif kind == "scene":
        # 801e7210: the scene data follows a size word; its motion block
        # (offset at scene + 0x514) is the stage model's script table.
        yield "", script_table(data, 4 + struct.unpack_from("<i", data, 4 + 0x514)[0])
    else:
        raise ValueError(f"unknown file kind {kind}")


def disc_tables(disc: Disc):
    """Yield (dialect, family, label, data, script table) for every effect script table."""
    sets = (-disc.size(ENEMY_SETS, 1) - 1) // 2
    files = [("enemy", "enemy", f"set{n}", ENEMY_SETS, 2 * n + 3) for n in range(sets)]
    scenes = -disc.size(STAGES, 5) // 2
    files += [("stage", "scene", f"scene{s}", STAGES, 7 + 2 * s) for s in range(scenes)]
    files += [("object_set", "script", f"set{s}", OBJECT_SETS, 2 * s + 2) for s in range(3)]
    for gear in range(len(GEAR_FILES) // 2):
        base, variants = GEAR_FILES[2 * gear : 2 * gear + 2]
        if base:
            files.append(("gear", "script", f"gear{gear}", GEARS, base + 2))
            files += [
                ("gear_part", "gear_part", f"gear{gear}.{v}", GEARS, base + 2 + v)
                for v in range(1, variants + 1)
            ]
    files += [
        ("extra", "script", f"file{f}", EXTRAS, f) for f in range(2, -disc.size(EXTRAS, 1) + 2)
    ]
    # ovl2143: the field/menu model pairs after it (file 0x6b9) in its directory record.
    pairs = (-disc.size(MODELS, 0x6B8) - 1) // 2
    files += [("model", "script", f"model{k}", MODELS, 0x6BA + 2 * k) for k in range(pairs)]
    for family, kind, label, directory, number in files:
        data = disc.read(directory, number)
        for suffix, table in file_tables(kind, data):
            yield (
                ("ovl2143" if family == "model" else "battle"),
                family,
                label + suffix,
                data,
                table,
            )


@dataclass
class Sweep:
    tables: Counter = field(default_factory=Counter)
    scripts: Counter = field(default_factory=Counter)
    empty: Counter = field(default_factory=Counter)
    instructions: Counter = field(default_factory=Counter)
    opcodes: Counter = field(default_factory=Counter)
    errors: list[str] = field(default_factory=list)


def sweep(root: Path = ROOT) -> tuple[dict[str, Sweep], bool]:
    """Decode every table of both discs; also whether both discs hold the same files."""
    results = {"battle": Sweep(), "ovl2143": Sweep()}
    digests = []
    for name in ("disc1", "disc2"):
        digest = hashlib.sha256()
        for dialect, family, label, data, (_, scripts) in disc_tables(Disc(root, name)):
            digest.update(f"{family} {label}".encode() + data)
            result = results[dialect]
            live = [s for s in scripts if s]
            result.tables[family] += 1
            result.scripts[family] += len(live)
            result.empty[family] += len(scripts) - len(live)
            listing = disassemble(data, live, dialect)
            result.instructions[family] += len(listing.instructions)
            result.opcodes.update(i.opcode for i in listing.instructions.values())
            result.errors.extend(f"{name} {family} {label}: {e}" for e in listing.errors)
        digests.append(digest.digest())
    return results, digests[0] == digests[1]


def print_listing(data: bytes, kind: str, dialect: str) -> None:
    for label, (animations, scripts) in file_tables(kind, data):
        print(f"table{label}: animations at 0x{animations:x}")
        names = {}
        for index, offset in enumerate(scripts):
            if offset:
                names.setdefault(offset, f"script_{index:02x}")
        listing = disassemble(data, list(names), dialect)
        for offset in sorted(listing.instructions):
            if offset in names:
                print(f"{names[offset]}:")
            insn = listing.instructions[offset]
            print(
                f"  {insn.text()}"
                + (f"  ; events 0x{insn.data[0]:x}-0x{insn.data[1]:x}" if insn.data else "")
            )
        for error in listing.errors:
            print(f"  error: {error}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--sweep", action="store_true", help="decode every effect script of both discs"
    )
    parser.add_argument(
        "--list", type=Path, help="disassemble one extracted file (prints to stdout)"
    )
    parser.add_argument(
        "--kind", default="script", choices=("script", "gear_part", "enemy", "scene")
    )
    parser.add_argument("--dialect", default="battle", choices=tuple(DIALECTS))
    args = parser.parse_args()
    if args.list:
        print_listing(args.list.read_bytes(), args.kind, args.dialect)
        return
    if not args.sweep:
        parser.error("choose --sweep or --list")
    results, identical = sweep()
    print(f"both discs ({'identical' if identical else 'different'} script files), counts summed:")
    for dialect, result in results.items():
        table = DIALECTS[dialect]
        print(
            f"{dialect} ({HANDLERS[dialect]}): {sum(result.tables.values())} script tables, "
            f"{sum(result.scripts.values())} scripts ({sum(result.empty.values())} empty ids), "
            f"{sum(result.instructions.values())} instructions"
        )
        for family in result.tables:
            print(
                f"  {family}: {result.tables[family]} tables, {result.scripts[family]} scripts, "
                f"{result.instructions[family]} instructions"
            )
        used = sorted(result.opcodes)
        print(f"  opcodes defined {len(table)}, used {len(used)}")
        print("  " + " ".join(f"{op:02x}:{result.opcodes[op]}" for op in used))
        print(f"  unused: {' '.join(f'{op:02x}' for op in sorted(set(table) - set(used)))}")
        print(f"  unknown/undecodable: {len(result.errors)}")
        for error in result.errors:
            print(f"    {error}")


if __name__ == "__main__":
    main()
