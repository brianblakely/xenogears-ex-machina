# battle: decoded overlay image at 0x8006faf0 (0x53f80 bytes).
SPLAT_CONFIG := decomp/targets/overlays/battle.yaml
ORIGINAL := .local/extract/overlays/battle.bin
ORIGINAL_SHA256 := 1830b4ef1fe37129972fc310dfad534f8161d6c0b123e74254c3711334a3e291
BUILD := .local/decomp/build/battle
IMAGE := .local/decomp/build/battle.bin
LINKER_SCRIPT := .local/decomp/battle/battle.ld
LINKER_EXTRA := .local/decomp/battle/undefined_syms_auto.txt .local/decomp/battle/undefined_funcs_auto.txt
SOURCE_DIRS := decomp/src/battle
# battle.c's code expands `li` to `ori $r, $zero, imm` (maspsx: ASPSX before
# 2.50) and indexed symbol accesses to `lui/addu $at` without addiu (2.30 or
# later). maspsx behaves identically for any version in [2.30, 2.50); 2.30
# selects that behaviour and is not a claim about the exact original ASPSX.
$(ROOT)/$(BUILD)/decomp/src/battle/battle.o: MASPSXFLAGS := --aspsx-version=2.30
