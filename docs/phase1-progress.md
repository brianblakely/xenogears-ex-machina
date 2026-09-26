# Phase 1 progress and remaining proof

Phase 1 is **in progress**. No broad todo or slice proof is complete yet. The
[slice contract](phase1-slice-contract.md) and all nine required proof domains
remain the completion criteria. The exact field 23 encounter/victory/return/menu
base route is now fixed in the existing manifest against its original input
schedule and captures. The complete slice is still a candidate: its required
extensions and alternatives have not received final source qualification and
independent acceptance review. All 120 Phase 1 facets remain required, all nine
manifest proofs are `defined`, and `review.exit_reviewers` is empty.

## Current checkout checkpoint

The merged checkout inspected on 2026-09-25 contains substantial reconstruction
beyond the latest published finding, EVID-REF-046. Existing C++ includes the field
frame and main loop, loading/reloading and mode dispatch, sound ticks and
interrupts, battle turn/menu/result steps, menu/card operations and movie paths.
These are recovery work within Phase 1. Their presence does not establish a
complete lifecycle or qualify the later private reports for acceptance.

Preserve and rerun the existing reports before capturing or reimplementing:

| Existing private checkpoint | Recorded result and limit |
| --- | --- |
| `.local/execution/p1music-reports/{dlg,dlg-after-music,walk,ret}.json` | The frozen `f699718` runner records 99, 48, 99 and 65 consecutive frames, all 618 entry/exit boundaries matching, with `supplied_state_bytes: 0`. These start from four separate original images; they are not a continuous full route. Current merged-source replay and independent methodology/source review remain required. |
| `.local/execution/p1fieldov/reports-bca8d71/transition-chain.json` | The frozen `bca8d71` runner records 35 frames and 69 exact boundaries without intermediate state imports. Qualify the ordinary transition, source/resource lifetime, setup and readiness as one reviewed route extension. |
| `.local/execution/p1-merge-frozen/reports/` | Per-call reports cover additional decoder/disc/interrupt/sound, battle turn and menu steps, defeat, menu items/equipment, save/load payloads, sound modes, movie decisions and transition stages. A report name or matched subset does not close its parent operation. Retain stops and unfinished presentation/results paths. |
| `.local/execution/p1menuov/reports-v2/` | Whole-menu attempts remain failing or incomplete. `movie-transition` first differs at frontend run 1820, `sound-stereo` at 2630, `load` at 3391 and `save` at 8632. Each has a non-null `first_divergence` despite runner status `completed_boundary`. `menu-actions` matches 594 of 1,294 recorded frames before hardware-read ordering diverges. `encounter` and `save-card1-failure` match 504 and 2,201 recorded frame entries but exhaust platform input without an original return comparison. |

The whole-menu report's runner status is an execution outcome, not comparison
acceptance. Inspect `first_divergence`, expected versus matched boundaries,
platform-input consumption and the original return. A truncated capture, exhausted
service input or successful subprocess cannot qualify a complete menu lifecycle.
Do not overwrite these historical reports when rerunning the merged source.

| Slice proof domain | Remaining required work on the frozen route |
| --- | --- |
| Source and loading | Rerun and independently review the existing decoder, disc completion/interrupt, loader/reloader and mode-dispatch source together. Close initialization, allocation/release and every required resource lifetime across entry, transition, battle, menu and return; per-call heap/read agreement does not establish these lifecycles. |
| Field representation | Qualify the existing frame/model/sprite/dialogue drawing and GTE reconstruction against the used geometry, materials, textures/palettes, camera, sprite/animation formats and visual references. Close required variants and source-to-runtime changes; preserve every owned byte and original write. |
| Script execution | Qualify the existing disassembler and every instruction, branch, typed operand, scheduler/wait, dialogue control, trigger and resumed side effect reached by the complete route and alternatives. Base-route per-call agreement in EVID-REF-042 does not cover remaining source-only branches or all dialogue controls. |
| Field behavior | Rerun and review the existing continuous frame and ordinary transition paths, then connect actual initialization, all required walking/running/jumping/collision/camera/interaction boundaries and full return. Close action eligibility and readiness; retain the complete movement-alternatives requirement. |
| Encounter and return | Qualify and connect existing turn scheduler, input/target selection, escape, defeat and result steps beyond the EVID-REF-043/046 per-call subset. Finish required screen contents, formulas/AI branches, cleanup and return readiness. The HP-reduced defeat probe described in `analysis/formats/battle-actions.md` is an analysis probe, not ordinary progression or a reviewed setup contract. |
| Menu and persistence | Resolve the whole-menu mismatches and input-order stops above; connect field caller, menu ownership, inventory/equipment and actual card save/load round trip, checksum and failure cases. Payload comparisons and matched prefixes do not prove the complete card/menu lifecycle. |
| Required media | Qualify existing music/sample/effect, sound-mode and movie reconstruction, including complete lifetimes, ordinary FMV trigger/framing/completion and return to gameplay. Matched Mono/Stereo/Wide parameter calls do not establish the required matched unprocessed signals, panning/polarity/reverb/master/streamed-FMV routing or Wide playback model. |
| Time and services | Rerun and review the existing controller, disc, GPU, sound-tick and interrupt implementation with qualified external inputs and exact ordering. Extend the EVID-REF-044 hardware-intent census to required paths; distinguish logical cadence and readiness from HLE frontend counts and hardware timing. |
| Reproducibility and remainder | Freeze and independently review the entire route and all ten required extension/case records, publish qualified findings for accepted WIP, reconcile the source/format/opcode/content inventory and complete source-qualified Phase 4 ownership. All Phase 1 facets and integrated acceptance remain open. |

The [frozen manifest](../analysis/slices/forest23.json) retains route extensions,
alternatives and proof obligations. Every row above is still open.

Agent-state recovery must identify resident versus mode-local ownership, valid
actions and disabled reasons, menu/dialogue control, spawn/transition
preconditions and subsystem initialization. Source-correlated C++ fields are a
starting point; the required findings must separately establish assets available,
map initialized, event completion, actionable dialogue/menu, battle readiness and
player-control readiness. The existing guarded field adapter proves only its
documented map initialization. No native Phase 2 API or readiness claim follows
from it or from the matching frame windows.

The Phase 4 recovery tasks own the source-qualified whole-game remainder after
the slice is complete. The manifest explicitly assigns world-map formats and
minigame rules to Phase 4; it does not yet map every remaining symbol, format,
instruction and content obligation to a source-qualified Phase 4 owner. Preserve
that distinction when updating `analysis/recovery.json` and the content inventory:
required slice gaps stay in Phase 1, and each outside-slice remainder needs source
identity/coordinates, evidence, a Phase 4 facet and a next experiment. The
inventory validator currently reports valid structure with 659 incomplete entries
(8 symbol, 10 format, 14 behavior and 627 instruction entries), not complete
discovery or acceptance.

## Published findings and their original scope

[EVID-REF-041](../analysis/findings/EVID-REF-041.json) moved connected comparison
to complete memory images, and [EVID-REF-042](../analysis/findings/EVID-REF-042.json)
completes its compared field-update and move-phase calls. From each original entry image, the C++
program reproduces the exact exit image, GTE state and return value of all 2,589
route field updates and 2,589 move phases, all 2,836 control/ramp route calls,
all 1,418 held-out calls, all 413 initialization and post-battle event passes,
every resident heap allocation and release, all 11 music stops and the return
checkpoint pass, with no stops. This includes dialogue windows, the sound-driver
paths the field reaches and the complete music change of opcode `75`.
[EVID-REF-045](../analysis/findings/EVID-REF-045.json) adds the resident disc
read chain: 39 of the route's 42 file reads match exactly and the other three
stop explicitly at the completion interrupt of the previous read. Evidence
remains per call. The later frame, sound-tick and stream work is recorded in the
current-checkout section above and does not expand this finding's scope.

[EVID-REF-043](../analysis/findings/EVID-REF-043.json) brings the encounter
into the shared library. The [battle source](../analysis/formats/battle-actions.md)
reproduces, from each original entry image, all four party attacks of the frozen
encounter (commit, resolver, physical formula with hit roll, attack, defense,
element and rand variance), all four knockouts, all six alive-mask updates
including the victory, all 2,080 per-frame ATB ticks and 4 turn-timer
reloads, the reward totals and the victory reward call:
experience split, a level-up with rand stat growth and the write-back to all 11
persistent character records in the resident game data (`8006d634`, `2358`
bytes, now owned whole).
[EVID-REF-046](../analysis/findings/EVID-REF-046.json) adds enemy turns: on an
alternative input where the party defends, all 20 enemy AI script runs
(`800799c8`) and all 24 action commits (20 enemy, 9 of them through formula
type 3, and 4 party) match exactly. On an input that escapes, every hooked battle, teardown
and field-return call also matches. Remaining script operations and formula
types, the escape decision, defeat, the turn scheduler, menu input, the result screens and full return
readiness remain required work.

An earlier connected milestone is the [C++ return sequence](executable-reconstruction.md)
recorded by [EVID-REF-040](../analysis/findings/EVID-REF-040.json). A shared program
owner invokes the original return caller: snapshot data restore computes all 25
actors and represented gates, then the same state feeds all 25 consecutive sprite
factories. Factory arguments and intermediate environment state are computed,
not supplied per call. The 26 restore/factory checkpoints and upload requests
match qualified original observations; the selected C++ factory boundary returns.
This does not compare final whole-field state or execute the separate `800a3c8c`
checkpoint pass. Initial actor/shadow storage and heap results remain explicit
external inputs; complete return/readiness and every slice proof remain open.
Original snapshot bytes are not native persistence.

`Program` also composes existing event scheduling, core/call/control/divisor
handlers, battle requests/continuation and music waits using resident variables,
requests and mode-local controls. These entry/ownership tests are synthetic;
they do not promote whole-scheduler or field-update original agreement.

The existing C++ library now also contains the packed decoder and bounded event/
collision parsers, actor defaults with RNG and initial floor queries, and primary
`71` battle requests. The parsers feed the same event program and collision types
used by the recovered handlers and actor initialization. The original actor-to-
event projection is shared by initialization and field snapshot restore. Original
comparisons cover all 27 recorded decoder calls (374,216 bytes), all 25 actor
initializations and 50 floor queries, and all six battle requests with thirteen
boundaries. These close reusable source boundaries, not complete loading,
physics, combat or readiness. Python references and historical findings remain.

The library also contains reviewed [sprite construction and checkpoint source](../analysis/formats/sprite-construction.md),
[motion stages](../analysis/formats/field-motion-source.md), and field-control
handlers. [EVID-REF-034](../analysis/findings/EVID-REF-034.json) records their
original comparisons and independent integration review. The scheduler, control
handler and motion prefix share one input-update word; ordinary sprite A0/A1
commands execute the same recovered speed and impulse functions. Constructors,
binding, facing, timers and later checkpoints now run in C++.
[EVID-REF-036](../analysis/findings/EVID-REF-036.json) originally qualified factory
publication, bounds and part allocation for actors 0–18. EVID-REF-040 connects
the remaining six factories on the selected return route, including sprite
commands `96`, `FC`, `A3`, `BC` and `94` and their encountered task/resource effects.
Other command selectors and routes, cleanup and readiness remain required,
along with reusable full movement, sweep and position source.

The [music callers and wave callback](../analysis/formats/field-media-source.md)
now connect to the recovered [resident CD ring](../analysis/formats/disc-stream-source.md).
Original comparisons cover every observed wave callback, chunk query and release,
including complete staging buffers and ring metadata. Allocation mode is forwarded
to the game heap unchanged. EVID-REF-035 and EVID-REF-038 preserve the boundary:
CD producers, heap internals, resident audio, complete media ownership, sound modes
and FMV remain required source.

The [battle continuation](../analysis/formats/battle-continuation.md) now uses
real extended-7F and primary-86 handlers in the shared event context.
[EVID-REF-033](../analysis/findings/EVID-REF-033.json) compares the original
ready wait and unequal variable-zero branch after return, including complete
captured storage and all 124 uninstrumented control artifacts. Battle result
and pending-word producers remain unresolved; synthetic pending/equal paths do
not establish additional observed outcomes.

The current [source and state handoff](../analysis/formats/field-lifecycle.md)
organizes the field loader, event initialization, scheduling, motion/position,
encounter return and menu dependencies around related Ghidra C exports. Shared
actor/descriptor/event types, operand signatures and normal startup GP context
have been corrected and dependent functions re-decompiled. The battle and menu
overlays now have separately qualified provisional surveys. Survey coverage is
not reviewed-source completion.

The reusable C++ event library implements ten primary handlers plus actor
scheduling and batching. It matches 81,527 existing original transitions; the
connected scheduler tests retain a separate source/synthetic scope. Separate
`05/06/0D` call/return handlers share the same actor stack and interpreter controls;
their nested-call and failure-boundary tests pass. EVID-REF-037 records source
review only: the frozen original route did not invoke those handlers. Preserved
Python position models match all 4,385 complete captured calls, without replacing
the established motion, collision or sprite references. EVID-REF-027 records this
milestone. Its remaining blockers are initialization services and additional event
handlers, contact/encounters and complete return ownership, battle auxiliary code and
rules, menus/card persistence, required media, timing and semantic readiness.

The field return C++ module now captures, parses and restores the original data
snapshot. [EVID-REF-028](../analysis/findings/EVID-REF-028.json) records exact
comparison of all 25 actors, the full 14,340-byte supplied storage, five global
regions and all event variables, with independent source and replay review.
Sprite recreation, complete ownership and control readiness remain separate
requirements. This is an original-format correlation module;
raw actor/global pointers are not a native state API or persistence format.

The later [sprite checkpoint stage](../analysis/formats/sprite-return.md) now
matches all 25 actor decisions and 19 sprite restorations on that route.
[EVID-REF-029](../analysis/findings/EVID-REF-029.json) composes animation selection,
six ordinary timed commands, frame-list updates and compact metadata. Original
return-time decoding qualifies both current party resources, including changed
adjacent-input suffixes. This leaves sprite creation/rebinding, remaining VM
commands, return variants and readiness open.

The [actor-default state](../analysis/formats/actor-defaults.md) now matches all
25 return-time initializations, 25 RNG steps and 50 floor queries, including all
captured actor, descriptor and local scratch bytes. EVID-REF-030 preserves the
source-only status of no-match, zero-query and Y-replacement branches. Seventy-six
corrupted observations/results and 19 independent wrong-model probes reject.
Sprite creation/rebinding, allocator ownership and complete entry readiness remain
the next connected dependencies; no complete slice domain is promoted.

The [battle-request reconstruction](../analysis/formats/battle-request.md) now
matches all six original primary-71 calls, including the intermediate mode latch
and every captured actor, bytecode and control byte. EVID-REF-031 independently
reviews the source, original replay and address-based projection. The public
disassembler now follows both retry and continuation paths. Other gate and
variable-selector cases remain source/synthetic; combat and complete scheduler
and return readiness are still required.

The [recovery inventory checks](recovery-inventory.md) now validate incomplete
entries, original dispatch fingerprints, status labels and evidence/source links.
Explicit dependency queries report the affected coverage as blocked, including
unlisted symbols, formats and instructions. Synthetic fault tests exercise these
diagnostics. Complete discovery, runtime behavior and P01-T15 remain open.

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
| Field return data | [EVID-REF-028](../analysis/findings/EVID-REF-028.json): original snapshot writer, sprite serializer and data restore; all 25 actors, 14,340 storage bytes, globals, variables, cursors and modes compare exactly | Optional actor extensions have source/synthetic coverage only; sprite recreation/replay, complete ownership and control readiness remain open |
| Later sprite checkpoints | [EVID-REF-029](../analysis/findings/EVID-REF-029.json): 25 actor decisions, 19 complete sprite restores, six timed VM iterations, four compact metadata calls, 32 metadata positions and both complete return-time party decodes | Compact auxiliary writes and unobserved timer/command variants are source/synthetic only; creation/rebinding, remaining VM, return readiness and timing remain open |
| Actor defaults and RNG | [EVID-REF-030](../analysis/findings/EVID-REF-030.json): all 25 return-time default calls, 25 random steps, 50 initial floor queries and 686,400 captured payload bytes compare exactly | Every observed query matches and preserves descriptor Y; allocation/shadow addresses are opaque inputs, and complete entry/return ownership remains open |
| Battle request | [EVID-REF-031](../analysis/findings/EVID-REF-031.json): all six calls and thirteen complete boundaries; ordered gates, tagged operands, mode latch, request stores and decoder successors | Five music retries and one immediate-zero acceptance only; other gates/selectors are source/synthetic, and native dispatch, combat and readiness remain open |
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
The new return-data captures retain all 124 control artifacts per run, including
every image, RAM sample, final external state and audio. Nine altered lineage or
oracle cases reject; public tests cover sparse padding, signed sprite bytes,
descriptor count versus event count, allocation ordering and malformed inputs.
Independent review qualified all ten new source bodies and 3,004 instructions,
then reviewed the three implemented functions and rebuilt the original replay
input before passing the C++ comparison under sanitizers. Byte qualification of
the remaining seven functions does not establish their behavior in that finding.
The later EVID-REF-029 comparison now reconstructs the 19 sprite restore calls,
including every captured full sprite state and nested stack/return boundary.
Three fresh recordings retain all 124 controls each; they repeat one route. The
compact calls make no auxiliary writes, and those write branches remain
source/synthetic coverage. Thirty-five new corruption cases reject; all 49 sprite
tests and the earlier animation replay with its 67 corruption cases pass.

Independent review also found and corrected a position-model bounds error: a
triangle index could read other tables inside the same component. Every starting
and visited triangle and its vertices now stay within their declared layer.
Eleven malformed cases reject in three new regression groups; all 23 position
tests and all 4,385 original call comparisons pass. The `-1` sentinel and valid
original arithmetic are preserved.

The earlier findings retain their historical confidence and scope. Fresh
independent audits reproduce the decoder subset, core C++ event transitions,
position comparisons and complete-party-motion composition. The last audit
reviews the main/bounds/idle functions; it does not supply a complete independent
review of every callee in findings 022–025. These bounded reviews are recorded in
the recovery inventory. Subsequent bounded reviews cover the observed sprite,
jump/planar and sweep branches while preserving their excluded paths. A second
malformed-layer guard corrects collision queries: invalid triangle/vertex indices
reject, the original `-1` attribute read is retained, and all original sweep
comparisons and 157 corruption cases remain exact. Phase 1 independently reviewed
that guard and reran the 16 collision-query tests. Neither a passing tool test nor
an observed subset closes a broad Phase 1 requirement.

Continue from the [recovery inventory](../analysis/recovery.json). Required work
includes the remaining script handlers and scheduler stages, field movement/geometry/
visual behavior, encounter/return lifecycles, combat state/formulas,
menus and save/load, media and Mono/Stereo/Wide paths, timing, semantic readiness
and setup. Source loading, symbols and formats must be extended beyond the
currently observed components. No unsupported opcode may become a no-op.
