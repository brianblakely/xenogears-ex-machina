# ovl3381: decoded overlay image at 0x801fc000 (0x728 bytes).
# Built by the Cygnus CDK GCC 2.7.2 with a later ASPSX (positive li as
# addiu), like the other 0x801fc000 battle modules (docs/matching.md).
CC_VERSION := 2.7.2-cdk
MASPSX_FLAGS := --aspsx-version=2.56
SPLAT_CONFIG := decomp/targets/overlays/ovl3381.yaml
ORIGINAL := .local/extract/overlays/ovl3381.bin
ORIGINAL_SHA256 := 34d3343fb30a1c76327b53504c6dd12e03e6d1c8d96e66adc4509fb98c605b43
BUILD := .local/decomp/build/ovl3381
IMAGE := .local/decomp/build/ovl3381.bin
LINKER_SCRIPT := .local/decomp/ovl3381/ovl3381.ld
LINKER_EXTRA := .local/decomp/ovl3381/undefined_syms_auto.txt .local/decomp/ovl3381/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3381
