# Contributing independently

This is a new MIT-licensed implementation. Derive game-specific findings from the
original binaries/data and observed execution, not another Xenogears project.
General-purpose infrastructure is allowed after dependency review. A general
emulator is an external observation tool; it is neither our runtime nor a source
of game-specific implementation. Never add a PS1 executable/CPU emulator as the
shipping game. Original event bytecode will use our recovered native interpreter.

For each contribution, supply:

- Original authorship and all consulted sources, including AI assistance and any
  prior exposure to related game-specific work. Disclose uncertainty; do not hide
  source provenance or use a generated paraphrase to launder borrowed code/RE.
- Related requirement facets, original finding IDs, source profile and coordinates,
  and a clear distinction between observation, inference and intentional change.
- Scope of ownership and license permission for code, fixtures, example mods,
  assets, documentation and generated material. Third-party assets are optional
  user-supplied packs unless we have explicit redistribution rights.
- Reproduction commands from the pinned Nix environment, executed results and
  remaining unknowns. Review arithmetic, persistence and progression behavior
  against independent original evidence before claiming equivalence.

Reviewers verify origins and the independence boundary before accepting code or
findings. If prior exposure may have influenced a game-specific implementation,
isolate the affected work and collect fresh original observations/review before
acceptance. Do not consult other Xenogears source, decompilation maps or format
documentation to fill unknowns. Popular-mod research in Phase 9 is limited to
player-facing descriptions and dated adoption evidence; do not adopt patches,
source, or an existing mod/translation/texture pack as an engine dependency.

Use `nix develop path:./nix` with the documented experimental feature flags for
all compiler/build/test/format/script tooling. Review lock-file changes separately.
Keep new game-specific source in its owning repository area and avoid global
compiler flags or dependencies leaking across subsystem interfaces.

Every new dependency requires a record in `docs/dependencies.json` with pinned
source/version, SPDX license, purpose, scope (build/analysis/authoring-build/runtime), alternatives,
decision, redistribution obligations, transitive/native-code implications, and
review. Nix lock pinning does not itself satisfy license or supply-chain review.
Optional and proposed libraries are not approved runtime dependencies. No network
dependency resolution occurs during CMake configure or ordinary tests.

Authoring packages have a separate exact manifest/lock and complete transitive
review in `docs/authoring/dependencies.json`. A build-only dependency does not
authorize shipping it with the game or a packaged developer SDK. Review native/WASM
payloads, embedded component notices, install hooks and any applicable source or
relinking obligations before redistribution. Follow `docs/authoring/README.md` for
the untrusted-build boundary; a trusted qualification fixture is not sandbox proof.

Run `python3 tools/repository/check.py` inside the Nix shell before review. Public
CI requires no disc/BIOS/save data, no accounts and no third-party mods. Original
data tests run locally and report only permitted metadata. Keep the explicit
`packaging/source-files.txt` allowlist current; its review is necessary even when
Git ignores would hide private files. Never use the whole repository as a Nix
path input or upload local evidence directories as CI artifacts.
