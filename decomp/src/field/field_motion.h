#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

#include "common.h"

/* The motion part of a resident sprite: its 16.16 velocity. */
typedef struct FieldSpriteMotion {
    u8 unk00[0x0C];
    s32 velocity[3]; /* 0C: x, y, z */
    s32 unk18;       /* 18 */
} FieldSpriteMotion;

/* An object of the 801e module layer table; +128/+130 hold its planar
 * speed. */
typedef struct {
    u8 unk000[0x128];
    s32 speed_x;     /* 128 */
    u8 unk12C[4];
    s32 speed_z;     /* 130 */
} LayerObject;

extern u8 *D_801E8670[]; /* 801e module layers (LayerObject) */

extern void func_80021FE0(FieldSpriteMotion *sprite, s32 heading);

#endif
