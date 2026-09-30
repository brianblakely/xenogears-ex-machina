#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

/* The party members' battle sprites (resident sprite-engine objects) and
 * their tasks, as the late battle unit (800B15D8-) uses them. */

#include "common.h"
#include "psyq.h"
#include "scene.h"

/* A resident task node (0x38 bytes); a sprite task's sprite follows it. */
typedef struct ActorTask {
    struct ActorTask *owner;
    void *data;                           /* 0x04: its sprite */
    void (*update)(struct ActorTask *);   /* 0x08 */
    void (*destroy)(struct ActorTask *);  /* 0x0C */
    u32 id;                               /* 0x10 */
    u32 link;                             /* 0x14 */
    struct ActorTask *next;               /* 0x18 */
    s32 field1C;                          /* 0x1C: list node, or the task's argument */
    void *field20;                        /* 0x20: its drawn sprite */
    u8 pad24[0x38 - 0x24];
} ActorTask;

/* A sprite's renderer (fields as far as used). */
typedef struct {
    u8 pad0[0x2C];
    void *parts; /* 0x2C: its part block, allocated */
} ActorRenderer;

/* A battle sprite (the resident Sprite, 0xB4 bytes; fields as far as used). */
typedef struct {
    s32 x, y, z;                /* 0x00: 16.16 */
    s32 speedX, speedY, speedZ; /* 0x0C */
    s32 speed;                  /* 0x18 */
    s32 gravity;                /* 0x1C */
    ActorRenderer *renderer;    /* 0x20 */
    void *resource;             /* 0x24 */
    u8 pad28[6];
    s16 depth;      /* 0x2E: ordering-table depth, 0 hidden */
    s16 depthBias;  /* 0x30 */
    s16 direction;  /* 0x32 */
    u16 field34;    /* 0x34: 1 takes over a running camera sprite */
    u16 size;       /* 0x36 */
    s16 halfSize;   /* 0x38 */
    u8 pad3A[2];
    u32 render;     /* 0x3C */
    union {
        u32 word;
        struct {
            unsigned pad0 : 13;
            unsigned group : 4; /* 0x0A the camera's eye sprite */
            unsigned pad17 : 15;
        } bits;
    } flags;        /* 0x40 */
    u8 pad44[4];
    s32 field48;    /* 0x48 */
    s32 field4C;    /* 0x4C */
    u8 pad50[0x64 - 0x50];
    s32 framesLeft; /* 0x64 */
    u8 pad68[4];
    ActorTask *task; /* 0x6C */
    u8 pad70[0x78 - 0x70];
    s32 triangle;   /* 0x78: scene triangle under it */
    struct {
        u8 pad0[8];
        s32 field8;
        s16 fieldC;
    } *sequencer;   /* 0x7C */
    u16 field80;
    u16 field82;    /* 0x82 */
    s16 ground;     /* 0x84 */
    u8 pad86[0x9E - 0x86];
    s16 countdown;  /* 0x9E */
    s16 target[3];  /* 0xA0 */
    u8 padA6[2];
    union {
        u32 word;
        struct {
            unsigned sequencerOwned : 1;
            unsigned pad1 : 29;
            unsigned slotLow : 2; /* the slot's low bits */
        } bits;
    } frame;        /* 0xA8 */
    union {
        u32 word;
        struct {
            unsigned slotHigh : 2; /* the slot's high bits */
            unsigned flip : 1;
            unsigned pad3 : 2;
            unsigned owned : 1;    /* bit 5: destroy its child tasks with it */
            unsigned doubleStep : 1;
            unsigned pad7 : 25;
        } bits;
        s8 bytes[4];               /* [3]: the running animation */
    } motion;       /* 0xAC */
    s8 fieldB0;     /* 0xB0 */
} BattleSprite;

/* A slot's sprite source (0xC bytes). */
typedef struct {
    void *data;
    s16 x;
    s16 y;
    s32 variant;
} SpriteSource;

/* The battle overlay's work area at 0x800C3EB0 (fields as far as used).
 * battle_core.h declares its first member, the formation pointer, as
 * D_800C3EB0; BATTLE_AREA views the whole. */
typedef struct {
    void *formation;             /* 0x0000 */
    BattleSlot slots[11];        /* 0x0004 (D_800C3EB4) */
    u8 pad138[0x8C54 - 0x138];
    u32 *ot;                     /* 0x8C54 */
    u8 pad8C58[0x8C84 - 0x8C58];
    s32 buffer;                  /* 0x8C84 */
    u8 pad8C88[4];
    BattleSprite *sprites[11];   /* 0x8C8C */
    ActorTask *tasks[11];        /* 0x8CB8 */
    u8 pad8CE4[0x8D24 - 0x8CE4];
    SpriteSource sources[11];    /* 0x8D24 */
} BattleArea;

#define BATTLE_AREA (*(BattleArea *)&D_800C3EB0)

/* Resident sprite engine. */
ActorTask *func_8001D1D8(s32 size, ActorTask *owner, void (*update)(ActorTask *), void (*draw)(ActorTask *),
                         void (*destroy)(ActorTask *)); /* create a sprite task */
ActorTask *func_8001CD08(ActorTask *owner, s32 size);    /* create a task */
void func_8001CD74(ActorTask *task, void (*destroy)(ActorTask *));
void func_8001CB48(void *node);
void func_8001CD94(ActorTask *task);
void func_8001CE74(ActorTask *task);
void func_8001D3F4(BattleSprite *sprite);
void func_8001E298(BattleSprite *sprite, u32 *ot);
void func_80021B04(SVector *out, s32 x, s32 y, s32 z);
void func_80021B14(Vector *out, s32 x, s32 y, s32 z);
void func_80022B2C(BattleSprite *sprite);
void func_80022CDC(BattleSprite *sprite);
void func_80023210(BattleSprite *sprite);
void func_80023804(BattleSprite *sprite);
void func_800239A0(BattleSprite *sprite);
void func_800242F4(BattleSprite *sprite, s32 a, s16 b, s16 c, s32 d, s32 e, s32 f, s32 g);
void func_800245D8(BattleSprite *sprite, s32 animation);
s32 func_800286CC(void);

extern u32 *D_8005956C; /* the current ordering table */
extern s32 D_80059188;  /* tasks running */
extern u8 D_800591AF;
extern u16 D_800591A8;

extern u8 D_800C3664;  /* sprite updates paused */
extern s32 D_800C367C;
extern s16 D_800C3740;

/* Run the calls between the two on a stack ending at top. */
#define STACK_ENTER(top)                                                                           \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(top)                                                                    \
                     : "$8", "memory")
#define STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* Where the gear objects' images go (three places). */
typedef struct {
    s16 x;
    s16 y;
} ImagePlace;

extern u16 D_800C3666;           /* the places taken */
extern ImagePlace D_800C3668[3];
extern u8 D_800C3CB8;            /* gear file reads running */
extern s32 D_800C35D8;           /* gear object loads running */
extern u8 D_800C37CC;            /* the gear objects are loaded */

/* The camera. */
extern s32 D_800C3674;
extern s32 D_800C3678;
extern u8 D_800C3CC4;            /* eye and look-at sprites running */
extern s32 D_800C3CBC;
extern s32 D_800C3CC0;           /* camera mode */
extern ActorTask *D_800C3680;    /* the eye sprite's task */
extern ActorTask *D_800C3684;    /* the look-at sprite's task */
extern Vector D_8006F99C;        /* resident: the eye sprite's position (16.16) */
extern Vector D_8006F9AC;        /* resident: the look-at sprite's position (16.16) */
extern SVector D_800D30A0[2];    /* the camera's wanted eye and look-at points */
extern SVector D_800C3CCC;       /* the eye point saved while the camera sprites run */
extern SVector D_800C3CD4;       /* the look-at point saved while they run */
extern u16 D_80059454;
extern u16 D_800C3CDC;

extern Matrix D_800D30BC; /* the battle view matrix */

/* The battle camera (800d309c); its view matrix is also named D_800D30BC and
 * its eye and look-at points D_800D30A0. */
typedef struct {
    s32 field0;
    SVector eye;    /* +04 */
    SVector target; /* +0C */
    SVector rot;    /* +14 */
    s32 range;      /* +1C */
    Matrix matrix;  /* +20 */
} BattleCamera;
extern BattleCamera D_800D309C;
extern SVector D_800C354C; /* view shake offset */
extern u8 D_800C372C;      /* stage drawing off */
extern SVector D_800C3730; /* the camera's up vector */
extern BattleSprite *D_800D39EC; /* the sprite the camera circles */
extern s32 D_800C3738;           /* its distance from it */
extern s16 D_800C373C;           /* its angle round it */

/* This unit. */
void func_800BB13C(ActorTask *task);
void func_800BB760(s32 slot);
void func_800BAB0C(ActorTask *task);
void func_800BABDC(ActorTask *task);
void func_800BAC50(ActorTask *task);
void func_800BFC80(ActorTask *task, s32 arg1, s32 arg2);
void func_800BB350(u32 slot);
void func_800BA59C(BattleSprite *sprite, s16 direction);
void func_800BF2B8(BattleSprite *sprite);
void func_800BFBA0(void);
void func_800BC454(s16 value);
void func_800BC2F0(s32 mode);
void func_800BC460(u32 mask);

/* Other battle units. */
void func_800B136C(void);
void func_800B14CC(s32 keep);
void func_800A9540(s32 slot);
void func_800A979C(s32 index, s16 x, s16 y, s16 z, s16 angle);
void func_800A4654(Matrix *view, Matrix *light, s32 arg2, u32 *ot, s32 buffer, SVector *eye, SVector *target,
                   s32 depth);

#endif
