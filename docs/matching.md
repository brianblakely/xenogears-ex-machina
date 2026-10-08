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

## Qualified configuration and targets

GCC 2.6.3 and 2.7.2 (`psx-cc1-2.6.3`, `psx-cc1-2.7.2`; decompals/old-gcc 0.17
PSX builds) with `-O2 -mcpu=3000 -msoft-float -fgnu-linker -mgas`, ASPSX 2.34
behaviour through maspsx, and GNU as/ld reproduce original code exactly. The
version is per translation unit: 2.7.2 can move the stack adjustment into the
epilogue's `jr $ra` delay slot (`lw ra; move v0,0; jr ra; addiu sp`), which 2.6.3
never does (`move v0,0; lw ra; addiu sp; jr ra; nop`, battle 800716d8); 2.7.2
does not always fill it, and 2.6.3 also differs in commutative operand order and
narrow loads (menu 80070808, 80071794). A filled slot proves 2.7.2; 2.6.3 is
established per unit by functions that only it reproduces. Targets set `CC_VERSION`; `CC_<file> := 2.7.2` overrides. ASPSX below 2.50 expands
positive `li` to `ori` as the original does (resident, movie library). Qualification: resident
`80028aac` (ring reset) matches only under 2.7.2 — 2.8.1 omits its empty
8-byte frame and reschedules the stores. The small-data threshold is a
property of each translation unit: most code is `-G0`, while units that address
`.sdata`/`.sbss` (around `_gp = 0x80059170`) through `$gp` need `-G8`; set
`GP_<file> := 8` in the target fragment. ASPSX loads and stores small data through
`$gp` but forms every address (`la`) with `lui`/`addiu`, also of the unit's own
small string constants (resident heap report), so such units expand `la` before GNU as.

Jump tables: GCC emits `.align 3` before each table in `.rdata`. The original
assembler honoured it relative to the unit's own rodata section, and the
original linker placed each unit's section at a 4-byte boundary, so all tables
of one unit share one phase mod 8. `tools/jump_table_phases.py` (after
`all-verify`) finds every original switch dispatch in the 25 distinct images:
45 odd-length tables followed by another table are padded with one zero word
that keeps the phase, 20 of them at 4 mod 8 (resident 800188f4 -> 8001892c
within 8002a68c; slot39 801c50fc -> 801c512c; battle 80070514 -> 8007053c);
the phase changes 34 times, never within one function; no odd-length table
abuts a same-phase table. Ignoring the directive or taking it as 4-byte would
leave no pads; absolute 8-byte alignment would leave no tables at 4 mod 8.
GNU as pads the same way, and splat's `SUBALIGN(4)` overrides the 8-byte
section alignment it records, so maspsx passes `.align` through unchanged:
what matters is that each object's rodata starts where the original unit's
did. A phase change between two tables marks a unit boundary in the target
yaml (menu, battle, slot39). The five overlays at 8006faf0 open with their
number (field 4, worldmap 5, battle 6, menu 7, movie 8) ahead of the first
unit's rodata. spimdisasm emits `.align 3` only before 8-aligned tables, so a
file whose rodata starts at 4 mod 8 cannot hold assembly tables of a
0-mod-8 unit.

A third compiler builds the later battle code: the Cygnus CDK build of GCC
2.7.2 (`psx-cc1-2.7.2-cdk`, old-gcc 0.17 `gcc-2.7.2-cdk`, cdk-gcc b18) at `-O2`
with a later ASPSX (positive `li` as `addiu`; maspsx `--aspsx-version=2.56`).
It keeps a symbol's `%hi` in a register and addresses members from it, leaves
load-delay `nop`s and the epilogue `jr` slot of ovl3381 `801fc000`/`801fc278`
unfilled, and fills other `jr` slots (debug2611 `802818c4`). Units: the six
0x801fc000 battle modules (ovl3381, ovl3383-ovl3387), debug2611's tools unit
(80280844-end) and ovl2615's battle_loader and load_modes. Qualification: of
cc1 2.5.7, 2.6.0, 2.6.3, 2.7.2, 2.7.2-cdk, 2.8.0, 2.8.1, 2.91.66 and 2.95.2
(`-O1`/`-O2`/`-O3`, `-fno-delayed-branch`, `-fno-schedule-insns[2]`) under
ASPSX 2.34-2.86, only 2.7.2-cdk `-O2`/`-O3` with ASPSX >= 2.56 reproduces
ovl3381 `801fc000` and `801fc278`; over the nine units' existing C, `-O2`
reproduces 21 functions that 2.6.3/2.7.2 do not (`-O3` 18, `-O1` 1), and the
units' previously matching C still matches. ASPSX 2.56-2.86 give identical
bytes here. Set it with `CC_VERSION`/`CC_<file> := 2.7.2-cdk`. SDK library code (PsyQ 3.x-4.x) is
located with `tools/psyq_signatures.py` and classified, not decompiled.

`CC1FLAGS_<file>` adds unit-specific cc1 options; no unit needs one yet.

Targets (`decomp/targets/`): both resident executables (SLUS_006.64/69 share all
source; only the embedded disc index differs) and 24 decoded overlay images,
byte-identical on both discs. `tools/extraction/disc_files.py` and
`tools/extraction/overlays.py` write the local inputs; splat writes the local
assembly. `tools/extraction/code_census.py` scans every file of both discs for
MIPS function structure and fails unless each code-bearing file is the boot
executable or byte-identical to a target image (MDEC streams are reported
apart). Distinct overlays at the same address keep separate targets and symbol
files.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
python3 tools/extraction/disc_files.py .local/discs/disc1.bin .local/extract/disc1  # and disc2
python3 tools/extraction/overlays.py
make -C decomp all-split all-verify all-coverage
make -C decomp CONFIG=targets/overlays/field.mk verify
python3 tools/matching_diff.py decomp/targets/overlays/field.mk [-f func_8007xxxx]
```

## Compressed containers

The six packed overlay files (mode overlays in slots 35-40 and the slot-39
image's second copy) are reproduced from the rebuilt images by
`tools/packed_container.py` (`make -C decomp all-container`). The packer is
Okumura's LZSS binary-tree encoder without preset-ring matches, ending on a
complete eight-token group; the same rule reproduces a 25-file sample of other
packed disc files. This is a separate claim from image matching; whole-disc
filesystem/ECC reproduction is not attempted.

## Original-environment smoke check

`tools/matching_ram.py` compares code loaded by an original scenario run with the
rebuilt images. The painting-room (Disc 1) and disc2-field-15 (Disc 2) routes load
resident and field code identical to the rebuilt SLUS_006.64/69 and field images;
the only resident differences are the harness's documented startup guard at
0x80019930 (EVID-REF-007) and PsyQ variables kept inside SDK text at
0x8004e960-0x8004e96b.

```sh
nix ... develop path:./nix#observation -c python3 tools/reference/scenario.py painting-room \
  --content 'discs/Xenogears disc 1.chd' --output .local/scenarios/<new>
python3 tools/matching_ram.py .local/scenarios/<new>/capture/final.ram --header 0x800 \
  .local/decomp/build/SLUS_006.64@80010000:80019524-8004e960 \
  .local/decomp/build/field.bin@8006faf0:8006fdec-800ada68
```

`--targets` compares every built target's linked code range with a capture and
lists those resident in it. Across the ten retained routes (forest/encounter/
menu on both discs, the two painting-room smokes, the movie and Mono/Stereo/Wide
routes) the resident images, field, slot39 and the ovl3384 battle module are
loaded exactly as rebuilt, apart from the guard and SDK variables above.

```sh
python3 tools/matching_ram.py .local/scenarios/<capture>/capture/final.ram --targets
```

## Converting a function

Replace one `INCLUDE_ASM(...)` in the target's C file with C. Start from m2c
(`m2c --target mipsel-gcc-c <asm file>`), existing findings and the host
reconstruction (search `src/reconstruction` for the address), then compile,
`make verify`, and read `matching_diff.py -f` for the first difference. Keep
functions in original order; the function's jump tables and strings move with
it (splat migrated them into the function's assembly). Shared structures go in
small headers beside the source. A function that is understood but does not yet
match stays linked as assembly inside `#ifdef NON_MATCHING ... #else
INCLUDE_ASM(...) #endif`; the coverage report counts it separately.

Register allocation and scheduling differences can be searched with the pinned
decomp-permuter: `python3 tools/permuter_import.py <config.mk> <func>` prepares
`.local/permuter/<func>` from the unit's exact compiler settings, then
`permuter -j8 --best-only .local/permuter/<func>`. Its candidates are hints, often
nonsense C; keep only a readable, semantically identical rewrite that `make verify`
accepts. Its parser misreads `sizeof(X) + y` (parenthesise the sizeof in base.c) and
fails on calls to undeclared functions (declare them K&R in base.c).

`python3 tools/nonmatching_score.py <config.mk> [func...]` compiles a target's
NON_MATCHING drafts into a side build and ranks them by remaining instruction
differences. It compares relocated instructions without their immediates, so a wrong
struct offset or symbol offset scores as equal; confirm near misses in the real image.

For actual differing-instruction counts, use `make -C decomp all-audit` (or
`make -C decomp CONFIG=targets/overlays/menu.mk audit` for one target).
Alongside source coverage, this runs `nonmatching_score.py <config.mk> --exact`:
it verifies the pristine baseline, compiles all reviewed C drafts with
`NON_MATCHING` in an isolated local build, and links them with the target's layout
and symbol rules. It compares complete four-byte words at corresponding function
offsets, including relocated immediates; extra or missing words count as differences.
An insertion can therefore make later words differ too. The JSON reports differing
functions/instructions and both original and candidate sizes. Assembly without a C
draft has no measured candidate difference. Compilation or linking failures fail
the audit; they are never counted as matches. This diagnostic does not replace
`all-verify` or measure data/layout differences outside the selected draft functions.

## What counts as recovered source

- An unused aggregate local (`RECT unused; /* unused in the original; reserves 8
  bytes */`) may reproduce a frame slot that the original code never reads or writes.
  GCC 2.x allocates unused aggregates; leftover locals are ordinary shipped code.
- Named `do { ... } while (0)` statement macros may wrap real statement groups (the
  original used them; they add a loop note that changes scheduling, allocation weight
  and block placement). An empty one is allowed only as a named, commented,
  compiled-out debug macro at a plausible place.
- Never-read locals, dead assignments or dead stores that exist only to steer CSE,
  scheduling or allocation are rejected even when they match (battle 80087EDC was
  withdrawn for this). No new inline asm, register pinning or `.word`; the existing
  GTE, `break` and scratchpad-stack macros are original style.
- Strings whose alignment padding holds stray assembler bytes stay original data:
  mark the symbol `force_not_migration:True`, link it with INCLUDE_RODATA beside the
  function and reference it as `extern char[]`. A .data object whose padding holds
  such bytes (a byte flag followed by `04`, a halfword table ending in `"Mt"`) is
  linked the same way with `INCLUDE_ORIGINAL(".data", NAME, VRAM, SIZE)` at its place
  among the unit's definitions, from the pristine input as INCLUDE_ASSET does; use it
  only where the padding is non-zero and nothing reads it. Both count as `included`.
- Uninitialized variables are defined uninitialized in their unit, never as zero
  data. GCC emits a unit's function-local statics, then its file-scope tentative
  definitions in first-declaration order; the original assembler gave each a slot of
  whole words (two `u8` four bytes apart, `BSS := slots` in the target, a filter on
  maspsx's output). A unit's own variables come first, as statics where the commons
  follow apart, and a unit reads only its own: that fixes text boundaries (menu
  800707A8, 8007E528, 80081ECC; slot39 801DBDB4). The commons, which the original
  linker allocated after every unit's own in an order of its own, are defined by a
  commons unit linked last (slot39_common.c, menu_common.c, mdec commons/). Where a file holds its
  .bss as zeros the .bss is loaded (`ld_bss_is_noload: False`); zeros a packer added
  past the program are file padding (`OBJCOPY_FLAGS --gap-fill 0 --pad-to`, field.mk).
- Media and bytecode embedded in a unit's data (packed images, fonts, sound banks)
  stay user-supplied: `INCLUDE_ASSET(".data", NAME, VRAM, SIZE)` links them in place
  from the target's pristine input (`ORIGINAL_IMAGE`, with `ORIGINAL_BASE` set in the
  .mk), and an `asset` line in the classification names the format and its reader.
  Never commit their bytes as C initializers; numeric program tables (sine, pitch,
  note encodings, opcode lengths) are source.
- K&R definitions, unprototyped calls and implicit-int returns are legitimate where
  the original passes unpromoted arguments or keeps `$v0` live.
- Unit compiler settings are qualified per unit; compiling every remaining draft under
  single-flag variants (`-fno-schedule-insns[2]`, `-fno-strength-reduce`, CSE and loop
  options, `-O1`) produced no match, so do not change an existing unit's flags to fix
  one function. A match under another compiler or flag that the neighbours do not
  survive is a hint, not proof of a unit boundary: slot39 801c93a8 matched both as its
  own GCC 2.6.0 `-fno-rerun-cse-after-loop` unit and, more simply, under the unit's 2.6.3
  with a zero that only combine can see (below); prefer the explanation that needs no
  new toolchain or split.

## Matching levers (GCC 2.6.3/2.7.2)

Most matches came from data shape, not statement shuffling:

- Declare globals as the real struct other units already use. A member at a nonzero
  offset (`area.slots[i]` = `%lo(area+4)`) orders and reuses addresses differently
  from a separate symbol or offset 0. Index flat tables exactly as the original does
  (`tbl[i*2+1]`), use bit-field views where it inserts/extracts bits, the operands'
  real signedness, and PsyQ macros (`setXYWH`, `setRECT`, `setUVWH`) instead of
  hand-written corner arithmetic.
- sched1 places a pseudo set exactly once (a "register birth") next to its use; a
  variable assigned twice (`p = base; p += off;`) keeps an earlier load early.
- Global allocation ranks pseudos by references (weighted by loop depth) over live
  length. Statements duplicated in each branch, later merged by cross-jumping, still
  add references; reusing one scratch variable for several roles or narrowing a block
  scope also changes the order. `$sN` permutations are usually this.
- CSE: if/else arms create a join label it cannot reuse values across; a `(u16)` view
  stops sharing of an identical expression; a local copy of a global pointer or a
  shift keeps `base + index*size` base-first.
- loop.c moves an invariant when threshold x savings x lifetime >= loop insn count
  (threshold 52, 26 with calls, minus 3 per moved insn on 2.6.3; inner-loop invariants
  double the outer loop's count). A larger original body (per-branch statements,
  statement macros) keeps invariants in the loop; identical address computations
  pair into reduced pointers where distinct ones stay indexed.
- jump.c copies loop exit blocks shorter than about 22-26 insns to the loop entry and
  cross-jumps identical tails; keep tails distinct where the original does.
- Script interpreters read a signed 16-bit word: bytes taken as `word >> 8` into a
  `u8` get `andi 0xff` at every later use because combine cannot prove the upper bits.
- Store order of independent statements is free to search (semantics unchanged).

Inspect decisions with cc1 RTL dumps (`-dL` loop, `-dS`/`-dR` scheduling, `-dl`/`-dg`
allocation) on the preprocessed unit; the comments of each NON_MATCHING draft record
what has been measured for it.

## When one instruction will not move

Lessons from the hardest drafts (GCC 2.6.x/2.7.x `cse.c`, `sched.c`, `reorg.c`):

- cse replaces a register source by a known constant whenever it can (a MIPS
  CONST_INT costs 0, a pseudo 1), so a surviving `move` of a register that was just
  zeroed means both cse passes lost the value: the zero came from an expression only
  combine reduces, such as a byte shifted right by 8 (slot39 801c93a8). A cse block ends at a referenced label;
  the first pass also ends at a loop-end note (any `do { } while (0)`), the second
  (`-frerun-cse-after-loop`, on at `-O2`) does not. cse keeps going past a label whose
  remaining uses it removed itself, and `-fcse-skip-blocks` extends a block over an
  `if` without inner labels, so a dead `if` hides nothing.
- When `x = y;` copies a register and both live on, cse makes the one that outlives the
  block and the other the canonical name (`make_regs_eqv`); a test of `y` right after the
  copy is rewritten to `x`, `y` dies there, and allocation merges them. A later use of `y`
  (reusing the variable after a loop, sound driver 80039144) keeps both registers and the
  copy, as do a hard-register variable for `x` (never made canonical) or a dead store,
  which the rules above reject.
- A dead loop that flow deletes leaves its exit label until the jump pass after reload:
  it still splits the scheduling blocks, and an assignment before it can be hoisted
  into the prologue.
- In 2.6.3 the insn after a loop note is a scheduling barrier (2.6.0's is not).
- reorg never moves an `asm` into a branch delay slot: an original copy in a delay slot
  was compiler-generated, not inline asm.
- Splitting a file into units drops the prototypes that earlier definitions supplied:
  calls across the new boundary with narrow (`u8`/`s16`) parameters lose their
  `andi`/sign extension. Declare those prototypes in the shared header.
- When a draft resists every source shape, compile it and its variants under the other
  old-gcc releases (2.5.7-2.95.2, `-psx` and plain) and single-flag variants before more
  shuffling; compare whole units, not one function, before adopting a setting.

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

The coverage audit reports functions, bytes and static MIPS instructions per class.
From the link map it also attributes every loaded .rodata/.data/.sdata input
section to compiled C, original bytes INCLUDE_RODATA'd or INCLUDE_ORIGINAL'd in C
(`included`),
authored assembly, a classified `sdk`/`asset` range, or a generated
`placeholder` (`remaining_data_placeholder_bytes`). `asset` marks user-supplied
game data or bytecode that is parsed and documented rather than rewritten as source.
GCC emits a static initializer's string literals last to first once the initializer
ends, so a pointer table whose strings lie in reverse address order was written with
its literals (resident message and name tables); under `-G8` strings of up to 8 bytes
go to `.sdata`. `remaining_asm_functions` and `remaining_asm_instructions` total the unrecovered
assembly and reviewed nonmatching C candidates; SDK and handwritten assembly stay
separate. Instructions are four-byte words within ELF function ranges, including
nops and delay slots, excluding data sections and padding outside those ranges.

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
