# Field music loading — recovered policy and source inputs

[EVID-REF-018](../findings/EVID-REF-018.json) derives this subset from the selected
Disc 1 executable, the shared field overlay with decoded SHA256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`,
and guarded original execution of the candidate field 23 entry-dialogue route.
The reconstruction is in `tools/analysis/music_loading.py`; memory correlation
is separate from its named state. This is a loading policy, not an SMDS/WDS
decoder, mixer or native implementation.

## Selected resources and consumers

The original directory helper at resident `0x80028470` selects an index base
from the u16 directory table in logical sector 40. Entry 28 is 117, producing
base 116. A file number N resolves to source slot `116 + N - 1`. The music
selector's two-byte row at overlay `0x800adfcc + 2*selector` supplies a wave bank
and shared-bank flag. Only selector rows 6 and 12 are qualified here; this does
not establish the whole table's extent or semantics.

| Selector | Row: bank, shared flag | Sequence file / slot | Source LBA | Sequence bytes | Wave file / slot |
| --- | --- | --- | --- | --- | --- |
| 12 | 255, 0 | 44 / 159 | 112266 | 464 | No selected bank read |
| 6 | 6, 0 | 32 / 147 | 111705 | 5,436 | 31 / 146, LBA 111622, 168,000 bytes |

The sequence file formula is `0x14 + 2*selector`; the selected wave file is
`0x13 + 2*bank`. Original reads use resident `0x800295d8`. Both complete SMDS
files match original input at `0x80039850`, including their last bytes. That
consumer receives `0x80062648`, creates a sequence object, and retains the
input pointer at object `+8`. The subsequent `0x80039a80` call receives that
object with arguments 127 and 0; its return has bit `8000` set in the u16 flags
at object `+0x10`. Other object fields and sequencing semantics remain open.

The selected WDS read precedes resident `0x800380d0`. Its initial 8,192-byte
staging block has the same digest as the source prefix. This is only 8,192 of
168,000 bytes: subsequent transfers, SPU destinations and sample decoding are
unvalidated. The observed entry route does not call the other proposed WDS
consumer at `0x80037fd8`.

## Polling and completion

The overlay poller at `0x80085c90..0x80085ee8` accepts the selected music ID.
It returns the u32 pending sentinel `ffffffff` or completion zero. The caller at
`0x80078b5c..0x80078bc4` polls when its prior result equals that sentinel and
commits the return to `0x8004f308`. Extended event instruction `fe/a2` tests
this music result. It is not a combined field, dialogue or player-ready flag.

The source-derived policy proceeds in this order:

1. If the selected wave read is pending (flag exactly one), ask the wave helper
   to process up to five chunks. Its pending result ends this poll. Completion
   waits on audio-service flag `0x10` through resident `0x8003bdfc`, releases
   staging storage, records the selected bank and clears the wave-pending flag.
2. A zero shared-bank flag requires a shared wave resource. Shared state zero
   starts file 3 and enters state `80`; that poll returns pending. State bit
   `80` polls completion. Success changes shared state to one and clears the
   two observed shared-release controls. These paths are source/synthetic only
   in the current route; the shared resource is already ready when polled.
3. A deferred sequence request (flag exactly one) reads its sequence when the
   loaded selector differs, sets sequence-pending, then clears deferred-read.
   This poll returns pending even when no new file was necessary.
4. Query disc busy. Any nonzero result returns pending. With sequence-pending
   exactly one, either resume the cached sequence with arguments 127 and 240,
   or create/start the new SMDS. The usual start parameter `ffffffff` chooses
   arguments 127 and 0; other values choose 0 and 0 plus a configuration call.
   Mark sequence active, clear sequence-pending and record the loaded selector.
5. Set the start parameter to `ffffffff`, mark completion and return zero.
   The caller, rather than the poller, commits the music-result gate.

Wave/disc service returns are explicit original inputs to this policy. It
emits named service requests; no unknown service runs as a no-op. The policy's
state comparison covers all 196 captured global bytes, including opaque bytes,
but does not cover every global or side effect inside those services. Two
deferred reads, two creations and two starts additionally match original
consumer calls by frame, source selection and applicable arguments. Other
service operation details remain source-derived.

## Observed scope and verification

There are 35 exact original polls and 35 exact caller commits: selector 12 has
nine pending returns and one completion; selector 6 has 24 pending returns and
one completion. The deferred reads occur in frontend runs 573 and 1591, with
creation/start/completion in runs 591 and 1601. These frontend observations do
not establish a game simulation cadence or original hardware timings.

The earlier 993 pending `fe/a2` calls occur during initialization in runs
510–511, before these requests complete. The batch safeguard lets that original
initialization finish despite the wait. No ready `a2` branch is observed. Tests
for other sentinel values, PC wrapping, shared wave loading, sequence reuse and
alternate start parameters are authored synthetic cases, not original gameplay
proof. Music request replacement/stop behavior, source lifetimes, all remaining
wave bytes, sequence/sample/effect formats, playback, Mono/Stereo/Wide, FMV
routing and player readiness remain required work.

Three fresh instrumented cold boots match the control's full RAM, external
state, audio file and all 18 PNGs exactly. The verifier rejects 21 corruptions
of original input, state, control flow, object backlinks, registers and wave
digest. Earlier branch and event-state comparisons also pass with the extended
decoder. The finding remains supported pending independent review.

Enter the pinned `#observation-trace` Nix shell. Generate a private specification
with `tools/analysis/verify_media_ready.py prepare --raw <raw> --profile <profile>
--map 23 --group <group> --output <new-private-file>`. Use each of `events`,
`poll` and `resources` with the authored `tests/reference-inputs/field23-entry-dialogue.json`
route and `tools/reference/scenario.py --trace-instructions <spec>`. Run
`verify_media_ready.py compare` with the same source arguments, group and
`--capture <run>/capture`. All original payloads and trace specifications stay
under `.local/`. Public tests use authored synthetic bytes only.
