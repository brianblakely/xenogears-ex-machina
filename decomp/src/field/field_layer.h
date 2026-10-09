#ifndef FIELD_FIELD_LAYER_H
#define FIELD_FIELD_LAYER_H

/* The 801e module (file 6b9) and its layers: the module's layer objects, the
 * files the field loads for it (two per layer), and the module entries the
 * field calls. The work block keeps the field's view of each layer. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/cd.h"

/* A layer's model. */
typedef struct {
    u8 unk00[0x56];
    s16 facing;      /* 56 */
    u8 unk58[0x5C - 0x58];
    s32 x;           /* 5C */
    u8 unk60[0x64 - 0x60];
    s32 z;           /* 64 */
} LayerModel;

/* An object of the 801e module's layer table. */
typedef struct {
    u8 unk000[4];
    LayerModel *model; /* 004 */
    u8 unk008[0x1C - 0x8];
    s16 scale;       /* 01C */
    u8 unk01E[0x34 - 0x1E];
    u8 active;       /* 034: drawn */
    u8 unk035[0x4A - 0x35];
    u16 unk4A;       /* 04A: bit 0 hidden */
    u8 unk04C[0x60 - 0x4C];
    s16 y;           /* 060 */
    u8 unk062[0x128 - 0x62];
    s32 speed_x;     /* 128 */
    u8 unk12C[4];
    s32 speed_z;     /* 130 */
} LayerObject;

extern LayerObject *D_801E8670[]; /* the module's layers */
extern void *D_801E8644;

/* The field's side (field.c). */
extern void *D_800ADB20;           /* the module */
extern s32 D_800ADB1C;             /* the module is loaded */
extern FileRequest D_800B2394[10]; /* its file list: two files per layer, the module, the zero end */
void func_80077884(void);          /* load the module and its layers' files */
void func_80077AB4(void);          /* start its layers */
void func_80077C60(void);          /* load, then start */

/* Module entries. */
void func_801E7378(s32 on);
void func_801E738C(s32 a0);
void func_801E742C(s32 layer, s32 a1, void *resource_a, void *resource_b, s32 y, s32 a5, s32 a6, s32 a7,
                   SVECTOR *angles); /* create a layer */
void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b); /* pose a matrix (the bone routine) */
void func_801E7D14(MATRIX *world, s16 (*table)[3], u_long *ot, s32 buffer, s32); /* draw the layers */
void func_801E7FD4(void);          /* stop */
void func_801E8030(s32 layer);
void func_801E8330(s32 layer, s32, s32 frame); /* set a layer's frame */

#endif
