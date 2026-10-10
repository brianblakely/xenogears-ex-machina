# Xenogears Ex Machina

**Recover the complete original program first; port it second; extend it third.**

An independent Xenogears decompilation, multiplatform port and modding toolkit,
led on Arch Linux. Target native Linux, Windows, macOS, Android and Meta Horizon OS,
plus browsers through WebAssembly with WebXR support. Keep full authoring/editors
desktop-focused; support prebuilt play on native hosts and in desktop, mobile and
headset browsers. Add an optional HD-2D-inspired graphics mode alongside modern
and PS1-style presentation. Recover game-specific code and data formats from the
user's original images and observed execution, not another Xenogears project's
implementation. General-purpose compilers, decompilers, emulators and libraries
are welcome.

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

## Shared native/browser architecture — Phases 2 onward

Use a library-first Rust/C runtime, not a new general-purpose engine. Keep the
recovered C as the authoritative original-game implementation; use Rust for new
native infrastructure and narrow, source-correlated portability adapters. Do not
require a wholesale Rust rewrite or a second independently updated game-state
model. These choices add no work to Phase 1's original-compatible matching exit.

The decomp stays a pure, byte-identical implementation of the original game.
It remains the runtime's game-logic provider, compiled unchanged into the
native and browser builds, but it is never edited for native client
integration: no port-only conditionals, hooks, prototypes, renames or layout
changes in `decomp/`. Everything integration needs lives in the separate port
layer (`port/`) and the build tooling around the unmodified sources
([runtime.md](docs/runtime.md)). Changes to `decomp/` serve the original game
only and must keep every target byte-identical.

- **SDL3:** Default desktop/Android platform layer for game windows/surfaces,
  keyboard/mouse/touch/controllers, audio-device I/O and lifecycle integration.
  Reuse applicable services on Horizon OS; use native APIs only for demonstrated
  gaps. SDL is not the game renderer or the VR abstraction; do not add a parallel
  Windows XInput path for capabilities already supplied through SDL.
- **wgpu:** Shared game renderer across flat, stereo and XR presentation. Use the
  appropriate Vulkan/Direct3D 12/Metal backends on native hosts and WebGPU/WebGL2
  backends on qualified browsers. Native game surfaces integrate with SDL; browser
  surfaces use a canvas. OpenXR swapchains and WebXR render targets integrate
  through narrow graphics adapters, not a second renderer or per-frame CPU copy.
  Qualify WebXR/graphics interop separately: a working WebGPU canvas does not
  prove WebXR submission support. Use WebGPU where supported and a tested WebGL2
  path where needed for declared browser/XR coverage; expose capability limits.
- **OpenXR:** Horizon immersive sessions, headset/controller poses and actions,
  hand tracking and capability-gated eye gaze/thumb gestures, stereo views,
  predicted display timing and frame submission. Keep gesture interpretation in
  an input adapter to shared commands. An Android app displayed as a flat panel
  is not evidence of immersive hand/gaze access, stereo or diorama support.
- **WebAssembly/browser host:** Compile the same recovered C and Rust runtime to
  WebAssembly, with a qualified shared ABI/link strategy and thin JavaScript glue,
  not a JavaScript game rewrite, emulator or streamed native session. Browser APIs
  own canvas/input, Web Audio, asynchronous asset/storage access and page lifecycle;
  do not require the desktop SDL loop, native filesystem access or a local server
  process for hosted play. Keep a usable single-thread baseline; gate optional
  workers/shared-memory acceleration on proven browser and deployment support.
- **WebXR:** Browser immersive sessions, poses/views, reference spaces, frame
  scheduling and input sources, separate from the native OpenXR adapter. Support
  immersive VR on qualified browsers/devices, including a Horizon headset browser;
  detect session and optional hand/gaze capabilities rather than inferring them
  from native-device support. Require secure hosting, user-initiated entry and
  explicit exit; unavailable or denied XR must leave ordinary browser play usable.
- **Slint:** Application interface for launcher/settings, bindings, asset import,
  save/mod management and later tools on desktop, Android/Horizon panels and in-XR
  panels. Run it as a custom Slint platform under the host loop: forward host input
  and render through Slint's wgpu renderer into textures on the shared wgpu device
  and queue, not a second window, event loop or GPU stack. Pin Slint and wgpu
  together while Slint's wgpu integration is unstable. This UI uses an ordinary
  application style, not Seraph Glass. Original dialogue, battle menus and other
  game UI stay in game presentation. Qualify Slint's WebAssembly rendering before
  using it in browsers; otherwise use a browser-appropriate application UI over the
  same services. Settings must remain accessible during browser play and inside XR
  even when ordinary page UI is not visible.

Keep application services (validation, settings persistence, session control and
available save/mod operations) independent of widgets. GUIs and agents call the
same typed operations and observe acknowledged results; no capability exists only
in a GUI callback. Logical separation does not require separate executables or
processes: settings must remain accessible while playing.

Give each host explicit window, GPU-resource, input-focus and lifecycle ownership.
Drive Slint from the host loop under platform main-thread requirements; do not
start competing application loops. OpenXR or WebXR owns immersive frame
scheduling; browser animation callbacks own flat web presentation, not a blocking
native loop. One owner mutates simulation state through commands; presentation
consumes read-only state and never advances gameplay per eye, UI repaint or
spectator view. Yield browser work in bounded batches; page throttling, lost focus
and XR visibility changes must not create uncontrolled simulation catch-up.
Keep graphical dependencies optional: the native headless runtime needs no Slint,
wgpu, OpenXR, WebXR, browser or SDL video/audio initialization.

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

- [x] Establish an original-compatible compiler/assembler/linker configuration.
  Match representative arithmetic, globals, structures, loops, switches and larger
  callers across resident and overlay code. Record exact versions, flags, ABI,
  small-data/GP assumptions and layout in the build configuration. A modern MIPS
  compiler or selected m2c target does not establish the original compiler.
- [x] Build every supported executable image incrementally, preserving entry
  points, sections, original data, alignment, relocations and overlay identities.
  Original assembly/binary placeholders may keep intermediate builds working;
  report their remaining ranges separately from source-covered ranges.
- [x] Recover original game code in cohesive resident/overlay modules, using
  original-compatible C, small shared types and narrowly scoped headers. Replace
  placeholders continuously. Reuse existing analysis, recovered algorithms and
  types; do not require porting each function into Program or a new ownership model.
- [x] Cover every executable region on both discs, including world map, Gear and
  on-foot battles, field/event systems, menus/saves, audio/media, optional content,
  minigames, shared libraries, startup and hardware interfaces. Classify genuine
  handwritten assembly and SDK/library code explicitly; reconstruct/link their
  required code without silently excluding it from the image or coverage report.
- [x] Recover data layouts, dispatch tables and every used script instruction.
  Preserve game bytecode/data as user-supplied assets; document semantics and
  provide useful parsers/disassembly without rewriting every asset as source code.
- [x] Close source coverage: no unknown executable ranges, unreviewed generated
  pseudocode, binary-only placeholders standing in for recoverable compiled game
  logic, guessed formulas or unexplained exclusions. Track source-reviewed but
  nonmatching functions separately; they do not satisfy the matching exit.
- [x] Produce exact final code/data/layout comparisons for every executable and
  overlay target, with no address/stack masks or normalized-diff substitutes.
  Verify decoded overlay images first; track compressed-container reproduction
  separately. Whole-disc filesystem/ECC reproduction is not an extra hidden gate.
- [x] Recover the service/timing/state boundaries needed by the port and the
  original visual and Mono/Stereo/Wide behavior. Use targeted original observation
  for uncertainty, formats and integration, not a new capture/evidence ceremony
  for each function that already has a qualified binary match.
- [x] Verify clean builds from user-supplied sources and run original-environment
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

## Phase 2 — Shared runtime and platform foundation

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
  surface with a Slint settings panel rendered on the same wgpu device; Android
  startup, asset access and suspend/resume; and an actual Horizon headset rendering
  a stereo test scene and a Slint panel through wgpu/OpenXR. Verify swapchain/
  device ownership and synchronization, frame scheduling and head tracking while
  simulation is paused. These are foundation checks, not the full UX or VR feature
  work of Phases 5, 6 and 10.
- [ ] Prove the browser Rust/C WebAssembly build with a real execution path, canvas
  rendering, user-activated audio, asynchronous local asset import and persisted
  settings/save round-trip. Bound memory and stream/cache assets rather than
  requiring both discs to fit in memory. Keep imported game data local; the hosted
  application must neither bundle original assets nor upload users' images/saves.
- [ ] Render an actual stereo test scene through WebXR on a target headset browser.
  Qualify graphics-device/render-target ownership, projections, synchronization,
  session entry/exit and paused head tracking, including the chosen WebGPU or
  WebGL2 submission path. Neither cross-compilation nor a flat canvas closes this.
- [ ] Expose the same direct commands, full introspection, stepping, bounded
  run-until and snapshots to browser clients/automation without DOM-input simulation
  or privileged host services. Verify native/WebAssembly authoritative outcomes;
  browser scheduling limits must not weaken the native unlocked headless contract.
- [ ] Demonstrate a small original-game execution path through the real native
  services, including direct commands, inspection and snapshot/restore. Compare
  authoritative outcomes with presentation attached and absent; use the retained
  behavioral tests to detect portability regressions.

**Exit:** A tested shared, agent-ready Rust/C runtime and qualified platform/graphics
integration paths exist, including browser execution and actual WebXR submission.
Defining schemas or cross-compiling alone does not prove integration. No authoring
engine or full application UI is required before play.

## Phase 3 — First end-to-end multiplatform playable slice

**Goal:** Prove original gameplay through the shared runtime, not a replacement demo.

- [ ] Integrate the established original field/dialogue/encounter/reward/return/
  menu/save-load/media route from recovered source, including required branches.
- [ ] Compare meaningful original states, outcomes and timing. Test continuous
  legal play separately from debug-launched scenarios; no corrective state injection.
- [ ] Demonstrate human and direct-agent control, complete relevant state/debug
  access, valid setup, snapshots, replay and equivalent stepped/unthrottled runs.
- [ ] Attach/change/disconnect optional spectator output without changing state
  or simulation pacing. Image-free headless play must need no GPU or audio device.
- [ ] Run the same slice on native desktop/Android/Horizon flat-panel hosts and
  desktop, mobile and headset browsers. Verify local asset import, audio/media,
  saves across reload and lifecycle recovery on actual devices; do not maintain
  separate platform-specific game logic.

**Exit:** The complete slice works end to end under human/agent clients on the
target platform hosts. Its integration evidence, not a TypeScript authoring
prerequisite, closes this phase; full stereo/VR qualification remains later work.

## Phase 4 — Complete native/browser gameplay on both discs

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

**Exit:** Both discs and all original systems work on native and browser hosts with
human/agent parity, including disc transitions and streamed audio/FMV playback.
Unknown behavior and progression workarounds cannot be reported as completion.

## Phase 5 — Modern/HD-2D rendering, stereo and optional PS1 fidelity

**Goal:** Modern flat/stereo presentation by default, with optional HD-2D-inspired
visual effects and selectable original quirks on native and browser hosts.

- [ ] Use the qualified native and browser wgpu backends across the targets;
  support arbitrary resolution/aspect ratio and presentation framerate,
  resizing/high-DPI, correct projection/FOV/UI safe areas and interpolation
  without changing simulation/RNG/script/animation timing.
- [ ] Implement Modern, HD-2D-inspired, PS1-style and Custom profiles. Keep affine
  texturing, projected vertex snapping, color precision, dithering, low-resolution
  rasterization and recovered ordering/transparency quirks independently selectable
  at their appropriate rendering stages; adding HD-2D does not change the default.
- [ ] Make the HD-2D-inspired profile apply only Square Enix HD-2D-like visual
  effects to existing game assets: coherent sprite/world lighting, contact/cast
  shadows, ambient occlusion, atmospheric fog or light shafts, restrained bloom,
  color grading and adjustable depth of field for a miniature/diorama feel.
  Implement the look through scene-aware shaders, lighting and post-processing,
  not sprite smoothing, an upscale or an art remake. Do not create, redraw, replace
  or require new sprites, textures, models, animations or other art assets for
  this mode; no original-art production or replacement art pack is in scope.
  Preserve the existing sprite pixels/animation and 3D Gear/world geometry.
- [ ] Keep new lighting/material/scene-tuning metadata in the presentation layer,
  separate from recovered logic and original assets. Do not make new texture maps
  or other authored art a dependency. Preserve cutout silhouettes,
  transparency, intentional masks, important color cues and original scene layout;
  handle existing baked shading deliberately. UI/text/portraits/FMVs and Seraph
  Glass controls stay legible and outside world depth-of-field/post effects.
- [ ] Provide HD-2D effect controls and explicit quality tiers for desktop, mobile,
  browser and XR budgets. Preserve the pixel-art/lit-world identity at lower tiers;
  never silently replace the selected profile. Default depth of field off in XR,
  keep stereo effects and sprite planes coherent between eyes, and make bloom,
  blur and atmospheric effects reducible without changing gameplay or camera mode.
- [ ] Validate side-by-side representative interiors/exteriors, on-foot/Gear
  battles, world map, effects and cutscenes against the intended HD-2D look and
  original readability. Verify profile switching and native/browser/flat/XR output
  leave authoritative state, RNG and timing unchanged; HD-2D requires no mod tools.
- [ ] Preserve intentional masking, palettes, fog and compositing. Keep world,
  sprite and UI filtering independent; do not stretch text, portraits or FMVs.
- [ ] Default optional culling to off; distinguish visibility/frustum/occlusion/
  back-face rejection from clipping, intentional hiding and gameplay activation.
- [ ] Validate wider framing/content issues separately, cross-backend visuals and
  unchanged gameplay with rendering skipped, offscreen or spectator-streamed.
- [ ] Qualify native Horizon and browser flat virtual-screen play, then seated
  stereo-screen play through OpenXR and WebXR respectively. Render both eyes from
  the same simulation state using the headset's views/projections; place game UI,
  sprite planes and FMVs deliberately.
  Preserve monoscopic media as such rather than claiming reconstructed 3D content.
- [ ] Verify paused-game head tracking, display-paced XR rendering independent of
  simulation rate, and spectator attach/detach without extra game updates or RNG
  changes. Measure sustained headset frame pacing; never run game logic per eye.

**Exit:** Display options, the HD-2D-inspired and fidelity profiles, and native/
browser stereo-screen play work without altering game rules. Flat presentation
remains available; graphics style and viewing mode are independent choices.

## Phase 6 — Cross-platform controls and application UX

**Goal:** Native and browser human input use the same command path as agents.

- [ ] Implement SDL3/native OpenXR input and browser keyboard/pointer/touch/
  gamepad/WebXR input, context bindings and rebinding for direction/run/jump/
  confirm/cancel and camera left/right. Translate platform events into shared typed
  actions; agents issue those actions directly without SDL, DOM or OS injection.
- [ ] Make menus mouse-browsable with wheel scrolling; on desktop, Tab toggles the
  game menu and Esc toggles the app menu. Provide touch/headset equivalents and
  explicit focus/capture transitions without rebuilding original game UI in Slint.
- [ ] Implement the shared headset hand/gaze contract below as an optional
  assisted-input profile. Navigation and approach/interact use authoritative
  traversal data and shared legal commands, not coordinate rewrites, remote confirms
  or another game rules model. Expose resolved intents/actions to agents and deterministic replay;
  replay must not depend on recording raw eye movements or headset frame timing.
- [ ] Deliver the Slint launcher/settings and available application-service
  screens. Apply changes through shared validation, acknowledgment and persistence;
  changing settings from an agent uses the same path. Add later save/mod/editor
  screens when their owning phases implement those services.
- [ ] Qualify the Slint application controls on actual mobile and headset devices,
  including controller and hand-ray input on in-XR panels. Verify controller
  reconnects, live settings changes and return to play; a desktop companion is not
  a substitute for headset settings.
- [ ] Deliver browser application controls as their shared services become
  available, plus usable in-XR panels; do not depend on DOM overlays being available.
  Provide visible Enter/Exit XR and controller/pointer alternatives. Handle page
  focus, fullscreen/pointer-lock release, browser-reserved keys, audio activation,
  session visibility/end and permission denial without stuck inputs or lost saves;
  Esc must not be the only way to reach app settings in a browser.
- [ ] Verify human/agent parity, the pause/focus rules below and actual-device
  gesture reliability. Test each hand alone with the other absent from tracking,
  gaze availability/permissions and explicit pointer alternatives, running jumps,
  interaction interruptions, dialogue, shops, both battle types, world-map/vehicle
  controls and every minigame. Measure false activations, missed gestures, response
  latency, rapid pause/resume, sustained comfort and tracking-loss recovery.

### Shared headset hand/gaze interaction contract

Apply this contract through native Horizon OpenXR and browser WebXR adapters.
Qualify each supported browser/device input profile independently: hand joints or
a target ray do not establish eye-gaze or reliable thumb-microgesture support.
Keep unsupported gestures explicitly unavailable and provide complete controller/
pointer controls; do not claim hand/gaze parity from a generic select event.

All game functions, including Pause and inspection, must be operable with either
single hand; app settings and presentation manipulation may use two hands.
Qualify the Phase 6 controls on Phase 5's virtual-screen/stereo presentation.
Diorama scaling and first-person inspection use the bindings specified here when
Phase 10 implements those views; they are not prerequisites for Phase 6's exit.
These requirements do not add work to the Phase 1 matching decompilation.

Use capability/permission checks for hand tracking, eye gaze and supported thumb
microgestures; do not assume every device/browser provides them. Offer explicitly
selected head-directed or one-hand-ray targeting when eye gaze is unavailable,
without silently switching pointer sources during a gesture. Looking only aims
or highlights: no gaze-only walking, selection, dwell activation or retargeting
of an already committed action.

#### Gesture vocabulary and global controls

A normal **pinch** joins thumb and index fingertips. A **thumb tap** taps the side
of the index finger; thumb swipes travel up/down/left/right along that surface.
A **five-finger grab** curls the fingers into the palm with the thumb closing
around them. A **whole-hand pinch** instead gathers the fingertips toward the
thumb. Keep these recognizers distinct, including their transitional poses.

| Gesture | Action |
| --- | --- |
| Overhanded five-finger grab, palm down | Toggle gameplay Pause immediately on closure |
| Underhanded five-finger grab, palm up | Toggle stationary first-person inspection where available |
| Two whole-hand pinches, spreading/contracting | Enlarge/shrink the diorama |
| Prayer hands | Toggle app settings, not the game menu |

For either oriented grab, require a fresh open-to-closed transition. Classify
palm orientation relative to room vertical as closing begins and require it to
remain in that region through closure; allow comfortable angles and a sideways
neutral region that triggers neither action. No hold, dwell, release-to-trigger
or deliberate cooldown: reopening rearms the next closure. A resting fist,
rotating a closed fist or reacquiring a closed hand must not trigger. Coalesce
simultaneous matching grabs into one toggle rather than toggling twice.

Pause works in every gameplay context and during inspection without a gaze target.
Keep head tracking, presentation and app input live while simulation is paused.
Settings add a separate app-menu pause; closing them removes only that pause and
preserves the player's Pause toggle. Inspection changes the view, not pause state.
Clear transient gesture/button inputs across pause and focus transitions so no
buffered jump, attack or selection fires on return.

Prayer hands is a deliberate approach of open, aligned palms facing one another;
near-contact is sufficient. Trigger once, separate to rearm, and leave settings
open while the hands separate to operate them. Repeating the gesture or selecting
Return to Game closes settings. Anchor the panel comfortably outside the game
presentation, not between hands that must remain together.

Arbitrate gestures before dispatch: a grab must not also emit a navigation pinch
or thumb command. Two-hand scaling owns its full gesture through release and must
not emit grab toggles. Runtime system gestures take priority over XEM actions.
Each event belongs to one active context/target; never carry it into a newly
opened list, dialogue choice, battle turn or different pointer source. Loss of
active-hand tracking or app focus releases held inputs, cancels pending gestures
and queued interaction, and pauses uncontrolled gameplay; require neutral input
and explicit resume, not automatic reengagement. An unused second hand is not
required tracking. Lost gaze blocks new gaze-targeted actions, not a substitution
of a different target for an already accepted command.

#### Field traversal and interaction

- Use a discreet, stabilized, surface-aligned gaze cursor in the spirit of the
  iOS Measure cursor. Select visible walkable surfaces using collision/traversal
  data and occlusion, not scenery behind an obstruction. Distinguish walking
  reachability, a required manual jump and invalid destinations; do not reveal
  otherwise hidden interactions. Keep the live cursor and committed destination
  visually related but distinct.
- A single normal pinch commits the destination; no sustained pinch or joystick.
  Pathfinding supplies legal movement inputs. Run for distant destinations and
  walk only extremely nearby, using remaining route distance and separated speed
  thresholds to avoid oscillation. A new pinch replaces the route. Looking away
  or rotating the camera does not move its world-space destination.
- Thumb tap on an interactive object/NPC commits approach-and-interact: retain
  target identity, approach a valid position/facing and Confirm exactly once when
  the original interaction is legal. Bound replanning for moving NPCs. Cancel on
  replacement commands, unrelated dialogue, battle or scene transitions; never
  let a queued Confirm act on a different interface. Thumb tap on noninteractive
  ground stops the route without selecting another destination.
- Thumb swipe **up** jumps; **left/right** rotate the field camera; **down** opens
  the game menu, cancels navigation and does not restart it when the menu closes.
  Keep manual jump timing and original movement/jump physics. Support destinations
  across gaps: approach a takeoff area, preserve the intended direction/destination
  through a player-triggered jump and replan after landing. Do not reject every
  disconnected platform, auto-jump, guarantee a landing or slow a running jump
  merely because its landing point is nearby. Make ledge braking an explicit
  assistance option. Qualify gesture-to-jump latency with real platforming routes.

#### Menus, dialogue and shops

Use a 2D gaze cursor in the same visual language; thumb tap selects and thumb swipe
**right** cancels/goes back. Gaze over a list plus thumb **up/down** scrolls that
list: up moves content upward to later entries, down reveals earlier entries.
On character-switching menus, gaze outside scrollable lists lets up select the
**next** character and down the **previous** character. Show the receiving region;
list scrolling takes priority even at its boundaries, with no fallthrough to
character switching. **Never use pinch-and-drag scrolling**, including in native
and browser settings. Each swipe targets one stable region, with bounded scrolling
and no transfer to another panel during its animation.

Thumb tap advances ordinary dialogue one step. Choices require gaze selection
and a fresh tap; the input that reveals a choice must not also answer it. Keep
skipping/fast-forward separate from ordinary advance. Named gaze-selectable
context actions expose otherwise awkward commands such as Gear boarding/exiting
without requiring two-handed button chords or bypassing normal eligibility.

In shops, put **+, -, ALL, NONE** virtual quantity buttons outside the game surface
(and outside the Phase 10 diorama). They edit the selected item's proposed
quantity only: ALL selects its maximum legal quantity subject to funds, stock,
inventory and transaction limits; NONE clears that proposal. Display quantity
and total, then separately confirm the purchase/sale. Do not reuse left/right
thumb swipes for quantities or interpret ALL as every item in the shop.

#### Battles and bespoke minigames

On the command ring, thumb up/right/left/down directly activate the corresponding
visible slots; **thumb tap cycles command-ring pages** with a visible page
indicator. Do not use pinch for paging or require an extra tap for each command.
On the attack ring, **up = Weak, left = Strong, down = Fierce, right = End the
attack sequence**. Keep positions fixed and unavailable attacks visibly disabled;
each swipe submits one legal input, with no extra confirmation, automatic repeats
or undo of executed attacks. Thumb tap does nothing on an unpaged attack ring.
Respect the original on-foot/Gear costs and timing rather than assuming one model.

Ability/item lists follow the menu rules, including right to back out rather
than activating a ring behind the list. Select requested enemy/ally/group targets
by gaze plus thumb tap; offer a target list for overlap/occlusion. Confirmed targets
remain selected while looking elsewhere, unless the game requires new targeting.
No page-change/attack gesture may leak into a new list, target step or next turn.

Provide a custom virtual arcade control deck for each minigame, outside the game
surface/diorama. Tailor it to that game's actual directional, timing, selection,
held and simultaneous inputs rather than forcing field navigation or a generic
controller layout onto every game. Each deck must support complete one-handed
play, including simultaneous steering/actions where required; Pause remains
available. Discrete thumb events alone are not held inputs: define explicit
press/release or captured-control behavior and release everything on exit/focus
loss. Use shared legal commands, not automated gameplay sequences.

#### Seraph Glass visual language

**Seraph Glass** applies only to bespoke widgets added for VR functionality: hand/
gaze cursors, targeting rings, scaling handles, VR shop/arcade decks and other
native/browser VR controls. It does not apply to settings, launcher, bindings,
save/mod management, editors, in-XR settings panels or other ordinary application
UI, which use a plain application style. It is not a glass coating over the
original game world or a reason to rebuild original game UI. Give these widgets
iOS Liquid Glass-like material behavior with Xenogears' Seraph Angels-inspired
structure. Use faceted silver-white shells, hollow halos, tapered fins, dark
recesses and sparse gold/copper accents around translucent inner surfaces, not
generic blue holograms.

Keep cursor centers stable, silhouettes related across surface/2D targeting and
committed destinations, and feedback restrained. Give ornate shapes generous,
simple hit areas; communicate state through shape/contrast as well as color.
Text and essential symbols stay solid and readable, with reduced-transparency
and reduced-motion options. External controls retain comfortable angular sizes
independent of diorama scale and do not obscure the play area.

**Exit:** Every original system and application settings are usable through the
appropriate desktop, browser, touch and headset controls, including qualified
one-hand Horizon/WebXR gameplay, with human/agent action parity. Phase 10 qualifies
the diorama and stationary-inspection presentation bindings.

## Phase 7 — Mods and source-oriented agent authoring

**Goal:** Add authoring on the proven runtime; do not build a second game engine.

- [ ] Implement native mod packages, dependency/conflict handling, deterministic
  load order, validation, graphical mod management and explicit save compatibility.
- [ ] Provide Lua gameplay extensions with shared scheduling, debugging and
  serializable progress. Integrate custom tasks with snapshots/replay and errors.
- [ ] Build the small TypeScript/optional TSX source-to-playable bridge here:
  source/parameters -> supervised Node build -> IR -> meshes/world/collision/events
  -> runtime package. The shared Rust/C runtime/events/Lua own gameplay. Native
  prebuilt play needs no browser; native and browser prebuilt play need no Node,
  QML, authoring tools or second game runtime. Keep supervised source builds and
  full authoring on desktop; qualify portable prebuilt mods and Lua in WebAssembly
  without native plugins or privileged filesystem/process assumptions.
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
- [ ] Persist browser saves/settings in qualified origin-local storage (such as
  IndexedDB/OPFS), acknowledge completed writes and offer explicit import/export
  backup and native/browser interchange. Handle quota exhaustion, denied storage,
  eviction and interrupted writes visibly; do not rely on unload-time saving or
  describe browser-managed storage as guaranteed permanent. Bound rewind memory.
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

- [ ] Extend native OpenXR and browser WebXR stereo-screen play with seated
  diorama viewing. Retain flat/stereo play; VR must not require first-person
  inspection. Separate head pose from scripted cameras and provide recentering,
  reference-space reset handling and comfort controls. Where immersive AR is
  supported, allow the same diorama over passthrough without making AR-specific
  sensing a requirement for VR or ordinary browser play.
- [ ] Implement stationary first-person inspection on both VR and flat displays,
  using the Phase 6 underhanded-grab binding and equivalent conventional inputs.
  This is look-around only, not FPS locomotion: keep the character stationary,
  disable navigation/approach-and-interact/jump commands, and cancel the route on
  entry. Head tracking supplies VR looking; flat-display look input rotates the
  view without moving the character. Restore the prior field camera/diorama scale
  on exit without restarting the old route. Preserve Pause independently and
  avoid forced viewpoint changes from character turning or animation.
- [ ] Implement diorama scaling with two whole-hand fingertip pinches, distinct
  from fists and normal index pinches: spread to enlarge, contract to shrink,
  release either hand to finish. No rim targeting is required. Keep the anchor
  stable; change presentation scale only, never physics or game-state distances.
  The scaling gesture must not emit movement or grab toggles. Place the Seraph
  Glass shop/minigame decks and other external controls outside the diorama and
  retain readable control sizes while it scales; provide scale/reset settings.
- [ ] Qualify transitions, paused head tracking, scale/gesture ownership, either
  hand's oriented grabs and two-hand app controls on native/WebXR devices. Qualify
  sprite planes, missing surfaces, masks/effects and UI/media placement per camera
  mode; 2D assets do not become volumetric in stereo. Provide clearer FMVs with an
  original option independently of camera-mode qualification.
- [ ] Implement Wide-derived headphone surround only after verifying the actual
  Wide signal and intended playback model. Distinguish matrix encoding from
  phase-based expansion; select a justified reconstruction/decoder and HRTF path
  with exactly two output channels, not generic widening presented as surround.
- [ ] Retain selectable original Mono, Stereo and undecoded Wide; verify effects,
  music and FMV routing, device changes and synchronization.

**Exit:** Diorama viewing/scaling and stationary first-person inspection work on
native and browser flat/VR hosts, with capability-gated AR where supported.
Optional media/audio features are verified independently, without fidelity
overclaims or changes to game physics.

## Phase 11 — Graphical level and cutscene editors

**Goal:** Human clients of the same source/build/runtime/debug services agents use.

- [ ] Deliver Slint desktop level, geometry, object, event and cutscene editing,
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

**Goal:** Reproducible native/browser desktop/mobile/headset play and a desktop
modding toolkit.

- [ ] Qualify Arch-first Linux, Windows, macOS, Android and Meta Horizon OS builds,
  appropriate graphics backends, installation/upgrades, input/audio devices,
  performance and headless/no-image operation. Distinguish desktop authoring/tools
  from mobile/headset play; prebuilt packages use the same runtime contracts.
- [ ] Test mobile storage/asset import, save persistence, background/resume,
  surface/device recreation and controller/audio reconnects on actual devices.
  Qualify sustained performance/thermal behavior and XR frame pacing, stereo,
  tracking, session transitions and in-headset settings; a cross-build is not enough.
  Regress one-hand gameplay, oriented grabs, gaze/list focus and two-hand native
  gestures across supported tracking capabilities and presentation modes.
- [ ] Ship a reproducible browser WebAssembly/static-web build and qualify an
  explicit browser/OS/device matrix, including desktop/mobile flat play and actual
  Horizon-browser WebXR. Test the selected WebGPU/WebGL2 paths, secure hosting and
  required headers, loading/memory budgets, storage loss/quota errors, save backup,
  audio/media, both-disc transitions, page suspend/reload and GPU/context loss.
  Optional shared-memory builds must not make baseline play depend on them.
- [ ] Qualify WebXR entry/exit/reentry, denied/missing capabilities, reference-space
  and input-source changes, paused head tracking, controller/hand profiles and
  in-headset settings on actual supported browsers/headsets. Verify HD-2D quality
  tiers, stereo correctness and sustained frame pacing across native/browser
  presentation modes; report unsupported combinations rather than implying parity.
- [ ] Run original-game, agent, authoring, mod, editor, save/time and presentation
  regressions on the applicable native/browser targets; verify defaults, optional
  original behavior and error diagnostics.
- [ ] Audit licenses and the explicit authored-source allowlist; distribute no
  original images, extracted assets, personal saves or unreviewed dependencies.
- [ ] Document supported revisions, devices/OS/browser versions, graphics/XR
  capabilities, presentation modes, limitations, reproducible commands and user
  workflows, including self-hosting and local asset/save handling. Ship tested
  artifacts, not merely completed specifications.

**Exit:** All preceding phase outcomes remain passing in the supported release builds.
