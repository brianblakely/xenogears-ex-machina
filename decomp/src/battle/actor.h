#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

/* The party members' battle sprites (resident sprite-engine objects) and
 * their tasks, as the late battle unit (800B15D8-) uses them. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "scene.h"
#include "screen.h"
#include "sprite_effect.h"
#include "frame.h"


/* Resident sprite engine. */
void func_800242F4(Sprite *sprite, s32 a, s16 b, s16 c, s32 d, s32 e, s32 f, s32 g);


extern u8 D_800C3664;  /* sprite updates paused */
extern s32 D_800C367C;
extern SVECTOR D_800C3740;       /* the camera's framing angles */

extern u16 D_800C3666;           /* the gear image places taken (D_800C3668) */
extern s32 D_800C35D8;           /* gear object loads running */
extern u8 D_800C37CC;            /* the gear objects are loaded */

/* The camera. */
extern s32 D_800C3674;
extern s32 D_800C3678;
extern SpriteTask *D_800C3680;   /* the eye sprite's task */
extern SpriteTask *D_800C3684;   /* the look-at sprite's task */
extern SVECTOR D_800D30A0[2];    /* the camera's wanted eye and look-at points */

extern MATRIX D_800D30BC; /* the battle view matrix */

/* The battle camera (800d309c); its view matrix is also named D_800D30BC and
 * its eye and look-at points D_800D30A0. */
typedef struct BattleCamera {
    s32 field0;
    SVECTOR eye;    /* +04 */
    SVECTOR target; /* +0C */
    SVECTOR rot;    /* +14 */
    s32 range;      /* +1C */
    MATRIX matrix;  /* +20 */
    s32 drawn;      /* +40: vertical blank after drawing */
    s32 synced;     /* +44: after the GPU finished */
    s32 start;      /* +48: at the frame's start */
} BattleCamera;
extern BattleCamera D_800D309C;
extern u8 D_800C372C;      /* stage drawing off */
extern SVECTOR D_800C3730; /* the camera's up vector */
extern u8 D_800C3688;      /* frame the sprites without their gear heights */
extern Sprite *D_800D39EC; /* the sprite the camera circles */
extern s32 D_800C3738;           /* its distance from it */
extern s16 D_800C373C;           /* its angle round it */

/* This unit. */
void func_800BB13C(Task *task);
void func_800BB314(Task *task);
void func_800BB760(s32 slot);
void func_800BAB0C(Task *task);
void func_800BABDC(Task *task);
void func_800BAC50(Task *task);
void func_800BB350(u32 slot);
void func_800BA59C(Sprite *sprite, s16 direction);
void func_800BF2B8(Sprite *sprite);
void func_800BFBA0(void);
void func_800BC454(s16 value);
void func_800BC2F0(s32 mode);
void func_800BC460(u32 mask);

/* Other battle units. */
void func_800B136C(void);
void func_800B14CC(s32 keep);
void func_800A9540(s32 slot);
void func_800A4654(MATRIX *view, MATRIX *light, s32 arg2, u32 *ot, s32 buffer, SVECTOR *eye, SVECTOR *target,
                   s32 depth);

#endif
