# Battle enemy AI scripts

**Interpreter** (battle overlay, `decomp/src/battle/battle_800792F8.c` and
`battle_80079ED8.c`):

- Runners stop at `fd` or `ff`: `800799c8` runs an enemy's turn script,
  `80079ab0` its reaction script during a party member's attack step, and
  `80079c24` its targeted script after a party turn that targeted it.
- `80079934` steps four bytes. `80079948` skips a rule after a false condition.
- Actions go through `8007ef6c` (jump table `8006fc3c`, cases 01-74). The 114
  handled opcodes include `62`, a case with no handler. `00`, `6e`, `6f` and
  `75`-`7f` reach the default `8007a7bc`.
- Conditions go through `8007f8c0` (`8006fe0c`, cases 80-9b, 28 opcodes).
  `80` and `9a` are always true. `99` ors the conditions after it.
- `800793f0` executes the action list at `800d2e5c` afterwards. Entry types 1-16
  go to `80078998`...`8007887c`. Any other type gets the script error
  `800792f8`.

**Data.** Battle n's enemy data file is directory (12, 1) file 2n + 2: ovl2615
`801e5384` loads it and `801e4870` points the scripts.

- File layout: `+0x00` holds eight u16 block offsets (enemy ids 0-7), then
  zeros. `+0x30` is the name table offset, and `+0x32` holds eight 0x170-byte
  combatant records.
- Each block opens with four u16 offsets relative to the block: turn (AI block
  `+0x00`), sub (`+0x04`), reaction (`+0x08`) and targeted (`+0x0c`). For the
  last two, `ffff` means unarmed. The scripts follow back to back, and the
  block ends with an `ff` word.

**Instructions** are `op b1 b2 b3`.

- A rule is one or more conditions (all must hold) followed by actions, and
  usually closes with `fd`.
- After a false condition, `80079948` skips the remaining conditions. It then
  skips every opcode outside 80-ef, which also passes over `fd`/`ff`.
- Actions write the enemy's variables in its AI block at `800d3400`. The
  listing calls them `bN` (u8 at +0x30), `vN` (u16 at +0x20) and `lN` (s32 at
  +0x10).
- Actions also write action-list entries of 8 bytes: type, arg1, animation,
  name, parameter (2 bytes) and targets. `list_set +0, type` closes an entry.
- The opcode table (mnemonic, operand bytes, handler, effect) is
  `_ACTIONS`/`_CONDITIONS` in `tools/analysis/battle_ai.py`. The C handlers
  carry the same comments.

**Sweep.** Run `python3 -m tools.analysis.battle_ai --sweep` (it reads `.local/extract`
and `.local/discs`). `python3 -m tools.analysis.battle_ai FILE` lists one
enemy file; keep listings under `.local/`. Results are the same on both discs,
whose enemy files are identical:

- 76 enemy files, 608 id blocks, 575 script tables.
- 685 scripts: 575 turn, 1 sub, 27 reaction, 82 targeted.
- 26552 instruction words, 18764 of them reachable from the entries.
- 0 undecodable, 0 unknown opcodes.
- 144 opcodes defined (114 actions, 28 conditions, `fd`/`ff`). 73 occur in
  script words and 72 are reachable; `ff` only closes blocks.
- Unused actions: 03 0a 0b 0d-0f 12-25 27 32-39 3f 45 46 49 4c-4e 51 53 55 57
  5b-61 68 6a 6c 6d. Unused conditions: 87 8a-8e 91 92 95-9a.
- Action-list entry types written: 1-4, 7 and 12-16, all handled by
  `800793f0`.

**Findings.**

- 33 blocks have no script table (battles 0, 2 and 5-11). They hold one
  identical 0x170-byte record image with offsets 0, 0, 0xbb, 0. Their ids'
  combatant records are template images too: in battle 0 the same image,
  elsewhere a single record that all these ids share. They are reported, not
  decoded.
- Battle 70, id 0, targeted script: when its last rule's condition fails
  (`ge_b b2, 0x20` at file offset 0xcf6), the skip runs past `fd`/`ff` and id
  1's table. The original then runs id 1's turn rule at 0xd2a.
- Only battle 3, id 0 has a sub script (block +0xdc), and no recovered code
  runs it.
- Corrected C comments: the handlers of 70-74 were labelled 6e-72, and 2b's
  attribute operand is b2.
- Targets named from their readers: record +0x14c is the enemy's experience,
  +0x156 its gold and +0x150/+0x152/+0x154 its first drop's chance, item and
  category (ovl2596's `Combatant`; actions `39`, `3a`, `3c`); `800d2c60` and
  `800d2c8b` are the enemy slot's pending amount and result code in the battle
  work table (`damage[3 + enemy]`, `resultCode[3 + enemy]`; action `69` sets code
  4, which no results pass applies); `*800d3278` + 0x394 is the battle event
  script's variables (ovl3087 `ScriptState.vars`, action `70`); `8005a3a0` holds
  16 persistent battle halfwords that the menu keeps in the game data at +0x2324
  (slot39); the `800d32a1` slot flag is "in a gear" (ovl2615 copies the game
  data's per-member gear flag `D_8006F8E5` and the formation id's bit 7; battle
  reads it as `D_800D32A0[slot].unk1` to choose gear commands).

**Open.**

- Formations are not cross-checked: the encounter sets in field bundles and on
  the world map decide which (battle, enemy id) pairs are placed. The commands
  every enemy's scripts select are counted against the formula tables in
  [dispatch-tables.md](dispatch-tables.md) whatever places it.
