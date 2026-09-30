"""One-shot branch refactor; the branch-scoped workflow removes this file."""
from pathlib import Path
import ast
import re
import subprocess

ROOT = Path.cwd()
NEW = set()

def write(name, text):
    p = ROOT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text.strip() + "\n")
    NEW.add(name)

def replace(name, old, new):
    p = ROOT / name
    text = p.read_text()
    if old not in text:
        raise RuntimeError(f"Missing refactor anchor in {name}: {old[:80]}")
    p.write_text(text.replace(old, new))

write('plan.md', r'''
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
''')

write('AGENTS.md', '''
- Implement first. Read plan.md, docs/matching.md and only the relevant source/build
  files. Start from a real target/function or failing test; compile and inspect the
  first difference. Do not read all findings or regenerate status paperwork.
- Phase 1 is the complete both-disc PS1 matching decomp. New source belongs under
  decomp/, not in an expanded host Program model. Preserve original semantics and
  small subsystem headers; use original assembly privately as temporary scaffolding.
- A byte match is not source coverage. Never count placeholders, guess compiler
  identity, normalize away mismatches, replace expected bytes or invent passes.
- Keep the existing host reconstruction, captures and useful tests as reference/
  portability assets. Add original captures only for a concrete unresolved question.
- Do not preserve obsolete compatibility paths, fallbacks, migrations or duplicated
  requirement registries. Prefer the simplest implementation and existing tools.
- Use the pinned Nix shells. Run focused tests while editing and the public build
  before review. Record exact commands/results and missing private inputs honestly.
- Keep original assets and generated disassembly local. Do not modify prompt.md.
''')

write('README.md', '''
# Xenogears: Ex Machina

Independent Xenogears decompilation, native PC port and modding tools, with Arch
Linux leading development. The native application does not run the complete game.

Phase 1 now targets a **complete binary-matching PS1 decompilation of both discs**.
It does not stop at one playable slice or require every recovered function to be
ported into a host-side ownership model. See [the plan](plan.md) and the short
[matching workflow](docs/matching.md).

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
```

That smoke test exercises MIPS assembly/linking and exact comparison on an authored
fixture, not Xenogears decompilation. The original-compatible C compiler and game
build targets still need qualification. Source recovery and binary matching must
be reported separately; existing C++ comparisons imply neither a PS1 match nor a
complete decomp.

The existing `xem-reconstruction` library, original scenarios, findings and tests
remain useful reference/portability assets. Its state declarations are separated
into subsystem headers; `Program` remains its integration owner, not the design
for new PS1-target source. Run its public build with:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py --preset debug
```

[Development](docs/development.md) covers focused commands. Source profiles live
in `analysis/reference-profiles.json`; detailed findings are read on demand.
Original images stay in ignored `discs/`, and extracted bytes/captures/saves in
ignored `.local/`. The [source allowlist](packaging/source-files.txt) is audited.
The Nix path inputs contain tool configuration only, never original data.

The [native-agent](docs/agent/README.md) and [authoring](docs/authoring/README.md)
specifications preserve product requirements, not implemented capability. The
plan places native gameplay before the authoring bridge. Historical Phase 0
records and finding IDs retain their original scope; retired facet numbers are
not the current phase checklist.
''')

write('CONTRIBUTING.md', '''
# Contributing

Work from plan.md and the relevant implementation. Prefer a small source change
that compiles and reduces a real diff over a new framework, report or checklist.

For Phase 1 use docs/matching.md. Keep game-specific recovery independent of other
Xenogears projects. Reuse qualified source identities, types, algorithms and
existing observations. General-purpose tools and infrastructure libraries are
permitted; record their actual versions/licenses in the owning Nix recipe or a
short dependency note, not a new approval registry for every change.

Preserve original arithmetic, layout, call/overlay context and hardware intent.
Binary matching and readable source coverage are distinct. Keep unknowns visible;
never patch expected original bytes, hide mismatches or count retained assembly
as recovered compiled source. Do not demand a separate capture pipeline for an
already qualified exact match. Behavioral tests remain essential when adapting
source to a new platform or resolving uncertainty.

Use focused builds/tests during iteration; run the public build and relevant
matching tests before review. Report what ran and what was blocked by missing
private inputs. Source comments should explain actual semantics and source
coordinates. Findings are for new knowledge, not ceremonial per-function forms.

Original binaries, extracted assets, generated disassembly and private captures
remain local. Add only reviewed authored files to packaging/source-files.txt;
run tools/repository/source_archive.py --check. Preserve historical evidence at
its original scope. Do not change prompt.md as part of repository maintenance.
''')

write('docs/matching.md', r'''
# Matching-first source recovery

The loop is **recover source -> compile -> compare -> fix the first difference**.
Phase 1 covers every executable/overlay on both discs. The forest route is a
regression scenario, not the scope boundary. See ../plan.md for the phase exit.

## Tools and a working public check

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
m2c --help
spimdisasm --help
psx-objdump --version
```

This lean shell reuses the pinned m2c/spimdisasm packages and adds GNU MIPS
binutils, make and ordinary diff utilities. It does not install Ghidra merely to
compile/compare. Use the separate Ghidra default shell for program-wide inspection.
The stable psx-* commands wrap the pinned little-endian MIPS binutils. The smoke
check assembles an authored MIPS-I function and checks its exact linked bytes.
It establishes tooling, not Xenogears compiler identity or game-source progress.

## Establish the actual target

Qualify the compiler/assembler/linker on a representative resident/overlay sample.
Reuse existing Ghidra types and reviewed algorithms. Record exact flags, GP/small
-data settings, source identity, addresses and layout in each target's build file.
Do not select a PsyQ version from an unsupported loader default. Add maspsx or a
historical compiler through a pinned Nix recipe when that trial establishes the
need; do not install another large speculative toolchain upfront.

`decomp/Makefile` provides the small assemble/link/verify loop. A target supplies
`ORIGINAL`, `ORIGINAL_SHA256`, `IMAGE`, `OBJECTS`, `LINKER_SCRIPT` and its source
compilation rules in a make fragment. There is deliberately no invented game
configuration or default host C compiler. Until qualified targets exist, ordinary
`make -C decomp verify` fails with an actionable error rather than passing empty work.

```sh
make -C decomp CONFIG=targets/<target>/target.mk verify
python3 tools/matching.py ORIGINAL REBUILT --sha256 EXPECTED_ORIGINAL_SHA256
```

Original files and build output belong under ignored `.local/`; source/build
recipes belong under `decomp/`. Use the existing extraction/source profiles rather
than a new disc importer. Keep each overlay's identity with its addresses. Distinct
images sharing a load address must never collapse into one symbol space.

## Recover incrementally

Retain original assembly/data privately so unconverted callees can execute in the
original environment. Replace real functions with readable original-compatible C.
Use the upstream disassembler/decompiler/diff tools; consult machine instructions
for unresolved types, signedness, control flow and code-generation differences.
Build cohesive resident/overlay modules with small shared headers. No new giant
Program, parallel native rules model or per-helper ownership adapter is required.

The verifier first fingerprints the pristine input, rejects the same input/output
file, then checks every byte and length. Its output claims only binary agreement;
it does not infer source coverage. Keep the linker/build source map authoritative
for ranges still backed by original assembly, binary data or nonmatching source.
Measure source coverage and exact matching independently. Do not call a baseline
made entirely of original assembly a completed decompilation.

A normalized asm diff is a debugging aid, not final acceptance. Exact final image
comparison includes linked addresses and data/layout. Decoded overlay matching
and compressed-container reproduction are separate claims. Do not turn optical
filesystem/ECC reproduction into a prerequisite for recovering executable code.

## Evidence without paperwork

For a qualified exact match, retain source/build configuration and the comparison
result; a new elaborate finding and emulator capture are not additional mandatory
gates. Capture the original when it answers an unresolved semantic, format,
service or integration question. Do not rewrite expectations or hide uncertainty.

Existing original findings, captures and the host reconstruction remain valid only
within their recorded scope. They are especially useful for the later native port,
where pointer widths, arithmetic, memory layout and platform services change.
Use docs/executable-reconstruction.md only when working on that reference harness.
''')

write('docs/development.md', r'''
# Development

Phase 1 starts with [matching](matching.md), not a repository-wide document audit.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
```

For Ghidra, enter `path:./nix/ghidra` instead. Reuse qualified private projects and
source identities; importer usage is in [reverse engineering](reverse-engineering.md).

For the retained host C++ reference and its public regressions:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/build.py debug --target test-program --test connected-program
python3 tools/repository/check.py --preset debug
```

Use `--preset all` explicitly for debug/release/sanitizers. Formatting is an explicit
`python3 tools/repository/format.py --check` operation, not a reason to regenerate
requirements metadata. The source allowlist and JSON integrity are checked by
`tools/repository/validate.py`; historical evidence validators remain available to
the tests that exercise them. No generated requirements matrix is required.

Heavy reference captures/builds retain their host-wide concurrency locks. Do not
recapture a passing route per function or poll files in indefinite sleep loops.
Read only the current implementation, tests and evidence needed for the change.

Original images, extracted bytes and execution artifacts stay outside distributed
sources. Keep Nix path inputs restricted to their tool directories. Matching game
images needs the user's originals; public synthetic tests never claim game parity.
''')

write('docs/reverse-engineering.md', r'''
# Original-source analysis

[Matching-first recovery](matching.md) is the Phase 1 implementation workflow.
Ghidra remains the broad analysis environment; the matching shell is separate and
lightweight. Reuse existing qualified projects, exports, inferred types and findings.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra
python -m tools.analysis.ghidra_project \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/ghidra/new-disc1-analysis
```

The importer verifies original bytes and creates overlay-specific address spaces.
Preserve the resident/overlay identity alongside every address. Resume a qualified
working project instead of repeatedly importing the game. Ghidra paths cannot
contain hidden components; the importer handles its private temporary project.

Correct shared structures, calling conventions, GP context and dispatch boundaries
before re-decompiling dependent functions. Inferred names/types and automatic
pseudocode are not recovered source. Verify ambiguous signedness, delay slots,
fixed-point arithmetic, register forwarding, jump tables and hardware operations
against instructions. Keep unknown fields rather than inventing meanings.

Use m2c for matching-oriented C, spimdisasm for MIPS preparation, and the compiler
and exact image comparison for the primary feedback loop. A configured target
name does not prove the original compiler. Work on connected understanding, but
unconverted dependencies can remain original assembly in the PS1 build.

Original bytes, generated assembly, Ghidra projects and captures remain private.
Reviewed authored source goes under decomp/. Existing host C++ algorithms/tests
are references, not a compulsory rewrite destination. Record genuinely new source
knowledge concisely; do not require a capture or separate proof pipeline per helper.
''')

p = ROOT / 'docs/executable-reconstruction.md'
old = p.read_text()
start = old.index('## Memory-image comparison')
end = old.find('\n## ', start + 3)
technical = old[start:end if end != -1 else len(old)]
write('docs/executable-reconstruction.md', '''
# Host-reference validation

This is the retained host reconstruction harness, not the Phase 1 source-recovery
workflow or exit gate. Use matching.md for new PS1-target work. Preserve existing
C++ algorithms, regression tests and qualified captures; do not expand artificial
stack/service ownership solely to validate a function that can be binary-matched.

The library owns computed state. Runners provide explicitly qualified external
inputs and format observations; expected results must not reach the implementation.
State declarations now live in focused subsystem headers. Program remains the
host-reference integration owner, not a proposed original-target architecture.

Run focused cases only for changed reference behavior, unresolved questions or
native portability work. Prefer existing captures. Preserve failed/divergent
results, input lineage and the distinction between matching prefixes, completed
boundaries and a full original return. Historical findings keep their measured
scope. A host comparison does not imply a PS1 binary match or complete source.

''' + technical)

write('docs/phase1-progress.md', '''
# Phase 1 checkpoint

The goal is now the complete both-disc matching decomp in plan.md. No original-
compatible C compiler or whole-game matching build is claimed by this reorientation.
The new public MIPS fixture checks tooling only; source coverage is not inferred.

Next implementation: qualify representative compiler matches, establish incremental
resident/overlay targets, then replace original assembly continuously. Keep target
configuration and generated source/match counts authoritative rather than copying
hundreds of statuses into this file.

Existing host-reference recovery and original comparisons remain available in
analysis/findings/ and src/reconstruction/. EVID-REF-047 is the bounded menu/ownership
checkpoint at the reviewed starting commit 6ec589387649af12c719458daa4b3530c57ba3b4.
Its failures and limitations are not closed by the new plan. Earlier facet/phase
references describe the historical workflow, not the current decomp exit.
''')

removed = {
    'docs/requirements.md', 'docs/requirements.json', 'docs/requirement-facets.txt',
    'docs/requirement-review.json', 'docs/requirements-migration.json',
    'docs/traceability.json', 'docs/phase1-handoff.md', 'docs/phase1-slice-contract.md',
    'tools/repository/matrix.py', 'tools/repository/phase1_slice.py',
    'tests/test_phase1_slice.py',
}
for name in sorted(removed):
    p = ROOT / name
    if p.exists():
        p.unlink()

# Delete only tests of the retired matrix/slice bureaucracy; retain adversarial
# evidence, source-separation, schema, decoder and all game-behavior regressions.
obsolete = {'build_matrix', 'facet_specs', 'markdown', 'plan_sources', 'target_scope',
            'validate_plan_completion', 'validate_result', 'validate_traceability'}
for p in (ROOT / 'tests').glob('test_*.py'):
    text = p.read_text()
    tree = ast.parse(text)
    lines = text.splitlines(keepends=True)
    spans = []
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            if node.module == 'tools.repository.matrix':
                spans.append((node.lineno - 1, node.end_lineno, ''))
            elif node.module == 'tools.repository.validate':
                kept = [a for a in node.names if a.name not in obsolete]
                if len(kept) != len(node.names):
                    names = ', '.join(a.name + (' as ' + a.asname if a.asname else '') for a in kept)
                    replacement = f'from tools.repository.validate import {names}\n' if names else ''
                    spans.append((node.lineno - 1, node.end_lineno, replacement))
        if isinstance(node, ast.FunctionDef) and node.name.startswith('test_'):
            names = {n.id for n in ast.walk(node) if isinstance(n, ast.Name)}
            if names & obsolete:
                spans.append((node.lineno - 1, node.end_lineno, ''))
                print('Retire obsolete matrix test:', p.name, node.name)
    for a, b, value in sorted(spans, reverse=True):
        lines[a:b] = [value]
    p.write_text(''.join(lines))

# Keep original evidence/inventory validators and their tests, but remove the
# generated requirements orchestration and dependencies from the public gate.
p = ROOT / 'tools/repository/validate.py'
text = p.read_text()
tree = ast.parse(text)
keep_names = {'require', 'digest', 'unique', 'load', 'validate_finding',
              'validate_subsystem', 'validate_inventory', 'validate_observed_entrypoints'}
functions = {n.name: n for n in tree.body if isinstance(n, ast.FunctionDef)}
changed = True
while changed:
    changed = False
    for name in list(keep_names):
        if name not in functions:
            continue
        for n in ast.walk(functions[name]):
            if isinstance(n, ast.Name) and n.id in functions and n.id not in keep_names:
                keep_names.add(n.id)
                changed = True
parts = []
for n in tree.body:
    if isinstance(n, (ast.Assign, ast.AnnAssign)):
        names = {x.id for x in ast.walk(n) if isinstance(x, ast.Name)}
        if names & {'STAGES', 'CONFIDENCE', 'CATEGORIES'}:
            parts.append(ast.get_source_segment(text, n))
    elif isinstance(n, ast.FunctionDef) and n.name in keep_names:
        parts.append(ast.get_source_segment(text, n))
write('tools/repository/validate.py', '''
"""Small public integrity gate; source and tests, not generated requirement facets."""
from __future__ import annotations
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if __package__:
    from .source_archive import source_files
else:
    from source_archive import source_files

''' + '\n\n'.join(parts) + '''


def validate(root: Path = ROOT) -> None:
    files = source_files(root)
    for path in files:
        if path.suffix == '.json':
            json.loads(path.read_text())
    # Actual historical evidence remains tested by the focused validator tests.
    # Do not reinterpret its old phase/facet labels as the current checklist.
    print(f"Source boundary and JSON integrity passed: {len(files)} authored files")


if __name__ == '__main__':
    validate()
''')

write('tools/repository/check.py', '''
"""Build and test public code without generated requirements paperwork."""
from __future__ import annotations
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.reference.host_slots import slot  # noqa: E402


def run_all(commands: list[list[str]]) -> list[dict]:
    results = []
    for command in commands:
        print('Running: ' + ' '.join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, check=False)
        results.append({'command': command, 'exit_code': result.returncode})
        if result.returncode:
            raise SystemExit(result.returncode)
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', choices=['debug', 'sanitize', 'release', 'all'], default='debug')
    parser.add_argument('--format', action='store_true', help='also check repository formatting')
    args = parser.parse_args()
    commands = [[sys.executable, 'tools/repository/validate.py']]
    if args.format:
        commands.append([sys.executable, 'tools/repository/format.py', '--check'])
    presets = ['debug', 'sanitize', 'release'] if args.preset == 'all' else [args.preset]
    for preset in presets:
        commands += [['cmake', '--preset', preset], ['cmake', '--build', '--preset', preset],
                     ['ctest', '--preset', preset]]
    with slot('build'):
        run_all(commands)
    print('Public build and tests passed; no original-game match is claimed.')


if __name__ == '__main__':
    main()
''')

# Partition state representations, not behavior. No replacement rules, virtual
# dispatch, inheritance hierarchy or compatibility aliases are introduced.
p = ROOT / 'include/xem/reconstruction/program.hpp'
text = p.read_text()
class_at = text.index('\nclass Program {')
original_includes = text[:text.index('namespace xem::reconstruction {')]
original_includes = original_includes.replace('#pragma once\n', '').strip()
masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                lambda m: re.sub(r'[^\n]', ' ', m.group()), text, flags=re.S)

def declaration(name):
    match = re.search(r'(?m)^(?:struct|class) ' + re.escape(name) + r'\s*(?::[^\n{]+)?\{', masked[:class_at])
    if not match:
        raise RuntimeError('Missing state declaration: ' + name)
    start = match.start()
    opening = masked.index('{', match.start())
    depth = 1
    end = opening + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    while text[end].isspace():
        end += 1
    if text[end] != ';':
        raise RuntimeError('Missing declaration terminator: ' + name)
    end += 1
    # Include its adjacent source comments, but never the previous declaration.
    while start > 0:
        prev = text.rfind('\n', 0, max(0, start - 1)) + 1
        line = text[prev:start].strip()
        if line.startswith('//') or not line:
            start = prev
        else:
            break
    return start, end, text[start:end].strip()

groups = {
    'source_point.hpp': ['SourcePoint', 'MissingDependency'],
    'input_state.hpp': ['InputQueue', 'PadState'],
    'gpu_state.hpp': ['GpuState'],
    'interrupt_state.hpp': ['InterruptState'],
    'disc_state.hpp': ['DiscReadState', 'CdState', 'HardwareWrite', 'PartySpriteLoad'],
    'resident_state.hpp': ['ResidentState'],
    'field_state.hpp': ['FieldActor', 'ReloadState', 'FieldState'],
}
for candidate in ('MenuCallState', 'FrameCallAbi'):
    if re.search(r'(?m)^struct ' + candidate + r'\s*\{', masked[:class_at]):
        groups.setdefault('call_state.hpp', []).append(candidate)
owner = {name: file for file, names in groups.items() for name in names}
extracted = {name: declaration(name) for name in owner}
for filename, names in groups.items():
    body = '\n\n'.join(extracted[n][2] for n in sorted(names, key=lambda n: extracted[n][0]))
    deps = sorted({file for name, file in owner.items()
                   if file != filename and re.search(r'\b' + name + r'\b', body)})
    imports = original_includes + '\n' + '\n'.join('#include "xem/reconstruction/' + f + '"' for f in deps)
    write('include/xem/reconstruction/' + filename,
          '#pragma once\n\n' + imports + '\n\nnamespace xem::reconstruction {\n\n' + body + '\n\n} // namespace xem::reconstruction\n')
for start, end, _ in sorted(extracted.values(), reverse=True):
    text = text[:start] + text[end:]
text = text.replace('#pragma once', '#pragma once\n\n' + '\n'.join(
    '#include "xem/reconstruction/' + f + '"' for f in sorted(groups)), 1)
text = re.sub(r'\n{3,}', '\n\n', text)
p.write_text(text)

# Move autonomous actor projections out of the integration implementation.
p = ROOT / 'src/reconstruction/program.cpp'
text = p.read_text()
a = text.index('field::EventActor FieldActor::events()')
b = text.index('field::FieldSpriteEnvironment Program::sprite_environment()', a)
actor_methods = text[a:b].strip()
p.write_text(text[:a] + text[b:])
write('src/reconstruction/field_state.cpp', '#include "xem/reconstruction/field_state.hpp"\n\nnamespace xem::reconstruction {\n\n' + actor_methods + '\n\n} // namespace xem::reconstruction')
replace('CMakeLists.txt', '  src/reconstruction/program.cpp\n', '  src/reconstruction/program.cpp\n  src/reconstruction/field_state.cpp\n')
p = ROOT / 'CMakeLists.txt'
text = p.read_text()
# Retire only the removed slice-contract test registration when it is standalone.
text = re.sub(r'(?m)^.*tests\.test_phase1_slice.*\n', '', text)
headers = ' '.join(f.removesuffix('.hpp') for f in sorted(groups))
text += '''

# Every extracted state header must compile independently, without Program.
if(BUILD_TESTING)
  set(state_header_sources)
  foreach(header ''' + headers + ''')
    set(unit "${CMAKE_CURRENT_BINARY_DIR}/state-headers/${header}.cpp")
    file(GENERATE OUTPUT "${unit}" CONTENT "#include \\"xem/reconstruction/${header}.hpp\\"\\n")
    list(APPEND state_header_sources "${unit}")
  endforeach()
  add_library(test-state-headers OBJECT ${state_header_sources})
  target_link_libraries(test-state-headers PRIVATE xem-reconstruction)
  add_test(NAME matching-verifier COMMAND "${Python3_EXECUTABLE}" -m unittest tests.test_matching)
  set_tests_properties(matching-verifier PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
'''.replace('\\\\"', '\\"').replace('\\\\n', '\\n')
p.write_text(text)

# Reuse the existing pinned matching tools. Add MIPS binutils without bringing
# Ghidra/Java into the compile-and-diff shell or inventing a compiler identity.
p = ROOT / 'nix/ghidra/flake.nix'
text = p.read_text()
anchor = '      base = pkgs.mkShell {'
addition = '''      psxBinutils =
        let
          cross = pkgs.pkgsCross.mipsel-linux-gnu;
          tools = cross.buildPackages.binutils;
          prefix = "${cross.stdenv.hostPlatform.config}-";
        in
        pkgs.runCommand "xem-psx-binutils" { } ''
          mkdir -p "$out/bin"
          for tool in as ld objcopy objdump readelf nm size; do
            test -x "${tools}/bin/${prefix}$tool"
            ln -s "${tools}/bin/${prefix}$tool" "$out/bin/psx-$tool"
          done
        '';
'''
if anchor not in text:
    raise RuntimeError('Missing Nix tools anchor')
text = text.replace(anchor, addition + anchor, 1)
text = text.replace('inherit ghidra m2c spimdisasm;', 'inherit ghidra m2c spimdisasm psxBinutils;')
old = '''        matching = base.overrideAttrs (previous: {
          nativeBuildInputs = previous.nativeBuildInputs ++ [
            m2c
            spimdisasm
          ];
        });'''
new = '''        matching = pkgs.mkShell {
          packages = [ m2c spimdisasm psxBinutils pkgs.gnumake pkgs.diffutils pkgs.git pkgs.python3 ];
          shellHook = ''
            export PYTHONDONTWRITEBYTECODE=1
            export SOURCE_DATE_EPOCH=0
          '';
        };'''
if old not in text:
    raise RuntimeError('Missing matching-shell anchor')
p.write_text(text.replace(old, new))

write('tools/matching.py', r'''
"""Fingerprint the pristine input and compare complete rebuilt bytes, without masks."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def compare(original: Path, rebuilt: Path, expected_sha256: str) -> dict:
    if not re.fullmatch(r'[0-9a-fA-F]{64}', expected_sha256):
        raise ValueError('Expected original SHA256 must contain exactly 64 hexadecimal digits')
    if original.samefile(rebuilt):
        raise ValueError('Original and rebuilt must be distinct files')
    expected = original.read_bytes()
    actual = rebuilt.read_bytes()
    original_hash = hashlib.sha256(expected).hexdigest()
    if original_hash != expected_sha256.lower():
        raise ValueError('Pristine original fingerprint mismatch; do not update expectations to pass')
    first = next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b), None)
    if first is None and len(expected) != len(actual):
        first = min(len(expected), len(actual))
    return {
        'matched': expected == actual,
        'original_sha256': original_hash,
        'rebuilt_sha256': hashlib.sha256(actual).hexdigest(),
        'original_bytes': len(expected),
        'rebuilt_bytes': len(actual),
        'first_difference': first,
        'claim': 'binary_agreement_only',
        'source_coverage': 'not_measured',
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('rebuilt', type=Path)
    parser.add_argument('--sha256', required=True, help='qualified pristine original digest')
    args = parser.parse_args(argv)
    try:
        result = compare(args.original, args.rebuilt, args.sha256)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0 if result['matched'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
''')

write('tests/test_matching.py', r'''
"""Exact comparison failures and an authored MIPS-I assemble/link smoke test."""
from __future__ import annotations
import hashlib
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from tools.matching import compare, main


class MatchingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.original = self.root / 'original.bin'
        self.rebuilt = self.root / 'rebuilt.bin'
        self.original.write_bytes(b'\x01\x02\x03\x04')
        self.rebuilt.write_bytes(self.original.read_bytes())
        self.digest = hashlib.sha256(self.original.read_bytes()).hexdigest()

    def test_equal_is_only_binary_agreement(self):
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertTrue(result['matched'])
        self.assertEqual(result['source_coverage'], 'not_measured')
        self.assertIsNone(result['first_difference'])

    def test_one_byte_difference_is_not_masked(self):
        self.rebuilt.write_bytes(b'\x01\x02\x00\x04')
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertFalse(result['matched'])
        self.assertEqual(result['first_difference'], 2)

    def test_both_length_mismatches_fail(self):
        for data in (b'\x01\x02', b'\x01\x02\x03\x04\x00'):
            with self.subTest(data=data):
                self.rebuilt.write_bytes(data)
                result = compare(self.original, self.rebuilt, self.digest)
                self.assertFalse(result['matched'])
                self.assertEqual(result['first_difference'], min(4, len(data)))

    def test_wrong_original_rejects_even_equal_outputs(self):
        with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
            compare(self.original, self.rebuilt, '0' * 64)

    def test_same_file_and_hardlink_reject(self):
        with self.assertRaisesRegex(ValueError, 'distinct'):
            compare(self.original, self.original, self.digest)
        self.rebuilt.unlink()
        self.rebuilt.hardlink_to(self.original)
        with self.assertRaisesRegex(ValueError, 'distinct'):
            compare(self.original, self.rebuilt, self.digest)

    def test_invalid_hash_and_missing_input_reject(self):
        with self.assertRaises(ValueError):
            compare(self.original, self.rebuilt, 'not-a-hash')
        self.rebuilt.unlink()
        with self.assertRaises(OSError):
            compare(self.original, self.rebuilt, self.digest)

    def test_cli_exit_codes(self):
        args = [str(self.original), str(self.rebuilt), '--sha256', self.digest]
        self.assertEqual(main(args), 0)
        self.rebuilt.write_bytes(b'wrong')
        self.assertEqual(main(args), 1)
        self.rebuilt.unlink()
        self.assertEqual(main(args), 2)

    @unittest.skipUnless(shutil.which('psx-as') and shutil.which('psx-ld') and shutil.which('psx-objcopy'),
                         'enter the matching Nix shell to test MIPS binutils')
    def test_mips_assemble_link_and_exact_bytes(self):
        assembly = self.root / 'fixture.s'
        linker = self.root / 'fixture.ld'
        obj = self.root / 'fixture.o'
        elf = self.root / 'fixture.elf'
        assembly.write_text('.section .text,"ax"\n.set noreorder\n.globl fixture\nfixture:\n'
                            'addiu $2,$0,7\njr $31\nnop\n')
        linker.write_text('SECTIONS { .text 0x80010000 : { *(.text) } '
                          '/DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) } }\n')
        subprocess.run(['psx-as', '-EL', '-mips1', '-mabi=32', '-G0', '-o', str(obj), str(assembly)], check=True)
        subprocess.run(['psx-ld', '-EL', '-T', str(linker), '-o', str(elf), str(obj)], check=True)
        subprocess.run(['psx-objcopy', '-O', 'binary', '-j', '.text', str(elf), str(self.rebuilt)], check=True)
        self.original.write_bytes(struct.pack('<4I', 0x24020007, 0x03e00008, 0, 0))
        digest = hashlib.sha256(self.original.read_bytes()).hexdigest()
        self.assertTrue(compare(self.original, self.rebuilt, digest)['matched'])


if __name__ == '__main__':
    unittest.main()
''')

write('decomp/README.md', '''
# Original-target source

This directory owns new PS1-target recovery. Group source and small shared headers
by resident/overlay subsystem. Keep the existing host reconstruction separate as
reference/portability code; do not duplicate its Program abstraction here.

Read ../docs/matching.md. No Xenogears target/compiler is claimed qualified yet.
Add a target make fragment with its exact source identity, object list, linker
script and original-compatible compile rules. Unconverted original assembly/data
remain in ignored .local/ and are explicitly unfinished source recovery.

`make smoke` runs the authored MIPS toolchain fixture. `make CONFIG=... verify`
links the declared objects and performs exact, fingerprinted binary comparison.
Neither command infers source coverage. Track remaining source/assembly ranges
from actual target inputs and linker maps, not from another handwritten dashboard.
''')
write('decomp/Makefile', r'''
# Explicit per-target recipes; no implicit host compiler or invented game image.
ROOT := $(abspath ..)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
.DEFAULT_GOAL := verify
.PHONY: verify smoke

smoke:
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify:
	@echo 'Set CONFIG=targets/<target>/target.mk after qualifying the original toolchain.' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE OBJECTS LINKER_SCRIPT,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

$(IMAGE): $(OBJECTS) $(LINKER_SCRIPT)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $(LINKER_SCRIPT) -Map $@.map -o $@.elf $(OBJECTS)
	$(OBJCOPY) -O binary $@.elf $@

verify: $(IMAGE)
	$(PYTHON) $(ROOT)/tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)
endif
'''.replace('\\t', '\t'))

# C and linker/build files are authored source types, never extracted binaries.
replace('tools/repository/source_archive.py', '    ".cpp",', '    ".cpp",\n    ".c",\n    ".ld",\n    ".mk",')
replace('tools/repository/source_archive.py', 'ALLOWED_BASENAMES = {"LICENSE",', 'ALLOWED_BASENAMES = {"Makefile", "LICENSE",')

# Fix live documentation links, but leave historical JSON findings untouched.
link_replacements = {
    'phase1-handoff.md': 'matching.md',
    'phase1-slice-contract.md': 'matching.md',
    'requirements.md': 'matching.md',
}
for p in (ROOT / 'docs').rglob('*.md'):
    if p.name == 'matching.md':
        continue
    text = p.read_text()
    for old, new in link_replacements.items():
        text = text.replace(old, new)
    p.write_text(text)

# Preserve the explicit allowlist, removing deleted projections and adding only
# the reviewed files created by this refactor. Do not glob private working trees.
p = ROOT / 'packaging/source-files.txt'
names = {n for n in p.read_text().splitlines() if n and not n.startswith('#')}
names = {n for n in names if (ROOT / n).is_file() and n not in removed}
names |= NEW
p.write_text('\n'.join(sorted(names)) + '\n')

# No one-shot execution machinery belongs in the final source tree.
print('Refactor written. New subsystem headers:', ', '.join(sorted(groups)))
print('Retired duplicated resources:', ', '.join(sorted(removed)))
subprocess.run(['git', 'diff', '--stat'], check=True)
