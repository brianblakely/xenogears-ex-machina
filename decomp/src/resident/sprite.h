#ifndef RESIDENT_SPRITE_H
#define RESIDENT_SPRITE_H

#include "gpu.h"

/* A task node of the sprite engine's lists. */
typedef struct Task {
    struct Task *owner;
    u8 unknown4[4];
    void (*update)(struct Task *task);  /* +0x8 */
    void (*destroy)(struct Task *task); /* +0xc */
    u32 serial;                          /* +0x10 */
    u32 generation;                      /* +0x14: bit 31 active */
    struct Task *next;                   /* +0x18 */
} Task;

/* Resident sprite/actor engine (the unit around 0x8001c8dc-0x8002709c).
 * Only the fields the recovered functions use are named. */
typedef struct {
    u8 unknown0[0x28];
    u8 red, green, blue;     /* +0x28: colour of one-sided parts */
    u8 colour_flags;         /* +0x2b: bit 0 set: no colour */
    u8 unknown2c[0x14];
    u32 flags;               /* +0x40: bits 8-12 facing group */
    u8 unknown44[8];
    s32 resource;            /* +0x4c */
    u8 unknown50[0x18];
    void *callback;          /* +0x68: completion callback */
    u8 unknown6c[0x1C];
    u8 *frames;              /* +0x88 */
    s8 stack_top;            /* +0x8c: byte stack index, growing down */
    u8 unknown8d;
    u8 stack[0x1E];          /* +0x8e */
    u32 flags2;              /* +0xac: bits 7-18 gravity divisor */
} Sprite;

void func_8001F6B0(Sprite *sprite); /* recolour the parts */

#endif
