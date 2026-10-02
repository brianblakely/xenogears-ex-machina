#ifndef BATTLE_SCREEN_H
#define BATTLE_SCREEN_H

/* Screen effects run as resident tasks: the camera quake, the screen fade
 * and the stage light fade (800B3358-800B3E04). */

#include "common.h"
#include "psyq.h"

/* A resident task node (0x1C bytes; resident/sprite.h's Task). */
typedef struct BattleTask {
    struct BattleTask *owner;
    void *data;                             /* 0x04 */
    void (*update)(struct BattleTask *task);  /* 0x08 */
    void (*destroy)(struct BattleTask *task); /* 0x0C */
    u32 id;                                 /* 0x10 */
    u32 link;                               /* 0x14: bit 31 active */
    struct BattleTask *next;                /* 0x18 */
} BattleTask;

/* The camera quake task (D_800C3548): an amplitude easing from one to
 * another, applied with alternating signs to the view offset D_800C354C. */
typedef struct {
    BattleTask task;
    SVECTOR amplitude; /* 0x1C */
    SVECTOR from;      /* 0x24 */
    SVECTOR to;        /* 0x2C */
    s32 tick;          /* 0x34 */
    s32 left;          /* 0x38 */
    s32 total;         /* 0x3C */
} Quake;

/* The screen fade (D_800C3558): a full-screen blended rectangle whose
 * colour eases to a target, drawn by a second task. */
typedef struct {
    BattleTask task;
    BattleTask draw;  /* 0x1C */
    s32 total;        /* 0x38 */
    s32 left;         /* 0x3C */
    u8 field40;
    u8 blend;         /* 0x41: the blend mode, 1 none */
    u8 to[3];         /* 0x42 */
    u8 from[3];       /* 0x45 */
    u8 colour[3];     /* 0x48 */
} ScreenFade;

/* The stage light fade (D_800C3560): light slot 0 (800A6444) with its green
 * at 32 less a level easing to a target. */
typedef struct {
    BattleTask task;
    BattleTask draw; /* 0x1C */
    s32 total;       /* 0x38 */
    s32 left;        /* 0x3C */
    s16 from;        /* 0x40 */
    s16 to;          /* 0x42 */
    s16 applied;     /* 0x44 */
    s16 level;       /* 0x46 */
    s16 red;         /* 0x48 */
    s16 blue;        /* 0x4A */
    s16 field4C;     /* 0x4C */
    s16 field4E;     /* 0x4E */
} LightFade;

/* A list of points (the points at offset, count of them; flags bit 15 once
 * scaled). */
typedef struct {
    s32 offset; /* 0x00: from the list */
    s32 count;  /* 0x04 */
    u8 pad8[0x18 - 0x8];
    u32 flags;  /* 0x18 */
} VertexList;

/* An 8-byte primitive (tag and one command word). */
typedef struct {
    u32 tag;
    u32 code;
} DrawPrim8;

/* A shard of the shattered screen (0x7C bytes): turning and falling, drawn
 * as a textured triangle per buffer. */
typedef struct {
    SVECTOR angles;    /* 0x00 */
    SVECTOR spin;      /* 0x08: added to the angles each frame */
    VECTOR position;   /* 0x10 */
    u8 pad20[4];
    POLY_FT3 poly[2];  /* 0x24: one per drawing buffer */
    s32 delay;         /* 0x64: frames before it moves */
    VECTOR velocity;   /* 0x68: 16.16, easing out, with gravity */
    s32 fall;          /* 0x78: added to velocity.vy each frame */
} ScreenShard;

/* The shattered screen (800B73A0, 0x10F7C bytes): two layers of 14 rows of
 * 20 shards cut from the screen copied to VRAM (0x2C0, 0x100). */
typedef struct {
    BattleTask task;
    BattleTask draw;                 /* 0x1C */
    s32 frame;                       /* 0x38 */
    ScreenShard shards[2][14][20];   /* 0x3C */
} ScreenShatter;

extern DrawPrim8 D_800C3BF8; /* a draw mode primitive */
extern RECT D_800C3C9C;

extern Quake *D_800C3548;
extern SVECTOR D_800C354C; /* the quake's view offset */
extern ScreenFade *D_800C3554;
extern ScreenFade *D_800C3558;
extern u8 D_800C355C; /* fade on the second screen fade */
extern LightFade *D_800C3560;
extern ScreenFade D_800C3C00; /* the second screen fade */
extern ScreenFade D_800C3C50;
extern u8 D_800D3638;
extern u8 *D_80059580;  /* resident: the primitive buffer cursor */
extern u8 *D_80059534;  /* its end */
extern u32 *D_8005956C; /* resident: the ordering table */

/* Resident tasks. */
void *func_8001CD08(void *owner, s32 size);        /* create a task */
void *func_8001D1D8(s32 size, void *owner, void (*update)(), void (*draw)(), void (*destroy)());
void func_8001CC18(s32 owner, void *task);
void func_8001CA58(void *owner, void *node);
void func_8001CD64(void *task, void (*update)());
void func_8001CD74(void *task, void (*destroy)());
void func_8001CD94(void *task); /* end a task */
void func_8001CB48(void *node);
void func_80025180(void *owner); /* end the owner's sprites */

/* Run the calls between the two on a stack ending at top. */
#define STACK_ENTER(top)                                                                           \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(top)                                                                    \
                     : "$8", "memory")
#define STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

void func_800B3358(Quake *quake);
void func_800B3588(Quake *quake);
void func_800B36BC(ScreenFade *fade);
void func_800B3878(BattleTask *draw);
void func_800B383C(ScreenFade *fade);
void func_800B3B94(LightFade *fade);
void func_800B3C74(BattleTask *draw);
void func_800B3C2C(LightFade *fade);

extern u32 *D_800C3CB4; /* the ordering table the shatter draws into */
extern SVECTOR D_800C3594[3]; /* the shards' triangles, per layer */
extern SVECTOR D_800C35AC[3];
u8 func_80021AD8(u8 value, s32 delta); /* add, clamped to 0-255 */
void func_800B6F0C(BattleTask *task);
void func_800B7134(BattleTask *draw);
void func_800B7160(BattleTask *draw);
void func_800B7364(ScreenShatter *shatter);
void func_800B73A0(void);
ScreenShatter *func_800B7424(ScreenShatter *shatter);
void func_800A6444(s32 index, s32 r, s32 g, s32 b, s32 field4, s32 field5);
void func_800A6F98(void);
void func_800A5EB4(void);

/* The shattered screen's set-up (800B7424). */
extern VECTOR D_800C35C4; /* a shard's launch velocity before turning */

#endif
