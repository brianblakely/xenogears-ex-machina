# debug2611: decoded overlay image at 0x80280000 (0x20f0 bytes).
CC_VERSION := 2.7.2-cdk
SPLAT_CONFIG := decomp/targets/overlays/debug2611.yaml
ORIGINAL := .local/extract/overlays/debug2611.bin
ORIGINAL_SHA256 := f0e10e1880c209d56fe9f22a25e9fa223ee6d6c238c0cb15b529eb151704429b
BUILD := .local/decomp/build/debug2611
IMAGE := .local/decomp/build/debug2611.bin
LINKER_SCRIPT := .local/decomp/debug2611/debug2611.ld
LINKER_EXTRA := .local/decomp/debug2611/undefined_syms_auto.txt .local/decomp/debug2611/undefined_funcs_auto.txt decomp/targets/overlays/debug2611.resident.ld
SOURCE_DIRS := decomp/src/debug2611
# The tools unit (80280844-end) is Cygnus CDK GCC 2.7.2 with a later ASPSX
# (positive li as addiu), like the 0x801fc000 battle modules; the state pages
# unit keeps li as ori (ASPSX 2.34). GCC 2.6.3 and 2.7.2 build identical
# objects for the pages unit (2.7.2-cdk does not), so its bytes do not decide
# between them; its setting follows the GCC 2.6.3 battle overlay whose state
# it prints.
MASPSX_FLAGS := --aspsx-version=2.56
MASPSX_pages := --aspsx-version=2.34
CC_pages := 2.6.3
