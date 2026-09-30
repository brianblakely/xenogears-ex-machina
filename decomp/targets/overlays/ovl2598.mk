# ovl2598: decoded overlay image at 0x801c5000 (0x659c bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2598.yaml
ORIGINAL := .local/extract/overlays/ovl2598.bin
ORIGINAL_SHA256 := b0aaf001bb97fde7f8df89f2d7de0906713a155f54097897dc9a9482117b736b
BUILD := .local/decomp/build/ovl2598
IMAGE := .local/decomp/build/ovl2598.bin
LINKER_SCRIPT := .local/decomp/ovl2598/ovl2598.ld
LINKER_EXTRA := .local/decomp/ovl2598/undefined_syms_auto.txt .local/decomp/ovl2598/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2598
