# Xenogears: Ex Machina

Independent Xenogears decompilation, native PC port and modding tools, with Arch
Linux leading development. The native application does not run the complete game.

Phase 1 now targets a **complete binary-matching PS1 decompilation of both discs**.
It does not stop at one playable slice or require every recovered function to be
ported into a host-side ownership model. See [the plan](plan.md) and the short
[matching workflow](docs/matching.md). Code and data are named by their module's
prefix and what they do ([Names](docs/matching.md#names)); each definition keeps its
original address in its comment, so `git grep -i -n 80031bdc src/decomp` finds the code
at an address.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C src/decomp smoke
make -C src/decomp all-split all-verify all-coverage
```

That smoke test exercises MIPS assembly/linking and exact comparison on an authored
fixture, not Xenogears decompilation. The game targets need the user's discs (the
matching workflow's clean build); `all-verify` then compares every rebuilt resident
executable and overlay with the original byte for byte, and `all-coverage` reports
source coverage by class. Those reports, not this file, own the status. Source
recovery and binary matching are reported separately.

Everything that ships is under `src/`: the decomp (`src/decomp`, its own
mini-project with targets, Makefile and headers), the port layer (`src/port`) and
the Rust runtime (`src/runtime`). The former host C++ reconstruction was retired;
the findings that cite it name files at commit b969b11. Run the public checks
(validator, Python tests, ruff and, in the runtime shell, the Rust workspace) with:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/runtime
python3 tools/repository/check.py
```

[Development](docs/development.md) covers focused commands. The
[later-phases handbook](docs/later-phases.md) collects what the decomp gives
Phases 2-5: the oracle, the program's structure, the boundaries a port replaces
and the hazards it must keep in view. Source profiles live in
`analysis/reference-profiles.json`; detailed findings are read on demand.
Original images stay in ignored `discs/`, and extracted bytes/captures/saves in
ignored `.local/`. The [source allowlist](packaging/source-files.txt) is audited.
The Nix path inputs contain tool configuration only, never original data.

The [native-agent](docs/agent/README.md) and [authoring](docs/authoring/README.md)
specifications preserve product requirements, not implemented capability. The
plan places native gameplay before the authoring bridge. Historical Phase 0
records and finding IDs retain their original scope; retired facet numbers are
not the current phase checklist.
