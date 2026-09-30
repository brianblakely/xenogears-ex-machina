#ifndef OVL2596_BATTLE_RESULTS_H
#define OVL2596_BATTLE_RESULTS_H

#include "common.h"

/* libgpu primitives (PsyQ layout). */
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

/* A glyph-table sprite part, one primitive per draw buffer. */
typedef POLY_FT4 Glyph[2];

/* A run of glyphs drawn together: how many parts, and the draw buffer they
 * were built for. */
typedef struct {
    u8 count;
    u8 buffer;
} GlyphRun;

/* Per party member result card (pointers 800d32f8, one per slot). */
typedef struct {
    Glyph portrait[4];    /* 0x0000 */
    Glyph labels[20];     /* 0x0140 */
    Glyph field780[3];    /* 0x0780 */
    Glyph field870[3];    /* 0x0870 */
    Glyph field960[3];    /* 0x0960 */
    Glyph fieldA50[3];    /* 0x0A50 */
    Glyph fieldB40[2];    /* 0x0B40 */
    Glyph fieldBE0[2];    /* 0x0BE0 */
    Glyph fieldC80[8];    /* 0x0C80 */
    Glyph fieldF00[8];    /* 0x0F00 */
    Glyph field1180[7];   /* 0x1180 */
    Glyph field13B0[7];   /* 0x13B0 */
    GlyphRun runs[12];    /* 0x15E0 */
    u8 flag15F8;          /* 0x15F8 */
    u8 flag15F9;          /* 0x15F9 */
} MemberCard;

/* The battle UI state block (pointer 800d2d28). */
typedef struct {
    u8 pad0[0x8F];
    u8 show8F;            /* 0x8F */
    u8 pad90[0x10];
    u8 showCards;         /* 0xA0 */
    u8 showSummary;       /* 0xA1 */
    u8 padA2[0xA];
    u8 showSpoils;        /* 0xAC */
} BattleUi;

/* Battle slot info (800c3eb4, 0x1c per slot); the symbol names its
 * character id byte. */
typedef struct {
    u8 id;                /* 0x7f: empty */
    u8 pad[0x1B];
} SlotInfo;

extern BattleUi *D_800D2D28;
extern MemberCard *D_800D32F8[3];
extern SlotInfo D_800C3EB6[];
extern u8 D_800CCB34;       /* current draw buffer */
extern u32 *D_800CCB04;     /* current ordering table */

void func_80043B48(void *ot, void *prim);                   /* AddPrim */
void func_800728B8(POLY_FT4 *prims, s32 count, s32 buffer);
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y);   /* glyph sprite */

void func_801DE1C4(void);
void func_801DE408(void);

#endif
