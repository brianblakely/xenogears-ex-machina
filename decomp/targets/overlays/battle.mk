# battle: decoded overlay image at 0x8006faf0 (0x53f80 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/battle.yaml
ORIGINAL := .local/extract/overlays/battle.bin
ORIGINAL_SHA256 := 1830b4ef1fe37129972fc310dfad534f8161d6c0b123e74254c3711334a3e291
BUILD := .local/decomp/build/battle
IMAGE := .local/decomp/build/battle.bin
LINKER_SCRIPT := .local/decomp/battle/battle.ld
LINKER_EXTRA := .local/decomp/battle/undefined_syms_auto.txt .local/decomp/battle/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/battle
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:38 2:33
