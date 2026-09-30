#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

#include "common.h"

/* An object of the 801e module layer table; +128/+130 hold its planar
 * speed. */
typedef struct {
    u8 unk000[0x128];
    s32 speed_x;     /* 128 */
    u8 unk12C[4];
    s32 speed_z;     /* 130 */
} LayerObject;

extern u8 *D_801E8670[]; /* 801e module layers (LayerObject) */

/* One 0x48-byte record of the controlled actor's movement history (32 at
 * 800b14f0, newest at 800b2360, filled downward). */
typedef struct {
    u32 flags;          /* 00: actor +000 */
    u32 layer_flags;    /* 04: actor +004 */
    s16 position[3];    /* 08: whole x, y, z */
    s16 unk0E;
    u16 model84;        /* 10: model +84 */
    s16 unk12;          /* 12: actor +0e8 */
    s16 heading;        /* 14 */
    s16 triangle[4];    /* 16 */
    u8 unk1E[2];
    s32 model_c[3];     /* 20: model +0c..+14 */
    u8 unk2C[4];
    s32 unk30[3];       /* 30: actor +050 */
    u8 unk3C[4];
    u32 unk40;          /* 40: actor +014 */
    u8 layer;           /* 44 */
    u8 unk45[3];
} FieldHistory;

extern FieldHistory D_800B14F0[32];
extern s32 D_800C3910; /* history reset */

#endif
