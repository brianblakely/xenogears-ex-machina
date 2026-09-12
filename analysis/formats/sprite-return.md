# Sprite checkpoints and ordinary timed commands

[EVID-REF-029](../findings/EVID-REF-029.json) recovers the later field-return actor
policy and composes sprite checkpoint restoration with animation selection,
ordinary timer dispatch and frame metadata. The
[policy and checkpoint model](../../tools/analysis/sprite_return.py),
[timer and command model](../../tools/analysis/sprite_vm.py) and
[frame model](../../tools/analysis/sprite_replay.py) are reusable original-format
analysis source. Sprite creation, complete resource ownership and player-control
readiness remain required before a native field return can succeed.

## Source boundaries

The profile is `na-slus-00664-39c547a9afc6`. Resident source is executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`,
LBA 108606. Field source is decoded overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`,
source slot 36, LBA 108933, base `8006faf0`. Ends below are exclusive.

| Original boundary | Recovered behavior |
| --- | --- |
| Field `800a3c8c..800a3f4c` | Per-actor checkpoint choice, animation override and record stride; outer descriptor count, transform copy and cursor order also checked |
| Resident `80021d50..80021ebc` | Temporary rate, animation selection, replay to saved step, motion integration, position and sequencer restoration |
| Resident `80023210..80023290` | Countdown and conditional ordinary VM dispatch |
| Resident `800248d4..80024f20` | Timed commands `00..7f`, signed relative jump `e1` and dispatch of the `b3` index store; remaining commands fail explicitly |
| Resident `8001f8e8..8001fab4` | Frame bound and standard/compact metadata selection |
| Resident `8001f750..8001f8e8` | Compact metadata stream with existing auxiliary storage |
| Resident `8001fbe4..80021ad8` | Only the `b3` index-store case is newly composed here; the full generic-effect body is byte qualified, not fully reconstructed |

The primary Ghidra project records all 22 exported source bodies, instructions,
P-code and automatic C privately. The old generic-effect body stopped at
`80021974`; its original shared tail and epilogue through `80021ad7` are now in
the same function. No source bytes changed. Source-byte qualification of an
export does not establish every branch or callee's behavior.

## Checkpoint policy and restoration

The policy restores the descriptor count from the image's first byte and copies
the `74`-byte transform group at image `+3c`. It walks current event actors, using
the [return image](field-return.md) at `8005a4e4+95c`. The supplied actor objects
are the current state after recreation; this module does not produce them.

When signed actor `+124` is not `-1` and signed animation `+ea` is not `255`, the
policy overwrites checkpoint `+14`, even if it then skips that sprite. Actor
`+4 & 01000000` skips restoration. Otherwise a nonzero return gate, actor
`+0 & 600`, and a difference between any of the three saved party-mode words and
current party-mode bytes also skip it. Record size uses the current actor's
extension flags: base `174` bytes, optionally `0c` and `10` more. The final cursor
points to the variable bank; this later routine does not copy the variables.

The sprite helper saves rate `80059198` and forces zero, restores facing,
animation bytes, renderer settings and scale, then selects the saved animation.
Its signed animation load follows the renderer stores; an authored alias test
checks this ordering. It repeatedly advances the ordinary timer until flag bits
22–27 equal signed checkpoint `+18`. Each iteration integrates X, Z and Y in that
order, then adds gravity to vertical velocity, with original 32-bit wrapping.
Finally it restores the three checkpoint positions, two sequencer words and the
incoming rate. Sequencer pointer reload ordering is retained. The result keeps
the frame-list head computed by the reconstructed operations.

An unreachable step or bounded inspection limit raises an error. The original
does not have this host inspection safeguard. A limit is never a successful yield
or a ready-field result. Unknown binding, allocation, platform or VM operations
also fail explicitly.

## Ordinary timer and metadata

Rate `-1` disables timer work. Other rates perform `rate+1` iterations with
32-bit comparison; the original reloads the rate after dispatch. A zero countdown
does nothing. A nonzero signed halfword is decremented with halfword wrapping;
the VM runs only when the previous value was one.

Timed commands increment a frame (`00..0f`), increment the six-bit lookup index
(`10..1f`), decrement a frame (`20..2f`), or retain it (`30..3f`). Their duration is
the low nibble plus one. `40..7f` instead use the incoming `S3` value, which must
be supplied explicitly. Duration is multiplied with the 12-bit scale in sprite
`+ac`, wrapped to 32 bits and divided by 256 with signed truncation. Zero becomes
one. The countdown receives the halfword result. The six-bit step increments;
wrapped zero becomes 63. `b3` stores an operand's low six bits as the lookup index
and uses the original width table. `e1` adds a signed 16-bit relative displacement
to the current PC. These two commands have source and synthetic coverage here;
the captured return executes six `13` commands.

Frame-directory bit `8000` selects compact part entries. Metadata begins at
record `+4+2*count`, compared with standard `+6+4*count`. The later control stream
has the same source operations in both formats: part-coordinate skips, optional
metadata bytes, and selected eight-byte auxiliary slots. Missing auxiliary
storage still reports the unreconstructed allocation. Negative frame input and
out-of-buffer accesses fail explicitly.

## Original comparisons and remaining work

Three fresh captures repeat the existing ordinary encounter route. All 124
uninstrumented control artifacts match per capture: 372 comparisons, representing
one route rather than three independent scenarios. The main recording has 586
records, of which the complete later policy accounts for 163. All 25 actor
decisions agree: 19 restorations and six flag skips. Every intermediate and final
512-byte sprite state, checkpoint, frame head, temporary/restored rate, cursor,
saved return address and observed nested stack boundary agrees. The 423 earlier
battle/transition records, including calls already open when tracing starts,
remain explicitly outside this complete-policy comparison.

The detailed recording has 248 records, with 124 in the same complete policy.
It independently compares six complete timers and four compact metadata calls
with 32 command positions. All 137 shared records agree, including registers and
observed cycle counters. The four compact calls contain only `00` part skips and
make no auxiliary writes. Auxiliary-write branches have source review and public
synthetic tests only. Cycle equality qualifies the captures; it proves no native
or original hardware cadence.

The return-time loader recording reproduces both complete party resources,
163,483 bytes, using their original source files and captured adjacent input.
Five ending output bytes differ from the earlier field-entry decode because the
adjacent input changed. The exact suffixes are retained. Live resource digests
and the first post-return RAM checkpoint agree; later menu work reuses this
memory, so final RAM is not a valid resource oracle. Every used field-resource
read separately agrees with original disc-derived bytes. The previously observed
68 field-resource byte changes remain outside that claim.

All 35 new corrupted state, source, lineage or operation cases reject. Sixteen
new public timer/return tests and the existing sprite suite total 49 passing
tests. The complete earlier EVID-REF-025 replay and its 67 corruption cases also
pass with the current source. Its changed historical source artifacts are
preserved with their original hashes.

Still required: sprite creation/rebinding and allocator ownership, optional
checkpoint/party-mode return variants, ordinary VM commands beyond this subset,
field initialization, rendering, semantic actions and readiness, combat/results,
menu/card saves, media and timing. The data restore and this later checkpoint
stage do not yet constitute a complete native encounter return or a Phase 1 exit.
