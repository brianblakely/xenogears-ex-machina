# Battle effect-script VM

Battle objects (the stage model, stage object sets, gears and their part
objects, 3D enemies and their copies) run effect scripts.

- Interpreter: battle `battle_run_effect_script` (`decomp/src/battle/battle_scene.c`,
  matching; switch on the original's jump table at 8007056c, 118 cases 00-75). Each case's operands
  and effect are listed in `decomp/src/battle/effect_vm.h`.
- Sibling: ovl2143 `gear_model_run_effect_script` (`decomp/src/ovl2143/gear_model_scene.c`, matching;
  113 cases 00-70) runs the same format for the field and menu gear models.
  Its cases 04-07, 09, 0F, 12, 1B, 1C, 2C, 2D, 2F, 3A, 3E, 3F, 51-53, 58-5A,
  60, 61 and 65-6A do nothing and take no parameter words. Cases 33, 34 and 3B
  consume their word without jumping. 71-75 are unknown there.
- Command: an s16 word (opcode in the low byte, argument in the high byte),
  then the opcode's u16 parameter words. Jump offsets are signed bytes from the
  command's start. 31 loops back past the 30 command it names. An unknown
  opcode stops the script on itself. The files put a word 7777 after each
  script, and no handler reads it.
- Animation events: 63 gives the object animation the `arg` events after its
  operand and jumps past them; an animation gives its own list (count at 0x12,
  offset at 0x14, 800AE1BC). Each frame 800AE2A4 (ovl2143: 801E5D44) runs the
  events whose s16 time is the frame. A type byte follows the time: 1 sprite
  (0x14 bytes), 2 light (0x12), 3/4 colour fade (0x1C), 5 sound (8), 6 menu
  update (4), 7 draw a part (6), 8 slot scripts (0xA), 9 image animation
  (0x1C); 2, 3, 4 and 9 take 6 bytes when byte 4 is 0. A type without a case
  is never stepped over. ovl2143 steps over 1, 5 and 6 without effect.
- Script tables are relocatable blocks (8003342C). An object script file holds
  a count, its script table and its data block; the script table's first
  offset is the animation table, the others are scripts (0 marks an empty
  id). Script ids from 0x50 and animation ids from 0x40 are the extra file's.
  Unused animation ids repeat an entry that points at a script or the data
  block. A stage scene's motion block holds the stage motions first, and the
  stage model gets no animation table (801E7210).
- Per-opcode semantics are in the module's `BATTLE`, `MODEL_VIEWER`,
  `BATTLE_EVENTS` and `MODEL_VIEWER_EVENTS` tables, each entry naming its
  case. The C comments are in `effect_vm.h` and on the cases of 801E39F0,
  800AE2A4 and 801E5D44.

Where the scripts are. `directory` is group + index of 80028470. A file's
disc slot is file + table[directory] - 2, where the table is the boot
executable's copy of sector 40 (80018004).

| family | directory | files | loader |
|---|---|---|---|
| enemy | 0C/1 | 2n+3, model entries' script files | ovl2615 801E6314 |
| stage | 0C/3 | 7+2s, the scene's motion block | ovl2615 801E7210 |
| object_set | 28/0 | 2s+2, s = 0-2 (file 7 ends the sets) | 800A96B4, 800A979C |
| gear, gear_part | 28/1 | base+2; part files base+2+v (800C3508) | 800A9540, 800A979C |
| extra | 28/2 | all 231 files | opcodes 04/05 |
| model (ovl2143) | 04/0 | 0x6BA+2k, the 72 pairs after ovl2143 | field 80077884, ovl2602 801CF9BC |

`python3 -m tools.analysis.battle_effect_vm --sweep` decodes every table on
both discs, which hold identical files. The counts below are summed:

- battle: 926 tables, 11824 scripts, 174100 instructions; 99 of 118 opcodes
  used. All 9 event types are used: 4896 events in the 3072 63 commands and
  13260 in 8480 animations; 250 entries are unused ids.
- ovl2143: 144 tables, 1266 scripts, 10198 instructions; 24 of 113 opcodes
  used. 5 event types (1, 4, 5, 7, 8) are used: 190 events in 928 animations,
  none in 63 commands; 4 entries are unused ids.
- 0 unknown or undecodable commands or events in either dialect.
- Nothing starts, in the script areas (the first script up to the data block
  or the file end): 5422 battle and 338 ovl2143 separator words, and 40
  battle runs that decode as scripts of 288 commands. These are left out of
  the use counts. No unreached word fails to decode.

`--list FILE --kind script|gear_part|enemy|scene [--dialect ovl2143]` prints
one file's animations, events and disassembly. Keep listings under `.local/`.
`tests/test_battle_effect_vm.py` checks the tables against the parameter
words that each case of 800AAD54 and 801E39F0 reads, and against the steps of
800AE2A4 and 801E5D44.

Data notes. Three 63 commands per disc, in enemy sets 19 and 61, count one
event fewer than they hold, so their last event (8 bytes of sound) never
runs. 62 commands per disc in enemy sets 29, 46, 53, 70 and 74 name an unused
animation id (13 tweens, 11 animations, 10 poses). One 13 in model file 0x6BA
names id 44 of a 29-entry table. Neither lookup (800AF518, 801E6910) checks
its bounds.

The resident's `model_slot_ring_tmd` (0x170 bytes) is not an effect script. It is a
TMD model (id 0x41: one object, 6 vertices, 13 normals, 8 primitives) for the
slot-highlight ring, built by 800B15D8-800B2AEC. The battle headers call that
format an "effect script file".

Open: 19 battle opcodes (09 0B 12 18 20 29 2A 2B 2C 2D 42 45 47 52 55 56 59
5E 6B) have no use in the data. They are decoded from their handlers only.
