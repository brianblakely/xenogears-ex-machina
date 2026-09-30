# slot39: decoded overlay image at 0x801c5000 (0x25908 bytes).
SPLAT_CONFIG := decomp/targets/overlays/slot39.yaml
ORIGINAL := .local/extract/overlays/slot39.bin
ORIGINAL_SHA256 := 82f84a24ac1fd1979754e1cc9c861405fb59ce95dd78db00a76f00c8f1bd177d
BUILD := .local/decomp/build/slot39
IMAGE := .local/decomp/build/slot39.bin
LINKER_SCRIPT := .local/decomp/slot39/slot39.ld
LINKER_EXTRA := .local/decomp/slot39/undefined_syms_auto.txt .local/decomp/slot39/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/slot39
# The overlay's assembler expands `li` of a positive constant to `ori` (2003
# such loads, none as `addiu`): maspsx's ASPSX-before-2.50 behaviour.
override MASPSXFLAGS := --aspsx-version=2.34
