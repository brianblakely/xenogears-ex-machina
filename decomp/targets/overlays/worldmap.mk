# worldmap: decoded overlay image at 0x8006faf0 (0x2c0c6 bytes).
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/worldmap.yaml
ORIGINAL := .local/extract/overlays/worldmap.bin
ORIGINAL_SHA256 := 4c15fd32b3a03d7cd5ea4403dcaabc70abaf99b6aaca65d63d6866803edaac70
BUILD := .local/decomp/build/worldmap
IMAGE := .local/decomp/build/worldmap.bin
LINKER_SCRIPT := .local/decomp/worldmap/worldmap.ld
LINKER_EXTRA := .local/decomp/worldmap/undefined_syms_auto.txt .local/decomp/worldmap/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/worldmap
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:37 2:32
# Assembled with ASPSX >= 2.50 behaviour: small constants load with addiu
# (80071a50 `addiu $v0,$zero,1`); ori appears only for values >= 0x8000.
# Every division carries inline zero/overflow checks (all 24 div, e.g. 800935dc).
MASPSX_FLAGS := --aspsx-version=2.79 --expand-div
CLASSIFICATION := decomp/targets/overlays/worldmap.classification.txt
