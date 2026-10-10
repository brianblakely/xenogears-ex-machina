# worldmap: decoded overlay image at 0x8006faf0 (0x2c0c6 bytes).
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/worldmap.yaml
ORIGINAL := .local/extract/overlays/worldmap.bin
ORIGINAL_SHA256 := 4c15fd32b3a03d7cd5ea4403dcaabc70abaf99b6aaca65d63d6866803edaac70
# Its uninitialized data ends at 8009d810: the resident's mode table entry 3
# (800180bc) clears the words after 8009bbb0 through 8009d80c (80019560).
BSS_END := 0x8009D810
# Mode 3 enters func_80070CFC (main.c mode_table); after every target links,
# tools/cross_image.py compares the entry and the BSS bounds with this link.
MODE := 3
MODE_ENTRY := func_80070CFC
BUILD := .local/decomp/build/worldmap
IMAGE := .local/decomp/build/worldmap.bin
LINKER_SCRIPT := .local/decomp/worldmap/worldmap.ld
LINKER_EXTRA := .local/decomp/worldmap/undefined_syms_auto.txt .local/decomp/worldmap/undefined_funcs_auto.txt decomp/targets/overlays/worldmap.resident.ld decomp/targets/overlays/worldmap.data.ld
# worldmap.data.ld names the shared read list's first destination member
# D_8009D3FC, from which two loaders pass the list (WORLD_READ_LIST): formed
# from D_8009D3F8 itself, the constant lets cse store the list's first entry
# through the argument register in func_80071FEC. verify accepts it as a view.
LINK_VIEWS := decomp/targets/overlays/worldmap.data.ld
SOURCE_DIRS := decomp/src/worldmap
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:37 2:32
# Assembled with ASPSX >= 2.50 behaviour: small constants load with addiu
# (80071a50 `addiu $v0,$zero,1`); ori appears only for values >= 0x8000.
# The maspsx setting is 2.79; 2.56 builds identical objects for every unit.
# Every division carries inline zero/overflow checks (all 24 div, e.g. 800935dc).
MASPSX_FLAGS := --aspsx-version=2.79 --expand-div
CLASSIFICATION := decomp/targets/overlays/worldmap.classification.txt
# INCLUDE_ASSET reads the embedded scripts from ORIGINAL, whose file offset 0
# is VRAM 0x8006FAF0.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8006FAF0
# The program ends at 0x8009bbb4 (file 0x2c0c4), where the resident's mode
# table starts the BSS. The original packer appended zero literal tokens
# until its last group held eight and recorded the padded length: the plain
# encoding of the program plus two zero literals is the disc stream
# (tools/packed_container.py). The two bytes follow the link as file padding.
PACKER_TAIL := 2
