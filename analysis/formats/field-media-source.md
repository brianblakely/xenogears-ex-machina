# Connected field music source

`include/xem/reconstruction/field_media.hpp` and
`src/reconstruction/field_media.cpp` recover the field music loading policy,
stream wrapper, wave staging callback and shared-bank wrappers in reusable C++.
They connect the caller-owned music result to the existing FE/A2 event handler
and real event batch. `tests/test_field_media.cpp` exercises that connection,
resource ownership and the shared battle-request gate.

The source is project-authored from this project's qualified original Disc 1
executable and field overlay, corrected Ghidra output, original instructions
where the output was ambiguous, and guarded original execution. No external
Xenogears implementation or reverse-engineering source was consulted. Existing
[EVID-REF-018](../findings/EVID-REF-018.json) and its verified Python references
remain unchanged. This document adds the reusable source boundary; it does not
promote the entire required-media domain to complete.

| Source function | Original field entry / statement | Boundary |
| --- | --- | --- |
| `start_music_stream` | `80085560` | Allocate the eight-block stream descriptor, issue mode `100` read, install consumer |
| `poll_music_stream` | `800854d0` | Obtain chunk, call its consumer, or release idle stream descriptor |
| `finish_music_wave_chunks` | `80085c3c` | Up to five stream steps, translating stream completion to poll completion |
| `consume_music_wave_chunk` | `800859dc` | Four initial 2,048-byte chunks, then repeated leading-chunk replacement and transfer calls |
| `start_shared_music_wave` | `80085fb8` | Directory selection, file 3 size/allocation/read, directory restoration |
| `finish_shared_music_wave` | `80085f30` | Disc wait, shared wave creation, owner publication, audio wait and staging release |
| `poll_music_load` | `80085c90` | Selected/shared waves, deferred sequence read, disc gate, sequence creation/reuse and completion |
| `update_music_load_gate` | `80078b6c..80078b94` | Poll only when the prior result is exactly `ffffffff`, commit the return |
| Existing FE / A2 implementation | `800869b8` / `8008825c` | Extended dispatch PC increment; pending retry or completed advance with break request |

All field entries belong to the decoded overlay with SHA256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.
The containing update function also advances the RNG and decrements a cooldown;
`update_music_load_gate` deliberately identifies only its recovered music
statement, not that whole function.

## State and call boundaries

`MusicLoadState` owns music flags, selected resource tokens, the wave chunk
counter, transfer owner and stream descriptor/callback. Resource tokens are
opaque values supplied by the owning resource implementation. They are never
cast to host pointers. The wave callback receives bounded views of actual
resource bytes. `BattleRequestState.menu_gate` is the single owner of original
`800adb2c`: stream start sets it and stream completion clears it. Battle requests
therefore observe the same gate directly. `EventContext` owns event PCs and
interpreter controls. A music result is not player readiness.

The original callback takes its chunk pointer in A0. The old automatic export
had omitted that argument. A qualified project copy corrects the callback
boundary, signature and shared function-pointer type, then re-decompiles the
caller and callee. Instructions `800854e0`, `80085500`, `800859e4`, `80085a78`
and `80085b08` establish forwarding and release arguments. The immutable
qualified project and old exports remain intact.

The connected allocator review also established that the stream starter forwards
its second argument in A1 through `8002a260` into game allocator `80031bdc`.
The earlier automatic export omitted that argument. The source API preserves
the allocation mode explicitly; startup tests cover a nondefault mode as well
as the selected music request's mode one. The qualified copy's allocator type
is corrected before re-decompiling the stream caller.

The staging copy retains the original sequence of four word loads followed by
four stores for each 16-byte group. It fills four consecutive 2,048-byte blocks,
releases each input chunk, and starts the initial transfer after the fourth
release. Later chunks wait on audio flag `10`, replace the leading block,
submit 2,048 bytes, then release the input. Negative or greater-than-four chunk
indices execute the original no-operation branches. Pending and active flags
retain their exact sentinel/equality comparisons; stale resource tokens are not
inventively cleared after release.

`UnrecoveredMusicCalls` names remaining original game-specific calls explicitly.
These include resident CD ring/read wrappers, memory ownership, wave transfer,
SMDS sequence creation/start/resume/configuration and audio-service waits. They
are **not** a claim that all remaining work is a platform backend. Pure virtual
methods have no successful defaults, and a missing stream consumer fails
explicitly. Test doubles exercise the recovered caller contract but do not
validate those callees. Original wave/sample/sequence decoding, playback,
Mono/Stereo/Wide, stop/replacement lifetimes and required FMV behavior remain
required recovery work.

## Validation and limits

The focused harness runs the actual stream, callback, poll, gate commit, FE/A2
and event batch code. It covers pending/ready continuation, exact flag values,
shared resource ownership and call ordering, sequence reuse and alternate starts,
shared battle gating, input/PC errors, initialization safeguard behavior, PC
wrapping, and overlapping 16-byte wave copies. These are authored synthetic
cases, not claims that every branch occurred in original gameplay.

Fresh current-source comparisons preserve the existing original route evidence:
2,921 FE prefix boundaries, 993 pending A2 calls, 35 complete music poll states
and returns, 35 caller commits, and six independently captured sequence
read/create/start correlations. The poll comparison uses original aggregate
wave-helper and disc results as inputs. Its low-level stream realization is
synthetic and is not evidence for actual disc streaming.

The newly qualified wave callback is additionally compared to a focused original
capture of all 83 callbacks for source wave slot 146. That comparison checks all
169,984 input bytes (the complete 168,000-byte WDS plus 1,984 sector-tail bytes),
all 8,192 staging bytes before and after every callback, the directly owned
counter/transfer state, preserved captured globals, and direct wait/transfer/
release arguments, ordering and staging digests. The original creator return is
an explicit unrecovered-callee input. The CD ring and resident WDS/SPU internals
remain outside this source boundary. This establishes staging and transfer-call
behavior, not audio rendering, SPU destination equivalence or logical timing.

Private reports, probes, Ghidra correction scripts and captures are under
`.local/verification/phase1-music-source-cpp-20260918/` and the matching private
`phase1-wave-callback-20260918-*` scenario directories. Original files, payloads,
automatic exports, RAM and traces are not distributed. The focused source tests
run through the repository's normal pinned Nix CMake build as
`field-media-reconstruction`; strict Clang ASan/UBSan checks also pass. Source
review and full repository/distribution integration remain separate gates.
