# Disc 2 resident boot program. Identical to Disc 1 outside the embedded disc
# index at 0x80010000-0x8001807c; it builds from the same decomp/src/resident.
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/resident/slus_006.69.yaml
ORIGINAL := .local/extract/disc2/SLUS_006.69
ORIGINAL_SHA256 := 3246e15f4040305b280adae06bc7bb908ee882794183bec9fc23e71d85c19c35
BUILD := .local/decomp/build/resident2
IMAGE := .local/decomp/build/SLUS_006.69
LINKER_SCRIPT := .local/decomp/resident2/slus_006.69.ld
LINKER_EXTRA := .local/decomp/resident2/undefined_syms_auto.txt .local/decomp/resident2/undefined_funcs_auto.txt decomp/targets/resident/link.ld
OBJCOPY_FLAGS := --gap-fill 0 --pad-to 0x4a000
SOURCE_DIRS := decomp/src/resident
CLASSIFICATION := decomp/targets/resident/classification.txt
# The heap unit addresses its small globals through $gp.
GP_heap := 8
# Data-only units holding initialized small globals that other units address
# absolutely (kernel_settings.c, sprite_settings.c): at -G8 GCC puts them in
# .sdata.
GP_kernel_settings := 8
GP_sprite_settings := 8
# The heap report file unit (80032DCC-80032E7C) owns its file handle as a
# $gp small common but addresses other units' small globals absolutely
# (80032E04's stores of D_800592B8 match only so).
GP_heap_80032DCC := 8
MASPSX_heap_80032DCC := --aspsx-version=2.34 --use-comm-section
EXTERN_heap_80032DCC := absolute
# The sound driver unit is compiled by GCC 2.6.3.
CC_sound := 2.6.3
# The sprite unit (8001C8DC-8002709C) is compiled by the Cygnus CDK GCC
# 2.7.2 (only it reproduces 80021c20's two loads of the stack top, lb then
# lbu, and 80022a70's register choice), assembles positive `li` as `addiu`
# (ASPSX 2.50+; the rest of the game code uses `ori`, the default 2.34) and
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
# The texture-scroll and disc unit (8002709C-8002A260) is compiled by GCC
# 2.6.3 with inline division checks.
CC_main_8002709C := 2.6.3
MASPSX_main_8002709C := --aspsx-version=2.34 --expand-div
# The battle-mode entry owns 8005959C, while its setup flags are owned by
# the following menu-support unit. This split preserves those GP/absolute
# accesses with the same qualified GCC 2.6.3 small-common pipeline.
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
# The CD read callback, stream and model buffer unit (8002A260-8002C3E8)
# is compiled by GCC 2.6.3.
CC_main_8002A260 := 2.6.3
# Embedded media stay user-supplied: INCLUDE_ASSET reads them from ORIGINAL,
# whose file offset 0 (the 2 KiB PS-X EXE header) is VRAM 0x8000F800.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8000F800
