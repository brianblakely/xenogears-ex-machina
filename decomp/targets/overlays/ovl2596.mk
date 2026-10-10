# ovl2596: decoded overlay image at 0x801de000 (0x6500 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2596.yaml
ORIGINAL := .local/extract/overlays/ovl2596.bin
ORIGINAL_SHA256 := f474fd482506b89eaff9601d5958a1c867d35a4ad11c1f58f0ecef630008a0c8
BUILD := .local/decomp/build/ovl2596
IMAGE := .local/decomp/build/ovl2596.bin
LINKER_SCRIPT := .local/decomp/ovl2596/ovl2596.ld
LINKER_EXTRA := .local/decomp/ovl2596/undefined_syms_auto.txt .local/decomp/ovl2596/undefined_funcs_auto.txt decomp/targets/overlays/ovl2596.resident.ld
# The result screens (battle_results_screens.c) read battle's nine decimal
# digits by digit position from bases of their own, which ovl2596.resident.ld
# gives as views before battle_decimal_digits: minus_29 from position 31,
# minus_21 from 22, minus_17 from 23, minus_7 from 13 and minus_3 from 9, each
# reaching digits 1-8 only. tools/cross_image.py accepts a view before its
# object only where this lists it.
BASE_VIEWS := battle_decimal_digits_minus_29 battle_decimal_digits_minus_21 battle_decimal_digits_minus_17 battle_decimal_digits_minus_7 battle_decimal_digits_minus_3
SOURCE_DIRS := decomp/src/ovl2596
# The reasons its included objects stay original (coverage class included).
CLASSIFICATION := decomp/targets/overlays/ovl2596.classification.txt
# INCLUDE_ORIGINAL reads original data from ORIGINAL, whose file offset 0 is
# VRAM 0x801DE000.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x801DE000
