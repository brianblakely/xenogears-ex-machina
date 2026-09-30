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

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    u32 pad;
} LINE_F3;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_MODE;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

/* Sprite sheet entry (80026338's six outputs). */
typedef struct {
    s32 u, v, w, h, x, y;
} SheetEntry;

/* A text label: its quad per draw buffer and the VRAM area of its pixels. */
typedef struct {
    POLY_FT4 poly[2]; /* 00 */
    u8 unk50[0x20];
    RECT rect;        /* 70: pixel area in VRAM */
    void *pixels;     /* 78: label pixel buffer (only labels[0] of the menu state) */
    u8 highlight;     /* 7c */
    u8 unk7D;
    u8 width;         /* 7e: text width in pixels */
    u8 dirty;         /* 7f */
} Label;

/* Cursor block (menu state + 348, 15ch bytes), per draw buffer. */
typedef struct {
    POLY_FT4 sprite[2];     /* 00 */
    POLY_G4 shade[2];       /* 50 */
    POLY_F4 screen[2];      /* 98 */
    LINE_F3 upper[2];       /* c8 */
    LINE_F3 lower[2];       /* f8 */
    DR_MODE mode_label[2];  /* 128 */
    DR_MODE mode_sprite[2]; /* 140 */
    u8 sprite_buffer;       /* 158 */
    u8 shade_buffer;        /* 159 */
    u8 unk15A;
    u8 width;               /* 15b */
} CursorBlock;

/* Scroll bar (menu state + 43c, 74h bytes). */
typedef struct {
    POLY_FT4 sprite[2]; /* 00 */
    SVECTOR quad[4];    /* 50 */
    u8 buffer;          /* 70 */
    u8 unk71[3];
} ScrollBar;

/* Animated marker (menu state + 444 + n * 4, 78h bytes). */
typedef struct {
    POLY_FT4 sprite[2]; /* 00 */
    SVECTOR quad[4];    /* 50 */
    s32 frame;          /* 70: 4..0 */
    u8 timer;           /* 74 */
    u8 buffer;          /* 75 */
    u8 unk76[2];
} Marker;

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
    u8 cursor_shown; /* 03 */
    u8 unk4;         /* 04 */
    u8 unk5[0x30 - 5];
    u8 members[3];   /* 30: party member ids, ff none */
    u8 unk33[0x49 - 0x33];
    u8 scroll_shown; /* 49 */
    u8 unk4A[0x50 - 0x4A];
    u8 marker_shown[2]; /* 50 */
    u8 unk52[0x6C - 0x52];
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
    void **resources;    /* 330: cch bytes of unpacked resources */
    u8 unk334;
    u8 unk335;
    u8 top_cursor;       /* 336 */
    u8 unk337;
    u8 unk338[0x33C - 0x338];
    PartyBlock *party;   /* 33c */
    u8 unk340[0x348 - 0x340];
    CursorBlock *cursor; /* 348 */
    u8 unk34C[0x350 - 0x34C];
    void *unk350;        /* 350: 1194h bytes */
    void *unk354;        /* 354: 140ch bytes */
    u8 unk358[0x43C - 0x358];
    ScrollBar *scroll;   /* 43c */
    u8 unk440[4];
    Marker *markers[2];  /* 444 */
    u8 unk44C[4];
    u8 *unk450;          /* 450: 4788h bytes */
    u8 unk454[0x46C - 0x454];
    SheetEntry sheet_entries[4]; /* 46c */
    u8 unk4CC[0x4E0 - 0x4CC];
    Label labels[4];     /* 4e0 */
    u8 unk6E0[0x1E20 - 0x6E0];
    void *unk1E20;       /* 1e20: dech bytes */
    u8 unk1E24[0x1E2C - 0x1E24];
    void *unk1E2C;       /* 1e2c */
} MenuState;

extern MenuState *D_800625A0;

/* Overlay data. */
extern u16 D_801D21F0[]; /* party bit of each member id */
extern u8 D_801D2018[];  /* label text ids */
extern s32 D_801D2194[]; /* cursor x per position */
extern s32 D_801D21B0[]; /* cursor y per position */

/* Game state. */
extern u16 D_8006F364;   /* party members joined */
extern u16 D_8006F366;   /* party members available */
extern u8 D_8006F368[3]; /* party member ids */
extern u16 D_80059414;   /* highlighted text CLUT */
extern u16 D_800595D4;   /* plain text CLUT */

/* Resident services. */
void *func_80031BDC(s32 size, s32 mode); /* heap allocate */
void func_800320E8(void *block);         /* heap free */
void func_8003F8E8(void *dst, s32 size); /* bzero */
void func_8003F99C(void *dst, void *src, s32 size); /* memmove */
void *func_80032E88(void *packed, s32 mode); /* unpack into a new block */
void func_8003342C(void *list);          /* relocate an offset list */
void func_80026338(void *sheet, s32 id, s32 *u, s32 *v, s32 *w, s32 *h, s32 *x, s32 *y);
void func_80033698(s32 x, s32 y);        /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
void func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800288EC(s32 file);             /* file size in words */
void func_800295D8(s32 file, void *dst, s32 offset, s32 mode); /* disc read */
s32 func_80028A60(s32 mode);             /* disc wait */

/* libgpu */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
void func_80043BFC(void *prim, s32 abe);           /* SetSemiTrans */
void func_80043C24(void *prim, s32 tge);           /* SetShadeTex */
void func_80043CB0(POLY_FT4 *prim);                /* SetPolyFT4 */
void func_80043CC4(POLY_G4 *prim);                 /* SetPolyG4 */
void func_80043C9C(POLY_F4 *prim);                 /* SetPolyF4 */
void func_80043DA0(LINE_F3 *prim);                 /* SetLineF3 */
void func_800454DC(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */
void func_80044894(RECT *rect, void *data);        /* LoadImage */
s32 func_800445D0(s32 mode);                       /* DrawSync */

/* This overlay. */
u16 func_801C50B0(u16 mask, u8 id);
void func_801C54B4(void);
void func_801C5A7C(Label *label, s32 index, s32 row, s32 mode);
void func_801C5E6C(void);
void func_801C6430(void);
void func_801C6460(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801C6E90(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h);

#endif
