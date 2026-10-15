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
  writes the map (and the entrance, `entry`) into `game_data`, selects mode 1
  and lets the menu finish into the original dispatcher. The entrance is
  `game_data.entry[2]`, which the field reads as its v0002 and places the
  party by, as a door's map change leaves it (`entry[0]` is the heading).
  `enter_field {map: 23}` reaches the
  forest map's opening dialogue (`tests/route.rs`) and matches the original
  capture of the same recipe (tools/reference/scenario.py) within 0.013 mean
  colour difference; on map 414 each `entry` puts the party on the map's
  entry record of that number (`tests/route.rs`).
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
  diorama shows, the seat's axes through its table) and a headset's head
  (`xem_core::binaural::head_axes` of the OpenXR or WebXR viewer pose as
  the seat sees it, the seat's forward its reference, so a recentred seat
  is ahead again; lost tracking keeps the last; leaving the headset clears
  it). The hosts' audio stays plain two-channel stereo at
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
sounds the same every time, and a restore, which jumps from one moment's
sound to another's in the original too, stands out no more there (the
click measure below: 1.0 and 1.5 dB less than the original's). In the
forest's battle (the slice's route, two sessions from one state at frame
6 000, plain and under Binaural) the combatants' sounds are placed at their
sprites (9 effects) around the formation's centre, which stays put through
the battle; read apart from the presenter, each sprite's position from
that centre through the sprites' view (`sprite_view_matrix`, the battle
camera's) is where it is heard within 0.11 degree, 1 899 times. That
battle's camera turns only in its opening sweep, before any combatant
sounds, so both sessions turn the yaw it frames the party from
(`battle_camera_framing_angles`) six times, and the game's own framing and
easing swing the camera over 97 frames: 196 placed directions follow it,
703 degrees in all. The battle's scripts start 16 effects no sprite's
command started: each is a coverage gap of the battle mode, heard in its
original mix (9 022 voice frames). The battle music's 20 parts keep their
places through the swings, and the swings, the 83 source switches and the
return to the field (frame 9 515, where the game's mode goes from the
battle back to the field) stand out at most 0.61, 1.10 and 0.62 dB more
than the original's in the high band. In Lahan's opening scene (field 2,
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
and a jump), eleven sessions from one state (plain Stereo; Binaural with its
defaults, bypassed, music only, effects only, immersion 0 and 100;
head-tracked music under a head sweeping 90 degrees either way; sweeping
first-person axes; the settings changed every 120 frames, off and on again
among them; a game view drawing every frame zoomed out four times with
wider framing, characters in view off, the port recording what it reveals)
have the same game memory (`memory_hash`), voice volume
registers and current volumes every frame and the same SPU every 60
frames, and every placed source reports exactly the gain the driver staged
for its voices (13 152 source frames whose voices held still and sounded
through the frame, 1 606 of them effects; within 0.3% of the driver's own
gain before panning, recomputed from its records by the decomp's formula,
which is the pan law's rounding); the zoomed-out session presents exactly
what the plain Binaural one does, status and sound alike (the presenter has
no zoom). A twelfth session keeps the field's characters moving in that
zoomed view (`set_actor_view`, as the hosts do with characters in view):
its game differs, the original's nearest three stay the same on this walk,
and its placed sources still report the driver's gains (13 152 checked).
In those sessions the field reader (`xem_core::field::Field` and the
emitter slots, apart from the presenter) puts each placed emitter where it
is heard, through the camera matrix the game draws with, within 0.5 degree
(0.21 at worst: the matrix's rows differ in length by up to 0.8%, and the
presenter takes their directions), 3 786 times each: in front or behind as
its camera-space depth says (3 382 and 384 times), above or below as its
camera-space height says (2 746 and 806), on the side the original pans it
to (4 281 voice frames), and through the first-person axes or the head as
they turn. The camera's turns (71 frames with the character and the
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
degree. Clicks: where sources switched (28 frames) and where the settings
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
way (within 2), the stage staying put. `tests/test_listening.py`: the
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

Not done or not measured: no listening tests with people: the
self-administered test is ready and has been answered only by a script,
and whether one listener beside the models makes the plan's calibrated
listening comparisons is the user's to judge; its analysis sets a person's
numbers over the test's 48 directions beside the models' over every
measured direction (the models' output holds no direction-by-direction
predictions). Direction is
predicted by models calibrated on 23 people's own localization, for how
they would hear the KEMAR head's cues, not measured on anyone; the models
are validated for static broadband sounds, so moving sounds, the game's
effects and music lie outside them, and the lateral model reads only
interaural delays, so the Stereo control's level differences go unheard
there (people hear those inside the head). barumerli2023's full-sphere
predictions are not run (Octave, above), and no regression port of the
models' numbers runs in the suite (the AMT is a local tool). Musical
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
memory, not reached in the game; headsets and phones only through IWER and a
throttled processor (OpenXR's Monado run does not check the presented
head). The battle camera's swings the battle test hears are its own (the
framing yaw turned in both sessions): the retained battle's camera turns
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
| headset | the view: the screen (default) or the diorama; the screen's distance and width; the diorama's scale (25-400%, 100), the table's height (-100 to 30 cm, -45), distance (30-200 cm, 60), tilt (0-45 degrees, 0) and turns (Smooth, Snap, Fixed); passthrough (off; no host offers it yet); depth of field in a headset (off) |

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
across as the screen spans in an eye (its width, times two-hand scaling,
over its distance, in the eye's pixels per unit of tangent), at most the
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
map is left, and under the Original renderer. It begins only where the
first frame shown with the field camera's record (the port records it
from the frame after the view asks) draws the field's scene as the game
drew it (its map, models or sprites, not only faces recorded for a wider
view): the title screen is field 490, whose camera shows none of its map
(the game culls it whole behind the logo), so a toggle there ends at the
next frame instead of looking around a room the game never shows or
holding back the title's D-pad. A headset's eye pictures begin anew with
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
`Session::set_head` adds once. Turned off, the screen is back where it
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
turns the view on over a field's frames of its interface alone: it waits
through the trace's start (no camera record) and ends at the first frame
with one; over a field's scene it stays; an eye that saw a green floor,
the view off, another frame with the floor blue and the view on again,
sees blue (held from before, it saw green: the tour's title).
`xr-game-smoke.sh` plays it on Monado ([Headsets](#headsets)) and
`web/tests/xr-game.test.mjs` on IWER, whose scene tour toggles it with
the left hand's underhand grab in each scene (only in field 30 and
field 12's dialogue does it look from within, the title included among
the rest).
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
stand still). OpenXR's own calls have not run in a session: the
held-back buttons (`xr_play`'s `look::game_buttons`), the Original
renderer's end, the controllers' Back toggle, the snap turn and the
listener's view (Monado's simulated controllers, `khr/simple_controller`,
have no stick, and the smoke plays without the Binaural mode). The rules
they share are unit-tested, and WebXR's toggle, snap turn, listener and
Original end ran on IWER (its held-back buttons are the browser page's
code, which `first-person.test.mjs` runs). The view has not been carried
into a session either (only out of one). The touch
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
the battle's and the world map's cameras with `ANYWHERE` only. `GameView`
asks for `ANYWHERE` while its headset view is the [Diorama](#diorama) of a
field, a battle or the world map.
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

Not done or not measured: the [Diorama](#diorama) presents these records
in both headset hosts ([In a headset](#in-a-headset)), where only field 30
and field 12 ran; the cost of `ANYWHERE` in battles and on
the world map is not measured; the world map's scripted scenes, the
arena, menus and movies were not run with it; the first battle's camera
turned while it attacked, the desert's only came in (0.9 degrees); of the
real game, only the desert was presented several frames at a time; a frame
built with the bits before a change is not told apart.

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
  the diorama by 70%, fading over 0.25 s. A battle keeps the point and the
  heading its first frame had (its cameras orbit and cut, and its input is
  not camera-relative); the world map's points already follow its camera's
  target. The table's volume is x and z within 0.45 m of its centre and y
  from 0.25 m below its top to 0.55 m above: what lies outside is not drawn
  (draw.wgsl discards it).
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
  width, a line there a ribbon a native pixel wide. Flat primitives drawn
  after it (windows, text, portraits, menus, the compass and other
  interface models, battle numbers) and pictures uploaded to the screen
  go onto the interface pane, drawn as the screen
  draws them into a transparent premultiplied texture (`PASS_PANE`): an
  opaque pixel covers, mode 0 is half the colour at half coverage, modes 1
  and 3 add without coverage, so over the room the pane shows as the
  original blends it; mode 2 (B-F) has no such form and darkens by F's
  brightest channel. A full-screen untextured one (a fade, a flash), or a
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
  A headset frame the host shows without the view (the device's own
  picture: the Original renderer, a 24-bit movie frame the view does not
  draw) calls `hide_diorama`: the diorama lies nowhere until it is
  presented again, and then eases in from the screen.
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
  the diorama and the profile has effects a headset draws, and the profile
  stays as chosen: the screen the diorama gives way to keeps them). The
  screen's limits have no place in a view from anywhere:
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
`frames_without_a_camera_record_or_in_other_modes_show_the_screen` (and
the screen easing into the diorama over 0.3 s, and the screen view);
`the_listener_takes_the_tables_axes_only_while_the_diorama_is_presented`
(the seat's axes through the table while it shows; none after
`hide_diorama`, then the share 1, 2/3, 1/3 and 0 at +0, 0.1, 0.2 and 0.4 s
as it eases in again; none under the Original renderer, whose settings
give the screen);
`leaving_the_diorama_eases_to_the_screen_as_the_last_diorama_fades_out`
(a menu's frame, a 24-bit movie frame and first-person inspection after a
diorama: the screen's share 1/3, 2/3 and 1 at 0.1, 0.2 and 0.35 s, the
floor of the last diorama in the eye at one minus it, and its pane);
`frames_without_a_camera_record_or_a_scene_keep_the_diorama_for_two_frames_then_ease_to_the_screen`
(frames of a fade without the scene a vertical blank after the last
record, which they take, and field frames five blanks after it: the
diorama before for two frames, not an empty table, then the share rising
0.11, 0.56, 1; the field with its camera again eases back in);
`the_diorama_is_built_once_a_game_frame_while_the_table_turns` (the
camera turning 3 degrees a frame, each frame presented three times at
90 Hz: 10 builds for 10 frames; paused, one build in 90 presentations as
the table turns on); `nothing_outside_the_tables_volume_is_drawn`. The
unit tests in `ps1/diorama.rs` check the build: the backdrop on one plane and the
interface on the pane only, never in the eyes' pass; a sprite's parts
upright in one plane facing the seat at its floor point, as tall as
drawn, the second carrying the lift; split sprites standing together on
their floor point and a jumping one above it; a shadow lifted over a
floor and a face beside it not; which faces may be dropped from behind;
fills after the scene over part of the screen on the pane and one over
all of it a tint; a line before the scene a ribbon on the wall and a
frame without a scene built without fault. They check the placement:
the table level under pitched and rolled cameras (the world's up its up,
the camera's level axis away from the seat); a 20-degree turn eased and
a 60-degree one a cut that jumps and dims for 0.25 s, as does a target
jumping beyond a third of the table; Snap holding within 30 degrees and
jumping past them, Fixed keeping the first heading; a battle keeping
its first point and heading whatever its camera does; a new scene
beginning its own point and scale (half at twice the depth).
`xem-headless/tests/render.rs`
`a_diorama_seen_from_a_moving_head_leaves_the_game_unchanged` presents
240 frames of field 12 from the same state as a diorama for two eyes of a
swaying head, the scale and the turns changing midway, and ends at the
RAM hash of the same frames drawn by nothing.

Not done or not measured (the headsets' own: [In a headset](#in-a-headset)):
the pane's place is a first choice, not one tried by anyone seated; HD-2D
effects per eye were not attempted; battles and the world map were
reviewed in one scene each, not their camera modes, cuts or scripted
scenes; the one-sided rule drops a double-sided face the game showed from
its front once an eye sees it from behind; surfaces the game never models
stay holes; the world map's distant terrain lies beyond the volume at
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
inspection takes the eyes over while it is on (the diorama gives way to
the screen then).

- **Settings** (`Graphics::xr`, the panel's Headset card in Graphics, the
  same panel in a browser): the view, Screen (the default) or Diorama; the
  diorama's scale (25-400%, one value for every scene, 100% by default),
  the table's height (-100 to 30 cm from the seated head, -45), distance
  (30-200 cm, 60), tilt (0-45 degrees, 0) and turns (Smooth, Snap, Fixed);
  and passthrough (off; its row shows only where a headset offers it, and
  none does yet). The settings service validates them (`diorama scale must
  be 25-400 percent`, ...). Reset diorama puts the scale, place and turns
  back (the view, the screen and passthrough stay). The screen's distance
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
  progress (each hand opens again before the next), lets go of a control
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
  device's own picture in (`GameView::hide_diorama`) has no diorama.
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
  settings panel where the pane floats, its lower edge there (over the pane
  while it shows: the game is paused), as wide seen from the seat as below
  the screen (`draw::panel`). The keys beside the pane look 0.72 to 1.19
  times as large as the screen's from 2.5 m over the settings' range of
  height, distance and tilt (the screen's own, off its middle, 0.81-0.83),
  the HUD 0.9995 to 1.0000 times as large as on the default screen, and
  both the same at every scale.
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
  no floor shows none, and on the pane the cursor is flat. Battles take
  their targets from the pane and the target keys.
- **The listener.** The Binaural head is the head as the seat sees it.
  While the diorama is presented, the listener's axes are the seat's right,
  down and forward taken through the table into the game camera's view
  space (`DioramaSpace::listener_axes`), so the head's turn composes once
  with the table's heading and tilt; the music's stage, turned by the head
  alone, stays put in the room. Otherwise (the screen, the Original
  renderer, the device's own movie frames) the listener has the screen's
  axes: none, or first-person inspection's. The listener stays at the
  character.
- **Logs.** OpenXR's `xrframe` line adds `xr_view=` (`screen` or
  `diorama`, the setting's view), `diorama=` (shown),
  `share=` (the screen's), `builds=`, `seat=` (its place; its turn,
  radians), `resets=` and `recenters=`, and an `event
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
panel above the table (its quad layer shown from frame 61 to 92, the game
held) and a pinch on its Close closes it, and a pinch through the table
at the world point (88.67, 0, -55.33) of field 30 walks Fei there
(moving, then arrived). `web/tests/xr-diorama.test.mjs` (IWER's Quest 3; field 30):
the automation's 120 frames end at the same RAM in and out of the headset
with the diorama on; the table covers rows of the left view; moving the
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
5.8 away).
`xem-headless/tests/gestures.rs`
`through_a_diorama_a_pinch_walks_to_the_floor_gazed_at_and_the_pane_shows_none`
(field 30, the table placed from the frame's own camera record as a
headset places it: a pinch at a walkable floor point on the table heads
the walk within 2 units of it and arrives within 16; one on the pane walks
nowhere) and `through_a_diorama_a_tap_by_a_villager_talks_to_them` (Lahan:
the villager approached and talked to). Unit tests: the settings' bounds,
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
a ray through the table becoming the world's, the seat, and the shared
reference handler cancelling a grab in progress, ending scaling and
letting a bout's stick go (`xem-assist`); the pane upright above the far
edge at every tilt and scale, the listener's axes through the table, and
the settings' mapping, the screen under the Original renderer
(`xem-render` diorama.rs); the listener's axes only while the diorama is
presented (`xem-render/tests/ps1.rs`, [Diorama](#diorama)).

Not done or not measured: passthrough and immersive AR (the setting exists,
no host offers it yet); scaling the diorama with two whole-hand pinches
(two-hand scaling still scales the screen, which the diorama gives way to);
battle targets through the table (they come from the pane and the keys);
the world map and battles were not run in a headset; on Monado's simulated
HMD `monado-ctl -c` delivers the event but the head's poses do not jump
(nothing visible moves LOCAL there), and no controller binding recentres
by default; comfort is not measured (no people tests) and nothing ran on a
Quest; marks are drawn over the diorama whatever stands in front of them;
a ray through the table meets the floor through scenery standing in front
of it (no depth test: a floor behind a wall can be pointed at); the HUD's
place and size beside the pane are checked by the unit test alone, not in
a host's eyes; the browser's panel is not opened in the diorama view, nor
its pane's pixels checked, in IWER (the OpenXR smoke does both).

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
the diorama gives way to it ([In a headset](#in-a-headset)). Everything
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
  the settings panel above the table opened and closed, and a pinch
  through the table walking Fei; and in field 12 the dialogue on the pane
  in both eyes ([In a headset](#in-a-headset)). Its
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
  rewinding behind the closed panel until its history ran out).
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
  exit, and an unchanged RAM hash; its movie test shows the title movie
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
  quad below the screen in the session (0.55 m wide, tilted toward the
  player; above the table with the diorama as the view) that takes the
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
  as the player's play does). It then clicks the panel's Saves
  and Save state with the right controller's ray and trigger (the state
  in IndexedDB, its checksum verified, the HUD's "State saved", the game
  held by Settings' pause), turns fast-forward on with Back+RT (the right
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
and the controllers' Back (the right stick's click) released alone, where
Ex Machina draws it (the Original renderer ends it, paused or not, and
neither toggles it there), whether the game runs or the player's Pause
holds it (not while Settings are open). Turned on during a pause, it
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
`scale=` and `present_us=`; `--xr-capture-at N,...` captures both eyes
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
headset's lists its underhand grab, the right stick's click, the head and
the right stick instead, the stick turning in 30-degree steps,
[Headsets](#headsets)).

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
pinch, a tap or a click picks the floor and the actors the picture shows
where it points, and floor marks lie where it shows that floor (desktop
and Android's taps and clicks, OpenXR and both browser paths).
First-person inspection's look has a viewpoint too, but the hosts pass
no field while it shows (nothing is approached from within the scene).
An agent's `navigate {screen}` is the game camera's screen. Tests:
xem-core's `a_camera_seen_from_another_view_projects_and_picks_as_its_picture_shows`
(pulled back about another centre, and turned within the scene), xem-render's
`a_pulled_back_or_inspecting_picture_has_the_viewpoint_it_shows_the_scene_from`
(a synthetic field at 250%: floor points show on the floor's colour where
the viewpoint places them and its rays meet them, where the game camera's
projection shows no floor; from within the scene, the floor and a wall
where the look's viewpoint places them) and xem-headless's
`pulled_back_a_pinch_walks_to_the_floor_the_picture_shows_and_marks_it_there`
(field 30 at 250%: the camera 6 915 units back, a floor point shown 46
pixels from where the game camera puts it; a pinch there walks to it and
arrives, the cursor and the destination's mark lying there; pointed
through the game camera instead, the walk heads 125 units away).

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
adds Board (L2 and R2 together: a Gear boarded or left), a field's scenes
and dialogue add AUTO above the face buttons (auto-advance dialogue on or
off, lit while on: [Text speed, auto-advance and movie
skipping](#text-speed-auto-advance-and-movie-skipping)), a shop's quantity
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
  pinches (two of them scale, owning both hands until both open, the
  frame they let go on included); prayer hands (open aligned palms
  near-touching, once until they part). A closing hand emits no pinch or
  thumb event, nor does a pinch held through a grab or a scaling once it
  ends, and the runtime's pinch or thumb events during a scaling (a
  runtime may take gathered fingertips for a pinch) are the scaling's.
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
that walks) and logs the hand gestures the runtime offers; the browser's
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

The app Settings screen has Audio, Graphics, Controls, Shortcuts, Time, Saves,
Gameplay and About sections ([Desktop host and settings panel](#desktop-host-and-settings-panel)).
Desktop widths use a sidebar; narrow windows use a strip of section tabs. Rewind
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
icon shows over the panel too). However the panel closes, and when the
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

Native file commands are `state_save`/`state_load` with `path`,
`card_import`/`card_export` with `path` and `slot` (0 or 1), and
`bios_import` with `path`, which gives the running game a BIOS image's Kanji
font as Import BIOS does ([Save titles and the Kanji
font](#save-titles-and-the-kanji-font)). `card_import`
reads any memory card or save file as Import original card 1/2 does
([Original saves and memory cards](#original-saves-and-memory-cards)) and
replies with what
it found (`message`, the status line's report) and the card's listing
(`card`); `card_export` writes the raw card. The shared, read-only
`card_list {slot}` reports a card on every host, the browser's
`window.xem.command` too, for agents and tests (no panel shows it): its
listing (`card`, none without a card: `formatted`, `free_blocks`, `problems`,
`notes` and `saves`, each with `name`, `first_block`, `blocks`, `title`,
`kind` and `problems`), whether the card is in (`present`) and why it cannot
change now (`busy`). The recorded `card_present {slot, present}` takes a
card out or puts it back ([Cards in and out](#cards-in-and-out)); `status`
reports both cards' (`card_present`) and where the Kanji font came from
(`kanji_font`: `bios` or `substitute`). File commands
are accepted only as explicit native host requests; scheduled logs accept
game input, setup and pacing commands and cannot access files or list cards.
Browsers use explicit Settings operations or
`window.xem.appOperation({Action: 'ExportState'})` and the
snapshot/card/settings automation methods. Browser file pickers require a
recognized activation; a refusal is shown in Settings and operation status.
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
failure chip. Every host's save states and time controls:
`xem-ui/tests/panel.rs`'s
`time_toggles_fast_forward_with_a_button_where_the_headset_and_android_smokes_press_it`
(Start fast-forward sends Toggle fast-forward and changes no setting, the
session's state turns it to Stop fast-forward, and the points the headset
and Android smokes press, at their panels' sizes and with a running
game's status texts, reach Saves, Time, Save state, Load state, the
fast-forward button and a held Hold to rewind, the headset smoke's let
go by `cancel_pointer`, as a closing panel lets it go);
`src/runtime/scripts/xr-game-smoke.sh`'s saves run (OpenXR on Monado:
the panel's Save state and Load state by pinches, Start fast-forward,
the Back+RT and Back+LT chords from scripted buttons, an agent's
`--control` command) and held run (Back+LT and the panel's Hold to
rewind under a scripted controller ray's trigger, both through Settings'
pause, and Settings closed mid-hold letting go of it; [Headsets](#headsets));
`web/tests/xr-game.test.mjs`'s
saves test (WebXR on IWER: the game's own save to card 2 in IndexedDB
during the session, Save state and Load state by a controller's ray,
Back+RT, Stop fast-forward, Back+LT through Settings' pause under the
HUD's rewind icon, a picker refused); `android-smoke.sh`'s
Settings by touch on the emulator (Stop and Start fast-forward, Hold to
rewind, Save state and Load state; [Android host](#android-host)).

Not done or not measured: in a headset the panel's Hold to rewind holds
only under an OpenXR controller's held trigger (a pinch or WebXR's
`select` is a click, which starts and ends it at once), so players with
hands alone have no rewind there; only a scripted ray and trigger hold
it in a test (Monado's simulated controllers press nothing and point
where no test chooses);
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
bench, not the original game's save menu; sealed but crafted values for the
entry, the stack pointer, fiber areas, the clock and the pads are accepted
and fail later in the bounds-checked module; the fingerprint sees only what
the stand-in states hold, so a variant added to the format is outside it
until a stand-in state holds one, and device semantics changed without a
layout change pass it.

### Original saves and memory cards

Settings → Saves → Backups' Import original card 1 and 2 (app actions on
every host, bindable like the others) take any file below and tell what it
is by its contents (`xem_core::devices::card_file::detect`; a raw single
save by its file name): a whole card becomes the card, a single save a
freshly formatted card holding only that save, written as the BIOS creates
and writes a file (blocks 1, 2, ... chained: the device's own create, which
the game's saves go through too). Either replaces the inserted card. The
status line says what was found: the file's container and the card's saves
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
call of the game's: it is refused, with the reason, while the game has a
file open on that card (a save or a load under way: the game's own saves
write a sector a frame), is checking it, or the cards are still busy with an
access (`Card::busy_with`; `card_list` reports it as `busy`). Natively the
file `cardN.mcd` takes the card before the device does, so a refused write
changes nothing. The browser pauses play, lands any pending card writes, has
the runtime read the file (`read_card_file`, refused while the card is busy
as above), writes the card to IndexedDB and only then gives it to the
session (`set_card_image`): a failed IndexedDB write leaves both as they
were. Were the card to become busy while IndexedDB writes (frames that
automation runs meanwhile), the session refuses it and IndexedDB takes back
the session's card. Natively, an import into a slot whose card file did not
load (a file of another size, a read error: the slot is left empty and the
file unchanged) replaces that file. The game reads its cards only in its menus'
file screens, which list them when they open and then act by name on what
they listed: an import while one of them shows leaves that screen's listing
as it was until it lists again (after its own save, copy or delete), so a
save it overwrites or deletes then is the one of that name on the card now.
Importing before opening the game's file screen avoids that.

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
the other card goes on, storage takes the new card) and
`an_imported_single_save_is_the_card_the_bios_makes_for_it_and_a_damaged_card_is_refused`
(a `.mcs` of a file the BIOS created and the game wrote gives that card byte
for byte; a damaged card and a text file are refused, nothing changed).
`xem-core/tests/persistence.rs`:
`a_card_changes_from_outside_the_game_only_between_its_card_calls` (the
stand-in's save holds its file open across frames: a DexDrive import is
refused and changes nothing, `card_list` says why; the save commits and the
import then applies); `scheduled_file_operations_are_rejected_before_any_command_is_queued`
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
`web/tests/persistence.test.mjs`'s "a card or save file imports into a card
by its contents, and a refused one changes nothing" (the fixture module: a
`.mcs` before a boot, in IndexedDB and then in the session, listed by
`card_list`; `.psv`, `.psx` and raw saves and DexDrive, VGS, PSP and raw
cards into the running session's card 2 with each report; a damaged card, a
text file and an unnamed raw save refused; IndexedDB's write failing on
quota with card 1 left as it was in both; Import original card 2's own
picker, on a key, offering every extension and reporting a raw save on the
status line).

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
game's file screens keep their listing across an import made while they
show (described above; the host does not take the card out and put it back
for them); a file screen's own operations over imported saves are tested
only through the slice's load, not its copy or delete.

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
the same. A refused file changes nothing and the status line says why
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

Not done or not measured: on a console a card put in is flagged new until
it is written (psx-spx, "Memory Card Read/Write Commands"), and the BIOS's
check may then report a new card, which the game takes as an unformatted
card's result (`menu_card_event_results`); this device does not model the
flag, and the game's handling of a real swap is not verified (no capture);
the save menus' own no-card and one-card paths (a field save point's, the
world map's, the disc change's) are not driven by a test, nor copy or
delete over a card put back.

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
| [Text speed](#text-speed-auto-advance-and-movie-skipping) | `text_speed`: `Original` (the default), `Fast`, `Instant` | `text_speed {glyphs}` | a message window's step reveals at least 4 glyphs, or a line (`src/port/text_speed.c`) | `status.text_speed`; `status.dialogue` (a page's prompt shows sooner) |
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
'Instant'})` answers `{ok: true, revision, settings}` or `{ok: false,
rejected, reason}`, and `xem.settings.get()`, `.export()` and
`.import(text)` read, back up and restore them. The next frame's `status`
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
settings file, and a second launch that starts with the choices. A
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
a compositor with no screen. The Android, OpenXR and WebXR hosts give the
values through the same `Game::update` and frame loop, and no run there
used the options on the game. No person tried the options.

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
shows the choice and, under it, the areas it turns off by name; the browser
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
section's choices apply through the service, are acknowledged, show the
note and follow an agent's change, and that the wheel never changes them.
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
| 4, the opening narration (`enter_field`) | 6 729 | 6 509 | map 2, story 3 | 0, 1 (`0F 02`) |
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

The opening narration differs in one way, which is presumably why the
mod re-scripted it. Each of its pages shows in a window while the
narration's script counts out the page's time, then waits for Confirm with
no prompt of its own (`31 0x0020` looping on itself, map 4 +0x1668). The
original's reveal lasts past that count, so the press that ends a page's
text also goes on; with Instant (or Fast) text the text ends before the
count does, a press then only closes the window, and the narration waits
for a second press. A player who presses only at prompts reaches map 2
after 9 731 frames with the original's text and is still waiting after
14 000 with Instant's; one who also presses when the narration waits
reaches map 2 with either (the table's row). The panel's note under Text
speed says that a scene timing its pages can wait for one more press. The
other scenes need no second press.

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
dialogue windows only: battle messages, and waits for Confirm with no
window (the opening narration's), still take the player's press. Measured
headless on map 482's entry scene (8 pages): a player pressing at each
prompt reaches control after 2 516 frames, auto-advance at 30 vertical
blanks a page after 2 740, at the same end and story value with the same
messages.

`AppAction::ToggleAutoAdvance` ("Toggle auto-advance dialogue", T or
Back+West by default, rebindable in Shortcuts) turns the setting on or
off through the settings service and says which on the status line.
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
blanks and closes.
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
opening are no wait. `xem-core/tests/persistence.rs` checks on the
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
the game's. `xem-ui/tests/panel.rs`
checks that the Gameplay section's three options apply through the
service, are acknowledged, show the wait only while auto-advance is on,
keep a refused wait and follow an agent's change.
`xem-headless/tests/text_and_movies.rs` (local, disc 1; about 80 s here
at four tests at a time) plays the table's scenes with the original's and
Instant text to the same end, story value and messages (the narration
also as the two-press difference above), checks that map 359's slow
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
unit-tested). Auto-advance does not turn battle messages or the opening
narration's window-less waits; the cut cutscene skip's battle-message
advance ([below](#cutscene-skipping)) is not offered either. Fast and
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
checked by `tests/test_port_text_speed.py` alone. Other scenes that time windows
against other scripts may need the narration's second press, and the
battle's, arena's and world map's windows were not measured under Fast
or Instant text. No HUD chip shows that auto-advance is on (AUTO's light
does, on touch screens and in headsets).
Of the mod's options that change the game's balance only half encounters
is offered ([above](#random-battles-in-platforming-areas), the field's
draw only); more EXP and gold is not. The
browser tests ran on the fixture module; the Android, headset and touch
hosts give the values through the same `Game::update` and frame loop, and
no phone or headset run used them on the game. A headset's AUTO key
(`Carry::toggle_auto_advance`, through OpenXR's and WebXR's app requests)
is unit-tested only: no Monado or IWER run aims at it.

### Cutscene skipping

Phase 7 (plan.md) asks for cutscene skipping that keeps the story's
outcome: the original scripts run unthrottled, Confirm is pressed at the
dialogue waits, and the skip ends at the next moment the player controls.
The user made it conditional on the game marking that moment clearly: the
skip stops at the first frame the marker is set and lands at the real end
of the scene on every tested scene, with no frame count or other
heuristic. The game's control grant, its only record of that moment, is
not such a marker, so the runtime has no cutscene skip. The measurements
follow. (A movie's end is such a moment: movies end on Confirm under
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
Saves at 405. Each section is a column of cards of settings, and the
status line under them says what the last change or operation did (a
note, done, or a problem in red). Controls edits
the game's bindings ([Controls](#controls)) and the assisted controls
(hand gestures, the headset pointer, Stop at ledges, touch controls,
reduced transparency and motion); Shortcuts lists every app operation with
its bindings and edits the chosen one in place (Recenter headset view among
them, with no default binding: the headset card's Recenter button and the
automation reach it); Gameplay holds the options
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
the right stick's click anywhere in a field; in it the arrow keys, the
right stick and the mouse (under the mouse camera setting) look around
([First-person inspection](#first-person-inspection)).
`--control` answers agents' JSON commands on stdin, a line each, between
frames while the game plays: the command layer's (`status`, `context`,
`navigate`, `act`, ...), `settings`, `settings_apply {change}` (the panel's
own typed, validated, persisted change; `SettingsService::command` answers
both), `panel {visible}`,
`import_disc {path}` (the launcher's path) and `bios_import {path}` (Import
BIOS's: kept beside the settings for every boot and given to the running
game); nothing passes through the window system. With `--xr` the headset
session answers the same commands but `import_disc` (`xem_host::Control`
and `SettingsService::command` serve both).
`xem-desktop/tests/control.rs` runs the desktop itself on weston's headless
backend with lavapipe (local, disc 1) and changes the gameplay options
through `settings_apply` ([Gameplay options](#gameplay-options)).
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
  time, at most two per animation frame and none after 12 ms, then one render;
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
- **Automation.** `window.xem`: `status()`, `boot({executable, run})`,
  `setRunning`, `command(cmd)` (any control command) with the shorthands
  `step(frames)`, `runUntil({maxFrames, until})`, `memoryHash()`,
  `snapshot(name)`/`restore(name)`, `exportSnapshot()`/`importSnapshot(bytes)`;
  the asynchronous `run(cmd)` and `runFrames(n)` (through blocked frames);
  `settings.get()`/`settings.apply(change)` (a `SettingChange`, e.g.
  `{MasterVolume: 40}`), `panel.show(bool)`, `importDisc(blob)`,
  `readSectors(lba, count)`, `save(slot)`/`load(slot)`,
  `cards.image(slot)`/`cards.stored(slot)`/`cards.flush()`,
  `cards.store(slot, image)` and `cards.importFile(slot, bytes, name)`
  (resolving to what it found, the status line's report). Each is the
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
  than on frames. `XEM_SMOKE_GAME_ONLY=1` skips the
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
