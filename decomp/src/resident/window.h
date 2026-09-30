#ifndef RESIDENT_WINDOW_H
#define RESIDENT_WINDOW_H

#include "common.h"
#include "psyq/libgpu.h"
#include "heap.h"

/* Resident message window: laid-out text lines drawn as sprites. Field
 * names follow their observed use; unknown bytes keep their offsets. */

/* A queued follow-up message (window list at 0x8C). */
typedef struct WindowQueue {
    struct WindowQueue *next;
    s32 message;
} WindowQueue;

/* One laid-out line: a textured sprite for each half, per draw buffer. */
typedef struct {
    SPRT sprite[4];
    u8 unk50[8];
    s16 unk58;
    u8 unk5A;
    u8 unk5B[5];
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

void func_80033DF0(Window *window);

#endif
