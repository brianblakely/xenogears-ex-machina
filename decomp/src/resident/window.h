#ifndef RESIDENT_WINDOW_H
#define RESIDENT_WINDOW_H

#include "common.h"

/* Resident message window: laid-out text lines drawn as sprites. Field
 * names follow their observed use; unknown bytes keep their offsets. */

/* A queued follow-up message (window list at 0x8C). */
typedef struct WindowQueue {
    struct WindowQueue *next;
    s32 message;
} WindowQueue;

/* One laid-out line: a textured sprite for each half, per draw buffer. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} Sprite;

typedef struct {
    Sprite sprite[4];
    u8 unk50[0x10];
} WindowLine;

typedef struct {
    s16 x;          /* cursor column */
    s16 y;          /* cursor row */
    s16 unk4;
    s16 unk6;
    s16 unk8;
    s16 width;
    s16 lines;
    s16 unkE;
    u16 flags;
    s16 stride;
    s16 unk14;
    s16 unk16;
    u8 unk18[4];
    u8 *text;
    s32 unk20;
    u8 unk24[4];
    WindowLine *layout;
    void *image;
    u8 unk30[0x38];
    u8 unk68;
    u8 unk69;
    u8 unk6A;
    u8 unk6B;
    u8 unk6C;
    u8 unk6D;
    u8 unk6E;
    u8 unk6F[0x13];
    s16 queued;
    s16 unk84;
    u8 unk86[6];
    WindowQueue *queue;
} Window;

extern void *func_80031BDC(s32 size, s32 mode);
extern s32 func_800320E8(void *data);
extern void func_800324B8(s16 kind);

#endif
