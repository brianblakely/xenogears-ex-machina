# worldmap: decoded overlay image at 0x8006faf0 (0x2c0c6 bytes).
SPLAT_CONFIG := decomp/targets/overlays/worldmap.yaml
ORIGINAL := .local/extract/overlays/worldmap.bin
ORIGINAL_SHA256 := 4c15fd32b3a03d7cd5ea4403dcaabc70abaf99b6aaca65d63d6866803edaac70
BUILD := .local/decomp/build/worldmap
IMAGE := .local/decomp/build/worldmap.bin
LINKER_SCRIPT := .local/decomp/worldmap/worldmap.ld
LINKER_EXTRA := .local/decomp/worldmap/undefined_syms_auto.txt .local/decomp/worldmap/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/worldmap
