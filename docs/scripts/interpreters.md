# Interpreters and data-selected dispatch

Every bytecode interpreter and every dispatch whose index comes from game data in
the 26 targets, with the document and decoder for each. They were found from the
recovered C: every file-scope initializer that names functions (the
function-pointer tables, listed below; `tests/test_dispatch_tables.py` fails
when one is missing here) and the 621 `switch` statements in `decomp/src`.

A `switch` cannot dispatch out of range: GCC's jump-table code compares the
index with the case range first and takes the default (or leaves the switch)
for any other value, even where a mask already bounds it (battle `800b23e8`:
`andi a0,v0,0x1c; sltiu v0,a0,29; beqz v0,default` before the jump through
`0x800707dc`). A function-pointer table has no such check, so every table
indexed by data is censused.

## Bytecode interpreters

A program counter walks game data; each machine's document lists every opcode
with its handler, and its decoder sweeps both discs.

| Machine | Image | Interpreter | Dispatch | Document | Decoder |
| --- | --- | --- | --- | --- | --- |
| Field events | field | `800a1ec8` | `field_event_primary_handlers[256]`, `field_event_extended_handlers[227]` | [field-events.md](field-events.md) | `events` |
| Enemy AI | battle | `800799c8`, `80079ab0`, `80079c24` | switches `8007ef6c` (actions), `8007f8c0` (conditions) | [battle-ai.md](battle-ai.md) | `battle_ai` |
| Effect scripts | battle, ovl2143 | `800aad54`, `801e39f0` | switches | [battle-effect-vm.md](battle-effect-vm.md) | `battle_effect_vm` |
| Battle events | ovl3087 | `801e879c` | switch | [battle-event-vm.md](battle-event-vm.md) | `battle_event_vm` |
| Sound sequences | resident | `8003c6e8` | `sound_seq_opcode_handlers[128]` | [sound-sequence.md](sound-sequence.md) | `sound_sequence` |
| Sprite animation | resident, battle | `800248d4`, `800c11cc` | switches, `8001fbe4`, `800b3f04` | [sprite-vm.md](sprite-vm.md) | `sprite_vm` |
| Text controls | resident | `80033df0` | bytes, 0F's jump table `80018a7c` | [text-control.md](text-control.md) | `text_control` |
| Staff roll | field | `800ac0f0` | CR only | [text-control.md](text-control.md) | `staff_roll` |
| World map actors | worldmap | `80076b34` | `worldmap_actor_script_handlers[12]` | [worldmap-actor.md](worldmap-actor.md) | `overlay_scripts` |
| World map scenes | worldmap | five directors | switches | [worldmap-scene.md](worldmap-scene.md) | `overlay_scripts` |
| Arena scenes | menu | `8007107c` | switch | [arena-scene.md](arena-scene.md) | `overlay_scripts` |
| Arena frame events | menu | `80074678` | switch on the kind | [arena-frame-events.md](arena-frame-events.md) | `overlay_scripts` |

The decoders are `python3 -m tools.analysis.<decoder> --sweep`.

## Cue timelines

Two readers step through (time, value) entries without dispatching on them: the
field's movie sound timelines (`80085678`, seeked by `80085788`) and the world map's
terrain texture animations (`80074f2c`, `80075104`). [timelines.md](timelines.md)
documents both; `overlay_scripts` decodes them.

## Function-pointer tables

| Table | Image | Index | Census |
| --- | --- | --- | --- |
| `battle_formula_table`, `battle_gear_formula_table` | battle | a command descriptor's formula | [dispatch-tables.md](dispatch-tables.md) |
| `model_primitive_types` | resident | a model primitive group's type | [dispatch-tables.md](dispatch-tables.md) |
| `sound_modulator_waves` | resident | a sound modulator's mode & 0xf (16 slots) | [sound-sequence.md](sound-sequence.md) |
| `worldmap_mode_handlers` | worldmap | the arrival word +0x2320 & 0x7fff | [dispatch-tables.md](dispatch-tables.md) |
| `sprite_draw_callbacks` | resident | a sprite header's kind (four bits) | [dispatch-tables.md](dispatch-tables.md) |
| `field_event_primary_handlers`, `field_event_extended_handlers` | field | the field event opcode | the field events interpreter |
| `sound_seq_opcode_handlers` | resident | the sound sequence opcode - 0x80 | the sound sequence interpreter |
| `worldmap_actor_script_handlers` | worldmap | the world map actor opcode | the world map actor interpreter |
| `arena_menu_port1_items`, `arena_menu_port2_items`, `arena_menu_title_items`, `arena_menu_bonus_battling_items`, `arena_menu_pause_items`, `arena_menu_settings_items`, `arena_menu_give_up_items`, `arena_menu_practice_items`, `arena_menu_pages` | menu | the menu page and cursor (menu4's items and lines) | code |
| `arena_mode_tasks` | menu | `mode_arena_task`, which only `mode_set_arena_task(0)` sets (resident and field) | code |
| `arena_spark_shapes`, `arena_spark_placement_rules`, `arena_spark_update_callbacks` | menu | an emitter's shape and placement, constants at both callers of `arena_spark_create_emitter` (1, 0 and 3, 0); its one update | code |
| `worldmap_open_map_actors` | worldmap | the actors every area starts, a list ended by kind 0 | code |
| `worldmap_area0_actors`, `worldmap_area3_actors`, `worldmap_area4_actors`, `worldmap_area5_actors`, `worldmap_area6_actors`, `worldmap_area8_actors` | worldmap | the area's actor list, `worldmap_area_actor_lists[worldmap_area_index]`; the area index comes from the position against `worldmap_area_thresholds`, whose last threshold is 0xffff | code |
| `mode_table` | resident | the mode number (0-6) the dispatcher `mode_dispatch` runs, set by `mode_select_next_mode` from code and from the movie's next-mode word `cd_movie_request_kind`; rows {entry, BSS start, BSS end, loaded} | [original-boundaries.md](../original-boundaries.md) |
| `exe_header` | resident | the PS-X EXE header's entry point, not a dispatch | none |

## Data-selected switches

Switches on a field of a data record take their default for a value without a
case. The TMD primitive kinds of battle `battle_tmd_build_packets`,
`battle_tmd_draw_object` and ovl3384 `battle_module_debris_start` (mode & 0x1c,
lit) are censused in [dispatch-tables.md](dispatch-tables.md). The effect events
of `800ae2a4` and `801e5d44`, the arena hit and effect types
(`arena_frame_event_hit`, `arena_frame_event_effect`), the AI action-list entry
types (`800793f0`) and the battle sprite commands' arguments (`800c11cc`,
`800b3f04`) are decoded with their machines. The battle formulas' sub-switches on
descriptor fields (`chanceSource`, `amountKind`, `defenseKind` in
`battle_menus_and_resolver.c`) are not censused; a value without a case takes the
switch's default.

## Rejected candidates

- TMD primitive builder (`battle_tmd_build_packets`, `battle_tmd_draw_object`, ovl3384): a model
  format (`battle/effect_script.h`'s "effect script file"), not an instruction
  stream. Its kinds are censused as a data-selected dispatch.
- Morph channels (resident `model_start_morph`, `model_step_morph_weight`): per-target vertex
  and normal delta lists that code weights; each channel's update is a function
  the code installs (the default steps the weight), and nothing reads an
  instruction stream.
- Debug overlays: debug595 (the field debug monitor), debug2611 (battle debug
  tools) and ovl2606 (the debug battle-scene selector) switch on pad input and
  their own screens, not on game data.
- The field's map entry records (`8009fa54`) and the world map's path regions
  (`worldmap_path_select_region`) are records the code tests, not dispatched; the world map
  arrival census lists the fields the path regions name.
