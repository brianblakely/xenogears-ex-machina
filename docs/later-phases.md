# Later-phases handbook

This handbook is for the engineer of plan.md Phases 2-5. It covers what the Phase 1
decomp gives them: how to use it as an oracle, how the program is built, what a
port replaces, and what it must keep. Every statement cites the recovered source
(`decomp/src`, with functions named by address), matched disassembly, a
committed tool or the document that records it. Existing documents are
summarised and linked, not repeated. Anything not established is listed under
[Open questions](#open-questions).

Read first: [plan.md](../plan.md) (shared architecture, Phases 2-5),
[matching.md](matching.md) (build, comparison, coverage),
[original-boundaries.md](original-boundaries.md) (services, timing, control flow,
display and sound modes) and [scripts/interpreters.md](scripts/interpreters.md)
(every bytecode machine and data-indexed table).

Scale: `make -C decomp all-coverage` counts the 25 distinct images
(SLUS_006.69 repeats SLUS_006.64). It finds 4,436 functions (1,545,260 bytes)
of compiled C, 68 (16,028 bytes) of handwritten assembly and 458 (71,168 bytes)
of PsyQ SDK code, with no remaining assembly or placeholders. Regenerate these
numbers rather than copying them.

## Using the decomp as an oracle

### Checks

Run them in the matching shell (`nix ... develop path:./nix/ghidra#matching`).
Commands and inputs are in [matching.md](matching.md).

| Check | What it establishes |
| --- | --- |
| `make -C decomp all-split all-verify` | every rebuilt image is byte-identical to the user's original (26 targets); a linker-script name inside an image fails it, and so does an own address the link did not relocate; then every address a link takes from another image must agree with that image's rebuilt symbols, and the resident's mode table with the mode overlays' links (`tools/cross_image.py`) |
| `make -C decomp all-coverage` | which class produced each byte: C, sdk, handwritten, asset, included, text_data |
| `make -C decomp all-container` | every packed container (`CONTAINERS` in the target `.mk` files, 14 across both discs) is reproduced from the rebuilt images |
| `python3 tools/matching_ram.py CAPTURE --targets` | which rebuilt images an original RAM capture holds unchanged |
| `python3 -m tools.analysis.<decoder> --sweep` | every shipped script decodes with opcode tables that tests tie to the C |
| `python3 -m unittest discover -s tests` | decoders and build tools against the C, on synthetic inputs |

### What a match proves and what it does not

It proves that this C, built with the qualified GCC 2.6.3/2.7.2/2.7.2-cdk,
maspsx and binutils configuration, yields the original code, data and layout of
every decoded image. It also proves each function's control flow, constants,
the offsets it uses and its calls, as that toolchain compiles them.

It does not prove:

- That names and comments are right. A match proves a switch's case labels, not
  its comments ([matching.md](matching.md), Script instructions). For example,
  `sprite_set_view_matrix` (sprite_vm_draw.c) is commented as copying light settings,
  but it stores the view matrix `sprite_view_matrix` that `sprite_set_draw_matrix` applies to
  sprite positions. Some shared views still disagree (Open questions).
- That the C means the same under another compiler. It relies on GCC 2.x's
  choices for unspecified and undefined behaviour
  ([Portability hazards](#portability-hazards)).
- Anything about timing. The build reproduces code, not the speed of the
  hardware. Battle consumes measured lag
  ([Timing consequences](#timing-consequences-for-a-port)).
- That every image was observed running. The ten retained routes load the
  resident, field, slot39 and ovl3384 exactly; their snapshots add battle,
  ovl2596, ovl2615 and part of mdec. The other 17 targets appear in no capture
  ([matching.md](matching.md), Original-environment smoke check).

### Keeping the reference while porting

- `make -C decomp all-verify` must stay at 26/26 after any change to
  `decomp/src` or `decomp/include`. Public CI runs the host build checks and the
  toolchain smoke test, not `all-verify` (`.github/workflows/ci.yml`), because
  the comparison needs the user's discs, so the gate is local.
- A cheap guard that needs no discs: the build pipes `psx-cpp` into `psx-cc1`
  with per-unit flags recorded in `<unit>.cflags` (`cc1_of`, decomp/Makefile).
  No source uses `__LINE__`, `__FILE__`, `__DATE__`, `__TIME__` or `__COUNTER__`.
  So when a unit's preprocessed text and flags are unchanged, and so are the
  authored `.s` files and `decomp/include/macro.inc` it includes, so is its
  object, given the same split and original images. The assembler reads those
  files through `.include` (`INCLUDE_ASM` and the end of
  `decomp/include/include_asm.h`), so their contents are not in the
  preprocessed text.
- Put native adaptations under a macro that the PS1 build never defines. The PS1
  build defines `__psx__` and `_PSYQ` (decomp/Makefile `CPPFLAGS`). Do not reuse
  `NON_MATCHING`: the coverage report counts such code as nonmatching drafts.
  Keep native-only headers (GTE, canonical prototypes, thunks) off the PS1
  include path. The coverage report rejects any asm statement that is not in
  `ORIGINAL_ASM` (tools/matching_coverage.py).
- Header edits can change PS1 code without touching a function body. Declaration
  order places uninitialized variables, and a prototype changes argument
  promotion and `$v0` liveness ([matching.md](matching.md), Recovering data;
  When one instruction will not move).
- Differential tests compare the native runtime with original runs, recorded by
  the tools in `tools/reference` (its README.md) from routes such as those in
  `tests/reference-inputs/`; captures stay local under `.local/scenarios`.
  Compare at dispatch boundaries ([Mode dispatcher](#mode-dispatcher)), where no
  stack state exists.

### Reference assets outside the decomp

| Asset | Gives | Where |
| --- | --- | --- |
| Host reconstruction | C++ models of recovered paths and services at original addresses, with synthetic tests; unrecovered paths stop with `MissingDependency`; a reference, not the implementation | `src/reconstruction`, [executable-reconstruction.md](executable-reconstruction.md) |
| Software GTE | the COP2 commands with MAC overflow and FLAG saturation, from general hardware documentation | `src/reconstruction/gte.cpp` |
| libgpu command stream | clear/load/store/move image, packet draws and GP1 control as a command list | `src/reconstruction/gpu_queue.cpp`, `include/xem/reconstruction/gpu.hpp` |
| Disc read and stream models | the resident read path and its CD interrupt side | `src/reconstruction/disc_read.cpp`, `disc_stream.cpp` |
| Card BIOS interface and replay double | the card service surface | `include/xem/reconstruction/menu_overlay.hpp` (`CardBios`), `src/analysis/recorded_card.hpp` |
| MDEC VLC decoder | libpress's variable-length stage only, with no IDCT or colour conversion | `src/analysis/mdec_codec.hpp` |
| GPU packet walker | decodes ordering tables and GP0 commands in RAM images | `tools/analysis/gpu_packets.py` |
| Service census | SDK calls, SPU and scratchpad use per function | `tools/service_calls.py` ([original-boundaries.md](original-boundaries.md)) |
| Data ownership | which unit forms each data address | `tools/data_users.py` |
| Save serialisation check | the save payload against a card image a capture wrote | `tools/analysis/menu_save_file.py` |

## Program architecture

### Images and their numbers

[original-boundaries.md](original-boundaries.md) (Control flow and state) gives
the mode table `mode_table` with each mode's entry, BSS, overlay file and the
word at 0x8006FAF0 that identifies the loaded image, and the secondary overlay
slots. Two more numberings name the same images: the kernel menu's labels
(`mode_kernel_menu_update`, main.c) and the disc slots (`OVERLAYS`,
tools/extraction/overlays.py).

| Mode | Kernel menu label | Target | Disc 1 / 2 slot |
| --- | --- | --- | --- |
| 0 | (the kernel menu itself) | resident `mode_run_kernel_menu` | none |
| 1 | Field | `field` | 36 / 31 |
| 2 | Battle | `battle`, entered via resident `mode_run_battle` | 38 / 33 |
| 3 | Worldmap | `worldmap` | 37 / 32 |
| 4 | Battling | `menu` (the Battling arena, not the game menu) | 35 / 30 |
| 5 | Menu | resident `mode_run_menu` and the 0x801C5000 tenants `slot39`, `ovl2598`, `ovl2600`, `ovl2601`, `ovl2602` | 39 / 34, not loaded |
| 6 | Movie | `movie` (with `mdec` at 0x801D3000) | 40 / 35 |

Every overlay has a disc 2 slot five lower (tools/extraction/overlays.py). Slot
39 holds a packed copy of the `slot39` image; the game loads its menu screens
from directory (0x10, 0), file kind + 5 (`field_run_menu`, `menu_state_run_screen`). The
two discs carry byte-identical overlays; only the disc index embedded in the
resident differs.

### Boot

From `boot_main` (main.c):

1. The handwritten entry `boot_entry_point` clears the resident BSS
   0x800592BC-0x8006FAEC, then `boot_reset_stack_and_gp` sets `$sp` = `$fp` = 0x80200000
   and `$gp` = `_gp`.
2. Library start-up: `ResetCallback`, `SetVideoMode(0)` (NTSC), `ResetGraph`,
   a 384x480 VRAM clear, `InitGeom`; pads (`pad_start_controllers`); `InitCARD(1)`,
   `StartCARD`, `_bu_init`; the vblank handler `pad_vblank_callback`.
3. The heap over [0x8006FAF0, 0x801FC000) (`heap_init`), then `SpuInit`.
4. The disc index: `cd_init_disc_access(cd_disc_files, cd_disc_directories, mode_disc_mode)` with the
   tables embedded in the executable.
5. The sound driver and its 240 Hz tick (`sound_start_driver`).
6. Directory (0, 1) files 2-5 (sound banks), 6 (font, `text_install_font`) and
   7 (system data, `text_install_system_data`).
7. Cross-mode words reset (`mode_reset_game_state`). New-game data loaded:
   `mode_init_game_data` calls `mode_load_initial_game_data`, which copies directory (0x10, 0)
   file 3 whole into `game_data`.
8. The boot logo (`boot_show_logo`), then mode 6 with the movie request
   `cd_movie_request_kind` = {kind 1, entry 0x10 when the directory reports disc 1, else 7,
   next mode 1, 0}. The movie mode reads it in `func_800737EC` (movie.c) and then
   selects the field.

The only outside inputs are the boot word `mode_disc_mode`, the disc index, pads and
cards. The boot word is -1 in both retail executables
(`decomp/targets/resident/classification.txt`, range 80010000). Any other value
switches on the development paths, such as the debug595 load to 0x80280000
(field.c `field_main`) and the debugger breaks. A pointer value also selects
the PC file server (`cd_init_disc_access`). `mode_disc_mode_pointer` points at the word
(kernel_settings.c).

### Mode dispatcher

`mode_dispatch` (main.c) runs each mode and never returns. Its steps, the
never-returning sites that call it from deep frames and what survives it are
in [original-boundaries.md](original-boundaries.md) (Control flow and state).
Three details matter for a native loop:

- The heap restarts twice: at the row's BSS end + 0x800 before the overlay is
  decoded (`heap_move_start`, which releases every block not marked keep), and at
  BSS end + 4 after the stack reset, when the allocation defaults and the pad
  queue are reset too.
- The packed overlay file is read into a top-of-heap block with tag 6, quietly
  (`mode_load_overlay_block`). The field's encounter draw, the world map and the battle
  results call it for the next mode before they leave (field.c `field_encounter_count_down`,
  worldmap.c `func_80070CFC`, ovl2596.c `func_801E252C`), so the read overlaps
  their last frames; the dispatcher waits for it (`cd_sync_reads(0)`) and
  decodes the block to 0x8006FAF0. A block read ahead like this has already
  been released by the first heap restart, so the decode reads a released
  block: a native heap must not scrub released blocks. Selecting mode 0 before
  `entry()` ends the cache (`mode_select_next_mode`). The overlay's `.data` is decoded
  afresh on every entry.
- No game stack frame survives the call. That makes the dispatch boundary the
  natural snapshot keyframe and the place for the host's top-level loop.

### Frames and yield points

- Each mode runs its own blocking frame loops, nested inside one another. The
  field loop `field_main` runs the menu (`field_run_menu` calls
  `mode_run_menu`), movies (`field_movie_play`) and transitions from inside its
  loop. Battle rules call the frame function from inside turn logic
  (`battle_wait_frame`, battle_turns_and_hud.c). There is no draw-free update in any mode
  ([Presentation](#presentation)).
- Waits are `VSync` (75 functions), `DrawSync` (159), and disc, SPU and card
  polls that spin without `VSync`. The census is in
  [original-boundaries.md](original-boundaries.md) (Timing).
- Recommended shape: run the recovered C on a game fiber that yields at every
  `VSync`, `DrawSync` and poll. One virtual clock delivers vblank work, sound
  ticks (240 Hz, about 4.01 per NTSC vblank) and disc, SPU and MDEC completions
  in a recorded order. This keeps the C authoritative; turning it into a
  `step()` function would mean rewriting it. In WebAssembly the fiber needs
  Asyncify, JSPI or stack switching. The arena coroutine below needs a second,
  nested fiber.

### Task systems and schedulers

| Scheduler | Where | State | Runs |
| --- | --- | --- | --- |
| Sprite engine tasks | resident sprite.c (`task_link_draw_node` links, `task_alloc_draw_task` allocates); `Task` in resident/sprite.h | heap nodes with `update`/`destroy` function pointers, lists `task_main_list` (main) and `task_draw_list` (second) | `task_run_draw_list` runs the second list, `task_run_main_list` the main list (paused while `task_main_pause_timer` counts down); battle reruns the main list for measured lag (`battle_run_frame`) |
| Field event VM | field_event.c `field_event_run_all_actors` schedules, `field_event_run_instructions` interprets | per actor: u16 PCs into the map's bytecode, 8 slots | each frame in the move phase of `field_run_frame` ([field-events.md](scripts/field-events.md)) |
| Battle event VM | ovl3087 `func_801E879C` | 16 threads | at battle start and between turns; each pass runs one battle frame per thread (`battle_wait_frame`), then up to four of its instructions, until opcode 22 ends the run: an outer blocking loop around battle frames ([battle-event-vm.md](scripts/battle-event-vm.md)) |
| World map actors | worldmap_80094A5C.c `func_80097800` | 64 slots `D_8009BE24` whose `kind` and `update` hold function addresses as integers | each world map frame |
| Arena coroutine | created by menu7.c `func_8008BA2C` on a 0x400-word stack at 0x801FE000 (menu6.c `func_80088E90`); `func_8008BB3C` resumes and `func_8008BC04` yields (handwritten) | the task's registers and stack | resumed once per arena frame; yields at 7 sites (menu2.c 1, menu5.c 6) |
| Sound sequencer | sound.c `sound_seq_interpret_channels`, called by the tick `sound_run_tick` | sequence channels in data | 240 Hz on root counter 2 ([sound-sequence.md](scripts/sound-sequence.md)) |

### Heap

- Every block has an 8-byte `HeapHeader` {next, caller:21, tag:4, keep:1,
  kind:6} (resident/heap.h). `heap_alloc(size, mode)` takes the first fit
  (mode 0), the smallest fit (2) or carves from the top (1). Exhaustion is fatal
  (error 0x82) unless quiet mode is on (`heap_set_quiet_failures`); then it returns NULL.
- Owner tags select the blocks a subsystem releases (`heap_free_tag`).
  `heap_protect_block` keeps a block across heap restarts. `heap_delay_free` defers a
  release by frames, and `heap_update_delayed_frees` drains the delayed releases once per
  frame.
- Secondary overlays are placed by allocating a 4-byte marker at the top and
  then a top block that reaches the slot address (`menu_state_run_screen`,
  mode_battle_and_menu.c). Placement, capacity and quiet-failure fallbacks are
  therefore heap state. A port that changes allocation order or sizes changes
  later addresses and which allocations fail.
- The `caller` field holds the allocating return address (`GET_RA`, heap.c),
  which only the heap report reads. Natively use a call-site id.

### Memory map

| Range | Holds | Source |
| --- | --- | --- |
| not loaded | the PS-X EXE header, file offset 0-0x7FF; the vram 0x8000F800 the split gives it is a splat convenience that nothing addresses | `decomp/targets/resident/slus_006.64.yaml` |
| 0x80010000-0x80018080 | embedded disc data: boot word, file index, directory table; the only bytes that differ between the two executables | resident `classification.txt` |
| 0x80018080-0x80019524 | resident rodata, including the mode table `mode_table` | main.c |
| 0x80019524-0x8003F8E8 | game code (the entry point first) | resident `classification.txt` |
| 0x8003F8E8-0x8004EAA0 | PsyQ SDK code | resident `classification.txt` |
| 0x8004EAA0-0x80059170 | `.data`, SDK data from 0x800563F0 | resident `classification.txt` |
| 0x80059170-0x800592BC | `.sdata`, `_gp` = 0x80059170 | [matching.md](matching.md) |
| 0x800592BC-0x8006FAF0 | resident BSS, including the game data 0x8006D634-0x8006F98C | `decomp/targets/resident/link.ld` |
| 0x8006FAF0-0x801FC000 | the mode overlay image, its BSS (mode table), then the heap up to 0x801FC000 | main.c `mode_table`, `boot_main` |
| 0x801C5000-0x801EA908 | secondary overlays, placed inside the heap (extents below) | [original-boundaries.md](original-boundaries.md) (Control flow and state) |
| 0x801FC000-0x80200000 | battle modules from 0x801FC000 (the largest, ovl3387, ends at 0x801FCE4C); the stack, top 0x80200000; the arena task stack 0x801FE000-0x801FF000 | `boot_reset_stack_and_gp`, menu6.c `func_80088E90` |
| 0x80200000- | development-kit RAM: debug595 at 0x80280000-0x802861C8, debug2611 at 0x80280000-0x802820F0 | [original-boundaries.md](original-boundaries.md) |
| 0x1F800000-0x1F8003FF | scratchpad: work memory and a temporary stack | [original-boundaries.md](original-boundaries.md) |
| 0x1F801C00 | SPU registers, written through `sound_spu_registers` | resident/sound.h |

The secondary overlays' extents, from the sizes of the images (the rebuilt
images equal the decoded originals, and none has BSS past its image in its link
map):

| Slot | Tenants and where they end |
| --- | --- |
| 0x801C5000 | slot39 0x801EA908; ovl2598 0x801CB59C, ovl2600 0x801CC134, ovl2601 0x801D2264, ovl2602 0x801D90A0 |
| 0x801D3000 | mdec 0x801E8A1C |
| 0x801DC000 | ovl2143 0x801E86B4 |
| 0x801DE000 | ovl2596 0x801E4500 |
| 0x801E0000 | ovl2606 0x801E1DDC |
| 0x801E4000 | ovl2615 0x801E96C0 |
| 0x801E5000 | ovl3087 0x801E9C3C |

Images of different slots overlap (ovl2615 and ovl3087, for example), so
loading one can overwrite part of another slot's image.

A snapshot holds 2 MB of RAM, 1 KB of scratchpad, 1 MB of VRAM and 512 KB of SPU
RAM, and the device state outside them: GTE registers (the handwritten
renderers project with the rotation, translation and projection their callers
left there, as `func_8009980C.s` states), GPU drawing and display settings, SPU
voice and control registers (written through `sound_spu_registers`), the CD drive's
position, mode and any command or read in progress, root counters (the sound
tick runs on counter 2) and pending interrupts, and the memory cards' contents.

### The resident API

The shared headers under `decomp/include` hold one definition per object that
several targets use ([matching.md](matching.md), Converting a function). They are
the starting schema for introspection and snapshots. Where a target still
declares its own partial view of a shared object, the shared header is the
schema, not the view.

| Header | Subsystem | Key entries and state |
| --- | --- | --- |
| `psyq/*.h` | SDK types and prototypes; unnamed SDK members under their `func_` names | libgpu, libgte, libcd, libspu, libapi, libetc, libpress, libsn, `inline_c.h` (GTE macros) |
| `resident/mode.h` | start-up, dispatcher, cross-mode words | `ModeEntry mode_table[]`, `mode_dispatch`, `mode_select_next_mode`, `mode_unread_play_record_word`-`mode_shared_wave_bank_needs_reload` (reset by `mode_reset_game_state`) |
| `resident/gamedata.h` | the saved game data | `GameData game_data` (0x2358 bytes, with `LAYOUT_CHECK`s) |
| `resident/heap.h` | heap | `HeapHeader`, `heap_alloc`, `heap_free`, `heap_delay_free` |
| `resident/cd.h` | disc index, file reads, CD state, PC file server | `cd_select_directory`, `cd_get_file_size`, `cd_read_file`, `cd_read_file_list`, `cd_get_pending_read_count`, `cd_pc_file_names` |
| `resident/stream.h` | disc stream ring, image streams | `StreamRing`, `stream_create_ring`, `stream_load_image_strip` |
| `resident/pad.h` | controllers | `PadBuffer pad_receive_buffers[]`, held/pressed/repeat words, `pad_dequeue_state` |
| `resident/sound.h` | sound driver, SPU voices, banks, sequences | `SpuRegs` through `sound_spu_registers`, driver state `sound_volumes` |
| `resident/gpu.h` | texture scroll, panorama, OT link helpers | `gpu_update_texture_scroll`, `gpu_draw_panorama`, `gpu_ot_link_poly_g4` |
| `resident/model.h` | model renderer | `ModelGroup`, `SpriteModel`, `model_primitive_types`, `model_draw_sprite_model` |
| `resident/sprite.h` | sprite engine and tasks | `Task`, `Sprite`, task lists |
| `resident/text.h`, `resident/window.h` | message text and windows | font and system data resources, `Window` |
| `resident/menu.h` | resident side of mode 5 | `MenuState menu_state_current`, the 0x801C5000 entries |
| `resident/console.h` | debug console, report printf | `console_printf`, `console_report_printf` |
| `battle/area.h`, `battle/work.h` | battle area `battle_area` and work area `battle_work_area` | `BattleArea`, `BattleWork`, `Combatant` |

Some resident functions are declared differently by callers in other targets:
narrow parameters or results, another parameter count, or a by-value structure
split another way. The shared headers leave these out. The resident keeps its
own declarations in `decomp/src/resident/own_declarations.h`, and each target
declares its own. For example, `sound_sync_transfer` returns `s32` in the resident,
`s16` in battle/resident_views.h and `void` in field/field_resident.h. A single
native link needs one canonical prototype per function, plus adapters where
callers depend on narrowing.

### Authoritative state

| State | Where | Notes |
| --- | --- | --- |
| Game data | `game_data`, resident/gamedata.h | pointer-free; new game copies (0x10, 0) file 3 whole (`mode_load_initial_game_data`); save layout in [original-boundaries.md](original-boundaries.md) |
| Cross-mode words and flags | `mode_unread_play_record_word`-`mode_shared_wave_bank_needs_reload` (mode.h), small data 0x80059170-0x800591B8 (kernel_settings.c, sprite_settings.c) | map, music, battle module and entry requests |
| Field state across battle and the arena | `mode_snapshot_block` (0x22FC-byte resident common): field `field_save_snapshot` writes it on those exits (`field_main`), `field_restore_snapshot` restores it; the world map parks its actors there (`func_80075460`) | holds raw actor pointers and world map function addresses |
| Field event variables | `field_event_variables[0x400]`: the lower half comes from game data `vars` at map load (`field_reset_state`) and returns each frame (`field_event_save_map_and_variables`); the upper half is per map | game data is stale until the frame ends |
| Battle party | `BattleWork battle_work_area` (battle/work.h): ovl2615 `func_801E5384` copies the party in, ovl2596 `func_801E2888` writes it back | game data is stale during battle |
| RNG seed | `libc_rand_seed`: libc `rand` (8003fa38) computes `seed = seed * 0x41C64E6D + 0x3039` and returns `(seed >> 16) & 0x7FFF` | cleared with the BSS at boot and soft reset; `srand` has no caller in `decomp/src` |
| Clocks | `pad_vblank_count` (vblank count, the saved play time) and the vblank h:m:s clock | the field copies the clock into event variables 0xC/0xE each frame (`field_update_play_record`) |
| Draw-buffer parity | field `field_draw_buffer_index` | gates field exits (Presentation) |
| Patched renderer fields | `model_set_envmap_mapping` rewrites shifts and offsets inside `model_draw_ft3_envmap` | persists across modes |
| VRAM, SPU RAM, GPU/CD/SPU driver state | hardware | survive mode changes; the saved screen at VRAM (0x2C0, 0x100) carries across modes |

## Boundaries a port replaces

| Boundary | Original interface | Documented in | Port notes |
| --- | --- | --- | --- |
| Disc and files | resident cd.h API over libcd; world map stream reader; mdec `St*` ring; disc swap in slot39 | [below](#disc-and-files); [original-boundaries.md](original-boundaries.md#disc-and-files) | cut at the resident API plus the direct libcd users listed below, over raw sectors; key files by (directory, file) and content hash from the index, not by slot, and serve streams and raw reads by LBA ([Importing the discs](#importing-the-discs)) |
| Controllers | `PadBuffer pad_receive_buffers`, vblank handler `pad_vblank_callback`, 16-entry queue | [original-boundaries.md](original-boundaries.md), Controllers | inject one pad state per vblank; a disconnected pad starts the game's pause loops |
| Memory card and saves | BIOS `bu00:`/`bu10:` file calls, card events, slot39 save/load | Memory card and saves | the commit point is the rename of `__tmp_file`; names for slots 10-14 end in `:;<=>` |
| Game data layout | resident/gamedata.h; payload in slot39/file.h `SaveData` | Memory card and saves | a load finishes in field scripts (event variables 0x46 and 4); copying game data directly is not a load |
| Sound and SPU | sound.c driver, tick `sound_run_tick`, register writes through `sound_spu_registers` | Sound output modes; [sound-sequence.md](scripts/sound-sequence.md) | software SPU (ADPCM, ADSR, reverb, CD input); the driver tick stays in the simulation |
| GPU and VRAM | libgpu, ordering tables, VRAM transfers, readbacks | Presentation | readable 1024x512 VRAM; framebuffer feedback ([Presentation](#presentation)) |
| Timing and pacing | `VSync`, root counters, `DrawSync`, polls | Timing | virtual clock; [consequences below](#timing-consequences-for-a-port) |
| Interrupt work | vblank, sound tick, SPU, CD, MDEC, DrawSync callbacks, card events | Interrupt-context work | fixed delivery points, recorded for replay |
| Control flow | entry, stack reset, dispatcher, mode exits, soft reset, arena coroutine, self-modifying patcher | Control flow and state | host loop with a non-local mode exit; nested fiber |
| BIOS Kanji ROM | `Krom2RawAdd` (save titles; the staff roll, which only the unused field ext `be` enables) | Services, BIOS Kanji ROM | needs the user's BIOS font or a labelled substitute |
| Movies | mdec `St*` streaming, libpress, MDEC DMA callback | Interrupt-context work; Presentation | software MDEC; logical movie clock without output |

### Disc and files

[original-boundaries.md](original-boundaries.md#disc-and-files) describes the
disc service: the index and directory tables, the drive's users (the resident
disc unit behind resident/cd.h, the world map's terrain reader, the movie
library's `St*` ring, the swap's disc check and the movie overlay's development
tools), the read entries and their callbacks, the drive commands and retries,
the streams, the disc swap and the development PC file server. What a port
takes from it:

- **Serve every user from sectors.** Back the resident API, the world map's
  request lists, the movie ring and the swap's disc check with the imported
  discs, and the raw sector reads with them: the arena's portraits and the
  swap's label and index ([Importing the discs](#importing-the-discs)).
- **Report counts, not only busy.** `cd_get_pending_read_count` counts the files of a list
  still to come, and the world map goes on once fewer than three are left
  (`while (cd_get_pending_read_count() >= 3)`, eleven sites).
- **Present the swap.** The drive shows the sequence that section gives for
  `func_801E93A0`: the lid opening and closing, a running motor, GetTN, Setloc
  and SeekL, the label at sector 0x17 and the other disc's index and directory
  at sectors 0x18 and 0x28. RAM is kept.
- **The host-file path is an original seam, with gaps.** It finishes sizes,
  whole files, file lists and image streams inside the call, reads a ring
  stream a sector per poll and refuses raw sector reads (`cd_read_raw_sectors`
  returns -1). A port that backs these calls with its asset store must still
  serve raw reads by LBA (the arena) and the ring streams sector by sector.
  Reads that end inside the call also change how many frames run during a load
  ([Timing consequences](#timing-consequences-for-a-port)).

### Importing the discs

Import each disc as its raw MODE2/2352 track, the form `chdman extractcd`
writes and the extractors read (`SECTOR = 2352` in
`tools/extraction/disc_files.py` and `tools/extraction/overlays.py`;
[matching.md](matching.md), Qualified configuration and targets). Keep whole
sectors addressable by LBA, each with its header, subheader and payload, Form 1
or Form 2. An import of 2048-byte file data cannot serve:

- the resident and world-map sector callbacks. Reads run in Setmode 0xA0
  (`cd_init_disc_access` sets it at start-up), whose bit 0x20 delivers whole
  0x924-byte sectors (psx-spx). Each callback copies the 4-byte header and the
  8-byte subheader (`CdGetSector(..., 3)`) before the 2048 data bytes, and drops
  a sector whose header position is not the one it expects (`CdPosToInt` in
  cd_reads_and_streams.c `cd_copy_list_sector` and `cd_copy_file_sector`, worldmap_80094A5C.c
  `func_80096C0C`);
- movies. Their sizes in the index count 2336 bytes per sector, the subheader
  and the rest of the raw sector, and their sectors hold video, XA audio or
  nothing (`tools/extraction/code_census.py`). mdec reads the video through the
  `St*` ring and lets the drive play the XA audio into the SPU's CD input,
  filtered to file 1 and the movie's channel (`movie_start`). The original's
  host-file path keeps these records too: it seeks a movie by 0x920 bytes per
  sector (mdec.c `movie_restart`), reads each record's 8-byte subheader and
  skips a record whose file byte is 1 (cd_reads_and_streams.c `stream_get_next_movie_frame`). Decode
  XA ADPCM from the Form 2 audio sectors that the file and channel select;
- raw reads by LBA: the arena's portraits, and the swap's label, index and
  directory at sectors 0x17, 0x18 and 0x28.

Derive the (directory, file) keys and content hashes from the disc's own index
(`cd_select_directory`, `cd_get_file_sector`; `tools/analysis/disc_index.py`), but serve
streams, header checks and raw reads at sector level.

### Timing consequences for a port

[original-boundaries.md](original-boundaries.md#timing) gives each mode's
pacing, the battle catch-up and what consumes it, the interrupt work and the
waits. What follows for a port:

- **Lag is battle state.** The battle frame measures its overrun, `task_catch_up_frame_count`
  (0-4), and the next frame reruns the camera step, the main sprite-task list
  and the battle step that many more times (`battle_run_frame`). `frameTicks`
  (`battle_frame_ticks`) also scales the stage steps and the image animations of every
  drawn object (`battle_draw_object`) and of the stage object (`battle_draw_stage`),
  both through `battle_step_image_anim` (battle_scene.c). The rerun list holds the
  sprite tasks (`sprite_task_update`), whose scripts run through `sprite_vm_run`,
  and their command `c1` draws from `rand()` (sprite.c `sprite_vm_run_generic_command`), so lag
  also moves the random stream. A lag-free port diverges from a lagging original
  unless it replays the recorded per-frame overrun
  ([Open questions](#open-questions)).
- **Frames run while the disc reads.** Battle runs frames until the disc is
  idle (battle_flow.c `battle_wait_for_disc`), the field does so before a movie
  (field_screen.c `field_movie_wait_disc_idle`), the field's frame feeds its music ring,
  and the dispatcher waits for the overlay read its mode started earlier
  ([Mode dispatcher](#mode-dispatcher)). A disc service that completes reads at
  once runs fewer of these frames than the drive did.
- **A headless runtime must run the sound driver.** Field event `fe 64`
  (`field_event_wait_sound_channels`) and battle event opcode 48 (battle `battle_play_sound_to_end`) wait
  for sound effects to end, and boot, the field's music loader and battle wait
  for SPU transfers (`sound_sync_transfer`). Without the 240 Hz tick and the transfer
  callback they never return.
- **One wait is not a wait.** The world map's `while (sound_driver_flags & 0x10) {}`
  (`func_80072238`) compiled to one test and a branch to itself (0x80072584): if
  a transfer is still running there, it hangs. Code the intended behaviour
  explicitly; a modern compiler may re-read the flag, keep the hang or delete
  the loop.
- **Sprite and script time follow the field's frame.** Sprite scripts tick
  `sprite_frame_skip + 1` = 2 times per update in the field and the world map, a field
  frame lasts 2, 3 or 4 vblanks (event `d6`), and the field timer in event
  variable 0xA steps every 31 field frames (`field_update_play_record`), not per vblank.
- **The sound tick excludes the main loop** through `DisableEvent` on its event
  `sound_tick_event` around driver updates (console_and_sound_driver.c, sound.c). A port that
  runs the tick on another thread must keep each such section exclusive with
  it.

## Handwritten assembly and SDK contracts

This handbook recommends, as a proposal for Phase 2 rather than a recorded
decision, that the runtime reimplement the handwritten routines and the SDK
natively, against their documented contracts, rather than emulating or
transliterating them. That follows plan.md: the recovered C stays the
authoritative game implementation with narrow, source-correlated adapters
(Shared native/browser architecture), and no CPU emulator ships (Phase 2). The
handwritten code is about 1% of all function code. Every routine has a reviewed
`.s` file whose header comment states its inputs, outputs and quirks. Ported
routines cannot be byte-matched, so they need behavioural differential tests
(packets, OT, counters, GTE state) against captures.

### Handwritten routines

These are the 15 `handwritten` lines of `decomp/targets/resident/classification.txt`
and `decomp/targets/overlays/*.classification.txt`, in 14 rows (the menu's
task-switch storage shares its routines' row):

| Range | Routines | Contract | Native form |
| --- | --- | --- | --- |
| resident 80019524-80019578 | entry, stack reset, word clear | `boot_entry_point.s`, `boot_reset_stack_and_gp.s`, `boot_clear_bss_range.s` | runtime service: start-up, dispatcher loop with a non-local mode exit, per-mode BSS clear from the link; soft reset keeps modified `.data` |
| resident 80026f44-8002709c | GTE pixel darken and blend loops | `sprite_darken_pixels.s`, `sprite_blend_pixels.s` | portable C on the software GTE |
| resident 8002e010-8003014c | model primitive renderers, with alternate entries per packet format, run through `model_primitive_types` | `model_draw.s`, `model_depth.s`, each `func_8002*.s` | portable C, and the capture point for pre-projection geometry; keep their culling separable |
| resident 80030750-80030a30 | environment-mapped renderer and its self-modifying patcher `model_set_envmap_mapping` | `model_draw_ft3_envmap.s`, `model_set_envmap_mapping.s` | four persistent globals (u/v shift and offset, image default 6, 6, 0x40, 0x40) |
| resident 800315a0-80031894 | ordering-table link helpers | `ot_link.s` | the OT adapter |
| resident 80032e88-80032f54 | LZSS decoder and its allocate-and-decode entry | `text_unpack_lzss.s`, `text_unpack_lzss_alloc.s` | portable C with bounds; `tools/analysis/packed.py` and `tools/packed_container.py` are the format |
| resident 800379b4-800379d0 | arena task index store; report printf tail-jump | `mode_set_arena_task.s`, `console_report_printf.s` | C with a `va_list` printf |
| resident 8003f738-8003f8e8 | rotation matrix and sin/cos lookups (table `rcossin_tbl`, main3.c) | `gpu_build_rotation_matrix.s`, `gpu_get_sin.s`, `gpu_get_cos.s` | portable C with 32-bit wrapping |
| menu 80072d18-800732cc | ground triangle batch; GTE vector, colour and matrix helpers | `func_80072D18.s`, `ground_packet.s`, `vector_scale.s`, `func_8007313C.s`, `func_800731F8.s`, `func_800732AC.s` | portable C on the software GTE |
| menu 8008bb00-8008bcc8, storage 80096d88 | task context switch (resume, yield, nested-scheduler save and restore) | `func_8008BB3C.s`, `func_8008BC04.s`, `func_8008BB00.s`, `func_8008BB1C.s`; `TaskContext` in menu/task.h | the nested fiber service |
| menu 8008c3a8-8008c7c0 | mesh shadow projection, flat packet builders | `func_8008C3A8.s`, `func_8008C4B0.s`, `func_8008C620.s`, `mesh_packet.s` | portable C |
| menu 8008ddfc-8008df30 | GTE vector transform; a stub that never restores `$sp` | `func_8008DDFC.s`, `func_8008DE54.s` | port the transform; the stub has no reference in `decomp/src` |
| worldmap 8009980c-80099bfc | terrain quarter-block renderer | `func_8009980C.s`, `screen_bounds.s` | portable C; the terrain's pre-projection seam |
| worldmap 80099bfc-80099e8c | terrain billboards | `func_80099BFC.s` | portable C |

### SDK libraries

The SDK is classified, not decompiled ([matching.md](matching.md); the library
census is in [original-boundaries.md](original-boundaries.md), Services).

| Library | Role for the game | Native approach |
| --- | --- | --- |
| libgpu, submission and VRAM (`DrawOTag`, `Load/Store/Move/ClearImage`, environments) | presentation | native GPU service that walks the 24-bit ordering tables over a readable VRAM model |
| libgpu, packet helpers (`AddPrim`, `GetTPage`, `SetPoly*`, TIM parsing) | packets in game memory | portable C keeping the 32-bit packet layout |
| libgte | gameplay and rendering math | portable C on one bit-exact software GTE shared with the inline macros and the handwritten routines; its square root and arctangent tables are SDK data (resident classification, 800563f0-80059170), so import or regenerate them |
| libcd, mdec `St*` | disc | native disc service over raw sectors ([Disc and files](#disc-and-files), [Importing the discs](#importing-the-discs)) |
| libspu, driver register writes | sound | software SPU, shared by native and browser hosts |
| libetc, libapi events and root counters | time, interrupts | virtual clock and deterministic callback delivery |
| libapi pad, libcard, BIOS file calls | input, cards | per-vblank pad buffers; virtual cards with event completion |
| libc, libc2 | `rand`, `sprintf`, `memcpy`, `bzero` | exact PsyQ behaviour (the `rand` recurrence above), not host libc |
| libpress | movies | software MDEC (VLC, IQ, IDCT, YCbCr to RGB at 15 and 24 bits) |
| libsn | the development PC file server: `PCopen`, `PClseek`, `PCclose`, and `PCinit` and `PCread`, PCinit and PCread by their signatures and callers (psyq/libsn.h) | not needed on the disc path; if a port takes the host-file seam, `PCopen`, `PCread`, `PClseek` and `PCclose` are that seam's interface, and the port still serves raw reads by LBA ([Disc and files](#disc-and-files)) |

### Inline assembly in the C

The original-style asm forms are listed exactly in `ORIGINAL_ASM`
(tools/matching_coverage.py). They are the PsyQ GTE macros, the debugger break and
pollhost, the stack switches, `GET_RA` and `addPrimLen9`. Natively:

- **GTE macros.** One native header maps the macro names onto the software GTE.
  The PS1 build keeps `psyq/inline_c.h` and the few per-target macros it lacks
  or spells otherwise (`battle/gte.h` and `ovl2143/gte.h`: gte_rtv0tr;
  `menu/gte.h`, `worldmap/gte.h`), because their instruction orders are what
  match.
- **Stack switches.** These move `$sp` to the scratchpad or a heap block for a
  few calls (for example field.c `field_run_frame`). They become no-ops, but the
  heap allocation around them stays.
- **Debug breaks and pollhost.** These sit behind development tests (boot word
  or `field_monitor_absent`), with one exception: battle_flow.c `battle_menu_open_turn`
  loops on `break 1` when a battle menu is already open, an assertion with no
  development test around it. Make the guarded ones no-ops and the assertion a
  fatal diagnostic.
- **`GET_RA`** becomes a call-site id. The one register binding (world map
  `func_80086798`) becomes plain variables.

## Assets and script machines

### Script machines

[scripts/interpreters.md](scripts/interpreters.md) lists every bytecode machine
and data-indexed dispatch table, with its document and decoder. At this
revision all ten decoder sweeps exit 0 and report no unknown or undecodable
instruction on either disc. For Phases 7, 11 and 12:

- **One source of opcode facts.** The decoder tables (mnemonic, handler, size,
  flow, operands, effect) are the source for runtime introspection, editor
  disassembly and mod validators. Tests tie them to the C; extend them rather
  than re-deriving them.
- **Decoders only.** No assembler or encoder exists. The only encoder is the
  LZSS packer (`tools/packed_container.py`, which reproduces every container
  exactly).
- **Layout matters to re-encoding.** Field PCs are absolute u16 offsets;
  handlers skip or re-read neighbouring bytes (`9a`, `d4`/`fc`, ext `18`/`c6`,
  `10`/`11`, `57`, `a6`, `12`); a battle AI rule can run into the next enemy's
  table; sprite `be` has a 2-byte length entry for a 3-byte command
  ([field-events.md](scripts/field-events.md),
  [battle-ai.md](scripts/battle-ai.md), [sprite-vm.md](scripts/sprite-vm.md)).
  Editors should preserve layout and append rather than insert.
- **Unchecked indexes.** Interpreters index function tables without range
  checks; `switch` dispatch is bounded
  ([interpreters.md](scripts/interpreters.md)). Validators must reject what the
  original would read out of range.

### Data formats

| Format | Read by | Tool or document | Not yet decoded |
| --- | --- | --- | --- |
| Disc index and directories | resident cd.h | `tools/extraction/disc_files.py`, `tools/analysis/disc_index.py` | none |
| LZSS blocks, offset archives | `text_unpack_lzss`, `text_relocate_offset_table` | `tools/analysis/packed.py`, `tools/packed_container.py` | none |
| Field bundle (nine components) | field.c `field_load_from_bundle`; header `FieldBundle` in field_load.h | `tools/analysis/field.py` (components, event package, collision) | palettes, images, trigger zones and the header's view block (+0x154, lights and background) |
| Field event bytecode | field_event.c | [field-events.md](scripts/field-events.md), `events.py` | see its Open items |
| Messages and text controls | resident main2.c `window_reveal_text` | [text-control.md](scripts/text-control.md), `text_control.py` | the characters of punctuation and two-byte glyphs (its Open line) |
| Models (`ModelGroup`, `SpriteModel`, TMD) | `model_relocate_group`, `model_draw_sprite_model`; battle `battle_tmd_draw_object` | [dispatch-tables.md](scripts/dispatch-tables.md) (primitive census) | no mesh exporter |
| Sprite blocks | resident sprite units | [sprite-vm.md](scripts/sprite-vm.md), `sprite_vm.py` | none |
| Sound: `smds`, `seds`, `wds ` | sound.c, console_and_sound_driver.c | [sound-sequence.md](scripts/sound-sequence.md), `sound_sequence.py` | no ADPCM sample decoder |
| Battle enemy, AI, effect and event files | battle, ovl2615, ovl3087 | [battle-ai.md](scripts/battle-ai.md), [battle-effect-vm.md](scripts/battle-effect-vm.md), [battle-event-vm.md](scripts/battle-event-vm.md) | none |
| Formations and encounter sets | field component 6 into `formation_encounter_set`, world map `D_8009D73C`, battle `battle_main`, ovl2615, ovl3087; `BattleFormation` and `EncounterSet` in resident/formation.h | [formations.md](scripts/formations.md), `formations.py` | see its Open items |
| World map actor and scene scripts, arena scripts | worldmap, menu | [worldmap-actor.md](scripts/worldmap-actor.md), [worldmap-scene.md](scripts/worldmap-scene.md), [arena-scene.md](scripts/arena-scene.md), `overlay_scripts.py` | none |
| Cue timelines: field movie sounds, world map terrain texture animations | field `field_movie_play_due_sounds`; world map `func_80074F2C`, `func_80075104` | [timelines.md](scripts/timelines.md), `overlay_scripts.py` | see its Open item |
| Save files | slot39 `func_801CBD90`, `func_801CB304` | [original-boundaries.md](original-boundaries.md), `menu_save_file.py` | none |
| Movies (STR, XA) | mdec, movie, field `field_movie_play` | VLC only (`src/analysis/mdec_codec.hpp`) | IDCT, colour conversion and XA ADPCM |

### Original bytes the port must import

A distributed or hosted build may not bundle any of these. Load them from the
user's files at runtime, as writable copies where the code patches them in
place.

- **The discs**, as raw sectors ([Importing the discs](#importing-the-discs)).
- **Every `asset` range** of the targets' classifications, not only the
  `INCLUDE_ASSET` blobs. In the resident: the embedded disc data
  0x80010000-0x80018080 (below); the TMD model `model_slot_ring_tmd` (0x170 bytes), a
  splat data segment inside `.text` (`battle_effect_script` in
  `slus_006.64.yaml`) that only the battle overlay reads, for the slot-highlight
  ring; and, through `INCLUDE_ASSET`, the boot logo `boot_packed_logo`, a packed image
  `sprite_packed_pause_image`, the glyph `text_special_glyph_rows` of the character pair 0xFF 0xFF, the
  console font `console_packed_font`, and the error sound banks `sound_error_effect_bank` (used in
  place as a `SoundBank`) and `sound_error_wave_bank`. In the field: the movie sound
  timelines `field_movie_sound_timelines`. In the menu: the sprite model `D_80091FB0` (relocated
  in place by `model_relocate_sprite_model`) and the arena scene and setup scripts. In the
  world map: the terrain texture animation sequences, the actor scripts and the
  scene directors' cue tables ([timelines.md](scripts/timelines.md) and the
  classification lines give each format and reader).
- **`INCLUDE_ORIGINAL`/`INCLUDE_RODATA` objects** (`included` lines). These are
  real variables, tables and strings kept original because stray bytes that
  nothing reads follow them, in their alignment padding or before the next
  unit's data ([matching.md](matching.md), What counts as recovered source),
  or, for the SDK strings, because several library functions share them. A
  port build needs their values imported, or written as C once the stray bytes
  no longer matter.
- **The embedded disc data** of the inserted disc's executable
  (0x80010000-0x80018080: the boot word, index and directory table), the SDK
  data tables (libgte, the libpress VLC tables, the reverb presets) and the BIOS
  Kanji ROM.

## Presentation

[original-boundaries.md](original-boundaries.md) (Presentation) lists display and
draw environments per mode, projection settings, ordering tables, dithering,
semi-transparency, texture windows, framebuffer feedback, the mask bit, 24-bit
movie display and the renderers' culling. That includes the LZCR quirk a port's
GTE must keep. In short, video is NTSC only. Most modes double-buffer 320x224;
the world map uses 320x216, one field map 640x224, arena scenes 640x218, and
movies 320x240 with 24-bit MDEC frames.
[rendering-behavior](../analysis/formats/rendering-behavior.md) records the
observed packets of the Disc 1 forest slice. These facts constrain a renderer as
well:

- **There is no draw-free update.** Drawing passes advance state. In the field
  frame (field.c `field_run_frame`) these are the sprite task lists inside the
  character pass (`field_draw_characters`), the effect slots that spawn particles
  (`field_effect_update_slots`), the fades (`field_fade_update_channels`), the dialogue timers
  (`field_dialogue_close_expired_windows`), the delayed frees (`heap_update_delayed_frees`) and the VRAM upload
  queue (`sprite_queue_run_uploads`). In battle, the stage step that draws the objects
  (`battle_update_stage`) also pushes them apart and animates them. Presentation must
  consume the output of the original frame, never call game draw code per eye,
  per repaint or for a spectator.
- **Projection feeds gameplay.** The field character pass projects every actor
  with the original camera and sets `layer_flags` 0x200 when it falls outside
  x [-39, 360), y [-9, 314) (field.c `field_draw_sprite_actors`). An off-screen actor
  skips turning (`field_run_move_phase`) and the collision, floor and movement steps
  of the move phase (`field_update_events_and_actors`). A window for an off-screen speaker does
  not open (field_motion.c `field_dialogue_open_window`), and an open one closes
  (`field_event_wait_dialogue`), unless the window style's bit 0 is set. Field events `8a`
  and ext `02` branch on whether an actor projects inside 320x224 or inside
  x 33-287, y 33-191 (`field_event_branch_unless_on_screen`, `field_event_branch_unless_well_on_screen`). Keep computing these
  from the original camera and screen, headless too: an actor just outside the
  original screen stops moving even when a wider view shows it.
- **Buffer parity and fades are state.** The field leaves for battle, the world
  map, the arena or another mode, and starts a movie, only when the draw-buffer
  index `field_draw_buffer_index` is 1; it opens the menu only when it is 0. A map change
  waits for the first fade channel to finish (`fades[0].steps == 0`) and for the
  disc (field.c `field_main`).
- **Packets are pre-culled** to the original window (original-boundaries.md,
  GPU features: Culling). World-map terrain also drops triangles whose largest
  SZ is 0xF00 or more and stops at 0x7FE packets (`func_8009980C.s`). Widescreen,
  stereo and VR need a side-effect-free re-traversal of pre-projection data,
  not a reprojection of the packet list.
- **VRAM is persistent, read-back state** (original-boundaries.md, GPU
  features: Framebuffer feedback). The saved-screen slot (0x2C0, 0x100) also
  takes the world map's screen (`MoveImage` in worldmap_80072238.c and its
  scene directors), and the field's kind-3 transition copies it back into the
  draw buffer every frame (field.c `field_run_frame`). A bit-exact 1024x512x16
  VRAM belongs in snapshots; an HD renderer redirects samples of such regions to
  captured render targets.
- **Ordering is more than depth.** The field links its background table into
  the main one at a far cutoff (`unk21D4`). Sprites draw with depth biases and
  split parts (`field_draw_sprite_actors`, sprite.c `sprite_draw_parts`). Model types sort by
  average, farthest or nearest vertex (`model_depth.s`). A plain depth buffer
  breaks these intentional masks.
- **Primitives.** 3D models use the 17 types of `model_primitive_types` (model_renderer.c):
  POLY_F3, FT3, G3, GT3, flat quads, FT4, G4, GT4 and the environment-mapped FT3
  of type 16. Shipped models use 13 of them
  ([dispatch-tables.md](scripts/dispatch-tables.md)). Their packets are built
  once at load (`model_build_packets`). Each frame the renderers rewrite only screen
  coordinates and some colours or UVs, and link the packets (`model_draw.s`).
  Other code builds SPRT, TILE, LINE and the DR_* environment packets inline,
  with the libgpu setters and the OT helpers (`ot_link.s`, resident/gpu.h,
  console.h and window.h).

| Pre-projection seam | Where | Captures |
| --- | --- | --- |
| 3D models of every mode | `model_draw_sprite_model` (model_renderer.c), 11 call sites in battle, field, menu, ovl2143, resident sprites and the world map | mesh, GTE rotation/translation, H, offset, depth cue, sort mode, OT |
| Sprites | `sprite_set_draw_matrix` (position through the view matrix `sprite_view_matrix`), `sprite_draw_parts` (part corners through `RotTransPers4`) | world position, scale, facing, frame parts |
| World map terrain | `func_8009932C` calls `func_80099708`, which calls `func_8009980C.s`; billboards `func_8008615C` call `func_80099BFC.s` | the 9x9 vertex grid on the scratchpad, cell words |
| Cameras | field `FieldView field_view` (look-at `field_camera_build_lookat_matrix`); battle eye/target (`battle_camera_step`); world map camera (`func_80097244`) | read-only inputs to stereo, diorama and VR views; head pose must never write them, because the player control (event `a7`, `field_event_request_player_control`) subtracts the field camera's `angle` from the pad direction |

Text is rasterised on the CPU into the window's 4-bit line image, two glyph
planes per nibble (`text_draw_glyph`, main2.c), and drawn as sprites
(resident/window.h). Intercept text at character level for HD text. Agents read
it from the window state, not from pixels.

### Sound modes

The full arithmetic and recordings are in
[original-boundaries.md](original-boundaries.md) (Sound output modes) and
[sound-modes](../analysis/formats/sound-modes.md):

- Mono changes only the voice pan law.
- Stereo and Wide share the voice values.
- Wide negates the master volume's right side and the reverb depth's left side.
  It mixes no signal into the other channel, so it inverts polarity rather than
  matrix-encoding.
- CD, XA and movie audio stay stereo in every mode; `CdMix` is never called.
- The mode is not saved, and boot (also after a soft reset) restores Stereo.
- The mixer order behind Wide was observed in an emulator only. Verify it on
  hardware before building the Phase 10 decoder.

## Portability hazards

A syntax-only host compile of all 106 recovered C files measures the data-model
and compiler hazards. Run it with clang 21.1.8 from `nix develop path:./nix`,
for each `--target`:

```sh
NIX_HARDENING_ENABLE= clang --target=TRIPLE -fsyntax-only -std=gnu89 -undef -nostdinc \
  -Idecomp/include -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx -D_PSYQ -D_MIPSEL \
  -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C -funsigned-char -fno-wrapv \
  -DORIGINAL_IMAGE='"x"' -DORIGINAL_BASE=0 -ferror-limit=0 decomp/src/TARGET/FILE.c
```

The empty `NIX_HARDENING_ENABLE` stops the shell's compiler wrapper from adding
its hardening flags: `-fwrapv`, which hides the shift diagnostics, and `-fPIC`
and `-fzero-call-used-regs`, which the MSVC and wasm32 targets reject.
`-fno-wrapv` keeps the shift row where another wrapper adds `-fwrapv`.
Diagnostics are counted once per source location. These counts are from this
revision:

| Diagnostic | x86_64-linux-gnu | arm64-apple-macos | x86_64-pc-windows-msvc | wasm32, i686 | Cause |
| --- | --- | --- | --- | --- | --- |
| pointer to integer casts | 677 in 51 files (+30 from `void *`) | same | 792 in 58 files (+30) | 0 | 32-bit addresses kept in integers; on Windows also the casts to PsyQ's 32-bit `u_long` (packet words, the EXE header) |
| integer to pointer casts | 320 (+34 to `void *`) | same | 321 (+34) | 2 | pointers rebuilt from 32-bit words |
| non-constant static initializers | 17 in 3 files | same | 18 in 4 files | 0 | function addresses stored as `s32` (world map `D_80099E8C`; menu3.c, menu4.c); on Windows also header.c's EXE header |
| failed `LAYOUT_CHECK` | 13 in 12 headers (battle/area.h, effect.h, scene.h, ui.h, work.h twice; menu/card.h, panel.h, screen.h, shop.h, tables.h; ovl2143/actors.h; resident/menu.h) | same | 10 in 9 (all but battle/ui.h, menu/panel.h and menu/screen.h) | 0 | asserted offsets of structures with pointers; under LP64 also of the three whose PsyQ packets' `u_long` words widen (they hold with `-Dlong=int`) |
| rejected `section` attributes | 0 | 3 in 2 files | 0 | 0 | Mach-O section names need a segment (main.c `mode_next_mode`, `mode_table`; menu6.c `D_80088BFC`) |
| asm with MIPS register names | 150 in 24 files | same | same | same | GTE macros, stack switches, `GET_RA` |
| non-prototype declarations | 258 in 47 files | same | same | same | K&R definitions and calls |
| incompatible pointer types | 2 in 2 files | same | same | same | one object read through two types: a `VECTOR`'s `long` passed as `s32 *` (field_motion.c `field_actor_move` to `field_actor_is_outside_boundary`), an `s32` buffer as `SaveData *` (slot39.c to `func_801E4D10`) |
| `return;` in a non-void function | 45 in 13 files | same | same | same | implicit-int functions; see below |
| conflicting types | 2 in 2 files | same | same | same | a prototype after a call implicitly declared the function (field_effect.c `field_load_tim_at`), and slot39_801DBE54.c's `memcpy` prototype, which drops the built-in ([matching.md](matching.md)) |
| arrays of incomplete struct type | 34 in 6 files | same | same | same | commons units define their variables before the headers complete the types ([matching.md](matching.md), Recovering data) |
| incompatible function-pointer types | 2 in 2 files | same | same | same | a function stored under another prototype: the variadic report printf `console_report_printf` in the heap report's `void (*)(char *)` output hook (heap_host_report.c), a primitive type's unprototyped `prepare` in a prototyped local (model_renderer.c `model_build_packets`) |
| unsequenced modifications | 6 in 2 files | same | same | same | several `*pc++` in one call's arguments |
| shift of a negative value | 14 in 5 files | same | same | same | `-x << n` |
| array index -1 | 4 (battle.c, through `STEP_FUEL`) | same | same | same | tables read through an offset base |

wasm32 and i686 keep the original's data model, and every layout check holds
there. arm64 macOS is LP64 like x86_64 Linux; Windows is LLP64, where `long`
keeps PsyQ's 32 bits but pointers have 64. Choosing ILP32 everywhere (wasm32,
or ahead-of-time compiled wasm on 64-bit-only hosts) or a 64-bit handle port is
therefore the first Phase 2 decision.

| Hazard | Evidence | Handling |
| --- | --- | --- |
| 32-bit pointers in data | world map actor slots and spawn tables hold function addresses as integers (`func_80097800`, `D_80099E8C`); sprite tasks, sound modulators and hooks hold function pointers; models and archives are relocated in place (`model_relocate_group`, `model_relocate_sprite_model`, `text_relocate_offset_table`); `mode_snapshot_block` parks raw pointers across modes | keep game memory a contiguous 32-bit arena and map code addresses to stable ids; never serialise host pointers |
| 24-bit ordering-table links | `AddPrim` and the OT helpers store packet addresses masked to 0x00FFFFFF; the arena rebuilds pointers as `(tag & 0xFFFFFF) - 0x80000000` (menu7.c `func_8008ACB8`) | links as offsets into the arena, below 16 MB |
| Punned views | the same bytes read through several types: through explicit casts, which the census does not report (the SDK calls' among them: [matching.md](matching.md), Converting a function), and its 2 incompatible pointer types; the `link.ld`, `battle.data.ld`, `menu.bss.ld` and `worldmap.data.ld` views name parts of objects ([matching.md](matching.md)) | `-fno-strict-aliasing`; one canonical type per address for schemas |
| Signedness and width | plain `char` is unsigned (`-D__CHAR_UNSIGNED__`, `lbu`; [matching.md](matching.md), Qualified configuration); `long` is 32 bits in the PsyQ structures (`VECTOR`, `MATRIX`, packet tags); event variables are 16-bit and read signed or unsigned by the map's event package bits (`field_event_read_variable`) | `-funsigned-char`; 32-bit `long` in native SDK headers |
| Unspecified evaluation order | `sprite_set_svector(&angles, rand(), rand(), 0)` (sprite.c, case c1): the matched code draws the first `rand()` into the second argument (0x80020244-0x8002026c); the 6 unsequenced `*pc++` calls in battle_scene.c and ovl2143.c | read operands into locals in the order the matched disassembly shows |
| Values left in `$v0` | `battle_menu_set_acting_slot` is defined `void`, but callers use the acting sprite left in `$v0` (0x800bf094; battle_flow.c); `func_8008FACC` falls off the end (menu7.c); `func_8008B730` returns the header pointer with zero frames (0x8008b770); `sound_alloc_wave_bank_spu_memory` returns its allocator's result implicitly; `sound_switch_modulator_off` sits in the `s32` modulator table, and its value always scales to 0; `func_801E7210` returns the allocator's NULL into `drawEnv.isbg`; the movie test menu stores what `func_80074BA4` and `func_8007519C` leave for an index past the list | return the PS1 value explicitly; check every other `return;` site's callers before declaring it `void` |
| Uninitialized reads | `chance` in `battle_gear_hud_fill` (gear boost, battle_menus_and_resolver.c); `actions` in `battle_run_joint_turns` zeroes 0x100 bytes at an unset pointer; unset `parts` entries in `battle_resolve_attack_value`; the call event (type 8) of ovl2143 `func_801E5D44`; `owner` in ovl3383 `func_801FC020` and ovl3385 `func_801FC508`; `hold` (menu5.c `func_800852C4`), `last` (menu6.c `func_80088E90`); sprite opcodes 40-7f and f7 (resident and battle sprite VMs) | an optimising compiler treats these as undefined; give each an explicit native value and record whether it can diverge (Open questions) |
| Division | units built with maspsx `--expand-div` (ovl2615, mdec, movie, ovl2143, worldmap, resident `cd_reads_and_streams`, battle `battle_scene`; see the `.mk` files) trap on zero with `break 7` and on overflow with `break 6`; other units use bare `div`, which does not trap on the R3000 | a source-correlated division adapter where a zero divisor can occur (`battle_gear_hud_fill` divides by `maxHp / 10`); keep the field script divide `field_event_divide_variable`, which already maps 0 to 1 |
| Signed overflow and shifts | the handwritten trig and renderers rely on 32-bit wrapping; 14 shifts of negative values | `-fwrapv` and arithmetic-shift helpers |
| Overruns and offset bases | `STEP_FUEL` is `&gearHud.commands[-1]` (battle/command.h); `battle.data.ld` names `battle_combo_next_step_table_by_paid` and `battle_ap_timer_reload_table_by_max_ap` before their tables; menu5.c `func_80085EC8` writes `arrows[1][3..5]` past its array | explicit range-checked index arithmetic natively |
| Decompressors read past files | the overlay LZSS decoder reads its final flag byte past the file (tools/extraction/overlays.py); arena model files and map 145's messages read past their bytes ([arena-frame-events.md](scripts/arena-frame-events.md), [text-control.md](scripts/text-control.md)) | zero-pad imported files to whole sectors; bound the decoder |
| Scratchpad | work memory and stack switches ([original-boundaries.md](original-boundaries.md)); `TerrainDrawScratch` in worldmap_80094A5C.c and the layout func_80099BFC.s describes | a 1 KB static buffer behind one accessor |
| Fixed cross-image addresses | the mode table holds overlay entries and BSS bounds as numbers (main.c `mode_table`); slot tenants are called by address (`menu_state_run_screen`); cross-image names come from the original's addresses (splat's `undefined_syms_auto.txt`, `*.resident.ld`, `debug595.field.ld`), outside the strict linker-script check; all-verify's cross-image step compares them, and the mode table, with the rebuilt targets ([matching.md](matching.md)) | a per-slot registry that rejects calls into an absent image; 45 function names are defined by two or more targets' C, so give them per-image namespaces |
| Section placement and asm labels | `__attribute__((section(".rodata")))` on main.c `mode_next_mode` (which `mode_select_next_mode` writes) and `mode_table`, and `section(".text")` on the menu6.c table `D_80088BFC`, place data where the original had it. Mach-O rejects both names (the census above), and in a one-line clang 21.1.8 probe wasm32's code generator refuses data in `.text` ("data symbols must live in a data section"); ELF and COFF accept both. ovl2615's battle_setup.h declares `D_800CCCE8_setup __asm__("battle_work_area")` (movie_mode.h and worldmap.h one such view each): where C names take a leading underscore (Mach-O, 32-bit Windows), that label names another symbol than the definition `battle_work_area` | keep both behind the PS1-only build macro; natively, plain definitions and one name per object |
| K&R calls and per-target prototypes | unprototyped calls pass unpromoted arguments ([matching.md](matching.md)); targets declare shared functions differently (`own_declarations.h`) | canonical prototypes and thunks in native-only headers; in WebAssembly a mismatched indirect call traps |
| Non-volatile polling | `while (sound_driver_flags & 0x10)` was compiled to test once (world map); other polls re-read through calls (`cd_sync_reads` loops on `cd_get_pending_read_count`) | deliver interrupts at yield points inside polls; do not rely on the host compiler re-reading globals |
| GTE fixed point | gameplay calls libgte: field movement and collision use `ratan2` (field_motion.c), as does the arena AI (menu7.c); culling reads GTE registers (LZCR in `model_draw.s`) | one bit-exact software GTE whose state is in snapshots |
| Debug paths | the boot word is tested in the resident, field, battle, arena and menu screens (`mode_disc_mode`, `*mode_disc_mode_pointer`, `*(s32 *)0x80010000`); the field caches the test in `field_monitor_absent` | build with the retail value -1; never leave the word zero |

## Original quirks and bugs a port keeps

The C reproduces these as long as data stays byte-identical and loaded
contiguously. Keep them by default; fixes are opt-in options that are recorded
in replays.

| Quirk | Where | Effect |
| --- | --- | --- |
| The field timer counts 0-60 seconds and never saturates | `field_update_play_record` (`seconds != 0xFF3B` is never false; `> 60` wraps) | 61 seconds per minute; minutes wrap with the u16 variable |
| Sound `EA` ends its pan slide at the difference | sound.c `sound_seq_pan_slide`; [sound-sequence.md](scripts/sound-sequence.md) | the last frame sets pan to `(target - start) << 8` |
| Sprite replay steps `be` by 2 | `sprite_vm_replay_frames`; [sprite-vm.md](scripts/sprite-vm.md) | the interpreters take 3 bytes, so the replay reads `be`'s last byte as the next command |
| Battle 70's targeted script runs into enemy id 1's table | [battle-ai.md](scripts/battle-ai.md) | id 1's turn rule runs |
| Three effect commands count one event short; some name unused or out-of-range animations | [battle-effect-vm.md](scripts/battle-effect-vm.md) | an 8-byte sound event never plays; the animation lookups (800AF518, 801E6910) do not check bounds |
| One random stream for effects and gameplay | `rand` (8003fa38): 228 call sites in 28 files; the battle shatter setup `battle_shatter_cut_shards` draws 3,920 values (2 x 14 x 20 shards, 7 each) | skipping, adding or reordering a visual effect changes later rolls |
| Lag changes battle | `battle_run_frame` | see [Timing consequences](#timing-consequences-for-a-port) |
| Model renderers never reject overflowed faces | `model_draw.s` (reads LZCR, not FLAG) | oversized triangles reach the GPU ([original-boundaries.md](original-boundaries.md)) |
| Soft reset keeps modified `.data` | `boot_restart` jumps to the entry | not equivalent to restarting the process |
| Unreachable content stays off | the staff roll needs field ext `be` (`enable_movie_overlay`), which no shipped script uses ([original-boundaries.md](original-boundaries.md), Services: BIOS Kanji ROM); the field's debug key into map 0 needs the development word ([field-events.md](scripts/field-events.md)) | optional content, not default behaviour |

## Open questions

| Question | Evidence so far | What would settle it |
| --- | --- | --- |
| What the uninitialized reads that can change gameplay compute on hardware | the `battle_gear_hud_fill`, `battle_run_joint_turns`, `battle_resolve_attack_value` and ovl2143 `func_801E5D44` (event type 8) sites above | a targeted capture per site, or a census showing shipped data never reaches the unset path |
| How often battle frames overrun on hardware | `task_catch_up_frame_count` is measured at run time; all captures are emulator runs ([original-boundaries.md](original-boundaries.md)) | per-frame `task_catch_up_frame_count` from captures, or an accepted lag-free definition for Phase 3 parity |
| Conflicting shared views | the battle and slot39 read the ammo bytes as `ammo` and `gearAmmo` and `D_8006F8EA` as the u16 `flags` (resident/gamedata.h; [field-events.md](scripts/field-events.md)), but targets still keep partial views of the game data beside resident/gamedata.h (ovl2615's `game_data_slot_in_gear`, the world map's `game_data_worldmap_flag_word` and the party and vehicle spots in worldmap/party.h) | reconcile from the readers, then one canonical type per address |
| The disc and stream boundary | the code ([Disc and files](#disc-and-files)); no retained capture covers a disc swap, disc 2 media or the CD callback order ([matching.md](matching.md) lists the routes) | a disc-swap and streaming capture on both discs |
| Wide on hardware | emulator recordings only ([original-boundaries.md](original-boundaries.md)) | a hardware recording before Phase 10 |
| World map, Gear battle and arena presentation | no packets observed ([original-boundaries.md](original-boundaries.md)) | `tools/analysis/gpu_packets.py` on captures of those modes |
| Missing media decoders | no IDCT, colour conversion, XA ADPCM or SPU ADPCM in the repository | implementations in Phase 2, tested against captured frames and audio |
| BIOS font source | `Krom2RawAdd` users in slot39 and field | a user BIOS import or a labelled substitute |
| Script data open items | the Open lines of [field-events.md](scripts/field-events.md), [text-control.md](scripts/text-control.md), [battle-ai.md](scripts/battle-ai.md), [battle-effect-vm.md](scripts/battle-effect-vm.md), [battle-event-vm.md](scripts/battle-event-vm.md), [arena-frame-events.md](scripts/arena-frame-events.md), [formations.md](scripts/formations.md), [timelines.md](scripts/timelines.md) | as stated there |

## Suggested sequencing for Phases 2-5

[plan.md](../plan.md) owns the phase order. Within it, the order below follows
the decomp's structure: the dispatcher is the only stack-free boundary, every
mode depends on the same resident services, and the field route is the one with
captures on both discs.

1. **Data model and build (Phase 2).** Decide ILP32 against a 64-bit handle port
   from the host compile above. Compile the recovered C natively with
   `-funsigned-char`, `-fwrapv` and `-fno-strict-aliasing`, native-only GTE and
   prototype headers, and a native macro, keeping `all-verify` green.
2. **Runtime skeleton.** Add a contiguous game-memory arena, the dispatcher as a
   host loop with a non-local mode exit, the game fiber yielding at waits, and a
   virtual clock for vblank, the 240 Hz tick and completions. Put snapshots at
   dispatch boundaries; reach other points by replaying recorded per-vblank pad
   states.
3. **Resident services.** Implement the disc service at the resident API over
   the raw-sector import, with files keyed by (directory, file) and hash from
   the index and streams and raw reads served by LBA. Then add the pad buffers,
   the heap as is, a bit-exact software GTE, `rand`/`sprintf`, the LZSS decoder,
   and the handwritten routines from their `.s` contracts. Test them against
   captures with `tools/matching_ram.py`-selected routes.
4. **Headless slice (Phase 3).** Run plan.md's Phase 3 route with an SPU and GPU
   that keep state but produce no output: boot, movie (with a logical movie
   clock), the field and its dialogue, the encounter and the battle path
   (resident `mode_run_battle`, ovl2615's setup, the battle overlay and its
   modules at 0x801FC000 through the per-slot registry, ovl2596's results and
   rewards), the return to the field, the menu and save/load. Choose the battle
   lag policy here ([Open questions](#open-questions)). Compare game data, event
   variables and the RNG seed with the RAM snapshots of the forest/encounter
   routes, which hold the battle overlay, ovl2596 and ovl2615 as rebuilt
   ([matching.md](matching.md), Original-environment smoke check).
5. **Presentation and audio (Phases 2-3).** Add the GPU service over a readable
   VRAM, the software SPU with Mono/Stereo/Wide in the mixer, and the software
   MDEC. Add the pre-projection capture at the seams above.
6. **Remaining modes (Phase 4).** Add the world map with its own CD stream
   reader, the arena with its nested fiber, disc swap through `func_801E93A0`'s
   protocol, and the rest of the field, battle and menu content.
7. **Modern presentation (Phase 5).** Re-traverse pre-projection data with
   relaxed culling, keep the on-screen bit and parity on the original camera,
   and capture render targets for the VRAM regions games sample. Layer the PS1
   fidelity toggles on the same stream.
