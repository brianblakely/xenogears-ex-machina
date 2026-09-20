# Executable reconstruction workflow

Phase 1 grows one recovered C++ program. Run a relevant connected entry, compare
its computed checkpoints with qualified original observations, fix the earliest
divergence, then connect the next encountered dependency. The
[slice contract](phase1-slice-contract.md) and its nine proof domains remain the
exit requirements. This analysis executable is not a playable runtime or a new
Phase 0 implementation.

## Responsibilities and current foundation

| Responsibility | Implementation | Boundary |
| --- | --- | --- |
| Recovered behavior and ownership | `xem-reconstruction`, `Program`, `ResidentState`, `FieldState`, existing subsystem C++ | Original algorithms, call order and committed state; no case files, expected results, host clocks or presentation |
| Analysis execution | `xem-analysis-runner`, `tools.analysis.execution` | Prepare supported input, supply explicitly declared external service results, invoke the library, bound work, retain state and format reports |
| Original qualification and comparison | `tools.analysis.return_case`, `tools.analysis.execution compare` | Verify source/capture lineage, build immutable starting inputs and separate expectations, compare named exact projections |

The old field-only library target has been replaced by `xem-reconstruction`.
All C++ subsystem tests and the runner link this target. The future native runtime
must call these same operations. It must not copy their behavior into a second
headless, agent or presentation model. The installed Phase 0 baseline remains a
baseline.

Already reusable C++ includes packed decoding, event/collision parsing, actor
defaults/RNG/floor queries, event scheduling and handlers, control requests,
planar/jump/vertical stages, collision queries and sweeps, sprite construction/animation/VM
subsets, battle requests/continuation, return data, music callers and the resident
CD ring. Scheduling and control share the input-update word; sprite commands
invoke the existing motion arithmetic; music streaming invokes the resident ring.

Complete party motion and position integration still have useful Python
references and original captures. They are candidates for lowering into this
library, not alternative executable game logic. The existing compiled collision
queries/sweeps should be connected, not reimplemented. Their four tracked source,
test and format-note files were absent from the source allowlist; this integration
adds them so isolated builds include the existing dependency. Full initialization, ordinary
NPC motion, contact/followers, combat, menus/card state, media services and mode
dispatch remain incomplete. The loader, initializer, scheduler, field update and
mode dispatcher are distinct boundaries. In particular, `8008110c` runs events
before motion, controlled contact/position, other position updates, encounters
and followers; an event pass does not stand in for this update.

## First connected original case

The supported entry is the **return branch of field `800a28d4`**, with freshly
initialized actor/descriptor storage, an original field-return snapshot,
qualified sprite resources and explicit original heap results. The source is
Disc 1 profile `na-slus-00664-39c547a9afc6`, resident executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`, field overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.

```text
original return snapshot + initialized field storage
  -> 800a3474 data restore: 25 actors, globals, variable bank
  -> 800a28d4 computes resource selection and seven factory arguments
  -> 80076ac0 factory: allocation, construction, bounds, animation, publication
  -> next actor uses the resulting shared sprite/task/allocator state
  -> actors 0..18 return
  -> actor 19 executes C6 FF, then encounters sprite command 96
```

Before this integration, comparisons supplied restored actor records, factory
arguments and shared environment separately to individual calls. Now the snapshot
produces the records and represented globals, the caller computes resource/slot/
mode/variant/tag/defer arguments, and each factory produces the state used by the
next. The sprite gate at `800b218e` and party reassignment gate at `800b2268` come
from the restored field region. Original heap addresses and incoming allocation
bytes remain **external service inputs**; no recovered heap is claimed.

The original order rebuilds **all** sprites before the caller's optional party
reassignment. The later `800a3c8c` checkpoint pass is a separate operation. The
current entry stops before that pass, readiness publication or return to ordinary
field updates. Replaying a checkpoint after each individual factory would invent
an ordering that the original does not have.

The comparison checks 20 completed checkpoints and the full 356-byte partially
constructed sprite at the stop: 29,336 bytes of direct original output and 9,984
owned part bytes under the reviewed unchanged-write invariant, plus factory
arguments, 16 environment values per factory and allocation/release requests.
Six correlated captures of one route supply this evidence; they are not six
independent scenarios. Source/live differences in field sprite resources remain
unavailable ranges. No tolerance, replacement bytes or unexplained mask is used.
See [EVID-REF-039](../analysis/findings/EVID-REF-039.json).

## Focused commands

Enter the pinned environment, then build only the executable under investigation:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
cmake --preset debug
cmake --build --preset debug --target xem-analysis-runner test-program
```

Qualify and export once to new private paths. Inputs and expectations are separate;
the expectations bind the canonical case hash. No expected value reaches C++.

```sh
python3 -m tools.analysis.return_case export \
  --case .local/execution/return-case.json \
  --expected .local/execution/return-expected.json
python3 -m tools.analysis.execution run \
  --case .local/execution/return-case.json \
  --expected .local/execution/return-expected.json \
  --report .local/execution/return-run-001.json
python3 -m tools.analysis.execution compare \
  --report .local/execution/return-run-001.json \
  --expected .local/execution/return-expected.json
```

The current **run exits 1** with `dependency_needs_recovery`; **compare exits 0**
with 20 matched checkpoints and matched partial state. Agreement up to a dependency
does not complete the selected function. Use a new report path for each run;
rerunning does not overwrite evidence. `--runner build/sanitize/xem-analysis-runner`
uses the identical library under ASan/UBSan. `--max-operations 32` stops before
actor 19's allocation; `--max-operations 35` stops after C6 commits but before
command 96. `--timeout 30` is a separate process watchdog.

Public focused regression commands:

```sh
cmake --build --preset debug
ctest --preset debug -R 'connected-program|analysis-execution|field-sprite|field-event|field-return'
python3 -m unittest tests.test_return_case
```

The exporter requires the original `.local/references/source-1/disc.bin` and the
historical captures/reports recorded by EVID-REF-028, 030 and 036: lifecycle restore,
actor defaults, factory v2, constructor v2, return loader, return policy and their
124-artifact uninstrumented control. Exact paths and required fingerprints are in
`tools/analysis/return_case.py`. Missing artifacts fail qualification by name; the
public build and synthetic tests remain usable without them. The same export and
run commands reproduce validation after those artifacts are supplied. No fresh
emulator capture is needed for this joined boundary.

## Ownership and observation

`Program::resident` owns the variable bank, RNG, battle request, music/CD state,
sprite services, allocation controls, party resource identities and original
return snapshot. `Program::field` owns mode-local actors/descriptors, globals,
event package, collision data, resources, checkpoints and input/event controls.
Each actor owns its constructed sprite and parts from allocation onward. A failed
factory retains them; a released part allocation is no longer reported as live.

Borrowed event/program/resource views are made for each call and do not outlive
their owners. Moving a program preserves owned data without retaining pointers
into the old object. Already constructed sprites provide subsequent frame-list
links; they are not reintroduced as captured inputs. Original addresses identify
correlations and allocation service results and are never host pointers. Semantic
`event_pass`, `event_batch`, actor event/control queries and resident state are the
native integration surface; byte images remain original-format correlations,
not a proposed native save format.

Return data preserves newly allocated actor `+118` ownership. It restores variable
values while preserving the loaded signedness bitmap. Optional actor extensions
still require their allocation service. The existing narrow data-copy API is
transactional on errors; its allocator failure during optional extension restore
does not yet model resumable original stores. This variant remains outside the
connected claim. Existing live sprites require original cleanup before replacement.
Unsupported auxiliary bindings and party reassignment stop when reached.

Read-only observers expose source boundaries and sprite loop iterations. The
analysis host owns work/report budgets and the watchdog; fixed host inspection
limits have been removed from sprite algorithms. No observer supplies a gameplay
result. Native code may provide its own debugger observer or none. The original
event interpreter's 1,025-dispatch safeguard remains original behavior, reported
as a normal batch return with its diagnostic request.

## Outcomes and continuation

| Status | Meaning |
| --- | --- |
| `completed_boundary` | The selected original entry returned; not necessarily an enclosing update or mode transition |
| `dependency_not_connected` | An encountered operation is already implemented but unavailable through this path; integrate it |
| `dependency_needs_recovery` | Execution encountered a missing original behavior; investigate its caller/callee and state |
| `invalid_input` | Missing, malformed, conflicting or unqualified starting input/service data |
| `behavioral_divergence` | Earliest completed checkpoint or partial-state projection differs from independent expectations |
| `reconstruction_error` | Unexpected execution failure, invalid computed access or process failure |
| `host_budget_exhausted` / `host_timeout` | Analysis limit, not an original wait, frame or game result |

Reports retain checkpoints and committed partial state, selected entry, code
identity, machine address, actor, operation and reason. Event-relative PCs and
sprite command addresses have separate fields. Recovery-inventory records add
source/evidence and the next investigation. A watchdog-killed subprocess cannot
publish final in-memory state; that limitation is explicit.

Every CLI run starts from immutable input. Interrupted C++ call stacks have no
represented continuation: inspect the retained state, fix the cause, then rerun
the case. Do not retry the interrupted entry on partially restored actors.
Ordinary event waits return normally and preserve PCs/countdowns; repeated
`event_pass` entries count scheduler passes, never pretend to be full frames.
Battle requests are pending resident state, not combat or mode dispatch.

## Choosing the next work

1. Rerun the connected case and correct the earliest divergence before extending it.
2. Follow the encountered inventory entry and inspect existing source/captures. Use
   an existing implementation when available; recover only genuinely missing behavior.
3. Keep one integration owner. Delegate bounded source/type/format/capture questions
   with exact inputs and expected deliverables, then integrate in the shared library.
4. Keep narrow tests as regressions, compare the expanded path, and preserve the new
   checkpoint. Use synthetic tests for ownership/errors and original evidence for fidelity.
5. Extend backward through the loading/initialization that actually produces the entry.
   Extend forward through source-derived callers, cleanup and mode transitions. Add
   a concrete mode owner when battle/menu/world-map/persistence/media code needs one;
   do not build empty subsystem frameworks or invent boot state.

The first current dependency is `instruction:sprite:0x96`, actor 19, sprite PC
`0x8012a10a`, resident dispatcher `800248d4`, handler/table target `8001feec`.
The qualified generic-effect source calls task-list removal `8001ce74(sprite+6c)`.
Review that list's initialization, removal and ownership together, then rerun this
case. Subsequent `FC` upload, other required NPC commands/callbacks, all-factory
completion, party/auxiliary variants, checkpoint replay and cleanup remain queued.

Backward work is the real `80080f44` allocation/default/shadow caller and resource
loading. Its 25 RNG/default outputs already have exact captures, but shadow
initialization `8007aa44` and heap results remain external boundaries. Forward
ordinary field work must connect the already recovered movement/collision/position
models before claiming field updates. Alternative return paths, failure/defeat,
map transitions, menus/save-load, required media and the remaining slice obligations
remain in the existing inventory/manifest. Neither this case nor synthetic success
changes Phase 1 completion marks.
