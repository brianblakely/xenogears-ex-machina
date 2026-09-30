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
# The sprite unit (8001C8DC-8002709C) assembles positive `li` as `addiu`
# (ASPSX 2.50+; the rest of the game code uses `ori`, the default 2.34) and
# addresses its small globals through $gp.
GP_sprite := 8
MASPSX_sprite := --aspsx-version=2.79 --use-comm-section
