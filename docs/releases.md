# Releases and portable installation

Updates to `master` build a GitHub release, starting with `v0.5`; subsequent
updates increment the patch version (`v0.5.1`, `v0.5.2`, …). A retry reuses the
commit's tag. Publication waits for the complete artifact set and automated
checks. Published files are immutable: a retry must reproduce their checksums.
The same release's static web application deploys to GitHub Pages.
Branches under `release/` run the same builds and artifact checks without
reserving a tag, publishing a release or deploying Pages.

Release pages use the conversational installation and update guide in
`tools/release/notes.md`. For each change worth announcing to players, add a new
`docs/changes/<short-description>.md` with one or more short Markdown bullets.
Describe what the player can do or what now works; leave code names, test results
and build details in the PR. Keep existing entries so older releases retain
their history. Changes without a player-facing effect don't need an entry.
The publisher includes only entries added since the highest earlier published
version on the release commit's ancestry, skipping drafts, prereleases and tags
from failed builds. It reads them from that exact commit, links the full comparison,
and explicitly says when no player-facing changes were noted. A retry of an
already published release preserves its copy as well as its downloads.

Download from [GitHub Releases](https://github.com/brianblakely/xenogears-ex-machina/releases).
[Browser play](https://brianblakely.github.io/xenogears-ex-machina/) needs no local
server or native executable. The project supplies no original game images,
BIOS, extracted artwork/music, personal saves or generated PS1 disassembly.

| Platform | Artifact | Graphics |
| --- | --- | --- |
| Linux x86-64 / AArch64 | Executable AppImage | Vulkan or OpenGL ES; bundled Mesa, host NVIDIA drivers |
| Windows x86-64 | ZIP: `xem-desktop.exe`, `xem-headless.exe`, required DLLs | Direct3D 12 or Vulkan |
| macOS Intel / Apple Silicon | ZIP containing `Xenogears Ex Machina.app` | Metal |
| Android 10+ / Meta Horizon OS | Signed APK, arm64-v8a and x86_64 | Vulkan/GLES; OpenXR on Horizon |
| Browser desktop/mobile/headset | Static web ZIP and GitHub Pages | WebGPU/WebGL2; capability-gated WebXR |

For this release, the project's qualification policy accepts Linux testing as
the desktop stand-in and Android/Horizon emulation as device qualification.
The automated environments are the pinned Linux shell, Android emulator,
arm64 execution under QEMU/Android translation, Monado's simulated OpenXR
headset, and Chromium with IWER for headset-browser input/session tests.
These results are emulation/proxy evidence, not measurements of physical
devices, their thermal behavior, or every vendor's drivers. Native platform
builds remain separate artifacts; a Linux result does not claim that a Windows
or macOS executable itself was run on Linux.

This is a playable release of the recovered game and runtime. Phase 9's
continuous full-story progression, endings and exhaustive optional-content
qualification remain open. The passing scenarios below do not certify an
end-to-end playthrough of both discs.

The qualification matrix is explicit and intentionally bounded:

| Test environment | Coverage |
| --- | --- |
| Arch/Omarchy Linux, kernel 7.2.5, SDL 3.4.14, Mesa 26.2.2 | Native desktop proxy, Vulkan and GLES, input/audio, portable storage and headless operation |
| Android 14 / API 34, Pixel 6 AVD, x86_64 and translated arm64-v8a | Native mobile lifecycle, import, persistence and rendering on SwiftShader |
| Monado 25.1.0 simulated headset on Linux | Horizon native proxy, OpenXR/Vulkan, session changes, stereo, hand/controller/settings interactions |
| Chromium 152.0.7977.82 on Linux, desktop and emulated mobile viewports | Flat browser WebGPU and WebGL2, storage, audio, lifecycle and context recovery |
| Chromium 152 with IWER 2.5.0, Quest 3 profile | Horizon-browser proxy, WebXR-compatible WebGL2, controller/hand tracking and immersive settings |

Firefox, Safari and real vendor-specific headset/browser combinations are not
qualified by this matrix. Native Windows and macOS builds additionally run
packaged startup checks on GitHub's Windows 2025 and macOS 15 runners.
Presentation and performance limits, including the simulated rendering load,
remain described in the [runtime guide](runtime.md); emulator timing is not a
physical phone/headset thermal measurement.

## Desktop installation

Extract the archive into a writable folder. Put your own disc images beside
the executable, beside the AppImage, or beside the `.app` bundle. On Linux,
make the AppImage executable (`chmod +x filename.AppImage`) and run it. First
launch identifies the images and moves them into `discs/`. A CUE sheet moves
with all of its track files; keep those files in the same folder. Existing
destination filenames are refused rather than replaced. Other files, including
an optional BIOS, are left alone. Disc 1 starts automatically when present;
the runtime discovers the other disc in that directory for disc transitions.

The supported images are North American `SLUS-00664` and `SLUS-00669` (original
and Greatest Hits). Use CHD, BIN/CUE, or raw 2352-byte ISO images. A 2048-byte
ISO lacks XA audio/movie data and is rejected. Disc identity and whole-image
verification use the digests in `xem-disc`; renaming an unsupported disc does
not make it supported. A BIOS is optional; importing one supplies its Kanji
font for memory-card titles.

The app creates and uses:

```text
Xenogears folder/
  executable or application bundle
  discs/                 user-supplied images and cue sheets
  cards/card1.mcd         standard 128 KiB memory card
  cards/card2.mcd
  saves/state.xemstate    quick save state
  settings.json          settings and bindings
  bios.bin               optional imported BIOS
```

The executable's location determines the folder, independently of the launch
working directory. AppImage storage stays outside its temporary mount, and
macOS storage outside the application bundle. `XEM_DATA_DIR` explicitly
selects another writable folder; `play.sh` uses the checkout's root. There is
no automatic migration from older development configuration directories.

Back up the folder before upgrading, then replace the application while
retaining `discs/`, `cards/`, `saves/`, `settings.json` and an imported BIOS.
Original memory cards are the durable interchange format. States identify
the exact game module and refuse incompatible builds with a diagnostic.
Settings offers card/state import and export, storage retries, and save/time
controls. Escape opens it; Back+Start does so on a controller.

Linux AppImages include their library closure, glibc, Mesa drivers and fonts,
so they do not require a distro package manager or an installed Nix. They
require a compatible CPU/kernel and enabled unprivileged user namespaces.
Proprietary NVIDIA graphics use the host's driver libraries.
`APPIMAGE_EXTRACT_AND_RUN=1 ./filename.AppImage` avoids a
FUSE requirement. On Ubuntu 24.04 and later, AppArmor restricts unconfined
user namespaces. Following [Ubuntu's application-profile instructions](https://documentation.ubuntu.com/release-notes/24.04/#unprivileged-user-namespace-restrictions),
an administrator can grant this application `userns,` in an AppArmor profile
with `flags=(unconfined)` attached to its absolute AppImage path. Retain the
system-wide restriction. The Ubuntu CI runners exercise the packaged app
inside that explicit profile; Arch does not need it. Kernels or policies
that prohibit user namespaces entirely cannot run this launcher.
`--headless` selects the image's command-line host
without initializing video, GPU or audio. Windows DLLs must remain beside the
executables. macOS bundles are ad-hoc signed; no Developer ID notarization is
claimed. Native XR uses Vulkan/OpenXR; macOS flat Metal support does not imply
a macOS OpenXR runtime.

## Android, Horizon and browser storage

Install the signed APK and select local discs with the system document picker.
Persisted grants allow the app to reopen them; moving/deleting a document or
revoking permission requires reimport. Android controls executable and app
storage locations, so `cards/` and `saves/` are in its private files directory.
APK upgrades use the same signing key and increasing versionCode. Export
backups before uninstalling, which can erase app data.

Browser imports remain local and are never uploaded to the hosted site.
Storage persistence depends on browser permission and quota; export backups
before clearing site storage or changing origins. Missing/denied WebXR leaves
flat play available. Headset hands, gaze, passthrough and stereo modes are
offered only when the runtime reports their required capabilities. Simulated
tracking tests do not guarantee identical tracking quality on every headset.

Self-host the extracted web ZIP over HTTPS (localhost also works), serving
`.wasm` as `application/wasm`. All assets use relative paths, including under
the Pages project subdirectory. The single-thread build needs no COOP/COEP
headers, `SharedArrayBuffer`, service worker or local server process during
hosted play. For local serving:

```sh
node src/runtime/web/serve.mjs build/web 8080
```

## Reproducing a release

The checked-in Nix locks and Cargo lock pin the tools and dependencies. The
Intel macOS shell has a separate Nixpkgs 26.05 lock because the main package
set no longer builds for Intel macOS; its Rust toolchain remains the same.
`tools/release/shell.sh` also selects that lock for Nix's Bash launcher on
Intel macOS, without changing the checked-in locks.
The public `src/port/symbols.json` contains source-bound addresses and one-way
function fingerprints, no original code/data bytes. It lets a clean public
checkout build the recovered game without private PS1 links. Refresh it only
after verifying the original targets locally:

```sh
nix develop path:./nix/ghidra#matching -c make -C src/decomp all-verify
nix develop path:./nix/runtime -c python3 tools/game_module.py --export-symbols src/port/symbols.json
```

The ordinary public release build is:

```sh
nix develop path:./nix/runtime#release -c python3 tools/game_module.py --symbols src/port/symbols.json --release --stubs
nix develop path:./nix/runtime#release -c python3 tools/release/notices.py
nix develop path:./nix/runtime#release -c python3 tools/release/package.py linux --version v0.5
nix develop path:./nix/runtime#windows -c python3 tools/release/package.py windows --version v0.5
tools/release/shell.sh release python3 tools/release/package.py macos --version v0.5
nix develop path:./nix/runtime#release -c python3 tools/release/package.py web --version v0.5
python3 tools/repository/source_archive.py --output dist/xenogears-ex-machina-v0.5-source.tar.gz
```

Build Linux/macOS on their respective architectures. The Windows shell
cross-compiles with MinGW on Linux. The release module rejects missing code,
unmapped assembly and globals; its only unresolved functions are the named
retail-inactive development hooks in `release-debug-hooks.txt`, which trap
with diagnostics if reached. It does not silently skip unknown game logic.

For Android, set `XEM_ANDROID_KEYSTORE`, `XEM_ANDROID_STORE_PASSWORD`,
`XEM_ANDROID_KEY_PASSWORD` and `XEM_ANDROID_VERSION_CODE`, then run:

```sh
nix develop path:./nix/runtime#android -c python3 tools/release/package.py android --version v0.5
```

CI reads the persistent signing key from `XEM_ANDROID_KEYSTORE_BASE64` and the
two password secrets; a missing key fails the job. Preserve the signing key
securely to keep APK upgrades possible. GitHub Pages must use GitHub Actions
as its publishing source. Release jobs have narrowly scoped permissions; pull
requests do not get publishing/signing permissions.

`SHA256SUMS` lists every published download. Native runtime-source archives
contain the bundled libraries' sources and Nix recipes/options. The authored
source archive uses `packaging/source-files.txt`, with only the separately
reviewed KEMAR data allowed as a binary input. Rust notices include the Slint
Royalty-free 2.0 terms; the settings panel supplies its attribution widget.

During development use the affected suite or focused tests. Release copy and
publisher changes need focused release tests, not the runtime suite. Before review
or merging runtime changes, run `src/runtime/scripts/suite.sh full` once; it skips
when runtime inputs have no changes against the merge base with `origin/master`.
Use `--base REF` for another comparison or deliberately use `--force` to bypass
the guard. Its logs and exact stage results are under `build/suite/`; a skip
preserves previous logs and does not count as a passing run. Checks without private
disc inputs must report their skips; they are not original-game playthroughs.

## v0.5 qualification record

On 2026-10-17, `src/runtime/scripts/suite.sh full` ran once with both private
disc images available. Module construction, port tests (242 tests, one skip),
native/web builds, validation and Android build/boot passed. The first module
exposed an address-mapping defect: PS1 RAM mirroring also masked the runtime's
reserved presentation buffers. Nine Rust test targets, browser first-person
and binaural checks, and ARM64 scene-reveal checks failed. The correction is
in `tools/game_module.py`; no original-game source or expected capture changed.
The suite process returned 143 (SIGTERM) during diagnostic reruns, before its
serial device and timing stages. This run is **not recorded as a full pass**.

The corrected public module is 5,392,773 bytes. Its bytes and schema matched
the locally qualified module and the GitHub-built release input with `cmp`.
The compiled Wasm memory regression failed before the correction and passed
afterward. With `XEM_GAME_WASM` pointing to that corrected module,
`CARGO_TARGET_DIR=target-release`, `XEM_RELEASE_VERSION=v0.5` and
`VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD`, these commands ran from `src/runtime`
inside the pinned runtime shell:

```sh
cargo test --release -p xem-headless --test camera --test binaural --test diorama --no-fail-fast
cargo test --release -p xem-headless --test first_person --test gestures --test inspection --test render --test states --no-fail-fast
cargo test --release -p xem-host --lib --test states --no-fail-fast
```

Results: 40/40, 42/42 and 23/23 passed. The existing OpenGL renderer and
first-person ordering tests also passed with `WGPU_BACKEND=gl`,
`__EGL_VENDOR_LIBRARY_FILENAMES=$XEM_MESA_EGL`, `LIBGL_ALWAYS_SOFTWARE=1` and
`GALLIUM_DRIVER=llvmpipe`. Logs are `build/release-presentation-fixed.log`,
`build/release-presentation-remaining.log`, `build/release-host-fixed.log` and
`build/release-gl*.log`.

`src/runtime/scripts/arm64-smoke.sh`, in the pinned Android shell with the
corrected module and native `XEM_HEADLESS`, passed all recorded route hashes,
cross-platform state/card interchange and the Lahan/world-map reveal checks.
`node --test --test-concurrency=1 tests/first-person.test.mjs tests/xr-binaural.test.mjs`
passed 4/4 with no skips, using the corrected `XEM_WEB_ROOT` and native
`XEM_HEADLESS`. Logs are `build/release-arm64-fixed.log` and
`build/release-web-presentation-fixed.log`.

The public-check side of the initial local suite also had one compact-panel
drag failure. The same test binary passed in isolation without a code change;
the hosted public checks passed too. Treat that initial failure as retained
evidence, not a silently discarded pass. Original-game checks in public CI
skip private disc-dependent scenarios; the local runs above had those inputs.

The final Linux AppImage passed this command in the pinned runtime shell:

```sh
python3 tools/release/smoke.py linux --version v0.5 --graphical --disc 'discs/Xenogears disc 1.chd'
```

It rendered Settings, organized a disposable hard link of the disc image,
created the three storage directories beside the AppImage while launched from
another working directory, and ran 120 headless frames. The original image
remained in place. See `build/release-linux-smoke-final.log`.

From the repository root inside the pinned Android shell, both of these
original-game checks passed with the corrected APK and native binary:

```sh
export CARGO_TARGET_DIR="$PWD/src/runtime/target-android"
export XEM_HEADLESS="$PWD/src/runtime/target-release/release/xem-headless"
export XEM_SKIP_BUILD=1
src/runtime/scripts/android-smoke.sh
XEM_SMOKE_ABI=arm64-v8a XEM_SMOKE_GAME_ONLY=1 src/runtime/scripts/android-smoke.sh
python3 tools/release/android_smoke.py --version v0.5
```

The first run covered document import, touch settings, low memory, rotation,
activity recreation, restart persistence and background/resume; simulation
advanced 0.010 seconds over roughly ten seconds in the background. Both ABIs
played the original game, exercised rewind/fast-forward and touch saves, and
exchanged states with the native host byte for byte. Native libraries in the
instrumentation APK and signed release APK matched for all four `.so` entries.
The signed APK separately passed non-debuggable installation, rendering,
directory creation and data-preserving reinstall, including on a reset AVD.
Logs are `build/release-android-game-fixed.log`,
`build/release-android-arm64-fixed.log` and
`build/release-android-cold-start.log`.

The native XR check passed in the pinned runtime shell:

```sh
CARGO_TARGET_DIR="$PWD/src/runtime/target-release" XEM_CARGO_PROFILE=release XEM_SKIP_BUILD=1 src/runtime/scripts/xr-game-smoke.sh
```

Monado's simulated 20 Hz headset exercised original movies, fields, dialogue,
menus, on-foot/Gear battles and the world map, including screen/diorama
transitions, stereo geometry, one/two-hand gestures, controllers, tracking
loss/recentering, paused head tracking, settings and Binaural sound. Saved
states matched the headless host byte for byte and subsequent RAM hashes
matched. The sampled diorama frame interval was 50.17 ms median, 56.73 ms p99,
with zero of 276 intervals late by the test's threshold. These are simulator
measurements. See `build/release-xr-fixed.log` and its captured views under
`src/runtime/target/xr-game-smoke/`.

The final static web package passed all 27 persistence tests with no skips:

```sh
cd src/runtime/web
export XEM_WEB_ROOT="$PWD/../../../build/release/xenogears-ex-machina-v0.5-web"
export XEM_HEADLESS="$PWD/../target-release/release/xem-headless"
node --test --test-concurrency=1 tests/persistence.test.mjs
```

This covered rejected/corrupt/evicted storage, quota and interrupted writes,
backup/import, settings and input persistence, original-game pause behavior,
time controls and native/browser state/card interchange. Rewind latency was
8.4 ms p99 against the 16 ms limit. Persistence cases run sequentially so
wall-clock assertions do not compete for the software renderer. The assertions
are unchanged. See `build/release-web-persistence-final.log`.

[Hosted qualification at `55ceb35`](https://github.com/brianblakely/xenogears-ex-machina/actions/runs/39070701190)
passed every build and test job: both Linux AppImages, both Mac bundles,
Windows cross-build and native startup, signed Android installation/update,
web packaging and the public/runtime regression checks. Packaged browser
checks passed 43 tests and skipped six private-disc cases; the local runs
provide the original-game evidence. The signed APK's cold-start check also
passed after allowing Android's fresh boot services to settle. It retains
full startup, process-exit and crash diagnostics on failure. This preview
reserved no tag and published no release or Pages deployment.

With the same web/native paths and pinned shell, the remaining serial browser
checks passed:

```sh
node --test --test-name-pattern="drops missed time" tests/page.test.mjs
node --test --test-concurrency=1 tests/xr-game.test.mjs tests/xr-diorama.test.mjs
node --test --test-name-pattern="phone-class processor" tests/binaural.test.mjs
node --test --test-name-pattern="native and browser" tests/binaural.test.mjs
```

Results were 1/1, 9/9, 1/1 and 1/1, with no skips. WebXR covered stereo,
screen/diorama/movie transitions, tracking and recentering, controller/hand
interactions, session reentry, inspection, settings, saves and time controls.
Binaural processing used 2.2% of real time normally and 7.3% with CPU execution
throttled four times, below the 10%/40% limits. Native/browser output differed
by at most one sample unit against a tolerance of two.

The first focused audio run correctly rejected a test-generated state from
the earlier module. The timing test now regenerates that state using the
selected native binary for every run; module compatibility checks remain
unchanged. Its initial failure is retained in
`build/release-web-timing-final.log`, alongside the passing page/WebXR checks.
The corrected focused audio runs are in `build/release-web-binaural-final.log`.
