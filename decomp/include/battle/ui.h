#ifndef BATTLE_UI_H
#define BATTLE_UI_H

#include "common.h"
#include "psyq/libgpu.h"
#include "battle/work.h"

/* The battle UI: its state block, the drawing state, the glyph and quad
 * builders, the menu icons, the direction arrows and the text images
 * (battle overlay; the event script overlay ovl3087 reads the UI state). */

/* Battle UI state: the heap block at *800d2d28. */
typedef struct BattleUi {
    RECT textureWindows[6]; /* +0x00 */
    u32 unk30;         /* CLUT cycle position */
    s32 unk34;         /* +0x34 panel cursor window: x */
    s32 unk38;
    u32 unk3C;         /* y */
    s32 unk40;
    s32 unk44;         /* width */
    s32 unk48;
    u32 unk4C;         /* height */
    s32 unk50;
    u32 unk54;         /* x step (8.8) */
    s32 unk58;
    u32 unk5C;         /* step count */
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    u8 unk70[4];
    u8 statusParts[4]; /* +0x74 status glyph parts, [3] party-wide */
    u8 gaugeParts[3];  /* +0x78 */
    u8 unk7B;          /* AP text part count */
    u8 reaction[3];    /* +0x7C */
    u8 barShown[4];    /* +0x7F time bar shown, [3] party-wide */
    u8 portraitBuffer; /* +0x83 */
    u8 statusBuffer[4]; /* +0x84 draw buffer of the status glyphs, [3] party-wide */
    u8 unk88[0x8E - 0x88];
    u8 unk8E;
    u8 unk8F;
    u8 unk90[3];       /* per party member */
    u8 unk93[3];
    u8 unk96;          /* file 3 block loaded */
    u8 unk97;          /* selected list row */
    u8 unk98;
    u8 unk99[3];       /* party panel digit buffers */
    u8 unk9C;
    u8 unk9D;
    u8 unk9E;
    u8 unk9F;
    u8 unkA0;          /* the result screen counts */
    u8 unkA1;
    u8 gaugeBuffer;    /* +0xA2 */
    u8 unkA3;
    u8 unkA4;
    u8 unkA5;
    u8 unkA6;
    u8 unkA7;
    u8 unkA8;
    u8 unkA9;
    u8 unkAA;          /* frame counter */
    u8 unkAB;
    u8 unkAC;
    u8 unkAD;
    u8 unkAE;          /* menu module block loaded */
    u8 unkAF;
    u8 windows[7];     /* +0xB0 window shown */
    u8 unkB7;          /* command window page */
    u8 unkB8[7];       /* window opening */
    u8 unkBF[7];       /* window fully open */
    u8 unkC6;
    u8 unkC7;
    u8 unkC8;
    u8 unkC9;
    u8 unkCA;
    u8 unkCB;
    u8 unkCC[4];
    s32 unkD0[2];
    u8 unkD8[0xE0 - 0xD8];
    s32 unkE0[3];
    s32 unkEC[3];
    s32 unkF8;
    s32 unkFC;
    s32 unk100;
    u16 unk104;
    u16 unk106;
    u8 unk108[4];
} BattleUi;

extern BattleUi *D_800D2D28;

/* Battle drawing state (800ccb04, the battle area's ordering table pointer
 * on). The battle work area (800ccce8) follows it within one aggregate: some
 * code addresses the work area from here. */
typedef struct {
    u32 *ot;           /* current ordering table */
    u8 unk4[0x2C];
    s32 buffer;        /* +0x30 draw buffer index */
    u8 unk34[0x1E4 - 0x34];
    BattleWork work;   /* +0x1E4 (D_800CCCE8) */
} BattleDraw;

extern BattleDraw D_800CCB04;
extern u8 D_800CCB34;   /* D_800CCB04.buffer's low byte, read on its own */

/* Glyphs (the glyph table D_800D2F5C) and quads (80070e2c's unit). */
extern void *D_800D2F5C; /* glyph table */

extern u8 D_800C3CF4[9];   /* decimal digits */

/* A menu icon cell of the icon image (4 bytes, 800d2f68). */
typedef struct IconCell {
    u8 w;
    u8 alternate; /* uses the alternate CLUT */
    u8 u;
    u8 v;
} IconCell;

extern IconCell D_800D2F68[];

/* Direction arrow block (*800c3e24, 0xec bytes). */
typedef struct DirectionArrows {
    POLY_G3 prims[8]; /* per direction, one per draw buffer */
    s32 shade;           /* +0xE0 pulsing red level */
    u8 buffer;           /* +0xE4 */
    u8 fading;           /* +0xE5 the shade is going down */
    u8 arrows[4];        /* +0xE6 a target lies that way */
    u8 unkEA[2];
} DirectionArrows;

extern DirectionArrows *D_800C3E24;

/* Text images and decimal digits (battle.c). */
void func_800769E8(RECT *rect, u32 *pixels);
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y); /* full scale */
s32 func_80076A6C(s32 id, POLY_FT4 *prims, s16 x, s16 y); /* half scale */
void func_80076B00(POLY_FT4 *prim);
void func_80076B68(POLY_FT4 *prim);
void func_80076BF0(POLY_FT4 *prim);
void func_80076C34(POLY_FT4 *prim);
void func_80076C78(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 w);
void func_80076CE8(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h);
void func_80076D58(POLY_FT4 *prims, u8 alternate, u8 page);
void func_80077698(void);
void func_80077980(void);
void func_8008AAA0(u32 value);
s32 func_8008AC00(s32 count);
void func_80090B90(s32 x, s32 y, s32 *frame, u8 *ticks); /* animate a cursor glyph */

#endif
