"""Battle effect-script VM: opcode tables, decoder and both-disc sweep.

Battle objects (the stage model, stage object sets, gears and their parts, 3D
enemies, and the copies scripts create) run effect scripts in the battle
overlay's 800aad54 (decomp/src/battle/battle_scene.c, matching; switch on
jtbl_8007056c, opcodes 00-75). ovl2143's 801e39f0 (decomp/src/ovl2143/
gear_model_scene.c, matching, opcodes 00-70) runs the same format for the field and
menu gear models with a reduced command set: its no-op cases consume no
parameter words, so the two dialects decode differently.

A command is a signed 16-bit word: opcode in the low byte, argument in the
high byte, then the opcode's u16 parameter words (a word named a_b holds a in
its high byte and b in its low byte). Jump offsets are signed and count bytes
from the command's start. Unknown opcodes (default case) stop the script on
themselves; they are reported, never skipped. Command 63 embeds `arg`
animation events (800ae2a4's records, ovl2143: 801e5d44) between its operand
word and its jump target; they are decoded with it.

Scripts live in relocatable blocks (8003342c: a u32 count, then that many
offsets from the block's start). An object script file (count, script table,
data block) has a script table whose first offset is its animation table, the
others its scripts (offset 0 marks an empty id); a stage scene's motion block
holds the stage motions first and gives the stage model no animations. Each
animation has its event count at 0x12 and their offset at 0x14 (800ae1bc);
unused animation ids repeat an entry that points at a script or the data block.
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
from typing import NamedTuple

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
        "wait",
        "wait `frames` frames (counter at 0x40); a just-started script waits a frame first",
        ["frames"],
    ),
    0x02: _op(
        "popup_wait_or_menu_step",
        "acting for an event script (byte 0x35, 800aa384): wait while a number popup shows "
        "(800bf6f8), then clear 80059464, 800591ac and 0x35 and report to the event thread "
        "(80080c6c); otherwise count a battle menu step (800b9258) after the run",
    ),
    0x03: _op("popup_wait", "as 02 without the menu step"),
    0x04: _op(
        "load_extra",
        "without an extra file: read file arg + 800c3530[file] of directory 0x28/2 as it",
        ["file"],
    ),
    0x05: _op(
        "setup_extra",
        "once the extra file is read: relocate it, upload its images and sound bank and take its "
        "animations (ids from 0x40), waiting a run; byte 0x23 set: wait for the disc, clear it",
    ),
    0x06: _op("free_extra", "free the extra file and its sound bank (800b0060)"),
    0x07: _op("stop_disc_read", "request a disc read stop (8002a498(0))"),
    0x08: _op(
        "stop_effects",
        "release the parts' non-persistent effects (800a2acc), stop the object animation",
    ),
    0x09: _op("release_kind", "release every part's effects of kind arg (800a2bb8)"),
    0x0A: _op("release_part", "release part arg's effects 0-2 (800a216c mask 7)"),
    0x0B: _op(
        "reset_parts",
        "release the parts' non-persistent effects, zero the rotation and translation of every "
        "part below the root",
    ),
    0x0C: _op("stop_motion", "clear spin, spin acceleration, drift and drift acceleration"),
    0x0D: _op("release_part_rotation", "release part arg's rotation effect (attachment 0)"),
    0x0E: _op("release_part_translation", "release part arg's translation effect (attachment 1)"),
    0x0F: _op(
        "camera_end",
        "camera mode 1 (800bcad0), release the camera channels (800b00f4), effects stop "
        "(800c3df8 = 0)",
    ),
    0x10: _op("pose", "apply animation arg (800af518) as a frame at once (800a1b50)"),
    0x11: _op(
        "animate",
        "unless the id resolves flagged (800af518): start animation arg on the parts (800a2434; "
        "mode high byte, tag low byte), take its span into 0x8e and start its events (800ae1bc, "
        "looping when mode is set)",
        ["mode_tag"],
    ),
    0x12: _op(
        "animate_from_start",
        "as 11 through 800a2704 (the parts first take the frame's start values), no events",
        ["mode_tag"],
    ),
    0x13: _op(
        "tween_to_frame",
        "tween the parts to animation (low byte) as a frame (800a1cf4: mode arg, tag high byte; "
        "duration, smoothing) and stop the object animation",
        ["tag_index", "duration_smooth"],
    ),
    0x14: _op(
        "call",
        "start script (high byte) of the object of target code (low byte; 0xFF: the target's "
        "own) on the object of target code arg (800aa934, queued while its queue is on); arg "
        "0xFD (this object) switches without queueing and returns",
        ["script_source"],
        HANDOVER,
    ),
    0x15: _op(
        "clone",
        "copy the object into a free slot 13-1e with its own hierarchy; part `part` and its "
        "descendants are drawn on the copy only (800af180); start script arg on it (0xFF none)",
        ["part"],
    ),
    0x16: _op(
        "merge_back",
        "draw the parts on the original object again (800af270), free this copy (800a9ff0) and "
        "start script arg on the original (0xFF none); returns",
        flow=RETURN,
    ),
    0x17: _op("free_self", "free this object (800a9ff0); returns", flow=RETURN),
    0x18: _op(
        "object_animation",
        "start the events of animation arg (800ae1bc), looping when `loop` is set",
        ["loop"],
    ),
    0x19: _op("stop_animation", "stop the object animation (800aeeec)"),
    0x1A: _op(
        "move_image",
        "MoveImage the rectangle (width rounded up to even) to (dst_x, dst_y); arg bit 0: "
        "relative to the object's image placement (none: skipped)",
        ["x", "y", "dst_x", "dst_y", "w", "h"],
    ),
    0x1B: _op(
        "image_animation",
        "start image animation arg (800a3640) copying its frames into image animation `target` "
        "(0xFF none), with mode (bit 7: at the image placement), frame curve (800aa820), flags, "
        "rectangles and parameters; the 15 words are read even when arg is out of range",
        [
            "mode_target",
            "curve_flags",
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
        "tween part arg's rotation, translation or scale (flags & 7) from start to end over "
        "`duration` (800af678: flags 0x20/0x40 relative start/end, 0x80 its children too)",
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
    0x1E: _op("scaled_hierarchy", "byte 0x37 = arg: compose with per-part scales (8009f1c4)"),
    0x1F: _op(
        "select_object",
        "act on the object of target code arg (800af438) from here when it exists; it takes the "
        "script position at the end of the run",
    ),
    0x20: _op(
        "wait_tweens",
        "wait while a tween runs (step flag 0x100); a just-started script waits a frame first",
    ),
    0x21: _op("wait_tag", "tag (0x3c) = arg; wait while a tween with the tag runs (step flag 1)"),
    0x22: _op(
        "wait_loops",
        "wait for `count` tween loops (step flag 0x400; arg != 0xFF: tag = arg, flag 4; counter "
        "at 0x42)",
        ["count"],
    ),
    0x23: _op(
        "show_part",
        "draw part `part` or not by arg bit 0 (bit 7: with its descendants, 800afa98)",
        ["part"],
    ),
    0x24: _op("show", "active (0x34) = arg bit 0"),
    0x25: _op(
        "attach",
        "attach the object of the target code (low byte) to part (high byte) at the offset, or "
        "where it is (arg bit 0); arg bit 1 turns it with the part",
        ["part_code", *XYZ],
    ),
    0x26: _op("detach", "detach the object of target code arg"),
    0x27: _op("compose", "recompose the hierarchy (8009f1c4 or 8009ef3c by byte 0x37)"),
    0x28: _op(
        "wait_effect_near",
        "wait until part (low byte)'s rotation or translation effect is at step (high byte) and "
        "distance / 0x8e < arg",
        ["time_part"],
    ),
    0x29: _op("wait_effect_far", "as 28 with distance / 0x8e >= arg", ["time_part"]),
    0x2A: _op("wait_near", "wait while the distance to the position is >= 0x8e"),
    0x2B: _op("wait_far", "wait while the distance to the position is <= 0x8e"),
    0x2C: _op("wait_point_near", "wait while the distance to the point is >= 0x8e", XYZ),
    0x2D: _op("wait_point_far", "wait while the distance to the point is <= 0x8e", XYZ),
    0x2E: _op(
        "on_near",
        "jump in a later run once within 0x8e of the position (kept at 0x48); arg 0 cancels",
        ["offset"],
        DEFER,
    ),
    0x2F: _op("wait_popup", "wait while a number popup shows (800bf6f8)"),
    0x30: _op(
        "loop_start",
        "loop counter (the operand word itself) = 0; arg is the loop count",
        ["counter"],
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
        "on_timer",
        "jump in a later run after `frames` frames (timer at 0x44); arg 0 cancels",
        ["frames", "offset"],
        DEFER,
    ),
    0x37: _op(
        "on_ground",
        "jump in a later run once the root reaches the ground; arg 0 cancels",
        ["offset"],
        DEFER,
    ),
    0x38: _op(
        "track_position",
        "turn the root toward the position every frame by at most arg + (distance + t) * gain / "
        "(first distance + 1), t growing by rate a frame (800ae098 kind 7, pitch and yaw), until "
        "released",
        ["rate_gain"],
    ),
    0x39: _op("track_position_yaw", "as 38 for the heading only (kind 8)", ["rate_gain"]),
    0x3A: _op(
        "wait_popup_when_idle",
        "wait while a number popup shows when the presentation events from index 2 are at "
        "their end (800aa7dc), byte 0x22 is 0 or 0x35 is set",
    ),
    0x3B: _op(
        "jump_if_slot_flag",
        "jump when the object's slot bit of 800c48e8 is set",
        ["offset"],
        BRANCH,
    ),
    0x3C: _op(
        "fade_sound",
        "fade sound (low byte) of bank source arg (800ae220) to silence over (high byte) frames "
        "(8003a3b8)",
        ["frames_sound"],
    ),
    0x3D: _op(
        "replay_queue_if", "when arg is among the queued scripts: start the queue (5b 2), return"
    ),
    0x3E: _op(
        "jump_if_all_hit",
        "jump when every selected target's event code class (0: codes 0-1, 5: 2, 3, 5, 4: others) "
        "is arg; arg 8: when each target object has flag 2 (0x4a)",
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
    0x48: _op("keep_height", "byte 0x36 = arg: nonzero keeps the root off the ground (800aff9c)"),
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
    0x4F: _op(
        "drift_to_position",
        "drift (z) that covers the distance to the position in arg frames, negated when the "
        "position is within a quarter turn of the heading",
    ),
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
    0x5B: _op(
        "queue_mode",
        "queue state (0x2b) = arg (1 not while scripts are queued); 2 starts the queued scripts "
        "(800aa934) and returns when there were any",
    ),
    0x5C: _op("jump_if_at_position", "jump when the root is at the position", ["offset"], BRANCH),
    0x5D: _op(
        "set_draw_mode",
        "part `part`'s draw mode (0x52) = arg (1 upright billboard, 2 facing the view, 4-7 model "
        "modes 2-5; 800a48ec)",
        ["part"],
    ),
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
        "the object animation runs the arg animation events after the operand (800ae2a4, from "
        "the next frame); jump past them",
        ["offset"],
        EVENTS,
    ),
    0x64: _op("set_word3e", "halfword 0x3e = value", ["value"]),
    0x65: _op(
        "camera_target_to",
        "run camera channel 7 (the look-at point, 800b0164) from the look-at point to the "
        "position (code f6) or camera preset (low byte of word 2)'s look-at (f5) or eye (f4) "
        "raised by height, else after the object of the code (height as a fraction of its "
        "size); mode arg, kind (low byte), duration (high byte of word 2)",
        ["code_kind", "duration_preset", "height"],
    ),
    0x66: _op(
        "camera_eye_to",
        "as 65 for channel 8 (the camera position) from the camera position",
        ["code_kind", "duration_preset", "height"],
    ),
    0x67: _op(
        "camera_turn",
        "run camera channel (high byte: 0 orbit yaw, 1 look-at yaw, 2 orbit pitch, 3 orbit "
        "distance, 4 look-at distance, 5 look-at height, 6 orbit height) from start to end "
        "(800b0164); yaws add the root's heading unless arg has 0x20; flag 0x20 starts at its "
        "value + start, 0x40 ends at end past the start (with 0x20 the old value); yaws and "
        "pitch take the shorter way",
        ["channel_kind", "duration_flags", "start", "end"],
    ),
    0x68: _op(
        "camera_start",
        "release the camera channels, camera mode 4 (800bcaa4), effects run (800c3df8), no "
        "channel reported, the orbit faces the view with zero pitch, distances and heights",
    ),
    0x69: _op(
        "camera_wait",
        "report camera channel kind arg (800c3b84), then wait while it runs (800c3b88 bit 0)",
    ),
    0x6A: _op("camera_snap", "camera channels 7 and 8 start at their targets (800c3b8c = 1)"),
    0x6B: _op(
        "rotation_order", "part `part` composes its rotation in YXZ order when arg is set", ["part"]
    ),
    0x6C: _op("wait_disc", "wait while the disc is busy (800286cc)"),
    0x6D: _op("set_busy", "byte 0x38 = arg bit 0 (battle: 800b136c waits until none is set)"),
    0x6E: _op(
        "wait_busy",
        "wait while the object of target code arg has byte 0x38 == value bit 0",
        ["value"],
    ),
    0x6F: _op("mark_targets", "halfword 0x3a = the selected targets (800c3e30), or -1 for arg 0"),
    0x70: _op(
        "jump_if_same_targets",
        "jump and end the run when 0x3a equals the selected targets",
        ["offset"],
        BRANCH,
    ),
    0x71: _op("set_action", "arg 0: the single action to request (800d39e4) = action", ["action"]),
    0x72: _op(
        "wait_action",
        "end the run; stay here until the requested single action is done (800591b1)",
    ),
    0x73: _op(
        "request_action",
        "request single action 800d39e4 (800b8054; the frame loop starts it, 800b8068), end the "
        "run",
    ),
    0x74: _op(
        "count_hit",
        "count an effect hit (800bf998; the second frees object 11); returns",
        flow=RETURN,
    ),
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


@dataclass(frozen=True)
class EventType:
    name: str
    length: int
    short: int | None = None  # the length when byte 4 (on) is 0
    effect: str = ""
    handler: str = ""

    def size(self, on: int) -> int:
        return self.length if self.short is None or on else self.short


def _event(name, length, effect, short=None):
    return EventType(name, length, short, effect)


# Animation events, read from 800ae2a4's cases (battle/effect.h SpriteCommand to
# ImageEvent): an s16 frame time, the type byte, then the type's fields. The
# runner takes them in order while their time is the animation's frame; each
# case advances by the length below. A type without a case does not advance.
BATTLE_EVENTS = {
    1: _event(
        "sprite",
        0x14,
        "create sprite kind (byte 3) at part (byte 5) + offset by mode, angle, scale and "
        "resource (800afb4c), unless byte 4 excludes the first selected slot's event code "
        "(800b12d0)",
    ),
    2: _event(
        "light",
        0x12,
        "byte 4 set: light byte 3 (below 2) follows part byte 6 (byte 5: free) with colour and "
        "offset; clear: switch it off",
        6,
    ),
    3: _event(
        "fade",
        0x1C,
        "stop colour fade channel byte 3 (800a3484); byte 4 set: restart it (800a32d8, mode 0) "
        "with the event's bytes and values",
        6,
    ),
    4: _event("fade_1", 0x1C, "as 3 with mode 1", 6),
    5: _event(
        "sound",
        8,
        "play sound byte 3 and second sound byte 6 of bank source byte 5 (800ae220) at the "
        "object's volume, unless byte 4 excludes the first selected slot's event code",
    ),
    6: _event("menu_update", 4, "update the battle menu when open (800bf6cc)"),
    7: _event("show_part", 6, "part byte 4 visible = byte 5 bit 0"),
    8: _event(
        "slot_scripts",
        0x0A,
        "on each of the object's slots start the script that its event code picks (byte 5: "
        "codes 0-1, byte 6 for those when the slot object has flag 2, 7: code 5, 8: code 4, 9: "
        "codes 2-3) unless byte 3 excludes the code; byte 4 set: this object's script "
        "(800aa564), else the slot object's own (800aa454)",
    ),
    9: _event(
        "image",
        0x1C,
        "byte 4 set: start image animation byte 3 (800a3640) copying its frames into image "
        "animation byte 5 (0xFF none), with mode, frame curve byte 7 (800aa820) and rectangles; "
        "clear: stop it (800a429c); mode bit 7 without an image placement: not stepped over",
        6,
    ),
}
# 801e5d44 (ovl2143) steps over the same records.
MODEL_VIEWER_EVENTS = {
    **BATTLE_EVENTS,
    1: _event("skip", 0x14, "no effect"),
    2: _event(
        "anchor",
        0x12,
        "byte 4 set: anchor byte 3 (below 2) and its light column follow node byte 6 (byte 5: "
        "free) with colour and offset; clear: deactivate it",
        6,
    ),
    3: _event("stop_channel", 0x1C, "stop channel byte 3 (801e0844)", 6),
    4: _event("stop_channel", 0x1C, "stop channel byte 3 (801e0844)", 6),
    5: _event("skip", 8, "no effect"),
    6: _event("skip", 4, "no effect"),
    7: _event("show_node", 6, "node byte 4 visible = byte 5 bit 0"),
    8: _event(
        "call",
        0x0A,
        "call entry byte 5 of the masked actors (801e8394 or 801e8330 by a local the original "
        "never sets)",
    ),
    9: _event(
        "image",
        0x1C,
        "byte 4 set: start image animation byte 3 (801e0a00); clear: stop it (801e165c); mode "
        "bit 7 with h90 negative: not stepped over",
        6,
    ),
}
BATTLE_EVENTS = {
    code: replace(event, handler=f"800ae2a4 case {code}") for code, event in BATTLE_EVENTS.items()
}
MODEL_VIEWER_EVENTS = {
    code: replace(event, handler=f"801e5d44 case {code}")
    for code, event in MODEL_VIEWER_EVENTS.items()
}
EVENT_DIALECTS = {"battle": BATTLE_EVENTS, "ovl2143": MODEL_VIEWER_EVENTS}
EVENT_HANDLERS = {"battle": "800ae2a4", "ovl2143": "801e5d44"}


class EffectError(ValueError):
    """A script does not decode under the recovered handlers."""


@dataclass(frozen=True)
class Event:
    offset: int
    time: int
    type: int
    name: str
    length: int

    def text(self) -> str:
        return f"{self.offset:06x}: event t={self.time} {self.type} {self.name}"


@dataclass(frozen=True)
class Instruction:
    offset: int
    opcode: int
    argument: int
    operands: tuple[int, ...]
    name: str
    length: int
    successors: tuple[int, ...]
    data: tuple[int, int] | None = None  # the bytes of 63's events
    events: tuple[Event, ...] = ()

    def text(self) -> str:
        words = " ".join(f"{w:04x}" for w in self.operands)
        return (
            f"{self.offset:06x}: {self.opcode:02x} {self.argument:02x} {self.name} {words}".rstrip()
        )


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def decode_events(
    code: bytes, offset: int, count: int, end: int, dialect: str = "battle"
) -> tuple[Event, ...]:
    """Decode count animation events from offset; they must stay below end."""
    table = EVENT_DIALECTS[dialect]
    events = []
    for _ in range(count):
        if offset + 3 > end:
            raise EffectError(f"animation event at 0x{offset:x} runs past its block")
        kind = code[offset + 2]
        if kind not in table:
            raise EffectError(f"unknown animation event type {kind} at 0x{offset:x}")
        spec = table[kind]
        length = spec.size(code[offset + 4] if offset + 4 < end else 0)
        if offset + length > end:
            raise EffectError(f"animation event at 0x{offset:x} runs past its block")
        time = signed16(code[offset] | code[offset + 1] << 8)
        events.append(Event(offset, time, kind, spec.name, length))
        offset += length
    return tuple(events)


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
    data, events = None, ()
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
    else:  # EVENTS: arg events from the end of the command, then on at the target
        if not end <= target <= len(code):
            raise EffectError(f"events of 0x{offset:x} end before the command or past the code")
        events = decode_events(code, end, arg, target, dialect)
        successors, data = (target,), (end, target)
    return Instruction(offset, op, arg, operands, spec.name, spec.length, successors, data, events)


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


class ScriptTable(NamedTuple):
    """A script table's script offsets (0: an empty id), its animation table (None:
    the object gets none) and its script file's data block."""

    scripts: list[int]
    animations: int | None = None
    block: int | None = None


def relocatable(data: bytes, base: int, what: str) -> list[int]:
    """The offsets of a relocatable block at base (8003342c: a count, then offsets
    from base), as file offsets (0 stays 0)."""
    count = struct.unpack_from("<I", data, base)[0] if base + 4 <= len(data) else 0
    if not 1 <= count <= 0x100 or base + 4 + 4 * count > len(data):
        raise EffectError(f"no {what} at 0x{base:x}")
    offsets = [base + o if o else 0 for o in struct.unpack_from(f"<{count}I", data, base + 4)]
    if any(not base < o < len(data) for o in offsets if o):
        raise EffectError(f"{what} at 0x{base:x} points outside its file")
    return offsets


def script_file(data: bytes, base: int = 0) -> ScriptTable:
    """An object script file at base (count, ObjectScripts, data block): its first
    entry is the animation table, the others are the scripts (800a8bf0)."""
    count = struct.unpack_from("<I", data, base)[0]
    if count < 2 or base + 12 > len(data):
        raise EffectError(f"no object script file at 0x{base:x}")
    table = base + struct.unpack_from("<I", data, base + 4)[0]
    block = base + struct.unpack_from("<I", data, base + 8)[0]
    animations, *scripts = relocatable(data, table, "script table")
    return ScriptTable(scripts, animations, block)


def animation_entries(data: bytes, table: ScriptTable) -> list[tuple[int, bool]]:
    """The distinct entries of the table's animation table, each with whether it is
    an animation: unused ids repeat an entry that points at the next structure, a
    script or the data block."""
    others = {s for s in table.scripts if s} | {table.block}
    entries = dict.fromkeys(e for e in relocatable(data, table.animations, "animation table") if e)
    return [(entry, entry not in others) for entry in entries]


def animation_events(data: bytes, entry: int, dialect: str = "battle") -> tuple[Event, ...]:
    """An animation's events: 800ae1bc (ovl2143 801e5c74) runs the u16 at 0x12 of
    them from its u32 offset at 0x14."""
    if entry + 0x18 > len(data):
        raise EffectError(f"animation at 0x{entry:x} runs past its file")
    count, offset = struct.unpack_from("<HI", data, entry + 0x12)
    return decode_events(data, entry + offset, count, len(data), dialect)


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
        # (offset at scene + 0x514) holds the stage's motions, then the stage
        # model's scripts. The stage model gets no animation table.
        base = 4 + struct.unpack_from("<i", data, 4 + 0x514)[0]
        yield "", ScriptTable(relocatable(data, base, "motion block")[1:])
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


# The word the original tools put after each script; no handler reads it.
SEPARATOR = b"\x77\x77"


def mark(covered: bytearray, insn: Instruction) -> None:
    stop = insn.data[1] if insn.data else insn.offset + insn.length
    covered[insn.offset : stop] = b"\1" * (stop - insn.offset)


@dataclass
class Unreached:
    """The words of a script area that no started script reaches."""

    separators: int = 0  # 7777 words after scripts
    scripts: int = 0  # runs that decode as scripts nothing starts
    instructions: int = 0
    errors: list[str] = field(default_factory=list)


def unreached(data: bytes, table: ScriptTable, listing: Listing, dialect: str) -> Unreached:
    """Scan the table's script area (its first script up to the data block or the
    file end) for words no decoded command covers: count the 7777 separators and
    decode every other run as scripts of its own, kept out of the use counts."""
    result = Unreached()
    live = [s for s in table.scripts if s]
    if not live:
        return result
    first = min(live)
    end = table.block if table.block is not None and table.block > first else len(data)
    area = data[:end]
    covered = bytearray(len(area))
    for insn in listing.instructions.values():
        mark(covered, insn)
    position = first
    while position + 2 <= end:
        word = area[position : position + 2]
        if covered[position] or word == b"\0\0":
            position += 2
        elif word == SEPARATOR:
            result.separators += 1
            position += 2
        else:
            run = disassemble(area, [position], dialect)
            if run.errors:
                result.errors.append(f"0x{position:x}: {run.errors[0]}")
                while position < end and not covered[position]:
                    position += 2
                continue
            fresh = [i for i in run.instructions.values() if not covered[i.offset]]
            result.scripts += 1
            result.instructions += len(fresh)
            for insn in fresh:
                mark(covered, insn)
    return result


@dataclass
class Sweep:
    tables: Counter = field(default_factory=Counter)
    scripts: Counter = field(default_factory=Counter)
    empty: Counter = field(default_factory=Counter)
    instructions: Counter = field(default_factory=Counter)
    opcodes: Counter = field(default_factory=Counter)
    event_blocks: int = 0  # 63 commands
    event_tails: list[int] = field(default_factory=list)  # bytes after the counted events
    animations: int = 0  # distinct animation entries with their event lists
    not_animations: int = 0  # entries of unused ids (a script or the data block)
    events: Counter = field(default_factory=Counter)  # (63 or "animation", type)
    unstarted: Unreached = field(default_factory=Unreached)
    errors: list[str] = field(default_factory=list)


def sweep_table(
    result: Sweep, family: str, dialect: str, data: bytes, table: ScriptTable, where: str = ""
):
    """Count one table's scripts, their 63 events, its animations' events and the
    words nothing starts into result; return the errors (prefixed by where)."""
    live = [s for s in table.scripts if s]
    result.tables[family] += 1
    result.scripts[family] += len(live)
    result.empty[family] += len(table.scripts) - len(live)
    listing = disassemble(data, live, dialect)
    result.instructions[family] += len(listing.instructions)
    result.opcodes.update(i.opcode for i in listing.instructions.values())
    for insn in listing.instructions.values():
        if insn.data:
            result.event_blocks += 1
            result.events.update((0x63, e.type) for e in insn.events)
            last = insn.events[-1] if insn.events else None
            tail = insn.data[1] - (last.offset + last.length if last else insn.data[0])
            if tail:
                result.event_tails.append(tail)
    extra = unreached(data, table, listing, dialect)
    result.unstarted.separators += extra.separators
    result.unstarted.scripts += extra.scripts
    result.unstarted.instructions += extra.instructions
    result.unstarted.errors.extend(where + e for e in extra.errors)
    errors = list(listing.errors)
    if table.animations is not None:
        try:
            entries = animation_entries(data, table)
        except EffectError as error:
            entries = []
            errors.append(str(error))
        for entry, animation in entries:
            if not animation:
                result.not_animations += 1
                continue
            try:
                events = animation_events(data, entry, dialect)
            except EffectError as error:
                errors.append(f"animation at 0x{entry:x}: {error}")
                continue
            result.animations += 1
            result.events.update(("animation", e.type) for e in events)
    return [where + e for e in errors]


def sweep(root: Path = ROOT) -> tuple[dict[str, Sweep], bool]:
    """Decode every table of both discs; also whether both discs hold the same files."""
    results = {"battle": Sweep(), "ovl2143": Sweep()}
    digests = []
    for name in ("disc1", "disc2"):
        digest = hashlib.sha256()
        for dialect, family, label, data, table in disc_tables(Disc(root, name)):
            digest.update(f"{family} {label}".encode() + data)
            where = f"{name} {family} {label}: "
            results[dialect].errors.extend(
                sweep_table(results[dialect], family, dialect, data, table, where)
            )
        digests.append(digest.digest())
    return results, digests[0] == digests[1]


def print_listing(data: bytes, kind: str, dialect: str) -> None:
    for label, table in file_tables(kind, data):
        print(f"table{label}:")
        if table.animations is not None:
            for entry, animation in animation_entries(data, table):
                if not animation:
                    print(f"  animation 0x{entry:x}: unused ids (not an animation)")
                    continue
                events = animation_events(data, entry, dialect)
                print(f"  animation 0x{entry:x}: {len(events)} events")
                for event in events:
                    print(f"    {event.text()}")
        names = {}
        for index, offset in enumerate(table.scripts):
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
            for event in insn.events:
                print(f"    {event.text()}")
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
        inline = Counter({t: n for (where, t), n in result.events.items() if where == 0x63})
        listed = Counter({t: n for (where, t), n in result.events.items() if where != 0x63})
        types = set(inline) | set(listed)
        print(
            f"  animation events ({EVENT_HANDLERS[dialect]}): types defined "
            f"{len(EVENT_DIALECTS[dialect])}, used {len(types)}"
        )
        print(
            f"    in {result.event_blocks} 63 commands: {sum(inline.values())} ("
            + " ".join(f"{t}:{inline[t]}" for t in sorted(inline))
            + f"); {len(result.event_tails)} hold {sum(result.event_tails)} bytes after "
            "their counted events"
        )
        print(
            f"    in {result.animations} animations: {sum(listed.values())} ("
            + " ".join(f"{t}:{listed[t]}" for t in sorted(listed))
            + f"); {result.not_animations} entries of unused ids point at a script or "
            "the data block"
        )
        print(f"  unknown/undecodable: {len(result.errors)}")
        for error in result.errors:
            print(f"    {error}")
        unstarted = result.unstarted
        print(
            f"  nothing starts: {unstarted.separators} separator words (7777) after scripts; "
            f"{unstarted.scripts} runs decode as scripts of {unstarted.instructions} commands"
        )
        print(f"  unreached words that do not decode: {len(unstarted.errors)}")
        for error in unstarted.errors:
            print(f"    {error}")


if __name__ == "__main__":
    main()
