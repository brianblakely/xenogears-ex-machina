# ovl3383: decoded overlay image at 0x801fc000 (0x5e4 bytes).
# Built by the Cygnus CDK GCC 2.7.2 with a later ASPSX (positive li as
# addiu), like the other 0x801fc000 battle modules (docs/matching.md).
CC_VERSION := 2.7.2-cdk
MASPSX_FLAGS := --aspsx-version=2.56
SPLAT_CONFIG := decomp/targets/overlays/ovl3383.yaml
ORIGINAL := .local/extract/overlays/ovl3383.bin
ORIGINAL_SHA256 := 0d975d1f4e0aea2ebaf942bc66b00670d782ed12b3efec9b2c6b062f63504c05
BUILD := .local/decomp/build/ovl3383
IMAGE := .local/decomp/build/ovl3383.bin
LINKER_SCRIPT := .local/decomp/ovl3383/ovl3383.ld
LINKER_EXTRA := .local/decomp/ovl3383/undefined_syms_auto.txt .local/decomp/ovl3383/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3383
