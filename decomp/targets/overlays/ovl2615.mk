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
# Division checks are expanded inline (break 7 / break 6) in this image.
MASPSX_FLAGS := --aspsx-version=2.34 --expand-div
