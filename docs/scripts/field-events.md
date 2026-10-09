# Field event scripts

**Interpreter** (field overlay, `decomp/src/field/field_800854D0.c`):

- `800a1ec8` runs the current actor's script until an instruction yields, its
  slot ends or the pass limit runs out (8, raised by some opcodes). The
  scheduler `800a2030` runs it for every active actor each frame.
- The byte at the PC indexes `D_800AE2A0[256]`. `fe` (`800869b8`) steps onto the
  next byte and indexes `D_800AE6A0[227]` with it, so extended handlers read
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
  `func_8007008C` ignores the size it is passed: `80032eb4` writes the whole
  packed stream, which ends 0-7 bytes past the size (the packer's last group
  took them from the bytes after the component). The rest of the allocation is
  undefined. The decoder walks that stream; its byte counts and gaps cover the
  component's size.
- A field change loads the bundle of the operand's low 12 bits (`8001b484`);
  bits 14 and 15 are flags.
- Component 6 is the map's encounter set; `71` and ext `84` request a battle with
  one of its formations ([formations.md](formations.md)).

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
  only when 80010000 is not -1 (`func_80077E88`; both retail executables hold
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
  ferry's saved place (`+1844`, `+1846`; `D_8006EE78`), `d6`/`d7` the circling
  flight's (`+184e`, `+1852`; `D_8006EE80`), `c0` the arena bout's outcome
  (`80050622`, menu3 `func_80075060`), `61` the field movie's start (`800adb7c`,
  `800a7c58`), `b5` the gathering warp (`800b2348`), and `2a`/`2b` and `cd`/`ce`
  the actor flags 0x20000 and 0x800000 that keep talk and touch, or touch alone,
  from starting events 2 and 3 (`8008399c`).
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
- Forms still named by a flag bit, whose readers are not traced here: camera flag
  0x4000 (`b7`/`b8`), layer flags 0x400, 0x800, 0x20000 and 0x2000000 (ext `07`,
  `09`, `46`, `c3`, `c4`), the character bits of the game's `+2318` (ext `9f`)
  and flag 0x4000 of its `+22b6` (ext `d1`).
