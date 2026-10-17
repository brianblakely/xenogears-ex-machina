# Releases and portable installation

Updates to `master` build a GitHub release, starting with `v0.5`; subsequent
updates increment the patch version (`v0.5.1`, `v0.5.2`, …). A retry reuses the
commit's tag. Publication waits for the complete artifact set and automated
checks. Published files are immutable: a retry must reproduce their checksums.
The same release's static web application deploys to GitHub Pages.
Branches under `release/` run the same builds and artifact checks without
reserving a tag, publishing a release or deploying Pages.

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
FUSE requirement. Security policies that disable user namespaces are not
supported by this packager. `--headless` selects the image's command-line host
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

During development use the affected suite or focused tests. Before review or
merging a release change, run `src/runtime/scripts/suite.sh full` once. Its
logs and exact stage results are under `build/suite/`. Checks without private
disc inputs must report their skips; they are not original-game playthroughs.
