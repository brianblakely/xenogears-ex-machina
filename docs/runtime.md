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

## Crates

| Crate | Role |
| --- | --- |
| `xem-core` | headless authority: game instance, virtual clock, commands, introspection, stepping, snapshots, services |
| `xem-game` | the game module binding (wasm2c natively, imports in browsers) |
| `xem-gpu`, `xem-spu`, `xem-media`, `xem-disc` | the console devices as host services, and disc import |
| `xem-render` | the wgpu renderer for flat, stereo and XR views |
| `xem-ui` | the Slint settings panel as a custom platform rendered into a texture on the shared wgpu device |
| `xem-xr` | the OpenXR adapter |
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
