# Independent evidence workflow

Game-specific claims start with the user's unmodified original images and their
hash-defined reference profiles. General-purpose disassemblers, debuggers, media
tools and external emulators may assist; other Xenogears implementations, source,
symbol maps, format descriptions and reverse-engineering results may not supply
this project's implementation foundation. This includes undocumented knowledge
copied from an AI answer. Do not use downloaded patched images as a baseline.

1. Select an exact profile in `analysis/reference-profiles.json`. Re-run container
   integrity and decoded-track hashes before collecting evidence. A matching file
   name, serial, ISO date or header alone is insufficient to identify a revision.
2. Create a local capture directory under `.local/`. Record tool versions, Nix
   lock, command arguments, working directory, disc/profile, executable and overlay
   hashes, BIOS hash if applicable, emulator/hardware settings, save/input/seed
   prerequisites, and logical start/end events. Keep bytes and personal paths local.
3. Create a finding using `analysis/templates/finding.json`. Record file-relative
   byte offset and/or zero-based track LBA, sector size and user-data offset. For
   addresses, name the address space and executable/overlay identity; an address
   reused by two overlays is two distinct locations. Nonapplicable fields carry
   an explicit reason, never an unqualified missing value.
4. Separate observed facts from interpretation. Record observed inputs, outputs,
   state changes and timing units, competing explanations, confidence and unknowns.
   Static bytes establish static structure; a trace establishes only its exercised
   paths. Neither an emulator's output nor automatic pseudocode is automatically
   a correct interpretation. Review inferred arithmetic, types and timing rules.
5. Give a reproducible procedure with preconditions, exact commands/input record,
   independently sourced expected result, actual result, tolerances and local
   artifact hashes. A comparison must never manufacture its oracle from the new
   implementation's own output. Keep non-observed expected behavior `unknown`.
6. Review provenance and reproduction. Publish original explanatory prose,
   addresses, minimal factual metadata and hashes; keep copyrighted media, code
   dumps, saves and traces containing original payloads private. Public synthetic
   fixtures prove tooling invariants only, and are labeled accordingly.
7. Link the finding to content inventory entries, subsystem states and test facets.
   Promote only what the evidence proves. Record each unresolved case and the
   next experiment; unknown instructions must eventually produce clear diagnostics,
   never silent no-ops. Recheck affected findings on any reference/tool change.

Confidence vocabulary: `unknown` (no supporting observation), `tentative` (a
plausible interpretation), `supported` (repeatable evidence with unresolved scope),
`confirmed` (reproduced and independently reviewed within the stated scope).
Confidence and subsystem maturity are separate axes. A confirmed boot filename
does not mean the loader or game is decompiled or behaviorally validated.

`tests/validation-results.json` records only executed facet outcomes. Its evidence
links must resolve to `analysis/findings/` or `docs/verification/` records. Every
multi-target result must enumerate tested targets; one platform pass does not close
the others. A test definition, generated matrix, test count, synthetic fixture, or
successful compile cannot establish original-game behavior.

The metadata probe is deliberately limited to standard raw CD sectors, ISO9660
directories and PS-X EXE headers. It records neither a game-content directory nor
retail revision names. `chdman verify` verifies the container's recorded integrity;
it does not establish a clean retail dump or check behavior. Hash-defined profiles
are exact accepted research inputs; gameplay/runtime support is a separate gate.
