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
    SVector amplitude; /* 0x1C */
    SVector from;      /* 0x24 */
    SVector to;        /* 0x2C */
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

extern Quake *D_800C3548;
extern s16 D_800C354C; /* the quake's view offset */
extern s16 D_800C354E;
extern s16 D_800C3550;
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
void *func_8001CD08(s32 owner, s32 size);          /* create a task */
void *func_8001D1D8(s32 size, s32 owner, void (*update)(), void (*draw)(), void (*destroy)());
void func_8001CC18(s32 owner, void *task);
void func_8001CA58(void *owner, void *node);
void func_8001CD64(void *task, void (*update)());
void func_8001CD74(void *task, void (*destroy)());
void func_8001CD94(void *task); /* end a task */
void func_8001CB48(void *node);

/* GTE: IR0, IR1-IR3 and the general purpose interpolation (no shift). */
#define gte_lddp(r0) __asm__ volatile("mtc2 %0, $8" : : "r"(r0))
#define gte_ldlvl(r0)                                                                              \
    __asm__ volatile("lwc2 $9, 0(%0);"                                                             \
                     "lwc2 $10, 4(%0);"                                                            \
                     "lwc2 $11, 8(%0)"                                                             \
                     :                                                                             \
                     : "r"(r0))
#define gte_gpf0() __asm__ volatile("nop;nop;.word 0x4B90003D")
#define gte_stlvl(r0)                                                                              \
    __asm__ volatile("swc2 $9, 0(%0);"                                                             \
                     "swc2 $10, 4(%0);"                                                            \
                     "swc2 $11, 8(%0)"                                                             \
                     :                                                                             \
                     : "r"(r0)                                                                     \
                     : "memory")

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
void func_800A6444(s32 index, s32 r, s32 g, s32 b, s32 field4, s32 field5);
void func_800A6F98(void);
void func_800A5EB4(void);

#endif
