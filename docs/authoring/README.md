# Agent authoring contract 0.1

This is the Phase 0 design and dependency qualification, not an implemented SDK.
No `xem-content` command, source editor, isolated build service, authored-package
loader or playable authored world exists yet. The [machine-readable contract](contract.json)
and [acceptance gates](acceptance.json) specify what Phases 2A–3 and 7 must prove.
Passing the repository tests proves specification integrity only. The independent
original-game evidence and [native-agent contract](../agent/README.md) still govern
the underlying game; generated examples cannot supply an original-behavior oracle.

## Selected stack and separately reviewed dependencies

The build-only project is [tools/authoring/package.json](../../tools/authoring/package.json),
with exact direct versions and a complete npm lock, including optional platform
packages. The [dependency review](dependencies.json) binds every resolved package
to its integrity, license evidence, executable payload and installation scripts.
The central [dependency record](../dependencies.json) indexes this separate review
and the independently pinned future native importer. Neither authoring packages
nor their transitive libraries become shipping runtime dependencies by selection.

| Layer | Pin | Decision and boundary |
|---|---|---|
| Node.js / npm | Node 24.19.0 and npm 11.17.0 from `nix/authoring/flake.lock` | Supported LTS line; a separately entered `nix/authoring` shell. No Node in the default build or installed game. |
| TypeScript | 7.0.2 | `tsc --noEmit`, strict types and explicit module configuration; its platform compiler binaries are build tools. |
| esbuild | 0.28.2 | Transpile/bundle after type checking; native platform binaries stay in the build worker. |
| Manifold | `manifold-3d` 3.5.3 | WASM solid primitives, booleans, extrusion and revolution; explicitly free kernel allocations. |
| Three.js | `three` and `@types/three` 0.186.0 | Geometry utilities and typed buffers only; renderer, scene graph, animation loop and physics are not authoritative game systems. |
| glTF Transform | `@gltf-transform/core` 4.5.0 | Deterministic supported-subset GLB serialization; its document is a render export, never gameplay state. |
| Zod | 4.6.2 | Strict JSON parameter validation and JSON Schema export; no silent coercion or dropping unknown fields. |
| Native glTF parser | cgltf v1.15, exact commit/header hash in the central record | Selected for the future C++ bridge; unlinked and not vendored at Phase 0. Loader hardening and runtime adoption remain gates. |
| Deferred infrastructure | SQLite/FTS5 and measured meshoptimizer integration | Catalog/optimization are later optional work. A transitive package or Node-bundled SQLite does not enable an SDK/runtime capability. |

Node's [release schedule](https://nodejs.org/en/about/previous-releases) identifies
the supported LTS lines. [esbuild documents](https://esbuild.github.io/content-types/#typescript)
that transpilation does not type-check. [Manifold](https://github.com/elalish/manifold)
is a solid-mesh kernel; open surfaces must use a different adapter.
[Zod's JSON Schema conversion](https://zod.dev/json-schema) can reject types with
no JSON representation; keep that behavior instead of replacing them with `{}`.

`manifold-3d` itself brings glTF extensions/functions, an esbuild-WASM peer,
archive/XML helpers and mesh optimization dependencies. Three's type package
brings additional helper/physics declarations and WASM. These are explicitly
locked and reviewed; their presence does not authorize imports or promise their
features. Start from the direct core geometry imports. Do not suppress transitive
review, automatically enable every glTF extension, or install a second gameplay
engine because an upstream package happens to include one.

## Source, build, package and runtime ownership

Authoritative inputs are hand-edited `.ts` files, explicit JSON parameters, Lua
modules and source assets. Optional TSX comes only after function-based parity;
its custom JSX factory returns serializable definitions and has no UI lifecycle.
Project files declare namespaced stable IDs. File moves, display-name changes,
mesh re-triangulation and parameter changes must not silently rename entities,
components, anchors, semantic surface regions or operations.

The project owns the SDK, normalized mesh data, world/collision/event IR, schema
versions, build orchestration, source maps, package writer and C++ bridge. Library
types terminate at adapters. No exported Three.js object, Manifold handle, JS
closure or host path is native game state. Runtime systems execute ordinary
movement/collision/interaction, serializable native event instructions and the
explicitly permitted Lua API. No generated C++, embedded JavaScript engine,
per-frame build callback, browser or editor toolkit is needed to play a package.

Build stages have separate diagnostics and identities: resolve an immutable input
revision; validate parameters; type-check; transpile; execute in isolation;
normalize; validate world/collision/events; export; verify package; atomically
publish. A later stage never treats a failed previous stage as an empty success.
Temporary files and partial packages cannot replace the last working artifact.
The build graph records transitive sources/assets, settings, seeds, toolchain
closure, importer/compiler versions and schema IDs. Cache keys use all of them.
Do not promise cross-platform byte identity from a seed alone: qualify each target,
or report a build compatibility class and retain the exact tested artifact.

The first package consists of GLBs for render meshes and versioned JSON world,
collision, event and dependency manifests. It includes content hashes, source
revision, source maps, required engine/schema versions, resource budgets and
local base-asset IDs/hashes. Gameplay/collision never comes from guessed glTF
`extras`. Prebuilt runtime installation includes only supported compiled data,
explicit Lua, required notices and distribution-permitted assets. Editable source
is a separate deliverable. Original extracted assets resolve from the immutable
local original store and are never copied into a distributable mod by default.

The C++ bridge must check the entire package before publication: manifest/path
bounds; hashes and compatibility; material/extension allowlist; buffer lengths,
strides, indices and finite values; world references; collision compilation;
safe spawns; event signatures and capabilities. It stages entities, collision,
resources and tasks, then commits or rejects the whole candidate. A valid GLB or
successful cgltf parse is insufficient. Logical headless loading must skip GPU
allocation while using the same native entity, collision and event services.

## Geometry and behavior rules

Authoring uses right-handed metres, +Y up, +X right and forward -Z, radians, and
column-vector transforms with local scale then rotation then translation. Indices
use outward counter-clockwise winding. A versioned adapter maps Manifold's local
Z-up operations with a proper rotation; glTF shares the authoring basis. Negative
scale requires a winding/normal correction or an explicit rejection. Normals use
the inverse-transpose for nonuniform scale. Bounds derive from transformed data.
Conversion to recovered native collision units, axis conventions and fixed-point
precision is a separate evidence-qualified mapping; no guessed PS1 unit ratio is
approved here. Out-of-range or precision-losing conversion fails diagnostically.

Normalized project-owned meshes carry positions, indices, normals, UVs, optional
vertex colors, material groups, bounds and source/object attribution. Separate
solid, open-surface, arbitrary-mesh and imported-model kinds preserve their real
preconditions. Non-manifold boolean operands cannot be coerced into valid solids
without an explicit authored repair operation. Degenerate/nonfinite triangles,
inconsistent attributes, excessive topology and invalid material assignments fail
before export. Capabilities describe both supported operations and known limits.

Collision is explicit: supported static surfaces/triangles, composed primitives,
separate simplified geometry, or none. It is compiled to the actual native
movement representation. An arch needs an open traversable collider; a bounding
box or convex hull sealing the opening fails the traversal gate. Persistent anchors
and doorway constraints record solved transforms, adjacency and clearance.
Library success is build validity; legal native movement proves collision validity;
native captures and human/agent review assess presentation separately.

TypeScript event helpers produce closed instruction/condition records. Targets,
waits, timeouts, cancellation and reward ownership are explicit and serializable.
No arbitrary callback can masquerade as a declarative instruction. Discovery must
reject unavailable battle/quest/minigame operations even when a named resource
exists. Native tasks hold progress, one-time rewards and persistent mod data; Lua
extensions join the same introspection, scheduler, snapshot and permission model.
Node, GPU animation and wall clocks cannot own gameplay timers or completion.

## Discovery, editing and reproducible verification

A local versioned JSON-RPC/CLI surface will expose capability/schema discovery,
exact ID catalogs, examples, semantic edits, validation/build, scenario launch,
queries/traces/captures, diffs, tests and packaging. MCP is an optional adapter.
The Phase 0 baseline exposes none of those authoring methods yet. Every advertised
capability needs an implementation state: verified, partial, opaque-preserved or
unsupported, with version and limits. Original lore/evidence and creative proposals
are separately attributed. SQLite indexing is rebuildable metadata, never truth.

Edits use an expected source revision, transaction ID and ordered semantic changes.
Dry runs return the exact diff; commit validates cross-file constraints atomically;
duplicate transaction IDs return the original result; stale revisions reject with
the current revision. Undo restores an identified source transaction. Direct
file edits go through the same build pipeline. Editors may change exposed JSON
parameters or make reviewable source patches, preserving unrelated custom code.
No arbitrary TypeScript round-trip or rewrite from generated manifests is promised.

Source revision, input graph hash, package hash, native session ID and snapshot ID
are distinct. A test session pins an immutable package and a separately protected
acceptance/harness revision. Subsequent source changes cannot modify that session.
Structured diagnostics contain code, stage, severity, source span, object/operation
IDs, expected/observed values and a reproducible scenario. Missing source attribution
is an explicit diagnostic limit, not a fabricated location.

The [early gates](acceptance.json) require both a minimal wall/door/pickup and a
two-room environment with an arch, elevation, custom object, dialogue and a one-time
reward. Generate that geometry from source without imports. Test a separate variant
with a redistributable model, and local original references when qualified. Walk
through real collision and eligibility checks using native actions. Test closed
and open doors, blocked approaches, repeated interactions, leave/re-enter, ordinary
save/load and exact snapshots in stepped and unthrottled modes. Optional images
must come from the native renderer and cannot alter authoritative results.

Supply an intentionally blocked-arch revision. The agent must diagnose its source
parameter, repair it, pass unchanged protected tests, then widen the arch while
preserving unrelated objects and stable IDs. Deliver the source, focused diff,
immutable installable package, exact run/replay artifacts and optional captures.
Initial privileged setup is logged separately. Any later corrective teleport,
flag/reward write, harness edit or test weakening disqualifies the ordinary-play
result. `passed`, `failed`, `timed_out`, `cancelled` and `unsupported` are distinct.

## Untrusted-build policy

Build input and dependency installation are executable code. Phase 0 specifies
the following required policy; OS isolation enforcement is an unimplemented
Phase 2A gate. Until enforcement passes on a host, the future service must return
`isolation_unavailable` for untrusted projects. An explicit trusted developer
qualification of repository-owned fixtures is not evidence that untrusted input
is safe. Neither [Node `vm`](https://nodejs.org/api/vm.html) nor type checking is
a security boundary.

1. A trusted supervisor creates an immutable source snapshot and dependency
   closure. Dependency acquisition is a separate operation with registry allowlist,
   exact lock/integrity checks, audit trail and no project credentials. Ordinary
   builds are offline. Disable lifecycle scripts during acquisition/installation;
   inspect any required script and grant it a separate scoped reviewed recipe.
2. Run type checking, transpilation and source generation inside an OS sandbox
   with private process/temp namespaces, read-only source/tool mounts and a new
   bounded output directory. Expose only declared assets. No host home, credential
   store, environment secrets, SSH/agent sockets, container daemon, runtime control
   sockets, display/GPU devices, inherited file descriptors or original disc tree.
   Mount only the referenced original inputs, locally and read-only, when needed.
3. Deny outbound, inbound and loopback networking and process escape. Use process
   tree limits, memory/CPU/file/open-descriptor limits and an independent supervisor
   watchdog. Cancellation kills descendants, closes pipes and discards candidate
   output. Baseline limits are explicit in contract.json and may only be changed
   by host policy; content code cannot raise them. Linux namespaces/cgroups with a
   reviewed launcher are the first candidate; Windows/macOS need separate proven
   isolation or a configured disposable VM. Never fall back silently to plain Node.
4. Protect compiler/SDK/schema/acceptance files outside writable mounts. Worker
   errors/logs are bounded untrusted data. The supervisor validates outputs again,
   rejects path traversal, links, extra files, external URIs, unsupported executable
   payloads and excessive expansion before atomic publication. Caches are scoped
   by trust/input identity and never grant a worker write access to shared tools.
5. Untrusted generator/Lua code receives no agent-control/debug lease. Runtime
   scenario execution is a separate process granted the immutable package and
   explicit test authority by the supervisor. Package permission declarations
   cannot grant themselves filesystem, network, subprocess or debug privileges.

The attack gate covers source and dependency hooks, symlink/hardlink/path escape,
network and socket access, inherited credentials/descriptors, fork/memory/disk/log
exhaustion, stale/cache poisoning, cancellation, output swapping and test tampering.
A policy file passing schema tests is not a sandbox penetration test.

## Maintenance and Phase 0 verification

The public repository gate checks contract identity, stage/boundary completeness,
locked/reviewed dependency consistency and protected future gate coverage with
negative fixtures. It never installs packages or starts Node. Optional dependency
qualification uses the separate pinned shell and repository-owned geometry fixture,
with lifecycle scripts disabled. It proves only library/type/export compatibility
on the recorded host, not native loading, playability, sandbox enforcement or
Windows/macOS support. See [development instructions](../development.md).

An update must re-review source/version/license and every changed transitive
package, integrity, native/WASM payload and install hook; run the compatibility
fixture and affected gates. Preserve older executed evidence by source identity.
Do not regenerate review hashes merely to hide drift, and do not turn a defined
native gate into a pass because its documentation now exists.
