# Original field 23 encounter and return

[EVID-REF-020](../findings/EVID-REF-020.json) records a repeatable original route
through contact, combat, victory, rewards, field return, movement and the party
menu. The [candidate input sequence](../../tests/reference-inputs/field23-encounter-observation.json)
uses the existing guarded field adapter followed by ordinary controller inputs.
It adds no battle, position, progression or reward setup writes. The full slice
manifest remains unfrozen and all nine proof domains remain open.

The route crosses the eastern ramps, goes around the creatures' initial approach
radius, then approaches actor 14 from the direction of its scripted destination.
Earlier approaches let it move out of interaction range. The successful attempt
faces toward the creature and supplies a fresh confirm press.

The source-qualified contact function begins at field `0x8008399c`. At frontend
run 5903, its store sequence at `0x80083fe4` queues actor 14's event 2 in slot 1
with priority 3 and entry PC `0x8a8`. The comparison checks every captured actor
byte, including the slot PC, tag, priority bits and facing stores. This verifies
the observed slot insertion; the complete eligibility function remains to be
reconstructed and tested.

The event reaches opcode `71` at PC `0x8ab`, implemented at field `0x80093568`.
Five calls retain that PC while the music-loading result is `-1`. At frontend
run 5915 the result is zero: the original resolves the tagged immediate selector
to zero, updates its request fields, requests an interpreter break and advances
the PC to `0x8ae`. All sampled actor and control bytes, including preserved bytes,
match the independently expressed private comparison. Other gates and selector
modes remain unobserved. [EVID-REF-031](../findings/EVID-REF-031.json) subsequently
reviews the [request reconstruction](battle-request.md) and adds opcode `71`
to the public event disassembler. Its original coverage remains these six calls.

| Capture frame | Visible original state |
| --- | --- |
| 6582 | Four-enemy battle with Fei and Citan |
| 6710 | Attack input displays one-, two- and three-point choices |
| 9230 | Victory experience screen |
| 9538 | Fei level 2, total experience 26; Citan level 3, total experience 73 |
| 9846 | Fei's level-up statistics |
| 10154 | Two Hob-Jerky gained, zero gold gained, total 100 G |
| 10762 | Returned to field 23 |
| 10794 | Ordinary movement after return |
| 11402 | Party menu: Fei HP 50/56, EP 10/11; Citan HP 200/200, EP 40/40 |

The source comparison passes before battle and at both return checkpoints.
Player position before battle and on return is exactly
`(6298887, -18874368, -17383881)` in the original signed 16.16 representation.
The movement input changes X to `8264487` while preserving Y and Z. Actor 14's
state word changes from `0x00010130` to `0x00010131`; the lifecycle and script
removal semantics still need reconstruction.

The complete traced run and its uninstrumented control have identical RAM
snapshots, serialized external state, audio and every image: 124 artifacts in
total. Fifteen deliberately corrupted traces are rejected. Original payloads and
the source-dependent comparisons remain private; public files contain authored
inputs and evidence descriptions.

These are observations and bounded comparisons, with independent review still
outstanding. They do not recover attack accuracy, damage, enemy AI, experience,
drop formulas, result-screen input states, menu semantics or the complete return
lifecycle. The capture frames are frontend observations, not established native
simulation deadlines. Battle code replaces the field overlay, so field addresses
must not be interpreted with their field meanings during battle.
