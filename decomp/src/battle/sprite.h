#ifndef BATTLE_SPRITE_H
#define BATTLE_SPRITE_H

/* The battle's view of the resident sprite engine's sprites (resident
 * sprite.h's Sprite and SpriteRenderer; fields as far as the battle uses
 * them), and its sprite effects (800B4EDC-800B6004). */

#include "common.h"
#include "psyq.h"
#include "screen.h"

/* A 16.16 coordinate, read whole or by its integer half. */
typedef union {
    s32 fixed;
    struct {
        u16 frac;
        s16 whole;
    } part;
} Fixed16;

/* An anchor of a sprite's frame (8 bytes): an offset and angles. */
typedef struct {
    s8 x;
    s8 y;
    s16 angle[3];
} SpriteAnchor;

/* A drawn part of a sprite (resident SpritePart, 0x18 bytes). */
typedef struct {
    s16 x, y;
    u8 u, v;   /* 0x04 */
    u8 w, h;   /* 0x06 */
    u8 byte8, byte9;
    u16 tpage; /* 0x0A */
    u16 clut;  /* 0x0C */
    u8 pad0E[0x18 - 0x0E];
} SpriteImagePart;

/* A sprite's renderer (resident SpriteRenderer). */
typedef struct {
    s16 angle[3];            /* 0x00 */
    s16 scale[3];            /* 0x06 */
    MATRIX matrix;           /* 0x0C: local screen matrix */
    void *parts;             /* 0x2C */
    SpriteImagePart *part;   /* 0x30 */
    SpriteAnchor *anchors;   /* 0x34: 8 of them */
    u8 pad38[0x3C - 0x38];
    u8 field3C;
    u8 field3D;
} SpriteView;

/* A sprite's task (resident Task) with its draw task after it (0x38
 * bytes); the task's data is the sprite. */
typedef struct ActorTask {
    struct ActorTask *owner;
    void *data;                                /* 0x04: its sprite */
    void (*update)(struct ActorTask *task);    /* 0x08 */
    void (*destroy)(struct ActorTask *task);   /* 0x0C */
    u32 id;                                    /* 0x10 */
    u32 link;                                  /* 0x14 */
    struct ActorTask *next;                    /* 0x18 */
    BattleTask draw;                           /* 0x1C: its data the drawn sprite */
} ActorTask;

/* A task with a slot argument after it. */
typedef struct {
    BattleTask task;
    s32 slot; /* 0x1C */
} SlotTask;

/* A resident sprite (resident Sprite, 0xB4 bytes; fields as far as used). */
typedef struct BattleSprite {
    Fixed16 x, y, z;       /* 0x00: 16.16 */
    s32 velocity[3];       /* 0x0C */
    s32 speed;             /* 0x18: bits 7-22 its speed setting */
    s32 gravity;           /* 0x1C */
    SpriteView *view;      /* 0x20 */
    u8 *base;              /* 0x24: its resource block */
    u8 colour[3];          /* 0x28 */
    u8 colourFlags;        /* 0x2B */
    s16 scale;             /* 0x2C */
    s16 depth;             /* 0x2E: ordering-table depth, 0 hidden */
    s16 depthBias;         /* 0x30 */
    s16 direction;         /* 0x32 */
    u16 frame;             /* 0x34: 1 takes over a running camera sprite */
    u16 size;              /* 0x36 */
    s16 halfSize;          /* 0x38 */
    u16 field3A;           /* 0x3A: scale script lengths with the sprite */
    union {
        u32 word;
        u8 bytes[4];
    } render;              /* 0x3C: bits 0-1 sides, bits 5-7 blend */
    union {
        u32 word;
        u8 bytes[4];       /* [0] bits 2-7: the part count */
        struct {
            unsigned pad0 : 13;
            unsigned group : 4; /* 0x0A the camera's eye sprite */
            unsigned pad17 : 15;
        } bits;
    } flags;               /* 0x40 */
    u8 pad44[0x48 - 0x44];
    s32 field48;           /* 0x48 */
    s32 field4C;           /* 0x4C */
    void *sound;           /* 0x50: its command file's sound bank */
    u8 pad54[0x64 - 0x54];
    s32 framesLeft;        /* 0x64 */
    u8 pad68[0x6C - 0x68];
    ActorTask *task;       /* 0x6C */
    struct BattleSprite *parent;  /* 0x70 */
    struct BattleSprite *partner; /* 0x74 */
    s32 triangle;          /* 0x78: scene triangle under it */
    struct {
        s32 file;          /* its file first */
        u8 pad4[4];
        s32 field8;
        s16 fieldC;
    } *resource;           /* 0x7C */
    u16 field80;
    u16 field82;           /* 0x82 */
    s16 ground;            /* 0x84 */
    u8 pad86[0x9E - 0x86];
    s16 countdown;         /* 0x9E */
    s16 target[3];         /* 0xA0 */
    u8 padA6[0xA8 - 0xA6];
    union {
        u32 word;          /* bits 28-29 the phase */
        struct {
            unsigned sequencerOwned : 1;
            unsigned pad1 : 29;
            unsigned slotLow : 2; /* the slot's low bits */
        } bits;
    } frameBits;           /* 0xA8 */
    union {
        u32 word;
        struct {
            unsigned slotHigh : 2; /* the slot's high bits */
            unsigned flip : 1;     /* mirrored */
            unsigned pad3 : 2;
            unsigned owned : 1;    /* bit 5: destroy its child tasks with it */
            unsigned doubleStep : 1;
            unsigned pad7 : 25;
        } bits;
        s8 bytes[4];               /* [3]: the running animation */
    } motion;              /* 0xAC */
    union {
        u32 word;          /* 0x100 an effect sprite */
        s8 mode;           /* the idle motion mode */
    } idle;                /* 0xB0 */
} BattleSprite;

/* A little-endian s16 at index i of a sprite script, and the script data
 * at the relative offset in a command's arguments. */
#define SCRIPT_S16(p, i) ((((s8 *)(p))[(i) + 1] * 256) | (p)[i])
#define SCRIPT_DATA(args) ((args) + SCRIPT_S16(args, 0))

/* A point returned by value. */
typedef struct {
    s16 x;
    s16 y;
} Point2;

/* A sprite's trail (800B572C; two tasks, 0xB8 bytes): its five trail
 * anchors' positions in the current frame and their eased copies drawn by
 * 800C08CC. */
typedef struct {
    struct BattleTask task; /* 0x00 */
    struct BattleTask draw; /* 0x1C */
    BattleSprite *sprite;   /* 0x38 */
    s32 frame;              /* 0x3C */
    s32 motion;             /* 0x40: the sprite's motion byte 3 */
    s32 blend;              /* 0x44 */
    s32 count;              /* 0x48 */
    u8 *colours;            /* 0x4C */
    SVECTOR anchors[5];     /* 0x50 */
    SVECTOR trail[5];       /* 0x78 */
} SpriteTrail;

/* A watch of a sprite approaching its target (800B5924, 0x34 bytes). */
typedef struct {
    struct BattleTask task; /* 0x00 */
    BattleSprite *sprite;   /* 0x1C */
    u8 pad20[4];
    s32 motion;             /* 0x24 */
    s32 distance;           /* 0x28 */
    s32 near;               /* 0x2C */
    s32 frames;             /* 0x30 */
} SpriteApproach;

/* A link keeping a sprite's partner at one of its anchors (800B5C18). */
typedef struct {
    struct BattleTask task;  /* 0x00 */
    BattleSprite *sprite;    /* 0x1C */
    BattleSprite *partner;   /* 0x20 */
    s32 anchor;              /* 0x24 */
    s32 partnerAnchor;       /* 0x28 */
    s32 frame;               /* 0x2C */
    s32 motion;              /* 0x30 */
} SpriteLink;

extern u8 D_800C356C[5]; /* the anchors of a sprite's trail */
extern u8 *D_800D2FD8;   /* the trail being drawn: its colours */
extern s16 D_800C3E9C;   /* its colour count */
extern s16 D_800C3D4C;   /* its blend */
extern s16 D_800D3334;

void func_80023210(BattleSprite *sprite);
s32 func_80022CAC(BattleSprite *sprite, s32 value);
void func_8001E148(BattleSprite *sprite);
void func_800C08CC(s32 count, SVECTOR *points, void (*draw)());
void func_800B50D4(BattleSprite *sprite, SVECTOR *out);
void func_800B51B0();
void func_800B5DF4();
void func_800B5854(SpriteApproach *approach);
void func_800B5588(BattleTask *task);

/* A point on the ground passed by value. */
typedef struct {
    s16 x;
    s16 z;
} GroundPoint;

s16 func_80023124(GroundPoint to, GroundPoint from); /* the direction between points */
void func_80025A88();                      /* the resident sprite drawer */
void func_800245D8(BattleSprite *sprite, s32 value);
void func_8001F6B0(BattleSprite *sprite);


/* Sprite script commands (800B3F04). */
extern BattleSprite *D_800C3E1C;
extern u16 D_800D36BC;
void func_800A96B4(s32 set);
void func_800A979C(s32 index, s16 x, s16 y, s16 z, s16 angle);
void func_800B3CD4(); /* unprototyped (to, frames, red, blue, field4C, field4E) */
void func_800BEE2C(s32 index, s32 mask, s32 mode);
void func_800B6004();
void func_80021B04(SVECTOR *v, s32 x, s32 y, s32 z);
void func_801FC4C4(SpriteAnchor *anchors, void *parts, MATRIX *m, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_801FC53C(BattleSprite *sprite, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);

/* Sprite streaks and sprite effects (800B5DF4-800B7424). */
extern MATRIX D_8004FBB8; /* resident: the sprite camera */
extern MATRIX D_800C3574; /* the screen-space camera (render bit 24) */
extern s32 D_800D2FCC;     /* the trail segment being drawn */
extern s16 D_800C3CA4; /* the last trail segment's far corners */
extern s16 D_800C3CA6;
extern s16 D_800C3CA8;
extern s16 D_800C3CAA;

#endif
