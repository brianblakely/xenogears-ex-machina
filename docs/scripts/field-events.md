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
| reachable instructions | 572015 | 206123 |
| opcodes used, primary / extended | 219 / 191 | 209 / 176 |
| unknown / undecodable | 0 / 1 | 0 / 1 |

- 255 primary opcodes (`fe` is the prefix) and 227 extended ones are defined,
  in 508 forms.
- No script uses primary 06 0e 0f 13 30 45 48 54 55 66 73 78 7d 81-83 96 97
  9e 9f b0 b2 c3 c8 cc d1 d3 dc e3 e4 e8-ea ed fd ff. No script uses extended
  00 11 12 28-2f 30 31 33 35 37 6c-6e 71 75 78-7e ab b2 b3 be d7 d8 dc e2.
- Unreached bytes: 10226 of the 11165 gaps on Disc 1 (181801 bytes) and 4329
  of 4586 on Disc 2 decode as instructions. Most follow a return, end or jump
  and nothing jumps to them; they are reported, not counted.

**Findings.**

- Map 489, actor 1 event 1 (+0x0d): `fe a0 00 fe 61 fe a0 01 5b`. Ext `a0`
  (`8008ea58`) reads 12 bytes, but the 24-byte bytecode ends first, so these
  bytes read like a one-byte form that the handler does not have. Both discs
  have this script, and it is the only undecodable one.
- Map 222 +0x2186: `fc` names party slot 0 and is followed by the 2-byte `a9`,
  so its 6-byte skip would land inside it. Not followed.
- `8008d808` and `8008da04` write instructions into the bytecode, but nothing
  calls them.
- Compared with the host disassembler (`src/reconstruction/field_disassembler.cpp`),
  the lengths agree for every opcode and form. It differs in three places, and
  the C reading above is used:
  - it skips zero entries everywhere, missing event 0 at PC 0 in the bytecode
    that has no entry records;
  - it follows `12`'s +4 path;
  - it leaves variable `a6` tables unresolved.

**Open.**

- Whether map 489's actor 1 ever runs event 1.
- The bounds of variable `a6` indexes: the sweep follows consecutive jumps.
- Some targets are still only addresses: the six halfwords at `800b21a0` (ext
  `0c`) and the game's `+182c`-`+1856` words (ext `b9`-`bc`, `d5`-`d7`).
