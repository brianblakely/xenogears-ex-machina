# ovl2596: decoded overlay image at 0x801de000 (0x6500 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2596.yaml
ORIGINAL := .local/extract/overlays/ovl2596.bin
ORIGINAL_SHA256 := f474fd482506b89eaff9601d5958a1c867d35a4ad11c1f58f0ecef630008a0c8
BUILD := .local/decomp/build/ovl2596
IMAGE := .local/decomp/build/ovl2596.bin
LINKER_SCRIPT := .local/decomp/ovl2596/ovl2596.ld
LINKER_EXTRA := .local/decomp/ovl2596/undefined_syms_auto.txt .local/decomp/ovl2596/undefined_funcs_auto.txt decomp/targets/overlays/ovl2596.resident.ld
SOURCE_DIRS := decomp/src/ovl2596
# INCLUDE_ORIGINAL reads original data from ORIGINAL, whose file offset 0 is
# VRAM 0x801DE000.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x801DE000
# The original assembler gave each uninitialized variable a slot of whole
# words (decomp/Makefile, BSS).
BSS := slots
