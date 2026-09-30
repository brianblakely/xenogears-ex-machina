# ovl2600: decoded overlay image at 0x801c5000 (0x7134 bytes).
SPLAT_CONFIG := decomp/targets/overlays/ovl2600.yaml
ORIGINAL := .local/extract/overlays/ovl2600.bin
ORIGINAL_SHA256 := 6ce24a96a62ac897e83dff7d0bb4bb22bcafdef11b6d577ef8e1e30533bbba4b
BUILD := .local/decomp/build/ovl2600
IMAGE := .local/decomp/build/ovl2600.bin
LINKER_SCRIPT := .local/decomp/ovl2600/ovl2600.ld
LINKER_EXTRA := .local/decomp/ovl2600/undefined_syms_auto.txt .local/decomp/ovl2600/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2600
