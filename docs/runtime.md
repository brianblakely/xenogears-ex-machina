# Runtime (Phase 2)

The shared Rust/C runtime of [plan.md](../plan.md) Phases 2 onward. The recovered
C in `src/decomp/` stays the only game implementation; this document records how it
is built for native and browser hosts and how the hosts are layered. The
[later-phases handbook](later-phases.md) holds the program facts it relies on.

## Approved foundations

| Decision | Choice |
| --- | --- |
| Rust workspace | `src/runtime/` (`src/runtime/Cargo.toml`, crates in `src/runtime/crates/*`, committed `Cargo.lock`) |
| Toolchain | `nix develop path:./nix/runtime` (Rust 1.97.1 with wasm32/Android targets via rust-overlay, clang/lld 21.1.8, wabt 1.0.41, binaryen 132, wasm-bindgen-cli 0.2.127, SDL3 3.4.14, mesa 26.2.2, Monado 25.1.0, Chromium 152, Node 24); `#android` adds the Android SDK/NDK r29, emulator and API-34 x86_64 image, JDK 17, Gradle 8.14.4, cargo-ndk |
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
  checks (no signal handlers). Nothing interprets or emulates a CPU.
- **Browser.** The same `game.wasm` is instantiated next to the Rust runtime
  (wasm32-unknown-unknown, wasm-bindgen); the two modules keep separate
  memories and exchange scalars and bounds-checked offsets.

## Commands, introspection and snapshots

`xem_core::control::Session` is the one command layer for humans, agents,
tests and browser clients: a JSON command in, a JSON result out
(`xem-headless --control` reads one per line). Commands: `status`,
`frames {count}`, bounded `run_until {max_frames, until: {frames} | {vblanks} |
{path, equals|not_equals}}`, `pad {port, buttons}`, `press {buttons, frames}`,
`connect_pad`, `inspect {path, depth}`, `write {path, value}`, `globals
{prefix}`, `read_memory`/`write_memory`, `snapshot`/`restore {name}`,
`memory_hash`, `record`/`stop_recording`/`replay {changes}` (pad states per
vertical blank) and `scenario {writes, mode}`, which writes values through the
schema and enters a mode through the original dispatcher on an empty stack.
Pads are set as the BIOS driver reports them; nothing simulates lower-level
input.

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
  includes their target's headers first.
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
  writes the map (and entry) into `game_data`, selects mode 1 and lets the
  menu finish into the original dispatcher. `enter_field {map: 23}` reaches the
  forest map's opening dialogue (`tests/route.rs`) and matches the original
  capture of the same recipe (tools/reference/scenario.py) within 0.013 mean
  colour difference.
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
  a snapshot restored in battle and from a pad recording. The runtime counts
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
  menu reaches it). Mono gives every voice the centre gain and halves CD
  audio into both sides; Stereo and Wide share the pan law. Wide only flips
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
  which give the same picture.
- **Memory cards** (`xem_core::devices::card`) are two raw 128 KB images
  (the `.mcd` layout) in a `CardStorage`: files natively (`xem-headless
  --cards DIR` keeps `card1.mcd`, `card2.mcd`; a missing file is a fresh
  formatted card), memory in tests. The BIOS file calls on "bu00:" and
  "bu10:" (`src/port/bios.c`) and `_card_info` (`src/port/libcard.c`) run the BIOS's
  card file system over them; each access costs one 140-byte card command at
  250 kHz per 128-byte frame, during which the port suspends as the BIOS
  blocks, and a check ends with the card interrupt and its event (done,
  timeout without a card, new card for a card without a file system).
  Snapshots hold the images.
- **Kanji font.** Krom2RawAdd returns glyph addresses in the BIOS ROM's font
  through its KUSEG mirror 0x1FC66000; the host places the font there from the
  user's BIOS image (`--bios FILE`), or blank glyphs without one
  (`xem_core::bios`).
- The PC file server (libsn `PC*`) has no development host and fails.

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
(`field_view`, the battle and world map camera state). A renderer for other
views (widescreen, stereo, XR) re-traverses these records and the meshes in
game memory with its own cameras; the original camera and screen keep
deciding gameplay (on-screen tests, culling).

## Crates

| Crate | Role |
| --- | --- |
| `xem-core` | headless authority: game instance, virtual clock, commands, introspection, stepping, snapshots, services |
| `xem-game` | the game module binding (wasm2c natively, imports in browsers) |
| `xem-gpu`, `xem-spu`, `xem-media`, `xem-disc` | the console devices as host services, and disc import |
| `xem-render` | the wgpu renderer for flat, stereo and XR views |
| `xem-ui` | the Slint settings panel as a custom platform rendered into a texture on the shared wgpu device |
| `xem-xr` | the OpenXR adapter |
| `xem-webxr` | the WebXR adapter: immersive sessions, views, raw input and WebGL2 layer submission through wgpu |
| `xem-desktop`, `xem-headless`, `xem-android`, `xem-web` | hosts |
| `xem-host` | what the SDL3 hosts share: the window and its native handles, the wgpu surface (dropped and recreated with the native window), the presented view (game display or test scene, and settings panel), the game paced at 60 frames a second with its sound (`Game`) |
| `xem-settings` | typed application settings: validation, acknowledgement, change notification, JSON persistence |

## Desktop host and settings panel

`xem-desktop` opens an SDL3 window (Wayland or X11), presents through wgpu with
FIFO vsync and draws the game (`--disc DISC [--cards DIR] [--bios FILE]`; the
test scene without a disc), placed by the presentation settings, under the
Slint settings panel (F1, Escape or a gamepad's Guide toggles it, and its
Start/Back while no game runs). One `xem_render::Gpu` (instance, adapter,
device, queue) serves `SceneRenderer`, `Compositor` and `xem_ui::SettingsUi`;
Slint runs as a custom platform under the host loop and renders into a texture
only when it asks for a redraw. Settings live in
`xem_settings::SettingsService` (`$XDG_CONFIG_HOME/xenogears-ex-machina/settings.json`
on the desktop); the panel and agents change them only through `apply`.
With a disc, the game runs paced by the wall clock at 60 frames a second (a
stalled host drops time rather than catching up), pad 1 follows the keyboard
and the first gamepad, the GPU's display is uploaded as the game image after
each frame and the SPU's output is queued to an SDL3 audio stream at the
master volume and audio output (`xem_host::Game`; `AudioOutput::mix` is the
browser's mix too). Memory cards default to `cards/` beside the settings file.
`--script LOG` plays a session log instead of the player's input (its pad
states replace the keyboard's) and `--exit-at-frame N` prints the memory hash
when the game has run N frames and exits: the slice's logs end at their native
hashes in the desktop host too, the load from card 2 in a fresh process after
the save.

`./play.sh [DISC] [xem-desktop options]` (repository root) builds what is
missing or stale (the matched images, the game module, `xem-desktop`) and plays
disc 1 by default on the host's NVIDIA driver.

From `src/runtime/` in `nix develop path:./nix/runtime`:

- tests on lavapipe: `VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD cargo test -p xem-settings -p xem-render -p xem-ui`
- on the host NVIDIA driver: `scripts/xem-gpu-host target/debug/xem-desktop`
  (`--show-settings --exit-after-frames N --screenshot out.png` for a smoke run)
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
  `XemApp::snapshot_bytes`/`restore_bytes` carry `Runtime::snapshot` bytes for
  saves. `boot` starts the game as `xem_game::boot` does natively (the
  executable's text, blank Kanji glyphs, the memory cards, the user's disc in
  the drive read in place through `DiscImport::game_source`) and takes the
  game's `stubs.txt` and `schema.json` from beside `game.wasm`.
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
  time, at most two per animation frame and none after 12 ms, then one render;
  the page then posts the frames' sound and card writes. Time a throttled,
  hidden or stalled page missed is dropped (`status().loop.dropped`), never
  caught up.
- **Rendering.** wgpu on the canvas: WebGPU when the browser gives an adapter,
  WebGL2 otherwise (`?backend=webgl2` forces it); `status().renderer.backend`
  says which. With a session the canvas shows the GPU device's display
  (`display_rgba8`, uploaded to an sRGB texture after frames ran and placed by
  the presentation scale and filter, as xem-host's `View::set_game_image`);
  without one, the test scene. The Slint panel (FemtoVG's wgpu renderer on the same device,
  composited over the scene) works on both; F1 or the Settings button shows it,
  Escape or Close hides it. A lost device is replaced on a new canvas (WebGL2
  after repeated WebGPU losses).
- **Disc import.** A file input (and, where available, the File System Access
  API, whose handle is kept in IndexedDB for "Reopen") gives a `File`; the image
  is read in place over `PrefetchedFile` in 256 KiB chunks with an 8 MiB chunk
  budget and a 4 MiB decoded-hunk budget, whatever its size. Identifying disc 1
  reads 1.1 MB of its 358 MB CHD. CHD and raw MODE2/2352 `.bin` images are
  accepted (a `.cue` is not needed for a single-track image). Nothing is sent
  anywhere.
- **Audio.** A browser session keeps the SPU's output (`Spu::set_output`).
  After the frames of an animation frame (or a command) the runtime takes the
  samples into a bounded queue (a quarter second; older ones are dropped, as
  when automation runs frames faster than real time); each animation frame the
  page takes them, mixed by the master volume and output settings
  (`XemApp::take_audio`), and posts them to an `AudioWorklet`, whose ring keeps
  at most about 0.19 s and plays silence when dry (no SharedArrayBuffer).
  "Start audio" creates the `AudioContext` inside the click, at the SPU's
  44.1 kHz; the browser resamples to the device.
- **Input.** While the settings panel is hidden, the keyboard
  (`KeyboardEvent.code`, xem-host's default mapping: arrows, Enter Start,
  Right Shift or Backspace Select, Z or Space Cross, X Circle, A Square, S
  Triangle, Q/W L1/R1, 1/2 L2/R2) and the first gamepad (Gamepad API standard
  mapping; Home toggles the panel) hold pad 1's buttons (`src/runtime/crates/xem-web/src/input.rs`). The
  animation loop sets the pad when the player's buttons change, so a pad an
  automation client set (`pad`, `press`, `replay`) stays until the player
  presses something.
- **Persistence.** Settings: `SettingsService` over `localStorage`
  (`xem.settings`). Memory cards: two 128 KB images in IndexedDB (`xem`
  database, `cards` store, `card1`/`card2`; none stored is a fresh formatted
  card). The card device's `CardStorage` is synchronous and IndexedDB is not:
  the page loads both images before any session boots, the storage
  (`BrowserCards`) answers loads from that copy and takes the device's writes
  into it at once, marking the card dirty; after each animation frame and
  automation step the page takes the dirty images and writes them to
  IndexedDB on a chain of transactions, so they land in order. The runtime's
  copy is the authority; IndexedDB trails it by the writes in flight
  (`status().cards`, `status().page.cards`). Saves: the `saves` store holds
  session snapshots with their SHA-256, checked when read back.
  `window.xem.cards.store(slot, image)` puts a card image (a `.mcd`) in
  IndexedDB for the next session. `npm run test:slice` (local, with disc 1)
  plays the slice's session logs on a desktop browser, a phone (Playwright's
  Pixel 7) and a headset browser (Quest Browser's user agent, IWER's Quest 3
  as navigator.xr) to their native hashes; the save goes through a reload
  taken mid-route (the session saved to IndexedDB, restored after the reload)
  and the next page loads card 2 from the title.
- **Automation.** `window.xem`: `status()`, `boot({executable, run})`,
  `setRunning`, `command(cmd)` (any control command) with the shorthands
  `step(frames)`, `runUntil({maxFrames, until})`, `memoryHash()`,
  `snapshot(name)`/`restore(name)`, `exportSnapshot()`/`importSnapshot(bytes)`;
  the asynchronous `run(cmd)` and `runFrames(n)` (through blocked frames);
  `settings.get()`/`settings.apply(change)` (a `SettingChange`, e.g.
  `{MasterVolume: 40}`), `panel.show(bool)`, `importDisc(blob)`,
  `readSectors(lba, count)`, `save(slot)`/`load(slot)`,
  `cards.image(slot)`/`cards.stored(slot)`/`cards.flush()`. Each is the
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
  layer is RGBA8 without sRGB writes). Nothing passes through the CPU.
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
`com.oculus.permission.EYE_TRACKING`), and the APK packaging with Meta's loader.
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
  The URI is kept in the app's files directory and reopened on start.
- **Game.** An identified Xenogears disc boots the game from the same
  descriptor (`xem_game::boot_source` over `xem_disc::open_reader`), with the
  memory cards in `<files dir>/cards`; the settings panel hides. Like the
  desktop, `xem_host::Game` runs the frames due at 60 per second of wall time
  (at most two per rendered frame, never catching up), the GPU device's
  display becomes the game image and the SPU's output plays through an SDL3
  audio stream (AAudio) at the master volume and audio output; pad 1 is the keyboard (arrows,
  Z/X/A/S, Enter Start) or the first gamepad (its Guide button toggles the
  panel while the game runs). A trap stops the game and shows the panel.
  wasm2c's output compiles with the NDK clang for both ABIs; game memory is
  one 2 GiB private anonymous mapping (the module's whole linear memory, made
  read-write at once), committed only as pages are touched.
  A session log pushed to `<files dir>/script.json` plays instead of the
  player, and the frame and memory hash it ends at are logged:
  `XEM_SMOKE_SLICE=1 android-smoke.sh` plays the slice's logs on the
  emulator, the forest route with a background and return on the way, the
  save, and the load of card 2 in a fresh process.
- **Settings.** `xem_settings::FileStore` at `<files dir>/settings.json`.
- **Lifecycle.** SDL 3.4 delivers the application events only to event
  watches, synchronously on the SDL thread inside `SDL_PollEvent`: on
  `WILL_ENTER_BACKGROUND` the host drops the wgpu surface and stops the
  simulation clock and the game (its audio paused and flushed), SDL then blocks the thread until the activity resumes,
  and after `DID_ENTER_FOREGROUND` the surface is recreated on the new native
  window and the clocks restart where they stopped (a frame never advances
  the simulation clock by more than 100 ms nor runs more than two game
  frames, so there is no catch-up). A replaced native window
  (configuration changes) is detected by identity and the surface recreated.
  `LOW_MEMORY` is logged; on `TERMINATING` the run ends. A recreated activity
  (changes outside `configChanges`, such as density or asset paths) runs
  `SDL_main` again in the same process (`SDL_ANDROID_ALLOW_RECREATE_ACTIVITY`),
  which reloads settings and reopens the imported disc.

In `nix develop path:./nix/runtime#android`:

- build: `src/runtime/scripts/android-build.sh` (`XEM_ANDROID_ABIS` to pick ABIs)
  → `build/android/gradle/app/outputs/apk/debug/xem-debug.apk`
- emulator smoke test: `src/runtime/scripts/android-smoke.sh` (headless API-34
  x86_64 AVD under `.local/android`, guest Vulkan on SwiftShader; logcat and
  screenshots in `build/android/smoke`). With the user's disc 1 at
  `discs/Xenogears disc 1.chd` (or `XEM_DISC1`) it imports it through the
  picker and plays the game: boot logo, opening movie and title screenshots,
  frame rates, a pad press, and a background and return across which the
  game's vertical-blank count stays put. `XEM_SMOKE_GAME_ONLY=1` skips the
  synthetic-disc and lifecycle sections. The game module must be built first
  (`python3 tools/game_module.py --stubs` in the default shell).
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
Chromium for WebXR (`npm run test:xr`). What no emulator here exercises, and a
headset would be the first to show: the arm64-v8a build running (it is only
compiled and linked); Horizon OS's OpenXR runtime with Meta's loader in the APK,
its Vulkan driver, reprojection, hand tracking and eye gaze; Quest Browser's
WebXR compositor and whether it offers WebGPU XR layers (`XRGPUBinding`);
whether the 2 GiB read-write reservation of game memory fits Quest memory
limits; audible AAudio output; Bluetooth gamepad mapping. The game's sound
level (about 1.3x the reference emulator's recordings in every sound mode)
needs a console recording to settle.
