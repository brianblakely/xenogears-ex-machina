# Recovered field motion source

The reusable implementation is [field_motion.hpp](../../include/xem/reconstruction/field_motion.hpp)
and [field_motion.cpp](../../src/reconstruction/field_motion.cpp), linked into
`xem-field-reconstruction`. The `field-motion-reconstruction` CTest executes
the real implementations, including a connected animation-header gravity,
impulse, terrain lookup and landing sequence. This closes the bounded source
stages below; it does not implement complete field movement or the sprite VM.

## Original boundaries and ownership

The source profile is `na-slus-00664-39c547a9afc6`, with resident executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`
and field overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
The original findings are [EVID-REF-022](../findings/EVID-REF-022.json)
and [EVID-REF-023](../findings/EVID-REF-023.json).

| Recovered source | Public implementation | Boundary |
| --- | --- | --- |
| Field `80082bb8..80082c8c` | `begin_field_motion` | Publishes actor index, then inhibits or returns selected active-body mode. The active body remains a separate requirement. |
| Resident `8003f8b0/8003f8cc`, `80022974`, sprite A0 `80021958` | Trig, planar vector and speed operations | Uses caller-owned original tables and exact signed low-word arithmetic. |
| Field `80081f80` ordinary party branch | `update_field_party_velocity` | Rebuilds and quantizes horizontal sprite velocity; sentinel clears X/Z before actor/table reads. |
| Resident `80024edc..80024efc` | `store_sprite_command_pc` | Adds the unsigned source width to the post-handler PC. It does not execute arbitrary commands. |
| Field event 21 `8009e094`, resident `80021bcc` | `execute_motion_divisor` | Uses shared event operand resolution, stores actor divisor and packed sprite bits, then advances event PC. |
| Resident sprite A1 `800219ac..80021a44` | `apply_animation_impulse` | Reads an explicitly address-qualified reference when enabled, applies signed operand scaling when zero, then shifts/divides and stores vertical velocity. |
| Resident `80023538..80023658` | Shared `install_sprite_gravity` in `field_sprite` | Installs header pointers/flags and gravity. Full animation selection calls this same implementation. |
| Field `8008505c..8008515c`, terrain `80080968` | `apply_field_vertical`, `field_vertical_step`, existing `actor_terrain_attribute` | Integrates old Y velocity, computes terrain from the recovered collision package, and applies gravity or the selected floor. |

Original addresses identify storage and provenance; they are never host
pointers. `SpriteWindow` borrows caller-owned mutable bytes. The vertical
operation borrows the corresponding actor window and collision package;
the caller must supply the actor and sprite belonging to the same descriptor.
Its `previous_layer` input is the signed layer retained by the enclosing
position routine. Sprite `+84` already contains the floor selected upstream.
Actor `+f0` remains neutrally named `marker` at this boundary.

`EventContext::pass` owns original globals `800adb68` and `800c4268`.
The scheduler clears both at pass entry; the real control handler writes
`input_updated`; motion reads that same object when choosing walking/running.
There are no separate scheduler, control and motion copies to synchronize.
The motion actor-index global `80065b08` remains distinct from the event actor
index and is still published even on an inhibited motion return.

The impulse requires a readable word at sprite `+7c` when flag `+a8` bit zero
is set, including when that word equals zero. A nonzero reference bypasses
scaling but still undergoes the final shift and division. Division failure
retains the value stored before the division; the numerator store occurs
afterward in the original instructions. The command does not advance its PC.

The vertical mutator stores integrated Y before its real terrain-reader call.
Consequently an invalid terrain reference preserves that already-issued store.
Terrain selection uses the triangle attribute's low byte and its disabled-layer
short circuit. The stage compares the signed high halfword of integrated Y
with the signed floor, preserves upward velocity on floor contact, retains
the marker only for the source terrain bits, and preserves every unrelated
actor and sprite byte. Later ceiling, layer and rollback stages still follow.

## Review and validation

The existing qualified Ghidra 12.1.2 exports supply reviewed sprite types,
handler register arguments and the connected enclosing position function.
The A1 handler's 38 original instructions resolve reference, signed loads,
low-word products, division and delay-slot store ordering. All 64 vertical
instructions match the qualified original overlay. The earlier planar source
qualification remains valid; no planar arithmetic was changed in this chunk.
No other Xenogears implementation or reverse-engineering source was consulted.
The code, synthetic fixtures and this note are project-authored with Codex
assistance. Raw exports, original tables and recordings remain private.

Current C++ functions are substituted only as candidates into the unchanged
original Python comparison machinery. Expected outputs remain independent raw
original captures, with source bytes, argument identity, call/stack correlation,
overlapping ranges and unchanged bytes still checked. Python dataclasses serve
only as result containers in this comparison.

The jump comparison passes 7,720 original records: 61 gravity prefixes, six
impulses and 126 vertical stages, evenly divided between airborne and floor.
Additional direct C++ mutator comparisons cover complete 180-byte sprites,
complete 312-byte actors, impulse numerators and integrated Y. All 53 existing
corruptions are rejected. Both jump-related captures match all 66 control
artifacts, including the memory-sampling stream. The source loader remains
independently qualified by the existing comparison; this is not a claim that
this motion module implements it.

The planar comparison passes 43,414 original records, including 567 resident
vectors, 548 field vectors, 19 speed commands, 10,921 command PC stores, 4,890
trig returns, and 470 active plus 470 inhibited mode prefixes. Existing
planar corruption and control qualification remains available in its earlier
private report. The current source replay additionally compares complete
sprite windows for the mutable planar operations.

Eleven public C++ test groups exercise arithmetic wrap, sign extension, sentinel
ordering, explicit unsupported operations, partial writes, event continuation,
reference identity, layer changes, terrain-marker retention and a connected
synthetic jump. A scheduler → control → motion test verifies the single shared
pass state, including its reset when the next pass dispatches no actor.
These invented fixtures are boundary tests, not additional
original observations. Run inside the required pinned environment:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
cmake --build --preset debug --target test-field-motion
ctest --preset debug -R field-motion --output-on-failure
```

Private reproducible C++ adapters, source qualification and original comparison
reports are under `.local/analysis/phase1-motion-cpp-20260918/`. The final shared
state replays are under `.local/verification/phase1-field-pass-cpp-20260918/`.
They do not ship.

## Required continuation

Full active movement, collision sweeps, complete floor selection, later
ceiling/layer/rollback/history/follower/camera behavior, remaining sprite
commands and readiness remain required source work. The field ratio,
alternate actor and Gear branches and zero-divisor behavior fail explicitly.
The full event-21 behavior has source and synthetic coverage; its current
original execution proof is not claimed. Jump alternate rates, nonzero
references, layer changes and terrain-marker retention have source boundary
coverage but are unobserved in these original recordings.

For Phase 2, these modules expose owned inputs and visible state changes;
the caller supplies original tables/resources, retained timing inputs and
the surrounding recovered state machine. No native renderer, audio backend,
agent protocol or fabricated game-logic service is introduced here.
