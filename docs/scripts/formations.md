# Battle formations and encounter sets

A battle starts from one **formation**, 0x20 bytes that the battle overlay copies
from the **encounter set** `D_800658DC` into `D_8006F9DC` as it starts (battle
`func_80070F40`: formation `D_80059508`, or `D_8005947C - 1` when that is set). The
layout is `BattleFormation` and `EncounterSet` in
[`decomp/include/resident/formation.h`](../../decomp/include/resident/formation.h),
whose member comments name their readers; every unit that reads a formation or a set
uses it, and its `LAYOUT_CHECK` fixes the offsets.

## Formation

Party members take battle slots 0-2 and enemies slots 3-10 (`BattleSlot`,
`decomp/include/battle/area.h`); "slot byte n" is byte n of a slot's record.

| Offset | Member | Readers and effect |
| --- | --- | --- |
| 0x00 | `battle` | n: enemy file (12, 1) 2n + 2 and enemy set file 2n + 3 (ovl2615 `func_801E5384`); [battle-ai.md](battle-ai.md) decodes the enemy file. |
| 0x01 | `flags` | 0x08: no result screens (ovl2596 `func_801E2280` skips `func_801E1FB8`, whose spoils list adds the drops, `func_801E1690`) and no fade after them (battle `func_80070F40`). 0x10: the party is party slot 0's character and character 10 twice, with gear 17 (ovl2615 `func_801E5384`). 0x20: the battle event script runs (`func_801E5014` sets `D_800C3D48`; [battle-event-vm.md](battle-event-vm.md)). 0x40, 0x80: every member gets command 7, 8 (`func_801E5014`). No reader tests 0x01, 0x02 or 0x04. |
| 0x02 | `stage` | s, the scene selector: resident `func_8001BB0C` passes it to `func_800379D8`, which loads the stage files (12, 3) 6 + 2s and 7 + 2s (s must be below half the file count of entry 5). The stage holds each formation group's standing places (battle's `Formation`, `D_8005949C`). |
| 0x03 | `scriptSet` | the battle event script set, read only under flag 0x20 (ovl3087 `func_801E5160`). |
| 0x04 | `partyGroups[3]` | per member, & 0x7f: its formation group, unless it fights in its gear (then its slot number; ovl2615 `func_801E4160`). |
| 0x07 | `unk7` | no reader. |
| 0x08 | `enemyIds[8]` | per enemy, & 0x7f: its id in the enemy file (0x7f none); bit 7: it fights in a gear, slot byte 4 (`func_801E4160`). |
| 0x10 | `enemyFlags[8]` | bit 7 to slot byte 3 (`BattleSlot.hidden`, which targeting tests and AI action 63 and condition 9b write and read); bit 0 to slot byte 5 (`func_801E4160`). |
| 0x18 | `enemyGroups[8]` | & 0x7f: the enemy's formation group (`func_801E4160`). Bit 7 of byte 0x18 + s goes to slot byte 6 for slots s = 3-10, so the last three reads take the three bytes after the record (the SPU memory map `D_8006F9FC`). Setup phase 2 (`func_801E5014`) sets every slot's byte 6 anew from `func_80085310` before the slot sprites that read it are made (`func_800B81BC` runs after the setup phases in `func_80070F40`). |

## Encounter sets

`EncounterSet` is 16 formations (0x200 bytes). Three loaders fill `D_800658DC`:

- **Field maps.** Map bundle component 6, which `func_80070CC8` decodes into
  `D_800658DC` itself: the set, then the 16 weights `D_80065ADC` of the random draw.
  `func_80079288` picks a formation with a nonzero weight by a random number against
  the weights' running sums. Every component 6 that is not empty is 0x210 bytes; the
  packed stream ends 0-7 bytes later, in `D_80065AEC`, which no code reads. A map
  whose component 6 is empty leaves the set that was loaded before. The debug monitor
  prints the weights with its per-formation counts (debug595 page 10).
- **The world map.** The area file (0x24, 0) area + 1 of each set of `D_8009B584`
  (`func_80071B9C`). `func_80073530` points `D_8009D73C[kind]` at the offsets in header
  words 11-26 (`AreaHeader` +0x2c), one table per terrain kind. The roll
  (`func_80075E7C`, from the world map loop `func_800712D0` when a timer of
  `func_8007528C` expires) takes the terrain kind at the party's position
  (`func_80094028`, or its substitute `D_8009A3A0` when `func_80093F18` returns 4), the
  weight row of the bracket the scene id (variable 0) falls in (`D_8009B578`: 0-53,
  54-200, 201-339, 340 and up), draws a formation by those 16 weights and copies the
  kind's 0x200 bytes into `D_800658DC`. A table is the set and four weight rows; the
  next table follows 0x20 bytes later, bytes no reader reads. The area files 143,
  154, 165, 176 and 187 (modes 9, 10 and 12-15) hold no tables: their header ends at
  0x30, so words 12-26 are section data, and word 11 leaves no room for a table.
- **The debug battle selector.** ovl2606 `func_801E0A34` copies the first 0x200 bytes
  of (0x20, 3) file 7 + n (FileNo n) or 4-6 (Event1-3). Files 4-52 hold sets: 34 of
  0x210 bytes, 32 of them a field map's set, and 15 of 0x260 bytes whose sets are world
  map tables'. File 53 starts a sub-directory.

`D_80059508` names the formation. The field's draw and the world map's roll set it,
and so do field events 71 and fe 84 from operand 1 (`func_80093568`, `func_800933F8`:
an immediate when bit 15 is set, else a variable) and the debug selector's SceneNo.
Battle event opcode 24 (ovl3087 `func_801E7700`) sets `D_8005947C` to its operand + 1.
When that battle ends with outcome 1, 0x40 or 0x21, the resident battle mode
(`func_8001B6C4`) runs another battle (unless `D_800D3338` is set), which takes that
formation of the same set.

## Census and cross-check

`python3 -m tools.analysis.formations --sweep` reads `.local/extract` and
`.local/discs` and prints the counts below. `--list field|worldmap|debug [--item N]
[--disc D]` prints each formation with its weights: battle, scene selector, script
set, flags, party groups, and per enemy its id, group and flags. Keep listings under
`.local/`. `tests/test_formations.py` checks the decoder against `formation.h` on
invented sets, bundles, area files and discs. On the user's discs:

- Disc 1: 730 map bundles, 635 with a set and 95 with an empty component 6; 17 area
  files, 12 with 16 tables (192); 49 debug files. The field and world map sets hold
  13232 formations (587 distinct). 3550 can be drawn (a nonzero weight), 260 are named
  by a field script, 4 are chained by opcode 24, and 9441 are started by none of these.
- Flag counts: 0x08 21, 0x10 13, 0x20 1002, 0x40 3076, 0x80 2506. 0x01 and 0x02 are
  set in 340 formations each, though no reader tests them; 0x04 is never set. Byte 7
  is always 0. Of the 40835 enemies placed, 15534 fight in a gear; their flag bytes
  are 0x00 or 0x80, so bit 0 (slot byte 5) is never set.
- Field scripts: 348 battle requests in 125 maps. 334 are immediate, naming formations
  0-15 of maps that have a set; 14 take variable `v0400` (maps 480, 485 and 486).
  Opcode 24 occurs in event sets 2, 41 and 46, naming formations 0, 6 and 13 of the
  set they run from: maps 2 and 489 chain to 0, 713 to 6, 174 to 13.
- Disc 2 holds 205 bundles (525 placeholder files). Its 176 sets, the area files and
  the debug files are identical to disc 1's, as are the enemy files and the event
  archive (their sweeps), so disc 1's cross-check covers both: disc 2 places no pair
  disc 1 does not.
- **(battle, enemy id) pairs**, against the enemy files of `tools.analysis.battle_ai`:
  the formations place 256 pairs, every one a block of its battle's enemy file. 146
  are placed by formations that can be drawn, 152 by named ones and 7 by chained ones;
  10 only by formations nothing starts. None of the 33 blocks without a script table
  is placed. No formation names battle 58, not even a debug one. 319 script tables are
  placed by no formation. 313 of these are byte-identical to a placed block (277 are
  copies of one 84-byte block); the other six are battle 5 id 4, 8 id 4, 25 id 2,
  53 id 2, and 58 ids 0 and 1.
- **Script sets** (`tools.analysis.battle_event_vm`): formations with flag 0x20 name 44
  of the archive's 48 sets, none past it. Only the debug files name sets 25, 36, 43
  and 47.
- **Scene selectors:** 73 are used, the highest 74; directory (12, 3) holds 75 stage
  pairs.

## Open

- The six script tables and battle 58 that no formation places: a starter other than
  the three set loaders above has not been found, and no battle code is known to
  place an enemy id itself.
- Which formations the 14 variable requests name: maps 480, 485 and 486 copy `v0400`
  from `v0420`, `v0426` and `v042a` before them (`python3 -m tools.analysis.events
  --list 480`), and those are not traced.
- Slot byte 5 (`enemyFlags` bit 0, never set): no unit reads it through `BattleSlot`
  or ovl2615's `SlotInfo`, whose only accesses are the writes in `func_801E4048` and
  `func_801E4160`. A read through another view is not ruled out.
- Whether the modes that load the five tableless area files can roll: their timers
  (`func_8007528C`) are not traced.
