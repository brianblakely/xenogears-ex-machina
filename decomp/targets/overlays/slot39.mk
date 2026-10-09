# slot39: decoded overlay image at 0x801c5000 (0x25908 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/slot39.yaml
ORIGINAL := .local/extract/overlays/slot39.bin
ORIGINAL_SHA256 := 82f84a24ac1fd1979754e1cc9c861405fb59ce95dd78db00a76f00c8f1bd177d
BUILD := .local/decomp/build/slot39
IMAGE := .local/decomp/build/slot39.bin
LINKER_SCRIPT := .local/decomp/slot39/slot39.ld
LINKER_EXTRA := .local/decomp/slot39/undefined_syms_auto.txt .local/decomp/slot39/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/slot39
# The reasons its included objects stay original (coverage class included).
CLASSIFICATION := decomp/targets/overlays/slot39.classification.txt
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:39 2:34 1:3957 2:3952
# INCLUDE_ORIGINAL reads original data from ORIGINAL, whose file offset 0 is
# VRAM 0x801C5000.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x801C5000
