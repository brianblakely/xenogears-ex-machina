# Xenogears Ex Machina

**Recover the complete original program first; port it second; extend it third.**

An independent Xenogears decompilation, native PC port and modding toolkit, led on
Arch Linux. Recover game-specific code and data formats from the user's original
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

- [ ] Establish C++/C integration, local asset import and native platform services
  for storage, input, graphics, audio and timing. Preserve source correlation;
  isolate pointer-width, arithmetic, layout and PS1-specific adaptations.
- [ ] Separate authoritative simulation from presentation and host pacing. One
  runtime serves humans, agents, tests and editor previews; no CPU emulator ships.
- [ ] Implement direct typed engine actions and structured full introspection,
  semantic readiness/valid actions, exact stepping, bounded run-until, unlocked
  speed, debugging, snapshots/replay and typed go-anywhere scenario setup.
- [ ] Run without a window, display server, GPU, input device or audio device.
  Make optional screenshots/spectator streams read-only and independent of time.
- [ ] Demonstrate a small original-game execution path through the real native
  services. Use the retained behavioral tests to detect portability regressions.

**Exit:** A tested shared, agent-ready runtime boundary exists. Defining schemas
alone does not implement it. No authoring engine is required before native play.

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

**Exit:** The complete native slice works end to end under both clients. Its
integration evidence, not a TypeScript authoring prerequisite, closes this phase.

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

## Phase 5 — Modern rendering and optional PS1 fidelity

**Goal:** Modern presentation by default, with original quirks individually selectable.

- [ ] Support Vulkan/Direct3D 12/Metal, arbitrary resolution/aspect ratio and
  presentation framerate, resizing/high-DPI, correct projection/FOV/UI safe areas,
  and interpolation without changing simulation/RNG/script/animation timing.
- [ ] Implement Modern, PS1-style and Custom profiles: affine texturing, projected
  vertex snapping, color precision, dithering, low-resolution rasterization and
  recovered ordering/transparency quirks at their appropriate rendering stages.
- [ ] Preserve intentional masking, palettes, fog and compositing. Keep world,
  sprite and UI filtering independent; do not stretch text, portraits or FMVs.
- [ ] Default optional culling to off; distinguish visibility/frustum/occlusion/
  back-face rejection from clipping, intentional hiding and gameplay activation.
- [ ] Validate wider framing/content issues separately, cross-backend visuals and
  unchanged gameplay with rendering skipped, offscreen or spectator-streamed.

**Exit:** Display options and fidelity profiles work without altering game rules.

## Phase 6 — PC controls and application UX

**Goal:** Native human input uses the same authoritative command path as agents.

- [ ] Implement keyboard/mouse, SDL3 controllers and Windows XInput, action/context
  bindings, rebinding, direction/run/jump/confirm/cancel and camera left/right.
- [ ] Make menus mouse-browsable with wheel scrolling; Tab toggles the game menu,
  Esc toggles the app menu. Keep focus/capture transitions explicit.
- [ ] Verify equivalent actions and no OS-event injection requirement for agents.

**Exit:** Every original system is comfortably playable through PC controls.

## Phase 7 — Mods and source-oriented agent authoring

**Goal:** Add authoring on the proven runtime; do not build a second game engine.

- [ ] Implement native mod packages, dependency/conflict handling, deterministic
  load order, validation, graphical mod management and explicit save compatibility.
- [ ] Provide Lua gameplay extensions with shared scheduling, debugging and
  serializable progress. Integrate custom tasks with snapshots/replay and errors.
- [ ] Build the small TypeScript/optional TSX source-to-playable bridge here:
  source/parameters -> supervised Node build -> IR -> meshes/world/collision/events
  -> native package. Native C++/events/Lua own gameplay; prebuilt play needs no
  Node, browser, QML, authoring tools or second runtime.
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
- [ ] Verify deterministic restore across modes, long runs, errors and package
  changes. Reject incompatible states explicitly rather than silently migrating.

**Exit:** Saves and time controls preserve required state and timing semantics.

## Phase 9 — Gameplay quality of life

**Goal:** Explicit independently implemented options, separate from unknown behavior.

- [ ] Default random battles off in identified platforming-heavy areas; preserve
  deliberate settings and record how areas are classified.
- [ ] Implement modern cutscene skipping with correct story outcomes and desired
  mod-inspired options independently, without requiring patched original content.
- [ ] Verify settings, defaults, ordinary progression and agent visibility of changes.

**Exit:** Options work without disguising missing original logic or breaking progression.

## Phase 10 — Extended camera, media and headphone audio

**Goal:** Add optional presentation features grounded in original behavior.

- [ ] Implement playable first-person mode and clearer FMVs with an original option.
- [ ] Implement Wide-derived headphone surround only after verifying the actual
  Wide signal and intended playback model. Distinguish matrix encoding from
  phase-based expansion; select a justified reconstruction/decoder and HRTF path
  with exactly two output channels, not generic widening presented as surround.
- [ ] Retain selectable original Mono, Stereo and undecoded Wide; verify effects,
  music and FMV routing, device changes and synchronization.

**Exit:** Optional camera/media/audio features are verified without fidelity overclaims.

## Phase 11 — Graphical level and cutscene editors

**Goal:** Human clients of the same source/build/runtime/debug services agents use.

- [ ] Deliver graphical level, geometry, object, event and cutscene editing,
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

**Goal:** A reproducible, distributable PC game/toolkit with complete requested coverage.

- [ ] Qualify Arch-first Linux, Windows and macOS builds/backends, installation,
  upgrades, input/audio devices, performance and headless/no-image operation.
- [ ] Run original-game, agent, authoring, mod, editor, save/time and presentation
  regressions; verify defaults, optional original behavior and error diagnostics.
- [ ] Audit licenses and the explicit authored-source allowlist; distribute no
  original images, extracted assets, personal saves or unreviewed dependencies.
- [ ] Document supported revisions, limitations, reproducible commands and user
  workflows. Ship tested artifacts, not merely completed specifications.

**Exit:** All preceding phase outcomes remain passing in the supported release builds.
