# Bounded planar motion and sprite instruction stores

[EVID-REF-023](../findings/EVID-REF-023.json) records original Disc 1 evidence for
motion-mode selection, sprite speed and planar vectors, ordinary field velocity,
trigonometric lookups and committed sprite instruction-pointer stores. The
[readable model](../../tools/analysis/planar_motion.py) preserves integer widths;
the [verifier](../../tools/analysis/verify_planar_motion.py) separately qualifies
original sources, memory, pointers and call relationships. Full movement,
collision response and the sprite VM remain incomplete.

## Motion mode and speed

At field-overlay `80082bf8`, the motion function has actor, sprite, descriptor and
actor index in `s0`, `s3`, `s6` and `s5`. It always stores the current actor index
at `80065b08`. Actor flag `01000000` then skips the whole motion body. The 470
observed inhibited calls preserve every captured byte except that index store.

For an active call, mode 1 is the default. Actor ownership flag `4000`, held
Circle bit `0040` at `800afe9c` and the input-update word `800adb68` exactly equal
to 1 select mode 2. If actor flags intersect `1800`, an existing signed mode 1 or
2 at actor `+e8` is retained. The prefix ends at `80082c8c`, before the `+e3`
counter update. It selects register `s4` without storing a new actor mode yet.
The original window gives 456 mode-1 and 14 mode-2 selections. The following 470
active motion bodies are counted as opaque.

Sprite command `a0` is a separate namespace from field-event commands. The
resident dispatch table at `800183d8` sends it to `80021958`. With the signed
operand byte, signed sprite scale at `+82`, and rate word at `80059198`, it:

1. Multiplies `operand << 4` by `rate + 1`, retaining the low signed 32 bits.
2. Multiplies by the signed 16-bit scale, again retaining the low 32 bits.
3. Truncates toward zero by 4096, then shifts left eight with 32-bit wrap.
4. Stores sprite speed at `+18` and calls the planar vector routine.

All 19 observed commands use scale 8192 and rate 1. Operand 20 gives 327,680,
or **5.0** in 16.16 units; operand 64 gives 1,048,576, or **16.0**. These are
ground animation speed values. They are not final collision-resolved movement
or a universal per-frame displacement. The separate jumping path at
`80082ff8..8008302c` assigns other speeds and remains part of the unreconstructed
motion body.

## Source table, sprite vector and field quantization

Resident `8003f8b0` and `8003f8cc` mask angles to twelve bits and read signed
sine/cosine halfwords from 4,096 pairs at `800523f0..800563f0`. The verifier uses
that exact original table. Each captured lookup pair, index and sign-extended
result must agree with the source. Hooks in the return delay slots observe the
completed load; an earlier return-instruction hook would see the load delay.
All 2,445 sine and 2,445 cosine results compare exactly.

The sprite routine `80022974..80022a00` uses speed `+18`, signed direction `+32`
and divisor `(u32(sprite+ac) >> 7) & 0fff`. Its scalar is the signed quotient
of `s32((s32(speed) >> 4) << 8)` and that divisor, truncated toward zero. It computes:

```text
X = s32((s16(cosine) >> 2) * scalar) >> 6
Z = s32(-s32((s16(sine) >> 2) * scalar)) >> 6
```

The shifts are arithmetic and round negative fractions downward. Products wrap
before the following operation; Z negates before the final shift. The routine
stores X at sprite `+0c` before the sine call and Z at `+14` afterward. It
preserves vertical velocity at `+10`. All 567 complete vector calls, their
ordered cosine/sine calls, scalar registers, stack relationships and captured
stores compare exactly. Zero divisors fail explicitly in the authored model.

Field wrapper `80081f80..800821f4` selects the ordinary party path when descriptor
`+58` bit `40` is set and actor `+04` has neither `2000` nor `80000`. It stores the
new direction through `80021fe0`, calls the sprite routine and masks both X and Z
with `fffff000`. That preserves 16.16 fractions only to **one sixteenth** of a
unit and rounds negative components down. All 548 observed field calls take
this path with actor flags `04010400`. Their full shared captured state agrees;
the additional actor range at the selection hook supplies original branch input.
That extra range is not available at the return for every non-player actor.

Direction bit `8000` is a source-derived stop sentinel on this descriptor path:
it zeroes X/Z before reading actor flags, preserving the previous direction.
This branch is synthetically tested but unobserved here. Descriptor-bit-clear,
actor-ratio and Gear paths are explicitly unsupported; no no-op substitute is
used for them.

## Sprite instruction-pointer stores and field resources

The loop at `8002490c` first tests sprite `+9e`. A nonzero value returns without
reading a command. Otherwise it loads the command at sprite `+64` into `s2` and
forms the operand pointer in `s0`. Many subsequent paths remain opaque.

At the observed store path `80024edc..80024efc`, a byte from the source width
table `8004fc40..8004fd40` is added to the **current post-handler** sprite `+64`.
The store occurs in the jump delay slot at `80024efc`. A following loop hook
observes the committed pointer. The model does not assume that arbitrary
handlers preserve the pointer or that every VM path performs this store.

The verifier compares 10,921 additions and committed stores across twelve
opcodes. Width 1 occurs for `84`, `86`, `87`, `94`; width 2 for `a0`, `a1`, `a6`,
`b4`, `bc`, `c6`; width 3 for `ce`, `e4`. Every compared opcode agrees with its
preceding loop's original command source, and unrelated captured bytes remain
equal across the store. This establishes the observed advancement path and
widths, not the remaining handler effects. Another 11,516 loop entries are
explicitly opaque; their completeness depends on the qualified whole trace.

Fifty of these stores use the two party resources qualified in EVID-REF-022.
The other 10,871 use four resources indexed by field component 3. Original
`800712ac..800712e4` loads that component and stores its pointer at `800afb1c`.
Selection code, including `800a09b4..800a0a04`, reads a relative offset at
`component + 4 + index*4` and adds the component base.

This field's 8,380-byte logical component has five offsets: 24, 652, 3,060,
4,284 and 5,724. Its 24-byte index and each compared opcode match the source.
The captured base is `801294e0`; indices 1–4 occur in the compared stores.
The complete runtime component differs from its decoded source at **68 bytes**.
Those changes are recorded with exact offsets and both full hashes, and their
meaning remains unresolved. No whole-component equality or complete inner
sprite format is claimed. The two party resources still compare in full.

## Reproduction and limits

Use the pinned Nix shells and fresh private output paths. The existing
[control scenario](../../tests/reference-inputs/field23-control-observation.json)
and memory sampler are unchanged. Generate the trace specification with
`tools/analysis/verify_planar_motion.py prepare`, passing the qualified raw disc,
profile, map 23 and control RAM. Capture the same route with the pinned
`#observation-trace` core, then use `compare` with the party-loader and planar
captures. The finding records the exact commands and artifacts.

The `[4300,5241)` window contains 43,414 records, with no unavailable ranges,
guard failures, exhausted budget or collector errors. The comparison checks the
source code and tables, callback ordering, original pointer storage, overlapping
ranges, explicit shared range sets and nested return/stack relationships. The
corruption suite rejects all 82 alterations to arithmetic, source identity,
preserved state, call order and full-capture integrity. All 66 control artifacts, including the entire
memory stream, match the independently built uninstrumented core. Sixteen public
synthetic tests cover arithmetic, sentinel, unsupported-path and index bounds.

Still required: full active motion, collision/floor/layer/ceiling/rollback
response, history/follower behavior, camera and readiness, remaining sprite
commands and waits, field-sprite runtime changes, alternate source inputs and
independent review. Frontend-run observations do not establish hardware timing.
All fifteen broad Phase 1 todos and all nine slice proof domains remain open.
