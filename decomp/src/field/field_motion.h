#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

#include "common.h"
#include "field.h"

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

/* An actor's link to the platform it rides (actor +110, 12 bytes). */
typedef struct PlatformLink {
    SVECTOR rotation; /* 0: platform rotation last frame */
    s16 radius;       /* 8: distance to the platform */
    s16 unkA;
} PlatformLink;

extern s16 D_800ADFC4[4]; /* terrain push speeds */
extern u16 D_800ADFA8[8]; /* terrain push angles */
extern s32 func_800825AC(s32 from, s32 to);

/* A descriptor's collision model for the polygon check (80083288). */
typedef struct {
    u8 unk00[6];
    u16 groups;        /* 06 */
    SVECTOR *vertices; /* 08 */
    u8 unk0C[4];
    u32 *prims;        /* 10: per group a header word (type, flags, count << 16),
                        * then 8-byte items of vertex indices */
} PolyModel;

/* The polygon check's scratchpad work area (0xb8 bytes). */
typedef struct {
    s32 packed[4];     /* 00: projected vertices, x << 16 | z */
    s32 point;         /* 10: the queried x << 16 | z */
    SVECTOR v[4];      /* 14: transformed vertices */
    SVECTOR p;         /* 34: query point; vy receives the height */
    s32 flag;          /* 3C */
    MATRIX transform;  /* 40 */
    MATRIX local;      /* 60 */
    MATRIX view;       /* 80 */
    s32 lowest;        /* A0 */
    SVECTOR *vertices; /* A4 */
    u8 unkA8[4];
    s32 type;          /* AC */
    SVECTOR angles;    /* B0 */
} PolyCheck;

extern u32 *func_8007CD3C(s32 words);
extern void func_8007CD60(s32 words);

/* Talk and touch triggers (8008399c). */
extern s32 D_800ADF64; /* touch latch */
extern s32 D_80285988; /* 801e module: interaction debug flag */
extern s32 func_800A3090(s32 actor, s32 event);

/* The per-actor motion stages of the field update (8008110c). */
extern s32 D_800AF858;
extern s32 D_80065B08; /* actor in motion */
extern void func_800815F0(void);
extern void func_80082620(s32 index, FieldDescriptor *descriptor, FieldActor *actor);
extern void func_80082BB8(s32 index, FieldDescriptor *descriptor, FieldActor *actor);
extern void func_8008399C(s32 index, FieldDescriptor *descriptor, FieldActor *actor);
extern void func_80084158(s32 index, FieldDescriptor *descriptor, FieldActor *actor);

#endif
