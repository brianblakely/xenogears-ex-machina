# ovl3387: decoded overlay image at 0x801fc000 (0xe4c bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/ovl3387.yaml
ORIGINAL := .local/extract/overlays/ovl3387.bin
ORIGINAL_SHA256 := a647bafc3608e0d86dd5f72d6f867aef0e4004bc1f3b2a2079d5392dfce84d01
BUILD := .local/decomp/build/ovl3387
IMAGE := .local/decomp/build/ovl3387.bin
LINKER_SCRIPT := .local/decomp/ovl3387/ovl3387.ld
LINKER_EXTRA := .local/decomp/ovl3387/undefined_syms_auto.txt .local/decomp/ovl3387/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/ovl3387
