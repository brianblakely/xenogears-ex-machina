# Reusable sprite construction, animation and checkpoint source

[field_sprite.hpp](../../include/xem/reconstruction/field_sprite.hpp) and
[field_sprite.cpp](../../src/reconstruction/field_sprite.cpp) recover the connected
resident constructor, resource binding, animation selection, facing replay,
matrix and frame-list operations. The same module now restores the later field
checkpoint through the ordinary timer and supported command execution. These are
compiled game implementations; the preserved Python models remain independent
analysis references.

This work is independently authored with Codex assistance from qualified original
Ghidra exports, original instruction review and the repository's original captures.
No other Xenogears implementation, symbols or format research was consulted.
Original binaries, resource bytes, exports and captures remain private. The new
source, tests and this document follow the repository's MIT license.

The source profile is `na-slus-00664-39c547a9afc6`. Resident executable SHA-256 is
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`, LBA 108606.
The checkpoint decision uses field overlay SHA-256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`, slot 36,
LBA 108933, base `8006faf0`. The relevant findings are
[EVID-REF-030](../findings/EVID-REF-030.json),
[EVID-REF-025](../findings/EVID-REF-025.json) and
[EVID-REF-029](../findings/EVID-REF-029.json).

| Original entry | C++ responsibility |
| --- | --- |
| `80024524`, `8002435c` | Exact sprite allocation, constructor effects and part allocation |
| `80023804`, `800239a0`, `8002393c`, `80022000`, `8001ee74` | Defaults, inline storage, renderer scale and part count |
| `800222bc`, `80022224` | Resource rebinding, binding directory and control-byte update |
| `800245d8`, `80023538`, `800223b0` | Animation resource/header, gravity and orientation |
| `80022090` and qualified matrix callees | Ordinary rotation and row/column scaling |
| `80022660`, `80021cf8` | Facing command replay and its relative-call stack |
| `8001d2b0`, `80022d44`, `800234ac`, `8001f8e8`, `8001f750` | Frame lookup/list scheduling and existing auxiliary metadata |
| `800248d4`, `80023210` | Supported ordinary commands and countdown dispatch |
| `8001fbe4` cases `a0`, `a1`, `b3` | Shared motion speed/impulse effects and lookup-index store |
| `80021d50` | Connected animation/timer replay and checkpoint restoration |
| Field `800a3c8c` | Single recreated-actor checkpoint decision and record stride |
| Field `80076ac0`, resident `8001f5bc`, `80023340` | Supported actor factory, initial bounds and replacement part allocation |
| `8001c964` | Task wait state, list cursors and explicit callback frontier |

## Storage and platform boundary

`create_sprite` requests exactly 356 bytes, writes the allocation-size halfword,
and calls `construct_sprite`. The allocator also supplies the separate part block
whose size is the frame-directory part count times 24. Both allocations contain
their actual incoming bytes. Defaults use the preserved sprite scale at `+82`
before installing renderer scale; unwritten allocation and part bytes survive.
The fifth short supplied to the wrapper occupies an unused seventh constructor
stack slot and has no state effect.

`SpriteAllocation` owns these bytes. `SpriteWindow` borrows an address-qualified
window that can include pointed storage; a 512-byte observation does not change
the constructor's 356-byte ownership. Original addresses are integer identities,
never host pointers. Resources and other frame-list nodes are read-only qualified
spans. Mutable renderer, sequencer, binding and auxiliary storage must lie inside
the supplied sprite window for the implemented paths. Conflicting overlapping
resource inputs, incomplete windows and wrapping extents fail explicitly.

Allocation is the constructor's only effect callback. A read-only constructor observer exposes
the original stage order for comparison; it never supplies game effects.
Animation installation, orientation replay, matrices and frame scheduling run
their actual source. Same-resource binding leaves the control byte unchanged;
new binding clears it. A null resource leaves the binding alone.

Negative animation selection uses `sprite+4c` and complements the full signed
animation index. It retains the original signed animation byte at `+af`. A null
alternate resource preserves the existing binding directory, as the original
null-binding call does. These negative cases have source and synthetic coverage;
the qualified constructor and checkpoint routes use nonnegative animations.

## Command and return connection

Facing replay retains its original skip semantics; it does not execute ordinary
speed or impulse commands. Ordinary execution supports timed `00..7f`, speed
`a0`, impulse `a1`, lookup index `b3`, sequencer short `c6` and relative jump `e1`. The `a0` and `a1`
handlers call [field_motion.cpp](../../src/reconstruction/field_motion.cpp), then
use its shared original PC-store function. An enabled nonzero impulse reference
is loaded before, and bypasses, the operand. References inside the active sprite
use its current bytes; external references use supplied qualified source bytes.

The original width table and optional incoming `S3` duration are supplied in
`SpriteSources`. Commands `40..7f` require that register context. Ordinary timed
commands apply the packed duration scale, signed truncation and halfword wrap;
facing replay instead adds the unscaled duration. A zero ordinary scaled delay
becomes one, and a wrapped six-bit command step becomes 63. Timer rate `-1`
disables work. A zero countdown does not execute a command.

`restore_sprite_checkpoint` consumes the same 48-byte checkpoint produced by
[field_return.cpp](../../src/reconstruction/field_return.cpp). It temporarily sets
the rate to zero, selects the saved animation and runs the ordinary timer to the
saved step. It retains original X/Z/Y integration and gravity order, then restores
positions, sequencer words and the incoming rate. It returns the executed-command
count for inspection. `select_sprite_checkpoint` handles the actor's animation
override, skip policy and optional record extensions; it does not recreate actors
or run the full descriptor/cursor loop.

## Connected actor factory

[field_sprite_factory.cpp](../../src/reconstruction/field_sprite_factory.cpp)
connects field `80076ac0` to the existing constructor, animation, orientation and
timer implementations. It borrows one complete 312-byte actor and 92-byte
descriptor. It performs the original actor mode/tag stores, coordinate lookup,
sprite publication, bounds lookup, animation selection, callback-identity store,
position publication and initialization count update. Mode-one creation releases
the initial part allocation and requests exactly 768 replacement bytes. Allocation
and release remain memory services; required game behavior is implemented as
ordinary C++ functions or fails explicitly.

`initial_sprite_bounds` follows the original frame-index fallback and signed scale
truncation. `advance_sprite_tasks` implements the wait-counter and cursor stores;
a nonzero task callback is an explicit boundary. The original list walk writes
both current and next before invoking a callback. Empty lists preserve current.
The factory uses the original full defer argument for its initial-step decision,
while storing only its low bit in the actor.

The focused original capture contains 25 factory calls. Nineteen complete calls
(actors 0–18) match all actor, descriptor, sprite, allocation bytes and owned
control fields; these include all 11 replacement allocations and four initial
timer/empty-task passes. All 25 bounds calls match the three complete output
words. The comparison checks 24,724 actor/descriptor/sprite/part/bounds bytes plus
the complete represented environment, allocation requests and releases. Qualified
overlapping constructor observations supply each actual incoming allocation.

The contiguous task capture also contains two unrelated words, `80059504` and
`80059540`. The comparator reports their changes separately. Original source
registers callback `8003c020` through `OpenEvent` and that callback increments the
words; its execution during a particular factory call is inferred, not captured.
Those eight bytes are outside the factory-owned comparison, so this does not
claim exact global-state equivalence. All other captured control bytes outside
the owned fields must remain unchanged on the successful paths.

Six factories (actors 19–24) remain incomplete. Three stop at command `96` and
three at `fc`; the observed route also requires `e0` and task callback
`80022df4`. The six nonempty task passes are recorded as blocked, not successes.
Required source dependencies include list removal `8001ce74`, resource upload
`8001fb30`/`8002dde4`, child construction `80023b84`/`80023a48` and its scheduling
callees, and task motion `80022cdc`. Existing-sprite destruction `800230a8` and
alternate part constructor `80024294` remain explicit boundaries; neither occurs
on these 25 original inputs. This is a verified factory subset, not a completed
factory or field lifecycle.

## Verification and remaining dependencies

Build and run through the pinned environment:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command bash -c 'cmake --preset sanitize && cmake --build build/sanitize --target test-field-sprite && ctest --test-dir build/sanitize -R field-sprite --output-on-failure'
```

The current source is compared directly with preserved original outputs: 25
constructors with 225 stages, 25 full allocation wrappers, 94 binding calls, 65
complete facing replays, their 65 parent orientations and 198 frame-list calls.
The checkpoint extension compares 25 actor decisions, 19 complete restores, six
timer calls and their six VM calls. Full observed sprite and relevant control
bytes agree without pointer exclusions. Resource bytes, source exports, original
call/return lineage and unchanged control captures are qualified by the existing
readers. Nested calls do not count as independent scenarios.

Synthetic C++ tests cover signed/wrapping arithmetic, unknown and looping commands,
unreachable checkpoint steps, negative animation binding, part ownership, missing
source bytes, frame-list boundaries and connected `a0`/`a1` effects. The motion
module separately compares speed, impulse and PC effects with original captures;
the new combined `a0`/`a1` VM stream is synthetic. Private source-matched reports
and sanitizer binaries are under `.local/verification/phase1-source-20260915/`;
historical reports and source snapshots are preserved.

The following required dependencies remain explicit: alternate platform
construction/binding/header logic and VM `800c11cc`; flag-40 bit-0 matrix
`MulMatrix0` behavior and original matrix inputs; new 64-byte auxiliary allocation
in `8001f750`/`8001f8e8`; remaining ordinary VM/generic-effect commands and their
callees; the remaining factory paths, complete return-policy loop, scheduling,
cleanup and field readiness. Unsupported invocation throws; inspection limits
are host diagnostics and never successful gameplay yields. Renderer/audio
backends and a native scheduler are not hidden behind callbacks. This module
closes supported source boundaries, not the full field lifecycle or Phase 1.
