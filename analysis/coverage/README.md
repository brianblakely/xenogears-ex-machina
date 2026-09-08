# Original-game coverage inventory

`inventory.json` is the category and completeness ledger for both exact selected
reference profiles. Its Phase 0 baseline is ready: all ten categories on both
discs have an original source catalog, attributed source family, or directly
observed entry, with explicit remaining questions. The exhaustive content catalog
remains incomplete. `baseline_inventory_ready` and `catalog_complete` are separate
gates; passing the former cannot set the latter. Complementary records keep
measured facts separate from content meanings that have not been recovered:

- `sector-survey.json` accounts for every raw sector by its declared CD-XA header
  type. Its contiguous spans and end markers are physical coordinates; they are
  not counts of movies, audio tracks or game scenes.
- `source-index.json` records the independently observed candidate index at
  logical LBA 24. Each seven-byte slot has a candidate source LBA and signed
  length. Positive entries, negative-count groups, empty slots, unknown zero-length
  markers and terminal markers are preserved. Public records reproduce the exact
  measured table hash. No runtime content IDs are assigned from slot numbers.
- `observed-entrypoints.json` records actual original-program presentation along
  the qualified opening/menu route and reviewed cold-boot scenarios. Its `OBS-`
  identifiers belong to this project. Original field selectors 14, 15 and 290
  have separately recovered identities. The 32 scoped observations may reuse a
  capture for distinct category relationships; they are not 32 unique locations.
  Remaining IDs, totals and progression coverage stay open.
- `source-fingerprints.json` measures explicitly defined byte projections for all
  7,451 positive source slots, preserving duplicates and zero-filled entries. It
  records 2,181 distinct projection keys shared by the two sources. This is a
  source-byte comparison, not a gameplay-equivalence or unique-asset count.
- `field-pairs.json` supplies field-pair availability from the observed
  1,460-entry source group. Both original loader paths and source-table arithmetic
  corroborate its numeric field selectors in EVID-REF-007. It distinguishes
  non-dummy pairs from the exact
  original CDMAKE dummy marker. The first source has 730 non-dummy pairs; the
  second has 205 non-dummy pairs and 525 dummy pairs. A non-dummy pair does not
  prove reachability, map initialization, or player control. The category ledger
  separately lists these catalogued original IDs and actually observed selectors
  14/290 on Disc 1 and 15/290 on Disc 2. Neither count is a unique playable-location
  total.
- `checkpoints.json` defines representative future validation scenarios across
  both discs. A definition with an unknown oracle is not an executed test.
- `media.json` catalogues 17 and 12 movie selectors from the original counted
  movie group and loader arithmetic. All measured video frame headers have their
  declared chunks. It also records 153 WDS, 106 SMDS and 46 SEDS source slots on
  each disc, preserving identical duplicates, original dummy-music labels and
  older header variants. These are source-data counts, separate from playback and
  unique song, effect or cinematic identities.

The category ledger also anchors the original Worldmap and Battling presentation,
the selectable Bonus Battling/Practice/Tutorial activity relationship, title and
field menus, original transition observations, shared Battle code, and original
File/card-management code and UI. Each anchor binds a passed finding to the same
source profile and an exact source projection, catalog or captured entry.
Unsuccessful Battle startup remains recorded separately. File grids and initialized
RAM headers establish no actual save/load or card write. Story access, additional
optional activities, complete encounters and semantic content names remain open.

The [Phase 1 handoff](../../docs/phase1-handoff.md) identifies the measured starting
points and the format, event, battle and persistence work still required. This
baseline inventory does not advance any subsystem to decompiled, implemented or
behaviorally validated.

The two raw tracks explicitly contain `DS01_XENOGEARS` and `DS02_XENOGEARS` at
logical LBA 23. Both contain 43 negative-count groups whose following positive
record counts match exactly. There are 3,728 positive source entries on disc 1 and
3,723 on disc 2. Independent ISO boot extents and XA end markers corroborate the
candidate structure. Group meaning, logical-size conventions and
gameplay categories still require direct original-source evidence.

To reproduce the read-only physical surveys after CHD verification/extraction,
enter the pinned Nix shell from the repository root. Each output path must be new:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/reference/sector_survey.py \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/references/source-1/review-sector-survey.json
python3 tools/reference/index_survey.py \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/references/source-1/review-index-survey.json
```

Repeat for source 2 using profile `na-slus-00669-5eab85c683d4`. Both tools reject a
different full raw-track hash. Full measurements remain private; the public JSON
contains reviewed factual metadata and provenance. Findings EVID-REF-004 through
012 state their actual measurements, limits, and reproduction
procedures. Reproduce source fingerprints in the same Nix shell:

```sh
python3 tools/reference/fingerprint_survey.py scan \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/references/source-1/review-fingerprints.json
python3 tools/reference/fingerprint_survey.py scan \
  --raw .local/references/source-2/disc.bin \
  --profile na-slus-00669-5eab85c683d4 \
  --output .local/references/source-2/review-fingerprints.json
python3 tools/reference/fingerprint_survey.py compare \
  .local/references/source-1/review-fingerprints.json \
  .local/references/source-2/review-fingerprints.json \
  --output .local/references/review-fingerprint-comparison.json
python3 tools/reference/media_survey.py \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/references/source-1/review-media.json
```

Use the [original-game scenario system](../scenarios/README.md) for emulator work.
It cold-boots through the original loader, accepts declarative setup through
verified scene adapters, uses explicit readiness conditions and bounded timeouts,
executes ordered inputs and retains provenance. Unrecovered battle, position and
progression adapters remain an explicit backlog; unsupported setup fails clearly.
The historical route's emulator checkpoints remain qualified observations. No
offsets, game-specific catalogs or source from another game may serve as Xenogears
evidence.
