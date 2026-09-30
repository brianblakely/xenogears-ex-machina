/* Overlay 2598 (Disc 1 slot 2598, Disc 2 slot 2593; loaded at 0x801c5000):
 * the party member selection screen. It lists the three party slots and the
 * characters that may join (flags 8006f364 & 8006f366), lets the player swap
 * members between the two lists, and writes the chosen party back to
 * 8006f368. It shares the menu state (*D_800625A0) with the other menu
 * overlays. */
#include "common.h"
#include "party_menu.h"

extern u16 D_801CB57C[];

/* Test character `index`'s bit (table D_801CB57C) in `flags`. */
s32 func_801C5018(s32 flags, u8 index) {
    return D_801CB57C[index] & flags;
}

/* Allocate (nonzero) or release the 0x5034-byte work block. */
void func_801C5034(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->work = block;
        func_8003F8E8(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->work);
    }
}

/* Allocate (nonzero) or release the party list. */
void func_801C5098(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6C, 0);
        D_800625A0->party = block;
        func_8003F8E8(block, 0x6C);
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void func_801C50FC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->block_350 = block;
        func_8003F8E8(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->block_350);
    }
}

/* Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void func_801C5160(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140C, 0);
        D_800625A0->block_354 = block;
        func_8003F8E8(block, 0x140C);
    } else {
        func_800320E8(D_800625A0->block_354);
    }
}

/* Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void func_801C51C4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xCC, 0);
        D_800625A0->block_330 = block;
        func_8003F8E8(block, 0xCC);
    } else {
        func_800320E8(D_800625A0->block_330);
    }
}

/* Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void func_801C5228(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15C, 0);
        D_800625A0->block_348 = block;
        func_8003F8E8(block, 0x15C);
    } else {
        func_800320E8(D_800625A0->block_348);
    }
}

/* Allocate (nonzero) or release the six member and three party panels. */
void func_801C528C(u8 allocate) {
    s32 i;

    if (allocate) {
        for (i = 0; i < 6; i++) {
            void *block = func_80031BDC(0xBEC, 0);
            D_800625A0->member_panels[i] = block;
            func_8003F8E8(block, 0xBEC);
        }
        for (i = 0; i < 3; i++) {
            void *block = func_80031BDC(0xBEC, 0);
            D_800625A0->party_panels[i] = block;
            func_8003F8E8(block, 0xBEC);
        }
    } else {
        for (i = 0; i < 6; i++) {
            func_800320E8(D_800625A0->member_panels[i]);
        }
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->party_panels[i]);
        }
    }
}

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5390);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C559C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5714);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5724);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C57A0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C59E0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5B90);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5BEC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5CF4);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5D24);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C5DA0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C60EC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C6144);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C618C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C64A8);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C660C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C6854);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C6B98);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C6EE4);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C722C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7578);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C76FC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7788);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C790C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7A58);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7DA8);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7E38);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C7EC8);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8040);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C80BC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C83D0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C846C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C849C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8670);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8844);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8A18);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8BEC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8D28);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C8E74);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9098);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9210);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9270);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C92AC);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C94A0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C95A0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C969C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9748);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9908);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9A08);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C9F80);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CA24C);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CA5C0);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CA690);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CA810);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CA944);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CAB04);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CAB48);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CAD14);

INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CB0A8);
