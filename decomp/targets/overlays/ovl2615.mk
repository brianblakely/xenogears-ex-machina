# ovl2615: decoded overlay image at 0x801e4000 (0x56c0 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl2615.yaml
ORIGINAL := .local/extract/overlays/ovl2615.bin
ORIGINAL_SHA256 := 4300fdd99d19805a7a68ded0b21db49e1261688e90bf58e10dead9b978b52885
BUILD := .local/decomp/build/ovl2615
IMAGE := .local/decomp/build/ovl2615.bin
LINKER_SCRIPT := .local/decomp/ovl2615/ovl2615.ld
LINKER_EXTRA := .local/decomp/ovl2615/undefined_syms_auto.txt .local/decomp/ovl2615/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2615
# The reasons its included objects stay original (coverage class included).
CLASSIFICATION := decomp/targets/overlays/ovl2615.classification.txt
# Division checks are expanded inline (break 7 / break 6) in this image.
MASPSX_FLAGS := --aspsx-version=2.34 --expand-div
# battle_loader (801e62e0-801e70e8), load_modes (801e7f4c-801e8964) and
# burst_modes (801e8964-801e95bc) are Cygnus CDK GCC 2.7.2 units with a later
# ASPSX (positive li as addiu), like the 0x801fc000 battle modules; stage
# (801e70e8-801e7f4c) is GCC 2.6.3 again. burst_modes was split from
# load_modes: its jump table (0x34) follows load_modes' (0x20) at 4 mod 8
# with no pad, so it starts a new unit (docs/matching.md, jump tables).
CC_battle_loader := 2.7.2-cdk
CC_load_modes := 2.7.2-cdk
CC_burst_modes := 2.7.2-cdk
MASPSX_battle_loader := --aspsx-version=2.56
MASPSX_load_modes := --aspsx-version=2.56
MASPSX_burst_modes := --aspsx-version=2.56
# INCLUDE_ORIGINAL reads original data from ORIGINAL, whose file offset 0 is
# VRAM 0x801E4000.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x801E4000
