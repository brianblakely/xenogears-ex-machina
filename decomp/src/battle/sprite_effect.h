#ifndef BATTLE_SPRITE_EFFECT_H
#define BATTLE_SPRITE_EFFECT_H

/* The battle's sprite effects (800B3F04's unit, 800B4EDC-800B6F0C) on the
 * resident sprite engine's sprites (resident/sprite.h; battle/sprite.h has
 * the battle's views of them): trails, approach watches, partner links and
 * streaks. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* A task with a slot argument after it. */
typedef struct {
    Task task;
    s32 slot; /* 0x1C */
} SlotTask;

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
    struct Task task; /* 0x00 */
    struct Task draw; /* 0x1C */
    Sprite *sprite;   /* 0x38 */
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
    struct Task task; /* 0x00 */
    Sprite *sprite;   /* 0x1C */
    u8 pad20[4];
    s32 motion;             /* 0x24 */
    s32 distance;           /* 0x28 */
    s32 near;               /* 0x2C */
    u8 *resume;             /* 0x30: script to resume near the target */
} SpriteApproach;

/* A link keeping a sprite's partner at one of its anchors (800B5C18). */
typedef struct {
    struct Task task;  /* 0x00 */
    Sprite *sprite;    /* 0x1C */
    Sprite *partner;   /* 0x20 */
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

/* Sprite script commands (800B3F04). */
extern Sprite *D_800C3E1C;
extern s16 D_800D36BC;

/* Sprite streaks and sprite effects (800B5DF4-800B7424). */
extern MATRIX D_800C3574; /* the screen-space camera (render bit 24) */

s32 func_800B57E4(Sprite *sprite); /* the distance from a sprite to its target */
SpriteApproach *func_800B5924(Sprite *sprite, s32 near, u8 *resume); /* watch a sprite approach its target */

#endif
