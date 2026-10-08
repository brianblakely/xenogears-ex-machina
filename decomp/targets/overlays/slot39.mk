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
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:39 2:34 1:3957 2:3952
# The card refresh unit (801C93A8-801C9BCC) is built by GCC 2.6.0 with
# -fno-rerun-cse-after-loop: only that combination reproduces its register
# copy of the initial result and the result's placement in a load delay
# (slot39_801C93A8.c); the neighbouring units break under that setting.
CC_slot39_801C93A8 := 2.6.0
CC1FLAGS_slot39_801C93A8 := -fno-rerun-cse-after-loop
