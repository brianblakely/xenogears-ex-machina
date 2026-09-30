#ifndef OVL2601_MENU_CARD_H
#define OVL2601_MENU_CARD_H

#include "common.h"

/* PsyQ libgpu primitives and rectangles used by the card screens. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;

/* Sprite sheet entry (80026338's six outputs). */
typedef struct {
    s32 u, v, w, h, x, y;
} SheetEntry;

/*
 * Card state block (menu state + 32c, 5034h bytes): the directory scan, the
 * file heads and this game's save header.
 */
typedef struct {
    u8 unk0[0xB80];
    u32 icon_mode;     /* b80: TIM_IMAGE of the save icon */
    RECT *icon_crect;  /* b84 */
    u32 *icon_caddr;   /* b88 */
    RECT *icon_prect;  /* b8c */
    u32 *icon_paddr;   /* b90 */
    u8 heads[32][0x200]; /* b94: first 200h bytes of each file */
    u8 save_magic[2];  /* 4b94: "SC" */
    u8 save_icon_flag; /* 4b96 */
    u8 save_blocks;    /* 4b97 */
    u8 save_title[0x5C];   /* 4b98 */
    u8 save_palette[0x20]; /* 4bf4 */
    u8 save_icon[0x80];    /* 4c14 */
    u8 unk4C94[0x4F7C - 0x4C94];
    s32 cursor_slot;   /* 4f7c */
    u8 unk4F80[0x4FCE - 0x4F80];
    char game_prefix[13]; /* 4fce */
    u8 unk4FDB[0x5034 - 0x4FDB];
} CardState;

/* Party block (menu state + 33c, 6ch bytes). */
typedef struct {
    u8 unk0[3];
    u8 fade_active; /* 03 */
    u8 fade_step;   /* 04 */
    u8 unk5[0x30 - 5];
    u8 members[3];  /* 30: party member ids, ff none */
    u8 unk33[0x6C - 0x33];
} PartyBlock;

/* Menu state (*800625a0); only the fields this overlay touches are named. */
typedef struct {
    u8 unk0[0x2DC];
    void *sprite_sheet;  /* 2dc */
    void *label_text;    /* 2e0 */
    void *effect_bank;   /* 2e4 */
    u8 unk2E8[0x308 - 0x2E8];
    s32 buffer;          /* 308: draw buffer being built (0/1) */
    u8 member_present[16]; /* 30c */
    u8 digits[9];        /* 31c: decimal digits, leading zeros ff */
    u8 unk325;
    u8 poll_timer;       /* 326 */
    u8 unk327[0x32C - 0x327];
    CardState *card;     /* 32c */
    void *unk330;        /* 330: cc bytes */
    u8 unk334;
    u8 unk335;
    u8 top_cursor;       /* 336 */
    u8 unk337;
    u8 unk338[0x33C - 0x338];
    PartyBlock *party;   /* 33c */
    u8 unk340[0x348 - 0x340];
    void *unk348;        /* 348: 15c bytes */
    u8 unk34C[0x350 - 0x34C];
    void *unk350;        /* 350: 1194h bytes */
    void *unk354;        /* 354: 140ch bytes */
    u8 unk358[0x450 - 0x358];
    void *unk450;        /* 450: 4788h bytes */
    u8 unk454[0x46C - 0x454];
    SheetEntry sheet_entries[4]; /* 46c */
    u8 unk4CC[0x4E0 - 0x4CC];
    u8 labels[0x558 - 0x4E0]; /* 4e0 */
    void *label_pixels;  /* 558 */
    u8 unk55C[0x1E20 - 0x55C];
    void *unk1E20;       /* 1e20: dech bytes */
    u8 unk1E24[0x1E2C - 0x1E24];
    void *unk1E2C;       /* 1e2c */
} MenuState;

extern MenuState *D_800625A0;

/* Overlay data. */
extern u16 D_801D21F0[]; /* party bit of each member id */

/* Resident services. */
void *func_80031BDC(s32 size, s32 mode); /* heap allocate */
void func_800320E8(void *block);         /* heap free */
void func_8003F8E8(void *dst, s32 size); /* bzero */
void func_8003F99C(void *dst, void *src, s32 size); /* memmove */
void *func_80032E88(void *packed, s32 mode); /* unpack into a new block */
void func_8003342C(void *list);          /* relocate an offset list */
void func_80026338(void *sheet, s32 id, s32 *u, s32 *v, s32 *w, s32 *h, s32 *x, s32 *y);
void func_80033698(s32 x, s32 y);        /* text palettes */

/* libgpu */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
void func_80043BFC(void *prim, s32 abe);           /* SetSemiTrans */
void func_80043C24(void *prim, s32 tge);           /* SetShadeTex */
void func_80043CB0(POLY_FT4 *prim);                /* SetPolyFT4 */
void func_80043CC4(POLY_G4 *prim);                 /* SetPolyG4 */
void func_80044894(RECT *rect, void *data);        /* LoadImage */
s32 func_800445D0(s32 mode);                       /* DrawSync */

#endif
