# Phase 1 progress and remaining proof

Phase 1 is **in progress**. No broad todo or slice proof is complete yet. The
[slice contract](phase1-slice-contract.md) and all nine required proof domains
remain the completion criteria; the field 23 route is still a candidate.

The current [source and state handoff](../analysis/formats/field-lifecycle.md)
organizes the field loader, event initialization, scheduling, motion/position,
encounter return and menu dependencies around related Ghidra C exports. Shared
actor/descriptor/event types, operand signatures and normal startup GP context
have been corrected and dependent functions re-decompiled. The battle and menu
overlays now have separately qualified provisional surveys. Survey coverage is
not reviewed-source completion.

The reusable C++ event library implements ten primary handlers plus actor
scheduling and batching. It matches 81,527 existing original transitions; the
connected scheduler tests retain a separate source/synthetic scope. Preserved
Python position models match all 4,385 complete captured calls, without replacing
the established motion, collision or sprite references. EVID-REF-027 records this
milestone. Its remaining blockers are initialization services and additional event
handlers, contact/encounters and snapshot ownership, battle auxiliary code and
rules, menus/card persistence, required media, timing and semantic readiness.

The table below preserves each earlier finding's original scope and limitations.

| Verified work | Current evidence | Limit |
| --- | --- | --- |
| Original source identity | Fresh CHD/container and raw-track hash checks on both supplied discs | Exact selected revisions only |
| Packed-block reconstruction | [EVID-REF-014](../analysis/findings/EVID-REF-014.json): 27 decoder calls, 374,216 bytes, original input/end/return pointers, three fields on both discs | Adjacent RAM must remain distinct from disc-sector padding; other decoder callers remain open |
| Field structural parsing | Same finding: source/RAM comparison of immutable components, actor tables, formation copy and collision representation | Does not establish complete geometry, animation, rendering or collision behavior |
| Conditional event branches | [EVID-REF-015](../analysis/findings/EVID-REF-015.json): 16,509 original calls with equal resolved operands and successor PCs | Only mode `40`, comparisons 0, 1, 2 and 5 exercised in the candidate route |
| Event state transitions | [EVID-REF-016](../analysis/findings/EVID-REF-016.json): 65,018 exact slot selections, end/reset/jump, wait and variable transitions | Immediate waits and observed selector modes; unobserved paths, actor eligibility and readiness remain open |
| Interpreter batch policy | [EVID-REF-017](../analysis/findings/EVID-REF-017.json): 29,993 batches and 73,257 exact post-handler decisions, including the original 1,025-dispatch safeguard | Handler effects remain opaque original inputs; actor eligibility, blocking gates and readiness remain open |
| Music loading and event wait | [EVID-REF-018](../analysis/findings/EVID-REF-018.json): 2,921 extended-prefix increments, 993 pending music waits, 35 polls/commits, two complete sequence inputs and the initial 8,192-byte wave block | Ready wait branch, full wave transfer, decoding/playback, Mono/Stereo/Wide and player readiness remain open |
| Collision arithmetic | [EVID-REF-019](../analysis/findings/EVID-REF-019.json): 16,422 normalizations, 3,688 height calculations, 1,334 successful point-location queries and 520 actor-normal stores | Full movement, collision response, jump rules, unsuccessful queries and hardware exception behavior remain open |
| Ordinary encounter route | [EVID-REF-020](../analysis/findings/EVID-REF-020.json): contact, four-enemy battle, victory/rewards, field return, movement and party menu; exact event-slot insertion and six battle-request calls | Combat/AI/reward formulas, complete contact and return behavior, menu semantics and the frozen slice manifest remain open |
| Field control requests | [EVID-REF-021](../analysis/findings/EVID-REF-021.json): 4,669 exact direction/jump handler calls, 78 encounter early returns and three terrain predicates; three accepted jumps and one airborne rejection | Full physics, active encounter selection, looping wrapper execution, alternate/counter branches and complete readiness remain open |
| Sprite loading and jump stages | [EVID-REF-022](../analysis/findings/EVID-REF-022.json): two complete party sprite decodes, 61 gravity prefixes, six impulses and 126 vertical updates with exact source/state/call correlations | Complete animation, walking/running velocity, floor selection, collision response, alternate physics inputs and hardware cadence remain open |
| Planar motion and sprite PC stores | [EVID-REF-023](../analysis/findings/EVID-REF-023.json): 470 motion prefixes and 470 inhibited returns, 19 speed commands, 567 sprite vectors, 548 field vectors, 4,890 source lookups and 10,921 committed PC stores | Full active motion/collision, alternate paths, remaining sprite commands and field-resource runtime changes remain open |
| Movement sweeps and triangle traversal | [EVID-REF-024](../analysis/findings/EVID-REF-024.json): 729 sweeps, 3,230 queries, 10,538 signed areas, 5,594 heights and exact nested edge/slope arithmetic across two original routes | Later position/layer integration, animation, source-only branches, scratchpad copy lineage and hardware timing remain open |
| Sprite animation, facing replay and matrices | [EVID-REF-025](../analysis/findings/EVID-REF-025.json): 69 complete observed animation changes, 1,479 facing updates including 65 replays, 198 frame scheduling calls and 2,889 matrices | Ordinary animation VM, other resources/frame formats/rendering, full motion composition and hardware cadence remain open |
| Complete party motion updates | [EVID-REF-026](../analysis/findings/EVID-REF-026.json): 2,836 calls from entry through return, including 729 source-computed sweeps, 40 animation changes and 5,868 idle predicates across two original routes | 1,549 NPC movement calls are explicitly excluded; enabled bounds, later position/layer integration, followers/contact/camera and timing remain open |
| Reference instrumentation | Read-only region hashes, source boundaries and guarded instruction observations | Pure analysis infrastructure; native runtime remains unimplemented |

The decoder experiment identified a real boundary issue: field 23's last
component reads beyond its source file into adjacent RAM. Four resulting output
padding bytes differ from the physical disc-sector interpretation. Using the
captured original input reproduces the full output exactly. No padding mask or
invented zero suffix is used. The independently captured control runs still match
full RAM, external state, audio and all 30 decoder-scenario PNGs exactly.

These findings have reproducible local comparisons and public synthetic tests.
The event-state comparisons preserve all captured actor bytes and interpreter
controls, and all variable/type bytes for that group. Eleven corrupted original
input cases are rejected. All three new runs match the uninstrumented control's
RAM, external state, audio and 18 images exactly.
The two batch-policy runs also preserve those control outputs. Eight additional
corruptions are rejected. The original initialization mode continues through 993
set break requests and reaches one instruction safeguard, so startup readiness
cannot be modeled as a universal yield after a break request.
The music comparisons establish that those 993 waits test the music-loading
result. The original later completes two sequence loads; this route does not
observe the wait resuming afterward. Twenty-one new corruptions are rejected,
six sequence service calls correlate with the policy, and all three music
captures preserve the full control outputs. The earlier branch and event-state
comparisons still pass with the extended decoder.
The collision comparisons cover initialization and ordinary ramp traversal. They
verify signed integer operations, original triangle order, three collision layers
and a second normalization before storing an actor's floor normal. All 79 observed
zero-vector normalizations return zero. The scratchpad observation extension
reads 15,600 ranges, and both runs preserve every control capture artifact.
Twenty-two corrupted traces are rejected; unsuccessful point queries and vertical
planes remain unobserved.
The encounter route now reaches victory and returns to field 23 through ordinary
inputs. Its five pending battle requests wait for music loading; the sixth accepts
selector zero. The original player position is preserved on return, and subsequent
movement and party-menu entry work. All 124 control artifacts match exactly and
15 corrupted traces are rejected. The source comparison also passes before battle
and at both return checkpoints. This supplies an observed route for further
reconstruction; it does not establish combat, reward or menu rules.
The control reconstruction now distinguishes ownership, inhibition, held and
newly pressed buttons, terrain restrictions and direction selection. It preserves
all captured actor/control bytes, including 489 inhibited calls and 2,335 calls
without control ownership. All 65 original output artifacts plus the entire
memory-sampling stream match a separately built uninstrumented core, and 22
corrupted traces are rejected. Fourteen public fixtures cover source-derived
boundaries, including unobserved counter and alternate-mode behavior. The
reconstructed requests remain separate from velocity, animation and collision
integration; active encounter selection fails explicitly as unrecovered.
The jump reconstruction now binds both party sprites to their original disc
resources, including the decoder's actual adjacent input bytes. All 163,483
decoded bytes, 61 gravity prefixes, six impulses and 126 vertical stages compare
exactly. The source headers distinguish the observed idle, walk, run and jump
selections. Each new capture preserves all 66 control artifacts, and 53 corrupted
traces are rejected. Fourteen additional public fixtures cover arithmetic and
branch boundaries; complete animation, floor selection and collision response
remain outside this recovered subset.
The planar reconstruction adds the original mode prefix, signed speed scaling,
table-based vectors and field quantization. Every observed width-based sprite PC
store matches its preceding source opcode and committed memory. Four additional
field sprite resources supply 10,871 of those stores; their source index and
used opcodes qualify separately from 68 unexplained runtime byte changes.
The remaining 50 stores use the two fully qualified party resources. All 82
corruption cases are rejected and all 66 control artifacts match. Sixteen new
synthetic tests cover rounding, overflow, source-index bounds and explicit
unreconstructed branches. The other 11,516 VM loop entries and 470 complete
active motion bodies remain opaque in that finding.
The movement-sweep reconstruction now calculates every nested query and edge or
slope operation directly from source. The ramp route includes 103 edge slides,
57 stops and 17 rejected sweeps; failed queries preserve the original order of
point, attribute and edge writes. Separate arithmetic captures bind 3,573 lengths,
9,245 square roots and 737 normalizations to their inputs and enclosing sweeps.
All 119 independent control artifacts match, 157 corrupted source/trace/control
cases are rejected and 36 authored tests pass. Full active motion, animation and
the later position/layer integrator remain separate obligations.
The animation reconstruction now includes every observed header and selection
effect, facing replay, frame-list operation, previous-frame metadata update and
sprite matrix. Ghidra type review resolves the observed frame-list `GP` address;
the source model preserves matrix padding stores, replay timer restoration and
packed-stack reload behavior. The two recordings share 3,088 exact call records
and preserve all 66 control artifacts. Thirty-three authored tests pass and 67
corruptions are rejected. Unobserved source branches remain distinguished from
the observed route; the ordinary VM remains open.
The party motion reconstruction now composes the recovered source models from
function entry through all captured intermediate and return states. It verifies
1,418 active updates and 1,418 inhibited companion calls, including all 17 ramp
sweep rejections, temporary party-flag restoration and 40 complete animation
changes. All 5,868 idle-predicate calls also agree with compiled, reviewed m2c
output. Twenty-five authored tests pass and 73 corrupted cases are rejected.
All 119 distinct control artifacts match; three contiguous ramp recordings are
one route, not independent scenarios. The 1,549 other actor movement calls remain
explicitly unreconstructed. Optional enabled bounds have source-boundary tests
but no original-run claim; later position/layer integration remains required.
They remain **supported**, with independent review outstanding. Neither a passing
tool test nor an observed subset closes a broad Phase 1 requirement.

Continue from the [recovery inventory](../analysis/recovery.json). Required work
includes the remaining script handlers and scheduler stages, field movement/geometry/
visual behavior, encounter/return lifecycles, combat state/formulas,
menus and save/load, media and Mono/Stereo/Wide paths, timing, semantic readiness
and setup. Source loading, symbols and formats must be extended beyond the
currently observed components. No unsupported opcode may become a no-op.
