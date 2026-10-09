# movie: decoded overlay image at 0x8006faf0 (0x7453 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/movie.yaml
ORIGINAL := .local/extract/overlays/movie.bin
ORIGINAL_SHA256 := 50e1a9d9e08b90eed0c2da1c289507e71cbf51749893a92f457ce79a59701f9a
BUILD := .local/decomp/build/movie
IMAGE := .local/decomp/build/movie.bin
LINKER_SCRIPT := .local/decomp/movie/movie.ld
LINKER_EXTRA := .local/decomp/movie/undefined_syms_auto.txt .local/decomp/movie/undefined_funcs_auto.txt decomp/targets/overlays/movie.bss.ld
SOURCE_DIRS := decomp/src/movie
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:40 2:35
# Division checks (break 7 / break 6) are inline in the menu drawing code.
MASPSX_FLAGS := --aspsx-version=2.34 --expand-div
# The program ends at 0x80076f3c (file 0x744c), where the resident's mode
# table starts the BSS. The original packer appended zero literal tokens
# until its last group held eight and recorded the padded length: the plain
# encoding of the program plus seven zero literals is the disc stream
# (tools/packed_container.py). The seven bytes follow the link as file padding.
PACKER_TAIL := 7
