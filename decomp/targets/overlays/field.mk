# field: decoded overlay image at 0x8006faf0 (0x3fafe bytes).
SPLAT_CONFIG := decomp/targets/overlays/field.yaml
ORIGINAL := .local/extract/overlays/field.bin
ORIGINAL_SHA256 := 38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc
BUILD := .local/decomp/build/field
IMAGE := .local/decomp/build/field.bin
LINKER_SCRIPT := .local/decomp/field/field.ld
LINKER_EXTRA := .local/decomp/field/undefined_syms_auto.txt .local/decomp/field/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/field
