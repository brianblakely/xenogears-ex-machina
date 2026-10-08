#ifndef RESIDENT_TEXT_H
#define RESIDENT_TEXT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "heap.h"

/* Resident message text: the font ("MES FONT") and system data ("MES
 * SYSDATA") resources, and the conversion between text bytes and the
 * two-byte character codes the message system draws. */

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
extern u16 D_8005A0C8[12]; /* number character codes: color, 10 digits, 0xFFFF */
extern u8 D_8005A0E4[]; /* decoded text */
extern u16 D_80059414;  /* text CLUTs */
extern u16 D_800595D4;
extern u16 D_80050190[]; /* text palette */
extern u16 D_800501D0[11]; /* special 0xFFFF glyph */

/* Packed data (0x80032e7c-0x80032f54); window setup follows. */
s32 func_80032E7C(s32 *packed);
void *func_80032E88(void *data, s32 mode); /* allocate (func_80031BDC mode) and unpack */
void *func_80032EB4(void *source, void *destination); /* unpack; returns destination */

void func_80033558(u16 *font);
void func_800335F4(u8 *data);
u8 *func_80033728(u8 *resource, s32 index);
s32 func_80033BAC(u8 first, u8 second);
s32 func_80034F98(u16 first, u16 second);
void func_80034FFC(s32 first, u16 second, u16 *image, s16 stride, s32 plane);

extern u8 D_8006F2E8[]; /* map indirect name indices to their 20-byte slots */

#endif
