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

The link reads splat's `undefined_syms_auto.txt` and `undefined_funcs_auto.txt` as
`PROVIDE` (decomp/Makefile): an address splat gave a name defines it only where no
linked object defines it, so an object's own definition stands (GNU ld lets a plain
script assignment win silently and makes the name absolute; 63 names were so bound
before, the resident's D_800308D0 among them). The resident pads its file to the
link's `__exe_file_size` (`PAD_TO_SYMBOL`), the sectors its header declares.

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
Okumura's LZSS binary-tree encoder without preset-ring matches; it completes the
last eight-token group with zero literals, which count in the decoded length.
The plain encoding of the program plus those literals is every container's
stream, and some such tail reproduces each of the 24 other disc-1 files holding
one whole packed stream (`packed_container.py --sample .local/extract/disc1/files`);
splitting trailing copies into literals instead fails on files/0090.bin. A
decoded image therefore ends a few bytes past its program (worldmap 2, field 6,
movie 7, menu 5 bytes): they belong to no object and stay out of C. Where a
program ends in zeros the stream alone cannot place them (field's could be 4-6,
worldmap's 1-3); there the resident's mode table, which starts the overlay's BSS
at its program end, does. A target appends its tail after the link
(`PACKER_TAIL`), so the image matches only when the link ends at the program end.
This is a separate claim from image matching; whole-disc filesystem/ECC
reproduction is not attempted.

## Script instructions

Used script instructions are recovered from the matching interpreter, one machine
at a time. Read each opcode's size and flow from its handler (the returned step or
pointer advance of a dispatch-table entry or switch case), never from the data,
and keep the table in a `tools/analysis/` module whose entries name their
handlers, with synthetic tests that check it against the C (table order,
returns, case advances). Find scripts from the code that starts them (stores to
the script pointer, their pointer tables) and decode each disc's own files: a
`--sweep` prints per-opcode counts and every unknown opcode with its location,
and reports script-shaped data that nothing starts separately. Listings stay in
`.local/`; `docs/scripts/<vm>.md` summarises each machine. Address disc files as
the resident does: directory (g, i) file f is index slot f + table[g + i] - 2,
from the u16 directory table at sector 40 (`80028230`). Comment every handler
with its operands and effect as read from the callee, not from the call's shape
(world map opcode 10 slides an effect's volume rather than playing it). A match
proves a switch's case labels, not its comments: check handler comments against
the jump table (battle AI 70-74 were labelled 6e-72, because 6e and 6f share the
default).

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

A new file (a split or data-only unit, an overlay-number unit, a `.data.ld` alias
script, authored `.s`) also goes into packaging/source-files.txt, which lists every
tracked file. `source_archive.py --check` validates only the listed paths, so
compare the list with `git ls-files` after adding one.

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
  GCC also leaves never-accessed slots itself (field 8007E1C0: combine's `(use (reg))`
  of a folded sign-extension temporary), so rule that out first.
- Named `do { ... } while (0)` statement macros may wrap real statement groups (the
  original used them; they add a loop note that changes scheduling, allocation weight
  and block placement). An empty one is allowed only as a named, commented,
  compiled-out debug macro at a plausible place.
- Never-read locals, dead assignments or dead stores that exist only to steer CSE,
  scheduling or allocation are rejected even when they match (battle 80087EDC was
  withdrawn for this). No new inline asm or `.word`: the original-style macros the
  recovered C uses (the PsyQ GTE macros, the debugger break and pollhost, the stack
  switches, GET_RA, addPrimLen9) are listed by their exact templates in
  tools/matching_coverage.py (`ORIGINAL_ASM`), and the coverage report fails on any
  other asm statement in a compiled function. A form joins the list only on this
  section's evidence.
- A local register variable (`register s32 v asm("$14")`) is accepted only where the
  evidence says the original source bound it (project owner, 2026-10-08): a scratch
  pin of just those variables makes the function match exactly, no other compiler,
  flag or plain-C shape reproduces the registers (a replay of global allocation shows
  ordinary pseudos cannot reach them), and the surrounding code is register-level
  (world map 80086798: four projected corners in t6-t9, read with the exact
  three-`mfc2`-plus-`nop` shapes of LIBGTE.H's register-argument SXY macros).
  The function comment states that evidence. A pin that merely forces a register
  the plain C misses is not enough: field 80099AC0's `$a0`/`$a1` pair were variables
  set elsewhere in the function (`model`, `$a0` at the top, and the scratch `value`,
  `$a1` as the step count and the facing), which sched1 does not sink as births.
- Strings whose alignment padding holds stray assembler bytes stay original data:
  mark the symbol `force_not_migration:True` (with `size:` and a symbol after it where
  splat would join the strings that follow, menu6's `D_800705F0`), link it with
  INCLUDE_RODATA beside the function and reference it as `extern char[]`; never spell
  the stray bytes in a C initializer. A .data object whose padding holds
  such bytes (a byte flag followed by `04`, a halfword table ending in `"Mt"`) is
  linked the same way with `INCLUDE_ORIGINAL(".data", NAME, VRAM, SIZE)` at its place
  among the unit's definitions, from the pristine input as INCLUDE_ASSET does; use it
  only where the padding is non-zero and nothing reads it. Both count as `included`.
  The coverage report finds a string's stray byte itself; every other included object
  needs an `included` line in the target's classification with its reason (those data
  objects, and the resident's libcd/libgpu/libspu strings that several library
  functions share, which splat leaves to INCLUDE_RODATA). Any other string is C (below).
- A routine is classified handwritten (reviewed `.s` beside the C) only on code GCC
  does not emit: trapping `add`/`addi`/`sub`/`neg`, saves below `$sp` or beyond the
  frame, `ori` for a small positive constant where the unit's ASPSX emits `addiu`,
  `bne $zero, rt` operand order, dead delay-slot copies, or a register choice that a
  probe compile of the plain C does not make (resident 8003F8B0). Raw cop2 moves,
  absolute jumps and a missing frame are not evidence: PsyQ's GTE macros compile to
  them (worldmap 800987AC was plain C). In authored `.s` the GTE command macros emit
  `.word`, so GAS cannot fold label differences across them; a code patcher addresses
  its targets by literal offsets (resident 80030988).
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
  hand-written corner arithmetic. Conversely, words the original schedules as
  separate symbols are separate scalars (heap reset 80031A68: as members of one
  struct, its symbol-range clears could not move ahead of the block-header stores).
- Parameter types place copies: a `u16` parameter's conversion lands after the
  stack-argument load, while an `int` parameter narrowed at each use keeps the
  original order (glyph outline expander 80034FFC).
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
  pair into reduced pointers where distinct ones stay indexed. Identical constants
  across an interpreter loop's cases are hoisted as one group (then spilled); where
  the original loads 0xff at each use, one case's marker in a block-scope variable
  stops the hoist (ovl2143 801E39F0).
- jump.c copies loop exit blocks shorter than about 22-26 insns to the loop entry and
  cross-jumps identical tails; keep tails distinct where the original does. A table's
  first element loaded apart before a loop is usually that copied exit test: write
  the plain loop over the defined table, not a second symbol (worldmap 80072238).
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
- A repeated signed power-of-two division (`x / 16`, whose rounding is a `bgez`
  branch) can split a block without leaving an instruction. In slot39 801E78C8 each
  switch case writes `0x80 + file / 16 * 16` at every use: cse1 keeps the case's first
  division (its block starts after the loop's end note), cse2 merges it into the
  loop's quotient but nothing deletes dead code after cse2, so flow deletes the
  division and keeps its branch until jump2. Through allocation the case is then two
  blocks, and its record offset became a global pseudo (v1/a0/a2) as in the original.
  When the allocation pattern points to a missing block boundary and the draft caches
  a repeated expression in variables, try writing it inline at every use.
- In 2.6.3 the insn after a loop note is a scheduling barrier (2.6.0's is not).
- reorg never moves an `asm` into a branch delay slot: an original copy in a delay slot
  was compiler-generated, not inline asm.
- Splitting a file into units drops the prototypes that earlier definitions supplied:
  calls across the new boundary with narrow (`u8`/`s16`) parameters lose their
  `andi`/sign extension. Declare those prototypes in the shared header.
- When a draft resists every source shape, compile it and its variants under the other
  old-gcc releases (2.5.7-2.95.2, `-psx` and plain) and single-flag variants before more
  shuffling; compare whole units, not one function, before adopting a setting.
- To learn whether only allocation remains, bind the disputed variables to the
  original's registers (`register s32 v asm("$14")`) in a scratch build. Commit the
  binding only under the evidence rule above; world map 80086798's corners (48
  loop-weighted refs over ~195 insns) outrank every variable the original allocated
  before them, so as pseudos they always take t1-t3/t6.

## Recovering data

Data placeholders (`remaining_data_placeholder_bytes` in the coverage report) are
converted to C per unit. What converting the targets' `.data` established:

- A unit emits each section in definition order and the original linker joined them
  in unit order, so a block belongs to the unit whose section holds it, not to the
  units that read it (slot39.c holds tables only its later units use; battle
  80070E2C's `.data` opens with tables several units share). Define blocks in
  address order. Each unit has one `.data`, so an object that stays original data is
  linked at its place among the unit's definitions (INCLUDE_ORIGINAL, above) and its
  neighbours stay C. Data order is also unit-boundary evidence: each world map scene
  unit's data opens with that of mode handlers the text split still leaves in the
  preceding unit (8007DE98).
- The mode overlays' leading number is the first unit's `.rodata` (field.c,
  worldmap.c, menu.c), or a unit of its own where that unit's rodata starts at 4 mod 8
  right after it (battle_prefix.c, movie_number.c).
- Unreferenced objects are shipped data: define them at their offsets (movie's unread
  words, ovl3381's unused copy of the cell triangles). Two units whose data opens with
  the same table include it as a `static` from a shared header (field_music.h).
- splat names addresses the code forms from a base plus a constant (`D_8009A684`, four
  entries before the flame sizes; `D_801EA5D0`, 0x20 before the Shift JIS codes).
  Define the real object and index it as the code does (`D_8009A68C[index - 4]`,
  `D_801EA610[hi - 0x20]`): it compiles to the same address. Interior names that
  remaining assembly still uses go in `<target>.data.ld` (`D_x = D_y + off`; splat's
  `undefined_syms_auto.txt` covers only unaligned ones). These aliases are
  scaffolding, deleted when their last assembly user matches (slot39's and field's
  have gone). One of a static resolves only because maspsx makes `.lcomm` symbols
  global, where ASPSX kept them local.
- GCC emits a function's string literals into `.rodata` ahead of its code, in the order
  of first use, and its jump tables after it; an initializer's literals in reverse
  order once the definition ends (menu6's gear list and heap tag names). A unit emits
  identical literals once, so a second copy is an array (movie's second `"\n"`), and a
  table whose strings follow some function's literals is defined after that function,
  with the data defined around it in data order (menu3's ether name and combo names,
  menu4's level, command and menu line names).
- Several images end with zeroed `.bss` (slot39, menu, mdec, ovl2143, ovl2596,
  ovl2601, ovl2602, ovl2615). Uninitialized variables are defined uninitialized in
  their unit, never as zero data, and where a file holds its `.bss` as zeros the
  `.bss` is loaded (splat `ld_bss_is_noload: False`, one `.bss` subsegment per unit).
- GCC emits a unit's function-local statics, then its file-scope tentative
  definitions in the order of their first declaration (a header's `extern` counts),
  and maspsx allocates both in the unit's `.sbss`/`.bss`, packed without alignment
  (in `.sbss` it 8-aligns an 8-byte object). In the original images each object takes
  a slot of whole words, an 8-byte one also at 4 mod 8 (menu 8009265c, slot39's RECT
  801ea8e4), evidenced separately for ASPSX 2.34's `.lcomm` statics (mdec
  801e8958-801e8968: five u8, stored and loaded bytewise; menu3 80092678-800926a0;
  slot39 801ea710/801ea714) and for the commons PSYLINK allocated (ovl2596
  801e44e0/801e44e4; libcd's Stsector_offset alone at 801e89bc; the ASPSX 2.79 world
  map's four u16 at 8009bd10-8009bd1c, which three units share). The build gives each
  object whole words at a word boundary under the qualified ASPSX 2.34 and 2.79 (no
  2.79 unit allocates any in C yet) and rejects a smaller one under any other version
  (decomp/Makefile); no target or unit setting selects it. Variables that share a word
  are therefore one object, also in the resident's generated `.bss`: the sprite
  position D_800592E8 is a DVECTOR. The window colour D_800594D4, the overlays'
  `u8[3]`, is still a `u8` with extern +1/+2 bytes: ASPSX 2.34 addressed a common at
  an offset absolutely (maspsx models it), but GNU as moves a small common's offset
  accesses to `$gp`.
- A unit's own variables come first, in unit order, as statics where the commons
  follow apart, and a unit reads only its own: `tools/data_users.py CONFIG.mk`
  reports every FOREIGN reference, another unit's code forming an address in a unit's
  own `.bss` (none in any target), and, with `--end`, the order of variables still
  extern. That places menu 800707A8 and 8007E528 exactly, the menu4/menu5 boundary at
  80081E00, 80081E6C or 80081ECC, and slot39's after 801CD2AC and at or before
  801DBDB4 (an earlier one moves the `.bss` boundary with it); the latest is kept.
  The commons, which the original linker
  allocated after every unit's own in an order of its own (mdec's five player commons
  among the 20 of libcd's CDROM.OBJ), are defined by a commons unit linked last
  (slot39_common.c, menu_common.c, ovl2602_common.c, mdec commons/), which reproduces
  the linker's placement rather than modelling it. Zeros a packer added past the
  program are file padding (Compressed containers).
- The menu (GCC 2.7.2) splits its uninitialized variables by size. Those of up to
  eight bytes, each unit's own in unit order then the commons, fill 800925d4-80092954
  and end the program; the larger ones follow past it in the same order, each unit's
  own to 80096fa8, then the large commons up to the mode table's BSS end 8009b558
  (`tools/data_users.py decomp/targets/overlays/menu.mk --end 80096fa8`: no FOREIGN
  reference or INVERSION). The GCC 2.6.3 images keep one `.bss` in declaration order
  (slot39_801DBE54's 2-, 200-, 200- and 1-byte statics at 801ea72c-801ea8c0; mdec's
  64-byte movie_decoder among 4-byte statics), so the split is likely the original
  assembler's 8-byte small-data threshold applied to the `.lcomm`/`.comm` GCC emits
  after the code, with the linker's small commons. Until it is modelled the larger
  variables stay extern (`undefined_syms_auto.txt`): defined in their units they
  would be allocated among the small ones.
- GCC writes a `-G8` unit's data, commons and `.extern`s ahead of its code, also a
  definition placed after its use: `extern int late_var; int g(void) { return
  late_var; } int late_var = 2;` through `psx-cc1-<version> -O2 -G8` puts `late_var:`
  before `g:` under 2.6.3, 2.7.2 and 2.7.2-cdk, and every GP 8 unit's `.o.cc1.s` in
  the build directory shows the same order. A one-pass ASPSX has therefore seen a
  unit's own definitions before any use: small data that every user loads and stores
  absolutely is no user's own (addresses, `la`, are formed absolutely even of a unit's
  own). Only the link position constrains its owner, so the resident's
  (80059170-80059184, 80059198-800591b8) is defined by data-only `-G8` units there,
  the simplest owners that fit (kernel_settings.c, sprite_settings.c; their compiler
  is immaterial: the three give identical `.sdata` and relocations). Before calling
  shared data unreferenced, check the code of every image (`tools/data_users.py
  --range START:END`) and its data words, which that scan does not read (pointer
  tables; no image holds a word in 80059170-800591b8): four of the resident's fillers
  are battle and ovl2596 flags, and the battle-entry flag at 80059179 sat in what was
  taken for padding.
- The resident clears each mode overlay's `.bss` from the address its mode table
  records with a pre-increment loop, so the first object sits 4 bytes later (movie:
  80076f38, counters at 80076f3c; field's RECT ring).
- Embedded game data stays generated and is classified `asset` with its format
  (menu7's SpriteModel D_80091FB0); library data is classified `sdk` by the code that
  reads it. splat migrates rodata used only by an INCLUDE_ASM function into that
  function's `.s` file, and coverage counts it with the function (the resident's
  library strings and jump tables as `sdk`) until the function is C and the compiler
  emits it; mdec's libpress/libcd messages are a generated rodata segment classified
  `sdk`.
  Name data only by what its readers show (the libcd commons).

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
Every byte of every .text input section counts once: each function in its class, and
the bytes outside every function (padding and data words of an INCLUDE_ASM'd or
INCLUDE_RODATA'd file, data a unit places in .text such as menu6's D_80088BFC) as bytes,
not instructions, of their owner's class, attributed like data below: an INCLUDE_ASM'd
file's under its function's class, an INCLUDE_RODATA'd file's `included`, cc1's `c`, a
generated assembly unit's `asm`, an authored one's `handwritten`, a classified range
first. The report fails unless every function lies inside its input section and
overlaps no other, so `text_bytes` is exactly the sum of the .text input sections; it
also fails on an input section other than .text and the data sections.
From the link map it also attributes every loaded data byte: each .rodata/.data/.sdata
input section and, where an image holds its uninitialized variables as zeros, each
.bss/.sbss input section of a loaded output section (NOLOAD .bss is not in the image;
alignment gaps and a packer's tail belong to no input section). A function is `c` only
where its unit's cc1 output emitted it (`.ent`); one an INCLUDE_ASM'd or
INCLUDE_RODATA'd file defines is `sdk` or `handwritten` by range, `nonmatching` as the
fallback of a NON_MATCHING candidate (the only source scan), else `asm`. A data byte is
compiled C (`c`, or `bss` for C-defined loaded .bss), original bytes INCLUDE_RODATA'd or
INCLUDE_ORIGINAL'd in C (`included`, also the resident's libcd/libgpu/libspu strings
that several library functions share), the other bytes of an INCLUDE_ASM'd `.s` file
(under its function's class: `sdk`, `handwritten`, or `nonmatching`/`asm`, totalled in
`remaining_data_asm_bytes`), authored assembly, a classified `sdk`/`asset` range, or a
generated `placeholder` (`remaining_data_placeholder_bytes`, loaded .bss included).
`asset` marks user-supplied game data or bytecode that is parsed and documented rather
than rewritten as source. Each included object (a statement's bytes in one section,
from its label) that no classified range covers needs its reason, or the report fails:
either it is one text string whose terminator is followed by 1-3 bytes of alignment
padding, one of them non-zero, or a `START END included NAME REASON` line of the
target's classification names its range and label; a line that names no included
object fails too. Handwritten bytes count only inside a `handwritten` range, an
authored `.s` unit's too, so the classification stays the record of every
handwritten routine. `make coverage` passes these files as `CLASSIFICATION`, and
`matching_coverage.py ... --list CLASS` prints a class's functions, its .text bytes
outside every function and its data ranges with their section and object (`placeholder`,
`included`, `asset`, `bss`, ...).

The report attributes a C unit's bytes by where GAS put them, not by how the source
spells them. `make coverage` compiles each C unit again (`<unit>.cov.o` beside the
object) with a label line at both ends of the text of every asm statement and
assembles it with each label recording its position in every section and the number
of GAS macro expansions so far. The report fails unless, without the labels, that
build's cc1 output, GAS input and sections are exactly the object's and the positions
tile every section. A statement's bytes in any section are its own, whatever its text
includes, incbins, expands or redefines: a nested include, a redefined macro, a `;`
separator, alignment fill or a later `.size` in an included file cannot move bytes into
C. Every other byte came from cc1's own lines, which may expand no GAS macro. Each
statement must be one of include_asm.h's, read as cc1 emitted it (so token pasting,
backslash-newlines, wrapper macros and other include files change nothing):
INCLUDE_ASM, INCLUDE_RODATA, INCLUDE_ASSET/INCLUDE_ORIGINAL outside .text, or the
macro.inc include, which emits nothing. Any other asm statement must be written inside
a compiled function and be one of the original-style macros the recovered C uses
(`ORIGINAL_ASM`, What counts as recovered source): its text in the cc1 output is
exactly the macro's template with a register for each operand, it expands no GAS
macro, and its code, inside that function, counts with the function. Every other asm
statement fails the report, as do a function that cc1 did not emit and no included
file defines, a compiled function holding included bytes and original bytes in .text.
The marking also fails the coverage build on each string cc1 copies into its output as
it is unless it is a plain name (a declaration's asm name, such as a register
variable's `"$14"` or a symbol's assembler name; a section or alias attribute; a line
marker's file name), and on any other preprocessor line: an asm name
`"D_1\n\t.word 0x24020001\n\t#"` would otherwise put the word into each function that
reads the symbol, among cc1's own lines.

GCC emits a static initializer's string literals last to first once the initializer
ends, so a pointer table whose strings lie in reverse address order was written with
its literals (resident message and name tables); under `-G8` strings of up to 8 bytes
go to `.sdata`. `remaining_asm_functions`, `remaining_asm_bytes` and
`remaining_asm_instructions` total the unrecovered assembly and reviewed nonmatching C
candidates (the bytes with their files' bytes outside every function); SDK and
handwritten assembly stay separate. Instructions are four-byte words within ELF
function ranges, including nops and delay slots, excluding data sections and the .text
bytes outside every function, which count only as bytes.

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
