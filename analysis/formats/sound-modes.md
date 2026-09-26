# Sound output modes (Mono, Stereo, Wide-Stereo) and audio routing

Scope: the NA Disc 1 resident sound driver and the menu overlay's Sound screen,
as executed on the frozen title → opening movie → field 23 route. Game facts
come from the original instructions and from matched external-emulator
recordings. General SPU documentation (psx-spx, SPU register chapter) was used
only for register meaning; Sony library documentation was not available and
was not used. Evidence levels: **observed** (instructions read, or measured in
a capture), **inferred** (follows from observed facts), **untested**.

## Mode state and selection

The mode lives in bits `0x700` of the driver flag word `8005957c` (u16).

| Mode | Bits | 80038824 returns | Selected by |
| --- | --- | --- | --- |
| Mono | `000` | 0 | Sound screen |
| Stereo | `100` | 1 | driver init default; Sound screen |
| Wide-Stereo | `300` | 2 | Sound screen |
| (unnamed) | `500` | 2 | no caller |

- `800386c4(mode)` (observed): clear `0x700`; 1 → `|100`, 2 → `|300`,
  3 → `|500`, any other value leaves mono. Then `80038df4` recomputes the
  master/CD/reverb pairs, `8004e574` (libspu) writes the reverb output volume
  pair to SPU `+184/+186` at once, every sequence voice is marked `0x100`
  (`8003e680` over the list at `80059564`) so the next tick recomputes voice
  volumes, flag `0x4000` would call `8003885c` (below), and the "mode voice"
  record at `80059518` is recomputed (below).
- `80038824()` (observed): `0x700` clear → 0; `0x600` clear → 1; else 2.
- Callers (observed, whole resident and all analysed overlays): driver init
  `80037b88` calls `800386c4(1)`; the menu overlay's Sound screen reads the
  current mode through `80038824` and applies the choice at `801d9a90`. The
  title screen's Sound option is this same menu-overlay screen (the capture
  traces return to `801d9a98`). No code passes 3, so `500` is unreachable.
  On-screen labels are "Stereo", "Wide-Stereo", "Mono" (observed title frame);
  no explanatory text is shown.
- Driver init `80037b88` (called once from `80019668` with `a0 = 0`) sets the
  flags to `a0 | b801`, so `0x4000` is never set in this executable (no other
  write to `8005957c` sets it; observed by enumerating every store).

## Per-mode arithmetic

### Voice left/right volume — `8003ebf0`, pan law at `8003ece8..8003edbc`

Per sounding voice record with change bit `0x100` in `+2`:

```
att    = s16(+0x7a)
scale  = clamp(att - ((att * s16(+0xd2)) >> 15), 0, 0x7fff)
volume = ((s16(+0x76) * scale) >> 15) * s16(seq+0x72) >> 16      (max about 0x3fff)
pan    = clamp(s16(+0x74) + s16(+0xd4) + s16(seq+0x8a), 0, 0x7f00)   0 left, 0x4000 centre
```

- Flag `0x100` set (Stereo and Wide alike): two linear ramps.
  For `p = pan < 0x4000`: left `= 0x7f00 - (37*256*p >> 14)`, right
  `= 45*512*p >> 14`; for `pan >= 0x4000` the same with `p = 0x8000 - pan`
  and the sides swapped. Both give `0x5a00` at the centre; the edge is
  `0x7f00 / 0`. Output `= (gain * volume) >> 15` (signed multiply,
  arithmetic shift). Neither gain is negative; the mode never negates a voice.
- Mono (flag `0x100` clear): both sides `= (volume * 45 * 512) >> 15`, the
  stereo centre value regardless of pan.
- Wide changes nothing per voice (observed: the field RAM images of the Stereo
  and Wide runs have identical voice records; Mono differs, e.g. a hard-left
  voice `144d/0000` in Stereo is `0e62/0e62` in Mono, ratio 0.7085 ≈
  `0x5a00/0x7f00`).
- The values are staged at voice `+0x38/+0x3a` and written by the tick's
  `8003e900` to SPU voice `+0/+2` (direct volume mode).

### Master, reverb and CD pairs — `80038e6c(value, pair, is_reverb)`

Both halves are set to `value`; then, only when `flags & 0x600`:

| Flags | Master pair (`8005a3c4`, SPU `180/182`) | Reverb output pair (`8005940c`, SPU `184/186`) |
| --- | --- | --- |
| `300` Wide | right `= -value` | left `= -value` |
| `500` | left `= -value` | right `= -value` |

Negation is two's-complement 16-bit (`negu`). The master value is `0x3fff`
from init (`80038c68(3fff, 0)`), so Wide writes `0xc001`; libspu
`SpuSetCommonAttr` (`8004d988`) masks it to 15 bits (`0x4001`), which psx-spx
documents as direct-mode volume −0x3fff (range −4000h..+3FFFh, "negative
volumes are phase inverted"). Reverb depth comes from each sequence (below);
field 23's is `0x3c00`, so Wide writes `0xc400` (observed in RAM).

- Master fades (`80038c68` with frames ≠ 0, stepped by the odd-tick slide in
  `8003c028`) re-apply the same negation at every step (observed in the tick's
  master slide, `8003c484` inside `8003c028`; reconstructed in
  `sound_tick.cpp`).
- The CD input pair (`8005a3d0`, SPU `1b0/1b2`, set by `80038d18`) is never
  negated or changed by the mode.
- Mono and Stereo leave all three pairs positive.

### CD mix — `8003885c` (unreachable)

Behind flag `0x4000`: Stereo/Wide would program CD attenuation
L→L = R→R = v, L→R = R→L = 0; Mono all four `= v >> 1` (libcd `CdMix`,
`8004138c`). Because `0x4000` is never set, the game never calls `CdMix`; the
CD-to-SPU attenuation stays at its reset/library default (the emulator passes
each channel straight; untested on hardware).

### Mode voice — `800387c4..8003880c` (dormant)

For a record at `80059518` with bit 1 set: in any stereo mode the first voice
gets left `level << 7`, right 0 and the second left 0, right `level << 7`;
Mono gives both voices `level << 6` on both sides. The pointer is only ever
zeroed (init `80037c34`) and was 0 in the field RAM images of all three
mode runs.

## Reverb, master and CD routing

- Reverb type, depth, delay and feedback belong to each music sequence
  (object `+0x41`, `+0x44`, `+0x42`, `+0x43`), applied through `80038934` at
  sequence creation (`8003b22c`, call `8003b304`) and resume
  (`8003aa30`, call `8003aa78`), both only when driver flag `0x1000` is set,
  which init sets, and by a sequence opcode (call site `8003d51c`, type −1 keeps
  the type). `80038934` recomputes the pair via `80038df4`, so the mode sign is
  applied to every new depth. Field 23: type 4, depth `0x3c00`.
- Init `80038db4(0, 1)`: CD reverb off, CD audio input on (SPU control bits
  2 and 0). Streamed audio therefore never enters the reverb. The opening
  movie's audio is CD input (inferred): in Mono it keeps L≠R (correlation
  0.398) and is sample-identical to Stereo. Wide leaves its left channel
  unchanged, so no reverb return is present, and voices under the Mono law
  without reverb would give L = R.
- Streamed/FMV audio routing: CD input volume `0x7fff` at init
  (`80038d18(7fff, 0)`); the mode-6 overlay sets it to 0 at once
  (`8007384c`) and fades it to 0 over 10 frames (`80076a74`, `80076acc`),
  as does the field movie skip (`800a8034`). None of these depends on the
  mode, and the mode never touches the CD pair.

## Matched recordings

`p1cov-sound-{stereo,wide,mono}-v1` (and the bit-identical replicas
`p1media-sound-*-v1`) run one input program that differs only in the Sound
screen's cursor presses; each lasts 36,000 frames (PCSX-ReARMed, HLE BIOS,
44.1 kHz). The recordings are bit-identical up to sample 1,471,335
(frame 2001.8, the first cursor press) and sample-aligned afterwards.
Numbers from `tools/analysis/audio_modes.py` (windows in frontend frames):

| Window | Stereo corr | Wide corr | Mono corr | Wide L vs Stereo L | Wide R vs Stereo R | Mono vs Stereo |
| --- | --- | --- | --- | --- | --- | --- |
| Title music after selection, 2320–2620 | +1.000 | −1.000 | +1.000 | differs (menu effects) | inverted where only music plays | differs (effects) |
| Opening movie, 2960–20780 | +0.398 | −0.398 | +0.398 | identical (100% of 13,097,700 samples) | `−R`, residual −3..+3 (rms 0.98) | identical |
| Field 23, 21400–28900 | +0.400 | −0.390 | +0.699 | differs (rms 3271) | `−R`, residual −3..+3 (rms 0.96) | differs |

- Inter-channel timing: the Wide/Stereo right-channel correlation is −1.000
  at lag 0 and −0.98 at ±1 sample in both movie and field: no delay.
- Field Wide left: the part that changes sign, (Stereo − Wide)/2, has rms 1337
  against 2351 for the unchanged part (first 10 s); it is uncorrelated with
  the unchanged part at lag 0 (0.065) and peaks at a delay of 3935 samples
  (89.2 ms, 0.465). A delayed, uncorrelated component is the reverb return,
  consistent with the negated left reverb depth.
- Title music is mono content (Stereo L = R exactly); Wide turns it into pure
  L−R (sum/difference −73.6 dB).
- Mono field output keeps L≠R (correlation 0.70): the reverb unit returns a
  stereo signal from equal voice inputs, and nothing in Mono folds it down.

## Music, sample and effect lifetimes on the slice routes

Driver entry points each route executes (execution census of the `p1cov-*`
captures; all are reconstructed in `src/reconstruction/sound_*.cpp` except
where noted). The mode affects none of them; it only rescales their voice
output as above.

| Object | Created / started | Stopped / released | Routes |
| --- | --- | --- | --- |
| Sequence (music) | `80039850` create (header attributes and reverb `8003b22c`), `80039a80` start; resume `8003aa30` (not reconstructed; stops `sequence_resume`) | `80039c4c` stop, `800399d4` release | create/start on all 16 driver routes; stop/release on 13 (not load or movie routes) |
| Wave bank (samples) | `800380d0` upload (`80037fd8`, not reconstructed, on 10 routes) | `80038310` unlink + free SPU block `800396e0` | release on battle, menu and sound routes |
| Effect bank | `80038428` link | `8003852c` unlink, `8003a094` stop its voices | link on 18, unlink on 11 |
| Effect voices | `80039f9c`, `80039e60`, `80039db8` → `8003b644` | `8003a20c` | 10–18 routes |
| Driver | `80037b88` init (not reconstructed): flags `b801`, Stereo, master `3fff`, CD `7fff`, `80038db4(0,1)` (not reconstructed) | — | title/movie/sound routes |

## Conclusions

1. **Wide = static polarity matrix** (observed in source; observed in the
   emulator signal). Relative to Stereo: voices and pans unchanged; master
   right negative; reverb left depth negative. With the SPU applying the main
   volume after CD input and reverb return (observed in the emulator:
   Wide R is exactly −Stereo R including reverb and movie audio; not stated in
   the consulted documentation, untested on hardware), the output is
   `L = dry_L + cd_L − rev_L`, `R = −(dry_R + cd_R) − rev_R`
   (Stereo: `+ rev` on both). Dry voices and streamed/FMV audio become
   antiphase between channels; the reverb return is inverted on both channels,
   keeping its own inter-channel relation from Stereo.
2. **Not phase/delay expansion** (observed): no delay, no filtering, no
   partial cross-feed; every relation is sample-exact up to ±3 LSB.
3. **Not a surround matrix encoding** (inferred): no 0.707 centre/surround
   weights, no ±90° shift and no separate surround feed; the same gains apply
   to all sources. A passive or Pro Logic L−R decoder would steer the dry mono
   sum to the surround channel and the reverb to the front, i.e. the mix
   inside-out, so a matrix decoder is not the playback model. The intended
   model is two-speaker playback where antiphase dry content sounds wider and
   more diffuse (inferred; no primary documentation beyond the on-screen
   label was available; untested).
4. **Mono** (observed): every voice at the centre law on both sides; master
   and reverb unchanged; reverb stays stereo; CD/XA/FMV audio unchanged
   (sample-identical to Stereo in the movie).
5. **Stereo** (observed): linear two-ramp pan law, all pairs positive.
6. Reconstruction implication (inferred): reproducing a mode needs the signed
   register values and a mixer that applies the main volume after the CD and
   reverb sums; no decoder is required.
