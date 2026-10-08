# Battle effect-script VM

Battle objects (the stage model, stage object sets, gears and their part
objects, 3D enemies and their copies) run effect scripts.

- Interpreter: battle `func_800AAD54` (`decomp/src/battle/battle_8009E53C.c`,
  NON_MATCHING draft; switch on `jtbl_8007056C`, 118 cases 00-75). Its operand
  reads agree with the original's pc increments case by case.
- Sibling: ovl2143 `func_801E39F0` (`decomp/src/ovl2143/ovl2143.c`, matching;
  113 cases 00-70) runs the same format for the field and menu gear models.
  Its cases 04-07, 09, 0F, 12, 1B, 1C, 2C, 2D, 2F, 3A, 3E, 3F, 51-53, 58-5A,
  60, 61 and 65-6A do nothing and take no parameter words. Cases 33, 34 and 3B
  consume their word without jumping. 71-75 are unknown there.
- Command: an s16 word (opcode in the low byte, argument in the high byte),
  then the opcode's u16 parameter words. Jump offsets are signed bytes from the
  command's start. 31 loops back past the 30 command it names. 63 jumps over
  the animation events that follow it, which 800AE2A4 runs. An unknown opcode
  stops the script on itself.
- Script tables are relocatable blocks (8003342C): a count, the animation
  table, then script offsets (0 marks an empty id). Ids from 0x50 come from
  the object's extra file.
- Per-opcode semantics are in the module's `BATTLE`/`MODEL_VIEWER` tables (each
  names its case), in `decomp/src/battle/effect_vm.h` and in the comments on
  801E39F0's cases.

Where the scripts are. `directory` is group + index of 80028470. A file's
disc slot is file + table[directory] - 2, where the table is the boot
executable's copy of sector 40 (80018004).

| family | directory | files | loader |
|---|---|---|---|
| enemy | 0C/1 | 2n+3, model entries' script files | ovl2615 801E6314 |
| stage | 0C/3 | 7+2s, the scene's motion block | ovl2615 801E7210 |
| object_set | 28/0 | 2s+2, s = 0-2 | 800A96B4, 800A979C |
| gear, gear_part | 28/1 | base+2; part files base+2+v (800C3508) | 800A9540, 800A979C |
| extra | 28/2 | all 231 files | opcodes 04/05 |
| model (ovl2143) | 04/0 | 0x6BA+2k, the 72 pairs after ovl2143 | field 80077884, ovl2602 801CF9BC |

`python3 -m tools.analysis.battle_effect_vm --sweep` decodes every table on
both discs, which hold identical files. The counts below are summed:

- battle: 926 tables, 11824 scripts, 174100 instructions; 99 of 118 opcodes
  used; 0 unknown or undecodable.
- ovl2143: 144 tables, 1266 scripts, 10198 instructions; 24 of 113 opcodes
  used; 0 unknown or undecodable.

`--list FILE --kind script|gear_part|enemy|scene [--dialect ovl2143]` prints
one file's disassembly. Keep listings under `.local/`.

The resident's `D_8001C76C` (0x170 bytes) is not an effect script. It is a
TMD model (id 0x41: one object, 6 vertices, 13 normals, 8 primitives) for the
slot-highlight ring, built by 800B15D8-800B2AEC. The battle headers call that
format an "effect script file".

Open: 19 battle opcodes (09 0B 12 18 20 29 2A 2B 2C 2D 42 45 47 52 55 56 59
5E 6B) have no use in the data. They are decoded from their handlers only. The
animation-event bytes after 63 are 800AE2A4's format and are not decoded here.
