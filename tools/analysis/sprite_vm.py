"""The sprite animation command language: execution subset and disassembler.

Resident 80023210 dispatches 800248d4 when the signed halfword countdown reaches
zero. The first part of this module executes its timed frame families, index
store and relative jump; all other commands and alternate platform execution
fail explicitly there. The command table supplies original widths. That part is
original-format analysis, not a native tick scheduler or a complete sprite VM.

The second part decodes every command of both interpreters: the resident one
(800248d4 with the generic commands of 8001fbe4) and the battle overlay's copy
(800c11cc with the battle commands of 800b3f04), as recovered in decomp/src.
`python3 -m tools.analysis.sprite_vm --sweep` finds the sprite resource blocks
of both extracted discs and prints aggregate decoding results only, with the
animation headers that nothing recovered starts reported apart.
"""

from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import struct
import sys
from collections import Counter
from collections.abc import Iterable, Iterator
from dataclasses import dataclass, field, replace
from pathlib import Path

from .arithmetic import signed16, signed32
from .jump_physics import truncate_shift
from .packed import PackedError, decode_block
from .sprite_replay import frame_change, lookup_frame
from .sprite_state import put, read_exact, u16, u32


@dataclass(frozen=True)
class SpriteEnvironment:
    rate_control: int
    platform_mode: int
    frame_head: int


@dataclass(frozen=True)
class SpriteExecution:
    sprite: bytes
    environment: SpriteEnvironment
    commands: int


class UnsupportedSpriteCommand(ValueError):
    def __init__(self, pointer, opcode):
        self.pointer, self.opcode = pointer, opcode
        super().__init__(
            f"Unreconstructed ordinary sprite command 0x{opcode:02x} at 0x{pointer:08x}"
        )


def execute_sprite_commands(
    sprite,
    address,
    environment,
    widths,
    read,
    read_memory,
    *,
    incoming_duration=None,
    on_event=None,
    inspection_limit=4096,
):
    """800248d4: supported control/frame effects, from entry through return.

    Commands 40..7f consume the incoming S3 value in the original. Absence of
    that context is an explicit error. The inspection limit bounds hostile loops;
    it is not an original gameplay safeguard or a successful yield.
    """
    if len(widths) != 256 or inspection_limit <= 0:
        raise ValueError("Incomplete original command widths or inspection bound")
    if environment.platform_mode:
        raise ValueError("Alternate original sprite VM 800c11cc remains unreconstructed")
    out = bytearray(sprite)
    head, commands = environment.frame_head, 0
    duration = None if incoming_duration is None else signed32(incoming_duration)

    def event(name, data, **values):
        if on_event:
            on_event(name, bytes(data), replace(environment, frame_head=head), values)

    def frame_before(data, frame):
        event("frame-before", data, frame=frame)

    def list_step(node, data):
        event("frame-list-step", data, node=node)

    event("vm-before", out)
    while u16(out, 0x9E) == 0:
        if commands == inspection_limit:
            raise ValueError("Original sprite command execution exceeded inspection bound")
        pointer = u32(out, 0x64)
        opcode = read_exact(read, pointer, 1)[0]
        commands += 1
        event("vm-step", out, pointer=pointer, opcode=opcode)
        if opcode < 0x80:
            put(out, 0x64, pointer + 1)
            if opcode < 0x30:
                if opcode < 0x10 or opcode >= 0x20:
                    frame = u16(out, 0x34) + (1 if opcode < 0x10 else -1)
                    frame_before(out, frame)
                    updated, head = frame_change(
                        out, address, frame, head, read, read_memory, list_step
                    )
                else:
                    flags = u32(out, 0xA8)
                    put(out, 0xA8, (flags & 0xFFFE07FF) | ((((flags >> 11) + 1) & 63) << 11))
                    updated, head = lookup_frame(
                        out, address, head, read, read_memory, frame_before, list_step
                    )
                out = bytearray(updated)
                event("frame-after", out)
            if opcode < 0x40:
                duration = (opcode & 15) + 1
            if duration is None:
                raise ValueError("Original ordinary frame command needs the incoming S3 duration")
            delay = truncate_shift(signed32(duration * ((u32(out, 0xAC) >> 7) & 0xFFF)), 8)
            if delay == 0:
                delay = 1
            put(out, 0x9E, u16(out, 0x9E) + delay, 2)
            flags = u32(out, 0xA8)
            step = (((flags >> 22) + 1) & 63) or 63
            put(out, 0xA8, (flags & 0xF03FFFFF) | (step << 22))
            break
        if opcode == 0xB3:
            value = read_exact(read, pointer + 1, 1)[0]
            put(out, 0xA8, (u32(out, 0xA8) & 0xFFFE07FF) | ((value & 63) << 11))
            put(out, 0x64, u32(out, 0x64) + widths[opcode])
        elif opcode == 0xE1:
            delta = signed16(int.from_bytes(read_exact(read, pointer + 1, 2), "little"))
            put(out, 0x64, u32(out, 0x64) + delta)
        else:
            raise UnsupportedSpriteCommand(pointer, opcode)
    event("vm-after", out)
    return SpriteExecution(bytes(out), replace(environment, frame_head=head), commands)


def advance_sprite_timer(
    sprite, address, environment, widths, read, read_memory, *, inspection_limit=4096, **kwargs
):
    """80023210: countdown and conditional command dispatch, preserving wrap.

    A zero timer remains zero. Rate -1 skips all work. The source reloads the
    global rate after a dispatch; the result's environment supplies that value.
    Other rates are bounded for inspection rather than silently shortened.
    """
    if inspection_limit <= 0:
        raise ValueError("Invalid original timer inspection bound")
    out, count, iterations = bytearray(sprite), 0, 0
    if signed32(environment.rate_control) == -1:
        return SpriteExecution(bytes(out), environment, 0)
    while True:
        if iterations == inspection_limit:
            raise ValueError("Original sprite timer exceeded inspection bound")
        timer = u16(out, 0x9E)
        if timer:
            put(out, 0x9E, timer - 1, 2)
            if timer == 1:
                result = execute_sprite_commands(
                    out,
                    address,
                    environment,
                    widths,
                    read,
                    read_memory,
                    inspection_limit=inspection_limit,
                    **kwargs,
                )
                out, environment = bytearray(result.sprite), result.environment
                count += result.commands
        iterations += 1
        if iterations == (environment.rate_control + 1) & 0xFFFFFFFF:
            return SpriteExecution(bytes(out), environment, count)


# ---------------------------------------------------------------------------
# The command language, read from the recovered handlers in decomp/src
# ---------------------------------------------------------------------------

FIELD = "field"  # resident 800248d4 (sprite_in_battle clear: field, world map, other modes)
BATTLE = "battle"  # 800c11cc, which 800248d4 calls while sprite_in_battle is set (battle)
DIALECTS = (FIELD, BATTLE)
INTERPRETERS = {FIELD: "800248d4", BATTLE: "800c11cc"}

# Operand kinds and their sizes in bytes; multi-byte values are little endian.
OPERAND_SIZES = {
    "u8": 1,
    "s8": 1,
    "var": 1,  # 8001fba4: bit 7 set, table byte (b & 7f) at +88; else stack byte +8c + (s8)b
    "command": 1,  # a generic command number (8001fbe4), run by c8
    "battle": 1,  # a battle command number (800b3f04)
    "place": 1,  # a bc selector: bit 7 place by selector (bits 0-5), bit 6 into the target
    "arg": 1,  # an argument byte of a battle command, read as that command reads it
    "byte": 1,  # a byte no handler reads
    "s16": 2,
    "code": 2,  # command offset from the command's first byte
    "header": 2,  # animation header offset from the operand bytes (e0)
    "partner": 2,  # animation header offset from the command's first byte (e3)
    "data": 2,  # data offset from the operand bytes
    "table": 2,  # byte table offset from the command's first byte (cc)
    "frame": 2,  # be: frame bits 0-8, flip x 9, flip y 10, time - 1 11-14, remap 15
    "angle": 2,  # cd-cf: angle / 8 bits 0-8, part group 9-11, bit 12 set (else add)
    "data24": 3,  # data offset from the operand bytes
}
SIGNED = frozenset({"s8", "s16", "code", "header", "partner", "data", "table", "frame", "data24"})

# next and wait fall through (wait once the countdown runs, or after retrying
# the command each frame while its condition holds); end and return do not;
# jump reaches only its target; call and branch reach both; resume falls
# through and later resumes the sprite at its target (fb).
FALL_THROUGH = frozenset({"next", "wait", "call", "branch", "resume"})


@dataclass(frozen=True)
class Spec:
    name: str
    operands: tuple[str, ...]
    flow: str
    handler: str

    @property
    def length(self) -> int:
        return 1 + sum(OPERAND_SIZES[kind] for kind in self.operands)


def table_width(opcode: int) -> int:
    """sprite_vm_command_lengths[opcode - 0x80], the length of command 80-ff (resident data).

    The interpreters add it to the script pointer after every handler that keeps the pointer
    (the battle copy indexes the same bytes as sprite_vm_command_lengths_by_opcode). A test
    checks it against the C table in sprite_construction.c; the sweep checks it against each
    disc's resident executable.
    """
    if not 0x80 <= opcode <= 0xFF:
        raise ValueError(f"no width entry is read for command {opcode:#x}")
    if opcode < 0xA0:
        return 1
    if opcode < 0xC8:
        return 2
    return 3 if opcode < 0xF1 else 4


def _table(handler: str, rows: dict) -> dict[int, Spec]:
    return {
        opcode: Spec(name, tuple(kinds), flow, f"{handler} case {opcode:02x}")
        for opcode, (name, kinds, flow) in rows.items()
    }


# 8001fbe4: the commands both interpreters pass on (their default case) and c8.
GENERIC = _table(
    "8001fbe4",
    {
        0x8A: ("halt", (), "next"),
        0x8C: ("face_target", (), "next"),
        0x8D: ("texture_page", (), "next"),
        0x90: ("swap_animations", (), "next"),
        0x91: ("colour_on", (), "next"),
        0x92: ("colour_off", (), "next"),
        0x93: ("copy_creator_parts", (), "next"),
        0x94: ("copy_creator_direction", (), "next"),
        0x96: ("destroy_children", (), "next"),
        0xA0: ("set_speed", ("s8",), "next"),
        0xA1: ("set_rise", ("s8",), "next"),
        0xA2: ("set_groups", ("u8",), "next"),
        0xA3: ("set_gravity", ("s8",), "next"),
        0xA4: ("target_animation", ("s8",), "next"),
        0xA5: ("add_speed", ("s8",), "next"),
        0xA6: ("add_rise", ("s8",), "next"),
        0xA7: ("wait_or_pause", ("u8",), "wait"),
        0xA8: ("turn", ("s8",), "next"),
        0xA9: ("move_x", ("s8",), "next"),
        0xAA: ("move_y", ("s8",), "next"),
        0xAB: ("move_z", ("s8",), "next"),
        0xAC: ("turn_random", ("u8",), "next"),
        0xAD: ("set_bounce", ("u8",), "next"),
        0xAE: ("add_angle_z", ("s8",), "next"),
        0xAF: ("set_angle_z", ("s8",), "next"),
        0xB0: ("sound", ("u8",), "next"),
        0xB3: ("set_frame_index", ("s8",), "next"),
        0xB4: ("push", ("u8",), "next"),
        0xB5: ("set_scale", ("u8",), "next"),
        0xB6: ("add_angle_x", ("s8",), "next"),
        0xB7: ("add_angle_y", ("s8",), "next"),
        0xB8: ("stack_adjust", ("s8",), "next"),
        0xB9: ("voice", ("u8",), "next"),
        0xBA: ("set_blend", ("u8",), "next"),
        0xBB: ("add_depth", ("s8",), "next"),
        0xBC: ("place", ("place",), "next"),
        0xBD: ("spawn_shared", ("u8",), "next"),
        0xBF: ("set_height", ("u8",), "next"),
        0xC0: ("scatter_ground", ("u8",), "next"),
        0xC1: ("scatter", ("u8",), "next"),
        0xC4: ("scatter_velocity", ("u8",), "next"),
        0xC5: ("advance", ("u8",), "next"),
        0xC6: ("set_sequencer", ("u8",), "next"),
        0xC9: ("set_sequencer16", ("s16",), "next"),
        0xCC: ("set_table", ("table",), "next"),
        0xCD: ("angle_x", ("angle",), "next"),
        0xCE: ("angle_y", ("angle",), "next"),
        0xCF: ("angle_z", ("angle",), "next"),
        0xD0: ("var_add_var", ("var", "var"), "next"),
        0xD1: ("var_mul_var", ("var", "var"), "next"),
        0xD2: ("var_div_var", ("var", "var"), "next"),
        0xD3: ("var_add_var", ("var", "var"), "next"),
        0xD5: ("var_div_var", ("var", "var"), "next"),
        0xD6: ("var_add", ("var", "u8"), "next"),
        0xD7: ("var_mul", ("var", "s8"), "next"),
        0xD8: ("var_div", ("var", "s8"), "next"),
        0xD9: ("var_shl", ("var", "s8"), "next"),
        0xDA: ("var_sar", ("var", "s8"), "next"),
        0xDB: ("var16_shl", ("var", "s8"), "next"),
        0xDC: ("var16_shr", ("var", "s8"), "next"),
        0xDD: ("var_add_var", ("var", "var"), "next"),
        0xDE: ("var_add_var", ("var", "var"), "next"),
        0xDF: ("var_set", ("var", "u8"), "next"),
        0xE0: ("spawn", ("header",), "next"),
        0xE5: ("var_random", ("var", "u8"), "next"),
        0xE6: ("var16_set", ("var", "u8"), "next"),
        0xE7: ("add_scale", ("s16",), "next"),
        0xE9: ("add_scale_x", ("s16",), "next"),
        0xEA: ("add_scale_y", ("s16",), "next"),
        0xEB: ("add_scale_z", ("s16",), "next"),
        0xED: ("set_x", ("s16",), "next"),
        0xEE: ("set_y", ("s16",), "next"),
        0xEF: ("set_z", ("s16",), "next"),
        0xF1: ("set_colour", ("u8", "u8", "u8"), "next"),
        0xF2: ("add_colour", ("s8", "s8", "s8"), "next"),
        0xF5: ("load_model", ("data24",), "next"),
        0xF6: ("load_model_group", ("data24",), "next"),
        0xF7: ("load_model_group_clear", ("data24",), "next"),
        0xFC: ("upload_images", ("data24",), "next"),
    },
)


def _interpreter(vm: str) -> dict[int, Spec]:
    """The cases 800248d4 and its battle copy 800c11cc share."""
    return _table(
        vm,
        {
            0x80: ("end", (), "end"),
            0x81: ("hold", (), "end"),
            0x82: ("restart", (), "end"),
            0x85: ("return", (), "return"),
            0x86: ("wait_apex", (), "wait"),
            0x87: ("wait_landed", (), "wait"),
            0x8E: ("stop", (), "end"),
            0x98: ("wait_creator", (), "wait"),
            0xA7: ("wait_or_pause", ("u8",), "wait"),
            0xBE: ("frame_show", ("frame",), "wait"),
            0xC8: ("command_var", ("command", "var"), "next"),
            0xD4: ("jump_callback", ("code",), "jump"),
            0xE1: ("jump", ("code",), "jump"),
            0xE2: ("call", ("code",), "call"),
            0xE4: ("loop", ("code",), "branch"),
            0xFA: ("branch_nonzero", ("var", "code"), "branch"),
        },
    )


FIELD_SPECS = {**GENERIC, **_interpreter(INTERPRETERS[FIELD])}
BATTLE_SPECS = {
    **GENERIC,
    **_interpreter(INTERPRETERS[BATTLE]),
    **_table(
        INTERPRETERS[BATTLE],
        {
            0x88: ("aim_jump", (), "next"),
            0x89: ("aim_jump_keep_rise", (), "next"),
            0x8B: ("show_results", (), "next"),
            0x8F: ("finish", (), "end"),
            0x95: ("wait_disc", (), "wait"),
            0x97: ("land_face_partner", (), "wait"),
            0x99: ("camera_angles", (), "next"),
            0x9A: ("place_by_camera", (), "next"),
            0x9B: ("scale_by_camera", (), "next"),
            0x9C: ("place_by_scale", (), "next"),
            0x9D: ("mark_camera_point", (), "next"),
            0x9E: ("wait_button", (), "wait"),
            0x9F: ("nop", (), "next"),
            0xA4: ("partner_animation", ("s8",), "next"),
            0xC2: ("approach_partner", ("s8",), "next"),
            0xC3: ("battle_command", ("battle",), "next"),
            0xCA: ("fade", ("data",), "next"),
            0xCB: ("quake", ("data",), "next"),
            0xE3: ("partner_run", ("partner",), "next"),
            0xE8: ("battle_command_var", ("battle", "var"), "next"),
            0xEC: ("battle_command_1", ("battle", "arg"), "next"),
            0xF3: ("bind_parts", ("data24",), "next"),
            0xF8: ("branch_event", ("code", "u8"), "branch"),
            0xF9: ("battle_command_2", ("battle", "arg", "arg"), "next"),
            0xFB: ("approach_resume", ("code", "u8"), "resume"),
        },
    ),
}
_SPECS = {FIELD: FIELD_SPECS, BATTLE: BATTLE_SPECS}

# 8001fbe4 case bc: selectors (bits 0-5 with bit 7 set) of its 39-way switch.
# target: the sprite aimed at (+74); creator: the sprite attached to (+70);
# actor: the battle's acting sprite (battle_acting_sprite); part n: a group offset of
# the renderer's part table (+34 entry n).
PLACE_SELECTORS = {
    0: "target",
    1: "actor",
    2: "group_centre",
    3: "actor_midpoint",
    4: "group_centre_with_self",
    5: "empty_sum",
    6: "point_f99c",
    7: "point_f9ac",
    8: "target_part_2",
    9: "target_part_1",
    10: "target_part_3",
    **{11 + i: f"creator_part_{1 + i}" for i in range(7)},
    18: "target_top",
    19: "target_middle",
    20: "target_depth",
    21: "target_half_depth",
    22: "creator",
    23: "screen_centre",
    24: "self",
    **{25 + i: f"actor_part_{1 + i}" for i in range(7)},
    32: "actor_top",
    33: "actor_middle",
    34: "actor_depth",
    35: "actor_half_depth",
    36: "self_link_on",
    37: "self_link_off",
    38: "formation_place",
}

# 800b3f04: the battle commands of c3, ec, f9 (inline arguments) and e8 (a
# variable's bytes), with the argument bytes each handler reads.
BATTLE_COMMANDS = {
    number: Spec(name, tuple(kinds), "next", f"800b3f04 case {number:02x}")
    for number, (name, kinds) in {
        0x01: ("trail", ("var",)),
        0x02: ("end_trail", ()),
        0x03: ("link_partner", ("u8",)),
        0x04: ("end_links", ()),
        0x05: ("draw_streak", ()),
        0x06: ("camera_actor", ()),
        0x07: ("camera_partner", ()),
        0x08: ("clear_anchors", ()),
        0x09: ("reverse", ()),
        0x0A: ("roll_to_velocity", ()),
        0x0B: ("pitch_to_velocity", ()),
        0x0C: ("face_velocity", ()),
        0x0D: ("turn_velocity", ("s8",)),
        0x0E: ("set_direction", ("s8",)),
        0x0F: ("unmirror_face_zero", ()),
        0x10: ("place_at_look_point", ()),
        0x11: ("pause_sprites", ()),
        0x12: ("black_background", ()),
        0x13: ("resume_sprites", ()),
        0x14: ("restore_background", ()),
        0x15: ("own_placement", ()),
        0x16: ("draw_at_back", ()),
        0x17: ("scale_velocity", ("s8",)),
        0x18: ("shatter_screen", ()),
        0x19: ("spin", ("data",)),
        0x1A: ("destroy_children", ()),
        0x1B: ("take_parent_velocity", ()),
        0x1C: ("aim_velocity", ()),
        0x1D: ("break_image", ("data",)),
        0x1E: ("draw_line_to_parent", ()),
        0x1F: ("ground", ()),
        0x20: ("orbit", ()),
        0x21: ("set_velocity_z", ("s8",)),
        0x22: ("add_velocity_z", ("s8",)),
        0x23: ("face_target_point", ()),
        0x24: ("module_801fc7b0", ()),
        0x25: ("module_801fc6fc", ()),
        0x26: ("slot_animation", ("u8", "u8")),
        0x27: ("shift_cluts", ("u8", "u8")),
        0x28: ("set_velocity_x", ("s8",)),
        0x29: ("add_velocity_x", ("s8",)),
        0x2A: ("set_velocity_y", ("s8",)),
        0x2B: ("add_velocity_y", ("s8",)),
        0x2C: ("draw_unlit_model", ()),
        0x2D: ("set_position", ("data",)),
        0x2E: ("set_target", ("data",)),
        0x2F: ("mark_sprite", ()),
        0x30: ("anchor_offset", ("u8",)),
        0x31: ("centre_geometry", ()),
        0x32: ("battle_geometry", ()),
        0x33: ("own_animation", ("u8",)),
        0x34: ("set_gravity", ("s8",)),
        0x35: ("render_bit_27_on", ()),
        0x36: ("render_bit_27_off", ()),
        0x37: ("request_images", ()),
        0x38: ("upload_images", ("data",)),
        0x3A: ("fade_lights", ("data",)),
        0x3B: ("creator_depth_on", ()),
        0x3C: ("creator_depth_off", ()),
        0x3D: ("copy_part_texture", ()),
        0x3E: ("next_target", ()),
        0x3F: ("render_bit_26_on", ()),
        0x40: ("steer_velocity", ("u8",)),
        0x41: ("camera_partner_actor", ()),
        0x42: ("motion_bit_5_off", ()),
        0x43: ("screen_centred", ()),
        0x44: ("debug_select", ()),
        0x45: ("debug_deselect", ()),
        0x46: ("load_object_set", ("u8",)),
        0x47: ("show_stage_object", ("u8",)),
        0x48: ("flag_0_on", ()),
        0x49: ("add_view_angle_x", ("s8",)),
        0x4A: ("add_view_angle_y", ("s8",)),
        0x4B: ("add_view_angle_z", ("s8",)),
        0x4C: ("lit", ()),
        0x4D: ("stop_voice", ("u8",)),
        0x4E: ("stop_sound", ("u8",)),
        0x4F: ("voice_last", ("u8",)),
        0x50: ("wait_hit", ()),
        0x51: ("free_stage_object", ()),
        0x52: ("voice_value", ("u8", "u8")),
        0x53: ("unmirror", ()),
        0x54: ("module_801fc898", ()),
        0x55: ("camera_actor_group", ()),
        0x56: ("gear_sound", ()),
        0x57: ("set_3b74", ()),
        0x58: ("clear_3b74", ()),
        0x59: ("allow_fades", ()),
        0x5A: ("block_fades", ()),
        0x5B: ("set_view_44", ("data",)),
        0x5C: ("set_view_4c", ("data",)),
        0x5D: ("add_view_44", ("data",)),
        0x5E: ("add_view_4c", ("data",)),
        0x5F: ("set_3564", ()),
        0x60: ("set_3564_slot", ()),
        0x61: ("approach_partner_side", ("s8",)),
        0x62: ("show_result", ()),
        0x63: ("camera_scale", ()),
        0x64: ("camera_scale_idle", ()),
        0x65: ("camera_group", ()),
        0x66: ("set_3688", ()),
        0x67: ("wait_camera", ()),
        0x68: ("bind_by_animation", ()),
        0x69: ("rebind_block", ()),
        0x6A: ("fade_music", ()),
        0x6B: ("copy_vram_columns", ()),
    }.items()
}
# Commands that wait by moving the script pointer back two bytes and adding a
# frame: coherent only in the two-byte c3 form, which they then repeat.
BATTLE_RETRY_COMMANDS = frozenset({0x50, 0x67})


def spec_for(opcode: int, dialect: str = FIELD) -> Spec:
    """The command's handler in a dialect (unhandled commands still advance)."""
    if dialect not in DIALECTS:
        raise ValueError(f"unknown sprite VM dialect {dialect!r}")
    if not 0 <= opcode <= 0xFF:
        raise ValueError(f"not a command byte: {opcode!r}")
    vm = INTERPRETERS[dialect]
    if opcode < 0x80:
        # Shown frame, then wait (op & f) + 1 frames scaled by the divisor.
        family = ("frame_next", "frame_step", "frame_back", "wait")
        if opcode < 0x40:
            return Spec(family[opcode >> 4], (), "wait", f"{vm} op < {(opcode >> 4) + 1}0")
        # 40-7f set no duration: the original reads a stale register (s3).
        return Spec("wait_stale", (), "wait", f"{vm} op < 80")
    spec = _SPECS[dialect].get(opcode)
    if spec is not None:
        return spec
    padding = ("byte",) * (table_width(opcode) - 1)
    return Spec("unhandled", padding, "next", f"{vm} default, 8001fbe4 has no case {opcode:02x}")


def defined_opcodes(dialect: str) -> int:
    """Commands with a handler: the 128 timed ones and the switch cases."""
    return 0x80 + sum(1 for op in range(0x80, 0x100) if spec_for(op, dialect).name != "unhandled")


def _check_lengths() -> None:
    for dialect in DIALECTS:
        for opcode in range(0x80, 0x100):
            spec = spec_for(opcode, dialect)
            # be advances by three itself; every other command by its width.
            expected = 3 if opcode == 0xBE else table_width(opcode)
            if spec.length != expected:
                raise AssertionError(f"{dialect} {opcode:02x}: {spec.length} != {expected}")


_check_lengths()


class ScriptError(ValueError):
    def __init__(self, pc: int, message: str):
        self.pc = pc
        super().__init__(f"+0x{pc:x}: {message}")


@dataclass(frozen=True)
class Instruction:
    pc: int
    opcode: int
    spec: Spec
    operands: tuple[int, ...]
    targets: tuple[int, ...] = ()  # commands reached besides the next one
    headers: tuple[int, ...] = ()  # animation headers whose scripts it starts
    data: tuple[int, ...] = ()  # data it addresses
    command: str = ""  # c8/c3/e8/ec/f9: the command run; bc: the selector
    unhandled: str = ""  # why its dialect has no handler for it
    overread: int = 0  # argument bytes a battle command reads past its own
    reentry: int | None = None  # where a retrying battle command wider than c3 resumes

    @property
    def name(self) -> str:
        return self.spec.name

    @property
    def length(self) -> int:
        return self.spec.length

    @property
    def flow(self) -> str:
        return self.spec.flow

    @property
    def successors(self) -> tuple[int, ...]:
        follow = (self.pc + self.length,) if self.flow in FALL_THROUGH else ()
        return follow + self.targets


def _operand(kind: str, raw: int) -> int:
    bits = 8 * OPERAND_SIZES[kind]
    return raw - (1 << bits) if kind in SIGNED and raw >> (bits - 1) else raw


def _battle_command(ins: Instruction, code: bytes) -> Instruction:
    number = ins.operands[0]
    spec = BATTLE_COMMANDS.get(number)
    if spec is None:
        return replace(ins, command=f"{number:02x}", unhandled=f"800b3f04 has no case {number:02x}")
    reentry = None
    if number in BATTLE_RETRY_COMMANDS and ins.opcode != 0xC3:
        reentry = ins.pc + ins.length - 2  # back two, then on by the width
    if ins.opcode == 0xE8:  # the arguments are the bytes at the variable
        return replace(ins, command=spec.name, reentry=reentry)
    inline = ins.length - 2
    needed = sum(OPERAND_SIZES[kind] for kind in spec.operands)
    data = []
    at = ins.pc + 2
    for kind in spec.operands:
        if kind == "data" and at + 2 <= ins.pc + ins.length:
            data.append(at + _operand("data", int.from_bytes(code[at : at + 2], "little")))
        at += OPERAND_SIZES[kind]
    overread = max(0, needed - inline)
    return replace(ins, command=spec.name, data=tuple(data), overread=overread, reentry=reentry)


def decode(code: bytes, pc: int, dialect: str = FIELD) -> Instruction:
    """Decode the command at pc of code (offsets are relative to code)."""
    if not 0 <= pc < len(code):
        raise ScriptError(pc, "command outside the supplied bytes")
    opcode = code[pc]
    spec = spec_for(opcode, dialect)
    if pc + spec.length > len(code):
        raise ScriptError(pc, f"command {opcode:02x} runs past the supplied bytes")
    values, targets, headers, data = [], [], [], []
    at = pc + 1
    for kind in spec.operands:
        size = OPERAND_SIZES[kind]
        value = _operand(kind, int.from_bytes(code[at : at + size], "little"))
        values.append(value)
        if kind == "code":
            targets.append(pc + value)
        elif kind == "header":
            headers.append(at + value)
        elif kind == "partner":
            headers.append(pc + value)
        elif kind == "table":
            data.append(pc + value)
        elif kind in ("data", "data24") and not (opcode == 0xF3 and value == 0):
            data.append(at + value)  # f3 offset 0 unbinds the parts
        at += size
    ins = Instruction(pc, opcode, spec, tuple(values), tuple(targets), tuple(headers), tuple(data))
    if spec.name == "unhandled":
        return replace(ins, unhandled=spec.handler)
    if opcode == 0xC8:
        sub = GENERIC.get(values[0])
        if sub is None:
            return replace(
                ins, command=f"{values[0]:02x}", unhandled=f"8001fbe4 has no case {values[0]:02x}"
            )
        return replace(ins, command=sub.name)
    if opcode == 0xBC:
        selector = values[0]
        if not selector & 0x80:
            return replace(ins, command=f"creator_offset_{selector}")
        name = PLACE_SELECTORS.get(selector & 0x3F)
        if name is None:
            return replace(
                ins,
                command=f"selector_{selector & 0x3F}",
                unhandled=f"8001fbe4 case bc has no selector {selector & 0x3F}",
            )
        return replace(ins, command=name + ("_target" if selector & 0x40 else ""))
    if dialect == BATTLE and opcode in (0xC3, 0xE8, 0xEC, 0xF9):
        return _battle_command(ins, code)
    return ins


def render(ins: Instruction) -> str:
    """One listing line: offset, command bytes' meaning and references."""
    words = []
    for kind, value in zip(ins.spec.operands, ins.operands, strict=True):
        if kind == "var":
            words.append(f"table[{value & 0x7F}]" if value & 0x80 else f"stack[{signed8(value)}]")
        elif kind in ("command", "battle", "place"):
            words.append(ins.command)
        elif kind == "frame":
            text = f"frame={value & 0x1FF} time={((value >> 11) & 15) + 1}"
            text += " flip" * ((value >> 9) & 1) + " flip_y" * ((value >> 10) & 1)
            words.append(text + " remap" * (value < 0))
        elif kind == "angle":
            verb = "set" if value & 0x1000 else "add"
            words.append(f"{verb} {(value & 0x1FF) << 3} part={(value >> 9) & 7}")
        elif kind in ("u8", "arg", "byte"):
            words.append(f"0x{value:02x}")
        elif kind in ("code", "header", "partner", "data", "table", "data24"):
            continue
        else:
            words.append(str(value))
    refs = [f"-> +0x{t:x}" for t in ins.targets]
    refs += [f"header +0x{h:x}" for h in ins.headers] + [f"data +0x{d:x}" for d in ins.data]
    text = f"+0x{ins.pc:05x}: {ins.opcode:02x} {ins.name} " + " ".join(words + refs)
    if ins.unhandled:
        text += f"  ; unhandled: {ins.unhandled}"
    return text.rstrip()


def signed8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def script_start(code: bytes, header: int) -> int:
    """80023538: the command pointer of the animation header at code[header]."""
    return header + 2 + u16(code, header + 2)


@dataclass(frozen=True)
class Listing:
    instructions: tuple[Instruction, ...]  # by offset
    scripts: tuple[int, ...]  # entry offsets: animations and the scripts they start
    errors: tuple[tuple[int, str], ...]


def disassemble(
    code: bytes,
    entries: Iterable[int],
    dialect: str = FIELD,
    *,
    bounds: tuple[int, int] | None = None,
) -> Listing:
    """Decode every command reachable from the script entries.

    Commands, and the headers spawned scripts start from, must lie within
    bounds (default: all of code). A command overlapping another one, an
    offset outside the bounds or a truncated command is an error; decoding
    continues on the other paths.
    """
    low, high = bounds if bounds is not None else (0, len(code))
    scripts = set(entries)
    pending = sorted(scripts, reverse=True)
    decoded: dict[int, Instruction] = {}
    owner: dict[int, int] = {}
    errors: list[tuple[int, str]] = []
    while pending:
        pc = pending.pop()
        if pc in decoded:
            continue
        if not low <= pc < high:
            errors.append((pc, "command outside the animation section"))
            continue
        if pc in owner:
            errors.append((pc, f"command inside the command at +0x{owner[pc]:x}"))
            continue
        try:
            ins = decode(code[:high], pc, dialect)
        except ScriptError as error:
            errors.append((pc, str(error)))
            continue
        clash = [b for b in range(pc + 1, pc + ins.length) if b in decoded or b in owner]
        if clash:
            errors.append((pc, f"command overlaps the command at +0x{clash[0]:x}"))
            continue
        decoded[pc] = ins
        for b in range(pc, pc + ins.length):
            owner[b] = pc
        if ins.reentry is not None:
            errors.append((pc, f"{ins.command} resumes inside its command at +0x{ins.reentry:x}"))
        for header in ins.headers:
            if not low <= header <= high - 6:
                errors.append((pc, f"animation header +0x{header:x} outside the section"))
                continue
            start = script_start(code, header)
            if start not in scripts:
                scripts.add(start)
                pending.append(start)
        pending.extend(ins.successors)
    return Listing(
        tuple(decoded[pc] for pc in sorted(decoded)), tuple(sorted(scripts)), tuple(errors)
    )


# ---------------------------------------------------------------------------
# Sprite resource blocks
# ---------------------------------------------------------------------------

# A resource block is an offset table: word 0 the section count n, words 1
# to n the section offsets, word n + 1 the size, so section 1 starts at
# 8 + 4n. The sprite engine reads only words 1-3 (80022224, SpriteSource):
# section 1 the animations, 2 the frame directory (8002435c sizes the part
# list from its first entry), 3 the palette. The data have 3 sections, or
# 4 to 6 in battle enemy and party sprite files (battle_loader.c passes them
# to 800242f4). Section 1 holds the animations: a directory halfword (bits
# 0-5 the animation count, as the frame map pointer +60 uses it, 8002435c;
# bits 6-11 a battle value, 80022224), the header offsets from the
# directory, the be frame map, then the headers (halfword 0 flags, 1 command
# offset from itself, 2 frame table offset from itself, 80023538) and their
# commands.
SECTION_COUNTS = range(3, 16)  # n scanned; first section offsets 0x14-0x44


def block_signature(sections: int) -> bytes:
    return struct.pack("<II", sections, 8 + 4 * sections)


@dataclass(frozen=True)
class ResourceBlock:
    offset: int  # in its view
    size: int
    sections: tuple[int, int, int]  # offsets of sections 1-3, from the block
    directory: int  # the directory halfword
    headers: tuple[int, ...]  # view offsets of the animation headers
    count: int = 3  # sections in the block's table

    @property
    def animations(self) -> tuple[int, int]:
        """The animation section's view offsets."""
        return self.offset + self.sections[0], self.offset + self.sections[1]


def resource_block(view: bytes, offset: int) -> ResourceBlock | None:
    """The sprite resource block at view[offset], when its layout checks.

    Besides the offsets, each animation header must lie in section 1 and
    start its commands and frame table there, after its first three
    halfwords (in the data the command offset is at least 6 and the table
    offset at least 2; only two coincidental tables elsewhere fail this).
    """
    if offset % 4 or offset + 8 > len(view):
        return None
    sections, first = struct.unpack_from("<II", view, offset)
    if sections not in SECTION_COUNTS or first != 8 + 4 * sections:
        return None
    if offset + first + 2 > len(view):
        return None
    bounds = struct.unpack_from(f"<{sections + 1}I", view, offset + 4)
    size = bounds[-1]
    if any(b < a for a, b in zip(bounds, bounds[1:])) or offset + size > len(view):
        return None
    second, third = bounds[1], bounds[2]
    if second <= first:
        return None
    directory = offset + first
    end = offset + second
    word = u16(view, directory)
    count = word & 0x3F
    if directory + 2 + 2 * count > end:
        return None
    headers = []
    for index in range(count):
        relative = u16(view, directory + 2 + 2 * index)
        header = directory + relative
        if relative < 2 + 2 * count or header + 6 > end:
            return None
        start = script_start(view, header)
        table = header + 4 + u16(view, header + 4)
        if not header + 6 <= start < end or not header + 6 <= table <= end:
            return None
        headers.append(header)
    return ResourceBlock(offset, size, (first, second, third), word, tuple(headers), sections)


def resource_blocks(view: bytes) -> Iterator[ResourceBlock]:
    """Every resource block of a view, by offset."""
    found = []
    for sections in SECTION_COUNTS:
        signature = block_signature(sections)
        position = view.find(signature)
        while position != -1:
            block = resource_block(view, position)
            if block is not None:
                found.append(block)
            position = view.find(signature, position + 1)
    yield from sorted(found, key=lambda block: block.offset)


def block_listing(view: bytes, block: ResourceBlock, dialect: str) -> Listing:
    """Decode a block's animation scripts and every script they reach."""
    starts = [script_start(view, header) for header in block.headers]
    return disassemble(view, starts, dialect, bounds=block.animations)


# Frame tables of a header by its facing groups (bits 0-1): one, three of
# four and five of eight (the others mirror one, 800223b0).
FRAME_TABLES = (1, 3, 5, 1)


def header_size(code: bytes, header: int) -> int:
    return 4 + 2 * FRAME_TABLES[u16(code, header) & 3]


def _header_fits(code: bytes, header: int, end: int) -> bool:
    """A header whose commands and frame tables follow it within the section."""
    size = header_size(code, header)
    if header + size > end or not header + 6 <= script_start(code, header) < end:
        return False
    tables = (header + 4 + 2 * k for k in range(FRAME_TABLES[u16(code, header) & 3]))
    return all(header + 6 <= at + u16(code, at) <= end for at in tables)


def unlisted_headers(view: bytes, block: ResourceBlock, listing: Listing) -> tuple[int, ...]:
    """Animation headers of section 1 that nothing in the recovered code starts.

    The bytes no listed or spawned header, frame table start or reached
    command covers (the directory area up to the first listed header counts
    as covered) form runs; a run made wholly of headers that fit is taken as
    headers that no directory lists and no command spawns. Data can take
    this shape by chance, so the result is reported apart from the scripts.
    """
    low, high = block.animations
    covered = bytearray(high - low)

    def mark(start: int, end: int) -> None:
        start, end = max(start, low), min(end, high)
        if start < end:
            covered[start - low : end - low] = b"\1" * (end - start)

    mark(low, min(block.headers))
    headers = set(block.headers)
    for ins in listing.instructions:
        mark(ins.pc, ins.pc + ins.length)
        headers.update(ins.headers)
    tables = set()
    for header in headers:
        mark(header, header + header_size(view, header))
        for k in range(FRAME_TABLES[u16(view, header) & 3]):
            at = header + 4 + 2 * k
            tables.add(at + u16(view, at))
    found = []
    at = 0
    while at < len(covered):
        if covered[at]:
            at += 1
            continue
        end = at
        while end < len(covered) and not covered[end]:
            end += 1
        run, header = [], low + at
        if header not in tables:
            while header < low + end and _header_fits(view, header, high):
                run.append(header)
                header += header_size(view, header)
        if run and header == low + end:
            found.extend(run)
        at = end
    return tuple(found)


# ---------------------------------------------------------------------------
# Locating the blocks on the extracted discs
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[2]
RESIDENT_BASE = 0x80010000 - 0x800  # file offset of an address in the boot executable
WIDTH_TABLE = 0x8004FCC0  # sprite_vm_command_lengths, the lengths of commands 80-ff
FIELD_ARCHIVES = {1: 606, 2: 601}  # slot of field 0's archive (verify_field.load_sources)
FIELD_MAPS = 730
FIELD_SPRITE_COMPONENT = 3  # the sprite bundle (field_load.cpp adopt_loaded_field)
FIELD_HEADER = 0x154
# Directory groups (cd_select_directory group + index) the battle overlay reads its
# files from: 0c+0 battle_mode.c, 0c+1 ovl2615 (enemy sets), 0c+2
# battle_action_files, 10+0/2 battle_turns_and_hud, 20+0/2/3 battle.c, 28+0/1/2
# battle_scene, 2c+0/1 battle_flow and ovl2615 battle_loader.c. Their
# sprites run while sprite_in_battle is set (battle 800B8840 to 800B8774); all
# other data runs under 800248d4 itself.
BATTLE_DIRECTORIES = frozenset({12, 13, 14, 16, 18, 32, 34, 35, 40, 41, 42, 44, 45})


@dataclass(frozen=True)
class View:
    slot: int
    name: str
    data: bytes


def directory_starts(raw_track: Path) -> list[tuple[int, int]]:
    """(first slot, directory) pairs from sector 40 (cd_directory_table, 1-based)."""
    with raw_track.open("rb") as stream:
        stream.seek(40 * 2352 + 24)
        sector = stream.read(122)
    starts = []
    for index, value in enumerate(struct.unpack(f"<{len(sector) // 2}H", sector)):
        if value not in (0, 0xFFFF):
            starts.append((value - 1, index))
    return sorted(starts)


def _unpack(data: bytes) -> bytes | None:
    if len(data) < 8 or int.from_bytes(data[:4], "little") > 0x200000:
        return None
    try:
        return decode_block(data).data
    except PackedError:
        return None


def _container(data: bytes) -> list[tuple[int, int]] | None:
    """Entries of an offset table: a count, offsets and (usually) the size."""
    if len(data) < 12:
        return None
    count = int.from_bytes(data[:4], "little")
    if not 1 <= count <= 1024 or 8 + 4 * count > len(data):
        return None
    offsets = list(struct.unpack_from(f"<{count}I", data, 4))
    if offsets[0] == 8 + 4 * count:
        end = int.from_bytes(data[4 + 4 * count : 8 + 4 * count], "little")
    elif offsets[0] == 4 + 4 * count:
        end = len(data)
    else:
        return None
    entries = list(zip(offsets, offsets[1:] + [end], strict=True))
    if end > len(data) or any(b < a for a, b in entries):
        return None
    return entries


def _packed_entries(name: str, data: bytes, depth: int) -> Iterator[tuple[str, bytes]]:
    """Packed entries of nested offset tables, unpacked."""
    entries = _container(data) if depth else None
    for index, (start, end) in enumerate(entries or ()):
        unpacked = _unpack(data[start:])
        if unpacked is not None and end - start >= 8:
            yield f"{name}/{index}p", unpacked
            yield from _packed_entries(f"{name}/{index}p", unpacked, depth - 1)
        else:
            yield from _packed_entries(f"{name}/{index}", data[start:end], depth - 1)


def file_views(disc: int, slot: int, data: bytes, counts: Counter) -> Iterator[View]:
    """The raw file, the field sprite bundle, unpacked files and entries."""
    first = FIELD_ARCHIVES[disc]
    if first <= slot < first + 2 * FIELD_MAPS and (slot - first) % 2 == 0:
        if len(data) < FIELD_HEADER:
            counts["field archive placeholders"] += 1
        else:
            index = FIELD_SPRITE_COMPONENT
            size = int.from_bytes(data[0x10C + 4 * index : 0x110 + 4 * index], "little")
            offset = int.from_bytes(data[0x130 + 4 * index : 0x134 + 4 * index], "little")
            bundle = decode_block(data[offset:], output_limit=min(size + 16, 0x200000))
            counts["field archives"] += 1
            yield View(slot, "field sprite bundle", bundle.data[:size])
    yield View(slot, "raw", data)
    unpacked = _unpack(data)
    if unpacked is not None:
        yield View(slot, "unpacked", unpacked)
    for name, entry in _packed_entries("raw", data, 2):
        yield View(slot, name, entry)
    if unpacked is not None:
        for name, entry in _packed_entries("unpacked", unpacked, 2):
            yield View(slot, name, entry)


@dataclass
class SweepReport:
    counts: Counter = field(default_factory=Counter)
    opcodes: dict[str, Counter] = field(default_factory=lambda: {d: Counter() for d in DIALECTS})
    commands: dict[str, Counter] = field(default_factory=dict)  # c8, battle, bc
    directories: dict[str, set] = field(default_factory=lambda: {d: set() for d in DIALECTS})
    sections: dict[str, Counter] = field(default_factory=lambda: {d: Counter() for d in DIALECTS})
    widths: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)  # undecodable, with locations
    # Decoded commands their interpreter has no case for (they advance by their
    # width without effect): description -> locations, and distinct blocks.
    unhandled: dict[str, list[str]] = field(default_factory=dict)
    unhandled_blocks: dict[str, set] = field(default_factory=dict)
    unique: dict[str, set] = field(default_factory=lambda: {d: set() for d in DIALECTS})
    # Opcodes of the commands that only unlisted headers' scripts reach.
    unlisted: dict[str, Counter] = field(default_factory=lambda: {d: Counter() for d in DIALECTS})


def _note(report: SweepReport, key: str, value: str) -> None:
    report.commands.setdefault(key, Counter())[value] += 1


def _note_unlisted(
    report: SweepReport, view: bytes, block: ResourceBlock, listing: Listing, dialect: str
) -> None:
    """Count a distinct block's unlisted headers and the commands their scripts add."""
    unlisted = unlisted_headers(view, block, listing)
    if not unlisted:
        return
    counts = report.counts
    counts[f"{dialect} unlisted headers"] += len(unlisted)
    counts[f"{dialect} blocks with unlisted headers"] += 1
    entries = [script_start(view, h) for h in block.headers + unlisted]
    combined = disassemble(view, entries, dialect, bounds=block.animations)
    reached = {ins.pc for ins in listing.instructions}
    for ins in combined.instructions:
        if ins.pc not in reached:
            counts[f"{dialect} unlisted commands"] += 1
            report.unlisted[dialect][ins.opcode] += 1
    counts[f"{dialect} unlisted undecodable"] += len(set(combined.errors) - set(listing.errors))


def sweep_disc(root: Path, disc: int, report: SweepReport, listing_dir: Path | None = None) -> None:
    extract = root / ".local" / "extract" / f"disc{disc}"
    manifest = json.loads((extract / "manifest.json").read_text())
    exe = (extract / manifest["boot"]["name"]).read_bytes()
    table = exe[WIDTH_TABLE - RESIDENT_BASE : WIDTH_TABLE - RESIDENT_BASE + 0x80]
    wrong = [op for op in range(0x80, 0x100) if table[op - 0x80] != table_width(op)]
    report.widths.append(
        f"disc {disc} {manifest['boot']['name']}: widths 80-ff "
        + ("match the length rule" if not wrong else f"differ at {wrong}")
    )
    starts = directory_starts(root / ".local" / "discs" / f"disc{disc}.bin")
    firsts = [slot for slot, _ in starts]
    counts = report.counts
    for entry in manifest["files"]:
        if entry["size"] <= 0:
            continue
        slot = entry["slot"]
        directory = starts[bisect.bisect_right(firsts, slot) - 1][1]
        dialect = BATTLE if directory in BATTLE_DIRECTORIES else FIELD
        data = (extract / "files" / f"{slot:04d}.bin").read_bytes()
        counts[f"disc {disc} files"] += 1
        for view in file_views(disc, slot, data, counts):
            counts["views"] += 1
            for block in resource_blocks(view.data):
                where = f"disc{disc} slot {slot} {view.name} block+0x{block.offset:x}"
                if not block.headers:
                    counts["blocks without animations"] += 1
                    continue
                counts[f"{dialect} blocks"] += 1
                report.sections[dialect][block.count] += 1
                report.directories[dialect].add(directory)
                listing = block_listing(view.data, block, dialect)
                body = view.data[block.offset : block.offset + block.size]
                digest = hashlib.sha256(body).digest()
                fresh = digest not in report.unique[dialect]
                report.unique[dialect].add(digest)
                counts[f"{dialect} animations"] += len(block.headers)
                counts[f"{dialect} scripts"] += len(listing.scripts)
                counts[f"{dialect} instructions"] += len(listing.instructions)
                if fresh:
                    counts[f"{dialect} unique scripts"] += len(listing.scripts)
                    counts[f"{dialect} unique instructions"] += len(listing.instructions)
                for pc, message in listing.errors:
                    report.errors.append(f"{where} +0x{pc - block.offset:x}: {message}")
                for ins in listing.instructions:
                    report.opcodes[dialect][ins.opcode] += 1
                    at = f"{where} +0x{ins.pc - block.offset:x} {ins.opcode:02x}"
                    if ins.unhandled:
                        kind = f"{dialect} {ins.opcode:02x} {ins.command}".rstrip()
                        report.unhandled.setdefault(kind, []).append(at)
                        report.unhandled_blocks.setdefault(kind, set()).add(digest)
                    if ins.opcode == 0xC8:
                        _note(report, f"{dialect} c8 command_var commands", ins.command)
                    elif ins.opcode == 0xBC:
                        _note(report, f"{dialect} bc place selectors", ins.command)
                    elif dialect == BATTLE and ins.opcode in (0xC3, 0xE8, 0xEC, 0xF9):
                        _note(report, f"battle {ins.opcode:02x} {ins.name} commands", ins.command)
                        if ins.overread:
                            _note(report, "battle commands reading past their bytes", at)
                if fresh:
                    _note_unlisted(report, view.data, block, listing, dialect)
                if listing_dir is not None and fresh:
                    name = f"disc{disc}-{slot:04d}-{view.name.replace('/', '_').replace(' ', '_')}"
                    path = listing_dir / f"{name}-{block.offset:06x}-{dialect}.txt"
                    path.write_text("\n".join(render(i) for i in listing.instructions) + "\n")


def sweep(root: Path = ROOT, listing_dir: Path | None = None) -> SweepReport:
    report = SweepReport()
    for disc in (1, 2):
        sweep_disc(root, disc, report, listing_dir)
    return report


def print_report(report: SweepReport, out=sys.stdout) -> None:
    counts = report.counts

    def line(text: str = "") -> None:
        print(text, file=out)

    for text in report.widths:
        line(text)
    line(
        f"files {counts['disc 1 files']} + {counts['disc 2 files']}, views {counts['views']}, "
        f"field archives {counts['field archives']} "
        f"(+{counts['field archive placeholders']} placeholders)"
    )
    for dialect in DIALECTS:
        vm = INTERPRETERS[dialect]
        dirs = ",".join(str(d) for d in sorted(report.directories[dialect]))
        widths = ", ".join(f"{n} sections {c}" for n, c in sorted(report.sections[dialect].items()))
        line(
            f"{dialect} ({vm}): blocks {counts[f'{dialect} blocks']} "
            f"({len(report.unique[dialect])} distinct; {widths}; directories {dirs}), "
            f"animations {counts[f'{dialect} animations']}, "
            f"scripts {counts[f'{dialect} scripts']} "
            f"({counts[f'{dialect} unique scripts']} in distinct blocks), "
            f"instructions {counts[f'{dialect} instructions']} "
            f"({counts[f'{dialect} unique instructions']} in distinct blocks)"
        )
    line(f"blocks without animations: {counts['blocks without animations']}")
    for dialect in DIALECTS:
        used = report.opcodes[dialect]
        cases = [op for op in used if op >= 0x80 and spec_for(op, dialect).name != "unhandled"]
        line()
        line(
            f"{dialect} opcodes used: {len(used)}; with a handler "
            f"{len(cases) + sum(1 for op in used if op < 0x80)} of {defined_opcodes(dialect)} "
            f"({len(cases)} of {defined_opcodes(dialect) - 0x80} cases 80-ff, "
            f"{sum(1 for op in used if op < 0x80)} of 128 timed 00-7f), "
            f"without one {sum(1 for op in used if op >= 0x80) - len(cases)}"
        )
        row = []
        for opcode in sorted(used):
            row.append(f"{opcode:02x} {spec_for(opcode, dialect).name} {used[opcode]}")
            if len(row) == 4:
                line("  " + "; ".join(row))
                row = []
        if row:
            line("  " + "; ".join(row))
    for key in sorted(report.commands):
        values = report.commands[key]
        line()
        line(f"{key}: {len(values)} distinct, {sum(values.values())} uses")
        if key != "battle commands reading past their bytes":
            line("  " + "; ".join(f"{name} {n}" for name, n in sorted(values.items())))
    forms = [f"battle {op:02x} {BATTLE_SPECS[op].name} commands" for op in (0xC3, 0xE8, 0xEC, 0xF9)]
    battle = set().union(*(report.commands.get(key, ()) for key in forms))
    line()
    line(f"battle commands (800b3f04) used in any form: {len(battle)} of {len(BATTLE_COMMANDS)}")
    line()
    line(f"undecodable: {len(report.errors)}")
    for text in report.errors[:50]:
        line("  " + text)
    if len(report.errors) > 50:
        line(f"  ... {len(report.errors) - 50} more")
    uses = sum(len(v) for v in report.unhandled.values())
    line(f"decoded without a handler (no effect, advance by width): {uses}")
    for kind, places in sorted(report.unhandled.items()):
        blocks = len(report.unhandled_blocks[kind])
        line(f"  {kind}: {len(places)} uses in {blocks} distinct blocks, e.g. {places[0]}")
    line()
    line("not started by recovered code: headers no directory lists and no command spawns")
    for dialect in DIALECTS:
        headers = counts[f"{dialect} unlisted headers"]
        if not headers:
            line(f"  {dialect}: none")
            continue
        used = report.unlisted[dialect]
        line(
            f"  {dialect}: {headers} headers in {counts[f'{dialect} blocks with unlisted headers']} "
            f"distinct blocks; their scripts add {counts[f'{dialect} unlisted commands']} commands, "
            f"{counts[f'{dialect} unlisted undecodable']} undecodable"
        )
        new = sorted(op for op in used if op not in report.opcodes[dialect])
        line(
            "    opcodes the started scripts do not use: "
            + ("; ".join(f"{op:02x} {spec_for(op, dialect).name} {used[op]}" for op in new) or "none")
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sweep", action="store_true", help="decode the sprite data of both discs")
    parser.add_argument("--root", type=Path, default=ROOT, help="checkout holding .local/")
    parser.add_argument(
        "--listing",
        type=Path,
        help="also write one listing per distinct block into this directory under .local/",
    )
    args = parser.parse_args(argv)
    if not args.sweep:
        parser.error("nothing to do: pass --sweep")
    listing_dir = None
    if args.listing is not None:
        listing_dir = args.listing.resolve()
        private = (args.root / ".local").resolve()
        if private not in listing_dir.parents and listing_dir != private:
            parser.error("listings hold original data: write them under .local/")
        listing_dir.mkdir(parents=True, exist_ok=True)
    report = sweep(args.root, listing_dir)
    print_report(report)
    return 1 if report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
