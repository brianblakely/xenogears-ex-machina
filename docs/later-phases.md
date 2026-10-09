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
of compiled C, 68 (16,028 bytes) of handwritten assembly and 452 (71,168 bytes)
of PsyQ SDK code, with no remaining assembly or placeholders. Regenerate these
numbers rather than copying them.

## Using the decomp as an oracle

### Checks

Run them in the matching shell (`nix ... develop path:./nix/ghidra#matching`).
Commands and inputs are in [matching.md](matching.md).

| Check | What it establishes |
| --- | --- |
| `make -C decomp all-split all-verify` | every rebuilt image is byte-identical to the user's original (26 targets); a linker-script name inside an image fails it |
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
  `func_80024FF4` (sprite_800248D4.c) is commented as copying light settings,
  but it stores the view matrix `D_8004FBB8` that `func_8001E148` applies to
  sprite positions. Some shared views still disagree (Open questions).
- That the C means the same under another compiler. It relies on GCC 2.x's
  choices for unspecified and undefined behaviour
  ([Portability hazards](#portability-hazards)).
- Anything about timing. The build reproduces code, not the speed of the
  hardware. Battle consumes measured lag ([Timing](#timing-couplings-the-pacing-table-omits)).
- That every image was observed running. The ten retained routes load the
  resident, field, slot39 and ovl3384 exactly; their snapshots add battle,
  ovl2596, ovl2615 and part of mdec. The other 17 targets appear in no capture
  ([matching.md](matching.md), Original-environment smoke check).

### Keeping the reference while porting

- `make -C decomp all-verify` must stay at 26/26 after any change to
  `decomp/src` or `decomp/include`. Public CI runs only the toolchain smoke test
  (`.github/workflows/ci.yml`), because the comparison needs the user's discs,
  so the gate is local.
- A cheap guard that needs no discs: the build pipes `psx-cpp` into `psx-cc1`
  with per-unit flags recorded in `<unit>.cflags` (`cc1_of`, decomp/Makefile).
  No source uses `__LINE__`, `__FILE__`, `__DATE__`, `__TIME__` or `__COUNTER__`.
  So when a unit's preprocessed text and flags are unchanged, so is its object,
  given the same split.
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

Five numbering schemes name the same mode overlays. The kernel menu's labels are
in `func_8001A344` (main.c), the files in `D_8004EAA0` (main.c) and the disc
slots in `OVERLAYS` (tools/extraction/overlays.py). The first word at 0x8006FAF0
identifies the loaded overlay ([matching.md](matching.md), Jump tables).

| Mode | Kernel menu label | Target | Word at 0x8006FAF0 | File in directory (0, 1) | Disc 1 / 2 slot |
| --- | --- | --- | --- | --- | --- |
| 0 | (the kernel menu itself) | resident `func_8001A4B4` | none | none | none |
| 1 | Field | `field` | 4 | 0xE | 36 / 31 |
| 2 | Battle | `battle`, entered via resident `func_8001B6C4` | 6 | 0x10 | 38 / 33 |
| 3 | Worldmap | `worldmap` | 5 | 0xF | 37 / 32 |
| 4 | Battling | `menu` (the Battling arena, not the game menu) | 7 | 0xD | 35 / 30 |
| 5 | Menu | resident `func_8001C634` and the 0x801C5000 tenants `slot39`, `ovl2598`, `ovl2600`, `ovl2601`, `ovl2602` | none | 0x11 (not loaded) | 39 / 34 |
| 6 | Movie | `movie` (with `mdec` at 0x801D3000) | 8 | 0x12 | 40 / 35 |

Every overlay has a disc 2 slot five lower (tools/extraction/overlays.py). Slot
39 holds a packed copy of the `slot39` image; the game loads its menu screens
from directory (0x10, 0), file kind + 5 (`func_800799D4`, `func_8001C1A8`). The
two discs carry byte-identical overlays; only the disc index embedded in the
resident differs. Secondary overlays share fixed load addresses and are placed
by heap carving. Resident and overlay code call their entries by absolute
address, whichever tenant is loaded. The slot table is in
[original-boundaries.md](original-boundaries.md) (Control flow and state).

### Boot

From `func_80019578` (main.c):

1. The handwritten entry `func_80019524` clears the resident BSS
   0x800592BC-0x8006FAEC, then `func_80019548` sets `$sp` = `$fp` = 0x80200000
   and `$gp` = `_gp`.
2. Library start-up: `ResetCallback`, `SetVideoMode(0)` (NTSC), `ResetGraph`,
   a 384x480 VRAM clear, `InitGeom`; pads (`func_80036288`); `InitCARD(1)`,
   `StartCARD`, `_bu_init`; the vblank handler `func_8003634C`.
3. The heap over [0x8006FAF0, 0x801FC000) (`func_80031A68`), then `SpuInit`.
4. The disc index: `func_80028230(D_80010004, D_80018004, D_80010000)` with the
   tables embedded in the executable.
5. The sound driver and its 240 Hz tick (`func_80037B88`).
6. Directory (0, 1) files 2-5 (sound banks), 6 (font, `func_80033558`) and
   7 (system data, `func_800335F4`).
7. Cross-mode words reset (`func_8001AADC`). New-game data loaded:
   `func_8001BB50` calls `func_8001B970`, which copies directory (0x10, 0)
   file 3 whole into `D_8006D634`.
8. The boot logo (`func_80019D48`), then mode 6 with the movie request
   `D_8004FE44` = {kind 1, entry 0x10 when the directory reports disc 1, else 7,
   next mode 1, 0}. The movie mode reads it in `func_800737EC` (movie.c) and then
   selects the field.

The only outside inputs are the boot word `D_80010000`, the disc index, pads and
cards. The boot word is -1 in both retail executables
(`decomp/targets/resident/classification.txt`, range 80010000). Any other value
switches on the development paths, such as the debug595 load to 0x80280000
(field.c `func_80077E88`) and the debugger breaks. A pointer value also selects
the PC file server (`func_80028230`). `D_8005917C` points at the word
(kernel_settings.c).

### Mode dispatcher

`func_80019ACC` (main.c) runs each mode and never returns. It reads the row of
`D_8001808C` {entry, BSS start, BSS end, loaded} chosen by `func_8001996C`.

1. On a nonzero error it records the caller (`GET_RA`) and shows the fatal
   screen `func_80019EF8`, which never returns.
2. `ResetGraph(1)`, then the DrawSync callback and the vblank hook are cleared,
   followed by `DrawSync`, `VSync(2)`.
3. The heap restarts at the row's BSS end + 0x800 (`func_80031B10`, which
   releases every block not marked keep).
4. For an overlay row: clear its BSS (`func_80019560`), fetch the packed file
   (`func_800199CC`: cached in a top-of-heap block with tag 6, read quietly)
   and LZSS-decode it to 0x8006FAF0 (`func_80032EB4`). Then `FlushCache`. The
   overlay's `.data` is therefore fresh on every entry.
5. The stack is reset (`func_80019548`). The heap restarts at BSS end + 4, the
   allocation defaults and the pad queue are reset, and the next mode defaults
   to 0.
6. `entry()` runs, then the dispatcher calls itself.

Modes leave by calling `func_80019ACC` from deep frames; the never-returning
sites are listed in [original-boundaries.md](original-boundaries.md). At this
point no game stack frame survives. Only resident `.data`/`.sdata`/`.bss`, kept
heap blocks, VRAM, SPU RAM and driver state carry over. That makes the dispatch
boundary the natural snapshot keyframe and the place for the host's top-level
loop.

### Frames and yield points

- Each mode runs its own blocking frame loops, nested inside one another. The
  field loop `func_80077E88` runs the menu (`func_800799D4` calls
  `func_8001C634`), movies (`func_800A7C58`) and transitions from inside its
  loop. Battle rules call the frame function from inside turn logic
  (`func_800716D8`, battle_80070E2C.c). There is no draw-free update in any mode
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
| Sprite engine tasks | resident sprite.c (`func_8001CA58` links, `func_8001CAF0` allocates); `Task` in resident/sprite.h | heap nodes with `update`/`destroy` function pointers, lists `D_8005958C` (main) and `D_80059594` (second) | `func_8001C9F8` runs the second list, `func_8001C964` the main list (paused while `D_80059428` counts down); battle reruns the main list for measured lag (`func_800BE790`) |
| Field event VM | field_800854D0.c `func_800A2030` schedules, `func_800A1EC8` interprets | per actor: u16 PCs into the map's bytecode, 8 slots | each frame in the move phase of `func_8007554C` ([field-events.md](scripts/field-events.md)) |
| Battle event VM | ovl3087 `func_801E879C` | 16 threads | each battle frame ([battle-event-vm.md](scripts/battle-event-vm.md)) |
| World map actors | worldmap_80094A5C.c `func_80097800` | 64 slots `D_8009BE24` whose `kind` and `update` hold function addresses as integers | each world map frame |
| Arena coroutine | created by menu7.c `func_8008BA2C` on a 0x400-word stack at 0x801FE000 (menu6.c `func_80088E90`); `func_8008BB3C` resumes and `func_8008BC04` yields (handwritten) | the task's registers and stack | resumed once per arena frame; yields at 7 sites (menu2.c 1, menu5.c 6) |
| Sound sequencer | sound.c `func_8003C6E8`, called by the tick `func_8003C020` | sequence channels in data | 240 Hz on root counter 2 ([sound-sequence.md](scripts/sound-sequence.md)) |

### Heap

- Every block has an 8-byte `HeapHeader` {next, caller:21, tag:4, keep:1,
  kind:6} (resident/heap.h). `func_80031BDC(size, mode)` takes the first fit
  (mode 0), the smallest fit (2) or carves from the top (1). Exhaustion is fatal
  (error 0x82) unless quiet mode is on (`func_80031BB4`); then it returns NULL.
- Owner tags select the blocks a subsystem releases (`func_8003218C`).
  `func_800320A4` keeps a block across heap restarts. `func_80032C18` defers a
  release by frames, and `func_80032CB8` drains the delayed releases once per
  frame.
- Secondary overlays are placed by allocating a 4-byte marker at the top and
  then a top block that reaches the slot address (`func_8001C1A8`,
  main_8001B6C4.c). Placement, capacity and quiet-failure fallbacks are
  therefore heap state. A port that changes allocation order or sizes changes
  later addresses and which allocations fail.
- The `caller` field holds the allocating return address (`GET_RA`, heap.c),
  which only the heap report reads. Natively use a call-site id.

### Memory map

| Range | Holds | Source |
| --- | --- | --- |
| 0x8000F800 | PS-X EXE header (file offset 0) | `decomp/targets/resident/slus_006.64.yaml` |
| 0x80010000-0x80018080 | embedded disc data: boot word, file index, directory table; the only bytes that differ between the two executables | resident `classification.txt` |
| 0x80018080-0x80019524 | resident rodata, including the mode table `D_8001808C` | main.c |
| 0x80019524-0x8003F8E8 | game code (the entry point first) | resident `classification.txt` |
| 0x8003F8E8-0x8004EAA0 | PsyQ SDK code | resident `classification.txt` |
| 0x8004EAA0-0x80059170 | `.data`, SDK data from 0x800563F0 | resident `classification.txt` |
| 0x80059170-0x800592BC | `.sdata`, `_gp` = 0x80059170 | [matching.md](matching.md) |
| 0x800592BC-0x8006FAF0 | resident BSS, including the game data 0x8006D634-0x8006F98C | `decomp/targets/resident/link.ld` |
| 0x8006FAF0- | the mode overlay image, its BSS (mode table), then the heap | main.c `D_8001808C` |
| up to 0x801FC000 | heap end; secondary overlay slots 0x801C5000-0x801E5000 inside it | main.c `func_80019578`; [original-boundaries.md](original-boundaries.md) |
| 0x801FC000-0x80200000 | battle modules at 0x801FC000; the stack, top 0x80200000; the arena task stack 0x801FE000-0x801FF000 | `func_80019548`, menu6.c `func_80088E90` |
| 0x80200000- | development-kit RAM (debug overlays at 0x80280000) | [original-boundaries.md](original-boundaries.md) |
| 0x1F800000-0x1F8003FF | scratchpad: work memory and a temporary stack | [original-boundaries.md](original-boundaries.md) |
| 0x1F801C00 | SPU registers, written through `D_800508E4` | resident/sound.h |

All mutable state fits in 2 MB of RAM, 1 KB of scratchpad, 1 MB of VRAM and
512 KB of SPU RAM. That bounds a flat snapshot.

### The resident API

The shared headers under `decomp/include` hold one definition per object that
several targets use ([matching.md](matching.md), Converting a function). They are
the starting schema for introspection and snapshots. Where a target still
declares its own partial view of a shared object, the shared header is the
schema, not the view.

| Header | Subsystem | Key entries and state |
| --- | --- | --- |
| `psyq/*.h` | SDK types and prototypes; unnamed SDK members under their `func_` names | libgpu, libgte, libcd, libspu, libapi, libetc, libpress, libsn, `inline_c.h` (GTE macros) |
| `resident/mode.h` | start-up, dispatcher, cross-mode words | `ModeEntry D_8001808C[]`, `func_80019ACC`, `func_8001996C`, `D_8004F2F4`-`D_8004F384` (reset by `func_8001AADC`) |
| `resident/gamedata.h` | the saved game data | `GameData D_8006D634` (0x2358 bytes, with `LAYOUT_CHECK`s) |
| `resident/heap.h` | heap | `HeapHeader`, `func_80031BDC`, `func_800320E8`, `func_80032C18` |
| `resident/cd.h` | disc index, file reads, CD state, PC file server | `func_80028470`, `func_80028738`, `func_800295D8`, `func_80029AFC`, `func_800286CC`, `D_8004FE48` |
| `resident/stream.h` | disc stream ring, image streams | `StreamRing`, `func_8002A260`, `func_8002BB50` |
| `resident/pad.h` | controllers | `PadBuffer D_800625FC[]`, held/pressed/repeat words, `func_80035CDC` |
| `resident/sound.h` | sound driver, SPU voices, banks, sequences | `SpuRegs` through `D_800508E4`, driver state `D_8005A3C0` |
| `resident/gpu.h` | texture scroll, panorama, OT link helpers | `func_80027EAC`, `func_800273C4`, `func_80031678` |
| `resident/model.h` | model renderer | `ModelGroup`, `SpriteModel`, `D_8004FE50`, `func_8002C700` |
| `resident/sprite.h` | sprite engine and tasks | `Task`, `Sprite`, task lists |
| `resident/text.h`, `resident/window.h` | message text and windows | font and system data resources, `Window` |
| `resident/menu.h` | resident side of mode 5 | `MenuState D_800625A0`, the 0x801C5000 entries |
| `resident/console.h` | debug console, report printf | `func_8003700C`, `func_800379C8` |
| `battle/area.h`, `battle/work.h` | battle area `D_800C3EB0` and work area `D_800CCCE8` | `BattleArea`, `BattleWork`, `Combatant` |

Some resident functions are declared differently by callers in other targets:
narrow parameters or results, another parameter count, or a by-value structure
split another way. The shared headers leave these out. The resident keeps its
own declarations in `decomp/src/resident/own_declarations.h`, and each target
declares its own. For example, `func_8003BDFC` returns `s32` in the resident,
`s16` in battle/frame.h and `void` in field/field.h. A single native link needs
one canonical prototype per function, plus adapters where callers depend on
narrowing.

### Authoritative state

| State | Where | Notes |
| --- | --- | --- |
| Game data | `D_8006D634`, resident/gamedata.h | pointer-free; new game copies (0x10, 0) file 3 whole (`func_8001B970`); save layout in [original-boundaries.md](original-boundaries.md) |
| Cross-mode words and flags | `D_8004F2F4`-`D_8004F384` (mode.h), small data 0x80059170-0x800591B8 (kernel_settings.c, sprite_settings.c) | map, music, battle module and entry requests |
| Field state across battle and the arena | `D_8005A4E4` (0x22FC-byte resident common): field `func_800A3F4C` writes it on those exits (`func_80077E88`), `func_800A3474` restores it; the world map parks its actors there (`func_80075460`) | holds raw actor pointers and world map function addresses |
| Field event variables | `D_800C3A68[0x400]`: the lower half comes from game data `vars` at map load (`func_800705DC`) and returns each frame (`func_800A30FC`); the upper half is per map | game data is stale until the frame ends |
| Battle party | `BattleWork D_800CCCE8` (battle/work.h): ovl2615 `func_801E5384` copies the party in, ovl2596 `func_801E2888` writes it back | game data is stale during battle |
| RNG seed | `D_8005A1FC`: libc `rand` (8003fa38) computes `seed = seed * 0x41C64E6D + 0x3039` and returns `(seed >> 16) & 0x7FFF` | cleared with the BSS at boot and soft reset; `srand` has no caller in `decomp/src` |
| Clocks | `D_80059488` (vblank count, the saved play time) and the vblank h:m:s clock | the field copies the clock into event variables 0xC/0xE each frame (`func_800A31E8`) |
| Draw-buffer parity | field `D_800ADB08` | gates field exits (Presentation) |
| Patched renderer fields | `func_80030988` rewrites shifts and offsets inside `func_80030750` | persists across modes |
| VRAM, SPU RAM, GPU/CD/SPU driver state | hardware | survive mode changes; the saved screen at VRAM (0x2C0, 0x100) carries across modes |

## Boundaries a port replaces

| Boundary | Original interface | Documented in | Port notes |
| --- | --- | --- | --- |
| Disc and files | resident cd.h API over libcd; world map stream reader; mdec `St*` ring; disc swap in slot39 | [below](#disc-and-files) | cut at the resident API plus the direct libcd users listed below; key assets by (directory, file) and content hash, not by slot |
| Controllers | `PadBuffer D_800625FC`, vblank handler `func_8003634C`, 16-entry queue | [original-boundaries.md](original-boundaries.md), Controllers | inject one pad state per vblank; a disconnected pad starts the game's pause loops |
| Memory card and saves | BIOS `bu00:`/`bu10:` file calls, card events, slot39 save/load | Memory card and saves | the commit point is the rename of `__tmp_file`; names for slots 10-14 end in `:;<=>` |
| Game data layout | resident/gamedata.h; payload in slot39/menu.h `SaveData` | Memory card and saves | a load finishes in field scripts (event variables 0x46 and 4); copying game data directly is not a load |
| Sound and SPU | sound.c driver, tick `func_8003C020`, register writes through `D_800508E4` | Sound output modes; [sound-sequence.md](scripts/sound-sequence.md) | software SPU (ADPCM, ADSR, reverb, CD input); the driver tick stays in the simulation |
| GPU and VRAM | libgpu, ordering tables, VRAM transfers, readbacks | Presentation | readable 1024x512 VRAM; framebuffer feedback ([Presentation](#presentation)) |
| Timing and pacing | `VSync`, root counters, `DrawSync`, polls | Timing | virtual clock; [couplings below](#timing-couplings-the-pacing-table-omits) |
| Interrupt work | vblank, sound tick, SPU, CD, MDEC, DrawSync callbacks, card events | Interrupt-context work | fixed delivery points, recorded for replay |
| Control flow | entry, stack reset, dispatcher, mode exits, soft reset, arena coroutine, self-modifying patcher | Control flow and state | host loop with a non-local mode exit; nested fiber |
| BIOS Kanji ROM | `Krom2RawAdd` (save titles; staff roll through unused event BE) | Services | needs the user's BIOS font or a labelled substitute |
| Movies | mdec `St*` streaming, libpress, MDEC DMA callback | Interrupt-context work; Presentation | software MDEC; logical movie clock without output |

### Disc and files

[original-boundaries.md](original-boundaries.md) has no section on the disc yet.
This is what the recovered code asks of it:

- **File addressing.** The file index has 7-byte records: a 24-bit start sector
  and a signed 32-bit size, where a negative size marks a directory of `-size`
  files (`func_800289D0`, `func_80028738`, `func_80028928` in
  main_8002709C.c). The u16 directory table holds 1-based first files per
  (group, index); its word 0x3C is the disc number (`func_80028530`).
  `func_80028470(group, index)` selects a directory, and file f of it is index
  entry `f + table[group + index] - 2` ([matching.md](matching.md), Script
  instructions). The index format is also read by
  `tools/extraction/disc_files.py`.
- **Where the index comes from.** `func_80028230(files, directories, mode)`:
  mode -1 (retail) uses the tables embedded at 0x80010004/0x80018004; mode 0
  reads sectors 0x18 and 0x28 of the disc; any other value is a PC file-server
  name table (`D_8004FE48`, 64 bytes per file).
- **Reads.** These are size (`func_80028738`), a whole file (`func_800295D8`,
  `func_80029690`), a zero-terminated file list (`func_80029AFC`), an image
  stream into VRAM strips from the CD callback (`func_80029EB0`, `func_8002BB50`)
  and a raw sector (`func_8002954C`). Waits are the busy test `func_800286CC` and
  the spin `func_80028A60(0)`.
- **CD commands.** The resident disc service (main_8002709C.c: the command state
  machine `func_8002A68C` and the read functions) issues Getstat, Setloc, ReadN,
  Standby, Stop, Pause, Setmode, GetTN, SeekL and Setfilter. Setfilter selects
  XA file 1, channel `D_8004FE38` (`func_8002A68C`). The
  world map runs its own terrain stream reader with CD callbacks
  (`func_800967E4`, `func_8009699C`, `func_80096A6C`, `func_80096C0C`,
  worldmap_80094A5C.c). The movie library streams through libcd's `St*` ring
  (mdec.c).
- **Disc swap.** slot39 `func_801C8694` asks for a disc until `func_80028530`
  reports it. `func_801E93A0` (slot39_801E8070.c) polls Getstat until the shell
  opens and closes and the motor runs. It reads 16 bytes at sector 0x17 (tag
  `_XEN`, disc digit at +3) and then reloads the index and directory from sectors
  0x18 and 0x28; RAM is kept. Callers: the "CD Change" menu (kind 6,
  `func_801C57A4`), which offers a save and then asks for disc 2, and the
  "Load Game" title screen (kind 2), which asks for disc 1 for a new game and
  for the saved disc `D_8006F008` after a load (slot39.c). The kind names are
  `D_8004F39C` (main_8001B6C4.c).
- **An original synchronous seam.** With `D_8004FE48` set, every resident read
  path uses libsn `PCopen`/`PCread`/`PClseek`/`PCclose` instead of the drive
  (`func_80028738`, `func_80028570`). The disc swap then loads
  `c:\work\cdrom[2].mdg/.fid/.fnd`, and the world map reads its stream lists at
  once. Backing these calls with the port's asset store gives synchronous file
  I/O. It changes timing, though: reads complete before returning
  ([Timing](#timing-couplings-the-pacing-table-omits)).

### Timing couplings the pacing table omits

These come in addition to [original-boundaries.md](original-boundaries.md),
Timing:

- **Battle consumes measured lag.** `func_800BE790` measures each frame's
  overrun, `D_80059494` (0-4). The next frame reruns the camera step
  (`func_800BBAB8`), the main sprite-task list (`func_8001C964`) and
  `func_8008A9C0(1)` that many more times. `BATTLE_AREA.frameTicks`
  (`D_800CCC5C`) = `D_80059494 + D_80059198` also steps the stage
  (`D_800C3E88 += 1 + frameTicks`, battle_8009E53C.c), the object animation
  (`func_800AAA20`), `func_8009F844` and the sky (`func_800A3E98`). Sprite tasks
  run sprite scripts (`func_80022DF4` calls `func_80023210`, which calls
  `func_800248D4`), and sprite commands draw from `rand()` (sprite.c
  `func_8001FBE4`, command `c1`). A lag-free port therefore diverges from a
  lagging original unless the per-frame deltas are replayed.
- **Sprite time per frame.** Sprite scripts tick `D_80059198 + 1` times per
  update (sprite_80022090.c). The field (field.c) and world map
  (worldmap_80072238.c) set it to 1. A field frame lasts 2, 3 or 4 vblanks
  (`unk217C`), so sprite time per vblank changes with the field's frame rate.
- **Scripts wait on sound.** Field ext `64` (`func_8008F5E4`) stalls while
  effect channels play (`func_8003A5D0`). battle_800B8098.c runs battle frames
  until a sound ends and while an SPU transfer is busy (`func_8003BDFC`). A
  headless runtime that does not advance the driver hangs at both.
- **A wait that is not a wait.** World map `while (D_8005957C & 0x10) {}`
  (`func_80072238`) compiles to one test and a self-loop at 0x80072584. The
  source calls it a debug halt. If a transfer is in flight, it hangs forever.
- **Script-visible clocks.** The field timer (event variable 0xA) ticks every
  31 field frames, not per vblank. Variables 0xC/0xE copy the vblank clock
  (`func_800A31E8`).
- **The sound tick and the main loop exclude each other** by disabling the tick
  event `D_800595BC` around driver updates. There are 17 `DisableEvent` sites in
  main2_800366E0.c and sound.c. A port that ticks the driver on another thread
  must keep that exclusion.

## Handwritten assembly and SDK contracts

Recommendation (agreed with the project owner): the Phase 2 runtime
reimplements the handwritten routines and the SDK natively, against their
documented contracts, rather than emulating or transliterating them. The
handwritten code is about 1% of all function code. Every routine has a reviewed
`.s` file whose header comment states its inputs, outputs and quirks. Ported
routines cannot be byte-matched, so they need behavioural differential tests
(packets, OT, counters, GTE state) against captures.

### Handwritten routines

These are the `handwritten` ranges of `decomp/targets/*/*.classification.txt`
(14 ranges):

| Range | Routines | Contract | Native form |
| --- | --- | --- | --- |
| resident 80019524-80019578 | entry, stack reset, word clear | `func_80019524.s`, `func_80019548.s`, `func_80019560.s` | runtime service: start-up, dispatcher loop with a non-local mode exit, per-mode BSS clear from the link; soft reset keeps modified `.data` |
| resident 80026f44-8002709c | GTE pixel darken and blend loops | `func_80026F44.s`, `func_80026FE8.s` | portable C on the software GTE |
| resident 8002e010-8003014c | model primitive renderers, with alternate entries per packet format, run through `D_8004FE50` | `model_draw.s`, `model_depth.s`, each `func_8002*.s` | portable C, and the capture point for pre-projection geometry; keep their culling separable |
| resident 80030750-80030a30 | environment-mapped renderer and its self-modifying patcher `func_80030988` | `func_80030750.s`, `func_80030988.s` | four persistent globals (u/v shift and offset, image default 6, 6, 0x40, 0x40) |
| resident 800315a0-80031894 | ordering-table link helpers | `ot_link.s` | the OT adapter |
| resident 80032e88-80032f54 | LZSS decoder and its allocate-and-decode entry | `func_80032EB4.s`, `func_80032E88.s` | portable C with bounds; `tools/analysis/packed.py` and `tools/packed_container.py` are the format |
| resident 800379b4-800379d0 | arena task index store; report printf tail-jump | `func_800379B4.s`, `func_800379C8.s` | C with a `va_list` printf |
| resident 8003f738-8003f8e8 | rotation matrix and sin/cos lookups (table `D_800523F0`, main3.c) | `func_8003F738.s` | portable C with 32-bit wrapping |
| menu 80072d18-800732cc | ground triangle batch; GTE vector, colour and matrix helpers | `func_80072D18.s`, `ground_packet.s`, `vector_scale.s`, `func_8007313C.s`, `func_800731F8.s`, `func_800732AC.s` | portable C on the software GTE |
| menu 8008bb00-8008bcc8, storage 80096d88 | task context switch (resume, yield, nested-scheduler save and restore) | `func_8008BB3C.s`, `func_8008BC04.s`, `func_8008BB00.s`, `func_8008BB1C.s`; `Task` in menu/system.h | the nested fiber service |
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
| libcd, mdec `St*` | disc | native disc service ([Disc and files](#disc-and-files)) |
| libspu, driver register writes | sound | software SPU, shared by native and browser hosts |
| libetc, libapi events and root counters | time, interrupts | virtual clock and deterministic callback delivery |
| libapi pad, libcard, BIOS file calls | input, cards | per-vblank pad buffers; virtual cards with event completion |
| libc, libc2 | `rand`, `sprintf`, `memcpy`, `bzero` | exact PsyQ behaviour (the `rand` recurrence above), not host libc |
| libpress | movies | software MDEC (VLC, IQ, IDCT, YCbCr to RGB at 15 and 24 bits) |
| libsn | development host only | not needed |

### Inline assembly in the C

The original-style asm forms are listed exactly in `ORIGINAL_ASM`
(tools/matching_coverage.py). They are the PsyQ GTE macros, the debugger break and
pollhost, the stack switches, `GET_RA` and `addPrimLen9`. Natively:

- **GTE macros.** One native header maps the macro names onto the software GTE.
  The PS1 build keeps `psyq/inline_c.h` and the per-target macro sets
  (`battle/gte.h`, `field/field_gte.h`, `ovl2143/gte.h`, `menu/gte.h`,
  `menu/spark.h`, `menu/system.h`, `resident/gte.h`, `worldmap/worldmap.h`),
  because their instruction orders are what match.
- **Stack switches.** These move `$sp` to the scratchpad or a heap block for a
  few calls (for example field.c `func_8007554C`). They become no-ops, but the
  heap allocation around them stays.
- **Debug breaks and pollhost.** These sit behind development tests (boot word
  or `D_800C268C`), with one exception: battle_800B8098.c `func_800B89FC`
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
| LZSS blocks, offset archives | `func_80032EB4`, `func_8003342C` | `tools/analysis/packed.py`, `tools/packed_container.py` | none |
| Field bundle (nine components) | field.c `func_80070CC8` | `tools/analysis/field.py` (components, event package, collision) | palettes, images, trigger zones, the view block |
| Field event bytecode | field_800854D0.c | [field-events.md](scripts/field-events.md), `events.py` | see its Open items |
| Messages and text controls | resident main2.c `func_80033DF0` | [text-control.md](scripts/text-control.md), `text_control.py` | none |
| Models (`ModelGroup`, `SpriteModel`, TMD) | `func_8002C3E8`, `func_8002C700`; battle `func_800B1F6C` | [dispatch-tables.md](scripts/dispatch-tables.md) (primitive census) | no mesh exporter |
| Sprite blocks | resident sprite units | [sprite-vm.md](scripts/sprite-vm.md), `sprite_vm.py` | none |
| Sound: `smds`, `seds`, `wds ` | sound.c, main2_800366E0.c | [sound-sequence.md](scripts/sound-sequence.md), `sound_sequence.py` | no ADPCM sample decoder |
| Battle enemy, AI, effect and event files | battle, ovl2615, ovl3087 | [battle-ai.md](scripts/battle-ai.md), [battle-effect-vm.md](scripts/battle-effect-vm.md), [battle-event-vm.md](scripts/battle-event-vm.md) | none |
| Formations and encounter sets | field component 6 into `D_800658DC`, world map `D_8009D73C`, battle | none | the 0x20-byte record has no layout (Open questions) |
| World map actor and scene scripts, arena scripts | worldmap, menu | [worldmap-actor.md](scripts/worldmap-actor.md), [worldmap-scene.md](scripts/worldmap-scene.md), [arena-scene.md](scripts/arena-scene.md), `overlay_scripts.py` | none |
| Save files | slot39 `func_801CBD90`, `func_801CB304` | [original-boundaries.md](original-boundaries.md), `menu_save_file.py` | none |
| Movies (STR, XA) | mdec, movie, field `func_800A7C58` | VLC only (`src/analysis/mdec_codec.hpp`) | IDCT, colour conversion and XA ADPCM |

### Original bytes the port must import

A distributed or hosted build may not bundle any of these. Load them from the
user's files at runtime, as writable copies where the code patches them in
place.

- **`INCLUDE_ASSET` blobs** linked from the user's images. In the resident: the
  boot logo `D_8004EABC`, a packed image `D_8004FBD8`, the console font
  `D_80050240`, and the error sound banks `D_80050910` (used in place as a
  `SoundBank`) and `D_80050940`. In the menu: the sprite model `D_80091FB0`
  (relocated in place by `func_8002C59C`) and the arena scripts. In the world
  map: the actor and scene-director cue scripts. Each has an `asset` line in its
  target's classification.
- **`INCLUDE_ORIGINAL`/`INCLUDE_RODATA` objects** (`included` lines). These are
  real variables, tables and strings kept original because stray assembler
  bytes fill their padding, or, for the SDK strings, because several library
  functions share them. A port build needs their values imported, or written as
  C once the padding no longer matters.
- **The embedded disc index** of the inserted disc (0x80010000-0x80018080), the
  SDK data tables (libgte, the libpress VLC tables, the reverb presets) and the
  BIOS Kanji ROM.

## Presentation

[original-boundaries.md](original-boundaries.md) (Presentation) lists display and
draw environments per mode, projection settings, ordering tables, dithering,
semi-transparency, texture windows, framebuffer feedback, the mask bit, 24-bit
movie display and the renderers' culling. That includes the LZCR quirk a port's
GTE must keep. [rendering-behavior](../analysis/formats/rendering-behavior.md)
records the observed packets of the Disc 1 forest slice. These facts constrain
a renderer as well:

- **There is no draw-free update.** Drawing passes advance state. In the field
  frame (field.c `func_8007554C`) these are the sprite task lists inside the
  character pass (`func_800752C8`), the effect slots that spawn particles
  (`func_800A9688`), the fades (`func_80071CB4`), the dialogue timers
  (`func_800805F4`), the delayed frees (`func_80032CB8`) and the VRAM upload
  queue (`func_80025044`). In battle, the stage step that draws the objects
  (`func_800A9A50`) also pushes them apart and animates them. Presentation must
  consume the output of the original frame, never call game draw code per eye,
  per repaint or for a spectator.
- **Projection feeds gameplay.** The field character pass projects every actor
  with the original camera and sets `layer_flags` 0x200 when it falls outside
  x [-39, 360), y [-9, 314) (field.c `func_80075B44`). An off-screen actor
  skips turning (`func_800739C0`) and the collision, floor and movement steps
  of the move phase (`func_8008110C`). A window for an off-screen speaker does
  not open (`func_8007F814`), and an open one closes (`func_8009BB0C`), unless
  the window style's bit 0 is set. Field events `8a` and ext `02` branch on
  whether an actor projects inside 320x224 or inside x 33-287, y 33-191
  (`func_80095C00`, `func_80095B3C`). Keep computing these from the original
  camera and screen, headless too: an actor just outside the original screen
  stops moving even when a wider view shows it.
- **Buffer parity and fades are state.** The field leaves for battle, the world
  map, the arena or another mode, and starts a movie, only when the draw-buffer
  index `D_800ADB08` is 1; it opens the menu only when it is 0. A map change
  waits for the first fade channel to finish (`fades[0].steps == 0`) and for the
  disc (field.c `func_80077E88`).
- **Packets are pre-culled** to the original window. The model renderers test
  the screen bounds set by `func_8002DFF0`. World map terrain drops SZ >= 0xF00
  and stops at 0x7FE packets (`func_8009980C.s`). Widescreen, stereo and VR need
  a side-effect-free re-traversal of pre-projection data, not a reprojection of
  the packet list.
- **VRAM is persistent, read-back state.** The world map copies its screen to
  (0x2C0, 0x100) (`MoveImage` in worldmap_80072238.c and its scene directors).
  The field's kind-3 transition copies that slot back every frame (field.c
  `func_8007554C`). The battle intro (battle_800B7134.c `func_800B7870`) stores
  the shown screen, sets STP on every pixel and loads it there. The menus copy a
  saved screen into the back buffer each frame
  ([original-boundaries.md](original-boundaries.md)). A bit-exact 1024x512x16
  VRAM belongs in snapshots; an HD renderer redirects samples of such regions to
  captured render targets.
- **Ordering is more than depth.** The field links its background table into
  the main one at a far cutoff (`unk21D4`). Sprites draw with depth biases and
  split parts (`func_80075B44`, sprite.c `func_8001E3D8`). Model types sort by
  average, farthest or nearest vertex (`model_depth.s`). A plain depth buffer
  breaks these intentional masks.

| Pre-projection seam | Where | Captures |
| --- | --- | --- |
| 3D models of every mode | `func_8002C700` (main_8002C3E8.c), 11 call sites in battle, field, menu, ovl2143, resident sprites and the world map | mesh, GTE rotation/translation, H, offset, depth cue, sort mode, OT |
| Sprites | `func_8001E148` (position through the view matrix `D_8004FBB8`), `func_8001E3D8` (part corners through `RotTransPers4`) | world position, scale, facing, frame parts |
| World map terrain | `func_8009932C` calls `func_80099708`, which calls `func_8009980C.s`; billboards `func_8008615C` call `func_80099BFC.s` | the 9x9 vertex grid on the scratchpad, cell words |
| Cameras | field `FieldView D_800AF880` (look-at `func_80073750`); battle eye/target (`func_800BBAB8`); world map camera (`func_80097244`) | read-only inputs to stereo, diorama and VR views; head pose must never write them, because the player control (event `a7`, `func_8009F5F4`) subtracts the field camera's `angle` from the pad direction |

Text is rasterised on the CPU into the window's 4-bit line image, two glyph
planes per nibble (`func_80034FFC`, main2.c), and drawn as sprites
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
clang --target=TRIPLE -fsyntax-only -std=gnu89 -undef -nostdinc -Idecomp/include \
  -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx -D_PSYQ -D_MIPSEL \
  -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C -funsigned-char \
  -DORIGINAL_IMAGE='"x"' -DORIGINAL_BASE=0 -ferror-limit=0 decomp/src/TARGET/FILE.c
```

Diagnostics are counted once per source location. These counts are from this
revision:

| Diagnostic | x86_64-linux-gnu | wasm32, i686 | Cause |
| --- | --- | --- | --- |
| pointer to integer casts | 653 in 44 files | 0 | 32-bit addresses kept in integers |
| integer to pointer casts | 278 (+33 to `void *`) | 2 | pointers rebuilt from 32-bit words |
| non-constant static initializers | 17 in 3 files | 0 | function addresses stored as `s32` (world map `D_80099E8C`; menu3.c, menu4.c) |
| failed `LAYOUT_CHECK` | 3 (battle/area.h, battle/effect.h, battle/scene.h) | 0 | asserted offsets of structures with pointers |
| asm with MIPS register names | 150 in 24 files | same | GTE macros, stack switches, `GET_RA` |
| non-prototype declarations | 328 in 48 files | same | K&R definitions and calls |
| incompatible pointer types | 316 in 27 files | same | one object read through several types |
| `return;` in a non-void function | 45 in 13 files | same | implicit-int functions; see below |
| conflicting types | 32 in 10 files | same | implicit declarations, then definitions |
| arrays of incomplete struct type | 41 in 6 files | same | commons units define their variables before the headers complete the types ([matching.md](matching.md), Recovering data) |
| incompatible function-pointer types | 27 in 5 files | same | cast dispatch tables |
| unsequenced modifications | 6 in 2 files | same | several `*pc++` in one call's arguments |
| shift of a negative value | 14 in 5 files | same | `-x << n` |
| array index -1 | 4 (battle.c, through `STEP_FUEL`) | same | tables read through an offset base |

wasm32 and i686 keep the original's data model, and every layout check holds
there. Choosing ILP32 everywhere (wasm32, or ahead-of-time compiled wasm on
64-bit-only hosts) or an LP64 handle port is therefore the first Phase 2
decision.

| Hazard | Evidence | Handling |
| --- | --- | --- |
| 32-bit pointers in data | world map actor slots and spawn tables hold function addresses as integers (`func_80097800`, `D_80099E8C`); sprite tasks, sound modulators and hooks hold function pointers; models and archives are relocated in place (`func_8002C3E8`, `func_8002C59C`, `func_8003342C`); `D_8005A4E4` parks raw pointers across modes | keep game memory a contiguous 32-bit arena and map code addresses to stable ids; never serialise host pointers |
| 24-bit ordering-table links | `AddPrim` and the OT helpers store packet addresses masked to 0x00FFFFFF; the arena rebuilds pointers as `(tag & 0xFFFFFF) - 0x80000000` (menu7.c `func_8008ACB8`) | links as offsets into the arena, below 16 MB |
| Punned views | the same bytes read through several types (316 incompatible pointer types); the `link.ld`, `battle.data.ld`, `menu.bss.ld` and `worldmap.data.ld` views name parts of objects ([matching.md](matching.md)) | `-fno-strict-aliasing`; one canonical type per address for schemas |
| Signedness and width | plain `char` is unsigned (`-D__CHAR_UNSIGNED__`, `lbu`; [matching.md](matching.md), Qualified configuration); `long` is 32 bits in the PsyQ structures (`VECTOR`, `MATRIX`, packet tags); event variables are 16-bit and read signed or unsigned by the map's event package bits (`func_800A3018`) | `-funsigned-char`; 32-bit `long` in native SDK headers |
| Unspecified evaluation order | `func_80021B04(&angles, rand(), rand(), 0)` (sprite.c, case c1): the matched code draws the first `rand()` into the second argument (0x80020244-0x8002026c); the 6 unsequenced `*pc++` calls in battle_8009E53C.c and ovl2143.c | read operands into locals in the order the matched disassembly shows |
| Values left in `$v0` | `func_800BEFF4` is defined `void`, but callers use the acting sprite left in `$v0` (0x800bf094; battle_800B8098.c); `func_8008FACC` falls off the end (menu7.c); `func_8008B730` returns the header pointer with zero frames (0x8008b770); `func_800381F4` returns its allocator's result implicitly; `func_8003F190` sits in the `s32` modulator table, and its value always scales to 0; `func_801E7210` returns the allocator's NULL into `drawEnv.isbg`; the movie test menu stores what `func_80074BA4` and `func_8007519C` leave for an index past the list | return the PS1 value explicitly; check every other `return;` site's callers before declaring it `void` |
| Uninitialized reads | `chance` in `func_8009A2D4` (gear boost, battle_8008CCCC.c); `actions` in `func_80072324` zeroes 0x100 bytes at an unset pointer; unset `parts` entries in `func_80096FBC`; the call event (type 8) of ovl2143 `func_801E5D44`; `owner` in ovl3383 `func_801FC020` and ovl3385 `func_801FC508`; `hold` (menu5.c `func_800852C4`), `last` (menu6.c `func_80088E90`); sprite opcodes 40-7f and f7 (resident and battle sprite VMs) | an optimising compiler treats these as undefined; give each an explicit native value and record whether it can diverge (Open questions) |
| Division | units built with maspsx `--expand-div` (ovl2615, mdec, movie, ovl2143, worldmap, resident `main_8002709C`, battle `battle_8009E53C`; see the `.mk` files) trap on zero with `break 7` and on overflow with `break 6`; other units use bare `div`, which does not trap on the R3000 | a source-correlated division adapter where a zero divisor can occur (`func_8009A2D4` divides by `maxHp / 10`); keep the field script divide `func_8009D768`, which already maps 0 to 1 |
| Signed overflow and shifts | the handwritten trig and renderers rely on 32-bit wrapping; 14 shifts of negative values | `-fwrapv` and arithmetic-shift helpers |
| Overruns and offset bases | `STEP_FUEL` is `&gearHud.commands[-1]` (battle_command.h); `battle.data.ld` names `D_800C34B3` and `D_800C31D4` before their tables; menu5.c `func_80085EC8` writes `arrows[1][3..5]` past its array | explicit range-checked index arithmetic natively |
| Decompressors read past files | the overlay LZSS decoder reads its final flag byte past the file (tools/extraction/overlays.py); arena model files and map 145's messages read past their bytes ([arena-frame-events.md](scripts/arena-frame-events.md), [text-control.md](scripts/text-control.md)) | zero-pad imported files to whole sectors; bound the decoder |
| Scratchpad | work memory and stack switches ([original-boundaries.md](original-boundaries.md)); `TerrainDrawScratch`/`TerrainPassScratch` in worldmap/worldmap.h | a 1 KB static buffer behind one accessor |
| Fixed cross-image addresses | the mode table holds overlay entries and BSS bounds as numbers (main.c `D_8001808C`); slot tenants are called by address (`func_8001C1A8`); cross-image names come from the original's addresses (splat's `undefined_syms_auto.txt`, `*.resident.ld`, `debug595.field.ld`), outside the strict linker-script check ([matching.md](matching.md)) | a per-slot registry that rejects calls into an absent image; 45 function names are defined by two or more targets' C, so give them per-image namespaces |
| K&R calls and per-target prototypes | unprototyped calls pass unpromoted arguments ([matching.md](matching.md)); targets declare shared functions differently (`own_declarations.h`) | canonical prototypes and thunks in native-only headers; in WebAssembly a mismatched indirect call traps |
| Non-volatile polling | `while (D_8005957C & 0x10)` was compiled to test once (world map); other polls re-read through calls (`func_80028A60` loops on `func_800286CC`) | deliver interrupts at yield points inside polls; do not rely on the host compiler re-reading globals |
| GTE fixed point | gameplay calls libgte: field movement and collision use `ratan2` (field_8007A44C.c), as does the arena AI (menu7.c); culling reads GTE registers (LZCR in `model_draw.s`) | one bit-exact software GTE whose state is in snapshots |
| Debug paths | the boot word is tested in the resident, field, battle, arena and menu screens (`D_80010000`, `*D_8005917C`, `*(s32 *)0x80010000`); the field caches the test in `D_800C268C` | build with the retail value -1; never leave the word zero |

## Original quirks and bugs a port keeps

The C reproduces these as long as data stays byte-identical and loaded
contiguously. Keep them by default; fixes are opt-in options that are recorded
in replays.

| Quirk | Where | Effect |
| --- | --- | --- |
| The field timer counts 0-60 seconds and never saturates | `func_800A31E8` (`seconds != 0xFF3B` is never false; `> 60` wraps) | 61 seconds per minute; minutes wrap with the u16 variable |
| Sound `EA` ends its pan slide at the difference | sound.c `func_8003DEE4`; [sound-sequence.md](scripts/sound-sequence.md) | the last frame sets pan to `(target - start) << 8` |
| Sprite replay steps `be` by 2 | `func_80022660`; [sprite-vm.md](scripts/sprite-vm.md) | the interpreters take 3 bytes, so the replay reads `be`'s last byte as the next command |
| Battle 70's targeted script runs into enemy id 1's table | [battle-ai.md](scripts/battle-ai.md) | id 1's turn rule runs |
| Three effect commands count one event short; some name unused or out-of-range animations | [battle-effect-vm.md](scripts/battle-effect-vm.md) | an 8-byte sound event never plays; the animation lookups (800AF518, 801E6910) do not check bounds |
| One random stream for effects and gameplay | `rand` (8003fa38): 230 call sites in 29 files; the battle shatter setup `func_800B7424` draws 3,920 values (2 x 14 x 20 shards, 7 each) | skipping, adding or reordering a visual effect changes later rolls |
| Lag changes battle | `func_800BE790` | see Timing couplings |
| Model renderers never reject overflowed faces | `model_draw.s` (reads LZCR, not FLAG) | oversized triangles reach the GPU ([original-boundaries.md](original-boundaries.md)) |
| Soft reset keeps modified `.data` | `func_80019CD0` jumps to the entry | not equivalent to restarting the process |
| Unreachable content stays off | the staff roll needs event BE, which no shipped script uses (`tools/analysis/staff_roll.py`); the field's debug key into map 0 needs the development word ([field-events.md](scripts/field-events.md)) | optional content, not default behaviour |

## Open questions

| Question | Evidence so far | What would settle it |
| --- | --- | --- |
| What the uninitialized reads that can change gameplay compute on hardware | the `func_8009A2D4`, `func_80072324`, `func_80096FBC` and ovl2143 `func_801E5D44` (event type 8) sites above | a targeted capture per site, or a census showing shipped data never reaches the unset path |
| How often battle frames overrun on hardware | `D_80059494` is measured at run time; all captures are emulator runs ([original-boundaries.md](original-boundaries.md)) | per-frame `D_80059494` from captures, or an accepted lag-free definition for Phase 3 parity |
| Formation and encounter-set layout | worldmap/worldmap.h keeps `EncounterSet` opaque; field component 6 decodes into `D_800658DC`, which field.h names "messages" and battle_core.h "scene settings"; the battle AI census does not cross-check formations ([battle-ai.md](scripts/battle-ai.md)) | one layout from the readers (field, world map, battle, ovl2615, ovl3087) and a decoder |
| Conflicting shared views | `D_8006F8EA` is a u16 flags word in ovl2596.c (and in `func_8009A2D4`) but a u8 per-part durability array in battle/combatant.h (battle_80079ED8.c, battle_8008CCCC.c); targets still keep partial views of the game data beside resident/gamedata.h | reconcile from the readers, then one canonical type per address |
| The disc and stream boundary | summarised above from the code; no capture records disc 2 media, swap timing or CD callback order | a disc-swap and streaming capture on both discs |
| Wide on hardware | emulator recordings only ([original-boundaries.md](original-boundaries.md)) | a hardware recording before Phase 10 |
| World map, Gear battle and arena presentation | no packets observed ([original-boundaries.md](original-boundaries.md)) | `tools/analysis/gpu_packets.py` on captures of those modes |
| Missing media decoders | no IDCT, colour conversion, XA ADPCM or SPU ADPCM in the repository | implementations in Phase 2, tested against captured frames and audio |
| BIOS font source | `Krom2RawAdd` users in slot39 and field | a user BIOS import or a labelled substitute |
| Script data open items | the Open lines of [field-events.md](scripts/field-events.md), [battle-effect-vm.md](scripts/battle-effect-vm.md), [battle-event-vm.md](scripts/battle-event-vm.md), [arena-frame-events.md](scripts/arena-frame-events.md) | as stated there |

## Suggested sequencing for Phases 2-5

The order follows the decomp's structure: the dispatcher is the only stack-free
boundary, every mode depends on the same resident services, and the field route
is the one with captures on both discs.

1. **Data model and build (Phase 2).** Decide ILP32 against LP64 from the host
   compile above. Compile the recovered C natively with `-funsigned-char`,
   `-fwrapv` and `-fno-strict-aliasing`, native-only GTE and prototype headers,
   and a native macro, keeping `all-verify` green.
2. **Runtime skeleton.** Add a contiguous game-memory arena, the dispatcher as a
   host loop with a non-local mode exit, the game fiber yielding at waits, and a
   virtual clock for vblank, the 240 Hz tick and completions. Put snapshots at
   dispatch boundaries; reach other points by replaying recorded per-vblank pad
   states.
3. **Resident services.** Implement the disc service at the resident API, with
   assets keyed by (directory, file) and hash, then the pad buffers, the heap as
   is, a bit-exact software GTE, `rand`/`sprintf`, the LZSS decoder, and the
   handwritten routines from their `.s` contracts. Test them against captures
   with `tools/matching_ram.py`-selected routes.
4. **Headless field slice.** Run boot, movie (with a logical movie clock), the
   field and the menu with an SPU and GPU that keep state but produce no output.
   Compare game data, event variables and the RNG seed with the RAM snapshots of
   the forest/encounter routes.
5. **Presentation and audio (Phases 2-3).** Add the GPU service over a readable
   VRAM, the software SPU with Mono/Stereo/Wide in the mixer, and the software
   MDEC. Add the pre-projection capture at the seams above, then the arena's
   nested fiber.
6. **Remaining modes (Phase 4).** Add battle with the chosen lag policy and the
   module registry at 0x801FC000, then the world map with its own CD stream
   reader, the arena, and disc swap through `func_801E93A0`'s protocol.
7. **Modern presentation (Phase 5).** Re-traverse pre-projection data with
   relaxed culling, keep the on-screen bit and parity on the original camera,
   and capture render targets for the VRAM regions games sample. Layer the PS1
   fidelity toggles on the same stream.
