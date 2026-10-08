"""Enemy AI script disassembler for the battle overlay (original 800799c8).

The battle setup module (ovl2615 801e4870) points four scripts per enemy into
the enemy data file of battle n: directory (12, 1) file 2n + 2 (801e5384).
The file starts with eight halfword block offsets (enemy ids 0-7; entries
8-0x17 are zero), at +0x30 the name table offset and from +0x32 the 0x170-byte
combatant records. Each block opens with four halfword script offsets relative
to the block, the roles named here: turn (AI block +0x00, run by 800799c8),
sub (+0x04, only copied by 80078e24, never run), reaction (+0x08, 80079ab0
during a party member's attack step) and targeted (+0x0c, 80079c24 after a
party turn for each enemy it targeted). 0xffff leaves the last two unarmed.

Instructions are four bytes, opcode and b1-b3 (80079934 steps by four). The
runners stop at fd or ff. Opcodes 00-7f are actions (8007ef6c), 80 and up
conditions (8007f8c0): consecutive conditions must all hold, 99 or-s the
conditions after it up to the next action, and a false condition skips the
remaining conditions, then everything outside 80-ef (80079948), passing the
rule's closing fd. Actions write the enemy's variables and the action list at
800d2e5c that 800793f0 executes afterwards (ENTRY_TYPES).

Every table entry names the handler it was read from. Opcodes that reach a
dispatcher's default are reported as unknown: an unknown action queues an
entry of type 0x80 (8007a7bc) that 800793f0 rejects with its script error
(800792f8); an unknown condition is false. The sweep reads the user's discs
and prints aggregate results only; keep listings under .local/.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIZE = 4  # 80079934
ENDS = (0xFD, 0xFF)  # the loop tests of 800799c8, 80079ab0, 80079c24
ALWAYS = (0x80, 0x9A)  # 8007f8c0: result = 1
CHAIN = 0x99  # 8007f8c0: chained = 1
ENEMY_DIRECTORY = (12, 1)  # 801e5384: 80028470(0xc, 1)
ENEMY_IDS = 8
NAME_TABLE = 0x18  # halfword index of the name table offset (801e4870)
SCRIPT_ROLES = ("turn", "sub", "reaction", "targeted")
SECTOR, USER = 2352, 24

# Operand kinds: indices of the enemy's AI block at 800d3400 (b: u8 at +0x30,
# v: u16 at +0x20, l: s32 at +0x10), an action-list entry byte offset (off)
# or entry index (entry), immediates, attribute selectors of 80079ed8 (a8)
# and 8007a280 (a16), and the other indices named by what they select.
FORMATS = {
    "b": "b{}",
    "v": "v{}",
    "l": "l{}",
    "off": "+{}",
    "entry": "entry{}",
    "u8": "0x{:02x}",
    "u16": "0x{:04x}",
    "a8": "attr{}",
    "a16": "attr{}",
    "res": "res{}",
    "slot": "slot{}",
    "group": "group{}",
    "item": "item{}",
    "char": "char{}",
    "any": "any={}",
    "sel": "sel={}",
    "set": "set={}",
    "bit": "bit{}",
}

# opcode mnemonic handler operand@bytes...; indented lines give the effect.
# The handler is a function, or switch/case for a case without a call.
_ACTIONS = """
01 list_set 8007a828 off@1 u8@2
    action-list entry byte b1 = b2; offset 0 (the type) advances to the next entry
02 list_set_b 8007a874 off@1 b@2
    action-list entry byte b1 = byte variable b2
03 list_copy 8007a8b4 entry@1 entry@2
    copy action-list entry b1 to entry b2
04 set_b 8007a900 b@1 u8@2
    byte variable b1 = b2
05 set_v 8007a92c v@1 u16@23
    variable b1 = b2 | b3 << 8
06 set_l16 8007a968 l@1 u16@23
    long b1 = (b2 | b3 << 8) * 16
07 set_res 8007a9a8 res@1 u8@2
    resident halfword 8005a3a0[b1] = b2
08 add_b 8007a9d0 b@1 u8@2
    byte variable b1 += b2, saturating at 0xff
09 sub_b 8007aa1c b@1 u8@2
    byte variable b1 -= b2, saturating at 0
0a mul_b 8007aa60 b@1 u8@2
    byte variable b1 *= b2, saturating at 0xff
0b div_b 8007aab8 b@1 u8@2
    byte variable b1 /= b2
0c mod_b 8007aaf4 b@1 u8@2
    byte variable b1 %= b2
0d and_b 8007ab30 b@1 u8@2
    byte variable b1 &= b2
0e or_b 8007ab68 b@1 u8@2
    byte variable b1 |= b2
0f xor_b 8007aba0 b@1 u8@2
    byte variable b1 ^= b2
10 add_v 8007abd8 v@1 u16@23
    variable b1 += b2 | b3 << 8, saturating at 0xffff
11 sub_v 8007ac30 v@1 u16@23
    variable b1 -= b2 | b3 << 8, saturating at 0
12 mul_v 8007ac80 v@1 u16@23
    variable b1 *= b2 | b3 << 8, saturating at 0xffff
13 div_v 8007acdc v@1 u16@23
    variable b1 /= b2 | b3 << 8
14 mod_v 8007ad24 v@1 u16@23
    variable b1 %= b2 | b3 << 8
15 and_v 8007ad6c v@1 u16@23
    variable b1 &= b2 | b3 << 8
16 or_v 8007adb0 v@1 u16@23
    variable b1 |= b2 | b3 << 8
17 xor_v 8007adf4 v@1 u16@23
    variable b1 ^= b2 | b3 << 8
18 add_bb 8007ae38 b@1 b@2 b@3
    byte variable b3 = byte b1 + byte b2, saturating at 0xff
19 sub_bb 8007ae98 b@1 b@2 b@3
    byte variable b3 = byte b1 - byte b2, saturating at 0
1a mul_bb 8007aef0 b@1 b@2 b@3
    byte variable b3 = byte b1 * byte b2, saturating at 0xff
1b div_bb 8007af5c b@1 b@2 b@3
    byte variable b3 = byte b1 / byte b2
1c mod_bb 8007afac b@1 b@2 b@3
    byte variable b3 = byte b1 % byte b2
1d and_bb 8007affc b@1 b@2 b@3
    byte variable b3 = byte b1 & byte b2
1e or_bb 8007b040 b@1 b@2 b@3
    byte variable b3 = byte b1 | byte b2
1f xor_bb 8007b084 b@1 b@2 b@3
    byte variable b3 = byte b1 ^ byte b2
20 add_vv 8007b0c8 v@1 v@2 v@3
    variable b3 = variable b1 + variable b2, saturating at 0xffff
21 sub_vv 8007b134 v@1 v@2 v@3
    variable b3 = variable b1 - variable b2, saturating at 0
22 mul_vv 8007b198 v@1 v@2 v@3
    variable b3 = variable b1 * variable b2, saturating at 0xffff
23 div_vv 8007b208 v@1 v@2 v@3
    variable b3 = variable b1 / variable b2
24 mod_vv 8007b264 v@1 v@2 v@3
    variable b3 = variable b1 % variable b2
25 and_vv 8007b2c0 v@1 v@2 v@3
    variable b3 = variable b1 & variable b2
26 or_vv 8007b310 v@1 v@2 v@3
    variable b3 = variable b1 | variable b2
27 xor_vv 8007b360 v@1 v@2 v@3
    variable b3 = variable b1 ^ variable b2
28 set_own_attr8 8007b3b0 a8@1 u8@2
    the enemy's byte attribute b1 (80079ed8) = b2
29 set_own_attr16 8007b3e4 a16@1 u16@23
    the enemy's halfword attribute b1 (8007a280) = b2 | b3 << 8
2a get_attr8 8007b424 b@1 a8@2 v@3
    byte variable b1 = byte attribute b2 of the first slot in variable b3
2b put_attr8 8007b4b8 b@1 a8@2 v@3
    byte attribute b2 of the first slot in variable b3 = byte variable b1; for a
    party slot instead entry 0's type = 0x20 (800793f0 rejects it) and count + 1
2c get_attr16 8007b578 v@1 a16@2 v@3
    variable b1 = halfword attribute b2 of the first slot in variable b3
2d put_attr16 8007b608 v@1 a16@2 v@3
    halfword attribute b2 of the first slot in variable b3 = variable b1; a party
    slot as in 2b
2e get_long 8007b6c0 l@1 sel@2 v@3
    long b1 = record +0x104 of the first slot in variable b3 (b2 1) or the
    party's gold (b2 2); other b2 change nothing
2f put_long 8007b7b0 l@1 sel@2 v@3
    record +0x108 (b2 0) or +0x104 of the first slot in variable b3 = long b1; a
    party slot as in 2b
30 get_res 8007b8d4 v@1 res@2
    variable b1 = resident halfword 8005a3a0[b2]
31 put_res 8007b914 v@1 res@2
    resident halfword 8005a3a0[b2] = variable b1
32 copy_b 8007b958 b@1 b@2
    byte variable b2 = byte variable b1
33 copy_v 8007b98c v@1 v@2
    variable b2 = variable b1
34 copy_l 8007b9c8 l@1 l@2
    long b2 = long b1
35 b_to_v 8007ba04 b@1 v@2
    variable b2 = byte variable b1
36 v_to_l 8007ba44 v@1 l@2
    long b2 = variable b1
37 clear_b 8007ba88
    clear the 16 byte variables
38 clear_v 8007bab8
    clear the 8 variables
39 set_own_14c 8007bae8 u16@12
    the enemy's record +0x14c = b1 | b2 << 8
3a set_own_156 8007bb2c u16@12
    the enemy's record +0x156 = b1 | b2 << 8
3b set_own_155 8007bb70 u8@1 u8@2 u8@3
    the enemy's record bytes +0x155, +0x153, +0x151 = b1, b2, b3
3c set_own_154 8007bbd8 u8@1 u8@2 u8@3
    the enemy's record bytes +0x154, +0x152, +0x150 = b1, b2, b3
3d list_set16 8007bc40 off@1 u16@23
    action-list entry bytes b1, b1 + 1 = b2, b3 (no advance)
3e random_b 8007bc84 b@1 u8@2
    byte variable b1 = random 0..b2 (8001bd40)
3f random_v 8007bce8 v@1 u16@23
    variable b1 = random 0..(b2 | b3 << 8) (80089b50)
40 pick_party 8007bd5c v@1 any@2
    variable b1 = the bit of a random party slot passing 8007a628(b2) with
    800d32a1 clear; 0 if none
41 pick_party_own_group 8007bea8 v@1 any@2
    as 40, in the enemy's formation group
42 pick_enemy_own_group 8007c040 v@1 any@2
    variable b1 = the bit of a random enemy slot passing 8007a628(b2) in the
    enemy's formation group with 800d32a1 clear; 0 if none
43 pick_party_other_group 8007c1a4 v@1 any@2
    as 40, outside the enemy's formation group
44 pick_enemy_other_group 8007c33c v@1 any@2
    as 42, outside the enemy's formation group
45 party_lowest_timer 8007c4a0 v@1 any@2
    variable b1 = the bit of the party slot passing 8007a628(b2) with the
    lowest turn timer (slot 0 if none)
46 enemy_lowest_timer 8007c580 v@1 any@2
    as 45 for the other enemy slots
47 party_lowest_hp 8007c678 v@1 any@2
    variable b1 = the bit of the party slot passing 8007a628(b2) with the
    lowest HP (slot 0 if none)
48 enemy_lowest_hp 8007c75c v@1 any@2
    as 47 for the enemy slots
49 pick_party_flagged_own_group 8007c840 v@1 any@2
    variable b1 = the bit of a random party slot passing 8007a628(b2) with
    800d32a1 set, in the enemy's formation group; 0 if none
4a pick_party_flagged 8007c9d4 v@1 any@2
    as 49 in any group
4b pick_enemy_flagged 8007cb20 v@1 any@2
    variable b1 = the bit of a random enemy slot passing 8007a628(b2) with
    800d32a1 set; 0 if none
4c count_party_in_group 8007cc50 b@1 group@2
    byte variable b1 = the party slots passing 8007a628(0) in formation group b2
4d count_enemies_in_group 8007cd10 b@1 group@2
    byte variable b1 = the enemy slots passing 8007a628(0) in formation group b2
4e group_distance 8007cdd0 b@1 v@2
    byte variable b1 = the formation's distance from the enemy's group to the
    group of the first slot in variable b2
4f count_party_with 8007cea4 b@1 v@2
    byte variable b1 = the party slots passing 8007a628(0) in the group of the
    first slot in variable b2
50 count_enemies_with 8007cfb8 b@1 v@2
    as 4f for the enemy slots
51 item_count 8007d0cc b@1 item@2
    byte variable b1 = the count held of item b2 (0 if not held)
52 list_set_v 8007d148 off@1 v@2
    action-list entry bytes b1, b1 + 1 = variable b2 (no advance)
53 get_gold 8007d1a8 l@1
    long b1 = the party's gold
54 pick_character 8007d1dc v@1 char@2
    variable b1 = the bit of a random slot passing 8007a744 whose record +0x56
    is b2; 0 if none
55 get_2c8b 8007d30c l@1
    long b1 = the enemy's byte at 800d2c8b
56 pick_enemy_80 8007d344 v@1 any@2
    variable b1 = the bit of a random enemy slot passing 8007a6c8(b2) with slot
    info +3 bit 0x80; 0 if none
57 pick_party_8000 8007d478 v@1
    variable b1 = the bit of a random party slot with record +0x7c bit 0x8000
    and without 0x4002; 0 if none
58 count_party_up 8007d5b0 b@1
    byte variable b1 = the party slots without record +0x7c bits 0xc000
59 count_enemies_up 8007d610 b@1
    byte variable b1 = the present, visible enemy slots without +0x7c 0xc000
5a party_flagged_lowest_104 8007d6a8 v@1 any@2
    variable b1 = the bit of the party slot passing 8007a628(b2) with 800d32a1
    set and the lowest record +0x104 (slot 0 if none)
5b enemy_flagged_lowest_hp 8007d7b4 v@1 any@2
    variable b1 = the bit of the enemy slot passing 8007a628(b2) with 800d32a1
    set and the lowest HP (slot 0 if none)
5c pick_party_attr 8007d8c0 a16@1 v@2 v@3
    variable b2 = the bit of a random party slot passing 8007a628(0) whose
    halfword attribute b1 shares a bit with variable b3; 0 if none
5d pick_enemy_attr 8007da1c a16@1 v@2 v@3
    as 5c for the enemy slots
5e pick_party_flagged_attr 8007db78 a16@1 v@2 v@3
    as 5c, limited to party slots with 800d32a1 set
5f pick_enemy_flagged_attr 8007dcf8 a16@1 v@2 v@3
    as 5d, limited to enemy slots with 800d32a1 set
60 pick_party_attr_any 8007de78 a16@1 v@2 v@3
    as 5c with 8007a628(1)
61 pick_party_flagged_attr_any 8007dfd4 a16@1 v@2 v@3
    as 5e with 8007a628(1)
62 mark 8007ef6c/62
    nothing here; a reaction script (80079ab0) that runs it returns 1, which
    ends the attacking member's combo (80087af0)
63 set_hidden 8007e154 u8@1
    the enemy's slot info +3 = b1; with bit 0x80 it rejoins its own group
    (80087edc), otherwise it leaves its formation group (800883ac)
64 self_bit 8007e1d0 v@1
    variable b1 = the enemy's own slot bit
65 party_mask 8007e234 v@1 sel@2
    variable b1 = the party slots passing 8007a744 with 800d32a1 set (b2 1),
    clear (b2 2) or either (other b2)
66 enemy_mask 8007e334 v@1 sel@2
    as 65 for the enemy slots
67 party_group_mask 8007e438 v@1 v@2
    variable b1 = the party slots passing 8007a744 in the group of the first
    slot in variable b2
68 enemy_group_mask 8007e554 v@1 v@2
    as 67 for the enemy slots
69 reset_2c60 8007e674
    the enemy's 800d2c60 long = 0 and its 800d2c8b byte = 4
6a add_ll 8007e6a0 l@1 l@2 l@3
    long b3 = long b1 + long b2
6b sub_ll 8007e6f0 l@1 l@2 l@3
    long b3 = long b1 - long b2
6c mul_l 8007e740 l@1 u8@2
    long b1 *= b2
6d divu_l 8007e780 l@1 u8@2
    long b1 /= b2 (unsigned)
70 set_3278 8007e7c0 u8@1 u8@2
    halfword b1 of the table at *800d3278 + 0x394 = b2
71 party_flag_7a 8007e7e4 bit@1 set@2
    set (b2 != 0) or clear flag b1 + 7 (80089bec) in every party record's +0x7a
72 set_group_distance 8007e8ac group@1 group@2 u8@3
    formation group distance b1 -> b2 = b3
73 next_turn 8007e8e0 v@1
    the first slot in variable b1 takes the next turn (800d2dc0 = slot + 1)
74 reset_timers 8007e934
    reset every slot's turn timers (80078508), its order into a scratch buffer
"""

_CONDITIONS = """
80 always 8007f8c0/80
    true
81 eq_b 8007e954 b@1 u8@2
    byte variable b1 == b2
82 eq_v 8007e98c v@1 u16@23
    variable b1 == b2 | b3 << 8
83 le_b 8007e9d0 b@1 u8@2
    byte variable b1 <= b2
84 le_v 8007ea08 v@1 u16@23
    variable b1 <= b2 | b3 << 8
85 ge_b 8007ea4c b@1 u8@2
    byte variable b1 >= b2
86 ge_v 8007ea84 v@1 u16@23
    variable b1 >= b2 | b3 << 8
87 eq_bb 8007eac8 b@1 b@2
    byte variable b1 == byte variable b2
88 eq_vv 8007eb08 v@1 v@2
    variable b1 == variable b2
89 le_bb 8007eb50 b@1 b@2
    byte variable b1 <= byte variable b2
8a le_vv 8007eb90 v@1 v@2
    variable b1 <= variable b2
8b test_b 8007ebd8 b@1 u8@2
    byte variable b1 & b2 is nonzero
8c test_v 8007ec10 v@1 u16@23
    variable b1 & (b2 | b3 << 8) is nonzero
8d test_bb 8007ec54 b@1 b@2
    byte variable b1 & byte variable b2 is nonzero
8e test_vv 8007ec94 v@1 v@2
    variable b1 & variable b2 is nonzero
8f ne_b 8007ecdc b@1 u8@2
    byte variable b1 != b2
90 ne_v 8007ed14 v@1 u16@23
    variable b1 != b2 | b3 << 8
91 ne_bb 8007ed58 b@1 b@2
    byte variable b1 != byte variable b2
92 ne_vv 8007ed98 v@1 v@2
    variable b1 != variable b2
93 eq_ll 8007ede0 l@1 l@2
    long b1 == long b2
94 leu_ll 8007ee28 l@1 l@2
    long b1 <= long b2 (unsigned)
95 slot_8000 8007ee70 slot@1
    slot b1's record +0x7c bit 0x8000 is set
96 group_empty 8007eea8 group@1
    formation group b1 has no members
97 alive_low_clear 8007eed0
    the alive mask 800d39dc has neither slot 0 nor slot 1
98 alive_high_clear 8007eee8
    false when the alive mask 800d39dc has a slot above 4 and an enemy slot
    lacks slot info +3 bit 0x80; else true
99 or 8007f8c0/99
    or the conditions after it up to the next action (with none: false)
9a always 8007f8c0/9a
    true
9b own_hidden_80 8007ef44
    the enemy's slot info +3 bit 0x80 is set
"""


@dataclass(frozen=True)
class Opcode:
    mnemonic: str
    operands: tuple[tuple[str, tuple[int, ...]], ...]
    handler: str
    effect: str


def _table(text: str) -> dict[int, Opcode]:
    rows: dict[int, list] = {}
    effect: list[str] = []
    for line in text.strip().splitlines():
        if line.startswith(" "):
            effect.append(line.strip())
            continue
        code, mnemonic, handler, *operands = line.split()
        effect = []
        rows[int(code, 16)] = [mnemonic, handler, operands, effect]
    table = {}
    for code, (mnemonic, handler, operands, effect) in rows.items():
        parsed = []
        for spec in operands:
            kind, positions = spec.split("@")
            parsed.append((kind, tuple(int(p) for p in positions)))
        table[code] = Opcode(mnemonic, tuple(parsed), handler, " ".join(effect))
    return table


ACTIONS = _table(_ACTIONS)  # 8007ef6c: jump table 8006fc3c, cases 01-74
CONDITIONS = _table(_CONDITIONS)  # 8007f8c0: jump table 8006fe0c, cases 80-9b
END = Opcode("end", (), "800799c8/80079ab0/80079c24 loop test", "end of the script")
UNKNOWN_ACTION = Opcode(
    "raw",
    (("u8", (1,)), ("u8", (2,)), ("u8", (3,))),
    "8007a7bc (8007ef6c default)",
    "queue an entry of type 0x80 with the four bytes; 800793f0 rejects it (800792f8)",
)
UNKNOWN_CONDITION = Opcode(
    "raw_cond",
    (("u8", (1,)), ("u8", (2,)), ("u8", (3,))),
    "8007f8c0 (no case)",
    "false; inside an or chain the previous result stands",
)

# 800793f0: action-list entries are 8 bytes (+0 type, +1 arg1, +2 animation,
# +3 name, +4/+5 parameter, +6 targets); other types reach 800792f8.
ENTRY_TYPES = {
    0: ("end", "800793f0/0", "end of the list"),
    1: ("act", "80078998", "name text, attack step arg1 + 1, animation event on the targets"),
    2: ("approach", "80078b34", "move event 0xfd toward the target"),
    3: ("event_fc", "80078c9c", "event 0xfc"),
    4: ("event", "80078cec", "event of type +4"),
    5: ("together", "80078d48", "800d39e0 = the targets"),
    6: ("leave", "80078d6c", "event 0xf9: the actor leaves the battle"),
    7: ("split", "80078e24", "the actor becomes a copy of its first target (event 0xfb)"),
    8: ("set_attr8", "80079054", "own byte attribute arg1 = +4"),
    9: ("add_attr8", "80079098", "own byte attribute arg1 += +4"),
    10: ("set_attr16", "80079114", "own halfword attribute arg1 = +4 | +5 << 8"),
    11: ("add_attr16", "8007916c", "own halfword attribute arg1 += +4 | +5 << 8"),
    12: ("name", "80078658", "show name text +3 (event 0xfa)"),
    13: ("named_f4", "800791fc", "name text, then event 0xf4"),
    14: ("event_f7", "800787e0", "event 0xf7 with parameter +4"),
    15: ("event_f6", "80079270", "event 0xf6 with the targets"),
    16: ("message_f8", "8007887c", "event 0xf8 with the pending message"),
}


def lookup(opcode: int) -> tuple[str, Opcode]:
    """Class and table entry of an opcode as the runners dispatch it."""
    if opcode in ENDS:
        return "end", END
    if opcode < 0x80:
        if opcode in ACTIONS:
            return "action", ACTIONS[opcode]
        return "unknown-action", UNKNOWN_ACTION
    if opcode in CONDITIONS:
        return "condition", CONDITIONS[opcode]
    return "unknown-condition", UNKNOWN_CONDITION


@dataclass(frozen=True)
class Instruction:
    pc: int
    raw: bytes
    kind: str
    op: Opcode

    @property
    def opcode(self) -> int:
        return self.raw[0]

    def operands(self) -> list[str]:
        out = []
        for kind, positions in self.op.operands:
            value = sum(self.raw[p] << (8 * i) for i, p in enumerate(positions))
            out.append(FORMATS[kind].format(value))
        return out

    def text(self) -> str:
        return f"{self.op.mnemonic} {', '.join(self.operands())}".rstrip()


def decode(data: bytes, pc: int) -> Instruction:
    raw = bytes(data[pc : pc + SIZE])
    if len(raw) != SIZE:
        raise ValueError(f"Truncated AI instruction at 0x{pc:x}")
    kind, op = lookup(raw[0])
    return Instruction(pc, raw, kind, op)


def condition_end(data: bytes, pc: int, limit: int) -> int:
    """8007f8c0: the pc after one condition, or after a 99 chain."""
    chained = False
    while True:
        chained = chained or data[pc] == CHAIN
        pc += SIZE
        if not chained or pc + SIZE > limit or data[pc] < 0x80:
            return pc


def skip_rule(data: bytes, pc: int) -> int:
    """80079948: skip conditions, then everything outside 80-ef."""
    while pc + SIZE <= len(data) and data[pc] >= 0x80:
        pc += SIZE
    while pc + SIZE <= len(data) and (data[pc] - 0x80) & 0xFF >= 0x70:
        pc += SIZE
    return pc


@dataclass
class Script:
    entry: int
    end: int
    roles: list[str]
    instructions: list[Instruction]
    reachable: set[int] = field(default_factory=set)
    # Statement-level condition pc -> its false target (None: always true).
    branches: dict[int, int | None] = field(default_factory=dict)
    chained: set[int] = field(default_factory=set)
    # False targets outside [entry, end): the runner continues there.
    escapes: list[tuple[int, int]] = field(default_factory=list)
    # pcs whose next instruction lies past the end without an fd/ff.
    runs_off: list[int] = field(default_factory=list)
    # fd/ff consumed as conditions inside a 99 chain.
    ends_in_chain: list[int] = field(default_factory=list)


def analyse_script(data: bytes, entry: int, end: int, roles=()) -> Script:
    """Decode [entry, end) word by word and follow the runner from entry."""
    if not 0 <= entry < end <= len(data) or (end - entry) % SIZE:
        raise ValueError(f"AI script 0x{entry:x}-0x{end:x} is not whole instructions")
    script = Script(entry, end, list(roles), [decode(data, pc) for pc in range(entry, end, SIZE)])
    work, seen = [entry], set()

    def follow(source: int, pc: int) -> None:
        if pc < end:
            work.append(pc)
        else:
            script.runs_off.append(source)

    while work:
        pc = work.pop()
        if pc in seen:
            continue
        seen.add(pc)
        script.reachable.add(pc)
        opcode = data[pc]
        if opcode in ENDS:
            continue
        if opcode < 0x80:
            follow(pc, pc + SIZE)
            continue
        after = condition_end(data, pc, end)
        for inner in range(pc + SIZE, after, SIZE):
            script.reachable.add(inner)
            script.chained.add(inner)
            if data[inner] in ENDS:
                script.ends_in_chain.append(inner)
        follow(pc, after)
        if any(data[p] in ALWAYS for p in range(pc, after, SIZE)):
            script.branches[pc] = None
            continue
        target = skip_rule(data, after)
        script.branches[pc] = target
        if entry <= target < end:
            work.append(target)
        else:
            script.escapes.append((pc, target))
    return script


@dataclass
class Block:
    enemy: int
    offset: int
    end: int
    table: tuple[int, ...]
    scripts: list[Script]
    malformed: str | None = None


def enemy_blocks(data: bytes) -> list[Block]:
    """801e4870: each enemy id's script table and its scripts."""
    if len(data) < 0x32:
        raise ValueError("Enemy data file shorter than its header")
    header = struct.unpack_from("<25H", data, 0)
    if any(header[ENEMY_IDS:NAME_TABLE]):
        raise ValueError("Enemy data header names ids beyond 7")
    bounds = sorted({*header[:ENEMY_IDS], header[NAME_TABLE], len(data)})
    blocks = []
    for enemy, offset in enumerate(header[:ENEMY_IDS]):
        end = next(b for b in bounds if b > offset)
        if end - offset < 8:
            raise ValueError(f"Enemy id {enemy} block at 0x{offset:x} is shorter than its table")
        table = struct.unpack_from("<4H", data, offset)
        entries: dict[int, list[str]] = {}
        for role, value in zip(SCRIPT_ROLES, table, strict=True):
            if value != 0xFFFF or role == "turn":
                entries.setdefault(value, []).append(role)
        block = Block(enemy, offset, end, table, [])
        bad = sorted(v for v in entries if not 8 <= v < end - offset)
        if bad:
            listed = ", ".join(f"0x{v:x}" for v in bad)
            block.malformed = f"script offsets {listed} outside 8..0x{end - offset:x}"
        else:
            starts = sorted(entries) + [end - offset]
            for start, stop in zip(starts, starts[1:], strict=False):
                block.scripts.append(
                    analyse_script(data, offset + start, offset + stop, entries[start])
                )
        blocks.append(block)
    return blocks


def directory_table(disc: Path) -> tuple[int, ...]:
    """80028230: the 0x7a-byte directory table the resident reads from sector 40."""
    with disc.open("rb") as raw:
        raw.seek(40 * SECTOR + USER)
        data = raw.read(0x7A)
    if len(data) != 0x7A:
        raise ValueError(f"{disc}: no directory table at sector 40")
    return struct.unpack("<61H", data)


def enemy_files(extract: Path, disc: Path) -> list[tuple[int, Path]]:
    """(battle n, path) of every enemy data file: directory (12, 1) file 2n + 2."""
    manifest = json.loads((extract / "manifest.json").read_text())
    slots = {entry["slot"]: entry for entry in manifest["files"]}
    group, index = ENEMY_DIRECTORY
    directory = directory_table(disc)[group + index] - 1  # 80028470
    count = -slots[directory]["size"]  # a directory entry's file count (80028928)
    files = []
    for n in range(count // 2):
        slot = 2 * n + 2 + directory - 1  # 80028738
        path = extract / "files" / f"{slot:04d}.bin"
        if slots.get(slot, {}).get("size", 0) <= 0 or not path.is_file():
            raise ValueError(f"{extract}: battle {n} enemy file (slot {slot}) is missing")
        files.append((n, path))
    return files


@dataclass
class Totals:
    files: int = 0
    blocks: int = 0
    tables: int = 0
    malformed: list[str] = field(default_factory=list)
    malformed_images: Counter = field(default_factory=Counter)
    scripts: Counter = field(default_factory=Counter)
    instructions: int = 0
    reachable: int = 0
    words: Counter = field(default_factory=Counter)
    executed: Counter = field(default_factory=Counter)
    entry_types: Counter = field(default_factory=Counter)
    dynamic_types: list[str] = field(default_factory=list)
    unknown: list[str] = field(default_factory=list)
    undecodable: list[str] = field(default_factory=list)
    escapes: list[str] = field(default_factory=list)
    runs_off: list[str] = field(default_factory=list)
    ends_in_chain: list[str] = field(default_factory=list)
    contents: list[bytes] = field(default_factory=list)


def add_file(totals: Totals, label: str, data: bytes) -> None:
    totals.files += 1
    totals.contents.append(data)
    try:
        blocks = enemy_blocks(data)
    except ValueError as error:
        totals.undecodable.append(f"{label}: {error}")
        return
    for block in blocks:
        totals.blocks += 1
        where = f"{label} id {block.enemy} @0x{block.offset:x}"
        if block.malformed:
            totals.malformed.append(f"{where}: {block.malformed}")
            totals.malformed_images[data[block.offset : block.end]] += 1
            continue
        totals.tables += 1
        for script in block.scripts:
            totals.scripts.update(script.roles)
            at = f"{where} {'/'.join(script.roles)}"
            for ins in script.instructions:
                live = ins.pc in script.reachable
                totals.instructions += 1
                totals.words[ins.opcode] += 1
                totals.reachable += live
                totals.executed[ins.opcode] += live
                if ins.kind.startswith("unknown"):
                    state = "" if live else " (unreachable)"
                    totals.unknown.append(f"{at} +0x{ins.pc:x}: {ins.raw.hex(' ')}{state}")
                if live and ins.raw[1] == 0 and ins.opcode in (0x01, 0x3D):
                    totals.entry_types[ins.raw[2]] += 1
                elif live and ins.raw[1] == 0 and ins.opcode in (0x02, 0x52):
                    totals.dynamic_types.append(f"{at} +0x{ins.pc:x}: {ins.text()}")
            totals.escapes += [f"{at} +0x{pc:x} -> 0x{to:x}" for pc, to in script.escapes]
            totals.runs_off += [f"{at} +0x{pc:x}" for pc in script.runs_off]
            totals.ends_in_chain += [f"{at} +0x{pc:x}" for pc in script.ends_in_chain]


def sweep(root: Path) -> list[tuple[int, Totals]]:
    results = []
    for disc in (1, 2):
        extract = root / "extract" / f"disc{disc}"
        totals = Totals()
        for n, path in enemy_files(extract, root / "discs" / f"disc{disc}.bin"):
            add_file(totals, f"disc{disc} battle {n}", path.read_bytes())
        results.append((disc, totals))
    return results


def _listed(items: list[str], limit: int) -> list[str]:
    lines = [f"    {item}" for item in items[:limit]]
    if len(items) > limit:
        lines.append(f"    ... {len(items) - limit} more")
    return lines


def report(results: list[tuple[int, Totals]]) -> list[str]:
    out = []
    for disc, t in results:
        roles = ", ".join(f"{role} {t.scripts[role]}" for role in SCRIPT_ROLES)
        images = len(t.malformed_images)
        out += [
            f"disc {disc}: {t.files} enemy files, {t.blocks} enemy id blocks, "
            f"{t.tables} script tables",
            f"  scripts decoded: {sum(t.scripts.values())} ({roles})",
            f"  instructions: {t.instructions} words, {t.reachable} reachable from the entries",
            f"  blocks without a script table: {len(t.malformed)} ({images} distinct images)",
            *_listed(t.malformed, 3),
        ]
        for name, items in (
            ("undecodable", t.undecodable),
            ("unknown opcodes", t.unknown),
            ("false branches leaving their script", t.escapes),
            ("flow past the script end without fd/ff", t.runs_off),
            ("fd/ff inside an or chain", t.ends_in_chain),
            ("entry types taken from variables", t.dynamic_types),
        ):
            out += [f"  {name}: {len(items)}", *_listed(items, 12)]
    same = all(t.contents == results[0][1].contents for _, t in results)
    out.append(f"enemy files identical on both discs: {'yes' if same else 'no'}")
    known = ("action", "condition", "end")
    words = sum((t.words for _, t in results), Counter())
    executed = sum((t.executed for _, t in results), Counter())
    used = [op for op in words if lookup(op)[0] in known]
    live = [op for op in executed if executed[op] and lookup(op)[0] in known]
    out.append(
        f"opcodes defined: {len(ACTIONS)} actions, {len(CONDITIONS)} conditions, "
        f"{len(ENDS)} ends; used: {len(used)} in script words, {len(live)} reachable"
    )
    columns = "  ".join(f"disc{disc} words/reachable" for disc, _ in results)
    out.append(f"per-opcode use ({columns}):")
    for op in sorted(words):
        kind, entry = lookup(op)
        counts = "  ".join(f"{t.words[op]:>6} {t.executed[op]:>6}" for _, t in results)
        flag = "" if kind in known else "  UNKNOWN"
        out.append(f"  {op:02x} {entry.mnemonic:<28} {counts}  {entry.handler}{flag}")
    types = sum((t.entry_types for _, t in results), Counter())
    out.append("action-list entry types written as constants (reachable, both discs; 800793f0):")
    for value in sorted(types):
        name, handler, _ = ENTRY_TYPES.get(value, ("UNKNOWN", "800792f8", ""))
        out.append(f"  {value:02x} {name:<12} {types[value]:>6}  {handler}")
    return out


def disassemble(data: bytes) -> list[str]:
    """A listing of one enemy data file; '-' marks words no entry reaches."""
    lines = []
    for block in enemy_blocks(data):
        table = " ".join(f"{v:04x}" for v in block.table)
        lines.append(
            f"; enemy id {block.enemy}: block 0x{block.offset:x}-0x{block.end:x}, table {table}"
        )
        if block.malformed:
            lines.append(f";   no script table: {block.malformed}")
            continue
        for script in block.scripts:
            roles = "/".join(script.roles)
            lines.append(f"; {roles} script 0x{script.entry:x}-0x{script.end:x}")
            for ins in script.instructions:
                mark = " " if ins.pc in script.reachable else "-"
                note = ""
                if ins.pc in script.chained:
                    note = "  ; or"
                elif script.branches.get(ins.pc) is not None:
                    note = f"  ; else 0x{script.branches[ins.pc]:x}"
                lines.append(f"{mark}{ins.pc:05x}  {ins.raw.hex(' ')}  {ins.text()}{note}")
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("file", nargs="?", type=Path, help="list one enemy data file")
    parser.add_argument(
        "--sweep", action="store_true", help="decode every enemy script, both discs"
    )
    parser.add_argument("--root", type=Path, default=ROOT / ".local", help="extract/ and discs/")
    args = parser.parse_args(argv)
    if args.sweep:
        results = sweep(args.root)
        print("\n".join(report(results)))
        return 1 if any(t.unknown or t.undecodable for _, t in results) else 0
    if args.file is None:
        parser.error("give an enemy data file or --sweep")
    print("\n".join(disassemble(args.file.read_bytes())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
