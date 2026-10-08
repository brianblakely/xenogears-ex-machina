# field: decoded overlay image at 0x8006faf0 (0x3fafe bytes).
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/field.yaml
ORIGINAL := .local/extract/overlays/field.bin
ORIGINAL_SHA256 := 38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc
BUILD := .local/decomp/build/field
IMAGE := .local/decomp/build/field.bin
LINKER_SCRIPT := .local/decomp/field/field.ld
LINKER_EXTRA := .local/decomp/field/undefined_syms_auto.txt .local/decomp/field/undefined_funcs_auto.txt decomp/targets/overlays/field.data.ld
SOURCE_DIRS := decomp/src/field
# The data ends at 0x800af5e8 (file 0x3faf8), where the resident's mode table
# starts the field BSS. The original packer then appended zero literal tokens
# until its last group held eight and recorded the padded length: the plain
# encoding of the 0x3faf8 linked bytes plus six zero literals is the disc
# stream. The six zero bytes are reproduced here as file padding.
OBJCOPY_FLAGS := --gap-fill 0 --pad-to 0x3fafe
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:36 2:31
