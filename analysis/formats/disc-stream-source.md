# Resident disc stream ring source

`disc_stream.hpp` / `disc_stream.cpp` recover the resident ring allocator wrapper,
selection/reset, retail chunk lookup and chunk release. `Program::Music`
(`field_music.cpp`) connects them, over the Program-owned ring header, to the
field stream, wave staging callback and five-step poll. The focused
`test-disc-stream` harness executes that chain with heap, CD-delivery and audio
doubles.

The source is independently authored from this project's qualified original
Disc 1 executable, connected Ghidra exports and original captures. It uses no
other Xenogears implementation or reverse-engineering material. The original
resident SHA256 is
`dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119`.
The private correction/export is
`.local/verification/phase1-music-source-cpp-20260918/ring-source-v2/`.

| Original entry | Recovered source | Behavior |
| --- | --- | --- |
| `8002a260` | `Program::Music::allocate_stream_buffer` | Reject nonpositive counts, request `count * 808 + 24` bytes using the forwarded allocation mode, store count, select and reset ring |
| `80028a94` | `select_disc_stream_ring` | Replace the shared ring resource, return its previous value |
| `80028aac` | `reset_disc_stream_ring` | Clear four halfwords per slot, then write the low count halfword at ring offset 8 |
| `80028b14`, retail branch | `next_disc_stream_chunk` | Find state 3 and the expected sequence, compare against the separate active count, advance the u16 sequence, return the selected 2,048-byte chunk |
| `8002945c` | `release_disc_stream_chunk` | Return and clear the selected slot's state halfword; preserve original interior-pointer alias arithmetic |

`DiscStreamState` owns resident globals `8004fe30`, `8004fe40`, `8004fe24` and
`8004fe48`. The source preserves the separate header count and active count;
it does not silently equate them. Each slot occupies eight bytes beginning at
offset 4. Payload begins at `36 + 8 * header_count`. Bytes outside the reset or
release stores remain unchanged, including the 32-byte header tail and payload.

Resource values identify byte-addressable 32-bit allocations. Arithmetic uses
unsigned original address values; none are converted to host pointers.
`stream_buffer_storage` is only the caller's host storage lookup and must reject
missing or released resources. It is separate from the original game heap.
No live storage spans are retained by the ring state. Invalid host bounds fail
explicitly; stores already issued, such as advancing the expected sequence,
remain visible. The field's activity gate remains owned solely by
`BattleRequestState`, as described in [field media source](field-media-source.md).

The allocator review corrected a missing shared calling-contract detail:
`80085560` forwards incoming A1 through `8002a260` to game allocator `80031bdc`.
The original instructions preserve A1 at both calls. Updated Ghidra signatures
make this visible in the re-decompiled caller. The selected request supplies
mode one; the source harness also checks another mode. The qualified original
project and earlier correction/export versions remain immutable.

The nonzero host-file-table branch of `80028b14` remains explicitly unsupported.
That branch uses PC-file calls and game error-display logic; it cannot be
represented as a successful no-operation backend. Existing loader suggestions
for PC-file library symbols were inspected, including the `8004c338` trap
wrapper, but no new verified PsyQ match is claimed. The retail ring policy is
game-specific source. The custom game heap `80031bdc`, CD read setup/interrupt
producers, remaining file/directory wrappers and resident WDS/audio internals
are still distinct required recovery boundaries.

A fresh qualified execution of the existing field-23 route compares one
allocation, all 96 next-chunk calls (83 successful and 13 empty), and all 83
releases. Comparisons cover every captured byte of the 100-byte eight-slot ring
header, all four owned globals, return values and actual heap-call arguments.
The observed allocation requests 16,484 bytes with mode one. The game heap's
returned resource and pre-initialization bytes are explicit original callee
inputs; the heap is not supplied by guessed logic.

All 21 original control artifacts—complete RAM, external state, audio and
18 PNGs—match the uninstrumented cold-boot route exactly. Seven corruptions of
original allocation arguments, ring state, sequence, returns and opaque bytes
are rejected. The probe supplies unused synthetic payload storage solely to
represent the known allocation bounds; these ring algorithms do not read that
payload. The separately validated 83 wave callbacks establish payload handling
at the next connected boundary.

Synthetic tests additionally cover allocation failure and nonpositive counts,
u16 sequence wrap, state preservation, unmatched scans, interior-pointer release,
invalid bounds, explicit rejection of host-file mode, and real ring operations
feeding the field wave callback. Pinned strict Clang ASan/UBSan checks pass.
These tests do not establish unobserved original execution.

Private capture, probes, source qualification, comparison and snapshot records
are under `.local/verification/phase1-disc-stream-cpp-20260918/`. Original
binaries, automatic exports, buffers and captures remain outside distribution.
