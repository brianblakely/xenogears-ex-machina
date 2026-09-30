# movie: decoded overlay image at 0x8006faf0 (0x7453 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/movie.yaml
ORIGINAL := .local/extract/overlays/movie.bin
ORIGINAL_SHA256 := 50e1a9d9e08b90eed0c2da1c289507e71cbf51749893a92f457ce79a59701f9a
BUILD := .local/decomp/build/movie
IMAGE := .local/decomp/build/movie.bin
LINKER_SCRIPT := .local/decomp/movie/movie.ld
LINKER_EXTRA := .local/decomp/movie/undefined_syms_auto.txt .local/decomp/movie/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/movie
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:40 2:35
# Division checks (break 7 / break 6) are inline in the menu drawing code.
MASPSX_FLAGS := --aspsx-version=2.34 --expand-div
