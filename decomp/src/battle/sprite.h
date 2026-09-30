#ifndef BATTLE_SPRITE_H
#define BATTLE_SPRITE_H

/* The battle's view of the resident sprite engine's sprites (resident
 * sprite.h's Sprite and SpriteRenderer; fields as far as the battle uses
 * them), and its sprite effects (800B4EDC-800B6004). */

#include "common.h"
#include "psyq.h"

/* A 16.16 coordinate, read whole or by its integer half. */
typedef union {
    s32 fixed;
    struct {
        u16 frac;
        s16 whole;
    } part;
} Fixed16;

/* An anchor of a sprite's frame (8 bytes): an offset and angles. */
typedef struct {
    s8 x;
    s8 y;
    s16 angle[3];
} SpriteAnchor;

/* A drawn part of a sprite (resident SpritePart, 0x18 bytes). */
typedef struct {
    s16 x, y;
    u8 u, v;   /* 0x04 */
    u8 w, h;   /* 0x06 */
    u8 byte8, byte9;
    u16 tpage; /* 0x0A */
    u16 clut;  /* 0x0C */
} SpriteImagePart;

/* A sprite's renderer (resident SpriteRenderer). */
typedef struct {
    s16 angle[3];            /* 0x00 */
    s16 scale[3];            /* 0x06 */
    Matrix matrix;           /* 0x0C: local screen matrix */
    void *parts;             /* 0x2C */
    SpriteImagePart *part;   /* 0x30 */
    SpriteAnchor *anchors;   /* 0x34 */
} SpriteView;

/* A resident sprite (fields as far as used). */
typedef struct {
    Fixed16 x, y, z;       /* 0x00 */
    u8 pad0C[0x20 - 0xC];
    SpriteView *view;      /* 0x20 */
    u8 pad24[0x2C - 0x24];
    s16 scale;             /* 0x2C */
    u8 pad2E[0x34 - 0x2E];
    u16 frame;             /* 0x34 */
    u8 pad36[0x3C - 0x36];
    union {
        u32 word;
        u8 bytes[4];
    } render;              /* 0x3C: bits 0-1 sides, bits 5-7 blend */
    u8 pad40[0x6C - 0x40];
    s32 owner;             /* 0x6C: its task list */
    u8 pad70[0xA0 - 0x70];
    s16 target[3];         /* 0xA0 */
    u8 padA6[0xAC - 0xA6];
    union {
        u32 word;
        s8 bytes[4];
    } motion;              /* 0xAC: bit 2 mirrored */
} BattleSprite;

extern u8 D_800C356C[5]; /* the anchors of a sprite's trail */

void func_8004A414(Vector *v, Vector *out); /* Square0 */

#endif
