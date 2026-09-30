/* Overlay 2600 (Disc 1 slot 2600; loaded at 0x801c5000): the character name
 * entry screen. A character grid (D_801CBEC0) is walked with the cursor, the
 * name is built as text codes and decoded for display, and the result is
 * stored in the character name table at 8006d634 + id * 0x14. The three
 * party portraits are loaded for the screen. Much of the drawing/list code is
 * the same as overlay 2598 (the party screen) but compiled into this image. */
#include "common.h"
#include "name_entry.h"

extern u16 D_801CC114[];

/* Test character `index`'s bit (table D_801CC114) in `flags`. */
s32 func_801C5040(s32 flags, u8 index) {
    return D_801CC114[index] & flags;
}

/* Allocate (nonzero) or release the 0x5034-byte work block. */
void func_801C505C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->work = block;
        func_8003F8E8(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->work);
    }
}

/* Allocate (nonzero) or release the party list. */
void func_801C50C0(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6C, 0);
        D_800625A0->party = block;
        func_8003F8E8(block, 0x6C);
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void func_801C5124(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->block_350 = block;
        func_8003F8E8(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->block_350);
    }
}

/* Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void func_801C5188(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140C, 0);
        D_800625A0->block_354 = block;
        func_8003F8E8(block, 0x140C);
    } else {
        func_800320E8(D_800625A0->block_354);
    }
}

/* Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void func_801C51EC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xCC, 0);
        D_800625A0->block_330 = block;
        func_8003F8E8(block, 0xCC);
    } else {
        func_800320E8(D_800625A0->block_330);
    }
}

/* Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void func_801C5250(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15C, 0);
        D_800625A0->block_348 = block;
        func_8003F8E8(block, 0x15C);
    } else {
        func_800320E8(D_800625A0->block_348);
    }
}

/* Allocate (nonzero) or release the name entry block. */
void func_801C52B4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xDEC, 0);
        D_800625A0->entry = block;
        func_8003F8E8(block, 0xDEC);
    } else {
        func_800320E8(D_800625A0->entry);
    }
}

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5318);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C58B8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5A30);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5A40);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5ABC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5CFC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5EAC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5F08);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6010);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6040);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C60BC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6408);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6460);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C64A8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C67C4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6928);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6B70);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C6EB4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7200);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7548);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7894);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7A18);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7AA4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7C28);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7D74);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7F48);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C811C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C82F0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C84C4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8600);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C874C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8970);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8AE8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8E38);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8EC8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8F58);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C90D0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9160);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C92BC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9338);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C97FC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C983C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C989C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C98E8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9AF4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9C34);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9D5C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F1C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F60);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F90);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9FC0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA39C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA400);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA558);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CADC8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB1C4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB25C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB2F0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB33C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CBDBC);
