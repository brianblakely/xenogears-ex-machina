# World map scene directors

- **Interpreters:** five update handlers in the `worldmap` overlay (Disc 1 file 37,
  Disc 2 file 32), one per scripted world-map mode. Each mode's setup clears the actor
  slots (`func_8009766C`) and registers the screen fade (`func_800925A0`) in slot 0,
  the director in slot 1 and its scene actors after them.

  | Director | Mode (setup) | Starter | Sequences (states / waits, entries) |
  | --- | --- | --- | --- |
  | `func_8007A9F8` | 14 (`func_8007A5DC`) | `func_8007A9B4`, no step | `D_8009A450` / `D_8009A46C`, 14 |
  | `func_8007C3B8` | 12 (`func_8007BF50`) | `func_8007C36C` | `D_8009A4D8` / `D_8009A4E8`, 8 |
  | `func_8007DE98` | 15 (`func_8007D918`) | `func_8007DE14` | `D_8009A65C[D_8009D3D4]`: `D_8009A5D4` (14), `D_8009A60C` (9), `D_8009A634` (9) |
  | `func_80080370` | 13 (`func_8007FF70`) | `func_8008032C`, no step | `D_8009A698` / `D_8009A6AC`, 9 |
  | `func_800811C0` | 16 (`func_80080D00`) | `func_80081174` | `D_8009A6C0` / `D_8009A70C`, 37 |

- **Tables:** each director's unit opens its data with its u16 state and wait
  tables. They stay user-supplied: the units link them from the user's image
  (`INCLUDE_ASSET`; `asset` lines in
  `decomp/targets/overlays/worldmap.classification.txt`). The wait tables of
  `func_80080370` and `func_800811C0` end with a stray halfword in their alignment
  padding (0x7542, 0x2E07), which no entry reaches.
- **Dispatch:** `switch (actor->state)`, 65 cases in all (12, 10, 20, 10, 13), each
  commented with its effect. Case 1 decrements the s16 wait and, once it drops below
  0, loads the state and wait of entry `u.step` from the two u16 tables and steps.
  Every other case runs one cue and stores state 1 (or 0 after the last cue): requests
  to actor slots (`func_80097770`, command 1 with an argument, dropped while one is
  pending), area sounds, emitter groups, the fade quad's rate and step, the music fade.
  Case 0 idles. There are no jumps.
- **Timing:** the actor pass after the setup runs the starter (update 0); the director
  then runs once per frame. A cue runs on the update after its fetch, and the next
  fetch comes wait + 1 updates later. The starters that do not step fetch entry 0 a
  second time. The cue that clears `D_8009D554` (case 11 or 0x40) ends the world-map
  loop after that frame with exit 0.
- **Coverage:** 65 cases defined; reached entries hold 58. The other seven are the
  five idle cases 0, `func_8007C3B8`'s case 1 (its cues return to it, but no entry
  holds it) and `func_8007DE98`'s case 18. Each disc decodes 7 sequences and 98
  reached entries, none undecodable. The entries after an end cue are never fetched
  (`D_8009A450` entries 12-13). `func_8007C3B8`'s end cue stores state 1, so another
  update would fetch entry 8, past its table.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` reads the tables from
  each disc's own packed `worldmap` container and prints aggregates;
  `--list worldmap-scene [--disc N]` prints each sequence as a timeline (update, entry,
  cue, and the slot handler each request goes to). `tests/test_overlay_scripts.py`
  checks each cue table against the switch cases, the starters, the setups' slots and
  the table lengths.
- **Requests:** `func_80097770` sets the slot's command to 1, so the actor pass
  (`func_80097800`) runs its update handler from then on, waking a slot whose start
  handler returned 3 (idle), and, unless a request is pending, stores the argument
  in `unk4`. A receiver that reads `unk4` (a `switch`, `== n` tests or `!= 0`)
  clears it and acts on it, mostly by moving to the state it names; its comments
  give each state's effect. Of the 98 request calls in the directors' cases, 96 name
  an argument their receiver takes. `func_8007A9F8`'s case 9 wakes the rig's flight
  (`func_8007BBEC`, idle since `func_8007BB60` returned 3), which never reads the
  argument, and `func_800811C0`'s case 3 sends 0, the "none pending" value, so the
  heat haze keeps its state. `tests/test_overlay_scripts.py` checks every request
  against its receiver.
