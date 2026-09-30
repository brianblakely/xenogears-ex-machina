#ifndef OVL2143_OVL2143_H
#define OVL2143_OVL2143_H

#include "common.h"

/* libgte types. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

/* Scratchpad matrix used as a temporary. */
#define SCRATCH_MATRIX ((MATRIX *)0x1F800000)

/* A model record of a relocated model group (0x38 bytes; the resident
 * relocates the group's offsets, 8002c3e8). */
typedef struct {
    u8 header[0x34];
    s32 packet_bytes; /* +34: size of one buffer's packets */
} ModelRecord;

/* The model records of a relocated group (they follow its 0x10-byte header). */
typedef struct {
    ModelRecord **models;
    u32 count;
} ModelList;

/* A hierarchy entry: a model index (ffff: none) and its parent entry. */
typedef struct {
    u16 model;
    u16 parent;
} HierarchyLink;

/* One node of a model hierarchy (0x7c bytes); element 0 is the root and holds
 * the node count. */
typedef struct ModelPart {
    struct ModelPart *parent;
    u8 dirty;       /* +4: world matrix must be recomposed */
    u8 rotate;      /* +5: local matrix must be rebuilt from rot */
    u8 yxz;         /* +6: RotMatrixYXZ instead of RotMatrix */
    u8 visible;     /* +7 */
    u16 model;      /* +8: model index, ffff none */
    u16 count;      /* +a: root: node count; else node index */
    MATRIX local;   /* +c */
    MATRIX world;   /* +2c */
    s16 scale[3];   /* +4c */
    s16 pad52;
    SVECTOR rot;    /* +54 */
    s32 pos[3];     /* +5c */
    void *packets[2]; /* +68: the model's packets for both buffers */
    s32 w70;
    s32 w74;
    s32 w78;
} ModelPart;

/* Resident heap. */
void func_80032498(s32 tag, s32 mode);        /* select the allocation tag */
void *func_80031BDC(s32 size, s32 mode);      /* allocate */
void func_800320E8(void *block);              /* release */

/* Resident models. */
u32 func_8002C3E8(void *group);               /* relocate a model group; returns model count */
void func_8002CB54(ModelRecord *model, void **packets0, void **packets1); /* allocate both packet buffers */
void func_8002C8CC(ModelRecord *model, void *packets, s32 mode); /* build a model's packets */
void func_8002CC10(s16 a, s16 b);
void func_8002CC74(s16 a, s16 b);
void *func_8003F968(void *dst, void *src, s32 size); /* memcpy */
void func_8002C700(ModelRecord *model, void *packets, s32 arg2, s32 arg3); /* draw a model's packets */
void func_8002CBBC(ModelRecord *model);       /* release a model's own packets */

/* libgte. */
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m);        /* RotMatrix */
MATRIX *func_8004A92C(SVECTOR *rot, MATRIX *m);        /* RotMatrixYXZ */
MATRIX *func_8004920C(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* MulMatrix0 */
MATRIX *func_8004931C(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* CompMatrix */
void func_80049EFC(MATRIX *m);                /* SetRotMatrix */
void func_80049F2C(MATRIX *m);                /* SetLightMatrix */
void func_80049F8C(MATRIX *m);                /* SetTransMatrix */

/* This overlay. */
void func_801DCD8C(ModelPart *parts);

#endif
