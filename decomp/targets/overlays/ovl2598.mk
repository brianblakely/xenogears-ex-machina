# ovl2598: decoded overlay image at 0x801c5000 (0x659c bytes).
SPLAT_CONFIG := decomp/targets/overlays/ovl2598.yaml
ORIGINAL := .local/extract/overlays/ovl2598.bin
ORIGINAL_SHA256 := b0aaf001bb97fde7f8df89f2d7de0906713a155f54097897dc9a9482117b736b
BUILD := .local/decomp/build/ovl2598
IMAGE := .local/decomp/build/ovl2598.bin
LINKER_SCRIPT := .local/decomp/ovl2598/ovl2598.ld
LINKER_EXTRA := .local/decomp/ovl2598/undefined_syms_auto.txt .local/decomp/ovl2598/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2598
# Every small constant load is `ori rt, $zero, imm` (262 in this image, no
# `addiu rt, $zero, imm`): the assembler expanded `li` itself, which maspsx
# models for ASPSX before 2.50. 2.30 selects that behaviour; nothing in this
# image yet distinguishes the versions within it.
override MASPSXFLAGS := --aspsx-version=2.30
