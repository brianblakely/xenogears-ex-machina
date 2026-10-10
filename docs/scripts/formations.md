# Battle formations and encounter sets

A battle starts from one **formation**, 0x20 bytes that the battle overlay copies
from the **encounter set** `formation_encounter_set` into `formation_active` as it starts (battle
`battle_main`: formation `formation_selected_index`, or `mode_pending_battle_formation - 1` when that is set). The
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
| 0x01 | `flags` | 0x08: no result screens (ovl2596 `func_801E2280` skips `func_801E1FB8`, whose spoils list adds the drops, `func_801E1690`) and no fade after them (battle `battle_main`). 0x10: ovl2615 `func_801E5384` sets `battle_uses_fixed_party`: the party is party slot 0's character and character 10 twice, members 1 and 2 with gear 17. Every member then fights in a gear (`func_801E4048`), so `partyGroups` is not read (`func_801E4160`); members 1 and 2 get fixed HP and stats (battle `battle_set_debug_party_stats`, from `func_801E4AC0`), character 11's portrait (battle `battle_upload_party_portraits`) and no place in the results (ovl2596 `func_801E2280`). 0x20: the battle event script runs (`func_801E5014` sets `battle_uses_event_script`; [battle-event-vm.md](battle-event-vm.md)). 0x40, 0x80: every member gets command 7, 8 (`func_801E5014`). No reader tests 0x01, 0x02 or 0x04. |
| 0x02 | `stage` | s, the battle stage: resident `mode_load_current_battle_stage` passes it to `mode_load_battle_stage`, which loads the stage file (12, 3) 6 + 2s into `mode_battle_stage_file` and the scene data 7 + 2s, after its size word, into `mode_battle_scene_file` and `mode_battle_scene_data` (s must be below half the file count of entry 5). ovl2615 `func_801E7210` takes them as its `stage` and `scene`. The scene data holds each formation group's standing places (ovl2615 `BattleScene.group`, read by `func_801E4160`; battle's `Formation`) and the cameras (`battle_enter`). |
| 0x03 | `scriptSet` | the battle event script set, read only under flag 0x20 (ovl3087 `func_801E5160`). |
| 0x04 | `partyGroups[3]` | per member, & 0x7f: its formation group, unless it fights in its gear, as every member does under flag 0x10 (then its slot number; ovl2615 `func_801E4160`). |
| 0x07 | `unk7` | no reader. |
| 0x08 | `enemyIds[8]` | per enemy, & 0x7f: its id in the enemy file (0x7f none); bit 7: it fights in a gear, slot byte 4 (`func_801E4160`). |
| 0x10 | `enemyFlags[8]` | bit 7 to slot byte 3 (`BattleSlot.hidden`, which targeting tests and AI action 63 and condition 9b write and read); bit 0 to slot byte 5 (`func_801E4160`). |
| 0x18 | `enemyGroups[8]` | & 0x7f: the enemy's formation group (`func_801E4160`). Bit 7 of byte 0x18 + s goes to slot byte 6 for slots s = 3-10, so the last three reads take the three bytes after the record (the SPU memory map `sound_spu_memory_map`). Setup phase 2 (`func_801E5014`) sets every slot's byte 6 anew from `battle_is_target_at_lower_x` before the slot sprites that read it are made (`battle_enter` runs after the setup phases in `battle_main`). |

## Encounter sets

`EncounterSet` is 16 formations (0x200 bytes). Three loaders fill `formation_encounter_set`:

- **Field maps.** Map bundle component 6, which `func_80070CC8` decodes into
  `formation_encounter_set` itself: the set, then the 16 weights `formation_encounter_weights` of the random draw.
  `func_80079288` picks a formation with a nonzero weight by a random number against
  the weights' running sums. Every component 6 that is not empty is 0x210 bytes; the
  packed stream ends 0-7 bytes later, in `commons_unused_4_words_b`, which no code reads. A map
  whose component 6 is empty leaves the set that was loaded before. The debug monitor
  prints the weights with its per-formation counts (debug595 page 10).

  A script of the map has to arm the draw. Player control (events 0c and a7,
  `func_8009F5F4`) calls `func_80079288` on frames the player holds a direction with
  no dialogue open, and `func_80079288` returns at once while the period
  `D_800B2078.unk2298` is 0. Every map load clears the period and the count `unk229C`
  (`func_800705DC`, which `func_80070CC8` calls first). Event f7 (`func_8008E85C`)
  sets the period from operand 1 and the count from operand 3 (at most 32);
  `func_8008E718` then gives that many countdowns `unk22A0` distinct values from 1 to
  period + 1, or clears the period when the count is 0. `func_80079288` counts them
  down and draws when one reaches 0, and `func_8008E718` deals new values each time
  `unk2294` counts the period down. Nothing else writes the period or the count but
  the debug monitor's page 10 (TIME and ENCOUNT, `func_80281B90`).
- **The world map.** The area file (0x24, 0) area + 1 of each set of `D_8009B584`
  (`func_80071B9C`). `func_80073530` points `D_8009D73C[kind]` at the offsets in header
  words 11-26 (`AreaHeader` +0x2c), one table per terrain kind. The roll
  (`func_80075E7C`, from the world map loop `func_800712D0` when a timer of
  `func_8007528C` expires) takes the terrain kind at the party's position
  (`func_80094028`, or its substitute `D_8009A3A0` when `func_80093F18` returns 4), the
  weight row of the bracket the scene id (variable 0) falls in (`D_8009B578`: 0-53,
  54-200, 201-339, 340 and up), draws a formation by those 16 weights and copies the
  kind's 0x200 bytes into `formation_encounter_set`. A table is the set and four weight rows; the
  next table follows 0x20 bytes later, bytes no reader reads. The area files 143,
  154, 165, 176 and 187 (modes 9, 10 and 12-15) hold no tables: their header ends at
  0x30, so words 12-26 are section data, and word 11 leaves no room for a table.
- **The debug battle selector.** ovl2606 `func_801E0A34` copies the first 0x200 bytes
  of (0x20, 3) file 7 + n (FileNo n) or 4-6 (Event1-3). Files 4-52 hold sets: 34 of
  0x210 bytes, 32 of them a field map's set, and 15 of 0x260 bytes whose sets are world
  map tables'. File 53 starts a sub-directory.

`formation_selected_index` names the formation. The field's draw and the world map's roll set it,
and so do field events 71 and fe 84 from operand 1 (`func_80093568`, `func_800933F8`:
an immediate when bit 15 is set, else a variable) and the debug selector's SceneNo.
Battle event opcode 24 (ovl3087 `func_801E7700`) sets `mode_pending_battle_formation` to its operand + 1.
When that battle ends with outcome 1, 0x40 or 0x21, the resident battle mode
(`mode_run_battle`) runs another battle (unless `battle_continue_to_movie_mode` is set), which takes that
formation of the same set.

## Census and cross-check

`python3 -m tools.analysis.formations --sweep` reads `.local/extract` and
`.local/discs` and prints the counts below. `--list field|worldmap|debug [--item N]
[--disc D]` prints each formation with its weights: battle, stage, script set, flags,
party groups, and per enemy its id, group and flags; for a field map also its battle
requests, its f7s and whether it arms the draw. Keep listings under `.local/`.
`tests/test_formations.py` checks the decoder against `formation.h` on invented sets,
bundles, area files and discs. On the user's discs:

- Disc 1: 730 map bundles, 635 with a set and 95 with an empty component 6; 17 area
  files, 12 with 16 tables (192); 49 debug files. The field and world map sets hold
  13232 formations (587 distinct). 1955 can be drawn: 1343 world map formations with a
  weight and 612 field formations with a weight on a map that arms the draw. 260 are
  named by a field script, 3 are chained by opcode 24, and 11017 are started by none
  of these.
- The field draw: 611 maps have a weight. 127 reach an f7 with a nonzero period and
  count, and player control; map 723 reaches only f7 900/0, which clears the period;
  483 reach no f7, so their weights are never used (1595 weighted formations in all).
  No map whose component 6 is empty arms the draw, so a field draw only takes the
  map's own set. The scripts reach 181 f7s, all with immediate operands: 152 arm the
  draw (periods 240-900, counts 1-3) and 29 have a count of 0.
- Flag counts: 0x08 21, 0x10 13, 0x20 1002, 0x40 3076, 0x80 2506. 0x01 and 0x02 are
  set in 340 formations each, though no reader tests them; 0x04 is never set. Byte 7
  is always 0. Of the 40835 enemies placed, 15534 fight in a gear; their flag bytes
  are 0x00 or 0x80, so bit 0 (slot byte 5) is never set.
- Field scripts: 348 battle requests in 125 maps. 334 are immediate, naming formations
  0-15 of maps that have a set; 14 take variable `v0400` (maps 480, 485 and 486).
  Opcode 24 occurs in event sets 2, 41 and 46, naming formations 0, 6 and 13 of the
  set they run from. Each runs from a named formation: map 2's formation 2 (set 2)
  chains to 0, map 713's 5 (set 41) to 6 and map 174's 14 (set 46) to 13. Map 489's
  formation 2 also runs set 2, but only its weight could start it, and map 489's
  scripts reach no f7.
- Disc 2 holds 205 bundles (525 placeholder files). Its 176 sets, the area files and
  the debug files are identical to disc 1's, as are the enemy files and the event
  archive (their sweeps), so disc 1's cross-check covers both: disc 2 places no pair
  disc 1 does not, and draws, names or chains none that disc 1 does not.
- **(battle, enemy id) pairs**, against the enemy files of `tools.analysis.battle_ai`:
  the formations place 256 pairs, every one a block of its battle's enemy file. 120
  are placed by formations that can be drawn, 152 by named ones and 6 by chained ones;
  11 only by formations nothing starts. None of the 33 blocks without a script table
  is placed. No formation names battle 58, not even a debug one. 319 script tables are
  placed by no formation. 313 of these are byte-identical to a placed block (277 are
  copies of one 84-byte block); the other six are battle 5 id 4, 8 id 4, 25 id 2,
  53 id 2, and 58 ids 0 and 1.
- **Script sets** (`tools.analysis.battle_event_vm`): formations with flag 0x20 name 44
  of the archive's 48 sets, none past it. Only the debug files name sets 25, 36, 43
  and 47.
- **Stages:** 73 are used, the highest 74; directory (12, 3) holds 75 pairs of a stage
  file and its scene data.

## Open

- The six script tables and battle 58 that no formation places: a starter other than
  the three set loaders above has not been found, and no battle code is known to
  place an enemy id itself.
- Which formations the 14 variable requests name: maps 480, 485 and 486 copy `v0400`
  from `v0420`, `v0426` and `v042a` before them (`python3 -m tools.analysis.events
  --list 480`), and those are not traced.
- Slot byte 5 (`enemyFlags` bit 0, never set): no unit reads it through `BattleSlot`
  (`field5`), whose only accesses are ovl2615's writes in `func_801E4048` and
  `func_801E4160`. A read through another view is not ruled out.
- Whether the modes that load the five tableless area files can roll: their timers
  (`func_8007528C`) are not traced.
- The draws' other gates. "Can be drawn" means the weights, the arming and player
  control do not rule a formation out. `func_80079288` also returns while other field
  states are set (`D_800ADBDC`, `D_800ADBE4`, `D_800ADBEC`, `mode_music_load_pending`, `D_800ADB2C`,
  `D_800ADB04`, `encounter_inhibition`), and the world map loop has its own
  (`func_800712D0`); when scripts set them is not traced.
