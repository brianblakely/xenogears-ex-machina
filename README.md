# Xenogears: Ex Machina

Independent Xenogears decompilation, native PC port and modding tools, with Arch
Linux leading development. The native application does not run the complete game.

Phase 1 now targets a **complete binary-matching PS1 decompilation of both discs**.
It does not stop at one playable slice or require every recovered function to be
ported into a host-side ownership model. See [the plan](plan.md) and the short
[matching workflow](docs/matching.md). Code and data are named by their module's
prefix and what they do ([Names](docs/matching.md#names)); each definition keeps its
original address in its comment, so `git grep -i -n 80031bdc decomp` finds the code
at an address.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
make -C decomp all-split all-verify all-coverage
```

That smoke test exercises MIPS assembly/linking and exact comparison on an authored
fixture, not Xenogears decompilation. The game targets need the user's discs (the
matching workflow's clean build); `all-verify` then compares every rebuilt resident
executable and overlay with the original byte for byte, and `all-coverage` reports
source coverage by class. Those reports, not this file, own the status. Source
recovery and binary matching are reported separately; existing C++ comparisons
imply neither a PS1 match nor a complete decomp.

The existing `xem-reconstruction` library, original scenarios, findings and tests
remain useful reference/portability assets. Its state declarations are separated
into subsystem headers; `Program` remains its integration owner, not the design
for new PS1-target source. Run its public build with:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/check.py --preset debug
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
