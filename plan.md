# Xenogears Ex Machina — Independent Decompilation and Native PC Port Plan

**Original-game analysis → independent decompilation → agent-ready native runtime and content pipeline → TypeScript world/geometry authoring → modern PC features and Lua mods → shared graphical editors.**

Xenogears Ex Machina is a new project. Independently decompile the original Xenogears game and implement a new native runtime and companion editor suite, with Arch Linux as the leading platform. Do not start from, fork, extend, or adopt another Xenogears decompilation, recompilation, fan port, or game engine implementation.

**Foundational requirement:** The native application must be fully playable by agents through a first-class headless interface with direct engine input, full game introspection, complete debugging and go-anywhere capabilities, unlocked simulation speed, and an optional image stream for human spectators. This is part of the native runtime's initial architecture and first playable milestone, not a later automation layer, emulator wrapper, or editor-only feature.

**Foundational authoring requirement:** Agents must also be able to create and revise playable mods from prompts through a discover → specify → author → build → play → diagnose → repair → package loop. TypeScript is the primary source language for authored worlds, procedural geometry, reusable components, and declarative event definitions, with optional TSX composition. Authors must be able to construct actual towns, dungeons, environments, and custom objects without importing any geometry, while optionally mixing in external models and locally imported original assets. Graphical editors and agents use the same source contracts, compiler, native loaders, and verification services. A prompt box or GUI automation alone does not satisfy this requirement.

## Project boundaries

- **Independent game-specific work:** Recover behavior, formats, algorithms, and scripts from the original game binaries, disc data, and observed execution. Develop this project's own analysis records, decompiled source, importers, runtime, renderer, and editors. Do not use other Xenogears projects' source code or reverse-engineering results as the implementation foundation.
- **Original-game evidence:** Use the original game as the behavioral reference. General-purpose disassemblers, decompilers, debuggers, and an external emulator may assist inspection and testing; an emulator is neither the shipping runtime nor a source of borrowed game-specific implementation.
- **General-purpose dependencies:** SDL3, Lua, compiler/build tools, shader tools, codecs, and other infrastructure libraries are permitted. Starting a new game-specific implementation does not require reinventing general-purpose infrastructure. Record dependency choices and licenses explicitly.
- **Mod-inspired options, not a mod-based engine:** Research popular existing mods only to identify desired player-facing options. Implement those capabilities independently. No existing mod, patched disc, translation package, or texture pack is required to build or play the base port; separately installed content packs remain optional.
- **Native execution:** Ship platform-native binaries, not a PS1 executable running inside a CPU emulator. Interpret the original game's event bytecode through this project's own recovered script runtime where appropriate.
- **One runtime for humans and agents:** Interactive play, headless agent play, automated tests, and editor previews must use the same simulation, state, rules, loaders, and command handlers. The human UI and agent API are clients of that runtime; headless execution must not substitute a simplified game model or require simulated OS input.
- **Presentation-independent execution:** Logical simulation must run without a window, display server, GPU, physical input device, or audio output device. Optional offscreen rendering and spectator streaming may use a renderer, but neither rendering nor a spectator may govern the simulation clock or be required for agent control.
- **Data separation:** Import original game data from user-supplied disc images into a local asset store. Keep disc images, original executables, extracted copyrighted assets, and personal saves out of distributed source and build artifacts.
- **Behavior versus presentation:** Preserve gameplay-relevant behavior and intentional effects. Modern rendering remains the default; preserve PS1 rendering quirks as independently selectable options and a PS1-style preset in the native renderer. Binary-identical recompilation is not a release gate; independent decompilation, documented understanding, and behavioral validation are.
- **Wide-derived headphone surround:** Provide an optional mode that decodes/reconstructs the original game's Wide spatial signal and binaurally renders its verified intended sound field to exactly two headphone channels. Recover the actual Wide behavior before selecting a decoder; do not substitute generic stereo widening or assume a particular surround matrix from the mode name.
- **Authoring is not runtime execution:** TypeScript runs in a supervised build-time Node.js process to manufacture meshes, world definitions, and event instructions. It is not translated into engine C++, evaluated every rendered frame, or automatically shipped as executable gameplay JavaScript. Native C++ systems and explicit event graphs execute ordinary behavior; Lua remains the custom gameplay extension language. Playing a prebuilt package must not require Node.js, a browser, Qt/QML, or an authoring tool.
- **One source-oriented authoring pipeline:** TypeScript source, explicit parameter data, Lua modules, and referenced assets are authoritative. The intermediate representation, generated GLBs, and compiled packages are derived outputs, not competing editable sources. Preserve reusable components, stable IDs, and handwritten code through graphical and agent edits; do not promise arbitrary source-code round-tripping.
- **Geometry tools are not a replacement game engine:** Use general-purpose geometry/import/export libraries without adopting another game's implementation or making Three.js, Blender, Qt Quick 3D, or Godot the authoritative runtime. Blender or other external tools may supply optional assets. QML is not the selected world/geometry language; any later editor UI toolkit remains a client of the shared services.

Existing completion marks are retained. New requirements remain unchecked until demonstrated; previously completed work does not automatically satisfy an expanded requirement. A phase is complete only when its current exit criterion has been demonstrated.

## Foundational native-agent contract

| Capability | Required behavior |
|---|---|
| Same complete game | Agents can play every implemented field, battle, menu, cutscene interaction, world-map activity, and minigame through the actual native runtime. No separate mock simulation or screenshot-driven UI automation is required. |
| Direct input | Accept typed gameplay actions and commands at the engine boundary, including analog/directional state, held/released actions, menu selections, dialogue choices, and battle commands. Do not inject keyboard/mouse/controller events into the OS, synthesize SDL events, or require virtual devices. |
| Full introspection | Expose structured, consistent game-state snapshots, targeted queries, deltas, events, and diagnostics, including non-visible state. Do not limit agents to screenshots, textual summaries, or public-facing player information. |
| Complete debug and go-anywhere access | Provide pause/step, breakpoints, watchpoints, traces, state inspection/editing, snapshots, deterministic replay, arbitrary valid scenario setup, and native parity with every supported emulator-test capability. Extend typed setup to the complete native game rather than inheriting current emulator-adapter limitations. |
| Agent-controlled time | Support exact stepping, bounded run-until conditions, real-time play, fixed acceleration, and fully unthrottled execution as fast as the simulation can run. Removing pacing must not change tick semantics or skip required gameplay work. |
| Optional human observation | Offer on-demand images and an attachable live image stream from the native renderer. Viewing is optional and read-only by default; attaching, disconnecting, slowing, or dropping a spectator must not change game state or force real-time execution. |
| Stable automation interface | Provide a documented, versioned, discoverable machine-readable API with explicit capabilities, IDs, tick semantics, errors, cancellation, and reproducible run artifacts. It must be usable without a GUI or a specific agent vendor. |
| Architectural gate | No subsystem is considered complete until its state, valid actions, setup, time progression, and debugging are available headlessly. The first native playable slice must demonstrate agent play and optional human observation before broad game-completion work advances. |

## Foundational agent-authoring contract

| Capability | Required behavior |
|---|---|
| Actual geometry creation | Author shapes, surfaces, topology, and complete playable environments from code, not merely place external models or select prepared prefabs. Support reusable parameterized components and an arbitrary-mesh escape hatch alongside imported models. |
| Discoverable content SDK | Supply types, schemas, exact asset/entity IDs, searchable examples, supported operations, and explicit unsupported/opaque states. An agent must not invent a game capability because it found a resource name. |
| Native build bridge | Type-check/transpile/execute authoring source, normalize geometry and world definitions, validate, export, and load the resulting package into the actual C++ runtime. Geometry libraries and glTF export do not supply this game-specific bridge automatically. |
| Shared authoring access | Expose semantic, revision-aware, transactional edits and headless build/test commands. Graphical panels are clients; no content operation may be available only through a GUI callback. |
| Inspectable behavior | Compile declarative events to native instructions with explicit serializable progress. Custom Lua state, tasks, actions, and errors participate in the same introspection, snapshots, and scheduler as the base game. |
| Verifiable iteration | Test immutable built content through legal runtime actions, repair failures using source-aware diagnostics, and repeat. Debug setup cannot stand in for gameplay, and changing protected acceptance criteria cannot turn a failed run into a pass. |
| Human-editable delivery | Produce source, targeted diffs, an installable package, exact build/test provenance, and optional native captures. Follow-up prompts patch the existing project without discarding unrelated human work. |
| Early architecture gate | Prove a small source-to-playable authoring bridge during Phases 2A–3, before broad content expansion or the full graphical editor suite. Expand the same foundation through Phases 7 and 11–13. |

## Phase 0 — Establish scope, evidence standards, and the repository

**Goal:** Define a new project's scope and workflow without selecting another project's codebase.

- [x] Create a requirement-to-test matrix covering every requested feature, platform, default setting, and editor capability.
- [x] Select and hash the reference source images. Initially target both US discs and identify their supported revisions explicitly; record region/revision support rather than guessing from filenames.
- [x] Inventory the original game's fields, world map, battles, menus, event sequences, audio, FMVs, saves, optional activities, and minigames.
- [x] Establish repository areas for runtime source, independent analysis, extraction tools, editors, tests, and packaging. Ignore original disc data and generated local asset stores.
- [x] Record research findings with disc/revision, file or sector offset, executable/overlay identity, address where applicable, observed behavior, confidence, and a reproducible validation procedure.
- [x] Track each subsystem through identified, analyzed, decompiled, natively implemented, and behaviorally validated states. Distinguish unknown behavior from intentionally changed behavior.
- [x] Establish an Arch Linux development environment and a C++/CMake/Ninja build with debug configurations, sanitizers, formatting, and automated tests.
- [x] Define contributor provenance and dependency-review rules consistent with the independent implementation boundary.
- [x] Define representative test scenarios and progression checkpoints across both discs before implementation begins.
- [x] Use an original-game go-anywhere testing system, informed by the general testing design in `../slapstick-english-translation`: checkpoint-free cold boot through original loaders, declarative setup through verified scene adapters, explicit readiness conditions, bounded timeouts, ordered inputs, and original-source/tool/write provenance. Track unrecovered battle, position, and progression adapters as an explicit backlog and reject unsupported setup.
- [x] Extend the requirements matrix with agent-native execution, introspection, direct commands, full debug/go-anywhere access, optional image streaming, unlocked simulation speed, optional PS1 visuals, and Wide headphone surround. Preserve existing completion evidence without marking these additions complete prematurely.
- [x] Specify the native agent protocol, state-query schema, command model, simulation-time units, execution modes, and debug permission boundary before implementing gameplay systems. Define headless operation and first-slice acceptance tests as architecture gates.
- [x] Inventory emulator-test behavior from [the scenario contract](analysis/scenarios/README.md), [its schema](analysis/scenarios/schema.json), and [the reference runner](tools/reference/scenario.py). Maintain a versioned parity table separating verified emulator capabilities, native equivalents, and native extensions; do not confuse an emulator backlog item with an already-supported feature.
- [x] Extend the requirement-to-test matrix with the agent-authoring contract, selected TypeScript geometry stack, source/build/runtime boundaries, untrusted-build policy, and source-to-playable acceptance gates. Review and pin authoring dependencies separately from shipping runtime dependencies; do not retroactively mark these additions complete.

Verification: [current Phase 0 authoring audit](docs/verification/phase0-authoring.json), with the [earlier native-agent audit](docs/verification/phase0-agent.json) retained. The separate dependency qualification passes; native gameplay, the authoring SDK/bridge, and untrusted-build isolation remain specified and unimplemented.

**Exit criterion:** A reproducible empty-project build, original-game coverage inventory, evidence workflow, updated requirements matrix, original-game testing system, and foundational native-agent and agent-authoring contracts exist.

## Phase 1 — Independently reverse-engineer and decompile the original game

**Goal:** Produce this project's own understanding and source reconstruction directly from the original binaries and data.

Use Ghidra as the main reverse-engineering environment, supplemented by lab313ru/ghidra_psx_ldr for PlayStation-specific loading and analysis. Use m2c selectively when a function benefits from matching-oriented reconstruction. Decompiler output still requires source, type, control-flow and original-execution review.

- [ ] Analyze disc layout, boot metadata, executable loading, archives, sector references, compression, and shared versus disc-specific resources.
- [ ] Map the main executable and dynamically loaded code/data. Track overlay identity alongside addresses so reused memory locations do not become ambiguous symbols.
- [ ] Disassemble the original MIPS code and recover function boundaries, calling conventions, globals, structures, tables, dispatchers, and subsystem relationships.
- [ ] Decompile functions into readable, reviewed source. Resolve inferred types, control flow, pointer arithmetic, signedness, fixed-point arithmetic, and overflow behavior rather than treating automatic pseudocode as finished code.
- [ ] Identify hardware-facing code, BIOS interfaces, GPU command generation, audio interfaces, controller reads, and disc I/O; separate their gameplay intent from the PS1-specific mechanism.
- [ ] Recover game-visible PS1 rendering behaviors needed by the optional fidelity settings, including projected-vertex precision, texture interpolation, color quantization, dithering, rasterization resolution, primitive ordering, and transparency. Capture representative original-game visual references.
- [ ] Trace original Mono, Stereo, and Wide sound paths, including panning/polarity, reverb, master output, and streamed/FMVs' routing. Capture matched unprocessed two-channel signals and establish Wide's actual changes and intended playback model from original-game evidence and primary documentation where available. Distinguish matrix encoding from phase-based stereo expansion before selecting a decoder.
- [ ] Trace original execution to validate uncertain findings. Capture inputs, seeds, state transitions, script instructions, and timing observations for focused scenarios.
- [ ] Recover field, geometry, sprite, animation, collision, camera, texture/palette, UI, dialogue, battle, world-map, audio, and FMV data formats through direct analysis.
- [ ] Recover event-script bytecode, operands, scheduling, waits, branching, concurrent actors, and side effects. Build this project's own disassembler and instruction reference.
- [ ] Recover movement and jump rules, encounter logic, random-number generation, battle formulas, enemy behavior, story-state changes, minigame rules, and save formats.
- [ ] Identify simulation cadences and dependencies on frame counters, asynchronous loads, interrupts, and hardware timing. Document which timings are gameplay rules and which are implementation artifacts.
- [ ] Recover state ownership, action eligibility, menu/dialogue structure, event/control readiness, spawn/transition preconditions, and subsystem initialization needed for native agent queries and typed go-anywhere setup. Map original addresses and symbols to native semantic fields without making raw memory offsets the native API.
- [ ] Create original parser fixtures and behavioral tests for each finding. Generate tests requiring original data locally from the user's imported assets; use synthetic or redistributable fixtures in public CI.
- [ ] Maintain explicit unresolved-symbol, unknown-format, and unimplemented-instruction inventories. Never silently treat unknown behavior as a no-op.

Verification in progress: [Phase 1 evidence and remaining proof](docs/phase1-progress.md).
The verified decoder, event-control, music-loading, collision-arithmetic, field-control, sprite/jump, planar-motion, movement-sweep, animation/replay, composed party-motion, position, actor-default, battle-request, encounter-return data and sprite checkpoint subsets, the reusable C++ event library, and the observed encounter-victory-return route do not yet complete a broad todo or the slice exit. Related Ghidra surveys and shared-type corrections support the [source/state handoff](analysis/formats/field-lifecycle.md); provisional decompilation does not close the remaining requirements.

**Exit criterion:** The code, formats, scripts, and behavior required for the first playable slice are independently documented and validated, including its agent-visible state and scenario-entry semantics, with a tracked backlog for the rest of the game.

**Execution rule:** Continue this analysis throughout development. Do not wait for a complete decompilation of both discs before implementing an understood subsystem.

## Phase 2 — Build the agent-ready native runtime, asset pipeline, and state architecture

**Goal:** Build one real game runtime whose headless agent interface, introspection, debugging, clock control, and optional rendering are foundational services.

### Shared simulation and platform boundaries

- [ ] Separate simulation, original-script execution, rendering, input/UI, audio/media, asset management, persistence, agent/control services, and editor services behind explicit interfaces.
- [ ] Implement original asset extractors and decoders from Phase 1 findings. Add bounded reads, corruption checks, source hashes, versioned caches, and clear unsupported-revision errors.
- [ ] Give imported resources stable IDs independent of PS1 memory addresses. Preserve source provenance and unknown data needed for later investigation.
- [ ] Use SDL3 for interactive windowing, platform integration, and human input. Bring up a Vulkan renderer on Arch first, but keep window/GPU/audio-device initialization optional and absent from the pure headless path.
- [ ] Define the rendering abstraction and shader pipeline for Vulkan on Linux, Direct3D 12 on Windows, and Metal on macOS. Evaluate SDL3 GPU as general-purpose infrastructure, with small backend validation programs rather than borrowing a game renderer.
- [ ] Separate modern rendering from selectable PS1-fidelity operations at the appropriate geometry, rasterization, and compositing stages. Keep visual precision choices outside authoritative simulation state.
- [ ] Separate original sound-mode generation from output processing: preserve a two-channel Wide tap, a Wide-specific decoding/spatial-reconstruction stage, and an HRTF binaural stage. Preserve interchannel phase/timing and original-mode bypass paths; allow a null audio output without stalling logical media/event progression.
- [ ] Replace PS1 memory-layout and hardware assumptions with native resource management, typed state, and explicit services. Do not make the native game depend on original BIOS execution or a PS1 CPU loop.
- [ ] Separate simulation time from render time and wall time. Preserve recovered gameplay arithmetic and subsystem update rules while using high-precision transforms for presentation.
- [ ] Define authoritative state for entities, party, inventory, story flags, RNGs, events, battles, minigames, logical audio positions, and media timelines.
- [ ] Use stable IDs or handles for serializable references. Keep GPU objects, decoder internals, caches, and platform handles rebuildable and outside authoritative state.
- [ ] Define a deterministic scheduling and persistence contract for both original scripts and future Lua tasks. Store explicit task progress; do not assume arbitrary Lua stacks can later be serialized.
- [ ] Make asynchronous asset availability and background jobs unable to silently change simulation outcomes or event ordering. Gate required data explicitly rather than letting host timing decide when gameplay changes happen.
- [ ] Define shared native content/entity/collision/event interfaces for both original-data import and Phase 2A authored packages. Keep origin-specific decoding separate from common simulation services; generated content must not bypass recovered movement rules or require a second gameplay model.

### Direct agent control and full introspection

- [ ] Provide a documented versioned machine-readable API and a command-line client over local process I/O or IPC, with structured requests/responses, request IDs, capability/schema discovery, stable resource IDs, and machine-readable errors. Do not require a window, terminal UI, network account, or particular agent product.
- [ ] Expose typed engine actions for direction/analog axes, run, jump, confirm, cancel, camera rotation, menu toggles, menu-item selection, scrolling, dialogue choices, battle commands/targets, and minigame controls as each subsystem is implemented. Include direct pointer actions for UI tests without requiring OS mouse injection.
- [ ] Dispatch human input and agent commands through the same gameplay command handlers, eligibility checks, and state transitions. Agent commands must not simulate keyboard/mouse/controller events, enqueue synthetic SDL input, or rely on a virtual controller, screen coordinates, OCR, or focus on a desktop window.
- [ ] Define action press/hold/release semantics, simultaneous actions, analog ranges, tick scheduling, ordered batches, and acknowledgments with the exact applied simulation tick. Reject invalid or unavailable commands with reasons and no unintended partial mutation.
- [ ] Provide semantic menu/dialogue/battle queries with stable selection and target IDs, available and disabled actions, reasons for disabled choices, focus, scrolling state, and current control ownership. Direct semantic selection must obey the same game rules as human selection, not bypass locks or grant resources.
- [ ] Expose complete structured snapshots and targeted queries for all simulation state: active/inactive entities and components, transforms, collision/terrain, camera, maps/transitions, flags, variables, party, stats, equipment, inventory, encounters, battle state/AI, minigames, original script stacks/instruction pointers/waits, Lua tasks, RNG state, pending events/timers, and logical audio/media state. Include internal and offscreen state, not only what the player can see.
- [ ] Expose subsystem diagnostics, resource/loading state, rendering/audio status, and native symbol/source correlations separately from authoritative state. Establish schema coverage so new state cannot silently become invisible to agents.
- [ ] Provide consistent tick-tagged snapshots, filtered queries, state diffs, subscriptions, event streams, and state hashes. Large results need pagination/chunking with snapshot consistency and explicit truncation; do not require dumping the whole game after every action.
- [ ] Support an atomic observe → submit actions → advance → observe cycle and bounded run-until operations so agents can act on an identified state rather than race a continuously advancing process. Keep the control service responsive while paused or running, with explicit cancellation and input ownership/handoff rules.
- [ ] Separate normal gameplay actions from opt-in debug mutations. Log debug writes, setup shortcuts, and loaded snapshots; do not count a debug-assisted outcome as an unmodified gameplay-completion test.
- [ ] Keep privileged control local by default, with read-only spectators, explicit trusted debug access, and authenticated opt-in remote access. Resource bounds and command cancellation must remain available even in unthrottled runs.

### Clock control, snapshots, and complete debugging

- [ ] Implement real-time, fixed-multiplier, paused/stepped, and fully unthrottled execution modes in the core scheduler. Unthrottled mode removes artificial sleeps and wall-clock frame caps; it is not just high-FPS rendering or a fixed 2x/4x fast-forward setting.
- [ ] Let agents advance an exact number of simulation ticks or run until a typed condition, event, decision point, breakpoint, or simulation-time budget. Report the stop reason, elapsed simulation time, tick count, wall time, and throughput; support interruption without corrupting state.
- [ ] Preserve fixed/recovered tick semantics and execute every required gameplay update at maximum speed. Do not enlarge physics timesteps, skip collision/event work, or automatically skip cutscenes to manufacture throughput. Advancing logical media time without output must preserve the same script completion and synchronization events.
- [ ] Make simulation pacing independent from VSync, GPU presentation, audio-device clocks, video playback, image encoders, and spectator consumption. Use null/rebuildable presentation paths when testing without rendering/audio, and measure actual throughput without promising a hardware-independent acceleration factor.
- [ ] Provide exact portable debug snapshots, restore, reset, seeded replay, and branching test sessions before the first playable slice. Phase 8 adds player-facing save-state/rewind features on this foundation rather than postponing agent checkpoint support.
- [ ] Implement pause/resume, simulation single-step, original-script and Lua task/instruction stepping, conditional breakpoints, state watchpoints, event/transition breakpoints, variable inspection/editing, call/task stacks, and trace capture. Expose debug symbols and native-debugger integration for native code stepping, memory inspection, assertions, crashes, and hangs.
- [ ] Provide guarded typed state mutation with validation, preconditions, rollback on failure, before/after diffs, and provenance. Make raw diagnostic access explicitly unsafe/debug-only rather than the routine agent gameplay interface.
- [ ] Capture structured logs, coverage, event histories, performance counters/profiles, assertion/crash reports, and relevant stack/state dumps. Produce deterministic replay inputs and a bounded recent-history buffer for failures.

### Native go-anywhere scenario system

- [ ] Build native parity with every supported feature in the existing emulator scenario apparatus: clean reset/cold start without prior saves, source/profile and catalog checks, numeric/named scene selection, ordered input/wait/capture steps, explicit readiness predicates, bounded execution, guarded setup, and success/failure artifacts with exact provenance.
- [ ] Use the existing emulator contract as the parity baseline, not as the native implementation. Its documented verified paths include original Kernel MENU and numeric field entry; position/facing, typed party/progression setup, battle launch, and other entries are an explicit adapter backlog. Implement and test native equivalents/extensions rather than claiming those are already verified emulator features.
- [ ] Implement typed native scenario entry for any supported field/map, world-map location, battle/formation, minigame, cutscene/event, and menu state as its subsystem comes online, across both discs. Allow explicit player/party position and facing, collision-safe spawn, party/stats/equipment/inventory, story flags/progression, encounter state, camera, RNG seed, and mode/configuration setup.
- [ ] Load scenarios through the same native loaders, initialization services, and state validators as ordinary play. Reconstruct required dependent state; reject invalid combinations and unknown entry types explicitly instead of silently falling back, leaving partial state, or requiring a conveniently preexisting save.
- [ ] Distinguish asset availability, map initialization, script completion, actionable dialogue/menu, battle readiness, and actual player-control readiness. Native scenarios must use recovered logical ticks and semantic conditions; emulator frontend-frame counts are not automatically interchangeable with native ticks.
- [ ] Provide a discoverable location/event/formation catalog and declarative scenario schema with conditions, setup, direct actions, waits, assertions, snapshots, and optional images. Extend the emulator/native parity table as either apparatus grows.
- [ ] Record source/asset/build/mod identities, normalized setup, seeds, ordered commands, applied ticks, guarded mutations, expected/observed readiness, state hashes, captures, and completion/failure status. Label exploratory/unsafe setup separately from validated gameplay evidence.
- [ ] Enforce simulation-work budgets plus a separate wall-clock watchdog for hangs. Do not inherit an emulator-specific frame ceiling as the native test limit or require real-time waiting to detect readiness.

### Optional human image stream

- [ ] Offer image-free headless execution by default, on-demand screenshots, and an optional live image stream using offscreen rendering of the same authoritative session. Pure logical headless operation must need no GPU; document any renderer requirements when image output is enabled.
- [ ] Supply a lightweight spectator viewer or documented stream client that can attach/detach without restarting the session. A spectator is read-only unless control is explicitly handed over; the agent must not depend on a viewer being connected.
- [ ] Tag each image with session ID, simulation tick/time, render profile, and output dimensions. Allow configurable resolution and capture cadence, plus a paused-state image without advancing simulation.
- [ ] Decouple rendering/encoding/stream transport from simulation with bounded queues and an explicit sample/drop policy. Slow viewers must not back-pressure game progress; report dropped/skipped frames and label accelerated timelines honestly. Exact every-tick captures may be a separate opt-in offline mode with a documented performance cost.
- [ ] Validate image-free, offscreen, and streamed runs against the same action trace/state hashes. Human observation may cost computation but must not change gameplay outcomes, require real-time pacing, or become a prerequisite for full introspection.

- [ ] Start Windows and macOS compile/headless protocol checks now while Arch remains the primary execution and integration platform.

**Exit criterion:** The new runtime independently imports original assets and supports direct agent commands, full structured state access, exact stepping and unthrottled execution, debug snapshots/breakpoints, and valid scenario launch for its implemented systems. The same session runs image-free without a display/GPU/audio device or with an optional offscreen spectator stream. Identical actions produce equivalent authoritative state across those modes. These capabilities must exist before Phase 3 is declared complete.

## Phase 2A — Agentic TypeScript world authoring and the native content bridge

**Goal:** Make an ordinary coding agent able to author actual geometry and playable content, compile it into native assets, test it in the real game, and repair it without changing engine C++ or automating a graphical editor.

**Ordering:** Start alongside Phase 2 as understood native services become available. The early bridge gate below is required for Phase 3; the broader authoring SDK, geometry vocabulary, and agent workflow continue through Phase 7, with graphical clients in Phases 11–12. Do not wait for both discs or the complete editor suite to prove source-to-playable content. Authored test scenes do not replace original-game behavioral evidence.

### Selected stack and responsibility boundaries

| Layer | Selected technology | Responsibility |
|---|---|---|
| Authored source | TypeScript; optional TSX after the function-based SDK works | Reusable world components, geometry recipes, parameters, procedural algorithms, and declarative event graphs. A custom JSX factory returns definitions, not React UI elements. |
| Type checking and transpilation | `tsc --noEmit` and esbuild | Check types separately from transpiling/bundling. Neither step alone generates playable assets. |
| Build execution | A pinned supported Node.js toolchain in isolated worker processes | Execute the compiled authoring program with explicit inputs, seeds, budgets, and output paths; never require it during ordinary play of a prebuilt package. |
| Solid geometry | `manifold-3d` WebAssembly bindings | Primitive solids, union/difference/intersection, profile extrusion, and revolution; normalize results into project-owned mesh data. |
| Surface and unrestricted geometry | `three` geometry utilities plus project-authored generators | Extrusion/path utilities, terrain/surface generation, and arbitrary vertex/index buffers. Do not use a Three.js scene or renderer as the authoritative world/runtime. |
| Render-asset packaging | `@gltf-transform/core`, with additional modules only as required | Import/process/export glTF/GLB render assets through an explicit supported-material/extension subset. |
| Parameters and schemas | Zod and exported JSON Schema | Validate inputs and expose machine-readable parameters; define a serializable schema subset and test native-validator parity. |
| Native asset import | `cgltf` behind project-owned C++ adapters | Parse supported glTF/GLB and resolve assets; the project still implements decoding, GPU upload, materials, entities, collision, and gameplay. |
| Native execution | Existing C++/CMake runtime, native renderer, event interpreter, and Lua | Load and run compiled content with the same headless/interactive semantics as the base game. |
| Search and optimization | SQLite/FTS5 catalog; `meshoptimizer` when justified | Rebuildable content/metadata lookup and measured post-generation mesh optimization, not prerequisites for the first bridge. |

Dependency names are selected implementation targets, not claims that these integrations already exist. Pin compatible versions, review licenses, and test supported platforms before adoption. A Blender/Python asset path may remain optional; an embedded gameplay JavaScript runtime is not part of this plan.

### Source → build → package → runtime

```text
TypeScript / optional TSX + parameters + assets + Lua modules
    → type check → transpile/bundle → isolated Node.js execution
    → typed world/geometry intermediate representation
    → Manifold / surface generators / imported-model adapters
    → normalized meshes + collision + entities + event definitions
    → validation and immutable, versioned content package
    → native C++ loader
    → simulation/collision/events + optional GPU resources
    → actual-runtime tests, diagnostics, source revision, rebuild
```

- [ ] Define a typed authoring SDK distinguishing solid geometry, open surfaces, render meshes, collision, asset references, entities, and runtime event definitions. Begin with ordinary functions; add a minimal custom TSX factory after equivalent function-based builds pass. Avoid UI lifecycle semantics and implicit per-frame generation.
- [ ] Define a versioned intermediate representation preserving namespaced content IDs, entity instances, reusable mesh references, transforms, materials, collision, spawn/portal/trigger definitions, NPCs, encounters, and event graphs. Keep object identity independent of file paths, display names, triangle indices, and PS1 addresses; do not flatten a whole town into an anonymous mesh.
- [ ] Use `.ts`/`.tsx`, explicit parameter files, Lua, and source assets as authoritative inputs. Generated JSON manifests are build products, not a second authoring source; graphical clients may edit exposed parameter data or propose source patches, but must not silently overwrite arbitrary code.
- [ ] Implement type-checking, transpilation, isolated execution, dependency/reference resolution, validation, and export as distinct build stages with structured errors. Reject unsupported callbacks, nonserializable state, invalid references, and nonfinite geometry rather than emitting a superficially successful package.
- [ ] Normalize geometry adapters into project-owned mesh data containing positions, indices, normals, UVs, material groups, bounds, and source/object attribution. Validate winding, degeneracy, topology requirements, resource budgets, and attribute consistency before export; release geometry-kernel allocations reliably.
- [ ] Start with GLBs for render assets plus versioned JSON manifests for the world, collision, events, and package dependencies. Include source maps, asset hashes, supported engine/schema versions, and required local base-asset references. Defer custom binary packing until profiling justifies it; glTF is not the gameplay or collision contract.
- [ ] Implement the C++ bridge: parse/validate the package, decode required assets, stage entities/components, build collision structures, resolve references, initialize event tasks, and atomically publish a valid field. Unsupported materials/extensions, missing resources, and corrupt data must fail explicitly without leaving a partially loaded scene.
- [ ] Load the same package without GPU allocation in logical headless mode and with native GPU meshes/materials when rendering is enabled. Preserve origin-specific decoding but share entity, movement, collision, interaction, and event services with imported original content.
- [ ] Keep the original imported store immutable. Build separate mod layers with explicit dependencies and overrides; resolve original assets locally by verified IDs/hashes instead of embedding extracted game assets in distributable source or packages.
- [ ] Make builds reproducible from source, lockfiles, explicit seeds, generator/compiler versions, imported-asset hashes, and relevant settings. Record output content hashes and test the exact immutable artifact; a seed alone is not a cross-platform determinism guarantee.
- [ ] Cache generated meshes by complete dependency/parameter identity, preserve instancing, and rebuild only affected components. Generate static geometry during builds, not rendered frames; opening a door changes native state/transforms rather than rerunning a boolean operation. Runtime procedural generation requires a separately specified generator and persistence contract, not just a seed in a manifest.

### Actual geometry, environments, and custom objects

- [ ] Implement an initial geometry vocabulary of box/cylinder primitives, union/difference/intersection, extrusion, revolution, arbitrary meshes, and imported models. Prove actual vertex/triangle generation; placing prepared prefabs alone does not satisfy the feature.
- [ ] Separate manifold-solid operations from open-surface/custom-mesh and imported-model paths. Validate boolean operands, retain useful failure details, and do not silently force arbitrary external meshes through a solid-modeling kernel.
- [ ] Add parameterized rooms, corridors, houses, stairs, bridges, doors, machinery, and user-authored custom components built from those operations. Expose lower-level algorithms and mesh arrays so the component catalog does not limit what an agent can invent.
- [ ] Expand surface generators incrementally with path sweeps, terrain/heightfields, and specialized surfaces as demonstrated use cases require. Do not present unimplemented lofting, deformation, rigging, or modeling operations as available capabilities.
- [ ] Establish one authoring coordinate/unit convention and documented conversions to imported assets, geometry backends, renderer coordinates, and recovered collision arithmetic. Make conversions explicit and test scale, handedness, normals, winding, and precision at boundaries.
- [ ] Specify materials, texture density/projection, UV generation, hard/smooth edges, vertex colors, and semantic surface regions. Preserve intended materials on boolean cut faces and through parameter changes; define the renderer-supported material subset and report unsupported features rather than assuming export implies shader support.
- [ ] Provide explicit collision modes: supported static surface/triangle data, composed primitives, separate simplified geometry, or none. Compile into this runtime's actual movement representation and preserve openings; neither one bounding box nor a convex hull around an arch is an acceptable doorway collider.
- [ ] Support named anchors, doorway connections, placement/facing constraints, collision-aware clearance, room adjacency, and repeatable prop distribution. Persist solved transforms and geometry; do not require the runtime to reinterpret a prompt or silently teleport the player to satisfy traversal tests.
- [ ] Preserve component/instance/operation IDs and source spans in generated geometry and diagnostics. Report invalid dimensions, failing boolean operands, blocked interactions, and connectivity failures against editable source parameters; stable semantic region IDs must not depend only on triangle numbers.
- [ ] Distinguish build validity, native traversal/collision validity, and presentation quality. Use actual-runtime movement tests plus optional native captures/overlays; a successful GLB export or external preview is not proof of a playable level.

### Declarative behavior and custom Lua

- [ ] Make TypeScript behavior helpers return serializable event/condition definitions, such as interact → set door state → play sound, rather than retaining JavaScript closures. Validate instruction signatures, targets, state ownership, and supported runtime capabilities during compilation.
- [ ] Implement a small native event vocabulary first: triggers, doors, dialogue, pickups, conditions, ordered tasks, and simulation-tick waits. Expand into quests, cutscenes, encounters, battles, and minigames as those systems are recovered; unavailable operations must be discoverably unsupported.
- [ ] Store explicit event-graph progress, wait conditions, targets, timeouts, cancellation state, and mod-defined persistent data in authoritative native state. Provide source-correlated inspection, stepping, breakpoints, and snapshot/replay support; do not hide quest progress in build-time JavaScript or unserializable closures.
- [ ] Reference packaged Lua modules for behavior beyond the declarative vocabulary, using the Phase 7 deterministic API and permission model. Ordinary content authoring must not require C++ rebuilds; a genuinely new engine capability remains an explicit engineering task, not an invented SDK call.
- [ ] Provide reliable movement, one-time reward, interaction, and quest-transition primitives with defined failure/re-entry behavior. Keep actor-arrival waits and logical timers on the simulation scheduler, independent from GUI animation or wall clocks.

### Agent discovery, edits, and build services

- [ ] Publish SDK types, schemas, examples, capability/version discovery, and compact agent-facing guidance with runnable original examples. Index assets and content by exact IDs, tags, dimensions, anchors, animations, compatibility, references, and provenance; provide previews when useful.
- [ ] Mark recovered original content as verified, partially understood, opaque/preserved-only, or unsupported. Preserve unknown original script blocks losslessly and keep factual game/lore records distinct from generated creative suggestions. Asset existence must not imply safe editability.
- [ ] Expose a CLI and versioned JSON-RPC service over local stdio/IPC for discovery, source/parameter edits, validation, build, runtime scenario launch, inspection, traces, captures, tests, diffs, and packaging. Add MCP only as an optional adapter; do not require an embedded chatbot, network service, GUI, or particular agent vendor.
- [ ] Use semantic, batched, atomic edit transactions with expected source revisions, idempotent retries, dry runs, diffs, and undo. Validate cross-file changes as a unit, reject stale writes, and share command contracts with graphical clients. Direct file edits must use the same build/validation pipeline.
- [ ] Keep source revision, package hash, runtime session ID, and snapshot ID distinct. Support cancellation and bounded/paginated queries; pin active test sessions to immutable package revisions so subsequent edits cannot silently change what is being verified.
- [ ] Return machine-readable diagnostic codes, severity, source spans, object/operation IDs, observed/expected values, and exact reproduction scenarios. Include execution traces, native state, and optional object-ID/collision/path/camera overlays so an agent can map a visible problem back to an editable component.
- [ ] Treat source generators and dependency installation as executable build code. Isolate them with OS/process-level filesystem/network/credential restrictions, controlled dependencies, separate output/temp directories, memory/work limits, and a watchdog. Node `vm` or TypeScript typing is not the isolation boundary; never expose runtime debug privileges to untrusted generators or Lua.
- [ ] Implement build watching with atomic publication of successful outputs and retention of the previous working package on failure. Initially restart the preview scenario after rebuilding; add compatible incremental reload and state migration only at validated safe boundaries with explicit snapshot invalidation.

Proposed command surfaces, to be implemented and documented rather than assumed to exist:

```sh
xem-content build mods/mine/MineEntrance.ts --out build/mine --diagnostics json
xem-tool scenario run --package build/mine --scenario mine:open-gate-and-enter --headless --unthrottled --report build/mine-test.json
```

The build command type-checks/transpiles and executes source; the native scenario command consumes its output. Node packages, geometry kernels, and authoring callbacks do not execute inside ordinary gameplay.

### Early bridge gate and prompt-to-playable verification

- [ ] First demonstrate one generated wall, a usable doorway/door interaction, and a collectible through the complete SDK → mesh/package → native loader → gameplay path. Use ordinary TypeScript before adding TSX or a broad editor shell.
- [ ] Extend the bridge to two traversable rooms with an arched passage, stairs or another elevation change, a custom procedurally modeled object, an NPC/dialogue interaction, and a one-time reward. Build the environment/object geometry from source without external geometry; verify an additional variant mixing in an original or redistributable external model.
- [ ] Have an agent discover the supported SDK, write the source, build it, and complete the scenario through legal native commands. Test collision/clearance, interactions, transitions, reward-once semantics, save/restore, and leave/re-enter behavior under stepped and unthrottled execution with optional native images.
- [ ] Supply a known failing revision, require source-aware diagnosis and repair, and rerun the protected tests. Then request a targeted change such as widening the arch while preserving unrelated content and IDs. Deliver the source diff, immutable package, captures where enabled, and exact test/replay artifacts.
- [ ] Separate privileged fixture setup from subsequent gameplay. Test helpers must move through actual collision and eligibility rules, not teleport or set completion flags. Protect acceptance criteria and harness configuration from authoring edits; report passed, failed, timed out, and unsupported distinctly.
- [ ] Expand to a prompt-authored small town/dungeon with a branching quest, optional encounter, failure/retry paths, and custom Lua behavior through Phase 7. Validate references, declarative reachability where analyzable, script/runtime behavior, save/rewind, duplicate rewards, and presentation separately. Do not claim static graph checks prove arbitrary scripts correct.
- [ ] Record engine/compiler/package versions, seeds, initial state, actions, coverage, result assertions, failures, and optional images/audio for every verification run. Use multiple seeds/player policies for balance checks and distinguish simulation time from estimated human reading/decision time.

**Early exit criterion, required by Phase 3:** The two-room authored bridge builds, loads, plays, saves/restores, and survives a diagnosed source revision in the actual native runtime without engine C++ changes or GUI automation. Generated and imported geometry paths are demonstrated separately; headless execution requires no authoring runtime or GPU. The broader Phase 2A checklist remains open until demonstrated through the Phase 7/creator-toolkit milestones.

## Phase 3 — Deliver an independently implemented, agent-playable native slice

**Goal:** Prove the full pipeline from original-game evidence to native gameplay, including foundational agent play rather than only human-visible output.

- [ ] Select a compact field-to-battle scenario with enough variety to exercise the engine rather than a display-only room.
- [ ] Implement field loading, character rendering, movement, running, jumping, collision, camera rotation, interaction, and map transitions.
- [ ] Implement the original-script instructions needed for the slice, including dialogue, concurrent activity, waits, triggers, and story-state changes.
- [ ] Implement a representative on-foot encounter with action selection, attacks, targeting, damage, enemy behavior, rewards, and return to the field.
- [ ] Implement the required game menus, inventory/equipment interactions, ordinary save/load, and basic keyboard/controller input.
- [ ] Implement the required music sequence/sample playback, sound effects, and one FMV-to-gameplay transition using this project's own game-specific decoding and playback integration.
- [ ] Capture original-game reference runs and compare meaningful states, outcomes, and timing rather than requiring PS1 framebuffer artifacts to match in Modern mode. Retain separate references for optional PS1 visual fidelity.
- [ ] Test state restore across the field/event/battle boundaries and fix architectural gaps before broad content expansion.
- [ ] Have an agent cold-start the slice through typed scenario setup, inspect complete state, traverse/jump/interact, make dialogue and menu choices, complete the encounter through legal gameplay commands, and save/restore without OS input injection or screenshot interpretation. Record setup/debug operations separately from subsequent normal play.
- [ ] Demonstrate exact stepping, semantic readiness, a conditional breakpoint/watchpoint, a failed assertion with replay artifacts, and an unthrottled run of the same scenario. Verify outcome equivalence with real-time execution rather than accepting a faster but simplified simulation.
- [ ] Attach a human spectator to the headless agent session, stream images, change capture cadence, and disconnect while the agent continues. Confirm unchanged state hashes and no pacing dependency on the viewer.
- [ ] Demonstrate the Phase 2A early authored-content bridge alongside, not instead of, the recovered original-game slice. Both paths must use the same native loaders/services, movement and interaction rules, authoritative state, agent API, and optional renderer; record separate original-fidelity and authoring acceptance evidence.

**Exit criterion:** Field exploration → dialogue/cutscene → battle → reward → menu → save/load works natively on Arch under human input and direct headless agent control. The agent run requires no desktop/display/GPU/audio device in image-free mode, offers optional spectator images, and executes without real-time throttling or emulator execution. Debug/setup capabilities are demonstrated; ordinary slice progression needs no corrective debug intervention. The Phase 2A source-to-playable early gate also passes before broad expansion.

## Phase 4 — Complete both discs and all original gameplay systems

**Goal:** Expand independent decompilation and implementation until the complete game is playable by humans and agents.

- [ ] Complete field behavior: terrain/elevation, ladders, traversal, party following, triggers, camera rules, map transitions, and story-dependent variants.
- [ ] Complete the recovered event interpreter and all used instructions. Track coverage by map, scene, branch, and original instruction family.
- [ ] Complete menus, dialogue, shops, equipment, inventory, party management, and ordinary save/load; implement original-save import/export from independently recovered formats where supported.
- [ ] Complete on-foot battles, including combos, Deathblows, status effects, enemy AI, scripted encounters, rewards, and progression.
- [ ] Complete Gear battles, resources, equipment, animation, enemy behavior, and their distinct rules and interfaces.
- [ ] Complete the world map, transportation, location entry, and story-dependent world changes.
- [ ] Complete music sequencing, sample playback, sound effects, mixing, FMV demux/decoding integration, and event/media synchronization.
- [ ] Reproduce and validate original Mono, Stereo, and Wide signal behavior, including phase/polarity and routing, before using native Wide output for headphone surround. Identify which content paths actually carry Wide processing.
- [ ] Complete every minigame, optional activity, side quest, boss, ending sequence, and relevant failure/retry path.
- [ ] Complete both discs' content and transitions, with unified imported-data access instead of requiring a physical disc swap during native play.
- [ ] Eliminate silent fallback logic and progression workarounds; account for remaining unimplemented or unvalidated behavior explicitly.
- [ ] Build automated progression checkpoints plus manual coverage for interactions that scripted runs do not meaningfully exercise.
- [ ] Extend direct agent actions, complete introspection, typed go-anywhere setup, breakpoints, and semantic readiness alongside every subsystem. Native world-map, battle, minigame, cutscene, and progression setup must not remain limited to the emulator apparatus's initial field-entry adapters.
- [ ] Run both-disc progression and optional-content scenarios headlessly at unlocked speed, with reproducible inputs/seeds and bounded assertions. Test human and agent command-path equivalence and separate debug-launched scenario coverage from continuous unmodified playthrough evidence.

**Exit criterion:** Both discs and optional activities are playable from beginning to end, with no known progression-blocking gaps. All game systems are accessible through the foundational agent/debug interface and valid native go-anywhere setup; no gameplay subsystem requires a desktop UI or real-time pacing.

**Parallelization:** After Phase 3, rendering, human input, and mod infrastructure may advance alongside this phase. The agent interface is already required, not a parallel task that can be deferred. Complete-game validation remains mandatory before the playable PC alpha checkpoint.

## Phase 5 — Modern rendering, optional PS1 quirks, arbitrary resolution, aspect ratio, and framerate

**Goal:** Deliver modern rendering by default while preserving PS1 rendering quirks as optional, independently configurable presentation features.

- [ ] Render independently decoded geometry, materials, sprites, textures, and camera transforms through the native renderer; offer optional low-resolution rasterization without embedding a PS1 emulator framebuffer.
- [ ] Preserve affine texture distortion, vertex snapping/wobble, low-color framebuffer quantization, PS1 dithering, and low-resolution rasterization as selectable capabilities, disabled in the default Modern profile.
- [ ] Provide Modern, PS1-style, and Custom visual profiles with individual controls for affine/perspective-correct texturing, projected-vertex quantization, original color precision, dithering, rasterization resolution, and recovered primitive-order/depth quirks where applicable.
- [ ] Implement each PS1 quirk at its appropriate geometry, interpolation, rasterization, ordering, or color-processing stage and validate it against original-game captures, rather than substituting unrelated final-screen blur/noise/scanline filters.
- [ ] Keep PS1-style effects independent from gameplay timing, output resolution, aspect ratio, culling defaults, and texture/UI filtering. Permit widescreen/high-refresh output with selected PS1 effects; profiles must not silently reset independent settings.
- [ ] Reproduce intentional transparency, masking, palette animation, fog, and compositing with modern rendering techniques, including selected fidelity behavior where relevant.
- [ ] Support arbitrary window/internal resolutions within device limits, independent render scaling, resizing, fullscreen/windowed modes, and high-DPI displays.
- [ ] Support arbitrary aspect ratios with correct projection, configurable FOV, UI anchors, and safe areas. Do not stretch portraits, text, or FMVs.
- [ ] Audit wider framing for missing surfaces, incomplete backgrounds, and out-of-frame staging. Record content repairs separately from renderer changes.
- [ ] Support arbitrary presentation framerates, custom caps, uncapped rendering, synchronization options, and stable frame pacing.
- [ ] Interpolate camera/entity transforms and suitable animations between simulation updates. Preserve authored sprite timing and do not equate high refresh rates with mandatory invented animation frames.
- [ ] Verify that presentation framerate and PS1-fidelity settings do not alter movement, jumping, encounters, battle timing, scripts, minigames, or RNG progression.
- [ ] Default optional rendering culling to off. Expose legacy visibility rejection, distance, frustum, occlusion, and back-face culling separately; do not conflate culling with clipping, depth testing, intentional hidden entities, or gameplay activation.
- [ ] Add independent world-texture and UI filtering settings, with a separate sprite-filter setting where useful. Support clear nearest/smoothed choices without forcing UI blur when filtering the world.
- [ ] Validate depth ordering, alpha edges, texture seams, palette effects, and shader behavior on Vulkan, Direct3D 12, and Metal in Modern, PS1-style, and Custom configurations, including live profile switching.
- [ ] Make the same renderer available to offscreen screenshots, agent observation, and optional spectator streams. Allow render skipping without skipping simulation; verify identical authoritative state in interactive, headless/no-image, and headless/streamed modes.
- [ ] Validate procedural and imported mod meshes through the same native rendering path, supported material subset, visual profiles, and optional captures. Keep generator source/object attribution available for selection and debugging without making a browser or external preview authoritative.

**Exit criterion:** Recorded gameplay reaches equivalent authoritative state across resolutions, aspect ratios, framerates, backends, visual profiles, and render-disabled execution. Modern is the default; supported PS1 quirks are selectable and validated, including in agent spectator images.

## Phase 6 — Native keyboard, mouse, SDL3 input, and XInput

**Goal:** Make the full game usable as a native PC application while keeping human input and foundational direct agent input on the same engine command layer.

- [ ] Implement action-based input contexts for exploration, combat, minigames, game menus, the app menu, and editors.
- [ ] Implement SDL3 gamepad support, hot-plugging, remapping, dead zones, analog movement, rumble, and appropriate prompts.
- [ ] Support XInput devices on Windows through the selected SDL3 input path; validate that one physical controller cannot generate duplicate actions through overlapping providers.
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

- [ ] Provide bindings for every battle and minigame action, not just exploration. Detect conflicts within active contexts.
- [ ] Make game menus mouse-browsable: inventory, equipment, shops, battle commands, applicable targeting, save/load, and configuration.
- [ ] Implement real hit-testing, hover feedback, selection, clicking, disabled states, and tooltips instead of translating clicks into repeated directional inputs.
- [ ] Make every scrollable menu respond to the scroll wheel, including nested lists and high-resolution wheel/trackpad events.
- [ ] Keep keyboard/controller selection and mouse focus coordinated without a stationary cursor stealing focus.
- [ ] Make Tab toggle the game menu where gameplay permits it. Keep the app menu on Esc independent from skipping; Esc is not ordinary game-menu Cancel.
- [ ] Specify pause behavior and input routing so overlays cannot leak actions to gameplay beneath them. Resolve text-entry/editor focus explicitly.
- [ ] Handle mouse capture, cursor visibility, focus loss, controller disconnection, and device switching cleanly.
- [ ] Map these physical-input clients to the existing native command handlers. Keep agent commands independent of physical key bindings, focus, and virtual devices; expose semantic equivalents for menu toggles, scrolling, selection, and all new actions with the same validation rules.
- [ ] Test explicit human/agent input ownership and handoff. Spectator input must not alter the agent's session unless control is deliberately granted.

**Exit criterion:** The entire game is operable without a controller, every player-facing menu has appropriate mouse support, and every scrollable menu responds correctly to the wheel. Agents can invoke the equivalent validated engine actions directly without any synthesized OS/SDL input.

## Phase 7 — Lua scripting, agentic content authoring, and the native mod framework

**Goal:** Build the extension system used by player mods, agents, and the graphical editors without weakening deterministic state or debug visibility. Expand the Phase 2A source-to-playable bridge into a usable prompt-driven mod SDK.

- [ ] Define native mod packages with stable IDs, versions, dependencies, compatibility requirements, load order, conflicts, and configurable settings.
- [ ] Implement asset overrides and structured data patches for maps, models, textures, sprites, portraits, UI, dialogue, audio, FMVs, encounters, and gameplay tables.
- [ ] Expose documented Lua APIs for entities, levels, triggers, dialogue, cameras, cutscenes, battles, minigames, audio, and UI.
- [ ] Run original bytecode in this project's independent interpreter; let Lua extend or replace specific behaviors without requiring a wholesale rewrite of original content first.
- [ ] Make the underlying battle and minigame rule boundaries replaceable, so later tools can change mechanics rather than only constants.
- [ ] Implement explicit, versioned Lua state and resumable tasks compatible with snapshots, rewind, and editor scrubbing. Reject unsupported persistence patterns with actionable diagnostics.
- [ ] Specify deterministic event order, task scheduling, RNG access, and simulation-time access. Distinguish presentation-only scripts from state-changing logic.
- [ ] Restrict filesystem, process, native-module, and network access; apply memory/execution budgets and contain script failures.
- [ ] Build a graphical mod manager with installation, profiles, enable/disable controls, dependency resolution, conflict reporting, and safe mode.
- [ ] Allow hot reload only at defined safe boundaries. Invalidate or migrate snapshots whose scripts, assets, or state schemas changed.
- [ ] Define optional content-pack import interfaces independently. Treat existing texture/audio/translation packs as separately supplied content, not dependencies or automatically compatible engine code.
- [ ] Ship original example mods changing a level event, cutscene, battle behavior, and minigame through Lua without recompiling the engine.
- [ ] Require mod-defined gameplay state, actions, tasks, errors, and setup hooks to participate in agent introspection, direct commands, scenario launch, breakpoints, snapshots, and unlocked-speed tests. Publish capability/schema extensions instead of hiding mod behavior behind rendered UI.
- [ ] Keep trusted native debug access separate from untrusted in-game Lua privileges. Test all example mods through the same image-free and streamed agent sessions used for the base game.
- [ ] Complete the Phase 2A SDK/build/service checklist for the supported content types, including schema/catalog discovery, reusable procedural components, scoped edits, source-aware diagnostics, immutable builds, and a prompt-authored town/dungeon with branching progression and custom Lua behavior.
- [ ] Extend typed authoring definitions to items, characters, Gears, enemy behaviors, encounters, rewards, dialogue, cutscenes, and minigame configuration. Use shared native/event/Lua contracts rather than a second simulation or generated C++ for routine content.
- [ ] Standardize source mod projects with a manifest, intent/acceptance specification, TypeScript components, parameter data, optional assets, Lua behavior, and scenario tests. Separate creative proposals from verified game facts and explicit engine capability limits.
- [ ] Demonstrate prompt → retrieval → small specification → blockout → native test → diagnosis/repair → presentation refinement → package. Include a follow-up prompt that edits only the requested content and preserves human changes, stable identities, and protected tests; do not regenerate the whole project by default.
- [ ] Treat optional editor extensions and external asset-generation integrations as separate trusted tooling capabilities. A gameplay mod must run without executing its editor plugin or generator; generated artwork remains a candidate until import validation and native review pass.

**Exit criterion:** All four example mods run, their state survives save/restore, they remain playable/debuggable headlessly, and the unmodified base game runs with no third-party mod installed. The expanded Phase 2A workflow produces and revises a source-authored town/dungeon mod, including real generated geometry and tested gameplay, without engine C++ changes or GUI automation.

## Phase 8 — Native save states, rewind, and fast forward

**Goal:** Build player-facing time-control features on the snapshots and agent-controlled scheduler already required in Phase 2, not postpone those foundational facilities until this phase.

- [ ] Implement versioned player snapshot files containing game state, original-script/Lua task progress, RNGs, content IDs, and active mod versions, using the existing exact debug snapshot service.
- [ ] Keep ordinary game saves separate from full execution snapshots. Define migration and compatibility rules for each.
- [ ] Add snapshot slots, quick-save/load, timestamps, thumbnails, and an undo-load safeguard, with atomic writes and corruption checks.
- [ ] Support snapshots during field events, battles, minigames, and cutscenes, not just at ordinary save-safe locations.
- [ ] Implement configurable player fast-forward speeds with hold/toggle controls on the core scheduler. Keep the agent's fully unthrottled mode, exact stepping, and bounded run-until APIs independently available; higher presentation framerate is neither fast forward nor unthrottled simulation.
- [ ] Add audio policies such as time stretching or muting above a selected speed, with correct return to normal playback.
- [ ] Build rewind from periodic snapshots and recorded input/events with deterministic replay. Expose a duration or memory budget, snapshots, and a timeline indicator.
- [ ] Discard the abandoned future when play resumes after rewind. Prevent duplicated rewards, persistent writes, or other external side effects during replay.
- [ ] Restore audio and FMVs from logical playback positions; rebuild platform-specific decoder, audio-device, and GPU state.
- [ ] Preserve or deterministically reconstruct Wide-decoder history, HRTF convolution tails, and resampler delay through portable presentation-state caches or bounded audio pre-roll. Validate phase-preserving linked-channel processing or muting during acceleration; prevent stale cues/clicks after load, rewind, skip, or device changes.
- [ ] Support map changes, battles, cutscenes, and disc-content transitions across rewind. Track and resolve unsupported boundaries before feature completion.
- [ ] Expose save/load, rewind, branching replay, and snapshot inspection through the agent API with explicit ticks, compatibility errors, and isolated save directories. An image-free run must restore the same simulation state and rebuild presentation on demand.
- [ ] Test replay/state equivalence across supported platforms and representative mod combinations, in real-time, stepped, accelerated, and unthrottled modes with images off/on. Declare incompatible snapshots rather than silently accepting divergent state.
- [ ] Include compiled event-task progress and exact authored-package identities in snapshot compatibility. Test save/re-entry/rewind during generated doors, quests, battles, and Lua tasks; package reload must explicitly preserve, migrate, restart, or reject state rather than silently changing definitions beneath it.

**Exit criterion:** Save → restore and record → rewind → replay produce equivalent authoritative state throughout the game and with supported Lua mods. Agent-controlled execution remains independent of real-time pacing and rendering; audiovisual state resumes without stale processing history.

## Phase 9 — Mod-inspired options, encounter defaults, and cutscene skipping

**Goal:** Independently implement the requested convenience features without basing the project on a preexisting mod.

### Identify options from popular mods

- [ ] Research player-facing descriptions and documented behavior of popular Xenogears mods. Record dated adoption evidence such as downloads or community usage where available; do not label an unranked shortlist as a verified popularity ranking.
- [ ] Translate the findings into this project's own option specifications, defaults, tests, and conflict rules. Do not adopt mod source, binary patches, or another project's implementation as the base.
- [ ] Evaluate independently implemented encounter-rate controls, separate experience/money multipliers, fast/instant text, optional bug fixes, combat/item/character rebalance settings, and other strongly supported convenience requests.
- [ ] Provide optional dialogue/localization, FMV audio/subtitle, portrait/UI, and texture replacement support. Keep third-party authored content separately installable and unnecessary for the base runtime.
- [ ] Separate correctness fixes, convenience features, balance changes, translations, and visual replacements. Do not hide unrelated modifications behind one mandatory preset.

### Platforming and encounter behavior

- [ ] Identify platforming-heavy areas by original-game inspection and exact map/room IDs; document the default list and rationale.
- [ ] Disable random battles in those areas by default while preserving bosses, mandatory encounters, and scripted fights.
- [ ] Provide global/per-area overrides and visible settings explaining the area-specific default.
- [ ] Independently test and fix input loss or unsafe traversal interruptions during encounter preparation, especially jump inputs and transitions while airborne.
- [ ] Offer reward compensation separately rather than silently increasing experience or money when encounter rates change.

### Text and modern cutscene controls

- [ ] Separate text reveal speed from event timing and completion. Validate fast/instant text against waits, automatic dialogue closure, concurrent actor movement, and rapidly advanced conversations.
- [ ] Provide pause, a visible skip action, and configurable hold-to-skip protection. Keep the app menu on Esc independent from skipping.
- [ ] Implement semantic skipping: bypass presentation while preserving required story flags, party changes, inventory/rewards, transitions, and other consequential actions.
- [ ] Stop at meaningful choices, mandatory gameplay, and battle boundaries rather than silently choosing an outcome.
- [ ] Give complex original scenes explicit completion handlers or validated safe skip segments; expose equivalent skip/completion hooks to Lua and the cutscene editor.
- [ ] Apply consequential changes exactly once and return the correct camera, location, actors, and player control.
- [ ] Compare watched and skipped outcomes for every supported scene path, including scenes entered after load or rewind.
- [ ] Expose options, dialogue choices, and explicit skip commands to agents. Test both natural completion under unthrottled logical time and semantic skipping; headless mode must not silently skip scenes, auto-select choices, or substitute debug writes for consequential actions.

**Exit criterion:** Platforming-area defaults work, optional modifications remain independent, and cutscene skipping preserves progression without requiring any preexisting mod or real-time-only test path.

## Phase 10 — First-person mode, clearer FMVs, and Wide headphone surround

**Goal:** Deliver the requested camera and audiovisual options, including two-channel headphone surround faithful to the verified intent of the original Wide mode.

### First-person mode

- [ ] Implement a playable first-person camera with mouse/controller look, camera-relative movement, configurable FOV, sensitivity, and inversion.
- [ ] Preserve running, jumping, collisions, interaction, and traversal; this is not merely a detached inspection camera.
- [ ] Define on-foot and Gear exploration behavior and explicit transitions for battles, minigames, and authored cinematic framing.
- [ ] Hide or adapt the controlled character's representation, handle sprite orientation and targeting, and prevent near-plane clipping through the player or scenery.
- [ ] Let scripted scenes temporarily own the camera, then restore the user's selected mode and orientation appropriately.
- [ ] Inspect first-person and wide-FOV views for missing surfaces or incomplete scenery. Create original content overrides where needed; disabling culling does not manufacture missing geometry.

### Clearer FMVs

- [ ] Implement high-quality original FMV decoding/playback with correct proportions, color interpretation, timing, and audio synchronization.
- [ ] Add an enhanced pipeline using reviewed deblocking/denoising and upscaling, with cached derived media or optional replacement assets.
- [ ] Provide an explicit Original / Enhanced FMV setting with per-video original fallback. Enhanced media must not be required to play.
- [ ] Keep scaling, sharpening, and any motion interpolation independent; high-framerate gameplay must not force synthetic FMV frames.
- [ ] Review faces, linework, subtitles, motion, compression artifacts, skip/seek behavior, and transitions back to gameplay in both video modes.

### Wide-decoded headphone surround

**Required signal path:** Original-game Wide output (two channels) → verified Wide-specific decoding/spatial reconstruction → intended virtual sound field → HRTF binaural rendering → headphone left/right output (exactly two channels).

- [ ] Complete original Wide analysis and native-signal validation before finalizing the decoder. Document the intended spatial reference, signal relationships, routing, and unresolved ambiguity; the Wide name alone does not establish Dolby/Pro Logic encoding or a discrete 5.1/7.1 mix.
- [ ] Independently implement the decoder/spatial reconstruction appropriate to the recovered signal. Use matrix decoding only where supported; represent verified phase-based expansion through an appropriate virtual playback model rather than forcing anti-phase content into invented rear channels.
- [ ] Recover only spatial components justified by the signal and original sound behavior. Preserve left/right placement, centered material, ambience, and any verified surround cues without inventing independent rear, height, LFE, or arbitrary object positions.
- [ ] Render the reconstructed field with head-related transfer functions (HRTFs) to ordinary two-channel headphones, not a simple stereo downmix. Select general-purpose binaural infrastructure and appropriately licensed HRTF data independently of the game-specific decoder; offer a reference profile and optional calibrated HRTF choices.
- [ ] Expose Headphones — Wide surround as an optional output mode that processes native Wide exactly once. Retain original Mono, Stereo, and undecoded Wide choices, and restore the prior original mode when headphone surround is disabled.
- [ ] Route music, effects, reverb, FMVs, and replacement audio according to verified signal behavior, preserving interchannel phase/timing. Do not double-process material already spatialized or assume every stereo asset contains Wide encoding.
- [ ] Keep this mode faithful to the original Wide mix rather than arbitrarily repositioning emitters or rotating the entire field when first-person mode is selected. The reference profile must not add unsupported artificial rear/room effects.
- [ ] Maintain headroom, mix balance, transients, linked-channel gain behavior, latency compensation, and click-free switching. Provide level-matched A/B playback against original Stereo and undecoded Wide; prevent external virtualization from being silently stacked on the binaural output.
- [ ] Validate original-versus-native Wide signals, decoder routing, and binaural output separately using synthetic phase/polarity/panning fixtures and local original-game captures across both discs. Include listening validation against the established spatial reference; a generic widener or unverified surround preset does not satisfy the requirement.
- [ ] Validate Phase 8 state restoration, accelerated playback, sample-rate changes, device reconnects, and operation on ordinary two-channel Linux/Windows/macOS output devices without an external surround processor.

### Agent and headless integration

- [ ] Expose camera mode/orientation, media timeline, sound mode, processing parameters, and diagnostic captures through the agent interface. Support optional spectator rendering in first-person, original/enhanced video, and all visual profiles.
- [ ] Keep media completion and cutscene synchronization driven by logical simulation time when output is disabled. Provide offline audio/video/DSP captures for fidelity tests without requiring an audio device or wall-clock playback; rebuild omitted presentation history before enabling live output.

**Exit criterion:** First-person exploration is playable, camera handoffs work, every enhanced FMV retains an original option, and Wide headphone surround reproduces the verified intended presentation through exactly two channels. These features remain inspectable/testable by agents without forcing real-time simulation or physical audiovisual devices.

## Phase 11 — Graphical level and cutscene editors

**Goal:** Author and inspect content using the same independently built runtime and foundational agent/debug services as the game. Build graphical clients over the Phase 2A/7 authoring pipeline rather than introducing a competing project format or editor-only operations.

- [ ] Create a shared editor shell with project management, asset browser, inspectors, console, undo/redo, autosave, and validation.
- [ ] Reuse this project's renderer, importers, simulation, state system, and Lua APIs for exact in-engine previews.
- [ ] Build graphical level tools for geometry editing/import/export, object placement, collision, spawns, map connections, trigger volumes, encounter regions, and cameras.
- [ ] Visualize collisions, event volumes, navigation constraints, visibility rules, and missing-geometry problems that are otherwise difficult to inspect.
- [ ] Build a cutscene timeline and branching event graph for actors, movement, cameras, dialogue, audio, FMVs, story changes, interactions, and battles.
- [ ] Import original scripts through this project's recovered instruction model. Preserve opaque or not-yet-converted constructs losslessly and flag them instead of destructively guessing their meaning.
- [ ] Integrate Lua editing with diagnostics, API completion, breakpoints, variable inspection, and execution stepping.
- [ ] Define lossless boundaries between visual graphs/timelines and handwritten Lua. Preserve custom Lua nodes when visual content is edited or regenerated.
- [ ] Use snapshots and deterministic replay for timeline scrubbing and preview; scrubbing must not duplicate or permanently apply story consequences.
- [ ] Add graphical authoring and validation of skip segments, completion handlers, mandatory choices, and return-to-gameplay behavior.
- [ ] Store projects in version-control-friendly formats and export native mod packages without modifying the original imported asset store.
- [ ] Build the editors' play/debug/inspect/go-anywhere controls on the existing native agent services rather than a second debug implementation. Let agents launch and validate edited levels/cutscenes headlessly, while humans optionally inspect the live image stream and state.
- [ ] Open TypeScript/TSX components and their parameter schemas directly, display generated geometry/instances with source mapping, and route edits through the same revisioned command/build services used by agents. Keep source documents and history outside disposable UI components.
- [ ] Define editable source boundaries: expose declared parameters and supported declarative graphs; propose reviewable source patches for code-driven content. Preserve handwritten algorithms, expressions, Lua nodes, and opaque original-script blocks instead of reverse-generating arbitrary source from a flattened scene.
- [ ] Add selection-aware agent context containing entity/component IDs, source revision, selected properties, graph nodes, diagnostics, and optional viewport captures. Support targeted prompt edits with diffs and undo; do not regenerate unrelated rooms or overwrite intervening human changes.
- [ ] Expose procedural geometry/material/collision parameters, connection anchors, and source-correlated errors in inspectors. Rebuild affected content through the compiler and preview it in the native renderer; external previews may supplement but never replace native traversal and event tests.
- [ ] Allow specialized inspector/generator/visualization extensions through documented services with matching headless operations where they change content. Treat extensions as separately enabled trusted tools, not code automatically executed when loading a gameplay mod. Choosing QML or another UI toolkit must not change the TypeScript source or native-runtime contracts.

**Exit criterion:** A creator can build or modify a level and branching cutscene, author Lua behavior, export a mod, and play it in a normal game build. The same content can be launched, inspected, and tested headlessly through the native agent interface. Agent-created TypeScript worlds remain editable through supported source/parameter operations, graphical changes preserve custom code, and human/agent revisions share validation and undo semantics.

## Phase 12 — Graphical battle-system and minigame modding

**Goal:** Enable substantial rules changes, not just texture replacements or numerical table edits.

- [ ] Build editors for characters, Gears, enemies, equipment, items, abilities, status effects, progression, encounters, and rewards.
- [ ] Expose combat action availability/costs, turn scheduling, targeting, damage/defense, combos, Deathblows, Gear rules, victory, and defeat.
- [ ] Allow replaceable battle-rule modules through Lua rather than limiting creators to callbacks around an immutable battle system.
- [ ] Add battle animation, effects, camera, and interface editing with immediate runtime preview.
- [ ] Build minigame-specific tools for arenas/layouts, entities, input actions, rules, scoring, timing, opponent behavior, and win/loss conditions.
- [ ] Let Lua change minigame state machines and runtime behavior, not just assets or difficulty values.
- [ ] Provide isolated battle/minigame test sessions with configurable starting state, deterministic seeds, pause/step inspection, and repeatable automated runs.
- [ ] Validate references, resources, script failures, dependencies, persistence, and compatibility before export.
- [ ] Publish original example projects demonstrating a substantially changed battle ruleset and a materially redesigned minigame using only the tools and Lua.
- [ ] Use native go-anywhere entry, direct gameplay commands, full mod-state introspection, and unthrottled headless runs for battle/minigame testing. Expose all creator-defined rules and state through the versioned debug schema and allow optional human spectator images during agent play.
- [ ] Demonstrate an agent creating a new enemy/encounter or minigame with procedural arena geometry, TypeScript definitions, custom Lua mechanics, exposed editor parameters, and native tests. Evaluate balance over multiple seeds/policies and preserve legal-action, persistence, and source-revision evidence.

**Exit criterion:** A mod author can make those changes through the graphical tools and Lua without modifying or rebuilding engine C++. Agents can play, inspect, debug, and rapidly test the modified battles/minigames through the same native interface. Prompt-driven creation and targeted revision use the shared TypeScript/Lua pipeline rather than an editor-specific implementation.

## Phase 13 — Platform hardening, packaging, and release

**Goal:** Validate and ship the complete feature set, including the first-class agent interface, led by Arch Linux.

- [ ] Validate Arch Linux + Vulkan first: Wayland/Hyprland, X11, fractional scaling, mixed-DPI displays, ultrawide framing, fullscreen changes, focus loss, and controller reconnects.
- [ ] Validate Windows + Direct3D 12, including both the SDL3 input system and XInput controller behavior.
- [ ] Validate macOS + Metal on explicitly declared processor/OS targets, with correct application lifecycle, input, and display behavior.
- [ ] Test shader compilation, rendering correctness, resource lifetime, frame pacing, and authoritative state equivalence across the backends/platforms.
- [ ] Run complete-playthrough and optional-content coverage with default settings, original-gameplay settings, and representative mod combinations. Test Modern, PS1-style, and Custom visuals independently of gameplay presets, and original sound modes independently of Wide headphone surround.
- [ ] Validate image-free headless execution on machines without a display server, GPU, physical input device, or audio output device. Test the same protocol and simulation on all declared native platforms; enabling optional images must remain an explicit renderer-dependent choice.
- [ ] Require direct-agent-command playthroughs and go-anywhere scenarios in CI, with full state/assertion output, semantic readiness, deterministic replay, and unthrottled execution. Maintain emulator/native capability parity and explicit native-extension coverage as the reference apparatus evolves.
- [ ] Verify action-trace/state equivalence across interactive, image-free, offscreen, and streamed execution; across real-time, stepped, fixed-speed, and unlocked modes; and across supported mod configurations. Treat expected presentation differences separately from gameplay divergence.
- [ ] Test protocol versioning, capability discovery, invalid requests, stale snapshots, cancellation, concurrent inspectors, control handoff, debug permissions, disconnect/reconnect, state-query bounds, image-stream backpressure, and failure-artifact completeness.
- [ ] Validate the complete Wide decode-to-binaural path on ordinary two-channel headphone devices, with signal tests, listening comparisons, sample-rate/latency changes, and restoration checks. Confirm that no multichannel endpoint or external processor is required.
- [ ] Stress-test arbitrary presentation framerates, fast forward, repeated rewind/load, cutscene skipping, device changes, malformed assets/mods, and interrupted writes.
- [ ] Measure CPU/GPU, memory, snapshot-storage, loading, and editor performance; address regressions against documented hardware. Benchmark headless simulation ticks per second separately from rendering/streaming and Wide/HRTF costs; confirm there is no hidden real-time cap.
- [ ] Package an Arch PKGBUILD, Windows releases, and a macOS application bundle, including the editor suite and documented native headless/agent entry point. Agent/debug capabilities are shipped features with explicit access controls, not an abandoned private test harness.
- [ ] Add first-run disc-data import, hash/revision validation, configuration migration, controller setup, and understandable failure reporting. Provide noninteractive equivalents for agent startup/import/configuration without GUI dialogs.
- [ ] Verify clean-install defaults: Modern rendering with PS1 quirks available but off; culling off; random battles off in the platforming-area list; Tab for game menu; Esc for app menu; independent texture/UI filtering; original FMVs selectable; optional Wide headphone surround with original sound modes retained. Headless mode defaults to no image stream and local control, with explicit speed selection.
- [ ] Verify the base game builds and runs with no other Xenogears project, preexisting mod, or third-party game-specific code installed.
- [ ] Review distributed artifacts to exclude original source images, executable dumps, extracted game assets, private saves, and unintended development files.
- [ ] Publish developer setup, independent research/decompilation notes, player documentation, the Lua API, editor tutorials, content-pack guidance, and compatibility/versioning policies. Include the agent protocol/schema, direct-input examples, full debug/go-anywhere guide, spectator setup, speed-control semantics, reproducible scenarios, PS1 visual options, and Wide/HRTF evidence and provenance.
- [ ] Package/document the TypeScript SDK, pinned Node/build dependencies, geometry generators, schema/catalog tools, and headless content CLI separately from runtime requirements. Verify prebuilt mods play on supported platforms without Node.js, Qt/QML, a browser, or external modeling applications installed.
- [ ] Run source-to-native build, collision/material, schema-parity, protected scenario, malformed-package, generator isolation, reload/migration, and content-conflict tests in CI using original redistributable fixtures. Record package hashes and supported-toolchain reproducibility results; locally sourced game data remains outside public artifacts.
- [ ] Publish original prompt-to-playable examples covering generated-only geometry, mixed imported assets, declarative events, custom Lua, targeted revisions, source-aware repair, and graphical parameter edits. Include build/runtime architecture diagrams, exact CLI/RPC contracts, extension trust rules, and a capability/unsupported-feature catalog.
- [ ] Close the requirement-to-test matrix. Story completion alone is not completion of the requested port, agent-native runtime, source-authoring pipeline, and creator toolkit.

**Exit criterion:** The complete requirements matrix passes on the declared platforms. Human and agent play, image-free and optional streamed execution, complete debugging/go-anywhere access, and unlocked-speed testing are shipped and documented alongside all audiovisual and editing features. The agentic TypeScript build–play–repair workflow, source-preserving editors, and prebuilt native mod execution are also packaged and verified.

## Release checkpoints

| Checkpoint | Required result |
|---|---|
| Research/tooling baseline | Phases 0–2 provide independently imported original content and a serializable native runtime with direct agent commands, full introspection, debug/scenario control, exact stepping/unlocked execution, and optional spectator images for implemented systems. |
| Engineering preview | Phase 3: the same field/event/battle/save-load slice runs under human control and a direct headless agent on Arch, with debug demonstrations, no-image operation, optional live images, and no required real-time pacing. The Phase 2A early source-to-playable bridge passes separately alongside it. |
| Agent-authoring preview | Phases 2A–3: TypeScript generates actual two-room geometry and interactive content, exports a versioned package, and an agent plays, diagnoses, repairs, and revises it through the native runtime without engine edits or GUI automation. Not a substitute for original-game fidelity. |
| Playable PC alpha | Phase 4 completes both discs and full agent/debug/go-anywhere coverage; Phases 5–6 provide modern-default rendering, optional PS1 quirks, and complete physical PC controls alongside the direct agent API. |
| Feature-complete runtime beta | Phases 7–10 provide agent-compatible Lua mods, the expanded Phase 2A prompt-authored town/dungeon workflow, player save states/rewind/fast forward, optional gameplay changes, safe skipping, first-person mode, enhanced/original FMVs, and verified Wide-decoded two-channel headphone surround. |
| Creator-toolkit beta | Phases 11–12 provide graphical level, cutscene, battle-system, and minigame editing through the shared TypeScript content pipeline, native runtime/Lua, and existing agent/debug services, with source-preserving revisions, unlocked headless testing, and optional human observation. |
| 1.0 | Phase 13 validates and packages all features, including the native agent interface and agent-authoring SDK/build services, across Linux/Vulkan, Windows/Direct3D 12, and macOS/Metal. Arch leads development and release testing; pure headless simulation needs no rendering backend, and prebuilt content needs no authoring runtime. |

## Required feature coverage

| Requirement | Primary phases |
|---|---|
| Independent decompilation and native implementation; no foundation in another Xenogears project | 0–4, 13 |
| Arch Linux as the leading platform | 0, 2–3, 13 |
| Foundational agent-playable native runtime, not a later automation layer | 0–4; maintained through every later phase |
| True headless gameplay without a display, GPU, physical input, or audio output device | 2–4, 8, 10, 13 |
| Direct typed engine input; no simulated OS/SDL keyboard, mouse, or controller events | 2–4, 6–7, 13 |
| Full structured introspection, snapshots/deltas/events, and discoverable versioned agent API | 0–4, 7, 11–13 |
| Full debug access: stepping, breakpoints/watchpoints, state editing, stacks/traces, snapshots, replay, and failure diagnostics | 2–4, 7–8, 11–13 |
| Native go-anywhere parity with the emulator test apparatus plus complete typed native scenario setup | 0–4, 11–13 |
| Agent-unlocked simulation speed, exact ticks, bounded run-until, and no required real-time pacing | 2–4, 8–10, 12–13 |
| Optional live image stream and on-demand images for human spectators, independent from simulation pacing | 2–3, 5, 10–13 |
| Same authoritative game and command semantics for humans, agents, tests, and editors | 2–13 |
| Foundational prompt-to-playable agent authoring with build–play–diagnose–repair–package verification | 0, 2A–3, 7, 11–13 |
| TypeScript/optional TSX source defining actual environments, geometry, reusable components, and custom objects | 2A–3, 7, 11 |
| Manifold solid geometry plus surface/arbitrary-mesh generators and optional external models | 2A, 5, 11, 13 |
| Explicit source → Node build → intermediate representation → GLB/world/collision/events → C++ native-loader bridge | 2–3, 2A, 7, 13 |
| Prebuilt mod execution without Node.js, QML, browser, or a second game engine; native events and Lua own behavior | 2A, 7–8, 13 |
| Discoverable SDK/catalog, revisioned transactions, isolated builds, immutable test artifacts, and actionable source diagnostics | 0, 2A, 7, 11–13 |
| Shared source-preserving human/agent editing, targeted prompt revisions, and protected legal-action playtests | 2A–3, 7, 11–13 |
| Vulkan / Direct3D 12 / Metal on Linux / Windows / macOS when rendering is enabled | 2, 5, 13 |
| Arbitrary resolution, aspect ratio, and presentation framerate | 2, 5 |
| Culling off by default | 5, 13 |
| Modern rendering by default; PS1 quirks preserved as individual options and a PS1-style preset | 1–2, 5, 13 |
| Keyboard/mouse controls, mouse-browsable menus, wheel scrolling | 6 |
| SDL3 input and Windows XInput | 2, 6, 13 |
| Direction, run, jump, confirm, cancel, and left/right camera bindings | 6 |
| Tab toggles game menu; Esc toggles app menu | 6, 9, 13 |
| Player save states, rewind, and fast forward built on foundational agent snapshots/clock control | 2–3, 7–8 |
| Native mod support and graphical mod management | 7 |
| Lua for level, cutscene, battle, minigame, and other mod editing, with full agent/debug integration | 2, 7, 11–12 |
| Random battles off by default in identified platforming-heavy areas | 9, 13 |
| Independently implemented options informed by popular existing mods | 9 |
| Modern cutscene skipping with correct story outcomes | 9, 11 |
| Playable first-person mode | 10 |
| Clearer FMVs with an original option | 10 |
| Wide-specific decoding/spatial reconstruction and HRTF headphone surround faithful to verified Wide intent, with exactly two output channels | 1–2, 4, 8, 10, 13 |
| Original Mono, Stereo, and undecoded Wide playback remain selectable | 4, 10, 13 |
| Independent texture and UI filtering | 5, 13 |
| Graphical level and cutscene editors | 11 |
| Graphical battle-system and minigame mod tools | 12 |
