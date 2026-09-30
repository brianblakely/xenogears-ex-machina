# Agent authoring contract

This is retained Phase 0 design and dependency qualification, not an implemented
SDK. No authoring command, isolated build service or playable authored world is
claimed by specification tests. Native runtime and original-game play come first;
**Phase 7 owns the authoring bridge**. The [plan](../../plan.md) is authoritative.
Legacy phase/facet labels in the retained JSON describe the earlier specification,
not additional Phase 1 requirements or current implementation completion.

Consult the [machine-readable contract](contract.json), [acceptance cases](acceptance.json)
and their referenced schemas when implementing a specific capability. Exact tool
versions belong in [package.json](../../tools/authoring/package.json), its lock and
`nix/authoring/`, not a second manually synchronized version table. The existing
[dependency record](dependencies.json) preserves the Phase 0 qualification.

## Source-to-native pipeline

TypeScript, explicit parameters, Lua and source assets are authoritative. Optional
TSX is composition, not a UI lifecycle. A supervised Node worker type-checks,
transpiles and executes source; normalizes project-owned geometry/world/collision/
event data; validates; and exports immutable native packages. IR and generated GLBs
are derived outputs, not competing editable sources.

Manifold solids, open surfaces, arbitrary meshes and optional imported assets have
explicit adapters and limits. Three.js geometry helpers are not a runtime scene
graph. Keep coordinate conventions, transforms, winding, normals, bounds and
source attribution explicit. Conversion to the recovered game's units and
collision representation needs qualified rules, not an assumed scale factor.

The C++ loader validates the complete package before publishing it: identities,
paths, hashes, versions, supported materials/extensions, buffer bounds, finite
values, collision, references, spawns and event capabilities. A valid GLB alone
is not a valid world. Loading without images must not allocate GPU resources.

Native C++ systems, explicit serializable events and permitted Lua execute gameplay.
A prebuilt package requires no Node, browser, QML, authoring tool or second game
engine. Logical state, time, collision and rewards use the same runtime as the
original game, agents, tests and editor previews.

## Shared editing and isolation

Expose discoverable types, exact IDs, examples, supported operations and explicit
unsupported states. Use revision-aware transactional source edits, stable IDs and
source diagnostics. Preserve components, handwritten code and unrelated human
changes; do not promise arbitrary source-code round-tripping.

Generators and dependency installation are executable code. Enforce OS/process
filesystem, network and credential restrictions, resource budgets and watchdogs.
Node vm and TypeScript typing are not isolation boundaries. Untrusted generators
and mods receive no privileged runtime debugging access. Stage outputs separately,
publish successful immutable packages atomically and retain the last working build
on failure. Pin playtests to exact package revisions.

## Implement and prove the small bridge

Start with ordinary TypeScript defining a wall, usable door and collectible. Then
build two traversable rooms with an arch, elevation change, a generated custom
object, NPC/dialogue and a one-time reward. Verify generated-only and mixed-asset
variants through the real native loader and legal gameplay commands.

Have an agent discover, author, build, play, diagnose a known failure, repair it,
make a targeted revision and deliver source plus an immutable package. Test actual
collision/clearance, transitions, reward-once behavior, persistence and leave/re-enter
semantics under stepping and unlocked execution. Debug setup is not legal play,
and author edits cannot alter protected acceptance criteria to manufacture a pass.
Retain reproduction inputs, results and source-aware diagnostics. Broader SDK,
Lua/hot-reload work and graphical clients follow the owning tasks in `plan.md`.
