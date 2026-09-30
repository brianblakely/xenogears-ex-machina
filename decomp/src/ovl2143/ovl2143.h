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

/* libgpu gouraud textured triangle. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
} POLY_GT3;

void SetPolyGT3(POLY_GT3 *p);

/* libgpu rectangle. */
typedef struct {
    s16 x, y, w, h;
} RECT;

/* libgte 32-bit vector. */
typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

/* libgpu primitive tag and addPrim. */
typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
} P_TAG;

#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (u32)(_addr))
#define getaddr(p) (u32)(((P_TAG *)(p))->addr)
#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)

extern s32 D_80050100; /* ordering-table depth shift */

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
    s16 billboard;  /* +52: 1 upright, 2 fully facing the view */
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
    union {
        s16 value[6];   /* +4: start values and deltas / targets */
        struct {
            u8 *start;  /* +4: keyframe track data */
            u8 *pos;    /* +8 */
        } track;
    } u;
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
typedef struct Particle {
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
typedef struct ParticlePool {
    Particle *items;
    s16 capacity;
    s16 next;
} ParticlePool;

/* An actor's 0x70-byte channel: a ribbon traced by two points of a node,
 * emitted as particles (on screen, or in 3D when `solid`). */
typedef struct {
    s16 id;         /* +0: the node, -1: unused */
    u8 solid;       /* +2 */
    u8 semi_trans;  /* +3 */
    struct ParticlePool *pool; /* +4 */
    struct Particle *particle; /* +8: the one being extended */
    SVECTOR ends[2];           /* +c: the points in the node's space */
    union {
        struct {
            s16 x, y;
        } sxy[2][8];           /* on screen, a ring of 8 frames */
        VECTOR pos[2][2];      /* in 3D, the last two frames */
    } trail;                   /* +1c */
    s16 frame;      /* +5c */
    s16 count;      /* +5e: frames of the current particle */
    s16 max;        /* +60 */
    s16 lifetime;   /* +62 */
    u16 color[3];   /* +64 */
    s16 fade[3];    /* +6a */
} Channel;

/* A curve mapping time to a frame: func_801E0850/08d4/0938/0988 (called
 * without a prototype: time, divisor, base). */
typedef s16 (*FrameCurve)();

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
    u16 speed;              /* +16 */
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

/* An entry of a records24 table (0x10 bytes). */
typedef struct {
    s16 h0, h2, h4, h6, h8, hA, hC, hE;
} Record24Entry;

/* A point of a records24 ring (0x18 bytes): its radius and angle index
 * about the ring's centre; a ring ends with a zero radius. */
typedef struct {
    s16 radius;
    s16 angle;
    s16 centre[3];
    u8 padA[0xE];
} RingPoint;

/* Two triangles' textured primitives (one per buffer) and their vertex
 * indices (0x58 bytes). */
typedef struct {
    u16 index[3];
    u8 pad6[2];
    POLY_GT3 prim[2];
} RingPoly;

/* An actor's 0x24-byte record (records24): a surface of rings of points. */
typedef struct {
    u16 h0;
    u8 pad2[2];
    s16 rings;              /* +4 */
    s16 polys;              /* +6: twice the rings' first point counts */
    s16 points;             /* +8 */
    s16 entry_count;        /* +a */
    u8 b[6];                /* +c */
    u8 pad12[2];
    SVECTOR *centres;       /* +14: a centre per ring */
    Record24Entry *block18; /* +18 */
    RingPoint **block1C;    /* +1c: each ring's first point */
    RingPoly *block20;      /* +20 */
} Record24;

/* A node's tracks of a keyframe (read through a u16 cursor): byte offsets
 * of its rotation and position tracks (0xffff none) and their kinds. */
typedef struct {
    u16 offset[2];          /* rotation, position */
    u8 kind[2];
} TrackEntry;

/* A keyframe: rotations then positions of the nodes after the root. */
typedef struct {
    u8 pad0[2];
    u16 duration;           /* +2 */
    u16 flags;              /* +4: bit 0 no rotations, bit 1 no positions */
    u16 packed;             /* +6: 0: rotations follow a skipped block */
    u8 pad8[4];
    u16 rot_count;          /* +c */
    u16 pos_count;          /* +e */
    u8 pad10[8];
} Keyframe; /* followed by the track entries or packed values */

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

/* An animation event (at `frame` of its animation). */
typedef struct {
    s16 frame;
    u8 type;                /* 1-9 */
    u8 index;
    union {
        struct {            /* type 2: set up an anchor and its light column */
            u8 active;
            u8 fixed;       /* not attached to this actor */
            u8 node;
            u8 color[3];
            s16 offset[3];
            s16 enable;
        } anchor;
        struct {            /* type 7: show or hide a node */
            u8 node;
            u8 visible;
        } show;
        struct {            /* type 8: call an entry of the masked actors */
            u8 pad4;
            u8 entry;
        } call;
        struct {            /* type 9: start an image animation */
            u8 active;
            u8 target;      /* 0xff none */
            u8 mode;        /* bit 7: shifted by the actor */
            u8 curve;
            u16 x, y;
            u16 x2, y2;
            u16 h10;
            u8 b12;         /* high nibble 1: shift the second point too */
            u8 b13;
            u8 b14;
            u8 pad15;
            s16 h16, h18, h1A;
        } image;
        u8 more;            /* types 3/4: nonzero for the long form */
    } u;
} AnimEvent;

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
    s16 h90;                /* +90: -1 none */
    s16 h92;                /* +92 */
    u16 shift_x;            /* +94: added to image animation positions */
    u16 shift_y;            /* +96 */
    s16 anim_state;         /* +98: -1 none */
    s16 anim_loop;          /* +9a: -1 no loop */
    s16 anim_frame;         /* +9c */
    s16 anim_frames;        /* +9e */
    u8 *anim_pos;           /* +a0: next event */
    u8 *anim_start;         /* +a4 */
    void *group;            /* +a8: the model group block */
    void *blockAC;          /* +ac */
    ViewOwner *ownerB0;     /* +b0 */
    ViewOwner *ownerB4;     /* +b4 */
    POLY_FT4 prims[2];      /* +b8: a shadow quad per buffer */
    u8 pad108[2];
    u16 mask;               /* +10a */
    u8 channel_count;       /* +10c */
    u8 count10D;            /* +10d: 0x24-byte records at +114 */
    u8 count10E;            /* +10e: 0x30-byte records at +118 */
    u8 pad10F;
    Channel *channels;      /* +110 */
    Record24 *records24;    /* +114 */
    ImageAnim *records30;   /* +118 */
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

/* Resident image decoders (frame `frame` of a packed image into `out`). */
void func_80026F44(s32 arg0, s32 frame, u16 *out, u16 *pixels);
void func_80026FE8(s32 arg0, s32 frame, u16 *out, u16 *pixels2, u16 *pixels);
void func_801E1708(ImageAnim *anim, s16 level);
void func_801E17B8(ImageAnim *anim, s16 level);

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
extern MATRIX *D_801E8644;
extern s16 D_801E8698;
extern ParticlePool D_801E86A0;
extern SlotPool D_801E86A8;
extern u16 D_801E86B0;

/* An actor's scene description (the file's +10 record's +4). */
typedef struct {
    u8 pad0[2];
    s16 size[3];            /* +2 */
    s16 scale;              /* +8 */
    u8 reference;           /* +a */
    u8 padB;
    u16 flags;              /* +c */
    u8 channel_count;       /* +e */
    u8 padF;
    u8 count30;             /* +10 */
    u8 pad11;
    u8 count24;             /* +12 */
    u8 pad13;
    u16 records[1];         /* +14: the records24 descriptions */
} ActorDesc;

typedef struct {
    u8 pad0[4];
    ActorDesc *desc;        /* +4 */
    s32 *tables[1];         /* +8: per records24 entry */
} ActorInfo;

/* An actor's files: images, the model group (up to the hierarchy links) and
 * its description; and its script block with its sound bank. */
typedef struct {
    u8 pad0[4];
    void *images;           /* +4 */
    u8 *group;              /* +8 */
    HierarchyLink *links;   /* +c: follows the model group */
    ActorInfo *info;        /* +10 */
} ActorFile;

typedef struct {
    u8 pad0[8];
    void *bank;             /* +8 */
    void *end;              /* +c */
} SoundBlock;

typedef struct {
    u8 pad0[4];
    s32 *locals;            /* +4 */
    s32 entries[1];         /* +8 */
} ScriptBlock;

typedef struct {
    u8 pad0[4];
    ScriptBlock *script;    /* +4 */
    ViewOwner *owner;       /* +8: also the sound block */
} ActorScript;

extern s32 D_801E8634;      /* model list in use */
extern u8 *D_801E8638;      /* copy of the model group */

void func_8003342C(void *table);   /* relocate an offset table in place */
s32 func_8003864C(void *bank, s32 arg1);
void func_80038428(void *bank);     /* load a sound effect bank */
void func_80030988(s32 a0, s32 a1, s32 a2, s32 a3);
void func_8002DDE4(void *images, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_8002C644(u8 *group);
void func_8002C4BC(u8 *group);
s32 func_80031894(u8 *group);       /* the group's size */
ModelList *func_801DC22C(u8 *group, ModelList *list);
ModelPart *func_801DC2D0(ModelList *group, HierarchyLink *links, s32 mode, s32 configure,
                         s16 param0, s16 param1, s16 param2, s16 param3);
void func_801E1A14(Record24 *record, u16 *table, s16 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
                   s16 count, s16 tx, s16 ty, s16 u_span, s16 v_span, s16 clut_x, s16 clut_y, u8 b0,
                   u8 b1, u8 b2, u8 b3, u8 b4, u8 b5);
void func_801E3534(Actor *actor, SlotPool *pool, s32 *entries, s32 *locals);
void func_801E8510(Actor *actor);

s16 ratan2(s32 y, s32 x);
s32 func_8003F8B0(s16 angle);                 /* sine (4096 = 1.0) */
MATRIX *func_80049ACC(MATRIX *m, MATRIX *scale); /* scale a matrix's columns */
struct Particle *func_801E0248(struct ParticlePool *pool, s16 semi_trans);
s16 func_801E1258(ImageAnim *anim, s32 ticks);
void func_801E22F8(Record24 *record, SVECTOR *light, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s32 floor);

/* This overlay. */
SlotPool *func_801DF5F4(SlotPool *pool, s32 capacity);
PoolSlot *func_801DF6F0(SlotPool *pool);
ParticlePool *func_801E0064(ParticlePool *pool, s32 capacity);
void func_801DF668(SlotPool *pool);
void func_801E00DC(ParticlePool *pool);
void func_801E35D0(Actor *actor, Actor *source, SlotPool *pool, s32 entry);
void func_801E39F0(Actor *actor, SlotPool *pool, s32 arg2, s32 arg3, s32 arg4);
s32 func_801E67F8(void);
s16 func_801E08D4(s16 value, s16 divisor, s16 base);
s32 func_801E0354(ParticlePool *pool, Particle *particle);
void func_801E0844(s16 *id, s32 unused);
ImageAnim *func_801E0A00(ImageAnim *anim, ImageAnim *target, u16 mode, u16 flags, ColorRow *colors,
                         s16 x, s16 y, s16 z, s16 x2, s16 y2, s16 z2, s16 x3, s16 y3, s16 w, s16 h,
                         u16 speed, s16 divisor, s16 base, FrameCurve curve);
void StoreImage(RECT *rect, u16 *pixels);
s32 DrawSync(s32 mode);
FrameCurve func_801E34BC(s32 type);
void func_801E8330(u16 index, u16 mask, s32 arg2);
void func_801E8394(Actor *source, u16 index, u16 mask, s32 arg3);
void func_801DCEC8(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer);
void func_801E0398(ParticlePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer);
void func_801E1880(Actor **actors);
void func_801E37D0(Actor *actor);
s32 func_801E36BC(Actor *actor, SlotPool *pool, s32 ticks, s32 arg3, s32 arg4);
void func_801DFE8C(SlotPool *pool, ModelPart *parts);
void func_801DFF78(SlotPool *pool, ModelPart *parts, u8 tag);
void func_801DCE18(ModelList *list, s32 release_models);
void func_801E165C(ImageAnim *anim);
void func_801E3438(Record24 *record);
u16 func_801DEF10(ModelPart *parts, s16 *data);
void SetColorMatrix(MATRIX *m);              /* SetColorMatrix */
void func_8003852C(void *bank);              /* release a sound effect bank */
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
