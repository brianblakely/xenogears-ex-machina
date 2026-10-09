# menu: decoded overlay image at 0x8006faf0 (0x22e69 bytes).
CC_VERSION := 2.7.2
SPLAT_CONFIG := decomp/targets/overlays/menu.yaml
ORIGINAL := .local/extract/overlays/menu.bin
ORIGINAL_SHA256 := 3e6df915e9c7f05f5fb997cb331392f1333e867dfb2628cd65e5e1ea1575646e
# Its uninitialized data ends at 8009b558: the resident's mode table entry 4
# (800180cc) clears the words after 800925d0 through 8009b554 (80019560).
BSS_END := 0x8009B558
BUILD := .local/decomp/build/menu
IMAGE := .local/decomp/build/menu.bin
LINKER_SCRIPT := .local/decomp/menu/menu.ld
LINKER_EXTRA := .local/decomp/menu/undefined_syms_auto.txt .local/decomp/menu/undefined_funcs_auto.txt decomp/targets/overlays/menu.bss.ld
SOURCE_DIRS := decomp/src/menu
# Packed containers of this image (tools/packed_container.py).
CONTAINERS := 1:35 2:30
# Hand-written assembly ranges (coverage class handwritten).
CLASSIFICATION := decomp/targets/overlays/menu.classification.txt
# INCLUDE_ASSET/INCLUDE_ORIGINAL read original data from ORIGINAL, whose file
# offset 0 is VRAM 0x8006FAF0.
TARGET_CPPFLAGS += -DORIGINAL_BASE=0x8006FAF0
# The program ends at 0x80092954 (file 0x22e64) with the small uninitialized
# variables (.sbss), where the larger ones (.bss) start. The original packer
# appended zero literal tokens until its last group held eight and recorded
# the padded length: the plain encoding of the program plus five zero literals
# is the disc stream (tools/packed_container.py). The five bytes follow the
# link as file padding.
PACKER_TAIL := 5
# The menu's assembler allocated the uninitialized variables GCC emits after
# each unit's code by size: those of up to 8 bytes in .sbss, the larger ones
# in .bss, each in declaration order, while the code, assembled before them,
# addresses all of them absolutely (cc1, maspsx and GNU as stay -G0; maspsx's
# own -G8 would also move the code's accesses to $gp). Evidence: the link
# under this rule puts every variable the code addresses where the original
# code addresses it: all 208 objects of up to 8 bytes in 800925d4-80092954 and
# all 50 larger ones in 80092954-8009b558, each group in unit order with its
# commons last. Field, compiled by the same GCC 2.7.2, keeps one .bss with its
# small and large commons interleaved. SBSS (decomp/Makefile) makes the split
# in every unit with variables (menu.yaml, menu.bss.ld).
SBSS_menu2 := 8
SBSS_menu3 := 8
SBSS_menu4 := 8
SBSS_menu5 := 8
SBSS_menu6 := 8
SBSS_menu7 := 8
SBSS_menu_common := 8
