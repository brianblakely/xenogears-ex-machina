# Field event scripts

**Interpreter** (field overlay, `decomp/src/field/field_event.c`):

- `800a1ec8` runs the current actor's script until an instruction yields, its
  slot ends or the pass limit runs out (8, raised by some opcodes). The
  scheduler `800a2030` runs it for every active actor each frame.
- The byte at the PC indexes `field_event_primary_handlers[256]`. `fe` (`800869b8`) steps onto the
  next byte and indexes `field_event_extended_handlers[227]` with it, so extended handlers read
  their operands from the extended byte. Extended `e3`-`ff` index past the
  table and are unknown.
- Handlers advance the PC themselves. One that does not advance runs again, at
  once unless it yielded; extended waits step back onto `fe`. An extended handler that
  does neither leaves the PC on its byte, which then runs as the primary opcode
  of the same value: ext `00` and `78`-`7e` are empty, and the selector cases
  missing from ext `27`, `5c`, `ca`, `d4` and `dd` do the same.
- Operands come from `800acd7c`/`800acdb8` (halfwords), `800acdec` (bit 15 an
  immediate, else a variable), `8009cf78`-`8009d154` (immediate when a bit of a
  flags byte is set) and `8009cdb4` (actor selector: `ff`/`fe`/`fd` party slots,
  `fb` the actor itself).

**Data.** Map n's bundle is directory (4, 0) file `0xb8 + 2n` (`800777dc`,
`8001b53c`). Its event component (5) holds 128 bytes of variable type bits,
the actor count and 32 entry PCs per actor, then the bytecode (`80070cc8`).

- `80070cc8` allocates the header's component size + 0x10 bytes, and
  `field_decompress_component` ignores the size it is passed: `80032eb4` writes the whole
  packed stream, which ends 0-7 bytes past the size (the packer's last group
  took them from the bytes after the component). The rest of the allocation is
  undefined. The decoder walks that stream; its byte counts and gaps cover the
  component's size.
- A field change loads the bundle of the operand's low 12 bits (`8001b484`);
  bits 14 and 15 are flags.
- Component 6 is the map's encounter set; `71` and ext `84` request a battle with
  one of its formations, and `f7` arms its random draw
  ([formations.md](formations.md)).

- Events start only at their entry PC: event 0 at load (`800a28d4`), event 1
  whenever no slot is active (`800a2030`), event 2 on talk and 3 on touch
  (`8008399c`), actor 0's events 2 and 3 directly (`800a22ac`), a joining
  member's event 0 (`8008b978`), and any event that `07`-`09` request.
- Bytecode that opens with `ff` starts with 7-byte map entry records
  (`8009fa54`), so a zero entry there names no script.

**Instructions.** The table is `_PRIMARY`/`_EXTENDED` in `tools/analysis/events.py`
(mnemonic, handler, size, flow, operand offsets, effect), and the C handlers
carry fuller comments. `tests/test_events.py` checks it against
`decomp/src/field`: dispatch order, every literal PC advance and jump, and
every operand read.

- Forms pick by a selector byte. `10`/`11` and `57` are set-up and step pairs:
  the step re-reads the operands of the set-up 9 or 11 bytes back.
- Some handlers can advance further: `9a` by 6 (with operand 0), `d4`/`fc` by 6
  (an empty party slot) and ext `18`/`c6` past the following `fe 1a`.
- `a6` skips index × 3 bytes into a table of jumps. `12` continues at +4 inside
  its own operands while a movie is pending; that path is reported, not followed.

**Sweep.** Run `python3 -m tools.analysis.events --sweep` (it reads `.local/extract` and
`.local/discs`). `--list MAP [--disc N]` prints one map; keep listings under
`.local/`. Both discs carry the recovered field image (`38a1ce82`). Disc 2
holds 205 bundles, the other 525 slots are 24-byte placeholders, and its 205
maps are identical to Disc 1's.

| | Disc 1 | Disc 2 |
| --- | --- | --- |
| maps / actors | 730 / 18117 | 205 / 5211 |
| script starts (event entries) | 77394 (105703) | 21773 (32123) |
| reachable instructions | 572016 | 206124 |
| opcodes used, primary / extended | 219 / 191 | 209 / 176 |
| unknown / undecodable | 0 / 0 | 0 / 0 |

- 255 primary opcodes (`fe` is the prefix) and 227 extended ones are defined,
  in 508 forms.
- No script uses primary 06 0e 0f 13 30 45 48 54 55 66 73 78 7d 81-83 96 97
  9e 9f b0 b2 c3 c8 cc d1 d3 dc e3 e4 e8-ea ed fd ff. No script uses extended
  00 11 12 28-2f 30 31 33 35 37 6c-6e 71 75 78-7e ab b2 b3 be d7 d8 dc e2.
- Unreached bytes: 10226 of the 11164 gaps on Disc 1 (181801 bytes) and 4329
  of 4585 on Disc 2 decode as instructions. Most follow a return, end or jump
  and nothing jumps to them; they are reported, not counted.
- Field changes (`12`, `47`, `98`, ext `84`, ext `cf`): 3290 on Disc 1 and 1444
  on Disc 2. Their immediate operands name every bundle except maps 42 and 489
  (Disc 2 also 342 and 441). The sweep lists the 56 (44) operands read from a
  variable.

**Findings.**

- Map 489, actor 1 event 1 (+0x0d): `fe a0 00 fe 61 fe a0 01 5b`, on both
  discs. The header gives the component 0x11c bytes (24 of bytecode), but the
  stream decodes 0x11e: its last two bytes (`00 17`) end ext `a0`'s 12 operand
  bytes (`8008ea58`), so the instruction is whole, its flags byte is 0x17 and
  operand 1 names variable 0xfe00, past the bank. It also covers events 2 and 3
  at +0x17. Once a movie request is accepted (`800adbdc` set) the PC moves to
  +0x1a, past the stream, into the allocation's 14 undefined bytes. The bytes
  read like shorter `a0` forms (`fe a0 00`, `fe 61`, `fe a0 01`, `5b`) that the
  handler does not have. The sweep reports the instruction past the
  component's size and its successor past the stream.
- Only the three-digit map selectors load map 489. No immediate field operand
  names it, the new-game state names map 490 (directory 0x10 file 3, +0x231a),
  and no world-map exit names it (the path regions of every area file and the
  scripted exits; `python3 -m tools.analysis.dispatch_tables --sweep --only
  modes`). The variable operands are the previous field (`v0004`, set by
  `80092f44` at each change; map 317 also stores 312), the field `800a30fc`
  saved in `v003c` (maps 488 and 723), 319 with bit 15 (maps 316, 318, 320), map
  317's `v0420` (310, 312-315, 318, 319), and the selectors of maps 0 (`v0408`),
  488 and 723 (`v0432`), which build any number 0-799 from three digits. Map 0,
  whose own scripts also list maps 720-729, is entered by the field's debug key
  only when 80010000 is not -1 (`field_main`; both retail executables hold
  -1) and by maps 96, 722 and 728 (map 96 from a choice whose message 0x2e its
  retail table lacks) and world-map path regions with scene 0 in the scripted
  modes' area files. Map 723 is one of the listed maps. In map 488 (Shakhan and
  Bart's scene) actor 56's idle event (event 1) opens a menu whose first choice
  is the selector while port 1 holds exactly button bit 1 (0x0002); its messages
  13 and 18 are missing from the retail table, and its flag `v0050` is written
  only by maps 0, 721, 723, 728 and that menu.
- Map 222 +0x2186: `fc` names party slot 0 and is followed by the 2-byte `a9`,
  so its 6-byte skip would land inside it. Not followed.
- `8008d808` and `8008da04` write instructions into the bytecode, but nothing
  calls them.
- Ext `0c` sets six halfwords at `800b21a0` that no code reads:
  `tools/data_users.py --range 800b21a0:800b21ac` finds only its stores and the
  field setup's (0x100 three times, then 0x200).
- Named from their readers: ext `b9`-`bc` the world map vehicle's saved
  position, heading and flags (game `+182c`-`+1834`, `WorldmapReturn`), `d5` the
  ferry's saved place (`+1844`, `+1846`; `8006ee78`), `d6`/`d7` the circling
  flight's (`+184e`, `+1852`; `8006ee80`), `c0` the arena bout's outcome
  (`80050622`, arena_fighters_bout_and_effects `arena_bout_record_outcome`), `61` the field movie's start (`800adb7c`,
  `800a7c58`), `b5` the gathering warp (`800b2348`), and `2a`/`2b` and `cd`/`ce`
  the actor flags 0x20000 and 0x800000 that keep talk and touch, or touch alone,
  from starting events 2 and 3 (`8008399c`).
- Named from the readers of the flags they set:
  - `b7`/`b8` camera flag 0x4000. Only the follow camera (`80073230`, modes 0
    and 2) reads it: while it is clear, the eye goal sinks no lower than the floor
    of the last collision layer under it (`8007b1c4`). Ext `54` also sets it,
    with 0x8000 (`77`, which stops the shoulder-button turns of `800726e8`), and
    ext `53` and the camera reset (`8007254c`) clear it.
  - Ext `07` layer flag 0x400. `80075b44` sets layer flag 0x200 when an
    actor's sprite projects off screen. Such an actor moves (`8008110c`), turns
    (`800739c0`) and runs its sprite animation (`800752c8`) only with 0x400.
  - Ext `09` layer flag 0x800, which every actor starts with (`80080a74`).
    `800764b4` draws no drop shadow for it. For an 801e layer actor, `80075b44`
    sets bit 0 of its layer object's +4a, and ovl2143 `801dcec8` then skips the
    object's shadow quad.
  - Ext `46` layer flag 0x20000. It applies to an 801e layer actor (layer flag
    0x2000). The actor then moves by its layer object's speed (+128/+130;
    `80081f80`, `80082620`) and takes the layer model's facing. Without the flag
    it turns the model to its own facing (`80075b44`).
  - Ext `c3`/`c4` layer flags 0x2000000 and 0x800. `80075b44` still projects a
    sprite actor with 0x2000000 but does not draw its sprite; `22` and `24`
    clear it.
  - Ext `9f` the character bits of the game's `+2318`. The party menu (ovl2598
    `801cab48`) does not exchange a locked member, and the debug monitor (debug595
    `80281b90`) prints the bits as `FrLock`.
  - Ext `d1` flag 0x4000 of the game's `+22b6`. With it a gear at attack level 3
    may start the boost (battle `8009a2d4`). The battle results also learn
    counter skills 7-12 (ovl2596 `801e3be0`) and raise tier 6 to 7 at level 50
    (`801e403c`), and the field menu rates rows 7 and up of a member's
    completion table (slot39 `801e1418`).
- Character 4's special parts are its ammo. Its special slots (character
  record `+6f`) and its gear's (gear record `+04`) hold weapon and gear part
  ids from 50, 0 for an empty slot: the field menu offers inventory ids from
  50 (slot39 `801de5cc`), the battle lists weapon ids 50-72 (ovl2615
  `801e4cd0`) and copies weapon records 50-97 (`801e5384`). The two name
  tables, system texts 23 and 51 (`80033848`, `80033a5c`; `python3 -m
  tools.analysis.text_control --list system --item 23 --chars`, the same on
  both discs), end the name of every id 50-72 in the glyphs `20 49 49 4b`,
  which `--chars` spells "Ammo" by the method of
  [text-control.md](text-control.md): `o` (4b) is one of the 25 letters the
  memory card titles give, and `A` (20) and `m` (49) are two of the 27 that
  the name entry grid's alphabet runs name, an inference from the grid's
  order that those 25 letters confirm. Ids 73-99 have no kind, and the
  tables end at 98 with the text `ffff` for 73-98. Only character 4 (weapon
  users 0x10) and gears 5 and 13 (gear part users 0x2020) can use ids 50-72,
  and the new game gives character 4 weapons 31, 35, 35, 37 with special
  parts 50, 67, 69, 57, and gears 5 and 13 weapons 31 and 37 with 50 and 57
  (`python3 -m tools.analysis.special_parts --sweep`). Each id's byte
  (`ammo`, `gearAmmo`) counts its rounds: the field menu sets it to 100
  when the id is loaded (slot39 `801df0d4`), each action takes one from the
  slots its command number names (battle `8009afd8`, `8009e788`), a command
  whose descriptor names a slot at 0 misses (`itemKinds` 0x80 the first, 0x10
  or a gear's 0x20 the fourth; `80096ab8`, `8009d3a0`), and the battle window
  prints the count beside the name (`80093b08`).
- The round counts lie between the words of `d1` and `9f`. The code addresses
  an id's byte from 50 bytes before its array (`8006f8ba`, `+2286`, and
  `8006f8ea`, `+22b6`), so ids 50-97 are the 48-byte arrays at `+22b8`
  and `+22e8`. A gear's empty slot (id 0) reads the low byte of `d1`'s word
  (below), a gear part id 98 or 99 would reach `9f`'s word `+2318`, and a
  weapon id from 98 the gears' array. The sweep finds no source of ids from
  98 on either disc: give_item (its 172 and 53 variable operands resolved
  from the same map's set_variable immediates), the shops the field opens,
  the enemies' drops and the AI's drop setters supply weapon ids 50-64 (and
  67-70 on disc 1) and gear part ids 50-64, and the new-game file holds 100
  in all 96 bytes, 0x8000 at `+22b6` and 0 at `+2318`. Of the shops in the
  two tables (40 weapon shops of 0x5c bytes, 30 gear shops of 0x64), only
  gear shop 15 lists ids past 72 (gear parts 93-95 and 101; 101 would reach
  the saved map's high byte `+231b`), and only the menus of maps 488 and 723
  (with a number they step in `v0016` without a bound, map 488
  `+1998`-`+19f3`) and the resident's debug start (`8001c1a8`, which also
  wraps below 0 to 0xff) open it. The same openers reach every shop number to
  255 (the u8 `menu_state_screen_parameter`), and a shop past its table reads the memory after
  the unpacked table (ovl2601 `801c6a6c`, ovl2602 `801c6e74`; open).
  take_item (`8d`, field `8009640c`) leaves the id of a slot it empties at
  0xff. Map 701 takes gear part 43 (`+116`) and gives 42 (`+11a`) on both
  discs; when the give does not reuse the emptied slot (it adds to a slot
  holding 42, or fills an earlier free one: `8009635c`, `80094dec`), the gear
  part list keeps 0xff, which would reach `0x8006f9e9`, past the game data
  (open).
- An empty slot reads the low byte of `+22b6` for a gear, `+2286` (gear
  accessory list entry 108) for character 4. No code sets that low byte
  (`d1` sets 0x4000, `d0`'s copies to characters 9 and 10 set 0x2000 and
  0x1000, slot39 `801df0d4` fills the byte of a nonzero id, and `8009e788`,
  which takes the gear's rounds, lowers only a nonzero byte), so every gear
  without ammo in its first slot, the enemies' included (their records hold
  none), adds its `+9f` to its accuracy (battle `8009d3a0`); character 4's
  gears add it only while that slot reads 0 rounds.
- Compared with the host disassembler (`src/reconstruction/field_disassembler.cpp`),
  the lengths agree for every opcode and form. It differs in three places, and
  the C reading above is used:
  - it skips zero entries everywhere, missing event 0 at PC 0 in the bytecode
    that has no entry records;
  - it follows `12`'s +4 path;
  - it leaves variable `a6` tables unresolved.

**Open.**

- Whether map 488's selector menu opens in a retail game. The scripts leave
  its button test live (`v0050` 0), so it depends on actor 56 running event 1
  and the field's input mask there; a capture in map 488 would settle it.
- The bounds of variable `a6` indexes: the sweep follows consecutive jumps.
- Whether the gear accessory list ever holds an id at entry 108. Character
  4's empty special slot (slot39 `801e0434` empties slot 0 when its weapon
  changes) then reads that id as its rounds (battle `80080160`,
  `80093b08`, `80096ab8`) and its attacks lower the id (`8009afd8`).
- What the field menu does with an id 0xff that take_item leaves in the gear
  part list (above). slot39 `801de5cc` tests every id from 50 by the users and
  kind of its gear weapon record, not by its count; for 0xff that is record
  255, 5100 bytes into the 2003-byte table that `801c72bc` unpacks into a heap
  block of its own, so whether 0xff is offered depends on the heap. Loaded, it
  would set byte 0x0D of the resident's battle formation `formation_active`
  (`enemyIds[5]`, resident/formation.h; `0x8006f9e9`) to 100 (`801df0d4`).
  Every battle copies its formation over `formation_active` before its turns (battle
  `battle_main`), so it would read and lower that formation's enemy id byte
  as the rounds, not the 100.
- The stock of the shop numbers past the tables, weapon shops 40-255 and gear
  shops 30-255, which maps 488 and 723's selectors and the debug start can
  open: each reads the memory after its unpacked table.
