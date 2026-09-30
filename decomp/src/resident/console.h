#ifndef RESIDENT_CONSOLE_H
#define RESIDENT_CONSOLE_H

#include "common.h"

/* Resident debug text console (the default heap/printf report output).
 * Unknown bytes keep their offsets. */
typedef struct {
    u16 flags;
    u8 unk2[2];
    u8 *buffer[2];   /* text buffers, selected by flags2E bit 0 */
    s16 left;        /* origin */
    s16 top;
    u8 unk10[4];
    s16 unk14;
    s16 unk16;
    u8 r, g, b;
    u8 mode;         /* bit 0: bright colour */
    u8 unk1C[0x12];
    u16 flags2E;
    s16 x;           /* cursor */
    s16 y;
    s16 unk34;
    s16 unk36;
    u8 *current;     /* active text buffer */
    u8 unk3C[0x90];
    s16 saved_x;
    s16 saved_y;
    s16 saved_36;
} Console;

extern Console *D_80059394;
extern s32 D_800593A0; /* the console block is not owned (not released) */

#endif
