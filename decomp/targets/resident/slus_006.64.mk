# Disc 1 resident boot program (PS-X EXE, text 0x80010000, entry 0x80019524).
# Code and data after the embedded disc index are identical to Disc 2's
# SLUS_006.69; only 0x80010000-0x8001807c differs.
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/resident/slus_006.64.yaml
ORIGINAL := .local/extract/disc1/SLUS_006.64
ORIGINAL_SHA256 := dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119
BUILD := .local/decomp/build/resident
IMAGE := .local/decomp/build/SLUS_006.64
LINKER_SCRIPT := .local/decomp/resident/slus_006.64.ld
LINKER_EXTRA := .local/decomp/resident/undefined_syms_auto.txt .local/decomp/resident/undefined_funcs_auto.txt
# The header declares t_size 0x49800: the file is padded with zeros from the
# end of .data (0x800592bc) to the next 2048-byte boundary (file 0x4a000).
OBJCOPY_FLAGS := --gap-fill 0 --pad-to 0x4a000
SOURCE_DIRS := decomp/src/resident
CLASSIFICATION := decomp/targets/resident/classification.txt
# Disc 2 shares this assembly directory; a fresh split regenerates both.
SPLIT_ALSO := decomp/targets/resident/slus_006.69.yaml
# The heap unit addresses its small globals through $gp.
GP_heap := 8
# The sound driver unit is compiled by GCC 2.6.3.
CC_sound := 2.6.3
# The sprite unit (8001C8DC-8002709C) assembles positive `li` as `addiu`
# (ASPSX 2.50+; the rest of the game code uses `ori`, the default 2.34) and
# addresses its small globals through $gp.
GP_sprite := 8
MASPSX_sprite := --aspsx-version=2.79 --use-comm-section
# The texture-scroll and disc unit (8002709C-8002A260) is compiled by GCC
# 2.6.3 with inline division checks.
CC_main_8002709C := 2.6.3
MASPSX_main_8002709C := --aspsx-version=2.34 --expand-div
