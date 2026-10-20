# Runtime (Phase 2)

The shared Rust/C runtime of [plan.md](../plan.md) Phases 2 onward. The recovered
C in `src/decomp/` stays the only game implementation; this document records how it
is built for native and browser hosts and how the hosts are layered. The
[later-phases handbook](later-phases.md) holds the program facts it relies on.

## Approved foundations

| Decision | Choice |
| --- | --- |
| Rust workspace | `src/runtime/` (`src/runtime/Cargo.toml`, crates in `src/runtime/crates/*`, committed `Cargo.lock`) |
| Toolchain | `nix develop path:./nix/runtime` (Rust 1.97.1 with wasm32/Android targets via rust-overlay, clang/lld 21.1.8, wabt 1.0.41, binaryen 132, wasm-bindgen-cli 0.2.127, SDL3 3.4.14, mesa 26.2.2, Monado 25.1.0, Chromium 152, Node 24, perf 7.2.3); `#android` adds the Android SDK/NDK r29, emulator and API-34 x86_64 image, JDK 17, Gradle 8.14.4, cargo-ndk |
| Data model and link | the recovered C is compiled once to a wasm32 *game module* (ILP32, the original data model); natively it is translated ahead of time with wasm2c, in browsers it runs as is beside the wasm-bindgen runtime |
| Versions | slint =1.18.1 (`unstable-wgpu-30`, FemtoVG wgpu renderer), wgpu/wgpu-hal =30.0.1, sdl3-sys =0.6.8+SDL-3.4.14, openxr 0.22.0, wasm-bindgen =0.2.127, iwer 2.5.0, playwright-core 1.64.0 |
| Slint license | Royalty-free 2.0 (attribution shown in the settings panel); GPLv3 is the alternative |

## The decomp mandate

The decomp is the game-logic provider and stays a pure, byte-identical
implementation of the original game ([plan.md](../plan.md), Shared
native/browser architecture). It is compiled unchanged; nothing in `src/decomp/` is
added or edited for integration. When integration needs something the original
sources do not give, it goes into `src/port/` or `tools/game_module.py`: shadow
headers through the prelude, port functions, the inline-assembly map, IR
rewriting. A change to `src/decomp/` is made only for the original game and is
verified with `make -C src/decomp all-verify`.

## The game module

`python3 tools/game_module.py` (inside `nix develop path:./nix/runtime`) builds
`build/game/game.wasm` from the same C units the PS1 targets link and the port
layer in `src/port/`. The decomp is not changed for the port: its sources and
headers stay exactly what the byte-matching build compiles, and everything the
port needs lives in `src/port/` and the tool. It needs the matched PS1 links
(`make -C src/decomp all-verify`), whose symbol tables give every object its
original address.

- **Front end.** clang compiles the untouched C for `mipsel-unknown-unknown`,
  which accepts the original inline assembly, and the IR is retargeted to
  wasm32: both are ILP32 and little-endian with the same ABI alignments, so
  every structure keeps its layout. `src/port/include/xem/prelude.h` is
  force-included first; the headers it includes replace originals of the same
  include guard (`include_asm.h`, whose macros include nothing; PsyQ's
  `stdarg.h`, which walks MIPS argument words) and give canonical prototypes
  where a unit calls a function before declaring it.
- **Inline assembly.** Each statement (GTE transfers and commands, stack
  switches, `break`s) becomes a call to the port function that
  `src/port/asm_map*.json` names for its template and constraints.
- **Port layer.** `src/port/*.c` defines what the matched images take from
  assembly: the PsyQ SDK and BIOS calls (over host imports), the handwritten
  routines (from their `.s` contracts) and the entry and restart logic. A
  function the port does not define yet traps through `xem.missing`
  (`--stubs` lists them in `build/game/stubs.txt`).
- **Host queries.** Between frames the hosts ask the port whether the game
  is paused (`xem_game_paused`), whether a battle's attack page takes the
  buttons (`xem_game_combo_input`) and what the field allows
  (`xem_game_field_controls`). Natively these run under wasm2c's trap
  handler as the game's own entries do (a browser's call catches a trap
  itself): a fault in one leaves the stack pointer as the call found it,
  the query answers as if nothing applied, and the first is reported
  (`status`'s `query_trap`, a line on stderr), where it once ended the
  desktop host. The combo query reads the battle's turn state only through
  a pointer in RAM: until the battle allocates it (as it starts), the
  pointer holds what the overlay's memory held.
- **Handwritten routines.** Their ports (`src/port/model_draw.c`, `arena.c`,
  `worldmap_terrain.c`, `sprite_pixels.c`, `lzss.c`, `resident.c`) keep game
  addresses as 32-bit integers and access memory through
  `src/port/include/xem/memory.h`, which maps KUSEG and KSEG1 RAM mirrors onto
  KSEG0: a packet pointer the original reduced to its 24-bit DMA address stays
  reduced where the game sees it (`model_current_packet`). They make the
  original's GTE transfers and commands in its order, including those in
  branch delay slots and the projection of the record after the last face, so
  the GTE is left as the original leaves it. `model_set_envmap_mapping`
  patches `model_draw_ft3_envmap`'s instruction words in game memory, and the
  renderer reads its shifts and offsets back from them on every call. Native
  tests (`tests/port_native.py` maps RAM and the scratchpad at their PS1
  addresses) check the contracts; `tests/test_port_original_traces.py`
  replays original calls captured by the reference emulator
  (`tools/reference/handwritten_trace.py`).
- **Original memory layout.** Game memory is the PS1 address space: RAM at its
  KSEG0 addresses 0x80000000-0x801FFFFF and the scratchpad at 0x1F800000, inside
  one wasm32 linear memory. Every global the C defines resolves to its original
  address; its bytes come from the images the game itself loads (the host loads
  the executable as the BIOS did, the game decodes its overlays), so heap
  placement, overlay slots, overruns into neighbouring objects and pointer-laden
  data behave as on the console, and game RAM compares directly with captures.
  The port's own data (shadow stacks, asyncify areas, port statics) starts at
  0x10000000 (`PORT_BASE` in tools/game_module.py), outside RAM, the
  scratchpad, I/O and every mirror the game or its DMA addresses reach. The
  linear memory ends at 0x80200000.
  The build translates load/store and bulk-memory operands through the PS1
  RAM mirrors before optimization, while preserving stored pointer values.
  Thus a 24-bit graphics link writes the same RAM that the GPU reads at
  KSEG0. `tests/test_game_module_memory.py` executes those aliases in Wasm;
  the Battling snapshot test runs through the first particle effects and
  past the former frame-725 display-list failure.
- **Code addresses.** A function's address, wherever C takes it, is its original
  address. Indirect calls go through generated dispatchers that select the
  function by address and check, from a fingerprint of its original code bytes in
  game memory, that the image holding it is the one loaded there; a call into an
  absent or overwritten image traps.
- **Unprototyped calls.** A direct call whose types differ from the
  definition goes through a generated adapter (`build/game/adapters.txt`
  lists each): integer arguments are truncated or extended by the caller's
  type (an unsigned char is zero-extended, as MIPS widens it into the register
  the callee reads), a missing argument is 0 and an extra one is dropped. Where the PS1 callee read the
  register or stack word the caller left, or the caller used the `$v0` a void
  function left, `src/port/adapters.c` defines `xem_adapt_<function>` with the
  call's types and the matched code's value, and the build calls it instead.
  A function stored where its callers use another result is reached through
  the port's `xem_thunk_<function>` of the stored type, which the dispatcher
  of that type selects at the function's address: `sound_switch_modulator_off`,
  which returns nothing, in the `s32` table of modulator waves (its shapes
  8-15, which shipped sound scripts select; before the thunk, the debug Gear
  battle trapped there once Cross played such a sound).
- **Suspension.** `wasm-opt --asyncify` instruments the module so that the
  imports `xem.yield` (frame waits, polls) and `xem.restart` (the dispatcher's
  stack reset, soft reset) unwind the game stack into linear memory. The host
  resumes a yield by calling `xem_run` again and answers a restart by calling
  `xem_run(kind, arg)` on an empty stack; the dispatcher (`mode_dispatch`,
  wrapped by the port) therefore runs each mode from an empty stack, as the
  original does. A snapshot at a yield point is game memory, the module's
  globals and the host services' state.
- **Fibers.** The arena runs its mode task as a coroutine on its own MIPS
  stack (`arena_task_resume.s`, `arena_task_yield.s`); the port cannot switch
  stacks, so the game is two fibers (`src/port/fiber.c`): the game, whose bottom
  frame is `xem_run`, and the arena task, whose bottom frame is the export
  `xem_task_run`. Each has its own asyncify save area (`xem_unwind_area`
  returns the running fiber's) and its own shadow stack (the task's is a port
  array; the PS1 task stack at 0x801FE000 is never written). `arena_task_resume`
  and `arena_task_yield` call the import `xem.task_switch(fiber, stack_top)`:
  the runtime unwinds the running fiber, keeping its area and shadow stack
  pointer, then starts the other on the shadow stack ending at `stack_top`
  (nonzero for a TaskContext the task fiber does not run yet: it calls
  entry(arg), the context's ra and a0) or rewinds it where it suspended, all
  within one `Runtime::step`. Frame waits and polls inside the task unwind
  only the task fiber, interrupts run below the suspended fiber's frames, and
  a restart abandons both fibers (`xem_run` resets the port's fiber state when
  it starts afresh). The original's register bank is not modelled: a yield
  leaves the TaskContext untouched, `arena_current_task` stays set after it,
  and `arena_task_caller_stack` holds the address of the resuming frame on
  the game fiber's shadow stack. The task entry never returns
  (`arena_mode_task` loops); the original would re-enter it through ra, the
  runtime stops the game with a diagnostic. `tests/fiber_module/build.sh`
  runs `src/port/fiber.c` and `src/port/arena_task.c` under asyncify, wasm2c and the
  runtime end to end.
- **Native.** `wasm2c` output is compiled into `xem-game` with explicit bounds
  checks (no signal handlers). Nothing interprets or emulates a CPU. The
  translation comes in 16 parts compiled in parallel (one file took GCC
  nearly four minutes, the parts 41 s on 16 threads), built without
  semantic interposition so the parts' global functions inline as one
  file's static ones did (a Lahan frame's game work: 1.395 ms, one file
  1.396), and with `-ffp-contract=off`: each floating-point operation is
  rounded on its own, as wasm's are. GCC otherwise fuses a product into
  the sum after it where the processor has a fused multiply-add: for
  aarch64 at `-O2` it gives `xem_present_face_visible` (with
  `xem_present_points_visible` the module's only floating point, the
  port's tests of which culled faces a host reveals) two `fmadd` and three
  `fmsub` in place of eight `fmul`, two `fadd` and three `fsub`. The builds made here round
  alike either way: x86_64's baseline has no fused multiply-add and the
  NDK's clang fuses only within one C expression, which wasm2c never
  writes, so their objects are the same byte for byte with and without the
  flag (GCC's x86_64 ones, the static arm64 `xem-headless`'s and the APK's
  for both ABIs).
- **Browser.** The same `game.wasm` is instantiated next to the Rust runtime
  (wasm32-unknown-unknown, wasm-bindgen); the two modules keep separate
  memories and exchange scalars and bounds-checked offsets.

## Commands, introspection and snapshots

`xem_core::control::Session` is the one command layer for humans, agents,
tests and browser clients: a JSON command in, a JSON result out
(`xem-headless --control` reads one per line). The hosts' app operations
(the panel's and the bindings': a held rewind, card write retries, ...)
sit on it the same way for players and agents (`window.xem.appOperation`,
`--control`'s `app {request}`; [Saves and time
controls](#saves-and-time-controls)). Commands: `status`,
`frames {count}`, bounded `run_until {max_frames, until: {frames} | {vblanks} |
{path, equals|not_equals}}`, `pad {port, buttons}`, `press {buttons, frames}`,
`hold {actions}` and `act {actions, frames}` (pad 1 by typed actions,
[Controls](#controls)), `context`, `targets`, `field`, `pick {screen}`,
`navigate`, `interact {actor}` and `assist_stop` ([Assisted
movement](#assisted-movement)),
`connect_pad`, `inspect {path, depth}`, `write {path, value}`, `globals
{prefix}`, `read_memory`/`write_memory`, `snapshot`/`restore {name}`,
`memory_hash`, `record`/`stop_recording`/`replay {changes}` (pad states per
vertical blank; a log that spans a restore is refused, [Saves and time
controls](#saves-and-time-controls)) and `scenario {writes, mode}`, which writes values through the
schema and enters a mode through the original dispatcher on an empty stack.
Pads are set as the BIOS driver reports them; nothing simulates lower-level
input. `xem-headless --control` adds `screenshot {path}` and `vram {path}`
(PNG files), `display` (where the display shows VRAM) and `reveal_frames
{count, reach: [x, y], pull_back, around}`: frames drawn as a view wider or
pulled back draws them, the draw trace asking the port for the faces the
game culls, answered with the primitives traced in them (the first frame's
too), how many were culled faces and an FNV-1a digest of those (their
shapes and screen points in the order traced; presentation only: the RAM
goes on as without; [State interchange](#state-interchange)).

- **Introspection.** `tools/game_schema.py` compiles every unit again with
  debug information and writes `build/game/schema.json`: each global the C
  defines, at its original address, with its full type (records, arrays,
  bit-fields, enums, pointers to named records), plus the matched links' other
  data symbols untyped. `inspect` decodes paths such as
  `game_data.characters[0]`, `mode_table[1].entry` or, through a pointer,
  `field_view.components.descriptors[4].matrix.t`; `write` stores scalars.
  `xem-game` embeds the schema built with the module, so every native host
  has it.
  The commons units, which define arrays of types completed by later headers
  (accepted by GCC, not clang), are described through a temporary wrapper that
  includes their target's headers first. A unit whose inline assembly
  clang's MIPS assembler rejects (the GTE's coprocessor moves, `pollhost`'s
  `break 1024`: `field.c`, the dialogue windows, the battle scene and 13
  more, whose globals the schema once lacked) is described from its
  assembly, written without parsing the inline blocks and assembled
  without them; only `menu_common.c` stays out (its types incomplete even
  after its target's headers).
- **Snapshots** (`Runtime::snapshot`/`restore`) hold the module's low memory
  (port data, shadow stack, asyncify area), the scratchpad and I/O pages and
  the PS1's 2 MB of RAM (zero runs compressed), the module's stack pointer and
  suspension, the clock, the pads, pending interrupts and every device's state.
  They are taken at waits, where the whole game stack lives in game-module
  memory. `memory_hash` hashes RAM and scratchpad: the authoritative state the
  native and browser hosts compare.
- **Time.** The game runs in zero time between its waits; the clock advances
  only there (a frame wait to the next vertical blank, a poll to the next
  interrupt), and every loop back-edge of the game polls now and then
  (`xem_loop_poll`), so waits that spin on memory an interrupt writes reach the
  clock. The field paces its frames by spinning on `VSync(-1)`: the port's
  `VSync(-1)`, read again with no wait in between, waits for the next
  interrupt at once (`xem_spin_waiting`) instead of after the poll's 65536
  iterations, which took about 12 ms of host time per game frame. Headless runs are unlocked (as fast as the host computes) and
  reproducible. A frame ends just before its vertical blank interrupt, as an
  emulator's frame ends at the blank before the CPU takes it; the next frame
  delivers it first. Pad input a host sets between frames is therefore what
  the blank's pad read sees at once.
- **Interrupts.** The host delivers device interrupts in time order through
  the export `xem_interrupt` (`src/port/libetc.c`), between the game's steps
  and at its waits. `SetIntrMask` is modelled: a masked interrupt stays pending,
  as I_STAT holds it, and runs when the mask enables it again (libgpu masks
  everything while it runs its queue). A handler cannot suspend: a wait inside
  one (`xem_wait`, `src/port/boot.c`) asks the host to deliver what is due now
  (`xem.poll_now`) instead of yielding, and the runtime traps if a handler
  suspends anyway.
- **GPU transfers.** libgpu (`src/port/libgpu.c`) keeps the SDK's queue: work
  runs at once when DMA2 is free, else waits for its completion interrupt.
  A DMA2 transfer moves its words when it starts and has ended by the next
  CHCR read (`xem_core::devices::gpu`), as a block transfer ends on the
  console long before the CPU reuses what it read: the battle's HUD sheet
  uploads go out before the heap reuses their buffer. A packet or image on
  the port's stack, which is outside PS1 RAM where the original's stack was,
  goes through the GP0 and GPUREAD ports by the CPU instead (`PutDrawEnv` of
  a local environment, as the boot logo does).
- **Boot mode.** `boot_mode {mode}` (before the first dispatch) makes boot
  leave into `mode` instead of the opening movie: the value main's
  `mode_select_next_mode(6)` chose is replaced when main's `mode_dispatch`
  restarts into the dispatcher, before the dispatcher reads it. It is the
  runtime's form of the reference tools' patch of that call's argument.
- **Go anywhere.** `enter_field {map, entry?}` enters a field map the way the
  original's development kernel menu does: once the boot reaches the opening
  movie request, it restarts the dispatcher into mode 0 (the kernel menu),
  writes the map (and the entrance, `entry`) into `game_data`, selects mode 1
  and lets the menu finish into the original dispatcher. The entrance is
  `game_data.entry[2]`, which the field reads as its v0002 and places the
  party by, as a door's map change leaves it (`entry[0]` is the heading).
  From a field that runs it leaves that field as its scripts' "end field
  mode" event does instead of restarting the dispatcher: encounters held
  off, the field loop then frees its party sprite blocks, heap and sound
  bank and starts mode 0. A restart there left them to the next field,
  which loaded over them (field 30 after field 12's dialogue made its
  party's sprite from garbage and trapped). The field's menu runs a loop of
  its own, so with it open `enter_field` is refused, the reason given
  (`tests/route.rs`).
  `enter_field {map: 23}` reaches the
  forest map's opening dialogue (`tests/route.rs`) and matches the original
  capture of the same recipe (tools/reference/scenario.py) within 0.013 mean
  colour difference; on map 414 each `entry` puts the party on the map's
  entry record of that number (`tests/route.rs`).
- **Settings → Debug.** The shared panel launches the original kernel menu's
  Field, Battle, Worldmap, Battling, Menu and Movie operations. Field includes
  a **Go to developer room** shortcut (map 0, entrance 0). Launches restart
  the session; save progress first. Battle and Menu take their selector values
  from the panel, then run the original game's loading and setup code.
  The same typed requests are available as `debug {request: {tool: "field",
  map: 0, entry: 0}}` (the panel sends `AppRequest::Debug`). `debug_status`
  reports progress; normal frames advance it, or `debug_tick` runs a bounded
  batch while a host is paused. Pending launches reject another launch and
  save-state creation; Reset or restoring a state cancels them. Requests are
  recorded for replay. `xem-headless/tests/debug.rs` exercises all six modes,
  selector parameters, the developer room and the request lifecycle.
- **Routes and regression.** `tests/reference-inputs/*.json` are pad routes
  (frame ranges and buttons) played identically by `xem-headless --inputs`
  and the reference emulator (`tools/reference/observe.py`); the captures of
  the original environment stay in `.local/scenarios`. `tools/compare_frames.py
  RUN_DIR CAPTURE_DIR` scores checkpoint frames against a capture at the best
  vertical offset. `tools/native_scenario.py OUT_DIR --capture CAPTURE_DIR` runs a
  scenario program capture (tools/reference/observe.py `--program`) on the
  runtime with the reference's own executor (`tools/reference/scenario_program.py`:
  the same conditions, fingerprint-guarded writes, held buttons and capture
  labels, over `xem-headless --control`, whose `screenshot {path}` and
  `vram {path}` write the display and VRAM, raw halfwords for a `.bin`) and
  compares, at every labelled capture, `game_data`, the RNG seed,
  the field event variables and the whole RAM, and the frame each step
  completed. `--scenario FILE` runs a scenario JSON compiled as
  tools/reference/scenario.py does, without a comparison; `--card1/--card2`
  give the cards' first images (`unformatted` for a blank one);
  `--sample-memory` records the reference sampler's ranges every frame;
  `--fight-random-battles` makes it an agent that fights any battle with
  Cross and resumes the program after it. `tools/field_agent.py` is a direct agent that
  walks a map by the game's state: it reads the party leader's position
  through the schema each frame and holds the direction that points best at
  the next waypoint of the path the original took (a sampled capture),
  fighting battles on the way; it reaches field 22's save point and saves.
  `--log FILE` on both writes a session log: every state-changing command at
  the frame it was given, for `schedule`.
- **Session logs.** `schedule {commands: [[frame, command], ...]}` runs each
  command before the frame with that number (frames run since boot, `status`'s
  `frames`), so a recorded session replays on any host that speaks these
  commands. `tests/reference-inputs/slice-{forest,save,load}.json` are the
  Phase 3 slice: the forest route (entry dialogue, encounter, battle,
  rewards, return, menu), the agent's save at field 22 and the load of that
  card from the title. `src/runtime/crates/xem-headless/tests/slice.rs`
  (local, release) replays them to the recorded RAM/scratchpad hashes,
  stepped and unthrottled, with spectator output (presentation capture,
  sound output, screenshots) attached, changed and detached on the way, from
  a snapshot restored in battle and from a pad recording.
  `tests/reference-inputs/world-map-lahan-exit.json` leaves Lahan by its
  south exit for the world map (`enter_field {map: 1}`, two assisted walks
  kept as the `assist` intents they resolved to, and two presses), ending on
  foot on the world map at frame 2560; `xem-headless/tests/saves.rs`,
  `states.rs` and the browser's state interchange start there. A log that
  enters a field runs frames inside that command, so it is replayed to its
  frame count from boot (`run_until {frames}`), not by a `frames` count.
  The runtime counts
  the frames it completed (kept in snapshots), which `status` reports and
  `run_until {frames}` waits for. The slice's media is checked at the
  opening movie's own frames: `tests/reference-inputs/new-game-movie-frames.program.json`
  captures at movie frames 400 to 4000 (`field_movie_frame`), on the original
  (observe.py `--program`) and natively (`native_scenario.py --capture`): equal
  movie frames show the same picture (mean difference 0.02-2.6 of 255; 15 at
  frame 400, whose engine flame changes every frame) at the same pace, 400
  movie frames per 1600 vertical blanks. `src/runtime/crates/xem-headless/tests/route.rs` (local, it
  needs disc 1) checks that the new-game route's RAM and scratchpad do not
  depend on presentation capture or on a snapshot restored on the way, and
  the browser suite checks the same route's `memory_hash` against
  `xem-headless --control`.

### Gameplay crash exploration

`tools/game_explore.py` explores short gameplay branches from a shared corpus
of snapshots, without replaying the story before every experiment. Its default
`frontier` strategy targets actors, dialogue choices, menus and battle commands
through the existing pad/assisted-walking controls. Build the optional probes
before Asyncify, then use the same native runtime (commands from the repository root):

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/runtime
python3 tools/game_module.py --coverage --stubs --out build/game-coverage
XEM_GAME_WASM="$PWD/build/game-coverage/game.wasm" \
  CARGO_TARGET_DIR="$PWD/src/runtime/target-coverage" \
  cargo build --manifest-path src/runtime/Cargo.toml --profile suite -p xem-headless
python3 tools/game_explore.py \
  --headless src/runtime/target-coverage/suite/xem-headless \
  --coverage build/game-coverage/coverage.json \
  --stubs build/game-coverage/stubs.txt \
  --disc 'discs/Xenogears disc 1.chd' --field 23 --seed-frames 400 \
  --out build/explore/fields --iterations 10000 --seconds 60
```

The process is:

1. Seed different gameplay systems and story prerequisites directly, using
   retained input logs, validated saves/checkpoints and targeted scenarios.
   `--log` checks a log's recorded RAM/scratchpad hash when present; `--field`
   enters a debug field scenario. `--add-log` and `--add-field` add more seeds
   to the same search. With no supplied seed, the retained new-game route is used.
2. Try short, distinct actions at each checkpoint. Retain new engine blocks,
   field-script dispatches/outcomes, and coarse map/story/position progress.
   Prioritize productive chains and rare coverage; deprioritize repeated NPC
   attempts across nearby positions. Non-choice dialogue/events advance for up
   to `--settle-frames` (default 900) before another snapshot is needed.
3. Reuse the corpus with `--resume PREVIOUS_OUTPUT` in a **new** output directory.
   This restores checkpoints directly and merges their measured coverage and
   full replay witnesses. Repeat `--resume` to combine runs. `--max-corpus`
   bounds retained snapshots; every completed branch retains its input lineage.
   A changed module rejects old snapshots. Use `--add-replay PREVIOUS_OUTPUT/replay-suite.json`
   to rebuild a corpus from its short input witnesses, remeasuring coverage
   instead of importing old feature IDs; mixed-disc suites use each case's boot disc.
4. Reduce repeat testing with `replay-suite.json`: greedy cost-weighted selection
   keeps witnesses for every feature observed in the run. Replay it with
   `--replay PATH/replay-suite.json`; it runs each selected case from boot,
   using its recorded disc. It also runs on an ordinary, uninstrumented binary.
5. Inspect the remaining blocks/scripts and add the appropriate seed or short
   transition chain. A plateau means another state or action is needed; it is
   not a declaration that the remaining code cannot run.

For example, add a world-map route without repeating the field setup:

```sh
python3 tools/game_explore.py \
  --headless src/runtime/target-coverage/suite/xem-headless \
  --coverage build/game-coverage/coverage.json --stubs build/game-coverage/stubs.txt \
  --disc 'discs/Xenogears disc 1.chd' --resume build/explore/fields \
  --add-log tests/reference-inputs/world-map-lahan-exit.json \
  --out build/explore/combined --iterations 10000 --seconds 60
```

Use disc 2 with its own seeds and merge its corpus in the same way. The running
game's disc directory identifies script observations, so equal map/PC numbers
on the two discs remain distinct. Original assets, states and generated results
stay local. Setup writes/debug entries are labelled `scenario`, not legal
progression. Branching preserves that origin and never edits story flags,
inventory or battle outcomes. A scenario that violates prerequisites still
needs a valid save/route before a finding is called player-reachable.

Each optimized LLVM block has its own byte, outside the game's and port's
reserved memory, set by a volatile store. No host call runs per probe, there
are no block-bitmap collisions or wrapping counters, and linking the probes does
not move game-visible port/stack addresses. An initialized signature rejects
a stale map or an ordinary binary. The map correlates probes to the compiled
source, image, function and block. It includes unvisited and unlinked blocks;
zero hits are never silently classified as unreachable. A separate observer
brackets the field interpreter's actual primary-opcode dispatch, recording
`(disc, map, PC)` and its completed `(PC, next PC)` outcome. This sees instructions
that begin and finish within one frame. Its sparse table compares exact keys;
collisions are resolved, and overflow fails the measurement rather than hiding
coverage. Epochs clear it in constant time. The loaded disc/map identity occupies
eight fixed bytes at the end of the reserved port snapshot region, so restoring
a checkpoint restores its identity too. Pending next-map requests cannot relabel
the old map's remaining scripts. The decomp is unchanged.

`--seconds` limits search time after setup; `--iterations`, `--seed` and
`--timeout` bound the search and its independent process watchdog. `--strategy
random` retains a single-seed random-input control experiment (`--stall`
controls its rewinding). Compare observed gains under the same budget; one
strategy is not assumed best for every state.

`summary.json` reports the run's completion, failures, blocks per image and
field-script counts. `coverage-hits.json` plus `coverage.json` identify engine
blocks; `coverage-script-hits.json` stores exact script keys. Their encoding is
`disc << 45 | map << 33 | pc << 17 | outcome`, with outcome `0x10000` meaning
dispatch observed and a halfword meaning the resulting PC. `steps.jsonl` records
gain, action and time. The replay suite's cost totals are estimates from measured
prefix/branch costs, not timings of an independently rerun suite.

These measurements do not establish all-path coverage or crash freedom. Field
outcomes are handler results, not every possible history through them; battle
AI/effect bytecode, story prerequisites, data variants, loop counts and action
ordering still need targeted seeds/short chains. Static candidates can be
enumerated with the existing `tools.analysis.events`, `battle_ai`,
`battle_event_vm`, `battle_effect_vm` and `formations` tools; keep generated
listings local. Do not equate a map-start sweep or a coverage plateau with
completion of both discs and optional content.

Before each input sequence, `pending.json` records its checkpoint and full
input lineage. Traps, query traps, exits and timeouts leave `failure.json` and
`stderr.log`. Reproduce with `--replay build/explore/NAME/failure.json` plus
`--headless` and `--disc`; add `--from-boot` to replay the complete lineage,
including on an ordinary build. A confirmed matching failure exits 1, a
different failure exits 2, and a run without a failure exits 0. A timeout is a
watchdog finding, not by itself a confirmed deadlock. Confirm scenario findings
on a legal route before claiming a player can encounter them. Coverage-build
snapshots have their own module identity and cannot load into an ordinary build.

`tests.test_game_coverage` executes synthetic wasm branches/PHIs and the field
observer, including distinct discs/maps, alternate outcomes, duplicate hits,
epoch reset and overflow. It checks map rejection, watchdogs, ordinary-pad
choice enumeration and coverage-preserving replay reduction. The desktop/full
suite runs it. Keep ordinary-build regression comparisons and retained exact
hashes as the independent check that instrumentation preserves game outcomes.

## Discs

`xem-disc` reads the user's images in place as raw MODE2/2352 sectors: CHDs
(by their signature), cue sheets with their `.bin`, and raw tracks (`.bin`,
`.iso`). An image is identified by its boot program (`identify`) and, when
that is a Xenogears disc, accepted only as Redump's dump of the North
American disc ([disc 1](http://redump.org/disc/177/), SLUS-00664,
718 738 272 bytes, SHA-1 `12db8ccb…21f4`; [disc 2](http://redump.org/disc/178/),
SLUS-00669, 688 700 880 bytes, SHA-1 `0a4c56e6…665c`; the original and
Greatest Hits editions alike; `verify`, `Verification`). A raw image's whole
track must have that size and SHA-1 (about a second natively); a CHD is
checked by the SHA-1 its header records of its raw data, which must be that
of `chdman createcd`'s CHD of Redump's dump (`KnownDisc::chd_raw_sha1`),
since decoding a whole CHD to hash its track takes most of a minute. The
CHDs in `discs/` are those CHDs: their decoded tracks and their extracted
`.bin`s hash to Redump's values (`xem-disc/tests/user_discs.rs`). A
2048-byte-sector ISO, which keeps none of the XA audio and movie sectors, is
refused with a hint; a disc that is not Xenogears is identified as such
and not booted. Native boot (`xem_game::boot`) verifies the disc it boots
and the discs beside it, Android each import on its worker thread, and the
browser each import in a `verifying` stage that reads ahead 4 MiB at a time.

**The disc changer.** The game asks for its other disc in three places
(`menu_cd_ask_for_disc`): the story's disc change at the end of disc 1 (menu
kind 6, which offers a save first), a new game started from disc 2 (the
title's file screen wants disc 1) and a file loaded from the other disc.
Each waits in `menu_cd_check_disc` for the lid to open and close, the motor
to spin and the disc's label (sector 0x17) to name the disc, then loads its
file index. The port wraps that check (`src/port/disc.c`, game_module.py's
`WRAPPED`) and tells the drive which disc it waits for and whether this is
the story's change (import `cd_wanted`). The drive's changer
(`xem-core/src/devices/cd.rs`) then opens the lid and puts that disc in, up
to speed and with the head at its start; the lid closes a vertical blank
after the game last read its status with it open (libcd reads the status
again while it reports the lid open, so the check must find it open
throughout a poll), so the check goes on at once, about half a second to
the new disc's file index. At the story's change this happens five seconds
of game time after the request, with the game's change-disc notice shown
(fast-forward shortens it); elsewhere at once. All of it runs on the
game's clock, so recordings, rewind and replays reproduce it. Without the
wanted disc the game waits at its notice; the hosts ask for it (the panel
opens on Saves with the request in its error bar) and the change happens
when the host adds it (`Services::add_disc`). Natively `xem_game::boot`
gives the changer the game's other disc from the boot disc's directory
(`xem_disc::discs_beside`: the first CHD, cue, ISO or raw track there that
is the other disc and verifies). Android and the browser keep both
imported discs: the game boots from disc 1 when it is there, else disc 2,
the other goes to the changer, and a disc imported while the game runs goes
to the changer rather than starting it again.

**Save states.** A snapshot names the game rather than the boot disc
(`state_identity`: both discs hold one program, their boot programs
differing only in the file index each embeds, which the game loads again
from the disc it changes to) and keeps the number of the disc in the drive;
restoring puts that disc back in, or is refused when the session has not
got it. A state saved while the game waits for a disc its session lacked
takes that disc at once in a session that holds it (`Cd::restore_state`,
as when the host adds it; at the story's change the five-second wait is
not taken again). A state saved with either disc in the drive restores in a session
booted from either (`xem-headless/tests/discs.rs`, with both discs: a new
game on disc 2 gets disc 1 at once and a state from before the change goes
on identically in a session booted from disc 1; without disc 1 the game
waits until it is added; a state saved in that wait, restored in a session
booted from disc 1, takes disc 1 and the new game begins; the story's
change waits five seconds; and
`web/tests/disc.test.mjs`: in the browser, disc 1 imported while the game
waits for it goes in). A state saved in the story's five-second wait by a
session that holds disc 2 keeps the change due at its cycle: restored in
that session or in a fresh one booted from disc 1 with disc 2 beside it,
disc 2 goes in at the frame and cycle where the session that saved it puts
it in without a restore, and the game goes on to the same RAM
(`xem-headless/tests/states.rs`). The save the story's change offers asks
for disc 2 (its vars\[82\] is 1), and loaded from disc 1's title the
changer puts disc 2 in at once (`xem-headless/tests/saves.rs`).

## Sound and memory cards

- **libspu** (`src/port/libspu.c`) is the resident's libspu from its original
  code: its state at the original addresses, its register writes through
  `libspu_spu_registers` into the window at 0x1F801C00, where the sound
  driver writes the voice registers too. The SPU device
  (`xem_core::devices::spu`, over `xem_spu::Spu`) applies the window after
  every step and interrupt and at each of libspu's hardware waits
  (`xem.spu_sync`), the crate's register-window contract. DMA channel 4
  (`xem.spu_transfer`) moves whole 64-byte blocks at once and raises the
  channel's interrupt after 16 cycles per halfword (the rate the DuckStation
  emulator models; a 64 KB bank takes about 15.5 ms); libetc runs libspu's
  completion entry 0x8004CB3C for it, which calls the transfer callback or
  delivers the transfer event. The SPU IRQ reaches InterruptCallback(9).
  `advance` runs whole 44.1 kHz samples up to the clock (768 cycles each),
  into an output ring when a host plays audio (`Spu::set_output`,
  `take_audio`); headless runs advance the same state without output. CD
  audio reaches the SPU's CD input from the CD device
  (`Cd::take_audio`, `Services::route_cd_audio`) before the SPU advances.
- **Sound modes.** The configuration menu's Mono, Stereo and Wide are
  driver modes 0, 1 and 2 (`sound_set_output_mode`; mode 3 exists but no
  menu reaches it). Mono gives every voice the centre gain and leaves CD
  audio as Stereo has it (the driver's CD mix, `sound_set_cd_mix_volume`,
  which would halve it into both sides, runs only behind driver flag
  0x4000, which nothing sets); Stereo and Wide share the pan law. Wide only flips
  signs (`sound_set_stereo_volume`): the master volume's right side and the
  reverb depth's left side. The SPU applies the master volume after the
  reverb and CD mix, so the dry mix, XA included, comes out in anti-phase,
  and the reverb return in phase. On the sample-aligned stretches of the
  native Stereo and Wide routes (`tests/reference-inputs/new-game-sound-mode-*.json`,
  60-460 s), R(wide) = -R(stereo) to -64..-72 dB. L is identical without
  reverb and differs only by twice the reverb return with it.
  Wide is not a matrix-surround encoding: it has no surround channel, no
  ±90° phase shift and no band limit. `tools/surround.py` decodes WAV
  windows with FreeSurround (the `freesurround` filter in the nix/runtime
  shell, a test tool only, GPL-2.0+). Through such a decoder Wide moves the
  whole dry mix, including centred dialogue and movies, to the rears.
  The reverb stays in front:

  | window (s) | Stereo FC / rear | Wide FC / rear | original Stereo, Wide rear |
  |---|---|---|---|
  | 60-70 (XA) | 0.351 / 0.206 | 0.033 / 0.755 | 0.200, 0.752 |
  | 120-130 (XA) | 0.355 / 0.203 | 0.032 / 0.718 | 0.205, 0.722 |
  | 400-410 (SPU, reverb) | 0.246 / 0.117 | 0.021 / 0.585 | 0.106, 0.586 |
  | 550-560 (SPU) | 0.501 / 0.087 | 0.008 / 0.824 | 0.080, 0.793 |

  These are energy shares of the 5.1 output. "Original" means the
  PCSX-ReARMed recordings in `.local/scenarios/p1-original-sound-*-20261002`,
  which give the same picture. `xem-headless/tests/original_sound.rs`
  plays the three routes natively (from boot, one thread each, 30 s on
  this machine unloaded) and compares them with the recordings in the
  two windows `tools/analysis/audio_modes.py` reported on
  (`p1-original-audio-modes-20261002.json`, whose figures it first
  reproduces from the recordings). The relations hold as recorded:
  R(wide) = -R(stereo) within 1 (the recordings: 3), and in the opening
  movie (frames 4 200-4 800, CD input only) Mono is Stereo and L(wide) is
  L(stereo) sample for sample. The movie's left channel is the
  recording's own waveform (correlation 0.9998, natively 540 ms later),
  and so is the field's dry left part, (Stereo + Wide) / 2 (0.98, 70 ms
  earlier; frames 22 200-22 800); the field's reverb return,
  (Stereo - Wide) / 2, matches the recording's only to 0.67 and is 0.7 dB
  weaker against the dry part: xem-spu's reverb unit (psx-spx's formula at
  22.05 kHz through its 39-tap resampling filter) and PCSX-ReARMed's
  differ. Correlation, sum/difference and FreeSurround's rear share agree
  within 0.015, 0.15 dB and 0.009 in the movie (declared 0.03, 0.5 dB and
  0.05); in the field, where the reverb decorrelates the channels, the
  native mix is 0.04 (Stereo, Wide) to 0.08 (Mono) more correlated and up
  to 1.6 dB higher in sum/difference (declared 0.1 and 2 dB, which still
  tell the modes apart), the rear shares within 0.04. The native level is
  1.22 times the recordings' in the movie and 1.33 for the field's dry
  part, printed and never normalized.
  The hosts' Sound mode setting is this mode, not a mix of their own: they
  play the SPU's output as it is, at the master volume ([Binaural sound
  mode](#binaural-sound-mode), the one exception, presents the game's
  Stereo again for headphones). `src/port/sound.c`
  wraps `sound_start_driver` and `sound_run_tick`: the driver takes the
  host import `xem.sound_mode` (0-2, or none) when it starts, and its tick
  takes a mode the host changed since (`xem.sound_mode_change`, once per
  change, and again after a state is restored), through
  `sound_set_output_mode`. The setting follows the game's own Sound screen
  (title screen and menu): the port wraps `menu_sound_mode_screen_run`,
  which tells the host the mode it leaves (`xem.sound_mode_chosen`); that
  becomes the host's mode, and the hosts apply it to their setting
  (`Session::take_sound_mode_choice`, `Game::follow_sound_mode`), the
  panel following through the settings service. Until a host has taken a
  choice, its own `set_sound_mode` is ignored, so a frame batch run with
  the old setting does not undo it. Whether the tick took it is the runtime's, not
  the port's: a port global would move the port's stack, whose addresses
  the game keeps in RAM (`libgpu_last_call_argument`), and change the
  hashes. The hosts set it before each frame with `Session::set_sound_mode`,
  recorded with the inputs as a control (`sound_mode`, also in session logs
  and replays; a recording starts with the mode and whether the game took
  it; a replay keeps the recorded one). Without one, and in Stereo, the
  game runs as the original.
- **Memory cards** (`xem_core::devices::card`) are two raw 128 KB images
  (the `.mcd` layout) in a `CardStorage`: files natively (`xem-headless
  --cards DIR` keeps `card1.mcd`, `card2.mcd`; a missing file is a fresh
  formatted card), memory in tests. The BIOS file calls on "bu00:" and
  "bu10:" (`src/port/bios.c`) and `_card_info` (`src/port/libcard.c`) run the BIOS's
  card file system over them; each access costs one 140-byte card command at
  250 kHz per 128-byte frame, during which the port suspends as the BIOS
  blocks, and a check ends with the card interrupt and its event (done,
  timeout without a card, new card for a card without a file system).
  The images are persistent saves outside snapshots; a state keeps the
  calls under way, which a restore checks against the inserted cards
  ([Saves and time controls](#saves-and-time-controls)). A card or save
  file the player imports replaces a card between the game's card calls,
  never during one ([Original saves and memory
  cards](#original-saves-and-memory-cards)). Both cards are in, as a
  console's stay in; the recorded control `card_present` takes one out or
  puts it back for agents and tests ([Cards in and out](#cards-in-and-out)).
- **Kanji font.** Krom2RawAdd returns glyph addresses in the BIOS ROM's font
  through its KUSEG mirror 0x1FC66000; the host places the font there from the
  player's BIOS image (Import BIOS, or `--bios FILE`), or substitute glyphs
  for what save titles hold without one (`xem_core::bios`; [Save titles and
  the Kanji font](#save-titles-and-the-kanji-font)).
- The PC file server (libsn `PC*`) has no development host and fails.

## Binaural sound mode

Binaural (Settings → Audio → Sound mode, beside the game's own Mono, Stereo
and Wide) presents the game's sound for headphones, in two channels: the
music's parts arranged around the listener and the world's positional
effects from where their emitters are, each at the gain the game gives it
(plan.md, Phase 8). It is a presentation: the game runs in its own Stereo
under it (`Settings::game_sound_mode`; Stereo's pan law is what the
placement reads), and nothing it does reaches game memory, the sound driver
or the SPU. Turning it off gives back the original mode chosen before
(`Settings::audio_output` keeps it); choosing an original mode, in the panel
or on the game's own Sound screen, turns it off.

- **What the SPU mixed.** `xem_spu::Spu::run_tapped` says what each output
  sample was mixed from (`TapFrame`): each voice's sample after its
  envelope, the volumes it was mixed with, the reverb return, the CD input
  as mixed and the main volume; the SPU advances exactly as without it.
  While the mode is on, or fading out, the SPU device (`devices/spu.rs`)
  renders its ring from that through `xem_binaural::Renderer`. Headless runs
  keep no output and render nothing.
- **Who keyed a voice.** At each register sync that keys voices on, the
  presenter (`xem_core::binaural::Presenter`) reads the driver's
  `sound_voice_owners`: a channel of a playing sequence (a part of the
  music), one of the effect channels (an effect start, by its id and start
  tick; an effect's two channels are one source), or neither. A voice keeps
  its identity through its release until it is keyed on again. A restore
  forgets them all; the presenter reads the restored scene
  (`Session::game_state_restored`) and then the sounding voices at the next
  sync.
- **Gains.** A placed voice loses its pan and keeps its gain: the driver's
  pan law (`sound_stage_seq_voices`: two linear ramps, 0x5A00 on both sides
  at the centre) is inverted from the voice's own volumes
  (`xem_binaural::pan::unpan`), so the source gets exactly the gain the
  driver resolved (its fades and the field's distance volume included),
  once. Every direction's HRTF pair is levelled to one loudness (pink
  noise through BS.1770's K-weighting, both ears: `hrtf::PAIR_ENERGY`), so a
  source ahead is as loud as the original mix's centre and as loud
  anywhere. Nothing is added: no distance law, culling, occlusion, air
  absorption, Doppler or room. The main volume and the SPU's mute act as in
  the SPU.
- **Music.** The first time a part of a sequence plays, the presenter
  profiles every part from the sequence's data in game memory (notes,
  pitch with the instrument's note offset, lengths, drum-kit and noise
  notes, the pan before the first note; the opcode lengths of
  `tools/analysis/sound_sequence.py`), and `xem_binaural::music` arranges
  them by its rules: rhythm (half its notes drum-kit or noise), bass (the
  lowest melodic part, 9 semitones or more below the median), pad (half
  notes or longer on average), lead (the busiest part at or above the
  median, 16 notes or more), supporting parts, and uncertain ones (under 4
  notes, or data not read to its end). The lead, bass and rhythm stay where
  the music panned them ahead (the stereo arc: their pan as a direction
  within 30 degrees of ahead), as do uncertain parts; supporting parts and
  pads the music panned a quarter of the way or more to a side go to that
  side (supports at 70 to 130 degrees, pads 25 degrees up), and those it
  left near the centre keep the mix's balance (supports at the rear centre,
  pads 40 degrees up); at music immersion 0 every part is on the arc. A part keeps its place for the whole song,
  turned by the music's own pan movement (pan slides, autopan:
  `Target::Stage`, `spread` degrees per 0x4000). The stage is the
  listener's: character turns, camera swings and cuts leave it; under
  *VR: Stationary music* a headset's head turns are heard against it.
  Reviewed per-song changes would go in `OVERRIDES`; none were needed yet.
- **Effects.** World effects go where their emitters are: the field's three
  positional emitters (`field_sound_emitter_slots`, each following an
  actor), the arena's four positional voices (`arena_sound_voices`, a fixed
  point or a followed vector), and the sprites whose animation scripts play
  sounds (a character's swing, a spell's burst, a Gear's step: sprite
  commands b9 and b0, the battle's 4f, 52 and 56). For those the port tells
  the host which sprite runs the command and which effect the driver starts
  (`src/port/binaural.c` wraps `sprite_vm_run_generic_command`,
  `battle_sprite_command_run` and `sound_start_effect`; the calls only
  tell, the game runs as the original), and the sound follows the sprite's
  position, in the view sprites are drawn with (`sprite_view_matrix`), while
  the sprite is still that sprite (its resource, animation, bank and
  allocation words unchanged). The direction is from the listening
  position to the emitter, in the camera's orientation, then through the
  first-person view's axes (inspection, on every host) and a headset's
  head, each once. The listening position is the field's controlled character, the
  arena's player Gear, and in a battle the centre of its formation's
  occupied slots on the floor (their placements, fixed for the battle, at
  the combatants' average height), which no turn moves from one combatant
  to another. The distance is dropped: the game's own volume keeps its
  distance law. An emitter at the listener keeps the direction it had.
  Emitters are read with the key-on that starts them and after every frame;
  a start keeps the emitter it came from, so when its slot takes another
  actor (the original's nearest three changing), even for the same sound,
  the start's tail stays where it was rather than following the newcomer.
  Every other effect (interface sounds, effects field and battle scripts
  start and pan themselves, the world map's) keeps its original mix and is
  counted as a coverage gap by mode and effect id; an emitter that is gone
  (its scene left, its slot reused, its sprite freed) leaves its last voices
  where they were.
- **The rest.** The reverb return is the SPU's own (the original sends,
  type and depth changes), mixed once as it came. Voices nobody owns keep
  their mix. CD input keeps its mix while a movie plays (the MDEC decoded
  within the last half second); otherwise it is taken for premixed music
  and presented as a labelled estimate, `estimated arrangement`: its pair
  as two loudspeakers 30 degrees either side of ahead and part of its side
  signal moved to two at 110 degrees, delayed apart so they stay diffuse,
  the mid and the side's energy kept. On the routes checked no CD input
  sounds outside a movie: the title movie's music (frames 360 to 1 350
  from boot) keeps its mix as every movie's does, and the title screen is
  silent once that movie ends (from frame 1 380 to 3 360 with no input,
  when the opening movie begins); the sound-mode findings' "title music"
  window holds the title menu's sounds. The listening test below hears the
  estimate on the title movie's music, mixed again.
- **The renderer** (`xem-binaural`, the same Rust natively and in the
  browser) works in blocks of 128 samples (2.9 ms, its only delay): each
  heard source's block is convolved with its direction's HRTF pair by FFT
  (two sources to a transform) and the ears go back to time once. Directions
  glide (30 ms), a source fades between placed and its original mix (50 ms)
  and the whole presentation crossfades with the SPU's own mix (100 ms);
  under the diagnostic bypass or at rest it outputs the SPU's samples
  themselves. A filter is remade once its direction has moved half a
  degree; the block it is remade in fades, sample by sample, from all the
  old filters give (the block and the last one's tail through them) to all
  the new ones give (the last block's tail through them too), so neither a
  filter nor its interaural delay changes from one sample to the next. A
  glide past the front (a camera cut, the music's immersion changed) moves
  up to 6 degrees a block; without the fade each block boundary stepped,
  a zipper 6 to 15 dB above the high band around it. Above -1.4 dBFS a
  soft knee bends peaks towards full scale instead of clipping.
- **HRTF data.** MIT KEMAR, diffuse-field equalized, at 44.1 kHz
  (`crates/xem-binaural/hrtf/README.md`: source, checksums, terms and the
  citation the About section repeats; `tools/binaural_hrtf.py` repacks the
  archive). At start each response is cut from just before its onset (92
  taps), the onsets' difference between the ears kept as a delay, applied
  by phase after mixing the four measured directions around a source, and
  every pair levelled. The set holds no bass (its diffuse-field mean falls
  from 0.92 above 500 Hz to 0.65 at 172 Hz and 0.30 at 0 Hz, alike in
  every direction): a low shelf fitted to that mean (`Hrtf::bass_shelf`)
  restores it on the placed sound.
- **Settings** (`xem_settings::Binaural`, which is
  `xem_core::binaural::Config`): enabled, spatial music, positional effects,
  music immersion (0-100, 70 by default), head-tracked music, and a diagnostic
  bypass available through the service (not the Settings panel). The
  hosts apply them with each frame batch (`Session::set_binaural`; a
  presentation starting reads the scene at once, so the voices sounding
  then are placed by their emitters at the next register sync rather than
  counted as gaps), with the
  first-person view's axes (`GameView::listener_axes`; while a headset's
  diorama shows, the seat's axes through its table), each with the camera
  of the frame shown that they are in (`xem_core::binaural::ListenerView`:
  the presenter hears them through it, not through the camera the game
  holds at the frame's end, which leads the picture by one to three frames
  in the forest's battle, so the game's camera turning after the frame
  shown moves nothing heard) and a headset's head
  (`xem_core::binaural::head_axes` of the OpenXR or WebXR viewer pose as
  the seat sees it, the seat's forward its reference, so a recentred seat
  is ahead again; lost tracking keeps the last; leaving the headset clears
  it: the browser's session end does, and so does the OpenXR loop's end,
  which takes the listener's view away too before it gives the game back
  to a flat host, Android's window, whose own view follows). The view's
  axes and the head reach the presenter whenever the host
  computes them, every frame it presents, the game running or held
  (`xem_host::Game::set_listener_view`, `set_head`; the browser's step):
  presentation only (a held game sounds nothing), but the presenter holds
  the presentation's listener at once; in a headset held by the player's
  Pause a snap turn, inspection ending or a tilted table used to reach it
  only when the game ran again. The hosts' audio stays plain two-channel stereo at
  the SPU's 44.1 kHz (SDL and the browser resample to the device), so no
  platform spatializer is stacked on it, and an audio device or its rate
  changing, pausing and resuming reach the presented sound exactly as they
  reach the original: the renderer works before the host's queue, at the
  SPU's own rate and clock, and never reads the device or the wall clock.
- **Agents.** Agents change the mode as people do, through the settings
  service: `settings_apply` with `{"change": {"Binaural": {...}}}` on the
  desktop's `--control`, `xem.settings.apply({Binaural: {...}})` in the
  browser. Without a host (xem-headless) the session's `binaural` command
  changes the settings given by name. It reports what is presented: each
  source (music: song, part, role and `placement`, its place on the stage
  at the immersion set: azimuth, elevation, `home_pan`, `spread`; effect:
  id, `start` tick, channels and emitter), the direction it is heard from
  now (glided) and its `target`, where it is heading (null while it keeps
  its direction), its voices' `pan` in the last block, its pre-pan gain and
  level; each voice's identity (an effect's start too) and route, the
  listener, the first-person and head turns, the CD input's route and the
  coverage gaps. `audio_capture` (`action` start or stop, `samples: true`
  for the samples as hex) keeps the presented output, whoever takes the
  ring, for offline checks. With `tap: true` at the start (only while
  Binaural presents) it also keeps the renderer's journal
  (`xem_binaural::Journal`, presentation only, at most 30 s at about
  7.4 MB a second): the renderer as it was, every sample's tap and every
  change to its routes and sources, from which `Journal::replay` mixes the
  same output again sample for sample, or any part alone. The stop then
  reports, block by block in runs, which voices each source had sounding
  (`tap.routes`), with the capture's `frames` and `lead` (the output
  samples before its first). `xem-headless --binaural` (or
  `--binaural-config JSON`) with `--wav` writes the output to a file and
  prints the status. OpenXR's `xrframe` line has `binaural=`: what the
  presenter holds that frame, as JSON without spaces (`-` while nothing
  presents): `head` and `view` (the head's and the listener view's facing,
  `[yaw, pitch]` degrees, yaw to the right; null without), `at` (the
  listening position) and `effects` and `music` (each placed source's
  `[id, azimuth, elevation]`, where it heads); the session's `exit` line
  has it once more, the head and the view taken away. It is
  `Presenter::heard`, only the status's part the line needs: in field 15
  (9 sources, after 600 frames; the suite profile, 20 000 lines on this
  machine, other builds running) the line takes 4.0 us a frame where taken
  from the whole status it took 75 us, the same 346 bytes. WebXR's page reads the same
  through the `binaural` command (`head`, `first_person`, `sources`).
  `xem-headless`'s `first_person` example prints the status a native
  desktop host has looking around in first person from a state and look
  angles (`--state FILE --look YAW,PITCH`), for the browser's comparison.

**Calibrated computational listeners.** Nobody has listened to the mode
yet. In people's place, localization models fitted to listeners' own
localization experiments (the Auditory Modeling Toolbox's) hear the
renderer's measured responses: they predict how those listeners would hear
the KEMAR head's cues as the renderer delivers them, for static broadband
sounds, the conditions the models are validated in. They do not replace
people.

- *The responses.* `cargo run --release -p xem-binaural --example
  rendered_hrirs` measures what the renderer does to a voice placed in a
  direction, through `Renderer` as the game's voices reach it: a voice at
  the pan law's centre plays a periodic noise (a flat spectrum with random
  phases, 8 192 samples a period); the output's average over five periods,
  after a first in which the filters' tails and the bass shelf settle,
  divided by the noise's spectrum is the response, per unit of the
  source's signal at its gain before panning, kept for 2 048 samples from
  64 before the voice's sample (the fractional interaural delay rings on
  both sides of it, and a block's convolution can put it ahead). The output
  peaks at 0.133 of full scale, far under the soft knee (0.85); the noise
  floor is at least 83.7 dB under each response's peak, and at most
  1.3e-5 of a period's energy, that floor included, falls outside the
  samples kept (7 s for all of it on this machine). It writes
  `build/binaural/` (`hrirs.json` and one float32 file a set): the 710
  measured directions, a 5-degree grid (1 873: azimuth 0 to 355, elevation
  -40 to 85, and straight up), the arrangement's 24 places at full
  immersion (`music::ARC` and half of it either way, `SUPPORT_SIDES` on
  both sides and `SUPPORT_REAR` 5 degrees up, `PAD_SIDES` on both sides 25
  up, `PAD_CENTRES` 40 up) and the estimate's four loudspeakers
  (`UPMIX_SPEAKERS`); the KEMAR set's own responses at its measured
  directions (`Hrtf::measured`); and a control, the original Stereo pan law
  over headphones (a direction's lateral position, sin(azimuth)
  cos(elevation), as the pan: two gains, no other cue). At the measured
  directions the rendered responses keep the raw ones' cues: the
  interaural delay the same to an eighth of a sample (broadband
  cross-correlation), the level difference between the ears within 0.75 dB
  (median 0.19; broadband, the bass shelf included), each ear's spectral
  shape from 700 Hz to 16 kHz within 1.65 dB (the standard deviation over
  frequency of their level difference; median 0.15, 0.60 at the 95th
  percentile), and their level moved -4.5 to +0.4 dB (median -2.4) by the
  levelling.
- *The toolchain.* `nix develop path:./nix/amt` pins Octave 11.3.0 (the
  runtime shell's nixpkgs) with its io, netcdf, signal and statistics
  packages, and AMT 1.6.0's full package (GPL-3.0+, with LTFAT 2.6.0, the
  SOFA Toolbox 2.2.0, SFS 2.5.0 and Circular Statistics), with what its
  models would download on first use, fetched by hash from the AMT's
  server: the baumgartner2014 pool's own directional transfer functions
  (23 listeners, ARI, 48 kHz), their localization experiments (goupell2010,
  majdak2010, majdak2013, majdak2013ctc) and the toolbox's calibration of
  their sensitivities. It is a local test tool, like FreeSurround: nothing
  in the runtime uses it, and none of its data enters the repository. Two
  changes let it run from the read-only store: SFS's `rms.m`, which
  `amt_start` deletes under Octave, is left out, and the SOFA Toolbox
  counts the compiled conventions it ships as compiled when their time
  equals their tables' (the store gives every file one time). Run
  `nix develop path:./nix/amt -c octave-cli --no-init-file
  tools/binaural_models/evaluate.m` from the repository's root: it puts the
  toolboxes on the path before `amt_start` (which would otherwise save the
  path to `~/.octaverc`), starts it offline (`localonly`), writes
  `build/binaural/models.json`, keeps what it computes once
  (`models-cache/`: each listener's lookup takes a minute or two) and
  exits with status 1 when a check fails.
- *The toolchain's check.* First the pool hears its own ears: each
  listener's predictions (baumgartner2014 with their own DTFs as target
  and template, their own sensitivity, their experiments' targets within
  30 degrees of the median plane in three lateral segments, weighted by
  their targets) against the published ones (Baumgartner, Majdak and
  Laback 2014, JASA 136(2):791-802, Table 1). The targets' counts are
  equal, the actual errors equal as rounded, the toolbox's sensitivities
  within 0.013 of the table's, and the predictions within 0.17 points of
  quadrant error and 0.27 degrees of local polar error (0.23 and 0.22 with
  the table's own sensitivities, which it rounds to two places); the
  bounds are 0.5 each. Nothing else runs otherwise.
- *What the models hear.* baumgartner2014 for the sagittal planes, at
  lateral angles -30 to 30 in steps of 10 (the pool's calibrated range),
  each plane's targets weighted alike: quadrant errors, local polar error
  and front/back confusions (of the targets at least 30 degrees from the
  frontal plane, the probability of a response on its other side). It takes
  a response's band levels over the response's own length, so the shorter
  sets (the KEMAR set's 128 samples, which would cut its low bands' filters
  short, more or less by where an onset falls) are zero-padded to the
  rendered ones' 2 048, which is what they are beyond their samples.
  dietz2011 for the lateral angle: half a second of white noise through a
  direction's responses, the model's interaural delays read through an
  ITD-to-angle lookup made from the listener's own ears
  (`itd2angle_lookuptable`, the pool's resampled to 44.1 kHz, from a
  second of white noise the toolbox draws itself), as
  `wierstorf2013_estimateazimuth` does (its own result is checked equal for
  a direction): lateral RMS error and bias (positive further out than the
  target). Both noises come from fixed states of Octave's generator (2011
  for every lookup, 2014 for the half second heard from each direction),
  so a run with an empty cache computes the same lookups and estimates
  (two such runs here wrote the same `models.json`). The listeners: the
  pool's 23, each through their own ears at their own sensitivity, and the
  KEMAR head itself through its own (the measured set, at the pool's
  median sensitivity, 0.754), for whom the raw targets are its template
  and the rendered ones differ from it only by what the renderer does.
- *Sagittal planes.* The pool, hearing the KEMAR head's cues (others'
  ears), confuses much: quadrant errors 33.9% (pool median; 30.1 to 40.8)
  with the renderer's responses and 33.6% with the raw ones, local polar
  errors 43.9 and 43.8 degrees, front/back confusions 41.9 and 41.7%
  (people listening through others' ears confuse front and back often
  too: Wenzel et al. 1993; the paper checks the model against such
  listening, Middlebrooks 1999, in its Fig. 9). The renderer adds 0.06 to
  0.60 points of quadrant error (median 0.24), -0.03 to 0.17 degree of
  polar error (median 0.06) and -0.05 to 0.44 points of front/back
  confusions (median 0.24) to what the raw responses get, listener by
  listener; the model's own prediction error against people is about 3
  points and 3 degrees (the paper's Table 2). The KEMAR head through its
  own ears hears the renderer as it hears the raw set (-0.14, -0.07 and
  -0.22): 26.8 against 26.9% quadrant errors, 36.4 against 36.5 degrees,
  32.7 against 32.9% front/back confusions. The Stereo control is near
  chance for front and back (49.2%
  confusions, pool median; 49.3 for the KEMAR head; a pan cannot tell
  them) with 41.2% quadrant errors and 53.1 degrees: Binaural confuses
  front and back less for 22 of the 23 listeners (NH71, 48.8 against
  48.6%, is the exception) and for the KEMAR head (32.7 against 49.3%).
  On the 5-degree grid's directions (other targets in each plane,
  between the measured ones) the pool's quadrant errors are 29.1% and the
  polar errors 43.4 degrees.
- *Lateral angles.* The pool hears the renderer's responses with a
  lateral RMS error of 7.9 degrees (pool median; 4.9 to 12.7) and a bias
  of -2.9 (towards the middle; -8.8 to +6.6), the raw ones with 7.9 and
  -3.2: listener by listener the renderer changes the error by -0.52 to
  +0.38 degrees (median -0.07) and the bias by -0.14 to +0.76 (median
  +0.26). The KEMAR head through its own ears: 2.9 against 3.1 degrees,
  bias +1.9 against +2.2. The lookups' noise moves the errors themselves
  more than the renderer does: with another draw of it (the lookups
  before their noise was seeded) the pool's medians were 7.2 and 7.5
  degrees and a listener's errors differed by up to 2.0 degrees, while the
  renderer's change moved by 0.034 at most in the medians (0.37 for a
  listener), so the comparison with the raw set is steady where the
  errors themselves are not. Both are compared over the 684 directions
  both have an estimate for, for every listener: at the median plane's 26
  the raw set's two ears are the same samples (its left half mirrors the
  right, and the median plane is its own mirror), a signal alike at both
  ears whose interaural coherence never rises, so the model's gate takes
  nothing and gives no estimate, where the rendered responses differ by a
  little noise (2e-5 of their peak) and get one. The model reads only
  interaural delays, so it hears the Stereo control at the centre
  wherever it is panned (42.3 degrees RMS, bias -41.1); people would hear
  that pan's level difference inside the head, so the lateral comparison
  with the control says nothing about them.
- *The music's places and the estimate's loudspeakers.* In lateral angle
  (pool medians), the pool hears the arc's five places (the front
  loudspeakers among them, at its ends) within 1.6 degrees of where they
  are, the pads above on the median plane within 0.2 and the rear
  supports within 3.6, but those behind the sides (130 degrees, 50
  degrees out) 3.7 and 7.2 degrees towards the middle and the pads 25
  degrees up at 70 and 110 degrees (58 degrees out) 4.7 to 7.3 towards
  it; the places further out than 60 degrees (the side supports, the pads
  at 90 degrees, the rear loudspeakers, 65 to 79 degrees out) are heard
  at 55.2 to 61.9 degrees: the model compresses everything beyond 60
  degrees alike. Front and back, for the places within 60 degrees of the
  median plane (baumgartner2014's range): through others' ears the arc
  ahead is heard behind half the time (51 to 62%), the rear supports
  behind 67 to 81% of the time and those at 130 degrees 72 to 79%, the
  pads 25 degrees up at 70 and 110 degrees across the frontal plane 45 to
  51% of the time, and the pads above (40 up) ahead heard behind 51% of
  the time and behind heard there 69%.
- *barumerli2023 is not run.* AMT 1.6.0 returns from `barumerli2023` and
  `barumerli2023_featureextraction` under Octave with "barumerli2023 is
  not supported by Octave": both build their directions with
  `barumerli2023_coordinates`, a MATLAB handle class deriving from
  `matlab.mixin.Copyable`, with typed properties (`pos double`). Octave
  11.3 cannot parse the class ("syntax error near line 60, column 18",
  the typed property) and has no such base class ("class not found:
  matlab.mixin.Copyable", for a class deriving from it and nothing else);
  the toolbox's ticket 345 asks for the class to become a function. The
  script records both messages. Its full-sphere predictions need MATLAB
  or that rewrite, so its data is not fetched.
- *Acceptance, relative.* The rendered responses are heard within the
  models' noise of the raw ones (pool medians and the KEMAR head within 2
  points of quadrant errors and front/back confusions and 2 degrees of
  polar and lateral error and lateral bias: 0.32 at most here), and
  Binaural confuses front and back less than the Stereo control (pool
  median and the KEMAR head): the script exits with status 0. With its
  cache filled a run takes 4 minutes here; filling it takes about an hour
  once (the 24 lookups 30 to 40 minutes, the lateral estimates 22 to 27).
- *What the results hold for.* These results are the models' for the
  responses measured for that run, and the desktop tier keeps the two
  together: `xem-binaural`'s `tests/models.rs` measures every set again as
  `rendered_hrirs` does and fails when one differs from the set the models
  heard (by the digests of the evaluated files, which it keeps; Tests
  below), asking for the responses to be measured and `evaluate.m` run
  again on them, and these results and the digests updated from
  `models.json`. Measured again here (2026-10-16), every set is the
  evaluated files byte for byte.

**The self-administered listening test.** `tools/listening` is a test the
user can take alone over headphones (its README gives the steps): a local
page (`index.html`, `listening.js`) that `serve.py` serves on the loopback
address and that saves every answer at once under
`.local/listening/results`, and `analyse.py`, which reads the answers (both
Python's standard library only). `cargo run --release -p xem-headless
--example listening -- localization` and `-- music` make the stimuli and
plan the sessions into `.local/listening` (ignored: the music is the
disc's), every random choice from a seed (2026). Each manifest's
`generated` block names the commit and the seed and holds a SHA-256 of
every file written (its path and digest) and of the rest of the manifest:
the page resumes an unfinished session, and the analysis takes answers,
only where that block is the manifest's now, so stimuli made again in any
other way (another commit or seed, `--only`, `--state`, a changed tree)
start the session afresh and leave the old answers out of the analysis,
while stimuli made again the same way (commit, seed, options and tree)
keep them. A session that cannot save or load (`serve.py` stopped, a
refused save, a stimulus missing) stops with a message saying every saved
answer is kept, and started again resumes after the last one. Nobody has
taken the test yet.

- *Localization.* Half-second bursts of seeded Gaussian noise from 48
  measured directions (elevations -30, 0, 30 and 60 degrees, azimuths every
  30) in three conditions: placed by the renderer (a voice at the Stereo
  pan law's centre routed to one source, as the game's voices are), the
  KEMAR set's raw responses, and the Stereo pan law (the direction's
  lateral position as the pan). Each condition is scaled so that its
  bursts' median loudness (BS.1770-4) is -23 LUFS (+1.59, -0.49 and +0.61
  dB), keeping the level differences its directions have (-25.7 to -21.5
  LUFS rendered, -27.8 to -19.9 raw, -23.3 to -22.4 panned), and the page
  roves each trial's level by up to 3 dB either way; nothing reaches 0.33
  of full scale before the rove. Each of three sessions holds every
  direction twice per condition (96 trials a condition, 288 over the
  three), 32 moving sounds (rendered and panned: 60 degrees in 1.5 s across
  the front, the sides and the back, and from 30 degrees below to 30 above
  at four azimuths, each way; the renderer's source glides 1.35 degrees
  behind its target at the end, which the manifest records; the raw set
  has no directions between its measured ones, so it does not move) and 8
  catch trials (one ear only), shuffled into four blocks, after 24 training
  trials (12 in later sessions) that show where each sound was. The answer
  is a dial seen from above and an elevation slider, a start and an end
  for a moving sound, each marker's height set by moving or pressing the
  slider or by Level (no answer is taken before, so a slider left where it
  starts is never read as level); each test sound plays once. Every
  session first sets the level (pink noise at -23 LUFS) and checks left
  and right and the headphones' polarity (the same low noise in and out of
  phase). 1 032 files (110 MB), made in 3 to 6 s here.
- *Musical coherence.* Eight excerpts (12 s; the title's 10), each reached
  from boot by a fixed recipe and saved as a state (the hosts' format;
  `--state` takes one of the user's instead, a disc 2 one too, and labels
  the excerpt as the user's state; a name that is not an excerpt's is
  refused), then presented from it four times at Spatial Music immersion
  0, 35, 70 and 100, Positional Effects off (effects keep their original
  mix in every condition), each with a tap capture: Lahan village (field
  1), Kislev (fields 213 and 199), Solaris (field 466), the world map, a
  battle (the debug battle selector's: the ordinary battle theme),
  Weltall's battle in burning Lahan (field 2, the game's opening scene,
  Cross at every dialogue until the battle starts, about 2 000 frames: an
  event battle that keeps its scene's music, in place of a boss) and the
  title movie's music; 9 to 22 parts sound in the sequenced ones. The only
  other story battle the retained routes reach, on disc 2's `soul-vessel`
  route (from the user's memory card; vertical blanks 7 783 to 14 190;
  Fei, Elly and a third member on foot against four small creatures and a
  tall one), likewise keeps its scene's music: `mode_music_selected_track`
  holds track 20 in the field from vertical blank 7 000 at the latest and
  in the battle (Lahan's battle plays track 11, field 2's), a song unlike
  either battle's above. Whether either is a boss is not established. The
  generator reaches its scenes from boot, without the user's card; a state
  of the soul vessel's battle, made headless from the route by a local
  probe (`.local/p678/scratch/p8a/step4fix/`), presents through `--state`
  (disc 2 beside disc 1) with 23 parts. The conditions: the original
  Stereo output (the SPU's own, which the generator checks is the same in
  the four presentations), the four immersions as presented,
  and a disintegrated low anchor mixed again from the captured voices with
  every part at a random place of its own. Each condition's loudness, the
  gain to -23 LUFS the page applies (every peak stays under 0.4 of full
  scale after it), its peak and its interaural cross-correlation by octave
  go to the manifest; the eight excerpts take 1.5 to 3 minutes here, and
  two runs from one commit and seed write the same files (the localization
  stimuli's too, and the saved states). Two
  sessions rate every excerpt once per question (coherence, lead and
  foundation, envelopment, tone colour, preference), MUSHRA-style: the
  labelled reference and six versions, the reference hidden among them,
  playing in step and looping, each to be played and rated.
- *The CD estimate.* As no premixed CD music sounds outside a movie on the
  routes checked (the rest, above), the title movie's immersions are its
  capture mixed again through the estimate (`CdRoute::Upmix` at each
  immersion): what the estimate does to the game's premixed music, not
  what the title presents. Its anchor places the CD pair's frequency bands
  (to 250, 500, 1 000, 2 000 and 4 000 Hz, and above) at random places.
- *The analysis.* Per condition, over the static test trials, in the model
  evaluation's interaural-polar coordinates: the lateral RMS error and bias,
  the quadrant errors and local polar error (targets within 30 degrees of
  the median plane), front/back confusions (evaluate.m's targets at least
  30 degrees from the frontal plane, answered on its other side), up/down
  confusions (targets at least 30 degrees from the horizontal plane) and
  the elevation gain, each with a 95% bootstrap interval over trials
  (seeded); the moving trials' start and end errors and whether they moved
  the right way; the catch trials; beside the model listeners' pool median
  and range and the KEMAR head (theirs over every measured direction).
  Coherence: the ratings by question and condition and against the hidden
  reference, BS.1534-3's post-screening (the hidden reference at 90 or more
  in at least 85% of the trials of the questions the reference tops), the
  two sessions' agreement, and the envelopment ratings against the
  measured decorrelation from 500 Hz to 4 kHz (a proxy, not a model).
  It is of one listener (the name the page saved): results holding
  several are refused unless `--listener` chooses one, since the
  post-screening and the repeats are one person's.

Tests: `xem-spu` (the tap reconstructs every sample and changes nothing);
`xem-binaural`'s units (levelling, interaural cues, the shelf, the pan law,
gains, crossfades, resets, the knee, the stage's rotation; a 300 Hz tone
turned at once from right to left glides with its largest second
difference within one and a half times the tone's held still at either
end, 79 and 116 against 55 and 91 on the two sides, where without the
filters' fade a block boundary steps, 4 568; a source's status says where
it heads and how its voices pan; a journal mixes again exactly what was
mixed, its parts adding up; the set's raw responses come in its
directions' order, the left the right's mirror image; the BS.1770-4
loudness reads the standard's own calibration, a full-scale 997 Hz sine in
one channel at -3.01 LUFS, and identical sides correlate fully) and
`tests/directions.rs`, where noise placed in 48 directions between the
measured ones is recovered from the rendered ears alone (spectra and delay,
matched against every measured direction) within 1.5 degrees (median), 3.5
at worst, left and right, front and back and up and down always right; the
model listener there is the measured head, not people. `tests/models.rs`
measures every set `rendered_hrirs` writes again (3 to 5 s here, release
builds only) and finds each the
one the calibrated models heard (above): its digest, FNV-1a 64 over its
directions and every sample rounded to 1e-6 (half away from zero), is the
evaluated files' (computed from those files apart from the test), and with
the levelling's target raised by 0.005% the four rendered sets' digests
differ and it fails. `xem_core::binaural`
on synthetic driver memory (identities, songs, field and arena emitters,
sprites followed until freed, the battle's fixed centre, distance, zoom and
scale changing nothing, restores, head turns; a start's tail keeping its
place when its slot takes another actor for the same sound; a first-person
look a quarter right and a head pitched 45 degrees up, which do not
commute, turning a direction in that order, each once; a listener's view
heard through the camera it was given in, so that the game's camera turned
half round since moves nothing heard). On the
original game, `xem-headless/tests/binaural.rs` (field 15: three emitters
and its music): with the mode on, music only, effects only or bypassed, game
memory and the SPU (voice volumes, the driver's pre-pan gains, included)
match a Stereo run after 900 frames; the bypass is the original output
sample for sample; the presented mix is within 2 dB of the original's level
(1.5 dB louder there: each ear also hears the other side of a mix the
original leans far to the right, and the driver's pan law dips 0.7 dB at
quarter pans where the levelled filters do not), and the arranged music
leans the way the original does; each part keeps one source, every emitter is placed and on the side
the original pans it to; the opening movie's sound stays within 1 of the
original mix; fast-forward and rewind keep presenting; a restored state
sounds the same every time, and a restore, which jumps from one moment's
sound to another's in the original too, stands out no more there (the
click measure below: 1.0 and 1.5 dB less than the original's). In the
forest's battle (the slice's route, three sessions from one state at frame
6 000: plain, under Binaural, and under Binaural drawn as a headset's
diorama) the combatants' sounds are placed at their sprites (9 effects)
around the formation's centre, which stays put through the battle; read
apart from the presenter, each sprite's position from that centre through
the sprites' view (`sprite_view_matrix`, the battle camera's) is where it
is heard within 0.11 degree, 1 899 times, 263 of them from where the
sprite had moved since the frame before. That
battle's camera turns only in its opening sweep, before any combatant
sounds, so the three sessions turn the yaw it frames the party from
(`battle_camera_framing_angles`) six times, and the game's own framing and
easing swing the camera over 97 frames: 196 placed directions follow it,
703 degrees in all. The battle's scripts start 16 effects no sprite's
command started: each is a coverage gap of the battle mode, heard in its
original mix (9 022 voice frames). The battle music's 20 parts keep their
places through the swings, and the swings, the 83 source switches and the
return to the field (frame 9 515, where the game's mode goes from the
battle back to the field) stand out at most 0.61, 1.10 and 0.62 dB more
than the original's in the high band. Drawn as a headset's diorama at 100%
(every frame, for two small eyes from the seat at the room's origin; the
head sweeping 90 degrees either way every four seconds; the listener's
view the seat's through the table, with the camera of the frame shown, as
the host takes it before each frame), the battle's game memory and voice
volumes are the plain battle's frame by frame and its SPU every 60 frames,
the listener is the formation's centre as on the screen, and the table
shows in all 1 812 frames with a listener. Each sprite's sound is heard
where the table shows the sprite from the formation's centre (in the room,
by the table's matrix), seen from the head, within 0.03 degree (bound
0.5) in all 1 899 directions. In 211 of them the written swings had turned
the battle camera since the frame the table shows, which was drawn with
the camera of one to three frames before (up to 53.7 degrees: the game's
camera at the frame's end leads its picture), and that moves nothing
heard: the presenter hears the listener's view through the camera it came
with. In Lahan's
opening scene (field 2,
entered as the game's development menu enters it, 1 200 frames plain and
under Binaural) the scene's script cuts the camera three times, turning it
96, 114 and 59 degrees from one frame to the next as the field reader sees
it, while the scene's music sounds (the village burning at night; 8 to
20 parts either side of a cut)
and nothing else is placed: each of its 20 parts keeps its place on the
stage through the cuts, heard from it turned only by its own pan (15 857
part frames), the parts sounding before a cut play on after it on the same
sources from the same places, and around the cuts the high band stands
out 0.3 to 0.6 dB less than the original's, its level no more than 1.3 dB
over the original's (nothing placed moves there, so the band's level
itself is checked too).
Along a walk through
field 509 (`tests/reference-inputs/binaural-field509-walk.json`, a session
log: six emitter actors playing three sounds, of which the original keeps
the three nearest the character sounding, a set that changes nine times on
the walk; the camera turned by 45 and 90 degrees, turns in place, the menu
and a jump), fifteen sessions from one state (plain Stereo; Binaural with its
defaults, bypassed, music only, effects only, immersion 0 and 100;
head-tracked music under a head sweeping 90 degrees either way; sweeping
first-person axes; the settings changed every 120 frames, off and on again
among them; a game view drawing every frame zoomed out four times with
wider framing, characters in view off, the port recording what it reveals;
a headset's head as its host gives it, its turn from the seat
(`xem_assist::headset`'s seat, recentre and reference handler), sweeping
and pitched 15 degrees up, head-tracked music on, the player's Recenter
at walk frame 300, the runtime recentring its space on the head at 700,
tracking lost over frames 1 000 to 1 059; and a headset's diorama drawn
every frame for two small eyes at 25%, 100% and 400% of its scale,
characters in view off, its listener the seat's axes through the table
as the host takes them, the head sweeping)
have the same game memory (`memory_hash`), voice volume
registers and current volumes every frame and the same SPU every 60
frames, and every placed source reports exactly the gain the driver staged
for its voices (13 152 source frames whose voices held still and sounded
through the frame, 1 606 of them effects; within 0.3% of the driver's own
gain before panning, recomputed from its records by the decomp's formula,
which is the pan law's rounding); the zoomed-out session presents exactly
what the plain Binaural one does, status and sound alike (the presenter has
no zoom). A sixteenth session keeps the field's characters moving in that
zoomed view (`set_actor_view`, as the hosts do with characters in view):
its game differs, the original's nearest three stay the same in every
frame of this walk, and its placed sources still report the driver's
gains (13 152 checked).
In those sessions the field reader (`xem_core::field::Field` and the
emitter slots, apart from the presenter) puts each placed emitter where it
is heard, through the camera matrix the game draws with, within 0.5 degree
(0.21 at worst: the matrix's rows differ in length by up to 0.8%, and the
presenter takes their directions), 3 786 times each: in front or behind as
its camera-space depth says (3 382 and 384 times), above or below as its
camera-space height says (2 746 and 806), on the side the original pans it
to (4 281 voice frames), and through the first-person axes or the head as
they turn, the recentred head as the presenter held it (the one kept
while tracking was lost included), and the diorama's view of the table
then the head, each once (within 0.22 degree, 3 786 times in each; the
first-person axes, given in the view space of the field camera the frame
before ended with, are heard through that camera, within 2e-5 degree).
Where a diorama's table shows (2 954 directions at each scale), each
emitter is heard where the table shows it from the character, in the
room, seen from the head, within 0.03 degree. The
camera's turns (71 frames with the character and the
emitter still, 127 degrees in all) turn what is heard by the geometry; the
character's five turns in place move nothing beyond what the camera's glide
moves (within 4e-6 degree). Each of the 15 emitter starts is a source of
its own, starting with its slot's actor and sound and following that actor
only, both channels' voices on it (3 425 frames); no source is two starts,
the two tails (starts whose slot moved on, a frame each) keep their last
direction, and every voice on an emitter slot's channels playing the sound
the slot holds is placed (7 209 voice frames of its three sounds). The
only coverage gaps are the menu's interface sounds (0x3, 0x5d and 0x5e, in
the field mode, heard in their original mix): none is a sound an emitter
slot held, and each was keyed on with the menu open, opening or closing.
The music's 18 parts keep their places
(`placement`) through 372 frames of camera movement and 381 of walking,
heard there turned only by their own pan, which is their voice's (within
0.03 of unpanned volumes, 11 949 frames); looking around in first person
moves no music; head-tracked music is heard against the head within 2e-5
degree, the recentred head too. The recentred session's Recenter and the
runtime's recentring put the head ahead (yaw within 0.01 degree, its
pitch of 15 kept), lost tracking keeps the last head through all 60
frames and the head is followed again once found; there the head turned
at once by -90, 137 and -32 degrees, everything placed gliding after it,
and the high band stands out 2.31, 0.30 and 3.57 dB less than the
original's (the bound: no more than 3 dB more). No session presents a
direction that is not a number. The three diorama sessions take the same
listener's axes from their tables and present the same, status and sound
sample for sample, every frame (the table shown in 1 164 of the 1 500;
menus show the screen), and their listener is the controlled character
(`listening_position` the field reader's place). Clicks: where sources switched (28 frames) and where the settings
changed (12), from two blocks before to twenty after, the presented high
band stands out of its surroundings (its 20 blocks before and after) at
most 1.24 and 0.10 dB more than the original's, in the least of its three
parts (8-11, 11-15, 15-20 kHz), a click being a step from one sample to the
next and so in every part; placing a sound reshapes the band part by part,
its level there up to 5.3 dB over the original's (1.7 dB under it over the
walk), and taken whole rather than part by part the band stands out up to
3.1 dB more than the original's there (at an attack whose waveform is
smooth). Before the renderer's filters cross-faded, the music's immersion
going from 0 to 70 stood out 5.8 dB there. The placed session keeps a tap
capture over 500 frames (the camera's quarter turn, turns in place, the
menu): mixed again it is the output sample for sample; each of the 25
sources heard in it, alone (its voices and nothing else), has the energy
of its signal (its voices at the driver's gains before panning, times the
main volume) through the HRTF pairs of where its status says it was heard,
frame by frame, and the bass shelf, within 0.11 dB (0.02 for the 17 heard
from one place throughout; the other 8 moved), where mixed in the
original mix as well it would be 1.7 dB or more over and placed twice 3 dB
or more: each is mixed once. Against its voices' energy in the
original mix each source is within 3 dB, 18 of the 25 within 1.5 (the
pairs are levelled for pink noise: a rhythm part ahead, half its energy
above 4 kHz where the pair lowers it, is 2.8 dB under, supports to the
sides up to 2 dB over); and the direct path alone (every voice routed to a
source silenced) is the original minus those voices, the rest as the SPU
mixed it, the menu's sounds and the reverb return among it, within 3 LSB.
Mono and Wide on the same walk differ
from Stereo only in what the sound-mode findings
(`analysis/formats/sound-modes.md`) name: the mode bits;
Mono's staged volumes (every channel's gain before its law the same in all
three modes, each staging its mode's law: 110 586 stagings checked), its
voices' volumes and the reverb work area they feed; Wide's master right and
reverb-depth left (the driver's pair and the copy `SpuSetReverbModeDepth`
keeps) and the three SPU registers they reach. `tests/original_sound.rs`:
on the Wide route in the field after the opening movie, Binaural turned
on through the settings service (the game in Stereo) and off again four
seconds later gives back the never-enabled Wide run's output sample for
sample 4 393 samples (99.6 ms, the fade) after it went off, and leaves game
memory and the SPU as that run's. `tests/coherence.rs` (musical coherence
by proxies that are not calibrated against listeners: regression guards,
not predictions of what people hear): Lahan's opening scene (field 2 from
its entry, 17 parts of the burning village's music sounding, nothing
else placed)
presented at music immersion 0, 35, 70 and 100, a tap capture over ten
seconds (frames 300 to 900 after the entry) each. The mix's BS.1770-4 loudness is
the original Stereo mix's within 0.3 LU at every immersion (+0.09, +0.19,
+0.11 and +0.29 against -13.74 LUFS; each ear alone within 0.41); ear by
ear its long-term third octaves are the original's within 1.6 dB from
100 Hz to 4 kHz, and above that the frontal pairs lower 5 to 8 kHz by up
to 5.7 dB (the bounds: 1 LU, 3 dB to 4 kHz, 8 dB above). The interaural
cross-correlation of each octave from 125 Hz to 8 kHz, in the original
0.58 to 0.75, is 0.51 to 0.75 at immersion 0 up to 4 kHz and 0.13 at 8
kHz, where the pairs already decorrelate the arc. Every immersion above 0
makes the music much more diffuse than that from 500 Hz to 4 kHz (0.11
to 0.26 at 35, 70 and 100, against 0.51 to 0.66; the bound: at least 0.2
under immersion 0's) and changes it little in the other octaves (at 125
and 250 Hz 0.52 to 0.74, against 0.75 and 0.59; at 8 kHz 0.09 to 0.13,
against 0.13, where immersion 35's 0.126 against 0.125 is the only value
not under immersion 0's; the bound: at most 0.02 over it). It does not
do so step by step: from 70 to 100 it rises again in some octaves (500
Hz 0.13 to 0.23, 4 kHz 0.21 to 0.26), because the three supports placed
behind on or near the median plane (180, 180 and -160 degrees), which
the ears hear alike, pass the sides on their way there (alone, from 250
Hz up, 0.32 to 0.76 at 70 and 0.53 to 1.0 at 100); the ears stay
correlated far above chance (0.09 at least, against about 0.01 for
unrelated signals; the bound 0.05). The lead, the bass and
the rhythm, each alone (its voices replayed from the capture), are heard
where the original pans them on the stereo arc at every immersion alike:
the interaural delay of each 93 ms window (200 Hz to 1.4 kHz) read through
the KEMAR head's own delays, against the arc angle of the original's left
and right levels, within 0.0, 2.4 and 1.4 degrees (RMS; the bass, panned
at +26.3 degrees, is heard at +28.6; the bounds: 5, and 2 between
immersions, where the deviations are the same to a tenth).
`web/tests/binaural.test.mjs`: one state
restored natively and in Chromium presents the same mix (99.6% of the
samples identical, the rest within 1; the declared tolerance is 2, each
host's own sin, cos and atan2 making the filters), and the presentation
costs 2.1% of real time in Chromium on this machine, 5.4 and 7.4% in two
runs with the processor throttled four times (a phone's budget; the cost
test runs with the timed tests). `web/tests/sound.test.mjs`: from field 15
and from the opening movie (frame 4 200 of the new-game route, its sound
CD input), Chromium's raw Mono, Stereo and Wide, Binaural bypassed, and
Binaural turned on and off again are xem-headless's output sample for
sample (tolerance 0: raw playback is integer arithmetic on both hosts;
while Binaural presents, within its declared 2, measured 1 in the field
and 0 in the movie, where it keeps the CD's mix). On each host the
commands are seen to take effect: in the field the three modes' outputs
all differ, and in the movie Mono's is Stereo's and Wide's is not, as the
findings say; the bypass is engaged and plays Stereo's output; Binaural
presents halfway (in the field 92 572 of the 92 738 samples to the end of
its fade differ from Stereo's), and from the end of its fade the output is
Stereo's again. `xr-game.test.mjs`: the listener turns with the emulated headset
while the game is paused, and has no head after the session; in field 15
(a field 15 state imported in the headset, the game paused), a quarter turn
of the head leaves the music's four parts where they were against the head
without head-tracked music, and with it turns each by 90 degrees the other
way (within 2), the stage staying put. `xr-binaural.test.mjs` (IWER's
Quest 3, a field 15 state, the game held while the head turns): the
presenter's head is the head's turn from the seat (within 0.01 degree);
on the screen and in first-person inspection, six head turns (to 120
degrees right, 35 up and their mixtures) each hear the three emitters
where they lay with the head ahead turned once by the head's axes (within
5e-5 degree; a turn to the right moves each that far to the left in
azimuth, its elevation kept), never twice: inspection's view does not take
the head; the music keeps its places; inspection hears the screen's
directions turned up by the camera's pitch (25.03 degrees: its view is
level on the camera's heading) and the right stick's 30-degree step,
taken with the game held, turns them once more. The player's Recenter
(the head 90 degrees right and 20 up) leaves the head ahead of the seat
at its pitch, and IWER's recentring (its space's `reset`) the head as the
space then has it, what is heard following each; frames without a viewer
pose (lost tracking, as browsers report it) keep the last head and what is
heard with it for their 10 frames, the head followed again after; nothing
is NaN. In the diorama the listener is the controlled character, and from
four head turns, through the Recenter and through the reset, each emitter
is heard where the table shows it from the head (the character to its
emitter in the room by the table's matrix, in the head's axes; within
0.02 degree: the presenter takes the camera's rows' directions, the table
the record's axes); the head moved 15 to 27 cm without turning changes
nothing heard; the table tilted 30 degrees with the game held turns the
listener's view 30 degrees at once (25.03 up to -5.01) and is heard from
the head as it lies (within 0.0012); and two whole-hand pinches, the game
held, growing the table from 25% to 400% (16 frames read, the scale
committed once a gesture) change nothing heard at any scale, press
nothing and walk nobody; 240 frames from the state at 25%, 100% and 400%
end at the same RAM and the same sound (its frames and hash) and hear the
same. The OpenXR smoke (Monado's simulated HMD, field 15, the frame log's
`binaural=`; two runs, one alone and the xr tier's, whose simulated head
moves with time): the presenter's head is the pose's turn from the seat
within 0.0034 and 0.0023 degree every frame, the game running and paused
(the bound 0.02: the log's five decimals); in first-person inspection,
held by the player's Pause while the simulated head turned over 8.0 and
18.4 degrees, each effect with the head's turn taken off held still in the
listener's view within 0.0032 and 0.0022 degree (bound 0.02) through the
Recenter, Monado's recentring of LOCAL and 15 frames of scripted lost
tracking (`--xr-lose-tracking`: the last head kept, and what is heard
with it); inspection heard the screen's directions turned up by the
camera's pitch within 0.0005 degree in both (bound 0.05); inspection
ended in the pause took the listener's view away at once; the music held
still; in the diorama the same within 0.0024 and 0.0018 degree while the
head turned 17.4 and 12.9 degrees and moved 20 cm, two-hand scaling went
from the setting's 25% to the 400% limit and back to the 25% limit
(committed ten times: 63, 159, 400, 400, 239, 143, 85, 51, 30 and 25%; no
other gesture, nobody moved), LOCAL was recentred and the seat recentred
(turned 0.0543 and 0.0159 radians), the listener's view the same to the
last bit throughout; the listener was the controlled character
throughout, the session's end left the presenter no head and no
listener's view, and both runs' RAM at game frame 1 200 is
xem-headless's.
`first-person.test.mjs`: in field 15 the browser's listener looking
around in first person faces as the native desktop host's within 4e-6
degree for the same state and look (xem-headless's `first_person`
example, the game view natively), its three emitters are heard within
1e-6 degree of where the native host hears them (bound 1e-3 for both; in
three runs the listener 3.81e-6, 0 and 3.81e-6 degree apart, the
emitters 0, 9.54e-7 and 0), from the same place, the music in the same
places. `xem-render`'s `the_listener_is_the_same_at_every_scale_to_the_last_bit`:
the diorama's listener axes at 25%, 37.7%, 137%, 252% and 400% are 100%'s
to the last bit, the table tilted or not (taken through `room_from_world`
they were not, at 37.7%); `the_listener_is_the_same_from_every_seat_to_the_last_bit`:
from seats turned by eight yaws (Monado's recentred ones, -0.0868 to
0.0814 radians, among them) they are the seat at the origin's to the last bit,
tilted or not (taken through the seat and back they were not, at 0.0814).
`xem-core`'s `what_a_frame_log_takes_of_the_status_is_the_status`: the
frame log's `Presenter::heard` (head, view, listening position, each
effect's and music part's target) is the status's, number for number.
`tests/test_listening.py`: the
listening test's analysis on answers with known errors (exact ones, front/
back mirror images, negated and halved elevations, answers further out),
its seeded bootstrap, the model comparison and the coherence summary,
answers to other stimuli left out, and one listener at a time (two
listeners' results refused without `--listener`, and each one's
post-screening and repeats their own). `web/tests/listening.test.mjs`: the
page in headless Chromium, a script answering every planned session
through the page's own controls (the checks, the dial, the slider, Level
and the markers, the MUSHRA buttons and sliders; no answer taken before its
height is set; a session left mid-block and resumed; a save the server
refuses (503) stopping the session with its message, and the session
started again asking that trial again; each trial saved once);
`analyse.py` then reads the answers back as the test works them out apart
from it: the rendered ones exact (no error, elevation gain 1), every other
front/back-eligible raw one mirrored (that many front/back confusions, no
lateral error), the panned ones level (gain 0, the lateral error their
targets' elevations make), every catch and moving trial right, and each
condition's mean rating. On its own synthetic set (70 s; the refused save
falls on a test trial) an unfinished session saved against other stimuli
waits in the results: the page starts the session afresh beside it and
the analysis leaves it out. With `XEM_LISTENING_STIMULI` it runs the generated stimuli's
every session: here 1 032 localization trials and 80 coherence screens in
28 minutes (the refused save falls on a training trial there), the raw
set's front/back confusions read back as scripted (60 of 120). It shows
the page and the analysis work, not what anyone hears.

Not done or not measured: no listening tests with people (the plan asks
for none): the optional self-administered test is ready and has been
answered only by a script; its analysis sets a person's numbers over the
test's 48 directions beside the models' over every measured direction (the
models' output holds no direction-by-direction predictions). Direction is
predicted by models calibrated on 23 people's own localization, for how
they would hear the KEMAR head's cues, not measured on anyone; the models
are validated for static broadband sounds, so moving sounds, the game's
effects and music lie outside them, and the lateral model reads only
interaural delays, so the Stereo control's level differences go unheard
there (people hear those inside the head). barumerli2023's full-sphere
predictions are not run (Octave, above), and the models run by hand only
(the AMT is a local tool): the suite checks only that the renderer's
responses are still the ones they heard (`tests/models.rs`), and a change
in the arithmetic's last bits (another compiler, libm or processor) that
moves a sample across a rounding step fails it too, the models then to be
run again all the same. Musical
coherence is measured only by proxies not calibrated against anyone, and
the arrangement rules are not tuned by ear; the immersion control does not
make the music more diffuse step by step (the supports placed behind cross
the sides on their way there). The listening test's music has no boss
battle: an event battle that keeps its scene's music stands in, the
routes' only other story battle (the soul vessel's, on disc 2) keeps its
scene's music too and needs the user's card, and neither is established
as a boss. It hears the premixed-music estimate only on the title movie's
music mixed again, since no other premixed music was found. A generic
head, so a listener's own front and back may differ; positional effects cover the field's
emitters, the arena's voices and sprites' animation sounds (each effect left out is reported, not
guessed), and sounds that event scripts start (field and battle) are left
out, since the actor running a script is not necessarily the emitter; no
per-song overrides were reviewed; the arena path is tested on synthetic
memory, not reached in the game; headsets only through IWER and Monado,
phones through a throttled processor. Lost tracking is emulated on both
headsets (IWER's frames given no viewer pose, as browsers report it;
Monado's simulated HMD never loses tracking, so `--xr-lose-tracking` takes
frames as the runtime reporting no head and no valid views), and
Monado's recentring of LOCAL does not move its simulated head's poses, so
there the runtime's recentring re-references through the seat alone. The
headset tests place what is heard with the scene standing still (the
game held, or field 15 with nobody playing); with it moving (the walk),
the head, recentring, lost tracking and the diorama's scales are heard
only headlessly, through the hosts' own seat and view code. First-person
inspection's look reaches the presenter with the camera of the frame
shown, as the diorama's seat does, so a camera a scene's script turns
while the player looks around moves nothing heard; that is checked on
synthetic memory and with the walk's synthetic first-person axes, not with
a script turning the camera during an inspection in the game. A state taken in
the browser while a headset's diorama presents differs from run to run
at one scale too, after the same frames from the same state, in two
words of the module's own memory outside the game's, which are
presentation bookkeeping and no leak: values asyncify saved with the
suspended game fiber's frames (its save area, wasm 0x10001024 and
0x10001068, 0x4c and 0x90 into it), copies of presentation-ring running
counts, which lie outside every state, so that a restore does not reset
them. While the diorama presents they grow by exactly 9 640 and 408 every
game frame, at every scale and across restores (each 240-frame run from
the state adds 240 times as much); without a diorama (the flat page, the
headset's screen view) they hold 0xc00 and 0x8, unchanging. The save area
keeps one length in every state, 388 bytes of its 256 KiB: each
suspension writes it again from its start (`xem_unwind_area`, which both
hosts call at every unwind), so nothing accumulates. The counts are u32
and the faster wraps after about 445 500 diorama frames (about two hours
at 60 frames a second), harmlessly: the port reads them modulo the rings'
65 536 entries or as unsigned differences, the host by `wrapping_sub`
(`devices/gpu.rs`), and 65 536 divides 2^32; at the wrap only the stores
of the point projected as number 2^32 - 1 lose their source (its index
plus one is 0, the stores' "unknown"): one vertex for a frame,
presentation only. [Presentation
and the whole state](#the-renderer-phase-4) shows the same natively:
played on from the same presentation records, two sessions' saved values
agree. Game memory, scratchpad and sound are alike, and the scales are
compared by those.
The battle camera's swings the battle test hears are its own (the
framing yaw turned in every session): the retained battle's camera turns
only before any combatant sounds. No placed effect sounds across a camera
cut on the routes kept (Lahan's cuts carry the song alone), so an effect's
direction jumping at a cut is measured only through the same glide
elsewhere: the renderer's direction-jump test and the walk's immersion
change.
A start's tail keeping its place when its slot takes another actor for the
same sound is checked on synthetic memory only (on the walk no slot takes
a newcomer for the same sound, and its two tails last a frame each). Two
measures differ from the ones first set for this verification: each
source alone is within 1.5 dB of its voices' original energy for 18 of the
walk's 25 sources only (narrow-band sounds take their pair's colouring, up
to 2.8 dB here, and the 3 dB the test allows is this walk's, not a
guarantee), so that each is mixed once rests on its energy through the
pairs it was heard from instead; and the high band's level around an
event is no more than 3 dB over the original's at the cuts (asserted there,
where nothing placed moves), the swings, the settings changes, the
restores and the return to the field (measured), but not where a placed
sound starts (5.3 dB over in the field, 3.6 in the battle), so a click is
measured by how far a block stands out of its surroundings, in the least
of the band's three parts, rather than by that level. The interaural
delay, applied as a phase ramp, leaves up to 1e-4 of a filter's energy
(-40 dB) past the 128 samples a block
convolves without wrapping; it wraps into the start of the block (a 300 Hz
tone held to one side shows a second difference of 55 where its own is
about 13).

## Presentation

Presentation consumes the original frame; it never runs game draw code per
eye, repaint or spectator ([later-phases.md](later-phases.md#presentation)).
Besides the GPU's VRAM and display (`devices/gpu.rs`), the port records
read-only pre-projection data at the handbook's seams (`src/port/present.c`):
`model_draw_sprite_model` (the mesh, its packets, the ordering table, the sort
mode and the GTE transform its caller set), `sprite_set_draw_matrix` (the
sprite and the transform it produced) and `sprite_draw_parts` (the sprite, its
ordering table and transform). The wrappers record and then run the original
unchanged (`WRAPPED` in tools/game_module.py). The host keeps the records of
the last complete frame (`xem_core::presentation`, the `presentation` command);
capture is off until asked for. Cameras are read through the schema
(`field_view`, the battle and world map camera state). The original camera and
screen keep deciding gameplay (on-screen tests, culling).

### The renderer (Phase 4)

The software GPU (`xem-gpu`) stays the authority: VRAM, GPUSTAT, readbacks and
snapshots are its. The renderer only draws again what it drew:

- **GTE projections.** The port's GTE appends every RTPS/RTPT vertex to a ring
  (`src/port/include/xem/gte.h`: SXY2, SZ3, the 16.16 screen point before
  saturation, OFX/OFY, H, the sprite or model `present.c` says is being
  drawn, and the point before the division: MAC1, MAC2 and the depth SZ3
  saturates from, also behind the camera). The ring lives at the fixed address 0x70000000 in the game module,
  outside the port's data and stack and every snapshot region, and it is
  written whether or not anyone presents: the port's memory layout does not
  change, and with it neither the stack addresses the game keeps in RAM (a
  static ring shifted the shadow stack and changed the slice hashes). 65 536
  entries hold a battle frame. Beside it, by the same rules: a ring of every
  screen-point word a port routine stores (swc2 of SXY0-SXYP, with the
  projection each GTE screen-point register holds, which the GTE tracks; the
  handwritten renderers' stores, with their face's projections) at
  0x70300000, and a ring of culled faces at 0x70500000 with a capture area at
  0x70A00000. While the host asks (the culled-face ring's `wanted` word: how
  far beyond the original screen a wider or pulled-back view reaches), the
  renderers' screen-bounds tests record each face they cull (front-facing,
  projected without overflow, within that reach) as the GP0 words it would
  have had and the packet it would have been linked in front of (the
  newest at its ordering-table entry, so it is drawn just where the
  original would have drawn it), and
  `present.c` draws again, for the record, the field scenery pieces the
  field's bounding test culls (`field_instance_is_off_screen`, when the same
  test with margins widened by the reach keeps them) and the models a box
  test leaves undrawn: with the GTE as their drawing would have used it and
  the renderers' links captured instead of made; their packets, the
  scratchpad words the renderers spill to, the renderers' state, the GTE and
  the ring's bookkeeping are put back, so the game sees nothing of it. With
  the camera pulled back (the ring's `pull_back` word: how far behind the
  game's camera, in depth units) all of that is judged as the pulled-back
  camera sees the points before the division, nearer than the game's near
  plane and behind its camera too: winding, a bounding box on its screen
  within the reach (now its sides), and a field piece's bounding square
  around its centre in view space; the faces the renderers drop as facing
  away or at depth 0 are recorded as well (a quad's fourth point, not yet
  projected when the winding drops it, is projected for the record with the
  GTE put back). The flat backdrops that only span the original screen go
  on into the sides as culled faces without projections: a panorama's
  strip continued from the texture column the original ends at
  (`gpu_draw_panorama_strip`), and the sky, fade and ground fills and the
  battle sky's horizon bands that cross the screen copied beside it
  (`gpu_draw_panorama`, `battle_draw_stage_sky`), each drawn just before
  its band, in the drawing mode the band is drawn in (the desert's
  translucent haze adds over the horizon drawn before it, as the band
  does). Things the game leaves
  undrawn only because a projection overflowed (an object far off the
  original screen, or near or behind its camera), which a pulled-back
  camera sees, are drawn again for the record: the field's sprite actors
  the character pass skips (`field_draw_sprite_actors`'s `flag < 0`; drawn
  as the pass draws them, with the sprite, its renderer and parts, the
  sprite queue, the ordering table, the scratch corners and the GTE put
  back, their parts recorded with their corners' own projections, out of
  order as RotTransPers4 stores them: a record's `order`), and the world
  map's scene objects (`worldmap_objects_draw`, within its draw distance;
  `src/port/worldmap_present.c`). The world map's terrain quarter blocks
  its frustum test leaves undrawn (`worldmap_terrain_draw`: the 5x5
  window's blocks and quarters outside the game's camera) are built as the
  original builds a quarter (the water's waves too, into a local array,
  not the scratchpad) where the wanted view reaches their centre, and
  their triangles recorded as culled faces; with the camera pulled back, so
  are the terrain triangles its winding test drops (a far sliver has no
  area at the original's whole pixels, and its neighbours then left a
  crack showing the sky). The clouds' screen points, which the original
  reads from the GTE and stores with plain writes, are recorded as stored
  with the projections that made them (`worldmap_clouds_draw`), so they are
  scene; and the cloud quads it leaves out as outside its view cone or
  screen are recorded as the quads it would have filled, its pass replayed
  on the GTE as it ran (matrices, view-cone test, distances, flags and
  quad budget) so its own quads are known and the depths that stop a far
  cloud's layers stop the recorded ones too.
  Intentionally hidden ones stay hidden;
  RAM stays the same (Lahan 150 frames at 1x, 3x and 5x wider: one hash). Each
  sprite's floor point (its x and z at `ground` through the sprite view),
  in the field the depth of the camera's target, and, for a view looking
  from within or around the scene, the camera each frame is drawn with
  ([Camera records](#camera-records)) are ring entries of their own; the points the world map's overlay projects (its player marker,
  `worldmap_map_overlay_draw`) are marked as interface. libgpu hands the
  three rings' headers to the GPU device before each transfer
  (`xem.gpu_projections`); the device writes the projection ring's
  `recording` word back (1 while a draw trace takes the records), and
  without it the port records nothing (the Original renderer).
- **Draw trace.** `xem_gpu::Gpu::set_trace(true)` records every primitive with
  its environment (draw mode, texture window, drawing area, mask, drawing
  offset), each vertex's GTE projection (depth, sub-pixel position, class:
  model, sprite, world or flat interface, the last also for points the port
  marks as interface and for polygons whose every point the GTE projected
  at the projection distance H, drawn at their screen size: the field
  menu's counter backing, the world map's marker), the DMA node it came
  from, fills,
  copies and VRAM writes: CPU transfers as uploads, and drawn pixels a later
  command samples as a texture before that command, so the renderer's 16-bit
  VRAM copy equals the device's at every command. A vertex takes the
  projection stored at its packet word's address while the word still holds
  it, and only that: matching by screen value tied interface points (window
  corners, glyphs) to unrelated 3D points at the same pixel in busy battles.
  Culled faces are traced as `revealed` primitives when the DMA walk reaches
  the packet they would have been linked in front of (several before the
  same packet newest first, as the GPU walks an entry), never drawn into
  VRAM; a projected vertex carries its point before the division, sprite
  parts their sprite's floor point, and a take the field camera's focus
  depth. A culled face waits for that walk through up to three frames in
  which the port recorded something new, so a paused game, which holds its
  last list and draws it when it goes on (the world map), keeps its faces.
  Hosts ask for them in every frame they run, a fast-forward batch's too:
  the list a frame shows was built two to six game frames before it (the
  field and the world map draw every other vertical blank, from double
  buffers), so a batch that asked only in its last frames showed none.
  The trace is not part of `Gpu::save`; a snapshot load starts it again
  from all of VRAM, without what the abandoned timeline recorded (the
  port's projections, the faces it culled, the screen points stored in
  RAM), and what the game draws next follows that (a rewind step's
  frames); a trace past 100 000 ops at a vertical blank restarts too (tens of
  frames not taken: a host running frames without presenting them, such as
  a long browser command, keeps no backlog to replay). In the scenes
  measured every 3D point matches (Lahan: 41 640 matched, the 420 others 2D).
- **PS1 renderer** (`xem_render::ps1::Ps1Renderer`, `draw.wgsl`). Draws the
  trace into a scaled picture of all of VRAM (colour, and an auxiliary target
  with each pixel's GTE depth, class and baked-shading flag), so double
  buffering, the saved-screen slot and copies between screen regions work as
  on the console; textures are decoded in the shader from the native VRAM copy
  (4/8-bit CLUT, 15-bit, texture window, texel 0000h transparency, the STP bit
  selecting semi-transparency, modulation, raw textures). A fill shows the
  colour VRAM keeps of it (15 bits), expanded as texels are (to the full
  range, or as the console's 15-bit colours), so texels painted to match it
  match it here too. Each vertex sits at
  its own sub-pixel point when it has one (a point shared by neighbouring
  faces lands in the same place in each), and a polygon samples only its own
  texels (no bleeding from the page at higher scales or when filtered). On
  the world map, whose terrain blocks of different detail meet in
  T-junctions that open into hairline cracks at higher scales (the
  original's whole pixels hide them), opaque world triangles are grown by
  half a native pixel, their attributes taken from their own plane; and
  each game frame's scene points of different faces within half a native
  pixel and 1% in depth of each other are welded to one point (`weld`),
  closing the cracks between pieces the game places with different
  matrices. A trace of several frames (the host ran them before it
  presented) welds each apart: welded across them, the desert battle
  presented four game frames at a time showed frames with points an
  earlier frame of the presentation drew there, and its still scenery
  moved up to a median 3.31 units a frame through each frame's own
  camera, 86.73% of it within 4 units (welded apart: 1.00 and 99.94%,
  each frame shown as it is when presented alone;
  `xem-headless/tests/camera.rs`, and `tests/ps1.rs`
  `scene_points_weld_to_their_own_game_frames_only` without the game). A
  mirrored sprite part (its coordinates falling across the screen) samples
  one texel further on, as the game's u - 1 means for the GPU, which takes
  each texel at a native pixel's leading edge: the part keeps its first
  column at any scale, and pulled back, where a native pixel spans several
  of its texels (a native pixel's worth of texels there slid the parts of
  a sprite facing left apart). The interface's own models (primitives
  projected with another screen offset than the scene's: the field
  compass, the world map's, menu models) stay interface (in place, untouched
  by the effects) but are drawn as the scene is, at their sub-pixel points
  and the picture's resolution with the world's filtering. The rest of the
  interface (flat primitives) is drawn as the
  original draws it whatever the profile: whole native pixels at any scale,
  slanted edges and lines included (a quad over each row of native pixels
  the GPU fills, from `xem_gpu::TriangleRows` and its line stepping),
  screen-linear colour and texture coordinates taken at each native pixel's
  sample point, 15-bit colour and the original's dithering, and its
  translucent pixels stay interface in the auxiliary target (a marking draw
  after each translucent interface primitive, which also sets their flags
  to 255). The four blend modes use one blend state for both targets (WebGL2
  has no per-target blending: the weight goes in the source alpha, the
  auxiliary output keeps its pixel with weight 0), affine texture
  coordinates interpolate as (uv·w, w) (GLSL ES 3.0 has no `noperspective`),
  and the GPU's 1023x511 size limit is kept. At scale 1 with every original
  limit it matches the software rasterizer but for edge pixels of triangles
  and lines (`tests/ps1.rs`); at three times the original under Modern the
  interface matches it on 99.96% of the scaled pixels (slanted edges and
  lines included; 5-bit blends and a few texels the GPU's exact
  interpolation rounds the other way aside).
- **Screen captures** (`Capture`). When the game takes the screen into
  VRAM (a copy of a displayed area elsewhere of at least 256x192, or its
  pixels read back and written there with every texel's semi-transparency
  bit set: the field's saved screen at (2C0h, 100h), the battle's
  shatter), the renderer keeps the picture last presented, its wider
  framing's sides included, as that screen; uploads, copies and fills over
  it let it go (the native mirror of a copy or fill does not). The scaled
  picture's VRAM there holds it, and whatever is textured from it (15-bit
  texels within it) samples it at its own resolution, every texel drawn,
  semi-transparent as the screen's texel: the field's menu fading over it,
  its zoom into a map change and back from a battle, the battle's shatter
  show the zoomed-out, wider picture the player saw, not the original's.
  A screen taken elsewhere keeps the two taken before while VRAM still
  holds them, and one whose VRAM is overwritten stays while a logged frame
  copies it, so a copy of either still brings its picture and a replay
  never takes such a copy as the native pixels it copied: the field's menu
  takes the dimmed screen, and its own last frame, after the field's saved
  one, then fades back in from the saved one and gives its place back to
  textures while the frame faded in from it still shows.
  What is drawn of it is a screen effect (interface class, never pulled
  back, no effects), and the polygons the game draws with every point the
  GTE projected at depth 0 (the field's screen pieces, scaled with
  RotAverage4) are traced as interface (a culled face there lies behind the
  game's camera, where a pulled-back camera sees it: the ground at the
  bottom of a field zoomed out to 5x);
  a piece whose edge lies on the screen's left or right edge carries a
  strip of the picture's sides beyond it, mapped as the piece maps its
  texture, so the pieces fill a wider frame as they move, turn and grow.
  A replayed copy of the screen back into a buffer brings the sides too.
- **Frame** (`FrameRenderer`). The displayed area of the picture, or, for
  wider framing, a pulled-back camera and interpolation, a replay of the
  primitives drawn into the displayed buffer since its last clear
  (`Ps1Renderer::frame_log`: a fill or copy of the display's size starts its
  buffer's log, replacing an older, larger one, so a log holds one frame
  even after both buffers were cleared at once; it keeps every VRAM write
  of the frame, anywhere, with its pixels, and a copy of the native VRAM
  when the buffer was cleared, and a replay samples its own VRAM copy, from
  that snapshot through the frame's writes in order, so a texture slot the
  game fills again later in the frame shows each primitive its own texels;
  writes outside the display area are not shown; with no log for the shown
  buffer, as after a load, nothing is replayed until its next clear; a trace
  started again on all of VRAM that holds what the logs left, as after a new
  scale or the Original renderer shown while the game was paused, keeps
  them; logs are numbered across every buffer and every start of the trace,
  so a frame logged after a state is never taken for one logged before
  it; a display shows the log of the buffer holding its corner, so a
  buffer shown before its own log starts, as a menu's first, shows its
  VRAM, not the frame of another it overlaps, moved). A
  game frame whose opaque drawing leaves none of a buffer's displayed
  pixels as they were (fills, copies, uploads, untextured polygons and
  textured scenery; a battle's stage drawn over the last frame instead of
  a clear) also starts its log, a shown buffer without one too (a state
  loaded, a rewind's step), so a log holds about one frame where the
  game never clears (a world map battle's log had grown to 88 000 primitives
  replayed every presentation). Those pixels are the display's, where it
  shows the log now: the Battling arena shows its buffer at y 256 inside
  the log the kernel menu's buffer at y 240 began, never drawing the rows
  between, and with the log's own corner that log grew past 140 000
  operations, its replay showing the kernel menu's VRAM with the latest
  particles every other game frame while interpolating (`tests/ps1.rs`
  `a_buffer_shown_lower_than_a_log_begun_before_starts_it_with_each_frame_drawn_all_over_it`).
  When one presentation holds several game
  frames (the host ran them before it presented: fast-forward, catching
  up; a frame begins after each vertical blank), such a log starts with
  the last of them from which the trace draws all over the buffer, unless
  a clear there starts it, and holds that frame on, not the ones before
  (`tests/ps1.rs`
  `each_frame_keeps_the_camera_recorded_before_its_first_primitive`). A
  copy whose source the game changed after it shows the native pixels it
  copied then (the loading screen after a
  save is loaded copies the saved screen while the loader fills VRAM beside
  it), not the scaled picture's source as it is when replayed. The
  interface the game drew after its last scene primitive (in order, with
  the VRAM writes and copies its textures follow) is held back and drawn
  over the finished picture (`draw_overlay`), after the effects: they keep
  off it, its translucent pixels show what they produced, and culled faces
  the replay draws late stay beneath it. Untextured interface primitives
  over the whole screen (fades, flashes) cover the sides too. The scene
  that drew the shown frame frames it (each log is tagged with the
  session's scene when it starts): the field's last frame stays pulled
  back while a battle loads behind it, and a battle's last frame stays as
  the battle drew it while the field loads.
  Wider framing widens the target to the output's aspect ratio at the
  display's own height (a 320x216 display: 32 VRAM pixels a side for 16:9,
  so Fit fills the screen; at most 160) and lets scene primitives (not
  interface ones) draw past the drawing area,
  the culled faces filling the sides (the backdrops' continuations, flat,
  only there). Pulling the camera back projects every projected vertex
  again from its point before the division as the game's camera would see
  it from further back along its own axis (x = OFX + H·X / (Z + d), depth
  Z + d, d a multiple of the camera target's depth; points near or behind
  the game's camera included, a primitive reaching within the new camera's
  near plane, half H, dropped), the culled faces showing wherever they fall
  and the interface keeping its place and size.
  Interpolation shows the newest frame's scene by the time since that
  frame arrived, one game frame late, through a camera between the two the
  game drew the frame before and the newest with (the port records them:
  [Camera records](#camera-records)); what holds still in the world stays
  where it is, and each model and sprite the port recorded the transform
  of in both frames moves from where its points were then
  ([Interpolation](#interpolation)); the interface moves as the game moves
  it.
- **Effects** (`EffectsRenderer`, `effects.wgsl`). See below.
- **GameView** (`view.rs`) is what hosts use: it resolves the graphics
  settings for a host class, accumulates the trace between presentations,
  tells the host which culled faces the trace should record (`reveals`:
  the reach and the pull-back, passed to `Gpu::set_reveals`) and the view
  the field's characters keep moving in (`actor_view`, below), and
  presents (`present`; a headset's diorama `present_diorama`:
  [Diorama](#diorama)). A presentation with nothing new to draw (the display refreshing
  faster than the game draws: the same frame, its ops drawn so far, framing,
  settings and no interpolation; transfers elsewhere in VRAM, textures or a
  saved screen's place given back, change nothing shown) shows the picture
  presented last: no replay, no effects. 24-bit movie frames are not traced pictures. Under
  the Enhanced movies setting (the default) the view rebuilds them from
  what the MDEC decoded (`ps1::MovieRenderer`): while a trace runs, the
  decoder keeps each colour macroblock's planes from before its RGB
  conversion (the luminance, the two half-resolution chrominance blocks,
  each block's quantization scale) with a key of its output's bytes
  (`xem_media::mdec::Mdec::keep_decoded`; not in states); the runtime hands
  them to the draw trace, which finds them in the transfers that load the
  movie player's slices into VRAM (`TraceOp::Movie`, by those bytes), and
  the view keeps each until something else writes over it. The trace keeps
  the 4096 newest by their latest decoding: a block decoded again (a still
  part of the picture, every frame) stays while it comes (kept by its first
  decoding, it was dropped every few hundred blocks, and that frame showed
  the device's picture). When every
  macroblock of a 24-bit display is one of them, the frame is rebuilt: on
  the CPU the seams between 8x8 blocks are smoothed where the step at the
  seam is within the compression's error at the larger quantization scale
  of the two blocks and both sides are flat (H.264's in-loop filter rules,
  in float; real edges stay); on the GPU the chrominance is brought to
  full resolution from the 4x4 nearest samples, weighted by distance and by
  how close the luminance under each is to the pixel's (colour follows the
  brightness's edges instead of spreading over 2x2 pixels), converted with
  the MDEC's coefficients, enlarged to the output edge-adaptively (FSR 1's
  EASU) and sharpened (RCAS): about 0.5 ms of CPU when the frame changes
  and 0.2 ms of GPU (the attract movie at 1600x1080). A GPU that
  rasterizes in software (`wgpu::DeviceType::Cpu`: lavapipe, SwiftShader)
  enlarges the rebuilt frame bilinearly instead: FSR 1's passes cost it
  53 ms a new frame at 1080x729 (the Android emulator showed movies at 4
  frames a second), the bilinear one 4.4 ms. Otherwise, and under
  the Original setting, hosts show the device's display, unscaled in
  aspect, each pixel a block; the Original renderer keeps the chosen
  filter. The title movie ends on the title screen's picture, which the
  game shows next from VRAM: after each of its game frames the session
  notes the frames its player has left (the movie mode playing the boot's
  request; `TraceOp::MovieFramesLeft`), and over its last second the
  rebuilt picture eases into the MDEC's own (the 24-bit pixels in the
  native VRAM, each a block), its last frame the device's picture. Other
  movies stay rebuilt to their end ([Movies](#movies): the size each host
  makes them at, what hosts report, measurements).

The trace, the projections, the recording and every renderer object observe
only: the native test `xem-headless/tests/render.rs` runs the same 240 frames
from a field 12 snapshot with nothing drawn, with every frame drawn under
switching profiles (HD-2D with wider framing and interpolation, wider framing
with the camera pulled back four times, PS1, Custom, Modern), and with a
second view attaching, changing and detaching, and checks one RAM hash; the
browser test `web/tests/render.test.mjs` does the same per profile on WebGPU
and WebGL2, and the slice logs still replay to their hashes. `render.rs`
also plays 120 frames of Lahan through `xem-headless --control`'s
`reveal_frames` (the trace asking for the faces a 16:9 framing's view
pulled back by 3 000 depth units would show, with no renderer): 54 516
culled faces revealed against 11 564 without the pull-back here, and the
RAM hash of plain frames either way; the game's own primitives traced in
those 120 frames (23 538, the traced less the revealed) are as many as in
120 calls of one frame each, each call's trace starting anew, so the
first frame a trace plays counts too, and those calls end at the same RAM.

**Presentation and the whole state.** Presenting leaves the game's state
as it is, but a whole state (`Session::snapshot_bytes`) also holds the
port's own memory (its data, the fibers' asyncify save areas and its
shadow stack: [Commands, introspection and snapshots](#commands-introspection-and-snapshots)),
where the presentation's code leaves bookkeeping of its own while the
host asks for faces the game culls (reveals: wider framing, the camera
pulled back, first-person inspection, a diorama). `render.rs` compares
whole states too (`presentation_only`). Right after its runs (field 12 or
Lahan, 90 or 240 frames from one state) the states drawn and not drawn
are the same byte for byte outside the port's memory (PS1 RAM, the
scratchpad and I/O pages, KUSEG and every device's state), and differ in
128 to 564 bytes within it: frames the presentation's code left below the
game fiber's suspended shadow stack pointer (deeper than the game's own
frames reach), values the suspended frames saved on their way (asyncify's
save area: a pointer on one path, a count on the other), and copies of
the presentation rings' counts, which the port takes into locals
(`projections = xem_gte_ring->count`, model_draw.c) and which count only
while a trace records, at fixed addresses outside every state region (so a
rendering host's counts go on across a restore, and a headless session's
stay where they were). Played on two frames from the same presentation
records with no trace, the two hold the same saved values, and what still
differs is dead stack alone (128 to 236 bytes here, none after the sprite
facings' runs), between the stack's bottom and the suspended pointer,
which no code reads: every port and game function zero-initialises its
locals (`-ftrivial-auto-var-init=zero`, `tools/game_module.py`). With those
bytes the same the states are the same byte for byte. A host that asks
for no reveals (no wider framing, pull-back, inspection or diorama) leaves
no such bookkeeping in a state: the desktop window under HD-2D, Android's
default graphics, the OpenXR and WebXR screen views each save states
byte for byte xem-headless's at the same frame ([State
interchange](#state-interchange)).

### Profiles and settings

`xem_settings::Graphics` (the Graphics section of the panel, `SettingChange::Graphics`):

| Setting | Values |
| --- | --- |
| renderer | Ex Machina (default: everything below) or Original: the console's own picture (the software GPU's display) scaled to the window as before Phase 4, with no draw trace and no presentation records (the port's rings stay idle), every setting below hidden |
| profile | Modern (default), HD-2D, PS1, Custom |
| original limits (Custom) | affine textures, vertex snapping, 15-bit colour, dithering, native resolution, painter's ordering (off: depth-tested by the GTE depth) |
| HD-2D effect levels | lighting, shadows, ambient occlusion, fog, light shafts, bloom, colour grading, depth of field (0-100), focus (-100..100); HD-2D's and Custom's are separate |
| quality | Auto (desktop High, mobile and browser Medium, headset Low), Low, Medium, High, Ultra: sample counts only |
| internal scale | Auto (the output area the picture fills, at most 8 on desktops and 4 elsewhere), 1-8; PS1 and native resolution draw at 1 |
| presentation | interpolate motion: off (the default) presents a frame only when the game drew one, on presents every display refresh, the camera, models and sprites of a field, a battle or the world map moving smoothly between the game's frames (one game frame of latency); wider framing (to the output's aspect ratio) |
| zoom out | the field camera pulled back along its axis, 1x (default) to 5x in quarter steps; the mouse wheel over the game moves it (down out, up in; not while looking around in first person); the world map takes a tenth of it, other modes none; independent of the aspect ratio |
| characters in view | field characters keep moving wherever the zoomed-out or wider view shows them (on, the default; changes gameplay, recorded with the inputs; nothing while the view is the original's), or stop just beyond the original screen as the original's do (off) |
| filtering | world, sprites, interface: nearest or linear, each; movies: Enhanced (default: rebuilt from the decoder's planes, deblocked, colour following the brightness, enlarged and sharpened with FSR 1) or Original (the decoder's picture as it is) |
| culling | viewport culling (the game's own against its 4:3 screen: off, wider and pulled-back views show the scenery the game culled; a first-person view shows it either way), back faces (both off) |
| headset | the view: the screen (default) or the diorama; the screen's distance and width; the diorama's scale (25-400%, 100), the table's height (-100 to 30 cm, -45), distance (30-200 cm, 60), tilt (0-45 degrees, 0) and turns (Smooth, Snap, Fixed); passthrough (off; offered only where an OpenXR headset can show the room: alpha blending or `XR_FB_passthrough`, until its runtime refuses it in a session); depth of field in a headset (off) |

Modern draws at high resolution, perspective-correct and sub-pixel, in full
colour, in the original's drawing order. PS1 keeps every limit at the native
resolution. No tier or device replaces the chosen profile; a tier changes
sample counts. Settings apply on the next presented frame; a new internal
scale restarts the trace from all of VRAM.

Viewport culling is the game's own: its model renderers drop faces outside
their screen and the field drops whole scenery pieces before anything reaches
the GPU. With it off (the default) a wider or pulled-back view shows what
the port recorded of that (above); with it on, only what the game drew.
Back-face culling drops submitted triangles wound against the frame's
majority (counted in `DrawStats`), a quad's two as the quad winds: its
points run in Z order, so its second triangle (points 1, 2, 3) runs the
other way round from its first and is judged as 1, 3, 2, and a quad
facing the camera keeps both (`xem-render/tests/ps1.rs`
`culling_is_off_by_default_and_counted_apart_from_the_gpu_limits`: of a
scene's quads and triangles only the one face wound the other way is
dropped, and the picture changes only where it lay). Both are distinct
from clipping (the drawing area, and the GPU size limit, always kept) and
from intentional hiding and gameplay activation (game state, decided
against the original camera and screen whatever is drawn, except as the
next section says).

### Movies

Under the Enhanced movies setting `GameView` rebuilds 24-bit movie frames
from what the MDEC decoded (above, GameView); under Original, and for a
frame whose macroblocks it does not all know, hosts show the device's own
picture, each pixel a block. Hosts make the rebuilt picture at the size
they show it (`ViewConfig::movies`): a window (desktop, Android) the
display placed in its safe area as the presentation's scale places it, the
browser's canvas likewise, and a headset's virtual screen as many pixels
across as the screen spans in an eye (its width, times the screen's
two-hand scaling, over its distance, in the eye's pixels per unit of
tangent), at most the
eye's own width (`headset_movie_output`: a larger screen shows only part of
itself). Under the Fit and Integer scales it is enlarged once (a phone's
insets do not resample it again); under Stretch the rebuilt picture keeps
the display's shape within the stretched rectangle (`fitted`) and the
compositor stretches it across, a second enlargement one way. A rebuilt
frame is the display alone (`Presented::movie`): hosts place it, map
pointers onto it and set the headset screen's shape without wider
framing's sides. Each host tells whether its last frame showed a rebuilt
movie frame: the browser's `status().renderer.movie`, OpenXR's `xrframe`
`movie=0|1` (beside whose picture the screen shows, `view=1` the
renderer's and `view=0` the device's own, the screen's sides, `side=`, and
the first eye's field of view's tangents, `fov=`), Android's
`frame rendered ... movie=0|1` log line and the desktop's `--control`
`status` reply (`renderer.movie`); a browser's `status().xr.screen` gives
the headset screen's size, the display pixels it shows and its sides.

The title movie's frames (from boot, measured natively at 640x480 on
lavapipe on this machine) are 24-bit from game frame 371; the display shows
black until frame 401 (the player's frame 2), and from then every frame is
rebuilt to frame 1325, 925 frames. Over its last second the picture eases
into the MDEC's: averaged back to the display's pixels, it lies 2.64
levels on average from the device's with 14 frames left and about 0.19 a
frame less each frame after, 0.16 with one left. The movie's last frame
(1326) and the frames the game holds after it (to 1428) are the device's
own, and the title's 15-bit frame (field 490) follows at 1429. Every
rebuilt frame, averaged back to the display's pixels, lies at most 2.82
levels on average from the device's picture and at least 31.2 dB PSNR from
it when enlarged bilinearly, 2.12 levels and 33.9 dB by FSR 1 (forced on
lavapipe: `GameView::set_software_movies`). The block-edge measure (the
luminance steps across the 8x8 blocks' edges over those within the
blocks, about 1 without blocks) of the 869 frames at full strength: on the
788 where the MDEC's picture enlarged plainly (bilinearly to the same
size, averaged back) measures less than the picture itself, as a blur
does to the blocks' steps (the device's measure 0.73-1.72 there), the
rebuilt picture measures less than the device's on every frame (at most
0.964 of it bilinearly, 0.966 by FSR 1): the blocks show less. On the
other 81 the enlargement's blur alone raises the measure: the fade from
black (401-445, 514-520; the device's measure 0.43-1.11), whose blocks do
not show by it, and the SQUARESOFT logo moving (706-737), whose letters'
outlines lie beside the blocks' edges, where a blur moves their steps.
There the rebuilt picture measures less than the plain enlargement (at
most 0.994 of it bilinearly, 0.983 by FSR 1), and on the logo as much as
the device's own picture or more: up to 0.136 more on 29 frames (706-737)
bilinearly, 0.067 on 20 (706-725) by FSR 1. The new-game movie (field 4's
movie 13, 24-bit from game frame 2 168 to 20 013 on the new-game route) is
rebuilt to its last frame and the field's 15-bit frame follows at 20 014.
After a state restored mid-movie (30 or 310 frames back), the trace
restarted for a new internal scale, or a view attached mid-movie, the
blocks of the frame shown are unknown (a restart starts a trace from all
of VRAM, without what the abandoned one kept) until the player loads a
frame decoded since: the device's picture shows for 4, 4, 3 and 5 game
frames (one movie frame at 15 a second), and the first rebuilt frame is
the restored timeline's (0.82 and 0.59 levels from its device picture,
5.55 and 15.75 from the picture shown before the restore), never one from
before the restore. Through the desktop's and Android's view (frame 840
on, wider framing on, a 1366x768 target whose safe area leaves out 60
pixels each side and 34 above and below) the title movie is rebuilt at
1037x700 under Fit and 960x648 under Integer, the display's placed sizes in
the safe area, and shown at those sizes without sides. In a browser (frame
860 of the title movie, headless Chromium's SwiftShader adapters, both
software, so both enlarge bilinearly) the rebuilt frame lies 0.58 levels
from the device's over the display's pixels on WebGPU and WebGL2, whose
pictures are identical. On Monado's simulated headset (a 320x240 eye
whose field of view's tangents run from -0.9163 to 0.9163 across) the 3 m
screen 2.5 m ahead spans 210 pixels: the title movie is rebuilt at
210x142, with no sides while wider framing is on (the renderer's frames
before it have 39 each side), and under Original shown at the display's
320x216.

Tests: `xem-headless/tests/movies.rs` (local, disc 1; lavapipe) presents
every frame from boot through the title movie with a view enlarging
bilinearly and one enlarging by FSR 1 (each told so whatever the GPU),
checking the facts above: every frame with a picture rebuilt until the
ease ends, every rebuilt frame within 6 levels and 30 dB of the device's,
at full strength the two enlargements different on every frame that is
not black (FSR 1 ran) and on every frame the block-edge measure below the
device picture's where a plain enlargement lowers it and below the plain
enlargement's elsewhere, the device's picture the reference on at least
three quarters of the frames, the ease monotonic to under a quarter, the
held frames and the title's frame; under Original no frame is rebuilt, and
the RAM after 1 100 frames from boot is the same with Enhanced, Original
and no presentation; the new-game movie ends rebuilt into the field's
15-bit frame; and the restores, the restart and the attach above resume
within two movie frames (8 game frames), the restores with the restored
picture (within 6 levels of its device picture and less than half as far
from it as from the picture shown before). `xem-host`'s
`a_rebuilt_movie_frame_is_made_at_its_placed_size_in_the_safe_area_without_sides`
(local, disc 1) checks the desktop's and Android's view as above: the
rebuilt frame's size the display's placed size in the safe area under Fit
and under Integer, shown at that size and centred there, without sides
while the view's own frames have them, and the device's 320x216 picture
under Original. The view's unit tests check that every host class
rebuilds under Enhanced alone and the headset sizing.
`web/tests/render.test.mjs` shows frame 860 of the title movie twice from
one snapshot on WebGPU and on WebGL2 (each its backend, checked; wider
framing on, the Integer scale): `renderer.movie` true under Enhanced with
no sides and false under Original, the pictures different but within 6
levels of each other over the display's pixels, the RAM the same, and the
backends' rebuilt frames 97% within 8 levels (measured: 100%).
`web/tests/xr-game.test.mjs` does it in IWER's Quest 3 with wider framing
on (`renderer.movie` true then false, the movie on the screen at both
views' centres, the screen, `status().xr.screen`, showing the 320x216
display without sides in its shape under both settings, the RAM the same,
and the title's 15-bit screen after the movie with sides, 39 measured).
`scripts/xr-game-smoke.sh` runs its boot run with wider framing on and
checks `movie=1` on every headset frame from game frame 405, each made at
the screen's span in the eye from the logged field of view (210x142),
`side=0` on those frames and on the device's own pictures (`view=0`) and
sides on the renderer's frames before the movie (`view=1`; `side=39`
measured), and in a run under Original `movie=0` at 320x216 with the RAM
equal to `xem-headless`'s. `scripts/android-smoke.sh` checks a
`frame rendered` line with `movie=1` while the opening movie plays.

Not done or not measured: FSR 1 runs on real frames only where forced on
lavapipe; the browsers' and Android emulator's adapters are software
rasterizers and enlarge bilinearly, and its cost at a headset screen's
span on headset GPUs (the Low tier) is not measured. The macroblocks are
not carried across a restart or restore (matching them to the restored
VRAM would remove the one movie frame of the device's picture measured
above). Only the title movie eases; other movies end rebuilt and cut to
what follows. Under the Stretch scale a rebuilt frame is enlarged twice
(above). No person has judged the pictures: "clearer" is the block-edge
measure above, measured on the title movie alone. On its fade from black
and its SQUARESOFT logo that measure follows the picture's own outlines
and the enlargement's blur rather than the blocks, so there the rebuilt
pictures are compared with a plain enlargement only; on the logo they
measure as much as the device's own picture or more. The desktop's
`--control` `renderer.movie` is untested (the one test that runs the
desktop's window, `xem-desktop/tests/control.rs` on weston's headless
backend, checks the settings commands): the view's flag it reports is the
one the `xem-host` test checks. Android shows only its
logged flag; its real insets (the `xem-host` test stands in with a safe
area) and sizes are not checked.

### Characters in a wider view

The field's character pass marks each sprite actor whose raised point
projects outside the original screen (x -39..359, y -9..313) as off screen,
and such an actor stops walking, turning and animating
([later-phases.md](later-phases.md#presentation): projection feeds
gameplay), so zoomed out the townspeople the view shows stood still. With
characters in view on (the default), `src/port/activity.c` wraps
`field_draw_sprite_actors`: after the original, an actor it marked off
screen is marked on screen again when the host's view still shows its
raised point, projected as the pulled-back camera sees it (the camera's
target depth times the zoom less one, from the field's own view matrix)
into the original's box widened by the view's sides. The view comes from
the host import `xem.actor_view` (the pull-back in percent plus 100, bits
0-15, and the sides in pixels, bits 16-31; 0 the original's): the hosts
set it before each frame from `GameView::actor_view` with
`Session::set_actor_view`, which records it with the inputs as a control
(`actor_view`, also in session logs and replays; a replay keeps the
recorded one and ignores the host's), so replays and hashes stay
deterministic. A view no wider than the original's (1x, no wider framing)
or the setting off changes nothing: the game runs as the original, and the
slice logs replay to their hashes. The user accepts this as a gameplay
exception to Phase 4's unchanged game rules, as they do keeping things
from visibly vanishing because of the original's optimizations (above:
those fixes are presentation only). Over Lahan (`--example motion -- --map
1 --zoom N`, `still` in its JSON) the original leaves 43 sprite actors
still at every zoom; with the setting, 43 at 1x, 32 at 1.5x, 16-17 at 2x,
7 at 3x and none at 5x.

### First-person inspection

The flat hosts look around a field from its controlled character's eyes
(plan.md Phase 8's stationary first-person inspection): the SDL hosts
(`xem_host::View`: desktop, and Android by touch or a gamepad) and the
browser host (keyboard, mouse, gamepad and touch alike). Headsets look from
within the scene with the head instead, each eye drawn from the
character's eye (below, and [Headsets](#headsets)).
While the player walks in a field (the player control read the pad in the
field's last event pass, which it does with no dialogue open, and no menu
runs), Select toggles the view where the original's Select shows nothing:
`field_picture_show` shows a map's picture on ten maps once their item is
held, and there Select stays the game's (the port's
`xem_game_field_controls` tells the hosts whether the player walks, a menu
runs and the picture shows; the browser reads it through `WebModule` as the
native hosts do). Select is any of its bound keys (Backspace), the touch
deck's SELECT ([Touch screens](#touch-screens)) and a gamepad's Back
released alone (Back begins the app chords; a headset controller's Back is
its right stick's click), all of which toggle only there: while the player
walks where Select shows no picture. A gamepad's right stick's click (R3)
toggles it anywhere in a field but a menu; a headset's controllers have no
R3. The app action Toggle first-person view (`AppAction::ToggleFirstPerson`:
V and Back+R1 by default, a headset's right stick's click with its right
trigger, and the touch deck's LOOK; rebindable in Settings' Shortcuts) toggles
it anywhere in a field but its menu too, as a headset's underhand grab does
(`look::grab_toggles`), for players with no right stick to click and in
dialogues, events and on the picture maps; neither the chord's Back nor its
R1 reaches the game while the chord holds. Each of the player's toggles
works only where Ex Machina draws the view (not under the Original
renderer) and not while the settings panel has the input. While the view is on, the arrow keys (2 radians a second
across, 1.5 up and down, whatever the bindings give them), the right stick
(3 and 2 at full tilt, past a fifth of its travel), a finger alone dragged
on the picture (0.003 radians a logical pixel, the scene following the
finger: dragged right, the view turns left) and, under the mouse camera
setting, the mouse the game holds (0.0025 radians a pixel at the mouse
sensitivity, across and up) turn it, within 80 degrees of level, and the
mouse wheel's zoom rests. The field camera's turns (CameraLeft and
CameraRight: L1/R1 under the game's default assignment; the field takes
them whoever has control) are held back, so the game's camera stays; while
the player walks so are the directions and Jump, so the character stays
where it stands, while a dialogue's choices keep the directions. What is
held back follows the game's live button assignment
(`pad_button_assignment`, as every action does), and Run by default is off
while the view shows (it would leave Run pressed alone once the directions
are held back). A menu (the field's own, with the view as the screen it
takes, or a script's) keeps every button, and the view does not turn under
it (`xem_settings::look`). It ends when toggled again, when the field or its
map is left, and under the Original renderer. It begins only where a
frame shown with the field camera's record made looking around (its
`Camera::around`: the port records it so from the frame after the view
asks) draws the field's scene as the game drew it
(its map, models or sprites, not only faces recorded for a wider view):
the title screen is field 490, whose camera shows none of its map (the
game culls it whole behind the logo), so a toggle there ends at the
second new frame of the record alone instead of looking around a room the
game never shows or holding back the title's D-pad. The first such frame
proves nothing: the trace starts again as the Original renderer ends,
mid-frame, and its first frame holds the record without the scene the
game drew before it; on a busy page a toggle met that frame and the view
ended at once (`first-person.test.mjs`'s gamepad step in the full tier). A headset's eye pictures begin anew with
each inspection: what they hold through frames that draw no field (its
menu) is a picture of this inspection's, never an earlier one's (a
headset tour found the title showing field 12's room, held from a
dialogue inspected minutes before).

The player's inputs all keep the character where it stands: the keys,
buttons, mouse and touch deck above, the taps below and a headset's
gestures. Hosts give the session the view's state before each batch of
frames (`Session::set_inspecting`: `Game::want_inspection` on desktop,
Android and OpenXR, the browser's frame loop): turning it on stops a route
under way, recorded as the player's own `assist_stop` is, and while it is
on the session refuses agents' moves with "first-person inspection keeps
the character where it stands: turn it off to walk, jump, turn the camera
or interact": `navigate`, `interact` and an `assist` sent live (the intent
`navigate` and `interact` record) anywhere, and while the player walks
(the `walk` context) `pad`, `press`, `hold` and `act` holding pad 1's
directions, Jump's button or the camera's turns under the game's button
assignment (`action::INSPECTION_HELD`, what the view holds back from the
player; Triangle is Jump's whichever action names it, AttackWeak too). The
rest of pad 1, pad 2 and a release pass, and outside the walk (a
dialogue's choices take the directions) they all do. A refused command
changes and records nothing, so a recording replays as it played. A session
log's commands (`schedule`) and a replay's controls are the recording's
and run whatever the host shows, and the hosts' own `Session::set_pad` is
never refused (`tests/inspection.rs` turns the character by it under the
view); a hold an agent gave before the view came on stays until it is
released. `status` reports `inspecting`. A replay ignores the host's state,
and the native hosts give none while they play a session log (`--script`,
Android's `script.json`). Turning it off restarts nothing. Taps on the
picture resolve without the field, since the view shows no field camera to
walk or point through: walking, they walk nowhere and talk to no one, while
a dialogue's still advance and a menu's still choose, and no floor or
destination marks show. A headset's gestures follow the same rule
([Headsets: hands, gaze and control decks](#headsets-hands-gaze-and-control-decks)).

Agents turn the view on, off and around with the hosts' `first_person`
command (`xem_settings::look::Command`), which the app's Toggle first-person
view is without its fields: `{"cmd": "first_person", "on": bool, "yaw":
degrees, "pitch": degrees}`, each optional, on `xem-desktop --control`, the
headset's `--xr --control` and in browsers `window.xem.firstPerson({on, yaw,
pitch})` (or `window.xem.command`; the page answers it, not the session).
`on` absent toggles the view, or turns it on where a look is given; a
change is made only where the toggles allow one (anywhere in a field but
its menu, under Ex Machina) and refused otherwise, naming why (no field,
the field's menu, the Original renderer). `yaw` turns it to look that many
degrees right of the heading it began with (where a view turned on begins,
level), `pitch` up (held within 80 degrees of level); a look is refused
while the view is off or under the menu. In a headset the head does the
pitching: the eyes turn with it from the view's heading held level,
whatever `pitch` says. The pitch is kept, answered and reported (OpenXR's
`xrframe` `look=`) as the view's, and shows only where its flat picture
does (a browser's page once the WebXR session ends; in OpenXR, the virtual
screen before the runtime first locates the eyes). It answers `{"first_person": bool,
"look": {"yaw": degrees, "pitch": degrees}}` (to a thousandth; `look` null
while off) or `{"error": ...}`, and an unknown field is refused rather than
read as a toggle. The session takes the new state with the host's next
batch of frames, as for the player's toggles: from then `status` reports
`inspecting` and a route under way has stopped (a browser page held by
`setRunning(false)` runs none until it runs again). Android has no agent
channel.

The view itself is presentation only. While it is on, `GameView::reveals`
asks for every face the game culls (`Reveals::LOOK`, bit 0 of the
culled-face ring's `around` word: [Camera records](#camera-records)): the
port's visibility tests keep any face wherever it lies (behind the game's
camera, facing away or at depth 0 too), the scenery pieces the field's
bounding test culls and the sprite actors the character pass skips for
overflow are all drawn for the record, and `field_draw_models` records the
field camera (`XEM_PROJECTION_AXIS`, `XEM_PROJECTION_CAMERA`) and the
controlled character's position through it (its descriptor's translation)
with its sprite (`XEM_PROJECTION_EYE`). The trace hands the place over in
order with the commands (`TraceOp::Eye`), and each buffer's log keeps the
one recorded for its frame (the newest within four vertical blanks when the
frame's first primitive is drawn), so the view stands where the character
stood in the frame shown.
From the first frame recorded so it looks along the heading of the game's
camera, level, from 0.9 of the character's sprite's height above its feet,
with a 70-degree field of view across the original's 320 pixels (H 228.5).
`frame::first_person` moves every scene vertex's point before the division
into that camera (a vertex without one is taken back through the game's
projection); a sprite's parts face the view, standing along the world's up
at the sprite's floor point as wide and tall as the game's camera showed
them; the character's own sprite is left out. Polygons reaching behind its
near plane (a 32nd of the eye's height) are clipped there as they are drawn,
in the camera's space, colour and texture coordinates taken at the cut;
neither the GPU's size limit nor back-face culling drops scene polygons, and
depth is tested whatever the ordering, from its reciprocal, which
interpolates across the screen as depth does (polygons span much of the
view's depth), kept reversed (nearer larger: `DepthMode::Nearer`, fills
writing the far plane, 0), where a float keeps its precision at any
distance, and written by each fragment itself (draw.wgsl's
`NearerTargets`): wgpu's GL backend, which WebXR always draws through
(WebGL2), as do browsers without WebGPU and Android's GLES fallback, takes
a vertex's depth through GL's clip space (2z - 1), where a float's step
near the far plane is 6e-8, too coarse beyond a few hundred units for the
lift below (measured on Mesa's llvmpipe before the fragments wrote it: a
sprite's earlier part through a later at 3 456 pixels in field 30 flat and
139 in field 12 in a headset, and a face drawn over a wall 5 200 to 6 800
units away hidden at every pixel). The PS1 draws a primitive over another
whatever their depths, and in one plane depth cannot tell them apart: a
sprite's parts, split sprites at one floor point and faces drawn over
others showed through each other pixel by pixel (a townsperson's shirt
striped in field 30, seen from
within close by). So `first_person` lifts each face over the faces it
overlaps in its plane drawn before it (`diorama::Layers`, which the diorama
lifts its faces by; planes within a quarter of a view-space unit are one),
towards the eye along each point's ray by a hundred-thousandth of its depth
a face below it (a tenth of a unit 10 000 away), where it shows as it did.
The faces below each are found in the game camera's view space, the same
from any look (a look moves the world's faces as one and lays a sprite's
parts on its billboard as the game's screen shows them), once a game frame
for all of a view's looks (both eyes, each presentation: kept by the
renderer, `frame::PlaneLayers`): on this machine's CPU about 2 ms for the
5 000 primitives of a frame in Lahan or the forest and 0.1 ms in a room,
a look's own work 0.5 to 0.9 ms there as before.
The game also draws a sprite reaching below its floor point, over the floor
whatever their depths (in fields 1, 12 and 30, 32 of 33 sprites, by 6 to 48
view units, 2% to 24% of their height), which a look from within hid under
the floor (the townspeople's feet in Lahan): the sprites at a floor point
stand with their lowest part on it, or above it as far as the game drew them
above it (a jump), as the diorama stands them (`frame::sprite_raises`); the
eye stays at its height above the character's floor point. The interface
stays as the game drew it (the compass too).
A frame recorded looking around but shown from the game's camera (the view
just turned off) shows none of its culled faces: they lie anywhere, not
only beside the screen.

With characters in view on, the view the field's characters keep moving in
is the widest (`actor_view` 1000% with 1024-pixel sides: the view may turn
anywhere; in the forest, map 22, the 30 sprite actors the original stops
keep moving); off, the game runs as the original. The
[Binaural sound mode](#binaural-sound-mode)'s listener looks where the view
looks (`GameView::listener_axes`, which every host gives the session each
frame it presents, the game running or held: the look alone, never a
headset's head, which `Session::set_head` adds once more after it, with
the camera of the frame shown, which the presenter hears it through).
`status().renderer.firstPerson` and
`status().renderer.look` (radians turned from the heading it began with,
yaw right and pitch up; `first_person` answers in degrees) show it in
browsers. `--example motion -- --map 30
--look YAW,PITCH[,TURN]` shows a field from the eyes (degrees from the
camera's heading, TURN more each frame kept: map 30's room with its
townspeople standing round the table). Measured in the forest (960x720
output, this machine's CPU and lavapipe, means per game frame): the game
frame 1.5 ms without the view and 2.5 ms with it (4 763 culled faces
recorded a frame), the presentation's CPU work 1.9 and 5.5 ms.

**In a headset** the view is immersive (`GameView::present_headset`, every
headset frame, paused or not): each eye is drawn from the character's eye, turned
and moved as the head turns and moves it. The session's reference space is
laid on the view: its -z along the view's level heading (the field
camera's when the view began, plus the right stick's steps), its y the
world's up and its origin the character's eye (OpenXR's LOCAL space and
WebXR's `local` begin at the head as the session began, where the virtual
screen's place is reckoned from too). A metre of head movement moves the
eye by its height above the character's feet over `EYE_HEIGHT_METRES`,
1.6 m (an assumed standing eye height: Fei's eye at 66.6 units in a field
of scale 1 makes about 42 units a metre, 2.7 units between the eyes of a
64 mm IPD). Each eye's field of view, asymmetric as a headset's are, is
fitted onto the frame (the display with the wider framing's sides): the
look projects with a distance and a shift per axis (`Look::h`,
`Look::shift`, about each primitive's projection centre), so the
frustum's edges fall on the frame's, its pixels as wide as the eye's view
and the display make them; the near plane and its clipping are the flat
view's. The sprites stand along the world's up turned to face the head
between the eyes (`Look::facing`), worked out from that one point for both
eyes, so both see one billboard and turning the head turns none. The eyes
leave out every interface primitive the game drew (`Interface::Omitted`),
which would otherwise go about with the head: the interface alone goes on
a pane where the virtual screen was, as large (`Interface::Only`: the
scene and the fills that clear it are left out, and `FrameRenderer::pane`
makes the coverage its alpha from the auxiliary target, the frame's own
alpha being the PS1's mask bit: opaque where the interface drew, half
covering where it was semi-transparent, as the original's commonest blend
lets half of what lies behind through, and a semi-transparent effect over
the whole screen added to what lies behind). Gestures and pointers aim at
the pane as at the screen, and a screen the game takes meanwhile (the
field menu's backdrop) is the first eye's picture, what the player looked
at. A frame that does not draw the field leaves the eyes on the last that
did (`Framing::hold`), replayed where it was drawn and seen as the head now
is: the field's menu draws none, in both buffers by turns, so the room
stays round the menu on the pane. The port records the character's place
as the field draws its models, and a frame may keep a record from up to
four vertical blanks before it, so a frame without a record or without
the field's world (its map's scenery through the GTE, which a menu's own
models are not) counts; the eyes keep the last record too, rather than
cut to the game's camera. A frame with nothing to replay (a state loaded
or a rewind's step, with no game frame since) leaves the pane empty
rather than showing the display, scene and interface together, while the
eyes keep the last room. Turned on while no game frame runs (the player's
Pause), the view begins with the game's next frame, which records the
place, unless the frame shown has it already: turned off and on again
within one pause, it looks from within at once. The right stick turns
the view in 30-degree steps (`look::SnapTurn`: once past 0.7 of its travel
to a side, the next once back within 0.3), never smoothly, and pitching is
the head's alone (the arrows, the mouse and a drag do not turn it in a
session). The toggles and the stick work whether the game runs or the
player's Pause holds it, but not while Settings are open: the panel has
the controllers then, and the hands keep only the app's gestures (Pause,
Settings, two-hand scaling, lost tracking). The eyes' pictures and the
screen's are composited opaque (`Quad::opaque`): the renderer's frames
keep the PS1's mask bit in their alpha, which is no coverage (taken as
coverage, a pixel of alpha 0 went undecoded onto OpenXR's sRGB
swapchains, which encoded it again, paler, and was added to the room
behind the screen); the pane alone covers by its alpha. The character's
moves (an agent's, a script's) carry the eyes with it without turning
them: the heading stays the world's, so the view follows the character's
place but neither its turning nor its animation. The Binaural listener
looks along the view's level heading without the head, which
`Session::set_head` adds once (so an effect is heard as on the screen
turned up by the camera's pitch, then by the head: measured on IWER and
Monado, [Binaural sound mode](#binaural-sound-mode)); the right stick's
step turns it with the view, while the player's Pause holds the game too.
Turned off, the screen is back where it
was and as large (two-hand scaling does nothing while the view shows).

Measured on Monado's simulated HMD (20 Hz, eyes of 320x240) with lavapipe
on this machine, with other work keeping a load average of about 9
(`xr-game-smoke.sh`'s inspection run twice, and the same run under each
profile; a frame is late after more than 1.5 periods): from within the
scene no headset frame was late in any of these runs (0 of 117 each); as
the screen none were in the per-profile runs (0 of 217 and 0 of 219), 2
of 216 in the smoke's later run and 10 of 215 in its earlier one. The
presentation's time on the loop's thread, p50, was 11.3 ms from within
against 8.9 ms as the screen under HD-2D (12.4 ms, p95 16.4 ms, against
8.7 ms in the smoke's later run) and 5.6 against 3.1 ms under Modern.
Other work raising the load (to 14 in one of the `xr` tier's runs) made 21
of 117 headset frames late from within and 31 of 209 as the screen; at a
load average of 18 the frames from within took 110 ms (p50) against the
screen's 53. On IWER's Quest 3 in headless Chromium (WebGL2
on SwiftShader, the layer at a quarter scale, load average about 5), the
session ran 7.8 frames a second from within against 11.7 as the screen,
and the game, whose frames the session's paces (at most four a frame),
31.2 against 41.1.

Tests: `xem-headless/tests/render.rs` looks around in field 12 for 160
frames (more than 100 shown from the eyes) and ends at the same RAM hash;
`tests/controls.rs` checks the field query (the map's picture, walking,
the menu) on the original's maps. `tests/inspection.rs` (local, disc 1)
plays a flat host's part in field 30: turning the view on stops a route 15
frames under way (`Stopped`) and refuses new ones and an agent's `act` Up,
`press` Triangle and `hold` CameraLeft; the character and the
game's camera stay over 60 frames of a held direction, jump and camera
turn (more than 50 of them shown from the eyes); while an agent's presses
turn the character five ways and the game's camera a quarter turn (0 to
1024) over 120 frames, the view's heading in the world moves at most
0.0027 radians (the camera matrix's fixed point); turning it off moves no
camera, starts no route, keeps the character and shows the game's camera
pixel for pixel against a view that never looked, and a new route walks.
`xem-core/tests/persistence.rs` checks the session's part on a stand-in
module (one recorded stop, the refusals, a live `assist` refused while a
session log's intent still runs, and a replay begun while the host
inspects, whose recorded `assist` starts its route and runs on whatever
the host's toggles do while it plays) and, with the stand-in walking in a
field and reading pad 1 into its RAM, that eight live commands holding
directions, Jump (as Jump and AttackWeak), CameraLeft or R1 are refused
and reach no pad, that Confirm, Start, pad 2 and a release pass, as a
direction does in a dialogue, that a session log's Up and L1 run without
stopping the session, that the recording holds exactly what passed, and
that it replays to the recording's RAM hash with the replaying host
inspecting from the replay's start; `xem-settings`' look tests the toggles
(Select's keys and the deck's SELECT alike), the underhand grab's rule
(`look::grab_toggles`: in a field but its menu, under Ex Machina only),
the `first_person` command (parsed, an unknown field refused, toggled in a
walk, a dialogue and a picture map, refused under the menu, outside a field
and under the Original renderer, off anywhere it is off, a look from the
heading when it comes on or from where it looks, a look alone keeping it
on, refused while off or under the menu, and its answer in degrees), the
held-back buttons under the default, an unswapped and a
shoulder-swapped assignment and the turn rates, and `controls`' tests
Toggle first-person view's V and Back+R1 (held by no other shortcut nor a
game key; Back then R1 asks once, the game has neither while the chord
holds, and Back let go after it is no Select toggle), `xem-assist`'s the
headset's gestures and pointer keys while
inspecting and two-hand scaling that leaves the screen, the deck's
SELECT and drag, and LOOK (in a walk, a scene and a dialogue on a
letterboxed phone, a 16:9 phone and a tablet, clear of every other control
and within the screen, pressing none of the game's buttons; in no menu,
world map, battle or other context; lit while the view shows), `xem-host`'s
a gamepad's toggles and the touch deck (SELECT
toggling and staying the game's where the map's picture shows, no Run left
alone, a 100-pixel drag turning 0.3 radians and tapping nothing; LOOK
toggling the view in a dialogue and on a picture map once a press, lit
while it shows, nothing under the menu or the Original renderer, and an
agent's command answered with its look), `xr_play`'s a headset's right
stick's click with its trigger asking Toggle first-person view once (the
game having neither button) and the headset view taking it in a dialogue,
not under its menu nor the Original renderer, `xem-core`'s Binaural test a
look and a head turn each applied once, and
`xem-ui/tests/panel.rs` the look row's drag on touch screens, a
headset's own rows and Shortcuts' Toggle first-person view (V and Back+R1
on its row and in its editors, rebound to G through the settings service,
Save state's chord refused). `xem-desktop/tests/control.rs` (weston,
local) sends `first_person` to `xem-desktop --control`: refused at the
title and for an unknown field; in field 30, with a route under way, on
(answering the camera's heading), the next frames' `status` showing
`inspecting` and the route `stopped`, `navigate` and `act` Up refused,
turned to 30 degrees right and 10 down, and off, the session following.
In a headset: `xem-render/tests/ps1.rs` presents a synthetic field to two
eyes 32 mm apart with asymmetric fields of view (tangents -1 to 0.5 and
-0.5 to 1 across, 0.5 up, 1 down): each eye's line of sight falls at two
thirds and a third across and a third down, as the frustums put it, a floor
reaching from behind the eye fills each frame's bottom, clipped at the near
plane, the interface is on the pane alone (a white rectangle opaque, a grey
semi-transparent one half covering, the scene transparent) and in neither
eye, the listener stays level and head-free through a quarter turn of the
head, which turns the view, and a frame without the character's record
keeps the eyes where they were, as does a state loaded (all of VRAM again,
unlike what the logs left), which leaves nothing on the pane; an eye's
wall of middle tones drawn without the mask bit composites in its own
colours (within a level) onto an sRGB and a linear target, the pane's
white rectangle covering it (taken as coverage, that alpha of 0 turned
32, 64, 96 into 99, 137, 165 on the sRGB target); and canted eyes see one
sprite, as wide as the game drew it and facing the head between them,
where each eye's own plane would show two. `xem-settings`' look tests
the stick's 30-degree steps (one a push, none until it comes back, none
for a stick still pushed when input returns). `tests/inspection.rs`
presents field 30 to a headset: the eyes see apart (89% of pixels
differ), the pane holds the compass alone (7% of it covered); with no
game frame a head turned 0.5 radians and moved 10 cm turns the view while
the listener and the pane stay, and the head back gives the same picture; an agent walking the character 50
units carries the eyes without moving the heading (0 radians), and the RAM
hash is the run's without any view; through 240 frames of the field's menu
(more than 30 with its commands) the eyes keep the room every frame and
the pane shows the menu (over 30% of it covered); a state loaded then,
with no frame since, leaves the pane empty and the eyes on the room.
`xem-render`'s `inspection_begins_only_over_the_fields_scene_and_its_eyes_begin_anew`
turns the view on over a field's frames of its interface alone, presented
one at a time: it waits through the trace's start (no camera record) and
the first frame with one (shown again, still the first), and ends at the
second; over a frame of the record alone and then a field's scene it
begins; over a field's scene it stays; an eye that saw a green floor,
the view off, another frame with the floor blue and the view on again,
sees blue (held from before, it saw green: the tour's title).
`xem-render`'s `from_within_a_part_drawn_over_another_stands_nearer_and_a_sprite_on_its_floor`:
a sprite's part reaching 50 below its floor point stands on it, its top as
high above as the game drew it, a part drawn over it in its plane stands a
hundred-thousandth of its depth nearer where it showed, a part beside them
stays; `from_within_a_face_drawn_over_another_in_its_plane_shows_wherever_they_overlap_far_away`
draws a face over a wall 5 200 to 6 800 units ahead, slanting away, which
shows at all 19 152 pixels they share. The desktop tier runs it and
`first_person.rs`'s order test (below) again on wgpu's GL backend
(`suite.sh`'s rust stage: Mesa's llvmpipe through EGL, `XEM_MESA_EGL`),
where they agree with lavapipe.
`xem-headless/tests/first_person.rs` (local, disc 1) qualifies the
view as a camera mode on the game, flat (the desktop's look) and in a
headset's eyes (`present_headset`, the eyes 64 mm apart with fields of view
of tangents ±1 across and ±0.75 up and down), beside the game camera's flat
frame of the same trace and a look from within the same trace with each
scene primitive drawn plain in a colour of its own. From the controlled
character's eye in fields 1, 12, 22 and 30, turned to eight headings at two
pitches (level and 30 degrees down), the walkmesh's floors in view (every
actor layer's, as `diorama.rs` takes them) show the scene at every one of
their pixels, flat and in either eye (0.48 to 5.6 million a field; under
0.5% without it asserted, in each field); what shows none lies above them,
the ceilings and skies the game never models: 0 to 1.6% of the flat
pictures and 0.5 to 4.1% of the eyes' a field, up to 23.5% of one eye
looking back past the camera's side in field 30, where the room has no
ceiling. Where the game draws its
interface opaque after the scene, the flat look shows every pixel of it as
the flat frame does, and in a headset all of it is on the pane in that
colour, no opaque pane pixel unlike the flat frame (4 514 to 27 712 pixels
a field, field 12's dialogue the most). In fields 1, 30 and 12 from those
sixteen looks, flat and in a headset, wherever two primitives the look draws
meet in one plane (around the pixel too: faces crossing are not one) the
picture shows the one the game drew last at every one of 0.08 to 1.4
million such pixels a field (the renderer before the lift and the
reversed depth, the tie judged at the pixel alone, showed a sprite's
earlier part through a later at 4 702 and 35 979 pixels in field 30, flat
and in a headset, and 1 684 and 1 331 in field 12); a face the game draws
from both sides (twice, its corners in the other order) is counted apart:
where its quad does not lie flat its two drawings meet along a fold and
each shows on its side of it, the other side's texture coordinates there
(1 500 and 1 257 pixels in Lahan, 24 142 and 26 525 in field 12's room,
none in field 30). On wgpu's GL backend the order holds at every such
pixel too (Lahan's faces drawn from both sides at 1 495 and 1 259 pixels,
the rest as on lavapipe). Every sprite drawn reaching below its floor point
stands with its lowest part on it, within what the lifts move it (half a
unit and two ten-thousandths of its distance), but for the controlled
character's second sprite in Lahan, at its floor point, whose lower parts
lie behind the eye; one drawn 6 units
above its floor point stays as high. Both eyes see each sprite as one
billboard, its parts within 0.6 units of one plane (their lifts) and the
two eyes' within 0.02 units. In a headset, map 25's story movie begun while
inspecting shows on the screen with no eyes for its 4 233 rebuilt frames,
inspection on through it; after it the eyes look from within in map 25
again, until map 23's first frame ends the view. The Mountain Path's random
battle entered while inspecting (the host's pad walking the character up,
which nothing holds back) keeps the eyes and the pane, the compass alone
on it (5.5% covered), for the 48 blanks the battle loads behind the
field's last frame; the battle's first frame, its shatter, ends inspection
on the screen, its pieces the eye's picture (the screen the game takes is
the first eye's, what the player looked at: its first picture follows the
eye's last at a correlation of 0.91 of brightness), and the battle shows on
the screen.
`xr-game-smoke.sh` plays it on Monado ([Headsets](#headsets)) and
`web/tests/xr-game.test.mjs` on IWER, whose scene tour toggles it with
the left hand's underhand grab in each scene (only in field 30 and
field 12's dialogue does it look from within, the title included among
the rest). The smoke's `agent-inspect` run plays field 30 from a state with
`--xr --control`: a walk by `navigate`, `first_person` on looking 30 degrees
right (every frame after it `fp=1`, `look=0.5236,0.0000` and the walk
`stopped`; the eyes look from within some ten game frames after the reply,
so up to four headset frames go without, as the grab's check allows),
`navigate` and `act` Up refused and `status`'s
`inspecting` true, `first_person` off, then Toggle first-person view's chord
by `--xr-pad` (Back, then Back+R1) on for 30 headset frames and off.
`web/tests/first-person.test.mjs` (local, disc 1) runs field 30 in a page
with WebGL2: Backspace held as the loop resumes stops an 8-frame route and
`navigate` is refused; W and Space reach no pad and the character stays;
ArrowRight and ArrowUp turn the view right and up; the wheel leaves the
zoom; the Binaural listener faces right with the view; the mouse under
pointer lock turns it right and down; a tap on the picture walks nowhere,
the deck's SELECT toggles it off and on, and a 100-pixel drag turns it
0.3 radians left; Backspace gives W back and the listener its camera; the
Original renderer ends the view; a gamepad's right stick's click toggles
it, its right stick turns it right and its Back released alone ends it,
while after the page's `blur` neither the stick, the click nor Back acts,
and a click held as focus returns toggles nothing; `window.xem.firstPerson`
turns it on looking 30 degrees right and 10 down (answered in degrees,
`status().renderer.look` in radians) and an 8-frame route stops once the
loop runs, `act` Up, `press` Triangle and `navigate` are refused, a bad `on`
is refused, and it turns it off; then V, the deck's LOOK, the automation's
`appOperation` Toggle first-person view and the bare command toggle it in
turn. Its canvas pictures go
to `build/web-test/first-person-*.png`. `web/tests/page.test.mjs` holds
the game's Select with the deck's SELECT outside a field.

Not done or not measured: in a headset, comfort is not measured (no people
tests): the view follows the character's scripted moves (vection, with no
comfort vignette), and its pictures are the display's frame (320 by about
216 pixels and the sides, times the internal scale) spread over each eye's
field of view. Its cost was measured on Monado and IWER only, not on a
Quest-class CPU: the eyes double the replay and the effects (HD-2D's per
eye too), and the pane adds a third, interface-only replay. The pane takes
a semi-transparent interface pixel over another as half covering, and
shows no subtractive effect (it darkens nothing behind it); the field
menu's backdrop is the first eye's view squeezed into the display's shape,
and the room round the menu is the last frame before it (its characters
stand still). From within, what the game never models shows its clear:
the ceilings of rooms and the skies above outdoor fields (up to 23.5% of
an eye looking back past the camera's side in field 30), measured and shown
as they are, not made up; the walkmesh's floors in view show the scene
(none of their pixels, in fields 1, 12, 22 and 30). A face the game draws
from both sides (two quads wound each way) is drawn twice from within,
whichever side faces the eye; where its quad does not lie flat the two meet
along a fold, the far side's texture coordinates showing on part of it
(measured above: field 12's railings). The eye stands at 0.9 of the height
the character's sprite reaches above its floor point (the part reaching
below the point left out), while the other sprites stand raised by that
part (in fields 1, 12 and 30 by 6 to 48 view units, 2% to 24% of their
height; the player's own sprite reaches 48 units below its floor point in
field 30): beside a character drawn as the player's is, the eye stands at
0.9 of its height less that share (0.68 to 0.88 for the shares measured),
not at 0.9. Raising the eye with its own sprite instead showed more of
field 30's missing ceiling (the smoke's inspect run's eyes covered 89% to
90% of the view, 91% to 93% at this height) and changed the headset's
metre (the eye's height is 1.6 m), so the eye keeps its height. Holes,
the game's order and the pane are measured in `xem-headless`'s harness,
which draws a headset's eyes with the renderer the hosts use, on lavapipe
(Vulkan) and, for the game's order, also on wgpu's GL backend (Mesa's
llvmpipe, through the clip space WebGL2 and GLES draw with), not in a
headset's captures nor on a phone's or a Quest's GPU. In an OpenXR
session (Monado) `xr-game-smoke.sh` plays `xr_play`'s own paths: its
controllers run, the
player's own play from field 30's state (no session log, so the host gives
the session the view's state), toggles the view with the controllers' Back
released alone, holds Down, Jump (North) and a camera turn (L1) while it
shows, the character staying where it stands (`player=` in the frame log;
it walks once the view is off), turns the view with the right stick 30
degrees a push and ends it with an agent's `settings_apply` to the
Original renderer; its binaural run looks from within with the Binaural
mode on and checks the listener's view, held still through head turns,
both recentrings and lost tracking, and gone when inspection ends
([Binaural sound mode](#binaural-sound-mode)); its diorama-hands run stops
a walk under way with the underhand grab, none restarting. Monado's
simulated controllers (`khr/simple_controller`) press nothing and have no
stick, so those buttons (`--xr-pad`) and the stick (`--xr-stick`) are
scripted beside the controllers' own; a session log played in a headset
(`--script`) stands in for the player and gives the session no view state,
so those runs are the player's own play; no headset with real controllers
(a Quest) has played them. WebXR's toggle, snap turn, listener and
Original end ran on IWER (its held-back buttons are the browser page's
code, which `first-person.test.mjs` runs), and the view carried out of a
session and into the next. The touch
look's rate is a constant chosen to follow the finger on a phone-sized
picture, not measured with people, and the Android emulator has not
played the touch toggle or drag (the unit tests and the browser's touch
pointers have), nor the deck's LOOK. On the ten maps where Select shows the
map's picture, and in dialogues and events, Select's toggles stay the
game's; keyboard-only, touch-only and headset-controller-only players toggle
there with Toggle first-person view (V, the deck's LOOK, Back+R1), as
gamepad players can with R3 and hands with the underhand grab. The
desktop's SDL keyboard (V, as Backspace) is driven by no test, the
browser's is; the chord ran on Monado (scripted buttons), not on IWER,
whose page maps the same buttons (`xrGamepad`). The session takes a
command's change with the host's next batch of frames, as the player's: a
`navigate` an agent sends before then is still taken, and that batch stops
it.

### Camera records

A view that looks at the scene from elsewhere than the game's camera needs
the camera each frame was drawn with, to take the frame's view-space points
back into its world (plan.md Phase 8's seated diorama), and so does
[Interpolation](#interpolation), which shows the scene from a camera
between two frames': the port records the camera of every frame of a
field, a battle and the world map while a trace takes its records. The culled-face
ring's `around` word carries bits (`Reveals::around`, xem/gte.h
`XEM_AROUND_*`; 0 the game's own camera): `Reveals::LOOK` (1) for
first-person inspection, `Reveals::ANYWHERE` (2) for a view from anywhere
around a field, a battle or the world map. With either bit every face
counts wherever it lies, the field's culled scenery pieces and skipped
sprite actors are drawn for the record, and so are the world map's scene
objects its projection skips, the terrain triangles its winding test
drops (before, with a pulled-back camera only) and those its projection's
FLAG error drops behind or through its camera's plane, both of a cell
(the original leaves the second unprojected after the first fails; the
port projects it for the record, the GTE put back): a diorama's table at
25% showed no terrain over a third of it, behind the world map's camera,
in steps of its cells; the sprites the port draws
again facing the character's eyes come with `LOOK` only (`sprite_facing.c`),
the battle's stage and the world map's ground with `ANYWHERE` only, the
controlled character's place with either. Every camera record carries the
bits it was made with (`Camera::around`): a frame recorded with either
holds culled faces from anywhere, which the game's camera shows none of,
and first-person inspection and the diorama begin only with frames
recorded for them. `GameView` asks for `ANYWHERE` while its headset view is
the [Diorama](#diorama) of a field, a battle or the world map.
The port reads the bits (as the reach and the pull-back) that the host wrote
into the ring's header when the port's GPU commands last handed the rings
over, and the rings are no part of a state: the first frame the game builds
after the bits change, or after a state is restored, is built with the bits
before. In Lahan, after 90 frames with `LOOK`, 90 frames run again from the
same state with `ANYWHERE` replayed 57 sprite facings in the first frame
the game drew and none after it.

The port records a camera as ring entries that are no drawn points
(xem/gte.h), in this order: the world's x, y and z axes through its rotation
(three `XEM_PROJECTION_AXIS` entries owned by 0-2: the matrix's columns in
4.12, times the field's scale in a field), its translation (an AXIS entry
owned by 3), and the point it looks at in the same world units with that
point's depth through it (`XEM_PROJECTION_CAMERA`, owned by the mode:
`XEM_CAMERA_FIELD`, `_BATTLE`, `_WORLDMAP`, plus the host's `around` bits
shifted 8 bits), so that a point's view is the rotation times the world
point, shifted 12 bits, plus the translation (`present.c`
`xem_present_camera`). Each mode records the matrix its frame is drawn
with where it is set:

| Mode | Where | Camera | Target |
| --- | --- | --- | --- |
| field | `field_draw_models` (then, with either bit, the controlled character, `XEM_PROJECTION_EYE`) | `field_view.scaled_world` | `field_view.target` |
| battle | `sprite_set_view_matrix` (WRAPPED), when the battle sets it from `battle_camera.matrix` once a frame, after drawing its stage; with `ANYWHERE`, its stage just before (below) | the look-at matrix with the quake's offset | `battle_camera_view_target` |
| world map | `worldmap_objects_draw`, which every world-map frame calls (the actors' sprite pass that also sets the sprites' view matrix runs in the open map only); the world's point at its target and, with `ANYWHERE`, the ground under it just before (below) | `worldmap_camera_matrix`, which maps points relative to the camera's target with z negated, as the world map places its objects | (0, the target's height, 0), the point the orbit and look-at cameras look at |

Before a battle's camera the port records its stage (`battle_present.c`,
`XEM_PROJECTION_STAGE`), what a diorama stands the battle on its table by
whatever the camera does: each combatant the battle shows, as its camera
frames them (`battle_camera_frame_slots`: a slot with a sprite, not hidden),
its sprite's place in the battle's world and its height (its frame's extent:
a person's about 96 units, a Gear's about 500, the debug battle's boss
1523), owned by its slot; then the scene data's camera 0, the start view
`battle_enter` begins the battle with, its look-at (its depth 1 once the
battle's turns run, `battle_turns_active`: its opening is over, its
combatants have entered and stand) and its eye. Before the world map's
camera it records where its world lies in the game's (a STAGE entry owned
by `XEM_STAGE_ORIGIN`: the target's x, 0 and its z negated, in the whole
units the world map places its objects relative to; `Camera::origin`), and
the ground's height under the camera's target (a STAGE
entry owned by `XEM_STAGE_GROUND`, `worldmap_present.c`): the plane of the
terrain triangle there, the cell split along one diagonal as
`worldmap_terrain_get_normal` splits it, which `worldmap_terrain_get_height`
takes with the GTE and the scratchpad; the port only reads the cell's
corners (none where no terrain block is loaded). The vehicle boot mode 3
begins in hovers 136 units above it there, and the terrain the frame draws
passes over the target within a unit and a half of it. The trace hands
each one over in order with the commands (`TraceOp::Camera`:
`xem_gpu::Camera` with its axes, translation, target, focus depth and
`CameraMode`, a battle's with its `Stage`, the world map's with its
`ground`; `Camera::world_point` inverts it, `Camera::view_point` applies
it). The record is boxed (`TraceOp::Camera(Box<Camera>)`): a battle's
stage makes a `Camera` 324 bytes, more than a primitive, and every op of a
trace, of a renderer's logs and of its replays is as large as the largest
kind of op, so inline it made every op 328 bytes on x86_64 against a
primitive's 256, and 324 against 248 in the browser's wasm32, in every
mode, the diorama off too; boxed, an op is a primitive's size on both
(measured with rustc's `-Zprint-type-sizes`). A buffer's log keeps the
records current
when its frame's first primitive is drawn (`FrameLog::camera` and `eye`,
the newest within four vertical blanks), and `FrameRenderer::camera` the
shown frame's: the port makes a frame's records while the game builds it,
before any of its primitives, so a buffer cleared and one the game draws
over instead (a battle's stage, whose log begins with the frame that draws
all over it) both keep their own, also when one presentation takes several
game frames. Taken when a log began, as the place was before, the desert
battle's frames each kept the camera of the frame before: no primitive
held still between its frames with alike records. A frame recorded with
either bit but shown from the game's camera shows none of its culled
faces, which lie anywhere. The records write the rings only: from the same
state the game ends at the same RAM, VRAM, sound and clock with `around`
0, 1 and 2 as without a trace.

Tests: `xem-headless/tests/camera.rs` presents frames with `ANYWHERE` as a
host would, one game frame a presentation unless said, and takes each
shown frame's camera and primitives. For the scenery that holds still
(primitives whose view-space points stay the same from a frame to the next
while the camera record does), each primitive's distance from where it was
a frame before is taken through each frame's own camera while the game's
camera turns or moves: the median over a pair of frames must be within 4
world units, and more than four times that through the frame before's
camera, and 95 in 100 of all the distances must be within 4. Measured here
(suite profile, lavapipe): in Lahan (map 1, L1 turning the camera 30 of
every 150 frames, 600 frames) 298 of 299 frames shown keep a camera and
the camera turns 300 degrees; the median still primitive moves at most
0.31 units a frame (the largest pair's median; 99.21% of all 737 667
distances are within 4, the 90th percentile 0.38 and the 99th 2.30),
against a median 50.2 through the frame before's camera. The battle the
debug battle selector starts as it is, with Cross every 10 frames (900
frames): the camera turns 173 degrees and travels 19 530 units, the median
at most 1.02 (99.85% of 770 773 within 4, 1.08 and 1.30), against 18.5.
Its desert, drawn over the last frame, as its camera comes in (1500
frames): 1.02 (99.97% of 117 530, 1.12 and 1.35), against 24.5. The same
desert presented four game frames at a time, as in fast-forward (it draws
one every vertical blank into its two buffers in turn and shows the one
before the last, so the buffer shown is drawn twice a presentation): 374
of 374 frames shown keep a camera, each with the same primitives as the
frame with that camera presented alone, 1.00 (99.94% of 32 143, 1.14 and
1.44), against 33.2; with the logs begun at the presentation's start, as
before, they held both frames: 12 primitives held still and no pair could
be compared. The world map turned by L1 (600 frames): 85 degrees, 1.07
(98.86% of 334 803, 1.18 and 5.56), against 41.7. The distances beyond 4
units belong to primitives the test's key cannot tell from a like one
elsewhere (inspected: a world map tile matched with the same tile in
another cell, a model's face with another of the same texture elsewhere)
and, in Lahan, quads that move with the camera. The target the battle and
the world map record lies on their camera's axis within 2% of its depth,
at its recorded depth; the world map's (its height -256 there, y down)
negated would lie off it.
`a_battles_cameras_carry_its_stage_as_the_game_keeps_it` (the first debug
battle, Cross every 10 frames for 300 frames, then 120 without): every
battle camera of the frames shown carries a stage, and over 8 frames more,
taking each frame's trace, the one camera a frame holds the combatants at
the places `battle_area` keeps for its slots' sprites, each as tall as the
sprite is in RAM before or after its frame within a fifth (a sprite's
height changes within a frame as its animation steps: 90 as the battle
draws, 96 before and after, measured), the scene data's start view as RAM
holds it, and the turns running; measured at rest, the party at
(4360, 0, 4160), (4420, 0, 3840) and (3810, 0, 4040), 90 to 130 tall, and
the boss at (3820, 0, 4040), 1523 tall; the start view looking at
(4192, 0, 3680) from (4192, -1719, 464).
`the_world_maps_terrain_lies_all_around_its_target_for_a_view_from_anywhere`
takes the world map's last frame shown with `ANYWHERE` back through its
camera: of the 2 828 cells of 50 units within 1 500 of the target (a
diorama's reach at 25%) all are covered by its terrain (with the
triangles behind its camera dropped, as before, 2 095).
`the_world_maps_camera_carries_the_ground_under_its_target_in_flight_too`
(the vehicle boot mode 3 begins in, 30 frames hovering, then 200 with its
throttle, Triangle, held: it climbs): the world map's camera carries its
ground, and the terrain the frame drew passes over the target at that
height within 4 units (measured: the target at -256, the ground -120, the
triangles at -120.5 and -121.3; driven on, -452, -142, -142.4 and
-142.9), the vehicle 136 units above it, then 310.
`camera_records_leave_ram_vram_sound_and_time_unchanged` runs 240 frames
from the same state in each of the four with `around` 0, 1 and 2 and
without a trace (with `ANYWHERE`, the battles' stages and the world map's
ground too): one RAM hash, VRAM, sound and clock each (field
fd3154d0f8c7dc47, battle ff78fb7899ba79a4, desert 2c9cfe67bf5c6dcb, world
map 9b83d31eba10763f, as before the stages, the ground and the terrain
behind the camera were recorded). One
desert battle frame drawn with `ANYWHERE` held 8 262 primitives, 4 654 of
them culled faces. `xem-headless/tests/render.rs`
`first_person_sprite_facings_replay_only_when_looking_and_leave_original_ram_vram_and_sound_unchanged`
replays no sprite facings in 90 frames of Lahan with `ANYWHERE` (after a
run with the original's bits) and some with `LOOK`, at the same RAM, VRAM,
sound and clock. `xem-render/tests/ps1.rs`
`each_frame_keeps_the_camera_recorded_before_its_first_primitive` (a cleared
buffer, one drawn over, a presentation of two frames cleared, drawn over or
one of each, a state's first presentation of two frames drawn over, a
record older than four vertical blanks; with the logs begun at the start
of a presentation, the last of two frames drawn over kept the first one's
camera and 142 primitives instead of its own 71),
`xem-gpu/tests/gpu.rs`
`a_recorded_camera_comes_before_its_frame_and_its_inverse_gives_back_world_points`,
`a_battles_camera_carries_its_stage_and_the_world_maps_its_ground_and_no_other_camera_does`,
`polygons_the_backdrop_projects_are_the_world_marked_as_the_backdrop`,
`a_traced_op_takes_no_more_room_than_a_drawn_primitive` (an op as large as
a primitive; with the camera inline it failed, 328 bytes against 256)
and `tests/test_port_presentation.py` (the entries `xem_present_camera`
writes) check the format and the logs without the game (the stage's port
code reads `battle_area`, whose pointers a native build would widen, and
the ground's the world map's block pointers, so they are checked on the
game only).

Not done or not measured: the [Diorama](#diorama) presents these records
in both headset hosts ([In a headset](#in-a-headset)), where field 30,
field 12, the debug battle on foot and the world map ran; the cost of
`ANYWHERE` in battles and on the world map is not measured; the ground
under the world map's target was compared with its terrain for its boot
mode 3 vehicle only (not walking, nor on its sea); the world map's
scripted scenes, the arena, menus and movies were not run with it; the first battle's camera
turned while it attacked, the desert's only came in (0.9 degrees); of the
real game, only the desert was presented several frames at a time; a frame
built with the bits before a change is not told apart.

### Interpolation

With interpolation on, every display refresh presents the newest game
frame's scene with its camera and each of its models and sprites on their
way from where they were before their last move, one move late: each
moves over the time between its own last two moves (1 to 4 vertical
blanks; after a longer rest, the time its moves took before), from the
host's time the frame it moved in was first shown (`frame.rs` `Clock`,
`Moved`). A battle draws every display frame while its camera and Gears
move every other one and its effects every one: a frame in which a thing
did not move keeps its move going instead of starting another, so each
moves evenly at its own pace. Once nothing moves the picture is the frame
as the game drew it, and a presentation with nothing new draws nothing.
Nothing is matched by its look: the port records what moved
(`Motion`).

- **The camera.** Each frame of a field, a battle or the world map carries
  the camera the game drew it with ([Camera records](#camera-records); the
  world map's placed at its origin, so its world is the game's in every
  frame). The camera between two is the first's rotation turned towards
  the second's (a quaternion's slerp; a mirrored one as a mirrored world's
  rotation), its scale between theirs, and what it looks at moved straight
  in the world and in its view, so it turns about that point as the game's
  camera does; what each end's matrix holds besides a scaled rotation (its
  4.12 rounding) is blended in, so each end is the game's own. A scene
  point that holds still goes through the newest camera into the world and
  through that camera back.
- **Models and sprites.** Each model draw (`model_draw_sprite_model`, and
  a model drawn again for the record) and each sprite's parts record the
  transform the GTE holds as they begin (four `XEM_PROJECTION_PLACEMENT`
  entries, xem/gte.h; `TraceOp::Placement`); the trace gives each
  projection its owner's newest placement and each primitive its first
  point's (`Primitive::placement`), and a buffer's log keeps the placements
  recorded since the frame before was drawn (`FrameLog::placements`).
  A placement of the newest frame follows the frame before's of the same
  sprite (its owner), or of the same model instance: an instance's two
  packet buffers are one block (`model_alloc_packet_buffers`), so its draws
  alternate between buffers a buffer's size apart, or keep one; each
  model's draws agree on which way they stepped (the field's and the
  sprites' buffers flip on counters of their own), which tells adjacent
  instances apart. Its points go back through its newest transform to its
  own space and out through the one before into the world then, and move
  from there to where they are now. One drawn twice differently, or moved
  further than the camera stands from what it looks at (another thing
  drawn into that memory), holds still in the world.
- **What is moved.** Each projected point of the scene (and a sprite's
  floor point) moves its sub-pixel point and depth by as much as its
  projection moves; a primitive with a point without depth, or at or behind
  the camera's plane, stays as drawn, and so does the interface. The
  faces the game culls beside its screen come into it as the camera turns:
  `GameView::reveals` asks for them 32 pixels beyond the screen (and the
  sides) while interpolation is on, and an interpolated picture shows them
  wherever they fall, unless its frame was recorded looking around (whose
  culled faces lie anywhere).
- **Cuts.** Between cameras of other modes or mirrored worlds, a turn of
  more than 45 degrees, a scale changed by more than a quarter, or the
  camera or what it looks at moved further than the one stands from the
  other, the camera does not move (its frames' models and sprites still
  do, in the world); a frame without a camera (the trace's first, menus,
  the arena, movies) shows as drawn.
- **Cost.** The pairing, each thing's move and its clock are worked out
  once for each new frame; a presentation blends a map for each placement
  still moving and takes each point through one 3x4 map.

Measured (release, `--example motion`, lavapipe, 2 presentations a game
frame): turning Lahan's camera with L1 (map 1, 80 frames, 800x540), the
presentation's CPU cost was 0.95 ms against 1.04 ms for the nearest-match
interpolation this replaced and 0.41 ms with interpolation off, and the
dark tile-shaped holes that match left where like grass tiles swapped are
gone; the picture as each new frame arrives was within a pixel of the
frame shown before in all but 0.56% of its pixels (0.61% with the old
match, where its pairs held; the two renderers differ about that much),
its game state (RAM hash) the same as with interpolation off. The save
point (map 76) turns in every presentation, against every fourth off.
Turning the world map's camera (boot mode 3, L1), each new frame's first
picture differed from the frame before in 0.01% of its pixels; walking
south from Lahan's exit (the exit log, then Down), 0.04-0.34% from the
frame before against about 1% from the newest. The debug battle's Gears
attacking: 0-0.6% from the frame before, up to 9% from the newest.

Tests: `xem-render/tests/ps1.rs`
`interpolation_turns_the_camera_between_frames_with_still_scenery_in_place`,
`interpolation_moves_models_and_sprites_from_their_own_placements`
(adjacent instances of one model, a sprite, a model moved further than the
camera's distance, one not drawn before) and
`each_camera_and_sprite_moves_over_the_time_between_its_own_moves` (a
camera turning every other display frame, a sprite moving every one),
`xem-gpu/tests/gpu.rs`
`a_placement_comes_before_its_owners_primitives_which_hold_its_number`,
and `tests/test_port_presentation.py` (the entries a placement and a
camera with the host's bits write).

With one clock for the whole frame (the time between drawn frames), the
debug battle's attack moved its camera and Gears over two presentations
and held them for the next two (one presentation in four repeated the one
before); with a clock for each thing, every presentation of the attack
changed (0.17 to 0.36 of the picture each, against none in four before)
and the picture neared the game's next frame evenly through its four, at
the same cost (1.5 ms).
The Battling arena records no camera: interpolating, it shows its frames
as drawn, now as the display does (0.08 at most of a level apart on
average over 240 presentations of a bout; before the frame-log fix above,
every other game frame showed the kernel menu's VRAM and the latest
particles), at 0.29 ms a presentation.

Not done: a model's own vertex animation (`MorphTable`) steps with the
game's frames, as do the points effects compute each frame (they follow
the camera); scenes without a camera record (the Battling arena, menus'
models, movies) are not interpolated; menus and movies were not run.

### Diorama

The headsets' seated diorama (plan.md Phase 8): the game's 3D scene as a
miniature on a table in front of a seated player, drawn for each eye with
its own view and projection (`ps1::diorama`, `GameView::present_diorama`).
It is presentation only: nothing it does reaches the game but the records
it asks the port for ([Camera records](#camera-records)) and, in a field,
the widest view the characters keep moving in (the recorded `actor_view`
of first-person inspection). Both headset hosts show it when Graphics'
headset view is the diorama ([In a headset](#in-a-headset)); the review
camera of `--example motion` and the tests below drive it too.

- **Asking for it.** `GameView::set_xr_view(XrView::Diorama(table))` (or
  `XrView::Screen`, the default); `ps1::xr_view(&graphics)` gives the view
  the settings choose, the screen under the Original renderer whatever the
  headset view (it draws no scene of its own). While the frame shown is a field's, a
  battle's or the world map's and first-person inspection is off,
  `reveals` asks for `Reveals::ANYWHERE` with the sides' reach: every face
  the game culls and the camera each frame is drawn with.
- **World points.** Each game frame shown is taken back into its world
  through its camera record: every projected point of the scene (its point
  before the GTE's division, or its screen point taken back through its
  projection, behind the camera too) goes through the inverse of the
  camera's matrix, once a game frame. The vertices hold world points; each
  eye's pass uniform holds its matrices from the world and from the table's
  space to clip space (draw.wgsl `vs_world`), so both eyes and every
  headset frame between two game frames draw one build. A build is made
  again within a game frame only once the table has turned 10 degrees
  since it (a game paused while the table turns on towards its camera) or
  its scale has changed by a quarter: the billboards face the seat's way
  as the table stood at the build, and while the game draws, a smooth
  turn moves the table less than that from a game frame to the next (at
  most 6.9 degrees at 20 frames a second, the camera just short of a cut
  away). Textures are perspective-correct through clip space.
- **The table** (`Table`: scale 1, height -0.45 m, distance 0.6 m, tilt 0,
  turns Smooth by default). The host gives the seat's pose in the room (its
  reference space, turned about its up only); the table's top lies at
  `seat · (0, height, -distance)`, tilted about its x axis (positive raises
  its far edge). The world stands on it upright: shifted to the point the
  table follows, turned about the world's up to the table's heading, y
  flipped up and z with it (z kept should a camera's matrix mirror its
  world; Lahan's, the debug battle's and the world map's do not), which
  keeps the world's handedness (nothing is mirrored and every face keeps
  its winding), and scaled so the original screen's width
  at the camera's target depth (320 times the target's depth over H, in
  view units, over the field's scale) spans the table's 0.9 m, fixed when a
  scene begins, times the player's scale. The camera's pitch and roll do
  not reach it. The followed point eases towards the camera's target with a
  0.3 s time constant; the heading follows the camera's axis, level:
  `Turns::Smooth` eases it likewise, `Snap` jumps once the camera is 30
  degrees away, `Fixed` keeps the scene's first. A cut (the target jumping
  more than a third of the table, or the heading more than 45 degrees, from
  a game frame to the next) jumps at once, and a jump (a cut or a snap) dims
  the diorama by 70%, fading over 0.25 s. A battle's table stands where its
  stage puts it for the whole battle ([Camera records](#camera-records);
  its cameras orbit, cut and shake, and its input is not camera-relative),
  so no camera's move, cut or quake moves it (a quake offsets the camera's
  translation, which the frame's camera record carries: its world stays
  put through it), nor does a recentring: it faces the start view's way;
  during the battle's opening (until its turns run: its combatants enter,
  leaping in and appearing) it stands on the start view's look-at, spanning
  the original screen's width at its depth; once the turns run, a jump
  places it on the middle of the combatants' extent across the table (as
  the battle's camera frames them all), on the ground the lowest of them
  stands on, spanning their extent with the tallest one's height to spare,
  at least the original screen's width at two projection distances (the
  nearest the battle's camera frames them from: 640 units) and enough for
  the tallest to stand within nine tenths of the volume's height; a new
  start view (another battle without leaving the mode) places it again.
  Measured over four battles, the start view looks 324 to 1473 units
  across the floor from the combatants' middle (the debug battle's 324,
  Babel Tower's 543, the debug Gear battle's 614): the
  Mountain Path's random battle (map 15) looks at (1632, -96, 2368) from
  its start view, its four combatants stand within 241 by 163 units at
  (2092 to 2333, -514 to -562, 932 to 1095), where its table now stands at
  1406 um a unit (two projection distances' screen: a person 13.5 cm tall),
  against 231 um placed by the start view (2.2 cm, at the table's edge).
  The world map's points already follow its camera's target (its frame is
  the target's: the target stays at (0, its height, 0) and the map moves
  under it); its camera looks from 640 units at its target, so its table
  spans 799 units at 100% and 3 196 at 25%, its far edge 2 238 deep at
  most, within the terrain's 5x5 window of blocks (at least 4 096 units
  from the target every way) and the game's draw distances (scene objects
  to 3 456 deep, terrain to 3 840): at every scale the table's edges bound
  what it shows, not the game's. A target in flight keeps the ground
  under it ([Camera records](#camera-records)) in view: the table stands
  below the target as far as that ground, settling there with a 3 s time
  constant (the map rises and falls with the target, not with each hill
  it passes over), at once as far as keeps the ground within eight tenths
  of the volume below the table's top, and never so far that the target
  stands beyond eight tenths of the volume above it (the debug world
  map's vehicle hovers 136 units above its ground and climbs to 310 above
  it: at 100%, with the target on the top, the ground lay 0.35 m below
  it, beyond the volume, and the table showed the vehicle and the clouds
  alone). The table's volume is x and z within 0.45 m of its centre and y
  from 0.25 m below its top to 0.55 m above: what lies outside is not
  drawn (draw.wgsl discards it).
- **Sprites** stand as upright billboards facing the seat's way, built once
  a game frame for both eyes: each part's offset from its sprite's floor
  point on the game's screen laid along the billboards' right and the
  world's up, as large as the game showed it, the parts the game drew
  (no facings). The game draws a sprite facing its camera with its feet a
  little below its floor point (Fei in field 30: his lowest part 48 view
  units, 8 pixels, below it; in its own 3D the quad passes through the
  floor and the drawing order shows it) and a character in several sprites
  at one floor point (Fei: two at the same depth): the sprites at one floor
  point stand together with their lowest part on the floor, or above it as
  far as the game drew them above it (a jump). From high above, upright
  billboards are seen edge-on.
- **Depth and the game's order.** The scene is depth-tested: opaque pixels
  write depth, translucent ones only test it, in the game's order. The
  PS1 draws a primitive over another whatever their depths, and in one
  plane depth-testing cannot tell them apart: a part of a sprite over
  another, split sprites at one floor point and a shadow on a floor showed
  through each other pixel by pixel (Fei's sprites striped in field 30;
  his shirt over his hair seen from behind in the forest). So a face
  drawn over faces in its plane (within 0.2 mm on the table) that its
  outline overlaps by more than that (not one sharing an edge) is lifted
  0.1 mm (a hundred times the depth buffer's resolution a metre away)
  above the highest of them, towards the side the game's camera saw it
  from. A sprite's billboard parts are lifted likewise along the
  billboards' normal, towards whichever side of the billboard each eye
  sees: the vertex carries the lift (`w`, with `FLAG_BILLBOARD`) and the
  pass the eye's point in the world, so the game's order holds from behind
  the billboards too, where their mirrored picture shows (field 30's Fei
  seen from behind at scale 3, 0.5 m away: before, the layers his drawing
  hides covered him; the seat's side is unchanged, its picture the same
  in every pixel). Faces are found by their plane's normal and distance
  and by 256-unit cells of the plane. From the default seat at scale 1
  (960x720, the review camera 1.1 m
  from the table's centre and 40 degrees up), the faces and parts lifted a
  frame and the pixels the lift changes against the same frames built
  without it: Lahan 382 and 0, field 12 11 and 18, the forest 40 and 227,
  field 30 39 and 38, the debug battle 190 and 31, the world map 11 and 0
  (a sprite or a shadow's area at that distance; close up, a character's
  whole body). The field's shadows, translucent subtractive World quads
  drawn twice, lie 1 to 1.5 view units above the floor and need no lift
  but their second drawing over their first. Faces the game shows from
  their front (its majority winding, judged from its camera's view-space
  points, so faces behind it count) and the faces it culled are drawn
  front-facing only, emitted counter-clockwise seen from their front (the
  transform keeps the world's handedness), so an eye sees into a room
  through walls it sees from behind and under nothing it sees from below;
  faces the game drew from behind (double-sided ones), sprites and lines
  (ribbons a native pixel wide where the game's camera saw them) are
  drawn either way.
- **What else a frame holds.** Flat primitives and fills drawn before the
  scene's first primitive (the clear, panoramas, sky bands) are the
  backdrop: a wall at the back of the volume, facing the seat (seen from
  behind it is not drawn), the original screen mapped across the volume's
  width, a line there a ribbon a native pixel wide. So is, wherever it
  comes in the frame, what the game projects far behind its scene as its
  backdrop, a battle's sky (the port marks `battle_draw_stage_sky`'s
  projections `XEM_PROJECTION_BACKDROP`; the trace keeps them the world,
  `Primitive::backdrop`, so the screen and its effects see no change), its
  culled faces left out: the Mountain Path's random battle projects its sky
  11 913 to 16 366 deep before its sky bands and panorama; taken as the
  scene's start, the sky lay far beyond the table and the bands and
  panorama went on the pane, opaque over its upper half. All in one plane, the
  wall's pictures paint over one another in the game's order, as the PS1
  draws them: they test the depth (the scene stands in front of them) but
  do not write it (written, the clear's depth fought the pictures drawn
  over it: the debug battle's forest showed black or in bands). Where the room shows
  around the table ([The room](#the-room-passthrough-and-immersive-ar);
  hosts say so every frame: `GameView::set_room`), the wall leaves out
  those fills, the game's clear, which the room stands in for; the
  panoramas and sky bands stay on it. Flat primitives drawn
  after it (windows, text, portraits, menus, the compass and other
  interface models, battle numbers) and pictures uploaded to the screen
  go onto the interface pane, drawn as the screen
  draws them into a transparent premultiplied texture (`PASS_PANE`): an
  opaque pixel covers, mode 0 is half the colour at half coverage, modes 1
  and 3 add without coverage, so over the room the pane shows as the
  original blends it; mode 2 (B-F) takes F off the pane as far as the pane
  covers there (exactly B-F over the interface's opaque pixels, as on the
  screen: the debug battle's interface subtracts so over a portrait and
  its gauges) and, where the scene or the room shows
  through, which no premultiplied form can take F off, darkens it by F's
  brightest channel (before, it darkened the pane's own pixels that way
  too: 3 816 of the battle's 27 708 opaque pane pixels were not the
  screen's). A full-screen untextured one (a fade, a flash), or a
  fill over nine tenths of the screen shown, tints each eye's diorama and
  the pane over what they hold then, weighted by their coverage (the room
  around the table stays as it is); a smaller fill (a window's, a
  letterbox band) goes on the pane. Uploads keep
  their order with the draws: each group of draws between two goes into
  every eye and the pane before the next, so each samples VRAM as the game
  left it for it in every eye (two such groups a frame in the fields
  measured, five in the debug battle).
- **The screen instead.** The frame shows on the flat screen in other
  modes than fields, battles and the world map (menus, movies, the arena,
  booting, the kernel), for 24-bit movie frames, frames drawing a screen
  the game took (menu fades, shatter, transitions) or sampling their own
  picture (the heat haze), while first-person inspection is on, and for
  frames without a camera record or without a scene to place. Both ways
  the view eases over 0.3 s: going to the screen, the last diorama built
  stands, fading out (at one minus the screen's share) as the screen fades
  in; coming back, the frame shown fades in. A frame takes the camera
  recorded within the four vertical blanks before its first primitive
  ([Camera records](#camera-records)): a frame with its scene drawn then
  without a record of its own is built with that camera, and the frames
  the field draws over its last picture as it opens its menu (a fade over
  the held field, no scene drawn) carry it too, but having no scene to
  place, they hold no diorama of their own. Two game frames in a row
  without a camera record or a scene keep the diorama of the frame before
  (the port records with the bits it had, so the first frame after a
  change has none); from the third the screen eases in. While the game
  plays on, the view stays on the diorama once eased in (the review
  camera: 180 frames of the world map turning with L1, 240 of field 12
  with Cross tapped every 30, 120 of Lahan turning). Opening the field
  menu in the forest (map 22, Square, the review camera): the forest's
  last diorama stands for two game frames, then eases out over 18
  presentations of a 60 Hz review (0.056 a presentation) while the
  darkening field and the menu ease in; likewise in field 30, and at the
  end of the debug battle won (the share rising 0.056 a presentation from
  0 to 1, no step larger in 240 frames). The Original renderer does not
  use `GameView`: a host shows its picture on its screen as before.
  A frame the host shows without the view (the device's own picture: the
  Original renderer, a 24-bit movie frame the view does not draw) calls
  `away`: the diorama lies nowhere until it is presented again, and then
  eases in from the screen.
- **Presenting.** `present_diorama(encoder, display, scene, now, eyes, seat)`
  draws the pending trace and, while the diorama shows, the diorama for
  each `DioramaEye` (`view` from the room to the eye, `proj` to wgpu clip
  space, the picture's `size`) into a picture of its own, as first-person
  inspection's eyes are drawn. It returns `DioramaPresented`: the screen's
  share and the flat frame for it (as `present` gives it, for the host's
  screen quad), the eyes' pictures (premultiplied, sRGB-encoded,
  transparent around the scene) and the `opacity` the host draws them at
  over its eye's whole view (one minus the screen's share, dimmed after a
  jump), the pane and the
  `DioramaSpace`: the room from the world and from the table, the volume,
  the pane's pose and size (`Table::pane`: 0.6 m wide whatever the scale,
  its lower edge 2 cm above the middle of the volume's far top edge,
  upright and facing the seat whatever the table's tilt, so it stays
  outside the volume) and the display pixels its picture spans, the
  camera and scale of the frame shown, and the seat (`diorama_space`
  gives the last; `DioramaSpace::first` the space a camera's frame lies in
  as the first of its scene, for tests). The host draws its screen (at the
  screen's share), the eyes' pictures (at their opacity), the pane (at one
  minus the share), its HUD and panels after.
  A presentation that shows no flat picture leaves none for a screen the
  game takes next (it takes the native pixels).
- **What it does not draw.** HD-2D effects are not drawn in the diorama
  (`effects_unavailable`; Graphics' headset card says so while the view is
  the diorama and the profile has effects a headset draws: "The diorama
  does not draw the profile's HD-2D effects; the screen it gives way to
  keeps them."; the profile stays as chosen). They are not drawn for each
  eye because each pass works over the flat frame in the original
  screen's terms: normals and occlusion from its per-pixel depth a native
  pixel apart, fog and depth of field by that depth across the scene's
  span, shafts from its depthless pixels, sprite shadows laid by owner on
  the floor behind each sprite as the screen shows it, and an average
  colour, exposure and focus eased over frames. Per eye they would need
  the eye's own depth in the game's units (the table's depth over its
  scale) and the classes, the eased state taken once a game frame and
  shared by both eyes (two eyes easing their own exposure or focus differ,
  which shows as rivalry between them), shadows and shafts placed in the
  world rather than on a screen, and the passes twice at an eye's
  resolution, on headsets whose automatic quality is already the Low
  tier. The screen's limits have no place in a view from anywhere:
  affine textures, vertex snapping, native resolution, dithering and the
  painter's order are not applied (15-bit colour and filtering are);
  viewport culling keeps what the game drew. The world map's terrain is not
  sealed against its T-junctions.

`--example motion -- --map N --diorama YAW,PITCH,DIST[,SCALE[,FOV]]`
(`--battle`, `--worldmap`; `--turns smooth|snap|fixed`) shows the diorama
through a flat review camera YAW degrees round the default table from the
seat's side, PITCH up, DIST metres from its centre, with the pane and,
while it falls back, the screen 2.5 m ahead; the JSON reports each build
(`diorama`) and `NAME-pane.png` the last pane. From the default seat the
table's centre lies 0.75 m away, 36.9 degrees down. Reviewed (local
pictures) for fields 1, 12, 22 and 30, the debug battle and the world map
from the seat's side, 60 degrees round, behind, above and from further
back: the scene stands on the table as the game's camera saw it with the
scenery the game culls round it (Lahan's houses, paths and townspeople;
field 12's dialogue and portrait and the battle's interface on the pane);
the near walls of rooms are cut away from every side; the light shafts of
the forest and the battle stand as translucent volumes. Holes: surfaces the
game never models, such as the ground under Lahan's raised platform, show
the room. A frame built from the seat's side (yaw 0, 40 degrees up)
holds per frame: Lahan 5 157 scene primitives (4 723 of them faces the
game culled), 126 sprite parts and 25 interface primitives; field 12 287,
15 and 15; the forest 4 905, 22 and 25; field 30 291, 30 and 25; the
debug battle 4 468, 30 and 119 (and four backdrop primitives: its sky);
the world map 5 767, none and 32. Cost (this machine's CPU and lavapipe,
under a load average of about 14 from other builds; 960x720, means per
presentation, each game frame presented once and four times): building a
game frame's diorama with its first presentation 5.2 ms in Lahan, 6.1 in
the forest, 6.5 in the battle and 6.0 on the world map; presenting a built
one again 0.3-1.1 ms. While the table turns (Lahan with L1 held, 120
frames presented four times each at 640x480, two runs each, load average
about 14) the presentation's CPU work averages 1.70 and 2.69 ms with
smooth turns and 1.73 and 1.78 ms with fixed ones, 57 builds in 480
presentations either way (the review measured 5.24 against 1.92 ms when
a build was made again whenever the table had turned half a degree); the
second runs end at one memory hash, the one the review's runs ended at.
Profiled over 120 frames of Lahan (`perf record
--all-user`), the diorama's own code takes 1.1% of the run and lavapipe
70%. The same RAM and VRAM hash end every review angle of a scene.
Battles and the world map were reviewed again (640x480, from the seat's
side 40 degrees up, 70 degrees round, behind and from the left 70 degrees
up; the world map 120 degrees round and from the left 75 degrees up):
the Mountain Path's random battle (map 15) and a Gear battle at map 414,
700 frames each from the field's last frame with Cross every 30, and the
world map's vehicle driven on (Triangle held) for 300 frames at 25% and
100%. The battles' openings stand on the start view's look-at, the whole
stage small; then the combatants stand large on their ground (a person
about 13 cm tall), the Mountain Path's panorama on the wall behind them;
the Gears are models, whole from every side; the people are billboards
facing the seat's way, edge-on from 70 degrees round and from above, their
mirrored picture from behind (as the pane's numbers are); the room's walls
at map 414's far corners stand cut by the volume. The world map's terrain
covers the table at both scales, the vehicle at its centre and its shadow
on the ground, at 100% climbing above it with the ground still on the
table. RAM and VRAM hashes were the same at every angle:
95acd663240213e4 and 40b93bcf4b861613 (map 15), 1c484f7a4beb17d4 and
67464e9990d7ac20 (map 414), da2b9da103c59614 and 895666ed33a9237a (the
world map).

Tests: `xem-render/tests/ps1.rs` with synthetic GTE frames:
`a_diorama_eye_at_the_game_cameras_pose_shows_the_flat_frames_scene` (an
eye placed at the game camera's pose through the diorama's space, with the
game's projection: 0 of the 300 000 pixels outside the interface's
rectangle differ from the flat frame by more than 24 levels, and the
interface is on the pane only, its other pixels transparent);
`split_sprites_at_one_floor_point_keep_the_games_order_in_the_diorama`
(from five sides, two of them beyond the billboards' plane, nothing of
the earlier sprite shows through the later one; without the lift 219
pixels of it did from the seat's side, and lifted towards the seat
whatever the eye, from behind the earlier one covered the later);
`both_eyes_see_one_upright_sprite_apart_only_by_their_disparity` (6.4 cm
apart: the same rows 287-380 and width in both eyes, 75 pixels apart);
`the_diorama_drops_faces_an_eye_sees_from_behind_but_not_double_sided_ones`;
`a_fade_tints_the_diorama_and_its_pane_but_not_the_room` (mode 2's 64
off the floor and off the pane's interface, the room's grey and the pane's
transparency kept);
`a_picture_the_game_draws_over_its_clear_shows_whole_on_the_wall` (the
clear, then a panorama in three panels reaching above the screen's top, as
the debug battle's forest: the same 36 908 pixels of it in the eye as with
the clear left out where the room shows; with the wall's depth written,
17 640);
`frames_without_a_camera_record_or_in_other_modes_show_the_screen` (and
the screen easing into the diorama over 0.3 s, and the screen view);
`the_listener_takes_the_tables_axes_only_while_the_diorama_is_presented`
(the seat's axes through the table while it shows; none after
`away`, then the share 1, 2/3, 1/3 and 0 at +0, 0.1, 0.2 and 0.4 s
as it eases in again; none under the Original renderer, whose settings
give the screen);
`leaving_the_diorama_eases_to_the_screen_as_the_last_diorama_fades_out`
(a menu's frame, a 24-bit movie frame and first-person inspection after a
diorama: the screen's share 1/3, 2/3 and 1 at 0.1, 0.2 and 0.35 s, the
floor of the last diorama in the eye at one minus it, and its pane;
inspection there through `present_diorama`, a path neither headset host
takes: they present inspection through `present_headset`, which takes
the eyes at once, [In a headset](#in-a-headset));
`frames_without_a_camera_record_or_a_scene_keep_the_diorama_for_two_frames_then_ease_to_the_screen`
(frames of a fade without the scene a vertical blank after the last
record, which they take, and field frames five blanks after it: the
diorama before for two frames, not an empty table, then the share rising
0.11, 0.56, 1; the field with its camera again eases back in);
`the_diorama_is_built_once_a_game_frame_while_the_table_turns` (the
camera turning 3 degrees a frame, each frame presented three times at
90 Hz: 10 builds for 10 frames; paused, one build in 90 presentations as
the table turns on);
`after_inspection_the_table_eases_on_from_where_it_stood_at_a_frames_step`
(inspection presented through `present_headset` for a second while the
camera turns 20 degrees; back, the table stands where it stood, 0.90
degrees on a frame later at 72 Hz and 19.98 two seconds on; with the
table's ease clock left running through inspection, its first step back
took it 11.3 degrees at once); `nothing_outside_the_tables_volume_is_drawn`;
`a_subtracting_interface_quad_takes_its_colour_off_the_pane_as_off_the_screen`
(a grey window, 132 as the interface's 15 bits show 128, under a quad
subtracting 64, 66 in those bits: 66 on the screen and on the pane,
opaque; beside the window, over the scene, the pane holds nothing but the
darkening, its alpha 66; the earlier form gave the window 98 on the
pane). The
unit tests in `ps1/diorama.rs` check the build: the backdrop on one plane and the
interface on the pane only, never in the eyes' pass; a sprite's parts
upright in one plane facing the seat at its floor point, as tall as
drawn, the second carrying the lift; split sprites standing together on
their floor point and a jumping one above it; a shadow lifted over a
floor and a face beside it not; which faces may be dropped from behind;
fills after the scene over part of the screen on the pane and one over
all of it a tint; a line before the scene a ribbon on the wall and a
frame without a scene built without fault; a battle's sky projected far
behind the scene (the Mountain Path's: the clear, a band, the sky 14 000
deep, its panorama strip) on the wall at its screen points with the
pictures after it, the scene beginning after them, nothing on the pane,
and its culled faces left out. They check the placement:
the table level under pitched and rolled cameras (the world's up its up,
the camera's level axis away from the seat); a 20-degree turn eased and
a 60-degree one a cut that jumps and dims for 0.25 s, as does a target
jumping beyond a third of the table; Snap holding within 30 degrees and
jumping past them, Fixed keeping the first heading; a battle's table
facing its start view's way, centred on its combatants' extent across it
on the lowest one's ground and spanning that extent with the tallest's
height to spare, standing still and never dimming while its camera
orbits, cuts and shakes (a point drawn through a shaken camera the same
world point) and through recentrings, on the start view's look-at during
the opening or without a combatant (no jump), placed by the combatants
once the turns run (a jump) and staying should the turns pause, and placed
again for another start view; a tall enemy (the debug battle's 1523)
within nine tenths of the volume's height and a small formation (the
Mountain Path's, 241 across) spanning two projection distances' screen;
the world map's target on the table's top on its ground, with no ground
recorded or one above it; beginning 136 and 600 units up, the ground on
the top, or the target eight tenths of the volume above it where it would
stand higher (600 units at 400%); at 100%, climbing 100 units a second for
5 s, the ground at most eight tenths of the volume below the top (and the
followed point's ease), settled back on the top within 25 s of holding its
height, and a hill 100 units high coming under the target moving the
table by less than a twentieth of it; a new scene beginning its own point
and scale (half at twice the depth).
`xem-headless/tests/render.rs`
`a_diorama_seen_from_a_moving_head_leaves_the_game_unchanged` presents
240 frames of field 12 from the same state as a diorama for two eyes of a
swaying head, the scale and the turns changing midway, and ends at the
RAM hash of the same frames drawn by nothing.
`xem-headless/tests/diorama.rs` measures the diorama on the game, its
harness playing a headset host's part (the view configured as a
headset's, the diorama its headset view, the session told whether
first-person inspection shows, each vertical blank presented to a seated
head's two eyes, 64 mm apart and parallel; inspection's through
`present_headset`, the device's own pictures through `away`), measured
on this machine's lavapipe:
- `the_scene_covers_the_walkmesh_seen_from_eight_sides_at_two_heights`:
  in fields 1, 12, 22 and 30 at the default scale, from eight sides (every
  45 degrees round the table) at 30 and 60 degrees up, 1.1 m from its
  centre (640x480, 60 degrees high), the walkmesh's footprint: the floors
  of every actor layer, within the table's volume as the eye sees it, a
  pixel in from its edges. The actor layers are all but the top one, the
  camera's floor (`field_camera_walk_top_layer` walks layer_count - 1,
  actors' floor searches the layers below it); a floor is a triangle
  whose attribute word lacks bit 0x800000, which
  `field_collision_find_actor_floor` takes as no floor on its layer (a
  wall on layer 0, a pit above it). A footprint pixel whose eye shows no
  scene (its class, `GameView::diorama_aux`, the room's or the wall's) is
  a hole. Measured: Lahan 448 of 847 534 pixels (0.053%): 173 of 831 001
  on its ground (layer 0; the 73 seen from behind at 30 degrees lie on the
  volume's edge) and 275 of 19 123 on the floors of its second layer (520
  of its 802 triangles), where actors 21 and 24 stand: at the edges of the
  raised building's roof, which those floors run past; fields 12 and 30
  none of 165 587 and 101 280 (their second layers are the camera's); field 22
  7 900 of 661 193 (1.195%; its second layer's floors lie outside the
  volume), every one on floors the party may not walk (64 of its ground's
  1 255 floor triangles: their attribute bits 5-7 against the leader's
  flags 8-10), a strip beyond the path's right-hand edge, below where its
  modelled ground ends, that shows the room from the table's right (5 164
  of 44 442 pixels from 90 degrees at 30 up). The qualification's bound,
  fewer than 0.5% of the walkmesh's footprint without the scene (a step's
  criterion, not the plan's: the plan asks the missing surfaces measured,
  handled and documented, as here), holds in fields 1, 12 and 30 and is
  not met in field 22 (asserted both ways: a change in field 22 fails the
  test). On the floors the controlled
  character may stand on it holds in all four (asserted): 172 of 817 466
  (0.021%) in Lahan, none of 143 036, 497 227 and 76 639. Counted with the
  camera's floors and the upper layers' pits too, large triangles reaching
  beyond the rooms, the room showed in 13.7% of Lahan's footprint and 64%,
  38% and 72% of fields 12, 22 and 30's. The gap under Lahan's raised
  building, between its supports, shows the room from the seat's side at
  30 degrees up; no floor of the walkmesh lies there, and the game models
  no ground under it.
- `the_scene_covers_a_battles_ground_and_the_world_maps_terrain_seen_from_eight_sides_at_two_heights`:
  from the same 16 views, the ground the frame shown drew, as the diorama
  places it (each face at its points before the division; the faces
  within 45 degrees of level, within the table's volume, a pixel in from
  its edges). A battle's is its stage, which the game draws as models (the
  models whose faces reach across more than the table's 0.9 m: the debug
  battle's two, its floor and the ground about it, 1 504 faces, and the
  Mountain Path's four, 1 089; a combatant's model parts reach 0.38 m at
  most and its sprites are no model; a battle's faces of the world's class
  are effects, shadows and distant scenery), each with its turns running;
  the world map's is its terrain at the end of Lahan's exit near its
  target, the world's opaque triangles (11 084, 10 660 of them level; its
  billboards, clouds, shadows and the sky's bands are quads). Measured:
  the debug battle none of 1 426 250 pixels, the Mountain Path's battle 10
  of 1 262 488 (0.001%, at the volume's edge from 225 degrees at 30 up),
  the world map none of 1 398 533: under the 0.5% bound in all three
  (asserted). At this resolution the world map's unsealed T-junctions open
  no crack; its clouds, semi-transparent quads 0.13 to 0.32 m above the
  terrain at scale 1, are no ground (counted with every level face of the
  world's class, the room or the wall showed in 5.4% of that footprint,
  where the clouds lie).
- `sprites_stand_flat_and_both_eyes_see_one_diorama_apart_along_their_rows`:
  in Lahan (seen from behind the table, where its townspeople stand in
  view), field 30, field 12, the debug battle on foot and the Mountain
  Path's random battle (each with its turns running) and the world map on
  foot at the end of Lahan's exit (at 400%: at 100% its walker spans 5 rows
  of an eye) (from the seat, 640x480 eyes 90 degrees high), 120
  presentations to two eyes make one build a game frame (60 for 60; 120
  for 120 in the battles, which draw a frame every blank); the thickest
  stack of a sprite's parts stands 300 um off its billboard's plane in
  Lahan (79 parts, 385 lifted) and the debug battle (30, 187), 400 um in
  the Mountain Path's battle (40, 23), 200 um on the world map (16, 13) and
  100 um in fields 30 and 12 (`DioramaStats::thickest`): 2D stays 2D; each
  eye of the pair shows exactly what it shows presented alone (0 bytes
  differ), one diorama through two views; and in Lahan, where the sprites
  stand apart, each of their 54 rows holds as many sprite pixels in either
  eye (a billboard's row lies at one depth before both). In field 30 (89
  of 107 rows) and field 12 (44 of 50) characters stand one behind another
  and behind furniture, so each eye sees another part of the farther one
  (up to 8 pixels a row; not asserted there); the battles' rows (25 of 25,
  84 of 84) and the world map's (22 of 22) were alike (recorded).
- `the_pane_holds_the_interface_as_the_flat_frame_draws_it`: beside the
  diorama a view showing the screen from the same trace, and one from the
  trace without what each frame draws before its scene or projects behind
  it (the wall's pictures); on field 12's dialogue, the debug battle (its
  turns running) and the world map, every opaque pane pixel holds the
  flat frame's colour exactly, and the interface's class there (21 272 of
  21 272 and 27 708 of 27 708), but for 21 of the world map's 10 120: the
  first native pixel of six rows of its compass's red cone, the cone's
  left edge (native columns 286 to 293, rows 141 to 150), one 15-bit
  step off, the cause not established (asserted: those 21 pixels at
  most, there, a step off at most). Every opaque pixel of the interface
  drawn after the scene
  (the second view's) is opaque on the pane in its colour (21 272, 21 752
  and 9 708 of them), and where the game draws no pictures before its
  scene (the wall holds its clear alone: field 12, the world map) so is
  every opaque interface pixel of the flat frame (21 272 and 9 708). The
  debug battle's forest, drawn before its scene, lies on the wall.
- `the_title_movie_and_the_title_after_it_show_on_the_screen_the_movie_in_its_own_shape`
  (from boot, 2 200 blanks): the title movie on the screen alone (1 033
  blanks: 925 rebuilt frames, 640x432 for its 320x216 display, its shape;
  52 of the renderer's own frames as its mode begins and 56 of the
  device's own 24-bit ones about the rebuilt ones), then the title (field
  490) on the screen alone (it draws nothing a diorama places).
- `a_fields_menu_and_first_person_inspection_take_the_view_and_give_the_diorama_back`
  (field 30 at twice its scale): inspection's eyes take the view from its
  first presentation, and when it ends the diorama is back at once (the
  screen's share 0) at 2.8186e-3 m a unit as before and where the table
  stood (its room from the world the same to the last bit); a route under
  way (`navigate` to (100, 20)) stops as inspection begins (`stopped`, the
  character at (65, 0, 38)) and none restarts when it ends (120 blanks
  on, the character still there); the field's menu (its mode the field's,
  drawing no scene) eases out over 17 blanks, shows on the screen alone,
  and eases back in over 17 once it closes.
- `a_random_battle_and_its_end_ease_from_the_fields_diorama_to_the_battles_and_back`
  (the Mountain Path, map 15, Up held until its random battle, blank 360,
  then Cross every 30 until the field again, blank 1 600): the field's
  diorama shows on into the battle's first blanks and eases out over 17
  as the battle's entry draws (405-422), the battle's eases in (571-588)
  and shows to its end, its own eased out (1 618-1 635) and the field's in
  (1 762-1 779).
- `leaving_lahan_for_the_world_map_and_entering_a_field_from_it_ease_between_their_dioramas`
  (tests/reference-inputs/world-map-lahan-exit.json, then `enter_field 1`):
  Lahan's diorama eases out (1 843-1 860), the world map's in (2 067-2
  084); entered from it, Lahan's again (2 692-2 709).
- `a_story_movie_eases_out_of_the_fields_diorama_and_back_and_the_next_maps_follows`
  (map 25's scene, its dialogue auto-advanced, as `text_and_movies.rs`
  plays it): its movie (#7) begins at blank 5 469; the diorama eases out
  over the movie's first rebuilt frames (5 515-5 531), the movie shows on
  the screen alone to its last (9 747; 4 233 rebuilt frames, 640x432 for
  its 320x216 display), map 25's diorama eases back in (9 750-9 766) as the
  field goes on there, and map 23's follows (from 9 878).
- `switching_the_headset_view_cuts_to_the_screen_and_eases_back_into_the_diorama`
  (field 30): Graphics' headset view set to the screen shows the screen at
  once, presented as the hosts then present it (`present_headset`); set
  to the diorama again, the screen shows until the frames carry their
  camera records again (5 blanks), then the diorama eases in (17 blanks).
Throughout, between two presentations the screen's share moves by no
more than the time passed over the 0.3 s switch (but where the headset
view changes, the device's own picture shows or inspection takes the
eyes), and the diorama is drawn at no more than one minus it. The
harness runs 242 s on three threads (the suite profile, the machine
shared with other builds).

Not done or not measured (the headsets' own: [In a headset](#in-a-headset)):
the pane's place is a first choice, not one tried by anyone seated; HD-2D
effects per eye were not attempted (What it does not draw, above, says
why); battles were reviewed in four (the debug battle on foot and in
Gears, the Mountain Path's random battle, a Gear battle at map 414) and
measured in two (on foot: the debug battle and the Mountain Path's, Tests
above), and the world map in the vehicle boot mode 3 begins in and on foot
at Lahan's exit (measured there), not their scripted scenes (an event
battle's, the world map's) nor walking on the world map, on its sea or in
its other vehicles; the table's slow settling on the
ground under a target in flight is a first choice (comfort not measured);
the one-sided rule drops a double-sided face the game showed from
its front once an eye sees it from behind; surfaces the game never models
stay holes, beyond the qualification's 0.5% bound in field 22: a strip of
floor the party may not walk shows the room in 1.2% of the walkmesh's
footprint there (measured in four fields, two battles and on the world
map: Tests above), and the room shows through the gap under Lahan's raised
building, where no floor lies; on the world map's pane 21 pixels of its
compass cone's edge are one 15-bit step off the flat frame, the cause not
established; the world map's distant terrain lies beyond the volume at
scale 1; the cost on a headset's CPU (Quest's) and GPU is not measured;
the per-eye pass count grows with the uploads between draws (two to five
groups measured); a fade the game draws over its held picture without
its scene is not shown on the diorama held (the screen shows it as it
eases in); close up, a one-pixel column of the layers beneath shows down
the middle of Fei's head in field 30 (640x480, scale 3, 0.5 m, from the
seat's side and from behind), which the game's picture does not have and
a tenfold lift does not change: its cause is not established.

#### In a headset

OpenXR's loop (`xr_play`) and WebXR's (`render_xr`) show the diorama when
Graphics' headset view is Diorama, and the screen otherwise; first-person
inspection takes the eyes over while it is on, at once, from its first
frame, and when it ends gives the diorama back at once (no crossfade
between two views of the scene from different places), at the scale it
had and, the camera still, where it stood; had the camera moved
meanwhile, the table eases on to it from where it stood, a frame's step
at a time (a presentation that draws no diorama, inspection's or the
screen's alone, stops the table's ease clock: `Diorama::undrawn`; before,
the first step back, its time held to 0.25 s, took the table 56% of the
way at once). Graphics' headset view set to the screen in a session
shows the screen at once (the diorama begins again from the screen), and
set to the diorama again the view eases in from the screen as a scene's
start does.

- **Settings** (`Graphics::xr`, the panel's Headset card in Graphics, the
  same panel in a browser): the view, Screen (the default) or Diorama; the
  diorama's scale (25-400%, one value for every scene, 100% by default),
  the table's height (-100 to 30 cm from the seated head, -45), distance
  (30-200 cm, 60), tilt (0-45 degrees, 0) and turns (Smooth, Snap, Fixed);
  and passthrough (off; its row shows only where the headset can show the
  room: [The room](#the-room-passthrough-and-immersive-ar)). The settings
  service validates them (`diorama scale must be 25-400 percent`, ...).
  Agents change them through the same service, as the panel's rows do:
  `settings_apply` with a `Graphics` block on a headset's `--control`
  (`xem-desktop --xr --control`) and `xem.settings.apply` in a browser,
  answered with the acknowledged settings; the OpenXR smoke and IWER switch
  the view so in a session (Headset qualification).
  Two-hand scaling sets the scale too ([Scaling](#scaling)). Reset diorama
  puts the scale, place and turns back (the view, the screen and
  passthrough stay). The screen's distance
  and width stay in the card under the diorama: the screen it gives way
  to. While the view is the diorama and the profile has effects a headset
  draws (`Graphics::headset_effects`), the card says the diorama does not
  draw them.
- **The seat** (`xem_assist::headset::Seat`). Everything stands in front of
  it: the screen 2.5 m ahead, the table, its pane, the settings panel and
  the keys; first-person inspection's eyes and the Binaural head are taken
  as the seat sees them. It is the reference space's origin (OpenXR's
  LOCAL, WebXR's `local`: the head as the session began) until the player
  recentres: Recenter (an app action; Graphics' button while the panel is
  in a headset, `xem.appOperation({Action: 'Recenter'})` in a browser,
  `--xr-recenter-at N,...` on OpenXR) puts it at the head, facing the
  head's way about the vertical (its pitch and roll left out). When the
  runtime recentres the reference space itself (OpenXR's
  `ReferenceSpaceChangePending` for LOCAL; the `reset` event of WebXR's
  `local` space), the handler both hosts share
  (`headset::reference_changed`) puts the seat back at the origin (the
  runtime moved the space to the head already), cancels the gestures in
  progress (each hand opens again before its next grab; the two hands of a
  scaling under way both open again before anything else), lets go of a control
  deck's captured stick and ends two-hand scaling. Both make the table take
  the camera's point and heading at once rather than ease from where it
  stood (`GameView::recenter_diorama`).
- **Each eye** is drawn every headset frame, paused or not: the renderer
  draws the diorama into a picture of each eye's own and hands them over
  with their opacity (`DioramaPresented::eyes`, as first-person
  inspection's eyes are), and the host composes the eye: the room's
  backdrop, the screen at its share, the eye's diorama picture at its
  opacity, the pane at the diorama's share, then the marks, keys and HUD;
  OpenXR's settings panel is a quad layer over all of it, WebXR's a quad
  drawn last. Under the Original renderer the headset view is the screen
  whatever the setting (`ps1::xr_view`), and a frame the host shows the
  device's own picture in (`GameView::away`) has no diorama.
- **Around the table**, outside its volume whatever the scale: the HUD
  inside the pane's top left corner, as large seen from the seat as on the
  default screen (3 m wide, 2.5 m ahead) wherever the table puts the pane,
  raised to stand on the pane's lower edge where it is taller than the pane
  (`draw::pane_hud`, both hosts; on the screen `draw::hud` sizes it against
  the screen, as against the default one); the keys
  (a control deck's, a shop's quantity keys and a battle's targets right of
  the pane, the pointer's left of it) beside the pane's edges, scaled to
  look as large from the seat as the screen's do from 2.5 m
  (`draw::key_scale` from the pane edge's distance), their columns standing
  on the pane's lower edge where they are taller than the pane; and the
  settings panel beside the pane on its right (`draw::panel`, both hosts):
  its inner edge in the pane's plane beyond the column of keys there (at
  the largest scale the pane's edge gives them, for a picture no taller
  than wide), 4 degrees further round as seen from the seat, its lower
  edge on the pane's (so above the table's volume), upright and turned
  about that edge to face the seat, as wide seen from the seat as below the
  screen. All of it lies further round than the pane and its keys, so the
  game's interface stays in view while the panel shows (Phase 5's panel
  outside the game's presentation). It stands there while the frame shows
  the diorama (easing too); where the screen stands in for the diorama (a
  menu, a movie, the title, first-person inspection: The screen instead,
  [Diorama](#diorama)) it stands below the screen as in the screen view,
  off the screen (OpenXR places it from the frame before, the browser
  draws it from the frame itself, so on OpenXR the first frame after
  either change places it for the other). The keys beside the pane look 0.72 to
  1.19 times as large as the screen's from 2.5 m over the settings' range
  of height, distance and tilt (the screen's own, off its middle,
  0.81-0.83), the HUD 0.9995 to 1.0000 times as large as on the default
  screen, and both the same at every scale. With the default table the
  panel's middle stands 38.4 degrees right of the seat's way and 21.2 up
  (at (0.707, 0.442, -0.893) for OpenXR's 880x1120 panel, 0.505 m wide);
  over the settings' range of height, distance and tilt it stands 4.22
  degrees or more off the pane and 1.33 or more off its keys as seen from
  the seat (a vertical gap of 4 degrees round narrows towards the zenith,
  where a table above the head and near it puts the pane), its middle 28.6
  to 99.2 degrees round: past 90, behind the seat's shoulder line and
  nearly overhead (99.2 degrees round and 71.1 up for OpenXR's panel, 96.0
  and 68.4 for the browser's), only with the table at its nearest, highest
  and most tilted (0.3 m ahead, 0.3 m up, 45 degrees), which lifts the
  pane towards the zenith.
- **Aiming** at the diorama: the pointer source's ray meets the pane (its
  pixels are the screen's: menus, dialogue, choices, battle lists and the
  keys; walking, the pane shows no floor, so a pinch there walks nowhere and
  a tap stops a route) or the table, where the ray's part inside the volume
  becomes a ray in the game's world (`headset::world_ray`;
  `dispatch::Pointed::World`) with an actor's reach of 4 cm on the table
  (`ACTOR_REACH_METRES`, in world units at the table's scale). Walking, a
  pinch walks to the floor that part meets first (`Field::pick_ray`, the
  player's layer) and a tap, or a pointer's pinch, approaches and talks to
  the actor nearest it at its feet or middle (`Field::actor_near_ray`):
  the same `navigate` and `interact` commands as on the screen. Floor marks
  (the cursor's and the destination's) lie on the table's floor at their
  world points, 3 mm above it (`overlay::Place::Floor`), across the world's
  x and z as on the screen (their tops along its z: a Jump mark's chevron
  points as it does there, not mirrored); a ray that meets
  no floor shows none, and on the pane the cursor is flat. In a battle a
  tap (or a pointer's pinch) with the ray through the table chooses the
  target whose upright axis, from its sprite's feet to its height
  (`menu::Ground::sprite` and `height`, as the game keeps them), passes
  nearest the ray, within the actor's reach or the target's half width
  (0.35 of its height, as on the screen, where its box is 0.7 times as
  wide as tall): `Targets::near_ray`, then the same steps and confirmation
  as a target gazed at on the screen (on the attack page, selected only);
  a ray by no target confirms the one selected. The cursor then lies on
  the floor at that target's feet, as wide as its frame. The pane's lists
  and the target keys beside it (for targets that overlap or hide one
  another) take the gaze as before.
- **The listener.** The Binaural head is the head as the seat sees it.
  While the diorama is presented, the listener's axes are the seat's right,
  down and forward taken through the table into the view space of the
  camera the frame shown was drawn with, given with that camera
  (`DioramaSpace::listener_axes`), so the head's turn composes once with
  the table's heading and tilt; the music's stage, turned by the head
  alone, stays put in the room. The presenter hears the axes through that
  camera, so what is heard is the table's world from the seat whatever the
  game's camera does after the frame shown: a battle's table stands still
  while its camera swings, and so does what is heard (before, the
  presenter took them through the camera the game held at the frame's end,
  up to 53.7 degrees ahead of the table shown in the forest battle's
  written swings). The table stands where the seat puts it,
  so through the table is through its tilt from the seat and its turn from
  the world (`DioramaSpace::tilt`, `turn`), both rotations: neither the
  player's scale nor where the seat stands (a recentring) is in the axes,
  not even by rounding (taken through `room_from_world` and normalised,
  they differed from scale to scale in their last bits, moving what is
  heard by about 1e-5 degree; taken through the seat's place in the room
  and back, a recentring moved them by about 3e-7 degree, so a check of
  the view held still through one failed in two of five Monado runs).
  Otherwise (the screen, the Original renderer, the device's own
  movie frames) the listener has the screen's axes: none, or first-person
  inspection's. The hosts give the presenter these axes, as they give it
  the head, whenever they present, the game running or not, so a table
  tilted, recentred or scaled while the game is held reaches the
  presenter at once.
  The listener stays where the screen's is, whatever the head's place: the
  controlled character in a field, the formation's centre in a battle (the
  world map places nothing).
- **Logs.** OpenXR's `xrframe` line adds `xr_view=` (`screen` or
  `diorama`, the setting's view), `diorama=` (shown),
  `share=` (the screen's), `presented_us=` (when the frame was presented,
  microseconds into the loop: the time the view eases by; `wall_us=` is
  taken as the frame's line is printed, after its presentation and
  drawing, so a slow frame's eases by more than its `wall_us=` shows),
  `builds=`, `seat=` (its place; its turn,
  radians), `resets=` and `recenters=`, `pane=` (the pane's middle while
  the diorama shows, beside `panel=`, the settings panel's), and an `event
  reference_space_change` line; a capture in the diorama saves each eye's
  diorama picture beside it (`left-diorama.png`, `right-diorama.png`) and
  the pane (`pane.png`), and prints a `capture pane` line (the headset
  frame, the pane's pose and size, each eye's projection times view, column
  major).
  WebXR's `status().xr` has `diorama`, `share`, `space` (the room from the
  world, column major; metres per world unit; the pane's centre), `seat`,
  `resets` and `recenters`.

Measured on Monado's simulated HMD (20 Hz, 320x240 eyes, this machine's
CPU and lavapipe; the smoke's diorama run in field 30 under HD-2D, the
load average falling from 8 to 2 over the smoke): the diorama shown in
277 of 297 headset frames (the first are the boot's), intervals p50
50.42 ms and p99 61.28 ms, none of 276 late (beyond 1.5 periods), and
the presentation's time on the loop's thread p50 1.17 ms, p95 1.49 ms
(the same smoke's screen under HD-2D, its effects drawn: p50 9.19 ms).
In IWER's Quest 3 (a quarter-scale layer on SwiftShader; the browser runs
at most four game frames a session frame), the web tier's timed stage,
one test after another, measured the session at 11.6 frames a second and
the game at 46.5 with the diorama, against 27.2 and 60.7 as the screen
and 10.9 and 43.7 from within the scene; the diorama runs while
developing gave 7.1 to 12.3 and 25.7 to 49.1 (ten runs, load averages
of 1 to 15).

Tests: `xr-game-smoke.sh`'s diorama runs (Monado; field 30, Fei walked
into the room by the inspection run's log): the diorama shows in most
headset frames and alone at the two captures; the eyes' disparity, the
horizontal shift that best maps one eye's picture on the other's per band
of rows, grows from the far rows to the near ones by 5 pixels or more
(measured, top band first: 10, 10, 16, 18 and 25 pixels at headset frame
150; 11, 11, 16, 22 and 28 at 250), where the screen's (field 12) shifts
alike in every band, within a pixel (4, 5, 5 and 5 pixels); each eye
shows its diorama picture in the renderer's colours (over the blocks of
a 16x12 grid the picture covers whole, 46 and 55 of them, the median mean
colour difference at most 4 levels: 0.0 measured); the app's Recenter at
headset frame 60 puts the seat at the head of that frame, and
`monado-ctl -c`, once the session has drawn 100 frames, reaches it as an
`event reference_space_change` that puts the seat, moved until then, back
at LOCAL's origin on the frame it comes (103 measured); Recenter at 220
puts the seat at the head again; the RAM at game frame 1200 equals
`xem-headless`'s for the same log (23da911a1e0e26bc). In field 12, at
headset frame 120 (its dialogue open, the diorama alone), each eye shows
the pane where it stands: every eye pixel whose centre, taken through the
eye's projection and view onto the pane's plane, meets the pane on an
opaque texel of its picture (the window's frame and text and the
portrait, 8% of the pane: the window's body is translucent) clear of the
texels' edges shows that texel's colour, within 2 levels for 98% of at
least 100 such pixels (measured: all of 311 and 301, at 0 levels). With
the gaze scripted, prayer hands at headset frame 60 open the settings
panel beside the pane (its quad layer shown from frame 61 to 92, the game
held; its middle at (0.707, 0.442, -0.893), the pane's at (0, 0.323,
-1.050), 22.4 degrees round from the pane's right edge and 38.4 from its
middle as seen from the seat: off it) and a pinch on its Close closes it,
and a pinch through the table
at the world point (88.67, 0, -55.33) of field 30 walks Fei there
(moving, then arrived). Its battle and world map runs show them as
dioramas: the debug battle on foot (Start on the selector's defaults, then
Cross every 30 game frames from game frame 1500: its attacks), and the
world map at 25%, the vehicle boot mode 3 begins in driven on by its
throttle (Triangle) from game frame 800 to 1200. From the first headset
frame the diorama shows in (397 and 200 measured, game frames 1194 and
607) it shows in every one to the end. The battle's table centre
(`table=`) shows two world points only, the start view's look-at, (4192,
0, 3680), during the opening, then the combatants' middle, (4140, 0,
4000), from headset frame 447 (game frame 1320) to the end, while the
camera's target took 127 places. The world map's shows the camera's
target across, (0, its height, 0), the map moving under it, the target
climbing from 256 to 472 units up (y down: -256 to -472) and standing
0.031 to 0.087 m above the table's top (within eight tenths of the
volume's 0.55 m) as the table settles towards the ground under it. At
headset frames 600 and 700 (the battle) and 350 and 420 (the world map)
the eyes' disparity grows with nearness (measured: 11 to 21 and 10 to 17
pixels; 10 to 19 and 10 to 16), each eye showing its diorama picture in
the renderer's colours (0.0 levels over 56 to 64 and 31 to 41 covered
blocks), and the RAM at the end is `xem-headless`'s for the same log
(fb5a86347ce8e142, 0cf9bacdeea04a58). Its hands run on the diorama is the
player's own play from field 30's state (a session log's inputs stand in
for the player's, and inspection then stops no walk: a log keeps its own
record), the gaze the eyes': the left hand's Pause grab at headset frame
100 pauses (103) and at 130 resumes, both hands' at 160 toggle once, the
right hand's at 190 resumes; a sideways grab, a fist turned while closed
and a hand found closed toggle nothing (its loss pauses, at 282; the right
hand's Pause resumes), the diorama shown in every frame of them; while
the Pause holds the game (30 frames, one game frame) the head takes 30
poses, the left eye changes over 2% between the captures at 110 and 125
(16.1% measured) and each pair shows the table in depth (the disparity by
band of rows, top first, 10 to 19 pixels at both; the left eye's own move
between them, recorded, 12 pixels in the far bands to 16 in the near:
the head's move seen in depth too); the left hand's
underhand grab looks from within (343 to 382, no diorama) and, again,
gives the diorama back on the next frame at the scale and table it had
(1.408385e-3 m a unit, the table's centre at (0, -32, 90) as before); a
pinch through the table walks (`moving` from 422), the underhand grab
stops it (`stopped` from 430) and none restarts to the run's end; prayer
hands show the settings panel beside the pane for 30 frames, the game
held (its middle where the walk run's stood). From the same state with the
diorama as the view, an agent's `settings_apply` through `--control` sets
Graphics' headset view to the screen once the session has drawn 100
frames and to the diorama again at 200, each answered with the settings
it made: from the first headset frame after the first (101) the screen
shows alone (`xr_view=screen`, no diorama, the screen's share 1), at
once; after the second the view is the diorama again (`xr_view=diorama`),
the screen showing on for three frames until the frames carry their
camera records, then the diorama eases in over five (0.835 to 0.165) to
the diorama alone from frame 210, each step within its time over the
0.3 s switch.
With the diorama as the view from boot, the title movie's 308 rebuilt
frames show on the screen alone at the screen's span, 210x142 for
320x216, and the title (field 490) on the screen. Its fallback runs,
session logs with the diorama as the view, each go from the diorama
alone to the screen alone and back to a diorama, each way through frames
between the two (headset frames): field 30's menu (the hands run's log)
out over 100-104, on the screen alone 105-312, back over 313-317; the
Mountain Path's random battle (Up held from game frame 120 to 700, Cross
every 30 frames from 760): the field's diorama out over 135-140 as the
battle's entry draws, the battle's in over 190-195, out at its end over
530-534 and the field's in over 578-583; Lahan left for the world map
(`tests/reference-inputs/world-map-lahan-exit.json`) and a field entered
from it (`enter_field 1` at game frame 2860): Lahan's out over 615-619,
the world map's in over 688-690, out over 819 and Lahan's in over
856-861. From a state `xem-headless` saved in map 25's scene (its
dialogue auto-advanced to game frame 5080), the player's own play: the
diorama eases out over the story movie's first rebuilt frames (145-150),
1 405 more show on the screen alone, and map 25's diorama eases back in
after it (1 556-1 561). In these four runs the settings panel (`panel=`,
placed before the frame presents) stands beside the pane in every headset
frame after one that showed the diorama (`pane=`) and below the screen,
straight ahead, in every one after a frame that showed the screen alone.
In all thirteen runs with
the diorama as the view the screen's share moves from a headset frame to
the next by no more than the time between their presentations
(`presented_us=`) over the 0.3 s switch, within the log's rounding (the
device's own pictures and inspection take the view at once): 104 frames
between the screen and the diorama over them, the first frames of a
diorama coming late (its first build), so a switch of 0.3 s spans one to
six headset frames at 20 Hz.
`web/tests/xr-diorama.test.mjs` (IWER's Quest 3; field 30):
the automation's 120 frames end at the same RAM in and out of the headset
with the diorama on; the table covers more than half of a row of the left
view (its points unlike the room's backdrop, the layer's top left
corner's colour: 0.76 to 1.00 of each row across the table measured, none
of the row above it); moving the
head 10 cm sideways shifts the rows with texture the move matches by
their depth, the nearest at least 5 points (0.2% of the layer's width
each) more than the farthest (measured: 38, 35, 27 and 30 near the near
edge, 25 across the far part; the uniform backdrop wall's rows are left
out), and the head back gives the same picture;
turned 30 degrees, the view changes, Recenter turns the seat with the
head and puts the table in front again (the same picture); IWER's own
`recenter()` reaches the app as `local`'s `reset` (`resets` 1), the seat
is the origin again and the picture the first; and a pinch (the hand's
select) with the head aimed through the table at a floor point heads the
walk for it, within 2 units (measured: (99.99998, 0, 19.99994) for
(100, 0, 20)), and Fei arrives within 16 units of it (at (95, 0, 23),
5.8 away); the test goes on to scale the diorama and to show it in
immersive AR ([Scaling](#scaling), [The
room](#the-room-passthrough-and-immersive-ar)). Its second test shows the
debug battle on foot (saved by `xem-headless` with its turns running) and
the world map (boot mode 3) as dioramas at 100%: the automation's 120
frames end at the same RAM in and out of the headset in each; once the
diorama shows, moving the head 10 cm sideways shifts the rows across the
table by their depth, by 5 points or more from one to another (measured:
17 to 44 points in the battle, 17 to 36 on the world map, where a flat
picture shifts every row alike); played on, the party attacking (Cross
every 30 game frames) and the vehicle driven on (Triangle held), the
diorama alone shows in all 40 samples 4 session frames apart (312 and 528
game frames), the battle's room from its world the same in every sample
while its camera's target took 17 places, and the world map's camera's
target at (0, its height, 0) at 21 heights, standing in the room where it
first stood across the floor (within 0.0001 mm). Its third test puts
IWER's tracked hands (the test hand model's poses) on the diorama of field
30 at 150%: the left hand's overhand grab pauses, the diorama showing
through it; while the player's Pause holds the game (one game frame) the
head moved 10 cm to the right shifts the rows across the table by their
depth (measured: 38, 35, 26 and 30 points near the near edge, 23 across
the rest), the viewer following it; the left hand's grab again resumes,
both hands' together toggle once (the pauses counted), the right hand's
resumes, and a sideways grab and a fist turned palm down and up while
closed count nothing; the left hand's underhand grab looks from within
the scene (inspecting, the views within, no diorama) from the first
session frame after it and, again, gives the diorama back alone (the
screen's share 0) on the first session frame after that, at 150%, its
room from the world the same in every number (each session frame
recorded as the page's frame callback left it); the field's menu, opened
and closed through the automation (Menu, then Cancel once its commands
take the input), takes the screen's share from 0 to 1 over two session
frames (0.46, 0.91), shows on the screen alone (124 session frames) and
gives the diorama back over three (0.86, 0.55, 0.19), each step within
its frames' time over the 0.3 s switch (inspection on at session frame
307, off at 344, in the final run, the session drawing 6.8 frames a
second on a loaded machine), the settings panel's place
(`status().xr.panel`, recorded each session frame) beside the pane in
every frame that shows the diorama, easing included, and below the
screen, straight ahead, in every one that shows the screen alone (the
menu's); prayer hands open settings beside the diorama's pane (the
diorama shown, the settings' pause holding the game),
the panel's inner edge 9.93 degrees round from the pane's right edge as
seen from the seat (beyond the keys' place), its lower edge on the pane's
(within 0.00001 m), upright and facing the seat (its middle at (0.703,
0.335, -0.898), 0.492 m wide), and prayer again closes them, the
player's Pause never set; then Graphics' headset view set to the screen
through `xem.settings.apply` shows the screen alone from the next session
frame (560; the diorama alone on 559) for 33 frames, and set to the
diorama again eases in from it over three (0.81, 0.48, 0.13) to the
diorama alone. Its fourth test plays map 25's scene from a state
`xem-headless` saved at game frame 5080 (its dialogue auto-advanced, as
the OpenXR smoke's movie run starts): the diorama alone shows to session
frame 137, eases out over the story movie's first rebuilt frame (0.41),
1 959 of its 1 960 rebuilt frames show on the screen alone, and map 25's
diorama eases back in over three session frames (0.89, 0.48, 0.08), every
session frame within its time over the 0.3 s switch, the settings panel
beside the pane in every one that shows the diorama and below the screen
in every one that shows the screen alone (the movie's).
`xem-headless/tests/gestures.rs`
`through_a_diorama_a_pinch_walks_to_the_floor_gazed_at_and_the_pane_shows_none`
(field 30, the table placed from the frame's own camera record as a
headset places it: a pinch at a walkable floor point on the table heads
the walk within 2 units of it and arrives within 16; one on the pane walks
nowhere), `through_a_diorama_a_tap_by_a_villager_talks_to_them` (Lahan:
the villager approached and talked to) and
`through_a_diorama_a_tap_on_a_battle_target_chooses_it` (the debug battle's
targeting after an item, the table placed by the battle's stage: a tap
with the gaze through the table at Elly's middle, (4420, -48, 3840), 0.71 m
from the eyes, steers the game's cursor to her slot and confirms it; with
world rays choosing nothing, as before, it confirmed Fei, the target
selected). Unit tests: the battle targets' rays (`xem-core` menu.rs: at
Fei's chest and feet, Citan, high on the boss and within its half width,
none beside Fei or away from them all, none the game keeps its cursor
from) and their dispatch (`xem-assist` dispatch.rs: the boss and Fei by
the ray, Confirm by none, the pointer's pinch, the attack page's
selection, the cursor at the boss's feet as wide as its frame); the settings' bounds,
defaults, old files and Reset diorama (`xem-settings` graphics.rs); the
panel's Headset rows, Recenter and Reset diorama through the settings
service (`xem-ui/tests/panel.rs`
`a_headset_shows_the_screen_or_the_diorama_and_recenters_through_the_shared_services`);
`Field::pick_ray` within a ray's reach and `actor_near_ray`
(`xem-core` field.rs); world and pane aims in `dispatch`, floor marks at
their world points, right-handed with their tops along the world's z as
on the screen, the pane, keys, HUD and panel outside the volume and the
same at every scale, the keys as large as the screen's over the settings'
range of height, distance and tilt, the HUD as large as on the default
screen within 1% and the panel as wide as below the screen within 1%,
the panel beside the pane (`with_the_diorama_the_settings_panel_stands_beside_its_pane_and_keys_above_the_table_facing_the_seat`:
above the volume, upright and facing the seat, its lower edge on the
pane's, 4 degrees or more round from the pane and its 1 to 10 keys for
four picture shapes, from two seats, over the settings' range),
a ray through the table becoming the world's, the seat, and the shared
reference handler cancelling a grab in progress, ending scaling and
letting a bout's stick go (`xem-assist`); the pane upright above the far
edge at every tilt and scale, the listener's axes through the table (the
same at every scale and from every seat to the last bit), and
the settings' mapping, the screen under the Original renderer
(`xem-render` diorama.rs); the listener's axes only while the diorama is
presented (`xem-render/tests/ps1.rs`, [Diorama](#diorama)).

Not done or not measured: on Monado's simulated
HMD `monado-ctl -c` delivers the event but the head's poses do not jump
(nothing visible moves LOCAL there), and no controller binding recentres
by default; comfort is not measured (no people tests) and nothing ran on a
Quest; a Gear battle was not shown as a diorama in a headset (map 414's in
the review camera's captures only; the smoke plays the debug Gear battle,
nothing pressed in it, on the screen view), nor the world map walking;
marks are drawn over the diorama whatever stands in front of them;
the settings panel moves with what the frame shows, and while it shows
the game holds, so only a held rewind or a view switch changes that under
it: a rewind held under the panel's pointer (OpenXR's controller trigger
on Hold to rewind) that crosses a fallback moves the panel to its other
place, which by the code lets go of the hold (the ray leaves the panel;
Back+LT holds on; not run);
a ray through the table meets the floor through scenery standing in front
of it (no depth test: a floor behind a wall can be pointed at); the HUD's
place and size beside the pane are checked by the unit test alone, not in
a host's eyes; the browser's pane's pixels are not checked in IWER (the
OpenXR smoke checks field 12's dialogue on them); in IWER the diorama's
transitions ran for inspection, the field's menu, map 25's story movie
and Graphics' headset view switched in the session (a battle's and the
world map's ran on Monado and in `xem-headless`'s harness), and no walk
was stopped by inspection there (Monado's hands run and the harness stop
one); the
paused head's motion parallax between two captures is recorded, not
checked: it depends on how far the simulated head, turning as it moves,
went between them (no band of rows matched in one run, 12 to 16 pixels
from the far bands to the near in the next).

#### Scaling

Two whole-hand pinches (each hand's five fingertips gathered, its fingers
not folded into the palm: `gesture::Shape::whole_pinch`, apart from fists
and the runtime's pinch) scale what a headset shows (plan.md, Phase 8):
spreading them enlarges it, contracting shrinks it, by the palms' distance
over theirs as the gesture began; letting go with either hand ends it, and
both open again before either begins another gesture (no grab fires as
they close and no pinch counts: the recognizer's ownership rules). Nothing
is aimed at and nothing reaches the game: no press, command or pause.

- **The diorama**, while it is the view (both hosts tell
  `headset::Scaling::view` every frame from the settings; never under the
  Original renderer): the hands scale it from its setting's scale, within
  the setting's range (25-400%), live (the frames show the table at the
  hands' scale: `Scaling::shown`), and the scale the gesture ends at
  becomes the setting once, through the settings service
  (`headset::commit_scale`: the graphics block as it is with the new
  `diorama_scale`, validated, saved and acknowledged to every subscriber,
  so the panel's slider shows it; Reset diorama puts 100% back). It scales
  about the table's anchor: the table's chain (the room from the table,
  then the scale, the heading, the flip to y up and the shift to the point
  the table follows) keeps that point, the camera's target, at the centre
  of the table's top whatever the scale, so it stays where it stands in the
  room while everything else moves away from it or towards it by the
  scale. The table's volume, the pane, the keys beside it, the HUD and the
  settings panel depend on no scale (they stand outside the volume and
  keep their size: [In a headset](#in-a-headset)). A scaling cut short
  ends where it reached and is committed the same way: the app losing the
  runtime's focus (OpenXR not FOCUSED, WebXR hidden), the runtime moving
  the reference space (`headset::reference_changed`), the session ending,
  or the screen chosen as the view meanwhile. Cut short, it keeps both
  hands as one that ends does: hands still gathered begin no new scaling,
  and nothing else fires until both have opened (`Recognizer::cancel`; a
  cancel with no scaling under way leaves each hand its own gestures: one
  that opens grabs, and the runtime's pinch and taps fire, whatever the
  other hand does). No scaling
  begins while first-person inspection shows; one under way as it began
  ends where it reached when the hands let go.
- **The screen**, otherwise: the hands scale its width for the session, 0.4
  to 3 times the setting's (in memory; a rebuilt movie frame is made at the
  scaled width), and nothing is committed.
- **Synthetic gestures** for runtimes without hand tracking
  (`xem_assist::synthetic`, `--xr-hands N:spread,M:contract`): `spread`
  gathers both hands 20 cm apart and parts them to 50 cm in ten steps, then
  opens them (2.52 times by the palms' distance), `contract` brings them
  from 50 to 30 cm (0.598 times). The browser tests' hand model has the
  same gather pose for IWER's hands.
- **Logs.** OpenXR's `xrframe` line adds `diorama_scale=` (as shown: the
  hands' while they scale it; 0 for the screen), `world_scale=` (the
  table's metres per world unit), `target=` (the camera's target, world
  units), `anchor=` (where it stands in the room, metres) and `table=`
  (the world point the table's centre shows, world units) while the
  diorama shows, and `key=` (a key's width beside the screen or the pane,
  metres); each commit prints `scale commit diorama_scale=N revision=R`.
  WebXR's `status().xr` has `scale` (the screen's), `dioramaScale`,
  `scaling`, `scaleCommits`, `keyWidth` and `space.target` and
  `space.anchor`.

Measured on Monado's simulated HMD (20 Hz, this machine's CPU and
lavapipe; the smoke's scaling run in field 30 under HD-2D, Fei walked
into the room and the camera still): the spread from headset frame 110
took the shown scale from 1 to 2.5213 in ten steps of 0.152 (frames 113 to
122) and committed 252% as the hands opened (124), the contraction from
150 took it from 2.52 to 1.5063 in ten steps of 0.101 and committed 151%
(164), and the setting file held 151 at the end; the table's metres per
world unit followed the shown scale
on every frame (1.408e-3 at 100%, 3.551e-3 at 2.5213); the camera's target
stood still (0.00 units) and its room point, the anchor, moved 0.03 mm
over the spread and less than 0.01 mm over the contraction (the table's
eased point settling, as it does between the gestures); a key beside the
pane stayed 0.10020 m wide; the diorama showed in 277 of 297 headset
frames, intervals p50 50.40 ms and p99 60.71 ms, 1 of 276 late, presenting
p50 1.32 ms and p95 2.09 ms (load average falling from 27 to 4 over the
smoke). In IWER's Quest 3 (field 30, the game stopped) ten steps of
1.5 cm per hand showed 1.152 to 2.518 and committed 252%, the anchor
within 0.003 to 0.16 mm (several runs; the eased point settling after
Fei's walk) and a key 0.1002 m wide throughout.

Tests: `xr-game-smoke.sh`'s scaling run (Monado; the inspection run's log,
scripted `spread` at headset frame 110 and `contract` at 150): exactly two
`scale commit` lines, the first 240-260%, the second 0.6 times it within 2,
each the scale shown on the frame before (within half a percent), the
setting file holding the last; the scale 1 before the spread; every
gesture recognised a scaling one, the player's Pause and the game's
pause never set, the assist state unchanged; over each gesture, while the
diorama shows, the table's scale growing more than 2 times (shrinking
below 0.7 times) and by the shown scale's ratio within 0.01, the camera's
target within a unit and its room point within a millimetre; one key
width on every diorama frame; and the RAM at game frame 1200 equal to
`xem-headless`'s for the same log (23da911a1e0e26bc).
`web/tests/xr-diorama.test.mjs` (IWER's Quest 3, field 30): two gathered
hands parted from 20 to 50 cm grow the diorama live (scaling on every
step), the table's scale more than 2 times and the shown scale within 0.1
of 2.52, the anchor within a millimetre, one key width, the scale shown
last committed once (one settings revision), no pause or pinch; then the
automation's 120 frames from the state at 252% end at the flat run's RAM.
`web/tests/xr-hands.test.mjs` (the fixture, no private input): with the
screen as the view the same spread scales the screen to about 2.5 for the
session and commits nothing; with the diorama the shown scale grows step
by step, the setting takes the last shown one with one acknowledged
change, the screen's scale stays, a contraction commits about 0.6 times
it, and nothing pauses, pinches or opens settings. Unit tests
(`xem-assist`): `two_hand_scaling_scales_the_diorama_about_its_anchor_live_and_commits_once_when_it_ends`
(the spread to 2.52 live and committed once as 252%, the shown view, the
anchor's room point within a millimetre at 25 to 400% with another point
moving away by the scale and the table, volume and pane unchanged, the
clamp at 400% and 25%, the screen's session scaling),
`a_diorama_scaling_cut_short_ends_where_it_reached_and_inspection_scales_nothing`
(focus lost, inspection begun, the screen chosen and a moved reference
each end it where it reached; hands still gathered after the focus or the
reference came back begin nothing until they open; inspecting, a spread
scales nothing),
`after_a_cancel_one_hand_gestures_alone_and_a_cut_scaling_keeps_both_hands_until_both_open`
(cancelled with the other hand half curled throughout, an overhand grab
from open, the runtime's pinch and its tap each fire; cancelled while
scaling, gathered hands begin nothing and a closing hand grabs nothing
until both open, then a grab fires),
`after_focus_loss_one_hands_pause_resumes_whatever_the_other_hand_does`
(the focus lost and back, the left hand half curled, the right hand's
overhand grab resumes), `a_reached_scale_is_committed_to_the_setting_through_the_settings_service`
(one acknowledgement, the block otherwise as it was, Reset diorama back to
100%, 401% refused), `spread_and_contract_read_as_two_hand_scaling_alone_by_their_factors`
(both runtimes' synthetic gestures, nothing but scaling, 2.52 and 0.598)
and the keys, HUD and panel test above, now at every scale from 25 to
400% in steps of 25%.

Not done or not measured: the scaling's feel (no people tests, no Quest
hand tracking: the gestures are synthetic or IWER's posed hands); a scale
per scene or per mode (one global value, as the setting is); battles and
the world map were not scaled in a headset.

#### The room (passthrough and immersive AR)

Where a headset can show the room around the player, the diorama (or the
screen) stands in it; nothing AR-specific is required for it (no hit
tests, planes, anchors, depth or meshes), and VR and the browser's flat
play stay as they are.

- **WebXR.** The page offers Enter AR beside Enter VR only where the
  browser supports both `immersive-vr` and `immersive-ar`
  (`navigator.xr.isSessionSupported`): a headset's browser, with the
  XR-compatible WebGL2 renderer, which immersive VR's support alone
  chooses. A browser offering immersive AR alone (a phone's handheld AR,
  whose input is the screen's touches, which this page does not map in a
  session) offers neither button and plays flat as before, on the
  renderer a browser without immersive VR starts (WebGPU first). Both
  buttons request their mode with the same optional features
  (`local-floor`, `hand-tracking`) and none required. A session whose
  `environmentBlendMode` is not `opaque` (immersive AR's `alpha-blend`)
  gets an `XRWebGLLayer` with alpha, and the host leaves the room's
  backdrop out (`XemApp::xr_session`, `WebRenderer::xr_room`): each view,
  the buffer the views are assembled in and the layer are cleared to
  transparent black, and the compositor's premultiplied colour (what
  WebXR's compositor takes) leaves alpha 0 where nothing is drawn, the
  diorama's own alpha around its scene and full alpha on opaque surfaces,
  the screen, the pane's opaque texels and the keys. The canvas's WebGL2
  context has alpha (an emulated headset such as IWER uses the canvas's
  own framebuffer as the layer); the canvas and VR stay opaque, their
  clears being opaque. The view setting stays the player's: AR shows the
  screen or the diorama as VR does. The page's `status().page.xr` adds
  `arSupported`, `mode` and `blend`, and `status().xr` `blend` and `room`.
- **OpenXR** ([OpenXR adapter](#openxr-adapter)): the room can show where
  the system blends by alpha or offers `XR_FB_passthrough`
  (`Capabilities::passthrough`), and only there does the panel offer
  Passthrough (`SettingsUi::set_passthrough_offered`). With it on and the
  diorama the view, the loop asks the session to show the room behind the
  frame before its eyes are drawn (`XrSession::show_room`), and where it
  shows clears the eyes to transparent black instead of the backdrop. A
  runtime refusing `XR_FB_passthrough`'s objects (not permitted, short of
  resources) does not end the session: that frame and every later one are
  opaque with the backdrop, the loop prints `xr room: refused (...):
  passthrough=unavailable for the session` and the panel's row goes
  (`Room::Refused`); a system whose passthrough properties cannot be read
  starts without the room by `XR_FB_passthrough` (`xr room:
  XR_FB_passthrough unusable (...)`). Monado 25.1's
  simulated HMD enumerates `OPAQUE` alone and offers no
  `XR_FB_passthrough`: the loop prints `xr room: passthrough=unavailable
  blend_modes=OPAQUE`, the panel offers no Passthrough, and a setting
  asking for it changes nothing (`passthrough=0` on every `xrframe` line,
  the backdrop drawn). The APK declares `com.oculus.feature.PASSTHROUGH`
  with `required="false"` (Horizon OS offers the extension to apps that
  declare it; the app runs without it).
- **The wall behind the table.** Where the room shows, the diorama's
  backdrop wall leaves out the game's clear (its fills before the scene,
  black in most fields), which the room stands in for, as it stands in for
  the room's backdrop around the table; the panoramas and sky bands the
  game drew before its scene stay on the wall, its own pictures
  (`GameView::set_room`, said by both hosts every frame; a change rebuilds
  the frame's diorama).

Tests: `web/tests/xr-diorama.test.mjs` (IWER's Quest 3, field 30, the
diorama at its default scale after the scaling part's 252%): with the
game stopped, a VR session and then an AR session are each begun afresh
at that one game frame, the head where they begin, and both place the
same table (asserted equal: a diorama takes its scale's measure as it
begins, so a session begun earlier would differ by a fraction of a per
cent). In VR the layer is opaque at every point read (alpha 255, the
context's alpha notwithstanding); Enter AR is offered; the three sessions'
requests asked for `optionalFeatures` `local-floor` and `hand-tracking`
and no `requiredFeatures`, and their `XRWebGLLayer`s for alpha false,
false and true (the page's calls, recorded in the page: IWER's layer is
the canvas's own framebuffer, which takes no alpha of its own, so only the
request shows what a headset's browser gets); the AR session's blend mode
is `alpha-blend` and the room shows. Over a grid of 40x30 points across
both views, the points that showed the room's backdrop in VR (its clear,
sRGB 29, 31, 39 as the layer holds it) are transparent black in AR for 98%
or more; of those that showed black in VR (the wall's clear, and the
scene's black texels, which AR draws) there are at least 5% of the grid,
85% or more of them transparent in AR; the others, the diorama, its pane
or the keys, are drawn for 98% or more, and the opaque ones show VR's
colour within 4 levels for 95% or more. Measured: 771 room points, 769
transparent (the other 2 black at alpha 1, coverage VR's 8-bit composite
rounds back to the backdrop); 111 black, 102 transparent in AR (the 9
others opaque black scene texels along the layer's bottom row; with the
wall's clear kept in AR none was: 0 of 111, measured once with the web
host's room flag forced off); 318 drawn of 318, all 308 opaque ones as in
VR (median and 90th percentile 0 levels off).
`web/tests/xr-hands.test.mjs` (the fixture): IWER's Quest 3 offers Enter
VR and Enter AR, the same headset without immersive AR Enter VR alone
(`arSupported` false, no XR error), and a device offering immersive AR
alone neither (`supported` and `arSupported` false), its renderer WebGL2
(WebGPU disabled) rather than the XR one.
`xr-game-smoke.sh`'s scaling run (Monado, the
setting asking for Passthrough): `xr room: passthrough=unavailable
blend_modes=OPAQUE`, `passthrough=0` on all 297 frames, every rendered
frame the projection layer alone, and the eye's corner the backdrop,
opaque, as in the screen run. `xem-xr`
`the_room_shows_only_where_the_system_blends_by_alpha_or_offers_fb_passthrough`
(opaque or additive alone: none; alpha blending first or after opaque:
alpha blending; opaque with the extension supported: `XR_FB_passthrough`)
and `the_room_shows_where_wanted_and_a_refused_passthrough_leaves_it_out_for_the_session`
(the session's room state with stand-in objects: alpha blending shown
where wanted with nothing made; `XR_FB_passthrough`'s objects made when
first wanted, kept, dropped when not wanted and made again; a refusal,
`ERROR_NOT_PERMITTED_PASSTHROUGH_FB`, hides that frame, is returned once
and leaves the room unavailable, never asked for again). `xem-render`
`where_the_room_shows_the_wall_leaves_out_the_games_clear_and_keeps_its_pictures`
(the clear and a panorama strip on the wall without the room, the strip
alone with it, the scene and a later fade as they were).
The panel's Passthrough row shows only where offered (`xem-ui/tests/panel.rs`,
[In a headset](#in-a-headset)).

Not done or not measured: OpenXR's alpha blending and `XR_FB_passthrough`
paths have run on no runtime (Monado offers neither, and nothing ran on a
Quest), so the room behind an OpenXR frame is not observed, nor
premultiplied against straight alpha on a real compositor, nor a
runtime's refusal of `XR_FB_passthrough` (its handling is unit-tested
with stand-ins only); IWER's immersive AR has no camera picture (its
synthetic environments, `@iwer/sem`, are not installed), so only the
layer's alpha is checked, not a composition over a room; the wall without
the clear was checked in field 30 alone, not where a wall also holds
panoramas or sky bands (a battle's, the world map's); handheld AR (a
phone's immersive AR) is not offered; comfort over passthrough is not
measured (no people tests).

### HD-2D effects

The effects use only the game's own pictures and the frame's per-pixel GTE
depth and class; no art is added, redrawn or replaced, and sprites keep their
pixels and animation (no smoothing unless a filter is chosen). Interface
pixels (primitives without a GTE projection, or projected with another screen
offset than the scene's, such as the field compass, translucent ones
included) keep their colours through every pass; full-screen translucent
fades count as scene. A face of the scene whose texture fades into the
colour its buffer was cleared with, at each of its corners and along its
outline but for one texel in ten, is sky the game painted (Shevat's sea of
clouds below its towers, the sky around each cloud; not the desert's
floor, whose sand merely shares its clear's colour): it takes the cleared
sky's place in the auxiliary target, without depth, so no light, shade or
haze sets it apart from the sky it meets. The look aims at Square Enix's HD-2D (Final Fantasy
Resonance's field and battle pictures: a sharp playfield with soft
foreground and background, glowing lamps that light what is near them, airy
distance haze, warm highlights and cool shadows). In order: the frame's
average scene colour, eased over frames; lighting from depth-derived normals
taken a native pixel apart (key light and ambient, gentler over baked
Gouraud or modulated shading, sprites facing the camera); ambient occlusion
from nearer neighbours; sprite shadows (each sprite the game drew, by its
owner address, lays its silhouette on the floor and a soft contact ellipse
under it, both at the floor point the game keeps for it, so a jumping
character's shadow stays on the ground, smaller and fainter the higher it
is; a surface takes them in full from a little in front of that point
backwards, fading out nearer, with floors taking more than upright
surfaces, so nothing flickers at the caster's depth); aerial perspective
(distance lightens and desaturates towards a haze at the scene's own
brightness); bloom (13-tap down, tent up; coloured lights more than white
walls, characters barely), which also lights the surfaces around bright
sources; light shafts from bright depthless or distant pixels; depth of
field keeping the playfield sharp (the subjects, sprites and models of a
character's size, near in depth to the one nearest the screen's middle,
and every subject in the middle of the screen, else the middle depths
there; blur growing beyond over parts of the scene's depth span, faster
towards the camera, weighted towards bright samples; eased between
frames); grading (exposure towards a bright middle grey within limits,
cool-shadow and warm-highlight toning, saturation, a highlight shoulder,
contrast, vignette). Passes whose effect is off are not drawn. Scene
tuning (light direction and colour, fog tint and range, shaft source,
thresholds, grading, shadow lean, length and strengths) is presentation
metadata in
`crates/xem-render/src/ps1/scenes.json`, keyed by `Session::scene()`
("field:22", "battle", "worldmap", ...).

`cargo run --release -p xem-headless --example validation` writes
`build/validation/` (local only: pictures of the game): a sheet of ten scenes
(Lahan's exterior, an interior with a dialogue window and portrait, a room
with characters, the forest, the world map, an on-foot and a Gear battle
reached through the original's debug battle selector, the field menu, the
opening text, a movie frame) under Modern, HD-2D, PS1, wider framing (16:9)
and wider framing with the camera pulled back to 2.5x, and a report: the RAM
hash after each look (equal), the pixels each look changes, the interface
pixels HD-2D changed (0 in every scene), and the interface pixels that
differ from the console's own picture scaled up (with an image of them;
opaque and translucent apart): opaque ones 0 in every menu, text and world
map scene under every look (the map's marker included) and in the fields
but for the field compass, a model drawn at the picture's resolution (4 120
to 4 702 of the scaled pixels); in battles only on the flat forest backdrop
where the battle's translucent 3D light shafts and the models' edges, drawn
at the finer scale, lie over it (the HUD itself matches). Translucent
interface pixels (windows, the Gear HUD, the minimap) show the scene
beneath, drawn at the finer scale, after the effects under HD-2D. `--example render -- --map N
--profile hd2d --compare` draws one scene beside Modern;
`XEM_DEBUG_VIEW=shadows|depth|class` shows the effects' inputs.

`--example motion` checks frames in motion and their cost: from a field, the
world map, a battle (optionally won, for the result screens), boot
(`--title`), a host's saved state (`--state`) or a session log (`--script`),
traced and presented from boot as a host does with `--from-boot` (`--load-at`
loads a field's state over the title screen, as a player loads a save;
`--cards DIR` inserts memory card images, for the title screen's Continue),
holding or tapping buttons (taps over a script's pads too; `--press` one at
a kept frame), each game frame
presented one or more times through
`GameView` (the characters following the view unless `--original-actors`;
`--ff N` runs N game frames for each presented one, as fast-forward does;
`--original FROM,TO` shows those frames with the Original renderer, then the
view again),
it writes a contact sheet (beside the console's picture with
`--compare`), an animated PNG of the last 240 pictures, a probe of the
primitives at a point, how many culled faces each game frame drew (sprite
parts among them: `revealed_sprites`; in the frame shown: `shown_revealed`), how many sprite actors the field
stopped as off screen (`still`), the final RAM and VRAM hashes
(`memory_hash`, `vram_hash`: the software GPU's picture), and the
mean, 95th-percentile and worst cost of the game frame, the trace, the
presentation's CPU work and the GPU's. Measured on Lahan walking (1080p
output at three times the original, this machine's CPU and lavapipe, means
per game frame over 90 frames): the game alone 2.36 ms; traced, under Modern
2.71 ms with 0.68 ms of presentation CPU (0.40 ms over 300 frames); wider
framing 2.90 and 0.88 ms; pulled back three times 4.55 and 1.70 ms (it draws
2 840-3 080 faces the game culled each frame, steady over 900 frames); HD-2D
adds 0.4 ms of CPU. GPU times there are a software rasterizer's (Modern
4.4 ms, pulled back 12.9 ms, HD-2D 36 ms at 1080p), useful only relative to
each other: lighting, ambient occlusion, depth of field and light shafts are
the costly effects. On the host's GPU (`scripts/xem-gpu-host`, an RTX 5070
Ti, i9-9900K; 4K output at the automatic scale, eight times the original,
each game frame presented twice as a 120 Hz display does; means in ms, the
game frame including the port's records and the software GPU, presentation
CPU per presentation): Lahan walking under Modern 1.62 game, 0.19
presentation, 0.17 GPU; wider 1.79, 0.52, 0.29; wider and pulled back five
times 2.68, 0.92, 0.29; HD-2D wider 1.80, 0.67, 0.89 (pulled back: 2.73,
1.17, 0.92); a debug battle wider 4.94, 1.16, 0.44; the world map's battle
wider and pulled back three times 3.04, 0.72, 0.33. Where the time goes
(`perf record --all-user --call-graph lbr` in the runtime shell, which pins
`perf`): the software GPU and the GTE in the game frame, then the replay's
vertices and its uploads. For them the GTE's steps are inlined (the module's
units otherwise reach its optimizer `noinline`), the software GPU steps its
triangle edges and only the attributes that vary, the trace's stores sit in
a table of RAM's words and culled faces' words in place, the replay keeps its
buffers and uploads only the rows of VRAM a new frame changed; none of it
changes the game's RAM or the software GPU's VRAM (`memory_hash`,
`vram_hash`).

### Headsets

The game is shown as a flat, monoscopic virtual screen in front of a seated
player: each eye or view draws the same presented frame with its own view and
projection; nothing claims reconstructed 3D there. With Graphics' headset
view set to the diorama, each eye sees the scene in 3D on a table in front
of the seat instead, the interface on a pane above it, and the screen where
the diorama gives way to it ([In a headset](#in-a-headset)); two
whole-hand pinches scale the diorama, or else the screen
([Scaling](#scaling)), and where the headset can show the room (a
headset browser's immersive AR, OpenXR's passthrough where offered) the
room stands around them in place of the backdrop and behind the table in
place of the game's clear ([The
room](#the-room-passthrough-and-immersive-ar)). Everything
stands in front of the seat, which Recenter moves to the head and the
runtime's own recentring puts back at the reference space's origin. The game runs from the host's
clock (at most four catch-up frames per host frame, so a 15 Hz session still
runs all 60), once per headset frame, never per eye; the settings use the
Headset class (Low tier, depth of field off unless enabled).

- **OpenXR** (`xem-desktop --xr --disc DISC`; the Android host's `--xr`
  argument, for a standalone headset such as Horizon OS, enters the same loop
  once the game is booted, with the activity and JavaVM as the loader's
  platform data; `crates/xem-host/src/xr_play.rs`): the
  screen 2.5 m ahead (Settings: headset), the settings panel a quad
  layer (left menu button, or both sticks' clicks), movie frames on the
  same screen (rebuilt as wide as the screen spans in an eye:
  [Movies](#movies)); controllers are the game bindings' gamepad
  ([Controls](#controls)) after the app's chords take theirs ([Saves and
  time controls](#saves-and-time-controls)); `--control` answers agents'
  commands between frames as in the window (but `import_disc`). One
  `xrframe` line per frame (with `ff=`, the fast-forward factor the
  session runs at, 0 when off, and `hud=`, what the HUD shows: `ff`,
  `rewind`, `notice`), and an `app` line per operation with the game's
  vblank and memory hash just after it. `src/runtime/scripts/xr-game-smoke.sh` (Monado's simulated
  HMD, null compositor) checks 57-63 game frames a second against the
  simulated 20 Hz headset, sustained pacing (wall-clock frame intervals: p50
  50.06 ms, p99 under 57 ms, at most 5% beyond 1.5 periods; measured on
  lavapipe), a paused game with head tracking and rendering
  continuing, the RAM at frame N equal to `xem-headless`'s, stereo eye
  captures of the screen, an HD-2D field on it, and the title movie
  rebuilt (`movie=1`) at the screen's span in an eye (`fov=`) with no
  sides while wider framing is on (`side=0`, the renderer's frames before
  it with sides) and, in a run under the Original movies setting, the
  device's own (`movie=0`, 320x216). Its inspection run
  (field 30 under HD-2D, Fei walked into the room by the session log's
  pads; the underhand grab at headset frames 150 and 270, the Pause grab
  at 180 and 240) checks that the view is on from the first grab to the
  second (`fp=1` on every frame between, `fp=0` after), that the eyes are
  drawn from within the scene (`within=1`) on all but the frame or two
  before a game frame records the place, also on every paused frame (the
  game frames still, the head tracked), that both eyes captured twice
  while paused are covered by their pictures (over 90% of the view unlike
  the room), that those pictures as the renderer drew them (saved beside
  each capture as `left-picture.png` and `right-picture.png`) hold the
  room (over 100 colours) and differ between the eyes and between the
  captures (the simulated head sways), that each eye shows its picture in
  the renderer's colours (the median over 192 blocks of their mean colour
  difference at most 4 levels), that the screen keeps its size (`scale=`),
  and that the RAM at frame 1200 equals `xem-headless`'s for the same log;
  it prints the pacing and the presentation's time (`present_us=`) from
  within the scene and as the screen
  ([First-person inspection](#first-person-inspection)). A shorter run
  under the default Modern profile (no effects: each eye's picture is the
  frame itself, whose alpha is the mask bit) turns the view off and on
  with the underhand grab at 195 and 210 while the Pause grab holds the
  game (`fp=0` only between them, the game's frames still, `within=1`
  again at once) and checks its eyes captured at 230 the same way. Its
  diorama runs check the diorama in field 30: depth in the eyes'
  disparity, Recenter and Monado's recentring of LOCAL (`monado-ctl -c`)
  putting a moved seat back at its origin, the RAM against `xem-headless`,
  the settings panel beside the pane opened and closed, a pinch
  through the table walking Fei and an agent's `settings_apply` switching
  the headset view to the screen and back; and in field 12 the dialogue on
  the pane in both eyes ([In a headset](#in-a-headset)). Its
  saves run plays the player's own game (no session log) from field 30,
  a state `xem-headless` saved that an agent's `--control` command loads
  at the first headset frame: prayer hands open Settings, pinches on the
  panel choose Saves and Save state, the panel closes and opens again for
  Load state, then Time's Start fast-forward, and scripted controller
  buttons (`--xr-pad N:BUTTONS,...`, for runtimes whose controllers have
  none to press) hold Back+RT and later Back+LT. It checks the agent's
  reply, that Settings held the game while it saved, the HUD's two results
  (`hud notice="State saved"` and `"State loaded"`, each shown for four
  seconds of headset frames), that Load state left the vblank and memory
  hash Save state did, that `xem-headless` restores the state file the
  panel wrote to that RAM, that fast-forward ran the game at 0.8 to 1.1 of
  60 frames a second times the factor the session paced it at (the 10×
  setting is paced at what the 20 Hz loop's 12 ms batches reach, which
  follows the machine's other load: in smoke runs here on lavapipe 566
  frames a second at a mean factor of 9.56 with little other load, 329
  at 5.64, and 205 at 3.51 beside another track's test suite, each 0.97
  to 0.99 of it), 60.0
  frames a second once the chord turned it off, and that Back+LT held for
  two seconds rewound (game frame 2895 back to 2102) under the HUD's
  rewind icon, the game playing on after. Its held run (the same state)
  opens Settings with Back+Start at headset frame 200 and holds Back+LT
  from 205 to 211, then points the right controller's ray at the panel
  (`--xr-pad N:BUTTONS@panel:X/Y`, the entry's R1 its trigger: Monado's
  simulated controllers press nothing and point where no test chooses), clicks
  Time, holds its Hold to rewind from 230, and closes Settings with
  Back+Start at 245 while the trigger still holds it, letting go at 270.
  It checks that both rewinds go on through Settings' pause (the panel
  shown, the game not paused, its frames going back under the HUD's
  rewind icon), that Settings hold the game again once Back+LT is let
  go, and that closing them lets go of Hold to rewind in that frame
  (`UiRewind(false)` at 245), the game playing on behind the closed
  panel with the trigger still held (in a run here Back+LT rewound game
  frame 1306 to 1194 and Hold to rewind 1194 to 1014, and the game ran
  on to 1086 by headset frame 269; with that release removed it went on
  rewinding behind the closed panel until its history ran out). Its
  controllers run (the same state, the player's own play) plays
  first-person inspection on the controllers: Back released alone
  (scripted) at headset frame 42 turns it on, Down, North (Jump) and L1 (a
  camera turn) held from 60 to 120 leave the character where it stands
  (`player=`, the controlled character's place in whole world units:
  0,0,90 throughout), Back at 132 turns it off and Down held from 140 to
  160 walks (to 0,0,50 in a run here), Back at 172 turns it on again, the
  right stick (`--xr-stick N:X/Y,...`, held from that headset frame to the
  next entry, beside the controllers' own: Monado's have none) pushed right
  from 190 and again from 205 turns the view 30 degrees each push (yaw
  0.5236, then 1.0472), and an agent's `settings_apply` to the Original
  renderer, sent once the session has drawn 230 frames, ends it in the
  headset frame it arrives in (231 in that run). Its
  held-hands run (the same state, hands alone, the gaze the eyes') has an
  agent's `app {"Action": "Recenter"}` arrive once the session has drawn
  100 frames, opens Settings with prayer hands at 200, picks Time with a
  pinch at 220 and holds a pinch on Hold to rewind from 242 to 281
  (`pinch-hold`), the gaze moving down off the button at 260 (`rest`, an
  aim alone). It checks the agent's reply and that `recenters=` goes from 0
  to 1 at that frame, that the pinch's start and end are Hold to rewind's
  (`UiRewind(true)` within ten frames of 242, `UiRewind(false)` at 282 to
  284: a press the narrow panel's form takes only once the pointer has
  stayed put a moment, and the gaze moving away neither pans the form nor
  lets go), that the game goes back at least 120 frames through Settings'
  pause under the HUD's rewind icon meanwhile, and that Settings hold it
  again after. Its
  interchange run loads the same state with `--load-state` (frame 710)
  under HD-2D with no focus pause (Monado's session is focused only once
  it runs) and plays it on with no input, writing the headset's state at
  frame 1010 (`--save-state-at`) and ending at 1310: the RAM at both is
  `xem-headless`'s from the same state (`e6366cb7f5d61d8c`,
  `a7cb468cae1e226e` in a run here), the headset's state is
  `xem-headless`'s at 1010 byte for byte (the screen without wider
  framing asks for no reveals), and loaded natively it goes on to the same
  end ([State interchange](#state-interchange)). Its
  qualification runs (either hand's grabs and two-hand scaling in field
  30, a scene tour, the world map's recentring and the battles) are in
  [Headset qualification](#headset-qualification); the `xrframe` line's
  `keys=` gives the keys beside the screen (how many, their widths and how
  far each stands beside the screen's edge) and `panel=` where the
  settings panel's middle stands. Its Binaural runs (field 15, Binaural
  on) hear the head through the frame log's `binaural=` (what the
  presenter holds: [Binaural sound mode](#binaural-sound-mode)): one on
  the screen, then in first-person inspection held by the player's Pause,
  with the app's Recenter, Monado's recentring of LOCAL and
  `--xr-lose-tracking N,M` (headset frames N to N+M-1 taken as the
  runtime reporting no head and no valid views, which Monado's simulated
  HMD never does) in the pause and inspection ended there; one in the
  diorama at 25% of its scale, held by `--xr-pause`, with two-hand
  scaling to the 400% limit and back to 25%, Monado's recentring (no
  scaling under way: a recentring ends one) and Recenter in the pause.
  They check that the presenter's
  head is the logged pose's turn from the seat, that with it taken off
  every effect holds still while the game does (the head applied once),
  that the session's end leaves the presenter no head and no listener's
  view (the `exit` line's `binaural=`), and the rest the Binaural section
  lists; both runs' RAM at game frame 1 200 is `xem-headless`'s for the
  same log.
- **WebXR** (browser host): a browser that offers immersive VR renders
  through an XR-compatible WebGL2 context from the start (WebXR layers on
  WebGPU are not testable; flat WebGPU stays for the others); Enter VR runs
  the session (`local` space), whose animation frames step the game exactly
  as the canvas's did while the canvas loop rests, and draw each view's screen
  into the layer (eye buffers assembled and flipped on the GPU). `?xrScale=F`
  sets the layer's framebuffer scale. `web/tests/render.test.mjs` also
  compares WebGPU's and WebGL2's pictures of the same frames (99.97-100% of
  pixels within 8 levels). `web/tests/xr-game.test.mjs` (IWER's
  Quest 3) checks the backend, two views, the game near 60 frames a second at
  a slower session rate, both views showing the screen, paused head tracking,
  recentring on the screen (the head moved and turned a quarter away, both
  views' centres the room's; the player's Recenter puts the seat at the
  head, facing its way, and IWER's own recentring, its `local` space's
  `reset`, at the origin, the screen in front after each), exit, and an
  unchanged RAM hash; its movie test shows the title movie
  rebuilt in the headset and the device's own under Original, the screen
  without wider framing's sides (`status().xr.screen`)
  ([Movies](#movies)); its inspection test (field 30) toggles
  the view with the right controller's stick click, turns it one
  30-degree step with the stick (no pitch; the Binaural listener faces the
  turn with the head on it, and the camera's view again once it is off),
  toggles it off, then with tracked hands turns it on with the underhand
  grab (the views' centres no longer the screen's), pauses with the
  overhand grab and turns the head a quarter (the centres change, the
  game's frames do not, the head back gives the same picture), then,
  still paused and with the controllers, toggles it off and on with the
  stick's click (looking from within again at once), turns it a step,
  ends it with the Original renderer and turns it on again under Ex
  Machina, the game's frames still throughout; it resumes, turns it off
  (the screen again at its size), turns it on again and exits, the canvas
  going on with the view. Its scene tour loads natively made states of
  field 30, field 12's dialogue, the field's menu, the world map, a
  battle on foot and one in Gears, the title movie and the title screen
  with the game running: each shows on the screen in front of both views
  (at least 12 of 18 points about the views' centres unlike the room;
  17 or 18 measured), the left hand's underhand grab is recognised in
  each and looks from within the scene in the field and the dialogue
  only, and a battle loaded after the field ends the view (pictures in
  `build/web-test/xr-tour-*.png`, read for this qualification: the
  pointer's keys beside the screen, the world map's deck on its right,
  the title's Start key, the menu's list region and the dialogue's
  window as the game draws them). `web/tests/xr-diorama.test.mjs` checks
  the diorama: parallax, Recenter, IWER's own recentring and a pinch
  through the table heading the walk for the floor point gazed at
  ([In a headset](#in-a-headset)). The session asks for `hand-tracking`;
  each frame the page passes the viewer's pose, the session's visibility,
  each hand's target ray and its 25 joints to the headset core
  ([Headsets: hands, gaze and control decks](#headsets-hands-gaze-and-control-decks)),
  whose Pause and settings hold the game's clock; the settings panel is a
  quad below the screen in the session (0.55 m wide, off the screen as
  OpenXR's is while the screen's lower edge, whatever its scale, stands
  less than 69.5 degrees below the seat's level: [Headset
  qualification](#headset-qualification); above the table with the
  diorama as the view) that takes the
  pointer source's ray with a pinch, or a
  controller's ray with its trigger (`select`). The button reads Enter VR
  or Exit VR; a hidden session (the browser's own menu) or its end lets go
  of held input. After each immersive frame the page runs the operations
  asked for in it and writes the cards the game changed to IndexedDB, as
  after the window's ([Browser host](#browser-host)). The saves test of
  `web/tests/xr-game.test.mjs` plays the slice's save route
  (`slice-save.json`, `xem-headless` up to frame 10 100, card 2
  unformatted as there) to its end in the session: the game formats card
  2 and saves to it, and IndexedDB holds that card (formatted, the save in
  its first block, equal to the session's image) while the session runs,
  the canvas's loop never having run (the session keeps rewind history,
  as the player's play does). Once the history holds its 30 seconds of
  play, it clicks the panel's Saves
  and Save state with the right controller's ray and trigger (the state
  in IndexedDB, its checksum verified, the HUD's "State saved", the game
  held by Settings' pause), keeps Settings shown 4 seconds more and checks
  that the pause took no play time off the history (as many states after
  it, one fewer at most: 28 from game frame 10 100 before and after in a
  run here), turns fast-forward on with Back+RT (the right
  stick's click with the right grip; inspection not toggled) and checks
  the game's rate against 1× at the factor the session paces it at
  (above 1.2, and the rate 0.7 to 1.3 of 1× times it: in runs on IWER's
  software WebGL here 38.1 frames a second at 1× and 66.4 at a mean
  factor of 1.73, 1.01 of it; 56.4 and 79.6 at 1.55, 0.91 of it; and
  30.0 and 46.8 at 1.50, 1.04 of it, under other tracks' load), turns it off with
  Time's Stop fast-forward, holds Back+LT (the right stick's click with
  the left grip) with the panel still shown, which rewinds through
  Settings' pause under the HUD's rewind icon (game frame 11 809 back to
  11 507 here) and, let go, leaves the game held by Settings again,
  loads the state by the ray (the saved RAM again) and has Export state
  backup refused in the session.

In a field both toggle first-person inspection with the underhand grab
and Toggle first-person view's chord (the right stick's click with the
right trigger, Back+R1) anywhere but in its menu, and with the controllers'
Back (the right stick's click) released alone only where Select toggles
it, while the player walks where Select shows no picture (the controllers
have no R3), where Ex Machina draws it (the Original renderer ends it,
paused or not, and none toggles it there), whether the game runs or the
player's Pause holds it (not while Settings are open). Agents turn it on,
off and around with `--xr --control`'s `first_person` (OpenXR) and
`window.xem.firstPerson` (WebXR, the page's), as on the flat hosts, but
a `pitch` turns no eye: the head does the pitching, and the pitch shows
only in the view's flat picture
([First-person inspection](#first-person-inspection)). Turned on during a pause, it
looks from within once the game's next frame records the character's
place, at once if the frame shown has it (the view turned off and on
within the pause). The view
is immersive: each eye's picture from the character's eye, turned and
moved with the head, covers its whole view, and the game's interface
shows alone on the virtual screen's pane, at its place and size
([First-person inspection](#first-person-inspection)); the right stick
turns it 30 degrees a push. The player's input leaves the character where
it stands as on the flat hosts, and the Binaural listener looks along the
view's heading before the head turns it. Each headset frame presents the
eyes again, paused or not: OpenXR from its views' poses in LOCAL (held
through frames the runtime cannot locate them in) and their fields of
view, WebXR from each view's inverse transform and the tangents its
projection matrix holds (`XrView::eye`). OpenXR's `xrframe` line reports
`fp=` (the view on), `within=` (drawn from within), `look=` (yaw, pitch),
`player=` (where the controlled character stands), `scale=` and
`present_us=`; `--xr-capture-at N,...` captures both eyes
at those headset frames (from within the scene, with the renderer's
picture for each); `status().xr.inspecting` and `.within` show it
in a browser. The settings panel's Controls section lists the grab, the
right stick's click, the head and the right stick in place of the flat
hosts' keys (in a browser, while the session runs). The canvas and the
session share the browser's view, so it carries across Enter and Exit VR.

## Controls

Humans and agents play through one vocabulary of typed actions
(`xem_core::action::GameAction`): Up, Down, Left, Right, Confirm, Cancel,
Run, Jump, Menu, CameraLeft, CameraRight, Start, Select, L2, R2, and the
battle's attack inputs AttackWeak, AttackStrong, AttackFierce and
AttackEnd. Each holds one pad button. The game reassigns its face and
shoulder buttons through `pad_button_assignment` (0x80050238; `pad_init`
swaps Circle/Cross and Triangle/Square), so an action resolves through the
table as the game holds it (`Runtime::button_assignment`): Confirm is the
button the game reads as talk and confirm whatever the table says.
Agents send `hold {actions}` (held until the next hold) or `act {actions,
frames}` (held for that many frames) to the command layer; nothing passes
through SDL, the DOM or the operating system. `actions` is a list of names.
While first-person inspection shows and the player walks, the session
refuses those holding the directions, Jump or the camera's turns
([First-person inspection](#first-person-inspection)).

Hosts translate their platform events into actions through the game
bindings (`xem_settings::bindings`, saved as `game_bindings`): for each
action, any of its keys (`KeyboardEvent.code`), gamepad inputs (the standard
mapping's buttons South, East, West, North, L1, R1, LT, RT, Back, Start, L3,
R3, Up, Down, Left, Right, and each stick's directions past half travel,
`LStickUp` .. `RStickRight`) and mouse buttons (`MouseLeft`, `MouseRight`,
`MouseMiddle`) holds it. Bindings have two contexts: Play everywhere, and
Attack while the battle's attack inputs take the buttons
(`xem_game_combo_input`), where an input Attack binds means its attack
action instead of what Play binds it to. An input may not hold two
different buttons in one context. `xem_settings::player::PlayerInput` holds
the inputs and resolves them, the same code on desktop, Android, headsets
and the browser; the mouse camera's sideways motion holds CameraLeft or
CameraRight while it lasts. Mouse sensitivity is saved as 10–200 percent,
defaults to 50 percent of the previous rate, and scales both field camera
motion and first-person mouse look. The mouse deadzone beside it (0–100
pixels of motion, 40 by default) holds back the field camera's motion
within that distance of where the mouse came to rest (no motion for a
tenth of a second), so a nudge turns nothing; motion past it turns the
camera, the deadzone following along until the mouse rests again.
First-person look takes every pixel. A headset's
controllers are the gamepad (`xr-standard` layout: A South, B East, X West, Y North, triggers L1/R1,
grips LT/RT, the right stick's click Back, the left's Start, both sticks),
natively (`xem_host::xr_play::gamepad`) and in the browser (`xrGamepad`).

| Action | Keys | Gamepad | Mouse |
| --- | --- | --- | --- |
| Up, Down, Left, Right | W, S, A, D | D-pad, left stick | |
| Confirm | E, Backslash | South | left button |
| Cancel | F | East | right button |
| Run (held) | Shift | East | |
| Jump | Space | North | |
| Menu | Tab | West | |
| CameraLeft, CameraRight | Left, Right | L1, R1, right stick | sideways motion (setting) |
| Start, Select | Enter, Backspace | Start, Back | |
| L2, R2 | 1, 2 | LT, RT | |
| AttackWeak, AttackStrong, AttackFierce, AttackEnd | W, A, S, D | North, West, South, East | |

In a field, Select (where the game's shows nothing, while the player
walks), the right stick's click and the app shortcut Toggle first-person
view (V, Back+R1 and the touch deck's LOOK by default) also toggle
first-person inspection, which the arrow keys, the right
stick, a finger on the picture and the mouse camera's motion turn
([First-person inspection](#first-person-inspection)); Settings' Controls
section lists Select, R3 and what turns the view (with the finger's drag
where touch controls show; a headset's lists its underhand grab, the right
stick's click, the head and the right stick instead, the stick turning in
30-degree steps, [Headsets](#headsets)), and Shortcuts lists Toggle
first-person view with its bindings.

### The game's interface context

`xem_core::context` reads what the player's input reaches from the game's
memory (`context` command): `walk` (the player has control in a field),
`event` (with the map and the pad buttons its scripts test, from
`python3 -m tools.analysis.events --buttons`), `dialogue` (with its open `choice`: count and index), `menu`
(`menu_state_screen` and its cursors; `commands` while its commands take
the input, the arc's highlight shown over no command's screen, `open`
while a command's screen is open; the field menu runs inside the field
mode, and is read while it runs, `menu_state_debug_start` 0, not while it
loads nor while the field fades back in after it), `battle` (the command page, whether the attack inputs have the
buttons, AP left, an open list, a target being chosen for an item, art or
command; the attack page's own target choice is part of its attack
inputs), `world_map` and `arena`. `xem_core::menu` (`targets`)
lists the selectable entries the screen shows (320x224 screen pixels, as
the display shows the drawing area): the field menu's commands and a
command's choice window (the arc tables `menu_highlight_x/y_table`; the
labels sit about the tables' points), the menu's lists (the item and arts
screens' two columns of eight rows, the equipment screen's column, as
`menu_list_cursor_place` lays them out around the drawn list cursor), the
shops' commands and their stock and sell lists (one column of eight rows), Yes/No (their markers' quads), a dialogue's choice lines,
a battle's lists (arts, items, the combo setup, a Gear's commands, as
their cursor glyph is placed), a battle's targets (each
candidate's sprite projected as `battle_get_slot_screen_box` does, as tall
as the sprite's frame: a person's 96 units, a Gear's about 500, a big
enemy's more; where boxes overlap the smallest takes the point), the
command ring (its four slots and the page arrows above it, each glyph's
parts as the command panel's constant glyph tables and the battle's glyph
sheet place them; an arrow reaches well beyond itself, upward and away
from the ring, over the slots' labels where they meet; a slot the member
cannot use, drawn dim, takes no pointing) and the
attack page (its four attacks, each its button's glyph with the label and
digits right of it, which keep their places through the sequence; and
the attack's target among the candidates until the first attack fixes
it), with the
selected entry and how the D-pad steps it (a battle target is reached by
the moves `battle_find_candidate_in_direction` makes: nearest candidate
that way on the floor; a ring slot by its own direction).

On every host, `PlayerInput` turns these into presses the game reads as
any player's (held and released for whole game frames, counted as the host
reports the frames run):

- **Run by default** (`SettingChange::RunByDefault`, off initially) inverts
  the assigned run button during directional field movement: holding it
  walks. Keyboard, gamepad, touch and headset inputs share the preference;
  menus, dialogue, scripted events and battles retain their buttons.
- **Menu toggles the field menu**: pressed while it runs (any screen of
  it), it presses Cancel at the menu's commands, which closes it, after
  backing out of each command's screen it is in, one Cancel each, the next
  once that screen has gone back (30 frames; the item screen takes 21):
  the menu sounds every Cancel it reads, while it closes too, so one more
  would be heard. Other screens (shops) keep the button.
- **The wheel** steps a menu's or a choice's cursor (a notch toward the
  player is Down); elsewhere it pulls the field camera back or in.
- **Pointing** at an entry (the mouse over the picture,
  `xem_render::Picture` mapping target pixels to screen pixels) steers the
  game's cursor there with the D-pad; a click on an entry selects it and
  then confirms it once; a click elsewhere is the mouse's binding. An entry
  the cursor stops short of (past a list's end) is given up after three
  steps that leave the selection where it was (one on the battle's
  command ring: a slot the game refuses is not pressed again into a page
  flip), and not steered to again while the pointer stays on it. In
  battle, pointing at a ring slot highlights it and a click activates it
  (one the member cannot use takes neither, and the ring stays on its
  page); a click on a page arrow above the ring flips it to the other
  page's slot that way, highlighted (the left arrow to Defense or Escape,
  the right to Attack or the combo setup, the up arrow to the Up slot:
  the arrow's direction, pressed twice when another slot is highlighted),
  a click on an attack presses it (Weak, Strong, Fierce, the end of the
  sequence), and on the attack page a click on an enemy makes it the
  attack's target, selected only (the attacks confirm it; Confirm is one
  of them, and pointing there selects nothing). A sequence whose first
  attack a pointer pressed keeps its attack page shown until it ends,
  where the game would fade it after that attack (`PlayerInput::attack_pointed`,
  `Session::set_attack_page`: src/port/attack_page.c skips the panel's
  fade, `battle_command_panel_fade`, which only shades its quads; recorded
  with the inputs as the `attack_page` control, in session logs and
  replays too); one the player's buttons begin fades as the original's,
  its attacks still pointed at where it showed them. A click on a target that
  an item, art or command asks for chooses and confirms it. A touch's tap
  on the picture chooses the same way. While the game shows its pause
  screen (`Runtime::game_paused`, the player context's `paused`), a click
  or a tap anywhere on the game presses Start once, which resumes it,
  instead. With the
  mouse camera on, menus, choices, every battle command/attack page and
  the original pause screen release the cursor. Camera movement resumes
  only when play can take it again: the desktop takes the cursor back then,
  a browser (whose pointer lock needs a click) at the next click.

### Assisted movement

`xem_core::field` reads the field camera (`scaled_world`, the projection,
`field_view.angle`), the walkmesh (`FieldComponents`: per-layer
triangles with neighbours and attributes over SVECTOR vertices) and the
event actors. A screen point picks the floor the camera ray meets on the
player's layer (`pick`). Hosts point at their picture: when it shows the
scene from another viewpoint than the game camera's (wider views' zoom
out pulls the camera back along its axis), `GameView::viewpoint` gives
it (the view's rows and offset in the game camera's view space, its
projection distance and centre, in display pixels from the display's
corner), and `xem_assist::overlay::seen` sees the field's camera from it
(`Camera::seen`: the view composed with the camera, its depth sheared
into x and y where the picture's centre is not the screen's), so that a
pinch or a tap picks the floor and the actors the picture shows where it
points, and floor marks lie where it shows that floor (desktop, Android
and browser taps, OpenXR and WebXR pinches). A host that shows the
device's own picture instead (the Original renderer, a 24-bit movie frame
the view does not draw) says so (`GameView::away`), and the view gives no
viewpoint until it presents again: that picture is the game camera's,
also after first-person inspection, which the Original renderer ends.
First-person inspection's look has a viewpoint too, but the hosts pass
no field while it shows (nothing is approached from within the scene).
An agent's `navigate {screen}` is the game camera's screen. Tests:
xem-core's `a_camera_seen_from_another_view_projects_and_picks_as_its_picture_shows`
(pulled back about another centre, and turned within the scene), xem-render's
`a_pulled_back_or_inspecting_picture_has_the_viewpoint_it_shows_the_scene_from`
(a synthetic field at 250%: floor points show on the floor's colour where
the viewpoint places them and its rays meet them, where the game camera's
projection shows no floor; from within the scene, the floor and a wall
where the look's viewpoint places them; after `away` no viewpoint, and
the same one presented again), xem-headless's
`pulled_back_a_pinch_walks_to_the_floor_the_picture_shows_and_marks_it_there`
(field 30 at 250%: the camera 6 915 units back, a floor point shown 46
pixels from where the game camera puts it; a pinch there walks to it and
arrives, the cursor and the destination's mark lying there; pointed
through the game camera instead, the walk heads 125 units away) and
xem-host's
`taps_point_through_the_picture_presented_and_through_the_game_camera_under_the_original_renderer`
(the desktop and Android `View` presenting field 30 at 150%, the camera
2 305 units back: a tap where the picture shows a floor point 17.5 pixels
from the game camera's place walks there, its mark there; under the
Original renderer, after that picture and after first-person inspection,
the view gives no viewpoint and a tap walks to the floor the game's
camera shows, where through the last picture's viewpoint it would pick no
floor; presented again, the camera is pulled back again; at 250% the
middle of each of the 30 walkable triangles that picture shows lies
within a tap's reach, 18 pixels, of an actor, so a tap there talks).

Routes plan over the triangles the player may enter (an attribute
blocks by the actor's flags, bit 23 blocks layer 0, a
higher bit-22 ledge needs a jump), string-pulled a body's width off the
walls; a goal on mesh cut off by a gap within 300 units gets a takeoff at
the nearest edge. `xem_core::assist` plays a route as the pad: the D-pad's
eighth by the camera's angle (Right 0, Down 0x400, Left 0x800, Up 0xC00
less `field_view.angle`, measured on the game), held until the route turns
well away from it; Run beyond 96 units of route, walking within 40;
around another actor in the way; a step aside when the route stops
shrinking, and an end when it stays so. Nothing jumps for the player: at
a takeoff the assist waits (or presses on toward the landing unless the
ledge brake is on), holds its direction through the player's jump and
plans again after landing. `navigate {to: [x, z]} | {screen: [x, y]}`
walks to a floor point, `interact {actor}` approaches an actor (or the
nearest floor within its talk reach) and presses Confirm exactly once when
the player walks within the original's talk reach and facing, and
`assist_stop` ends it. A direction of the player's own replaces it; a
dialogue, menu, battle or map change interrupts it; `status` reports the
intent, its phase and the last outcome. Recorded sessions keep the
intents as resolved (`assist` controls) and replay to the same state;
nothing records pointer, gaze or headset timing. `field` lists the player
and the actors with their screen points.

Settings' Controls section lists every action and edits its inputs in
place (`SettingChange::BindGame {context, action, binding}`, and
`ResetGameBindings`; an input may hold one button in a context, so a swap
clears one side first); agents change them through the same
`SettingsService::apply` (`window.xem.settings.apply` in browsers,
`xem-desktop --control`'s `settings_apply` natively). `tests/controls.rs`
(xem-headless, local) checks that an agent's `act` and a player's bound key
reach the game as the same pad, and xem-host's tests that a gamepad pulled
out mid-press holds nothing, plays again when plugged back in, and that
bindings changed from the open panel hold when play resumes.

### Touch screens

On a touch screen (Android; phone and tablet browsers) the game's controls
are a Seraph Glass deck (`xem_assist::touch`) beside the picture when the
letterbox has room (three controls and their gaps, at least 40 dp each),
else over its corners: a D-pad on the left with Select above it (the
game's Select, and in a field first-person inspection's toggle where the
game's shows nothing), the face buttons on the right (Jump, Cancel and
Run, Confirm, Menu), the camera's turns at the top corners, Settings and
Start at the top. A finger holds the control it is on and slides across the
D-pad's directions; lifting or a cancelled touch lets go (explicit press
and release). A finger alone dragged on the picture turns inspection's
view, the scene following it. Battling names the face buttons and
turns as the bout does (A, B, Jump, Dash, Charge, Special), the world map
adds Board (L2 and R2 together: a Gear boarded or left), a field (walking,
its scenes and dialogue; not its menu) adds LOOK above the face buttons
(Toggle first-person view: [First-person
inspection](#first-person-inspection) on or off anywhere in the field,
lit while it shows, holding none of the game's buttons), its scenes and
dialogue AUTO beside it (auto-advance dialogue on or off, lit while on:
[Text speed, auto-advance and movie
skipping](#text-speed-auto-advance-and-movie-skipping)), the two each a
little narrower than the deck's other wide keys, and a shop's quantity
list adds +, -, ALL and NONE. A tap on the picture (short, not travelling)
is the pointer's tap (`xem_assist::dispatch::tap_at`; on the pause
screen, Start, as a click): walking, on an actor
it approaches and talks, on the floor it walks there; in a menu or choice
it selects the entry and confirms it; in battle it chooses as a click does
([The game's interface context](#the-games-interface-context)); in a
dialogue it advances. The deck
shows once a finger touches the game (always on Android) under Settings'
Touch controls. Android logs the deck's place (`touch deck ...`) and its
smoke test holds Confirm on it; the browser suite plays it with touch
pointers.

### Headsets: hands, gaze and control decks

`xem_assist::headset` is the shared headset core of the OpenXR loop and
the browser's WebXR sessions (plan.md, the hand/gaze contract):

- **Pointer.** The player's explicit choice (Settings: eye gaze, head,
  left or right hand ray; a hand's ray is the runtime's aim, a
  controller's or the hand's own: `XR_EXT_hand_interaction`'s aim pose,
  WebXR's target ray) aims at the virtual screen (`Screen`: game screen pixels, sides included) and the panel and
  keys beside it; while a diorama shows, at its interface's pane (the
  screen's pixels, no floor under them) and through its table into the
  game's world (`dispatch::Pointed`: `Screen`, `Pane` or `World`; [In a
  headset](#in-a-headset)). An unavailable source aims at nothing; nothing else
  stands in.
- **Gestures** (`xem_assist::gesture`). The standard ones are the
  runtime's, never imitated from joints: the pinch (OpenXR's
  `XR_EXT_hand_interaction` `pinch_ext`, begun at 0.9 and ended below
  0.6; WebXR's select of a hand source) and the thumb's taps and swipes
  (`XR_META_hand_tracking_microgestures`: forward up, backward down, left,
  right). A runtime without them has none (WebXR has no microgestures):
  its hands play as a pointer, a pinch clicking where it aims as a
  touch's tap does (walking to the floor, talking to an actor, choosing
  an entry, advancing dialogue) and a column of keys left of the screen
  pressing what the swipes would (the directions, OK, BACK, MENU, JUMP,
  the camera's turns). The plan's own gestures come from 25 room-space
  joints (WebXR's order, OpenXR's without the palm): five-finger grabs, the palm's region (down, up, sideways) classified as closing
  begins and kept through closure, firing on closure from an open hand
  (no hold or cooldown; reopening rearms; a fist found closed fires
  nothing; the other hand's simultaneous grab coalesces); whole-hand
  pinches (two of them scale the diorama while it is the view, about its
  table's anchor, the scale committed to the setting when either hand lets
  go, or else the screen for the session: [Scaling](#scaling); they own
  both hands until both open, the frame they let go on included); prayer
  hands (open aligned palms near-touching, once until they part). A
  closing hand emits no pinch or thumb event, nor does a pinch held
  through a grab or a scaling once it ends, and the runtime's pinch or
  thumb events during a scaling (a runtime may take gathered fingertips
  for a pinch) are the scaling's.
- **Resolution** (`xem_assist::dispatch`, table in its documentation):
  walking, a pinch walks to the floor under the pointer (`navigate`, if
  reachable), a tap approaches and talks to the actor under it
  (`interact`) or, on ground, stops the route; swipe up jumps, left and
  right turn the camera, down opens the menu and stops navigation. A
  dialogue's tap advances; a choice answers only under the gaze, with a
  fresh tap. Menus select the entry under the gaze on a tap (else confirm
  the selected one), swipe right backs out, up and down step the list
  (later entries up) or, beside the list on a character screen, change
  the character. The command ring: a swipe activates its direction's slot
  (selecting it first unless highlighted, whose direction would flip the
  page), a tap pages the ring (the game's own arrows above it show that
  it pages); the attack ring:
  up Weak, left Strong, down Fierce, right ends the sequence; lists
  take the entry under the gaze on a tap (else confirm the selected one)
  and back out to the right; targets take the one under
  the gaze on a tap (else confirm the selected one), move up, down and
  left with those swipes and back out to the right; beside the screen
  they also stand as a list (ALLY 1-3, FOE n; the selected one lit) for
  targets that overlap or hide one another. In a field's scenes and
  dialogue AUTO stands beside the screen (auto-advance dialogue on or off,
  lit while on); a tap or pinch aimed at it is that app toggle, never the
  dialogue's advance. Presses queued for one
  interface never reach the next (`PlayerInput`). While first-person
  inspection shows (`Headset::inspecting`), walking, the pinch, the tap and
  the swipes up, left and right do nothing (the swipe down still opens the
  menu), the pointer's keys keep only OK, BACK and MENU, and the hosts
  resolve gestures and draw marks without the field, so nothing walks,
  approaches or jumps.
- **Pauses.** The overhand grab toggles the player's Pause in every
  context (no game frame runs; head tracking, drawing, the panel and app
  input stay live); the underhand grab toggles first-person inspection in
  a field but its menu, where Ex Machina draws it (`look::grab_toggles`);
  prayer hands toggle Settings, which hold their own pause, and
  closing them keeps the player's. Losing the hands or the app's focus
  drops what was pending, stops assisted movement and pauses until the
  player's Pause (from an open hand, whatever the other hand does); a
  two-hand scaling under way ends where it reached, its hands both opening
  again before anything else. Two-hand scaling leaves the screen's and the diorama's
  size while inspection shows (the gesture still owns both hands, so no
  grab fires as they close; one under way as inspection began ends where
  it reached when the hands let go).
- **Control decks** (`xem_assist::arcade`) for the bespoke inputs,
  one-handed: Battling's bouts and the open world map (walking, Gears,
  vehicles). A pinch captures a stick at the hand, the hand's travel holds
  the D-pad's eighth (a 3 cm dead zone) and the next pinch lets it go,
  while the thumb plays (Battling: tap A, swipes B, Jump, Special and Dash
  held; the world map: tap OK, down the menu, left and right the camera,
  up the throttle held). Keys beside the screen, gazed at and tapped,
  press once or toggle a held button (lit while held); the world map's
  Board presses L2 and R2 together. The field's script-driven activities
  (an event on one of the 84 maps whose scripts test the pad: puzzles,
  button prompts, mashing) get a deck of exactly the buttons those
  scripts test (their symbols as the game's prompts name them), the tap
  the first face button, the swipes the directions they read and the
  stick the held ones and diagonals. Everything lets go when the context
  ends.
- **Drawing.** `xem_assist::overlay` and `xem_render::glass`: the live
  cursor (a hollow halo with fins and a gold centre; on the floor it lies
  as the floor projects, silver where walkable, a gold chevron where a
  jump is needed, broken and struck through where unreachable), the
  committed destination (gilded, related but distinct), the region a
  swipe up or down would scroll (corner brackets
  around the list; on a character screen only while the gaze is on the
  list), and keys beside the screen. Seraph Glass is drawn on the
  processor (faceted silver shells, halos, fins, dark recesses, gold
  accents, solid outlined symbols; state by shape and contrast; reduced
  transparency makes bodies dark and opaque; a key's label too long for
  it, such as the world map deck's THROTTLE, drawn smaller to stay on the
  key's glass, as `glass`'s tests check of the longest) and composited as
  room or screen quads; `cargo run -p xem-render --example glass` draws a
  review sheet. Settings and other application interface stay plain.

Agents never need any of this: they send the commands the gestures
resolve to.

**Qualification.** `xem-headless/tests/gestures.rs` plays the host's part
headless on the original game: synthetic hands (`xem_assist::synthetic`)
before a head whose gaze aims at the screen, the headset core, the
player's input and the command layer, no game frame while paused. It
walks where a pinch points (and holds the route under Pause), stops a
route with a tap on ground, talks to a villager and taps through 71
dialogue pages (a choice answered under the gaze), opens and leaves the
menu, buys with the quantity keys, stops and pauses on lost tracking,
plays both battle rings (on foot: pages, slots, AP spent by Weak and
Strong, the sequence ended; in Gears: the Gear's ring and a strike),
chooses a battle item under the gaze and battle targets (a swipe to the
nearest that way, a tap under the gaze, a key of the target list),
flies the world map by the captured stick and boards with L2+R2, plays a
Battling bout (A, Dash held, the stick, Pause) and jumps from a running
route (two frames from the thumb's microgesture to the air; the route
goes on after landing); its scripted hands stand in for a runtime with
the standard gestures (`synthetic::Runtime::HORIZON`: each step's pinch
and microgestures as the runtime would report them). As a browser's
hands (`Runtime::WEBXR`) it talks to a villager by a pinch on him,
pinches through the dialogue and opens and leaves the menu with the
pointer's MENU and BACK keys. `xem_assist::reliability` measures the
plan's own recognizers on the hand model under per-joint jitter (2 mm),
tremor (2 mm), varied speed, size (±10%) and turn (±20°) (`cargo run -p
xem-assist --example reliability`, 1000 tries each): grabs, prayer and
two-hand scaling spreading to twice the hands' distance and contracting to
half 100%, latency within a frame of the closing (or gathering) pose; 30
minutes of idle play (relaxed, curling, reaching, turning hands) fire
nothing; 200 pause grabs a twelfth of a second apart toggle 200 times; a
hand lost mid-grab and found closed toggles nothing; every one-hand
gesture works with either hand alone. The standard gestures' reliability
is the runtime's. The scripted gestures (`xem_assist::synthetic::gesture`,
which `--xr-hands` names) cover the contract's cases: `pause` and
`inspect` with either hand (`-left`), `pause-both` (both hands closing
in the same frames: one toggle), `grab-sideways` (neither), `fist-turn`
(a fist closed sideways turned palm down and palm up: nothing),
`fist-reacquire` (lost while closing, found closed: the loss, no grab),
`scale-out`, `scale-in` and `scale-pinch` (two whole-hand pinches
spreading from 15 to 30 cm and contracting back, the last with the
runtime's pinch reported from the gather to a frame after the release:
nothing but the scale); unit tests read each as the contract says in
front of a turned head, and the headset core's test finds a scaling with
the gaze on walkable floor walking nowhere, pausing nothing, the screen
held between 0.4 and 3 times its width and a factor after the scaling's
end changing nothing. The XR game smoke plays scripted gestures in OpenXR
(`--xr-hands`: Pause, prayer hands, a pinch on the panel's Close, a pinch
that walks, and every case above in its hands run: [Headset
qualification](#headset-qualification)) and logs the hand gestures the
runtime offers; the browser's
`tests/xr-hands.test.mjs` drives IWER's hands over the module fixture
(either hand's overhand grab pausing and resuming, both hands' together
once; either hand's underhand grab counted as inspection's, a sideways
grab, a fist turned while closed and a hand found closed toggling
nothing; the head moved and turned during the player's Pause and during
the settings' pause, the session drawing on, the views following it and
the game's frames standing still; prayer; lost tracking; a standard
pinch that counts once and the pointer it makes; two whole-hand pinches
(a gathered hand's pose) spreading from 30 to 60 cm and contracting back
with IWER's pinch held, the screen twice as wide and back, no pinch
counted, no grab as they let go into fists, the game running on and the
pointer's keys keeping their 0.22 m and staying beside the screen's edge)
and controllers (the trigger on the panel's Close, found where the
session says the panel stands). A browser's `status().xr.counts` counts
each kind of gesture recognised (pauses, inspections, settings, scalings
begun, pinches, taps, swipes, hands lost), `.keys` gives the keys beside
the screen (centre and width) and `.panel` where the panel stands; IWER
draws a hand's joints toward its pinch pose by the pinch's value, so the
test makes that pose the gathered hand's while it scales.

The game-level harness also covers every field map whose scripts read the
pad (84, from `events.py --buttons`): its event reports the map and those
buttons, and each key of its deck, gazed at and tapped, reaches the game
(`pad_port0_held`, after the game's own button assignment, every other map
under a swapped one) as exactly the button the scripts test; the stick
holds diagonals where they read directions. It interrupts an approach to
talk with the menu's swipe (the route stops, no Confirm reaches the menu,
closing it restarts nothing) and replaces one with a pinch, and with eye
gaze lost a pinch aims nowhere while the head or a hand's ray aims instead.

Sustained use is measured by its physical proxies (`cargo run -p
xem-assist --example reliability`): from a relaxed posture (wrists dropped
35 degrees and turned in 15, either hand alone) the grabs stay at 100%;
over an hour of tiring hands (tremor from 2 to 6 mm, poses held up to 8
frames, wrists dropping from 10 to 40 degrees, the hands taking turns
alone) every ten-minute stretch recognises all of them and fires nothing
during idle play; at 3 and 4 mm of per-joint jitter (noisier trackers)
grabs and prayer still hold at 100% with no idle activation. Nothing
needs a held pose, dwell or raised arm, and the decks keep hands low. A
device's own noise, its eye tracking, its standard pinch and
microgestures need that device, and people's comfort needs people.

### Headset qualification

What the headset contract asks of both headset hosts and what shows that
it holds, on emulated headsets: Monado's simulated HMD (null compositor,
lavapipe) with scripted hands for OpenXR (`xr-game-smoke.sh`, whose
`xrframe` lines and eye captures it checks), IWER's Quest 3 with its
tracked hands for WebXR (`web/tests/xr-hands.test.mjs` over the module
fixture, `xr-game.test.mjs` and, for the diorama, `xr-diorama.test.mjs` on
the game), and the shared code's tests (for the diorama
`xem-headless/tests/diorama.rs`: [Diorama](#diorama)). The rows from "The
diorama" on are its own: Graphics' headset view the diorama (plan.md
Phase 8 item 4 for that camera mode).

| The contract | OpenXR (Monado) | WebXR (IWER) | Shared code |
| --- | --- | --- | --- |
| Either hand's grabs | field 30: the left hand's Pause grab pauses and resumes, both hands' together toggle once, the right hand's resumes | each hand's grab alone, both hands' once (`status().xr.counts`) | `synthetic`'s scripts read as the contract says before a turned head; `reliability`: either hand alone, 100% |
| Grabs that toggle nothing | a sideways grab, a fist turned palm down and up while closed, a hand found closed (its loss pauses) | the same three, counted | the same scripts; 30 minutes of idle play fire nothing |
| Two-hand scaling owns both hands | in field 30 to 2.02 times, back to 1 and to 2.02 again (the runtime's pinch held through the last): only the scaling's gestures, no pause or walk, the game running on; a pinch after it walks; on the world map, beside its control deck, the deck's keys as many, as wide and as far beside the screen's edge at every scale (field 30's walk shows no keys) | to twice the width and back with IWER's pinch held: no pinch counted, no grab as the hands close, the game running on, the keys 0.22 m wide beside the screen's edge | `gesture`: the runtime's pinch and thumb during a scaling, its release frame included, are the scaling's; `headset`: no walk or pause, 0.4 to 3 times, ending at the scale reached |
| Paused head tracking | the player's Pause and Settings' own: a head pose of its own on most frames, the left eye changing (over 2% of it), one game frame (measured: 60 poses in 60 frames and 30 in 30, 17.5% and 16.6% of the eye) | the head moved and turned under both: the views and the viewer follow it, the game's frames stand still | |
| Settings panel and keys off the screen | the panel's middle (`panel=`) lower beside the scaled screen (measured: 0.869 m below the seat, 1.128 m at 2.02 times); the world map deck's keys (`keys=`) as many, as wide and as far beside the edge at every scale (measured: 8 keys 0.22 m wide, 0.04 m beside the edge, at 1.00 to 2.02 times) | the pointer's keys 0.22 m wide beside the scaled screen's edge (`status().xr.keys`); the panel's Close found where `status().xr.panel` says it stands | `draw`'s layout test (below) |
| Recentring the screen view | the app's Recenter puts the seat at the head; `monado-ctl -c` (`ReferenceSpaceChangePending` for LOCAL) puts it back at LOCAL's origin; the screen in front of both eyes after each | the player's Recenter with the head moved and turned away; IWER's `recenter()` and its space's `reset`; the screen in front after each | `a_moved_reference_cancels_gestures_lets_the_stick_go_ends_scaling_and_resets_the_seat` (`headset::reference_changed`) |
| A scene tour on the screen | the title movie (rebuilt), the title (field 490), field 30 and its menu, field 12's dialogue, the world map, battles on foot and in Gears: both eyes captured, the screen in front; the dialogue's and the Gears' RAM `xem-headless`'s | the same scenes from natively made states: 17 or 18 of 18 points about the views' centres unlike the room | |
| First person on the tour | the underhand grab looks from within (`fp=1`, `within=1`, the picture covering the eye) in the field and its dialogue only, the grab again giving the screen back; nothing on the title, in the menu, on the world map or in battles | the left hand's underhand grab: from within in the field and the dialogue only; a battle loaded after the field ends the view | `inspection_begins_only_over_the_fields_scene_and_its_eyes_begin_anew` |
| First person: transitions | in field 30 the underhand grab on and off, and again while the player's Pause holds the game (inspect, modern); the controllers' Back released alone on, off and on again, the right stick turning it a step a push, and an agent's Original renderer ending it in the headset frame it arrives in (controllers); from the diorama and back to it at its scale and place, a walk stopped (diorama-hands) | the grab and the controllers' click on and off, paused or not, the Original renderer ending it; on as the session ends, the canvas going on with it, and the next session looking from within at once; a battle loaded after the field ending it; from the diorama and back (`xr-diorama.test.mjs`) | `first_person.rs` in a headset: map 25's story movie begun while inspecting on the screen with no eyes for its 4 233 rebuilt frames, inspection on through it, the eyes from within again in map 25 after it, map 23's first frame ending the view; the Mountain Path's random battle entered while inspecting, the eyes and the pane (the compass alone) for the 48 blanks it loads behind the field's last frame, its shatter, the eye's picture broken up, ending inspection on the screen; `inspection.rs`: through the field's menu and a state loaded |
| First person: holes | | | `first_person.rs`: from the character's eye in fields 1, 12, 22 and 30 at eight headings and two pitches, flat and in both eyes, no pixel of the walkmesh's floors in view without the scene (under 0.5% asserted in each); the ceilings and skies the game never models show its clear, 0 to 1.6% of the flat pictures and 0.5 to 4.1% of the eyes', up to 23.5% of an eye looking back in field 30 (measured) |
| First person: the game's order, sprite planes and both eyes | the eyes differ and look from within, in the renderer's colours (inspect, modern) | | `first_person.rs`: in fields 1, 30 and 12 from sixteen looks, flat and in a headset, at every pixel where two primitives meet in one plane the one the game drew last (0.08 to 1.4 million a field, on lavapipe and on wgpu's GL backend, whose clip space WebXR's WebGL2 draws through; a sprite's parts lost it at up to 35 979 before the lift, and on GL at up to 3 456 before the fragments wrote their depth), faces drawn from both sides counted apart; sprites drawn reaching below their floor points standing on them; both eyes one billboard a sprite (within 0.02 units), its parts within 0.6 units of one plane; `ps1.rs`: a part drawn over another a hundred-thousandth of its depth nearer where it showed, a face drawn over a wall 5 200 to 6 800 units away showing at every pixel they share on both backends, a sprite on its floor point, one sprite for canted eyes |
| First person: interface and media | | (the pane's pixels not read) | `first_person.rs`: where the game draws its interface opaque after the scene, all of it as the flat frame shows it, on the flat look and on the headset's pane (4 514 to 27 712 pixels a field, field 12's dialogue the most), no opaque pane pixel unlike the flat frame; a story movie begun while inspecting on the screen (transitions); `inspection.rs`: field 30's compass alone on the pane (7% of it covered); `ps1.rs`: the pane covering by its alpha, the interface in neither eye |
| Pinches pick what is shown | a pinch with the gaze on the 2.02-times screen walks and arrives (where it heads is not logged); no run pulls the camera back, so the screen shows the game camera's picture | the diorama's pinch through the table heads for the floor point gazed at (the table's own ray: [In a headset](#in-a-headset)); no run pulls the camera back | `Camera::seen` through `GameView::viewpoint`: field 30 pulled back at 250%, a pinch walks to the floor shown; the desktop and Android `View` at 150%, a tap walks to the floor shown, and under the Original renderer (`GameView::away`) to the floor the game's camera shows ([Assisted movement](#assisted-movement)) |
| The diorama: transitions | in all thirteen runs with the diorama as the view the screen's share moves from a headset frame to the next by no more than their presentations' interval over the 0.3 s switch (`presented_us=`): from boot into fields, from the debug battle's selector into its battle, into the world map; field 30's menu, the Mountain Path's random battle and its end, and Lahan to the world map and a field from it, each from the diorama to the screen and back to a diorama (the battle's, the world map's, the field's), each way through frames between the two; map 25's story movie on the screen between its field's dioramas; from boot the title movie's rebuilt frames (210x142, 320x216's shape at the screen's span) and the title on the screen alone; the underhand grab takes the eyes from within at once and the grab again gives the diorama back on the next frame at the scale and table it had; an agent's `settings_apply` through `--control` in field 30: the screen alone from the first frame after it, at once, and the diorama eased in again from the screen over five frames (`xr_view=`, `diorama=`, `share=`) | the underhand grab over the diorama at 150%: from within on the first session frame after it, the diorama back alone on the first after the next (every session frame recorded), at 150%, its room from the world unchanged; field 30's menu through the automation: from the diorama to the screen over two session frames, the menu on the screen alone, back over three, each step within its time over 0.3 s; map 25's story movie from the state at game frame 5080: eased out, 1 959 of its 1 960 rebuilt frames on the screen alone, eased back over three; Graphics' headset view set through `xem.settings.apply` in the session: the screen alone from the next session frame, the diorama eased in again over three | `xem-headless/tests/diorama.rs`: the title movie (640x432 for 320x216) and the title on the screen; a field's diorama easing in, its menu out and back, a random battle's entry and end, Lahan to the world map and a field again, map 25's story movie out and back, each way over 17 blanks of 60 Hz; inspection giving the diorama back at once at its scale and place; the headset view set to the screen in a session, the screen at once, and to the diorama again, eased in; `ps1.rs`: a menu's and a 24-bit movie frame's screen after a diorama, eased (and inspection's through `present_diorama`, a path the hosts do not take); after inspection, the table easing on from where it stood |
| The diorama: paused head tracking | the left hand's Pause in field 30: 30 head poses in 30 frames, one game frame, 16.1% of the left eye changed between two captures, each pair in depth (disparity 10 to 19 pixels, far rows to near) | the left hand's Pause: the head moved 10 cm, the rows across the table shift by their depth (38 to 23 points, near to far), the game's clock still | |
| The diorama: either hand's grabs | in field 30 the left hand's Pause grab pauses and resumes, both hands' together toggle once, the right hand's resumes; a sideways grab, a fist turned while closed and a hand found closed toggle nothing (its loss pauses, the right hand's Pause resumes); the diorama shown in every frame of them | the left hand's grab pauses and resumes, both hands' toggle once, the right hand's resumes, a sideways grab and a turned fist count nothing; the diorama on | the screen's (above) |
| The diorama: inspection stops a walk | a pinch through the table walks, the underhand grab stops it (`stopped`) and none restarts to the run's end (the player's own play) | | `diorama.rs`: stopped, the character where it stopped 120 blanks on |
| The diorama: two-hand scaling owns both hands | 252% spread and 151% contracted, each committed once: only the scaling's gestures, no pause, press or walk, the camera's target still and its room point, the table's anchor, within 0.03 mm, a key beside the pane one width ([Scaling](#scaling)) | gathered hands 20 to 50 cm apart: 2.52 live, committed once, the anchor within a millimetre, no pause or pinch ([Scaling](#scaling)) | `headset`'s scaling tests ([Scaling](#scaling)) |
| The diorama: two-hand app controls | prayer hands show the settings panel beside the pane (its middle at (0.707, 0.442, -0.893), 22.4 degrees round from the pane's right edge as seen from the seat: `panel=`, `pane=`), the game held, the diorama on; a pinch on its Close (the walk run) or prayer hands again (the hands run) close it; in the four fallback runs the panel stands beside the pane after every frame that showed the diorama and below the screen after every one that showed the screen alone | prayer hands open settings beside the diorama's pane (`status().xr.panel`: its inner edge 9.93 degrees round from the pane's right edge, its lower edge on the pane's, facing the seat), its own pause holding the game; prayer again closes them; through the field's menu and map 25's movie the panel stands beside the pane in every session frame that shows the diorama and below the screen in every one that shows the screen alone | `draw`'s layout tests: the panel outside the volume, as wide seen from the seat as below the screen, beside the pane 4 degrees or more round from it and its keys, its middle 28.6 to 99.2 degrees round (past 90 only at the nearest, highest, most tilted table) |
| The diorama: recentring | the app's Recenter at 60 and 220 puts the seat at the head, `monado-ctl -c` (at 103) back at LOCAL's origin | the player's Recenter with the head turned 30 degrees: the table in front again; IWER's `recenter()` (`reset`): the seat at the origin, the first picture | `headset::reference_changed`, `GameView::recenter_diorama` ([In a headset](#in-a-headset)) |
| The diorama: sprite planes and both eyes | each capture's disparity grows from the far rows to the near, each eye showing its own diorama picture in the renderer's colours (0 levels off) | moving the head shifts the rows by depth; VR and AR sessions begun at one head place one table | `diorama.rs`: in three fields, the debug battle and the Mountain Path's on foot and the world map on foot, one build a game frame for both eyes, each eye of a pair as it is alone (0 bytes differ), the thickest stack of a sprite's parts 400 um off its billboard at most (the Mountain Path's battle; Lahan's and the debug battle's 300 um), Lahan's sprite rows alike in both eyes; `ps1.rs`: one upright sprite in both eyes, apart by its disparity only; split sprites in the game's order from five sides |
| The diorama: holes | | | `diorama.rs`: of the walkmesh's footprint (every actor layer's floors), seen from 16 sides and heights, 0.053% without the scene in Lahan and none in fields 12 and 30 (under the qualification's 0.5% bound, a step's criterion); 1.2% in field 22, beyond it (a strip of floor the party may not walk, beyond where the path's modelled ground ends); of the floors the controlled character may stand on, 0.021% in Lahan and none in the other three; of the ground the frame drew, from the same views, none of the debug battle's stage, 0.001% of the Mountain Path battle's and none of the world map's terrain at Lahan's exit |
| The diorama: masks and effects | the Mountain Path's random battle's entry and end on the screen between the field's diorama and the battle's; the debug battle's and the world map's dioramas ease in from the screen (the transitions row) | | `ps1.rs`: a fade tints the diorama and its pane, not the room; a subtracting interface quad takes its colour off the pane as off the screen; the pictures before the scene paint the wall in the game's order; `diorama.rs`: the random battle's entry and end shown on the screen between the dioramas; HD-2D effects left out, said by the Headset card (`xem-ui/tests/panel.rs`) |
| The diorama: interface and media | field 12's dialogue on the pane in each eye at its texels (all of 339 and 337 pixels within 2 levels); the title movie on the screen in its own shape | (the pane's pixels not read) | `diorama.rs`: the pane's opaque pixels the flat frame's exactly (field 12's dialogue, the debug battle, the world map's but for 21 pixels of its compass cone's edge, a 15-bit step off: cause not established), the interface drawn after the scene all on it in its colour, all of field 12's and the world map's interface; the movies' frames in their shape; `draw`: keys, HUD and panel outside the volume at a constant size |

The settings panel stands below the screen (`draw::panel` with
`Beside::Screen`, both hosts giving the screen's size as scaled and its
distance): its middle 1.33 m from the seat (as far as its old place, so
as large), facing the seat, its top edge's middle 4 degrees below the
screen's lower edge as seen from the seat; the larger the screen, the
lower the panel, its lower edge at most straight down. Its old place
(0.75 m down, 1.1 m ahead, tilted 25 degrees) covered the default screen's
lower edge, where the dialogue windows stand, and more of any screen
scaled larger. With the diorama as the view it stands beside the pane on
its right while the diorama shows (`Beside::Table`: [In a
headset](#in-a-headset)), and below the screen where the screen stands in
for it; it stood where the pane floats before, over the game's interface
while it showed.

Tests: the runs and tests in the table, and `draw`'s
`the_settings_panel_and_the_keys_stay_off_the_screen_at_every_scale_and_shape`,
which lays out three screens (3 m wide 2.5 m ahead, 2 m wide, 4 m wide
3 m ahead) at 0.4 to 3 times their width in four picture shapes (320x224,
398x224, 320x240, 320x216) from two seats, with OpenXR's panel (880x1120)
and the browser's (640x560) and 1 to 10 keys beside each edge: the panel
stands at least 3.9 degrees off the screen as seen from the seat (its
middle at most 75.1 degrees below level), and the keys keep their size,
stand beside the screen (at least 0.15 degrees off it) and more than a
degree off the panel; the old place overlaps the default screen.
`with_the_diorama_the_settings_panel_stands_beside_its_pane_and_keys_above_the_table_facing_the_seat`
lays out the diorama's pane for the same four picture shapes with 1 to 10
keys beside its right edge, from two seats, the table at the ends and the
middle of the settings' height (-1 to 0.3 m), distance (0.3 to 2 m) and
tilt (0 to 45 degrees), with both panels: the panel stands above the
table's volume, upright and facing the seat, its lower edge on the
pane's, all of it 4 degrees or more round from the pane and its keys
(4.22 degrees of view or more off the pane, 1.33 or more off a key),
its middle 28.6 to 99.2 degrees right of the seat's way (round about the
vertical through the seat), past 90 only with the table at its nearest,
highest and most tilted, from both seats with both panels.
`the_panel_covers_the_screen_only_where_its_lower_edge_stands_over_sixty_degrees_down`
finds where the room below the screen runs out, a screen 2.5 m ahead
growing in 0.1 degree steps of its lower edge: OpenXR's panel keeps its
gap (3.9 degrees) to a lower edge 56.5 degrees below level and covers it
from 60.6, the browser's to 65.5 and from 69.5; spread to 3 times, the
default 3 m screen keeps the gap and a 4 m screen of a 4:3 picture or a
4.5 m one of the field's 320x224 is covered. The
tour's pictures were read for this qualification: both eyes of each
OpenXR capture (the screen in front with the game's picture on it, the
field and its dialogue from within the scene) and IWER's
`build/web-test/xr-tour-*.png`. They showed the title screen (field 490)
turning inspection on, the first look from within replaying the last
field's eyes and the world map deck's THROTTLE label running off its key;
all three are fixed and tested ([First-person
inspection](#first-person-inspection), the glass's labels above).

Not done or not measured: comfort (no people tests). Monado's simulated
headset tracks no hands, so OpenXR's hand cases are scripted
(`--xr-hands`), and its `xrframe` line lists each frame's gestures rather
than counting them. While first-person inspection shows, the hosts pass
no field to the pointer (nothing is approached from within the scene), so
its viewpoint is tested but picks nothing. No headset run pulls the
camera back or points under the Original renderer: the headset hosts
hand the shared headset core the field through `GameView::viewpoint`
(and call `GameView::away` for the device's own picture) as the desktop
host does, which only the desktop host's and xem-headless's tests show
on the game. The OpenXR hands run logs that its pinch walks, not where
to. Below a screen whose lower edge stands more than 56.5 degrees below
the seat's level (65.5 for the browser's shorter panel) the panel's gap
narrows, its lower edge stopping straight down, and from 60.6 degrees
(69.5) the panel covers the screen's lower edge: once the screen's
height, as scaled, over twice its distance passes 1.78 (2.67 for the
browser). The settings' larger screens at their nearest reach that
(Graphics' headset width and distance run from 0.5 to 20 m), and so does
two-hand scaling at its largest, 3 times: a 4 m screen at the default
2.5 m with a 4:3 picture (61 degrees) or a 4.5 m one with the field's
320x224 (62 degrees); the default 3 m screen spread 3 times keeps its
gap (at most 53.5 degrees). With the diorama as the view the panel
stands beside the pane, 4 degrees round from its keys as seen from the
seat, which narrows to 1.33 degrees of view where a table set near and
above the head raises the pane towards the zenith ([In a
headset](#in-a-headset)). The diorama's rows: inspection and the diorama
hand the eyes over at once each way, without a crossfade, and so does the
headset view set to the screen in a session (set to the diorama, the view
eases in); the fallbacks to the screen are the ones eased. A battle's and
the world map's transitions ran on Monado and in `xem-headless`'s
harness, not in IWER (inspection's, the field menu's, a story movie's and
the headset view switched in the session there, the last on Monado too),
and no Gear battle nor the world map walking as a diorama in a headset
(map 414's Gear battle in the review camera's captures only). Holes
exceed the qualification's 0.5% bound (a step's criterion, not the
plan's) of the walkmesh's footprint in field 22 (1.2%:
[Diorama](#diorama)); holes, sprite planes and the pane's equality with
the flat frame are measured in the harness (the renderer a headset's
eyes are drawn by), not in a headset's captures, and IWER's checks read
no pane pixels; in a session log played in a headset (`--script`),
inspection stops no walk, the log keeping its own record, so OpenXR's
walk-stopping run is the player's own play from a state, and so is its
controllers run. The first-person rows: a story movie and a random battle
begun while inspecting ran in the harness alone (as a headset presents,
`present_headset`), not on Monado or IWER; Monado's controllers' buttons
and stick are scripted (`--xr-pad`, `--xr-stick`: its simulated
controllers press nothing and have no stick); the ceilings and skies the
game never models show its clear from within (measured, not made up:
[First-person inspection](#first-person-inspection)); holes, the game's
order in a plane and the pane are the harness's measures, not a headset's
captures.

## Crates

| Crate | Role |
| --- | --- |
| `xem-core` | headless authority: game instance, virtual clock, commands, introspection, stepping, snapshots, services |
| `xem-game` | the game module binding (wasm2c natively, imports in browsers) |
| `xem-gpu`, `xem-spu`, `xem-media`, `xem-disc` | the console devices as host services, and disc import |
| `xem-binaural` | the Binaural sound mode's renderer: HRTF placement of the SPU's voices for headphones, music arrangement rules (native and browser) |
| `xem-render` | the wgpu renderer for flat, stereo and XR views |
| `xem-ui` | the Slint settings panel as a custom platform rendered into a texture on the shared wgpu device |
| `xem-xr` | the OpenXR adapter |
| `xem-webxr` | the WebXR adapter: immersive sessions, views, raw input and WebGL2 layer submission through wgpu |
| `xem-desktop`, `xem-headless`, `xem-android`, `xem-web` | hosts |
| `xem-host` | what the SDL3 hosts share: the window and its native handles, the wgpu surface (dropped and recreated with the native window), the presented view (game display or test scene, and settings panel), the game paced at 60 frames a second with its sound (`Game`) |
| `xem-settings` | typed application settings: validation, acknowledgement, change notification, JSON persistence; the game bindings and the player's input (`player`) |
| `xem-assist` | assisted input: hand gestures, the headset core, touch and arcade decks, gesture resolution, overlay marks, synthetic hands and reliability measures |
| `xem-storage` | bounded reads and durable replacement of native settings, cards and state files |

## The test suite

`src/runtime/scripts/suite.sh [desktop|web|android|xr|full] [--force] [--base REF]`
runs the tests from anywhere in the repository, in the runtime shell. Before
starting Nix or replacing logs, it checks tracked changes (including staged and
unstaged edits) and non-ignored untracked files against the merge base of HEAD
and `origin/master`. Committed branch changes count too. If no runtime sources,
tests or build inputs changed, it reports a skip and exits successfully without
running tests. Documentation and standalone release tooling do not trigger the
runtime suite; use their focused checks. The boundary in `suite.sh` is conservative:
any runtime input change enables the selected tier, rather than trying to maintain
a dependency map for every test. Missing Git history is an error, never a silent
skip. The check does not fetch or cache test results.

Use `--base REF` to select another comparison point, such as `--base HEAD^` for
the most recent committed update on master. Use `--force` deliberately to bypass
change detection, including when private/ignored game inputs, drivers or the test
environment changed. It also works without a comparison ref. A skip leaves
`build/suite/` intact and does not establish a new passing result. Help and flag
parsing need no Nix shell. While developing, run only the platform whose host is
changing, and `full` once at the end before review or a merge only when runtime
inputs changed (AGENTS.md): `desktop` (the default, for the
native and shared code) builds the game module and runs the port's unit
tests, the Rust workspace's tests and the validation run; `web` the web
build, the browser suites and the browser tests that measure time
(WebXR's among them); `android` the APK with both its libraries, the
emulator's smoke test, the smoke's game again with the APK's arm64-v8a
library (which the emulator runs through its ARM translation) and the
game's arm64 build under qemu (`arm64-smoke.sh`); `xr`
the native build and the OpenXR smoke test. `full` runs them all and what
CI runs (`.github/workflows/ci.yml`) as on a clone without the user's discs:
`tools/repository/check.py`, which builds and tests the workspace (here
without a game module), and the matching toolchain's smoke test
(`make -C src/decomp smoke`). In `full`, the builds run side by side, each
in a cargo target directory of its own (`src/runtime/target-*`, none of
them play.sh's): the native one, then the Rust tests and the validation run
at once; CI's checks; the web build, then the browser suites (the slice's, the
longest, from the start beside the others two files at a time; the slice's
and the persistence tests two at a time within them: at most five
browsers); the Android APK, and the emulator boots (its boot measures
nothing); the arm64 build plays the slice's logs under qemu (measuring
nothing either). What measures frame rates then runs alone, one after another,
since their emulator, headset and browsers render on the
processor (beside the browser suites the Android smoke's frame rate fell
from 56 to 10 a second, the XR smoke's headset frames came late, and their
checks failed): the Android smoke (with the APK's x86_64 library, then its
game with the arm64-v8a one), the XR smoke, and the browser tests that
measure time (the headset's frame rate, rewind frame times, fast-forward's
music, the stalled page, a resume read within the rewind's one-second
interval, the Binaural mode's cost). Rust builds use the `suite` profile
(release's optimization without link-time optimization or debug
information: after a change to xem-gpu the test binaries rebuild in 37 s,
against release's 161 s); a failing test target is built again with line
tables (`suite-debug`) and run with a backtrace into `rust-debug.log`. A
module build that changes nothing keeps `build/game/game.wasm` and its time,
so nothing downstream rebuilds (play.sh compares the sources with
`build/game/stamp`, which each finished module build touches). The
desktop tier's Rust tests include the original game's long run
(`xem-headless/tests/long_run.rs`, 36 000 frames twice: 138 s of the
tier's 866 s in a run here beside other tracks' work), the diorama's
qualification on the game (`xem-headless/tests/diorama.rs`: 73 s in a
desktop tier of 946 s, its Rust stage 817 s) and `xem-host`'s original-game tests (rewind frame latency, the
focus pause's recording, the held rewind), which skip without the disc as
the others do. The Android smoke compares states with `xem-headless`: the
android stage builds it (the suite profile, the native target directory:
nothing to do after the native stage) and gives it to the smoke
(`XEM_HEADLESS`). The Android
smoke, run by itself, boots its emulator while the APK builds
(`XEM_SMOKE_BOOT=1` only boots it, and a later run uses it), and turns off
the system's "isn't responding" dialogs on its AVD (on a loaded host one
took its taps). Logs, and each stage's result and time: `build/suite/`.
Measured here (16 threads, warm builds): `full` 3762 s with every stage
passing (2026-10-17, at the end of Phases 6-8; 723 s before their tests
joined it), its longest stages the Rust tests (1492 s), the browser
suites (1078 s), the XR smoke (994 s), the arm64 build under qemu (754 s)
and the browser tests that measure time (717 s); `desktop` 804 to 955 s
in that week's runs beside other tracks' work, its Rust tests 784 to
889 s (114 s before Phases 6-8); `android` 824 s with an up-to-date APK
(2026-10-15: the APK's build, 220 s; the arm64 stage, 350 s, beside the
emulator's boot, 25 s; then the emulator's smoke, 243 s, and the smoke's
game with the arm64-v8a library, 211 s, which boots the emulator again),
and earlier 902 s after a change to the translation's compiler flags (the
APK's build of both libraries, 149 s, and the arm64 stage, 417 s, each
compiling the translation again).

## Saves and time controls

The app Settings screen has Audio, Graphics, Controls, Shortcuts, Time, Saves,
Gameplay and About sections ([Desktop host and settings panel](#desktop-host-and-settings-panel)).
Desktop widths use a sidebar; narrow windows use a strip of section tabs.
Saves' Backups card puts its buttons two to a row on a panel at least 600
logical pixels wide (a desktop's 640), one to a row on narrower ones (a
phone's, a headset's 440), so that every label fits its card. Rewind
(with Hold to rewind), the fast-forward toggle's button and the notched FF rate
slider live in Time, with compact interval and history fields and the retained
queue's available seconds and memory usage. Graphics contains
Show FPS, which draws a counter over the game on desktop, Android, browser and
headsets; it can also be toggled through a keyboard or gamepad binding.
The same HUD shows icons during active fast-forward and successful held rewind,
including with Show FPS off. Paused play and exhausted rewind history hide their
indicators. Save-state saves/loads and state backup imports/exports show a result
for four seconds; failures show the cause for eight seconds. Ordinary memory-card
saves do not produce HUD notifications. Every HUD element has one treatment
(`FrameRateOverlay`): a dark chip 32 pixels high with the same border and
corners, a plain 16-pixel icon coloured by what it says (a success's check
green, a failure's mark red) and one weight of text; the counter, then the
activity icons, in a row, a result below them, and a failure's chip grown
downward to give its cause. Settings contains save-state, rewind, backup
and binding controls, and windowed play their file pickers. Every host
also takes these operations' keyboard and gamepad chords
(`xem_settings::AppInput`, which takes a chord's buttons before the game
has the rest). In a headset the controllers are that gamepad, OpenXR's
(`xr_play`) and a WebXR session's alike: Back is the right stick's
click and LT and RT the grips, so by default Back with A saves the
state, with Y loads it, with the left grip rewinds while held and with
the right grip toggles fast-forward, and both sticks' clicks toggle
Settings. The operations a headset frame asks for (the panel's, a
chord's) run in that frame, and a session refuses the file pickers and
downloads (the backups' imports and exports, Import BIOS, the browser's
Import disc…) with "File pickers are available in windowed play.", as
neither can show them. Fast-forward is an on/off toggle, bound to
backtick (`Backquote`) or Back+RT by default. Settings → Time's Start
fast-forward button (Stop fast-forward while it runs, beside a line
saying which) is the same action for touch screens and headsets without
a keyboard or a gamepad: not a setting, so nothing about it persists
and there is no checkbox. Its separate
saved rate defaults to 10×. The slider snaps to 1×, 5×, 10×, 15×, 20× and
Uncapped; changing the rate does not turn fast-forward on. Uncapped has no
game-frame multiplier or frame-count ceiling: each host runs as much game work
as its responsive update budget permits. The original music driver and SPU
continue on a normal-time clock, including when the host cannot run every
requested game frame in one batch. Interactive fast-forward uses elapsed host
time for music at both fixed and uncapped rates. A fixed rate the host cannot
sustain adapts to the rate its batches reach (`Session::pace_fast_forward`,
measured every half second by `xem_core::time::FastForwardPacer` and raised a
step at a time while the host keeps up). Fixed-rate headless and scripted
stepping keeps the deterministic game clock divided by its rate unless
explicit music-clock commands attach another clock. Recordings include FF
rate changes, toggles, the initial music-clock attachment and elapsed music
time, so replay uses the same timing on another host. Speed changes preserve
queued PCM; native and browser output accumulate 50 ms before starting or
resuming playback. Presentation remains owned by the host.

Normal-tempo music is an explicit player requirement. The original sound driver
also gates gameplay: scenes waiting on its progress run at normal speed during
FF. Consequently FF play can differ from 1× play with the same per-frame pads.
This is an intentional exception to the simulation/pacing independence in
plan.md (host scheduling, Phase 2 authority, and Phase 6 timing semantics).
Fixed-rate FF remains reproducible for the same timed inputs. Uncapped play's
host clock is an additional input and must be included in a recording for
reproducible replay. CD/movie audio retains only the latest 100 ms when input
outruns playback; stale cutscene audio cannot accumulate behind the music.

Settings → Time has **Pause when window loses focus**, enabled by default.
Focus loss releases held inputs and requests the original game's pause screen.
The game pauses at its next legal Start check in field play, battles, the world
map or the arena; original script locks and transitions still defer pausing.
Title screens, app menus and movies receive no synthetic Start input. Focus is
sampled before each game frame (also when retrying a disc-blocked frame), so
refocusing before the legal check cancels the request. Refocusing after the game
pauses leaves it paused until the player resumes with the normal Enter/Start
binding or a click or tap on the game. The pause screen keeps running its original input and display logic;
rewind snapshots and history time remain frozen. Pause flags are read by the
port itself, so this also works when the browser cannot load `schema.json`.
The focus-loss preference is
bindable to keyboard/gamepad, persists with Settings and is included in settings
backups. Pending focus requests are outside game snapshots; input recordings
include their requests and cancellations so replay reaches the same game pause.

Rewind runs while R (`KeyR`) or Back+LT is held and resumes on release
(also Settings → Time's Hold to rewind, held: a headset's settings pause
holds the game while the panel shows, but not a held rewind, whose HUD
icon shows over the panel too). In a headset the panel's pointer is pressed
where a pinch or a controller's select (its trigger) begins and let go
where it pressed when that pinch or select ends, moving nowhere meanwhile
(no pinch-and-drag scrolling, plan.md Phase 5; `xem_assist::headset::PanelPointer`,
OpenXR's hands and WebXR's hands and controllers; OpenXR's controllers
press with their trigger's ray), so a pinch held on Hold to rewind rewinds
with hands alone. However the panel closes, and when the
window or a headset session loses focus, it lets go of what its pointer
held (`SettingsUi::cancel_pointer`): a Hold to rewind the pointer still
holds ends then, not on a release the hidden panel never sees.
Every 250 ms (`REWIND_STEP_MS`) a held rewind takes the newest state before
the game's frame off the queue and plays on from it until the display shows
a frame both built and drawn after it (its third change of buffers: the
first shows one drawn before the state, the second one drawn from what the
game had built before it, whose projections the port recorded then; at
most 12 vertical blanks, never as far as where the step began), with those
frames' sound at the master volume (under fast-forward the music, on the
host's clock, plays them too). Its picture is the game's at that moment as
the renderer draws it (wider, in high resolution), not the restored VRAM;
where the game draws nothing new (a transition holding its last picture)
the restored picture shows. History
is enabled by Settings for interactive play. Headless sessions, scripted native
playback and browser command-driven boots have history off unless requested;
browser automation can boot with `rewind: true` or use `rewind_configure`.
The queue stores the same serialized game-state content as save files. File
checksums are generated for export and checked on import, outside the periodic
checkpoint; restoring a queued state still validates its identity and devices.
Settings controls the seconds between states (default 1)
and the history length (default 30 seconds). Both use real elapsed play time,
independent of FF speed; paused time is excluded (a headset's pause, the
player's or Settings', in OpenXR and WebXR alike). The interval cannot exceed
the history length. The original game's pause screens stop snapshots and freeze
the queue's elapsed-play time, preserving history for when play resumes. Pausing
restarts the snapshot interval on resume. Increasing and decreasing either value
can be bound to keyboard/gamepad chords.
The Settings checkbox **Invert LT/RT chords** makes LT alone
rewind and RT alone toggle fast-forward; Back+LT and Back+RT then send L2 and R2
to the game, without also sending Select. This preference is saved, included
in settings backups, and can itself be bound to a keyboard or gamepad chord.
App bindings remain available while Settings is open; editing a settings text field
captures keyboard input.
Rewind queues the same serialized game saves used by save states and steps
back through them. Loading or rewinding keeps app commands, recording controls,
the FF toggle, FF rate and app settings as currently set. A recording goes on
across a load or a rewind step and `stop_recording` lists them (`restores`:
each a `state_load` or a `rewind_step`, with the vblank it left and the one
it restored). The game's vblank count is restored with its state, so the
log's later entries belong to another timeline: `replay` refuses a log whose
vblanks go back, naming the entry and the vblanks, and one that lists a
restore (a load forward in time keeps them in order), rather than
interleave two timelines.

Ordinary saves still use the original game save menu and its two memory
cards. Settings' Import original card 1/2 takes any memory card or save file
the runtime reads, and Export original card 1/2 writes the exact 128 KiB raw
`.mcd` ([Original saves and memory cards](#original-saves-and-memory-cards)).
An import replaces the inserted card between frames; the browser writes it
to IndexedDB first, with play paused until that completes. Import BIOS (save
titles) gives the file screens' titles the font of the player's BIOS image;
without one they use substitute glyphs ([Save titles and the Kanji
font](#save-titles-and-the-kanji-font)). State backups use
`.xemstate`; settings have a separate JSON backup. Native writes acknowledge
after writing, syncing and replacing the file. Document providers on Android
acknowledge after write, flush and close; their own replacement guarantees
apply.

`Session::snapshot_bytes` contains only the runtime's game state: game memory,
globals, suspended fibers, event queues, clock phase, card protocol, GPU, SPU and media
decoder/drive progress, with the number of the disc in the drive. The state
identifies the exact game module, the game (both discs alike, [Discs](#discs))
and ordered package digests. Checksums, versions, identity, buffer bounds,
device data and the drive's disc being at hand are validated before committing
the restore, and a refused state changes nothing. A refusal names what
differs: a size outside 32 bytes to 64 MiB, the checksum, the magic, the
format version found and the one expected (`bad snapshot: format version 8,
expected 9`), each identity part that differs with the state's value and the
session's (`incompatible state: the state's game module is "…", this
session's is "…"`, likewise the disc and the packages), an identity field
the format has not got, a device's version found and expected, truncated or
trailing bytes, or the drive's disc not being at hand. Hosts show that text
as it is (the HUD's failure chip, the panel's error bar; `Game::command`
returns it plain). Host disc/storage attachments and presentation settings
and current memory-card images stay attached (the drive takes the state's
disc from them); queued future audio is discarded. App pacing is omitted
from the state and reapplied after restoring the game's music phase. The
game state format is version 9 (`snapshot::VERSION`); a state of another
version is refused, never converted. `snapshot::FORMAT_FINGERPRINT` beside
it is the SHA-256 of three stand-in states
(`format_fingerprint_matches_version`) that between them hold each part of
the format and each variant it writes: every kind of fiber (the game's
fresh after a restart and suspended, the arena task's beside it), drive
state (reading, seeking, idle with the lid open), drive event (a response,
a sector, a seek's end, the story's disc change waiting, the lid's closing)
and GPU receiver (a partial command, an image load, a polyline), a VRAM
read, queued sectors and a sector buffer, decoded XA audio, a macroblock's
output in flight, sound transfers and CD audio for the SPU, open card
files, a listing and a check, the dispatcher's mode, a dispatch write and
the module's globals. A change to what a state holds or to how any of these
is written fails that test until the version changes with it.

Card device protocol state is saved, but persistent card contents are
excluded. Loading, importing and rewinding never roll back or write ordinary
card saves; card backups are separate explicit operations. The protocol
state is the calls under way: the open files, a directory search and a
pending check. Each open file carries what its calls depend on as the device
last left it (recorded when it opens, kept in step as the device's own calls
change the card): the directory frame of its first block and a digest of
its bytes, all of them for a file open only to read, those before its
position for one open to write (what it has written; it writes the rest).
The search carries the digest of the directory it lists. Restored over the
inserted cards, a file whose frame or bytes read otherwise there is closed:
its calls fail as on a removed card, and the game takes its own retry and
failure path (the save command's failed-save notice, 0x35; the load's, 0x3e)
instead of writing the rest of an old save over what the block holds now (a
newer save that reused it, a later save's temporary file, another card's
file) or reading the rest of a load from a save written over it (the same
save made again, whose directory frame reads the same). A search over a
directory that differs ends, and a pending check reports the card inserted
now. Over the same card a state goes on as it would have, also after a
rewind inside a save: the bytes it wrote are still there, and it writes the
rest again.

Rewind expires states beyond the configured history length and discards the
future after stepping back. Retained allocations also stay within 2 GiB
(`MAX_REWIND_BYTES`, the budget whenever history is on); Settings reports
the actual seconds available and
memory used. The real-time queue's landing points depend on elapsed play time;
they are player conveniences, not deterministic replay checkpoints. An oversized
checkpoint reports an error without stopping play.
Loading a state starts a new rewind history with that state; state backups
contain game progress rather than the app or the retained queue of states.

The shared control commands are available through `xem-headless --control` and
`window.xem.command`. For example:

```json
{"cmd":"fast_forward_configure","rate":{"Fixed":10}}
{"cmd":"fast_forward"}
{"cmd":"fast_forward","enabled":false}
{"cmd":"rewind_configure","max_bytes":2147483648,"interval_seconds":1,"history_seconds":30}
{"cmd":"rewind","steps":1}
{"cmd":"rewind_step","steps":4}
{"cmd":"reset"}
```

`rewind {steps}` restores the state `steps` back at once, the game waiting
there; `rewind_step {steps}` (default 1) is a held rewind's, as the hosts
hold one: each step takes the newest state before the game's frame and
plays on from it (`Session::rewind_step`, a host's every 250 ms while held),
and the reply gives the steps taken (`steps`, fewer than asked once the
history's start is reached) with the `status`. It is how a headless agent
holds rewind; neither can be scheduled, and a recording lists their
restores.

`reset` abandons the current game stack and boots the game as the console
does after its reset line: the executable is copied to its address again
(its code and initialized data as the disc holds them), the port's BIOS
returns to power-on (`xem_run` with `XEM_RESTART_POWER_ON`: no event open,
no interrupt callback set, the pad driver stopped; the game's own soft reset
keeps them, as the original's does), and the original boot routine runs. It
releases held pads, restarts the root counters, clears rewind history and
retains attached discs, memory cards and app preferences. The sound chip and
the drive return to their power-on state as the console's reset line returns
them (no voice sounding, the SPU's registers and RAM cleared, its unplayed
output dropped; no read or stream under way, the disc still in), and the
Binaural presentation forgets the sound so far, so nothing of the abandoned
game outlives it (its sound driver's tick event would otherwise go on beside
the new one's, the music at twice its tempo): the boot sounds as it does
after power-on.

Native file commands are `state_save`/`state_load` with `path`,
`card_import`/`card_export` with `path` and `slot` (0 or 1), and
`bios_import` with `path`, which gives the running game a BIOS image's Kanji
font as Import BIOS does ([Save titles and the Kanji
font](#save-titles-and-the-kanji-font)). `card_import`
reads any memory card or save file as Import original card 1/2 does
([Original saves and memory cards](#original-saves-and-memory-cards)) and
replies with what
it found (`message`) and the card's listing
(`card`); `card_export` writes the raw card. The shared, read-only
`card_list {slot}` reports a card on every host with a command channel
(not Android's, below), the browser's `window.xem.command` too, for agents
and tests (no panel shows it): its
listing (`card`, none without a card: `formatted`, `free_blocks`, `problems`,
`notes` and `saves`, each with `name`, `first_block`, `blocks`, `title`,
`kind` and `problems`), whether the card is in (`present`) and why it cannot
change now (`busy`). The recorded `card_present {slot, present}` takes a
card out or puts it back ([Cards in and out](#cards-in-and-out)); `status`
reports both cards' (`card_present`), where the Kanji font came from
(`kanji_font`: `bios` or `substitute`) and what the native hosts' storage
line shows (`storage`: `pending_writes`, the card writes the storage has
not yet taken; `errors`, the cards' storage errors; `kanji_font_notice`,
why an imported BIOS image is unused, or null). The browser's page writes
the cards to IndexedDB and checks an imported BIOS image itself, so its
session's `storage` always reads no pending writes, no errors and no
notice; browser agents read its storage line's results from
`window.xem.status().page`: `cards` (`pending`, `error`, `notice`) and
`biosNotice`. File commands
are accepted only as explicit native host requests; scheduled logs accept
game input, setup and pacing commands and cannot access files or list cards.
Browsers use explicit Settings operations or
`window.xem.appOperation({Action: 'ExportState'})` and the
snapshot/card/settings automation methods. Browser file pickers require a
recognized activation; a refusal is shown in Settings and operation status.
Native agents reach the panel's and the bindings' operations the same way:
`xem-desktop --control` and the headset's (`--xr --control`) take
`app {request}`, an `xem_settings::AppRequest` as JSON, and carry it out as
the panel's and the bindings' are (`Operations::execute`; a failure in the
panel's error bar, a state's outcome on the HUD), answering `{ok, message}`
or `{error}`. `{"Rewind": true}` holds rewind as R held does
(the window's and the headset's frames then step it, through a headset's
Settings pause as well) until `{"Rewind": false}`; `{"Action":
"RetryStorage"}` retries the pending card writes; `{"Action": "Recenter"}`
recentres a headset's view (the window refuses it: "Recentering places a
headset's view; play in a headset to use it."); the rest are the panel's
actions. The file pickers are refused
("ExportState opens a file picker, which only a player answers: agents name
the file"): agents use the file commands above. So is Toggle first-person
view: agents turn the view with `first_person`, which answers the view it
leaves ([First-person inspection](#first-person-inspection)). Android has
no command channel (the agent surface it has: [Android host](#android-host)).
Headset play saves and loads through the same session as the window:
OpenXR's Save state keeps `state.xemstate` where the window's does (beside
the settings; on Android in the files directory, which the Android host
gives the headset loop as `XrPlay::state_directory`), and agents reach
these commands there too (`xem-desktop --xr --control`); a WebXR session
keeps its state in IndexedDB's `saves` store, and the game's own card
writes reach IndexedDB after every immersive frame, as after the window's
([Browser host](#browser-host)). No Lua runtime exists yet (plan.md, Phase
11), so these state paths cover the game, event, audio and media services.

Tests: `xem-core/tests/persistence.rs`, over a stand-in module and the real
clock and devices (no local inputs):
`every_damaged_or_foreign_state_is_refused_by_name_and_changes_nothing` (an
empty file, 31 bytes, one byte over the cap, a flipped byte, a truncated
file, garbage sealed and not, another magic, versions 8 and 10, another game
module, disc or package list, an extra identity field, a trailing byte, a
truncated last device and a drive's disc the session has not got: each
refused with its message through `Runtime::restore` and
`Session::restore_bytes`, the live state and the session's timeline
unchanged; given the disc, the state restores and goes on as where it was
saved); `random_damage_never_panics_or_half_restores` (200 seeded mutations
of a state's content, resealed: a bit flipped, a word overwritten, inserted
or deleted, anywhere or where the structure is dense; 98 refused with
nothing changed, 102 accepted whole, each saving as given or restoring as
itself; an unoptimised build runs the first 20, about six seconds here);
`format_fingerprint_matches_version` (the three stand-in states, each also
restoring as itself; measured here, it fails when the way a drive's disc
change, the lid's closing, a seek's end, a macroblock's output in flight or
its order, a fresh fiber, XA audio's channels, a polyline or a card file is
written changes and the version does not);
`rewinding_into_a_save_never_changes_another_file_and_the_same_card_goes_on_identically`
(the stand-in saving as the game does, a card call a frame: rewound into its
writes after a newer save took the block, its remaining writes and its
rename are refused and the newer save and its stored copy stay; over the
card as the state left it, the save and the state end as the first time);
`a_recording_reports_the_restores_it_spans_and_a_replay_refuses_them`. The
card device's tests: a restored save writes on over the same card and never
over another file, a restored save writes on where its bytes are (a rewind
inside it makes the save it would have made) and never over a later save's
temporary file in its block, a restored load reads on over the same save
and never from the same save made again over it, a file the device changed
while it was open (the game's failure path leaves one open) restores over
the same card, and a restored listing and check follow the inserted card;
the drive's: a state waiting for a disc takes it where the session holds
it, as when the host adds it. `web/tests/persistence.test.mjs`: a state
another game module build saved (the fixture with one custom section more)
is refused with the module message, the running state and the stored state
unchanged, besides the storage failures above. `xem-host`'s
`a_refused_state_load_reports_the_runtimes_message_as_it_is` (the game
module, nothing booted): a state naming another build, loaded through
`Game::command` and through the panel's Load state, is refused with the
runtime's text, unquoted. `xem-ui/tests/hud.rs` shows a refusal in the
failure chip. `xem-headless/tests/states.rs` (local: the user's discs; its
five tests side by side 27 s in the desktop tier here and 54 s beside
other builds, the held rewind's alone, rendering on lavapipe, 39 s) takes
the original game's states where each kind of progress is
under way and checks that they go on identically (`continues_identically`:
a state taken, frames run keeping each frame's RAM/scratchpad hash, the
SPU's samples, the data sectors the drive delivers and the XA sectors it
plays, the state restored, which drops the run's sound still queued for a
host and whose own snapshot is then the state byte for byte, the same
frames run again: the same hashes, samples and sectors and the same state
at the end, byte for byte): the new game's opening movie with its XA sound
(600 frames from frames 4000 and 9000 of `new-game-sound-mode-stereo.json`,
then 1200 frames at 10× fast-forward, whose 2 s of sound are the divided
music clock's; a state holds the music clock at 1×, and the restore puts
the host's 10× back on it and on the SPU), the world map while the party
walks and the drive streams its terrain (600 frames from the end of
`world-map-lahan-exit.json`), a Battling bout whose state holds the arena's
task fiber (150 frames; the bout launched by writes, as `gestures.rs`
launches it) and the story's five-second disc change wait (restored in the
session that took the state and in a fresh two-disc session, disc 2 goes
in at the frame and cycle where the session that took it puts it in
without a restore, and the RAM 300 frames on is the same). A held rewind
from the forest battle of `slice-forest.json` (frame 6400) steps back into its field (ten steps, to
frame 5826), each step showing a frame the game drew after its state as the
renderer draws it (`GameView`) but the one landing in the battle's opening
while it loads (frame 5953), which draws nothing new, and the field goes on
identically from there. `web/tests/persistence.test.mjs`'s "original-game
states and raw card files interchange between native and browser": native
states inside the opening movie (frame 600) and on the world map (the end
of `world-map-lahan-exit.json`, at its recorded hash) import into the
browser exactly; 30 frames there give a state that loads natively to the
browser's RAM hash, and the same 30 frames natively end byte for byte in
the browser's state. Every host's save states and time controls:
`xem-ui/tests/panel.rs`'s
`time_toggles_fast_forward_with_a_button_where_the_headset_and_android_smokes_press_it`
(Start fast-forward sends Toggle fast-forward and changes no setting, the
session's state turns it to Stop fast-forward, and the points the headset
and Android smokes press, at their panels' sizes and with a running
game's status texts, reach Saves, Time, Save state, Load state, the
fast-forward button and a held Hold to rewind, the headset smoke's let
go by `cancel_pointer`, as a closing panel lets it go; at the WebXR
test's 640 by 560, its Time, Stop fast-forward and Hold to rewind);
`xem-assist`'s
`a_pinch_or_select_held_on_the_panel_presses_where_it_began_and_lets_go_there_moving_nowhere_between`
(the headsets' panel pointer: pressed where a pinch or a select begins,
nothing sent while it holds as the pointer moves or leaves, let go where
it pressed when it ends; one that begins and ends in a frame clicks;
another source taking the pointer lets it go);
`src/runtime/scripts/xr-game-smoke.sh`'s saves run (OpenXR on Monado:
the panel's Save state and Load state by pinches, Start fast-forward,
the Back+RT and Back+LT chords from scripted buttons, an agent's
`--control` command), held run (Back+LT and the panel's Hold to
rewind under a scripted controller ray's trigger, both through Settings'
pause, and Settings closed mid-hold letting go of it) and held-hands run
(hands alone: a scripted pinch held on Hold to rewind for 40 headset
frames holds it from the pinch's start to its end, the gaze moving 125
pixels down off the button meanwhile, and rewinds through Settings'
pause; an agent's `app` Recenter; [Headsets](#headsets));
`web/tests/xr-game.test.mjs`'s
saves test (WebXR on IWER: the game's own save to card 2 in IndexedDB
during the session, Save state and Load state by a controller's ray,
Back+RT, Stop fast-forward, Back+LT through Settings' pause under the
HUD's rewind icon; Hold to rewind held by the right controller's select,
then with IWER's tracked hands alone by a held pinch, the head turning
off the button meanwhile without moving the pointer and the rewind going
on after it, each going back at least 120 game frames through Settings'
pause and Settings holding the game again after; a picker refused);
`android-smoke.sh`'s
Settings by touch on the emulator (Stop and Start fast-forward, Hold to
rewind, Save state and Load state; [Android host](#android-host)).
Agents: `xem-core/tests/persistence.rs`'s
`an_agents_held_rewind_steps_as_a_hosts_does_and_stops_at_the_oldest_state`
(`rewind_step`: refused without history, two steps played on as
`Session::rewind_step`'s, then the three left of ten asked for, none at
the oldest, none scheduled) and
`status_shows_the_storage_line_pending_writes_card_errors_and_an_unused_bios`
(the stand-in's save into a storage that refuses it: one write pending and
the error; an unused BIOS image's notice; a retry with the storage working
stores the card); `xem-host`'s
`an_agents_app_requests_run_as_the_panels_and_the_bindings_do_but_open_no_picker`
(the game module, nothing booted: `{"Rewind": true}` and `false` hold and
let go as a binding does, Retry pending card writes and Toggle fast-forward
answer as the panel's, a picker, Recenter in the window and a request that
is none refused); `xem-desktop/tests/control.rs`'s
`agents_hold_rewind_retry_card_writes_and_read_the_storage_line_through_the_desktops_app_command`
(the window under weston: a card file that is no card and a kept BIOS
image that is no image in `status`'s `storage`; in field 30 an agent's held
rewind goes back at least 120 frames until it lets go, then the game plays
on; the retry clears the errors; Export state backup and Recenter refused).

Not done or not measured: the scripted hands' pinch and IWER's stand in
for a runtime's (Monado tracks no hands; only a scripted ray and trigger
hold Hold to rewind under an OpenXR controller, Monado's simulated
controllers pressing nothing and pointing where no test chooses); an
agent's held rewind and a binding's are one hold (either's release ends
it); xem-headless has no storage retry (`card.retry_writes` is the hosts'
Retry pending card writes);
the Android host's headset loop with its files directory as the state
directory has not run (the emulator has no OpenXR runtime); fast-forward
in a headset reaches what the host's batches reach, not the setting's
10× (a 20 Hz Monado loop here 3.5 to 9.6× as the machine's other load
varied, IWER's software WebGL 1.4 to 1.7×); the Android emulator draws
frames with the panel over the game
at about two a second, so its Hold to rewind went back only 6 and 9
vblanks in two runs' two-second holds (its history then holds states a
few frames apart). An open file's later blocks' directory frames are
not compared (the bytes it depends on there are; the game's saves are one
block); a writer whose bytes so far match a later save's (the same save
made again) writes on, making the save it would have made; states taken
inside a save or a load are tested with the stand-in and the card device's
bench, not the original game's save menu; the tests reach the arena only
by a debug launch (the bout's task and parameters written).
`states.rs` covers 600 frames of that bout, including particle effects;
the build's RAM-alias translation fixes the display-list failure formerly
reached at frame 725. This still does not cover a bout the story starts; held
rewind is tested from a battle into its field, not inside a movie or on the
world map; the drive's tally of XA sectors played (`Cd::xa_sectors`, the
host's, for verification) starts afresh at a restore; sealed but crafted
values for the entry, the stack pointer, fiber areas, the clock and the
pads are accepted and fail later in the bounds-checked module; the
fingerprint sees only what
the stand-in states hold, so a variant added to the format is outside it
until a stand-in state holds one, and device semantics changed without a
layout change pass it.

### State interchange

A state is one byte format on every host: `XEMSNAP\0`, the format version
and the identity record (JSON: `module`, `disc`, `packages`), then
fixed-width little-endian words and length-prefixed records, the module's
memory regions with runs of 16 or more zero bytes compressed, and each
device's own versioned record, sealed by the SHA-256 of all of it. It
holds no native pointers or host paths: the game module's memory is
wasm32 linear memory, addressed alike by the native wasm2c translation
on x86_64 and arm64 (desktop, OpenXR, Android) and by browsers, and the drive's discs are
named by number. A state made on one host loads on another when each of
these holds, and is refused otherwise with the message given, the running
game unchanged:

| Condition | What the restoring session needs | Refused with |
|---|---|---|
| Size | 32 bytes to 64 MiB | `bad snapshot: state size N bytes, outside 32 bytes to 64 MiB` |
| Seal | the SHA-256 the file ends with | `bad snapshot: checksum` |
| Format | `XEMSNAP\0` and format version 9 (`snapshot::VERSION`; never converted) | `bad snapshot: magic`; `bad snapshot: format version 8, expected 9` |
| Identity record | exactly `module`, `disc` and `packages` | `bad snapshot: identity: unknown field …` |
| Game module | the same `game.wasm` (its SHA-256: `XEM_MODULE_ID` natively, the fetched file's in browsers); any change to `src/port`, `tools/game_module.py` or `src/decomp` makes another | `incompatible state: the state's game module is "…", this session's is "…"` |
| Game | Xenogears (USA), either disc, any Redump-verified image (CHD, cue and bin, ISO): `SLUS-00664+SLUS-00669` | `incompatible state: the state's disc is "…", this session's is "…"` |
| Packages | the same ordered package digests (none exist yet) | `incompatible state: the state's packages are […], this session's are […]` |
| The drive's disc | the disc in the state's drive at hand (booted, or beside the boot disc) | `snapshot device state: the state has disc 2 in the drive, which is not here` |
| Devices | each device's record and version | `snapshot device state: bad snapshot: SPU device version 3, expected 4`, `snapshot device state: not a GPU device snapshot`, … |
| Length | nothing after the last device | `bad snapshot: trailing bytes` |

Identity differences are joined by `; `. Hosts show the message as it is:
the HUD's failure chip and the panel's error bar, `xem-desktop
--load-state FILE: …` on the command line and the Android log's `xem: app
Action(LoadState) … error: …`. The browser's own storage refuses before
the runtime does (`No stored state in this slot. It may have been
cleared or evicted; import a backup.`, `Invalid stored state. Import a
backup.`, `Stored state checksum failed. Import a backup.`, `State file
exceeds 64 MiB.`), and native reads refuse a file over the cap (`file
exceeds size limit`).

**What a state does not carry**, the restoring host's own from its next
frame: the game-affecting controls (the view the field's characters keep
moving in, which wider framing, the camera pulled back, first-person
inspection and the diorama set; the sound mode, which Binaural runs in
Stereo; the attack page; the focus pause; random battles off and half
encounters; text speed, auto-advance and skippable movies; fast-forward,
its rate and pace, and the music clock), rewind history and its settings,
and the attachments: the memory cards' contents, the discs beside the
boot disc and the Kanji font (a BIOS image's, else the substitute glyphs:
continuations differ where save titles are drawn). The same continuation
needs the same of these; a recording carries the controls
(`xem-host/tests/states.rs`'s headset test).

**Whole states.** Where no presentation bookkeeping is in play (headless,
or a host asking for no reveals) a state another host saves at a frame is
byte for byte the state `xem-headless` saves there. A host showing faces
the game culls (wider framing, the camera pulled back, inspection, the
diorama) keeps bookkeeping in the port's memory ([Presentation and the
whole state](#the-renderer-phase-4)), so its states differ from
`xem-headless`'s there while the game's RAM, scratchpad, devices and
continuation are the same. The tests compare the RAM and scratchpad
hash's continuation (`memory_hash`, the authority) on every pair and whole
states where they hold:

| From | To | Checked | Test |
|---|---|---|---|
| headless (state file, a fresh session) | headless | 36 000 frames (216 000 once): every 1 800 frames byte for byte, with rewinds of 1 to 4 steps and 10× fast-forward | `xem-headless/tests/long_run.rs` |
| headless | the desktop's window in process (`Game`, `View`, HD-2D, lavapipe) | 600 frames: the original's RAM and whole state | `xem-host/tests/states.rs` |
| headless | a headset's presentation in process (both eyes within the scene, then the diorama) | 600 frames: the RAM of a plain session given the recorded view; the whole state outside the port's memory | `xem-host/tests/states.rs` |
| headless | `xem-desktop` (weston's headless backend, lavapipe) | `--load-state`; frames 300 and 600 under HD-2D | `xem-desktop/tests/states.rs` |
| `xem-desktop` | headless | `--save-state-at`: byte for byte, then 300 frames | `xem-desktop/tests/states.rs` |
| headless | `xem-desktop --xr` (OpenXR on Monado) | frames 300 and 600 under HD-2D | `xr-game-smoke.sh` (interchange) |
| `xem-desktop --xr` | headless | byte for byte, then 300 frames | `xr-game-smoke.sh` (interchange) |
| headless | Android (x86_64 emulator) | Load state (F8) of `files/state.xemstate`, then 600 frames | `android-smoke.sh` |
| Android | headless | Save state (F5), pulled: byte for byte, then 600 frames | `android-smoke.sh` |
| headless | browser (WebGL2) | imported exactly; 30 frames byte for byte (the opening movie, the world map) | `web/tests/persistence.test.mjs` |
| browser (WebGL2) | headless | to the browser's RAM | `web/tests/persistence.test.mjs` |
| browser (WebGPU, WebGL2), after drawing under HD-2D, wider and pulled back | headless | to the page's RAM, then 240 frames | `web/tests/render.test.mjs` |
| browser (WebXR on IWER) | headless | byte for byte, then 120 frames | `web/tests/xr-game.test.mjs` |
| browser (IndexedDB), the page reloaded | browser | the slice's save to its native hash | `web/tests/slice.test.mjs` |
| the slice's session logs | headless on arm64 (`aarch64-linux-android` under `qemu-aarch64`) | the forest route, the save and that card's loads as card 1 and as card 2 to their recorded hashes, each ending in the native end state byte for byte | `arm64-smoke.sh` |
| headless | headless on arm64 | the forest route's state in its battle (frame 7000) to the route's end state | `arm64-smoke.sh` |
| headless on arm64 | headless | that state, byte for byte the native one, to the route's end state | `arm64-smoke.sh` |
| Lahan and the world map drawn pulled back and wider (`reveal_frames`) | headless on arm64 | 600 and 300 frames: the same culled faces revealed (their number and digest; the world map's clouds among them), the RAM and the whole state as natively | `arm64-smoke.sh` |
| headless | Android's arm64-v8a library (the x86_64 emulator's ARM translation) | Load state (F8) of `files/state.xemstate`, then 600 frames | `android-smoke.sh` (`XEM_SMOKE_ABI=arm64-v8a`) |
| Android's arm64-v8a library | headless | Save state (F5), pulled: byte for byte, then 600 frames | `android-smoke.sh` (`XEM_SMOKE_ABI=arm64-v8a`) |
| a session booted from disc 2 | a session booted from disc 1 | the same hash | `xem-headless/tests/discs.rs` |
| the game's own save in a browser | native | card 2 byte for byte | `web/tests/slice.test.mjs` |
| the game's own save on arm64 | native | card 2 byte for byte | `arm64-smoke.sh` |

**Cards and settings across devices.** A memory card file is the raw
128 KiB image on every host (Export original card 1/2 writes it, and every
host's Import reads it and the other formats: [Original saves and memory
cards](#original-saves-and-memory-cards)), and the game's own save writes
the same bytes to it in a browser and on arm64 as natively (the saves
above; no test compares the cards that the desktop window's, OpenXR's or
Android's in-game saves write). A settings backup is
`{"version": 1, "settings": …}` JSON on every host: another version or an
unknown envelope field is refused (`incompatible settings backup
version`, serde's message); a value the importing build cannot read (out
of its range, a variant it does not offer) leaves that setting at its
default and a name it does not know is dropped, the rest kept, without
the import saying which (`Settings::read`'s list of them goes unused on
import and when the settings file opens).

Tests: `xem-headless/tests/long_run.rs` (local: disc 1): from Lahan 300
frames after entering it, a reference and a run carried across sessions
play the same input, a pure function of the frame number (a seeded
xorshift's choice of the d-pad, Cross and Circle held 1 to 30 frames), and
10× fast-forward over 600 frames around every third checkpoint; the
carried run saves its state to a file at every checkpoint (every 1 800
frames) and goes on in a freshly booted session that loads it, and every
third interval turns rewind on (a fake clock, a state each 60 frames, a 24
MiB budget, 30 seconds of history) at the start of a quiet spell and steps
back 1 to 4 times at its end (a rewind step plays on from a state with the
state's own buttons, so nothing is held then), then plays on; at every
checkpoint the two are the same byte for byte, and the history stays
within its budget and length. 36 000 frames each (the default;
`XEM_LONG_RUN_FRAMES` for more) took 122.6 s here alone and 138 s in the
desktop tier (20 checkpoints and sessions, 7 rewinds); 216 000 frames (an hour of game time)
took 616 s alone in its process (120 checkpoints and sessions, 40 rewinds,
24 000 fast-forwarded frames; the walk ended in field map 8 at RAM
`30d46e72c5adf322`). `xem-host/tests/states.rs` (disc 1, lavapipe): Lahan's
state restored in a host's `Game` and played 600 frames paced by the wall
clock as the desktop's frame plays them (168 to 185 host frames under
HD-2D at 960×540 in four runs here) ends at the original's RAM and whole state; as a
headset's presentation plays them (both eyes from within the scene, then
the diorama; 255 to 259 host frames) at a plain session's replaying the
host's recording (the view `1000 | 1024 << 16`), whose whole state differs
from the host's only in the port's bookkeeping, and a plain session
without the view ends elsewhere; 17 s for both. `xem-desktop/tests/states.rs`
(disc 1, weston, lavapipe): field 30's state at frame 710 loaded by
`--load-state` reaches `e6366cb7f5d61d8c` at frame 1010, where
`--save-state-at` writes a state byte for byte a plain session's, and
`a7cb468cae1e226e` at 1310, as natively, and the window presented frames
meanwhile; 11 s. `xr-game-smoke.sh`'s interchange run reaches the same
hashes from the same state, its state at 1010 byte for byte
`xem-headless`'s; `android-smoke.sh`'s state step plays field 30's state
at frame 1910 on the emulator to `24a65171efb8d70b` at 2510 as natively,
and the state it saves there (2 681 637 bytes) is the native one byte for
byte and goes on natively to `b88b373fd2986cc2` 600 frames later, as the
native state's own does; `web/tests/render.test.mjs` has both backends' exports load
natively to `ea9b510132f1ff8d` and go on to `3ccc7cd1bb8bcdda` as in the
page; `xr-game.test.mjs`'s headset export is the native state byte for
byte; `slice.test.mjs`'s card 2 is the native save's byte for byte.
`src/runtime/scripts/arm64-smoke.sh` (disc 1, in `#android`) builds
`xem-headless` for `aarch64-linux-android` with cargo-ndk at API 29, the
APK's target, linked statically against bionic, and runs it under the
shell's `qemu-aarch64` (QEMU 11.1.0): the forest route (12 205 frames), the
save to an unformatted card 2 (11 235) and that card's loads as card 1
(5 252) and as card 2 (5 468) end at their recorded hashes there and
natively, each in the same state on both byte for byte, and the save's
card 2 is the native one byte for byte; the forest route's state in its
battle at frame 7000 is the same on both byte for byte, and each goes on
in a fresh session on the other to the route's end state. Lahan (field 1)
drawn for 600 frames and the world map for 300 as a 16:9 framing with the
camera pulled back by 3 000 depth units would draw them (`xem-headless
--control`'s `reveal_frames`, reaching 85 by 32 pixels beyond the screen:
the draw trace asks the port for the faces the game culls, which it
judges on the pulled-back camera's screen in floating point, the game
module's only floating point) reveal the same culled faces on both,
Lahan's 276 276 among 393 894 primitives and the world map's 554 585
among 673 614 (the same FNV-1a digests of their shapes and screen
points), to the same RAM and whole state byte for byte. The port judges
Lahan's culled scenery by `xem_present_face_visible` alone, the world
map's terrain by it too and the world map's cloud quads by
`xem_present_points_visible`, which nothing else calls
(`src/port/worldmap_present.c`): gdb's breakpoints on the two in the
native build counted 182 084 calls of the first and none of the second in
100 such frames of Lahan, and 791 112 and 49 158 in the world map's 300.
Those two functions round each product, sum and quotient on its own, as
wasm's do, in every build here (`-ffp-contract=off`: [The game
module](#the-game-module); `llvm-objdump` lists no fused multiply-add in
them in the static build or in the APK's `libmain.so`, nor `objdump` in
GCC's x86_64 build). It took 258 s in the
android tier (the reveals 30 s for Lahan and 40 s for the world map under
qemu, 3 s each natively), with the runs under qemu side by side at 67 to
80 frames a second (native `xem-headless` 435 to 656). With
`XEM_SMOKE_ABI=arm64-v8a`, `android-smoke.sh` installs the APK's
arm64-v8a library (`adb install --abi arm64-v8a`), which the emulator's
x86_64 image runs through its ARM translation
(`ro.dalvik.vm.native.bridge=libndk_translation.so`; the start checks that
the package's ABI is arm64-v8a and that the translation's arm64 linker,
`/system/bin/arm64/linker64`, and arm64 system libraries are mapped in the
app's process), with Vulkan through the translation's `libvulkan.so`
proxy to SwiftShader. In the smoke's game section the game
boots from disc 1 and plays (46 to 52 game frames and 31 to 43 drawn
frames a second over its first 25 s in the android tier, where the x86_64
library's were 54 to 60 and 38 to 42; 24 to 45 and 18 to 40 in a run
here beside other work), the background and return, fast-forward's Home
rounds, Hold to rewind and Save and Load state by touch pass as with the
x86_64 library, `xem-headless`'s field 30 state loaded by F8 plays to
`24a65171efb8d70b` at frame 2510 as natively, and the state F5 saves
there is `xem-headless`'s byte for byte (2 681 637 bytes) and goes on
natively to `b88b373fd2986cc2`, as the native state's own: 210 and 223 s
in two runs of the android tier, the emulator's boot among them. The
refusals in the table above are `xem-core/tests/persistence.rs`'s (a
stand-in game, no local inputs),
word for word: each damaged or foreign state, an older SPU device record,
another GPU record and the drive's disc.

Not done or not measured: no arm64 processor runs anything here: arm64 is
emulated, `xem-headless` (linked statically) by qemu's user-mode emulator
and the APK's arm64-v8a library by the Android emulator's ARM translation
(Google's libndk_translation on x86_64), and the translated app runs only
the smoke's game section (the synthetic disc, rotation, activity
recreation and restart run with the x86_64 library) under the default
graphics, which ask for no reveals, so the APK's floating point has run
only as the static build's, compiled to the same instructions; no Linux
arm64 build (`aarch64-unknown-linux-gnu`, GCC and glibc, as an arm64
desktop or OpenXR host would be built) of `xem-headless` or of the
desktop and OpenXR hosts has been made or run: every arm64 program built
here is the NDK clang's, for Android, and what `-ffp-contract=off`
guards against in GCC's code has been checked only by compiling the part
of the translation that holds both floating-point functions
(`game_15.c`) with the pinned nixpkgs' cross GCC and reading its
disassembly; Horizon OS's
OpenXR and the Android host's headset loop have not run (Monado stands in
for OpenXR; the emulator has no OpenXR runtime); Windows and macOS hosts
are not built here; states are compared between hosts with the same host
controls and attachments (a host whose wider framing, sound mode or BIOS
font differs goes on differently by design, which only the headset test
shows, for the view); whole states taken while a host reveals culled faces
are compared outside the port's memory and two frames on, not byte for
byte; the long run walks Lahan and wherever its input leads (it reached
field map 8 in the hour), not the story's other scenes; only the slice's
save is compared byte for byte as a card across hosts (the browser's and
arm64's; the Android emulator's card is reloaded on the device only by
`android-smoke.sh` with `XEM_SMOKE_SLICE=1`, which no suite stage passes:
manual); a settings
import does not report the settings it left at their defaults or the
names it dropped, and no test moves a settings backup between hosts.

### Original saves and memory cards

Settings → Saves → Backups' Import original card 1 and 2 (app actions on
every host, bindable like the others) take any file below and tell what it
is by its contents (`xem_core::devices::card_file::detect`; a raw single
save by its file name): a whole card becomes the card, a single save a
freshly formatted card holding only that save, written as the BIOS creates
and writes a file (blocks 1, 2, ... chained: the device's own create, which
the game's saves go through too). Either replaces the inserted card. Its
message says what was found: the file's container and the card's saves
("Card 1: DexDrive card, 3 saves", "Card 2: PS3 save (.psv) on a fresh card,
1 save"), then the first problem of this game's saves, if one has any (a
load the game refuses, below). Export original card 1 and 2 write the card
as it is, the raw 128 KiB `.mcd`. The native command `card_import` and the
browser automation's `cards.importFile(slot, bytes, name)` do what the
pickers do; `card_list` lists a card for agents and tests ([Saves and time
controls](#saves-and-time-controls)).

The files are told apart by size and magic; the layouts are psx-spx's
("Memory Card Images") and, for the PSP's and the PS3's, those of the tools
that write them. Each is imported only; the one export is the raw card.

| File | Extensions | Layout |
|---|---|---|
| Raw card | .mcd .mcr .mc .srm .mem .bin .vm1 .ddf .ps | 131 072 bytes beginning "MC" |
| DexDrive card | .gme | a 0xF40-byte header beginning "123-456-STD", then the card |
| Connectix VGS card | .vgs .mem | a 64-byte header beginning "VgsM", then the card |
| PSP or PS Vita card | .vmp | a 0x80-byte header beginning 00 "PMV", then the card |
| Single save | .mcs .ps1 | its first block's 0x80-byte directory frame, then its blocks |
| PS3 single save | .psv | a 0x84-byte header beginning 00 "VSP" (type 1, a PS1 save, at 0x3C; its name at 0x64), then its blocks |
| Action Replay, GameShark, Xploder, Caetla save | .psx .mcb .mcx .pda | a 54-byte header (its name at 0, a title at 0x15), then its blocks |
| Raw single save | its directory name, e.g. BASLUS-006640 | its blocks |

A single save is 1-15 whole 8 KiB blocks beginning with its title frame
("SC"). A raw one is named by its file, whose name must be a directory name
as PlayStation games make them (a region BA, BE or BI, a game code such as
SLUS-00664, up to eight characters more). The PSP's and the PS3's files
carry a signature over their contents, which is not checked. The browser's
picker offers every extension above (a raw single save, which has none,
under its "all files" choice); Android's document picker names a raw single
save where the provider's document id carries the file's path (the
external-storage provider's does). An import reads at most 0xF40 + 131 072
bytes, a DexDrive card's, the largest container; the browser refuses a
larger file before reading it.

An import is refused, naming why, for a file no container above holds (an
unformatted image among them: a raw card begins "MC"), a PS3 save of another
type, a raw save not named by a directory name, and a card whose directory
is damaged where its files depend on it: an in-use directory frame whose
checksum is wrong, a chain of blocks that breaks, loops or runs past the
card, a block two files claim. Oddities that leave the files whole are
notes of the listing instead: the header frame's checksum, a free frame's,
a frame neither free nor in use (its block is not used), a block in use by
no file. Other games' saves, and the Japanese game's, are card contents like
any other, listed as such.

For this game's saves the listing adds what the game itself checks
(`src/decomp/src/slot39/menu_framework.c`). Its own files are those whose
names begin with the twelve characters "BASLUS-00664"
(`menu_card_mark_own_saves`; on both discs), each the file its digit names
(at block offset 0x123; files 1-15, digits 0-14, `menu_save_choose_digit`).
Its load takes a save only when its payload (block offsets 0x100-0x1FFE)
sums to the byte at 0x1FFF (`READ_SAVE`; otherwise "Load failed"). After a
load from the title it asks for disc vars\[82\] + 1 (`menu_main`; vars\[82\]
at block offset 0x12D8, written by `menu_save_build_payload`). The listing
gives each such save's file and the disc it asks for (`kind`) and each of
these checks it fails (`problems`). The Japanese game's saves are told by
the name prefix "BISLPS-01160", which is inferred: the North American program keeps
"BASLUS-01160" as a second prefix that no code reads (`menu_load_resources`),
the Japanese first disc's serial under the American region code; no Japanese
disc or save was at hand to confirm it.

An import changes a card between frames (`Card::change`), never under a
call of the game's: it is refused, with the reason, while the game's file
screen shows (below), the game has a file open on that card (a save or a
load under way: the game's own saves write a sector a frame), is checking
it, or the cards are still busy with an access (`Card::busy_with`;
`card_list` reports it as `busy`). Natively the
file `cardN.mcd` takes the card before the device does, so a refused write
changes nothing. The browser pauses play, lands any pending card writes, has
the runtime read the file (`read_card_file`, refused while the card is busy
as above), writes the card to IndexedDB and only then gives it to the
session (`set_card_image`): a failed IndexedDB write leaves both as they
were. Were the card to become busy while IndexedDB writes (frames that
automation runs meanwhile), the session refuses it and IndexedDB takes back
the session's card. Natively, an import into a slot whose card file did not
load (a file of another size, a read error: the slot is left empty and the
file unchanged) replaces that file. The game reads its cards only in its
menus' file screens (the save, copy and delete screens of a field's and the
world map's menu and of the disc change, and the title's Load: slot39's
`menu_file_screen_run`), which list them when they open and then act by name
on what they listed, so an Overwrite or a Delete chosen after an import
would reach the save of that name on the imported card. While one shows,
from the frame it begins to read the cards (before it lists them) until its
command has returned, an import into either card is refused ("memory card 1
not changed: the game's file screen shows the cards' saves and would act on
the ones it listed; leave it, then try again"), on every host and through
`card_import` alike, and the next import after the player leaves it goes
through; the screen opened again lists the card anew. Whether one shows is
read from the game's memory when each frame ends, a state is restored or
the console resets (`xem_core::context::file_screen`, while a menu runs:
the card screen's polling, `MenuCard.mode`, which
`menu_file_screen_enter_card_mode` sets where it forgets the last listing
and `menu_top_command_run` clears once the screen has returned, or the
screen's card mode, `MenuFlags.card_mode`, set once it has listed the cards
and held until it ends), kept by the card device for the changes between
frames (`Card::file_screen`, which no state holds). On the load slice's
inputs to the title's Load, an import is taken up to frame 2787 and refused
from 2788, where the screen begins to read the cards, nine frames before
its card mode (2797): the card mode alone left those frames open, and an
import taken in them left the screen listing the card it replaced.

Tests: `xem-core`'s `card_file` tests make every container from a card or a
save built here (this game's save with its title, digit, disc and checksum
as the game writes them; another game's two-block save), no original content:
`every_card_container_reads_back_the_card` (raw, DexDrive, VGS, PSP with a
junk signature, and the report),
`every_single_save_container_becomes_a_fresh_card_holding_only_that_save`
(`.mcs` and `.ps1`, `.psv`, the four 54-byte extensions, raw saves named by
their file: each the card the BIOS makes for that save alone, its blocks as
the file holds them, and the report),
`files_that_are_not_cards_or_saves_are_refused_by_what_they_are_not` (a cut
DexDrive file, a PS2 save, a cut PS3 save, a raw save not named by its
directory name, a nameless `.mcs`, an unformatted image),
`the_listing_reads_titles_kinds_and_the_games_own_checks` (discs 1 and 2, a
payload one bit off, a digit past the 15 files, a disc variable of 7, the
Japanese prefix, another game's save; the report naming the first failure),
`a_damaged_directory_is_named_and_harmless_oddities_are_noted` and
`titles_decode_shift_jis_kana_and_say_where_they_stop`. The card device's:
`changes_from_outside_wait_for_the_games_files` (an open file refuses a
change and an import, a check under way and the busy time refuse a change,
the other card goes on, a file screen showing refuses an import into either
card, storage takes the new card) and
`an_imported_single_save_is_the_card_the_bios_makes_for_it_and_a_damaged_card_is_refused`
(a `.mcs` of a file the BIOS created and the game wrote gives that card byte
for byte; a damaged card and a text file are refused, nothing changed).
`xem-core/tests/persistence.rs`:
`a_card_changes_from_outside_the_game_only_between_its_card_calls` (the
stand-in's save holds its file open across frames: a DexDrive import is
refused and changes nothing, `card_list` says why; the save commits and the
import then applies);
`a_card_import_waits_for_the_games_file_screen_to_close_and_a_restore_reads_it_again`
(the stand-in's memory showing a file screen as the game's does when a
frame ends, reading the cards before its card mode, then in it: an import
refused by name each time, `card_list` saying why, nothing changed; closed,
the import goes through once a frame has ended; a state taken while it
showed shows it again once restored);
`scheduled_file_operations_are_rejected_before_any_command_is_queued`
covers the card commands and `card_list`. `xem-game/tests/cards.rs` (a
stand-in module, card files in a temporary directory):
`an_import_reads_any_card_or_save_file_and_says_what_it_found` (`.mcs`,
`.psv`, `.psx`, raw saves and a DexDrive card through `card_import` with each
reply, `card_export`'s raw card, a save the game's load refuses named in the
reply, each refusal leaving the card file as it was) and
`an_import_waits_while_the_game_checks_the_card`. `xem-host`'s
`a_picked_files_name_comes_from_its_path_or_its_android_document`.
`xem-headless/tests/slice.rs`'s
`the_agent_saves_at_field_22_and_the_title_screen_loads_it` (disc 1) lists
the save the game made as its first file of disc 1 passing the game's
checks, imports it as a `.mcs` through `card_import` (a fresh card 1, byte
for byte the card the game made) and plays the title's load from it to the
recorded hash (52 s for the test here, beside other builds).
`xem-headless/tests/file_screen.rs`'s
`a_card_import_waits_while_the_file_screen_shows_and_the_screen_lists_the_imported_card_after`
(disc 1: at the title's Load, its cursor on card 1's one save, `card_import`
of a card holding two saves is refused into either slot with the reason,
`card_list` says it, and neither card or its file changes; Circle back to
the screen's commands, still refused; Circle again to the title's commands,
then the import goes through, and the Load opened again lists the imported
card's two saves) and
`an_import_at_each_frame_of_the_loads_opening_is_refused_or_listed_as_the_card_it_put_in`
(disc 1: from a state taken as the Load opens, at frame 2775, an import at
each of the 35 frames to 2809 of a card holding a save of the same name
with another title and a second save: once the screen has settled, at
2860, it lists the card the slot holds, each save's name and title and
their count, whether the import was taken or refused; taken while the
screen does not read the cards, refused while it does or its card mode
holds; with the card mode alone, the import at 2789 was taken and the
screen listed one save of the two).
`xem-headless/tests/saves.rs` (local: both discs; its five tests side by
side 28 s here) plays the game's own file screens, pressing along the
cursor paths their screens show (the box cursor read from slot39's
`MenuCard`: five columns of three boxes a card, Right a column, Down a
row), and checks each card through `card_list` and its bytes (names, file
digits, the disc each save asks for, blocks, directory frames and the
payload's checksum). After
the slice's save at field 22, that save point saves No.02 (the first free
digit, in the first free block), overwrites No.01 in its block (a later
play time, No.02 untouched), deletes No.02 (its frame marked deleted,
0xA1), copies No.01 to card 1 (the same directory frame and block) and,
asked again, shows a third notice instead (the screenshots read "The same
file exists on the Memory Card in Slot 1. Cannot copy.", message 0x4d)
and writes nothing. On the world map (`world-map-lahan-exit.json`; the
menu on Square, the world map's button 0x10) the file screen shows "No
Memory Card" while both cards are out (`card_present`), and with them in,
after a walk south, saves to card 1; the title's Load puts the party back
on the world map where it was saved (`game_data.worldmap`, zero at the
title before a load: where the party walked to, not where it came out of
Lahan). The story's disc
change (its menu requested as `discs.rs` requests it) saves a file that
asks for disc 2, which disc 1's title loads with disc 2 put in at once and
disc 2's index read. Map 687's save point on disc 2 (where
`tools/memcard_routes.json`'s "first-and-last" saves; the map entered as
the kernel menu does) saves a disc-2 file that disc 2's title loads there.
`web/tests/persistence.test.mjs`'s "a card or save file imports into a card
by its contents, and a refused one changes nothing" (the fixture module: a
`.mcs` before a boot, in IndexedDB and then in the session, listed by
`card_list`; `.psv`, `.psx` and raw saves and DexDrive, VGS, PSP and raw
cards into the running session's card 2 with each report; a damaged card, a
text file and an unnamed raw save refused; IndexedDB's write failing on
quota with card 1 left as it was in both; Import original card 2's own
picker, on a key, offering every extension and reporting a raw save in its
message).

Not done or not measured: kanji in titles are not decoded (they show as
U+FFFD; the table is kept for the rows of symbols, digits, Latin letters,
kana, Greek, Cyrillic and box drawing); the Japanese prefix is inferred, not
confirmed with a Japanese save; the PSP's and PS3's signatures are not
checked; a DexDrive file's comments are not read; the broken sector list
(frames 16-35) and the write test frame (63) are not checked; the containers
are tested with files made here from their documented layouts, not with
files from DexDrive, VGS, a PSP, a PS3 or the save tools themselves; the
native pickers' imports (SDL's dialogs on the desktop, Android's document
picker, a raw save named by a document id among them) are not driven by a
test, only the command and the file name rule they share; the browser's
take-back of IndexedDB when frames run during an import is not tested; the
refusal while a file screen shows is driven at the title's Load only (the
save, copy and delete screens and the disc change's share its card
polling and card mode, read from their code, not driven with an import),
and the frame or two in
which a field or the world map has begun to run a menu but not yet made its
state block read the last menu's block, as the interface context does; a
file screen's copy and delete are tested over the game's own
saves (`saves.rs`), over imported saves only the slice's load; the disc
change's save is made where its menu is requested by a write, not after the
story's last event of disc 1, and the disc-2 save point is reached from the
kernel menu's field entry, not from a story save; the refusals for a full
card (0xac) and another game's file under Save (0xc4) are not driven.

### Save titles and the Kanji font

The game's file screens draw each save's title from the BIOS ROM's Kanji
font: slot39 `menu_save_title_render` asks Krom2RawAdd for each character's
16x15 glyph, narrows it to 12 columns and composes the title's two lines of
16 characters into VRAM, of which the file screen (the save menus' and the
title's Load) shows the second, the place, beside the play time. The ROM is
the player's, so the
host places the font where Krom2RawAdd finds it (`xem_core::bios`): the
player's BIOS image's, or substitute glyphs without one.

Settings → Saves → Backups' Import BIOS (save titles), an app action on
every host and bindable like the others, takes the image through the same
picker as the card imports (SDL's file dialog on the desktop, Android's
document picker, the browser page's own file input offering .bin, .rom and
.bios). `bios::check` takes a file of exactly 512 KiB whose font region (at
0x66000, where the BIOS keeps it) holds a plausible font: the full-width
space blank, and the 62 full-width digits and Latin letters drawn, no two
the same. A refused file changes nothing and the error bar says why
("BIOS not imported: a PS1 BIOS image is 512 KiB (524288 bytes), not 4194304
bytes."). Natively the image is kept as `bios.bin` in the app's directory
(`xem_game::storage::BIOS_FILE`: the desktop's beside `settings.json` and
`cards/`, Android's `<files dir>/bios.bin`); the browser's IndexedDB keeps it
in the `cards` store under the key `bios`, written before the session takes
it, so a failed write changes nothing. Every boot then installs its font
(`xem_game::boot`, the browser's `boot`), and a running game takes it at once
(`Runtime::install_kanji_font`), drawing its next title from it.
`xem-desktop --bios FILE` and `xem-headless --bios FILE` name an image for
that run instead; a file `check` refuses fails the boot, naming the file and
the reason. A kept image is an attachment like a kept card: one that cannot
be read or that the check refuses when it is taken (a file damaged since)
stays as it is, the session places the substitute glyphs, and the storage
line says why ("Imported BIOS image … not used: …; save titles use the
substitute glyphs."), on every host, as a card file that does not load
leaves its slot empty and says why. Natively the boot reads it
(`BootOptions::stored_bios`; the reason is `Services::kanji_font_notice`).
The browser page reads IndexedDB's when it starts (the reason is
`status().page.biosNotice`, which the line shows after the cards' own
error: IndexedDB that cannot be read at all is the cards' error, as they
share its store). Importing an image ends the notice. Agents import as the
panel does: the native file command `bios_import {path}` gives the running
game an image's font (`xem-headless --control`), and
`xem-desktop --control`'s also keeps it as `bios.bin` for every boot, as
Import BIOS does; the browser's automation has `bios.import(bytes)`.

Without an image the substitute glyphs (`xem-core/font/substitute.txt`,
described in its `README.md`) stand where the BIOS keeps those characters,
and every other glyph is blank: 68 glyphs drawn for this project and
licensed with it (MIT), taken from no BIOS and no other font. They are what
North American titles hold: "ＸＥＮＯＧＥＡＲＳ／Ｎｏ．", the file number, a
full-width space and 30 bytes of the line the story value picks from
directory (0x10, 1) file 1 (`menu_save_build_header_title`,
`menu_card_init_and_read_title`). `python3 -m tools.analysis.save_titles`
lists the 61 codes that both discs' 355 lines ask for with all 15 file
numbers (the space, the digits, 45 Latin letters and ． ！ ／ ’ −) and checks
that each has a glyph; the seven letters no line uses are drawn too. The
glyphs have thin strokes at a full-width font's height, placed for the
screen's narrowing (columns 1-2, 5-6, 9-10 and 13-14 merged), with the top
row clear because the screen reads a sixteenth row from the next glyph.
They are a substitute: other games' titles on the same card keep blank
glyphs where they use other characters, and so would the staff roll
(directory 4 file 0xAB, `field_staff_roll_write_line`), which asks the font
for 575 characters, most of them kanji in the staff's names, though no
shipped script enables it ([original-boundaries.md](original-boundaries.md),
Services: BIOS Kanji ROM).

The font region lies outside snapshots: like the disc and the cards it is
the session's attachment, which a restored state keeps; `status` says which
font is in (`kanji_font`). A title's pixels pass through game memory (the
screen composes them in heap blocks it frees), so a title drawn from another
font changes RAM there until the game reuses the blocks. The slice logs,
whose save and load show titles, end at their recorded hashes with the
substitute as they did with blank glyphs, and as they do with every pixel of
every glyph set (measured once with the font region filled, by
`write_memory`).

Tests: `xem-core`'s `bios` tests:
`glyphs_follow_the_fonts_code_order` (known codes' glyphs, codes the font
lacks, each of the 3489 glyphs one code's),
`the_port_finds_glyphs_where_the_host_puts_them` (the host's table of rows
1-8 and the glyph count, address and size are `src/port/bios.c`'s),
`the_substitute_covers_what_north_american_save_titles_hold` (the 68 codes
in order, each drawn but the space, the top rows clear, the data's rows),
`a_bios_image_is_checked_for_its_size_and_its_font` (0 bytes, 512 KiB less
or more one byte and 4 MiB; zeros, 0xFF, noise and one glyph for every
letter, each refused naming why) and
`the_font_comes_from_a_bios_or_the_substitute` (the region as each leaves
it; a refused file writes nothing). `xem-host`'s
`an_imported_bios_is_kept_for_every_boot_and_a_file_that_is_not_one_is_refused`
(the native import once a file is picked: a 1000-byte file and a blank font
refused by name, nothing kept; a plausible image made here kept as
`bios.bin`, where the boots find it). `xem-headless/tests/file_screen.rs`
(disc 1): `the_file_screen_draws_save_titles_from_the_substitute_glyphs_or_a_bios_image`
(the title's Load over a card holding a save made here: the title image in
VRAM is, halfword for halfword, what `menu_save_title_render` composes from
the substitute font, and from the font of a BIOS image made here, the
substitute glyphs drawn bold; a 256 KiB file named for the run fails the
boot by name) and
`an_imported_bios_the_check_refuses_is_reported_and_bios_import_gives_the_running_game_a_font`
(a kept image gives the boot its font; kept as 256 KiB, the boot places the
substitute glyphs with the notice naming the file and the reason, the file
unchanged; then, before the Load's cursor reaches the save, `bios_import`
refuses that file by name and takes the bold image, ending the notice, and
the title drawn next is the bold image's).
`web/tests/persistence.test.mjs`'s "a BIOS image imports beside the cards
for every boot, and a file that is not one changes nothing" (a 1000-byte
file and a blank font refused by name, nothing kept; IndexedDB's write
failing on quota; the image there after a reload; Import BIOS on a key
opening the page's picker; a 1000-byte image written to IndexedDB past the
check, after a reload: kept, not used, reported by name, the notice ending
with an import) and "denied storage remains usable and refuses to
acknowledge unsaved settings" (no BIOS notice: the cards' error says it);
`web/tests/disc.test.mjs`'s "boots with an
imported BIOS image's Kanji font, else the substitute glyphs" (disc 1: the
font's Ａ without an image, then from an imported one at once and after a
boot); its comparison with `xem-headless` reads the substitute's Ａ on both.
The Android emulator's smoke (`android-smoke.sh`, with disc 1) boots the
game with a plausible image made there in `<files dir>/bios.bin` and checks
that the boot took its font (`font=bios` in the boot's log line).

Not done or not measured: no BIOS image exists on this machine, so no title
has been drawn from a real BIOS's font: the BIOS path is tested with images
made here, and the plausibility check is reasoned from the font's layout
(a blank space, distinct letters), not run on BIOS versions; the desktop's
and Android's pickers are not driven by a test, only what follows a pick
(`import_bios`), nor is `xem-desktop --control`'s `bios_import` (the
panel's `import_bios` of the named file); the substitute's look is its
author's judgement from the file screen's screenshots (no people tests).

### Cards in and out

Both memory cards are in, as a console's stay in. The recorded control
`card_present {slot, present}` takes a card out or puts it back
(`Card::set_present`), for agents and tests; no panel offers it. A card
taken out stays the slot's, its image and storage as they were (`card_list`
lists it, `present` false; an import replaces it there, for the game to find
when it is put back), but the game finds no card there, as the BIOS
answers without one: `_card_info` ends in the timeout event and every file
call fails; the game's files open on it close and a directory search there
ends. Put back, the card answers again: a check ends done for a formatted
card, new for an unformatted one. The game's file screens check both cards
while they show (`menu_card_poll_ports`) and treat the change as a card
pulled out and put in (`menu_card_check_port`): a check that times out marks
the card absent and clears its listing, one that ends done after a timeout
marks it present again, and a presence that changed makes the screen list
that card anew. With both cards out, the title's Load shows the game's "No
Memory Card" notice for about a second and returns to the title
(`menu_card_refresh`); with card 2 in, taking card 1 out at the file screen
drops card 1's listing and putting it back lists its save again, each within
a few frames here (the game's next check).

It is recorded like the other controls: in a recording's controls and at
its start when a card is out, reset to both in when a replay starts, in
session logs, and in the agent tools' logs (`tools/native_scenario.py`).
Whether a card is in is the host's, not the game state's: a restored state,
a rewind and a console reset keep it, and a state's file open on a card
that is out closes on restore, as on a card taken out. Its default, both
in, is the original's, so the slice logs and replays are unchanged.

Tests: the card device's `a_card_taken_out_answers_as_no_card_and_again_when_put_back`
(the timeout, a closed file, an ended search, no listing, no file opened,
created, deleted or formatted, the card unchanged and still listed for the
host; done and the file listed when it is back),
`a_check_ends_with_what_the_slot_holds_when_it_ends` and
`a_restore_keeps_the_cards_in_or_out`; `xem-core/tests/persistence.rs`'s
`card_presence_is_recorded_and_a_replay_keeps_it` (the stand-in's save
refused with card 1 out and made with it back, each change recorded once at
its vblank, a replay putting both in first and then following the recorded
changes, a restore and a reset keeping card 1 out, a recording's start, a
session log, and validation); `xem-headless/tests/file_screen.rs`'s
`the_titles_load_finds_no_card_while_both_are_out_and_lists_a_card_put_back`
(disc 1: the notice with both cards out and the return to the title's
commands, card 1 untouched; card 1 put back and listed with this game's
save; card 1 out and back at the file screen with card 2 in, its listing
dropped and made again). The two `file_screen.rs` tests take 7 s together
here (suite profile, with other builds beside them).
`xem-headless/tests/saves.rs`'s
`the_world_maps_file_screen_finds_no_card_while_both_are_out` (disc 1, on
the world map at the end of `world-map-lahan-exit.json`): with both cards
out, File in the world map's menu shows the "No Memory Card" notice (the
notice up, both cards absent in the file screen's state, no card message)
and the menu takes the pad again, both cards untouched
([Original saves and memory cards](#original-saves-and-memory-cards)).

Not done or not measured: on a console a card put in is flagged new until
it is written (psx-spx, "Memory Card Read/Write Commands"), and the BIOS's
check may then report a new card, which the game takes as an unformatted
card's result (`menu_card_event_results`); this device does not model the
flag, and the game's handling of a real swap is not verified (no capture);
the save menus' one-card paths (a field save point's, the world map's, the
disc change's) and the no-card paths of a field save point's and the disc
change's are not driven by a test, nor copy or delete over a card put back.

## Gameplay options

Phase 7 (plan.md) adds options that change how the game plays, each
reimplemented from the decompiled program, none patching game data. They
sit together in the settings panel's Gameplay section (the browser shows
the same panel); each is a setting of `xem-settings` and reaches the game
as a recorded control of the session, whose zero is the original's:

| Option | Setting (default) | Control | What changes | Agents see its effect in |
| --- | --- | --- | --- | --- |
| [Random battles](#random-battles-in-platforming-areas) | `random_battles`: `NotWhilePlatforming` (the default), `Original` | `random_battles {off}` | the field's random draw does not run on the platforming areas' maps (`src/port/encounters.c`) | `status.random_battles_off`; `field.random_draw` (`off_here`, its window and steps left as dealt) |
| [Encounter rate](#random-battles-in-platforming-areas) | `encounter_rate`: `Original` (the default), `Half` | `half_encounters {enabled}` | the draw runs on every other field frame where it runs at all | `status.half_encounters`; `field.random_draw` (its steps count down half as fast) |
| [Text speed](#text-speed-auto-advance-and-movie-skipping) | `text_speed`: `Original` (the default), `Fast`, `Instant` | `text_speed {glyphs}` | a message window's step reveals at least 4 glyphs, or a line, except while a script counts out a page before its wait for Confirm (`src/port/text_speed.c`, `xem_core::dialogue`) | `status.text_speed`; `status.dialogue` (a page's prompt shows sooner) |
| [Auto-advance dialogue](#text-speed-auto-advance-and-movie-skipping) | `auto_advance`: off (the default) or on, a dwell of 30-600 vertical blanks (120) | `auto_advance {dwell}` | the session presses Confirm once a field dialogue's prompt has shown the dwell, never at a choice (`xem_core::dialogue`) | `status.auto_advance`, `status.auto_advance_waited`, `status.dialogue` |
| [Movies can be skipped](#text-speed-auto-advance-and-movie-skipping) | `movie_skip`: off (the default), on | `movie_skip {enabled}` | the story's movies take the original's early exit on the player's press (`src/port/movie_skip.c`) | `status.movie_skip`; `status.movie` (`ends_on_press`) |

Cutscene skipping is not offered: the game marks no next moment the
player controls clearly enough ([Cutscene skipping](#cutscene-skipping)),
so it was cut with the battle-message advance that belonged to it; nor is
an EXP and gold multiplier.

**Defaults.** A fresh settings file (the desktop's and Android's
`settings.json`, the browser's `xem.settings`) holds random battles off in
the platforming areas and the original's every other option, and the hosts
give the session the platforming areas' maps and the original's values.
Each choice is stored by name: a file saved before an option existed reads
its default and resets nothing else (`Settings::read`); a deliberate choice,
`Original` among them, survives other changes, reloads, a settings backup
and its import; and a revised table of platforming areas applies to
everyone who keeps the default. An import replaces the settings whole, so a
backup saved before the options existed brings their defaults. The
shortcut list is one setting, kept as saved, but an action with a default
binding that a saved list lacks (`ToggleAutoAdvance` in files saved
before it existed) reads that default, less any chord the list gives
another action ([below](#text-speed-auto-advance-and-movie-skipping)).
The interactive hosts (the
desktop, Android, OpenXR's play and a running browser page) give the
session the values before every game frame outside session logs;
headless sessions, session logs and replays play the original unless
commanded, and a replay keeps the recorded values.

**Agents** change the options as people do, through the settings service,
and get its acknowledgement. On `xem-desktop --control`,
`{"cmd":"settings"}` answers the settings and
`{"cmd":"settings_apply","change":{"TextSpeed":"Instant"}}` applies the
typed change and answers `{revision, settings}`, or `{error}` with the
reason and the settings unchanged (`SettingsService::command`, the
panel's own service). In a browser, `window.xem.settings.apply({TextSpeed:
'Instant'})` answers `{ok: true, revision, settings}`, or `{ok: false,
rejected, reason}` for a change the service refuses; a value that is no
`SettingChange` (`{TextSpeed: 'Warp'}`) throws an `Error` ("bad change:
…") where the desktop answers `{error}`. `xem.settings.get()`,
`.export()` and `.import(text)` read, back up and restore them. The next frame's `status`
shows the control the host gave. Where no host gives the values
(`xem-headless --control`, session logs, a browser page whose frames
commands drive) agents set the controls directly, recorded as the hosts'
are. Android has no agent channel.

**Ordinary progression with the defaults**, played on the game headless
(suite profile, disc 1; frames are vertical blanks):

- *Through a suppressed area.* From map 413's entrance from map 412
  (where map 412's door leads once the tower's gate is open), in the
  party's gears, a walk as a player makes it climbs Babel Tower through
  the original's doors of maps 413, 414, 415 and 416, each arming its
  draw and each turned off by the default, to map 418, the floor past the
  tower's platforming maps where the Self-repairing Robot resupplies (two
  maps below the top, map 420), after 3 266 frames with no battle;
  touching map 418's actor 20 still starts its fixed battle (formation 2,
  4 740 frames in). The walk is the
  assisted route to each map's door with Jump pressed at each takeoff, and
  a running jump toward the door each time the route stops short: the
  route plans a jump only where it enters a ledge (attribute bit 22, the
  [slides](#how-the-areas-are-classified)) from below, so it walks into a
  floor that is only higher and stops there. Under the original the same
  walk meets a random battle on map 414 after 428 frames. Map 172's Repair
  Bot still starts its scripted battle under the default
  ([below](#random-battles-in-platforming-areas)).
- *A chapter's scenes.* The game's opening from the title's New Game (the
  title, the narration on map 4, Lahan's maps 2 and 14 and the battle on
  map 14) plays to Fei's room in the village chief's house, story value 6
  on map 14 with the player walking. With what the hosts give the session
  from the default settings it ends at the original's frame (18 837) and
  memory; with Instant text, auto-advance at its shortest dwell, movies
  skippable and half encounters it shows the same maps, fights the same
  battle and reaches the same story value after 18 323 frames. Each
  option's scenes, timed ones among them, are compared with the
  original's [below](#text-speed-auto-advance-and-movie-skipping).

Nothing soft-locked in these runs.

Tests: `xem-settings`'
`the_gameplay_options_default_to_not_while_platforming_and_the_original_and_explicit_choices_survive_saves_backups_and_imports`
checks the first save of a fresh file and the values the sessions get;
that a file holding every key but the options' (a real file through
`FileStore` too) reads their defaults, keeps the rest and resets nothing;
and that a choice of each, acknowledged in turn, survives other changes,
a whole-settings change, a reload, a backup and its import, while a
backup saved before the options imports their defaults.
`a_shortcut_list_saved_before_an_action_existed_reads_its_default_binding_less_a_chord_another_holds`
checks that a list saved without `ToggleAutoAdvance` reads T and
Back+West and resets nothing, that a chord the list gives another action
stays that action's, that an unbound action stays unbound, and that the
next save writes the binding.
`agents_change_every_gameplay_option_through_the_settings_commands_and_read_the_acknowledgement`
sends each option's change as an agent's JSON line, reads the revision and
the settings back, and checks that refused changes (a dwell out of range,
values no longer offered) say why and change nothing. `xem-host`'s
`an_agents_gameplay_settings_reach_the_session_on_the_next_frame` follows
those commands through `Game::update` into the session's `status`, beside
its per-option tests of the defaults and of session logs.
`xem-desktop/tests/control.rs` (local: disc 1, the game module, and
weston's headless backend and lavapipe from the runtime shell; about 2 s
here) runs `xem-desktop --control` itself on a fresh configuration
directory: the defaults in `status`, each `settings_apply` acknowledged
with its revision, a refused dwell, the next frames' `status`, the
settings file, and a second launch that starts with the choices. In
headsets, the xr tier's smoke (`xr-game-smoke.sh`, Monado) has an agent on
`xem-desktop --xr --control` read `status` while field 30's state plays
(random battles off on xem-core's platforming maps, the other options
the original's), apply `RandomBattles` Original (acknowledged with its
revision) and find none off in the next `status`, and
`xr-game.test.mjs` (IWER in Chromium, the timing stage) reads the same
in an immersive session through `xem.status().session` and
`xem.settings.apply`. A
browser test (`persistence.test.mjs`, the fixture module) loads an
`xem.settings` saved without the options and the auto-advance shortcut
and reads their defaults, the
running page gives the session the default maps, `xem.settings.apply`
acknowledges each option's change in turn and refuses a dwell out of
range, the session shows each, and a reload and an exported backup's
import keep them. On the game,
`the_default_setting_climbs_babel_tower_through_its_doors_and_map_418s_fixed_battle_still_starts`
(`xem-headless/tests/encounters.rs`, about 11 s here) and
`the_opening_chapter_reaches_feis_room_with_the_defaults_as_the_original_and_with_every_scene_option`
(`xem-headless/tests/text_and_movies.rs`, about 31 s here, its three runs
side by side) play the runs above, and the second file also reads `status`
on map 25's story movie (`movie.ends_on_press` false without the option
and true with it, `null` on map 23), frame by frame through the boot's
and the title's movies from boot (`movie` only while the movie's loop
reads the press, a press it offers ending each and one it does not
changing nothing; [below](#text-speed-auto-advance-and-movie-skipping)),
and at map 172's choice (`dialogue` a choice, `auto_advance_waited` 0).
The slice logs replay to their recorded hashes
(`xem-headless/tests/slice.rs`).

Not done or not measured: under the default, the enemies met only in the
platforming areas' random battles (battle 32's enemy 0 and battle 59's
enemy 2) are not met ([below](#how-the-areas-are-classified)). The climb
does not cross Babel Tower's maps 407, 409 and 412 (map 412's door to map
413 opens only once map 417's switch, reached on foot, sets the tower's
flag, which an `enter_field` start does not), and it stops at map 418:
the climb on to the top (map 419, whose Climb or Stop choice leads up,
and map 420, the top, neither arming a random draw) and the top's battle
(the Mystery Girl's, `request_battle #14` on map 420) were not played. No
run crossed Blackmoon Forest or the Aveh-Kislev border mountains end to
end. The
climb starts from `enter_field`'s game data with
the gears set, not from a story save, and its route is the assist's with
running jumps where it stops short, not a person's. The chapter is the
game's opening only and holds no story movie; no later chapter was played
end to end with the options, and movie skipping was measured on map 25's
movie. The desktop's agent channel was run on weston's headless backend,
a compositor with no screen. The Android host gives the values through
the same `Game::update`, and no Android run used the options on the game
(it has no agent channel to read them). The headset runs read the
defaults, an agent's `RandomBattles` Original and the AUTO key
([below](#text-speed-auto-advance-and-movie-skipping)); no random battle
or scene was played there under an option. No person tried the options.

### Random battles in platforming areas

The field's random battles come from one draw, `field_encounter_count_down`,
which player control runs every field frame a direction is held on a map
whose scripts armed it (event f7; [formations](scripts/formations.md)): each
period of counted frames starts the armed number of battles, one as each of
the steps the period dealt comes down. Phase 7 (plan.md) turns them off by
default in platforming-heavy areas. `xem_core::encounters::PLATFORMING_AREAS`
lists those areas, a name and the maps there that arm the draw
(`platforming_maps()` their union, ascending), classified as [below](#how-the-areas-are-classified).

**The setting.** `Settings.random_battles` and `Settings.encounter_rate`
(the panel's Gameplay section, `SettingChange::RandomBattles` and
`SettingChange::EncounterRate`):

| Setting | Values |
| --- | --- |
| random battles | Not while platforming (`NotWhilePlatforming`, the default): none on the maps of `PLATFORMING_AREAS`, the game's elsewhere; Original: wherever the game has them |
| encounter rate | Original (the default): the random battles left on come as the game's draw deals them; Half: half as often for the same walk (as Perfect Works Build's half encounters, reimplemented here) |

The setting stores the policy's name, not its maps, so a revised table
applies to everyone who keeps the default and a deliberate Original is
never overwritten; a file saved before the setting existed reads the
default, and a policy no longer offered resets alone (`Settings::read`).
`Settings::random_battles_off()` resolves the policy to its maps. The
Gameplay section (between Saves and About, with the [dialogue and movie
options](#text-speed-auto-advance-and-movie-skipping) under it)
shows the choice as a button group, as Encounter rate's; the browser
renders the same panel, and a narrow panel's strip of tabs holds all eight
sections ([Desktop host and settings panel](#desktop-host-and-settings-panel)).

**The control.** Before every game frame of a batch outside session logs
the interactive hosts give the session the resolved maps, as they give it
the actor view: `Game::update` (`xem-host`: the desktop, Android and
OpenXR) and the browser's frame loop while it runs the session call
`Session::set_random_battles`. `src/port/encounters.c` wraps
`field_encounter_count_down` and asks the host import
`xem.random_battles(map)` with `mode_field_map_id & 0xFFF` (the high bits
mark the party's gears). On 0 the draw does not run at all: no step counts
down, `rand()` is not called and no battle is requested, as on a map whose
scripts never armed it. Scripted battles (events 71 and fe 84), the scripts'
f7 arming and inhibitions, map changes and the world map's encounters
(`worldmap_encounter_roll`) stay the game's, and a map the control leaves
out runs as the original's. The port keeps no data: the module's shadow
stack starts where it did (`__stack_pointer` 0x101C1340). The maps are a
recorded control: `set_random_battles` records
`{"cmd":"random_battles","off":[...]}` (ascending, each map once) with the
inputs whenever they change, a recording begun with maps off starts with
them, a replay starts from none and keeps the recorded maps over the
host's, and session logs may schedule it. Headless sessions, session logs
and replays play the original unless commanded, as with the actor view.
`status` reports `random_battles_off`; `field` reports `random_draw`: the
map, the draw's period and count as f7 armed it (a period of 0 is a draw
the map has not armed), the frames left of the period (`window`), the
frames left before each of its battles (`steps`, 65535 once spent) and
`off_here`. They read the field's memory, so they mean something only
while the field runs. Measured headless on disc 1 (suite profile): on
Babel Tower's map 414 (period 300, one battle) Up held from entrance 0
brought a battle request 140 frames in without the control; with the
default setting's maps, none in 6 020 frames, the window still 300 and the
step still 52 as dealt.

**Half the encounters.** The same import gives the draw's pace: 0 off, 1
the original's, 2 half (`Session::set_half_encounters`, the hosts' value
`Settings::half_encounters()`). At 2 `encounters.c` runs the draw only
on even field frames, by the dialogue's tick (`field_dialogue_tick_count`,
which the field's draw pass counts once a frame), so its steps count down
half as fast and its battles come after twice the walk; the steps it
deals, the battles' formations' weights and every other rule stay the
game's, and a map turned off stays off. The world map's encounters keep
the original's rate. It is a recorded control like the maps
(`{"cmd":"half_encounters","enabled":true}`; `status` reports
`half_encounters`). Measured headless on the Mountain Path (map 15, its
draw dealt period 240 and step 48) with Up held from entrance 0: in the
first 60 frames the step counted down 14 under the original and 7 halved,
and the battle came 130 frames into the walk under the original and 230
halved.

**Agents** change the setting as people do, through the settings service:
`{"cmd":"settings_apply","change":{"RandomBattles":"Original"}}` on
`xem-desktop --control` and `window.xem.settings.apply({RandomBattles:
'Original'})` in the browser; the next frame's `status` shows the maps the
host gave. The interactive hosts (the desktop, a running browser page,
Android and OpenXR) give the setting's maps before every frame, so there a
`random_battles` command lasts only until the next frame, both changes
recorded. Where no host gives them, agents set the maps directly with
`{"cmd":"random_battles","off":[407, 414]}` (field maps 0-4095, any
order): `xem-headless --control`, session logs, and a browser page whose
frames commands drive (`setRunning(false)`, then `runFrames` or
`step`).

Tests: `xem-settings`'
`random_battles_stay_off_in_platforming_areas_unless_the_original_is_chosen`
checks the default policy and its maps, that the default saves its name,
that a deliberate Original survives other changes, a reload and a backup,
an agent's change, and that an old file or a policy no longer offered
reads the default alone; the same test checks that the encounter rate
defaults to the original's and that Half is saved, reloaded, carried by a
backup and a value no longer offered resets alone.
`tests/test_port_encounters.py` (suite.sh's port
stage) builds `src/port/encounters.c` natively with `-Wall -Werror`
against an invented host: the draw runs where the host leaves it on and
not where it turns it off, asked with the map's low 12 bits, and halved it
runs on even field frames only. `xem-core/tests/persistence.rs` checks on
the stand-in module, whose draw asks the import every frame: no draw on a
map turned off (its gear bits aside), each change recorded once with its
maps ascending, a replay starting from none and keeping the recorded maps
over the host's, drawing as the recording did, a recording begun with maps
off, `schedule`, `readiness`, refused maps and refused recorded controls;
`field`'s `random_draw` with `off_here`; and
`half_encounters_are_recorded_a_replay_keeps_them_and_a_map_turned_off_stays_off`
(the pace 2 where the draw stays on and 0 where it is off, each change
once, the replay's start from the original's and its recorded halving over
the host's, a recording begun halved, `schedule`, `readiness` and refused
values). `xem-host`'s
`frames_give_the_session_the_settings_random_battle_maps_and_a_session_log_none`
checks that `Game::update` gives the session the default policy's maps,
none under Original, the halving under Half, and none of either while a
session log plays. `xem-ui/tests/panel.rs` checks that the Gameplay
section's choices apply through the service, are acknowledged and follow
an agent's change.
`xem-headless/tests/encounters.rs` (local, disc 1, 14-23 s here) checks on
the game that on map 414 Up held brings a random battle within two of the
draw's windows without the control and none in five under the default
setting's maps, the draw as dealt; that map 414's door still leads to 415
and map 172's Repair Bot, its third choice picked, still starts its
scripted battle (formation 3) under them; and that on the Mountain Path
(map 15, left on) the same walk brings its battle at the same frame and to
the same RAM with them as without, and halved the same draw counts half
as far in 60 frames of walk and its battle comes later; and that under
them the party climbs Babel Tower from map 413 through its doors to map
418 with no random battle, where map 418's fixed battle still starts,
while the original's random battle interrupts the same walk on map 414
([Gameplay options](#gameplay-options)). The slice logs
replay to their recorded
hashes (`xem-headless/tests/slice.rs`). A browser test
(`persistence.test.mjs`, the fixture module) checks that the running page
gives the session the default policy's maps, that `RandomBattles:
'Original'` reaches it on the next frames, is saved in `localStorage` and
survives a reload, and that the Gameplay tab is reachable.

Not done or not measured: the world map's random battles stay on (no
platforming there) at the original's rate whatever the encounter rate, and
no option turns all of them off. Under the default the enemies met only in the listed
maps' random battles ([below](#how-the-areas-are-classified)) are not met.
The Android and OpenXR hosts give the maps through `Game::update`, which
the desktop shares; no headset or phone run checked it on the game.

#### How the areas are classified

`python3 -m tools.analysis.platforming` classifies both discs' maps (disc
2's 205 map bundles are disc 1's of the same numbers): `--table` (the default) prints
every area with its maps' metrics and the reason, then the unresolved
areas and the enemies the table loses, `--map N` one map's layers, regions,
links and entrances, `--rust` the table, and `--check` fails unless
`src/runtime/crates/xem-core/src/encounters.rs` holds exactly that table.
It reads `.local/extract` and `.local/discs`; a run takes about 33 s here
(16 threads, one process).

**The jump**, measured headless (`xem-headless --control`, suite profile,
disc 1): the party leader's `FieldActor` position (16.16) every vertical
blank of a jump with Run and a direction held over open floor. A field
update takes two vertical blanks; distances are from where the leader left
the floor.

| Party | Where | Rise | Flat reach | Arc |
| --- | --- | --- | --- | --- |
| Fei on foot | map 22, `enter_field` | 118.25 | 240 | up 20.75 an update less 2.0 each (analysis/formats/jump-physics.md), 11 updates up and 11 down |
| The party in its gears | map 414, `game_data.inGear` set | 131.25 | 204 | up 24.75 less 2.0 for 7 updates, one still update, then a fall from rest, 11 updates |

Both go straight up for two updates, then 12 units an update in the air
with Run held and 6 without; on the floor Fei runs 16 units an update and
walks 5, a gear 12 and 6. A jump lands nearer the higher it lands: at a
rise of 100 Fei reaches 156 units and a gear 132. `enter_field` alone
leaves `game_data.inGear` clear, so on map 414 the leader is then its pilot
on foot with the gears' alternate sprite set (fe 1e), which hops 31.5
units; the gear measurement takes `enter_field`'s kernel-menu recipe with
`inGear` set for the three slots before the field loads.
`xem-headless/tests/jumps.rs` repeats both measurements. A walk off a
floor falls from rest under the same gravity: on map 15 (below) the leader
dropped 2, 6, 12, 20, 30 ... units after one, two, three ... updates. The
classifier gives such a fall the jumps' air speed, 12 units an update with
Run held.

**Slides.** On a floor of attribute 0x400000 the leader has no direction:
`field_actor_apply_additive_motion` sets bit 15 of its heading before
`field_actor_move` moves it, so only the slope's push moves it, down, and
`field_collision_walk_attribute_aware` refuses a step onto one whose floor
is above it. Placed on map 22's slide triangle 1118 with the D-pad held up
the slope, the leader slid down from y -23 through -21, -18, -14, -9 and -5
to the floor at 0 in seven updates and could not step back onto it;
`xem-headless/tests/jumps.rs` repeats this. A pressed jump starts on no
slide; a leader stuck on one (not moved) for 32 frames with Jump held jumps
from it (`field_event_request_player_control`). The assisted movement's
planner (`xem_core::field`) walks ledges to ledges freely; the classifier
does not.

**Layers.** A map has two to four collision layers; actors stand on layers
0 to count - 2 and the top one is the camera's floor
(`field_camera_walk_top_layer`). The game chooses the leader's layer itself
on every move (`field_actor_update_position`): in the air, and where its
own layer has no floor, it takes the highest floor below it on any actor
layer, and a floor of attribute 0x4 lifts it to the next floor up. A
triangle of attribute 0x800000 has no floor on its layer
(`field_collision_find_actor_floor`): on layer 0 the walk planners refuse
it, and on an upper layer the leader walks into it and drops. Placed on
map 15's layer-1 floor at (101, -327, 74) and walked toward the 0x800000
triangle beside it, the leader left layer 1 for layer 0 after 15 updates,
fell 209 units and walked on along layer 0's floor;
`xem-headless/tests/jumps.rs` repeats this. The classifier's first version
walked such triangles as floors at their own height: on map 641 (Anima
Relic 2) that joined layer 1's two platforms, and the area was listed.

**Method**, per map and actor layer, from the walkmesh and the scripts:

- **Party.** In its gears when the map's scripts reach fe 1e, else on
  foot. On foot attribute bit 7 blocks walking and bit 4 airborne moves
  and jump starts, in a gear bits 6 and 3 (the leader's flag bits 8-10,
  `field_event_become_party_character`). Each actor layer is measured on
  its own.
- **Floors and pits.** Nobody stands on a triangle that blocks the party
  or has no floor (0x800000). On an upper layer a floorless triangle is a
  pit when the highest floor the other actor layers have under its
  centroid, below its highest corner, is one the party may stand on: the
  leader falls through to it. One with no such floor, or whose floor
  blocks the party, is a wall, as on layer 0 (`field_actor_update_position`
  refuses the move).
- **Walk graph and regions.** The leader walks from a floor to any
  neighbouring floor and onto a slide not more than 8 units above it
  (centroids, as `xem_core::field` compares them); from a slide it only
  goes on to neighbours not more than 8 above. A walk into a pit falls, as
  measured above, over pits and the slides it clears to a floor of the
  layer, unless it reaches a pit's floor below first. Regions are the
  strongly connected components of the floors, a slide or a fall joining
  the floor it starts from to the floors it ends on. A walk link between
  regions that leads down a slide or a fall is a **fall** when its way
  back needs a jump.
- **Jump links.** From a floor's edge into a slide or a pit, over slides
  and pits, to a floor of another region: every edge crossed (its
  midpoint) within the measured arc, as far and as high, above each pit's
  floor below, and no triangle the party may not cross in the air. A
  pressed jump starts on no slide.
- **Entrances** are the entry records (the bytecode opens with ff; record
  n at 1 + 7n holds x, z and the layer) that other maps' map changes name
  (12, 47, 98, fe 84, fe cf with immediate operands; the debug maps 720-729
  aside) and the world map's path regions (four tables per area file and
  `worldmap_fixed_path_regions`; a region of kind 4 only names a
  destination), each on the first triangle whose edges wind around it as
  `field_collision_find_floor_triangle` tests them. One outside every
  triangle, on a floorless one, on one that blocks the party or on a slide
  that ends on no floor stands on no floor; `--map` says which.
- **Traversal jumps.** For each ordered pair of a map's entrances on one
  layer, the fewest jump links on the way (a 0-1 breadth-first search,
  walk links free); the map's figure is the most any joined pair needs. A
  pair on two layers, one that its layer does not join and one with an
  entrance on no floor are not measured and count nothing, so the figure
  of a map with such pairs is a lower bound: the map is *partly measured*,
  or *not measured* when none of its pairs is joined. A map's *climb* is
  the most jumps any region needs that a walk from one of its entrances
  reaches.

**Areas and the rule.** An area is the armed maps whose draws start the
same encounters (the battle and enemy ids of each formation with a weight;
sets that differ only in a stage, a weight or an undrawn formation are one
area's, as maps 444-450's are) that doors join, through maps that are not
armed with other encounters. An area is platforming-heavy when at least
half of its armed maps need at least one traversal jump and its maps'
traversal jumps sum to at least 3, as measured; an area heavy on either
disc is listed. One that is not, but would be if each of its maps with
pairs not measured needed a jump, is *unresolved*: the classification
cannot decide it and does not list it. Players' complaints augment the
rule: an area that independent sources name for its platforming, and
whose maps arm the draw, is listed whatever its figures (the tool's
`PLAYER_EVIDENCE`, below). Its name is then the players', else the world
map's for the entrance fewest door steps away (at most two); the later
areas' name tables are Japanese and name nothing.

**Player evidence.** At the user's direction (2026-10-14) the
classification was cross-checked against what players say: web searches of
reviews, retrospectives, forum threads, walkthroughs and a Let's Play for
the places Xenogears players complain about for their platforming or
jumping, alone or with random battles. An area with several independent
complaints and armed random battles joins the table; a measured area
without any would stay only on clearly platforming figures (none had to).
The sources name places, not maps; the game's own data maps them: Babel
Tower is the world map's name for scenes 405 and 411, whose doors join
maps 405-422; Blackmoon Forest is its name for map 22. The Aveh-Kislev
border mountains have no world-map entrance (the Xeno Series Wiki's
[Mountain Cave](https://www.xenoserieswiki.org/wiki/Mountain_Cave) "does
not appear on the map"); doors join maps 172-177, map 174's script sends
Elly's Gebler unit against Fei while "Bart's team is already on their way
to the castle", map 175's says "Head for the mountain peak", and the draws
of maps 172, 173 and 175 start battles 21, 27 and 32, whose name tables
hold the Nomad Fix Bot, Medusoid, May Fly and Edelweiss, the enemies the
wiki lists for the Mountain Cave. Quotes are verbatim; reddit refuses
direct reads here, so its quote comes from the search engine's index.

| Area | Source | Quote |
| --- | --- | --- |
| Babel Tower | [Kotaku's look back at the game](https://kotaku.com/gears-gods-and-giant-robots-a-look-back-at-one-of-th-5911139) (Jason Schreier) | "Especially when you're trying to pull off a tricky jump in the Babel Tower and suddenly you get into a random battle" |
| Babel Tower | [Hardcore Gaming 101](https://www.hardcoregaming101.net/xenogears/) (Maciej Miszczyk) | "a lot of frustration will be had in the mountains on Aveh-Kislev border and in the infamous Babel Tower" |
| Babel Tower | [Carl's Backtrack and Bosses, Wayback Wednesday vol. 25 part 2](https://buymeacoffee.com/carldexter/xenogears-wayback-wednesday-vol-25-part-2) | "A few times, a random encounter occurred just as I was pushing the jump button to cross a gap" |
| Babel Tower | [r/XenoGears, "Babel Tower"](https://www.reddit.com/r/XenoGears/comments/1kt4ito/babel_tower/) | "the prime example of why the platforming in this game was so awful" |
| Aveh-Kislev border mountains | Hardcore Gaming 101 (above) | "the mountains on Aveh-Kislev border", with "most of mistakes being punished with annoying random encounters" |
| Aveh-Kislev border mountains, Blackmoon Forest | [Nintendo World Report, "The Thirsty Mage: Xenogears"](https://nintendoworldreport.com/feature/50044/the-thirsty-mage-xenogears), comment by David Lloyd | "the forest at the beginning and the mountain hopping in the Gear were particularly annoying" |
| Aveh-Kislev border mountains | [Almar's Guides, chapter 2](https://almarsguides.com/retro/walkthroughs/PS1/Games/Xenogears/FullPlaythrough/Chapter2/) ("Fei At The Rockies"; a walkthrough, no complaint) | "In the next area you'll find a bunch more jumping that we need to do" |
| Blackmoon Forest | [Almar's Guides, chapter 1](https://almarsguides.com/retro/walkthroughs/PS1/Games/Xenogears/FullPlaythrough/Chapter1/) (a walkthrough) | "There will be a hobgoblin here which will jump across the roots, you need to follow him" |

Others name no place: [PlayStation LifeStyle's review](https://www.playstationlifestyle.net/?p=135866)
(Heath Hindman) finds "a few segments of required jumping tend to be
frustrating", and a commenter on the Nintendo World Report piece
"segments that were needlessly hard with having to hold down the run
button and then time your jump properly". The searches found no
platforming complaint about the other candidate areas: Anima Relic 2
(Almar's Guides' chapter 5 calls the block-pushing puzzle before it
"arguably the worst puzzle in the entire game", which has no jump), maps
444-450 (Shevat's emergency shaft), the Aveh excavation site (maps 64-69,
where chapter 1 of the same guide only describes "a bunch of platforms we
have to jump up to"), the Mountain Path ("the only complicated part is the
running jump to get over the gap") and the Soylent System. One belief
disagrees with the game: a commenter in the same reddit thread holds that
"most of babel tower has fixed encounter rates/tiles", but its maps arm
the random draw (f7 periods 360, 300 and 240, one battle each), whose
steps `rand()` deals. Babel Tower and Blackmoon Forest are listed by the
rule as well; the Aveh-Kislev border mountains on two independent
complaints and a walkthrough's description, though the rule cannot decide
them (below).

| Area | Maps | Party | Maps needing a jump | Traversal jumps | Reason |
| --- | --- | --- | --- | --- | --- |
| Blackmoon Forest | 22 | foot | 1 of 1 | 3 | From the early world map's entrance (entry 0) up to entry 1, where map 23's door and the later world map arrive, the floor rises 150-250 units over a staircase of eight stumps 50-95 units across (-40 to -257 high): 3 jumps up, none down. An actor there hops up the same stones ahead of the party (`arc_jump`), and a scripted battle waits at the top. Measured; 66 jump links, 23 falls. One complaint and a walkthrough name it. |
| Aveh-Kislev border mountains | 172, 173, 175 | gears | 0 of 3 as measured | 0 as measured | Listed on player evidence. As measured unresolved: 173 and 175 are partly measured, their entrance 0 not joined to entrance 1 on its layer (175: from entrance 0, walks and up to 6 jumps reach 19 regions, the highest at y -35, but not entrance 1's at y -323; 173: no walk or jump leaves entrance 0's region), and 172 has one entrance. 241 jump links, 62 falls. |
| Babel Tower | 407, 409, 412-416 | gears | 5 of 7 | 15 | 412: 5 (climb 6), 413: 3, 414: 2, 415: 3, 416: 2; 407 and 409: 0 as measured, their entrance 1 (the doors of maps 408 and 410, which the party crosses on foot) on a strip (word 0x48) that blocks the party in its gears. 412 and 415 have pairs on two layers or not joined, so the area's figures are lower bounds. 88 jump links, 39 falls. Four independent complaints. |

Unresolved, not listed, for the user's review (disc 1; disc 2 holds the
same maps 15, 632-634 and 641-651 and no other):

| Area | As measured | Not measured | Heavy if |
| --- | --- | --- | --- |
| Anima Relic 2, maps 641, 642, 644, 650, 651 (gears) | 2 of 5 (642: 2, 650: 1), 3 jumps | 641: no walk or jump on layer 1 joins entrance 2 (map 644's door) to entrances 0 and 1 (map 643's): its two platforms, both at y -360, lie 430 units apart over slides and pits, and layer 0's floor below them is 280-360 lower. 644 and 651: entrance 1 (map 645's door, crossed on foot) is on a strip (word 0x48) that blocks the gears. | 641 needs a jump (3 of 5). The classifier's first version listed the area by walking 641's floorless triangles. |
| Maps 444-450 (foot) | 3 of 7 (444: 1, 447: 1, 448: 2), 4 jumps | 449: its layer-2 entrances 0 and 1 lie in regions layer 2 does not join, entrance 2 is on layer 0. 450: its entrances are on layers 2 and 1. 445: entrances 1 and 2 (map 447's doors) are on floorless layer-0 triangles. 446 has one entrance. | 449 or 450 needs a jump (4 of 7). Without 449 and 450 the area is heavy (3 of 5, 4 jumps), and grouped by identical encounter-set bytes so are maps 444 and 448 (2 of 2, 3 jumps). |
| Maps 64-68 (gears) | 2 of 5 (64: 3, 66: 5), 8 jumps | 64: 0->2 and 1->2 not joined. 65: entrances on layers 0 and 1. 66: no walk or jump takes entrance 0 to the others (climb 6). | 65 needs a jump (3 of 5) |
| Map 69 (gears) | 0 of 1 | its two entrances are on layers 2 and 1; 36 jump links | its crossing needs 3 jumps |
| Map 241 (foot) | 1 of 1, 1 jump | entrance 2 stands in a region of layer 0 joined to no other: 6 of its 12 pairs | one of those needs 3 jumps |
| Mountain Path, map 15 (foot) | 0 of 1 | 8 of its 12 pairs not joined; 21 jump links, climb 1 | one of those needs 3 jumps |
| Maps 239 and 259, Bart's Lair 333, the Ethos HQ 339-350, Anima Relic 1 632-634 (all on foot) | no jump but 633's one | pairs on two layers or not joined; each map's layers hold 0 to 6 jump links and its climb is at most 1 | half their maps need jumps, 3 in all |

Reviewed and not heavy:

| Area | Maps needing a jump | Traversal jumps | Why not |
| --- | --- | --- | --- |
| Goliath Factory, maps 262-266 (gears) | 1 of 5 (266: 1) | 1 | fewer than half, fewer than 3 jumps; 263 and 265 have one entrance each |
| Blackmoon Forest, maps 23, 24 | 1 of 2 (23: 2) | 2 | fewer than 3 jumps; both measured |
| Maps 546-590 (32 maps, the spec's Soylent 544-556 among them) | 1 of 32 (551: 3) | 3 | fewer than half, even if its nine partly or not measured maps each needed a jump |
| Deus, map 703 (gears) | 0 of 1 | 0 | measured: 112 jump links, none between its two entrances |

**Enemies met only in these random battles.** `--table` ends with the
(battle, enemy id) pairs some formation starts on either disc
(`formations.reach`: drawn, named or chained) that none starts once the
listed maps' draws stop: battle 32's enemy 0 (the Aveh-Kislev border
mountains, formation 2 of maps 172, 173 and 175, disc 1) and battle 59's
enemy 2 (Babel Tower, formation 6 of maps 407, 409 and 412-416 on disc 1
and 412-416 on disc 2). With random battles off there, they are not met at
all. Battle 71's enemy 4 is met only in Anima Relic 2's random battles
(formation 0 of its five maps on both discs), so listing that area would
lose it too.

Tests: `tests/test_platforming.py` (`python3 -m unittest
tests.test_platforming`, suite.sh's port stage) checks the classifier on
invented walkmeshes, scripts, area files and two invented discs: a slide
up needs a jump and its way down is a fall, floors join at any height, a
slide entered level is no stair, a jump links the sides of a valley of
slides, the arc lets a gear but not Fei land 125 up and Fei but not a gear
150 on at 100 up, a walk off falls from rest, the party's blocking bits, a
floorless triangle is a wall on layer 0 and over no floor or a blocking
one and a pit above a floor that a jump crosses and a walk falls into, a
walk through a narrow pit lands on the lower floor beyond and comes back
by a jump, the game's floor search (its winding, a negative z's borrow,
the first triangle on an edge, a triangle without area taking its whole
line), why an entrance stands on no floor, entry records, doors and fe
1e, pairs on two layers or not joined left unmeasured, path regions
without destinations, `worldmap_fixed_path_regions`, encounter keys that
ignore stages and weights, area grouping, the rule and unresolved areas,
names, a two-map dungeon classified, listed and its lost enemy found, an
area the player evidence names listed whatever its figures and an evidence
entry that is no area refused, and `--rust`, `--check` and `--map`. `encounters.rs`'s unit test checks that
each area's maps ascend and that no map is in two areas.
`xem-headless/tests/jumps.rs` (local, disc 1, about 6 s) checks on the
game that Fei's running jump on map 22 rises 118.25 and touches down 240
units on and the party's in its gears on map 414 131.25 and 204, as the
classifier's arcs hold them; that on map 22's slide 1118 the leader comes
down to the floor at its foot within ten updates with the D-pad held up
the slope and never climbs more than 8 back up it in 40 updates; and that
walking from map 15's layer-1 floor into a floorless triangle takes the
leader off layer 1 into the air and down at least 150 units onto layer
0's floor.
With the user's discs extracted, `python3 -m tools.analysis.platforming
--check` proves the shipped table: suite.sh's port stage runs it when
`.local/extract` and `.local/discs` hold both discs, and says it skips it
otherwise.

Not done or not measured: the classification is a static approximation of
the walkmesh. It measures each actor layer on its own and does not follow
the game's own moves between layers: landings in the air on the highest
floor below on any layer, the lift of a 0x4 floor to the next floor up, or
where a fall through a pit lands on another layer, so pairs of entrances on
two layers stay unmeasured. Nor does it follow the layer masks scripts set
for an actor (1f `set_layer_mask`, f8 on its layer flags), which switch a
layer's floor or its attributes off for it (the party characters of maps
239-241, 445, 446, 632-634 and most of 546-590, among others, run 1f 0x10,
which ignores layer 0's attributes; when depends on the scripts' state), the
slabs other layers' triangles stand on (a triangle's +0d extent, checked
against the actor's height), scripted lifts and moves, or the jump a
leader stuck on a slide makes after 32 frames with Jump held. It takes
heights from centroids, jumps and falls between edge midpoints and a pit's
floor below from its centroid, lets a slide end on any neighbour not above
it rather than where its push leads, finds entrances only through
immediate map-change operands and the world map's path regions (not
variables or other scripted placements), and takes the party kind for a
whole map from a reachable fe 1e, though some doors of gear maps are
crossed on foot (maps 407, 409, 644, 651). Each party's jump was measured
once, on one map, with `enter_field`'s game data (Fei, and the gears the
kernel menu's setup gives); other characters' and gears' jumps are not
measured, nor is a walk off a floor with Run held (the fall's 12 units an
update are the jumps' air speed).

### Text speed, auto-advance and movie skipping

Three options in the Gameplay section change how the game shows its
dialogue and movies, after the conveniences mods such as Perfect Works
Build offer (reimplemented here from the decompiled program; nothing of
the mod's is used and no game data is patched). Each defaults to the
original and is a recorded control:

| Setting | Values | Control | What changes |
| --- | --- | --- | --- |
| text speed (`Settings.text_speed`, `SettingChange::TextSpeed`) | Original (the default), Fast, Instant | `text_speed {glyphs}`: 0, 4, 255 | the glyphs a message window's step reveals at least (`src/port/text_speed.c`) |
| auto-advance dialogue (`Settings.auto_advance`, `SettingChange::AutoAdvance`, the whole block) | off (the default) or on, and the wait on each page: 30-600 vertical blanks, default 120 (2 s) | `auto_advance {dwell}`: the wait while on, else 0 | the session presses Confirm once a field dialogue's prompt has shown that long (`xem_core::dialogue`) |
| movies can be skipped (`Settings.movie_skip`, `SettingChange::MovieSkip`) | off (the default), on | `movie_skip {enabled}` | by the original's own early exit, Confirm ends a field movie (the story's) and Cross or Start the movie mode's (the title's and the battle event's); window movies stay the scripts' (`src/port/movie_skip.c`) |

The settings are saved by name (a file saved before they existed, or a
value no longer offered, reads the default alone) and travel in backups.
The interactive hosts give the session their values before every game
frame of a batch outside session logs, as they give it the random-battle
maps (`Game::update` for the desktop, Android and OpenXR, and the
browser's running frame loop); headless sessions, session logs and
replays play the original unless commanded.

**Text speed.** A message window reveals its text a step at a time:
`window_draw_frame` (80034888) gives `window_reveal_text` (80033DF0) the
window's speed, one glyph per field frame (two where a scene's `d6
set_text_speed` slows the field frame), and a step stops at a newline, a
page, a pause, a timed wait or a line's end. `text_speed.c` wraps
`window_reveal_text`: with the host's `xem.text_speed()` nonzero, a step
reveals at least that many glyphs. Fast (4) shows four glyphs a field
frame where the original shows one; Instant (255) shows a line a field
frame, since a step still ends at each newline. Every page, pause and
timed wait (`0F 00`, `0F 02`, `0F 0D`) the text holds stays, so the
scripts that wait on a window wait as before, after a shorter reveal;
authored slow text (`0F 0E`: a glyph every few frames, the window's
`unk88` set) keeps its pace, a higher authored speed is never lowered
(`0F 01 FF`, a line at a time, stands 1 421 times in disc 1's field
messages), and `window_render_text_line`'s one-line layout window
(`window_single_line_window`: the menus', shops', name entry's and
battle lists' text) keeps its own budget. The option applies to every
window `window_draw_frame` draws: the field's dialogue, the battle's
message window, the arena's and the world map's. Measured headless
(suite profile) on map 131's entry scene, its 22
pages turned as each prompt shows: their reveals took 3 158 frames
(vertical blanks) in all with the original's speed, 1 086 with Fast and
434 with Instant, the scene 3 808, 1 736 and 1 224 frames.

Perfect Works Build ships re-scripted variants of disc 1's maps 4, 27,
131, 288, 326, 359 and 482 for its fast text, because their scenes time
their windows against other scripts. With Instant text (and the
original's, for comparison) a scene of each plays to the same end, story
value and map, showing the same messages (timed ones among them), a
player pressing Confirm as each prompt shows. A scene the map plays later
in the story starts from `enter_field` and a `scenario` entry with the
story value, the entry parameter (`game_data.entry[2]`, the map's v0002:
the entrance a door's `change_map_entry` names) and, where the scene's
actors are the party's, the party (`game_data.party`, with
`mode_party_file_kind` 0, as `field_main` leaves it when the field ends,
so the entry loads the party's files afresh). Frames are the vertical
blanks a run played, to the next map or through 300 frames of the
player's control:

| Map (start) | Original | Instant | End | Timed messages shown |
| --- | --- | --- | --- | --- |
| 4, the opening narration (`enter_field`) | 9 731 | 9 509 | map 2, story 3 | 0, 1 (`0F 02`) |
| 27 (`enter_field`) | 1 600 | 1 060 | map 28 | 0-7 (`0F 02`; no press needed) |
| 131 (`enter_field`) | 3 808 | 1 224 | the player's control | 3 (`0F 02`) |
| 288 at story 75 from entrance 2 (map 96's door; actor 22's scene) | 5 006 | 1 786 | map 96 | 0x4F (`0F 02`) |
| 288 at story 168 from entrance 8 (map 290's door), Fei, Bart and Billy the party (actor 27's scene) | 8 336 | 2 374 | map 273, story 169 | 0x8D (`0F 02`) |
| 326 at story 153, entered with parameter 5 (actor 31's scene) | 2 937 | 2 509 | map 335 | 0, 0x23, 0x2E, 0x35, 0x38 (`0F 02`) |
| 359 at story 158, Billy, Citan and Bart the party (the Ethos's data bank; Elly and Rico the map's own actors) | 22 741 | 9 439 | the player's control | 0x8C-0x8E (`0F 0E`), 0x8F (`0F 02`) |
| 482 (`enter_field`) | 2 516 | 1 754 | the player's control | none (pages and `0F 00` waits) |

Authored slow text keeps its pace on the game: map 359's terminal
readouts (messages 0x8C-0x8E, a glyph every two field frames) took 2 102,
738 and 218 frames from their windows' opening to their first wait (or,
for 0x8C, its timed close) with Instant text, as with the original's,
while the scene as a whole took 9 439 frames instead of 22 741.

The opening narration times its pages, which is presumably why the mod
re-scripted it. Each page shows in a window (actor 12's events 5-9,
messages 2-6) while the narration's script (actor 0's event 1) counts out
the page's time with two `26 wait_countdown`s (330-450 field frames a
page, so 660-900 vertical blanks: measured headless, the count stepped
once every two vertical blanks for 197 steps), then waits for Confirm
with no prompt of its own (it calls `31 0x0020` looping on itself,
map 4 +0x1668). The original's reveal lasts past that count, so the
press that ends a page's text also ends the wait; text that ended
before the count would take that press to close the window alone, and
the narration would wait for a second press with nothing on screen.
So while a field script counts out a page this way, the `xem.text_speed`
import answers 0, the original's (`xem_core::dialogue::page_counted_out`:
an actor's running slot rests on a `26` whose next instructions are more
`26`s and then, run into, called or jumped to, a `31` test of Confirm
whose not-held branch is a jump back to it). The page reveals at the
original's pace until its count ends and at the chosen speed after, so
its prompt shows only once the script waits for the press. A player who
presses only at prompts (the table's row) reaches map 2 after 9 731
frames with the original's text, 9 563 with Fast and 9 509 with Instant,
with the same five presses; one who also presses as soon as the
narration waits reaches it after 6 729 frames with the original's text
and 6 509 with Instant.

The rule's pattern was swept in both discs' field scripts (every map's
instructions walked from each event's entry with `tools.analysis.events`,
730 maps a disc). They hold 19 waits for Confirm outside a window (a
button test of Confirm whose not-held branch comes back to it), every one
a `31 0x0020`: on disc 1's maps 4, 14, 129, 131, 132, 133, 135, 144, 153,
154, 293 (two), 296 (two) and 431, and disc 2's maps 293 and 296 (two
each). Thirteen counts lead straight into one, all on disc 1: map 4's ten
(two a page), each with its page shown, and three on maps 129 (+0x0444
and +0x046e, 30 field frames each) and 431 (+0x0424, 40 field frames),
which the counting script reaches with no window of its own open (it has
waited for its window to close with `9c`, or shown none); a window
another script showed during one of those counts would reveal at the
original's pace for those 60 or 80 vertical blanks (neither map holds a
`d6`, so its field frame keeps the two vertical blanks
`field_reset_state` gives every map). So the opening narration is the
only timed narration the rule changes. The sweep is a local script, not
in the repository (`.local/p678/scratch/p7/p77/rule_sweep.py`); it reads
the discs extracted (`.local/extract`).

**Auto-advance dialogue.** `xem_core::dialogue::prompt` reads, between
frames, what a press of Confirm would do to the field's dialogue, as
`field_dialogue_draw_windows` (8008004C) takes one: it gives a press to
every window in use, done opening, and either selected (+0x412) or the
newest. A window waiting for a press (its text's flag 8; its end, a page
or a pause) is `Waiting`; one showing a choice (`choice.status` 0), or
whose text has ended while its owner actor rests on `a9 start_choice`
(every `a9` on both discs directly follows its message instruction, so a
press there would answer, or skip, the choice before it shows), is
`Choice`. With `auto_advance {dwell}` nonzero, after each frame the game
finishes the session counts the vertical blanks a `Waiting` prompt has
shown (nothing while the game is paused); at the dwell the next frame
holds Confirm on pad 1 over the host's buttons for one vertical blank, as
the player's press would, and the count starts again. A `Choice` never
counts, so a choice waits for the player however long it shows, and the
player's own presses advance as before. The dwell is game time: under
fast-forward it passes with the game's frames. The dwell counts from when it
changes, a recording or a replay starts or a state is restored, so the
presses follow from the game's memory and the recorded dwell alone, and a
replay presses where the recording did. Auto-advance reads the field's
dialogue windows only: battle messages, and the scripts' waits for
Confirm with no window open (the sweep above finds 19), still take the
player's press. Measured
headless on map 482's entry scene (8 pages): a player pressing at each
prompt reaches control after 2 516 frames, auto-advance at 30 vertical
blanks a page after 2 740, at the same end and story value with the same
messages.

`AppAction::ToggleAutoAdvance` ("Toggle auto-advance dialogue", T or
Back+West by default, rebindable in Shortcuts) turns the setting on or
off through the settings service and says which in its message.
Settings saved before the action existed (every saved file and browser
`xem.settings` holds the whole shortcut list) read its default binding
(`Settings::read`): T and Back+West, less a chord the saved list gives
another action, which stays that action's; an action unbound in
Shortcuts (an empty binding) stays unbound.
Touch screens and headsets have AUTO beside the picture in a field's
scenes and dialogue (`Context::Event` and `Context::Dialogue`): the touch
deck shows it above the face buttons, a headset beside the screen's right
edge (`overlay::side_keys`, where no control deck holds the keys there),
lit while auto-advance is on; a touch, or a tap or pinch aimed at it, is
the same app action, never a press of the game's.

**Movies can be skipped.** The original ends a movie early only where its
request lets it. A field movie whose request has fade bit 0x80
(`field_movie_fade_bits`) ends on Confirm in `field_movie_play`
(800A7C58): the loop drains the pad, fades the CD volume, waits five
vertical blanks and closes the movie as at its end. The movie mode's (6)
ends on Cross or Start unless its request kept it
(`movie_mode_read_playback_input`, 800769A4, `movie_mode_unskippable`).
The story's field movies (`fe 60` with mode 2, `fe a0`) are requested
without the bit and the battle events' movie is kept; on disc 1 only map
490's (the title's attract movie) and map 728's requests have the bit.
`movie_skip.c` wraps
`field_movie_run_frames` (800A732C) to set bit 0x80 as a field movie's
frames run and `movie_mode_read_playback_input` to clear the keep flag,
while the host's `xem.movie_skip()` is nonzero, so the player's press
takes the original's own early exit and its cleanup. Window movies (field
movie mode 2, `fe 67`), which the field's scripts end, stay as they are.
Armed, a field movie's loop drains the pad queue on each pass, which
changes the game's memory, hence the recorded control; a press held
across a movie's start (Confirm repeats after 32 vertical blanks) ends it
as it would end the movies the original lets the player skip. After the
early exit the loop's cleanup is the natural end's (`movie_close` stops
the stream and silences the CD input, as a movie's last frames do);
`field_movie_frame` keeps the frame the movie reached, which only `fe 85`
reads, on map 490 (the title's attract movie, skippable already) and map
665 (a window movie). Measured headless on map 25's scene (from
`enter_field`): its movie (#7, 1 059 frames) ignored Confirm and played
4 386 frames to its frame 1 059 without the option, map 23 coming 13
frames later; with it, a press 120 frames in ended the movie at its frame
21, 241 frames after it began, and map 23 came 13 frames after that; 600
frames into map 23 both runs are in the field with the same story value.

**Agents** change the settings through the settings service as people
do (`{"cmd":"settings_apply","change":{"TextSpeed":"Instant"}}` on
`xem-desktop --control`; `window.xem.settings.apply({AutoAdvance:
{enabled: true, dwell: 90}})` in the browser), and where no host gives the
values they set the controls directly: `{"cmd":"text_speed","glyphs":
255}` (0-255), `{"cmd":"auto_advance","dwell":120}` (0 or up to 3 600)
and `{"cmd":"movie_skip","enabled":true}`. `status` reports `text_speed`,
`auto_advance` and `movie_skip`; `readiness` lists the three commands,
session logs may schedule them, a recording begun with any of them on
starts with it, and a replay starts from the original's and keeps the
recorded values over the host's. `status` also reports what the options
act on, as the game shows it. `dialogue` is what a press of Confirm
would do to the field's dialogue now, read as auto-advance reads it
(`xem_core::dialogue::prompt`): `{"kind":"none"}`,
`{"kind":"waiting","wait":n}` (its text's end 1, a page 2, a pause 3) or
`{"kind":"choice"}`; Fast and Instant text make a page's prompt show
sooner. `auto_advance_waited` is the vertical blanks the shown prompt has
waited toward the dwell (0 while auto-advance is off, at a choice and
after each press). `movie` (`xem_core::movie`) is the movie whose
playback loop reads the press, or `null`: its `kind` (`field`, a story
movie; `window`, a window movie; `mode`, the movie mode's), the `frame` it
shows and `ends_on_press`, whether a press now would end it by the
original's early exit. A field movie shows from its first picture until
its loop closes the movie library's player (`movie_close`): before the
picture the player waits for its first frame in a loop that reads no pad,
and after the loop the field reloads what the movie displaced. Its loop
reads the press from the game's pad queue, which keeps 16 vertical
blanks' pads and drops the rest, so a press counts only while the queue
has room: before the first picture only in the first 16 vertical blanks
after the field last read the pad, and on the first picture after a
longer wait not until the loop's first read empties the queue.
`ends_on_press` follows the request's fade bit 0x80 (or the debug monitor
present) and the queue's room, and is false while the staff roll draws
over the movie, while the scripts' input mask holds Confirm back and once
the loop has taken a press, after which the movie fades for five vertical
blanks and closes. On the loop's first pass the taken press is the
field's last read before the movie (`field_pad_port0_repeated` still holds
it), so a Confirm the field read just before the movie began makes
`ends_on_press` false for that pass, at most 2 frames.
The movie mode's shows from its player's start (`frame` -1 until its
first picture) until the player stops, as its loop reads the pad from the
start, and not while the mode loads the movie library; its
`ends_on_press` is false where its request kept it and once its end
counts down. Which movie the player plays is its frame callback, the
field's or the movie mode's, in the movie library's memory, which the
library's load zeroes. The option turns `ends_on_press` on for the
story's movies as their frames run.

Tests: `xem-settings`'
`text_speed_auto_advance_and_movie_skip_default_to_the_original_validate_and_persist`
checks the defaults (the sessions get 0, 0 and off), the glyphs and
dwells the settings resolve to, a reload, the dwell's bounds, that off
keeps the dwell, an agent's change, a backup, and that an old file or a
value no longer offered reads the default alone. `xem-core`'s
`dialogue` unit tests read invented windows: a page's, an end's and a
pause's wait take a press; a shown choice, or an end whose owner rests on
`a9`, never; revealing text, a menu, another mode or a window still
opening are no wait. The `a9` reading rests on the sweep of both discs'
field scripts made for the cut cutscene skip (`tools.analysis.events`;
`.local/p678/gap/p7-cutscenes.json`, local): all 1 536 `a9` instructions
directly follow their message instruction (`d2` 1 225, `f5` 177, `fc` 97,
`d4` 37), and every message instruction is followed by `9c` or `a9`. The
same unit tests read invented scripts: an actor resting on a count that
calls, runs into or jumps to a `31` wait for Confirm counts out a page,
and the wait itself, the call after the counts, an ended slot, another
mode, a wait for another button, a test whose branch goes on or a count
before `9c` do not. `xem-core/tests/persistence.rs` checks on the
stand-in module, whose wrappers ask the imports every frame and whose
dialogue window ends its wait on a frame holding Confirm:
`the_text_speed_and_movie_skipping_are_recorded_and_a_replay_keeps_them`
(each change once, the replay's start from the original's, the host's
values ignored while it plays, a recording begun with them, `schedule`,
`readiness`, refused values and refused recorded controls) and
`auto_advance_presses_confirm_once_a_page_waited_its_dwell_never_at_a_choice_and_replays`
(no press while off; Confirm for one frame after the dwell and released
after it; the next page's own dwell; none at a choice in 60 frames or
while paused; a recording's presses replayed on the same frames over a
host's other dwell), and
`status_shows_the_prompt_auto_advance_counts_toward_and_the_movie_a_press_would_end`
(`dialogue` waiting, `auto_advance_waited` counting toward the dwell and
back to 0 after the press, a choice, and a field movie that shows once the
movie library's player streams it and whose `ends_on_press` follows its
request's fade bit). `xem-core`'s `movie` unit tests read invented memory:
a field movie ends on a press only when its request has the early exit or
the debug monitor is present (Triangle a window movie's); it shows from its
first picture until its player closes, and not before its request, after
its end, in another mode or while the movie library holds another movie's
player or none; its loop takes no press under the staff roll, once it has
taken one, through the scripts' mask or with the pad queue full; and the
movie mode's shows from its player's start until it stops and ends on a
press unless its request kept it or its end counts down. `tests/test_port_text_speed.py` and
`tests/test_port_movie_skip.py` (suite.sh's port stage) build the two
wrappers natively with `-Wall -Werror` against an invented host: a speed
raises a smaller budget and never lowers one, slow text and the one-line
layout window keep theirs; a field movie gains bit 0x80 only when armed
and never in mode 2, and the movie mode's keep flag clears only when
armed. `xem-host`'s
`frames_give_the_session_the_settings_text_speed_auto_advance_and_movie_skipping_and_a_session_log_the_original`
checks that `Game::update` gives the session the original's values by
default, the settings' (4, 90, on) once chosen, and a session log the
original's whatever they say. `xem-assist`'s touch and headset tests check AUTO: in scenes and
dialogue only, clear of every other control, a touch or an aimed tap or
pinch the app's toggle (never Confirm), lit while on; `xem-host`'s
`touch_controls_hold_their_buttons_taps_on_the_picture_are_kept_and_auto_asks_the_app`
checks that a finger on the desktop and Android view's AUTO in a field's
scene queues the app's toggle, one request a touch, holding no button of
the game's. In a headset on Monado, the xr tier's smoke loads field 12's
dialogue from a state xem-headless saved at its prompt and aims a pinch at
AUTO beside the screen's right edge (screen pixel 336/108 on the 3 m
screen): the app's toggle turns auto-advance on (the settings' and the
session's dwell 120), the prompt still waits some 45 vertical blanks
later and has counted toward the dwell on every one since the toggle (a
press of the game's would have turned the page and restarted the count),
and auto-advance turns the page after its dwell. `xem-ui/tests/panel.rs`
checks that the Gameplay section's three options apply through the
service, are acknowledged, show the wait only while auto-advance is on,
keep a refused wait and follow an agent's change.
`xem-headless/tests/text_and_movies.rs` (local, disc 1; about 90 s here
at four tests at a time) plays the table's scenes with the original's and
Instant text to the same end, story value and messages (the narration
also with Fast text, each with the original's five presses, and pressing
too as soon as it waits), checks that map 359's slow
readouts take the same frames with either, measures map 131's reveals at
the three speeds (each page no slower, the totals strictly faster), plays
map 482 with auto-advance to the pressed run's end, turns map 172's Repair
Bot's pages by auto-advance to its four choices and checks that the
choice still waits unanswered 1 200 frames later (no battle) and that the
player's own third choice then starts the bot's battle, ends map 25's
movie on Confirm only under the option and runs on into map 23 to the
same story value (reading `status.movie` before the press), reads
`status.movie` frame by frame from boot (below), plays the
game's opening to Fei's room with the defaults and with every option on
([Gameplay options](#gameplay-options)), and replays a recording that
turned all three on (and changed the dwell) on map 131 to the recording's
memory hash. From boot with no other input
(`status_shows_a_movie_while_its_loop_reads_the_press_and_a_press_it_says_ends_it_does`,
about 7 s here): the movie mode loads the movie library for 73 frames
with no movie shown; the boot's movie shows 372 frames in, at its player's
start, and Start then ends it, while Start on the frame before changes
nothing (the movie plays on 960 frames to its end). The title's attract
movie waits 38 frames for its first picture with no movie shown, shows
`ends_on_press` false on the picture and the next two frames, while the
queue that wait filled drops a press (Confirm on the picture changes
nothing), and true from the loop's first read; Confirm then ends it at
its frame 3: the loop reads the press a frame later, the movie fades 6
frames with `ends_on_press` false, and the field's 90 frames of reload
show no movie. The slice
logs replay to their recorded hashes (`xem-headless/tests/slice.rs`). A
browser test (`persistence.test.mjs`, the fixture module) checks that the
running page gives the session the original's values, then half
encounters, Instant text and movie skipping from `xem.settings.apply`,
and that T turns auto-advance on (dwell 120, saved in `localStorage`) and
off again; another (`page.test.mjs`, the fixture module reporting the
field's mode) that AUTO shows on the touch deck in a field's scene and
that touches on it turn auto-advance on (the session's dwell 120) and off
again through the settings, pressing no button of the game's.

Not done or not measured: the battle event's movie (the movie mode's,
event set 3) under the option was not reached on the game (the wrapper is
unit-tested). Auto-advance does not turn battle messages or the scripts'
waits for Confirm with no window open; the cut cutscene skip's
battle-message advance ([below](#cutscene-skipping)) is not offered either. Fast and
Instant text was compared on the scenes above only, and not every timed
message of their maps shows in them: map 27's message 8, map 131's
message 9, eleven of map 326's sixteen (1-7, 0x12, 0x17, 0x1E and 0x3F),
and four of map 288's six: 0x2F, in actor 14's talk at story 153-154,
which `interact` found no path to from entrances 0, 9 and 10 (entrances 1
and 6 open on a scene); 0x38 and 0x39, which actor 16 has actor 15 show in
turn, again and again, at story 153-154 while the map's local v0402 is 0
(seen in a probe; no end to compare); and 0x69, in the talk with Billy's
stand-in (actor 21, story 155-159, Bart in the party) after a choice,
which the runs never answer. Authored slow text was compared on the game
on map 359's readouts with Instant text only; that Fast keeps it too is
checked by `tests/test_port_text_speed.py` alone. The sweep looks for the
narration's one pattern (a count straight into a wait for Confirm); a
scene that times a window against another script some other way was
compared on the game only on the maps above, and the battle's, arena's
and world map's windows were not measured under Fast or Instant text.
No HUD chip shows that auto-advance is on (AUTO's light
does, on touch screens and in headsets).
Of the mod's options that change the game's balance only half encounters
is offered ([above](#random-battles-in-platforming-areas), the field's
draw only); more EXP and gold is not. The
browser tests ran on the fixture module; the Android and touch hosts give
the values through the same `Game::update` and frame loop, and no phone
run used them on the game. In headsets the options' defaults and an
agent's change were read on Monado and IWER, and the AUTO key was aimed at
on Monado only (no IWER run aims at it); beyond the page auto-advance
turned there, no headset run played text speeds, auto-advance or movie
skipping on the game.

### Cutscene skipping

The Phase 7 step that built the gameplay options asked for cutscene
skipping that keeps the story's outcome: the original scripts run
unthrottled, Confirm is pressed at the dialogue waits, and the skip ends
at the next moment the player controls. The user made it conditional on
the game marking that moment clearly: the skip stops at the first frame
the marker is set and lands at the real end of the scene on every tested
scene, with no frame count or other heuristic. The gate found no such
marker: the game's control grant, its only record of that moment, is not
one. So the feature was cut and removed from plan.md (`70fb543`), and the
runtime has no cutscene skip. The measurements follow. (A movie's end is
such a moment: movies end on Confirm under
[Movies can be skipped](#text-speed-auto-advance-and-movie-skipping), and
text shows faster and dialogue goes on by itself under the options beside
it.)

**The candidate marker.** The field's only record that the player controls
the character is its control grant, `field_player_control_polled`
(0x800ADB68): `field_event_run_all_actors` clears it at the start of every
event pass, and the controlled actor's `a7`
(`field_event_request_player_control`) sets it, reading the pad, when no
choice is open and `field_work.encounter_inhibition` is 0. A scene holds
the field through `fe 54` (`take_script_control`: the inhibition -1 and
both `script_control` bytes 1) or `15`, and lets it go through `fe 53` or
`14`. The candidate: the field mode, the grant set, the inhibition 0, both
script-control bytes 0, no menu requested (`field_menu_request` 0xFF) and
no dialogue window open (`field_work.open_windows` 0).

**Measured** headless (`xem-headless --control`, suite profile, at 1x)
from each scene's start: Confirm held for one vertical blank and released
for one only while a dialogue window waits (flag 8, wait state 1-3, no
choice shown, the owner's next opcode not `a9`) or an actor rests on a
`31 0x0020` loop; a choice answered with its first option, or with its
last once an answer has opened a menu (as `tools/memcard_saves.py`
answers), battles fought with Cross, menus left with Circle; the marker
read every vertical blank. A scene's real end is where the marker then
holds without input to the end of the run (1 800 vertical blanks for the
new game, 3 600 for the story routes, 600 for the two map openings).
Frames are vertical blanks since boot. The story routes start as
`tools/memcard_routes.json`'s saves do, from the user's first save patched
to the route's start and loaded through the title's Continue
(`tools/memcard_saves.py`).

| Scene (start) | Maps | Marker set | Real end |
| --- | --- | --- | --- |
| New game to Fei's room (disc 1, the title's New Game) | 490 (the title: movie 13, 2 057-20 139), 4 (the narration), 2, 14 (a battle 31 919-35 562) | 37 462 | 37 462 (map 14, story 6) |
| Kislev (`kislev-capital`: story 96, map 213) | 213, 218, 199 | 9 526-9 617 on map 218, then 15 853 | 15 853 (map 199, story 105) |
| The pirates' lair (`pirates-lair`: story 54, map 80) | 80, 79, 74 | 13 110 | 13 110 (map 74, story 56) |
| Shevat (`sky-city-shevat`: story 174, map 421) | 421, 420, 273, 437 | 6 998-7 107 on map 420, then 15 047 | 15 047 (map 437, story 176) |
| Into Solaris (`into-solaris`: story 207, map 453) | 453, 456, 452, 466 | 10 912 | 10 912 (map 466, story 209) |
| The soul vessel (disc 2, `soul-vessel`: story 258, map 622) | 622 (a choice at 7 279: its first option opens the field menu, its last goes on to a battle 7 783-14 190), 618, 273, 618, 628 | 34 393 | 34 393 (map 628, story 263) |
| First and last (disc 2, `first-and-last`: story 330, map 687) | 687 | 4 135 | 4 135 (story 340) |
| Map 286's opening (`enter_field`, story 147) | 286 | 7 193 | 7 193 |
| Map 197's opening (`enter_field`) | 197 | 2 676 | 2 676 |

In two of the seven story scenes the grant comes in the middle, each time
on a map a scripted map change has just entered, while that map loads its
music (and, on map 218, a party member's sprite file). On map 218
(map 213's scene changes to it at 9 419) the scene's director (actor 0's
event 1, at +0x017D) fades the screen out (`b4 fade_out #1`) and adds
party member 5: `fe 18 join_party_byte` waits until the stream of the
map's music (track 0x2f) has stopped and the disc is idle (9 526-9 583)
and starts reading the member's sprite file, and
`fe 1a apply_party_sprite` waits for that file (9 584-9 615). Only then
does the director take the field (`fe 54` at +0x0187, frame 9 618): the
player's `a7` grants control for the 92 vertical blanks between (about
1.5 s), and the scene goes on with dialogue from 10 418 to 12 346, moves
to map 199 at 12 554 and talks on there until 15 847; control comes at
15 853. On map 420 (map 421's scene changes to it at 6 698) actor 18's
event 1 (+0x49CA) fades out and runs `fe 0e music_fade #0, #0`, which
waits while the map's music (track 0x15) is selected but has not started:
its stream loads from 6 998 to 7 065 and the music starts at 7 106, when
`fe 0e` fades it to level 0 over 0 frames, silencing it at once. The
scene takes the field at +0x49DB (frame 7 108) after 110 vertical blanks
of grant; with Up held the leader walks from (x, z) (-2184, 256) to
(-2048, 280) in the dark before the scene takes over again. A skip
stopping at the first frame would stop 6 327 and 8 049 vertical blanks
before these scenes end. The runs are deterministic: Kislev's again from
its start gives the same frames.

**Refinements.** Both early grants come with the screen faded out
(`field_faded_out` 1, fade channel 0 at full), and also requiring the
screen faded in (`field_faded_out` 0) passes all nine scenes above. It
does not make the marker clear, because the scripts also let the field go
in the middle of a scene with the screen shown. A sweep of both discs'
scripts (the `tools.analysis.events` walk) finds `fe 53` or `14` followed
on the same flow by `fe 54` or `15` 44 times on 30 maps, most behind
exclusive branches on variables or trigger zones. Two let go and take the
field back unconditionally, each between messages of one scene: map 197's
zone scene (actor 26's event 1, +0x0AF9: let go, `wait_countdown #300`,
which the slot's 8-bit countdown keeps as 44 field frames, take the field
again, two more messages, a map change) and the talk event of map 286's
actor 7 (+0x05DE: let go after three messages, waits of 24 and 45 field
frames around an animation, take the field again, let go, a fourth
message). With the player's actor in its `a7` loop there, as it was
through both measured early grants, the grant, refined or not, is set for
about 90 and 140 vertical blanks (two a field frame) while the scene still
runs. Nothing the game keeps tells those windows from a scene's end:
telling them apart takes a time threshold or reading ahead in the scripts,
the heuristics the direction rules out. (`tools/memcard_saves.py`, which
plays routes to a story value, takes the field's control as the player's
only after 16 checks 8 vertical blanks apart for the same reason.)

Tests: nothing is implemented, so no test covers this section. The frames
and the sweep come from a local harness that is not in the repository
(`.local/p678/scratch/p7/gate/`, with the soul vessel's run and the probe
of the two waits in `fix3/` beside it). It drives `xem-headless --control`
one vertical blank at a time through `tools/native_scenario.py`, builds
each route's start with `tools/memcard_saves.py`'s functions, reads the
waits and holds Up from states saved just before the early grants, and
walks both discs' scripts with `tools.analysis.events`. It needs discs 1
and 2 (the sweep reads them extracted, `.local/extract`) and the first
save on the user's memory card 1, read from a copy.

Not done or not measured: the scenes at map 197 +0x0AF9 and map 286
+0x05DE were not reached on the game (at `enter_field`'s story, map 197's
trigger zones lie beyond a closed door and map 286's actor 7 is disabled),
so their windows are read from the scripts; the sweep follows one actor's
flow at a time, so a scene one actor lets go and another takes back is not
counted. The early grants were measured headless only; the browser and the
other hosts were not run.

## Desktop host and settings panel

`xem-desktop` opens an SDL3 window (Wayland or X11), presents through wgpu with
FIFO vsync and draws the game (`--disc DISC [--cards DIR] [--bios FILE]`,
without `--bios` the BIOS image Import BIOS keeps beside the settings, if
any; the test scene without a disc), placed by the presentation settings, under the
Slint settings panel (Escape or a gamepad's Back+Start toggles it). The
panel is the launcher too: Saves' Import disc… opens SDL's file dialog and
plays the chosen disc (CHD, cue sheet or Redump-verified ISO), or gives it
to a running game's disc changer.
The panel (`xem-ui/ui/settings.slint`) lists its sections in a sidebar at
560 logical pixels wide and more, else in a strip of tabs above the section.
A tab of the strip takes its label's width (bold, as when selected) and
shares out the rest while the strip has room for all eight: from 390
logical pixels with the panel's font (the panel on the Android emulator's
1080-pixel screen at density 2.625 is 405). A narrower strip takes the
same width from every tab, so the labels still show whole down to 342 (a
1080-pixel phone at density 3.0 gives 355) and elide below, and no tab
narrows below 24 pixels: all eight stay inside the panel down to its least
width, 280. `xem-ui/tests/panel.rs` clicks every tab at six widths from 280
to 405 and checks that the Android smoke's two taps select Graphics and
Saves at 405, and that Saves' Backups card keeps all of its buttons
inside it at ten widths from 280 to 640 (one to a row below 600 logical
pixels, where two of its longer labels side by side overflowed it: at 560
"Request browser persistence" ran past the card's edge). That button and
its shortcut are the browser's alone (`SettingsUi::set_browser_storage`):
a native host keeps its saves in application files, so its panel offers
ten. Each section is a column of cards of settings, some under a heading
outside the card (Graphics' Texture Filtering and VR, Controls' VR /
Touchscreen and Global, Time's Rewind and Fast-Forward). An error bar under
them shows only while something failed (a refused change, an operation, the
game stopping, a disc the game asks for) until a change or an operation
succeeds or its Dismiss closes it; a success says nothing there (agents'
`{ok, message}` and the hosts' operation lines still say what it did). A
button group's option of several words wraps them on a panel too narrow for
its line. About shows the app's version (`xem_ui::VERSION`: a release's
`XEM_RELEASE_VERSION` without its `v`, else the workspace's). Controls edits
the game's bindings ([Controls](#controls)) and the assisted controls
(hand gestures, the headset pointer, Stop at ledges, touch controls,
reduced transparency and motion); Shortcuts lists every app operation with
its bindings and edits the chosen one in place (Recenter headset view among
them, with no default binding: the headset card's Recenter button and the
automation reach it; Toggle first-person view, V and Back+R1, last);
Gameplay holds the options
that change the game itself ([Random battles in platforming
areas](#random-battles-in-platforming-areas); [Text speed, auto-advance and
movie skipping](#text-speed-auto-advance-and-movie-skipping)). Cancel closes either binding
editor without saving its changes. The header's Reset sends the shared
`reset` command. The header's Quit (desktop,
Android, a headset; not the browser) asks first: Cancel, a press beside the
question, Escape or Android's Back keep playing, and hiding the panel drops
the question. A confirmed Quit ends the run as closing the window does
(Android's activity finishes; a headset ends its session, then the app).
Graphics' Fullscreen (desktop, off by default, saved) asks SDL for the
whole display at start and whenever it changes, never otherwise, so a
window manager's own fullscreen is left as it is. `cargo run -p xem-ui
--example panel` draws each section on a headless device for review
(`build/validation/panel/`; `--desktop`, `--click`, `--hover` and
`--locate` show the desktop's controls, the question, hover states and
where elements lie for tests that tap the panel).
One `xem_render::Gpu` (instance, adapter,
device, queue) serves `SceneRenderer`, `Compositor` and `xem_ui::SettingsUi`;
Slint runs as a custom platform under the host loop and renders into a texture
only when it asks for a redraw. Settings live in
`xem_settings::SettingsService` (`$XDG_CONFIG_HOME/xenogears-ex-machina/settings.json`
on the desktop); the panel and agents change them only through `apply`.
A saved setting that does not read (a value no longer offered) or keeps the
whole from validating takes its default alone, the others staying as saved,
and an action with a default binding that the saved shortcut list lacks
takes that binding, less any chord another action holds
(`Settings::read`, also for imported backups).
With a disc, the game runs paced by the wall clock at 60 frames a second (a
stalled host drops time rather than catching up), pad 1 follows the keyboard,
the mouse (held in relative mode while the game has the input, only under
the Controls section's mouse camera setting, off by default) and the first
gamepad, the GPU's display is uploaded as the game image after
each frame and the SPU's output is queued to an SDL3 audio stream at the
master volume (`xem_host::Game`), as the [Binaural sound
mode](#binaural-sound-mode) presents it when it is on. Memory cards default to `cards/` beside the settings file.
Pad 1's buttons come from the game bindings ([Controls](#controls)).
Back+Start toggles Settings once and consumes
both buttons until both are released. Guide/Home has no app binding.
Walking in a field, Select (Backspace, or Back released alone) toggles the
first-person view where the original's Select shows no map's picture, and
the right stick's click and Toggle first-person view (V, Back+R1) anywhere
in a field but its menu; in it the arrow keys, the
right stick and the mouse (under the mouse camera setting) look around
([First-person inspection](#first-person-inspection)).
`--control` answers agents' JSON commands on stdin, a line each, between
frames while the game plays: the command layer's (`status`, `context`,
`navigate`, `act`, ...), `settings`, `settings_apply {change}` (the panel's
own typed, validated, persisted change; `SettingsService::command` answers
both), `panel {visible}`, `first_person {on, yaw, pitch}` (the view on, off
or toggled, then turned: [First-person
inspection](#first-person-inspection)),
`import_disc {path}` (the launcher's path), `bios_import {path}` (Import
BIOS's: kept beside the settings for every boot and given to the running
game) and `app {request}` (the panel's and the bindings' operations as they
run them, the file pickers and Toggle first-person view refused: [Saves and time
controls](#saves-and-time-controls)); nothing passes through the window
system. With `--xr` the headset session answers the same commands but
`import_disc` (`xem_host::Control` and `SettingsService::command` serve
both), its `app` Recenter recentring the view.
`xem-desktop/tests/control.rs` runs the desktop itself on weston's headless
backend with lavapipe (local, disc 1), changes the gameplay options
through `settings_apply` ([Gameplay options](#gameplay-options)), holds
rewind, retries card writes and reads the storage line through `app` and
`status`, and looks around field 30 by `first_person`.
`--script LOG` plays a session log instead of the player's input (its pad
states replace the keyboard's) and `--exit-at-frame N` prints the memory hash
when the game has run N frames and exits: the slice's logs end at their native
hashes in the desktop host too, the load from card 2 in a fresh process after
the save. `--load-state FILE` loads a state (Save state's, `xem-headless`'s
`state_save`, a browser's export) once the game has booted, before the
window or the headset plays it, as Load state does (a refusal ends the run
with its message), and `--save-state-at N:FILE` writes the game's state to
FILE when it has run N frames (the game's count since boot, which a loaded
state goes on from; the frame limit holds the game at N until the state is
written), printing `xem-desktop: state saved frame N memory_hash H path
FILE` (in a headset `state saved frame …`), and plays on to
`--exit-at-frame`; both work in the window and with `--xr`, for tests of
[State interchange](#state-interchange) (`xem-desktop/tests/states.rs`,
`xr-game-smoke.sh`).

`./play.sh [DISC] [xem-desktop options]` (repository root) builds what is
missing or stale (the matched images, the game module, `xem-desktop`) and plays
disc 1 by default on the host's NVIDIA driver.

From `src/runtime/` in `nix develop path:./nix/runtime`:

- tests on lavapipe: `VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD cargo test -p xem-settings -p xem-render -p xem-ui`
- on wgpu's GL backend (Mesa's llvmpipe through EGL; GL's clip space, as
  WebGL2 and GLES draw): `WGPU_BACKEND=gl __EGL_VENDOR_LIBRARY_FILENAMES=$XEM_MESA_EGL
  LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe cargo test -p xem-render --test ps1`
- on the host NVIDIA driver: `scripts/xem-gpu-host target/debug/xem-desktop`
  (`--show-settings --exit-after-frames N --screenshot out.png` for a smoke run;
  `XEM_FRAME_STATS=1` prints a line a
  second of the frame loop's time per stage: settings, game frames, taking
  the trace, acquiring, rendering, presenting)
- Android, in `#android`: `cargo ndk -t arm64-v8a -P 26 build -p xem-ui -p xem-render`
  (Slint compiles its FemtoVG module out on Android, so `xem-ui` takes the
  renderer from `i-slint-renderer-femtovg` there)

## Browser host

`xem-web` (wasm32-unknown-unknown, wasm-bindgen) and the thin ES-module page in
`src/runtime/web/` are the browser host; everything structured crosses the boundary
as JSON text or bytes.

- **Two modules.** `game.js` compiles `game.wasm` and instantiates it with one
  forward per `xem` import, `(...args) => xem_game_import(index, args)`. Like
  xem-game's native glue, the forward stops a rewind and returns the resume
  value, or asks the runtime's handler, which answers with a value, an unwind
  (the forward calls `xem_unwind_area` and `asyncify_start_unwind`) or a trap
  (a thrown error that leaves the game's frames). `WebModule` implements
  `GameModule` over the instance's exports; game memory is reached through
  copies from its `WebAssembly.Memory`. The module's 2 GiB memory (RAM at its
  KSEG0 address) is a reservation the browser commits as pages are touched.
  `boot` takes a fresh instance each time.
- **Commands.** The session is `xem_core::control::Session`, the command
  layer `xem-headless --control` serves: `XemApp::command` takes the same JSON
  commands (a command runs at most 10 000 frames per call), so a client drives
  either host identically; disc 1 answers the same replies in both.
  `XemApp::snapshot_bytes`/`restore_bytes` carry game state bytes for
  saves. `boot` starts the game as `xem_game::boot` does natively (the
  executable's text, the Kanji font of the BIOS image IndexedDB keeps or the
  substitute glyphs, the memory cards, the user's disc in the drive read in
  place through `DiscImport::game_source`) and takes the game's `stubs.txt`
  and `schema.json` from beside `game.wasm`. The page hands the runtime that
  image before any boot (`set_bios`; one it refuses is reported,
  `status().page.biosNotice`); Import BIOS checks a picked file
  (`check_bios`), writes it to IndexedDB, then sets it, and the automation's
  `bios.import(bytes)` and `bios.stored()` do and read the same.
- **Blocked frames.** The page reads the disc only asynchronously, so a frame
  whose sector is not fetched yet stops (`Session::frame` returns `Ok(false)`)
  without advancing the virtual clock, and continues where it stopped once the
  bytes arrived. The animation loop fetches the missing ranges after each
  animation frame (`disc_missing`, `disc_fill`) and counts the waiting time as
  dropped. A `frames` or `run_until` command returns early with
  `{"blocked": true, "frames": n, ...}`; the automation API's `run(command)`
  and `runFrames(n)` fetch and repeat until the command is done and answer
  what `xem-headless --control` answers.
- **Scheduling.** `requestAnimationFrame` calls `XemApp::frame`: while running,
  the player's input on pad 1, the game frames due at 60 per second of page
  time, at most four per animation frame times the fast-forward factor
  (`MAX_FRAMES_PER_ANIMATION`) and none after 12 ms, then one render;
  the page then posts the frames' sound and card writes. Time a throttled,
  hidden or stalled page missed is dropped (`status().loop.dropped`), never
  caught up.
- **Rendering.** wgpu on the canvas: WebGPU when the browser gives an adapter,
  WebGL2 otherwise (`?backend=webgl2` forces it); `status().renderer.backend`
  says which, and `status().renderer.picture` where the game's picture lies
  on the canvas (canvas pixels) and the screen it shows (its display size
  and the wider framing's sides, none for a rebuilt movie frame);
  `status().renderer.movie` whether the last frame was a rebuilt movie
  frame ([Movies](#movies)). With a session the canvas shows the GPU device's display
  (`display_rgba8`, uploaded to an sRGB texture after frames ran and placed by
  the presentation scale and filter, as xem-host's `View::set_game_image`);
  without one, the test scene. The Slint panel (FemtoVG's wgpu renderer on the same device,
  composited over the scene) works on both; Escape or the Settings button
  toggles it, and Close hides it. With the mouse camera setting the game holds the mouse with pointer lock
  after a click while play can take camera input. Game menus, battle inputs
  and pause release it without opening Settings; the browser's Escape
  that releases it during play opens the panel. A lost device is replaced on a new canvas (WebGL2
  after repeated WebGPU losses).
- **Disc import.** A file input (and, where available, the File System Access
  API, whose handle is kept in IndexedDB for "Reopen") gives a `File`; the image
  is read in place over `PrefetchedFile` in 256 KiB chunks with an 8 MiB chunk
  budget and a 4 MiB decoded-hunk budget, whatever its size. Identifying disc 1
  reads 1.1 MB of its 358 MB CHD. CHD and raw MODE2/2352 `.bin` and `.iso`
  images are accepted (a `.cue` is not needed for a single-track image),
  each verified as Redump's dump ([Discs](#discs)). Nothing is sent
  anywhere. Each import has an id that its byte ranges carry
  (`open_disc`, `disc_poll`, `disc_fill`, `disc_missing`); the verified
  Xenogears discs among them are the game's (`status().discs`, and
  `status().drive`: the disc in the drive and one the game waits for), the
  newest import is `status().disc`, and "Reopen" reopens both remembered
  discs. The panel's *Import disc…* opens the page's file chooser.
- **Audio.** A browser session keeps the SPU's output (`Spu::set_output`).
  After the frames of an animation frame (or a command) the runtime takes the
  samples into a bounded queue (a quarter second; older ones are dropped, as
  when automation runs frames faster than real time); each animation frame the
  page takes them at the master volume (`XemApp::take_audio`; the
  [Binaural sound mode](#binaural-sound-mode) presents them when it is on),
  and posts them to an `AudioWorklet`, whose ring keeps
  at most about 0.19 s and plays silence when dry (no SharedArrayBuffer).
  "Start audio" creates the `AudioContext` inside the click, at the SPU's
  44.1 kHz; the browser resamples to the device.
- **Input.** While the settings panel is hidden, the keyboard
  (`KeyboardEvent.code`), the mouse and the first gamepad (Gamepad API
  standard mapping; Back+Start toggles the panel) hold pad 1 through the
  game bindings ([Controls](#controls)); touch pointers play the touch
  deck and tap the picture ([Touch screens](#touch-screens)); the mouse
  points at menu entries. In a field, Select's keys, the deck's SELECT and
  the gamepad's Back alone toggle first-person inspection while the player
  walks where Select shows no picture, and the gamepad's right stick's
  click and Toggle first-person view (V, Back+R1, the deck's LOOK; the
  runtime keeps these requests for its next frame, so the page's operations
  never see them) anywhere in it but its menu; the arrow keys, the right
  stick, a finger on the picture and the mouse under pointer lock (both its
  motions: the page passes `movementY` too) turn it
  ([First-person inspection](#first-person-inspection)); agents use
  `window.xem.firstPerson`. The page keeps
  only the game's keys from the browser, and the arrow keys while
  inspection shows (and Escape, which toggles the panel; the header's
  Settings button and the deck's Settings do too, so Escape is never the
  only way).
  Audio starts with the first click, key or touch on the game (or "Start
  audio"); a Fullscreen button asks for the page's fullscreen, and
  entering or leaving it, blur and hiding let go of held input (and until
  focus returns the gamepad neither toggles nor turns first-person
  inspection, as it holds none of the game's buttons). The
  animation loop sets the pad when the player's buttons change, so a pad an
  automation client set (`pad`, `press`, `hold`, `act`, `replay`) stays until the player
  presses something.
- **Persistence.** Settings: `SettingsService` over `localStorage`
  (`xem.settings`). Memory cards: two 128 KB images in IndexedDB (`xem`
  database, `cards` store, `card1`/`card2`; none stored is a fresh formatted
  card, with a visible missing/eviction notice). The card device's `CardStorage` is synchronous and IndexedDB is not:
  the page loads both images before any session boots, the storage
  (`BrowserCards`) answers loads from that copy and takes the device's writes
  into it at once, marking the card dirty; after each of the window's
  animation frames and each automation step the page writes the latest
  dirty images to IndexedDB in order and acknowledges only completed
  transactions. A WebXR session's frames replace the window's and do the
  same: after each, the page runs the operations asked for in it (the
  panel's, the controllers' chords) and writes the dirty images, so a save
  made in the headset is in IndexedDB before the session ends. A failed write retains
  the image for explicit retry and export, with the error visible in Settings.
  Quota exhaustion, denied storage, interrupted transactions, corrupt states
  and missing/evicted saves have explicit errors. The runtime's
  copy is the authority; IndexedDB trails it by the writes in flight
  (`status().cards`, `status().page.cards`). Saves: the `saves` store holds
  game snapshots with their SHA-256, checked when read back.
  Storage belongs to the current origin/browser profile and can be cleared or
  evicted; Settings offers persistence requests and explicit backups. No write
  depends on an unload handler and persistence is not a permanent-storage promise.
  `window.xem.cards.store(slot, image)` puts a card image (a `.mcd`) in
  IndexedDB and inserts it into the live session, and
  `cards.importFile(slot, bytes, name)` any card or save file as Import
  original card 1/2 does ([Original saves and memory
  cards](#original-saves-and-memory-cards)); an import pauses play until the
  write completes, and the session takes the card only after IndexedDB has
  it. `npm run test:slice` (local, with disc 1)
  plays the slice's session logs on a desktop browser, a phone (Playwright's
  Pixel 7) and a headset browser (Quest Browser's user agent, IWER's Quest 3
  as navigator.xr) to their native hashes; the save goes through a reload
  taken mid-route (the session saved to IndexedDB, restored after the reload)
  and the next page loads card 2 from the title.
- **Automation.** `window.xem` (`version` 4): `status()`, `boot({executable, run})`,
  `setRunning`, `command(cmd)` (any control command) with the shorthands
  `step(frames)`, `runUntil({maxFrames, until})`, `memoryHash()`,
  `snapshot(name)`/`restore(name)`, `exportSnapshot()`/`importSnapshot(bytes)`;
  `firstPerson({on, yaw, pitch})` (the page's own `first_person`: [First-person
  inspection](#first-person-inspection));
  the asynchronous `run(cmd)` and `runFrames(n)` (through blocked frames);
  `settings.get()`/`settings.apply(change)` (a `SettingChange`, e.g.
  `{MasterVolume: 40}`), `panel.show(bool)`, `importDisc(blob)`,
  `readSectors(lba, count)`, `save(slot)`/`load(slot)`,
  `cards.image(slot)`/`cards.stored(slot)`/`cards.flush()`,
  `cards.store(slot, image)` and `cards.importFile(slot, bytes, name)`
  (resolving to what it found). Each is the
  runtime method the page's controls use; there is no DOM-input path (pads
  are set with the shared `pad`, `press` and `replay` commands).

From the repository root in `nix develop path:./nix/runtime`:

- build: `python3 src/runtime/web/build.py` (output `build/web/`; `--no-game` leaves
  the game module out, as a hosted build must until it ships separately)
- serve: `node src/runtime/web/serve.mjs build/web 8080`, then http://localhost:8080/
- tests: `cd src/runtime/web && npm ci && npm test` (Playwright on the shell's
  Chromium; `XEM_DISC1=<disc 1 image>` adds the user-disc tests, which send
  the same control commands to `src/runtime/target/release/xem-headless --control`
  when it is built and compare the replies, including the retained new-game
  route's `memory_hash` at the title, the story text and frame 4500).
  Headless Chromium needs `--use-vulkan=swiftshader` for WebGPU: with its
  default Vulkan choice it destroys a WebGPU device after the canvas presents.

## WebXR adapter

`xem-webxr` (wasm32 only) and its test page `src/runtime/web/xr/` render the
`xem-render` test scene in stereo through WebXR. The page is separate from the
main browser host so the adapter can move into it unchanged.

- **Device ownership.** The page canvas gets one WebGL2 context (no alpha,
  antialiasing, depth or stencil; `xrCompatible` whenever `navigator.xr`
  exists). wgpu's GLES adapter is created over it
  (`wgpu_hal::gles::Adapter::new_external`, `create_adapter_from_hal`), and the
  flat canvas surface resolves to the same context, so flat and immersive
  frames share one device and queue.
- **Submission (WebGL2).** The layer framebuffer of the session's
  `XRWebGLLayer` (antialias, depth, stencil and alpha off) is wrapped as a wgpu
  texture (`create_texture_from_hal`: `ExternalFramebuffer` for the opaque XR
  framebuffer, `DefaultRenderbuffer` when the layer reports `null`, as IWER's
  does), rebuilt only when the framebuffer or its size changes. The scene
  renders into a wgpu-owned eye buffer with its own depth (wgpu-hal would
  otherwise attach a depth texture to the opaque framebuffer, which WebXR
  forbids); one compositor pass then copies it into the layer on the GPU,
  flipped vertically (the GLES backend stores images top row first, WebXR
  reads framebuffers bottom row first) and sRGB-encoded in the shader (the
  layer is RGBA8 without sRGB writes). Nothing passes through the CPU. A
  clear colour is linear on every target, as the quads' pictures are: the
  compositor encodes it for a linear format holding encoded values (the
  browser's eye buffers), as an sRGB target (OpenXR's swapchains) does
  itself, so the room around the screen is the same colour in both hosts
  (sRGB 29, 31, 39; written as it was, a clear left the browser's room at
  3, 4, 5, nearly black, and the tests that tell the screen from the room
  by its colour told nothing). Tests: xem-render's
  `a_clear_colour_is_the_same_colour_on_linear_and_srgb_targets`.
- **Views.** Each view is drawn with the inverse of `XRView.transform`,
  `projectionMatrix` remapped from OpenGL depth (-1..1) to wgpu's (0..1), and
  its layer viewport (converted from WebXR's bottom-left origin).
- **Scheduling.** While a session runs, only `session.requestAnimationFrame`
  schedules frames; window animation frames drive the flat canvas and stop.
  One scene clock serves both; pausing freezes it while poses, views and
  rendering continue.
- **Session.** Enter VR requests `immersive-vr` synchronously in the click
  (user activation), requiring `local-floor` and offering `bounded-floor` and
  `hand-tracking`; granted features are read from `session.enabledFeatures`.
  Exit VR (or Escape) calls `session.end()`; any `end` event returns to the
  flat canvas. Missing `navigator.xr`, unsupported `immersive-vr` and refused
  requests leave the flat page usable.
- **Input.** Input sources are raw state: handedness, target-ray mode,
  profiles, target-ray and grip poses, gamepad buttons and axes, and the 25
  hand joints (pose and radius) when the session tracks hands, plus counts of
  select/squeeze/inputsourceschange events, visibility and reference-space
  resets. Nothing interprets gestures.
- **WebGPU.** `XRGPUBinding` is detected and reported (Chromium 152 exposes it
  only with `--enable-blink-features=WebXRGPUBinding`) but not used: no
  available runtime can test WebGPU layer submission, so WebGL2 is the
  submission path for now.

`src/runtime/.cargo/config.toml` sets `--cfg=web_sys_unstable_apis` for wasm32,
which web-sys's WebXR bindings require. In `nix develop path:./nix/runtime`:

- dependencies: `npm ci` in `src/runtime/web` (iwer 2.5.0, playwright-core 1.64.0)
- build: `src/runtime/web/xr/build.sh` (wasm-bindgen output in `src/runtime/web/xr/pkg`)
- tests: `node src/runtime/web/xr/tests/run.mjs [scenario...]` drives Chromium
  (`$XEM_CHROMIUM`, SwiftShader WebGL2) with IWER emulating a Quest 3, its
  controllers and hands; screenshots go to `build/xr-test/`
- by hand: serve `src/runtime/web` from localhost or HTTPS and open `/xr/`
  (`/xr/?emulate=quest3` installs IWER)

Emulation covers entry by click, granted features, session-scheduled frames,
two views with distinct view matrices, the projection conversion (including
asymmetric frusta), upright rendering into both the canvas framebuffer and an
opaque framebuffer (a test shim stands in for the headset compositor), head
pose, controller and hand input, visibility and recentering, paused head
tracking, exit by the app and by the runtime, re-entry, denial and missing
support. It cannot cover a headset browser's WebGL/WebXR driver, compositor
and display timing, which hand and gaze inputs it exposes, WebGPU XR layers or
performance; those need Quest Browser on a Horizon headset.

## OpenXR adapter

`xem-xr` runs an immersive OpenXR session on the host's one wgpu device. The
runtime creates the Vulkan instance and device (XR_KHR_vulkan_enable2) from the
create infos wgpu-hal would use, and wgpu-hal/wgpu wrap and own them, so the
scene, the compositor and the Slint panel render on the device and queue the
session is bound to. The crate documentation records the ownership and
synchronisation rules; in short:

- wgpu's single queue is the session's queue. A swapchain image is rendered only
  between acquire+wait and release, the work is submitted before
  `xrReleaseSwapchainImage`, and the image is explicitly returned to
  `COLOR_ATTACHMENT_OPTIMAL` (wgpu `transition_resources`) first. All frame,
  swapchain and queue calls stay on the session's thread.
- The eyes use one swapchain of two array layers: one acquire/wait/release and
  one image index per frame for both eyes, the layout Horizon's compositor and
  multiview rendering use, and per-layer views for today's per-eye passes.
- The Slint panel is a quad layer with its own swapchain: the runtime samples it
  directly (sharper text than resampling it through the eye images), and it is
  updated by a GPU copy only when Slint redraws; other frames resubmit the last
  released image.
- `xrWaitFrame` paces the loop; views, head and input are located at the
  predicted display time. Session states drive begin/end (READY, STOPPING) and
  exit (EXITING, LOSS_PENDING, instance loss).
- Controller actions (aim and grip poses, trigger, select, menu) are bound for
  `khr/simple_controller` and `oculus/touch_controller`. Hand joints
  (XR_EXT_hand_tracking) and eye gaze (XR_EXT_eye_gaze_interaction) are enabled
  only when the runtime offers the extension and the system reports support;
  `Capabilities` reports both. Input is raw state; gestures are interpreted
  elsewhere.
- The system's environment blend modes are enumerated
  (`Capabilities::blend_modes`; an opaque session uses the first). The room
  can show behind the frame (`Capabilities::passthrough`) where
  `ALPHA_BLEND` is among them (`Passthrough::AlphaBlend`), else where
  `XR_FB_passthrough` is offered (it is enabled whenever it is) and the
  system supports it (`XrSystemPassthroughProperties2FB`'s capability bit, or
  the first version's flag: `Passthrough::Fb`); else not at all. A system
  whose passthrough properties cannot be read offers none that way and
  the session starts all the same (`Capabilities::passthrough_error` says
  why). The host decides each frame's room before drawing its eyes
  (`XrSession::show_room(wanted)`, returning `Room::Hidden`, `Shown` or
  `Refused`); a frame showing it is ended (`end_frame(panel)`) with the
  projection layer's `BLEND_TEXTURE_SOURCE_ALPHA` (premultiplied colour,
  OpenXR's default) and either `ALPHA_BLEND` as its blend mode or, under
  it, a passthrough layer (`XrPassthroughFB` and a reconstruction
  `XrPassthroughLayerFB`, both running from their creation when first
  wanted, dropped once a frame no longer wants them: the cameras stop);
  elsewhere, and in a frame not decided, the opaque blend mode. A runtime
  refusing those objects leaves that frame opaque and the room unavailable
  for the rest of the session (later frames are opaque without asking
  again); the error goes to the host, never out of `end_frame`.
  `EndedFrame::passthrough` says how the room showed. The openxr crate keeps
  the passthrough layer's builder to itself, so it is submitted in its raw
  form.
- On Android the loader is initialised with XR_KHR_loader_init_android and the
  instance created with XR_KHR_android_create_instance (`Platform` is
  `openxr::AndroidPlatformInfo` there).

The `xem-xr-demo` example renders the test scene in stereo with the settings
panel as a quad layer. Its scene clock stands in for the simulation:
`--paused` freezes it while head tracking and rendering continue. It prints one
`key=value` line per frame (predicted display time and period, session state,
head and eye poses, swapchain operations in order, submitted layers, input) and
`--capture DIR` saves the eye layers and the panel image, read back from the
swapchains before release.

`src/runtime/scripts/xr-smoke.sh` (in `nix develop path:./nix/runtime`) runs it
against Monado 25.1 without a display or headset: `monado-service` with
`XRT_COMPOSITOR_NULL=1` (null compositor), `SIMULATED_ENABLE=1
SIMULATED_ROTATE=1` (the simulated HMD turns continuously),
`SIMULATED_LEFT/RIGHT=simple` (simulated simple controllers) and `XRT_NO_STDIN=1`,
in a private short `XDG_RUNTIME_DIR` (the IPC socket path must fit a
`sockaddr_un`); the demo finds it through `XR_RUNTIME_JSON=$XEM_MONADO_RUNTIME`.
Both use lavapipe unless `VK_ICD_FILENAMES` is set (`src/runtime/scripts/xem-gpu-host
src/runtime/scripts/xr-smoke.sh` runs both on the host NVIDIA driver). It checks the state order
READY, SYNCHRONIZED, VISIBLE, FOCUSED, STOPPING, EXITING after an exit request;
strictly increasing predicted display times; two distinct eye poses; projection
and quad layers on every rendered frame; acquire, wait, submit, release order;
changing head poses; and, with `--paused`, a constant simulation time.
`XEM_XR_COMPOSITED=1` adds a run under Monado's real compositor presenting to a
headless weston and screenshots it mid-session; the composition must show the
scene (a session that ended or never presented leaves the window black).

Monado cannot show what only a headset can: the Horizon OS runtime and its
Vulkan driver, Quest frame pacing and reprojection, real head and controller
tracking, hand tracking and eye gaze (Monado's simulated devices offer neither),
Android lifecycle and permissions (eye tracking needs
`com.oculus.permission.EYE_TRACKING`), the APK packaging with Meta's loader,
and the room behind the frame (the simulated HMD enumerates `OPAQUE` alone
and offers no `XR_FB_passthrough`, so neither passthrough path runs there;
the APK declares `com.oculus.feature.PASSTHROUGH`, not required, which
Horizon OS asks of apps using the extension).
`xem-xr` cross-compiles for `aarch64-linux-android`
(`cargo ndk -t arm64-v8a -P 26 build -p xem-xr` in `#android`).

The window, the surface and the view (`xem_host::View`: scene, compositor,
panel and its input) are shared with the Android host in `xem-host`.

## Android host

The APK (`dev.xem.app`, minSdk 29, target/compile SDK 34, x86_64 and
arm64-v8a) is SDL3's Java layer from the desktop's SDL release
(`$XEM_SDL3_SRC`, nixpkgs' `sdl3.src`: `SDLActivity` and its helpers are
copied into the Gradle build, never vendored), `libSDL3.so` built from the same
source with the NDK's CMake toolchain and `libmain.so`, the `xem-android`
cdylib whose `SDL_main` SDLActivity runs. `src/runtime/android` is a plain Gradle
project (Nix's Gradle 8.14, AGP 8.7.3 pinned, no wrapper; outputs under
`build/android`); `XemActivity` only adds what SDL lacks.

- **Startup.** SDL window on the activity's surface; wgpu on Vulkan, the GLES
  backend only when no Vulkan adapter can present (logged); the shared view
  with the settings panel shown (Back toggles it).
- **Assets.** A file bundled in the APK's assets is read through
  `SDL_IOFromFile` (the AssetManager). The user's disc is imported with SDL's
  open-file dialog, which is Android's `ACTION_OPEN_DOCUMENT` picker (the
  panel's *Import disc…* button); `XemActivity` takes a persistable read grant
  on the returned `content://` document and hands it to native code itself
  (SDL's dialog callback is lost when Android recreates the activity behind
  the picker), SDL opens it through the
  ContentResolver, its file descriptor is duplicated into a Rust `File` and
  `xem-disc` (`open_reader`, `identify`) reads it in place on a worker thread.
  The URIs of the last three imports are kept in the app's files directory
  and reopened on start.
- **Game.** Once the reopened documents are identified, disc 1 (else disc 2)
  boots the game from the same descriptor (`xem_game::boot_source` over
  `xem_disc::open_reader`), with the memory cards in `<files dir>/cards`;
  the settings panel hides. The other disc, and a disc imported while the
  game runs, go to the drive's changer for the game's disc changes
  ([Discs](#discs)) instead of starting it again. Like the
  desktop, `xem_host::Game` runs the frames due at 60 per second of wall time
  (at most four per rendered frame times the fast-forward factor,
  `MAX_CATCH_UP`, and none after 12 ms; a longer stall's time is dropped,
  never caught up), the GPU device's
  display becomes the game image and the SPU's output plays through an SDL3
  audio stream (AAudio) at the master volume (Binaural presenting it when
  on); pad 1 is the keyboard (arrows,
  Z/X/A/S, Enter Start) or the first gamepad (Back+Start toggles the
  panel). A trap stops the game and shows the panel.
  wasm2c's output compiles with the NDK clang for both ABIs; game memory is
  one 2 GiB private anonymous mapping (the module's whole linear memory, made
  read-write at once), committed only as pages are touched.
  A session log pushed to `<files dir>/script.json` plays instead of the
  player, and the frame and memory hash it ends at are logged:
  `XEM_SMOKE_SLICE=1 android-smoke.sh` plays the slice's logs on the
  emulator, the forest route with a background and return on the way, the
  save, and the load of card 2 in a fresh process.
- **Settings.** `xem_settings::FileStore` at `<files dir>/settings.json`.
  Import and Export original card 1/2 go through the document pickers
  (`content://` documents read and written through SDL); a raw single save
  takes its name from the document id ([Original saves and memory
  cards](#original-saves-and-memory-cards)). Import BIOS (save titles) does
  too, and keeps the image as `<files dir>/bios.bin`, whose Kanji font every
  boot takes ([Save titles and the Kanji font](#save-titles-and-the-kanji-font)).
  Save state keeps `<files dir>/state.xemstate`, in the window and in the
  headset loop (`--xr`, which takes the files directory as its state
  directory). Each app operation's outcome is logged with the game's
  vertical blank and memory hash just after it (`xem: app
  Action(SaveState) game_vblank=… memory_hash … ok: State written: …`).
- **Agents.** The Android host has no command channel (no `--control`):
  agents and tests reach it through the player's input, keys sent over adb
  (`adb shell input keyevent`) reaching the game and the app operations
  through the same bindings as a keyboard's (by default F5 Save state, F8
  Load state, R held to rewind, Backquote fast-forward, T auto-advance;
  [Saves and time controls](#saves-and-time-controls)); through session logs pushed to
  `<files dir>/script.json`, which hold game input, setup and pacing only
  (`card_present` and `fast_forward` among them; no file commands, no
  rewind); and through logcat, which carries each app operation's line
  above, the boot's and the frames' (`frame rendered`, `fps`) and a
  session log's end. Card and BIOS imports go only through the document
  pickers; `card_list`, `status` and the `app` command of the other native
  hosts have no Android counterpart.
- **Lifecycle.** SDL 3.4 delivers the application events only to event
  watches, synchronously on the SDL thread inside `SDL_PollEvent`: on
  `WILL_ENTER_BACKGROUND` the host drops the wgpu surface and stops the
  simulation clock and the game (its audio paused and flushed), SDL then blocks the thread until the activity resumes,
  and after `DID_ENTER_FOREGROUND` the surface is recreated on the new native
  window and the clocks restart where they stopped (a frame never advances
  the simulation clock by more than 100 ms nor runs more than four game
  frames times the fast-forward factor, so there is no catch-up). A
  replaced native window
  (configuration changes) is detected by identity and the surface recreated.
  `LOW_MEMORY` is logged; on `TERMINATING` the run ends. A recreated activity
  (changes outside `configChanges`, such as density or asset paths) runs
  `SDL_main` again in the same process (`SDL_ANDROID_ALLOW_RECREATE_ACTIVITY`,
  which `XemActivity` sets as soon as the libraries load: a recreation
  before `SDL_main` has started, such as an overlay changing on an
  emulator's first boot, otherwise ended the process), which reloads
  settings and reopens the imported disc. `XemActivity`'s
  `onDestroy` sends the run its quit and waits up to 5 s for it to end
  before SDL's own, which waits a second and then drops the quit: a run
  still booting the game when Android recreated the activity twice in a row
  missed it, kept its window, and the next run could not create one ("Android
  only supports one window"). A run still waiting for its first surface ends
  when the quit comes.

In `nix develop path:./nix/runtime#android`:

- build: `src/runtime/scripts/android-build.sh` (`XEM_ANDROID_ABIS` to pick
  ABIs, both by default) → `build/android/gradle/app/outputs/apk/debug/xem-debug.apk`
- emulator smoke test: `src/runtime/scripts/android-smoke.sh` (headless API-34
  x86_64 AVD under `.local/android`, guest Vulkan on SwiftShader; logcat and
  screenshots in `build/android/smoke`). With the user's disc 1 at
  `discs/Xenogears disc 1.chd` (or `XEM_DISC1`) it imports it through the
  picker and plays the game, whose boot takes the Kanji font of a BIOS image
  made there in `<files dir>/bios.bin` (`font=bios` in its log line): boot
  logo, opening movie and title screenshots,
  frame rates, a pad press, and a background and return across which the
  game's vertical-blank count stays put; after the 20× fast-forward
  rounds, Settings by touch (Back shows the panel), read from the
  operations' `xem: app` lines: Time's Stop fast-forward, Hold to rewind
  held for two seconds (the vblank its release logs is below the
  press's), Saves' Save state (its vblank past the release's: the game
  played on), the panel hidden while the game plays on, then Load state
  (the vblank and memory hash Save state logged; `files/state.xemstate`
  written), and Start fast-forward again (`fast_forward=true` in the
  `frame rendered` lines once the panel hides). The emulator's software
  Vulkan draws frames with the panel shown at about two a second (30
  without it), and a drawn frame runs only about three game frames (the
  host's one batch: at most four at 1×, its catch-up, within 12 ms; the
  `fps` lines of two passing runs here gave 2.8 to 3.2 at 1× and 3.9 to
  4.6 under fast-forward), so the section waits on those lines rather
  than on frames. Then, with `xem-headless` (`XEM_HEADLESS`, else the
  suite's build; the suite's android stage builds it), a state it made
  1 600 frames into field 30 (frame F) goes where Load state reads it, with
  a session log of no commands ending at F+600 (`files/script.json`: the
  game plays as the original, without the player's input or the host's
  controls); Load state's key (F8, held) after the boot loads it through
  the app's own path to `xem-headless`'s RAM (11 and 7.5 s after the boot
  in two runs here, the game the boot started meanwhile playing toward the
  log's end at 32 to 40 frames a second), the log's `script frame F+600 memory_hash` is
  `xem-headless`'s, and Save state's (F5) state there, pulled with
  `run-as`, is `xem-headless`'s state at that frame byte for byte (the
  default graphics ask for no reveals), loads natively to that RAM and
  goes on 600 frames to where `xem-headless`'s own goes ([State
  interchange](#state-interchange); about 40 s on the emulator). `XEM_SMOKE_GAME_ONLY=1` skips the
  synthetic-disc and lifecycle sections. `XEM_SMOKE_ABI=arm64-v8a` builds
  (unless `XEM_SKIP_BUILD=1`) and installs the APK's arm64-v8a library
  instead (`adb install --abi`), which the AVD's x86_64 image runs through
  its ARM translation; the start checks the package's ABI and the
  translation's arm64 linker and arm64 system libraries in the app's
  process (the x86_64 run: the linker's absence).
  The suite's android tier runs the whole smoke with the x86_64 library,
  then its game section with the arm64-v8a one. The game module must be built first
  (`python3 tools/game_module.py --stubs` in the default shell).
- arm64 under qemu: `src/runtime/scripts/arm64-smoke.sh` builds
  `xem-headless` for `aarch64-linux-android` (cargo-ndk, API 29, linked
  statically against bionic; `XEM_SKIP_BUILD=1` reuses it) and, with the
  user's disc 1, plays the slice's session logs under the shell's
  `qemu-aarch64` and with `xem-headless` (`XEM_HEADLESS`, else the suite's
  build), comparing their hashes, states and the save's card, and draws
  Lahan and the world map pulled back (`reveal_frames`), comparing the
  culled faces revealed ([State interchange](#state-interchange); inputs,
  replies and states in `build/arm64`)
- `cargo ndk -t arm64-v8a -P 29 build -p xem-ui -p xem-render` builds the
  shared crates alone (Slint compiles its FemtoVG module out on Android, so
  `xem-ui` takes the renderer from `i-slint-renderer-femtovg` there)

## Emulators as hardware

Phase 2's device items are verified on emulators, which the project counts as
the hardware: the Android emulator (x86_64 AVD, API 34) for Android
startup, asset access, lifecycle and the game (`src/runtime/scripts/android-smoke.sh`),
Monado's simulated headset for OpenXR stereo, the Slint panel quad, swapchain
and device ownership, frame scheduling and paused head tracking
(`XEM_XR_COMPOSITED=1 src/runtime/scripts/xr-smoke.sh`), and IWER's Quest 3 in
Chromium for WebXR (`npm run test:xr`), and for arm64 qemu's user-mode
emulator (the game headless: `src/runtime/scripts/arm64-smoke.sh`) and the
Android emulator's ARM translation (the APK's arm64-v8a library:
`XEM_SMOKE_ABI=arm64-v8a android-smoke.sh`). What no
emulator here exercises, and a headset would be the first to show: an arm64
processor running the APK's arm64-v8a library (the emulator translates it to
x86_64); Horizon OS's OpenXR runtime with Meta's loader in the APK,
its Vulkan driver, reprojection, hand tracking and eye gaze; Quest Browser's
WebXR compositor and whether it offers WebGPU XR layers (`XRGPUBinding`);
whether the 2 GiB read-write reservation of game memory fits Quest memory
limits; audible AAudio output; Bluetooth gamepad mapping. Phase 4 adds:
the Android host's `--xr` entry into OpenXR (compiled for both ABIs; its
loop is the one Monado runs through `xem-desktop --xr`), headset frame pacing
at 72-120 Hz on a mobile GPU, the HD-2D effects' cost at Medium and Low tiers
on mobile and headset GPUs, and D3D12 and Metal (only Vulkan, WebGPU and
WebGL2 run here). The game's sound
level (1.22 times the reference emulator's recordings for the movie's CD
input and 1.33 for the SPU's voices, in every sound mode) and the reverb
unit's return (it matches the recordings' only loosely:
`xem-headless/tests/original_sound.rs`) need a console recording to
settle.
