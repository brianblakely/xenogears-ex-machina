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

/* libgpu textured quad. */
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
} POLY_FT4;

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
    struct PoolSlot *attachments[3]; /* +70: pool slots attached to the node */
} ModelPart;

/* A pool slot (0x14 bytes): a tween attached to a model node. */
typedef struct PoolSlot {
    u8 used;
    u8 flag;
    u8 kind;        /* 3: rotation, 7 + n: movement */
    u8 tag;         /* 0xff: kept by func_801DFE8C */
    s16 value[6];   /* +4: start values and deltas / targets */
    s16 time;       /* +10 */
    s16 duration;   /* +12 */
} PoolSlot;

/* A pool of slots with the position where the search for a free one starts. */
typedef struct {
    PoolSlot *slots;
    u16 next;
    u16 capacity;
} SlotPool;

/* A particle (0x7c bytes): a quad of four vertices, a colour fading each
 * tick, and its quad for both buffers. */
typedef struct {
    s16 x0, y0, z0, pad06;
    s16 x1, y1, z1;
    s16 projected;  /* +e: vertices are 3D, projected with the GTE */
    s16 x2, y2, z2;
    s16 age;        /* +16: -1 free */
    s16 x3, y3, z3;
    s16 lifetime;   /* +1e */
    u16 color[3];   /* +20: 10.6 fixed point */
    s16 fade[3];    /* +26: per tick */
    POLY_FT4 poly[2]; /* +2c */
} Particle;

/* A pool of particles with a spare one past the end. */
typedef struct {
    Particle *items;
    s16 capacity;
    s16 next;
} ParticlePool;

/* An actor's 0x70-byte channel record. */
typedef struct {
    s16 id;         /* -1: unused */
    u8 pad2[6];
    s32 w8;
    u8 rest[0x64];
} Channel;

/* A scene actor: a model hierarchy with its script state. */
typedef struct Actor {
    ModelList *models;      /* +0 */
    ModelPart *parts;       /* +4 */
    u8 pad08[0x14];
    s16 scale;              /* +1c */
    u8 pad1E[6];
    s16 size[3];            /* +24: height, x and z extents */
    u8 pad2A[0xB];
    u8 b35;                 /* +35 */
    u8 pad36[0x14];
    u16 flags;              /* +4a */
    u8 pad4C[0xC0];
    u8 channel_count;       /* +10c */
    u8 count10D;            /* +10d: 0x24-byte records at +114 */
    u8 count10E;            /* +10e: 0x30-byte records at +118 */
    u8 pad10F;
    Channel *channels;      /* +110 */
    void *records24;        /* +114 */
    void *records30;        /* +118 */
    s32 previous[3];        /* +11c: root position before the step */
    s32 moved[3];           /* +128: root movement of the step */
} Actor;

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

/* libgpu. */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
u16 func_80043A58(s32 x, s32 y);                  /* GetClut */
void func_80043BFC(void *p, s32 abe);            /* SetSemiTrans */
void func_80043CB0(POLY_FT4 *p);                 /* SetPolyFT4 */

/* Resident maths. */
s32 func_8003F8CC(s16 angle);                    /* cosine (4096 = 1.0) */
s32 func_80048C4C(s32 value);                    /* square root */

/* libgte. */
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m);        /* RotMatrix */
MATRIX *func_8004A92C(SVECTOR *rot, MATRIX *m);        /* RotMatrixYXZ */
MATRIX *func_8004920C(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* MulMatrix0 */
MATRIX *func_8004931C(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* CompMatrix */
void func_80049EFC(MATRIX *m);                /* SetRotMatrix */
void func_80049F2C(MATRIX *m);                /* SetLightMatrix */
void func_80049F8C(MATRIX *m);                /* SetTransMatrix */

/* This overlay's data. */
typedef struct {
    s32 w0;
    s32 w4;
} Pair;

typedef struct {
    s16 h0;
    s16 h2;
    u8 rest[0x10];
} Record14;

extern s32 D_801E85CC;
extern Pair D_801E85F4[8];
extern u16 D_801E863C;
extern s32 D_801E8640;
extern Record14 D_801E864C[2];
extern Actor *D_801E8670[10];
extern s16 D_801E869C;
extern ParticlePool D_801E86A0;
extern SlotPool D_801E86A8;
extern u16 D_801E86B0;

/* This overlay. */
SlotPool *func_801DF5F4(SlotPool *pool, s32 capacity);
PoolSlot *func_801DF6F0(SlotPool *pool);
ParticlePool *func_801E0064(ParticlePool *pool, s32 capacity);
void func_801DF668(SlotPool *pool);
void func_801E00DC(ParticlePool *pool);
void func_801E35D0(Actor *actor, s32 source, SlotPool *pool, s32 arg3);
void func_801E8030(s32 index);
void func_801E011C(ParticlePool *pool);
void func_801DF6A8(SlotPool *pool);
s32 func_801DF7A8(SlotPool *pool, PoolSlot *slot);
void func_801DCD8C(ModelPart *parts);

#endif
