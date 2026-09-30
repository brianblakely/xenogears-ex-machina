#ifndef RESIDENT_TEXT_H
#define RESIDENT_TEXT_H

#include "common.h"

/* Resident message text: the font ("MES FONT") and system data ("MES
 * SYSDATA") resources, and the conversion between text bytes and the
 * two-byte character codes the message system draws. */

typedef struct {
    s16 x, y, w, h;
} RECT;

/* The text bytes of a character code (first is 0 for one-byte text). */
typedef struct {
    u8 first;
    u8 second;
} CharPair;

extern s32 D_8005934C;  /* font: first byte of a two-byte character */
extern s32 D_80059350;
extern s32 D_80059354;
extern s32 D_80059358;
extern u8 *D_8005935C;  /* font glyph data */
extern u8 **D_80059360; /* system data: resource table */
extern s32 D_80059364;
extern u8 *D_80059368;  /* system data block */
extern u16 *D_8005936C; /* font block: halfword 1 glyph offset, 2 first
                         * byte of a two-byte character */
extern u8 D_8005A0E4[]; /* decoded text */
extern u16 D_80059414;  /* text CLUTs */
extern u16 D_800595D4;
extern u16 D_80050190[]; /* text palette */

/* PsyQ libgpu (SDK region): LoadImage, GetClut. */
extern s32 func_80044894(RECT *rect, void *pixels);
extern u16 func_80043A58(s32 x, s32 y);

extern void func_800320A4(void *data);
extern void func_800320B8(void *data);
extern s32 func_800320E8(void *data);
extern void func_800324B8(s16 kind);

#endif
