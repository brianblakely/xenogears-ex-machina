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
/* A sprite's renderer (its part list header). */
typedef struct {
    u8 unknown0[6];
    s16 scale_x, scale_y, scale_z; /* +0x6 */
} SpriteRenderer;

typedef struct {
    s32 x, y, z;                /* +0x0: position (16.16) */
    s32 speed_x, speed_y, speed_z; /* +0xc */
    s32 word18;                 /* +0x18 */
    s32 word1c;                 /* +0x1c */
    SpriteRenderer *renderer;   /* +0x20 */
    u8 unknown24[4];
    u8 red, green, blue;     /* +0x28: colour of one-sided parts */
    u8 colour_flags;         /* +0x2b: bit 0 set: no colour */
    s16 scale;               /* +0x2c */
    u8 unknown2e[4];
    u16 half32;              /* +0x32 */
    u8 unknown34[6];
    u16 rate;                /* +0x3a: speed factor, 1024 = 1 */
    u32 render_flags;        /* +0x3c: bit 28 orientation dirty */
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
    u8 byteb0;               /* +0xb0 */
} Sprite;

void func_8001F6B0(Sprite *sprite); /* recolour the parts */
void func_80022090(Sprite *sprite); /* rebuild the orientation */
void func_80022974(Sprite *sprite);
void func_80022B2C(Sprite *sprite);
s32 func_80022CAC(Sprite *sprite, s32 value);

#endif
