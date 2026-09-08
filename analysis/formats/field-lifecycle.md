# Field lifecycle and reusable event source

This handoff connects qualified Ghidra C output to authored source. It is a
partial subsystem reconstruction, not a completed field runtime or Phase 1 exit.
See [EVID-REF-027](../findings/EVID-REF-027.json) and the
[recovery inventory](../recovery.json). Earlier findings retain their original
scope; subsequent work does not retroactively broaden their execution claims.

## Source identities and confidence

Field addresses below belong to decoded overlay
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`,
loaded at `8006faf0`: Disc 1 slot 36, LBA 108933, or identical Disc 2 slot 31,
LBA 173245. Resident addresses here use Disc 1 executable
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`.
Original bytes remain authoritative. Other overlays can occupy these addresses.

Private surveys preserve Ghidra body ranges/hashes, direct and computed calls,
data references, signatures and annotation sources. Successful discovery and C
export are **automatic output**. Proposed names, partial layouts and calling
context are **annotations**. The event C++ and position Python below are
**source-reviewed reconstruction**; original comparisons establish only their
stated **execution-validated subsets**. Independent review remains outstanding.
Raw C, projects and original payloads remain private. This document and the
implementation are authored source.

## Source modules Phase 2 can use

| Module | Recovered behavior | Integration boundary |
| --- | --- | --- |
| [field_events.cpp](../../src/reconstruction/field_events.cpp), [public types](../../include/xem/reconstruction/field_events.hpp) | Actor pass, slot selection, bounded dispatch, branches, waits, end/reset/jump and five variable stores | Ten primary handlers; others raise `UnsupportedInstruction` with PC and opcode |
| [field.py](../../tools/analysis/field.py), [packed.py](../../tools/analysis/packed.py) | Existing component, event-package and collision parsing; packed decoding | Extraction/reference tools; actual source-memory context is required for decoder overreads |
| [position.py](../../tools/analysis/position.py), [position_query.py](../../tools/analysis/position_query.py) | Position integration, layer-floor selection, height bounds and party history | Reuses collision and vertical arithmetic; diagnostic services and unobserved branches remain explicit |
| Existing [party motion](active-motion.md), [sweeps](movement-sweep.md) and [sprite models](sprite-animation.md) | Verified party movement and selected animation behavior | NPC active motion, ordinary sprite commands, rendering and contact remain incomplete |

`xem-field-reconstruction` is a C++20 library. Its structures hold semantic
values, not packed PS1 RAM. `EventProgram` borrows bytecode and entry rows;
descriptors borrow actors. Phase 2 must supply stable ownership and convert
references to serializable identities at its runtime boundary. Original offsets
are provenance, not the intended agent API. The baseline does not launch a game.

The library requires real dispatch. `execute_core_event` supports primary
`00,01,02,04,26,35,36,37,38,39`. Existing Python knowledge of `0c`, `a7` and
`fe/a2` remains available but is not silently substituted into the C++ VM.
Malformed storage raises an explicit error; this bounded host interface does not
emulate arbitrary invalid PS1 memory accesses.

## Load order and ownership

Field `80070cc8` owns loading and initialization. Wrapper `8007008c` forwards
source and destination to resident decoder `80032eb4`; its first argument is
unused. Correcting that signature removes the misleading interpretation of an
allocation size as a decoder input parameter.

The bundle pointer is resident `8005a4e0`. Nine logical sizes begin at bundle
`+10c`; nine packed offsets begin at `+130`. Allocations generally request logical
size plus 16. Packed output size and padding remain separate. The final component
can read adjacent allocated RAM ([EVID-REF-014](../findings/EVID-REF-014.json));
physical sector padding is not an equivalent replacement.

| Order | Component | Storage/consumer | Lifetime and remaining interpretation |
| --- | --- | --- | --- |
| 1 | 0 | Temporary decoded offset table; field `800771f8` consumes members | Upload-related resource; complete format open |
| 2 | 4 | Temporary offset table; resident `80022a70` conditionally consumes members | Both temporary buffers survive through `DrawSync(0)`, then are freed |
| 3 | 2 | Persistent `800afb14`; resident `8002c3e8` processes members | Geometry/model relationship inferred from consumers; material/model format open |
| 4 | 6 | Fixed resident storage `800658dc` | Formation records survive field-overlay replacement and feed battle setup |
| 5 | 5 | `800adbf8` package, `800adbfc` count, `800adc00` bytecode | Owns type map, actor entries and event code |
| 6, 7 | 8, 7 | `800adbf4`, `800adbf0` | Persistent resources; complete formats and consumers unresolved |
| 8 | 1 | `800afb18`, pointer families `800afb20..800afb54` | Collision attributes, triangles, vertices and layer counts |
| 9 | 3 | `800afb1c` | Field sprite bundle, used by initialization and return rebinding |

The loader first copies 256 configuration bytes and initializes several `0x70`
records. Descriptor count is bundle `+18c` (`u16`); 16-byte placement rows start
at `+190`. It allocates and clears 92 bytes per descriptor. Placement supplies
flags, rotation words, XYZ words and a model index. Initial integer XYZ loads are
zero extended; subsequent fixed-point conversion must retain its own width.

Field `80080f44` allocates a 312-byte actor for indices below the event actor
count, clears it, binds descriptor `+4c`, calls defaults `80080a74`, then creates
additional descriptor storage. Defaults also query collision layers, set floor
and position state and consume RNG. This dependency tree is not yet reusable C++.

After descriptor/resource preparation, the loader releases the source bundle.
Persistent consumers must retain decoded storage or their own copies. It sets
post-initialization `800adb1c` to zero, calls `800a28d4`, then sets it to one.
Remaining camera/render initialization follows. Map load completion alone cannot
establish player-control readiness.

## Event format and calling context

Component 5 contains a 128-byte variable signedness bitmap, `u32` actor count at
`+80`, then 32 little-endian `u16` entry PCs per actor at `+84`. Bytecode begins
at `84 + actor_count * 40` (hexadecimal offsets). PCs are relative to bytecode.
Resident `800c3a68` holds 1,024 variable halfwords. References are byte offsets;
odd references alias the preceding even reference. A set bitmap bit selects
zero extension, otherwise sign extension. Writes retain the low 16 bits.

| Actor offset | Recovered meaning |
| --- | --- |
| `00`, `04` | Actor flags and layer/control flags; unknown bits preserved |
| `8c + 8*i` | Eight slots: resume PC `u16`, countdown `u8`, event tag `u8`, control `u32` |
| Slot control bits 18..21 | Four-bit priority; other bits opaque |
| `cc`, `ce` | Working PC `u16`, selected slot `u8` |

Operand helpers `800acd7c`, `800acdb8`, `800acdec` and `8009cfbc` take the byte
offset in `a0`. Old automatic signatures hid this forwarded argument. Corrected
signatures expose it throughout dependent handlers. The halfword readers assemble
bytes, rather than using native unaligned halfword loads. `800a2fe0` returns zero
or minus one, not a boolean returning one. These ambiguities were checked
selectively in Ghidra instructions and corrections fed back into the project.

Resident startup `80019550/554` assigns `gp = 80059170`; mode dispatcher
`80019acc` invokes the startup continuation before its mode target. A zero Ghidra
GP context was therefore wrong for that normal path. This contextual annotation
does not turn arbitrary saved-context restores into constant-GP execution.

## Initialization, scheduling and field update

Fresh initialization (`800a28d4`, resident `8004f30c == 0`) clears variable
reference `10`, copies party values through `800a30b4`, and uses two actor passes.
The first examines entry 2: a leading zero opcode sets layer flag `04000000`;
it then installs entry 0 as the working PC. The second executes each initialization
batch in mode zero with limit 65535. If visual-creation marker `800afc74` remains
zero, it calls `80076ac0` using the first field sprite resource and sets layer
flag `800`. Sprite creation and marker producers remain required dependencies.

The ordinary pass (`800a2030..800a22a8`, exclusive end) first clears `800adb68`
and `800c4268`, whose meanings remain unknown. Single-actor mode equals one
selects one actor; otherwise it uses the event actor count.

1. Eligibility requires descriptor `(flags & f00) != 0` and clear actor layer
   flag `00100000`. With nonzero post-initialization, any zero gate at `800adbe0`,
   `800adbe4` or `800adbec` stops the pass before publishing the actor.
2. Publish actor/index/descriptor and clear actor flag `01000000`. Party-processing
   mode can then skip matching non-255 party indices.
3. Select the lowest priority, with the last slot winning a tie. If all priorities
   are 15, use slot zero, entry 1 and priority 7, preserving other bits/countdown/tag.
   Copy the resume PC to the working PC and set mode one.
4. Actor flag bit zero suppresses dispatch. Otherwise request a batch with limit eight.
   Commit the working PC through the current shared actor/slot after handlers,
   retaining the original context-replacement boundary.

Batch `800a1ec8` clears the break request and stores the limit even for nonpositive
limits. After each handler, mode zero extends the limit to 65535, then gates are
checked, then `break == 1 && mode == 1`, then count/limit. At most 1,025 dispatches
occur. The safeguard's original diagnostic request is returned explicitly as
`diagnostic_requested`; the library does not execute the PS1 diagnostic service.

Field update `8008110c` runs events before motion. It records previous integer
positions, prepares/runs eligible motion updates, calls `80084158` for controlled
actor contact/position, then `80084a40` for other eligible actors. Controlled
encounter/contact update `8008399c` follows under its gates, then followers at
`800815f0`. These calls connect the motion and position models; cadence and
complete contact/follower behavior remain open.

## Encounter return and persistence

The other `800a28d4` branch calls `800a3474` to restore field state from resident
storage beginning at `8005a4e8`, using saved count `8005a4e4`. It restores field
globals, collision attributes and descriptors/actors, preserves or rebuilds
selected allocation pointers, and restores the 2,048-byte variable bank. Sprite
selectors then choose party resources or field members for `80076ac0` rebinding.
This is a field return snapshot, not the ordinary memory-card file format.
Producer symmetry, complete buffer extents, ownership and return variants remain
blockers.

Battle overlay
`1830b4ef1fe37129972fc310dfad534f8161d6c0b123e74254c3711334a3e291`
also loads at `8006faf0`. Resident `8001b6c4` enters battle `80070f40`. The latter
copies a selected 32-byte formation from `800658dc` to `8006f9dc`, allocates
battle state, drives update/wait loops and eventually selects a return mode.
It also loads code at `801e0000` and `801de000`; their qualified imports and
complete contracts remain required. The automatic result path has unresolved
incoming-register behavior; it cannot supply guessed victory/defeat rules.

Menu overlay
`82f84a24ac1fd1979754e1cc9c861405fb59ce95dd78db00a76f00c8f1bd177d`
loads at `801c5000` (Disc 1 slot 39; Disc 2 slot 34). Its exports are provisional.
Shared resident state, menu transitions, inventory/equipment effects, card
operations, ordinary save checksums and reload equivalence remain under review.

## Validation and next dependencies

The C++ module passes five connected synthetic groups under sanitizers. Reusing
EVID-REF-015/016 captures compares 81,527 transitions: 30,166 end/jump/reset,
4,884 wait/variable, 29,968 selections and 16,509 branches. Complete captured
actor prefixes and controls compare; variable cases also compare the full bank
and type map. Actor eligibility and connected dispatch are source/synthetic
checks, not a new whole-scheduler original-execution claim.

The preserved position models compare every one of 4,385 complete calls in six
existing captures, including 626 active commits, 1,252 layer queries and 626
history writes. NPC position calls exercise idle paths, not the excluded NPC
motion body in EVID-REF-026. Terrain/volume rollback, failing queries and linked
floor variants retain source-only or unresolved status. Twenty authored tests pass.

Continue through event initialization and dialogue/actor dispatch, contact and
encounter selection, snapshot producer/restore symmetry, battle auxiliary code,
menu/card state and required media. Read related Ghidra C together and correct
shared types/context before lowering ambiguous operations. Reuse existing
captures/models; new original execution should resolve material boundaries.
Broad Phase 1 todos and all nine slice proof domains remain open.
