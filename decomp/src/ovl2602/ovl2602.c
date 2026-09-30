#include "menu_card.h"

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C511C);

/* Place a textured quad at (x, y) of size w x h showing texels (u, v)..(u + w, v + h). */
void func_801C51B8(POLY_FT4 *poly, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
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
s32 func_801C5228(s32 mask, u8 id) {
    return D_801D6C68[id] & mask;
}

/* The party bit of member `id`. */
u16 func_801C5244(u8 id) {
    return D_801D6C68[id];
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5260);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C527C);

/* Split `value` into nine decimal digits (menu state +31c), leading zeros blanked (ff). */
#ifdef NON_MATCHING
void func_801C5298(u32 value) {
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
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5298);
#endif

/* Allocate (nonzero) or release the card state block. */
void func_801C5344(u8 allocate) {
    if (allocate) {
        D_800625A0->card = func_80031BDC(sizeof(CardState), 0);
        func_8003F8E8(D_800625A0->card, sizeof(CardState));
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the party block. */
void func_801C53A8(u8 allocate) {
    if (allocate) {
        D_800625A0->party = func_80031BDC(sizeof(PartyBlock), 0);
        func_8003F8E8(D_800625A0->party, sizeof(PartyBlock));
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate (nonzero) or release the block at menu state +350. */
void func_801C540C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk350 = func_80031BDC(0x1194, 0);
        func_8003F8E8(D_800625A0->unk350, 0x1194);
    } else {
        func_800320E8(D_800625A0->unk350);
    }
}

/* Allocate (nonzero) or release the block at menu state +354. */
void func_801C5470(u8 allocate) {
    if (allocate) {
        D_800625A0->unk354 = func_80031BDC(0x140C, 0);
        func_8003F8E8(D_800625A0->unk354, 0x140C);
    } else {
        func_800320E8(D_800625A0->unk354);
    }
}

/* Allocate (nonzero) or release the block at menu state +330. */
void func_801C54D4(u8 allocate) {
    if (allocate) {
        D_800625A0->unk330 = func_80031BDC(0xCC, 0);
        func_8003F8E8(D_800625A0->unk330, 0xCC);
    } else {
        func_800320E8(D_800625A0->unk330);
    }
}

/* Allocate (nonzero) or release the block at menu state +348. */
void func_801C5538(u8 allocate) {
    if (allocate) {
        D_800625A0->unk348 = func_80031BDC(0x15C, 0);
        func_8003F8E8(D_800625A0->unk348, 0x15C);
    } else {
        func_800320E8(D_800625A0->unk348);
    }
}

/* Allocate (nonzero) or release the block at menu state +1e20. */
void func_801C559C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk1E20 = func_80031BDC(0xDEC, 0);
        func_8003F8E8(D_800625A0->unk1E20, 0xDEC);
    } else {
        func_800320E8(D_800625A0->unk1E20);
    }
}

/* Allocate (nonzero) or release the block at menu state +450. */
void func_801C5600(u8 allocate) {
    if (allocate) {
        D_800625A0->unk450 = func_80031BDC(0x4788, 0);
        func_8003F8E8(D_800625A0->unk450, 0x4788);
    } else {
        func_800320E8(D_800625A0->unk450);
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5664);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C56C8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5B08);

/* Start building draw buffer 0. */
void func_801C5C98(void) {
    D_800625A0->buffer = 0;
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5CA8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5EE8);

/* Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
#ifdef NON_MATCHING
void func_801C6098(void) {
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
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6098);
#endif

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6114);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6170);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6278);

/* Stop the screen fade. */
void func_801C665C(void) {
    D_800625A0->party->fade_step = 0;
    D_800625A0->party->fade_active = 0;
}

/* Initialise a gouraud quad fading from (r, g, b) on the top edge to black on the bottom. */
void func_801C668C(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6708);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6A54);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6E74);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7604);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C765C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C76A4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C782C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7870);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C78EC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7A88);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7AE4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7E00);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7F64);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C81AC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C84F0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C883C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C8B84);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C8ED0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C90E0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9264);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C93B0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C94CC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9550);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C959C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C962C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9690);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9864);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9A38);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9C0C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9DE0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9F1C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA068);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA28C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA404);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA754);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA7E4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA874);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA9EC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CAA7C);

void func_801CABD8(void) {
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CABE0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CAC20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB2E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB35C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB3D0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB498);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB4E4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB690);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBA2C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBDA0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBE60);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC1C4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC31C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC4DC);

void func_801CC520(void) {
}

void func_801CC528(void) {
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC530);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC9A0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCA40);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCC18);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCD20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCE90);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCEBC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCEE8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD310);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD564);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD838);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDA0C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDC68);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDD74);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE024);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE1D0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE2E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE32C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE7E0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE82C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CEA68);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CEEA8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF184);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF33C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF38C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF448);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF9BC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFAB8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFC60);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFF18);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0220);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0348);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0398);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D04E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D05EC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D06D8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0C20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0D4C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0EC8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1078);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1304);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D18F8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1F20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2784);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D27C4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2804);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2950);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2B74);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3558);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3A3C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3A80);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3C78);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D44FC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D4888);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D498C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5398);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D573C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D57A8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5828);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5D38);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5EB8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5F94);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6150);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D61B8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6250);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D62A4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6334);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6738);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D690C);
