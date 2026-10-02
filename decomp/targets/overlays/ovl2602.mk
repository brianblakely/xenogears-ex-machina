# ovl2602: decoded overlay image at 0x801c5000 (0x140a0 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2602.yaml
ORIGINAL := .local/extract/overlays/ovl2602.bin
ORIGINAL_SHA256 := 5a08a22f2c5cc1d131b33a944d3167a2ac460c5f9f17f2dcf94dd8860158a25c
BUILD := .local/decomp/build/ovl2602
IMAGE := .local/decomp/build/ovl2602.bin
LINKER_SCRIPT := .local/decomp/ovl2602/ovl2602.ld
LINKER_EXTRA := .local/decomp/ovl2602/undefined_syms_auto.txt .local/decomp/ovl2602/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2602
