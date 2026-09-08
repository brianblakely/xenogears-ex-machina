# Phase 0 evidence and Phase 1 handoff

Phase 0 establishes the independent project's build, evidence standards, complete
requirements matrix, original coverage inventory and go-anywhere reference test
system. It does not implement native gameplay. No Phase 1 checkbox is complete,
and private decoder/disassembly probes are evidence aids rather than reviewed
decompiled source or production importers.

## Inventory scope

The baseline inventory covers all ten requested categories on both exact selected
source profiles. Every row has a reviewed original-source catalog, attributed code
family or observed entry point, and records unknowns plus its next evidence step.
`baseline_inventory_ready` may pass while `catalog_complete` remains false.
Exhaustive semantic content IDs, story variants, ordinary progression, complete
formats and behavior remain work for Phase 1 onward. A source-slot count is not a
unique scene, song, encounter or save-format count.

| Category | Original basis | Still unresolved |
|---|---|---|
| Fields | Original selector arithmetic, 730 non-dummy Disc 1 pairs and 205 Disc 2 pairs; observed selectors 14/290 and 15/290 respectively | Semantic names, reachability, story variants, geometry/collision/script formats and player-control readiness |
| World map | Original mode 3 source and both-disc terrain/machine/minimap captures | Location/transport IDs, coordinates, transitions, progression variants and rules |
| Battles | Original mode 2 source slots 38/33, exact shared packed/decoded identity and resident/overlay call path | Formations, enemies, party setup, Gear/on-foot distinctions, readiness and combat behavior; the recorded reset-default attempt is unsuccessful |
| Menus | Opening-room branches, both-disc bridge main menus and title UI, plus exact shared menu code | Every branch, progression variant, layout/resource format and menu-state meaning |
| Events | Original prologue/painting interactions and Disc 2 New Game-to-disc-request transition | Event-script IDs/opcodes, scheduling, side effects, ordinary Disc 2 story sequences; the title transition may be resident UI code |
| Audio | 305 signature-bearing slots on each disc: 153 WDS, 106 SMDS, 46 SEDS, with exact cross-disc identities | Unique songs/effects, embedded resources, sequencing/sample semantics and audible equivalence |
| FMVs | Original movie selector/group arithmetic; 17/12 source entries with complete measured video-header chunks | Semantic cinematic names, original script references, decoding, playback and audio/video timing |
| Saves | Shared menu/card source family, File UI on both discs and initialized original header/prefix buffers | Ordinary files, slot/write/read/checksum behavior, corruption recovery, save/load equivalence and the second BASLUS prefix's purpose |
| Optional activities | Original selectable Bonus Battling/Practice/Tutorial relationship on both discs | Ordinary story access, optionality conditions, rewards, other activities and side quests |
| Minigames | Original Battling mode on both discs and Disc 1 Bonus setup, Practice Gear selection, Tutorial and Exit captures | Complete minigame rules, results, progression integration and other minigames |

The physical source catalog independently accounts for every positive source slot
and preserves dummy/zero/duplicate entries. Exact source-index numbers, raw versus
logical coordinate units, decoded overlays and observed numeric selectors retain
separate namespaces. Unknown names must remain unknown.

## Original sources and provenance

`analysis/reference-profiles.json` pins CHD, decoded raw-track, boot executable
and configuration hashes for both supplied North American sources. It records
hash-defined revisions, without guessing a retail revision label. The original
`DS01_XENOGEARS` / `DS02_XENOGEARS` labels and reproduced New Game response establish
disc sequence. Source filenames are not identity evidence.

Findings EVID-REF-001 through 012 record actual original source coordinates,
commands, measurements, interpretations and private artifact hashes. Begin with
EVID-REF-007 for the dispatcher/field loader, 008 for source projections/dummies,
009 for Worldmap/Battling routes, 010 for media, 011 for menu/card code, and 012 for
Battle source attribution. Do not infer behavior merely from the labels in this
handoff; inspect the linked finding and original bytes.

Original images, decoded code/assets, RAM, screenshots, audio and generated cards
remain under ignored `discs/` or `.local/`. Public source archives use an explicit
allowlist, reject symlinks and private paths, and are tested without original data.
New public files need an intentional allowlist entry. Never pass the repository
root as a Nix path input: the standalone flake input is `path:./nix`.

## Reference execution

Use `tools/reference/scenario.py` and the documented
[scenario contract](../analysis/scenarios/README.md) for emulator research. It
cold-boots original code, binds source/catalog identity, guards setup writes,
waits on explicit readiness conditions with timeouts, runs ordered inputs and
records source/tool/core/Nix/BIOS provenance. Reviewed Kernel and numeric field
adapters exist. Typed battle, position and progression adapters are explicit
backlog items and are rejected until their original semantics are recovered.
Arbitrary guarded RAM changes remain separately labelled analysis probes.

The pinned external PCSX-ReARMed core with HLE BIOS is research instrumentation,
not part of the native runtime or a hardware-timing oracle. Two fresh painting-room
cold boots match ten PNGs, full RAM, external state and audio exactly in the
recorded configuration. That scoped comparison does not establish general game
determinism. A separate checkpoint-restore experiment matched a final image while
two RAM bytes differed; their meaning remains unresolved. Prefer reproducible
cold boots and never hide those differences with an unexplained mask.

Kernel readiness proves the original Kernel loop is ready; field readiness means
map initialization. Neither implies player control or readiness of a later input
target. The Worldmap, bridge, menu and minigame routes were reviewed visually after
bounded inputs. Distinguish input-program completion from target success.

## Development and next work

All development tools come from the pinned Nix environment. The default shell has
the build, format and public test tools; `#analysis` adds CHD tools,
`#observation` adds the external core, and `#disassembly` adds Capstone. Core and
disassembler dependencies are analysis-only and recorded in `docs/dependencies.json`.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py
python3 tools/repository/reproduce.py
```

The public gate checks every reviewed requirement facet, evidence schemas,
baseline/exhaustive inventory distinctions, source isolation, formatting and
CMake/Ninja/CTest presets. Source-only Release reproduction uses two isolated
copies, records exact archive/binary hashes, installs only the baseline executable
and license, and preserves immutable proof logs.

Phase 1 should recover the code, data formats and event behavior needed for an
actual field-to-battle slice. Turn original probes into reviewed, bounded analysis
tools and decompiled functions only after resolving types, arithmetic, state and
source identity. Recover event instructions and a valid battle setup/readiness
contract; then validate focused original scenarios. Keep unresolved symbols,
formats, operands, timing and save behavior explicit. Later native subsystems need
their own original behavioral evidence; no synthetic fixture or Phase 0 build can
pass a gameplay requirement.
