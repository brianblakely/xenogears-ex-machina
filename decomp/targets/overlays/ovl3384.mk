# ovl3384: decoded overlay image at 0x801fc000 (0xe1c bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl3384.yaml
ORIGINAL := .local/extract/overlays/ovl3384.bin
ORIGINAL_SHA256 := 8c303aab8a88012d916e8d033f2e4cda36f388ecbb3d684a8d4729c1af3a78a3
BUILD := .local/decomp/build/ovl3384
IMAGE := .local/decomp/build/ovl3384.bin
LINKER_SCRIPT := .local/decomp/ovl3384/ovl3384.ld
LINKER_EXTRA := .local/decomp/ovl3384/undefined_syms_auto.txt .local/decomp/ovl3384/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3384
