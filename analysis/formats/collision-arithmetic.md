# Field collision arithmetic — observed subset

[EVID-REF-019](../findings/EVID-REF-019.json) records original comparisons for
the selected Disc 1 executable and shared field overlay, decoded SHA256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
Field addresses below refer to that overlay at `0x8006faf0`.
The reconstruction is [collision_math.py](../../tools/analysis/collision_math.py).

| Original function | Reconstructed operation | Compared calls |
| --- | --- | --- |
| Resident `0x80048dd8`, through `0x80048d7c` | Integer vector normalization with lookup quantization | 16,422, including 79 zero vectors |
| Field `0x8007b07c` | Triangle plane height and cross-product normal | 3,688 |
| Field `0x8007b1c4` | First containing triangle and interpolated point | 1,334 across layers 0, 1 and 2 |
| Field `0x80084fac..0x80084fc8` | Normalize the plane normal again before storing it in the actor | 520 stores |

These counts combine initialization `[508,650)` and ordinary traversal
`[4300,5600)` of field 23. They count original calls, not native simulation ticks.
Each height input's three vertices matches an original source triangle. The
comparison checks source/RAM component identity, active layer pointers/counts,
input/output addresses, preserved point bytes, return sites, table bytes, and
normalization lookup index, coefficient, shift and result. Successful location
queries correlate with the nested height calculation and its normal output.
All observed queries find a triangle; no-match normal clearing is not independently
sampled. No observed height call uses a vertical triangle.

## Integer normalization

The original transfers each component's signed low halfword into the GTE and
sums its three squared values. For the observed nonnegative signed magnitude,
it rounds the leading-zero count down to an even number, derives a scale shift,
and rescales the magnitude to 64–255. Subtracting 64 selects a signed halfword
at resident `0x80056b94`. Each signed component is multiplied by that coefficient
and shifted arithmetically. The source table remains user-supplied data; it is
not embedded in the reconstruction or public tests.

Zero magnitude uses leading-zero count 32, lookup index -64 and shift -1,
interpreted by the variable shift as 31. The original reads the halfword 128
bytes before the usual table. Both traces capture that region and its exact
coefficient. Every product and output is zero regardless of the coefficient,
so the semantic reconstruction returns zero. Signed magnitude overflow remains
explicitly unsupported; the observed calls do not establish that path's
hardware exception behavior.

## Height and point location

The height helper normalizes `B-A` and `C-A` independently, then calls resident
`0x8004a480` for their cross product with a 12-bit shift. It does not normalize
the cross product again. If its Y component is nonzero, the helper evaluates the
plane equation with wrapping 32-bit products/subtractions, signed division
truncated toward zero, and a final halfword store. A zero Y component stores
height zero; that branch has source and synthetic evidence only.

The separate actor update at `0x80084fac` normalizes the cross product and stores
it at actor `+0x50`. This second operation explains why an earlier comparison
against the actor's stored normal did not match the height helper's intermediate
normal. The original traces now verify both stages independently.

Point location tests three directed edges in source triangle order. Each test
packs X and Z using addition of shifted X and signed Z, so negative Z can borrow
from the other halfword. The signed 32-bit GTE area must be nonnegative for all
three edges; edges themselves are included. The first accepted triangle wins.
The original returns zero and zero outputs on failure, which is ambiguous with
triangle zero. The analysis interface returns `None` for that case.

## Observation and remaining work

The original uses a scratchpad stack during traversal. External observation
API 2 supplies its backing memory read-only; only the first 1 KiB and its
qualified aliases are available to register-based ranges. Hardware registers,
wrapping ranges and out-of-bounds reads are rejected. API 1 remains readable
for historical RAM-only captures. This is general analysis machinery, separate
from the shipping native application.

The ordinary ramp inputs are an observation route, not the frozen slice or an
encounter-completion proof. Movement resolution, collision adjacency/attributes,
layer selection, jumping/landing, actor contact, camera behavior, rendering and
semantic readiness still require original reconstruction and validation.
Agreement with the pinned external core does not establish physical PS1 timing
or every GTE exception/flag behavior. The finding remains supported pending
independent review, and all broad Phase 1 todos stay open.
