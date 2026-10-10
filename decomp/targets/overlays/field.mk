# field: decoded overlay image at 0x8006faf0 (0x3fafe bytes).
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/field.yaml
ORIGINAL := .local/extract/overlays/field.bin
ORIGINAL_SHA256 := 38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc
# Its uninitialized data ends at 800c4270: the resident's mode table entry 1
# (8001809c) clears the words after 800af5e4 through 800c426c (80019560).
# field.bss.ld asserts that the linked .bss is this span.
BSS_END := 0x800C4270
# Mode 1 enters func_80077E88 (main.c mode_table); after every target links,
# tools/cross_image.py compares the entry and the BSS bounds with this link.
MODE := 1
MODE_ENTRY := func_80077E88
BUILD := .local/decomp/build/field
IMAGE := .local/decomp/build/field.bin
LINKER_SCRIPT := .local/decomp/field/field.ld
LINKER_EXTRA := .local/decomp/field/undefined_syms_auto.txt .local/decomp/field/undefined_funcs_auto.txt decomp/targets/overlays/field.bss.ld
SOURCE_DIRS := decomp/src/field
CLASSIFICATION := decomp/targets/overlays/field.classification.txt
# INCLUDE_ASSET reads the movie sound timelines from ORIGINAL, whose file
# offset 0 is VRAM 0x8006FAF0.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8006FAF0
# The program ends at 0x800af5e8 (file 0x3faf8), where the resident's mode
# table starts the field BSS. The original packer then appended zero literal
# tokens until its last group held eight and recorded the padded length: the
# plain encoding of the program plus six zero literals is the disc stream
# (tools/packed_container.py). The six bytes follow the link as file padding.
PACKER_TAIL := 6
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:36 2:31
