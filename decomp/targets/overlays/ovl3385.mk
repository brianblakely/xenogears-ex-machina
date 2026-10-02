# ovl3385: decoded overlay image at 0x801fc000 (0x814 bytes).
# Built by the Cygnus CDK GCC 2.7.2 with a later ASPSX (positive li as
# addiu), like the other 0x801fc000 battle modules (docs/matching.md).
CC_VERSION := 2.7.2-cdk
MASPSX_FLAGS := --aspsx-version=2.56
SPLAT_CONFIG := decomp/targets/overlays/ovl3385.yaml
ORIGINAL := .local/extract/overlays/ovl3385.bin
ORIGINAL_SHA256 := 6b53e33996d44b095b1264a6301956672547550c877a9589b50f05460b8c54ab
BUILD := .local/decomp/build/ovl3385
IMAGE := .local/decomp/build/ovl3385.bin
LINKER_SCRIPT := .local/decomp/ovl3385/ovl3385.ld
LINKER_EXTRA := .local/decomp/ovl3385/undefined_syms_auto.txt .local/decomp/ovl3385/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3385
