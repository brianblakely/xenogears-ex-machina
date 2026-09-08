# Original party movement update

[EVID-REF-026](../findings/EVID-REF-026.json) reconstructs the party path of
Disc 1 field-overlay function `80082bb8..80083178`. Its overlay identity is
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
The public [source model](../../tools/analysis/active_motion.py) composes the
previously recovered velocity, sweep and animation models. It computes every
selected call from its function-entry state; no recorded callee result is used
as an implementation input. These byte views are diagnostic reconstruction,
not the native agent API or complete structure definitions.

## Source and calling convention

Primary analysis uses the pinned Ghidra 12.1.2 environment and
`lab313ru/ghidra_psx_ldr`. Reviewed exports contain all 507 original instructions
of the main routine, bounds helper `80082494..800825ac`, and idle predicate
`8008492c..80084a40`. Partial types resolve the main arguments, signed animation
fields, motion vectors, and the pointer to a quadrilateral at actor offset `114`.
Ghidra's default convention is named `__stdcall` in this PSX language; the
observed original arguments are MIPS registers, not an x86 calling convention.

The main routine receives actor index in A0, descriptor in A1, and actor in A2.
It loads the sprite from descriptor `+04`, allocates an `88`-byte stack frame,
and saves S0–S6 and RA. The observed caller returns to `80081288`. Its candidate
vector occupies frame `+10..1b`; the edge buffer begins at `+20`. The comparison
preserves all 40 captured local bytes, including untouched padding, and checks
the eight saved register words at return.

The idle predicate also has a selective m2c review. Its typed C body is compiled
on the host with only two absolute global reads replaced by explicit inputs.
The compiled body, the independent Python reconstruction and original returns
agree. This establishes behavior for those inputs, not a binary match or the
identity of the original compiler. Generated pseudocode and original assembly
remain private.

## Update order

1. Store the current actor index at `80065b08`. Flag `01000000` inhibits the rest
   of the routine, preserving all captured actor, sprite and local state.
2. Select walking/running mode using flag `4000`, held button `0040` and input
   update value one. Flags `1800` can retain old animation mode one or two.
   Decrement the byte at actor `e3` only when it exceeds eight.
3. Evaluate prior movement and collision eligibility. The idle predicate returns
   `-1` for terrain flags `420000`, nonzero collision mode, nonzero previous
   velocity, collision-enable value other than one, a linked actor, flags
   `401800`, or a disabled current layer zero, one or two. Other signed layer
   values do not trigger those three layer-bit checks.
4. A valid requested direction, additive movement, the predicate's `-1`, or flags
   `40800` selects candidate movement. A valid direction rebuilds the sprite's
   horizontal vector; each candidate component adds the corresponding actor
   additive component with 32-bit wrap. An invalid direction uses additive
   movement alone and the previous direction's low twelve bits.
5. Check optional motion bounds. If horizontal movement is nonzero, derive the
   sweep direction from the recovered integer atan routine. A missing current
   triangle skips the sweep and selects stopping.
6. For the controlled actor, temporarily OR only party members' `0600` flag bits
   into the actor. Party indices come from `8005a448/8005a44c`; `ff` skips a slot.
   Missing required member state is an explicit reconstruction error. Select the
   ordinary or special sweep from actor flags, linkage and collision mode.
   Compute the sweep's full effects, then restore only the actor's original
   `0600` bits. Other sweep flag and floor writes survive restoration.
7. Idle, failed bounds, missing triangle or rejected sweep clears candidate and
   additive movement, clears sprite X/Z motion, sets actor `f0` to `10000`, and
   sets the previous direction's `8000` bit. Sprite Y motion and speed survive
   this stop stage; later animation changes can still alter them.
8. Clear actor layer flag `1000`. Jump state can update sprite speed and select
   the global animation mode; otherwise invalid requested direction selects the
   idle animation, and terrain `200000` selects mode six. An already-idle mode
   six restores layer flag `1000`. A signed override other than `ff` takes
   precedence. A changed mode dispatches animation unless flag `02000000` blocks
   it. The source animation model computes the complete observed sprite and
   actor effects.
9. Terrain flag `0100` arithmetically halves candidate X/Z after the sweep and
   animation. Commit X/Y/Z to actor `30/34/38`, then clear additive movement at
   `40/44/48`. Position integration is a later function.

## Optional pointed bounds

Actor `12c` bit `1000` enables a read through the **pointer** at actor `114`.
It points to four consecutive signed `(x,z)` halfword pairs, 16 bytes in total.
The helper tests the proposed fixed-point position against edges 0–1, 1–2,
2–3 and 3–0, stopping at the first negative signed low NCLIP result. Zero counts
as inside. The point and vertices are packed with `(x << 16) + signed_z`; a
negative Z borrows from the packed X halfword. Replacing addition with OR changes
the original behavior.

All 729 selected original bounds calls have this flag clear, so they return zero
without reading the pointer. Enabled bounds, negative-coordinate packing and
each rejection side have authored source-boundary tests, with no claim that the
enabled branch was exercised by these original routes.

## Original observations and limits

| Route | Active party calls | Inhibited returns | Sweeps | Main animation changes |
| --- | ---: | ---: | ---: | ---: |
| Control and jump | 470 | 470 | 156 | 24 |
| Ramp traversal | 948 | 948 | 573 | 16 |

The recordings contain 36,764 instruction observations. The full party proof
covers 2,836 calls: 1,418 active updates and 1,418 inhibited companion returns.
It computes 729 sweeps, including all 17 rejected ramp sweeps, and 40 animation
changes. Stops divide into 689 idle shortcuts and 17 sweep rejections; the other
712 active updates reach a successful sweep. All 5,868 observed idle-predicate
calls compare independently, including calls made by other actors and the later
position routine.

The ramp route also contains 1,549 movement calls for actors 14, 15, 16, 19 and
20. Their complete movement paths are explicitly excluded. No missing NPC
effect is accepted as a no-op. The 59 external follower animation boundaries
are enumerated separately from the 40 modeled main-motion changes.

The control recording uses one window. The ramp uses three contiguous windows,
`[4300,4900)`, `[4900,5500)`, and `[5500,6200)`, because all actors' callbacks
exceed a single recording's payload budget. Each window replays the same complete
scenario from cold boot and preserves its uninstrumented control artifacts.
They are one traversal route, not three independent scenarios. All 119 distinct
control artifacts across the two routes match exactly, including RAM, external
state, audio and images. The control route also matches its memory-sampling
stream. No additional game-state writes are introduced by instruction tracing.

The [public verifier](../../tools/analysis/verify_active_motion.py), invoked with
`python -m tools.analysis.verify_active_motion`, regenerates all specifications,
qualifies source/Ghidra/core/caller provenance and compares every selected
intermediate and final state. Twenty-five authored tests exercise composition
and trace integrity; local fault injection rejects coherent output corruptions,
changed arguments, altered call relationships and provenance substitutions.

Other NPC and descriptor paths, enabled bounds in actual execution, unobserved
animation/resource branches, later position/layer/floor/ceiling/rollback,
followers, contact, camera, readiness and hardware cadence remain open. Passing
this diagnostic model does not demonstrate native gameplay or complete a broad
Phase 1 todo.
