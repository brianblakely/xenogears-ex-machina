# Field return snapshot and ownership

The original encounter route saves field state before replacing the field overlay,
then reloads the field and restores that state. [EVID-REF-028](../findings/EVID-REF-028.json)
qualifies the snapshot writer, sprite serialization and data restoration. This is
an in-memory field return format, separate from ordinary memory-card saves and
the future native snapshot format. The later
[sprite checkpoint stage](sprite-return.md) now has a separately compared source
model. Complete sprite rebinding, encounter rules and player-control readiness
remain required.

The authored C++ [interface](../../include/xem/reconstruction/field_return.hpp)
and [implementation](../../src/reconstruction/field_return.cpp) are reusable
format/correlation code in `field::original`. Inputs explicitly own their bytes.
The remaining raw actor and global fields retain original pointers and unknown
bits; native gameplay must translate these to semantic state and owned handles.
`ActorReturnRecord::event_state()` exposes the already recovered event flags,
eight slots, working PC and selected slot through the shared event types.

## Qualified source

Field functions use decoded overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`
at `8006faf0`, Disc 1 source slot 36, LBA 108933. Resident functions use executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`.
Only source profile `na-slus-00664-39c547a9afc6` was exercised by this finding.

| Function | Reviewed boundary |
| --- | --- |
| Field `800a3f4c..800a4748` | Snapshot producer; exclusive end, including the final diagnostic branch |
| Resident `80021ebc..80021fb8` | Sprite snapshot stores, including its return delay-slot halfword store |
| Field `800a3474..800a3c8c` | Snapshot data restoration; preserves or allocates selected actor-owned storage |
| Field `800a3c8c..800a3f4c` | Later sprite restoration policy; separately reconstructed in EVID-REF-029 |
| Resident `80021d50..80021ebc` | Sprite animation selection/replay and final position/timer restoration; separately reconstructed in EVID-REF-029 |

The previous automatic survey omitted the producer, sprite serializer, field
entry `80077e88` and teardown `800700b0`. A copied qualified Ghidra project now
contains their reviewed boundaries. Calls at field `80078454` and `80078660`
enter the producer. The new exports retain original bytes, body hashes, P-code,
delay slots and references. The broad entry/teardown exports remain provisional;
their existence does not establish full field lifecycle behavior.

Ghidra's unfused unaligned load/store expressions in the producer are not safe
host C. Reviewed `lwl/lwr` and `swl/swr` pairs copy the stated byte sequences,
including aligned tails. The reconstruction performs bounded byte copies and
explicit little-endian stores. It does not translate the pseudocode's potentially
undefined shifts or assume padding is zero.

## Snapshot layout

The image begins at resident `8005a4e4`. Byte zero receives the low eight bits of
the descriptor count. Bytes 1–3 remain unchanged. Event actor count comes from the
reloaded event package at `800adbfc`, independently of that descriptor byte.
The original writer does not enforce a count range; the parser bounds the supplied
event count by the available image before allocating or multiplying record sizes.

| Image offset | Size | Original source and purpose |
| --- | --- | --- |
| `004` | `038` | `800b007c`: object/global state, including original resource correlations |
| `03c` | `074` | `800afa54`: transform/global state |
| `0b0` | `400` | Pointer at `800afb20`: collision attribute table |
| `4b0` | `2e4` | `800b2078`: field globals, party/control and other retained state |
| `794` | `1c8` | `800af880`: camera/global state |
| `95c` | Variable | One record per event actor |
| After actors | `800` | All 1,024 event-variable halfwords from `800c3a68` |

These five global regions remain separate source-owned groups. The byte-copy
contract is established; complete interpretation and resource repair of every
member remain open. The variable signedness map is reloaded with the event
resource and is not serialized here.

Each actor has a base record of `174` hexadecimal bytes:

| Record offset | Size | Meaning |
| --- | --- | --- |
| `000` | `008` | Descriptor bytes `+50..+57` |
| `008` | `004` | Zero-extended low descriptor flag halfword from `+58` |
| `00c` | `030` | Sprite checkpoint |
| `03c` | `138` | Complete actor object, including event slots and opaque pointers |
| `174` | Optional `00c` | Actor `+134` bit 7 selects data pointed to by actor `+110` |
| Following | Optional `010` | Actor `+12c` bit 12 selects data pointed to by actor `+114` |

The serializer also widens the three party-mode bytes at resident state
`*8005a39c +22b1` into three words at `8005a408`. These are outside the image.
The tail can request diagnostic printing when `800c268c` is zero; the data module
does not execute that diagnostic service.

Sprite checkpoint stores are sparse. Offsets `00..0b` hold the three fixed-point
positions. `10` is facing; `12` receives sprite flag bits 11–16; `14` and `16`
receive sign-extended bytes from sprite `+af` and `+b0`; `18` receives flag bits
22–27. Two words from `*sprite+7c` occupy `1c..23`. Halfwords from `*sprite+20`
at `+6,+8,+a` occupy `24,26,28`; sprite `+2c` and `+82` occupy `2a` and `2c`.
Offsets `0c..0f`, `1a..1b` and `2e..2f` are not written. Existing snapshot bytes
must survive in these positions. The parser retains them without inventing a
purpose or using them as gameplay state.

## Restore ordering and readiness

The data-restoration routine copies the five global groups, restores each
descriptor's two auxiliary words and low flag halfword, then copies the full actor.
Descriptor flag bits 16–31 survive from the newly initialized descriptor. Actor
`+118` survives from the newly initialized actor instead of the saved allocation.
The optional `+110` and `+114` payloads request fresh 12- and 16-byte allocations,
copy their complete data, and replace their saved pointers. Allocation addresses
are supplied correlations, not reconstructed allocator behavior. Callback failure
is not a transactional native restore; Phase 2 must provide owned allocations and
rollback. The module leaves caller-owned initialized state unchanged.

The final step restores all event variables and publishes the image end cursor.
Sprite checkpoints are deliberately skipped by this routine. The C++ result
returns all of them as pending work rather than claiming a ready field.

Field `800a28d4` subsequently recreates sprites from party or field resources.
Field `800a3c8c` applies a later checkpoint policy, including skip conditions and a
possible checkpoint animation update, then calls resident `80021d50`. That helper
selects an animation and may replay ordinary sprite VM work until the saved
six-bit frame state is reached before restoring positions and timers. The ordinary
VM now has the timed-command subset needed by this captured checkpoint stage;
EVID-REF-029 compares all 19 restorations. Remaining commands, allocation/rebind
side effects and complete return variants remain open.
Map loaded, data restored, sprite restored and player control ready are distinct
states; these captures establish no native tick rate or readiness predicate.

## Original validation and limits

Two fresh read-only traces reuse the ordinary encounter route from EVID-REF-020.
There are 105 save records at frontend run 5919 and 73 restore records around
runs 10243–10244. All code guards, pointer lineages and capture budgets qualify.
Both full runs match all 124 control artifacts, including every PNG, RAM capture,
final external state and audio. The full retained image stays unchanged through
the battle.

The public C++ matches the complete 14,340-byte supplied snapshot region,
including all old padding and the 13,744-byte used extent. It matches 25 sprite
serializations, all 25 actor data restores, 2,392 global bytes and 2,048 variable
bytes in each direction, the descriptor count, end cursors and separate party-mode
stores. Actor-before hooks occur after the descriptor stores; observed descriptors
are compared in that restored state. Public fixtures independently check high
descriptor-bit retention, alongside sparse padding, signed bytes, optional
ownership/allocation ordering and malformed-size/count rejection. Sanitizers pass.
Nine changed source-state/oracle cases are rejected.

No optional actor extension occurs in this encounter capture. Those branches have
source review and public synthetic tests, not original-execution validation.
The 19 later sprite replay entries were dependencies in this finding; their
source-computed effects and remaining limits are recorded separately in
EVID-REF-029. Ordinary card saves, combat/results, complete loaders, media, timing
and all nine slice proof domains remain open.
