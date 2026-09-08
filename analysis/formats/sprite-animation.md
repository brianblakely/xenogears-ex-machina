# Sprite animation selection, facing replay and matrices

[EVID-REF-025](../findings/EVID-REF-025.json) reconstructs the observed animation
changes, facing updates, frame scheduling and matrix updates on the existing
Disc 1 field 23 control route. The models are authored from original source
review in Ghidra and compared with original execution. Automatic decompiler
output remains a private review aid.

The public [verifier](../../tools/analysis/verify_sprite_animation.py) uses the
[animation model](../../tools/analysis/sprite_animation.py),
[replay and frame model](../../tools/analysis/sprite_replay.py), and
[matrix model](../../tools/analysis/sprite_matrix.py). These are diagnostic byte
views of original storage, not native gameplay or a native agent API.

## Qualified observations

Two read-only recordings use the same 5,241-run input program and unchanged
scenario setup. The broad animation recording contains 9,418 records. A focused
replay recording contains 4,501 records, including interior frame-list and frame
metadata observations. All 3,088 shared entry/return records agree in registers,
captured memory and frontend run. Their event numbers differ because the hook
sets differ. These are two recordings of one route, not independent scenarios.

| Reconstructed operation | Original calls |
| --- | ---: |
| Field animation selection | 69: 24 player, 45 companion |
| Resource-binding fast path, animation selection, header installation | 69 each |
| Facing orientation | 1,479 |
| Facing command replay | 65 |
| Frame scheduling | 198 |
| Frame lookup | 119 |
| Frame auxiliary clearing | 69 |
| Previous-frame metadata application | 33 |
| Sprite matrix update | 2,889 |

All 65 replays belong to facing updates outside the 69 animation changes.
Reconstruction includes their 169 loop checkpoints, 36 frame-list visits and
205 frame metadata checkpoints. All 165 frame-list insertions are checked.
Every recorded helper call is source-calculated; captured return effects are
never substituted for an unreconstructed helper.

Each recording preserves all 66 control artifacts from the independently built
uninstrumented core: images, RAM, external states, audio and the complete memory
sampling stream. Source guards, callback budgets, pointer storage, overlapping
memory views, stack frames and saved return addresses also qualify. These counts
do not establish native ticks or original hardware timing.

## Ghidra source and type review

The primary environment is pinned Ghidra 12.1.2 with `lab313ru/ghidra_psx_ldr`,
using the exact resident executable and a separate address space for field
overlay `38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
See [the workflow](../../docs/reverse-engineering.md). The verifier checks every
instruction in the 21 selected export entries against original bytes, including
entries retained for related or still-unresolved functions. Their inclusion
does not claim that every exported function is fully reconstructed.

Partial source types distinguish the 180-byte sprite prefix, 312-byte actor and
92-byte field descriptor. The trace's 512-byte sprite view also includes pointed
renderer, binding, auxiliary and other storage; it is not a claim that the
original sprite structure itself has that size. The PSX compiler specification's
`__stdcall` name denotes its register/stack convention, not an x86 ABI or proof
of the original compiler.

An explicit runtime correction matters in frame scheduling. The loader's
default `GP=0` made early pseudocode display a list head at address `20`.
Every observed entry in the focused trace instead has `GP=80059170`, so the
original `GP+20` load/store addresses `80059190`. Reviewed Ghidra function
contexts use that observed value. It is not assigned as a universal property
of every function or execution context.

## Header and animation selection

Resident selection `800245d8` uses resource pointer `sprite+48`, the current
resource at `+44`, and the binding at `+24`. The binding's word `+10` points to
the animation directory. That directory must equal the original resource base
plus its word at `+04`. Animation headers are selected by a relative halfword at
`directory+2+2*animation`. The selected animation byte is stored at `sprite+af`.

The observed binding calls all reuse their current resource with platform mode
zero. Resource initialization `80022224` is not called. A different nonzero
resource, negative animation selection, alternate platform or model dispatcher
fails explicitly in the public model.

Header installation `80023538` includes the previously recovered gravity
prefix. It installs header, command and frame pointers and header mode bits.
Header bit `0800` preserves the four motion/speed words at `sprite+0c..18`;
otherwise they are cleared. With a renderer present, clear bit `1000` resets
its three angle halfwords and rebuilds its matrix. Mode-one renderers reset
their bytes `+3c/+3d`. Their eight auxiliary entries are cleared only when a
pointer exists and sprite flag `00100000` is clear.

The header sets stack cursor `+8c` to 16, halfword `+30` to zero, and timer
`+9e` to one. Its final `+a8` mask preserves facing-group bits 17 through 19.
Preserving those bits explains why the observed animation changes need no
facing replay. A separate auxiliary pointer at `sprite+7c`, when enabled by
flag bit zero, has words `+00/+04` and halfword `+0c` cleared; intervening bytes
are preserved.

Field wrapper `800821f4` requires descriptor flag `0040`. It clears actor jump
flag `0800` under the original animation/global conditions, normalizes animation
255 to zero, then applies the ordinary selection unless actor flag `01000000`
suppresses it. Actor flag `2000` selects an unreconstructed model dispatcher.

## Facing and replay

Resident `800223b0` stores the signed angle at `sprite+80`. With a resource
present, header mode controls frame-bank selection:

| Mode | Source rule |
| --- | --- |
| 0 | One bank; flip when `(angle+1024)&4095` exceeds 2048 |
| 1 | Four groups from `(angle+1536)>>10`; group 3 uses group-1 frames with a flip |
| 2 | Eight raw groups from `(angle+1280)>>9`; raw groups 5, 6 and 7 map to 3, 2 and 1 with a flip |
| 3 | No bank-selection case; retains the source fallthrough behavior |

The stored group occupies bits 17 through 19 of `+a8`. Relative frame-bank
halfwords are based at `header+4+2*index`. The rendered flip bit at `+3c` is
the XOR of bits 3 and 2 from `+ac`. Null-resource early return occurs before
that final render-bit update.

If the facing group changes, the routine remembers the command pointer and
six-bit replay ordinal. It resets the ordinal, sets the six-bit frame index to
63 and restores the command pointer from the header. It calls `80022660` to
replay to the remembered position, then restores the original timer `+9e`.
The replay's frame and queue effects remain; its temporary elapsed timer does
not replace the original timer.

Replay frame commands below `80` advance the command pointer by one. Groups
`00..0f` increment the rendered frame, `10..1f` increment the six-bit frame
index and look up a frame, `20..2f` decrement the rendered frame, and `30..3f`
only advance time. Their duration is the low nibble plus one. Commands `40..7f`
reuse S3's duration, including its incoming register value when no earlier
command set it. That source-derived case has authored fixtures but is not
observed on this route. A missing required incoming value fails explicitly.

Frame commands increment the six-bit replay ordinal; wrapping zero is replaced
by 63. Timer writes wrap to a halfword. Terminators `80..82` return immediately;
`86`, `87` and `97` also stop at the target pointer even if the ordinal differs.
`b3` installs a six-bit frame index. `be` supplies a nine-bit frame, flip and
duration. Other special commands advance by the supplied original width table.
That skip behavior belongs to this replay routine and does not establish their
ordinary animation-VM side effects.

`e2` pushes the low 24 bits of the return pointer and applies a signed relative
offset. The source decrements the byte cursor by three, sign-extends it for
addressing and reloads it between byte stores. The command pointer is also
reloaded before adding the offset. Authored underflow fixtures preserve the
resulting aliases when a store hits the cursor or command pointer itself.

The observed replay streams execute `a0` 42 times, `15` 30 times, `13` 20 times
and `05` 12 times. Other replay cases are distinguished as source-derived,
unobserved behavior.

## Frame scheduling and metadata

Frame lookup `80022d44` reads a halfword from the selected bank using the masked
six-bit index in `+a8`. Its low nine bits select the frame; bit `0200` supplies
the frame flip. It updates the XOR render flip and calls frame scheduling.

Scheduling `8001d2b0` only queues renderer mode one; other modes store frame zero.
Pending flag `00100000` is cleared first, with an auxiliary clear when pointed
storage exists. If queued flag `00020000` is set, the function searches the
list at observed `GP+20`, following renderer word `+38`. A found sprite keeps its
list position and may apply metadata from its previous frame. Otherwise the
sprite is prepended and its queued flag is set. The frame is stored as a
halfword in either ordinary case. A set queued flag alone does not prove that
the sprite is already in the current list.

Previous-frame helper `8001f8e8` reads the binding's frame directory, checks the
frame against its low nine-bit count, and selects a relative frame record.
For the observed format, the record's low six bits give its part count. Metadata
follows a four-byte table entry per part and a six-byte prefix. High-bit metadata
commands can skip optional bytes or update one of eight auxiliary entries:
coordinate bytes at `+00/+01` and a halfword at `+06`, scaled left four. Other
entry bytes remain intact. Record bit `80` changes the number of bytes skipped
at each part's ordinary data. Alternate frame formats and auxiliary allocation
remain explicitly unreconstructed.

## Matrix arithmetic and limits

Rotation `8003f738` uses the original 4,096-pair sine/cosine table. Products wrap
to signed 32 bits before arithmetic shifting by 12; coefficients are stored as
signed halfwords. Negations occur before the shift where the source places them.
Changing that order loses negative fractional results.

Row scaling `8004974c` and column scaling `80049dcc` use different vector
components. Both store the first eight results as halfwords, then store the
ninth result as a full word at matrix `+10`, overwriting padding at `+12/+13`.
Translation words `+14..1f` remain intact. Sprite updater `80022090` applies row
scaling and, for a nonzero extra unsigned scale, column scaling by `extra>>1`.
The observed matrices use unit row scales and no extra scale; authored fixtures
exercise nonunit scales, overflow, rounding and padding. The alternate GTE
matrix-product branch remains unreconstructed.

Thirty-three authored tests cover model boundaries and trace provenance. The
private fault suite rejects 67 semantic and source/control/Ghidra integrity
corruptions. Semantic trials alter relevant bytes consistently across aliases
and bypass whole-file digest rejection, reaching the reconstructed effects.
Final-source comparison, fault results, exact commands and artifact fingerprints
are recorded in the finding.

Use `verify_sprite_animation.py prepare --kind animation|replay` with the exact
raw source/profile, `--map 23`, qualified `--ram` and a fresh private `--output`.
Record each specification with the existing control input program and memory
sampler. `compare` requires the control, loader, animation and replay captures
and the three reviewed Ghidra exports. Run all tools in the pinned Nix shells.

Remaining work includes resource rebinding and lifetime, alternate rendering
and frame paths, the ordinary animation VM, complete frame/pose geometry,
texture/palette and primitive generation, field-resource runtime changes, and
composition with full movement and later position integration. Independent
review, native execution and all broad Phase 1 and slice gates remain open.
