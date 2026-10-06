#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

/* The party members' battle sprites (resident sprite-engine objects) and
 * their tasks, as the late battle unit (800B15D8-) uses them. */

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "screen.h"
#include "sprite.h"
#include "frame.h"


/* Resident sprite engine. */
void func_8001CE74(ActorTask *task);
void func_8001D3F4(BattleSprite *sprite);
void func_8001E298(BattleSprite *sprite, u32 *ot);
void func_80021B14(VECTOR *out, s32 x, s32 y, s32 z);
void func_80022B2C(BattleSprite *sprite);
void func_80022CDC(BattleSprite *sprite);
void func_80023804(BattleSprite *sprite);
void func_800239A0(BattleSprite *sprite);
void func_800242F4(BattleSprite *sprite, s32 a, s16 b, s16 c, s32 d, s32 e, s32 f, s32 g);

extern s32 D_80059188;  /* tasks running */
extern u8 D_800591AF;
extern s32 D_800591A8;

extern u8 D_800C3664;  /* sprite updates paused */
extern s32 D_800C367C;
extern SVECTOR D_800C3740;       /* the camera's framing angles */

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
extern VECTOR D_8006F99C;        /* resident: the eye sprite's position (16.16) */
extern VECTOR D_8006F9AC;        /* resident: the look-at sprite's position (16.16) */
extern SVECTOR D_800D30A0[2];    /* the camera's wanted eye and look-at points */
extern SVECTOR D_800C3CCC;       /* the eye point saved while the camera sprites run */
extern SVECTOR D_800C3CD4;       /* the look-at point saved while they run */
extern u16 D_80059454;
extern s32 D_800C3CDC;            /* the framed camera range */

extern MATRIX D_800D30BC; /* the battle view matrix */

/* The battle camera (800d309c); its view matrix is also named D_800D30BC and
 * its eye and look-at points D_800D30A0. */
typedef struct {
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
extern BattleSprite *D_800D39EC; /* the sprite the camera circles */
extern s32 D_800C3738;           /* its distance from it */
extern s16 D_800C373C;           /* its angle round it */

/* This unit. */
void func_800BB13C(ActorTask *task);
void func_800BB314(ActorTask *task);
void func_800BB760(s32 slot);
void func_800BAB0C(ActorTask *task);
void func_800BABDC(BattleTask *task);
void func_800BAC50(ActorTask *task);
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
void func_800A4654(MATRIX *view, MATRIX *light, s32 arg2, u32 *ot, s32 buffer, SVECTOR *eye, SVECTOR *target,
                   s32 depth);

#endif
