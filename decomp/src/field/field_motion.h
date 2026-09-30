#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

#include "common.h"

/* The model of an 801e module layer object. */
typedef struct {
    u8 unk00[0x56];
    s16 unk56;       /* 56: facing */
    u8 unk58[0x5C - 0x58];
    s32 unk5C;       /* 5C: x */
    u8 unk60[0x64 - 0x60];
    s32 unk64;       /* 64: z */
} LayerModel;

/* An object of the 801e module layer table; +128/+130 hold its planar
 * speed. */
typedef struct {
    u8 unk000[4];
    LayerModel *model; /* 004 */
    u8 unk008[0x1C - 0x8];
    s16 unk1C;       /* 01C: scale */
    u8 unk01E[0x34 - 0x1E];
    u8 unk34;        /* 034: drawn */
    u8 unk035[0x4A - 0x35];
    u16 unk4A;       /* 04A: bit 0 hidden */
    u8 unk04C[0x60 - 0x4C];
    s16 unk60;       /* 060: y */
    u8 unk062[0x128 - 0x62];
    s32 speed_x;     /* 128 */
    u8 unk12C[4];
    s32 speed_z;     /* 130 */
} LayerObject;

extern u8 *D_801E8670[]; /* 801e module layers (LayerObject) */
#define LAYER_OBJECT(i) ((LayerObject *)D_801E8670[i])

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
