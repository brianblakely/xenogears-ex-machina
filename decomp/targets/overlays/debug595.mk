# debug595: decoded overlay image at 0x80280000 (0x61c8 bytes).
SPLAT_CONFIG := decomp/targets/overlays/debug595.yaml
ORIGINAL := .local/extract/overlays/debug595.bin
ORIGINAL_SHA256 := c123c880e71cfa0448a6ea4d9961bb0f1c0e2ced0595d9fa5eefc4a913248048
BUILD := .local/decomp/build/debug595
IMAGE := .local/decomp/build/debug595.bin
LINKER_SCRIPT := .local/decomp/debug595/debug595.ld
LINKER_EXTRA := .local/decomp/debug595/undefined_syms_auto.txt .local/decomp/debug595/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/debug595
