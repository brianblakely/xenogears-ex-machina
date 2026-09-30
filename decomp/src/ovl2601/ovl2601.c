/*
 * ovl2601 (Disc 1 slot 2601, Disc 2 slot 2596; loaded at 801c5000): a
 * stand-alone memory-card screen built from the same card code as the menu's
 * save overlay (slot 2597), but for the file prefix "BISLPS-00800" instead of
 * this game's "BASLUS-00664". It loads its own menu resources (sprite sheet,
 * label text, icon TIM, sound bank), keeps the card directory and file heads
 * in the menu state's card block and draws the file list and details panel.
 */
#include "menu_card.h"

/* Place a textured quad at (x, y) of size w x h showing texels (u, v)..(u + w, v + h). */
void func_801C5040(POLY_FT4 *poly, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->u0 = u;
    poly->u2 = u;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* Test member `id`'s bit of a party bit mask. */
s32 func_801C50B0(s32 mask, u8 id) {
    return D_801D21F0[id] & mask;
}

/* The party bit of member `id`. */
u16 func_801C50CC(u8 id) {
    return D_801D21F0[id];
}

/* Split `value` into nine decimal digits (menu state +31c), leading zeros blanked (ff). */
#ifdef NON_MATCHING
void func_801C50E8(u32 value) {
    u32 digit;
    s32 i;
    u32 divisor;

    divisor = 100000000;
    for (i = 0; i < 9; i++) {
        digit = value / divisor;
        value %= divisor;
        divisor /= 10;
        D_800625A0->digits[i] = digit;
    }
    for (i = 1; i < 9; i++) {
        if (D_800625A0->digits[i] != 0) {
            if (D_800625A0->digits[i - 1] == 0) {
                D_800625A0->digits[i - 1] = 0xFF;
            }
            break;
        }
        D_800625A0->digits[i - 1] = 0xFF;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C50E8);
#endif

/* Allocate (nonzero) or release the card state block. */
void func_801C5194(u8 allocate) {
    if (allocate) {
        D_800625A0->card = func_80031BDC(sizeof(CardState), 0);
        func_8003F8E8(D_800625A0->card, sizeof(CardState));
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the party block. */
void func_801C51F8(u8 allocate) {
    if (allocate) {
        D_800625A0->party = func_80031BDC(sizeof(PartyBlock), 0);
        func_8003F8E8(D_800625A0->party, sizeof(PartyBlock));
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate (nonzero) or release the block at menu state +350. */
void func_801C525C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk350 = func_80031BDC(0x1194, 0);
        func_8003F8E8(D_800625A0->unk350, 0x1194);
    } else {
        func_800320E8(D_800625A0->unk350);
    }
}

/* Allocate (nonzero) or release the block at menu state +354. */
void func_801C52C0(u8 allocate) {
    if (allocate) {
        D_800625A0->unk354 = func_80031BDC(0x140C, 0);
        func_8003F8E8(D_800625A0->unk354, 0x140C);
    } else {
        func_800320E8(D_800625A0->unk354);
    }
}

/* Allocate (nonzero) or release the block at menu state +330. */
void func_801C5324(u8 allocate) {
    if (allocate) {
        D_800625A0->unk330 = func_80031BDC(0xCC, 0);
        func_8003F8E8(D_800625A0->unk330, 0xCC);
    } else {
        func_800320E8(D_800625A0->unk330);
    }
}

/* Allocate (nonzero) or release the block at menu state +348. */
void func_801C5388(u8 allocate) {
    if (allocate) {
        D_800625A0->unk348 = func_80031BDC(0x15C, 0);
        func_8003F8E8(D_800625A0->unk348, 0x15C);
    } else {
        func_800320E8(D_800625A0->unk348);
    }
}

/* Allocate (nonzero) or release the block at menu state +1e20. */
void func_801C53EC(u8 allocate) {
    if (allocate) {
        D_800625A0->unk1E20 = func_80031BDC(0xDEC, 0);
        func_8003F8E8(D_800625A0->unk1E20, 0xDEC);
    } else {
        func_800320E8(D_800625A0->unk1E20);
    }
}

/* Allocate (nonzero) or release the block at menu state +450. */
void func_801C5450(u8 allocate) {
    if (allocate) {
        D_800625A0->unk450 = func_80031BDC(0x4788, 0);
        func_8003F8E8(D_800625A0->unk450, 0x4788);
    } else {
        func_800320E8(D_800625A0->unk450);
    }
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C54B4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C58F4);

/* Start building draw buffer 0. */
void func_801C5A6C(void) {
    D_800625A0->buffer = 0;
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5A7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5CBC);

/* Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
#ifdef NON_MATCHING
void func_801C5E6C(void) {
    RECT rect;
    u16 *palette;

    palette = func_80031BDC(0x20, 0);
    func_8003F8E8(palette, 0x20);
    palette[1] = 0x7FFF;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.x = 0;
    rect.h = 1;
    func_80044894(&rect, palette);
    func_800445D0(0);
    func_800320E8(palette);
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5E6C);
#endif

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5EE8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5F44);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C604C);

/* Stop the screen fade. */
void func_801C6430(void) {
    D_800625A0->party->fade_step = 0;
    D_800625A0->party->fade_active = 0;
}

/* Initialise a gouraud quad fading from (r, g, b) on the top edge to black on the bottom. */
void func_801C6460(POLY_G4 *poly, u8 r, u8 g, u8 b) {
    func_80043CC4(poly);
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    poly->r2 = 0;
    poly->g2 = 0;
    poly->b2 = 0;
    poly->r3 = 0;
    poly->g3 = 0;
    poly->b3 = 0;
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C64DC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6828);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6A6C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6E90);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6EE8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6F30);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C70B8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C70FC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7178);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7314);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7370);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C768C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C77F0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7A38);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7D7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C80C8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8410);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C875C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C88E0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C896C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8AF0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8C3C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8D58);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8DDC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8E28);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8EB8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C908C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9260);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9434);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9608);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9744);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9890);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9AB4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9C2C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9F7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA00C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA09C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA214);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA22C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA388);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA404);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA444);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAB0C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAB80);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CABF4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAC7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CACC8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAED4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB014);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB13C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB2FC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB340);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB370);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB384);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB7F4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB894);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBA50);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBB08);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBC88);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBCF0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC024);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC278);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC54C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC720);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC97C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCAD8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCD28);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCE1C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCFF4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD404);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD5D0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD6F8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD7E4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD8D0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CDBA0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CDD14);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE480);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE8D8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE91C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CEB3C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF2A0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF678);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF780);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CFF58);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D05BC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D0C18);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D0E68);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1658);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D18A8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D18E8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1928);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1968);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1B18);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1CA4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1F10);
