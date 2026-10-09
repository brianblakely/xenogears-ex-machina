# Data-indexed dispatch tables

These tables have no program counter: one selector field of a fixed-layout record
picks the entry, and no code checks the index against the table's length.
`tools/analysis/dispatch_tables.py` reads the tables (entries, lengths, row values)
from the recovered C at run time and checks every record the game's loaders hand
to them on both discs:

```sh
python3 -m tools.analysis.dispatch_tables --sweep [--only formulas|primitives]
```

It prints aggregates only and exits nonzero when a selected record would index
past its table. `tests/test_dispatch_tables.py` ties the tables, offsets and
loader constants to `decomp/src` and walks synthetic records.

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

## Sound operand tables

`python3 -m tools.analysis.sound_sequence --sweep` counts the sound VM's two
operand-indexed tables (see [sound-sequence.md](sound-sequence.md)): the
modulator waves `D_800508A4[mode & 0xF]` (16 slots, always in range) and F0's
modulator index (`modulator[4]`, indices 0-3 used, none past the array).

## Open

- Whether a pilot of gear 17 or 18 can select the three records past
  `D_800C34DC`: they sit in technique slots 10, 13 and 14 (`gearCommands[21 + i]`,
  battle.c `func_8008B224` and `func_8008ADD0`), reachable when the pilot's
  technique mask (`CharacterBattleData.mask6`) has bit i. The census does not
  read the game's technique progression.
- The enemy census follows each script's writes in address order; it does not
  model a skipped rule's writes reaching a later rule's entry.
