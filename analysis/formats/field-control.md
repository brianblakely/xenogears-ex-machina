# Field direction and jump requests

This reconstruction comes from the selected original sources, with execution
evidence in [EVID-REF-021](../findings/EVID-REF-021.json). It handles input requests;
the later movement, animation and collision updates remain separate work.

The shared decoded field overlay has SHA256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`
and loads at `0x8006faf0`. The qualified primary table at `0x800ae2a0`
maps `a7` to `0x8009f5f4` and `0c` to `0x8009f5a8`.
The selected Disc 1 profile is `na-slus-00664-39c547a9afc6`.

## Recovered request behavior

Primary `a7` has no operands and always increments the actor's working u16 PC,
including blocked requests. Its normal return is `0x8009f9f8`. Wrapper `0c`
saves the PC, calls `a7`, restores the saved PC and sets the interpreter's break
request to one. It therefore has a self successor in the disassembler. Whether
a break actually ends an initialization batch remains governed by the separately
recovered scheduler. The wrapper has source/synthetic coverage only in this finding.

The handler first checks control ownership. Without actor flag `4000`, it leaves
the direction unchanged and sets flag `01000000` only when the original
nonplayer-motion option is zero. With ownership, it blocks if any of four signed
dialogue statuses is zero or the signed control-inhibition value is nonzero.
Blocking sets the entire requested direction to `8000` and preserves the other
sampled control state. These checks do not by themselves establish overall
player readiness or the complete dialogue lifecycle.

An eligible call invokes the original encounter poll when the held direction
nibble is nonzero, then sets the control-updated word to one. The poll's first
eight early-return tests are reconstructed in source order. It tests the music
pending sentinel as exactly `ffffffff`, the menu gate as exactly one and its
inhibition sentinel as exactly minus one. If every gate passes, the model raises
an explicit unrecovered-encounter error. Counter/RNG/selection/request effects
after these gates are still required work.

Normal jumping tests **newly pressed** Triangle (`0080`), actor flags `1800`,
terrain flag `00400000`, and an exact contact sentinel of `000000ff`. It then
calls `0x80081f5c`. That predicate returns minus one when the intersection of
`(actor_flags >> 9) & 3` and `terrain_flags >> 3` is nonzero, otherwise zero.
A successful request sets actor flag `0800` and copies the current original
jump-related word into its latched field. This copy does not establish a velocity
formula; the word also participates in the original position-history machinery.

The terrain-specific held-button path has different rules:

- With terrain bit `00400000`, equal cached and current **integer** XYZ increments
  a u16 stationary counter. A coordinate difference retains it. Other terrain
  resets it to zero.
- The subsequent comparison reads that stored counter as signed 16-bit. Values
  greater than 32 clamp to 32; increment wrap occurs before the signed test.
- On that exceeded-counter path, held Triangle, clear actor bits `1800` and the
  contact sentinel invoke the normal terrain predicate without requiring a new
  press. This branch also bypasses alternate-mode cooldown processing.

With the alternate jump mode enabled, a newly pressed Triangle instead requires
a zero repeat countdown, the contact sentinel and the same terrain predicate.
It does not perform the normal `1800` or `00400000` checks. Success additionally
sets the animation-mode halfword to `00ff` and reloads the repeat countdown.
A nonzero countdown is then decremented, including a value just reloaded.
The alternate and terrain-counter paths were not exercised by the original route.

Finally, the held direction nibble XOR `0f` selects one of 16 u16 entries in one
of two original tables. Entries with bit `8000` keep **all** their bits. Other
entries subtract the signed camera angle and mask the result to 12 bits. The
public fixtures use invented tables; original values remain local imports.

## Semantic state and original correlations

The authored [model](../../tools/analysis/field_control.py) takes typed semantic
values, retains exact integer widths and exposes eligibility, jump decisions and
ordered service calls. Original addresses are confined to the
[comparison codec](../../tools/analysis/verify_field_control.py).

| Semantic input/state | Original correlation |
| --- | --- |
| Current actor and script index | pointers `b0078` and `afd1c` |
| Actor flags, terrain flags | actor `+00`, `+14`, u32 |
| Integer XYZ and cached XYZ | signed halfwords `+22/+26/+2a`, `+68/+6a/+6c` |
| Working PC, requested direction, animation mode | u16 `+cc`, `+104`, `+e8` |
| Held and newly pressed buttons | u16 `afe9c`, `c2694`, already processed by original input code |
| Four dialogue statuses | s16 `c2a14 + i*498`, `i=0..3` |
| Control inhibition / preserve nonplayer motion | s16 `b2176`, u8 `b21ce` |
| Stationary counter, contact sentinel, updated word | u16 `adb02`, u32 `adb64`, u32 `adb68` |
| Alternate mode, repeat reload and remainder | s16 `b2344`, u16 `b2340/b2342` |
| Jump-related word and latched copy | u32 `b2360`, u32 `adb28` |
| Direction table selection, tables, camera angle | u8 `b2354`, `adf68/adf88`, s16 `af98c` |
| Encounter early-return gates | u32 `adbdc/adbe4/adbec/4f308/b2298/adb2c`, s16 `b2176`, u8 `adb04` |
| Interpreter break request | u32 `b00c0` |

Addresses in this table are offsets within the original `80000000` RAM alias;
they are source correlations, not the proposed native agent API.

## Original comparisons and limits

The [ordinary input route](../../tests/reference-inputs/field23-control-observation.json)
cold-boots through the existing guarded field adapter and performs the same
entry dialogue inputs before stationary/held/repressed/moving jumps, direction
and run combinations, and camera-button inputs. It adds no setup writes. The
5,241 frontend runs are not a recovered universal simulation tick count.

The guarded window contains 9,500 records: 4,669 complete `a7` calls, 78 complete
encounter early returns and three complete terrain predicates. Actor 3 has
2,334 calls, including 489 inhibited and 1,845 eligible calls. Actor 5 has 2,335
calls without control ownership. All 78 encounter polls return at the clear
`b2298` gate. Accepted jump requests occur in frontend runs 4,317, 4,697 and
4,789; a second press in run 4,705 is rejected by bits `1800`. Holding Triangle
does not produce another accepted request in this route.

The verifier checks every captured actor byte and control range, including
preserved opaque bytes, along with code, tables, event-bytecode digests, pointers,
actor identity, call order, PC and stack/return relationships. All 65 PNG/RAM/audio/
external-state artifacts and the memory-sampling stream match a separately built
uninstrumented core. Twenty-two semantic corruption trials are rejected.

The 14 public tests cover source-derived boundary cases; they do not turn
unobserved branches into original-run proof. Full physics, movement speed,
animation data, camera response, dialogue readiness, active encounter selection,
`0c` execution, hardware cadence and independent review remain open.
