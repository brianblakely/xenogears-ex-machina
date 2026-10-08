"""Field event script disassembler, read from the recovered field interpreter.

The field overlay runs each event actor's script with 800a1ec8
(decomp/src/field/field_800854D0.c): the byte at the actor's working PC
indexes the 256 handlers of D_800AE2A0; fe (800869b8) moves the PC onto the
next byte and indexes the 227 extended handlers of D_800AE6A0 with it, so an
extended handler reads its operands relative to the extended byte. Extended
bytes e3-ff index past the table and are unknown. A handler advances the PC
itself; one that does not advance runs again on the actor's next pass (waits
yield first). An extended handler that waits steps back onto fe; one that
neither advances nor steps back leaves the PC on the extended byte, which then
runs as the primary opcode of the same value ("rerun").

Scripts are the event component of each field map bundle (directory (4, 0)
file 0xb8 + 2 * map, read ahead by 8001b53c): 128 bytes of variable type bits,
the actor count, 32 u16 entry PCs per actor and the bytecode (80070cc8:
D_800ADC00 = entries + count * 64). Events start only at their entry PC:
event 0 of every actor at load (800a28d4), event 1 whenever no slot is active
(800a2030), event 2 on talk and 3 on touch (8008399c), actor 0's events 2 and
3 after a return and for the party rebuild (800a22ac), a joining member's
event 0 (8008b978) and any event 07-09 request. When the bytecode opens with
ff, 8009fa54 reads 7-byte map entry records from +1, so a zero entry names no
script there.

Each table entry names its handler; sizes are the PC advance of the handler's
completing path (extended sizes exclude the fe prefix) and operand offsets are
those the handler reads. The walk follows every successor and the alternative
advances that step over the next instruction whole; it reports, without
following, those that land inside an instruction and 12's movie-pending
continuation, and follows a variable a6 index into each consecutive three-byte
jump after it. `--sweep` decodes every map of both discs and prints aggregate
counts only; `--list MAP` prints one map's listing to stdout (keep listings
under .local/).
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from tools.analysis.arithmetic import signed16
from tools.analysis.field import EventPackage, FieldError, event_package, region
from tools.analysis.packed import PackedError, decode_block

ROOT = Path(__file__).resolve().parents[2]


class EventError(ValueError):
    """Bytes that the recovered interpreter cannot run as an instruction here."""


class UnknownInstruction(EventError):
    def __init__(self, pc: int, opcode: int, *, namespace: str = "primary"):
        self.pc, self.opcode, self.namespace = pc, opcode, namespace
        super().__init__(f"unknown {namespace} opcode 0x{opcode:02x} at bytecode PC +0x{pc:04x}")


COMPARISONS = (
    "equal",
    "not_equal",
    "greater",
    "less",
    "greater_or_equal",
    "less_or_equal",
    "and_nonzero",
    "not_equal_alias",
    "or_nonzero",
    "and_nonzero_alias",
    "not_left_and_right_nonzero",
)
CONDITION_TEXT = ("==", "!=", ">", "<", ">=", "<=", "&", "!=", "|", "&", "~&")

# Operand kinds and the readers they come from: u8 a byte; actor a selector
# byte (8009cdb4: ff/fe/fd party slots 0/1/2, fb the running actor); s16 and
# u16 halfwords (800acd7c, 800acdb8); var a variable reference (a byte offset
# into the bank, 800a3018/800a3074); iv 800acdec (bit 15 a 15-bit immediate,
# else a variable); sel 8009cf78-8009d154 (a signed immediate when the flags
# byte has the bit, else a variable); addr a bytecode PC; data a bytecode
# offset read as a table; bit a variable bit (variable >> 4, bit & 15); msg a
# message number; cond 02's mode and comparison byte; flags a selector byte.
WIDTHS = {
    "u8": 1,
    "actor": 1,
    "flags": 1,
    "cond": 1,
    "s16": 2,
    "u16": 2,
    "var": 2,
    "iv": 2,
    "sel": 2,
    "addr": 2,
    "data": 2,
    "bit": 2,
    "msg": 2,
    "u32": 4,
}
FLOWS = ("next", "wait", "end", "hang", "rerun", "jump", "branch", "call", "return", "table")

# opcode[/form] mnemonic handler size flow operands... [+N an alternative
# advance, ?zero/?party when only those operands allow it] [~N an advance
# into its own operands] [<N the step reads its set-up N bytes back]; the
# indented lines give the effect. A form `/k=v` (`/k&m=v`, `/k=*` for the
# rest) picks by the byte at offset k. Flows: next; wait (may run again first);
# end (ends the slot); hang (never advances); rerun; jump; branch (next or the
# address); call (the address, returning to next); return; table (a6).
_PRIMARY = """
00 end_slot func_800A1B70 1 end
    end the current script slot (priority 15, tag ff) and yield
01 jump func_800A1E74 3 jump addr@1
    continue at operand 1
02 branch_if_false func_800A1BD0 8 branch sel@1:5/80 sel@3:5/40 cond@5 addr@6
    compare operands 1 and 3 by the low nibble of byte 5 (bits 7/6 make them
    immediates; a variable typed unsigned makes the other side unsigned);
    continue when it holds, else jump to operand 6
03 message_box func_8009C104 4 wait msg@1 u8@3
    open the running actor's window for message operand 1 in the fixed box
    (8009c5a8 mode 2, byte 3 the style); retried until it opens
04 reset_idle_and_end func_800A1A8C 1 end
    point every priority-7 slot at event 1, end the current slot and yield
05 call func_800A17F4 3 call addr@1
    push pc + 3 and continue at operand 1; retried with four calls nested
06 call_long func_800A1730 5 call addr@1
    push pc + 5 and continue at operand 1; retried with four calls nested
07 request_event func_8009EB78 3 wait actor@1 u8@2
    queue event byte 2 & 1f at priority byte 2 >> 5 in the selected actor's
    first free slot (not when a slot has it already); retried while none is free
08 request_event_started func_8009ED68 3 wait actor@1 u8@2
    queue as 07, then yield until the selected actor has started it
09 request_event_finished func_8009F0A0 3 wait actor@1 u8@2
    queue as 07, then yield until the selected actor has run it to its end
0a call_in_zone func_8009533C 4 call u8@1 addr@2
    with the controlled actor inside trigger zone byte 1 (and call room), push
    pc + 4 and continue at operand 2, else continue
0b set_sprite func_800A1624 3 next iv@1
    give the running actor field sprite operand 1 (80076ac0, bank 0) and
    enable it
0c loop_player_control func_8009F5A8 1 hang
    run player control (a7) for this frame and yield on the same instruction
0d return func_800A18B8 1 return
    return to the innermost pushed pc; with none, report and end the slot
0e nop func_80092404 1 next
    advance
0f nop func_800923E4 1 next
    advance
10/1=00 walk_to func_80098C00 9 next u8@1 sel@2:8/80 sel@4:8/40 sel@6:8/20 flags@8
    set up a walk of the running actor to x, z, y operands 2, 4, 6 without
    a step limit (80098cac mode 0)
10/1=* walk_step func_80098C00 2 wait u8@1 <9
    step the walk set up 9 bytes back, yielding, until it arrives; then
    snap to its target and play the arrival animation (+e6)
11/1=00 walk_to_limited func_80098C3C 9 next u8@1 sel@2:8/80 sel@4:8/40 sel@6:8/20 flags@8
    set up as 10, latching the step limit at pc + 11 (operand 2 of the step)
11/1=* walk_step_limited func_80098C3C 4 wait u8@1 iv@2 <9
    step the walk set up 9 bytes back; out of steps it stops without snapping
12 change_map_transition func_80093200 9 wait iv@1 iv@3 iv@5 iv@7 ~4
    once field control allows, request map operand 1 at entry operand 3 (98)
    with transition kind operand 5 and frames operand 7; with a movie pending
    98 does not advance and the handler continues at pc + 4
13 nop func_800A2FC0 1 next
    advance one byte (also fd and ff)
14 allow_encounters func_80093C48 1 next
    clear the encounter inhibition
15 inhibit_encounters func_80093C6C 1 wait
    inhibit encounters once the field is ready
16 become_party_character func_800A08B8 3 next iv@1
    the running actor stands for character operand 1: a party member takes its
    slot, sprite and map entry; otherwise it is hidden and its script ends
17 set_boundary func_8009E91C 18 next sel@1:17/80 sel@3:17/40 sel@5:17/20 sel@7:17/10 \
        sel@9:17/08 sel@11:17/04 sel@13:17/02 sel@15:17/01 flags@17
    give the running actor a boundary quadrilateral of four x/z corners
18 set_extents func_8009E83C 5 next u8@1 u8@2 u8@3 u8@4
    set +18, +1c, +1a (height) and +1e (reach radius) from non-zero
    bytes 1-4, doubled
19 place func_8009E4BC 6 next sel@1:5/80 sel@3:5/40 flags@5
    place the running actor on the floor at x, z operands 1 and 3
1a set_layer func_8009E428 2 next u8@1
    move the running actor to collision layer byte 1 at its own x/z
1b place_on_layer func_8009E35C 7 next sel@1:6/80 sel@3:6/40 u8@5 flags@6
    place the running actor on layer byte 5 at x, z operands 1 and 3
1c set_height func_8009E2C8 4 next sel@1:3/80 flags@3
    set the running actor's height to operand 1 and its flag 0x40000
1d place_at_height func_8009E248 7 next s16@1 s16@3 s16@5
    place the running actor on the floor at x, z operands 1, 3, then set its
    height to operand 5
1e start_fall func_8009E208 1 next
    set flag 0x400000 (clearing 0x40000) from the running actor's height
1f set_layer_mask func_8009E1A0 2 next u8@1
    set layer flag bits 0-2 from byte 1 bits 0-2 and 3-5 from bits 4-6
20 set_actor_flags func_8009E10C 3 next iv@1
    map operand 1 bits 0, 2-6 onto actor flags 80, 20, 10, 8, 4, 8000000
21 set_motion_divisor func_8009E094 3 next iv@1
    set the running actor's motion divisor (+76: walks and moves advance
    0x4000000 / it per step) and its sprite's
22 enable_self func_8009DF10 1 next
    clear the running descriptor's flag 0x20 and layer flag 0x2000000
23 disable_self func_8009E040 1 next
    set the running descriptor's flag 0x20
24 enable_actor func_8009DDEC 2 next actor@1
    as 22 for the selected actor unless it is removed
25 disable_actor func_8009DE94 2 next actor@1
    set the selected actor's descriptor flag 0x20
26 wait_countdown func_8009DD34 3 wait iv@1
    wait operand-1 frames counted in the slot, yielding each frame
27 stop_hide_actor func_8009DC4C 2 next actor@1
    stop and hide the selected actor; free the running actor's idle window
28 show_actor func_8009DBC8 2 next actor@1
    clear the selected actor's hidden flag
29 remove_actor func_8009DAC4 2 next actor@1
    hide the selected actor, stop its scripts and disable its descriptor
2a set_flag_20000 func_8009DA1C 1 next
    set the running actor's flag 0x20000 (talk and touch do not start its
    events 2 and 3)
2b clear_flag_20000 func_8009DA44 1 next
    clear the running actor's flag 0x20000
2c set_animation func_8009A130 2 next u8@1
    set the running actor's requested animation (+ea) to byte 1
2d store_actor_position func_8009A024 8 next actor@1 var@2 var@4 var@6
    store the selected descriptor's x, z, y in three variables
2e store_facing_octant func_80099FC4 3 next var@1
    store the running actor's facing octant in a variable
2f store_own_character func_80099EF8 3 next var@1
    store the running actor's party character (+e4) in a variable
30 store_controlled_character func_80099F48 3 next var@1
    store the controlled actor's party character (+e4) in a variable
31 branch_unless_buttons func_800961A0 5 branch u16@1 addr@3
    continue when the held buttons share a bit with operand 1, else jump
32 branch_unless_buttons_seen func_800961C8 5 branch u16@1 addr@3
    continue when the buttons seen held since 33 (800afc6c) share a bit
    with operand 1, else jump to operand 3
33 forget_buttons_seen func_800961F0 1 next
    clear the buttons seen held (800afc6c)
34 store_item_count func_80096214 5 next iv@1 var@3
    store the carried count of item operand 1 in a variable
35 set_variable func_8009D9A4 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 = operand 3
36 set_variable_one func_8009D960 3 next var@1
    variable operand 1 = 1
37 set_variable_zero func_8009D91C 3 next var@1
    variable operand 1 = 0
38 add_variable func_8009D890 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 += operand 3
39 subtract_variable func_8009D804 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 -= operand 3
3a set_variable_bit func_8009D644 6 next var@1 sel@3:5/40 flags@5
    set bit operand 3 of variable operand 1
3b clear_variable_bit func_8009D408 6 next var@1 sel@3:5/40 flags@5
    clear bit operand 3 of variable operand 1
3c increment_variable func_8009D340 3 next var@1
    variable operand 1 += 1
3d decrement_variable func_8009D3A4 3 next var@1
    variable operand 1 -= 1
3e and_variable func_8009D5B8 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 &= operand 3
3f or_variable func_8009D52C 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 |= operand 3
40 xor_variable func_8009D4A0 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 ^= operand 3
41 shift_left_variable func_8009D2D0 5 next var@1 iv@3
    variable operand 1 <<= operand 3
42 shift_right_variable func_8009D260 5 next var@1 iv@3
    variable operand 1 >>= operand 3
43 random_variable func_8009D198 3 next var@1
    variable operand 1 = rand()
44 turn_move_angle func_80098184 5 wait iv@1 iv@3
    turn-move (80099ac0 mode 3) along angle operand 1 from the current position
    until it arrives or step limit operand 3 runs out
45 move_angle func_80097864 8 wait iv@1 sel@3:7/80 iv@5 flags@7
    move (80097a50 mode 3) along angle operand 1, height offset operand 3,
    until it arrives or step limit operand 5 runs out
46 set_interaction_offset func_80092808 1 next
    offset the running actor's interaction point (+60/+64) 36 units
    along the running descriptor's facing; layer flag 0x800
47 walk_player_ahead func_80092EA0 6 wait iv@2 iv@4
    once field control allows, walk the controlled actor (80092894 mode
    0) to the point 40 units along the running descriptor's facing - 0x20
    (operands 2/4: the map and entry it requests while none is pending)
48 load_code_byte func_80093CD0 7 next data@1 var@3 iv@5
    variable operand 3 = the bytecode byte at operand 1 + operand 5
49 load_code_half func_80093D48 8 next data@1 var@3 iv@5 u8@7
    variable operand 3 = the bytecode halfword at operand 1 + operand 5
    (unsigned when byte 7 is 0)
4a turn_move_to func_80099980 6 wait sel@1:5/80 sel@3:5/40 flags@5
    turn-move (80099ac0 mode 0) to x, z operands 1 and 3, no step limit
4b turn_move_to_limited func_80098430 8 wait sel@1:5/80 sel@3:5/40 flags@5 iv@6
    as 4a with step limit operand 6
4c move_to func_800979F0 8 wait sel@1:5/80 sel@3:5/40 flags@5 sel@6:5/20
    move (80097a50 in the slot's mode, 0 when idle) to x, z, y operands 1, 3,
    6, no step limit
4d move_to_limited func_80097954 10 wait sel@1:5/80 sel@3:5/40 flags@5 sel@6:5/20 iv@8
    as 4c with step limit operand 8
4e turn_move_by func_80098370 6 wait sel@1:5/80 sel@3:5/40 flags@5
    turn-move (mode 1) by x, z operands 1 and 3 from the current position
4f turn_move_by_limited func_80098274 8 wait sel@1:5/80 sel@3:5/40 flags@5 iv@6
    as 4e with step limit operand 6
50 move_by func_800977A4 8 wait sel@1:5/80 sel@3:5/40 flags@5 sel@6:5/20
    move (mode 1) by x, z, y operands 1, 3, 6 from the current position
51 move_by_limited func_800976A8 10 wait sel@1:5/80 sel@3:5/40 flags@5 sel@6:5/20 iv@8
    as 50 with step limit operand 8
52 turn_move_to_actor func_800980FC 2 wait actor@1
    turn-move (mode 2) to the selected actor, no step limit
53 turn_move_to_actor_limited func_80098038 4 wait actor@1 iv@2
    as 52 with step limit operand 2
54 move_to_actor func_800975C0 5 wait actor@1 sel@2:4/80 flags@4
    move (mode 2) to descriptor byte 1's x/z (the selector gives the reach) at
    height operand 2
55 move_to_actor_limited func_8009749C 7 wait actor@1 sel@2:4/80 flags@4 iv@5
    as 54 with step limit operand 5
56 change_map func_80093014 10 wait sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    once field control allows, request map operand 1 at entry operand 7 with
    heading operand 5 (ffff the camera's) and +231e operand 3; yields
57/1=0f arc_floor func_80099214 2 next u8@1
    re-read the running actor's floor triangles and end the jump; yields
57/1&03=03 arc_step func_80099214 2 wait u8@1 <11
    step the arc jump set up 11 bytes back until it lands; yields
57/1=* arc_jump func_80099214 11 next u8@1 sel@2:10/80 sel@4:10/40 sel@6:10/20 sel@8:10/10 flags@10
    set up a jump to x, z operands 2, 4 and height operand 6 (bit 7 of
    byte 1: the floor of layer operand 6); operand 8 gives the steps, a
    speed or the peak height by mode (byte 1 & 3); yields
58 set_rotation func_80094918 4 next iv@1 u8@3
    set the running descriptor's rotation axis byte 3 to operand 1
59 wander_pause func_8009F4CC 1 next
    every 16th pass turn or hold the facing at random; yields
5a stop func_8009524C 1 next
    stop the running actor (5b) and advance
5b stop_hold func_80095284 1 hang
    stop the running actor's motion and yield on the same instruction
5c become_party_slot func_800A0228 3 next iv@1
    the running actor stands for party slot operand 1 at the member's recorded
    position (bc's sprite when the slot is empty)
5d play_animation func_8009A174 2 next u8@1
    as 2c and clear layer flag 0x10000, which the sprite's completion
    callback sets again
5e wait_animation func_8009A1AC 1 wait
    once layer flag 0x10000 is set again, clear the requested animation
5f face_direction_table3 func_8009AD6C 2 next u8@1
    face direction byte 1 of the third facing table
60 save_target_goal func_8008FDD0 1 next
    copy the camera target goal to the saved target
61 set_saved_target func_8008FE2C 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the saved camera target to x, z, y operands 1, 3, 5
62 point_a_at_actor func_8008FF04 2 next actor@1
    set camera point A to the selected actor's position
63 set_point_a func_8008FF90 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set camera point A to x, z, y operands 1, 3, 5
64 save_eye_goal func_80090068 1 next
    copy the camera eye goal to the saved eye
65 set_saved_eye func_800900C4 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the saved camera eye to x, z, y operands 1, 3, 5
66 point_b_at_actor func_8009019C 2 next actor@1
    set camera point B to the selected actor's position
67 actor_face_direction func_8009ABFC 4 next actor@1 iv@2
    the selected actor faces direction operand 2
68 actor_face_view_direction func_8009AC34 4 next actor@1 iv@2
    the selected actor faces direction operand 2 relative to the camera
69 face_direction func_8009AC7C 3 next iv@1
    face direction operand 1
6a face_view_direction func_8009ACB4 3 next iv@1
    face direction operand 1 relative to the camera
6b turn_clockwise func_8009AB5C 3 next iv@1
    turn by operand-1 octants
6c turn_counterclockwise func_8009ABAC 3 next iv@1
    turn back by operand-1 octants
6d sine_variable func_8009A6AC 8 next var@1 sel@3:7/40 sel@5:7/20 flags@7
    variable operand 1 = sin(operand 3) * operand 5 >> 12
6e cosine_variable func_8009A768 8 next var@1 sel@3:7/40 sel@5:7/20 flags@7
    variable operand 1 = cos(operand 3) * operand 5 >> 12
6f face_actor func_8009A2A8 2 next actor@1
    face the selected actor
70 face_party_member func_8009A1E4 2 next u8@1
    face the actor of party slot byte 1
71 request_battle func_80093568 3 wait iv@1
    once field control allows, leave the field for module operand 1; yields
72 select_music func_8008F724 3 wait iv@1
    select field music operand 1 (8008f7b8, 8004f340 = 0) once music can change
73/1=00 emitter_skip func_80086C34 2 next u8@1
    continue
73/1=01 emitter_start func_80086C34 8 next u8@1 iv@2 iv@4 iv@6
    reset the emitter templates for the running actor, set template 0
    from operand 4 and start the effect
73/1=* emitter_halt func_80086C34 2 hang u8@1
    no case: never advances
74 play_sound func_8008F668 3 next iv@1
    play sound effect operand 1 on voice pair 3 (80085634)
75 select_music_keep func_8008F76C 3 wait iv@1
    select field music operand 1 (8008f7b8, 8004f340 = -1) once music can change
76 release_camera_hold func_80093A68 1 next
    clear camera flag 0x8000
77 hold_camera func_80093A98 1 next
    set camera flag 0x8000
78 read_map_ahead func_800973A4 4 wait u8@1 u16@2
    start reading file 0xb8 + operand 2 ahead as map data (8001b484,
    slot byte 1); retried until the read is under way or done
79 restore_all_hp func_80097264 1 next
    restore every character's HP
7a restore_all_ep func_800972AC 1 next
    restore every character's EP
7b reduce_party_hp func_800969FC 4 next sel@1:3/80 u8@3
    reduce the HP of the party members of mask byte 3 & 3 by operand 1
    (800968cc)
7c restore_party_ep_7c func_80096F18 4 next sel@1:3/80 u8@3
    restore the EP of the party members of mask byte 3 & 3 by operand 1
    (80096920), as 7e
7d reduce_party_ep func_80097010 4 next sel@1:3/80 u8@3
    reduce the EP of the party members of mask byte 3 & 3 by operand 1
    (800969a8)
7e restore_party_ep func_80097108 4 next sel@1:3/80 u8@3
    restore the EP of the party members of mask byte 3 & 3 by operand 1
    (80096920)
7f set_terrain_angle func_80095300 3 next iv@1
    set the terrain angle to operand 1
80 set_collision_attribute func_80092664 5 next u8@1 u8@2 iv@3
    set collision attribute byte 2 of entry byte 1 to operand 3
81 or_collision_attribute func_800926C8 5 next u8@1 u8@2 iv@3
    or operand 3 into collision attribute byte 2 of entry byte 1
82 store_collision_attribute func_80093664 5 next u8@1 u8@2 var@3
    store collision attribute byte 2 of entry byte 1 in a variable
83 and_collision_attribute func_80092768 5 next u8@1 u8@2 iv@3
    and operand 3 into collision attribute byte 2 of entry byte 1
84 branch_unless_var0_below func_80096644 5 branch iv@1 addr@3
    continue when variable 0 < operand 1, else jump to operand 3
85 branch_unless_var0_above func_800966B4 5 branch iv@1 addr@3
    continue when variable 0 > operand 1, else jump to operand 3
86 branch_variable_zero_unequal func_80096724 5 branch iv@1 addr@3
    continue when variable 0 == operand 1, else jump to operand 3
87 set_var0 func_80096790 3 next iv@1
    variable 0 = operand 1
88 store_var0 func_800967E8 3 next var@1
    variable operand 1 = variable 0
89 branch_unless_near func_80095E48 6 branch actor@1 iv@2 addr@4
    continue when the selected actor is nearer than operand 2, else jump
8a branch_unless_on_screen func_80095C00 4 branch actor@1 addr@2
    continue while the selected actor projects inside the screen, else jump to
    operand 2; yields
8b branch_unless_item func_800962C0 5 branch iv@1 addr@3
    continue when item operand 1 is carried, else jump to operand 3
8c give_item func_8009631C 3 next iv@1
    add one of item operand 1
8d take_item func_8009640C 3 next iv@1
    remove one of item operand 1
8e branch_unless_gold func_80095F24 7 branch u32@1 addr@5
    continue when the gold is at least operand 1, else jump to operand 5
8f add_gold func_80095FB8 3 next iv@1
    add operand-1 gold (at most 9999999)
90 remove_gold func_8009601C 3 next iv@1
    remove operand-1 gold (at least 0)
91 branch_unless_in_party func_800964B0 4 branch u8@1 addr@2
    continue when character byte 1 is in the party, else jump to operand 2
92 reset_slots func_800A19B0 1 end
    clear all eight script slots and the call depth and yield
93 set_layer_sprite func_800A1364 3 next iv@1
    make the running actor the next 801e layer with the first sprite;
    operand 1 doubled is the layer's resource pair (fe 5c loads it)
94 set_play_clock func_800945D4 5 next iv@1 iv@3
    set the play clock (variable 0a) to operand 1 minutes : operand 3
    seconds and stop it (8004f318/8004f328)
95 set_play_clock_mode func_80094650 2 next u8@1
    set the play clock mode 8004f328 (bit 2 counts down, bit 7 stops)
96 stop_play_clock func_8009468C 1 next
    restart the play clock's frame count and stop it
97 set_camera_heading_mask0 func_8009A634 3 next iv@1
    set camera heading mask 0 (800af880 +174, read by 800726e8)
98 change_map_entry func_800932D0 5 wait iv@1 iv@3
    once field control allows, request map operand 1 at entry operand 3; yields
99 scripted_camera_on func_8008FB98 1 next
    switch to the scripted camera from the working one
9a scripted_camera_off func_8008FC4C 3 wait iv@1 +6?zero
    leave the scripted camera at once (operand 1 zero: skipping the next 3
    bytes) or over operand-1 frames; waits while a blend runs
9b set_camera_blend_frames func_8008FD40 5 next iv@1 iv@3
    set the scripted camera's blend frames (at least 1)
9c wait_dialogue func_8009BB0C 1 wait
    with no window of its own, store the chosen line (+81) in variable
    14 and continue; else yield, ending the slot once its speaker lets go
9d blend_camera_distance func_8009A34C 4 next iv@1 u8@3
    blend the camera distance to operand 1 over byte-3 frames
9e save_camera_view func_8009B9A0 1 wait
    once the camera is idle, save its octant, projection and elevation
9f restore_camera_view func_8009BA0C 1 wait
    once the camera is idle, blend back to the saved view over 32 frames
a0 set_camera_view func_8009BA7C 7 next iv@1 iv@3 iv@5
    set the camera octant, elevation and projection
a1 set_camera_heading_mask1 func_8009A670 3 next iv@1
    set camera heading mask 1 (800af880 +175, read by 800726e8)
a2 wait_camera_flags func_8009A58C 2 wait u8@1
    yield while a camera flag of byte 1 is set
a3 set_point_b func_80090228 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set camera point B to x, z, y operands 1, 3, 5
a4 blend_camera_elevation func_8009A490 4 next sel@1:3/80 u8@3
    blend the camera elevation to operand 1 over byte 3 & 7f frames
a5 store_camera_octant func_8009A534 3 next var@1
    store the camera octant in a variable
a6 jump_table func_80097410 3 table iv@1
    skip operand-1 three-byte entries after the instruction
a7 request_player_control func_8009F5F4 1 next
    run player control for this frame
a8 random_below func_8009D1F0 5 next var@1 iv@3
    variable operand 1 = a random number in 0..operand 3
a9 start_choice func_8009BC98 2 wait u8@1
    once the running actor's window is typed, offer lines byte 1 >> 4 ..
    byte 1 & f as a choice; yields
aa face_view_direction_table2 func_8009ACEC 2 next u8@1
    face direction byte 1 of the second table relative to the camera
ab reset_camera_points func_80090300 1 next
    reset the saved and A points to the target goal, eye and B to the eye goal
ac move_camera func_800903BC 4 next u8@1 iv@2
    move the scripted target (0, 2) or eye (1, 3) from its saved point toward
    point A/B over (0, 1) or at (2, 3) operand 2
ad store_target_goal func_80090B18 7 next var@1 var@3 var@5
    store the camera target goal's x, z, y in three variables
ae store_eye_goal func_80090B9C 7 next var@1 var@3 var@5
    store the camera eye goal's x, z, y in three variables
af scripted_heading func_80090C20 4 next u16@1 u8@3
    byte 3 zero: store the scripted heading in variable operand 1, else set it
b0 scripted_elevation func_80090CB8 4 next u16@1 u8@3
    byte 3 zero: store the scripted elevation in variable operand 1, else set it
b1 scripted_zoom func_80090D50 4 next u16@1 u8@3
    byte 3 zero: store the scripted zoom in variable operand 1, else set it
b2 wait_camera_flags_b2 func_8009A5E0 2 wait u8@1
    yield while a camera flag of byte 1 is set
b3 fade_in func_8009731C 3 next iv@1
    fade the screen back in on channel 0 over operand-1 frames
    (8007d93c, 80071e58)
b4 fade_out func_80097364 3 next iv@1
    fade the screen out on channel 0 over operand-1 frames (80071dcc)
b5 turn_camera_octant func_8009B8E4 5 wait iv@1 iv@3
    turn the camera to octant operand 1 over operand-3 frames once idle
b6 blend_camera_projection func_8009B6AC 5 next iv@1 iv@3
    blend the camera projection to operand 1 over operand-3 frames
b7 set_camera_flag_4000 func_8009ADDC 1 next
    set camera flag 0x4000
b8 clear_camera_flag_4000 func_8009AE0C 1 next
    clear camera flag 0x4000
b9 branch_unless_game_flag func_80096534 4 branch u8@1 addr@2
    continue when game flag bit byte 1 (+1d30) is set, else jump to operand 2
ba set_game_flag func_800965A8 2 next u8@1
    set game flag bit byte 1 (+1d30)
bb clear_game_flag func_800965F4 2 next u8@1
    clear game flag bit byte 1 (+1d30)
bc show_first_sprite func_800A0D3C 1 next
    give the running actor the field's first sprite and show it
bd rotate_x_add func_80094A5C 3 next iv@1
    add operand 1 to the running descriptor's x rotation
be rotate_x_sub func_80094ACC 3 next iv@1
    subtract operand 1 from the running descriptor's x rotation
bf rotate_y_add func_80094B3C 3 next iv@1
    add operand 1 to the running descriptor's y rotation
c0 rotate_y_sub func_80094BAC 3 next iv@1
    subtract operand 1 from the running descriptor's y rotation
c1 rotate_z_add func_80094C1C 3 next iv@1
    add operand 1 to the running descriptor's z rotation
c2 rotate_z_sub func_80094C8C 3 next iv@1
    subtract operand 1 from the running descriptor's z rotation
c3 yield func_800972F4 1 next
    yield and advance
c4 swing_open func_80093E30 2 wait u8@1
    turn the running descriptor by 0x20 per run (direction byte 1) for 30
    runs, then set flag 0x100000; never yields
c5 swing_close func_80093FC0 2 wait u8@1
    the reverse of c4 while flag 0x100000 is set
c6 raise_batch_limit func_800A1E9C 1 next
    raise this pass's instruction limit by 32
c7 turn_camera_step func_8009B824 3 wait iv@1
    once the camera is idle, turn it one octant over operand-1 frames
c8 turn_camera_step_c8 func_8009B884 3 wait iv@1
    as c7
c9 branch_unless_in_zone func_80095734 4 branch u8@1 addr@2
    continue when the controlled actor is inside trigger zone byte 1, else jump
ca atan_variable func_8009A824 8 next var@1 sel@3:7/40 sel@5:7/20 flags@7
    variable operand 1 = atan2(operand 3, operand 5)
cb branch_unless_in_zone_height func_800958C0 4 branch u8@1 addr@2
    as c9, also requiring the zone's height within the controlled actor
cc call_in_zone_height func_80095520 4 call u8@1 addr@2
    as 0a, also requiring the zone's height within the controlled actor
cd set_flag_800000 func_8009DA70 1 next
    set the running actor's flag 0x800000
ce clear_flag_800000 func_8009DA98 1 next
    clear the running actor's flag 0x800000
cf set_window_layout func_8009CE48 5 next u8@1 u8@2 u8@3 u8@4
    set the running actor's window left, top, columns and rows from bytes 1-4
d0 set_window_layout_operands func_8009CEE0 11 next iv@1 iv@3 iv@5 iv@7 iv@9
    set the window left, top, columns, rows and style from operands
d1 halt func_8009CF70 1 hang
    empty: never advances
d2 message func_8009C0B4 4 wait msg@1 u8@3
    open the running actor's window for message operand 1 above or below it
    (8009c5a8 mode 0, byte 3 the style); retried until it opens
d3 message_fixed func_8009C0DC 4 wait msg@1 u8@3
    as d2 in the fixed box (mode 1)
d4 message_actor func_8009C01C 5 wait actor@1 msg@2 u8@4 +6?party
    as d2 with the selected actor speaking; an empty party slot skips 6 bytes
d5 set_input_mask func_80092628 3 next u16@1
    set the field input mask (800b217a) to operand 1
d6 set_text_speed func_800925A0 3 next iv@1
    set 800b217c to operand 1 and the text speed to 8, 6 or 4 for 0-2
d7 rotate_model_x func_800946BC 3 next iv@1
    turn the running actor's model about x by angle operand 1 (state
    mode 1, +70)
d8 rotate_model_y func_80094710 3 next iv@1
    turn the running actor's model about y by angle operand 1 (mode 2)
d9 rotate_model_z func_80094764 3 next iv@1
    turn the running actor's model about z by angle operand 1 (mode 3)
da add_texture_scroll func_800921E8 17 next s16@1 s16@3 s16@5 s16@7 u16@9 s16@11 u16@13 u16@15
    add a texture scroll (80027d64) of area operands 1-7, operand-9
    bands, source operands 11/13 and band speed operand 15
db set_channel_word func_80091F84 5 next iv@1 iv@3
    set entry operand 1 of the actor's animation channel words (+118) to
    operand 3 (at most fff)
dc swap_variables func_80092044 5 next var@1 var@3
    swap variables operand 1 and operand 3
dd set_sprite_draw_mode func_80091E00 6 next sel@1:5/80 sel@3:5/40 flags@5
    set the running actor's sprite draw mode (+134 bits 5-6) to operand
    1 and its height (+ee) to operand 3
de multiply_variable func_8009D6D8 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 *= operand 3
df divide_variable func_8009D768 6 next var@1 sel@3:5/40 flags@5
    variable operand 1 /= operand 3 (0 counts as 1)
e0 set_actor_sprite_draw_mode func_80091E98 7 next actor@1 sel@2:6/80 sel@4:6/40 flags@6
    as dd for the selected actor
e1 vram_rectangle func_80091BBC 14 next sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13
    move the VRAM rectangle at operands 1/3 (size 5/7) to 9/11, or clear it
e2 branch_unless_buttons_equal func_80096150 5 branch u16@1 addr@3
    continue when the held buttons equal operand 1, else jump to operand 3
e3 branch_unless_buttons_seen_equal func_80096178 5 branch u16@1 addr@3
    continue when the buttons seen held (800afc6c) equal operand 1, else
    jump to operand 3
e4 halt_e4 func_80091AD4 1 hang
    empty: never advances
e5 set_fog func_80091944 17 next iv@1 iv@3 iv@5 iv@7 iv@9 iv@11 iv@13 iv@15
    set the fog and far colours and the fog range, then apply them
e6 set_camera_bounds func_80091A08 9 next s16@1 s16@3 s16@5 s16@7
    set the four camera bounds (the last negated)
e7 set_clear_color func_80091A78 7 next iv@1 iv@3 iv@5
    set the clear colour
e8 shake_actor func_80094158 7 wait iv@1 iv@3 iv@5
    shake the running actor's target by operand 1 along direction operand
    5 for operand-3 runs, then set flag 0x100000; never yields
e9 shake_actor_back func_800943AC 7 wait iv@1 iv@3 iv@5
    the reverse of e8 while flag 0x100000 is set
ea walk_player_ahead_ea func_80092DFC 6 wait iv@2 iv@4
    as 47 along the running descriptor's facing (angle 0)
eb point_at_angle func_800910C0 20 next sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13 var@14 var@16 var@18
    store the point at heading 7, elevation 9, distance 11 from centre 1/3/5
    in variables operands 14, 16, 18
ec point_around func_80091318 15 next u8@1 sel@2:8/80 sel@4:8/40 sel@6:8/20 flags@8 var@9 var@11 \
        var@13
    as eb around camera point byte 1
ed store_camera_point func_800915C4 8 next u8@1 var@2 var@4 var@6
    store camera point byte 1's x, z, y in three variables
ee copy_camera_point func_80091720 3 next u8@1 u8@2
    copy camera point byte 1 into camera point byte 2
ef wait_camera_move func_8008FA38 3 wait iv@1
    yield until the camera moves of operand 1 (bit 1 target, 0 eye) are done
f0 store_scripted_camera func_80090DEC 7 next var@1 var@3 var@5
    store the scripted heading, elevation and zoom in three variables
f1 fade func_8008B248 11 next iv@1 iv@3 iv@5 iv@7 iv@9
    fade channel 1 to colour operands 3, 5, 7 over operand-9 frames, blend 1
f2 shake_camera func_8008F90C 9 next iv@1 iv@3 iv@5 iv@7
    shake the camera toward amplitudes 1, 3, 5 over operand-7 frames
f3 look_from_points func_80090E70 7 next var@1 var@3 var@5
    store the heading, pitch and zoom from point A to B in three variables
f4 close_window func_8009BE9C 2 next u8@1
    byte 1 zero: release the running actor's window, else reset its layout;
    yields
f5 message_centred func_8009C12C 4 wait msg@1 u8@3
    as d2 centred (mode 3)
f6 heading_lock func_8008E8C8 2 next u8@1
    by byte 1 clear (0) or set (1) flag 0x8000, or set layer flag 0x80000 (2)
f7 draw_random_picks func_8008E85C 5 next iv@1 iv@3
    draw operand-3 (at most 32) distinct random numbers 1..operand 1
    + 1 into 800b22a0 (8008e718)
f8 set_flag_bits func_8008E59C 4 next u8@1 u16@2
    by byte 1 set (0-3) or clear (4-7) operand 2 in the low or high half of the
    actor's flags or layer flags
f9 set_link_actor func_8008DE64 2 next actor@1
    set the running actor's +75 to the selected actor
fa rotate_actor func_800947B0 5 next u8@1 actor@2 iv@3
    turn the selected actor's descriptor by operand 3 about the axis of byte 1
fb branch_unless_bit func_8008D780 5 branch bit@1 addr@3
    continue when variable bit operand 1 is set, else jump to operand 3
fc message_as_actor func_8009BF8C 5 wait actor@1 msg@2 u8@4 +6?party
    as d4, first taking the selected actor's character (+80)
fd nop func_800A2FC0 1 next
    advance one byte
ff nop func_800A2FC0 1 next
    advance one byte
"""

_EXTENDED = """
00 rerun_00 func_8008D2D8 0 rerun
    empty: the extended byte runs next as primary 00 (end the slot)
01 wander func_8009F424 1 next
    every 16th pass turn the facing by an octant at random; yields
02 branch_unless_well_on_screen func_80095B3C 4 branch actor@1 addr@2
    continue while the selected actor projects 32 pixels inside the screen,
    else jump to operand 2; yields
03 set_scale func_8008D0F4 3 next iv@1
    scale the running actor by operand 1 and rebuild its matrix
04 set_model_82 func_8008D26C 3 next iv@1
    set the running model's +82 to twice operand 1
05 branch_unless_on_layer func_80095CC4 6 branch actor@1 iv@2 addr@4
    continue when the selected actor is on layer operand 2, else jump
06 branch_unless_on_attribute func_80095D6C 6 branch actor@1 iv@2 addr@4
    continue when the selected actor's floor has attribute operand 2, else jump
07 set_layer_flag_400 func_8008D604 2 next u8@1
    clear (0) or set (1) layer flag 0x400
08 set_scale_xyz func_8008D180 7 next iv@1 iv@3 iv@5
    scale the running actor by operands 1, 3, 5
09 set_layer_flag_800 func_8008D078 3 next iv@1
    clear (operand 1 zero) or set layer flag 0x800
0a set_variable_bit func_8008D684 3 next bit@1
    set variable bit operand 1
0b clear_variable_bit func_8008D700 3 next bit@1
    clear variable bit operand 1
0c set_b21a0 func_8008CFEC 13 next u16@1 u16@3 u16@5 u16@7 u16@9 u16@11
    set the six halfwords at 800b21a0
0d set_character func_8008CF9C 3 next iv@1
    set the running actor's character (+80) from operand 1
0e music_fade func_8008C84C 5 wait iv@1 iv@3
    with a sequence playing fade the music to level operand 1 over
    operand-3 frames (8003a89c); waits while a track loads
0f music_pitch func_8008C938 6 wait sel@1:5/80 sel@3:5/40 flags@5
    as 0e shifting the music's pitch (8003a948)
10 music_tempo func_8008CA60 5 wait iv@1 iv@3
    as 0e setting the music's tempo (8003a838)
11 music_pan func_8008CB4C 6 wait sel@1:5/80 sel@3:5/40 flags@5
    as 0f setting the music's pan (8003a9bc)
12 music_mute func_8008CC74 3 wait iv@1
    as 0e muting the music channels of mask operand 1 (8003aac4)
13 set_sound_emitter func_8008CD48 5 next iv@1 iv@3
    make the running actor a sound emitter of sound operand 1 at volume
    operand 3 (mode 0)
14 set_sound_emitter_80 func_8008CDD4 5 next iv@1 iv@3
    as 13 with mode 0x80
15 set_sprite_parameter func_800A14F0 5 next iv@1 iv@3
    give the running actor field sprite operand 1 in bank operand 3 and
    enable it
16 free_boundary func_8008C7D8 1 next
    release the running actor's boundary block (+114)
17 actor_face_actor func_8009AA00 3 next actor@1 actor@2
    selected actor byte 1 faces selected actor byte 2
18 join_party_byte func_8008BDD8 2 wait u8@1 +4
    once idle add character byte 1 to the party and read its sprite; when
    it cannot join mark it waiting and skip the next 2 bytes (fe 1a)
19 leave_party func_8008C334 2 wait u8@1
    once no sprite load is pending remove character byte 1 from the party
1a apply_party_sprite func_8008B894 1 wait
    once the disc is idle unpack the pending member's sprite and run its join
1b scroll_texture func_8008B5D4 5 next s16@1 s16@3
    scroll the running actor's textured polygons by u, v operands 1 and 3
1c set_position func_80098A7C 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    place the running actor at x, z, y operands 1, 3, 5
1d set_piece_drift func_800984EC 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the piece drift vector and enable it
1e switch_party_to_gears func_8009FB98 2 next u8@1
    mark the field id 0xc000, switch the party files to the gears and
    set the alternate sprite set (800b2268) to byte 1
1f stand_in func_8009FDD4 1 next
    stand the running actor in for its party slot's member (800ad4d4)
20 stand_in_return func_8009FE4C 2 next u8@1
    return party slot byte 1 to its member (800acfd0)
21 set_party_sprite func_800A06E8 3 next iv@1
    give the running actor party member operand 1's sprite, or hide it
22 store_projection func_8009B664 3 next var@1
    store the camera projection in a variable
23 gather_party func_8009B398 20 wait sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13 iv@14 iv@16 iv@18
    walk the party to x/z 1/3, 5/7, 9/11 facing 14, 16, 18 until all arrive
24 wait_party_gathered func_8009B210 1 wait
    yield until all party members are near the leader
25 set_camera_floor_fixed func_8008D5C8 2 next u8@1
    set 800b21cd to byte 1
26 screen_distortion func_8008B2F0 15 next iv@1 iv@3 iv@5 iv@7 iv@9 iv@11 iv@13
    start the screen distortion (800a484c) moving its six values to
    operands 1-11 over operand-13 frames
27/1=00 distortion_release func_8008B328 4 next u8@1 iv@2
    move the distortion's values to 0 over operand-2 frames, then stop;
    yields
27/1=01 distortion_wait func_8008B328 2 wait u8@1
    yield while the distortion runs (800b2078)
27/1=02 distortion_clear func_8008B328 2 next u8@1
    clear 800b2078; yields
27/1=03 distortion_stop func_8008B328 2 next u8@1
    stop the distortion and free its buffers (800a47d4); yields
27/1=* distortion_rerun func_8008B328 0 rerun
    no case: the extended byte runs next as primary 27
28 store_flags0 func_8008E4EC 3 next var@1
    store the running actor's flag halfword 0 in a variable
29 store_flags1 func_8008E518 3 next var@1
    store the running actor's flag halfword 1 in a variable
2a store_flags2 func_8008E544 3 next var@1
    store the running actor's flag halfword 2 in a variable
2b store_flags3 func_8008E570 3 next var@1
    store the running actor's flag halfword 3 in a variable
2c store_actor_flags func_8008DEBC 3 next actor@1 var@1
    store the actor selected by the variable's low byte's flags in it
2d store_actor_flags1 func_8008DF44 3 next actor@1 var@1
    as 2c with flag halfword 1
2e store_actor_layer_flags func_8008DFCC 3 next actor@1 var@1
    as 2c with the layer flags
2f store_actor_flags3 func_8008E054 3 next actor@1 var@1
    as 2c with flag halfword 3
30 branch_unless_flags0 func_8008E3E8 5 branch u16@1 addr@3
    continue when flag halfword 0 shares a bit with operand 1, else jump
31 branch_unless_flags1 func_8008E414 5 branch u16@1 addr@3
    as 30 with flag halfword 1
32 branch_unless_flags2 func_8008E440 5 branch u16@1 addr@3
    as 30 with flag halfword 2
33 branch_unless_flags3 func_8008E46C 5 branch u16@1 addr@3
    as 30 with flag halfword 3
34 branch_unless_actor_flags0 func_8008E298 6 branch u16@1 actor@3 addr@4
    as 30 for the actor selected by byte 3
35 branch_unless_actor_flags1 func_8008E2EC 6 branch u16@1 actor@3 addr@4
    as 31 for the actor selected by byte 3
36 branch_unless_actor_flags2 func_8008E340 6 branch u16@1 actor@3 addr@4
    as 32 for the actor selected by byte 3
37 branch_unless_actor_flags3 func_8008E394 6 branch u16@1 actor@3 addr@4
    as 33 for the actor selected by byte 3
38 store_actor_distance func_8008E1B4 5 next var@1 actor@3 actor@4
    store the planar distance between the selected actors in a variable
39 set_b218c func_8008D230 3 next iv@1
    set 800b218c to operand 1
3a set_party_bit func_8008CED0 3 next iv@1
    set character operand 1's bit of +1d32
3b clear_party_bit func_8008CE64 3 next iv@1
    clear character operand 1's bit of +1d32
3c call_layer_script func_8008B180 5 next iv@1 iv@3
    with the 801e module loaded run script entry operand 3 of layer
    actor operand 1 (801e8330)
3d set_layer_light_row func_8008AEC8 10 next sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    set row operand 1 of the 801e layers' light matrix (800b221c)
3e set_layer_color_column func_8008AFD8 10 next sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    set column operand 1 of the 801e layers' colour matrix (800b223c)
3f set_layer_back_color func_8008B0E8 7 next iv@1 iv@3 iv@5
    set the 801e layers' back colour (800b225c)
40 set_scroll_byte func_80092148 7 next iv@1 iv@3 u16@5
    set byte operand 3 of texture-scroll entry operand 1's buffer
    (800afea8) to operand 5
41 set_stand_in_flag func_8009FC48 3 next iv@1
    set party slot operand 1's stand-in flag (+22b1) and record the
    field id in its variables (8009fd10)
42 clear_stand_in_flag func_8009FCAC 3 next iv@1
    clear party slot operand 1's stand-in flag and record the field id
43 force_party_position func_8009B15C 1 next
    force the party position
44 release_party_position func_8009B184 1 next
    release the forced position and resettle the controlled actor
45 set_idle_animation func_8009A0FC 2 next u8@1
    set the running actor's idle animation (+e6, which walks end with)
    to byte 1
46 set_layer_flag_20000 func_8008AE5C 2 next u8@1
    set (byte 1 zero) or clear layer flag 0x20000
47 set_layer_turn_step func_8008B144 3 next iv@1
    set the turn step of layer-flag-0x2000 actors (800b21b4)
48 set_orbit_angles func_8008B518 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the camera orbit angles
49 clear_link_actor func_8008DAFC 1 next
    clear the running actor's +75
4a load_actor_block func_8008ACE8 3 wait iv@1
    once the stream and disc are idle load file 77a + operand 1 as +120
4b apply_actor_block func_8008A9AC 1 wait
    once the stream stops make the +120 block the sprite's resource
4c play_animation_complement func_8008A974 2 next u8@1
    as 4d and clear layer flag 0x10000
4d set_animation_complement func_8008A93C 2 next u8@1
    set the requested animation (+ea) to ~byte 1
4e free_actor_block func_8008AA60 1 next
    release the +120 block; yields
4f clear_script_control0 func_80093BB0 1 next
    clear script control byte 0
50 set_script_control0 func_80093BD4 1 next
    set script control byte 0
51 clear_script_control1 func_80093BFC 1 next
    clear script control byte 1
52 set_script_control1 func_80093C20 1 next
    set script control byte 1
53 release_script_control func_80093AC8 1 next
    clear the encounter inhibition, both control bytes and the camera holds
54 take_script_control func_80093B10 1 wait
    set the inhibition, both control bytes and the camera holds once ready
55 open_menu0 func_80093740 1 next
    request menu kind 0 with parameter 800b236c; yields
56 open_menu1 func_80093930 3 next iv@1
    request menu kind 1 with entry operand 1; yields
57 open_menu2 func_800937E0 1 next
    request menu kind 2; yields
58 open_menu3 func_80093824 3 next iv@1
    request menu kind 3 with operand 1; yields
59 open_menu4 func_800939A0 3 next iv@1
    request menu kind 4 with operand 1; yields
5a open_menu5 func_80093A04 3 next iv@1
    request menu kind 5 with operand 1; yields
5b set_turn_step func_8008B210 3 next iv@1
    set the running actor's turn step (+11e)
5c/1=00 layer_model_off func_800A0FD8 2 wait u8@1
    deactivate the running actor's 801e layer model once idle
5c/1=01 layer_model_load func_800A0FD8 2 wait u8@1
    start loading the layer's two resources (files 6ba/6bb + 2 * pc + 5, the
    following fe 5c 02's operand) once idle
5c/1=02 layer_model_build func_800A0FD8 4 wait u8@1 iv@2
    once loaded build the layer model at the actor's position
5c/1=* layer_model_rerun func_800A0FD8 0 rerun
    no case: the extended byte runs next as primary 5c
5d play_sound_effect func_8008F6AC 7 next iv@1 iv@3 iv@5
    play sound effect operand 1 on voice pair 3 at volume operand 5,
    pan operand 3 (800855c8)
5e set_sprite_blend func_8008F2D8 3 next iv@1
    set the running actor's sprite blend rate to operand 1 & 7
5f set_colors func_8008F1C8 8 next u8@1 iv@2 iv@4 iv@6
    set the running actor's colour triples selected by byte 1 bits 0/1
60 play_movie func_8008EC30 9 wait iv@1 iv@3 iv@5 iv@7
    once sound is available request movie operand 1 (layout and fade operand 7)
61 wait_adb7c func_8008E9F8 1 wait
    yield until 800adb7c is set, then clear it
62 set_voice_volume func_8008F444 5 next iv@1 iv@3
    set voice pair operand 3's volume to operand 1 (8003a344)
63 set_voice_pan func_8008F4A0 5 next iv@1 iv@3
    set voice pair operand 3's pan to operand 1 (8003a55c)
64 wait_sound_channels func_8008F5E4 3 wait iv@1
    yield while an effect channel of mask operand 1 << 8 plays (8003a5d0)
65 play_sound_effect_pair func_8008F4FC 5 next iv@1 iv@3
    play sound effect operand 1 on voice pair operand 3 (80085634)
66 play_sound_effect_full func_8008F558 9 next iv@1 iv@3 iv@5 iv@7
    play sound effect operand 1 on voice pair operand 7 at volume
    operand 5, pan operand 3 (800855c8)
67 play_movie_window func_8008EE14 19 next iv@1 iv@3 iv@5 iv@7 iv@9 iv@11 iv@13 iv@15 iv@17
    request movie operand 1 in the window of operands 11-17
68 walk_player_to func_80092C20 6 wait sel@1:5/80 sel@3:5/40 flags@5
    once field control allows walk the controlled actor to x, z operands 1, 3
69 store_character_sum func_8008A6E0 5 next var@1 iv@3
    store character operand 3's +77 + +78 in a variable
6a set_b21d4 func_8008A604 3 next iv@1
    set 800b21d4 to operand 1
6b set_character_78 func_8008A640 5 next iv@1 iv@3
    set character operand 3's +78 to operand 1 less its +77
6c clear_pad_byte func_8008A5A0 1 next
    clear 8005938c (8003633c(0)) when the byte after the instruction is
    zero
6d copy_camera_to_scripted func_8008FB28 1 next
    copy the working heading, elevation and zoom into the scripted camera
6e set_camera_heading func_8008FABC 4 next sel@1:3/80 flags@3
    set the camera heading
6f set_sprite_angles func_8008B45C 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the sprite view rotation
70 set_piece_drift_mode func_80089F54 3 next iv@1
    set 800b21d2 to operand 1 - 0x80
71 store_facing func_8009899C 3 next var@1
    store the running actor's facing in a variable
72 store_turn_step func_800988B8 10 next var@1 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    store 80073930(operands 3, 5, 7) in a variable
73 store_planar_distance func_8009861C 12 next var@1 sel@3:11/40 sel@5:11/20 sel@7:11/10 \
        sel@9:11/08 flags@11
    store the planar distance between two x/z points in a variable
74 debug_print func_800985BC 3 next var@1
    print a variable (debug)
75 store_actor_facing func_800989F0 4 next actor@1 var@2
    store the selected actor's facing in a variable
76 store_distance func_80098738 16 next var@1 sel@3:15/40 sel@5:15/20 sel@7:15/20 sel@9:15/10 \
        sel@11:15/08 sel@13:15/08 flags@15
    store the distance between two points in a variable
77/1=00 tim_load func_8008A2E8 2 wait u8@1
    once the stream stops load TIM file 7fb + pc + 5 (the following fe 77
    01's operand 2); yields
77/1=01 tim_upload func_8008A2E8 11 wait u8@1 sel@4:10/40 sel@6:10/20 sel@8:10/10 flags@10
    upload the TIM to x, y operands 4, 6 with palette row operand 8 + e8
77/1=* tim_free func_8008A2E8 2 wait u8@1
    release the TIM
78 rerun_78 func_8008A4F0 0 rerun
    empty: the extended byte runs next as primary 78
79 rerun_79 func_8008A4E8 0 rerun
    empty: the extended byte runs next as primary 79
7a rerun_7a func_8008A4E0 0 rerun
    empty: the extended byte runs next as primary 7a
7b rerun_7b func_8008A518 0 rerun
    empty: the extended byte runs next as primary 7b
7c rerun_7c func_8008A500 0 rerun
    empty: the extended byte runs next as primary 7c
7d rerun_7d func_8008A508 0 rerun
    empty: the extended byte runs next as primary 7d
7e rerun_7e func_8008A510 0 rerun
    empty: the extended byte runs next as primary 7e
7f wait_battle_request func_8008A244 1 wait
    yield while 800adb88 is set
80 set_panorama func_80089FD0 15 next u16@1 u16@3 u16@5 u16@7 u16@9 u16@11 u16@13
    set the panorama backdrop's texture, size, CLUT, mode and turn
81 set_panorama_position func_8008A08C 8 next sel@1:7/80 sel@3:7/40 sel@5:7/20 flags@7
    set the panorama backdrop's position
82 set_panorama_colors func_8008A148 25 next iv@1 iv@3 iv@5 iv@7 iv@9 iv@11 iv@13 iv@15 iv@17 \
        iv@19 iv@21 iv@23
    set the panorama backdrop's colours and fade and enable it
83 end_field_mode func_80092FB4 3 next iv@1
    when 800adbd8 is set, clear it and end the field with kind 3 into
    game mode operand 1
84 request_battle_field func_800933F8 9 wait iv@1 iv@5 iv@7
    once field control allows request battle operand 1 as 71, also
    setting map operand 5 at entry operand 7 unless it is 7fff
85 store_movie_frame func_8008A2A0 3 next var@1
    store the current movie frame (800b06a0) in a variable
86 set_afe84 func_80089F94 2 next u8@1
    set 800afe84 to byte 1
87 wait_menus_done func_800936E4 1 wait
    yield until the requested menus have run (8004f350 zero)
88 set_emitter_path func_80089BF0 18 next sel@1:17/80 sel@3:17/40 sel@5:17/20 sel@7:17/10 \
        sel@9:17/08 sel@11:17/04 sel@13:17/02 sel@15:17/01 flags@17
    set emitter operand 1's start and end points and step count
89 place_emitter func_80089DCC 11 next sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9 actor@10
    place sound emitter operand 1 and attach it to the actor of byte 10
8a set_listener func_80089F18 3 next iv@1
    set the sound listener selector (800b22e0)
8b store_party_position func_80089B54 3 next var@1
    store the running actor's party slot (or ff) in a variable
8c slide_voice_volume func_8008F3D0 7 next iv@1 iv@3 iv@5
    slide voice pair operand 3's volume to operand 1 over operand-5
    frames (8003a450)
8d set_effects_kept func_8008F394 3 next iv@1
    set the effects kept across a reload
8e set_visibility_margins func_8008F348 5 next iv@1 iv@3
    set the screen margins 800c3a5c/800c3a60
8f begin_effect_actor func_80088790 8 next actor@1 iv@2 iv@4 iv@6
    reset the emitter templates for the selected actor and keep launch
    frame operand 2 and operands 4, 6
90 select_emitter_template func_80089004 9 next iv@1 iv@3 iv@5 iv@7
    select emitter template operand 1 with count 3, delay 5 and life 7
91 set_record_vectors func_80089174 14 next sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13
    set the current record's +0c and +14 vectors
92 set_record_08 func_80089374 14 next sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13
    set the current record's +08, +1c, +26 and +28
93 set_record_56 func_80089574 11 next iv@1 iv@3 iv@5 iv@7 iv@9
    set the current record's +56, +58, +54 and flags
94 set_record_5a func_800896D4 10 next sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    set the current record's +5a and +62 vectors
95 set_record_6a func_80089880 14 next sel@1:13/80 sel@3:13/40 sel@5:13/20 sel@7:13/10 \
        sel@9:13/08 sel@11:13/04 flags@13
    set the current record's bytes +6a..+6c and +6e..+70
96 start_effect func_80089A80 1 next
    start the effect the templates define for the running actor
97 stop_effects func_80089AE4 2 next u8@1
    stop the running actor's effects (releasing particles with byte 1)
98 set_emitter_range func_800884CC 3 next iv@1
    set the sound-emitter range
99 set_menu_parameter func_8008848C 2 next u8@1
    set 800b236c (the menu parameter) to byte 1 ^ 1
9a set_actor_colors func_8008F0B4 9 next u8@1 actor@2 iv@3 iv@5 iv@7
    set the selected actor's colour triples chosen by byte 1 bits 0/1
9b request_transition1 func_8008EF5C 3 next iv@1
    request screen transition 1 over operand-1 frames
9c request_transition2 func_8008EFA0 3 next iv@1
    request screen transition 2 over operand-1 frames
9d request_transition3 func_8008F070 3 next iv@1
    request screen transition 3 over operand-1 frames
9e set_clip func_8008EFE4 9 next iv@1 iv@3 iv@5 iv@7
    set both draw buffers' clip areas
9f set_character_2318_bit func_800883D4 4 next u8@1 iv@2
    set (byte 1 zero) or clear character operand 2's bit of +2318
a0 play_movie_sound func_8008EA58 12 wait sel@1:11/80 sel@3:11/40 sel@5:11/20 sel@7:11/10 \
        sel@9:11/08 flags@11
    while 800adbdc is set request movie operand 1 with sound bank operand
    9 (else wait)
a1 set_gear func_80088360 5 next iv@1 iv@3
    set character operand 1's gear (+a0) to operand 3
a2 wait_music_load func_8008825C 1 wait
    yield while 8004f308 is -1
a3 save_vram_column func_800881E8 2 next u8@1
    save (byte 1 zero) or restore the VRAM column at (3c0, 100)
a4 restore_gears func_80088198 1 next
    restore every gear's points and gauge
a5 set_record_24 func_80088C1C 7 next iv@1 iv@3 iv@5
    set the current record's +24, flag high byte and +76
a6 set_sprite_sequence func_800888A4 5 next iv@1 iv@3
    give the running actor's sprite a sequencer buffer from operands
    1 and 3
a7 set_sprite_sequence2 func_800889BC 9 next iv@1 iv@3 iv@5 iv@7
    as a6 with two entries from operands 1/3 and 5/7
a8 store_camera_target func_80090A10 7 next var@1 var@3 var@5
    store the camera target's x, z, y in three variables
a9 store_camera_eye func_80090A94 7 next var@1 var@3 var@5
    store the camera eye's x, z, y in three variables
aa set_camera_actor func_8008DB2C 2 next actor@1
    make the selected actor the one the camera follows
ab add_gear_points func_8008DC74 4 next sel@1:3/80 u8@3
    add operand 1 to the gear points of the party members of mask
    byte 3 & 3
ac take_gear_points func_8008DD6C 4 next sel@1:3/80 u8@3
    take operand 1 from the gear points of the party members of mask
    byte 3 & 3
ad store_party_hp func_80096B58 4 next var@1 u8@3
    store the HP of party slot byte 3 in a variable
ae set_jump_mode func_80096AF4 7 next iv@1 iv@3 iv@5
    set the jump mode, animation mode and repeat delay
af transform_vector func_8008800C 18 next sel@1:11/80 sel@3:11/40 sel@5:11/20 sel@7:11/10 \
        sel@9:11/08 flags@11 var@12 var@14 var@16
    transform vector 5/7/9 by node operand 3 of layer actor operand 1
    into the variables operands 12, 14, 16 name
b0/1=01 sound_bank_register func_8008AACC 2 wait u8@1
    once the stream and disc are idle register the bank file read
    before
b0/1=* sound_bank_load func_8008AACC 6 wait u8@1 iv@2 iv@4
    once the stream and disc are idle release slot operand 2's bank and
    read bank file operand 4
b1 build_status_panel func_80087FD4 1 next
    build the status panel (800a8ba4)
b2 set_party_hp func_80096D28 4 next u8@1 iv@2 u8@3
    set party slot byte 1's HP to operand 2 (capped) when slot byte 3 is used
b3 set_party_ep func_80096E20 4 next u8@1 iv@2 u8@3
    set party slot byte 1's EP to operand 2 (capped) when slot byte 3 is used
b4 store_party_ep func_80096C40 4 next var@1 u8@3
    store the EP of party slot byte 3 in a variable
b5 count_b2348 func_80087FA4 1 next
    count 800b2348 up (party gathering then warps)
b6 set_controlled func_80087E98 2 next actor@1
    make the selected actor the controlled one
b7 set_battle_override func_80087E5C 3 next iv@1
    set the battle-entry override
b8 set_battle_sounds func_80087DE0 4 next u8@1 iv@2
    set the battle sound value of random (byte 1 zero, 800b2355) or
    scripted (800b2356) battles
b9 store_182c func_80087B5C 9 next var@1 var@3 var@5 var@7
    store the game's four halfwords at +182c in variables
ba set_182c func_80087C34 10 next sel@1:9/80 sel@3:9/40 sel@5:9/20 sel@7:9/10 flags@9
    set the game's four halfwords at +182c
bb store_1834 func_80087D30 3 next var@1
    store the game's +1834 in a variable
bc set_1834 func_80087D80 4 next sel@1:3/80 flags@3
    set the game's +1834
bd set_record_flag func_80088B68 7 next iv@1
    set flag 80 (operand 1 = 1) or 40 (2) of the current record
be enable_movie_overlay func_80087C0C 1 next
    set 8004f300 (the file ab sequence drawn over movie frames)
bf open_menu_task func_80087848 13 wait iv@1 iv@3 iv@5 iv@7 iv@9 iv@11
    once field control allows select menu task 0 with six parameter
    bytes and end the field (kind 2)
c0 store_50622 func_80087800 3 next var@1
    store 80050622 in a variable
c1 store_member_animation func_80088508 7 next var@1 var@3 iv@5
    store party member operand 5's animation +0c and actor in variables
c2 begin_effect func_80088674 9 next iv@1 iv@3 iv@5 iv@7
    as 8f for actor operand 1 with launch frame operand 3
c3 set_layer_2000800 func_8009E014 1 next
    set the running actor's layer flags 0x2000000 and 0x800
c4 set_actor_layer_2000800 func_8009DF78 2 next actor@1
    set the selected actor's layer flags 0x2000000 and 0x800
c5 attach_to_layer_node func_80086F7C 5 next iv@1 iv@3
    attach the running actor to node operand 3 of layer actor operand
    1 (ffff detaches)
c6 join_party func_8008BC80 3 wait iv@1 +5
    once idle add character operand 1 to the party; with no free slot (or it
    already pending) mark it waiting and skip 2 more bytes
c7 store_gear func_800882B8 5 next iv@1 var@3
    store character operand 1's gear (+a0, ff for none) in a variable
c8 set_template_pairs0 func_80088CF8 18 next sel@1:17/80 sel@3:17/40 sel@5:17/20 sel@7:17/10 \
        sel@9:17/08 sel@11:17/04 sel@13:17/02 sel@15:17/01 flags@17
    set the current template's +30 pairs 0-3 (80088d38)
c9 set_template_pairs4 func_80088D18 18 next sel@1:17/80 sel@3:17/40 sel@5:17/20 sel@7:17/10 \
        sel@9:17/08 sel@11:17/04 sel@13:17/02 sel@15:17/01 flags@17
    set the current template's +30 pairs 4-7 (80088d38)
ca/1=00 layer_inactive func_800A0EE8 2 next u8@1
    clear the running actor's 801e layer's active flag; yields
ca/1=01 layer_release func_800A0EE8 2 next u8@1
    release the running actor's 801e layer; yields
ca/1=* layer_rerun func_800A0EE8 0 rerun
    no case: the extended byte runs next as primary ca
cb end_window_movie func_800A0EB0 1 next
    count 800adb84 up (ends a window movie); yields
cc wait_movie_mode func_800A0E54 1 wait
    yield until 800adb74 is zero
cd store_disc_number func_800A0DFC 3 next var@1
    store the disc number (80028530) in a variable
ce set_tween_slots func_800A0DC0 3 next iv@1
    set the 801e module's tween slot count (800b234a)
cf change_map_menu func_80093888 5 next iv@1 iv@3
    request map operand 1 at entry operand 3 through menu kind 1; yields
d0 copy_character func_8008764C 5 next iv@1 iv@3
    copy character slot and record operand 1 over operand 3
d1 set_game_flag_4000 func_8008754C 1 next
    set flag 0x4000 of the game's +22b6
d2 skip_2 func_8008752C 3 next
    advance over two unread bytes
d3 scale_pair func_80087420 17 next iv@1 iv@3 iv@5 iv@7 iv@9 iv@11 var@13 var@15
    variables 13, 15 = operand 1 * 9 / 5 and 3 * 11 / 7
d4/1=00 overlay_sprites_on func_80086FD0 2 next u8@1
    allocate the 33 overlay sprites (800aac08)
d4/1=01 place_overlay_sprite func_80086FD0 10 next u8@1 iv@2 iv@4 iv@6 iv@8
    place overlay sprite operand 2 at operands 4, 6, anchor 8 (800aae4c)
d4/1=02 overlay_sprites_off func_80086FD0 2 next u8@1
    release the overlay sprites (800aabd8)
d4/1=03 color_overlay_sprite func_80086FD0 10 next u8@1 iv@2 iv@4 iv@6 iv@8
    set overlay sprite operand 2's colour to operands 4, 6, 8 (800aadc8)
d4/1=* overlay_sprites_rerun func_80086FD0 0 rerun
    no case: the extended byte runs next as primary d4
d5 store_1844 func_80087960 5 next var@1 var@3
    store the game's +1844 and +1846 in variables
d6 store_184e func_800879D0 5 next var@1 var@3
    store the game's +184e and +1852 in variables
d7 set_184e func_80087AB8 6 next sel@1:9/80 sel@3:9/40
    set the game's +184e and +1852 (flags byte 9, past the instruction)
d8 set_depth_cue_off func_80087A40 2 next u8@1
    nonzero byte 1 skips depth-cueing the actors' colour (800b2357)
d9 set_dpad_table func_80087A7C 2 next u8@1
    pick player control's d-pad heading table by byte 1 (800b2354)
da open_menu6 func_80093790 1 next
    request menu kind 6 with parameter 1; yields
db restore_character func_80097200 3 next iv@1
    restore character operand 1's HP and EP
dc set_layer_row func_800873C4 5 next iv@1 iv@3
    set entry operand 1 of the 801e layer row table (800b225f)
dd/1=00 screen_band_save func_800871B0 6 next u8@1 iv@2 iv@4
    save the 256-wide screen band at y operand 2, height 4; yields
dd/1=01 screen_band_rows func_800871B0 6 next u8@1 iv@2 iv@4
    run 80026f44 on operand-4 rows of the band from row operand 2
dd/1=02 screen_band_free func_800871B0 2 next u8@1
    release the band's buffers; yields
dd/1=03 screen_band_none func_800871B0 2 next u8@1
    yield
dd/1=* screen_band_rerun func_800871B0 0 rerun
    no case: the extended byte runs next as primary dd
de set_record_flags func_80087148 5 next iv@1 iv@3
    or operand 3 into the flags of game record operand 1
df display_mode func_80086E1C 3 next iv@1
    640-wide display (operand 1 zero), or show (1) / hide (2) the five
    overlay sprites (800adb54)
e0 set_pause_disabled func_80086DE0 2 next u8@1
    nonzero byte 1 disables the start-button pause (800b2358)
e1 copy_gear func_80087580 5 next iv@1 iv@3
    copy gear operand 1 over gear operand 3
e2 soft_reset func_80086D4C 1 next
    restart from the entry point (80019cd0); yields
"""


@dataclass(frozen=True)
class Operand:
    kind: str
    offset: int
    flags: int | None = None  # sel: offset of the flags byte
    bit: int | None = None  # sel: the flags bit that makes it an immediate


@dataclass(frozen=True)
class Opcode:
    mnemonic: str
    handler: str
    size: int
    flow: str
    operands: tuple[Operand, ...] = ()
    effect: str = ""
    select: tuple[int, int, int | None] | None = None  # offset, mask, value
    also: tuple[tuple[int, str | None], ...] = ()  # alternative advances, followed
    inner: tuple[int, ...] = ()  # advances into the instruction, not followed
    setup: int | None = None  # bytes back to the set-up instruction it steps


def _operand(spec: str) -> Operand:
    kind, place = spec.split("@")
    if kind == "sel":
        offset, rest = place.split(":")
        flags, bit = rest.split("/")
        return Operand(kind, int(offset), int(flags), int(bit, 16))
    return Operand(kind, int(place))


def _table(text: str) -> dict[int, tuple[Opcode, ...]]:
    rows: list[list] = []
    for line in text.strip().splitlines():
        if line.startswith(" "):
            rows[-1][1].append(line.strip())
            continue
        rows.append([line.split(), []])
    table: dict[int, list[Opcode]] = {}
    for (code, mnemonic, handler, size, flow, *rest), effect in rows:
        select = None
        if "/" in code:
            code, form = code.split("/")
            offset, value = form.split("=")
            mask = 0xFF
            if "&" in offset:
                offset, masked = offset.split("&")
                mask = int(masked, 16)
            select = (int(offset), mask, None if value == "*" else int(value, 16))
        operands, also, inner, setup = [], [], [], None
        for token in rest:
            if token[0] == "+":
                advance, _, condition = token[1:].partition("?")
                also.append((int(advance), condition or None))
            elif token[0] == "~":
                inner.append(int(token[1:]))
            elif token[0] == "<":
                setup = int(token[1:])
            else:
                operands.append(_operand(token))
        if flow not in FLOWS:
            raise ValueError(f"opcode {code}: unknown flow {flow}")
        spec = Opcode(
            mnemonic,
            handler,
            int(size),
            flow,
            tuple(operands),
            " ".join(effect),
            select,
            tuple(also),
            tuple(inner),
            setup,
        )
        table.setdefault(int(code, 16), []).append(spec)
    return {code: tuple(forms) for code, forms in table.items()}


PRIMARY = _table(_PRIMARY)  # D_800AE2A0, run by 800a1ec8; fe is the prefix
EXTENDED = _table(_EXTENDED)  # D_800AE6A0, run by fe (800869b8)
PREFIX = 0xFE
PREFIX_HANDLER = "func_800869B8"


@dataclass(frozen=True)
class Instruction:
    pc: int
    opcode: int
    name: str
    size: int
    successors: tuple[int, ...]
    operands: tuple[int, ...] = ()  # raw values in table order
    extended: int | None = None  # the extended opcode after fe
    spec: Opcode | None = None
    inner: tuple[int, ...] = ()  # continuations inside its own operands
    immediate: tuple[bool | None, ...] = ()  # per operand: sel/iv immediates
    skips: tuple[int, ...] = ()  # alternative advances these operands allow

    @property
    def key(self) -> str:
        return f"{self.opcode:02x}" if self.extended is None else f"fe {self.extended:02x}"

    def text(self) -> str:
        parts = [
            _format(operand.kind, value, immediate)
            for operand, value, immediate in zip(
                self.spec.operands, self.operands, self.immediate, strict=True
            )
            if operand.kind != "flags"
        ]
        return f"{self.name} {', '.join(parts)}".rstrip()


def _pick(forms: tuple[Opcode, ...], code: bytes, base: int) -> Opcode:
    for spec in forms:
        if spec.select is None:
            return spec
        offset, mask, value = spec.select
        if value is None or region(code, base + offset, 1, "form byte")[0] & mask == value:
            return spec
    raise EventError("no form matches")  # every form list ends with a default


def _read(code: bytes, base: int, operand: Operand) -> tuple[int, bool | None]:
    raw = region(code, base + operand.offset, WIDTHS[operand.kind], f"{operand.kind} operand")
    value = int.from_bytes(raw, "little")
    if operand.kind == "sel":
        flags = region(code, base + operand.flags, 1, "selector flags")[0]
        return value, bool(flags & operand.bit)
    if operand.kind == "iv":
        return value, bool(value & 0x8000)
    return value, None


def decode_instruction(bytecode: bytes, pc: int) -> Instruction:
    """The instruction at `pc` as 800a1ec8 dispatches it."""
    if type(pc) is not int or not 0 <= pc <= 0xFFFF:
        raise EventError(f"PC outside the original u16 address space: {pc}")
    if len(bytecode) > 0x10000:
        raise EventError("bytecode exceeds the original u16 PC address space")
    opcode = region(bytecode, pc, 1, "opcode")[0]
    extended, base, prefix, forms = None, pc, 0, PRIMARY.get(opcode)
    if opcode == PREFIX:
        # 800869b8 stores the incremented u16 PC before reading the byte.
        base, prefix = (pc + 1) & 0xFFFF, 1
        extended = region(bytecode, base, 1, "extended opcode")[0]
        forms = EXTENDED.get(extended)
        if forms is None:
            raise UnknownInstruction(base, extended, namespace="extended")
    spec = _pick(forms, bytecode, base)
    key = f"{opcode:02x}" if extended is None else f"fe {extended:02x}"
    region(bytecode, base, max(spec.size, 1), f"{key} {spec.mnemonic} operands")
    values = [_read(bytecode, base, operand) for operand in spec.operands]
    operands = tuple(value for value, _ in values)
    immediate = tuple(flag for _, flag in values)
    size = prefix + spec.size
    following = (pc + size) & 0xFFFF
    targets = [v for o, v in zip(spec.operands, operands, strict=True) if o.kind == "addr"]
    successors = {
        "next": [following],
        "wait": [pc, following],
        "end": [],
        "return": [],
        "hang": [pc],
        "rerun": [base],
        "jump": targets,
        "branch": [following, *targets],
        "call": [*targets, following],
    }.get(spec.flow)
    if spec.flow == "table":  # an immediate index resolves; a variable starts the slots
        index = operands[0]
        successors = [following + 3 * (index & 0x7FFF) if index & 0x8000 else following]
    skips = tuple(
        (base + advance) & 0xFFFF
        for advance, condition in spec.also
        if _possible(condition, spec, operands, immediate)
    )
    unique = tuple(dict.fromkeys(s & 0xFFFF for s in successors))
    for successor in unique + skips:
        if successor >= len(bytecode):
            raise EventError(f"opcode at +0x{pc:04x} targets +0x{successor:04x} outside bytecode")
    inner = tuple((base + advance) & 0xFFFF for advance in spec.inner)
    return Instruction(
        pc, opcode, spec.mnemonic, size, unique, operands, extended, spec, inner, immediate, skips
    )


def _possible(condition, spec: Opcode, operands, immediate) -> bool:
    """Whether an alternative advance can happen for these operand bytes:
    `zero` needs operand iv 0 or a variable (9a), `party` a party slot
    selector (d4, fc: 8009cdb4 only yields ff for an empty slot)."""
    if condition is None:
        return True
    kinds = [operand.kind for operand in spec.operands]
    if condition == "zero":
        index = kinds.index("iv")
        return not immediate[index] or operands[index] == 0x8000
    if condition == "party":
        return operands[kinds.index("actor")] in (0xFD, 0xFE, 0xFF)
    raise ValueError(f"unknown condition {condition}")


def _format(kind: str, value: int, immediate: bool | None = None) -> str:
    if immediate is not None:
        if kind == "iv":
            return f"#{value & 0x7FFF}" if immediate else f"v{value:04x}"
        return f"#{signed16(value)}" if immediate else f"v{value:04x}"
    if kind in ("u8", "flags"):
        return f"0x{value:02x}"
    if kind == "actor":
        names = {0xFF: "party0", 0xFE: "party1", 0xFD: "party2", 0xFB: "self"}
        return names.get(value, f"actor{value}")
    if kind == "s16":
        return str(signed16(value))
    if kind == "var":
        return f"v{value:04x}"
    if kind in ("addr", "data"):
        return f"@{value:04x}"
    if kind == "bit":
        return f"v{value >> 4:04x}.{value & 15}"
    if kind == "msg":
        return f"msg{value:04x}"
    if kind == "cond":
        index = value & 15
        return CONDITION_TEXT[index] if index < len(CONDITION_TEXT) else f"cond{index}"
    if kind == "u32":
        return str(value)
    return f"0x{value:04x}"


@dataclass(frozen=True)
class Variables:
    """The original field's 1024 halfwords and per-variable unsigned type bits."""

    values: bytes
    unsigned_bits: bytes

    def __post_init__(self) -> None:
        if len(self.values) != 2048 or len(self.unsigned_bits) != 128:
            raise EventError("variable bank needs 2048 value bytes and 128 type bytes")

    def index(self, reference: int) -> int:
        if type(reference) is not int or not 0 <= reference < 2048:
            raise EventError(f"variable byte reference outside recovered bank: {reference}")
        # The original SRA discards the low bit; odd references alias the even one.
        return reference >> 1

    def unsigned(self, reference: int) -> bool:
        index = self.index(reference)
        return bool(self.unsigned_bits[index >> 3] & (1 << (index & 7)))

    def read(self, reference: int) -> int:
        offset = self.index(reference) * 2
        value = int.from_bytes(self.values[offset : offset + 2], "little")
        return value if self.unsigned(reference) else signed16(value)


def branch_operands(instruction: Instruction, variables: Variables) -> tuple[int, int]:
    """800a1bd0's two compared values (modes 00, 40, 80, c0 of byte 5)."""
    if instruction.opcode != 2:
        raise EventError("operand resolution requires a decoded conditional branch")
    left_raw, right_raw, byte, _ = instruction.operands
    mode, comparison = byte & 0xF0, byte & 0x0F
    if mode not in (0x00, 0x40, 0x80, 0xC0) or comparison >= len(COMPARISONS):
        raise EventError(f"unsupported branch mode 0x{byte:02x} at +0x{instruction.pc:04x}")
    left = signed16(left_raw) if mode & 0x80 else variables.read(left_raw)
    right = signed16(right_raw) if mode & 0x40 else variables.read(right_raw)
    if mode in (0x00, 0x40):
        right = right & 0xFFFF if variables.unsigned(left_raw) else signed16(right)
    elif mode == 0x80 and variables.unsigned(right_raw):
        left &= 0xFFFF
    return left, right


def branch_matches(comparison: int, left: int, right: int) -> bool:
    if comparison == 0:
        return left == right
    if comparison in (1, 7):
        return left != right
    if comparison == 2:
        return left > right
    if comparison == 3:
        return left < right
    if comparison == 4:
        return left >= right
    if comparison == 5:
        return left <= right
    if comparison in (6, 9):
        return bool(left & right)
    if comparison == 8:
        return bool(left | right)
    if comparison == 10:
        return bool(~left & right)
    raise EventError(f"unsupported branch comparison {comparison}")


def branch_next_pc(instruction: Instruction, variables: Variables) -> int:
    left, right = branch_operands(instruction, variables)
    matches = branch_matches(instruction.operands[2] & 0x0F, left, right)
    return instruction.successors[0 if matches else 1]


JUMP = 0x01  # the three-byte entries a variable a6 index selects


def table_slots(bytecode: bytes, first: int) -> list[int]:
    """Entries of a variable a6 table: consecutive three-byte jumps."""
    slots = []
    while first + 3 <= len(bytecode) and bytecode[first] == JUMP:
        slots.append(first)
        first += 3
    return slots


@dataclass
class Walk:
    """Every instruction reachable from a set of entry PCs."""

    instructions: dict[int, Instruction] = field(default_factory=dict)
    unknown: list[tuple[int, int]] = field(default_factory=list)  # (pc, extended byte)
    undecodable: list[tuple[int, str]] = field(default_factory=list)
    overlaps: list[tuple[int, int]] = field(default_factory=list)
    inner: list[tuple[int, int]] = field(default_factory=list)
    tables: list[tuple[int, int]] = field(default_factory=list)
    unpaired: list[int] = field(default_factory=list)
    skips: int = 0  # alternative advances over the next instruction, followed
    into: list[tuple[int, int]] = field(default_factory=list)  # (pc, target) not followed


def walk(bytecode: bytes, entries) -> Walk:
    """Follow every successor; failures keep their PC and nothing is guessed."""
    result, pending, failed = Walk(), list(entries), set()
    while pending:
        pc = pending.pop()
        if pc in result.instructions or pc in failed:
            continue
        try:
            ins = decode_instruction(bytecode, pc)
        except UnknownInstruction as error:
            failed.add(pc)
            result.unknown.append((error.pc, error.opcode))
            continue
        except (EventError, FieldError) as error:
            failed.add(pc)
            result.undecodable.append((pc, str(error)))
            continue
        result.instructions[pc] = ins
        pending.extend(ins.successors)
        result.inner += [(pc, target) for target in ins.inner]
        for target in ins.skips:
            # Followed only when it steps over the next instruction whole.
            try:
                lands = (
                    target
                    == ins.pc + ins.size + decode_instruction(bytecode, ins.pc + ins.size).size
                )
            except (EventError, FieldError):
                lands = False
            if lands:
                result.skips += 1
                pending.append(target)
            else:
                result.into.append((pc, target))
        if ins.spec.flow == "table" and not ins.operands[0] & 0x8000:
            slots = table_slots(bytecode, ins.pc + ins.size)
            result.tables.append((pc, len(slots)))
            pending.extend(slots)
    ordered = sorted(result.instructions)
    for before, after in zip(ordered, ordered[1:], strict=False):
        if after < before + result.instructions[before].size:
            result.overlaps.append((after, before))
    for pc, ins in result.instructions.items():
        spec = ins.spec
        if spec.setup is not None:
            setup = result.instructions.get(pc - spec.setup)
            if setup is None or setup.opcode != ins.opcode or setup.spec.setup is not None:
                result.unpaired.append(pc)
    return result


def disassemble_reachable(bytecode: bytes, entry: int) -> tuple[Instruction, ...]:
    """Follow every successor from `entry`. An unknown instruction fails with
    its exact PC and overlapping instructions are rejected; nothing is guessed."""
    result = walk(bytecode, [entry])
    if result.unknown:
        pc, opcode = result.unknown[0]
        raise UnknownInstruction(pc, opcode, namespace="extended")
    if result.undecodable:
        raise EventError(result.undecodable[0][1])
    if result.overlaps:
        pc, other = result.overlaps[0]
        raise EventError(f"overlapping instructions at +0x{pc:04x} and +0x{other:04x}")
    return tuple(result.instructions[pc] for pc in sorted(result.instructions))


def script_entries(package: EventPackage) -> tuple[list[tuple[int, int, int]], int]:
    """(actor, event, pc) of every event that names a script, and the number
    of zero entries skipped because the bytecode opens with 8009fa54's map
    entry records (ff)."""
    records = package.bytecode[:1] == b"\xff"
    starts, skipped = [], 0
    for actor, row in enumerate(package.entries):
        for event, pc in enumerate(row):
            if pc == 0 and records:
                skipped += 1
            else:
                starts.append((actor, event, pc))
    return starts, skipped


def linear(bytecode: bytes, start: int, end: int) -> bool:
    """True when [start, end) decodes as consecutive known instructions."""
    pc = start
    while pc < end:
        try:
            pc += decode_instruction(bytecode, pc).size
        except (EventError, FieldError):
            return False
    return pc == end


FIELD_DIRECTORY = (4, 0)  # 80078d44 selects it before 800777dc reads the map ahead
MAP_FILE = 0xB8  # 800777dc: 8001b484(map * 2), and 8001b53c reads file 0xb8 + that
EVENTS = 5  # BUNDLE_EVENTS of the header sizes (+10c) and offsets (+130), 80070cc8
HEADER = 0x154  # the header bytes 80070cc8 reads


def map_files(extract: Path, disc: Path) -> list[tuple[int, Path]]:
    """(map, path) of every map bundle: directory (4, 0) file 0xb8 + 2 * map.
    The disc index's directory entry before the first bundle holds the 1460
    files of the 730 bundle and companion pairs."""
    from tools.analysis.battle_ai import directory_table

    manifest = json.loads((extract / "manifest.json").read_text())
    slots = {entry["slot"]: entry for entry in manifest["files"]}
    group, index = FIELD_DIRECTORY
    first = MAP_FILE + directory_table(disc)[group + index] - 2  # 80028738
    count = -slots[first - 1]["size"] // 2
    return [
        (map_id, extract / "files" / f"{first + 2 * map_id:04d}.bin") for map_id in range(count)
    ]


def map_events(data: bytes) -> EventPackage | None:
    """The event component of a map bundle (None for a file too short to be one)."""
    if len(data) < HEADER:
        return None
    size = struct.unpack_from("<I", data, 0x10C + 4 * EVENTS)[0]
    offset = struct.unpack_from("<I", data, 0x130 + 4 * EVENTS)[0]
    block = decode_block(data[offset:], output_limit=size + 16)
    return event_package(block.data[:size])


@dataclass
class Totals:
    overlay: str = ""
    maps: int = 0
    placeholders: int = 0
    actors: int = 0
    scripts: int = 0
    starts: int = 0
    no_script: int = 0
    instructions: int = 0
    bytecode: int = 0
    covered: int = 0
    gaps: int = 0
    code_gaps: int = 0
    code_gap_bytes: int = 0
    uses: Counter = field(default_factory=Counter)
    conditions: Counter = field(default_factory=Counter)
    unknown: list[str] = field(default_factory=list)
    undecodable: list[str] = field(default_factory=list)
    overlaps: list[str] = field(default_factory=list)
    unpaired: list[str] = field(default_factory=list)
    inner: list[str] = field(default_factory=list)
    tables: Counter = field(default_factory=Counter)
    skips: int = 0
    into: list[str] = field(default_factory=list)
    packages: dict[int, bytes] = field(default_factory=dict)


def add_map(totals: Totals, label: str, map_id: int, package: EventPackage) -> None:
    code = package.bytecode
    starts, skipped = script_entries(package)
    result = walk(code, [pc for _, _, pc in starts])
    totals.maps += 1
    totals.packages[map_id] = bytes(package.variable_unsigned_bits) + code
    totals.actors += len(package.entries)
    totals.scripts += len(starts)
    totals.starts += len({pc for _, _, pc in starts})
    totals.no_script += skipped
    totals.instructions += len(result.instructions)
    totals.bytecode += len(code)
    covered = set()
    for pc, ins in result.instructions.items():
        totals.uses[(ins.key, ins.name)] += 1
        covered.update(range(pc, pc + ins.size))
        if ins.opcode == 2:
            totals.conditions[ins.operands[2]] += 1
    totals.covered += len(covered)
    totals.unknown += [f"{label} +0x{pc:04x}: extended {op:02x}" for pc, op in result.unknown]
    totals.undecodable += [f"{label} +0x{pc:04x}: {error}" for pc, error in result.undecodable]
    totals.overlaps += [f"{label} +0x{pc:04x} in +0x{other:04x}" for pc, other in result.overlaps]
    totals.unpaired += [f"{label} +0x{pc:04x}" for pc in result.unpaired]
    totals.inner += [f"{label} +0x{pc:04x} -> +0x{target:04x}" for pc, target in result.inner]
    totals.tables.update(slots for _, slots in result.tables)
    totals.skips += result.skips
    totals.into += [f"{label} +0x{pc:04x} -> +0x{target:04x}" for pc, target in result.into]
    start = None
    for pc in range(len(code) + 1):
        if pc < len(code) and pc not in covered:
            start = pc if start is None else start
            continue
        if start is not None:
            totals.gaps += 1
            if linear(code, start, pc):
                totals.code_gaps += 1
                totals.code_gap_bytes += pc - start
            start = None


def field_overlay(root: Path, disc: int) -> str:
    """Whether this disc's own field overlay is the image the table was read from."""
    import hashlib
    import mmap

    from tools.analysis.overlay_scripts import expected_sha256
    from tools.extraction.overlays import OVERLAYS, image

    slot, other, packed = OVERLAYS["field"]
    manifest = json.loads((root / "extract" / f"disc{disc}" / "manifest.json").read_text())
    entry = next((e for e in manifest["files"] if e["slot"] == (slot, other)[disc - 1]), None)
    if entry is None or entry["size"] <= 0:
        return "missing"
    with open(root / "discs" / f"disc{disc}.bin", "rb") as handle:
        with mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as raw:
            data = image(raw, entry, packed)
    digest = hashlib.sha256(data).hexdigest()
    return (
        f"the recovered image ({digest[:8]})" if digest == expected_sha256("field") else "differs"
    )


def sweep(root: Path) -> list[tuple[int, Totals]]:
    results = []
    for disc in (1, 2):
        extract = root / "extract" / f"disc{disc}"
        totals = Totals(overlay=field_overlay(root, disc))
        for map_id, path in map_files(extract, root / "discs" / f"disc{disc}.bin"):
            label = f"disc{disc} map {map_id}"
            try:
                package = map_events(path.read_bytes())
            except (FieldError, PackedError) as error:
                totals.undecodable.append(f"{label}: {error}")
                continue
            if package is None:
                totals.placeholders += 1
                continue
            add_map(totals, label, map_id, package)
        results.append((disc, totals))
    return results


def _listed(items: list[str], limit: int = 12) -> list[str]:
    lines = [f"    {item}" for item in items[:limit]]
    if len(items) > limit:
        lines.append(f"    ... {len(items) - limit} more")
    return lines


def report(results: list[tuple[int, Totals]]) -> list[str]:
    forms = sum(len(f) for f in PRIMARY.values()) + sum(len(f) for f in EXTENDED.values())
    out = [
        f"opcodes defined: {len(PRIMARY)} primary (D_800AE2A0, fe the prefix) and"
        f" {len(EXTENDED)} extended (D_800AE6A0), {forms} forms"
    ]
    for disc, t in results:
        used = {key for key, _ in t.uses}
        primary = sum(1 for key in used if not key.startswith("fe "))
        out += [
            f"disc {disc}: {t.maps} maps decoded, {t.placeholders} placeholder files,"
            f" {t.actors} actors; field overlay: {t.overlay}",
            f"  scripts: {t.starts} distinct starts from {t.scripts} event entries"
            f" ({t.no_script} zero entries beside map entry records name none)",
            f"  instructions: {t.instructions} reachable, {t.covered} of {t.bytecode}"
            " bytecode bytes",
            f"  opcodes used: {primary} primary, {len(used) - primary} extended",
            f"  unreached gaps: {t.gaps}, {t.code_gaps} of them ({t.code_gap_bytes} bytes)"
            " decode as instructions (not counted)",
            f"  variable jump tables (a6) by slot count: {dict(sorted(t.tables.items()))}",
            f"  alternative advances over the next instruction (followed): {t.skips}",
            f"  12 continuations inside its operands (movie pending, not followed): {len(t.inner)}",
        ]
        for name, items in (
            ("unknown opcodes", t.unknown),
            ("undecodable", t.undecodable),
            ("overlapping instructions", t.overlaps),
            ("step forms without their set-up", t.unpaired),
            ("alternative advances into an instruction (not followed)", t.into),
        ):
            out += [f"  {name}: {len(items)}", *_listed(items, 6)]
    first, second = results[0][1].packages, results[1][1].packages
    same = sum(1 for m in second if first.get(m) == second[m])
    out.append(f"maps on both discs: {len(second.keys() & first.keys())}, identical: {same}")
    uses = sum((t.uses for _, t in results), Counter())
    used = {key for key, _ in uses}
    unused_primary = [f"{c:02x}" for c in sorted(PRIMARY) if f"{c:02x}" not in used]
    unused_extended = [f"{c:02x}" for c in sorted(EXTENDED) if f"fe {c:02x}" not in used]
    out.append(
        f"primary opcodes no script uses ({len(unused_primary)}): " + " ".join(unused_primary)
    )
    out.append(
        f"extended opcodes no script uses ({len(unused_extended)}): " + " ".join(unused_extended)
    )
    out.append("per-opcode use (reachable instructions; disc 1, disc 2):")
    for key, name in sorted(uses):
        counts = "  ".join(f"{t.uses[(key, name)]:>6}" for _, t in results)
        out.append(f"  {key:<5} {name:<32} {counts}")
    conditions = sum((t.conditions for _, t in results), Counter())
    out.append(
        "02 mode/comparison bytes: "
        + " ".join(f"{b:02x}:{n}" for b, n in sorted(conditions.items()))
    )
    return out


def listing(package: EventPackage) -> list[str]:
    starts, _ = script_entries(package)
    result = walk(package.bytecode, [pc for _, _, pc in starts])
    lines = [f"; actor {a} event {e}: +0x{pc:04x}" for a, e, pc in starts]
    code = package.bytecode
    for pc in sorted(result.instructions):
        ins = result.instructions[pc]
        raw = code[pc : pc + ins.size].hex(" ")
        after = " ".join(f"{s:04x}" for s in ins.successors if s != pc + ins.size)
        lines.append(f"{pc:04x}  {raw:<30} {ins.text()}" + (f"  -> {after}" if after else ""))
    lines += [f"; unknown +0x{pc:04x}: extended {op:02x}" for pc, op in result.unknown]
    lines += [f"; undecodable +0x{pc:04x}: {error}" for pc, error in result.undecodable]
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sweep", action="store_true", help="decode every map of both discs")
    parser.add_argument("--list", type=int, metavar="MAP", help="print one map's listing")
    parser.add_argument("--disc", type=int, choices=(1, 2), default=1)
    parser.add_argument("--root", type=Path, default=ROOT / ".local", help="extract/ and discs/")
    args = parser.parse_args(argv)
    if args.sweep:
        results = sweep(args.root)
        print("\n".join(report(results)))
        return 1 if any(t.unknown or t.undecodable for _, t in results) else 0
    if args.list is None:
        parser.error("choose --sweep or --list MAP")
    extract = args.root / "extract" / f"disc{args.disc}"
    files = dict(map_files(extract, args.root / "discs" / f"disc{args.disc}.bin"))
    package = map_events(files[args.list].read_bytes())
    if package is None:
        raise SystemExit(f"disc {args.disc} map {args.list} is a placeholder file")
    print("\n".join(listing(package)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
