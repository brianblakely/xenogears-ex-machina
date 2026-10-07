# Xenogears Ex Machina

**Recover the complete original program first; port it second; extend it third.**

An independent Xenogears decompilation, native multiplatform port and modding
toolkit, led on Arch Linux. Target Linux, Windows, macOS, Android and Meta Horizon
OS; keep full authoring/editors desktop-focused and support prebuilt play on mobile
and headsets. Recover game-specific code and data formats from the user's original
images and observed execution, not another Xenogears project's implementation.
General-purpose compilers, decompilers, emulators and libraries are welcome.

## Working rules

Implement first: select an actual function, build target or failing test; change
code; compile; inspect the first difference; fix it; commit a useful increment.
Do not substitute planning, inventories, capture infrastructure or framework work
for source recovery. Add a tool only when it removes a demonstrated bottleneck.

This file owns scope and phase order. Source, build configuration and executable
tests own implementation status. Do not regenerate per-sentence requirement
matrices, facet registries, migration ledgers or duplicate progress dashboards.
Read only this plan, the short workflow and the subsystem being changed. Existing
findings, source identities and Phase 0 artifacts remain evidence, not mandatory
startup reading or a second checklist to update for every helper.

Never weaken comparisons, rewrite expected original bytes, silently ignore unknown
code or count assembly placeholders as recovered source. Keep all original images,
extracted bytes, generated disassembly, captures and saves in ignored local paths.
Reusing original assembly privately during recovery is permitted; distributing it
as newly authored source or claiming it completes decompilation is not.

Complete the active phase before advancing. A checked task requires its stated
implementation and test, not another generated acceptance registry. Preserve past
findings at their measured scope; no existing host-reference result implies a
PS1 binary match, complete discovery or native gameplay.

## Native architecture — Phases 2 onward

Use a library-first Rust/C runtime, not a new general-purpose engine. Keep the
recovered C as the authoritative original-game implementation; use Rust for new
native infrastructure and narrow, source-correlated portability adapters. Do not
require a wholesale Rust rewrite or a second independently updated game-state
model. These choices add no work to Phase 1's original-compatible matching exit.

- **SDL3:** Default desktop/Android platform layer for game windows/surfaces,
  keyboard/mouse/touch/controllers, audio-device I/O and lifecycle integration.
  Reuse applicable services on Horizon OS; use native APIs only for demonstrated
  gaps. SDL is not the game renderer or the VR abstraction; do not add a parallel
  Windows XInput path for capabilities already supplied through SDL.
- **wgpu:** Shared game renderer across flat, stereo and XR presentation. Use the
  appropriate Vulkan/Direct3D 12/Metal backends per target. Ordinary game surfaces
  integrate with SDL; headset output integrates with OpenXR-owned swapchains
  through a narrow graphics adapter, not a second renderer or per-frame CPU copy.
- **OpenXR:** Horizon immersive sessions, headset/controller poses and actions,
  stereo views, predicted display timing and frame submission. An Android app
  displayed as a flat panel is not evidence of stereo or immersive support.
- **GPUI:** Desktop application interface for launcher/settings, bindings, asset
  import, save/mod management and later tools. Original dialogue, battle menus
  and other game UI stay in game presentation. Qualify GPUI on actual devices
  before adopting it beyond desktop; otherwise use a platform-appropriate client
  of the same services. Settings must be accessible on-device and in-headset.

Keep application services (validation, settings persistence, session control and
available save/mod operations) independent of widgets. GUIs and agents call the
same typed operations and observe acknowledged results; no capability exists only
in a GUI callback. Logical separation does not require separate executables or
processes: settings must remain accessible while playing.

Give each host explicit window, GPU-resource, input-focus and lifecycle ownership.
Coordinate SDL and GPUI event handling under platform main-thread requirements;
do not start competing application loops. OpenXR drives headset frame scheduling,
not SDL window refresh or GPUI repaint. One owner mutates simulation state through
commands; presentation consumes read-only state and never advances gameplay per
eye, UI repaint or spectator view. Keep graphical dependencies optional so the
headless runtime needs no GPUI, wgpu, OpenXR or SDL video/audio initialization.

## Phase 0 — Preserve the research and build baseline

**Goal:** Keep the established reproducible tools, source identities and useful evidence.

- [x] Retain source profiles, original-game inspection/scenario tools, findings,
  source separation, native-agent/authoring specifications and public build baseline.

**Exit:** The existing baseline remains available. Retired requirement projections
remain in Git history; preserving Phase 0 does not require running obsolete gates.

## Phase 1 — Complete the binary-matching PS1 decompilation

**Goal:** Readable, independently recovered source for the complete original game,
with reproducible byte-identical builds of the supported resident executables and
all executable overlays on both discs. This is not a first-slice milestone and
not a native C++ reimplementation.

- [ ] Establish an original-compatible compiler/assembler/linker configuration.
  Match representative arithmetic, globals, structures, loops, switches and larger
  callers across resident and overlay code. Record exact versions, flags, ABI,
  small-data/GP assumptions and layout in the build configuration. A modern MIPS
  compiler or selected m2c target does not establish the original compiler.
- [ ] Build every supported executable image incrementally, preserving entry
  points, sections, original data, alignment, relocations and overlay identities.
  Original assembly/binary placeholders may keep intermediate builds working;
  report their remaining ranges separately from source-covered ranges.
- [ ] Recover original game code in cohesive resident/overlay modules, using
  original-compatible C, small shared types and narrowly scoped headers. Replace
  placeholders continuously. Reuse existing analysis, recovered algorithms and
  types; do not require porting each function into Program or a new ownership model.
- [ ] Cover every executable region on both discs, including world map, Gear and
  on-foot battles, field/event systems, menus/saves, audio/media, optional content,
  minigames, shared libraries, startup and hardware interfaces. Classify genuine
  handwritten assembly and SDK/library code explicitly; reconstruct/link their
  required code without silently excluding it from the image or coverage report.
- [ ] Recover data layouts, dispatch tables and every used script instruction.
  Preserve game bytecode/data as user-supplied assets; document semantics and
  provide useful parsers/disassembly without rewriting every asset as source code.
- [ ] Close source coverage: no unknown executable ranges, unreviewed generated
  pseudocode, binary-only placeholders standing in for recoverable compiled game
  logic, guessed formulas or unexplained exclusions. Track source-reviewed but
  nonmatching functions separately; they do not satisfy the matching exit.
- [ ] Produce exact final code/data/layout comparisons for every executable and
  overlay target, with no address/stack masks or normalized-diff substitutes.
  Verify decoded overlay images first; track compressed-container reproduction
  separately. Whole-disc filesystem/ECC reproduction is not an extra hidden gate.
- [ ] Recover the service/timing/state boundaries needed by the port and the
  original visual and Mono/Stereo/Wide behavior. Use targeted original observation
  for uncertainty, formats and integration, not a new capture/evidence ceremony
  for each function that already has a qualified binary match.
- [ ] Verify clean builds from user-supplied sources and run original-environment
  smoke/integration routes, including the existing forest/encounter/menu/media
  route. Reconcile all targets and source coverage before declaring completion.

**Exit:** The complete both-disc original program is recovered, source coverage
is complete under the explicit assembly/library classification, and every target
matches exactly. A hash pass built mostly from original assembly is only a build
baseline. Matching proves original-target code, not historical source spelling,
semantic understanding of every name, or portability to a new compiler.

The existing host reconstruction and captures remain regression/reference assets.
They are not the required implementation path or acceptance gate for new Phase 1
functions. Do not grow its artificial stack/heap/service model merely to validate
code that can instead be compiled and compared on the original target.

## Phase 2 — Native runtime foundation

**Goal:** Adapt the recovered program through thin platform boundaries, without a
second independently maintained game-rules implementation.

- [ ] Establish Rust/C integration, local asset import and native storage, input,
  graphics, audio and timing through the boundaries above. Preserve source
  correlation; isolate pointer-width, arithmetic, layout and PS1-specific
  adaptations. Keep a narrow C ABI with explicit ownership, buffer bounds and
  error/unwind rules; do not mirror game logic in Rust or serialize native pointers
  and device handles as game state.
- [ ] Separate authoritative simulation from presentation and host pacing. One
  runtime serves humans, agents, tests and editor previews; no CPU emulator ships.
  Expose read-only presentation data, retaining pre-projection geometry/transforms,
  cameras and recovered ordering/masking semantics needed for multiple views;
  a final 2D framebuffer must not be the only presentation interface.
- [ ] Implement direct typed engine actions and structured full introspection,
  semantic readiness/valid actions, exact stepping, bounded run-until, unlocked
  speed, debugging, snapshots/replay and typed go-anywhere scenario setup.
- [ ] Build/run the headless target without the graphical stack, a window, display
  server, GPU, input device or audio device. Advance logical audio/media progress
  even without output devices; optional screenshots/spectator streams are read-only
  and independent of simulation time.
- [ ] Prove the integration risks with small working paths: a live SDL/wgpu game
  surface alongside a GPUI settings control; Android startup, asset access and
  suspend/resume; and an actual Horizon headset rendering a stereo test scene
  through wgpu/OpenXR. Verify swapchain/device ownership and synchronization,
  frame scheduling and head tracking while simulation is paused. These are
  foundation checks, not the full UX or VR feature work of Phases 5, 6 and 10.
- [ ] Demonstrate a small original-game execution path through the real native
  services, including direct commands, inspection and snapshot/restore. Compare
  authoritative outcomes with presentation attached and absent; use the retained
  behavioral tests to detect portability regressions.

**Exit:** A tested shared, agent-ready Rust/C runtime and qualified platform/graphics
integration paths exist. Defining schemas or cross-compiling alone does not prove
integration. No authoring engine or full application UI is required before play.

## Phase 3 — First end-to-end native playable slice

**Goal:** Prove original gameplay through the native runtime, not a replacement demo.

- [ ] Integrate the established original field/dialogue/encounter/reward/return/
  menu/save-load/media route from recovered source, including required branches.
- [ ] Compare meaningful original states, outcomes and timing. Test continuous
  legal play separately from debug-launched scenarios; no corrective state injection.
- [ ] Demonstrate human and direct-agent control, complete relevant state/debug
  access, valid setup, snapshots, replay and equivalent stepped/unthrottled runs.
- [ ] Attach/change/disconnect optional spectator output without changing state
  or simulation pacing. Image-free headless play must need no GPU or audio device.
- [ ] Run the same slice on desktop and Android, including Horizon flat-panel
  play. Verify asset import, saves and lifecycle recovery on actual mobile/headset
  devices; do not maintain separate platform-specific game logic.

**Exit:** The complete native slice works end to end under human/agent clients and
on the target platform hosts. Its integration evidence, not a TypeScript authoring
prerequisite, closes this phase; full stereo/VR qualification remains later work.

## Phase 4 — Complete native gameplay on both discs

**Goal:** Integrate the already recovered game, not resume deferred decompilation.

- [ ] Complete fields, story/events, movement/collision/cameras, world map and
  transportation, menus/shops/party/equipment, saves, on-foot and Gear battles,
  Deathblows/status/AI/rewards/progression, minigames and optional activities.
- [ ] Complete both-disc transitions, music/samples/effects/FMVs and original
  Mono/Stereo/Wide behavior, including polarity/reverb and streamed-media routing.
- [ ] Extend legal actions, introspection, semantic readiness, scenario setup,
  debug access and unlocked headless execution alongside every subsystem.
- [ ] Verify continuous unmodified progression, endings, optional content and
  failure/retry paths, with targeted coverage where a playthrough is insufficient.

**Exit:** Both discs and all original systems work natively with human/agent parity;
unknown behavior and progression workarounds cannot be reported as completion.

## Phase 5 — Modern rendering, stereo and optional PS1 fidelity

**Goal:** Modern flat/stereo presentation by default; original quirks stay selectable.

- [ ] Use wgpu's appropriate Vulkan/Direct3D 12/Metal backends across the targets;
  support arbitrary resolution/aspect ratio and presentation framerate,
  resizing/high-DPI, correct projection/FOV/UI safe areas and interpolation
  without changing simulation/RNG/script/animation timing.
- [ ] Implement Modern, PS1-style and Custom profiles: affine texturing, projected
  vertex snapping, color precision, dithering, low-resolution rasterization and
  recovered ordering/transparency quirks at their appropriate rendering stages.
- [ ] Preserve intentional masking, palettes, fog and compositing. Keep world,
  sprite and UI filtering independent; do not stretch text, portraits or FMVs.
- [ ] Default optional culling to off; distinguish visibility/frustum/occlusion/
  back-face rejection from clipping, intentional hiding and gameplay activation.
- [ ] Validate wider framing/content issues separately, cross-backend visuals and
  unchanged gameplay with rendering skipped, offscreen or spectator-streamed.
- [ ] Qualify Horizon flat virtual-screen play, then seated stereo-screen play
  through OpenXR. Render both eyes from the same simulation state using the
  headset's views/projections; place game UI, sprite planes and FMVs deliberately.
  Preserve monoscopic media as such rather than claiming reconstructed 3D content.
- [ ] Verify paused-game head tracking, display-paced XR rendering independent of
  simulation rate, and spectator attach/detach without extra game updates or RNG
  changes. Measure sustained headset frame pacing; never run game logic per eye.

**Exit:** Display options, stereo-screen play and fidelity profiles work without
altering game rules; flat presentation remains available.

## Phase 6 — Cross-platform controls and application UX

**Goal:** Native human input uses the same authoritative command path as agents.

- [ ] Implement SDL3 keyboard/mouse/controller/touch input and OpenXR controller
  actions, context bindings and rebinding for direction/run/jump/confirm/cancel
  and camera left/right. Translate platform events into shared typed actions;
  agents issue those actions directly without SDL or OS-event injection.
- [ ] Make menus mouse-browsable with wheel scrolling; on desktop, Tab toggles the
  game menu and Esc toggles the app menu. Provide touch/headset equivalents and
  explicit focus/capture transitions without rebuilding original game UI in GPUI.
- [ ] Deliver the GPUI desktop launcher/settings and available application-service
  screens. Apply changes through shared validation, acknowledgment and persistence;
  changing settings from an agent uses the same path. Add later save/mod/editor
  screens when their owning phases implement those services.
- [ ] Qualify mobile and in-headset application controls, using GPUI only where
  device integration is proven. Verify controller reconnects, live settings changes
  and return to play; a desktop companion is not a substitute for headset settings.
- [ ] Verify equivalent human/agent actions and explicit pause policy during app
  interactions; opening settings must not accidentally advance or stall simulation.

**Exit:** Every original system and application settings are usable through the
appropriate desktop, touch and headset controls, with human/agent action parity.

## Phase 7 — Mods and source-oriented agent authoring

**Goal:** Add authoring on the proven runtime; do not build a second game engine.

- [ ] Implement native mod packages, dependency/conflict handling, deterministic
  load order, validation, graphical mod management and explicit save compatibility.
- [ ] Provide Lua gameplay extensions with shared scheduling, debugging and
  serializable progress. Integrate custom tasks with snapshots/replay and errors.
- [ ] Build the small TypeScript/optional TSX source-to-playable bridge here:
  source/parameters -> supervised Node build -> IR -> meshes/world/collision/events
  -> native package. The shared native runtime/events/Lua own gameplay; prebuilt
  desktop/mobile/headset play needs no Node, browser, QML, authoring tools or
  second runtime. Keep the supervised source-build/authoring workflow on desktop.
- [ ] Support reusable procedural solids, surfaces/arbitrary meshes and optional
  external/original assets. Start with a wall/door/collectible, then two rooms,
  an arched passage, elevation change, custom object, NPC and one-time reward.
- [ ] Expose a discoverable typed SDK/catalog and revision-aware transactional
  source edits. Preserve stable IDs, handwritten code and unrelated edits; source
  is authoritative, generated GLBs/IR/packages are not competing editable masters.
- [ ] Isolate executable generators/dependency installation with filesystem,
  network and credential restrictions, resource limits and watchdogs. Node vm
  and typing are not security boundaries. Publish immutable successful builds.
- [ ] Prove prompt -> discover -> author -> build -> legal play -> diagnose ->
  repair -> targeted revision -> package. Protect acceptance tests from author edits;
  retain source diagnostics, immutable test versions and generated/mixed-asset tests.

**Exit:** Mods and the small authoring bridge work with protected native verification;
source-preserving human/agent edits and prebuilt headless play are demonstrated.

## Phase 8 — Persistence and time controls

**Goal:** Robust player conveniences built on foundational runtime state/time services.

- [ ] Complete ordinary saves and supported original-save import/export; implement
  save states, rewind and fast-forward, including event/Lua/audio/media progress.
- [ ] Verify deterministic restore across flat/stereo/XR and headless modes,
  long runs, errors and package changes. Qualify supported cross-device save/state
  interchange; reject incompatible states explicitly rather than silently migrating.

**Exit:** Saves and time controls preserve required state and timing semantics.

## Phase 9 — Gameplay quality of life

**Goal:** Explicit independently implemented options, separate from unknown behavior.

- [ ] Default random battles off in identified platforming-heavy areas; preserve
  deliberate settings and record how areas are classified.
- [ ] Implement modern cutscene skipping with correct story outcomes and desired
  mod-inspired options independently, without requiring patched original content.
- [ ] Verify settings, defaults, ordinary progression and agent visibility of changes.

**Exit:** Options work without disguising missing original logic or breaking progression.

## Phase 10 — Extended camera, VR, media and headphone audio

**Goal:** Add optional presentation features grounded in original behavior.

- [ ] Extend Horizon stereo-screen play with seated diorama viewing, then a
  separately qualified optional immersive first-person mode. Retain flat/stereo
  play; VR must not require first-person gameplay. Separate head pose from scripted
  cameras and provide recentering, world-scale and comfort controls.
- [ ] Implement playable first-person mode on flat displays and clearer FMVs with
  an original option. Qualify sprite planes, missing surfaces, masks/effects and
  UI/media placement per camera mode; 2D assets do not become volumetric in stereo.
- [ ] Implement Wide-derived headphone surround only after verifying the actual
  Wide signal and intended playback model. Distinguish matrix encoding from
  phase-based expansion; select a justified reconstruction/decoder and HRTF path
  with exactly two output channels, not generic widening presented as surround.
- [ ] Retain selectable original Mono, Stereo and undecoded Wide; verify effects,
  music and FMV routing, device changes and synchronization.

**Exit:** Diorama and immersive modes, flat-display first-person play and optional
media/audio features are verified independently, without fidelity overclaims.

## Phase 11 — Graphical level and cutscene editors

**Goal:** Human clients of the same source/build/runtime/debug services agents use.

- [ ] Deliver GPUI desktop level, geometry, object, event and cutscene editing,
  selection/source attribution, preview, diagnostics, undo/redo and transactions.
- [ ] Preserve components, stable IDs and handwritten source; do not promise
  arbitrary code round-tripping. No operation exists only in a GUI callback.
- [ ] Verify mixed human/agent revisions and build/play/repair workflows.

**Exit:** Editors produce source-preserving playable packages through shared services.

## Phase 12 — Battle and minigame tooling

**Goal:** Extend the shared tools to custom combat and activities.

- [ ] Provide graphical battle-system, encounter, AI, balance and minigame tools,
  including Lua/event authoring, direct headless playtests and full diagnostics.
- [ ] Verify multiple seeds/policies and distinguish simulation time from estimated
  human reading/decision time. Preserve the same runtime and package contracts.

**Exit:** These systems are editable and verifiable without another rules model.

## Phase 13 — Release qualification

**Goal:** Reproducible desktop/mobile/headset play and a desktop modding toolkit.

- [ ] Qualify Arch-first Linux, Windows, macOS, Android and Meta Horizon OS builds,
  appropriate graphics backends, installation/upgrades, input/audio devices,
  performance and headless/no-image operation. Distinguish desktop authoring/tools
  from mobile/headset play; prebuilt packages use the same runtime contracts.
- [ ] Test mobile storage/asset import, save persistence, background/resume,
  surface/device recreation and controller/audio reconnects on actual devices.
  Qualify sustained performance/thermal behavior and XR frame pacing, stereo,
  tracking, session transitions and in-headset settings; a cross-build is not enough.
- [ ] Run original-game, agent, authoring, mod, editor, save/time and presentation
  regressions; verify defaults, optional original behavior and error diagnostics.
- [ ] Audit licenses and the explicit authored-source allowlist; distribute no
  original images, extracted assets, personal saves or unreviewed dependencies.
- [ ] Document supported revisions, devices/OS versions, presentation modes,
  limitations, reproducible commands and user workflows. Ship tested artifacts,
  not merely completed specifications.

**Exit:** All preceding phase outcomes remain passing in the supported release builds.
