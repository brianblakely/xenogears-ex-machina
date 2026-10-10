# battle: decoded overlay image at 0x8006faf0 (0x53f80 bytes).
CC_VERSION := 2.6.3
SPLAT_CONFIG := decomp/targets/overlays/battle.yaml
ORIGINAL := .local/extract/overlays/battle.bin
ORIGINAL_SHA256 := 1830b4ef1fe37129972fc310dfad534f8161d6c0b123e74254c3711334a3e291
# Its uninitialized data ends at 800d39f4: the resident's mode table entry 2
# (800180ac) clears the words after 800c3a6c through 800d39f0 (80019560).
# battle.data.ld asserts that the linked .bss is this span.
BSS_END := 0x800D39F4
# Mode 2 (main.c mode_table) enters resident code (mode_run_battle) and clears
# this BSS; after every target links, tools/cross_image.py compares the
# bounds with this link.
MODE := 2
BUILD := .local/decomp/build/battle
IMAGE := .local/decomp/build/battle.bin
LINKER_SCRIPT := .local/decomp/battle/battle.ld
LINKER_EXTRA := .local/decomp/battle/undefined_syms_auto.txt .local/decomp/battle/undefined_funcs_auto.txt decomp/targets/overlays/battle.resident.ld decomp/targets/overlays/battle.data.ld
SOURCE_DIRS := decomp/src/battle
# battle.data.ld names parts of C objects, each from its object's linked
# address: members of the commons that the units address by names and views
# of their own, and the timer and combo step tables from before them, where
# only a declared array gives GCC 2.6.3's code. verify accepts those names
# inside the image and .bss as views (decomp/Makefile, SCRIPT_SYMBOLS).
LINK_VIEWS := decomp/targets/overlays/battle.data.ld
# The reasons its included objects stay original (coverage class included).
CLASSIFICATION := decomp/targets/overlays/battle.classification.txt
# Unit 8009E53C-800B16F0 divides with ASPSX's checked division.
MASPSX_battle_scene := --aspsx-version=2.34 --expand-div
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:38 2:33
# Unit 800B15D8-end: the Cygnus CDK GCC 2.7.2 with a later ASPSX (global
# stores through a register for %hi, positive li as addiu; docs/matching.md).
CC_battle_tmd_screen_effects := 2.7.2-cdk
MASPSX_battle_tmd_screen_effects := --aspsx-version=2.56
CC_battle_flow := 2.7.2-cdk
MASPSX_battle_flow := --aspsx-version=2.56
CC_battle_sprite_commands := 2.7.2-cdk
MASPSX_battle_sprite_commands := --aspsx-version=2.56
CC_battle_action_files := 2.7.2-cdk
MASPSX_battle_action_files := --aspsx-version=2.56
CC_battle_frame := 2.7.2-cdk
MASPSX_battle_frame := --aspsx-version=2.56
CC_battle_settle := 2.7.2-cdk
MASPSX_battle_settle := --aspsx-version=2.56
CC_battle_sprite_vm := 2.7.2-cdk
MASPSX_battle_sprite_vm := --aspsx-version=2.56
# INCLUDE_ORIGINAL reads original data from ORIGINAL, whose file offset 0 is
# VRAM 0x8006FAF0.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8006FAF0
