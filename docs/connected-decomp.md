# Connected decompilation: the Phase 1 execution architecture

The Phase 1 product is a growing connected program made from independently
recovered Xenogears source. Its tight loop is **run -> locate the first divergence
or blocker -> integrate/recover that dependency -> compare -> run farther**.
The goals and exit obligations in [plan.md](../plan.md) and the
[slice contract](phase1-slice-contract.md) are unchanged. This is an execution
strategy, not a smaller slice, a new completion ledger, or a matching-decomp gate.

## What is implemented now

`xem-field-reconstruction` remains the single C++ implementation of recovered
behavior. `PreparedFieldEventPass` owns the inputs and working state for one
prepared field-event scheduler pass: its event package, actors, descriptor flags,
variable bank, control/pass state and explicitly supplied battle/media boundary
values. It is not complete field state or a whole-game state object. Borrowed
views exist only during a call, so moving a prepared state cannot leave pointers
into its former owner.

`xem-reconstruct` links that library. It composes the existing event-package parser,
original actor-to-event projection, actor scheduler, slot/batch logic, core handlers,
call/return, battle request/continuation and extended battle/music waits. It does
not duplicate their algorithms. Variables, actor PCs, gates and battle state flow
between actual handler calls and between actors without trace-fed replacements.

`python -m tools.analysis.connected` validates input hashes, starts the executable,
and compares its returned state with an independently supplied expected projection.
Expected output is never passed to C++. Reports identify the input and executable
hashes, source overlay, stop location, completed dispatch count, full supported
semantic state and first difference. An instruction stop is enriched from the
existing recovery inventory: **integrate existing C++** when available, otherwise
investigate recovery. This does not alter that inventory's maturity statuses.

The initial entry is deliberately a **prepared field event-pass boundary** at
field overlay scheduler `800a2030`, not a cold boot or reconstructed initialization.
It stops at the first unconnected instruction, missing boundary input, malformed
operation or host budget. If the scheduler returns, the next stop is explicitly
`integration:field-update-tail`. Motion, collision/contact composition and the
rest of enclosing field update `8008110c` are not connected by this change.
Existing source for those systems is not erased or declared unrecovered.

There is no complete simulation tick yet. Do not repeat this event-only pass to
pretend that movement, media, interrupts or time have advanced. Do not resume a
partially executed scheduler after an exception: its stack continuation is not
serialized. Prepare a new run from the same immutable starting state. Partial
writes, including an FE prefix increment, remain visible for diagnosis.

`xem-baseline` is preserved as the Phase 0 build artifact. The analysis executable
is not installed as the shipping game and does not implement the native agent API.
The later runtime must consume this same recovered logic and supply its real
ownership/services, not rewrite the game from the Python reference models.

## Run the focused loop

Inside the existing [development environment](development.md):

```sh
cmake --preset debug
cmake --build --preset debug --target check-connected
# Once the private prepared case has been extracted from qualified evidence:
python -m tools.analysis.connected \
  --executable build/debug/xem-reconstruct \
  --case .local/connected/field23/case.json \
  --output .local/connected/field23/run-001.json
```

`check-connected` runs authored C++ ownership tests and Python/native composition
regressions. They are also CTest tests labeled `connected`. The native integration
suite is invoked with the built executable; generic Python discovery alone skips
its native cases. No game data is needed for those public fixtures.

The private command intentionally has no supplied retail fixture in this commit.
Qualifying the first original connected case is still required. Existing original
function-level comparisons are not automatically connected execution evidence.

Exit codes are `1` for an explicit integration/input/instruction stop, `2` for
invalid input or execution failure, `3` for an expected-state mismatch, and `4`
for the host instruction budget (a subprocess timeout is an execution failure).
Matching the prepared boundary still returns `1`: it does not
claim the enclosing field or Phase 1 is complete. `--help` returns `0`.
Reports use exclusive creation; there is no overwrite or golden-file blessing mode.

## Prepared-case format

The front end uses Python's standard JSON parser; there is no new runtime JSON
package or compatibility/version negotiation. The C++ transport is internal,
versionless decimal integers with strict widths, counts and trailing-data checks.
The public analysis command is the Python module, not that transport.

A case has exactly `provenance`, `inputs`, `instruction_limit`, and `expected`.
Every file reference is `{ "path": "relative/or/absolute/path", "sha256": "..." }`.
Relative paths resolve against the case directory. Inputs are checked before
execution; originals and reports remain private under `.local/`.

`provenance.kind` is `synthetic` or `original-projection` and
`provenance.overlay_sha256` must identify the qualified field overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
An original projection also records its existing `profile`, `finding`, and a
hash-qualified `capture_manifest` file. These identify evidence to review; hashing
a manifest does not itself validate its interpretation or establish original-source
qualification. Follow the existing [evidence workflow](evidence-workflow.md).

The `inputs` object contains all of the following; absent service state is `null`,
not silently initialized as ready:

| Field | Representation |
| --- | --- |
| `events` | Hash-qualified decoded logical component 5; the existing C++ parser owns its bitmap, entries and bytecode |
| `actors` | Hash-qualified concatenation of complete original `0x138` actor records in scheduler order; only the established event projection is interpreted |
| `variables` | Hash-qualified complete 2,048-byte little-endian variable bank |
| `descriptor_flags` | One original unsigned 32-bit flag word per actor |
| `control` | Eight signed 32-bit values: budget mode, break request, batch limit, post-initialization, three gates, diagnostic suppression |
| `pass` | Two unsigned 32-bit values: input-updated and unknown-c4268; the recovered scheduler resets them |
| `scheduler` | Single-actor mode, unsigned-byte party-processing mode, three signed party indices |
| `battle` | Null, or field-active, menu-gate, gate-90, pending (u32), selector, mode, resident-flag (u8) |
| `music_result` | Null, or the original u32 result at the prepared boundary |
| `battle_mode_source` | Null, or the original u8 mode source at the prepared boundary |

`instruction_limit` is a host bound from 1 to 1,000,000, not a game cadence or the
original interpreter budget. Original handler batching is unchanged. Host input
allocation limits are not claims about the game's maximum actor/resource counts.
Battle request requires its complete existing C++ argument contract, even when a
particular original retry branch would not read every argument. Such a stop is
labeled missing input, not newly discovered missing game code.

`expected` is null for exploration or a hash-qualified JSON file containing exactly
`state` and `location`. `location` is `[actor_index, selected_slot, working_pc]` at
the same boundary. `state` contains the complete semantic projection emitted by
C++: actors (all represented fields and eight slots), descriptor flags, all variable
words/type bits, control, pass, scheduler and supplied battle/media state. Unknown
original actor bytes are outside that projection; this is not a full-RAM comparison.
No partial key matching, ignored fields or state masks are supported. Shape, types,
values and location must agree. Expected values must come from independent original
observations/source review, or explicitly authored synthetic fixtures, never from
saving the new implementation's output. `tests/test_connected.py` contains the
small authored fixtures and an independently specified whole expected projection.

## Choosing and completing the next change

1. Run a qualified case before choosing work. A divergence in already connected
   behavior takes priority over advancing farther. Keep the previous comparison
   passing after correction. Use the actual stop, not a broad category of interest.
2. Distinguish missing input, existing-but-unconnected C++, genuinely unrecovered
   behavior, malformed input, a code defect and a host budget. Consult the current
   [source/state handoff](../analysis/formats/field-lifecycle.md), recovery inventory
   and captures before opening Ghidra or inventing a new experiment.
3. Integrate existing recovered modules first. Add the real owned state and caller
   relationships they require. For genuinely missing behavior, correct shared
   Ghidra types/signatures and reconstruct a useful connected boundary directly
   in C++. Use assembly/m2c selectively, not a compulsory second toolchain.
4. Rerun the same C++ with the same immutable prepared inputs. Intermediate
   observations become assertions or diagnostics, never handler results. A narrow
   helper fixture can supplement, but not replace, the connected regression.
5. Report the executed source boundary and first remaining stop/divergence. Reuse
   existing findings and inventory entries for original claims; a synthetic test
   or new executable does not upgrade historical evidence or phase status.

The first migration work is to qualify a real event-pass entry/exit projection
from existing captures, then connect the field-update tail through existing
control/motion/collision/position source and its real state dependencies. In
parallel with forward extension, move the prepared entry backward through recovered
actor initialization and loading. A checkpoint is a scoped research boundary,
not a permanent substitute for initialization. Continue toward encounter/return,
menus/persistence and media according to the unchanged slice obligations.

One integration owner maintains the runnable path. Bounded parallel source/type/
capture questions are useful; disconnected subsystem agents and phase-wide
proof-building are not the default. Existing Python models remain useful
independent references, but new stable gameplay logic should not need a Python
implementation before it becomes C++.

Keep original source identities, overlay qualification, arithmetic semantics,
explicit unknowns and historical evidence. Keep the Phase 0 contracts and audits.
Use focused tests inside the loop and comprehensive audits at integration
milestones. Do not introduce another evidence database, generic execution
framework, migration layer, emulator fallback or speculative shipping runtime.
