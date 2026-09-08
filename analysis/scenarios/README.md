# Original-game scenario testing

Run original-game observations through `tools/reference/scenario.py`. Each run
starts from reset, reaches the original Kernel MENU, and uses the game's own
dispatcher and field loader. A numeric field can be tested without first playing
to it or obtaining a save state. The general workflow follows the neighboring
Slap Stick project's scenario testing design; all Xenogears addresses and source
correlations here were independently recovered from the supplied original discs.

From the repository root:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#observation
python3 tools/reference/scenario.py --list
python3 tools/reference/scenario.py painting-room \
  --content 'discs/Xenogears disc 1.chd' \
  --output .local/scenarios/painting-room-example
python3 tools/reference/scenario.py disc2-field-15 \
  --content 'discs/Xenogears disc 2.chd' \
  --output .local/scenarios/disc2-field-15-example
python3 tools/reference/scenario.py --map 42 --dry-run
```

Use a new output directory for every run. `--map` defaults to the measured Disc 1
profile. For Disc 2, pass `--profile na-slus-00669-5eab85c683d4` with its CHD.
Named presets and custom JSON files can also be overridden explicitly with
`--map` or `--profile`. The actual content must match the selected profile's CHD
hash. The scene catalog's raw-track/index identities and resource-slot formula
must match the recovered adapter. Dummy or absent source pairs fail before the
emulator starts. Source availability does not establish that a particular scene
can finish its entry script with reset-default progression.

The presets currently cover the original Kernel MENU, Disc 1 field 14's painting
scene followed by opening the main menu, and Disc 2 field 15's rocky outdoor path.
The last label is a visual description; its in-game location name is unresolved.

## Scenario contract

`schema.json` describes the accepted JSON shape. The compiler also enforces
source availability, nonoverlapping guarded writes, button names, and the total
execution budget. A minimal scenario is:

```json
{
  "schema_version": 1,
  "name": "field-example",
  "source_profile": "na-slus-00664-39c547a9afc6",
  "entry": {"type": "field", "map": 14},
  "launch": {"ready": "map", "timeout_frames": 5000, "settle_frames": 1800},
  "steps": [
    {"buttons": ["cross"], "after_frames": 240, "capture": true},
    {"buttons": ["cross"], "after_frames": 600, "capture": true},
    {"buttons": ["square"], "after_frames": 300, "capture": true}
  ]
}
```

`kernel` entry accepts no map. `field` entry accepts a numeric map selector; only
the source pairs in the measured catalog are eligible. Launch readiness requires
the selected map and two original field-initialization indicators to agree for
two consecutive frontend frame boundaries. It establishes map initialization;
dialogue and scripted entry events may still be running. The painting-room preset
therefore waits for its observed entry scene and advances both dialogue pages
before requesting the menu. See [EVID-REF-007](../findings/EVID-REF-007.json).

Input steps hold the named buttons for `hold_frames` (default 8), release all
buttons, then run `after_frames` (default 60). Wait steps use `wait_frames`.
`capture: true` records an image and RAM at completion of that step. These counts
are frontend `retro_run` calls, not recovered simulation ticks. Every launch has
bounded readiness waits and every complete program has a maximum 36,000-frame
budget. A timeout or byte-fingerprint mismatch fails with the observed conditions
and writes recorded so far; it cannot produce a successful scenario report.

Advanced investigations may provide `state_writes`, each with a physical RAM
`offset`, equal-length lowercase hexadecimal `expected` and `value`, and a
nonempty `reason`. All fingerprints in a step are checked before any of its
writes. Launcher controls cannot be overwritten. Custom writes form a separately
labeled unreviewed setup step, and the entire program becomes an `analysis_probe`;
the recovered loader evidence does not validate arbitrary game-state changes.
No typed party, inventory, or progression semantics are implied by this escape
hatch. Low-level adapter research uses the same bounded executor via
`observe.py --program`, with `kind: analysis_probe` until its hooks are reviewed.

## Captures and provenance

The output contains the normalized `scenario.json`, compiled `program.json`,
the exact `source-catalog.json` read before compilation, and `started.json` with
their hashes and the compiler identity captured before execution. The nested
`capture/` directory holds the source/core/BIOS/executor identities, original
options, ordered condition and mutation events, screenshots, audio, RAM, and a
final external-emulator state. Only a successful `report.json` marks completion.
Failed runtime programs record `capture/failure.json` instead. All output stays
under ignored `.local/`.

Scenario runs use the pinned external core's interpreter so instrumented code
writes cannot leave stale translated instructions. The default BIOS is explicitly
HLE; an optional user-supplied `--bios` is hashed and a reported HLE fallback is
rejected. These are original-game reference observations. The shipped native
runtime does not embed this core, and cold-boot repeatability is not evidence of
native behavior or hardware equivalence.

## Adapter backlog

| Capability | Current status | Evidence needed to extend it |
|---|---|---|
| Original Kernel MENU | Verified on both selected discs | Other revisions require independent source selection and guards |
| Numeric field entry | Verified adapter, individually observed fields 14 (Disc 1) and 15 (Disc 2) | Scene-by-scene launch and entry-script coverage |
| Player position and facing | Unsupported; rejected JSON fields | Recovered actor identity, coordinates, collision-safe spawn and entry ordering |
| Party, inventory, equipment and progression | Typed setup unsupported; rejected JSON fields | Recovered layouts, original initialization paths and scenario-specific validity checks |
| Direct battle/formation launch | Unsupported; rejected entry type | Original transition, formation selectors, party setup, readiness and return path |
| World map, Battling and movie launch | Kernel labels observed; no dedicated adapters | Original loader parameters and readiness conditions |
| Player-control readiness | Unsupported; only `ready: map` accepted | Original event/control ownership and stable release conditions |
| Named scene catalog | Three presets; numeric IDs for other candidates | Independently corroborated location names and progression variants |

Unknown setup fails explicitly. Extend an adapter only with source coordinates,
guarded mutations, repeatable original execution, and a reviewed finding. Keep
data availability, initialized scenes, controllable scenes, and native coverage
as separate measurements.
