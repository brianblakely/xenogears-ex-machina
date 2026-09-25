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

The complete field update `8008110c` (events, party and NPC motion, contact,
positions, interactions and followers), the move phase `800739c0` (camera follow,
view matrices, facing and sprite orientation) and the return checkpoint pass
`800a3c8c` now run in this library. The resident heap (`80031bdc`, `800320e8`,
`80031ff8`) is reconstructed with synthetic coverage; its original comparisons
and use by callers are in progress. Initialization, dialogue/message, sound,
drawing, combat, menus/card state, media services and mode dispatch remain
incomplete. The loader, initializer, scheduler, field update and mode dispatcher
are distinct boundaries; an event pass does not stand in for a field update.

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
  -> actors 0..24 return on the selected original route
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

The current comparison checks 26 completed restore/factory checkpoints, plus
factory arguments, environment values, allocation/release and upload requests.
It verifies the selected runner boundary separately from the original output
projection; it does not compare final whole-field state or original return
readiness. Earlier EVID-REF-039 checked 20 checkpoints and actor 19's partial
sprite at command `96`.
Six correlated captures of one route supply this evidence; they are not six
independent scenarios. Source/live differences in field sprite resources remain
unavailable ranges. No tolerance, replacement bytes or unexplained mask is used.
See [EVID-REF-040](../analysis/findings/EVID-REF-040.json).

## Choosing the validation and capturing lean

Pick the cheapest validation that is exact for the behavior under test:

| Behavior | Validation |
| --- | --- |
| Deterministic in disc data or constant tables (decoders, parsers, lookups, layouts, disassembly, table arithmetic) | Static review against the original code plus disc-data checks: projection hashes, decoded bytes, and source-to-RAM correlation read from RAM already in an existing capture. Synthetic tests cover bounds and errors. |
| Depends on runtime state (interrupts, battle turns, menus and saves, field frame state, loader progress) | Exact per-call memory-image comparison, below. |

Snapshot captures are expensive, so capture per route, not per function:

1. **Reuse first.** `python3 tools/reference/scenario.py --captures [FILTER]` lists
   every completed capture with its route, frame window, hooks and snapshot hooks.
   A function whose calls fall in an existing snapshot window, or whose entry and
   exit are already hooked on that route, is compared against that capture.
2. **Capture wide, once or twice per route.** When a route lacks what you need,
   make one shared capture with every hook the route's current work needs, not a
   separate run per function. Always include the interrupt brackets. Mark
   `snapshot` only on hooks that need an image comparison. Hooks that only
   locate or count calls stay register-only. Name the output
   `p1shared-<route>-<purpose>`. Choose one frame window covering the calls, not
   many small windows. If one window exceeds the snapshot budget, split it into
   the fewest contiguous windows that fit.
3. **Capped concurrency for heavy work.** Captures, builds and comparisons take
   host-wide slots (`tools/reference/host_slots.py`): one capture, one build and
   two comparison runs at a time across every worktree of the checkout. The locks
   live in the git common directory. `observe.py`, `memory_case.py` and
   `check.py` take their slots themselves. Build and test through
   `python3 tools/repository/build.py PRESET [--target T] [--test REGEX]`, not
   bare `cmake --build`. Build presets cap compile jobs at 8 and tests at 4.
   Give a batch of comparisons to one sequential driver instead of launching
   them all at once.
4. **No polling loops.** Don't leave `until`/`while` sleep loops watching files
   or processes. Wait for a job by running it as a background command that exits
   when the job ends, and stop any watcher once its job is done.
5. **Probes are disposable.** Exploratory probes, such as navigation or
   discovering button meanings, use no snapshot hooks. Delete them once a finding
   cites a shared capture. Cited captures are immutable.

## Memory-image comparison

Connected entries are compared against complete original memory, not selected
projections. A qualified capture stores the full 2 MiB RAM, the scratchpad and
the CPU and GTE registers at an entry PC and at its return. The snapshot file
stores a complete image every 256 snapshots and otherwise only the 256-byte
pages changed since the previous snapshot; per-record digests verify every
reconstructed image. The analysis import builds a `Program` from the entry
image and the separately decoded field source. The runner executes the C++
entry and exports every Program-owned value to its original address. The
comparison then requires:

- every owned byte to equal the original exit image;
- every byte the original changed to be owned, except the callee stack window
  below the entry stack pointer and bytes changed only inside bracketed
  interrupt handlers (`8003c028` sound tick, `8004b9b4` dispatcher, and the BIOS
  exception save areas while such a handler ran);
- the GTE rotation and translation registers at exit to match exactly.

An owned byte that only interrupt code changed is attributed to the
interrupt when the C++ left it at its entry value. An owned byte the call
wrote and interrupt code then rewrote before the call returned is matched only
when the C++ value equals the byte's value in the snapshot at the start of the
first interrupt after the call's last change to it; such bytes are listed with
that value and interrupt as `interrupt_superseded`. Brackets must be disjoint
and in time order. Overlapping ownership, any other write by both the call and
an interrupt, or an unowned change is a divergence. The BIOS save areas are the
exception: exception entry writes them before the dispatch hook, so they are
never Program state and never a conflict.

Presentation brackets (`--presentation ENTRY:EXIT`) name original calls that do
only drawing or camera work inside a compared step, such as the battle camera
`800bc404` or model setup. Their byte changes are attributed to them the same
way as interrupt changes. The GTE registers must be unchanged across each
bracket, and owned state they touch still has to match. A bracket asserts that
the call is presentation. That assertion is part of the reviewed boundary, never
a mask for unexplained bytes. `--entry-repeats` selects later passes of an entry
hook placed on a loop head. Stack bytes below the entry stack pointer are
excluded from interrupt attribution as they are from ownership.

Two limits follow from snapshot granularity. A call store that repeats the
value an interrupt left is invisible, so both interrupt rules then expect the
older value. The comparison also does not model the call reading a value that
interrupt code wrote; carrying Program state from one call into the next would
need the interrupt's effects (for example the libcd callback pointer at
`800564a8`, which the Program leaves set where the machine has cleared it).
Captures also record the interpreter's load-delay slots. A load issued in a
caller's delay slot is still pending at the callee's entry hook; the tool
commits pending loads before passing entry registers (arguments, stack
pointer) and before reading the exit return value, which is what the code
observes one instruction later.
They also record all 64 GTE registers and the 4 KiB hardware I/O page;
the runner imports the GTE control registers (rotation, translation, screen
offset and H are compared at exit) and receives the I/O page as a read-only
platform input, for example the CD DMA status that libcd polls. The scratchpad
is not compared. Original
globals are correlated once, in `src/reconstruction/original_layout.cpp`; the
field-return snapshot restore writes through the same table, so no region has a
second representation. Resident entries (heap allocate and release) import only
resident state and need no loaded field; globals that live in overlay data
belong to field state, because at boot the heap holds that memory.

```sh
python3 -m tools.analysis.memory_case --capture CAPTURE --entry field_move \
  --entry-hook move-entry --exit-hook move-return \
  --interrupt tick-entry:tick-exit --interrupt dispatch-entry:dispatch-exit \
  --report .local/execution/connected/reports/move-NN.json
```

See [EVID-REF-041](../analysis/findings/EVID-REF-041.json) and
[EVID-REF-042](../analysis/findings/EVID-REF-042.json) for the qualified
captures, runners and results.

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
  --completed-through 24 \
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

The current run and comparison exit 0 with `completed_boundary`, 26 matching
checkpoints and matching upload requests. This is the selected factory sequence,
not the enclosing original return lifecycle. Use a new report path for each run;
rerunning does not overwrite evidence. `--runner build/sanitize/xem-analysis-runner`
uses the identical library under ASan/UBSan. `--timeout 30` is a separate process
watchdog.

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

The next return dependency is the separate `800a3c8c` checkpoint pass after the
factory sequence, followed by cleanup, control readiness and route variants.
For the next distinct connected entry, use the manifest's ordinary field update
`8008110c`. Its original order is event pass, previous positions, motion,
controlled contact/position `80084158`, other positions `80084a40`, encounter
`8008399c` and followers `800815f0`. Connect the existing C++ and qualified
Python/source behavior into the shared library. Start with
`tests/reference-inputs/field23-control-observation.json` and the original
source/capture lineage in EVID-REF-026/027; compare each computed boundary to
the corresponding original field 23 captures. Do not promote an event scheduler
pass to a complete update.

Backward work is the real `80080f44` allocation/default/shadow caller and resource
loading. Its 25 RNG/default outputs already have exact captures, but shadow
initialization `8007aa44` and heap results remain external boundaries. Forward
ordinary field work must connect the already recovered movement/collision/position
models before claiming field updates. Alternative return paths, failure/defeat,
map transitions, menus/save-load, required media and the remaining slice obligations
remain in the existing inventory/manifest. Neither this case nor synthetic success
changes Phase 1 completion marks.
