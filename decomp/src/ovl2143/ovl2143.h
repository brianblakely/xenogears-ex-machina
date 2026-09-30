#ifndef OVL2143_OVL2143_H
#define OVL2143_OVL2143_H

#include "common.h"
#include "gte.h"

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

/* libgpu rectangle. */
typedef struct {
    s16 x, y, w, h;
} RECT;

/* libgte 32-bit vector. */
typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

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

/* A curve mapping time to a frame: func_801E0850/08d4/0938/0988. */
typedef s16 (*FrameCurve)(s16 time, s16 divisor, s32 base);

/* A row of three colours. */
typedef struct {
    u16 c[3];
} ColorRow;

/* An image animation (0x30 bytes, an actor's records30): a VRAM rectangle
 * whose pixels are rebuilt each time the curve selects another frame. */
typedef struct ImageAnim {
    struct ImageAnim *target; /* +0: image the frames are copied into */
    u16 *pixels;            /* +4 */
    u16 *pixels2;           /* +8 */
    u16 *work;              /* +c */
    u8 mode;                /* +10: 0/1 resident decoders, 4/5 fades */
    u8 dirty;               /* +11 */
    s16 h12;                /* +12 */
    u16 time;               /* +14 */
    s16 speed;              /* +16 */
    u16 frame;              /* +18 */
    u16 active;             /* +1a */
    ColorRow *colors;       /* +1c */
    s16 divisor;            /* +20 */
    s16 base;               /* +22 */
    FrameCurve curve;       /* +24 */
    RECT rect;              /* +28 */
} ImageAnim;

/* An owner of a view record whose halfword +14 scripts read. */
typedef struct {
    u8 pad0[0x14];
    u16 h14;
} View;

typedef struct {
    u8 pad0[8];
    View *view;
} ViewOwner;

/* An actor's 0x24-byte record (records24): four heap blocks. */
typedef struct {
    u8 pad0[0x14];
    void *block14;          /* +14 */
    void *block18;          /* +18 */
    void **block1C;         /* +1c */
    void *block20;          /* +20 */
} Record24;

/* A keyframe: rotations then positions of the nodes after the root. */
typedef struct {
    u8 pad0[4];
    u16 flags;              /* +4: bit 0 no rotations, bit 1 no positions */
    s16 packed;             /* +6: 0: rotations follow a skipped block */
    u8 pad8[4];
    u16 rot_count;          /* +c */
    u16 pos_count;          /* +e */
    u8 pad10[8];
    s16 data[1];            /* +18 */
} Keyframe;

/* A colour fade record. */
typedef struct {
    s16 h0;
    u8 b2;
    u8 b3;
    s32 w4;
    s32 w8;
    s16 hC;
    s16 hE;
    s16 h10;
    u8 pad12[2];
    s16 h14;
    s16 h16;
    s16 h18;
    u8 pad1A[0x42];
    s16 time;               /* +5c */
    s16 h5E;                /* +5e */
    s16 h60;                /* +60: at most 7 */
    s16 duration;           /* +62 */
    s16 color[3];           /* +64: 10.6 fixed point */
    s16 step[3];            /* +6a */
} ColorFade;

/* An animation record: frame count and the offset of its frame data. */
typedef struct {
    u8 pad0[2];
    u16 loop;               /* +2 */
    u8 pad4[0xE];
    u16 frames;             /* +12 */
    s32 data;               /* +14: offset of the frame data */
} Animation;

/* A table of script entry points. */
typedef struct {
    s32 count;
    s32 *entries;
} EntryTable;

/* A scene actor: a model hierarchy with its script state. */
typedef struct Actor {
    ModelList *models;      /* +0 */
    ModelPart *parts;       /* +4 */
    s32 *entries;           /* +8: script entry points */
    EntryTable *shared;     /* +c: entries 0x50- */
    s32 pc;                 /* +10: script position */
    s32 *locals;            /* +14: [0] count, then entries 0-0x3f */
    s32 *globals;           /* +18: [0] count, then entries 0x40- */
    s16 scale;              /* +1c */
    s16 h1E;                /* +1e */
    u8 index;               /* +20: slot in D_801E8670 */
    u8 b21;                 /* +21 */
    u8 b22;                 /* +22 */
    u8 b23;                 /* +23 */
    s16 size[3];            /* +24: height, x and z extents */
    u8 reference;           /* +2a: default entry reference (bit 7 flag) */
    u8 depth;               /* +2b: queued calls + 1 */
    u8 queue_source[4];     /* +2c */
    u8 queue_entry[4];      /* +30 */
    u8 active;              /* +34 */
    u8 b35;                 /* +35 */
    u8 b36;                 /* +36 */
    u8 scaled;              /* +37: build with the scaled hierarchy update */
    u8 b38;                 /* +38 */
    u8 b39;                 /* +39 */
    s16 h3A;                /* +3a */
    u16 h3C;                /* +3c */
    s16 h3E;                /* +3e */
    u16 h40;                /* +40 */
    u16 h42;                /* +42 */
    u16 h44;                /* +44 */
    u16 h46;                /* +46 */
    s16 h48;                /* +48 */
    u16 flags;              /* +4a */
    s32 w4C;                /* +4c */
    s32 w50;                /* +50 */
    s32 w54;                /* +54 */
    s16 aim_actor;          /* +58: reference of the actor aimed at */
    s16 aim_node;           /* +5a */
    u8 parent;              /* +5c: actor carrying this one, 0xff none */
    u8 inherit;             /* +5d: take the carrier node's rotation */
    s16 parent_node;        /* +5e */
    s16 h60;                /* +60: root height */
    u8 b62;                 /* +62 */
    u8 b63;                 /* +63 */
    s16 aim_offset[3];      /* +64: point aimed at in the node's space */
    s16 offset[3];          /* +6a: position in the carrier node's space */
    s16 h70[12];            /* +70 */
    s16 target[3];          /* +88 */
    s16 h8E;                /* +8e */
    s16 h90[4];             /* +90 */
    s16 anim_state;         /* +98: -1 none */
    s16 anim_loop;          /* +9a: -1 no loop */
    s16 anim_frame;         /* +9c */
    s16 anim_frames;        /* +9e */
    u8 *anim_start;         /* +a0 */
    u8 *anim_pos;           /* +a4 */
    s32 wA8;                /* +a8 */
    s32 wAC;                /* +ac */
    ViewOwner *ownerB0;     /* +b0 */
    ViewOwner *ownerB4;     /* +b4 */
    u8 padB8[0x52];
    u16 mask;               /* +10a */
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

/* A resident sprite object (func_80023fd8); its link record lies `link`
 * bytes past its start. */
typedef struct {
    s32 x, y, z;            /* 16.16 position */
} SpriteBody;

typedef struct {
    u8 pad0[0x38];
    SpriteBody body;        /* +38 */
    u8 pad44[0x7A];
    s16 link;               /* +be */
} Sprite;

/* This overlay's link of a sprite to an actor node. */
typedef struct {
    u8 pad0[4];
    void (*update)(Sprite *sprite); /* +4: the sprite's own update */
    Actor *actor;           /* +8 */
    s16 node;               /* +c */
    s16 follow;             /* +e: take the height from the actor */
    SVECTOR offset;         /* +10 */
} SpriteLink;

/* A sprite attachment of an actor script. */
typedef struct {
    u8 pad0[5];
    u8 node;                /* +5 */
    u16 offset[3];          /* +6 */
    u8 follow;              /* +c */
    u8 padD[6];
    u8 linked;              /* +13 */
} SpriteSpec;

/* Resident sprites. */
Sprite *func_80023FD8(s32 a, s32 b, s32 c, s32 size);
void func_80021FE0(SpriteBody *body, s32 value);
void func_800223B0(SpriteBody *body, s32 value);
void func_80022000(SpriteBody *body, s32 scale);
void *func_8001CD7C(Sprite *sprite);          /* the sprite's update */
void func_8001CD6C(Sprite *sprite, void (*update)(Sprite *sprite)); /* set it */
void func_8004A480(void *a, void *b, VECTOR *out);

/* Resident models. */
u32 func_8002C3E8(void *group);               /* relocate a model group; returns model count */
void func_8002CB54(ModelRecord *model, void **packets0, void **packets1); /* allocate both packet buffers */
void func_8002C8CC(ModelRecord *model, void *packets, s32 mode); /* build a model's packets */
void func_8002CC10(s16 a, s16 b);
void func_8002CC74(s16 a, s16 b);
void *memcpy(void *dst, void *src, s32 size); /* memcpy */
void func_8002C700(ModelRecord *model, void *packets, s32 arg2, s32 arg3); /* draw a model's packets */
void func_8002CBBC(ModelRecord *model);       /* release a model's own packets */

/* libgpu. */
void LoadImage(RECT *rect, u16 *pixels);     /* LoadImage */
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
u16 GetClut(s32 x, s32 y);                  /* GetClut */
void SetSemiTrans(void *p, s32 abe);            /* SetSemiTrans */
void SetPolyFT4(POLY_FT4 *p);                 /* SetPolyFT4 */

/* Resident maths. */
s32 func_8003F8CC(s16 angle);                    /* cosine (4096 = 1.0) */
s32 SquareRoot0(s32 value);                    /* square root */

/* libgte. */
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m);        /* RotMatrix */
MATRIX *func_8004A92C(SVECTOR *rot, MATRIX *m);        /* RotMatrixYXZ */
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* MulMatrix0 */
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2); /* CompMatrix */
void SetRotMatrix(MATRIX *m);                /* SetRotMatrix */
void SetLightMatrix(MATRIX *m);                /* SetLightMatrix */
void SetTransMatrix(MATRIX *m);                /* SetTransMatrix */

extern View *D_8005919C;

/* This overlay's data. */
typedef struct {
    s32 w0;
    s32 w4;
} Pair;

/* A point attached to an actor node (0x14 bytes): its world position is
 * the node's matrix applied to `offset` (func_801E1880). */
typedef struct {
    s16 pos[3];
    s16 active;             /* +6 */
    SVECTOR offset;         /* +8 */
    s16 actor;              /* +10: -1 none */
    s16 node;               /* +12 */
} Anchor;

extern s32 D_801E85CC;
extern Pair D_801E85F4[8];
extern u16 D_801E863C;
extern s32 D_801E8640;
extern Anchor D_801E8648[2];
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
void func_801E35D0(Actor *actor, Actor *source, SlotPool *pool, s32 entry);
void func_801E39F0(Actor *actor, SlotPool *pool, s32 arg2, s32 arg3, s32 arg4);
s32 func_801E67F8(void);
s32 func_801E08D4(s16 value, s16 divisor, s32 base);
void func_80049BDC(MATRIX *m, MATRIX *out);  /* out = m * out (rotation) */
u32 func_801DC5C0(ModelPart *parts, s32 scale);
u32 func_801DC848(ModelPart *parts, s32 scale);
s32 func_801DDBF8(SlotPool *pool, ModelPart *parts, u16 arg2, s16 scale);
void func_801E5D44(Actor *actor, SlotPool *pool, s32 arg2);
void func_801E7298(Actor *actor);
void func_801E6F64(Sprite *sprite);
void func_801E8030(s32 index);
void func_801E011C(ParticlePool *pool);
void func_801DF6A8(SlotPool *pool);
s32 func_801DF7A8(SlotPool *pool, PoolSlot *slot);
void func_801DCD8C(ModelPart *parts);

#endif
