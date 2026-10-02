# ovl2601: decoded overlay image at 0x801c5000 (0xd264 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2601.yaml
ORIGINAL := .local/extract/overlays/ovl2601.bin
ORIGINAL_SHA256 := fd894b5113e4e12bb6aba5f775f1036744eb7f98ff2d1462ee10673868370c2a
BUILD := .local/decomp/build/ovl2601
IMAGE := .local/decomp/build/ovl2601.bin
LINKER_SCRIPT := .local/decomp/ovl2601/ovl2601.ld
LINKER_EXTRA := .local/decomp/ovl2601/undefined_syms_auto.txt .local/decomp/ovl2601/undefined_funcs_auto.txt decomp/targets/overlays/ovl2601.resident.ld
SOURCE_DIRS := decomp/src/ovl2601
