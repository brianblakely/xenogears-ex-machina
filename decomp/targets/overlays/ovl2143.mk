# ovl2143: decoded overlay image at 0x801dc000 (0xc6b4 bytes).
SPLAT_CONFIG := decomp/targets/overlays/ovl2143.yaml
ORIGINAL := .local/extract/overlays/ovl2143.bin
ORIGINAL_SHA256 := 18d35e7640a0763cdea5feaf485b8865854d0831bbf28fb3d86d0d1996f1756f
BUILD := .local/decomp/build/ovl2143
IMAGE := .local/decomp/build/ovl2143.bin
LINKER_SCRIPT := .local/decomp/ovl2143/ovl2143.ld
LINKER_EXTRA := .local/decomp/ovl2143/undefined_syms_auto.txt .local/decomp/ovl2143/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl2143
# Its code expands `li` to `ori` (positive) / `addiu` (negative): ASPSX
# behaviour before 2.50 in maspsx's model, not the resident's 2.79.
override MASPSXFLAGS := --aspsx-version=2.34
