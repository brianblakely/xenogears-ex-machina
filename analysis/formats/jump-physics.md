# Sprite loading and bounded jump physics

[EVID-REF-022](../findings/EVID-REF-022.json) binds this reconstruction to the
selected Disc 1 original, `na-slus-00664-39c547a9afc6`. It extends the separately
recovered input requests with actual sprite-source loading, animation-header
selection, impulse arithmetic and one vertical stage. Complete animation,
movement, floor selection and collision response remain required work.

The authored [arithmetic model](../../tools/analysis/jump_physics.py) uses typed
values and explicit integer widths. The [comparison tool](../../tools/analysis/verify_jump_physics.py)
contains original memory correlations. These addresses are analysis coordinates;
they do not define the native agent API.

## Original sprite resources

The resident directory routine `80028470..800284b4` selects a u16 from logical
sector 40 and stores its value minus one. Directory index 4 contains 424, so its
base is 423. The file-size path `80028738..80028808` uses
`slot = directory_base + file_id - 1`. The character loader selects file
`character_id + 5`. The two observed party IDs are 0 and 2.

| Original party resource | Source slot | LBA | Catalog bytes | Decoded bytes | Original output pointer |
| --- | --- | --- | --- | --- | --- |
| Fei, character 0 | 427 | 119504 | 56632 | 81576 | `800cb210` |
| Citan, character 2 | 429 | 119559 | 55060 | 81907 | `800df218` |

The two original decodes occur through `8001b430..8001b434`, calling the resident
decoder reconstructed in EVID-REF-014. The entry probe is in the call's delay slot,
after the argument load has taken effect. Both full decoded outputs match their
return-time hashes and all bytes in final RAM. Seventeen directory changes and
43 catalog-row observations also match source values, pointers and index arithmetic.

The last decoder groups consume **two and three bytes beyond the catalog lengths**,
respectively, followed by one speculative flag read. These bytes come from the
original adjacent allocations. The resulting last two and three output bytes
differ from decoding physical disc-sector padding. The comparison uses captured
input bytes and verifies the full output; it neither masks the tail nor invents
padding. This boundary does not establish that those bytes are meaningful
animation content.

For the observed nonnegative animation modes 0–3, resource word `+04` locates the
animation table. The u16 at `table + 2 + mode*2` locates its header relative to that
table. Source code at `80022224` installs the resource pointers, and `800245d8`
selects the header. All 61 captured headers match the fully qualified resources.
Other bundle components, header fields, modes and animation commands remain open.

## Gravity setup and impulse

The prefix `80023538..80023658` installs the header pointer at sprite `+58`,
`header + u16(header+2) + 2` at `+64`, and
`header + u16(header+4) + 4` at `+54`. Header bits 0–1 replace sprite flag bits
20–21. Header bits 2–7 form a signed six-bit gravity coefficient.

The model preserves the original ordering of low-32-bit multiplies, signed
shifts and truncation toward zero. With signed scale `s16(sprite+82)`, rate word
`u32(80059198)` and divisor `(u32(sprite+ac) >> 7) & 0fff`, the routine squares
the rate-plus-one and the integer inverse-divisor term before the final scaling.
It writes gravity at sprite `+1c`. The subsequent velocity resets, transform
updates and animation work after `80023658` are outside this recovered prefix.

Sprite command `a1` has its own dispatch namespace. The resident table at
`800183d8` maps it to `800219ac`; this is not field-event opcode `a1`.
If sprite flag `+a8` bit 0 is set, the handler first reads the word pointed to by
sprite `+7c`. A nonzero value bypasses operand scaling. A zero word falls through
to the signed operand byte. That path scales `operand << 4` by rate-plus-one and
the signed sprite scale, truncates by 4096 and shifts left eight. Both paths then
shift the value left eight with 32-bit wrap and divide by the packed divisor.
The intermediate numerator and final velocity store at sprite `+10` both compare
exactly. A missing reference or a zero divisor raises an explicit unresolved-path
error in the analysis model.

The original route observes six impulses: three for Fei and three for Citan.
All read a zero referenced word and the signed operand −83, with scale 8192,
rate word 1 and divisor 256. The result is −1,359,872 in 16.16 units, or −20.75.
The 61 gravity prefixes all yield 131,072, or +2.0 in the same units.
Different coefficients, scales, rates, divisors and nonzero references have
source-derived synthetic coverage but remain unobserved in original execution.

## Vertical stage

The field-overlay stage `8008505c..8008515c` first adds the **old** sprite vertical
velocity to actor Y with 32-bit wrap. Its terrain reader at `80080968` uses the
actor's current signed layer and triangle. A disabled-layer bit returns zero;
otherwise the low byte of the triangle's attribute halfword indexes a u32
attribute. This lookup and its returned value match the source collision mesh.

A layer change clears actor bit `04000000` before the floor test. With that bit
clear and the signed integer part of integrated Y below the already selected
floor, the stage adds gravity, sets bit `1000`, and copies the velocity to actor
`+f0`. Otherwise it clamps Y to `s16(sprite+84) << 16`, clears positive vertical
velocity, and clears bits `00400000` and `1000`. The floor branch clears actor
`+f0` unless terrain intersects `00420000`. It preserves negative velocity.
Both branches finally clear bit `04000000`.

Actor `+f0` is called a *vertical marker* in the authored model because its wider
ownership is not yet recovered. The selected floor is an input to this stage.
Later ceiling, layer, rollback and collision code can still change these results.

## Validation and remaining proof

The existing [control route](../../tests/reference-inputs/field23-control-observation.json)
uses ordinary inputs after the qualified field adapter and adds no setup writes.
Its physics window `[4300,4921)` contains 7,720 records: 61 gravity prefixes,
six complete impulses and 126 vertical updates, divided equally between airborne
and floor branches. The 61 enclosing animation calls are correlated, while
7,074 other sprite-command returns are explicitly opaque.
All vertical inputs use current and previous layer 0 and floor 0. Forty-nine
enter with bit `04000000` set; the other 77 include 63 airborne and 14 ordinary
floor outcomes. Layer changes and terrain-dependent marker retention still need
original observations.

Fei's first impulse is in frontend run 4,321, following the request in 4,317.
Its vertical stage reaches Y −118.25 in run 4,343 and floor Y 0 with zero velocity
in run 4,365. These observations use every second frontend run in this window;
they do not establish a universal game cadence or hardware timing guarantee.

The verifier checks source code, resource/header/operand bytes, complete decoder
hashes, pointer storage, overlapping ranges, original stacks and call ordering.
It compares all captured bytes across each recovered arithmetic stage, preserving
unrelated bytes. All 53 deliberately corrupted traces are rejected. Each of the
two captures matches all 65 image/RAM/audio/external-state artifacts and the
entire memory-sampling stream from the separately built uninstrumented core.
Fourteen public fixtures exercise invented arithmetic and branch boundaries.

Still required: the remaining sprite VM and formats, complete walking/running
velocity, history/follower behavior, collision/layer/floor/ceiling response,
terrain exceptions, nonzero velocity references, alternate rates, camera behavior,
readiness, hardware dependencies and independent review. No broad Phase 1 todo
or slice proof is closed by this subset.
