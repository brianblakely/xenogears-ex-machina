# ovl3386: decoded overlay image at 0x801fc000 (0x784 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl3386.yaml
ORIGINAL := .local/extract/overlays/ovl3386.bin
ORIGINAL_SHA256 := f47e5565d4b6159c48bbe1ca09c4c9ca12948d65d1324739f120e5fe4cd80d32
BUILD := .local/decomp/build/ovl3386
IMAGE := .local/decomp/build/ovl3386.bin
LINKER_SCRIPT := .local/decomp/ovl3386/ovl3386.ld
LINKER_EXTRA := .local/decomp/ovl3386/undefined_syms_auto.txt .local/decomp/ovl3386/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3386
