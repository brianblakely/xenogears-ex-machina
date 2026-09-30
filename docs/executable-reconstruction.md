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
- all Program-owned GTE control registers 0–30 at exit to match exactly (including
  rotation, translation, screen offsets and H); FLAG and data registers remain
  transient. Capture qualification separately checks the complete 64-word GTE
  observation wherever observers share a hook.

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
hook placed on a loop head. Native callee/interrupt windows exclude only unowned
original writes. Every computed-owned caller/heap stack byte is still compared;
persistent stack-window labels are diagnostics and never exclusions.

Two historical per-call limits follow from snapshot granularity. A call store that repeats the
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
the runner imports the GTE control registers (0–30 are compared at exit)
and receives the I/O page as a read-only
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
