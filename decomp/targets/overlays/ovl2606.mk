# ovl2606: decoded overlay image at 0x801e0000 (0x1ddc bytes).
SPLAT_CONFIG := decomp/targets/overlays/ovl2606.yaml
ORIGINAL := .local/extract/overlays/ovl2606.bin
ORIGINAL_SHA256 := ca291c99b6277615629846aee67b64e918f9b330ee008ac510816b2b39cc8760
BUILD := .local/decomp/build/ovl2606
IMAGE := .local/decomp/build/ovl2606.bin
LINKER_SCRIPT := .local/decomp/ovl2606/ovl2606.ld
LINKER_EXTRA := .local/decomp/ovl2606/undefined_syms_auto.txt .local/decomp/ovl2606/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2606
# Its code expands `li` to `ori` (positive) / `addiu` (negative): ASPSX
# behaviour before 2.50 in maspsx's model, not the resident's 2.79.
override MASPSXFLAGS := --aspsx-version=2.34
