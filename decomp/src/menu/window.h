#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include "common.h"

/* Screen rectangle (libgpu RECT). */
typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} Rect;

/* Textured sprite packet (libgpu SPRT). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} Sprite;

/* Flat rectangle packet (libgpu TILE). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} Tile;

/* The common packet head (libgpu P_TAG). */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0, g0, b0, code;
} PacketTag;

/* A centred one-line caption: its text image and one sprite per buffer. */
typedef struct {
    s32 image;         /* 0x00 */
    Sprite sprite[2];  /* 0x04 */
    s16 width;         /* 0x2C */
    s16 x;             /* 0x2E */
} Caption;

/* One line of a choice menu. */
typedef struct {
    u8 flags;          /* 0x00: 1 = runs every frame, 2 = pad to the widest
                        * line, 4 = skipped by the cursor */
    u8 caption;        /* 0x01: caption text while selected */
    u8 unk2[2];
    s32 text;          /* 0x04 */
    void (*handler)(s32 arg); /* 0x08: run on confirm */
    s32 arg;           /* 0x0C */
    u8 unk10[2];
    s16 half_width;    /* 0x12 */
} MenuItem;

typedef struct Menu Menu;

/* A choice menu drawn on a translucent panel (eight in D_800915AC). */
struct Menu {
    u8 title_width;    /* 0x00: nonzero shifts the lines right */
    u8 unk1[3];
    MenuItem *items;   /* 0x04 */
    s16 count;         /* 0x08 */
    s16 parent;        /* 0x0A: menu index returned to on cancel */
    void (*draw)(Menu *menu); /* 0x0C */
    u8 unk10[2];
    s16 cursor;        /* 0x12 */
    s16 y;             /* 0x14 */
    s16 x;             /* 0x16 */
    u8 unk18[4];
    Tile panel[2];     /* 0x1C: one per buffer */
};

extern u8 D_8009273C; /* caption text shown in D_80095510 */
extern u8 D_80092740; /* caption text shown in D_80095540 */
extern u8 D_800928A0; /* buffer being built */
extern s32 D_80092880;
extern Caption D_80095510;
extern Caption D_80095540;
extern u8 D_80095570[2][8]; /* per-buffer mode packets for the captions */
extern Menu D_800915AC[8];
extern Menu *D_80092734; /* menu being shown */
extern s32 D_80092700;
extern s32 D_80092744; /* caption of the selected line */
extern s32 D_80092748; /* pad buttons held */
extern s32 D_8009274C; /* pad buttons repeated */
extern s32 D_80092750; /* pad buttons pressed */
extern s32 D_80091364; /* pad port of the menu input */
extern u8 D_80092764;  /* stick is deflected */
/* Resident pad state, per port. */
extern u16 D_80059570, D_80059574;
extern u16 D_800594A4, D_800594A8;
extern u16 D_8005948C, D_80059490;
extern u8 D_80059438, D_8005943C; /* stick x */
extern u8 D_80059430, D_80059434; /* stick y */
extern s32 D_80092704;

s32 func_80033728(s32 table, s32 index); /* text string of an index */
s32 func_80034EAC(s32 string, s32 image, s32 colour, s32 arg); /* returns width */
void func_80043B48(void *ot, void *packet); /* link a packet into an ordering table */
void func_80044894(Rect *rect, s32 image);
s32 func_8007EB6C(s32 text);
void func_8007EE08(s32 arg);
void func_80080F04(void);
void func_8007E894(s32 x, s32 y);
void func_8007F948(Menu *menu, s32 line);
void func_8007EBE0(s32 text);
void func_8007ED84(s32 text, s32 half_width);
s32 func_80035734(s32 port); /* pad type */
s32 func_80048C4C(s32 value); /* square root */
void func_8008EB4C(s32 sound);
void func_80080964(s32 menu);

#endif
