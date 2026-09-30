# Disc 2 resident boot program. Identical to Disc 1 outside the embedded disc
# index at 0x80010000-0x8001807c; it builds from the same decomp/src/resident.
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
# The resident game code outside 0x8001c76c-0x8002709c is assembled with `li`
# of a positive constant expanded to `ori` (no `addiu` form there): maspsx's
# ASPSX-before-2.50 behaviour. The middle unit uses `addiu` (ASPSX 2.50+).
override MASPSXFLAGS := --aspsx-version=2.34
