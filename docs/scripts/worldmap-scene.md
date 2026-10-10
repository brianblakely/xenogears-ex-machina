# World map scene directors

- **Interpreters:** five update handlers in the `worldmap` overlay (Disc 1 file 37,
  Disc 2 file 32), one per scripted world-map mode. Each mode's setup clears the actor
  slots (`worldmap_actor_alloc_slots`) and registers the screen fade (`worldmap_screen_fade_update`) in slot 0,
  the director in slot 1 and its scene actors after them.

  | Director | Mode (setup) | Starter | Sequences (states / waits, entries) |
  | --- | --- | --- | --- |
  | `worldmap_scene14_director_update` | 14 (`worldmap_scene14_start`) | `worldmap_scene14_director_start`, no step | `worldmap_scene14_cue_states` / `worldmap_scene14_cue_waits`, 14 |
  | `worldmap_scene12_director_update` | 12 (`worldmap_scene12_start`) | `worldmap_scene12_director_start` | `worldmap_scene12_cue_states` / `worldmap_scene12_cue_waits`, 8 |
  | `worldmap_scene15_director_update` | 15 (`worldmap_scene15_start`) | `worldmap_scene15_director_start` | `worldmap_scene15_cue_sequences[worldmap_entry_index]`: `worldmap_scene15_entry0_cue_states` (14), `worldmap_scene15_entry1_cue_states` (9), `worldmap_scene15_entry2_cue_states` (9) |
  | `worldmap_scene13_director_update` | 13 (`worldmap_scene13_start`) | `worldmap_scene13_director_start`, no step | `worldmap_scene13_cue_states` / `worldmap_scene13_cue_waits`, 9 |
  | `worldmap_scene16_director_update` | 16 (`worldmap_scene16_start`) | `worldmap_scene16_director_start` | `worldmap_scene16_cue_states` / `worldmap_scene16_cue_waits`, 37 |

- **Tables:** each director's unit opens its data with its u16 state and wait
  tables. They stay user-supplied: the units link them from the user's image
  (`INCLUDE_ASSET`; `asset` lines in
  `decomp/targets/overlays/worldmap.classification.txt`). `worldmap_scene13_director_update`'s waits
  `worldmap_scene13_cue_waits` end its unit's data, and a stray halfword (0x7542) follows them
  before the next unit's ([matching.md](../matching.md#recovering-data));
  `worldmap_scene16_director_update`'s waits `worldmap_scene16_cue_waits` hold one (0x2E07) in their alignment
  padding. No entry reaches either.
- **Dispatch:** `switch (actor->state)`, 65 cases in all (12, 10, 20, 10, 13), each
  commented with its effect. Case 1 decrements the s16 wait and, once it drops below
  0, loads the state and wait of entry `u.step` from the two u16 tables and steps.
  Every other case runs one cue and stores state 1 (or 0 after the last cue): requests
  to actor slots (`worldmap_actor_request`, command 1 with an argument, dropped while one is
  pending), area sounds, emitter groups, the fade quad's rate and step, the music fade.
  Case 0 idles. There are no jumps.
- **Timing:** the actor pass after the setup runs the starter (update 0); the director
  then runs once per frame. A cue runs on the update after its fetch, and the next
  fetch comes wait + 1 updates later. The starters that do not step fetch entry 0 a
  second time. The cue that clears `worldmap_loop_running` (case 11 or 0x40) ends the world-map
  loop after that frame with exit 0.
- **Coverage:** 65 cases defined; reached entries hold 58. The other seven are the
  five idle cases 0, `worldmap_scene12_director_update`'s case 1 (its cues return to it, but no entry
  holds it) and `worldmap_scene15_director_update`'s case 18. Each disc decodes 7 sequences and 98
  reached entries, none undecodable. The entries after an end cue are never fetched
  (`worldmap_scene14_cue_states` entries 12-13). `worldmap_scene12_director_update`'s end cue stores state 1, so another
  update would fetch entry 8, past its table.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` reads the tables from
  each disc's own packed `worldmap` container and prints aggregates;
  `--list worldmap-scene [--disc N]` prints each sequence as a timeline (update, entry,
  cue, and the slot handler each request goes to). `tests/test_overlay_scripts.py`
  checks each cue table against the switch cases, the starters, the setups' slots and
  the table lengths.
- **Requests:** `worldmap_actor_request` sets the slot's command to 1, so the actor pass
  (`worldmap_actor_run_all`) runs its update handler from then on, waking a slot whose start
  handler returned 3 (idle), and, unless a request is pending, stores the argument
  in `unk4`. A receiver that reads `unk4` (a `switch`, `== n` tests or `!= 0`)
  clears it and acts on it, mostly by moving to the state it names; its comments
  give each state's effect. Of the 98 request calls in the directors' cases, 96 name
  an argument their receiver takes. `worldmap_scene14_director_update`'s case 9 wakes the rig's flight
  (`worldmap_scene14_rig_flight_update`, idle since `worldmap_scene14_rig_flight_start` returned 3), which never reads the
  argument, and `worldmap_scene16_director_update`'s case 3 sends 0, the "none pending" value, so the
  heat haze keeps its state. `tests/test_overlay_scripts.py` checks every request
  against its receiver.
