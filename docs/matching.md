# Matching-first source recovery

The loop is **recover source -> compile -> compare -> fix the first difference**.
Phase 1 covers every executable/overlay on both discs. The forest route is a
regression scenario, not the scope boundary. See ../plan.md for the phase exit.

## Tools and a working public check

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
m2c --help
spimdisasm --help
psx-objdump --version
```

This lean shell reuses the pinned m2c/spimdisasm packages and adds GNU MIPS
binutils, make and ordinary diff utilities. It does not install Ghidra merely to
compile/compare. Use the separate Ghidra default shell for program-wide inspection.
The stable psx-* commands wrap the pinned little-endian MIPS binutils. The smoke
check assembles an authored MIPS-I function and checks its exact linked bytes.
It establishes tooling, not Xenogears compiler identity or game-source progress.

## Establish the actual target

Qualify the compiler/assembler/linker on a representative resident/overlay sample.
Reuse existing Ghidra types and reviewed algorithms. Record exact flags, GP/small
-data settings, source identity, addresses and layout in each target's build file.
Do not select a PsyQ version from an unsupported loader default. Add maspsx or a
historical compiler through a pinned Nix recipe when that trial establishes the
need; do not install another large speculative toolchain upfront.

`decomp/Makefile` provides the small assemble/link/verify loop. A target supplies
`ORIGINAL`, `ORIGINAL_SHA256`, `IMAGE`, `OBJECTS`, `LINKER_SCRIPT` and its source
compilation rules in a make fragment. There is deliberately no invented game
configuration or default host C compiler. Until qualified targets exist, ordinary
`make -C decomp verify` fails with an actionable error rather than passing empty work.

```sh
make -C decomp CONFIG=targets/<target>/target.mk verify
python3 tools/matching.py ORIGINAL REBUILT --sha256 EXPECTED_ORIGINAL_SHA256
```

Original files and build output belong under ignored `.local/`; source/build
recipes belong under `decomp/`. Use the existing extraction/source profiles rather
than a new disc importer. Keep each overlay's identity with its addresses. Distinct
images sharing a load address must never collapse into one symbol space.

## Recover incrementally

Retain original assembly/data privately so unconverted callees can execute in the
original environment. Replace real functions with readable original-compatible C.
Use the upstream disassembler/decompiler/diff tools; consult machine instructions
for unresolved types, signedness, control flow and code-generation differences.
Build cohesive resident/overlay modules with small shared headers. No new giant
Program, parallel native rules model or per-helper ownership adapter is required.

The verifier first fingerprints the pristine input, rejects the same input/output
file, then checks every byte and length. Its output claims only binary agreement;
it does not infer source coverage. Keep the linker/build source map authoritative
for ranges still backed by original assembly, binary data or nonmatching source.
Measure source coverage and exact matching independently. Do not call a baseline
made entirely of original assembly a completed decompilation.

A normalized asm diff is a debugging aid, not final acceptance. Exact final image
comparison includes linked addresses and data/layout. Decoded overlay matching
and compressed-container reproduction are separate claims. Do not turn optical
filesystem/ECC reproduction into a prerequisite for recovering executable code.

## Evidence without paperwork

For a qualified exact match, retain source/build configuration and the comparison
result; a new elaborate finding and emulator capture are not additional mandatory
gates. Capture the original when it answers an unresolved semantic, format,
service or integration question. Do not rewrite expectations or hide uncertainty.

Existing original findings, captures and the host reconstruction remain valid only
within their recorded scope. They are especially useful for the later native port,
where pointer widths, arithmetic, memory layout and platform services change.
Use docs/executable-reconstruction.md only when working on that reference harness.
