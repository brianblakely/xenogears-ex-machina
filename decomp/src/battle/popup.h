#ifndef BATTLE_POPUP_H
#define BATTLE_POPUP_H

/* The battle's floating numbers (damage and recovery popups, 800BE11C-
 * 800BE6E8): sprite tasks spinning and shrinking over their lifetime. */

#include "common.h"
#include "psyq.h"

/* A glyph of a popup (0x18 bytes, a resident sprite part). */
typedef struct {
    s16 x, y;
    u8 u, v;
    u8 w, h; /* 0x06 */
    u8 pad8[0x18 - 0x8];
} PopupGlyph;

/* A number popup (a 0x130-byte task). */
typedef struct NumberPopup {
    u8 pad0[0xC];
    void (*destroy)(struct NumberPopup *popup); /* 0x0C */
    u8 pad10[0x38 - 0x10];
    SVector angle;        /* 0x38 */
    Vector scale;         /* 0x40 */
    u8 pad50[0x60 - 0x50];
    s32 life;             /* 0x60: frames left */
    s32 spin;             /* 0x64: per frame */
    union {
        u8 rgbc[4];       /* 0x68: red, green, blue, primitive code */
        s32 word;
    } colour;
    s32 glyphCount;       /* 0x6C */
    PopupGlyph glyphs[8]; /* 0x70 */
} NumberPopup;

/* The task drawing a popup. */
typedef struct {
    u8 pad0[4];
    NumberPopup *popup; /* 0x04 */
} PopupTask;

extern s32 D_800D2D68;
extern s32 D_800C374C;
extern Matrix D_800C3760; /* the popups' view */
extern s32 D_800C377C;    /* the projection distance when drawn */
extern s16 D_800C3752[];  /* the first glyph's x by digit count */
extern s32 D_800D3630;    /* the popup colour kind */
extern void *D_800D2F5C;  /* the number font */
extern u8 D_800C3784[];   /* hexadecimal digit glyphs */
extern u32 D_800C37A4[];  /* powers of ten */

/* Resident services. */
u8 func_80021AD8(u8 value, s32 delta); /* add, clamped to 0..255 */
void func_80021B24(SVector *out, SVector *in);
void *func_8001D1D8(s32 size, void *owner, void (*update)(), void (*draw)(), s32 arg4);
s32 func_80026DCC(void *font, s32 character, PopupGlyph *out, s16 x, s32 y); /* glyphs added */

void func_800BD810(PopupGlyph *glyph, s32 colour);
void func_800BE6E8(s32 value, u8 *text, s32 digits, u8 leading, s32 base);

#endif
