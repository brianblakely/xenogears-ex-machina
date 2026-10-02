# Disc 2 resident boot program. Identical to Disc 1 outside the embedded disc
# index at 0x80010000-0x8001807c; it builds from the same decomp/src/resident.
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/resident/slus_006.69.yaml
ORIGINAL := .local/extract/disc2/SLUS_006.69
ORIGINAL_SHA256 := 3246e15f4040305b280adae06bc7bb908ee882794183bec9fc23e71d85c19c35
BUILD := .local/decomp/build/resident2
IMAGE := .local/decomp/build/SLUS_006.69
LINKER_SCRIPT := .local/decomp/resident2/slus_006.69.ld
LINKER_EXTRA := .local/decomp/resident2/undefined_syms_auto.txt .local/decomp/resident2/undefined_funcs_auto.txt
OBJCOPY_FLAGS := --gap-fill 0 --pad-to 0x4a000
SOURCE_DIRS := decomp/src/resident
CLASSIFICATION := decomp/targets/resident/classification.txt
# The heap unit addresses its small globals through $gp.
GP_heap := 8
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
# The second (80022090-800248D4) and third (800248D4-8002709C) sprite
# units are built the same way.
CC_sprite_80022090 := 2.7.2-cdk
GP_sprite_80022090 := 8
MASPSX_sprite_80022090 := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite_80022090 := absolute
CC_sprite_800248D4 := 2.7.2-cdk
GP_sprite_800248D4 := 8
MASPSX_sprite_800248D4 := --aspsx-version=2.79 --use-comm-section
EXTERN_sprite_800248D4 := absolute
# The texture-scroll and disc unit (8002709C-8002A260) is compiled by GCC
# 2.6.3 with inline division checks.
CC_main_8002709C := 2.6.3
MASPSX_main_8002709C := --aspsx-version=2.34 --expand-div
# The menu-support unit (8001B6C4-8001C8DC) is compiled by GCC 2.6.3; its
# $gp accesses (8001B6C4-8001BBAC) stay assembly: they need small data the
# unit defines, which cc1 -G8 cannot express here (all small externs would
# become $gp-relative).
CC_main_8001B6C4 := 2.6.3
# The CD read callback, stream and model buffer unit (8002A260-8002C3E8)
# is compiled by GCC 2.6.3.
CC_main_8002A260 := 2.6.3
