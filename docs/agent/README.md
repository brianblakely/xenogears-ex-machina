# Native agent contract 0.1

This is the Phase 0 architecture specification. `xem-baseline` still implements
only help/version; no native game, agent server, debug service or spectator is
implemented. The schemas and public tests below validate this contract, not
gameplay. Phase 2 implements it and Phase 3 must demonstrate it before expansion.

The [method catalog](methods.json), [wire schema](protocol.schema.json),
[state/query schema](state.schema.json), [scenario schema](scenario.schema.json),
[examples](examples.json), [acceptance gates](acceptance.json) and
[emulator parity table](emulator-parity.json) are versioned parts of this contract.
The authored method/field vocabulary is native architecture, not a claim about
unrecovered original structures. Original semantic correlations still require
the [evidence workflow](../evidence-workflow.md).

## One engine and its clients

The native engine owns simulation, recovered event execution, authoritative
state, asset initialization, persistence and validated commands. Physical input,
agent control, tests and editor previews call the same command handlers. They
cannot advance separate simulations or substitute an emulator or mock game.
Input translation stops at the engine boundary: no synthetic OS/SDL events,
virtual devices, window focus, OCR or screenshot-based control is required.

The logical process initializes no window, display connection, GPU, physical
input or audio device in image-free mode. Do not merely hide a window or select
a dummy display driver. Device-backed clients are optional services. Asset jobs
may wait for data, but a deterministic load barrier and resource-ID order decide
when that data enters authoritative state. Host completion order never decides
gameplay order. Logical music, dialogue and FMV timelines continue without output.

The first native executable will expose `xem --headless --agent-stdio`; this is a
reserved future invocation, not a working baseline command. It starts a local
protocol connection in the ordinary `control` role with no images. The session
creation request must explicitly select its speed. Trusted debug requires an
explicit host launch option `--allow-debug`; unsafe diagnostics additionally
require `--allow-unsafe-debug`. A request or mod cannot grant itself either role.
Noninteractive asset import/configuration must precede play with structured
errors, isolated save roots and no GUI dialog. Renderer startup is requested
separately and reports unavailable devices without terminating logical control.

## Wire, discovery and lifecycle

Use UTF-8 newline-delimited JSON on inherited stdin/stdout, one compact JSON
object per LF, no BOM. Protocol traffic alone goes to stdout; human diagnostics
go to stderr. JSON numbers must be finite; duplicate object keys are invalid.
The host enforces a 1 MiB line limit, nesting depth 32, 64 admitted operations per
connection and the collection bounds in the schemas before allocation/work.
An overlong or undecodable line produces `invalid_request` (null request ID if
unrecoverable) and closes that connection; malformed bounded JSON is rejected
without mutation. EOF releases ownership as described below. No terminal or
network account is required. Optional IPC uses the same envelopes and semantics.

`protocol.hello` is first. It offers exact supported versions; the server chooses
the highest common version or returns `unsupported_version` and closes. Version
0.1 requires an exact match: incompatible method, unit, field or scheduling
changes increment the protocol version. Schema IDs and content/schema hashes
identify the selected contract. Later minor-version compatibility must be
explicitly negotiated; clients must not silently accept changed meaning.

Each request has `version`, unique `id`, `method`, `params`, and a nullable
`session_id`. Hello/discovery/create have no session; other methods require a
live session visible to that connection. Resource IDs are namespaced semantic
strings, not pointers, original RAM offsets or guessed scene names. A live entity
ID includes its generation and is never reused within a session. Persistent asset
IDs remain stable across reimports of the same source; catalogs disclose numeric
original selectors and source provenance separately.

Discovery returns available methods, their parameter/result schema references,
roles, capability status, limits, state-domain schemas, actions, scenario entry
types, semantic predicates and the recovered timebase. Every capability declares
`unimplemented`, `available` or `unavailable` with a reason and scope. Defined
schemas are not executable capabilities. A subsystem may advertise only recovered
and implemented content; asking for missing functionality returns
`unsupported_capability`, never an empty successful simulation. Discovery remains
usable before an asset/session exists.

Every request gets one terminal `completed` result or structured `error`.
Long operations may first emit `accepted` with an operation ID; this admits work
and does not assert it happened. The terminal result retains the request ID and
reports actual applied ticks, stop reason and artifacts. Streaming events use
their own monotonic sequence and subscription ID. Duplicate request IDs on a
connection are rejected with `duplicate_request` and never execute again; clients
use operation status/event history to resolve lost acknowledgments, not blind
retry of mutations. Disconnect does not undo committed work. Reconnection through
an authenticated local IPC connection can discover authorized live sessions and
read retained operations/events; expired history produces `history_lost`.

`schema.read` supplies discovered schema bytes in bounded UTF-8 chunks, retaining
one SHA-256 identity; chunk boundaries never split a UTF-8 character and offsets
count encoded bytes. `artifact.read` supplies authorized session artifacts in
bounded base64 chunks with byte offsets, total size and whole-artifact hash.
Neither method accepts a host path. Clients reconstruct and verify the digest.
Schema IDs used by discovery resolve to published schema documents or fragments.
Stream images use event envelopes with the registered payload schema
`protocol.schema.json#/$defs/imageEventPayload`; artifact IDs retrieve their bytes.
Dropped/expired image artifacts return `stale_id`, allowing a viewer to request the
latest image without affecting simulation. `state.unsubscribe` and `state.release`
free observation resources without changing the game.

State, command and readiness failures carry stable error codes, a message,
parameter/query path, retriable flag and current state token when available.
Required codes include malformed/version/method/capability errors, permission and
ownership errors, stale state/cursor/ID, invalid action/setup, incompatible
snapshot/source, resource limit, cancellation, watchdog and internal failure.
No raw implementation exception is the public error contract.

## Simulation time and deterministic control

`tick` and counters are unsigned 64-bit decimal strings, avoiding floating-point
or JavaScript integer loss. Tick 0 is the fully initialized scenario boundary.
A session declares an immutable, evidence-qualified `timebase_id` and positive
rational `seconds_per_tick`. Phase 0 does **not** assume 60 Hz, assign an original
cadence or equate `retro_run` calls with native ticks. Before implementing a
gameplay slice, recover a base quantum that can represent its required subsystem
cadences; store rational/integer accumulators for subrates. Reject a scenario if
its timing is unrecovered. Changing the timebase invalidates incompatible replays
and snapshots and requires a new schema/content compatibility decision.

At boundary N, tick N is complete. Actions scheduled at N+1 are validated and
applied before that tick's work. Then deterministic engine phases run, required
scripts/media/events update, watchpoints/conditions are evaluated, and the engine
publishes boundary N+1. Recovered intra-tick order is part of the timebase/runtime
contract and replay identity; do not invent its game-specific details here.
An acknowledgment names the exact `applied_tick`. No action targets a completed
tick. A request without a valid state guard never races against the latest state.

Every state token contains session, epoch, tick, revision and canonical hash.
Tick alone is insufficient: paused edits change revision, and reset/restore
increment epoch, invalidating queued actions, cursors and old guards. Restoring
may set tick back to a saved value. Epoch/session/revision are coordination
metadata outside the canonical game hash; equal game states in different sessions
can compare equal. Tick, held input and pending deterministic commands are part
of exact replay state, with transport request IDs excluded.

Execution modes are `paused`, `realtime`, `fixed` (positive rational multiplier)
and `unthrottled`. Exact advance and atomic cycles run only from paused mode and
return paused. A mode change settles at a complete tick boundary. Real-time/fixed
mode paces against wall time; falling behind reports lag and runs every required
tick, without dropping collision, script, battle, RNG or media work. Unthrottled
removes artificial sleeps and frame caps. It does not enlarge timesteps, silently
skip cutscenes, force choices or promise a fixed acceleration factor. Rendering,
VSync, audio clocks, video output and viewers never own this clock.

`simulation.advance` requests either an exact positive tick count or `until` a
typed condition, bounded by `max_ticks`, `max_work_units` and `watchdog_ms`.
`control.cycle` atomically checks one observed state, admits a same-next-tick
action batch, advances and queries the resulting immutable snapshot while holding
the controller lease. Competing mutation cannot interleave. A predicate already
true at admission returns with zero ticks; other predicates are evaluated at
each complete boundary, before budget exhaustion. Priority at one boundary is
failure, cancellation, debug stop, condition, tick target, then budget. Return all
matched stop details and the winning reason. Each native/script instruction and
scheduled job dispatch charges a documented work unit, so a tick that never
finishes cannot evade the work budget. Native loops also need interruption points.

For a cycle whose condition is already true, no actions are queued or applied;
the result reports zero work and returns the observed state. A cycle uses the
same guard for admission and advance and queries `latest` without a prior cursor,
meaning its resulting pinned boundary. A command scheduled in paused mode waits
for a subsequent advance; an accepted command acknowledgment never implies a tick
has run. Operation status and terminal event history expose its outcome.

`operation.cancel` and read/status methods remain responsive while paused or
unlocked. A normal cancellation stops at the next safe boundary and preserves
completed ticks; it does not roll back the run. Admission/validation errors leave
state unchanged. Later run failure retains completed work and produces a failure
artifact, rather than claiming atomic rollback of history. A separate supervisor
watchdog can diagnose/terminate a hung worker even if the simulation thread cannot
respond. Its terminal record distinguishes last committed state from an
unavailable live state. Watchdog wall time never becomes gameplay time.

## Actions, ownership and authorization

Actions are typed `button`, `axes`, `select`, `scroll`, `pointer` or `invoke`
records. Direction/analog values are signed integers in -32767..32767 with an
explicit engine axis space; physical dead-zone calibration belongs to the input
client. Press latches an action until an explicit release. Release of an already
released action is idempotent. Auto-repeat belongs to the recovered shared input
rules, not the transport. Simultaneous actions are one atomic batch. Duplicate or
conflicting writes to one action/axis in a batch are invalid; legal distinct
actions retain array order. Every batch validates against a scratch state through
the normal handlers before a single commit. Future batches are revalidated at
their target tick; rejection of one batch does not undo earlier ticks.

Semantic selections name a queried context and stable choice/target IDs. Battle
actions and minigame-specific commands use discovered typed `invoke` payload
schemas. Disabled actions have reasons, costs, targets and ownership information;
selecting by ID obeys the same locks, progression and resource rules as physical
selection. Pointer testing names a native viewport and normalized 0..65535
coordinates and uses the real UI hit-test path. Normal agent gameplay must remain
possible using semantic IDs without coordinates. Tab/game-menu, Esc/app-menu,
cancel and explicit skip are distinct engine actions with normal eligibility.

| Host-granted role | Access |
| --- | --- |
| `observe` | Discovery, full state/query/diff/events/diagnostics, operation status and optional images; no actions, mutation, clock or persistence control |
| `control` | Observer access plus session creation, controller lease, ordinary actions, clock control and ordinary legal save/load commands |
| `debug` | Control access plus typed scenario shortcuts, exact snapshots/restore/reset/branch, guarded edits, break/watchpoints and instruction/task stepping |
| `unsafe_debug` | Debug access plus explicitly unsafe raw diagnostic access, native debugger launch/attachment under the host's normal OS permissions |

Inspection includes internal/offscreen state even for observers authorized to the
session. Secrets such as auth tokens, host paths or unrelated processes are not
game state. Debug rights do not grant filesystem/process/network privileges to
Lua. In-game Lua uses its separately restricted API and budgets; it cannot acquire
a host role or control lease. Remote privileged access is disabled unless the
host explicitly enables an authenticated, encrypted transport and trusted role
mapping; protocol 0.1 does not require a network listener.

There is one gameplay controller lease per session. Inspectors and spectators
may coexist. Explicit handoff checks the old lease and new authorized connection,
releases held actions and cancels unapplied commands from the old owner at the
next boundary, then logs the transfer. On controller disconnect, apply the same
release/cancellation and pause at the next boundary. A spectator disconnect has
no simulation effect. Pause/debug mutation requires the controller lease as well
as the appropriate role; an inspector cannot freeze another client's run.

## State queries, serialization and events

The [state schema](state.schema.json) defines immutable snapshot tokens, bounded
path selection, paginated results, typed field descriptors and delta records.
[State domains](state-domains.json) enumerate authoritative coverage obligations
and separate diagnostic surfaces. Each implemented component must register a
closed concrete schema with field types, units, enum meanings, mutability, stable
IDs and original symbol correlations. No untyped hidden map or absent field is
allowed to stand for implemented state. Unknown original semantics are explicit
unrecovered fields with raw provenance or a blocked capability, never invented
values. Opaque original data is preserved and queryable as bounded bytes while
its meaning remains unknown.

Full coverage includes inactive entities, collision/terrain, camera/control,
maps/transitions, party/stats/equipment/inventory, story flags/variables,
encounters, battle AI/turns, minigames, script stacks/PCs/waits, resumable Lua tasks,
RNGs, pending events/timers, and logical audio/media state. Resource/loading and
render/audio/device diagnostics and native source correlations are separately
queryable and excluded from authoritative hashes. Additions fail the subsystem
gate until serializer, query registry, mutation policy, replay and tests account
for them. A zero-length domain is valid only if it is implemented and genuinely
empty; `unimplemented` is a distinct discoverable status.

`state.query` with `at: latest` captures one complete immutable boundary. All
pages use its returned snapshot ID, selector and opaque cursor; pagination cannot
mix ticks. The server retains at most 8 query snapshots per connection for at
least 60 seconds unless explicitly released or the configured memory limit would
be exceeded. Resource-limit rejection is explicit. Expiry returns `stale_cursor`
and a new query must start. Results declare `complete` and `next_cursor`; bounded
byte/chunk fields disclose total size, offset and continuation. Never silently
truncate. A read at a debug microstep is explicitly `boundary: instruction` and
is not confused with a completed tick.

Full execution snapshots contain authoritative state, tick/subrate accumulators,
held inputs, deterministic pending commands, original/Lua task progress, RNGs,
logical media positions and manifest compatibility IDs. They do not serialize
arbitrary Lua stacks, GPU objects, host pointers, decoder handles or wall clocks.
They also do not confer host roles or controller ownership. Rebuild transient
presentation separately; exact audio resumption may use bounded pre-roll or a
versioned portable presentation cache for Wide/HRTF/resampler history.

Canonical hashes use SHA-256 over the versioned deterministic serialization:
sorted map keys and stable entity IDs, explicit type/length tags, UTF-8 strings,
fixed-width little-endian integers, no padding/pointers, and exact recovered
fixed-point/rational values. Authoritative floating point, if a recovered system
requires it, needs a separately versioned canonical representation and cross-host
evidence. No NaN/host-dependent serialization is accepted. Presentation quality,
stream cadence, wall time, diagnostics, transport IDs and auth state are excluded.

Diffs name exact base/target tokens and ordered add/replace/remove operations;
unknown base or epoch mismatch is an error, not a guessed delta. Subscriptions
deliver tick/epoch-tagged events with sequence IDs. Events have bounded queues and
explicit loss ranges plus resync instructions. Persistent replay traces required
for evidence cannot silently drop: exhausted artifact storage stops the run with
`resource_limit`. Only lossy observation queues may discard older entries.

## Debugging and typed go-anywhere

Conditional breakpoints target semantic events/transitions, registered state paths
or original-script/Lua instruction IDs qualified by resource/source identity.
Watchpoints compare before/after values at declared commit boundaries. A script
instruction stop exposes its scheduler phase, task stack and instruction index;
tick remains the last complete tick. Resume completes the remaining scheduled
work exactly once. Full debug snapshots must include this microstep continuation
when the capability is advertised; unsupported stop/snapshot combinations return
an explicit error and block subsystem debug completion. Native machine-code
stepping and memory inspection use documented debugger symbols/source maps and
normal debugger integration, rather than interpreting machine instructions in the
game. Assertion/crash/hang artifacts preserve the native stack when available.

`debug.mutate` requires a paused state, exact guard, expected prior values,
nonempty reason and typed registered paths. Validate the entire prospective state
including references and invariants; commit all edits or none. Log before/after
diffs and provenance. Unsafe raw access needs the extra role, is always labeled
`unsafe_probe`, and invalidates ordinary gameplay-completion evidence. Ordinary
play never depends on raw memory offsets. Reset, restore and branch are explicit
debug operations, with private per-session save stores to prevent replayed rewards
or abandoned branches from duplicating external writes.

The scenario schema specifies source/profile/content/mod identities, catalog
entry, typed setup, seeds, configuration, readiness, actions, waits, assertions,
snapshots and optional images. Catalog entry kinds are field, world_map, battle,
minigame, event and menu; each must be enabled only with verified native semantics.
Names and numeric selectors resolve through the source-qualified catalog, not
filename guesses. Position/facing, party/stats/equipment/inventory, progression,
encounters and camera use registered typed setup schemas as each subsystem arrives.
Before those schemas exist, the corresponding entry/setup returns
`unsupported_capability` instead of accepting arbitrary JSON.

Prepare a scenario in an isolated candidate state using the same native loaders,
initializers and validators as normal play. Resolve dependencies, check collision-
safe spawn and source compatibility, run bounded initialization, then atomically
publish the session or return failure with no partial replacement. Ordinary
`control` sessions may cold-start the canonical new-game entry with reset defaults;
arbitrary entry/setup shortcuts require `debug` and are logged. Both need no prior
save. Initial validated scenario setup is separately classified from subsequent
legal play; a continuous unmodified playthrough additionally requires canonical
new-game entry and no corrective debug operation throughout.

Readiness predicates distinguish assets_available, map_initialized,
scripts_complete (scoped tasks), dialogue_actionable, menu_actionable,
battle_ready and player_control. Query observations identify actual control owner
and reasons. Conditions support typed comparisons, all/any composition and
stable-tick counts. Entry scripts running in a loaded map do not imply player
control. Missing/unrecovered predicates fail explicitly. An emulator scenario
requires a reviewed translation of its button semantics, readiness and timing;
the native API never imports emulator frame counts as ticks or guarded RAM writes
as typed native state.

Run artifacts include normalized requests/scenario, source/asset/catalog/build/
schema/mod hashes, seeds, setup class, admitted and applied commands, tick/epoch
events, guarded mutation diffs, stop conditions and observed values, state hashes,
snapshot/replay references and optional media. A terminal manifest records
success, assertion failure, budget exhaustion, cancellation, watchdog or crash;
presence of `started` or a capture is not success. Keep original assets, snapshots,
private saves and data-bearing traces in ignored local stores. Public CI uses
authored synthetic fixtures and never supplies an original-game behavioral oracle.

## Optional images and audiovisual settings

On-demand images and streams render immutable snapshots of the same session.
Images include session/epoch/tick/timebase, render profile, dimensions and sequence
plus dropped/skipped counts. A paused-state screenshot advances zero ticks. The
default stream holds at most two pending snapshots and two encoded images per
viewer; replace/drop oldest on overflow. Encoding/transport cannot block the
simulation thread. Capture cadence is explicit ticks or maximum wall-Hz sampling;
time labels always report actual simulation ticks, including acceleration.
Viewer detach, failure and slowness have no authoritative effect. Exact every-tick
offline rendering is a separate explicit operation with documented storage/work
cost; it is not the live spectator policy.

Modern rendering is default with optional PS1 operations off. Modern, PS1-style
and Custom profiles expose independently recovered geometry/interpolation/color/
raster/order controls, separate from culling, output size, aspect and texture/UI
filtering. The same controls apply to spectator images and never change gameplay
hashes. No Phase 0 claim is made that any original visual quirk is recovered.

Wide headphone surround is optional and must produce exactly two channels from
the verified original Wide spatial signal. First recover Mono/Stereo/Wide routing,
phase/polarity and intended playback; decoder choice and HRTF data remain open.
Do not infer Dolby, 5.1/7.1 or rear/height/LFE channels from the mode name. Maintain
raw Wide, reconstruction and HRTF taps for separately validated stages, original
mode bypass and previous-mode restoration; never process material twice or rotate
the mix merely because first-person mode is active. Logical media timing works
with null output; enabling presentation reconstructs required processing history.

## Gates and maintenance

[Acceptance cases](acceptance.json) are all `defined` until actual native evidence
passes. They bind preconditions, stimuli, measurable outcomes, artifact obligations
and matrix tasks. Public schema/negative tests prove only Phase 0 design consistency.
Phase 2 cannot call an implemented subsystem complete without all relevant state,
input, setup, time and debug surfaces. Phase 3 requires the actual original-qualified
field → dialogue/event → battle → reward → menu → ordinary save/load route under
human and direct agent control, exact snapshots and debugging, device-free play,
unlocked equivalence and optional spectator demonstration. Corrective debug edits
cannot stand in for legal progression. Broader content work cannot bypass this gate.

When changing either test apparatus, update the versioned parity rows, exact source
hashes, original scope and negative tests. Never promote a synthetic executor test
to an observed emulator capability or an emulator run to native implementation.
Requirements and defaults remain unpassed until their owning native phase supplies
platform-specific evidence.
