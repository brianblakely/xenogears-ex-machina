#ifndef BATTLE_SCREEN_H
#define BATTLE_SCREEN_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* Screen effects run as resident tasks: the camera quake, the screen fade,
 * the stage light fade and the saved VRAM columns (800B15D8's unit,
 * 800B3358-800B3F04), and the shattered screen (800B3F04's unit 800B6F0C,
 * 800B7134's to 800B7424). */

/* The camera quake task (D_800C3548): an amplitude easing from one to
 * another, applied with alternating signs to the view offset D_800C354C. */
typedef struct {
    Task task;
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
    Task task;
    Task draw;  /* 0x1C */
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
    Task task;
    Task draw; /* 0x1C */
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
    Task task;
    Task draw;                 /* 0x1C */
    s32 frame;                       /* 0x38 */
    ScreenShard shards[2][14][20];   /* 0x3C */
} ScreenShatter;

extern Quake *D_800C3548;
extern SVECTOR D_800C354C; /* the quake's view offset */
extern ScreenFade *D_800C3554;
extern ScreenFade *D_800C3558;
extern u8 D_800C355C; /* fade on the second screen fade */
extern LightFade *D_800C3560;
extern u8 D_800D3638;

extern SVECTOR D_800C3594[3]; /* the shards' triangles, per layer */
extern SVECTOR D_800C35AC[3];

/* The shattered screen's set-up (800B7424). */
extern VECTOR D_800C35C4; /* a shard's launch velocity before turning */

/* Fade light slot 0; defined without a prototype (to, frames, red, blue,
 * field4C, field4E). */
void func_800B3CD4();
void func_800B3E04(void);        /* save the three VRAM columns at 0x200-0x2BF */
void func_800B6F0C(Task *task);  /* the shattered screen's update */
void func_800B73A0(void);        /* shatter the screen copied to VRAM */

#endif
