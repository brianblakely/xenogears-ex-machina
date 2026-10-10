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
PSX builds) with `-O2 -mcpu=3000 -msoft-float -fgnu-linker -mgas`, ASPSX
behaviour through maspsx (7686f845) and GNU binutils 2.46 as/ld (the nixpkgs pin
of nix/ghidra) reproduce original code exactly. The
version is per translation unit: 2.7.2 can move the stack adjustment into the
epilogue's `jr $ra` delay slot (`lw ra; move v0,0; jr ra; addiu sp`), which 2.6.3
never does (`move v0,0; lw ra; addiu sp; jr ra; nop`, battle 800716d8); 2.7.2
does not always fill it, and 2.6.3 also differs in commutative operand order and
narrow loads (menu 80070808, 80071794). A filled slot proves 2.7.2; otherwise a
code unit's version is the one that alone reproduces some of its functions
(resident main 8001a344 and text_windows_and_pads 80032f54 only under 2.7.2; the ring reset
80028aac builds the same under 2.6.3 and 2.7.2, though not under 2.8.1, which
omits its empty 8-byte frame). Rebuilt under each pinned cc1, 79 of the 106 C
units match only under their own. Three small code units build identical objects
under 2.6.3 and 2.7.2 and follow a neighbour (resident battle_mode and
heap_host_report, debug2611 pages; their .mk comments say which). The 24 data-only
or INCLUDE_ASM-only units (overlay numbers, commons, the resident header, settings
and SDK units) are identical under all three, so their setting is immaterial.
Targets set `CC_VERSION`; `CC_<file> := 2.7.2` overrides. ASPSX below 2.50 expands
positive `li` to `ori` as the original does (resident, movie library, most units:
maspsx `--aspsx-version=2.34`, the default). The world map and the 2.7.2-cdk units
below expand it to `addiu`, which shows only ASPSX >= 2.50; they are set as 2.79
(world map, resident sprite units) or 2.56 (the others), and maspsx's output is
identical for every one of these 35 code units under either setting. Only the
build's slot rule for uninitialized variables (Recovering data) tells the settings
apart: battle's CDK statics need the size-aligned slots of 2.56, and the world map's
commons unit the whole-word slots of its 2.79 (under 2.56 its link fails the mode
table's BSS bounds). The small-data threshold is a property of each translation
unit: most code is `-G0`, while units that address `.sdata`/`.sbss`
(around `_gp = 0x80059170`) through `$gp` need `-G8`; set `GP_<file> := 8` in the
target fragment. The assembler of some `-G0` units still put their variables of up
to 8 bytes in `.sbss` (`SBSS_<file>`, Recovering data). ASPSX loads and stores small
data through `$gp` but forms every address (`la`) with `lui`/`addiu`, also of the
unit's own small string constants (resident heap report), so such units expand `la`
before GNU as.

The ABI, the same under all three cc1, is GCC's o32 convention for little-endian
MIPS I (R3000) with soft float: the first four argument words in `$a0`-`$a3`, the
rest on the stack after 16 bytes the caller reserves for those four, results in
`$v0`, every structure returned through a hidden pointer
(`-fpcc-struct-return`, the default), unsigned plain `char` (`lbu`), 16-bit
`short`, 32-bit `int`, `long` and pointers, 8-byte `long long` and `double`, both
8-aligned, and `$gp` = 0x80059170 for `-G8` units.

Vendor controls (local, `.local/original-toolchain-evidence`; the binaries stay out of
the repository): PsyQ CC1PSX 2.6.3.SN.2 gives text and relocations identical to old-gcc
2.6.3 for saved whole units of movie, battle_scene, slot39 and sound; CC1PSX
2.7.2.SN32.3.7.0002 does the same against old-gcc 2.7.2 for the resident heap,
field_motion, arena_stage_views_and_hud, worldmap_objects_effects_party and
worldmap_steering_camera_terrain; Psy-Q ASPSX 2.34 assembles movie 800737ec, battle
800a7064 and sound 8003b424 to the text that maspsx `--aspsx-version=2.34` and GNU as
give.

Jump tables: GCC emits `.align 3` before each table in `.rdata`. The original
assembler honoured it relative to the unit's own rodata section, and the
original linker placed each unit's section at a 4-byte boundary, so all tables
of one unit share one phase mod 8. `tools/jump_table_phases.py` (after
`all-verify`) finds every original switch dispatch in the 25 distinct images:
45 odd-length tables followed by another table are padded with one zero word
that keeps the phase, 18 of them at 4 mod 8 (resident 800188f4 -> 8001892c
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

A third compiler builds the later battle code and the resident sprite units: the
Cygnus CDK build of GCC 2.7.2 (`psx-cc1-2.7.2-cdk`, old-gcc 0.17 `gcc-2.7.2-cdk`,
cdk-gcc b18, banner `cygnus-2.7.2-970404 SN32.3.7.0004 (SonyPSX)`) at `-O2` with a
later ASPSX (positive `li` as `addiu`).
It keeps a symbol's `%hi` in a register and addresses members from it, leaves
load-delay `nop`s and the epilogue `jr` slot of ovl3381 `801fc000`/`801fc278`
unfilled, and fills other `jr` slots (debug2611 `802818c4`). Its 22 units: the
0x801fc000 battle modules (ovl3381, ovl3383-ovl3387), battle 800b15d8-end (seven
units), ovl2615's battle_loader, load_modes and burst_modes, ovl3087's
script_actor, debug2611's tools unit (80280844-end) and the four resident sprite
units (8001c8dc-8002709c). Qualification: of
cc1 2.5.7, 2.6.0, 2.6.3, 2.7.2, 2.7.2-cdk, 2.8.0, 2.8.1, 2.91.66 and 2.95.2
(`-O1`/`-O2`/`-O3`, `-fno-delayed-branch`, `-fno-schedule-insns[2]`) under
ASPSX 2.34-2.86, only 2.7.2-cdk `-O2`/`-O3` with ASPSX >= 2.56 reproduces
ovl3381 `801fc000` and `801fc278`. Across the 22 units, 293 functions build only
under 2.7.2-cdk, not under 2.6.3 or 2.7.2 (compared with relocated fields masked);
245 of them also build at `-O3`, 21 at `-O1`. ASPSX 2.56-2.86 give identical
bytes here. Set it with `CC_VERSION`/`CC_<file> := 2.7.2-cdk`. SDK library code (PsyQ 3.x-4.x) is
located with `tools/psyq_signatures.py` and classified, not decompiled.

`CC1FLAGS_<file>` adds unit-specific cc1 options; no unit needs one yet.

Targets (`decomp/targets/`): both resident executables (SLUS_006.64/69 share all
source; only the embedded disc index differs) and 24 decoded overlay images,
byte-identical on both discs. `tools/extraction/disc_files.py` and
`tools/extraction/overlays.py` write the local inputs; splat writes the local
assembly. `tools/extraction/code_census.py` checks that the targets hold all the
code on both discs. It counts every aligned `jr $ra`, leaf returns too, in each
file's raw bytes and in every packed block the original decoder completes from
any byte offset, checks that each movie stream's sectors are video, XA audio or
empty and that no sector outside the files holds code, and fails unless every
form holding code is the image of a `decomp/targets` .mk and all 26 occur. On the
user's discs it decodes 74,925 (Disc 1) and 42,383 (Disc 2) packed blocks and
finds code only in the targets (116 file entries and the boot programs); the
other `jr $ra` words are compressed bytes inside packed blocks (the world map
containers, Disc 1 file 732) and samples of the `wds ` wave bank in Disc 1 file
3039 / Disc 2 file 3034. Distinct overlays at the same address keep separate
targets and symbol files.

The link reads splat's `undefined_syms_auto.txt` and `undefined_funcs_auto.txt` as
`PROVIDE` (decomp/Makefile): an address splat gave a name defines it only where no
linked object defines it, so an object's own definition stands (GNU ld lets a plain
script assignment win silently and makes the name absolute; 63 names were so bound
before, the resident's model_envmap_patch_base among them). The resident pads its file to the
link's `__exe_file_size` (`PAD_TO_SYMBOL`), the sectors its header declares.

`verify` then fails on each name a linker script defines (a used `PROVIDE`, a `.data.ld`
or any other `LINKER_EXTRA` fragment) inside the target's own image or uninitialized
data (`matching_coverage.py --script-symbols`): an address copied from the original
where the link should place an object, which the exact comparison cannot see move. It
reads each assignment wherever it stands, also a second one on a line and one inside
`HIDDEN()`, `PROVIDE()` or `PROVIDE_HIDDEN()`, and fails on any fragment statement that is
neither such an assignment nor an `ASSERT`, so no name the link reads escapes it. Names
of other images (the `*.resident.ld` fragments, an overlay's addresses in the resident,
also the `_gp` an overlay's splat script sets) lie outside and pass; splat's main script
is not among the checked scripts, so the resident's `_gp` comes from its checked
`link.ld`. `BSS_END` in a target gives the end of its uninitialized data past the image,
the bound its loader clears (the resident's entry point, the mode table entries): the
check covers the data up to it, a link placing any past it fails, and the coverage
report counts what no linked object holds there. `LINK_VIEWS` names a script whose names
there are views, expressions of linked symbols, with the reason beside it in the target;
a number such a script assigns there still fails, and so does a views script that
defines no view. Four scripts are allowed: battle's (`battle.data.ld`, 42 names: parts
of its commons that the units, CDK ones too, address by names of their own, and the
timer reload and combo step tables from before them), the resident's (`link.ld`, 8: the
window colour's green and blue bytes, the CD mix bytes and a base for the name slots'
second bytes, which compile differently as members, the BSS's last word
boot_bss_last_word from the link's BSS end, for the entry point and the mode table, and
`_gp` from the start of the small data, where the original's 80059170 lies), the menu's
(`menu.bss.ld`: the opponent's command byte arena_settings_command, which
arena_brain_run_practice_command loads absolutely at each of its three reads) and the
world map's (`worldmap.data.ld`: worldmap_read_list_first_destination, the read list's
first destination, from which two loaders pass the list). `SCRIPT_SYMBOLS=strict` is the
default; `SCRIPT_SYMBOLS=warn` (on the command line or in the environment) reports the
names and passes a single target's `verify`, for a probe, which then prints
`NOT ACCEPTANCE: SCRIPT_SYMBOLS=warn`. `all-verify` gives every target
`SCRIPT_SYMBOLS=strict` on its command line, which neither the environment nor
all-verify's own command line overrides.

`verify` also fails on each address of the target's own image or uninitialized data
that the link did not produce (`matching_coverage.py --relocations`): an aligned word
holding one without an `R_MIPS_32` relocation in its input section, in a loaded data
section or in .text outside every function; in .text, a `lui` whose immediate is the
`%hi` of one without `R_MIPS_HI16`, and any `j`/`jal` without `R_MIPS_26`. Bytes
classified asset or included, original data the build copies, are exempt by class;
any other, the authored (handwritten) assembly's too, only by a
`START END unrelocated REASON` line of the target's classification, which may lie in
a class's range and must cover a reported word. No target needs one. Each resident's
21 such words lie in its asset ranges (its disc file index, the packed boot logo and
console font), and the mode table's overlay entries and BSS bounds (main.c
mode_table) are other images' addresses, past the resident's BSS end 8006faf0; its
own entries there carry relocations. The one such address in the 26 links was mdec's:
splat had emitted DecDCTvlcSize2 (801d5030, VLC_C.OBJ) as raw words after its
object's leading decode-call limit, so its `lui`/`addiu` of that word carried none.
The PsyQ signatures place each VLC object's `text_0` word before
DecDCTvlcSize/DecDCTvlcSize2 at +4; mdec.symbols.txt makes the two words data labels
(libpress_vlc_max_size, libpress_vlc2_max_size, counted `sdk` bytes outside every function) and
DecDCTvlcSize2 a function, relocated against libpress_vlc2_max_size.

After every target links, `all-verify` runs `make -C decomp cross-image`
(`tools/cross_image.py` over every configuration). Each name a target's linker scripts
assign (the `PROVIDE`s its link used, ovl2602's names in ovl2143 among them, and the
fragments: `*.resident.ld`, `debug595.field.ld`) whose value lies in another target's
image or uninitialized data is an address copied from the original. A name that gives an
address (splat's `D_`, `func_` and `jtbl_` names; the links hold none now) must hold
that address, and each must agree with the rebuilt targets by their own symbols: every
other target that defines the name defines it there, each that exports it wherever the
copied value points and one holding the value also by a local symbol (2325 of 2384 at
present); where none does, a fragment may give it as a view, another name plus a
constant that agrees by name, and the value must lie in the object holding that name in
its definer (50: the members of the game data, the field view and work block, the
resident's arena option bytes and battle's objects that debug595, the world map, the
menu, ovl2596, ovl2615 and battle address by names of their own, such as
`game_data_party_state = game_data + 0x1D30`); otherwise a target holding the value has
a symbol there (2: each resident's mode_overlay_area, which link.ld assigns where each
mode overlay's first unit defines its number; the movie library's entries and variable
that field and movie use, the battle functions ovl3087 passes and the resident's VSync
callback and sequence buffer that the world map uses all take their definers' names,
which the importing symbol files give); otherwise the address must lie inside an input
section that target's link places (7: bases that battle, ovl2596 and ovl2615 index
another image's object from, all from splat's lists). For this last group the check ties
a value only to the address its name gives, not to a particular object or, where
targets overlap, to a particular target. Values outside every target (the resident's
sizes, a constant) are not checked, nor is an address the C spells as a number, which
neither this check nor the relocation scan (a target's own range only) sees.
`python3 tools/cross_image.py decomp/targets/*/*.mk --numbers` lists those (each other
target's address a link holds without a relocation, outside asset and included bytes and
the mode table); in the 26 links they are battle's three reads of the boot word
mode_disc_mode (80010000; battle_sprite_commands.c battle_sprite_command_run sprite
commands 0x44/0x45, battle_settle.c battle_install_command_file_parts), a number in the
original too: its CDK units load it with one register (`lui v1,0x8001; lw v1,0(v1)` at
800b44b8), while by name they compile
`lui v0,%hi(mode_disc_mode); lw v1,%lo(mode_disc_mode)(v0)` and the battle link fails
its BSS bounds; and the load addresses of overlays and the heap's end (resident
main.c:91, mode_battle_and_menu.c:190/444/452; battle_turns_and_hud.c:324/387/400/501,
battle_frame.c:766; field.c:2906), which no check ties to the images loaded there. In
data the only such words are the mode table's, compared below, and five in each
resident's packed boot logo and console font, asset bytes that merely look like
addresses. It also compares each resident's mode table (`MODE_TABLE`) with the mode
overlays (`MODE`, `MODE_ENTRY` in field 1, world map 3, menu 4 and movie 6; battle's
mode 2 enters resident code and declares only `MODE`): the entry must be the overlay's
entry symbol, and the words after bss_start through bss_end, which the dispatcher clears
(boot_clear_bss_range), must be the overlay's linked .sbss/.bss. The check found one
resident byte that other images use outside every object: the arena bout's outcome at
80050622, which the menu writes and a field event reads, lay in the alignment fill after
mode_arena_task_parameters[6]; console_and_sound_driver.c now defines it
(mode_arena_bout_outcome), whether apart or as part of mode_arena_task_parameters left
open.

```sh
# the user's CHD images to raw MODE2/2352 tracks (and likewise disc 2); chdman
# (0.289) does not create the output directory: into a missing one it fails with
# "Unable to open file ... No such file or directory"
mkdir -p .local/discs
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#analysis -c \
  chdman extractcd -i 'discs/Xenogears disc 1.chd' -o .local/discs/disc1.cue -ob .local/discs/disc1.bin
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
python3 tools/extraction/disc_files.py .local/discs/disc1.bin .local/extract/disc1  # and disc2
python3 tools/extraction/overlays.py
make -C decomp all-split all-verify all-coverage
make -C decomp CONFIG=targets/overlays/field.mk verify
python3 tools/matching_diff.py decomp/targets/overlays/field.mk [-f func_8007xxxx]
```

## Compressed containers

The seven packed overlay files of each disc (14 in all: the mode overlays in slots
35-40 and the slot-39 image's second copy) are reproduced from the rebuilt images by
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
loaded exactly as rebuilt, apart from the guard and SDK variables above. Their
155 RAM snapshots add the battle overlay, ovl2596 and ovl2615, exact in the
forest/encounter routes, and mdec's first 6,760 bytes (801d30c4-801d4b2c, the
rest overwritten) in the movie routes. The other 17 targets are resident in no
retained capture; the image comparison and the census stand for them.

```sh
python3 tools/matching_ram.py .local/scenarios/<capture>/capture/final.ram --targets
```

## Converting a function

Replace one `INCLUDE_ASM(...)` in the target's C file with C. Start from m2c (`m2c
--target mipsel-gcc-c <asm file>`), existing findings and the host reconstruction
(search `src/reconstruction` for the address), then compile, `make verify`, and read
`matching_diff.py -f` for the first difference. Keep functions in original order; the
function's jump tables and strings move with it (splat migrated them into the function's
assembly). A unit's structures go in small headers beside the source; what several
targets share has one definition in `decomp/include`: `psyq/` (the SDK's types,
prototypes and macros, the inline GTE ones in `inline_c.h`; members no library signature
names under the PsyQ name their callers give them, else their library's prefix,
`libgte_rotate_vector`), `resident/` (one header per resident subsystem with its types,
variables and calls; `gamedata.h` holds the game data game_data), `battle/` (one header
per battle overlay subsystem whose types, variables or calls its modules and overlays
use, with the battle area battle_area and its work area battle_work_area; each function
sits in the header of the subsystem that defines it; also the screen burst that ovl2615
and ovl3387 both carry; `model.h` and `effect.h` hold the records of the battle's model
and effect library, which ovl2143 links a copy of, and declare no variables), `menu/`
(the blocks the menu mode's screens, slot39 and ovl2598-ovl2602, keep behind the menu
state of `resident/menu.h`), `mdec/` (the movie library's player, which the movie mode
and the field call), `field/` (`monitor.h`: the field state and calls that its debug
monitor, debug595, also uses, and the monitor's entries and debug-lines flag, which the
field calls and sets) and `ovl2143/` (the actor module's records, variables and entries,
which the field, the world map and the Gear parts shop use). Another image's functions
and variables keep their definer's names, which the importing overlay's symbol file
gives its link (movie and field name the library's entries, ovl3087 the battle's
callbacks it passes, the world map the resident's VSync callback). A resident, battle or
ovl2143 function whose callers in other targets were built with other argument or result
conversions (narrow parameters, another count) stays out of them: each target declares
it, the resident and the battle in their `own_declarations.h` (ovl2143's draw 801E7D14,
which ovl2602 calls with four arguments, in each caller). So does a variable some target
declares with another qualifier (the vertical blank count, volatile in the mode 4 menu),
and a target keeps its own view of an object whose members its code reads with other
types (ovl2615 reads the scene data's positions unsigned); a second declaration of one
object takes its assembler name (the world map's sequence header over mode_music_buffer,
whose address cse would otherwise keep from the copy before it). A shared header's
`extern` sets the order of the defining unit's tentative definitions, so that unit
defines them ahead of the header (gear_model_scene.c's state before `ovl2143/actors.h`,
as the commons units do). A unit declares what only it uses itself, before the first
use. A function that is understood but does not yet match stays linked as assembly
inside `#ifdef NON_MATCHING ... #else INCLUDE_ASM(...) #endif`; the coverage report
counts it separately.

Callers pass the types of the shared prototypes. Those in `psyq/` follow PsyQ 4.6's
headers (`.local/original-sdk-evidence/headers/Psy-Q_46.zip`, the only release at
hand; the game's own is not identified) for 184 of the 202 functions both declare,
compared by cc1 `-aux-info` (and, for the 15 libgte, libsn and libspu members that
took their PsyQ names with the renaming, against LIBGTE.H, LIBSN.H and LIBSPU.H by
reading). The other 18 differ in parameter types (memmove and memchr take `void *`
where MEMORY.H has `unsigned char *` and memchr's byte is an `int`, SpuReadDecodedData
takes a `void *`, DrawSyncCallback and VSyncCallback a `void (*)()`), in result types
(CdDataCallback, DrawSyncCallback, EnterCriticalSection, InitCARD, InitPAD, Square0,
StartPAD, VectorNormalSS) or in having a prototype (strlen, strcpy, InitGeom,
PushMatrix, PopMatrix, ReadGeomScreen). Every unit compiles to the same code under
4.6's declarations (Square0's `VECTOR *` result as well: none of the eleven units
calling it uses it), which would warn at 23 memmove calls and both SpuReadDecodedData
calls. Data the SDK names has its type where the code handles it only in that form:
VRAM words saved and reloaded through StoreImage and LoadImage `u_long` (battle's CLUT
strips, the field's saved VRAM areas and the pixel buffers it edits as words), the
GTE's depth, flag and screen outputs `long`, a `CdlLOC`, a `CdlCB`. Data the code
holds in another type keeps it, and the call casts as the SDK's samples do:
`(long *)&poly->x0`, `(u_char *)` for bzero, and `(u_long *)` for image data in bytes
or halfwords, for TIMs inside larger buffers and for words that `u32` views hold,
among them the ordering tables of the battle's and the menu's shared views and drawing
code and the field's packed screen words.

`psyq/libc.h` declares memcpy and memset unprototyped, as MEMORY.H does "to avoid
conflicting" with GCC's built-ins, which they keep: field 800AB808's copy needs the
built-in memcpy (with its computed length the built-in still calls memcpy).
menu_member_screens's two 0xa38-byte save copies call memcpy in the original; the
built-in would move them inline, their length being constant. That unit declares a
memcpy prototype, which drops the built-in there (cc1 warns of the conflict);
passing the length in a variable instead keeps the built-in and gives the same code
without the warning, and nothing in the bytes decides between the two. No other
unit's code depends on how libc.h declares them. To count the warnings, rebuild
every unit and read the log:
`make -B -j8 -O -C decomp all-verify > .local/build.log 2>&1`, then
`python3 tools/compiler_warnings.py .local/build.log` (`--list` prints each one).

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

## Names

splat's placeholders (`func_`/`D_`/`jtbl_` names, units and `.s` files named by an
address or by their image number, parameters named `argN` or `aN`) have given way to
names from which a reader, often an agent, knows what a symbol is and which module
owns it, and finds it with one grep:

- Game code and data are lower snake_case. PsyQ functions and library variables
  keep the SDK's spelling (`LoadImage`, `CdRead`, `longjmp`, `_card_info`); an SDK
  member gets its PsyQ name only where its comment or decomp/include/psyq says the
  code is that routine, else its library as prefix and what the code does
  (`libgte_rotate_vector`, `libcd_timeout_message`).
- Every game name starts with its owner's prefix: one per overlay image, one per
  resident subsystem. The resident's subsystems are those of decomp/include/resident
  (`cd_`, `console_`, `gpu_`, `heap_`, `menu_state_`, `mode_`, `model_`, `pad_`,
  `sound_`, `sprite_`, `stream_`, `text_`, `window_`, `game_` for GameData,
  `formation_`, `task_`), with `boot_` for the entry point and boot, `commons_` for
  the commons units' own, `psyq_` for the PsyQ unit psyq_libgte_to_libcard.c and
  `rcossin_tbl`, libgte's table. The overlays: field
  `field_` and its debug monitor debug595 `field_debug_`, battle `battle_` and its
  debug tools debug2611 `battle_debug_`, the world map `worldmap_`, the menu target
  (mode 4, the Battling arena) `arena_`, slot39 (the in-game menu) `menu_`, movie
  `movie_mode_`, the mdec movie library `movie_`, ovl2143 `gear_model_`, ovl2596
  `battle_results_`, ovl2598 `member_change_`, ovl2600 `name_entry_`, ovl2601
  `item_shop_`, ovl2602 `gear_shop_`, ovl2606 `battle_scene_select_`, ovl2615
  `battle_setup_`, ovl3087 `battle_event_script_` and the 0x801fc000 modules
  `battle_module_tiles_`, `_spin_`, `_debris_`, `_hold_`, `_scroll_` and `_burst_`.
  The longest prefix a name starts with is its owner's (`menu_state_` is the
  resident's, `menu_` slot39's, `battle_setup_` ovl2615's); a subsystem word may
  follow (`field_event_`, `sound_seq_`). A shorter prefix's grep therefore also
  finds the longer ones' names (`battle_` finds `battle_results_` and
  `battle_setup_`); grep the whole name to find a symbol. An image's sources are in
  decomp/src/<target>, named after the target, not the prefix: decomp/src/menu holds
  the arena (`arena_`), decomp/src/slot39 the in-game menu (`menu_`), decomp/src/movie
  the movie mode and decomp/src/mdec the movie library (`movie_`).
- Functions are verb phrases (`cd_select_directory`, `heap_alloc`), predicates
  `is_`/`has_`/`can_` (`battle_can_use_combo_step`). A script VM's handlers are the
  prefix, the VM and the mnemonic its decoder prints, so the C and the decoder
  listings agree: `field_event_<mnemonic>` for tools/analysis/events.py,
  `battle_event_script_<mnemonic>`, `battle_ai_<mnemonic>`, `sound_seq_<mnemonic>`;
  a handler serving several sub-opcodes takes their common stem. The tests compare
  the decoder tables with the C (test_events.py; test_battle_event_vm.py also checks
  each handler's name against its mnemonic).
  Data are noun phrases: tables plural or `_table`, counters `_count`, timers
  `_timer`, flag words `_flags`, pointers to the current object `current_`
  (`game_current_data`); data written but never read say `unread`
  (`arena_bout_unread_byte`). Parameters are named from their use, and every
  prototype with its definition's parameter count names them as the definition
  does (tests/test_names.py checks it); a view declaring another count, as its
  callers pass, keeps names of its own. Single letters stay where the comment
  gives their use (a triangle's corners `a`, `b`, `c`, a colour's `r`, `g`, `b`, the
  six targets of field_distortion_set_targets).
- A name states what the code does, from the code, its reviewed comments, the docs
  and the decoders: a game-level meaning only where established, else the mechanics
  (`battle_is_target_at_lower_x`, `worldmap_eval_quadratic_bspline`); never unknown,
  unk, maybe, misc, stuff, helper or a bare do/handle/process. Names are unique
  across the codebase (the resident counts once), collide with no SDK name, macro,
  typedef, tag or keyword, and stay under about 48 characters. Another image's
  functions and variables keep their definer's names (Converting a function); a
  second declaration of one under its assembler name takes a name of its own after
  the object (`cd_movie_request`, `mode_music_buffer_header`,
  `battle_setup_work_area`). Where a name and a comment disagree, the code decides
  and the comment is rewritten with it: the mapping that gave the names and their
  evidence stays local, so a definition's comment carries its reasons.
- A unit named by an address, its image number or an ordinal became
  `<prefix>_<subsystem>.c` (`cd_reads_and_streams.c`, `battle_event_script_vm.c`,
  the arena's menu2.c-menu7.c `arena_camera_and_scenes.c` to
  `arena_scene_graph_and_opponent.c`, the resident's main2.c and main3.c
  `text_windows_and_pads.c` and `gpu_rotation_and_psyq_libraries.c`); its overview
  comment keeps its address range and says what it holds. Units are the original
  objects, so one may hold several subsystems: a world map scene unit
  (`worldmap_sceneN.c`) holds scene N's director and another scene's set-up and
  leave handlers. An INCLUDE_ASM'd `.s` file takes its function's name. Headers
  follow the units and subsystems in address order, not the prefixes
  (resident/sound.h declares `mode_battle_kind`, resident/sprite.h `gpu_get_sin`),
  so to find a declaration grep the whole name or the address.
- Each definition keeps its original address in its comment: a function's leading
  comment (`/* 80031BDC: Allocate ...` above `heap_alloc`), a variable's or an
  INCLUDE_ASM statement's trailing or leading one, a symbol-file line's value for
  the SDK functions and labels the generated assembly defines. To find the symbol at
  an address, grep the address in decomp (`git grep -i -n 80031bdc decomp`):
  definitions spell it in upper case, prose in lower case, and a symbol file
  (decomp/targets) holds each name the splits give. Overlays that share a load
  address give one address several symbols (`801c5040` is ovl2600's
  `name_entry_test_bit` and ovl2601's `item_shop_quad_place`; `801fc000` starts six
  battle modules): narrow the grep to the image's decomp/src directory or its
  decomp/targets/overlays/<image>.symbols.txt. A tool, a test or a doc that needs an
  address keeps it beside the name or reads it from the definition (the world map
  actor script handlers' table in overlay_scripts.py, test_battle_event_vm.py).

To name a symbol: take its owner's prefix (the list above) and a name the code
establishes; write it into the C definition with the address in its comment
(`/* ADDR: ... */`), or, for a label only the generated assembly defines, into the
image's symbol file as `name = 0xADDR;` (`// type:func` for a function). To
rename a symbol and every reference to it, write a mapping of its row and the
prefix rows of the images involved (`resident prefix - cd_
decomp/include/resident/cd.h high ...`), run `names.py check` and `apply`. Then
run `make -C decomp all-split`, `make -j8 -C decomp all-verify` and `python3 -m
unittest tests.test_names`.

`tools/names.py` (module docstring) applied the mapping in the matching shell, one
image or group per commit after 8185d98: 7,671 rows (4,607 functions, 2,804 data, 7
jump tables, 120 parameters, 65 units, 68 `.s` files) and 44 prefixes. Four
reviewers checked all but 171 of them, which the consolidation took as they stood
from the rows it had deferred (108 symbols, most of them the resident's SDK data,
and 62 parameters); three mappings of corrections followed. The mapping stays
local (.local, with the reviewers' evidence); `inventory` finds no placeholder in any
image, and `check` takes every row of the mapping, the corrections folded in, as
applied:

```sh
python3 tools/names.py inventory               # placeholders left: .local/names/*.tsv
python3 tools/names.py check names.tsv         # the rules above; what stays unnamed
python3 tools/names.py apply names.tsv --dry-run [--overrides overrides.tsv]
python3 tools/names.py apply names.tsv [--overrides overrides.tsv]
make -C decomp all-split && make -j8 -C decomp all-verify && make -j8 -C decomp all-coverage
```

A mapping row is `image kind old new unit confidence evidence`: kind func, data,
jtbl, unit, asm, param (old `FUNCTION.argN`) or prefix (each image's declared
prefixes; a resident subsystem's row names its header as unit). A row may also
correct a name given before (a symbol, `FUNCTION.parameter` or a unit's path that
is no placeholder); a comment names such a parameter only in backquotes. `apply`
maps a token in a file the build reads to what every target reading it binds it to
(as cross_image.py binds imports; where they differ it stops), and elsewhere (docs,
tools, tests) only where one image holds the name or a path names the image; it
lists each other occurrence with file:line for an overrides file (`file line old
image`, line `*` for the file, image `-` to keep it). It writes each renamed
definition's address into its comment (an alias given its definer's name, at the
definer's definition) and each new name into a symbol file its split reads: an
image's own names into its own file (the resident's, symbol_addrs.txt, is read by
every split, which is how the overlays name the resident's symbols), another
image's names into a file only the importer's splits read (an overlay's own; the
resident's imports.txt). It moves units and `.s` files with git mv together with
their .mk settings, yaml subsegments and INCLUDE_ASM folders, never touches
prompt.md or plan.md, and changes nothing more when run again;
`.local/names/apply.tsv` lists every change. An SDK member is one in an `sdk` range
of the classification or defined in the generated assembly the units include and
named only by SDK functions (the resident's library strings and jump tables at
80018bf0-800194f4, which splat migrates into the functions' files). A label splat
writes inside another symbol's generated file (`alabel`: twelve SDK words) is named
by a `type:label` line of the symbol file, which keeps it a label of that file (a
plain name would split it out into a file no INCLUDE_ASM includes; its users are
that unit's); one directly after the function's end, where splat drops a label with
the words after it, is a data label (`type:u32`) with an INCLUDE_ASM of its own, as
mdec's limits `libpress_vlc_max_size` and `libpress_vlc2_max_size`: the resident's
`libgte_patch_code` and mdec's `libpress_vlc_saved_state` and
`libpress_vlc2_saved_state`. `check` refuses such a label and an alias given its
definer's name where one unit sees both. The names of a second declaration bound by
assembler name, the resident's own name for the overlay area (`mode_overlay_area`,
which link.ld assigns, where each mode overlay's first unit defines its number) and
`arena_mode_heap_tag_name_strings` (a string boundary for splat in
menu.symbols.txt) were given by hand. Splat-shaped names stay only in the synthetic
targets of tests/test_matching.py and tests/test_names.py (at 80010000-80200000)
and in the two examples of splat's naming of a base address in Recovering data
(`D_8009A684`, `D_801EA5D0`), which tests/test_names.py checks; the generated
assembly under .local keeps splat's names for what no symbol file names.

Each split lists every name of the symbol files it reads that lies outside its
segments as a symbol it cannot place ("Unable to determine a segment"): the
overlays list the resident's names, the resident its imports. splat 0.50 only
warns, and the names go to the split's undefined lists, which is how a link takes
them.

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
- Strings whose alignment padding holds stray bytes stay original data:
  mark the symbol `force_not_migration:True` (with `size:` and a symbol after it where
  splat would join the strings that follow, arena_mode_entry's
  `arena_select_first_gear_model_name`), link it with INCLUDE_RODATA beside the function
  and reference it as `extern char[]`; never spell the stray bytes in a C initializer,
  also not as invented trailing elements (Recovering data). A .data object whose padding
  holds such bytes (a byte flag followed by `04`, a halfword table ending in `"Mt"`) is
  linked the same way with `INCLUDE_ORIGINAL(".data", NAME, VRAM, SIZE)` at its place
  among the unit's definitions, from the pristine input as INCLUDE_ASSET does
  (`INCLUDE_ORIGINAL_UNALIGNED`, without the `.align 2`, for a byte that directly
  follows the object before it, slot39's flag menu_saving_at_cd_change); use it only
  where the padding is non-zero and nothing reads it. Both count as `included`. The same
  holds where the object ends its unit's section and stray bytes run to the next unit's
  (arena_scene_graph_and_opponent's arena_actor_combo_inputs, then `ind`), which are not
  established as the assembler's fill (Recovering data). The coverage report finds a
  string's stray byte itself; every other included object needs an `included` line in
  the target's classification with its reason (those data objects, and the resident's
  libcd/libgpu/libspu strings that several library functions share, which splat leaves
  to INCLUDE_RODATA). Any other string is C (below).
- A routine is classified handwritten (reviewed `.s` beside the C) only on code GCC
  does not emit: trapping `add`/`addi`/`sub`/`neg`, saves below `$sp` or beyond the
  frame, `ori` for a small positive constant where the unit's ASPSX emits `addiu`,
  `bne $zero, rt` operand order, dead delay-slot copies, or a register choice that a
  probe compile of the plain C does not make (resident 8003F8B0). Raw cop2 moves,
  absolute jumps and a missing frame are not evidence: PsyQ's GTE macros compile to
  them (worldmap 800987AC was plain C). In authored `.s` the GTE command macros emit
  `.word`, so GAS cannot fold label differences across them; a code patcher addresses
  its targets by literal offsets (resident 80030988).
- Media and bytecode embedded in a unit's data (packed images, fonts, sound banks,
  scripts such as the world map actor scripts and cue sequences and the arena scene
  scripts) stay user-supplied: `INCLUDE_ASSET(".data", NAME, VRAM, SIZE)` links them in
  place from the target's pristine input (`ORIGINAL_IMAGE`, with `ORIGINAL_BASE` set in
  the .mk), and an `asset` line in the classification names the format and its reader.
  Never commit their bytes as C initializers. Media is image, glyph, sound and model
  data in a format that a generic loader or renderer of the game parses for whichever
  file supplies it (LZSS-packed data, TIM images, the font block's 22-byte glyphs of
  eleven 12-bit rows that `text_draw_glyph` draws, seds/wds banks, TMD and SpriteModel
  models), also where the code picks one record itself (the resident's glyph
  `text_special_glyph_rows`, which `text_draw_glyph` draws for the character pair 0xFF
  0xFF); a bare palette the code uploads is source where the code builds or rewrites it
  before the upload, where it decodes pixel values the code writes or computes, or where
  it is a formula's ramp, and otherwise media, the colours of a picture. Media, and
  authored content that a reader walks as a sequence (scripts, cue timelines, scene
  directions: entries that say what happens or when, consumed in order from a position
  the reader keeps across updates up to the data's own end), are assets; tables the
  program indexes to compute a result are source (sine, pitch, note encodings, opcode
  lengths, dispatch, per-character file numbers, and points, paths and layouts that code
  interpolates or steps through on its own count and timing). So the field's movie sound
  timelines `field_movie_sound_timelines`, (frame, sound) runs ended by frame 0xFFFF
  that `field_movie_play_due_sounds` plays in order, are an asset, and the world map
  ferry's eight waypoints, which `worldmap_ferry_update` steps through on each update
  and wraps itself (`worldmap_ferry_start` only resumes the route when the ferry
  spawns), are source.
- The rule was applied to every initialized object cc1 emits from the 26 targets' C (at
  8c0508e): 1,026 named objects and 989 literals (strings, jump tables); every named
  data symbol of the C objects is one of them or linked by
  INCLUDE_ASSET/INCLUDE_ORIGINAL/INCLUDE_RODATA. All 571 that are not scalars (488
  numeric arrays and structures, 65 pointer tables, 18 character arrays) were read with
  their comments, and the 283 objects (56 of them scalars) flagged by shape (an end
  value 0xFF, 0xFFFF, -1, 0x8000 or 0x7FFF that ends the object or recurs), by reader
  (an index or pointer into it that persists or advances, a test of its elements against
  an end value, its address stored for later) or by their comment's wording were checked
  against their readers. Six were authored sequences and are now assets, 528 bytes:
  `field_movie_sound_timelines` and the world map's terrain texture animation runs
  `worldmap_texture_anim_slot0_frames`, `worldmap_texture_anim_slot1_frames`,
  `worldmap_texture_anim2_slot0_frames`, `worldmap_texture_anim2_slot1_frames` and
  `worldmap_texture_anim2_slot2_frames`, (image, duration) frames ended by a negative
  duration that `worldmap_texture_anim_advance` and `worldmap_texture_anim2_advance`
  step (docs/scripts/timelines.md). That pass looked for media only among the objects
  passed to LoadImage or SpuWrite. A second pass (at 5559538: 1,020 named objects, 565
  of them not scalars) followed each object into its readers: the calls it reaches
  itself or through a local pointer set from it, the other values that pointer takes,
  and where its address is stored. It found one more asset, the glyph
  `text_special_glyph_rows` (22 bytes), which `text_draw_glyph` takes in place of a
  22-byte record of the loaded font. No other object reaches a media reader as the data
  it parses (they give it file numbers, sound and character codes, VRAM places, draw
  modes, colours or a destination), except the four palettes below. None remains: the
  others are lookups by a key the code computes, also where an end value closes them
  (the picture table `field_picture_table` searched by map, the battle modes' sound
  programs `mode_battle_sound_programs`, the gear shop lamps' frames
  `gear_shop_lamp_frame_images` on the code's timing), lists one call processes whole
  (the battle panel glyph sets ended by 0xFFFF, the world map's object links
  `worldmap_airship_object_links`), geometry the code interpolates or steps through on
  its own count (the world map's camera and flight paths, which
  `worldmap_eval_quadratic_bspline` interpolates at the parameter its scene code
  advances; the ferry's waypoints; the scripted flights' waypoints, whose counts
  `worldmap_flying_vehicle_update` fixes, never reading their -1 ends), texture layouts
  (the menu font's glyph rectangles `arena_text_glyphs`), and masks and thresholds. The
  four bare palettes passed to LoadImage are source: the text palette `text_palette`
  decodes the 2-bit codes `text_draw_glyph` writes into either half of each 4-bit pixel
  (1 the glyph, 2 its outline), entry i of its first CLUT being the colour of code i & 3
  and of its second that of code i >> 2, and `window_open` gives each line the CLUT of
  its plane; `console_load_font_cluts` rebuilds all 64 entries of the console font CLUTs
  `console_font_cluts` before their only upload; the gauge palette
  `arena_hud_gauge_palette` is the grey ramp 0x8000 | 0x421 * i (i = 1..14, opaque black
  at 0 and 15); and arena_scene_graph_and_opponent's glow ramp `arena_glow_palette`
  colours the heat values `arena_glow_step` computes, with bit 15 set on every entry by
  `arena_glow_init` before its upload.
- K&R definitions, unprototyped calls and implicit-int returns are legitimate where
  the original passes unpromoted arguments or keeps `$v0` live.
- Unit compiler settings are qualified per code unit (Qualified configuration, above,
  names the units whose bytes leave it open); compiling every remaining draft under
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

- cse replaces a register source by a known constant whenever it can (a MIPS CONST_INT
  costs 0, a pseudo 1), so a surviving `move` of a register that was just zeroed means
  both cse passes lost the value: the zero came from an expression only combine reduces,
  such as a byte shifted right by 8 (slot39 801c93a8). A cse block ends at a referenced
  label; the first pass also ends at a loop-end note (any `do { } while (0)`), the
  second (`-frerun-cse-after-loop`, on at `-O2`) does not. cse keeps going past a label
  whose remaining uses it removed itself, and `-fcse-skip-blocks` extends a block over
  an `if` without inner labels, so a dead `if` hides nothing.
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
- The original's store order need not be the source order. In GCC 2.6.3 and 2.7.2,
  sched1 places a pseudo set once as a whole register (a register birth) beside its
  first store and puts any other set (an s16 local set through a subreg, a variable
  set twice) above the stores of equal priority, which win ties as memory-unit insns.
  In sched2 the reloads of spilled pseudos and the stores through a register never
  cross (the scheduler cannot tell a stack slot from the stored element), and between
  two reloads the stores of the latest-loaded values go last. So the stores between
  two of the original's reloads were already between them before sched2: the
  border of field 8007E1C0 (`field_dialogue_draw_frame`) matched written piece by
  piece with every coordinate inline, after a draft had copied the scheduled order
  with s16 locals.
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

Data placeholders (`remaining_data_placeholder_bytes` in the coverage report, and
`remaining_bss_placeholder_bytes` for the uninitialized data past an image) are
converted to C per unit. What converting the targets' `.data` established:

- A unit emits each section in definition order and the original linker joined them
  in unit order, so a block belongs to the unit whose section holds it, not to the
  units that read it (menu_framework.c holds tables only its later units use; battle
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
- A table's readers, not the span to the next symbol, give its extent, and stray fill
  after it is not an element. Four tables spelled theirs as extra elements and are
  INCLUDE_ORIGINAL objects with `included` lines now, their externs declaring the true
  extent: the world map's flame sizes worldmap_scene15_flame_sizes (5 u16 for flame
  actors 4-8, worldmap_scene15_flame_start and worldmap_scene15_flame_update; fill 65 79)
  and gear parameters worldmap_gear_sprite_height (3 s16 for the members 0-2
  worldmap_place_vehicle passes; fill 00 3c), slot39's sheet images
  menu_deathblow_row_images (13 rows of 5 u8, menu_deathblow_screen_layout_row for the
  rows of menu_deathblow_screen_build; fill 00 07 2e) and battle's combo flags
  battle_combo_step_flags (15 u8: battle_can_use_combo_step,
  battle_combo_chain_add_gear_step and battle_combo_record_gear_step index at most 14
  from combo steps 0-2, or 0xff at attack level 4; fill 35). `tools/stray_padding.py`
  (module docstring) lists every linked C data object whose last 1-3 bytes, whole byte
  or halfword elements as cc1 emitted them whatever the declared shape (a flat table's
  last elements, the end of a 2-D table's last row, a structure's last members), would
  be alignment fill before the next object and hold a non-zero byte that no
  constant-offset access reads (`tail`, noted `unread` where every access is exact and
  none reaches them, `text` or `outlier` where they look stray), whose accesses are all
  exact and leave an unread rest with a non-zero byte in an alignment slot, word
  elements too (`unread`), that nothing references (`unref`), that is declared wider
  than every access with a byte none touches (`wide`), or whose string holds bytes after
  its terminator (`string`). It reports
  `2301 C data objects, 244 to review; objects per flag: tail 209, tail read 8, unref 42`
  (222 distinct, 195 with a tail and 33 unreferenced; the second executable repeats the
  resident's; the resident's zero byte mode_arena_bout_outcome is an object in each
  executable and is not flagged), each reviewed against its readers. None other spells
  stray fill: the tails are read (masks `& 7` and `& 3`, the frame counts 0x10, the
  count passed with each label list, the 18-, 7- and 16-entry glyph label loops of
  ovl2596 and battle) or complete their structure (single-bit masks, permutations of the
  eight facings, a CLUT LoadImage'd 16 wide, the round map's 128th row continuing both
  column curves, frame rates 60/n for n = 2-60 dividing 60, the party panels' per-member
  pattern, a pilot per gear of the 20, lamp frames, per-character and per-gear tables;
  the last rows of 2-D tables, battle's timer reloads and list separators by AP and list
  size (battle_ap_timer_reload_table, battle_separator_rows_by_list_size: each row holds
  one value more than the row before), the field compass grid's (rows 4-8 alike or
  stepping on), the menu wheel's offsets (the second row's slide -0x24 mirrors the
  first's) and the resident's sound programs per battle mode
  (mode_battle_sound_programs, 0xff absent as for modes 0 and 4); and the last members
  of whole records, battle's 26 command panel pages
  battle_command_panel_page_00-battle_command_panel_page_19 (the lists each fills, 0xff
  none, and their glyph sets, read through battle_command_panel_pages) and sound banks
  battle_sound_table, the field's panel frames, particle sprites (the last corner closes
  the quad), portrait and text places (palette rows e0-e7), icon and strip origins,
  image pieces and style pages, the menu's shot kinds, animation rules (the last's next
  -1, as for the 17 before it), font glyphs and arena frame points, the world map's area
  file sets (each area's last parameter 2), the resident's texture positions and the
  last cosine 4096 of its sine table), or are the sentinels their loops stop at (-1,
  0xffff, also the world map's camera path pad); a stray byte that continued its
  structure's pattern would pass this review too. The unreferenced ones are words,
  structures, strings, documented unread tables and copies, or tables read through a
  base formed before them (`battle_combo_next_step_table_by_paid`,
  `menu_ascii_to_sjis_table[hi - 0x20]`, `[text[0] - 1]`,
  `[(top_cursor - 1) * 4 + list_cursor]`). slot39's unreferenced bytes 08 00 at
  801E96A6, between the flags menu_save_command_stays_open and menu_saving_at_cd_change
  and the u16 masks menu_bit_masks (GCC 2.6.3 emits consecutive byte scalars back to
  back and aligns the arrays to a word), are taken as the flag's padding by analogy with
  the flags closure D links with theirs, battle's battle_applying_item_results (08 00 00)
  and ovl2596's battle_results_fanfare_started (04 00 00): menu_saving_at_cd_change is
  linked with them, with INCLUDE_ORIGINAL_UNALIGNED (it follows
  menu_save_command_stays_open directly). Neither the bytes nor the vendor tools decide
  it, here or for those two, which could as well each be a flag, an unreferenced byte 8
  or 4 and zero fill: nothing in any image reaches 801E96A6; battle's flag battle_paused
  before the same two mask tables (battle_slot_bits, battle_flag_bits) is followed by
  zeros, as slot39's other byte groups are (801E977B, 801E9786-87); and Psy-Q 3.5's
  CC1PSX 2.6.3.SN.2 and ASPSX 2.34 under DOSBox build the whole slot39 unit, with or
  without a byte 8 there, to the original .data but for zeros at every stray byte (08
  without the byte, 07 2e after menu_deathblow_row_images in both). Battle's
  battle_in_automatic_turn and battle_effects_disabled (00 08 00 71, 00 74 72 73) are no
  such analogue: GCC would put the byte flag that follows each
  (battle_applying_item_results, battle_gear_objects_loaded) directly after it, so their
  three bytes need a unit boundary, unreferenced data or a word-aligned next flag.
- An object that ends its unit's section can be followed by stray bytes up to the next
  unit's. In the targets' links eleven included objects end their unit's section so: the
  strings field_clear_otag_label and field_error_id0_format (field),
  arena_debug_rate_format (arena_mode_entry), battle_debug_state_page_char_format (debug2611's
  pages.c) and item_shop_save_file_prefix (ovl2601) and the .data objects
  arena_actor_combo_inputs (arena_scene_graph_and_opponent, `ind` before
  arena_camera_and_scenes's .sbss) and battle_unreferenced_stray_byte (battle), all of
  ASPSX 2.34 units, and battle_music_lowered (battle) and
  battle_setup_next_member_image_column (ovl2615) of 2.56 units and the world map's
  worldmap_scene15_flame_sizes and worldmap_gear_sprite_height of 2.79 units; so does
  the world map's cue sequence asset worldmap_scene13_cue_waits. Psy-Q 3.5's ASPSX 2.34
  pads no section's end under DOSBox: a 6-byte .data or a 5-byte .rdata stays that long,
  also when another section follows, and a section entered again goes on at that offset.
  If the original 2.34 assembler did the same, the stray bytes of its units lie in the
  gap the link left before the next section, not in the assembler's fill (the later
  assemblers' section ends are not probed). PSYLINK 2.37 writes zeros in such a gap
  under DOSBox when every section is in its default group (also after 64 KB of pattern
  data and between two objects' .data); with text and bss groups `/p` writes the file
  only to the .data's end. The DOSBox runs also write zeros at the stray bytes inside
  sections, so which tool wrote any of these is open. The nine gaps GNU ld leaves
  between input sections in the targets' links (resident 6, menu 1, slot39 2) are zero
  in the originals; the non-zero ones lie inside the included objects and the asset
  above.
- One table's extent stays open. ovl2143's unread copy gear_model_battle_gear_file_table
  of battle's gear file table battle_gear_file_table is battle's byte for byte, as its
  copy gear_model_battle_extra_file_bases of the 18-byte extra file bases
  battle_extra_file_bases is, but for the last pair: 66 00 where battle's has 00 00. The
  bases of gears 0-18 chain (each is the previous gear's base plus 2 plus that gear's
  variant count, the files battle_read_gear_files reads) and fill directory (0x28, 1)
  exactly: on both discs its file 1 heads a sub-directory of the 62 files 2-63 that
  gears 0-18 take (tools/analysis/disc_index.py). Neither twentieth pair continues the
  chain or names gear files (base 0 reads that header, base 102 the `wds ` wave banks
  103 and 104), but the game data holds 20 gear records (field's
  field_event_restore_gears) and battle_read_gear_files indexes the table by a
  combatant's gear id without a range check. With 19 pairs, battle's 00 00 is GCC's zero
  fill before the word-aligned battle_extra_file_bases, and ovl2143's 66 00 follow the
  unit's last .data object as `ind` follows arena_scene_graph_and_opponent's
  arena_actor_combo_inputs (above): both are ASPSX 2.34 units, and both files go on with
  zeros for the uninitialized variables after those bytes (ovl2143's .bss, the menu's
  .sbss). Neither the readers nor the vendor tools, which write zeros at every stray
  byte under DOSBox, tell 19 entries with fill from 20 entries. So the copy whose last
  pair holds a non-zero byte is linked with INCLUDE_ORIGINAL and an `included` line
  rather than spelled as a twentieth C element, and battle's, whose last pair is zero
  either way, stays C with 20 pairs, until a reader decides.
- splat names addresses the code forms from a base plus a constant (`D_8009A684`, four
  entries before the flame sizes; `D_801EA5D0`, 0x20 before the Shift JIS codes).
  Declare the real object and index it as the code does (`worldmap_scene15_flame_sizes[index - 4]`,
  `menu_ascii_to_sjis_table[hi - 0x20]`): it compiles to the same address. Interior names that
  remaining assembly still uses go in `<target>.data.ld` (`D_x = D_y + off`; splat's
  `undefined_syms_auto.txt` covers only unaligned ones). These aliases are
  scaffolding, deleted when their last assembly user matches (slot39's and field's
  have gone). One of a static resolves only because maspsx makes `.lcomm` symbols
  global, where ASPSX kept them local. Battle's C addresses parts of its commons by
  names and views of their own (the battle area's slots, work and drawing state; the
  camera's matrix), hundreds of times and also from CDK units, where one symbol's
  members share a `%hi`: `battle.data.ld` defines those names from the objects'
  linked addresses. It also names two tables from before them (the timer reload
  rows, the combo step table one byte before battle_combo_next_step_table): GCC 2.6.3 forms such a
  base with `la` only for a declared array, and `battle_combo_next_step_table[step][paid - 1]` puts
  the -1 in the load instead. Where the index form compiles alike, C indexes the
  object (battle's party panel name glyph positions, `battle_panel_glyph_x[member * 24 + 7 + i]`).
  The resident's `link.ld` (the window colour's last two bytes, the CD mix bytes,
  a base for the name slots' second bytes), `menu.bss.ld` (the
  opponent's command byte) and `worldmap.data.ld` (the read list's first
  destination) name parts of uninitialized objects the same way, each where the
  member compiles differently. splat writes an interior address of a C object as an
  offset from it, not as a name of its own in `undefined_syms_auto.txt`, once the
  target's symbols.txt gives the object's `size:` (the world map's, the menu's and
  the small overlays'), and leaves out a name a C unit itself defines at an
  unaligned address that is marked `defined:True` there (debug2611, slot39, ovl2602,
  mdec).
- An address the code uses as an object of its own is one, also inside data that
  stays generated: the resident passes the disc data ahead of its code to the disc
  initialiser as its boot mode word, its sector-24 file index buffer and its sector-40
  directory table, so these are three rodatabins (`disc_mode`, `disc_files`,
  `disc_directories`), each name its own object. Generated data that points into a
  block of strings is relocated against the block once spimdisasm knows its size: a
  `size:` in symbol_addrs.txt (the resident's libcd command name tables and the
  libraries' `$Id:` strings) makes each pointer word the block's symbol plus an
  offset rather than a number.
- GCC emits a function's string literals into `.rodata` ahead of its code, in the order
  of first use, and its jump tables after it; an initializer's literals in reverse
  order once the definition ends (arena_mode_entry's gear list and heap tag names). A unit emits
  identical literals once, so a second copy is an array (movie's second `"\n"`), and a
  table whose strings follow some function's literals is defined after that function,
  with the data defined around it in data order (arena_fighters_bout_and_effects's ether
  name and combo names, arena_menu_screens's level, command and menu line names).
- Several images end with zeroed `.bss` (slot39, menu, mdec, ovl2143, ovl2596,
  ovl2601, ovl2602, ovl2615). Uninitialized variables are defined uninitialized in
  their unit, never as zero data, and where a file holds its `.bss` as zeros the
  `.bss` is loaded (splat `ld_bss_is_noload: False`, one `.bss` subsegment per unit).
  Where it does not (battle), the segment's `bss_size` and a `.bss` subsegment per
  unit at its vram past the file keep its names out of `undefined_syms_auto.txt`,
  whose absolute values would otherwise override the C definitions without a
  warning, and the link asserts the bounds the resident's mode table clears.
  Where it lies past the file (field, the world map, the menu's larger variables,
  movie, the resident) it is not loaded, and the yaml still lists each `.bss`
  subsegment with its vram and the segment's `bss_size`: splat then leaves their
  names out of `undefined_syms_auto.txt`, so the link places every variable from
  its definition, and the target asserts the bounds its loader clears
  (`field.bss.ld`, `movie.bss.ld`, `menu.bss.ld`, `worldmap.data.ld`,
  `battle.data.ld`, the resident's `link.ld`). Each target owning such data also
  sets `BSS_END`, that bound: coverage counts what generated assembly or only a
  linker-script name places up to there as `bss_placeholder`, and `verify` fails on
  such a name; none is left. The PsyQ libraries' uninitialized variables in the
  resident stay generated (`psyq_*` subsegments) and count as `sdk` by their
  classification lines, like the libraries' `.data`.
- GCC emits a unit's function-local statics, then its file-scope tentative
  definitions in the order of their first declaration (a header's `extern` counts),
  and maspsx allocates both in the unit's `.sbss`/`.bss`, packed without alignment
  (in `.sbss` it 8-aligns an 8-byte object). In the original images each object takes
  a slot of whole words, an 8-byte one also at 4 mod 8 (menu 8009265c, slot39's RECT
  801ea8e4), evidenced separately for ASPSX 2.34's `.lcomm` statics (mdec
  801e8958-801e8968: five u8, stored and loaded bytewise;
  arena_fighters_bout_and_effects 80092678-800926a0; slot39 801ea710/801ea714) and for
  the commons PSYLINK allocated (ovl2596 801e44e0/801e44e4; libcd's Stsector_offset
  alone at 801e89bc; the ASPSX 2.79 world map's four u16 at 8009bd10-8009bd1c, which
  three units share). The build gives each object whole words at a word boundary under
  ASPSX 2.34 and the world map's setting 2.79 (it stands for an assembler evidenced only
  as >= 2.50, above): the world map's commons unit, worldmap_common.c, puts consecutive
  halfwords a word apart as the image does (8009bd10-8009bd1c, 8009bd24/8009bd28,
  8009cd4c/8009cd50), where the 2.56 rule would pack them two bytes apart. PSYLINK
  placed those commons; no world map unit has statics and those of the 2.79 resident
  sprite units are words, so the 2.79 assembler's own `.lcomm` rule shows nowhere. ASPSX
  2.56 keeps each `.lcomm` object's size and aligns it by that size up to a word: battle
  800B3F04's four s16 statics lie two bytes apart at 800c3ca4-800c3cab (four symbols:
  the CDK compiler addresses one object's members from a single `%hi`), its u8 is
  followed by a u8[3] at the next word, and 800B8098's SVECTORs lie at 4 mod 8. Any
  other version rejects a sub-word object (decomp/Makefile); no target or unit setting
  selects a slot rule. Variables that share a word are therefore one object, also in the
  resident's `.bss`: the sprite position sprite_image_list_position is a DVECTOR. The window colour
  window_color, the overlays' `u8[3]` (a common of commons_small.c), is declared a
  `u8` in mode_battle_and_menu.c, which writes the other two bytes through `link.ld` names:
  ASPSX 2.34 addressed a common at an offset absolutely (maspsx models it), but GNU
  as moves a small common's offset accesses to `$gp`.
- A unit's own variables come first, in unit order, as statics where the commons follow
  apart, and a unit reads only its own: `tools/data_users.py CONFIG.mk` reports every
  FOREIGN reference, another unit's code forming an address in a unit's own `.bss` (in
  no target but the menu, whose one, 80092a30, arena_camera_and_scenes's
  arena_scene_update_bout_end only forms as the end of its loop over the embers
  arena_scene_bout_end_embers[3]: it is the pad word of
  arena_fighters_bout_and_effects's arena_effect_hit_spark_position behind them), and,
  with `--end`, the order of variables still extern. That places menu 800707A8 and
  8007E528 exactly, the arena_menu_screens/arena_stage_views_and_hud boundary at
  80081E00, 80081E6C or 80081ECC, slot39's after 801CD2AC and at or before 801DBDB4 (an
  earlier one moves the `.bss` boundary with it), and battle 800B7870's unit at or
  before 800B7134, whose shatter draw shares its battle_shatter_ot; the latest is kept.
  The commons, which the original linker allocated after every unit's own in an order of
  its own (mdec's five player commons among the 20 of libcd's CDROM.OBJ), are defined by
  a commons unit linked last (menu_overlay_common.c, menu_common.c, gear_shop_common.c,
  battle_common.c, field_common.c, worldmap_common.c, mdec commons/; the resident's
  commons/ units lie between the PsyQ libraries' generated ranges), which reproduces the
  linker's placement rather than modelling it. It defines them ahead of the headers that
  declare them, structures by their tag: GCC 2.6.3 lays out such a tentative definition
  once a header completes the type. Functions on both sides of a supposed unit boundary
  that read the same statics are one unit: the resident's cd_reads_and_streams.c runs
  from 8002709C to 8002C3E8. One declaration then serves every user, which leaves one
  accepted compromise there: the CD mode byte cd_setmode_parameter is a `u8` (80028f30's
  tests match only with a scalar; a `u8[4]`, a union or a word read bytewise keep its
  address in a register), while 80029690 and 8002a428 clear and pass all four bytes of
  the CdlSetmode parameter in its word slot through `&cd_setmode_parameter + 3`. Zeros a
  packer added past the program are file padding (Compressed containers).
- Code shows where an object starts and how far it reaches. A member at a nonzero
  offset is addressed through a pseudo holding `sym+off`, which cse reuses and relates
  to any other offset of the symbol and which can stay in a register (hoisted by
  loop.c in field 80077e88, kept across a call in 800859dc); a variable of its own is
  addressed absolutely at every use. Relative addressing (`addiu a0, s2, -0x162`)
  and block copies span one object. Field's work block 800b2078 ends at 800b235c,
  the 0x2e4 bytes 800a3f4c saves, and nothing addresses past 800b2358 from it;
  800b235c, 800b2360, 800b236c and 800b2370 are commons of their own (800815f0,
  80081c54, 80085738, 8008848c and 800859dc do not match them as members), the
  effect launch fields one struct from 800b2374.
- The menu (GCC 2.7.2) splits its uninitialized variables by size. Those of up to
  eight bytes, each unit's own in unit order then the commons, fill 800925d4-80092954
  and end the program; the larger ones follow past it in the same order, each unit's
  own to 80096fa8, then the large commons up to the mode table's BSS end 8009b558
  (`tools/data_users.py decomp/targets/overlays/menu.mk --end 80096fa8`: no
  INVERSION, and the one FOREIGN address above). The GCC 2.6.3 images keep one
  `.bss` in declaration order (menu_member_screens's 2-, 200-, 200- and 1-byte statics at
  801ea72c-801ea8c0; mdec's 64-byte movie_decoder among 4-byte statics), so the split
  is the assembler's own 8-byte small-data threshold, applied to the `.lcomm`/`.comm`
  GCC emits after a `-G0` unit's code, with the linker's small commons. The code,
  assembled before them, addresses every one absolutely (maspsx's own `-G8` would
  move those accesses to `$gp`), so the build keeps `-G0` and `SBSS_<file> := 8`
  (decomp/Makefile) moves each object of at most 8 bytes, by the size GCC declares,
  from the `.bss` maspsx appends to a `.sbss`, the slot rule applying to both. menu.mk
  sets it for every unit with variables, commons unit included; menu.yaml links each
  unit's `.sbss` into the file (data entries with `linker_section: .sbss`) and its
  `.bss` NOLOAD past it, one subsegment per unit with the segment's `bss_size`, and
  menu.bss.ld asserts both bounds. The task scheduler's two words open
  arena_scene_graph_and_opponent's larger variables: an explicit `.bss` in its
  handwritten arena_task_save_scheduler.s, outside the assembler's rule, classified
  `handwritten`.
- The resident's BSS (800592bc-8006faf0, the span its entry point clears) has the same
  four parts, the PsyQ libraries' statics after the game units' own and their commons
  among the game's: every unit's variables of up to 8 bytes in link order
  (800592bc-800593a4: the `-G8` units' `.sbss` and, by `SBSS_<file> := 8`, that of the
  GCC 2.7.2 `-G0` units main, model_renderer, text_windows_and_pads and
  console_and_sound_driver), the libraries' small statics, the small commons
  (commons_small.c), every unit's larger variables (800595e8-8005a1fc; the GCC 2.6.3
  unit cd_reads_and_streams keeps all its statics there in declaration order), the
  libraries' other statics, and the other commons (commons_before_libspu.c to
  commons_after_libgpu.c; GameData game_data with its full 0x2358 bytes). Each unit
  defines its own as statics. Where nothing addresses the end of a `-G0` unit's larger
  object, a word of its own would be small and lie in the unit's `.sbss`, so the object
  reaches to the next one (the number codes text_number_codes[14], the stage file list
  mode_battle_stage_file_list[4]). The SPU malloc table sound_spu_malloc_table, 8 * (4 +
  1) bytes, ends the BSS; link.ld names its last word boot_bss_last_word from the BSS
  end for the entry point and the mode table and asserts the span.
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
  are battle and ovl2596 flags, and the gear riding lock at 80059179 sat in what was
  taken for padding.
- The resident clears each mode overlay's `.bss` from the address its mode table
  records with a pre-increment loop, so the first object sits 4 bytes later (movie:
  80076f38, counters at 80076f3c; field's RECT ring). Each mode overlay's link fails
  unless its linked `.bss` is exactly that span (`field.bss.ld`, `movie.bss.ld`,
  `menu.bss.ld` with its `.sbss`, `worldmap.data.ld`, `battle.data.ld`), and the
  resident's unless its own is the span the entry point clears (`link.ld`).
- Embedded game data stays generated and is classified `asset` with its format
  (arena_scene_graph_and_opponent's SpriteModel arena_actor_extra_model); library data
  is classified `sdk` by the code that reads it. splat migrates rodata used only by an
  INCLUDE_ASM function into that function's `.s` file, and coverage counts it with the
  function (the resident's library strings and jump tables as `sdk`) until the function
  is C and the compiler emits it; mdec's libpress/libcd messages are a generated rodata
  segment classified `sdk`. Name data only by what its readers show (the libcd commons).

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
INCLUDE_RODATA'd file) as bytes, not instructions, of their owner's class, attributed
like data below: an INCLUDE_ASM'd file's under its function's class, an
INCLUDE_RODATA'd file's `included`, a generated assembly unit's `asm`, an authored
one's `handwritten`, a classified range first. The bytes cc1 puts in .text outside its
functions are data objects and never count as C: machine words in a
`section(".text")` array could replace an INCLUDE_ASM'd function and still match.
Such an object counts as `text_data` (bytes, no instructions) only where the target's
classification has a `START END text_data NAME REASON` line for it: NAME is cc1's
label at START, the range ends at the next symbol, and the reason is the evidence that
the original keeps the object in .text (arena_mode_entry's mode-task table
arena_mode_tasks). Every other such byte fails the report, whatever placed it: the
attribute in any spelling, a function's static under 2.7.2-cdk, or a definition cc1
emits while an INCLUDE_RODATA has left the assembler in .text. The report also fails on
a data directive cc1 emits among a function's code (`-membedded-pic` jump tables), on a
function in a data section and on an input section other than .text and the data
sections, and unless every function lies inside its .text input section and overlaps no
other, so `text_bytes` is exactly the sum of the .text input sections. From the link map
it also attributes every loaded data byte: each .rodata/.data/.sdata input section and,
where an image holds its uninitialized variables as zeros, each .bss/.sbss input section
of a loaded output section (alignment gaps and a packer's tail belong to no input
section). The uninitialized data past the image, which the file does not hold, counts
apart (`bss_noload_bytes`, `bss_noload_classes`): each no-load .bss/.sbss input section
as `bss` from a C unit, as its classified range (`sdk`: the resident's generated PsyQ
library statics and commons; `handwritten`: the menu scheduler's two words) or as
`bss_placeholder` from generated assembly, and as `bss_placeholder` every byte up to the
target's `BSS_END` that no input section holds, outside the no-load output sections or
named by a symbol there (a variable only a linker-script name places; the fill between
input sections is not counted). `remaining_bss_placeholder_bytes` totals that remaining
work, 0 in every target. A function is `c` only where its unit's cc1 output emitted it
(`.ent`); one an INCLUDE_ASM'd or INCLUDE_RODATA'd file defines is `sdk` or
`handwritten` by range, `nonmatching` as the fallback of a NON_MATCHING candidate (the
only source scan), else `asm`. A data byte is compiled C (`c`, or `bss` for C-defined
loaded .bss), original bytes INCLUDE_RODATA'd or INCLUDE_ORIGINAL'd in C (`included`,
also the resident's libcd/libgpu/libspu strings that several library functions share),
the other bytes of an INCLUDE_ASM'd `.s` file (under its function's class: `sdk`,
`handwritten`, or `nonmatching`/`asm`, totalled in `remaining_data_asm_bytes`), authored
assembly, a classified `sdk`/`asset` range, or a generated `placeholder`
(`remaining_data_placeholder_bytes`, loaded .bss included). `asset` marks user-supplied
game data or bytecode that is parsed and documented rather than rewritten as source.
Each included object (a statement's bytes in one section, from its label) that no
classified range covers needs its reason, or the report fails: either it is one text
string whose terminator is followed by 1-3 bytes of alignment padding, one of them
non-zero, or a `START END included NAME REASON` line of the target's classification
names its range and label; a line that names no included object fails too. Handwritten
bytes count only inside a `handwritten` range, an authored `.s` unit's too, so the
classification stays the record of every handwritten routine. `make coverage` passes
these files as `CLASSIFICATION`, and `matching_coverage.py ... --list CLASS` prints a
class's functions, its .text bytes outside every function and its data ranges with their
section and object (`placeholder`, `included`, `asset`, `bss`, ...); `bss_placeholder`
ranges are split at each symbol, so each remaining variable shows with its extent.

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
[original-boundaries.md](original-boundaries.md) records the services, timing,
control-flow, display and sound-mode boundaries the recovered source shows, with
the scan that inventories them (`tools/service_calls.py`) and each observation's
limits. Use docs/executable-reconstruction.md only when working on that reference
harness.
