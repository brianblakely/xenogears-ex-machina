# Disc 1 resident boot program (PS-X EXE, text 0x80010000, entry 0x80019524).
# Code and data after the embedded disc index are identical to Disc 2's
# SLUS_006.69; only 0x80010000-0x8001807c differs.
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/resident/slus_006.64.yaml
ORIGINAL := .local/extract/disc1/SLUS_006.64
ORIGINAL_SHA256 := dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119
# Its uninitialized data ends at 8006faf0: the entry point (80019524) clears
# the words after D_800592B8 through D_8006FAEC, the bounds of the mode
# table's entries 0 and 5.
BSS_END := 0x8006FAF0
BUILD := .local/decomp/build/resident
IMAGE := .local/decomp/build/SLUS_006.64
LINKER_SCRIPT := .local/decomp/resident/slus_006.64.ld
LINKER_EXTRA := .local/decomp/resident/undefined_syms_auto.txt .local/decomp/resident/undefined_funcs_auto.txt decomp/targets/resident/link.ld
# link.ld names parts of uninitialized objects from the objects' linked
# addresses where addressing the object itself compiles differently (the
# window colour's green and blue bytes, the CD mix bytes,
# the base of the name slots' second bytes), and the BSS's last word, which
# the entry point and the mode table name, from the link's BSS end. verify
# accepts these names inside the BSS as views (decomp/Makefile).
LINK_VIEWS := decomp/targets/resident/link.ld
# The header (decomp/src/resident/header.c) declares t_size 0x49800 from the
# link (link.ld), and the file is padded with zeros from the end of the small
# data (0x800592bc) to the same 2048-byte boundary (file 0x4a000), which
# the link gives as __exe_file_size.
OBJCOPY_FLAGS := --gap-fill 0
PAD_TO_SYMBOL := __exe_file_size
SOURCE_DIRS := decomp/src/resident
CLASSIFICATION := decomp/targets/resident/classification.txt
# Disc 2 shares this assembly directory; a fresh split regenerates both.
SPLIT_ALSO := decomp/targets/resident/slus_006.69.yaml
# The heap unit addresses its small globals through $gp.
GP_heap := 8
# Data-only units holding initialized small globals that every user loads
# and stores absolutely (kernel_settings.c, sprite_settings.c): at -G8 GCC
# puts them in .sdata. Their compiler is immaterial: GCC 2.6.3, 2.7.2 and
# 2.7.2-cdk give them identical .sdata and relocations, so the target's
# CC_VERSION, which they keep, qualifies nothing here.
GP_kernel_settings := 8
GP_sprite_settings := 8
# The heap report file unit (80032DCC-80032E7C) owns its file handle as a
# $gp small common but addresses other units' small globals absolutely
# (80032E04's stores of D_800592B8 match only so). GCC 2.6.3 and 2.7.2 build
# identical objects for it (2.7.2-cdk does not); it keeps the target's 2.7.2
# of the heap unit before it.
GP_heap_80032DCC := 8
MASPSX_heap_80032DCC := --aspsx-version=2.34 --use-comm-section
EXTERN_heap_80032DCC := absolute
# The sound driver unit is compiled by GCC 2.6.3.
CC_sound := 2.6.3
# The sprite unit (8001C8DC-8002709C) is compiled by the Cygnus CDK GCC
# 2.7.2 (only it reproduces 80021c20's two loads of the stack top, lb then
# lbu, and 80022a70's register choice), assembles positive `li` as `addiu`
# (ASPSX 2.50+, set as maspsx 2.79, with which 2.56 builds identical objects;
# the rest of the game code uses `ori`, the default 2.34) and
# addresses the small globals it defines through $gp (as small commons, so
# maspsx knows them). Other units' small globals it addresses absolutely
# although GCC declares them small (`.extern name, size`), and it takes
# addresses (`la`) absolutely.
CC_sprite := 2.7.2-cdk
GP_sprite := 8
MASPSX_sprite := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite := absolute
# The second (80022090-800248D4), third (800248D4-80025C04) and fourth
# (80025C04-8002709C) sprite units are built the same way.
CC_sprite_80022090 := 2.7.2-cdk
GP_sprite_80022090 := 8
MASPSX_sprite_80022090 := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite_80022090 := absolute
CC_sprite_800248D4 := 2.7.2-cdk
GP_sprite_800248D4 := 8
MASPSX_sprite_800248D4 := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite_800248D4 := absolute
CC_sprite_80025C04 := 2.7.2-cdk
GP_sprite_80025C04 := 8
MASPSX_sprite_80025C04 := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite_80025C04 := absolute
# The texture-scroll, disc, CD read callback, stream and model buffer unit
# (8002709C-8002C3E8) is compiled by GCC 2.6.3 with inline division checks
# (its code from 8002A260 on has no division).
CC_main_8002709C := 2.6.3
MASPSX_main_8002709C := --aspsx-version=2.34 --expand-div
# The battle-mode entry owns 8005959C, while its setup flags are owned by
# the following menu-support unit. This split preserves those GP/absolute
# accesses with that unit's small-common pipeline. GCC 2.6.3 and 2.7.2 build
# identical objects for it (2.7.2-cdk does not), so its bytes do not decide
# between them; its setting follows the GCC 2.6.3 menu-support unit.
CC_battle_mode := 2.6.3
GP_battle_mode := 8
MASPSX_battle_mode := --aspsx-version=2.34 --use-comm-section
EXTERN_battle_mode := absolute
# The menu-support unit (8001B844-8001C76C) is compiled by GCC 2.6.3.
# Its own small commons use $gp; other units' small externs and every
# address taken with `la` are absolute, as in the sprite units above.
CC_main_8001B6C4 := 2.6.3
GP_main_8001B6C4 := 8
MASPSX_main_8001B6C4 := --aspsx-version=2.34 --use-comm-section
EXTERN_main_8001B6C4 := absolute
# The assembler of the GCC 2.7.2 -G0 units placed each one's own variables of
# up to 8 bytes in its .sbss and the larger ones in its .bss (decomp/Makefile):
# all of them link among the units' small variables (800592bc-800593a4) or
# their larger ones (800595e8-8005a1fc) in unit order, and the code addresses
# every one absolutely.
SBSS_main := 8
SBSS_main_8002C3E8 := 8
SBSS_main2 := 8
SBSS_main2_800366E0 := 8
# Embedded media stay user-supplied: INCLUDE_ASSET reads them from ORIGINAL,
# whose file offset 0 (the 2 KiB PS-X EXE header) is VRAM 0x8000F800.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8000F800
