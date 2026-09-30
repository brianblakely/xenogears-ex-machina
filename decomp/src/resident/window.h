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

/* One laid-out line: per draw buffer, a textured sprite for each half
 * (the first 0x40 columns, then the rest). */
typedef struct {
    SPRT sprite[2][2];
    RECT rect;       /* the line's glyph image in VRAM */
    s16 width;       /* columns drawn so far */
    u8 plane;        /* glyph plane (odd lines share an image) */
    u8 slot;
    u8 row;          /* texture row of the line's image */
    u8 unk5D;
    u16 clut;
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
    s16 unk18;      /* lines started */
    u8 unk1A[2];
    u8 *text;
    s32 unk20;
    u8 unk24[4];
    WindowLine *layout;
    void *image;
    u8 unk30[0xC];  /* primitives linked after the text, */
    u8 unk3C[0xC];  /* and between the two sprite passes */
    TILE tile[2];   /* background, per buffer */
    u8 unk68;
    u8 unk69;
    u8 unk6A;
    u8 unk6B;
    u8 unk6C;
    u8 unk6D;
    u8 unk6E;
    u8 unk6F[0x13];
    s16 queued;
    s16 unk84;      /* frames to wait */
    s16 unk86;      /* frames to the next glyphs */
    s16 unk88;      /* frames between glyphs */
    u8 unk8A[2];
    WindowQueue *queue;
} Window;

void func_80033DF0(Window *window);
void func_80034888(Window *window, u_long *ot, s32 buffer); /* draw */

/* Hand-written ordering table link helper (800315a0-80031894). */
void func_80031798(u_long *ot, void *prim);

#endif
