# mdec: decoded overlay image at 0x801d3000 (0x15a1c bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/mdec.yaml
ORIGINAL := .local/extract/overlays/mdec.bin
ORIGINAL_SHA256 := 4606650a38c794ae4b27157d702eeea72d362b9e5c5871f67955c6284af1584b
BUILD := .local/decomp/build/mdec
IMAGE := .local/decomp/build/mdec.bin
LINKER_SCRIPT := .local/decomp/mdec/mdec.ld
LINKER_EXTRA := .local/decomp/mdec/undefined_syms_auto.txt .local/decomp/mdec/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/mdec
CLASSIFICATION := decomp/targets/overlays/mdec.classification.txt
# Division checks (break 7 / break 6) are inline in movie_slice_decoded.
MASPSX_FLAGS := --aspsx-version=2.34 --expand-div
