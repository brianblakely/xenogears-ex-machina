# Field movement sweeps and triangle queries

[EVID-REF-024](../findings/EVID-REF-024.json) reconstructs the ordinary and special
movement sweeps in the qualified Disc 1 field overlay. The
[sweep model](../../tools/analysis/movement_sweep.py) calls the reconstructed
[triangle queries](../../tools/analysis/collision_query.py) and
[integer geometry helpers](../../tools/analysis/sweep_math.py) directly.
The [verifier](../../tools/analysis/verify_movement_sweep.py) compares original
inputs, intermediate operations, ordered calls and complete captured outputs.
The later position and layer integrator remains separate.

## Original coverage

Two existing input routes supply six clean instruction recordings: one sweep,
one query-area and one arithmetic recording per route. Their query or sweep
boundaries agree across recordings, including all registers and captured bytes;
only the independent event numbers differ. Neither route adds setup writes.

| Comparison | Control and jump route | East-ramp route |
| --- | ---: | ---: |
| Frontend runs | 5,241 | 6,197 |
| Sweeps | 156: 78 ordinary, 78 special | 573 ordinary |
| Nested triangle queries | 702 | 2,528 |
| Signed-area calls | 2,106 | 8,432 |
| All captured height calls | 1,450 | 4,144 |
| Height calls within these queries | 234 | 1,272 |
| Slope adjustments | 78 | 556 |
| Edge projections | 0 | 160: 103 slide, 57 stop |
| Successful / rejected sweeps | 156 / 0 | 556 / 17 |
| Independent control artifacts | 66 | 53 |

The ramp queries produce 2,351 successes and 177 failures: 160 reject a transition
to a higher special surface and 17 reject layer-zero terrain. Query loops take
one to three iterations and exercise masks 0 through 6. The ordinary ramp slopes
include 100 upward-floor selections and 456 within the 64-unit height condition.

The separately built uninstrumented core reproduces every image, full RAM dump,
external state and WAV byte. The control route also reproduces the complete
memory-sampling stream. The six instrumented recordings stay below their
callback budgets, with no failed guards, unavailable ranges or collector errors.
These are external-emulator observations; frontend run counts do not establish
the original hardware or native simulation cadence.

## Sweep selection, probes and floor

The main motion caller selects ordinary routine `8007bac0` when actor flags do
not intersect `00041800`, the actor's linked index at `+74` is `ff`, and the word
at `800adb98` is zero. Otherwise it selects special routine `8007b814`. Their
arguments are candidate velocity, actor, caller-owned edge and signed direction.

Ordinary probes run in direction order `direction-0100`, `direction+0100`, then
`direction`. Special probes run center, left, right. Angles use the original
4,096-entry sine/cosine table. Each probe adds `cosine << 6` to candidate X and
subtracts `sine << 6` from Z, with 32-bit wrap. Probe Y is unwritten stack content
and neither query reads it. The model represents that word as unspecified; it
does not claim that the original stores zero there.

Probes use query mode -1. The first failure stops probing and calls edge
projection `8007b6c4`. Both routines then query the working vector with mode 0
to obtain a floor. A failed floor query returns -1 while preserving the original
input velocity and any edge writes already made.

The ordinary routine adjusts its vector to a slope when the selected floor is
less than the signed integer actor Y, terrain bit `00200000` is set, both new
and old terrain intersect `00420000`, or the new terrain has neither of those
last bits and its floor is below actor Y plus 64. These tests follow the original
branch order. Decreasing Y represents upward movement in this field convention.
After adjustment it queries the floor again; success sets actor flag `04000000`.

The special routine uses signed actor `+ec` as a forced floor when flag `40000`
is set. Otherwise, with `800adb98` zero, it rejects a floor whose fixed-point Y
is less than actor Y. That forced-floor branch is source-derived and synthetic
tested; it does not occur in these recordings.

On success, both routines set velocity Y to `(floor << 16) - actorY`, wrapping
to signed 32 bits, and store the signed floor at actor `+72`. The verifier checks
all 312 actor bytes, 12 velocity bytes, 16 edge bytes and 48 caller bytes. Shared
descriptor, sprite, field and global captures also agree. This establishes these
output effects, not arbitrary uncaptured writes or the subsequent position step.

## Triangle traversal and terrain

Ordinary query `8007bef4` and special query `8007c694` use actor layer `+10` and
the corresponding signed triangle index at `+08 + 2*layer`. A triangle occupies
14 bytes; a vertex occupies eight. The original mesh, attribute and layer
pointers are checked against the decoded source component, including counts.

The queries add candidate X/Z to actor position, wrap to 32 bits, shift right
16 and write signed halfword output X/Z with initial Y zero. They pack each X/Z
pair using the original addition `(X << 16) + signedZ`. Negative Z can borrow
from the packed X halfword; replacing this addition with bitwise OR changes
original results. The resident NCLIP wrapper returns the low signed MAC0 word
of the packed determinant.

Three signed areas select an edge mask. Mask 0 means inside. Masks 1, 2 and 4
select adjacency halfwords at triangle `+06`, `+08` and `+0a`. Masks 3, 5 and 6
perform an extra area test against the old actor position to choose an edge.
The 47 observed extra tests are included in the 10,538 exact area results.

Terrain indices use only the byte at triangle `+0c`. Both routines read the
selected neighbor's attribute before testing whether that neighbor is -1.
The model therefore reads the corresponding preceding source bytes for that
case; it does not supply an invented zero attribute or clamp the index.

Actor layer-disable bits or the byte at `800b21cc` mask attributes to zero.
Actor flags can reject attribute flag combinations. Attribute bit `00800000`
rejects layer zero. The ordinary routine also rejects entry into terrain
`00400000` when the resulting height is above the old actor height, unless the
initial triangle already had that bit or query mode is `80`. Mode `80` bypasses
this height rejection; it still performs the final height query.

Inside selection sets the loop counter to 255, which increments to 256 for
success. A counter reaching exactly 32 rejects traversal. Mode -1 skips only
the final height query; transition-related height calls may still occur.
Failures can retain point/attribute writes performed before rejection. Edge
outputs copy two XYZ halfword triples while preserving padding at `+06` and
`+0e`. No query changes the actor's stored triangle index.

## Integer projection helpers

Edge projection computes `-atan(edgeDeltaZ, edgeDeltaX) & 0fff`, using the
original 1,025-halfword atan table. It stops and clears XYZ when the relative
direction falls outside the inclusive interval 128 through 3,968. Otherwise it
may reverse the edge, normalizes the X/Z tangent and multiplies it by the
quantized length of `(velocityX >> 12, velocityZ >> 12)`. It clears Y. The
original fourth argument is unused by this routine.

The source length helper squares signed GTE IR halfwords, adds the low result
words and calls the original approximate square root. That routine selects from
192 signed halfwords using the positive input's leading-zero count, an even
normalization shift and an integer scale. It wraps its left shift before the
final logical right shift. Zero is stored in a return delay slot: the hook at
that instruction still sees the prior value 32, so its enclosing length return
also verifies the resulting zero. Negative square-root inputs and unresolved
atan division exceptions fail explicitly in analysis.

Slope adjustment normalizes `(-X >> 8, ((floor << 16)-actorY) >> 8, -Z >> 8)`
with negation and subtraction wrapped before shifting. It multiplies by the
source length of `(X >> 8, Z >> 8)`, negates X/Z products with wrap, then shifts
right four arithmetically. The floor requery replaces its Y afterward.

The arithmetic recording verifies 3,573 lengths and signed-square outputs,
9,245 square roots including 88 zero returns, 634 slope normalizations,
103 edge normalizations and 160 edge atan results. Inputs, lookup words,
indices, scale shifts, stack relationships and output bytes agree. It also
binds each movement calculation to its enclosing reconstructed sweep.

## Verification and remaining work

The public verifier regenerates all six guarded specifications from source and
original RAM. It checks collector/core provenance, the exact input programs,
independent controls, source instructions and tables, collision component,
pointer storage, memory aliases, ordered nested operations and output effects.
Scratchpad vertex triples match source geometry; their copying lineage remains
unreconstructed. GTE register side effects and hardware timing are not inferred
from GPR and memory captures.

Thirty-six authored tests cover integer and branch boundaries, adjacency,
terrain rules, output preservation and pointer provenance. The private suite
rejects 157 semantic, omission, metadata, source and control-artifact corruptions.
Semantic trials bypass whole-file digest checks. Selected query-boundary trials
also update their cross-recording binding, so the rejection reaches the source
comparison rather than merely detecting a changed recording.

Use the pinned Nix environment. The verifier's `prepare` mode accepts `--route
control|traversal`, `--kind sweep|area|math`, the source profile/raw track and a
qualified `--ram`. Record that specification with the same input route under
`#observation-trace`; obtain its control separately under `#observation`.
Then use `compare` with `--control-capture`, `--sweep-capture`, `--area-capture`
and `--math-capture`, writing to a fresh private `--output`. Exact capture paths,
commands and hashes are retained in the finding.

Unobserved source branches include missing initial/neighbor triangles, mask 7,
the 32-iteration limit, disabled or alternative terrain flags, other query
modes and special forced floors. Full active motion/animation, later position,
floor/layer/ceiling/rollback, follower/history, camera, contact and readiness
remain required. This supported finding awaits independent review and closes
neither a broad Phase 1 todo nor a slice proof domain.
