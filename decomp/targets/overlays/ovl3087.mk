# ovl3087: decoded overlay image at 0x801e5000 (0x4c3c bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl3087.yaml
ORIGINAL := .local/extract/overlays/ovl3087.bin
ORIGINAL_SHA256 := 64668d85bf48dea46cf6d5f04b38e38ca887877cfef53961f0c26cd2d363b670
BUILD := .local/decomp/build/ovl3087
IMAGE := .local/decomp/build/ovl3087.bin
LINKER_SCRIPT := .local/decomp/ovl3087/ovl3087.ld
LINKER_EXTRA := .local/decomp/ovl3087/undefined_syms_auto.txt .local/decomp/ovl3087/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3087
# 801e93e8-801e9b58 (script_actor) is a 2.7.2-cdk unit (docs/matching.md):
# %hi bases kept in registers, positive li as addiu (ASPSX >= 2.56), load
# delay nops and unfilled epilogue jr slots; neither 2.6.3 nor 2.7.2
# reproduces its functions, 2.7.2-cdk reproduces all but 801e9700.
CC_script_actor := 2.7.2-cdk
MASPSX_script_actor := --aspsx-version=2.56
