# menu: decoded overlay image at 0x8006faf0 (0x22e69 bytes).
SPLAT_CONFIG := decomp/targets/overlays/menu.yaml
ORIGINAL := .local/extract/overlays/menu.bin
ORIGINAL_SHA256 := 3e6df915e9c7f05f5fb997cb331392f1333e867dfb2628cd65e5e1ea1575646e
BUILD := .local/decomp/build/menu
IMAGE := .local/decomp/build/menu.bin
LINKER_SCRIPT := .local/decomp/menu/menu.ld
LINKER_EXTRA := .local/decomp/menu/undefined_syms_auto.txt .local/decomp/menu/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/menu
# This unit's assembler expanded `li` itself: every positive load immediate is
# `ori rt, $zero, imm` (1454 in the image, none as addiu; negatives are addiu).
# GNU as emits addiu, and maspsx expands li only below its 2.50 version switch.
override MASPSXFLAGS := --aspsx-version=2.49
