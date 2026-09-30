#ifndef BATTLE_MODEL_H
#define BATTLE_MODEL_H

#include "common.h"

/* GTE rotation/translation matrix (PsyQ MATRIX). */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} Matrix;

/* PsyQ SVECTOR. */
typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVector;

/* PsyQ POLY_FT4 (textured quadrilateral primitive, 0x28 bytes). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} PolyFT4;

/* A model's header (fields as far as the battle uses them). */
typedef struct {
    u8 pad0[0x34];
    u32 packetSize;         /* 0x34: bytes of one buffer's packets */
} Model;

/* A table of the models of a relocated model group (0x38 bytes each, from
 * group +0x10). */
typedef struct {
    Model **models;
    u32 count;
} ModelList;

/* An effect entry (0x14 bytes) of an effect pool. */
typedef struct {
    u8 used;
    u8 pad1[2];
    u8 kind;                /* +3: 0xFF persistent */
    u8 pad4[0x14 - 4];
} EffectEntry;

/* A pool of effect entries; next is the first entry that may be free. */
typedef struct {
    EffectEntry *entries;
    u16 next;
    u16 count;
} EffectPool;

/* A posed part of a model hierarchy (0x7C bytes); a hierarchy is a root part
 * followed by one part per (model, parent) pair. */
typedef struct ModelPart {
    struct ModelPart *parent; /* 0x00 */
    u8 flag4;
    u8 flag5;
    u8 flag6;
    u8 flag7;
    u16 modelId;            /* 0x08: 0xFFFF none */
    u16 index;              /* 0x0A: the root holds the part count */
    Matrix transform;       /* 0x0C: rotation and translation (the root's scaled) */
    Matrix world;           /* 0x2C: composed with the parents' */
    s16 scale[3];           /* 0x4C: 4.12 */
    u16 field52;
    SVector rotation;       /* 0x54: angles */
    s32 translation[3];     /* 0x5C */
    void *packets[2];       /* 0x68: one buffer per frame */
    EffectEntry *effects[3]; /* 0x70: attached effects */
} ModelPart;

/* Resident services. */
void func_80032498(s32 tag, s32 quiet);       /* select the heap owner tag */
void *func_80031BDC(u32 size, s32 mode);      /* allocate */
u32 func_8002C3E8(u8 *group);                 /* relocate a model group; its count */
void func_8002CB54(Model *model, void **packets0, void **packets1); /* allocate packets */
void func_8002CC10(s16 x, s16 y);
void func_8002CC74(s16 x, s16 y);
void func_8002C8CC(Model *model, void *packets, s32 mode); /* build packets */
void func_8003F968(void *to, void *from, u32 size);        /* copy */
void func_800320E8(void *block);                           /* free */
void func_8002CBBC(Model *model);                          /* release a model */

void func_8009F708(ModelPart *root);
void func_800A22E8(EffectPool *pool);
s32 func_800A23E8(EffectPool *pool, EffectEntry *entry);

/* Graphics library. */
void func_80043CB0(PolyFT4 *p);                              /* SetPolyFT4 */
void func_80043BFC(void *p, s32 abe);                        /* SetSemiTrans */
u16 func_80043A58(s32 x, s32 y);                             /* GetClut */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);            /* GetTPage */

/* GTE library. */
void func_8003F738(SVector *angles, Matrix *m);              /* RotMatrix */
void func_8004A92C(SVector *angles, Matrix *m);              /* RotMatrixYXZ */
void func_8004920C(Matrix *m0, Matrix *m1, Matrix *out);     /* MulMatrix0 */
void func_8004931C(Matrix *m0, Matrix *m1, Matrix *out);     /* CompMatrix */
void func_80049EFC(Matrix *m);                               /* SetRotMatrix */
void func_80049F2C(Matrix *m);                               /* SetLightMatrix */
void func_80049F8C(Matrix *m);                               /* SetTransMatrix */
void func_8002C700(Model *model, void *packets, s32 arg2, s32 arg3); /* draw a model */

#endif
