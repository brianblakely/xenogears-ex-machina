# Phase 1 state ownership and readiness checkpoint

This is a bounded source and observation finding, not a native API or an exit
review. All nine slice proofs and the complete setup/readiness requirement remain
open. Original executable `dc0b2dd7…` and field overlay `38a1ce82…` identify the
addresses below; battle and menu overlays have their own address meanings.

| State | Source interpretation and evidence | What remains unproved |
| --- | --- | --- |
| Assets available | Resident disc completion and field components have separate owned extents. A loaded pointer alone does not establish initialization or a usable action. The loader, events, sprite and message owners are represented in `Program`. | Every required resource lifetime across the complete route, including alternate mode-local resources. |
| Map initialized | The original adapter checks map `8004f34c`, entry word `8004f2f8 = 1`, and encounter byte `800adb04 = 1`. Field entry sets the word before loading, completes 32 fade frames, clears dialogue initialization gate `800afd04`, then the initial field-loop entry enables encounters. | Universal readiness, source-qualified arbitrary positions, and collision-safe spawn/setup conditions. |
| Event completion | Event actor PCs, waits, scheduler gates and resumed effects remain distinct. An ordinary wait return preserves continuation; it does not mean the enclosing event has finished. See EVID-REF-016/017/042. | All reached source-only controls, branches and stable completion conditions for the full route and alternatives. |
| Player control | The recovered control request requires actor flag `4000`, all four dialogue status halfwords nonzero, and inhibition `800b2176 = 0`. This is eligibility for that handler. The scheduler must actually dispatch it; transitions, field initialization and pending menu/battle/media work have separate gates. See EVID-REF-021/042 and `field_control.cpp`/`field_loop.cpp`. | A reviewed complete player-control readiness predicate and all disabled reasons across every required state. |
| Actionable dialogue/menu | Dialogue status is control state, not a universal UI-ready flag. The field menu request and its triangle inhibition are separate from menu allocation and the menu overlay's current screen/input state. The original caller preserves field ownership through synchronous menu execution. | Choice/control variants, complete field caller comparison and all required screen/action states. |
| Battle readiness | Battle presence and per-slot turn-ready bytes differ. At original frontend run 6360, ready-array index 1 is set; the acting-slot byte is 1 (slot index 0 plus one), page is 1, menu/event completion bytes are 0, and pause and outcome are 0. Field-overlay addresses at this time are battle code/state. | Complete scheduler/action/cancel/escape/defeat/result readiness and a reviewed typed setup contract. |

Existing immutable snapshots supply concrete counterexamples to a universal map
marker. In `p1shared-field23-p1dialogue-a-20260925`, frontend run 1227 (event 30),
all three adapter conditions hold and the controlled actor has flag `4000`, but
inhibition is -1 and input-updated is 0. In `p1frame-walk-a-20260924`, run 4901
(event 19), the same map conditions hold with inhibition 0 and input-updated 1.
In `p1frame-ret-a-20260924`, run 10304 (event 46), the actor's control-handler
conditions hold but the dialogue initialization gate is still 1, encounters are
0 and input-updated is 0. These are observations; the interpretation of handler
eligibility follows recovered source. They establish neither generic scene
readiness nor event completion.

The resident owns heap metadata and retained freed bytes, game data, variables,
controller state, disc and sound/GPU state. Field geometry, actor/event records,
dialogue, draw packets and loaded field resources have a field owner. Battle and
menu state use their actual mode-local owners. The synchronous field menu caller
does not restart the mode heap: its code/resources and game data transfer to the
menu owner for use and transfer back or release in original order. Shared menu
helpers borrow the live resident bus only during `Overlay`; destruction removes
the borrow. The initial qualified caller stack and saved registers are imported
once and evolve under source-derived stores. No original stack or gameplay image
is imported at a later menu boundary. Unrecovered intervening stack writes remain
a fidelity obligation; strict exports and comparisons must expose them.

Readiness observations and independent source/re-read reviews are private under
`.local/execution/p1-20260929-review/`. Original RAM and game bytes are not public.
The guarded adapter still accepts only its existing map contract. Broader setup
and readiness must stop explicitly until recovered; no Phase 2 API is introduced.
