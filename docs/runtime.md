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
- **Handwritten routines.** Their ports (`port/model_draw.c`, `arena.c`,
  `worldmap_terrain.c`, `sprite_pixels.c`, `lzss.c`, `resident.c`) keep game
  addresses as 32-bit integers and access memory through
  `port/include/xem/memory.h`, which maps KUSEG and KSEG1 RAM mirrors onto
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
- **Code addresses.** A function's address, wherever C takes it, is its original
  address. Indirect calls go through generated dispatchers that select the
  function by address and check, from a fingerprint of its original code bytes in
  game memory, that the image holding it is the one loaded there; a call into an
  absent or overwritten image traps.
- **Unprototyped calls.** A direct call whose types differ from the
  definition goes through a generated adapter (`build/game/adapters.txt`
  lists each): integer arguments are truncated or extended, a missing
  argument is 0 and an extra one is dropped. Where the PS1 callee read the
  register or stack word the caller left, or the caller used the `$v0` a void
  function left, `port/adapters.c` defines `xem_adapt_<function>` with the
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
  stacks, so the game is two fibers (`port/fiber.c`): the game, whose bottom
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
  runs `port/fiber.c` and `port/arena_task.c` under asyncify, wasm2c and the
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
| `xem-host` | what the SDL3 hosts share: the window and its native handles, the wgpu surface (dropped and recreated with the native window), the presented view (scene and settings panel) |
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

The window, the surface and the view (`xem_host::View`: scene, compositor,
panel and its input) are shared with the Android host in `xem-host`.

## Android host

The APK (`dev.xem.app`, minSdk 29, target/compile SDK 34, x86_64 and
arm64-v8a) is SDL3's Java layer from the desktop's SDL release
(`$XEM_SDL3_SRC`, nixpkgs' `sdl3.src`: `SDLActivity` and its helpers are
copied into the Gradle build, never vendored), `libSDL3.so` built from the same
source with the NDK's CMake toolchain and `libmain.so`, the `xem-android`
cdylib whose `SDL_main` SDLActivity runs. `runtime/android` is a plain Gradle
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
- **Settings.** `xem_settings::FileStore` at `<files dir>/settings.json`.
- **Lifecycle.** SDL 3.4 delivers the application events only to event
  watches, synchronously on the SDL thread inside `SDL_PollEvent`: on
  `WILL_ENTER_BACKGROUND` the host drops the wgpu surface and stops the
  simulation clock, SDL then blocks the thread until the activity resumes,
  and after `DID_ENTER_FOREGROUND` the surface is recreated on the new native
  window and the clock restarts where it stopped (a frame never advances it
  by more than 100 ms, so there is no catch-up). A replaced native window
  (configuration changes) is detected by identity and the surface recreated.
  `LOW_MEMORY` is logged; on `TERMINATING` the run ends. A recreated activity
  (changes outside `configChanges`, such as density or asset paths) runs
  `SDL_main` again in the same process (`SDL_ANDROID_ALLOW_RECREATE_ACTIVITY`),
  which reloads settings and reopens the imported disc.

In `nix develop path:./nix/runtime#android`:

- build: `runtime/scripts/android-build.sh` (`XEM_ANDROID_ABIS` to pick ABIs)
  → `build/android/gradle/app/outputs/apk/debug/xem-debug.apk`
- emulator smoke test: `runtime/scripts/android-smoke.sh` (headless API-34
  x86_64 AVD under `.local/android`, guest Vulkan on SwiftShader; logcat and
  screenshots in `build/android/smoke`)
- `cargo ndk -t arm64-v8a -P 29 build -p xem-ui -p xem-render` builds the
  shared crates alone (Slint compiles its FemtoVG module out on Android, so
  `xem-ui` takes the renderer from `i-slint-renderer-femtovg` there)
