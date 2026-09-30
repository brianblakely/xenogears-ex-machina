# debug2611: decoded overlay image at 0x80280000 (0x20f0 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/debug2611.yaml
ORIGINAL := .local/extract/overlays/debug2611.bin
ORIGINAL_SHA256 := f0e10e1880c209d56fe9f22a25e9fa223ee6d6c238c0cb15b529eb151704429b
BUILD := .local/decomp/build/debug2611
IMAGE := .local/decomp/build/debug2611.bin
LINKER_SCRIPT := .local/decomp/debug2611/debug2611.ld
LINKER_EXTRA := .local/decomp/debug2611/undefined_syms_auto.txt .local/decomp/debug2611/undefined_funcs_auto.txt decomp/targets/overlays/debug2611.resident.ld
SOURCE_DIRS := decomp/src/debug2611
# Positive li assembles as addiu in the tools (ASPSX 2.50 or later); the
# state pages unit keeps it as ori (ASPSX 2.34).
MASPSX_FLAGS := --aspsx-version=2.56
MASPSX_pages := --aspsx-version=2.34
CC_pages := 2.6.3
