# Original-source analysis

[Matching-first recovery](matching.md) is the Phase 1 implementation workflow.
Ghidra remains the broad analysis environment; the matching shell is separate and
lightweight. Reuse existing qualified projects, exports, inferred types and findings.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra
python -m tools.analysis.ghidra_project \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/ghidra/new-disc1-analysis
```

The importer verifies original bytes and creates overlay-specific address spaces.
Preserve the resident/overlay identity alongside every address. Resume a qualified
working project instead of repeatedly importing the game. Ghidra paths cannot
contain hidden components; the importer handles its private temporary project.

Correct shared structures, calling conventions, GP context and dispatch boundaries
before re-decompiling dependent functions. Inferred names/types and automatic
pseudocode are not recovered source. Verify ambiguous signedness, delay slots,
fixed-point arithmetic, register forwarding, jump tables and hardware operations
against instructions. Keep unknown fields rather than inventing meanings.

Use m2c for matching-oriented C, spimdisasm for MIPS preparation, and the compiler
and exact image comparison for the primary feedback loop. A configured target
name does not prove the original compiler. Work on connected understanding, but
unconverted dependencies can remain original assembly in the PS1 build.

Original bytes, generated assembly, Ghidra projects and captures remain private.
Reviewed authored source goes under decomp/. Existing host C++ algorithms/tests
are references, not a compulsory rewrite destination. Record genuinely new source
knowledge concisely; do not require a capture or separate proof pipeline per helper.
