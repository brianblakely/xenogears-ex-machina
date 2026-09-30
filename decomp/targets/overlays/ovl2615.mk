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
# battle_loader (801e62e0-801e70e8) and load_modes (801e7f4c-801e95bc) are
# units built by a later compiler than GCC 2.6.3/2.7.2: cc1 itself splits
# symbol addresses into scheduled %hi/%lo pairs (stores through lui into a
# free register instead of $at) and positive li is addiu. Neither qualified
# cc1 emits that, so those units stay assembly until that compiler is
# qualified. stage (801e70e8-801e7f4c) is GCC 2.6.3 again.
