# Runtime (Phase 2)

The shared Rust/C runtime of [plan.md](../plan.md) Phases 2 onward. The recovered
C in `decomp/` stays the only game implementation; this document records how it
is built for native and browser hosts and how the hosts are layered. The
[later-phases handbook](later-phases.md) holds the program facts it relies on.

## Approved foundations

| Decision | Choice |
| --- | --- |
| Rust workspace | `runtime/` (`runtime/Cargo.toml`, crates in `runtime/crates/*`, committed `Cargo.lock`) |
| Toolchain | `nix develop path:./nix/runtime` (Rust 1.97.1 with wasm32/Android targets via rust-overlay, clang/lld 21.1.8, wabt 1.0.41, binaryen 132, wasm-bindgen-cli 0.2.127, SDL3 3.4.14, mesa 26.2.2, Monado 25.1.0, Chromium 152, Node 24); `#android` adds the Android SDK/NDK r29, emulator and API-34 x86_64 image, JDK 17, Gradle 8.14.4, cargo-ndk |
| Data model and link | the recovered C is compiled once to a wasm32 *game module* (ILP32, the original data model); natively it is translated ahead of time with wasm2c, in browsers it runs as is beside the wasm-bindgen runtime |
| Versions | slint =1.18.1 (`unstable-wgpu-30`, FemtoVG wgpu renderer), wgpu/wgpu-hal =30.0.1, sdl3-sys =0.6.8+SDL-3.4.14, openxr 0.22.0, wasm-bindgen =0.2.127, iwer 2.5.0, playwright-core 1.64.0 |
| Slint license | Royalty-free 2.0 (attribution shown in the settings panel); GPLv3 is the alternative |

## The decomp mandate

The decomp is the game-logic provider and stays a pure, byte-identical
implementation of the original game ([plan.md](../plan.md), Shared
native/browser architecture). It is compiled unchanged; nothing in `decomp/` is
added or edited for integration. When integration needs something the original
sources do not give, it goes into `port/` or `tools/game_module.py`: shadow
headers through the prelude, port functions, the inline-assembly map, IR
rewriting. A change to `decomp/` is made only for the original game and is
verified with `make -C decomp all-verify`.

## The game module

`python3 tools/game_module.py` (inside `nix develop path:./nix/runtime`) builds
`build/game/game.wasm` from the same C units the PS1 targets link and the port
layer in `port/`. The decomp is not changed for the port: its sources and
headers stay exactly what the byte-matching build compiles, and everything the
port needs lives in `port/` and the tool. It needs the matched PS1 links
(`make -C decomp all-verify`), whose symbol tables give every object its
original address.

- **Front end.** clang compiles the untouched C for `mipsel-unknown-unknown`,
  which accepts the original inline assembly, and the IR is retargeted to
  wasm32: both are ILP32 and little-endian with the same ABI alignments, so
  every structure keeps its layout. `port/include/xem/prelude.h` is
  force-included first; the headers it includes replace originals of the same
  include guard (`include_asm.h`, whose macros include nothing; PsyQ's
  `stdarg.h`, which walks MIPS argument words) and give canonical prototypes
  where a unit calls a function before declaring it.
- **Inline assembly.** Each statement (GTE transfers and commands, stack
  switches, `break`s) becomes a call to the port function that
  `port/asm_map*.json` names for its template and constraints.
- **Port layer.** `port/*.c` defines what the matched images take from
  assembly: the PsyQ SDK and BIOS calls (over host imports), the handwritten
  routines (from their `.s` contracts) and the entry and restart logic. A
  function the port does not define yet traps through `xem.missing`
  (`--stubs` lists them in `build/game/stubs.txt`).
- **Original memory layout.** Game memory is the PS1 address space: RAM at its
  KSEG0 addresses 0x80000000-0x801FFFFF and the scratchpad at 0x1F800000, inside
  one wasm32 linear memory. Every global the C defines resolves to its original
  address; its bytes come from the images the game itself loads (the host loads
  the executable as the BIOS did, the game decodes its overlays), so heap
  placement, overlay slots, overruns into neighbouring objects and pointer-laden
  data behave as on the console, and game RAM compares directly with captures.
- **Code addresses.** A function's address, wherever C takes it, is its original
  address. Indirect calls go through generated dispatchers that select the
  function by address and check, from a fingerprint of its original code bytes in
  game memory, that the image holding it is the one loaded there; a call into an
  absent or overwritten image traps.
- **Unprototyped calls.** A direct call whose types differ from the
  definition goes through a generated adapter (`build/game/adapters.txt`
  lists each): integer arguments are truncated or extended, a missing
  argument is 0 and an extra one is dropped.
- **Suspension.** `wasm-opt --asyncify` instruments the module so that the
  imports `xem.yield` (frame waits, polls) and `xem.restart` (the dispatcher's
  stack reset, soft reset) unwind the game stack into linear memory. The host
  resumes a yield by calling `xem_run` again and answers a restart by calling
  `xem_run(kind, arg)` on an empty stack; the dispatcher (`mode_dispatch`,
  wrapped by the port) therefore runs each mode from an empty stack, as the
  original does. A snapshot at a yield point is game memory, the module's
  globals and the host services' state.
- **Native.** `wasm2c` output is compiled into `xem-game` with explicit bounds
  checks (no signal handlers). Nothing interprets or emulates a CPU.
- **Browser.** The same `game.wasm` is instantiated next to the Rust runtime
  (wasm32-unknown-unknown, wasm-bindgen); the two modules keep separate
  memories and exchange scalars and bounds-checked offsets.

## Commands, introspection and snapshots

`xem_core::control::Session` is the one command layer for humans, agents,
tests and browser clients: a JSON command in, a JSON result out
(`xem-headless --control` reads one per line). Commands: `status`,
`frames {count}`, bounded `run_until {max_frames, until: {vblanks} |
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
  `game_data.characters[0]` or `mode_table[1].entry`; `write` stores scalars.
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
  clock. Headless runs are unlocked (as fast as the host computes) and
  reproducible.

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
| `xem-settings` | typed application settings: validation, acknowledgement, change notification, JSON persistence |

## Desktop host and settings panel

`xem-desktop` opens an SDL3 window (Wayland or X11), presents through wgpu with
FIFO vsync and draws the test scene as the stand-in game image, placed by the
presentation settings, under the Slint settings panel (F1, Escape or a
gamepad's Start/Back toggles it). One `xem_render::Gpu` (instance, adapter,
device, queue) serves `SceneRenderer`, `Compositor` and `xem_ui::SettingsUi`;
Slint runs as a custom platform under the host loop and renders into a texture
only when it asks for a redraw. Settings live in
`xem_settings::SettingsService` (`$XDG_CONFIG_HOME/xenogears-ex-machina/settings.json`
on the desktop); the panel and agents change them only through `apply`.

From `runtime/` in `nix develop path:./nix/runtime`:

- tests on lavapipe: `VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD cargo test -p xem-settings -p xem-render -p xem-ui`
- on the host NVIDIA driver: `scripts/xem-gpu-host target/debug/xem-desktop`
  (`--show-settings --exit-after-frames N --screenshot out.png` for a smoke run)
- Android, in `#android`: `cargo ndk -t arm64-v8a -P 26 build -p xem-ui -p xem-render`
  (Slint compiles its FemtoVG module out on Android, so `xem-ui` takes the
  renderer from `i-slint-renderer-femtovg` there)

## Browser host

`xem-web` (wasm32-unknown-unknown, wasm-bindgen) and the thin ES-module page in
`runtime/web/` are the browser host; everything structured crosses the boundary
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
  saves. `boot` loads the executable as xem-headless does and takes the game's
  `stubs.txt` and `schema.json` from beside `game.wasm`.
- **Scheduling.** `requestAnimationFrame` calls `XemApp::frame`: while running,
  the game frames due at 60 per second of page time, at most two per animation
  frame and none after 12 ms, then one render. Time a throttled, hidden or
  stalled page missed is dropped (`status().loop.dropped`), never caught up.
- **Rendering.** wgpu on the canvas: WebGPU when the browser gives an adapter,
  WebGL2 otherwise (`?backend=webgl2` forces it); `status().renderer.backend`
  says which. The Slint panel (FemtoVG's wgpu renderer on the same device,
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
- **Audio.** "Start audio" creates the `AudioContext` inside the click; an
  `AudioWorklet` plays a ring of samples `XemApp::audio_render` produces and
  asks for more when low (no SharedArrayBuffer). Until the port drives xem-spu
  the samples are a test tone mixed by the volume and output settings.
- **Persistence.** Settings: `SettingsService` over `localStorage`
  (`xem.settings`). Saves: IndexedDB (`xem` database, `saves` store) holds the
  session snapshot, or without a session a formatted blank memory card, with its
  SHA-256, checked when read back.
- **Automation.** `window.xem`: `status()`, `boot({executable, run})`,
  `setRunning`, `command(cmd)` (any control command) with the shorthands
  `step(frames)`, `runUntil({maxFrames, until})`, `memoryHash()`,
  `snapshot(name)`/`restore(name)`, `exportSnapshot()`/`importSnapshot(bytes)`;
  `settings.get()`/`settings.apply(change)` (a `SettingChange`, e.g.
  `{MasterVolume: 40}`), `panel.show(bool)`, `importDisc(blob)`,
  `readSectors(lba, count)`, `save(slot)`/`load(slot)`. Each is the runtime
  method the page's controls use; there is no DOM-input path.

From the repository root in `nix develop path:./nix/runtime`:

- build: `python3 runtime/web/build.py` (output `build/web/`; `--no-game` leaves
  the game module out, as a hosted build must until it ships separately)
- serve: `node runtime/web/serve.mjs build/web 8080`, then http://localhost:8080/
- tests: `cd runtime/web && npm ci && npm test` (Playwright on the shell's
  Chromium; `XEM_DISC1=<disc 1 image>` adds the user-disc tests, which send
  the same control commands to `runtime/target/release/xem-headless --control`
  when it is built and compare the replies).
  Headless Chromium needs `--use-vulkan=swiftshader` for WebGPU: with its
  default Vulkan choice it destroys a WebGPU device after the canvas presents.

## WebXR adapter

`xem-webxr` (wasm32 only) and its test page `runtime/web/xr/` render the
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

`runtime/.cargo/config.toml` sets `--cfg=web_sys_unstable_apis` for wasm32,
which web-sys's WebXR bindings require. In `nix develop path:./nix/runtime`:

- dependencies: `npm ci` in `runtime/web` (iwer 2.5.0, playwright-core 1.64.0)
- build: `runtime/web/xr/build.sh` (wasm-bindgen output in `runtime/web/xr/pkg`)
- tests: `node runtime/web/xr/tests/run.mjs [scenario...]` drives Chromium
  (`$XEM_CHROMIUM`, SwiftShader WebGL2) with IWER emulating a Quest 3, its
  controllers and hands; screenshots go to `build/xr-test/`
- by hand: serve `runtime/web` from localhost or HTTPS and open `/xr/`
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

`runtime/scripts/xr-smoke.sh` (in `nix develop path:./nix/runtime`) runs it
against Monado 25.1 without a display or headset: `monado-service` with
`XRT_COMPOSITOR_NULL=1` (null compositor), `SIMULATED_ENABLE=1
SIMULATED_ROTATE=1` (the simulated HMD turns continuously),
`SIMULATED_LEFT/RIGHT=simple` (simulated simple controllers) and `XRT_NO_STDIN=1`,
in a private short `XDG_RUNTIME_DIR` (the IPC socket path must fit a
`sockaddr_un`); the demo finds it through `XR_RUNTIME_JSON=$XEM_MONADO_RUNTIME`.
Both use lavapipe unless `VK_ICD_FILENAMES` is set (`runtime/scripts/xem-gpu-host
runtime/scripts/xr-smoke.sh` runs both on the host NVIDIA driver). It checks the state order
READY, SYNCHRONIZED, VISIBLE, FOCUSED, STOPPING, EXITING after an exit request;
strictly increasing predicted display times; two distinct eye poses; projection
and quad layers on every rendered frame; acquire, wait, submit, release order;
changing head poses; and, with `--paused`, a constant simulation time.
`XEM_XR_COMPOSITED=1` adds a run under Monado's real compositor presenting to a
headless weston and saves a weston screenshot of the runtime's composition.

Monado cannot show what only a headset can: the Horizon OS runtime and its
Vulkan driver, Quest frame pacing and reprojection, real head and controller
tracking, hand tracking and eye gaze (Monado's simulated devices offer neither),
Android lifecycle and permissions (eye tracking needs
`com.oculus.permission.EYE_TRACKING`), and the APK packaging with Meta's loader.
`xem-xr` cross-compiles for `aarch64-linux-android`
(`cargo ndk -t arm64-v8a -P 26 build -p xem-xr` in `#android`).
