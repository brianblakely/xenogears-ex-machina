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

/* The camera quake task (battle_current_quake): an amplitude easing from one to
 * another, applied with alternating signs to the view offset battle_quake_view_offset. */
typedef struct {
    Task task;
    SVECTOR amplitude; /* 0x1C */
    SVECTOR from;      /* 0x24 */
    SVECTOR to;        /* 0x2C */
    s32 tick;          /* 0x34 */
    s32 left;          /* 0x38 */
    s32 total;         /* 0x3C */
} Quake;

/* The screen fade (battle_current_screen_fade): a full-screen blended rectangle whose
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

/* The stage light fade (battle_current_light_fade): light slot 0 (800A6444) with its green
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

extern Quake *battle_current_quake;
extern SVECTOR battle_quake_view_offset; /* the quake's view offset */
extern ScreenFade *battle_current_second_screen_fade;
extern ScreenFade *battle_current_screen_fade;
extern u8 battle_screen_fade_use_second; /* fade on the second screen fade */
extern LightFade *battle_current_light_fade;
extern u8 battle_screen_fade_blocked;

extern SVECTOR battle_shatter_upper_left_triangle[3]; /* the shards' triangles, per layer */
extern SVECTOR battle_shatter_lower_right_triangle[3];

/* The shattered screen's set-up (800B7424). */
extern VECTOR battle_shatter_launch_velocity; /* a shard's launch velocity before turning */

/* Fade light slot 0; defined without a prototype (to, frames, red, blue,
 * field4C, field4E). */
void battle_light_fade_start();
void battle_save_vram_columns(void);        /* save the three VRAM columns at 0x200-0x2BF */
void battle_shatter_update(Task *task);  /* the shattered screen's update */
void battle_shatter_start(void);        /* shatter the screen copied to VRAM */

#endif
