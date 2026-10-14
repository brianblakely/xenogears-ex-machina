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
  checks (no signal handlers). Nothing interprets or emulates a CPU. The
  translation comes in 16 parts compiled in parallel (one file took GCC
  nearly four minutes, the parts 41 s on 16 threads), built without
  semantic interposition so the parts' global functions inline as one
  file's static ones did (a Lahan frame's game work: 1.395 ms, one file
  1.396).
- **Browser.** The same `game.wasm` is instantiated next to the Rust runtime
  (wasm32-unknown-unknown, wasm-bindgen); the two modules keep separate
  memories and exchange scalars and bounds-checked offsets.

## Commands, introspection and snapshots

`xem_core::control::Session` is the one command layer for humans, agents,
tests and browser clients: a JSON command in, a JSON result out
(`xem-headless --control` reads one per line). Commands: `status`,
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
opens on Saves with the request on the status line) and the change happens
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
waits for it goes in).

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
  ([Saves and time controls](#saves-and-time-controls)).
- **Kanji font.** Krom2RawAdd returns glyph addresses in the BIOS ROM's font
  through its KUSEG mirror 0x1FC66000; the host places the font there from the
  user's BIOS image (`--bios FILE`), or blank glyphs without one
  (`xem_core::bios`).
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
  within the last half second); otherwise it is premixed music (the title
  screen's) and is presented as a labelled estimate, `estimated
  arrangement`: its pair as two loudspeakers 30 degrees either side of
  ahead and part of its side signal moved to two at 110 degrees, delayed
  apart so they stay diffuse, the mid and the side's energy kept.
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
  first-person view's axes (`GameView::listener_axes`) and a headset's head
  (`xem_core::binaural::head_axes` of the OpenXR or WebXR viewer pose, the
  screen's forward its reference; lost tracking keeps the last; leaving the
  headset clears it). The hosts' audio stays plain two-channel stereo at
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
  prints the status.

Tests: `xem-spu` (the tap reconstructs every sample and changes nothing);
`xem-binaural`'s units (levelling, interaural cues, the shelf, the pan law,
gains, crossfades, resets, the knee, the stage's rotation; a tone turned
from right to left stays as smooth through the glide as held at either end;
a source's status says where it heads and how its voices pan; a journal
mixes again exactly what was mixed, its parts adding up) and
`tests/directions.rs`, where noise placed in 48 directions between the
measured ones is recovered from the rendered ears alone (spectra and delay,
matched against every measured direction) within 1.5 degrees (median), 3.5
at worst, left and right, front and back and up and down always right; the
model listener there is the measured head, not people. `xem_core::binaural`
on synthetic driver memory (identities, songs, field and arena emitters,
sprites followed until freed, the battle's fixed centre, distance, zoom and
scale changing nothing, restores, head turns; a start's tail keeping its
place when its slot takes another actor for the same sound; a first-person
look a quarter right and a head pitched 45 degrees up, which do not
commute, turning a direction in that order, each once). On the
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
sounds the same every time; in the forest's battle (the slice's route) the
combatants' sounds are placed at their sprites (8 effects) around the
formation's centre, which stays put through the battle. Along a walk through
field 509 (`tests/reference-inputs/binaural-field509-walk.json`, a session
log: six emitter actors playing three sounds, of which the original keeps
the three nearest the character sounding, a set that changes nine times on
the walk; the camera turned by 45 and 90 degrees, turns in place, the menu
and a jump), ten sessions from one state (plain Stereo; Binaural with its
defaults, bypassed, music only, effects only, immersion 0 and 100;
head-tracked music under a head sweeping 90 degrees either way; sweeping
first-person axes; the settings changed every 120 frames, off and on again
among them) have the same game memory (`memory_hash`), voice volume
registers and current volumes every frame and the same SPU every 60
frames, and every placed source reports exactly the gain the driver staged
for its voices (13 152 source frames whose voices held still and sounded
through the frame, 1 606 of them effects; within 0.3% of the driver's own
gain before panning, recomputed from its records by the decomp's formula,
which is the pan law's rounding). Mono and Wide on the same walk differ
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
memory and the SPU as that run's. `web/tests/binaural.test.mjs`: one state
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
while the game is paused, and has no head after the session.

Not done or not measured: no listening tests with people (direction and
musical coherence were checked with the measured head as the listener, and
the arrangement rules are not tuned by ear); a generic head, so a listener's
own front and back may differ; positional effects cover the field's
emitters, the arena's voices and sprites' animation sounds (each effect left out is reported, not
guessed), and sounds that event scripts start (field and battle) are left
out, since the actor running a script is not necessarily the emitter; no
per-song overrides were reviewed; the arena path is tested on synthetic
memory, not reached in the game; headsets and phones only through IWER and a
throttled processor. The interaural delay, applied as a phase ramp, leaves
up to 1e-4 of a filter's energy (-40 dB) past the 128 samples a block
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
  replayed every presentation). When one presentation holds several game
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
  Interpolation moves each scene primitive of the newest frame from its
  match in the frame before (same shape, page, CLUT and texture coordinates,
  or colours; the nearest, within 48 pixels) by the time since that frame
  arrived, one game frame late; the interface moves as the game moves it.
- **Effects** (`EffectsRenderer`, `effects.wgsl`). See below.
- **GameView** (`view.rs`) is what hosts use: it resolves the graphics
  settings for a host class, accumulates the trace between presentations,
  tells the host which culled faces the trace should record (`reveals`:
  the reach and the pull-back, passed to `Gpu::set_reveals`) and the view
  the field's characters keep moving in (`actor_view`, below), and
  presents. A presentation with nothing new to draw (the display refreshing
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
  movies stay rebuilt to their end.

The trace, the projections, the recording and every renderer object observe
only: the native test `xem-headless/tests/render.rs` runs the same 240 frames
from a field 12 snapshot with nothing drawn, with every frame drawn under
switching profiles (HD-2D with wider framing and interpolation, wider framing
with the camera pulled back four times, PS1, Custom, Modern), and with a
second view attaching, changing and detaching, and checks one RAM hash; the
browser test `web/tests/render.test.mjs` does the same per profile on WebGPU
and WebGL2, and the slice logs still replay to their hashes.

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
| presentation | every display refresh, or game frames only; interpolation; wider framing (to the output's aspect ratio) |
| zoom out | the field camera pulled back along its axis, 1x (default) to 5x in quarter steps; the mouse wheel over the game moves it (down out, up in; not while looking around in first person); the world map takes a tenth of it, other modes none; independent of the aspect ratio |
| characters in view | field characters keep moving wherever the zoomed-out or wider view shows them (on, the default; changes gameplay, recorded with the inputs; nothing while the view is the original's), or stop just beyond the original screen as the original's do (off) |
| filtering | world, sprites, interface: nearest or linear, each; movies: Enhanced (default: rebuilt from the decoder's planes, deblocked, colour following the brightness, enlarged and sharpened with FSR 1) or Original (the decoder's picture as it is) |
| culling | viewport culling (the game's own against its 4:3 screen: off, wider and pulled-back views show the scenery the game culled; a first-person view shows it either way), back faces (both off) |
| headset screen | distance, width, depth of field in a headset (off) |

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
browser host (keyboard, mouse, gamepad and touch alike). Headsets toggle it
too, but show it on their flat virtual screen ([Headsets](#headsets)).
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
its right stick's click). The right stick's click toggles it anywhere in a
field but a menu. While the view is on, the arrow keys (2 radians a second
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
map is left, and under the Original renderer.

The player's inputs all keep the character where it stands: the keys,
buttons, mouse and touch deck above, the taps below and a headset's
gestures. Hosts give the session the view's state before each batch of
frames (`Session::set_inspecting`: `Game::want_inspection` on desktop,
Android and OpenXR, the browser's frame loop): turning it on stops a route
under way, recorded as the player's own `assist_stop` is, and while it is
on `navigate` and `interact` are refused ("first-person inspection keeps
the character where it stands: turn it off to walk or interact");
`status` reports `inspecting`. An agent's other input is not held back:
`pad`, `press`, `hold` and `act` reach the pad as sent, so its directions
and Jump walk, turn and jump the character under the view
(`tests/inspection.rs` turns it so), and an `assist` command (the intent
`navigate` and `interact` record, sent live or replayed) starts its
route. A replay ignores the host's state, and the native hosts give none
while they play a session log (`--script`, Android's `script.json`).
Turning it off restarts nothing. Taps on the picture resolve without the
field, since the view shows no field camera to walk or point through:
walking, they walk nowhere and talk to no one, while a dialogue's still
advance and a menu's still choose, and no floor or destination marks
show. A headset's gestures follow the same rule
([Headsets: hands, gaze and control decks](#headsets-hands-gaze-and-control-decks)).

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
view's depth). The interface stays as the game drew it (the compass too).
A frame recorded looking around but shown from the game's camera (the view
just turned off) shows none of its culled faces: they lie anywhere, not
only beside the screen.

With characters in view on, the view the field's characters keep moving in
is the widest (`actor_view` 1000% with 1024-pixel sides: the view may turn
anywhere; in the forest, map 22, the 30 sprite actors the original stops
keep moving); off, the game runs as the original. The
[Binaural sound mode](#binaural-sound-mode)'s listener looks where the view
looks (`GameView::listener_axes`, which every host gives the session each
frame: the look alone, never a headset's head, which `Session::set_head`
adds once more after it). `status().renderer.firstPerson` and
`status().renderer.look` (radians turned from the heading it began with,
yaw right and pitch up) show it in browsers. `--example motion -- --map 30
--look YAW,PITCH[,TURN]` shows a field from the eyes (degrees from the
camera's heading, TURN more each frame kept: map 30's room with its
townspeople standing round the table). Measured in the forest (960x720
output, this machine's CPU and lavapipe, means per game frame): the game
frame 1.5 ms without the view and 2.5 ms with it (4 763 culled faces
recorded a frame), the presentation's CPU work 1.9 and 5.5 ms.

Tests: `xem-headless/tests/render.rs` looks around in field 12 for 160
frames (more than 100 shown from the eyes) and ends at the same RAM hash;
`tests/controls.rs` checks the field query (the map's picture, walking,
the menu) on the original's maps. `tests/inspection.rs` (local, disc 1)
plays a flat host's part in field 30: turning the view on stops a route 15
frames under way (`Stopped`) and refuses new ones; the character and the
game's camera stay over 60 frames of a held direction, jump and camera
turn (more than 50 of them shown from the eyes); while an agent's presses
turn the character five ways and the game's camera a quarter turn (0 to
1024) over 120 frames, the view's heading in the world moves at most
0.0027 radians (the camera matrix's fixed point); turning it off moves no
camera, starts no route, keeps the character and shows the game's camera
pixel for pixel against a view that never looked, and a new route walks.
`xem-core/tests/persistence.rs` checks the session's part on a stand-in
module (one recorded stop, the refusals, a recorded intent that still runs,
a replay that ignores the host), `xem-settings`' look tests the toggles
(Select's keys and the deck's SELECT alike), the underhand grab's rule
(`look::grab_toggles`: in a field but its menu, under Ex Machina only),
the held-back buttons under the default, an unswapped and a
shoulder-swapped assignment and the turn rates, `xem-assist`'s the
headset's gestures and pointer keys while
inspecting and two-hand scaling that leaves the screen, and the deck's
SELECT and drag, `xem-host`'s a gamepad's toggles and the touch deck (SELECT
toggling and staying the game's where the map's picture shows, no Run left
alone, a 100-pixel drag turning 0.3 radians and tapping nothing),
`xem-core`'s Binaural test a look and a head turn each applied once, and
`xem-ui/tests/panel.rs` the look row's drag on touch screens and a
headset's own rows.
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
and a click held as focus returns toggles nothing. Its canvas pictures go
to `build/web-test/first-person-*.png`. `web/tests/page.test.mjs` holds
the game's Select with the deck's SELECT outside a field.

Not done or not measured: in a headset the view still shows on the flat
virtual screen, without head tracking (Phase 8's immersive inspection is
later work), and no test plays inspection in a headset: the underhand
grab's and the controllers' Back's toggles, the right stick's turning,
the held-back buttons, the Binaural listener's view, the Original
renderer's end and the view carried across Enter and Exit VR have not run
on Monado or IWER (only the grab's rule and the headset core's gating are
unit-tested); the immersive view's qualification takes them up. The touch
look's rate is a constant chosen to follow the finger on a phone-sized
picture, not measured with people, and the Android emulator has not
played the touch toggle or drag (the unit tests and the browser's touch
pointers have). Keyboard-only and touch-only players have no toggle on the
ten maps where Select shows the map's picture (the deck has no right
stick's click; a gamepad's remains). Inspection is the player's alone: no
command or page call lets an agent turn the view on, off or around on any
host (agents see it through `status`'s `inspecting` and browsers'
`status().renderer`), and an agent's pad input and `assist` commands are
not held back while it shows. Whether hosts should give agents the view,
and hold their input back during it, is undecided.

### Camera records

A view that looks at the scene from elsewhere than the game's camera needs
the camera each frame was drawn with, to take the frame's view-space points
back into its world (plan.md Phase 8's seated diorama). The culled-face
ring's `around` word carries bits (`Reveals::around`, xem/gte.h
`XEM_AROUND_*`; 0 the game's own camera): `Reveals::LOOK` (1) for
first-person inspection, `Reveals::ANYWHERE` (2) for a view from anywhere
around a field, a battle or the world map. With either bit every face
counts wherever it lies, the field's culled scenery pieces and skipped
sprite actors are drawn for the record, and so are the world map's scene
objects its projection skips and the terrain triangles its winding test
drops (before, with a pulled-back camera only); the sprites the port draws
again facing the character's eyes come with `LOOK` only (`sprite_facing.c`),
the battle's and the world map's cameras with `ANYWHERE` only. No host asks
for `ANYWHERE` yet: the diorama view that will is the next part of Phase 8.
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
`XEM_CAMERA_FIELD`, `_BATTLE`, `_WORLDMAP`), so that a point's view is the
rotation times the world point, shifted 12 bits, plus the translation
(`present.c` `xem_present_camera`). Each mode records the matrix its frame
is drawn with where it is set:

| Mode | Where | Camera | Target |
| --- | --- | --- | --- |
| field | `field_draw_models`, with either bit (then the controlled character, `XEM_PROJECTION_EYE`) | `field_view.scaled_world` | `field_view.target` |
| battle | `sprite_set_view_matrix` (WRAPPED), when the battle sets it from `battle_camera.matrix` once a frame, after drawing its stage | the look-at matrix with the quake's offset | `battle_camera_view_target` |
| world map | `worldmap_objects_draw`, which every world-map frame calls (the actors' sprite pass that also sets the sprites' view matrix runs in the open map only) | `worldmap_camera_matrix`, which maps points relative to the camera's target with z negated, as the world map places its objects | (0, the target's height, 0), the point the orbit and look-at cameras look at |

The trace hands each one over in order with the commands
(`TraceOp::Camera`: `xem_gpu::Camera` with its axes, translation, target,
focus depth and `CameraMode`; `Camera::world_point` inverts it,
`Camera::view_point` applies it). A buffer's log keeps the records current
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
`camera_records_leave_ram_vram_sound_and_time_unchanged` runs 240 frames
from the same state in each of the four with `around` 0, 1 and 2 and
without a trace: one RAM hash, VRAM, sound and clock each. One
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
`a_recorded_camera_comes_before_its_frame_and_its_inverse_gives_back_world_points`
and `tests/test_port_presentation.py` (the entries `xem_present_camera`
writes) check the format and the logs without the game.

Not done or not measured: no host shows a view from anywhere yet, so
nothing presents these records; the cost of `ANYWHERE` in battles and on
the world map is not measured; the world map's scripted scenes, the
arena, menus and movies were not run with it; the first battle's camera
turned while it attacked, the desert's only came in (0.9 degrees); of the
real game, only the desert was presented several frames at a time; a frame
built with the bits before a change is not told apart.

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
projection; nothing claims reconstructed 3D. The game runs from the host's
clock (at most four catch-up frames per host frame, so a 15 Hz session still
runs all 60), once per headset frame, never per eye; the settings use the
Headset class (Low tier, depth of field off unless enabled).

- **OpenXR** (`xem-desktop --xr --disc DISC`; the Android host's `--xr`
  argument, for a standalone headset such as Horizon OS, enters the same loop
  once the game is booted, with the activity and JavaVM as the loader's
  platform data; `crates/xem-host/src/xr_play.rs`): the
  screen 2.5 m ahead (Settings: headset screen), the settings panel a quad
  layer (left menu button), movie frames on the same screen; controllers
  are the game bindings' gamepad ([Controls](#controls)). One
  `xrframe` line per frame. `src/runtime/scripts/xr-game-smoke.sh` (Monado's simulated
  HMD, null compositor) checks 57-63 game frames a second against the
  simulated 20 Hz headset, sustained pacing (wall-clock frame intervals: p50
  50.06 ms, p99 under 57 ms, at most 5% beyond 1.5 periods; measured on
  lavapipe), a paused game with head tracking and rendering
  continuing, the RAM at frame N equal to `xem-headless`'s, stereo eye
  captures of the screen, and an HD-2D field on it.
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
  exit, and an unchanged RAM hash. The session asks for `hand-tracking`;
  each frame the page passes the viewer's pose, the session's visibility,
  each hand's target ray and its 25 joints to the headset core
  ([Headsets: hands, gaze and control decks](#headsets-hands-gaze-and-control-decks)),
  whose Pause and settings hold the game's clock; the settings panel is a
  quad below the screen in the session (0.55 m wide, tilted toward the
  player) that takes the pointer source's ray with a pinch, or a
  controller's ray with its trigger (`select`). The button reads Enter VR
  or Exit VR; a hidden session (the browser's own menu) or its end lets go
  of held input.

In a field both toggle first-person inspection with the underhand grab
and the controllers' Back (the right stick's click) released alone, where
Ex Machina draws it (the Original renderer ends it, and neither toggles
it there); it shows on the virtual screen, its view turned by the right
stick, the player's input leaving the character where it stands as on the
flat hosts ([First-person inspection](#first-person-inspection)) and the
Binaural listener looking along it before the head turns it. The settings
panel's Controls section lists the grab, the right stick's click and the
right stick in place of the flat hosts' keys (in a browser, while the
session runs). The canvas and the session share the browser's view, so it
carries across Enter and Exit VR.

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

In a field, Select (where the game's shows nothing) and the right stick's
click also toggle first-person inspection, which the arrow keys, the right
stick, a finger on the picture and the mouse camera's motion turn
([First-person inspection](#first-person-inspection)); Settings' Controls
section lists them (with the finger's drag where touch controls show; a
headset's lists its underhand grab, the right stick's click and the right
stick instead, [Headsets](#headsets)).

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
player's layer (`pick`); routes plan over the triangles the player may
enter (an attribute blocks by the actor's flags, bit 23 blocks layer 0, a
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
adds Board (L2 and R2 together: a Gear boarded or left), a shop's quantity
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
  keys beside it. An unavailable source aims at nothing; nothing else
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
  pinches (two of them scale, owning both hands until both open); prayer
  hands (open aligned palms near-touching, once until they part). A
  closing hand emits no pinch or thumb event, nor does a pinch held
  through a grab once the grab ends.
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
  targets that overlap or hide one another. Presses queued for one
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
  player's Pause (from an open hand). Two-hand scaling leaves the screen's
  size while inspection shows (the gesture still owns both hands, so no
  grab fires as they close).
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
  transparency makes bodies dark and opaque) and composited as room or
  screen quads; `cargo run -p xem-render --example glass` draws a review
  sheet. Settings and other application interface stay plain.

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
xem-assist --example reliability`, 1000 tries each): grabs and prayer
100%, latency within a frame of the closing pose; 30 minutes of idle play
(relaxed, curling, reaching, turning hands) fire nothing; 200 pause grabs
a twelfth of a second apart toggle 200 times; a hand lost mid-grab and
found closed toggles nothing; every one-hand gesture works with either
hand alone. The standard gestures' reliability is the runtime's. The XR
game smoke plays scripted gestures in OpenXR (`--xr-hands`: Pause,
prayer hands, a pinch on the panel's Close, a pinch that walks) and logs
the hand gestures the runtime offers; the browser's
`tests/xr-hands.test.mjs` drives IWER's hands (grabs, prayer, lost
tracking, a standard pinch that counts once and the pointer it makes)
and controllers (the trigger on the panel's Close).

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

`src/runtime/scripts/suite.sh [desktop|web|android|xr|full]` runs the tests
from anywhere in the repository, in the runtime shell. While developing,
run only the platform whose host is changing, and `full` once at the end,
before review or a merge (AGENTS.md): `desktop` (the default, for the
native and shared code) builds the game module and runs the port's unit
tests, the Rust workspace's tests and the validation run; `web` the web
build, the browser suites and the browser tests that measure time
(WebXR's among them); `android` the APK and the emulator's smoke test; `xr`
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
nothing). What measures frame rates then runs alone, one after another,
since their emulator, headset and browsers render on the
processor (beside the browser suites the Android smoke's frame rate fell
from 56 to 10 a second, the XR smoke's headset frames came late, and their
checks failed): the Android smoke, the XR smoke, and the browser tests that
measure time (the headset's frame rate, rewind frame times, fast-forward's
music, the stalled page, a resume read within the rewind's one-second
interval, the Binaural mode's cost). Rust builds use the `suite` profile
(release's optimization without link-time optimization or debug
information: after a change to xem-gpu the test binaries rebuild in 37 s,
against release's 161 s); a failing test target is built again with line
tables (`suite-debug`) and run with a backtrace into `rust-debug.log`. A
module build that changes nothing keeps `build/game/game.wasm` and its time,
so nothing downstream rebuilds (play.sh compares the sources with
`build/game/stamp`, which each finished module build touches). The Android
smoke, run by itself, boots its emulator while the APK builds
(`XEM_SMOKE_BOOT=1` only boots it, and a later run uses it), and turns off
the system's "isn't responding" dialogs on its AVD (on a loaded host one
took its taps). Logs, and each stage's result and time: `build/suite/`.
Measured here (16 threads, warm builds): `full` 723 s with every stage
passing (709 s before CI's checks joined it), where the same stages run
one after another took about 26 minutes; `desktop` 114 s.

## Saves and time controls

The app Settings screen has Audio, Graphics, Controls, Shortcuts, Time, Saves
and About sections ([Desktop host and settings panel](#desktop-host-and-settings-panel)).
Desktop widths use a sidebar; narrow windows use a strip of section tabs. Rewind and
the notched FF rate slider live in Time, with compact interval and history fields
and the retained queue's available seconds and memory usage. Graphics contains
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
and binding controls, and windowed play their file pickers. On desktop,
Android and in the browser these operations also take keyboard and gamepad
chords. Native headset play (OpenXR) reaches them only through its Settings
panel: its controllers and hands do not reach the app bindings. In a WebXR
session the operations asked for there, from the Settings panel or a
controller's chord, queue: they run at the page's next key, pointer or
window-frame input (after the session, or a key of an attached keyboard).
Fast-forward is an
on/off toggle, bound to backtick (`Backquote`) or Back+RT by default, with no
Settings checkbox for the toggle. Its separate
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

Rewind runs while R (`KeyR`) or Back+LT is held and resumes on release.
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
independent of FF speed; paused time is excluded. The interval cannot exceed
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

Ordinary saves still use the original game save menu and its two memory cards.
Settings can import or export an exact 128 KiB raw `.mcd` card. An import
replaces the inserted card at once, closing its open files; the browser
writes it to IndexedDB first, with play paused until that completes.
State backups use `.xemstate`; settings have a separate JSON backup. Native
writes acknowledge after writing, syncing and replacing the file. Document
providers on Android acknowledge after write, flush and close; their own
replacement guarantees apply.

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
as it is (the HUD's failure chip, the panel's status line; `Game::command`
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
{"cmd":"reset"}
```

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

Native file commands are `state_save`/`state_load` with `path`, and
`card_import`/`card_export` with `path` and `slot` (0 or 1). File commands are
accepted only as explicit native host requests; scheduled logs accept game
input, setup and pacing commands and cannot access files. Browsers use explicit
Settings operations or `window.xem.appOperation({Action: 'ExportState'})` and
the snapshot/card/settings automation methods. Browser file pickers require a
recognized activation; a refusal is shown in Settings and operation status.
Headset play (OpenXR's `xr_play`, the browser's WebXR session) saves and
loads through the same session. No Lua runtime exists yet (plan.md, Phase
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
failure chip.

Not done or not measured: an open file's later blocks' directory frames are
not compared (the bytes it depends on there are; the game's saves are one
block); a writer whose bytes so far match a later save's (the same save
made again) writes on, making the save it would have made; states taken
inside a save or a load are tested with the stand-in and the card device's
bench, not the original game's save menu; sealed but crafted values for the
entry, the stack pointer, fiber areas, the clock and the pads are accepted
and fail later in the bounds-checked module; the fingerprint sees only what
the stand-in states hold, so a variant added to the format is outside it
until a stand-in state holds one, and device semantics changed without a
layout change pass it.

## Gameplay options

### Random battles in platforming areas

The field's random battles come from one draw, `field_encounter_count_down`,
which player control runs while a direction is held on a map whose scripts
armed it (event f7; [formations](scripts/formations.md)). Phase 7 (plan.md)
turns them off by default in platforming-heavy areas, and
`xem_core::encounters::PLATFORMING_AREAS` is the reviewed list of those
areas: a name and the maps there that arm the draw, `platforming_maps()`
their union, ascending. No setting reads it yet. `python3 -m
tools.analysis.platforming` classifies both discs' maps (disc 2's 205 map
bundles are disc 1's of the same numbers): `--table` (the default) prints
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
cannot decide it and does not list it. Its name is the world map's for the
entrance fewest door steps away (at most two); the later areas' name
tables are Japanese and name nothing.

| Area | Maps | Party | Maps needing a jump | Traversal jumps | Reason |
| --- | --- | --- | --- | --- | --- |
| Blackmoon Forest | 22 | foot | 1 of 1 | 3 | From the early world map's entrance (entry 0) up to entry 1, where map 23's door and the later world map arrive, the floor rises 150-250 units over a staircase of eight stumps 50-95 units across (-40 to -257 high): 3 jumps up, none down. An actor there hops up the same stones ahead of the party (`arc_jump`), and a scripted battle waits at the top. Measured; 66 jump links, 23 falls. |
| Babel Tower | 407, 409, 412-416 | gears | 5 of 7 | 15 | 412: 5 (climb 6), 413: 3, 414: 2, 415: 3, 416: 2; 407 and 409: 0 as measured, their entrance 1 (the doors of maps 408 and 410, which the party crosses on foot) on a strip (word 0x48) that blocks the party in its gears. 412 and 415 have pairs on two layers or not joined, so the area's figures are lower bounds. 88 jump links, 39 falls. |

Unresolved, not listed, for the user's review (disc 1; disc 2 holds the
same maps 15, 632-634 and 641-651 and no other):

| Area | As measured | Not measured | Heavy if |
| --- | --- | --- | --- |
| Anima Relic 2, maps 641, 642, 644, 650, 651 (gears) | 2 of 5 (642: 2, 650: 1), 3 jumps | 641: no walk or jump on layer 1 joins entrance 2 (map 644's door) to entrances 0 and 1 (map 643's): its two platforms, both at y -360, lie 430 units apart over slides and pits, and layer 0's floor below them is 280-360 lower. 644 and 651: entrance 1 (map 645's door, crossed on foot) is on a strip (word 0x48) that blocks the gears. | 641 needs a jump (3 of 5). The classifier's first version listed the area by walking 641's floorless triangles. |
| Maps 444-450 (foot) | 3 of 7 (444: 1, 447: 1, 448: 2), 4 jumps | 449: its layer-2 entrances 0 and 1 lie in regions layer 2 does not join, entrance 2 is on layer 0. 450: its entrances are on layers 2 and 1. 445: entrances 1 and 2 (map 447's doors) are on floorless layer-0 triangles. 446 has one entrance. | 449 or 450 needs a jump (4 of 7). Without 449 and 450 the area is heavy (3 of 5, 4 jumps), and grouped by identical encounter-set bytes so are maps 444 and 448 (2 of 2, 3 jumps). |
| Maps 172, 173, 175 (gears) | 0 of 3, 0 jumps | 173: no walk or jump leaves entrance 0's region. 175: from entrance 0, walks and up to 6 jumps reach 19 regions, the highest at y -35, but not entrance 1's at y -323. 172 has one entrance. Both ways back need none. | 173 and 175 need 3 jumps between them |
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
listed maps' draws stop: battle 59's enemy 2 (Babel Tower, formation 6 of
maps 407, 409 and 412-416 on disc 1 and 412-416 on disc 2). With random
battles off there, it is not met at all. Battle 71's enemy 4 is met only
in Anima Relic 2's random battles (formation 0 of its five maps on both
discs), so listing that area would lose it too.

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
names, a two-map dungeon classified, listed and its lost enemy found, and
`--rust`, `--check` and `--map`. `encounters.rs`'s unit test checks that
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
--check` proves the shipped table.

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

## Desktop host and settings panel

`xem-desktop` opens an SDL3 window (Wayland or X11), presents through wgpu with
FIFO vsync and draws the game (`--disc DISC [--cards DIR] [--bios FILE]`; the
test scene without a disc), placed by the presentation settings, under the
Slint settings panel (Escape or a gamepad's Back+Start toggles it). The
panel is the launcher too: Saves' Import disc… opens SDL's file dialog and
plays the chosen disc (CHD, cue sheet or Redump-verified ISO), or gives it
to a running game's disc changer.
The panel (`xem-ui/ui/settings.slint`) lists its sections in a sidebar at
560 logical pixels wide and more, else in a strip of tabs above the section;
each section is a column of cards of settings, and the status line under
them says what the last change or operation did (a note, done, or a
problem in red). Controls edits
the game's bindings ([Controls](#controls)) and the assisted controls
(hand gestures, the headset pointer, Stop at ledges, touch controls,
reduced transparency and motion); Shortcuts lists every app operation with
its bindings and edits the chosen one in place. Cancel closes either binding
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
whole from validating takes its default alone, the others staying as saved
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
the right stick's click anywhere in a field; in it the arrow keys, the
right stick and the mouse (under the mouse camera setting) look around
([First-person inspection](#first-person-inspection)).
`--control` answers agents' JSON commands on stdin, a line each, between
frames while the game plays: the command layer's (`status`, `context`,
`navigate`, `act`, ...), `settings`, `settings_apply {change}` (the panel's
own typed, validated, persisted change), `panel {visible}` and
`import_disc {path}` (the launcher's path); nothing passes through the
window system.
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
  says which, and `status().renderer.picture` where the game's picture lies
  on the canvas (canvas pixels) and the screen it shows (its display size
  and the wider framing's sides). With a session the canvas shows the GPU device's display
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
  the gamepad's Back alone or its right stick's click toggle first-person
  inspection, which the arrow keys, the right stick, a finger on the
  picture and the mouse under pointer lock (both its motions: the page
  passes `movementY` too) turn
  ([First-person inspection](#first-person-inspection)). The page keeps
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
  transactions. A WebXR session's frames replace the window's and write
  nothing: dirty images wait for the session's end, an automation step or
  an explicit retry. A failed write retains
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
  IndexedDB and inserts it into the live session; import pauses play until the
  write completes. `npm run test:slice` (local, with disc 1)
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
  The URIs of the last three imports are kept in the app's files directory
  and reopened on start.
- **Game.** Once the reopened documents are identified, disc 1 (else disc 2)
  boots the game from the same descriptor (`xem_game::boot_source` over
  `xem_disc::open_reader`), with the memory cards in `<files dir>/cards`;
  the settings panel hides. The other disc, and a disc imported while the
  game runs, go to the drive's changer for the game's disc changes
  ([Discs](#discs)) instead of starting it again. Like the
  desktop, `xem_host::Game` runs the frames due at 60 per second of wall time
  (at most two per rendered frame, never catching up), the GPU device's
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
  which reloads settings and reopens the imported disc. `XemActivity`'s
  `onDestroy` sends the run its quit and waits up to 5 s for it to end
  before SDL's own, which waits a second and then drops the quit: a run
  still booting the game when Android recreated the activity twice in a row
  missed it, kept its window, and the next run could not create one ("Android
  only supports one window"). A run still waiting for its first surface ends
  when the quit comes.

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
