# Data-indexed dispatch tables

These tables have no program counter: one selector field of a fixed-layout record
picks the entry, and no code checks the index against the table's length.
`tools/analysis/dispatch_tables.py` reads the tables (entries, lengths, row values)
from the recovered C at run time and checks every record the game's loaders hand
to them on both discs:

```sh
python3 -m tools.analysis.dispatch_tables --sweep [--only formulas|primitives|modes|kinds|tmd]
```

It prints aggregates only and exits nonzero when a selected record would index
past its table. `tests/test_dispatch_tables.py` ties the tables, offsets and
loader constants to `decomp/src` and walks synthetic records. Every interpreter
and table, and how each table's index is chosen, is listed in
[interpreters.md](interpreters.md).

## Battle formulas

- Dispatch (`decomp/src/battle/battle_8008CCCC.c`): for each target in the mask,
  `func_800941A4` calls `D_800C348C[descriptor->formula]()` (8 handlers). An
  attacker whose record has +0x15a bit 0x80 (in a gear), or a descriptor with
  flagsA bit 0x10, goes to `func_8009C198`, which takes a party member's gear
  descriptor (an enemy keeps its own) and calls `D_800C34DC[formula]()` (11
  handlers). Each handler's comment names its slot.
- Data: the 0x28-byte descriptors (`CommandDescriptor`, formula at +0x16) of the
  battle setup archive, directory (12, 0) file 3, an offset table of packed
  blocks that ovl2615 `func_801E5384` unpacks: archive[5 + character] (copies
  0x5f0 bytes, 38 descriptors), archive[0x11 + gear] (0x690, 42) and archive[4]
  for the enemies (0x1f40, 200). An enemy's command is the arg1 byte of its AI's
  type-1 action-list entries (`func_80078998` -> `func_80085CCC`); the census reads
  those from the enemy data files with `tools.analysis.battle_ai`, with the
  enemy's in-gear bit from its record (+0x15a, copied by `func_801E4870`).
- Results (both discs, identical archives):
  - Party: 11 sets, 418 descriptors, formulas 0 1 2 5 6; none past
    `D_800C348C`.
  - Gears: 19 sets of 41 descriptors (the copy's 42nd comes from past the block).
    Formulas 0-3, 5, 9 and 10, and three records past `D_800C34DC`: gear 17 #31
    (formula 82) and gear 18 #34 (86) and #35 (115).
  - Enemies: 98 descriptors (the copy's other 102 come from past the block),
    formulas 0-9. The AI scripts close 1336 act entries, all with a constant
    command: on-foot enemies select 42 commands (formulas 0-5, 7), gear enemies
    41 (formulas 0-2, 4, 6-9); none past its table. Commands 31 (formula 9) and
    85 (8) need the gear table and only gear enemies select them; no act entry
    selects 27 descriptors, among them #91-#93 (formula 9).
  - No descriptor has flagsA bit 0x10. All 8 and all 11 handlers are used.
  - The enemy census follows the AI runner's paths (`battle_ai.analyse_script`: a
    condition goes on when it holds and skips its rule when it does not; a rule
    that runs ends at its `fd`), carrying the commands that can reach each act
    entry. Every one of the 1336 entries is reached with a single constant
    command, the same as in address order.
- Gear techniques. A gear's technique slot k is its descriptor 21 + k (battle.c
  `func_8008B224` offers it, `func_8008ADD0` commits it), offered only while bit
  `0x8000 >> k` of the pilot's `CharacterBattleData.mask6` is set. The three
  records past `D_800C34DC` are slots 10 (gear 17) and 13 and 14 (gear 18). The
  census derives each gear's pilots and each character's possible mask6:
  - pilots: the new-game state (directory 0x10 file 3, `+0xa0` of each 0xa4-byte
    record from +0x26c), field ext `a1` (`set_gear`; Bart is given gear 18 in maps
    198 and 728), ext `d0` (a character's record copied over another's), and
    character 10 put in gear 17 for party slots 1 and 2 when the formation has
    flag 0x10 (ovl2615 `func_801E5384`). The world map's new-world setup gives
    gears 2-9 and 15 only. Gear 17's pilot is character 10; gear 18's are
    characters 3 and 10.
  - masks: the new-game state (+0x16c0 + 0x20 per character, +6), battle results'
    learning (ovl2596 `func_801E3F28`: slot k for each of the character's growth
    `unlocksB` entries, k < 13; the growth table is item 0 of directory (0x10, 2)
    file 2), and ext `d0`'s copies. Only the debug battle selector (ovl2606
    `func_8009B1E4`) writes them otherwise, and it gives characters 3 and 10
    0xff00. Character 3 can reach 0xfc00 (slots 0-5), character 10 0xff00 (slots
    0-7, also through character 6's copy).
  - So slots 10, 13 and 14 are never offered to a pilot of gears 17 and 18, and
    the three records are never dispatched (`never offered` in the census).

## Model primitives

- Dispatch (`decomp/src/resident/main_8002C3E8.c`): `func_8002C8CC` builds and
  `func_8002C700` draws a model's primitive groups, `{u8 type, u8, s16 count}`
  and count records each, through `D_8004FE50[type]` (17 types): the prepare
  routine, the record stride, the auxiliary bytes per record, the packet size and
  six draw routines by sort mode. The textured types (odd, not 16) first consume
  texture page (c4) and CLUT (c8) words of the auxiliary data (`func_8002CD64`).
  The table's comments name each type's packet.
- Data: every model of each group a loader relocates (`func_8002C3E8`; a
  hierarchy may build only some): field map geometry (component 2, `func_80070CC8`),
  ovl2143 actor files (4, 0) 0x6bb + 2k, battle object model files (stages
  (12, 3) 6 + 2s, enemy set model entries, object sets (0x28, 0) 2s + 1, gears and
  their part files by `D_800C3508`), arena models (0x30, 1) id + 2 and the menu
  overlay's `D_80091FB0`, world map area files (0x24, 0) by `D_8009B584`, and the
  models sprite commands f5-f7 bind in the blocks `tools.analysis.sprite_vm`
  finds.
- Results: disc 1 has 16252 field, 1786 actor, 3404 battle, 1067 menu, 562 world
  map and 19 sprite-bound models; disc 2 the same but 4262 field and 16 sprite
  models. No group type is past the table and no count is negative; every
  model's packet size (+0x34) and primitive count (+4) agree with the table's
  packet sizes. 13 of 17 types are used; 6, 10, 14 and 15 are not. Disc 1 holds
  41937 c4 and 59039 c8 words (disc 2: 16412 and 32383); no other cx command byte
  starts a textured record.

## World map arrival modes

- Dispatch (`decomp/src/worldmap/worldmap.c`): the world map's entry runs mode
  `D_8006F954[0] & 0x7fff` of `D_8009A058` (19 rows of enter, start and leave
  handlers) without a bound check. The word is the game data's +0x2320.
- Data: field `56` (`change_map`, `func_80093014`) stores operand 7 there as the
  field leaves for the world map. Battle event opcode 26 (ovl3087
  `func_801E7770`) stores operand d with scene a; the world map runs after the
  battle only when the scene & 0x7ff is 0x400 or more (ovl2596 `func_801E252C`),
  otherwise d is a field's entry. The world map itself stores 1 for a new world
  state and keeps the word across its own battles (bit 0x8000 marks the return);
  its exits store a field's entry.
- Results (both discs): field `56` selects modes 1, 3-5, 8-10 and 12-18 on Disc 1
  (Disc 2: 1, 3-5, 8, 12, 17, 18); the one opcode 26 names field 3. One operand is
  a variable (debug map 723). None is past the table; modes 0, 2, 6, 7 and 11 are
  not selected by data.
- The census also lists the fields the world map's exits name: the scene of every
  path region with a link (kind 4 regions only record a destination) in the area
  files of `D_8009B584`, and the scenes its scripted modes store: 60 fields on
  both discs, among them field 0 (regions with scene 0 in the scripted modes' area
  files). Path table 3 of area file 198 points outside the file (the game
  relocates it anyway); worldmap.c reads table 3 only on a player's exit from a
  kind-3 region.

## Sprite kinds

- Dispatch (`decomp/src/resident/sprite_800248D4.c`): a new sprite task's
  auxiliary node takes the update `D_8004FD40[kind]` (16 entries,
  `func_80025224`); entries 3, 4 and 10-13 are NULL, which the task loop skips
  (`func_8001C964`).
- Data: the kind is bits 8-10 of the animation header's flags plus 8 for bit 14
  (`func_80023440`). Effect sprites take it from a directory animation
  (`func_80023FD8`), children from the header a command spawns (`func_80023B84`;
  kind 3 takes the parent's). `func_80024730` turns camera markers 12 and 13 into
  10 and 11.
- Results: the field is four bits, so no kind is past the table. Disc 1's 922
  distinct sprite blocks hold directory headers of kinds 0, 5, 6 and 15 and spawn
  kinds 0, 2-6, 9-15 (Disc 2: 803 blocks, the same kinds).

## TMD primitives

- Dispatch (`decomp/src/battle/battle_800B15D8.c`): `func_800B1720` builds and
  `func_800B1F6C` draws each primitive of a TMD object (`objects.h`'s effect
  script file) by kind: mode & 0x1c, plus 0x100 when flag bit 0 (no lighting) is
  clear. Both switches, and ovl3384 `func_801FC4C4`'s, have all 16 kinds, and a
  switch is bounds-checked, so the census checks what each kind needs instead: it
  reads from the C each kind's packet (the POLY type the drawer's mode switch
  writes) and the primitive bytes the two functions read.
- Data: object 0 of the resident `D_8001C76C` (the slot-highlight ring) and of
  every model a battle sprite command `f3` binds as its parts (which ovl3384 can
  break into pieces).
- Results (both discs, identical): 497 models (the resident one and 496 bound by
  809 `f3` commands in distinct blocks), all id 0x41, flags 0, one object. 11 of
  16 kinds are used (0x14, 0x1c, 0x110, 0x118 and 0x11c are not); every mode byte
  is a polygon code (0x20-0x3b), every olen sizes its kind's packet and every
  ilen covers the bytes read.

## Sound operand tables

`python3 -m tools.analysis.sound_sequence --sweep` counts the sound VM's two
operand-indexed tables (see [sound-sequence.md](sound-sequence.md)): the
modulator waves `D_800508A4[mode & 0xF]` (16 slots, always in range) and F0's
modulator index (`modulator[4]`, indices 0-3 used, none past the array).
