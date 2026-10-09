# debug595: decoded overlay image at 0x80280000 (0x61c8 bytes).
# GCC 2.7.2: only 2.7.2 reproduces 80284fb4's product in $t0, 80281b90's
# operand order and 80284ea4's word load of D_80065858 (2.6.3 loads a
# halfword); the other 26 functions build the same code under 2.6.3.
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/debug595.yaml
ORIGINAL := .local/extract/overlays/debug595.bin
ORIGINAL_SHA256 := c123c880e71cfa0448a6ea4d9961bb0f1c0e2ced0595d9fa5eefc4a913248048
BUILD := .local/decomp/build/debug595
IMAGE := .local/decomp/build/debug595.bin
LINKER_SCRIPT := .local/decomp/debug595/debug595.ld
LINKER_EXTRA := .local/decomp/debug595/undefined_syms_auto.txt .local/decomp/debug595/undefined_funcs_auto.txt decomp/targets/overlays/debug595.field.ld decomp/targets/overlays/debug595.resident.ld
SOURCE_DIRS := decomp/src/debug595
