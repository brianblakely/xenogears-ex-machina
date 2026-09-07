# Xenogears Ex Machina — Native PC Port Plan

**Native core → modern PC features → Lua modding → graphical editors.**

Build **an Arch-first native game runtime and a companion editor suite sharing the same engine**. The important sequencing decision is to design deterministic state, scripting, and asset replacement early—even though rewind and the graphical tools arrive later.

There is already work worth evaluating: **Noah** is a partial, non-matching C++ reimplementation that runs on Linux and Windows, while **xenogears-decomp** pursues a matching decompilation. Neither should be treated as an already-complete foundation. ([Noah](https://github.com/yaz0r/Noah), [xenogears-decomp](https://github.com/ladysilverberg/xenogears-decomp))

## Phase 0 — Audit existing work and establish the project

**Goal:** Choose the foundation and define what “complete” means.

- [ ] **Audit Noah on Arch Linux.** Build it, identify reusable systems, reproduce its documented limitations, and assess how much its rendering, game-state ownership, and scripting architecture need to change.
- [ ] Review the matching decompilation as a source of verified behavior and reverse-engineering discoveries. **Do not make 100% binary matching a prerequisite for the native port.** ([xenogears-decomp](https://github.com/ladysilverberg/xenogears-decomp))
- [ ] Compare the separate XenogearsRecomp effort for useful discoveries and testing approaches, while distinguishing static recompilation from the readable, extensible implementation needed here. Its current documentation describes an alpha focused on the US first disc, without end-to-end validation. ([XenogearsRecomp](https://github.com/OpokXeno/xenogears-recomp))
- [ ] Select a reference release and verify source-image hashes. Start with **both US discs**, with explicit identification of supported revisions.
- [ ] Create a subsystem inventory covering fields, world map, battles, menus, event scripting, audio, FMVs, saves, and every minigame.
- [ ] Create a mod inventory with features, authors, dependencies, conflicts, and available popularity evidence. Use the shortlist in Phase 7 as the initial research set.
- [ ] Establish source provenance, upstream licensing, and attribution rules. Keep original game assets separate from distributed engine code.
- [ ] Turn every requested feature into a tracked requirement with an acceptance test and an assigned phase.

**Exit criterion:** A documented foundation decision, a reproducible Arch build, and a prioritized compatibility backlog.

## Phase 1 — Build the architecture that later features depend on

**Goal:** Avoid having to redesign the engine for rewind, mods, or editors.

- [ ] Establish a **C++/CMake/Ninja** build with pinned dependencies, debug builds, sanitizers, and automated tests. Make Arch the primary development and release-validation environment.
- [ ] Separate the code into game simulation, original-script runtime, rendering, audio/media, input/UI, asset management, persistence, and editing modules.
- [ ] Use **SDL3 for windowing, input, and platform integration**.
- [ ] Prototype **SDL3 GPU as the shared rendering abstraction**, explicitly selecting Vulkan on Linux, Direct3D 12 on Windows, and Metal on macOS. Its supported backends match this requirement; establish the cross-platform shader pipeline now rather than maintaining three unrelated renderers. ([SDL3 GPU documentation](https://wiki.libsdl.org/SDL3/CategoryGPU))
- [ ] Implement asset import from the user’s source images into a versioned local asset store. Assign stable IDs to maps, entities, textures, dialogue, sounds, and other resources.
- [ ] **Separate simulation time from rendering time.** Recover the original subsystem update cadences and preserve gameplay-relevant arithmetic independently of high-precision rendering.
- [ ] Define a serializable authoritative state model: entities, party, inventory, story flags, random-number generators, script execution, timers, battle state, and minigame state.
- [ ] Represent references with stable IDs or handles rather than saved raw pointers. Keep GPU resources and other rebuildable presentation caches outside authoritative state.
- [ ] Define the Lua persistence contract now: explicit saved data, resumable scripted tasks, deterministic scheduling, and version migration.
- [ ] Add a headless test runner, recorded-input playback, state hashes, and commands to load particular maps, encounters, and cutscenes.
- [ ] Start Windows and macOS compilation checks immediately, while keeping Arch as the primary runtime-testing platform.

**Exit criterion:** A minimal scene can run, serialize, restore, and replay deterministically without depending on the rendering backend.

## Phase 2 — Recover and complete the native game

**Goal:** Make the original game work before judging enhancements against it.

- [ ] Recover field systems: movement, running, jumping, collision, elevation, ladders, triggers, interaction, party following, camera behavior, and map transitions.
- [ ] Complete the original event-script interpreter and document its instructions, scheduling rules, waits, and side effects.
- [ ] Complete menus, inventory, equipment, shops, party management, dialogue, and ordinary save/load functionality.
- [ ] Complete on-foot battles: action selection, combos, Deathblows, targeting, status effects, enemy behavior, rewards, and scripted battle events.
- [ ] Complete Gear battles and their distinct rules, animation, equipment, resources, and interfaces.
- [ ] Complete world-map navigation, transportation, location entry, and story-dependent changes.
- [ ] Complete music, sound effects, FMV playback, and synchronization between these systems and event scripts.
- [ ] Complete every minigame and optional activity—not just the main story route.
- [ ] Complete both discs, including their shared resources, transitions, and late-game event behavior.
- [ ] Replace assumptions about PS1 addresses, overlays, memory layout, and hardware I/O with explicit native systems.
- [ ] Build behavioral regression tests against the original game. Use an emulator as a **test reference**, not as the shipping game runtime.

**Milestone 2A — Playable slice:** Field exploration → dialogue/cutscene → battle → menu → save/load works on Arch.

**Milestone 2B — Complete baseline:** Both discs and optional activities are playable without debugger intervention or progression workarounds.

**Parallelization:** Renderer, input, and mod infrastructure work can begin after 2A while game-completion work continues toward 2B.

## Phase 3 — Modern rendering, arbitrary resolution, aspect ratio, and framerate

**Goal:** Preserve the game’s artwork and presentation intent without reproducing PS1 rendering defects.

- [ ] Render from usable geometry, materials, textures, and camera transforms rather than treating the final PS1 framebuffer as the game’s presentation.
- [ ] Eliminate intentional emulation of affine texture distortion, vertex snapping/wobble, low-color framebuffer quantization, PS1 dithering, and low-resolution rasterization.
- [ ] Preserve intentional visual effects—transparency, masking, palette animation, fog, and compositing—through modern rendering techniques.
- [ ] Support arbitrary window and internal rendering resolutions, independent render scaling, fullscreen/windowed modes, resizing, and high-DPI displays.
- [ ] Support arbitrary aspect ratios with correct projection, configurable field of view, UI anchoring, and safe-area handling. Do not stretch character portraits, text, or FMVs.
- [ ] Audit wider views for missing geometry, incomplete backgrounds, and objects placed outside the original framing. Track required content fixes separately from renderer fixes.
- [ ] Support arbitrary presentation framerates, including custom caps and uncapped rendering, with configurable synchronization and frame pacing.
- [ ] Interpolate camera motion, entity transforms, and suitable animations between simulation updates. Preserve intentional sprite-frame timing rather than assuming every animation needs invented intermediate frames.
- [ ] Verify that changing framerate does **not** change movement speed, jump behavior, encounter calculations, combat timing, minigames, or script execution.
- [ ] **Default optional geometry culling to off.** Expose legacy visibility rejection, distance, frustum/occlusion, and back-face culling separately. Keep clipping, depth testing, authored hidden-object states, and gameplay activation rules independent.
- [ ] Add **independent world-texture and UI filtering controls**. Consider a separate sprite setting so filtering scenery does not necessarily blur characters.
- [ ] Test depth ordering, sprite edges, transparency, texture seams, and rendering consistency across the three backends.

**Exit criterion:** The same recorded gameplay produces equivalent game state at different resolutions, aspect ratios, and rendering framerates.

## Phase 4 — Native keyboard, mouse, controller, and menu interaction

**Goal:** Make this behave like a PC game, not a controller-only game with keyboard emulation.

- [ ] Build an action-based input system with separate contexts for exploration, combat, minigames, game menus, the app menu, and editors.
- [ ] Support SDL3 gamepad input, hot-plugging, remapping, dead zones, analog movement, rumble, and appropriate button prompts.
- [ ] Support **XInput on Windows through SDL3’s controller handling**, without processing the same controller through two competing input paths. SDL3 exposes an XInput backend setting. ([SDL3 XInput setting](https://wiki.libsdl.org/SDL3/SDL_HINT_XINPUT_ENABLED))
- [ ] Implement rebindable keyboard defaults:

| Action | Proposed default |
|---|---|
| Move / navigate | WASD and arrow keys |
| Run | Shift, with hold/toggle setting |
| Jump | Space |
| Confirm | Enter |
| Cancel / back | Backspace |
| Rotate camera left / right | Q / E |
| Toggle game menu | **Tab** |
| Toggle app menu | **Esc** |
| Toggle first-person mode | V |

- [ ] Bind all additional combat and minigame actions; do not stop at the exploration controls.
- [ ] Make **all game menus mouse-browsable**, including inventory, equipment, shops, battle commands, targeting where appropriate, save/load, and configuration.
- [ ] Implement genuine hit-testing, hover feedback, selection, clicking, disabled-item behavior, and tooltips—not simulated directional-button presses.
- [ ] Make **every scrollable menu respond to the scroll wheel**, including nested lists. Support high-resolution wheel and trackpad scrolling with correct focus handling.
- [ ] Keep keyboard/controller selection and mouse hover coordinated without the cursor repeatedly stealing focus.
- [ ] Make Tab toggle the game menu where opening it is permitted. Make Esc toggle the app menu, including during cutscenes; **do not overload Esc as ordinary game-menu Cancel**.
- [ ] Define menu pause behavior and input priority so a click or keypress cannot activate both an overlay and the game beneath it.
- [ ] Handle mouse capture, focus loss, controller disconnection, and restoration cleanly.

**Exit criterion:** The entire game is operable without a controller, and all player-facing menus support appropriate mouse interaction.

## Phase 5 — Lua scripting and the mod framework

**Goal:** Provide the same extension system that the graphical editors will eventually use.

- [ ] Define mod packages with stable IDs, versions, dependencies, compatibility requirements, load order, declared conflicts, and optional configuration schemas.
- [ ] Implement asset replacement and structured data patches for textures, portraits, UI, models, maps, dialogue, audio, FMVs, encounters, and gameplay data.
- [ ] Expose documented Lua APIs for entities, levels, triggers, dialogue, cameras, cutscenes, combat, minigames, audio, and UI.
- [ ] Keep untouched original scripts executable through the recovered script runtime. Allow Lua to extend or replace individual behaviors without requiring an immediate translation of the entire game.
- [ ] Make mod state explicitly serializable and versioned. Represent long-running scripted activities as resumable tasks whose progress can be saved and rewound.
- [ ] Specify deterministic event ordering, random-number access, and time access for gameplay scripts.
- [ ] Restrict script access to files, processes, native modules, and networking; enforce execution and memory budgets and contain script errors.
- [ ] Add a graphical mod manager with installation, enable/disable controls, profiles, dependency resolution, conflict reporting, and safe mode.
- [ ] Support controlled hot reload at safe boundaries. Invalidate or migrate affected snapshots rather than silently restoring incompatible state.
- [ ] Build converters or import assistance for selected existing mod formats. Do not assume an emulator texture pack or ROM patch is automatically a native-engine mod.
- [ ] Ship sample mods that alter a level event, a cutscene, combat behavior, and a minigame through Lua.

**Exit criterion:** Those four sample mods work without recompiling the engine, and their state survives save/restore.

## Phase 6 — Save states, rewind, and fast forward

**Goal:** Implement these as native-engine capabilities rather than emulator-style bolt-ons.

- [ ] Build versioned save-state files containing game state, script/task progress, random-number state, active content identifiers, and mod versions.
- [ ] Keep **ordinary game saves** distinct from **full execution snapshots**. Define migration rules and clear incompatibility messages for both.
- [ ] Add snapshot slots, quick-save/load actions, thumbnails, timestamps, and an undo-load safeguard.
- [ ] Include in-progress battles, field events, minigames, and supported cutscene states—not merely “save anywhere” at otherwise safe locations.
- [ ] Implement configurable fast-forward speeds and hold/toggle controls. Advance simulation faster without confusing this with raising the rendering framerate.
- [ ] Add audio policies for fast forward, such as time-stretched playback or muting above a chosen speed.
- [ ] Implement rewind using periodic snapshots, recorded inputs/events, and deterministic replay. Add a configurable duration or memory budget.
- [ ] Discard the abandoned future when the player resumes from a rewound point.
- [ ] Reconstruct audio playback and FMV playback from their logical positions after restore; do not serialize platform-specific decoder or GPU handles.
- [ ] Handle map changes, battles, cutscene boundaries, and disc transitions. Make any remaining rewind boundaries explicit.
- [ ] Prevent rewind or repeated replay from duplicating external side effects, overwriting ordinary saves unexpectedly, or awarding the same persistent reward twice.

**Exit criterion:** Record → rewind → replay returns to equivalent authoritative state, including with supported Lua mods enabled.

## Phase 7 — Gameplay options, existing-mod features, and modern cutscene skipping

**Goal:** Add the preferred defaults without making unrelated rebalance choices mandatory.

### Initial mod-derived options

Use these projects as the initial feature-research shortlist, then prioritize the broader inventory using the evidence collected in Phase 0.

| Reference | Native options to investigate |
|---|---|
| **Perfect Works Build — quality of life** | Encounter-rate controls, separate experience/money multipliers, and faster text. Its documented options include reduced encounters and 1.5×/2× reward multipliers. ([Extra QOL Features](https://github.com/PWBuild-Team/Perfect_Works_Build/wiki/Extra-QOL-Features)) |
| **Perfect Works Build — translation and gameplay changes** | Optional revised script, selected bug fixes, and independently selectable battle/item/character rebalance packages. ([Perfect Works Build](https://github.com/NoharOSP/Perfect_Works_Build)) |
| **Perfect Works FMV Undub** | Optional Japanese FMV audio and subtitle choices, separate from visual enhancement. ([FMV Undub](https://github.com/PWBuild-Team/Perfect_Works_Build/wiki/FMV-Undub)) |
| **Perfect ART Works** | Optional improved portrait and UI packs, with independently selectable visual variants. ([Perfect ART Works](https://scientia.godsibb.net/t/xenogears-perfect-art-works-paw-retroarch-texture-pack/1158)) |
| **Existing HD texture packs** | Importable texture replacements with category-level enable/disable controls and original-asset fallback. ([Xenogears HD texture pack](https://sites.google.com/view/vierockretrohd/ps1/xenogears)) |

- [ ] Turn the selected features into native settings or mod packages rather than requiring users to patch their source images.
- [ ] Separate correctness fixes, convenience features, translation changes, visual replacements, and balance changes.
- [ ] **Identify platforming-heavy areas by exact map/room IDs and disable random battles there by default.** Preserve bosses, required fights, and other scripted encounters.
- [ ] Add global and per-area encounter overrides, with an explanation of why an area is exempt.
- [ ] Prevent encounter preparation from consuming jump input or interrupting traversal unsafely. Perfect Works’ documentation specifically identifies swallowed jump inputs during encounter loading as a platforming problem. ([Extra QOL Features](https://github.com/PWBuild-Team/Perfect_Works_Build/wiki/Extra-QOL-Features))
- [ ] Offer reward compensation separately rather than silently changing experience or money gains when encounters are reduced.
- [ ] Implement faster/instant text by separating text reveal from cutscene timing and script completion. Regression-test rapidly advanced and automatically closing dialogue; existing fast-text changes document timing and crash-sensitive scenes. ([Extra QOL Features](https://github.com/PWBuild-Team/Perfect_Works_Build/wiki/Extra-QOL-Features))
- [ ] Add modern cutscene controls: pause, a visible skip action, and optional hold-to-skip protection.
- [ ] Implement **semantic cutscene skipping**: execute required story changes and transitions while bypassing presentation. Do not merely jump the instruction pointer to an apparent ending.
- [ ] Stop skipping at meaningful choices, mandatory gameplay, or combat boundaries. Apply party changes, rewards, flags, and transitions exactly once.
- [ ] Give difficult original scenes explicit completion handlers, and expose equivalent skip/completion hooks to Lua.
- [ ] Compare watched-versus-skipped outcomes for story state, inventory, party, destination, camera ownership, and returned player control.

**Exit criterion:** Platforming-area defaults work, optional changes remain independent, and skipping cannot break progression.

## Phase 8 — First-person mode and improved FMVs

**Goal:** Add the two presentation features requiring substantial content-specific validation.

- [ ] Implement a playable first-person camera with mouse look, controller look, camera-relative movement, adjustable field of view, sensitivity, and inversion.
- [ ] Preserve normal running, jumping, collision, interaction, and traversal rather than making this only a detached inspection camera.
- [ ] Define behavior for on-foot exploration, Gear exploration, battles, and minigames. Make camera handoffs explicit where a mode requires authored framing.
- [ ] Hide or adapt the controlled character’s representation, prevent near-plane problems, and handle sprite orientation and interaction targeting.
- [ ] Allow scripted scenes to take camera control and restore the player’s selected camera mode afterward.
- [ ] Audit first-person and wide-angle views for missing surfaces and scenery. Repair these through identifiable content overrides; disabling culling alone is not a content-repair strategy.
- [ ] Implement high-quality playback of the original FMVs, preserving their proportions, timing, and audio synchronization.
- [ ] Add an enhanced FMV pipeline with deblocking/denoising and carefully reviewed upscaling, using cached results or replacement video assets.
- [ ] Provide an explicit **Original / Enhanced** FMV setting, with per-video fallback.
- [ ] Keep sharpening, scaling, and any frame interpolation separate. Do not require invented intermediate video frames to support a high-framerate game renderer.
- [ ] Validate enhanced videos for faces, linework, text, motion artifacts, and transitions back into gameplay.

**Exit criterion:** First-person exploration is genuinely playable, and every enhanced FMV has a working original-quality alternative.

## Phase 9 — Graphical level and cutscene editors

**Goal:** Make content authoring use the real runtime rather than a disconnected approximation.

- [ ] Build a shared editor shell with project management, asset browsing, inspectors, console output, undo/redo, autosave, and validation.
- [ ] Reuse the game’s renderer, asset loaders, simulation, and Lua APIs for editor previews.
- [ ] Build the level editor: geometry editing/import/export, object placement, collision, spawn points, connections, trigger volumes, encounter zones, and camera regions.
- [ ] Add views for otherwise invisible information: collisions, event volumes, navigation constraints, visibility rules, and missing-geometry problems.
- [ ] Build the cutscene editor with a timeline and branching event graph covering actors, movement, cameras, dialogue, audio, FMVs, state changes, and battle transitions.
- [ ] Import original event scripts into inspectable representations while retaining unconverted constructs losslessly.
- [ ] Integrate Lua editing with diagnostics, API completion, breakpoints, variable inspection, and execution stepping.
- [ ] Define how visual graphs and timelines coexist with handwritten Lua. Preserve custom Lua nodes rather than destructively regenerating their contents.
- [ ] Use snapshots and deterministic replay for timeline scrubbing and previews; dragging the timeline must not permanently duplicate story changes.
- [ ] Add graphical authoring of cutscene skip points, completion handlers, and mandatory interaction boundaries.
- [ ] Store projects in version-control-friendly formats and export changes as mod packages without modifying the original asset store.

**Exit criterion:** Create or modify a level and a branching cutscene, export them, and play them in a normal game build.

## Phase 10 — Graphical battle-system and minigame modding

**Goal:** Support meaningful redesigns, not just edits to numerical tables.

- [ ] Build graphical editors for characters, Gears, enemies, items, equipment, abilities, status effects, progression, encounters, and rewards.
- [ ] Expose combat rules for action availability, action costs, turn scheduling, targeting, damage, defense, combos, Deathblows, Gear mechanics, victory, and defeat.
- [ ] Support replacing battle-rule modules through Lua—not merely attaching callbacks to an otherwise immutable battle system.
- [ ] Add battle animation, effect, camera, and interface editing with immediate preview.
- [ ] Build minigame-specific editors for arenas/layouts, entities, input actions, rules, scoring, timing, opponent behavior, and win/loss conditions.
- [ ] Allow Lua to change a minigame’s runtime behavior and state machine rather than limiting editing to assets and difficulty values.
- [ ] Provide isolated battle/minigame test sessions with configurable starting states, deterministic seeds, step-through execution, and automated test runs.
- [ ] Validate references, required resources, script errors, save compatibility, and package dependencies before export.
- [ ] Publish example projects demonstrating a substantially different battle ruleset and a materially altered minigame.

**Exit criterion:** A mod author can make those changes entirely through the graphical tools and Lua, without rebuilding C++.

## Phase 11 — Platform hardening, packaging, and release

**Goal:** Make the whole feature set dependable on the promised platforms.

- [ ] Validate **Arch Linux + Vulkan first**, including Wayland/Hyprland, X11, fractional scaling, mixed-DPI monitors, ultrawide displays, fullscreen changes, focus loss, and controller reconnection.
- [ ] Validate Windows + Direct3D 12, including XInput and SDL3 controller behavior.
- [ ] Validate macOS + Metal on the explicitly supported processor and OS targets.
- [ ] Test shader compilation, resource management, rendering output, and state equivalence across backends.
- [ ] Run full-playthrough and optional-content coverage with the default settings, original-gameplay settings, and representative mod combinations.
- [ ] Stress-test high framerates, fast forward, repeated rewind/load, cutscene skipping, device changes, malformed mods, and interrupted writes.
- [ ] Package an Arch `PKGBUILD`, Windows releases, and a macOS application bundle.
- [ ] Add first-run asset import, configuration migration, controller setup, and understandable failure reporting.
- [ ] Verify clean-install defaults: **culling off; platforming-area random battles off; Tab for game menu; Esc for app menu; independent texture/UI filtering; original FMVs selectable**.
- [ ] Publish player documentation, developer setup, reverse-engineering notes, the Lua API, editor tutorials, and mod compatibility/versioning guidance.
- [ ] Complete the requirement-to-test matrix from Phase 0. Do not equate “the story is completable” with “the requested feature set is finished.”

## Release checkpoints

| Checkpoint | Required result |
|---|---|
| **Engineering preview** | Phase 2A: a reproducible native gameplay slice on Arch |
| **Playable PC alpha** | Phase 2B plus modern rendering and complete PC controls |
| **Feature-complete runtime beta** | Lua mods, rewind/save states, fast forward, gameplay options, safe skipping, first-person mode, and enhanced FMVs |
| **Creator-toolkit beta** | Level, cutscene, battle-system, and minigame editing works through the shared runtime |
| **1.0** | All requested features validated, packaged, and documented across Linux/Vulkan, Windows/DX12, and macOS/Metal |
