# ovl3087: decoded overlay image at 0x801e5000 (0x4c3c bytes).
SPLAT_CONFIG := decomp/targets/overlays/ovl3087.yaml
ORIGINAL := .local/extract/overlays/ovl3087.bin
ORIGINAL_SHA256 := 64668d85bf48dea46cf6d5f04b38e38ca887877cfef53961f0c26cd2d363b670
BUILD := .local/decomp/build/ovl3087
IMAGE := .local/decomp/build/ovl3087.bin
LINKER_SCRIPT := .local/decomp/ovl3087/ovl3087.ld
LINKER_EXTRA := .local/decomp/ovl3087/undefined_syms_auto.txt .local/decomp/ovl3087/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3087
# Its code expands `li` to `ori` (positive) / `addiu` (negative): ASPSX
# behaviour before 2.50 in maspsx's model, not the resident's 2.79.
override MASPSXFLAGS := --aspsx-version=2.34
