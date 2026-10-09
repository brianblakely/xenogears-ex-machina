#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

/* The party members' battle sprites (resident sprite-engine objects) and
 * their tasks, as the late battle unit (800B15D8-) uses them. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"




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

void func_800BA4E0(s32 value);
void func_800BA614(Sprite *sprite);
void func_800BA768(Sprite *sprite);
void func_800BA8F4(Sprite *sprite);
void func_800BAEB8(s32 slot);
void func_800BAF48(s32 slot);
void func_800BB350(u32 slot);
void func_800BB760(s32 slot);
void func_800BB9D4(void);
void func_800BBAB8(void);
void func_800BC2F0(s32 mode);
void func_800BC3F8(s32 value);
void func_800BC404(s32 mask);
void func_800BCAA4(void);
void func_800BCAD0(void);

extern SVECTOR D_800D30B0; /* the camera's angles */
extern s32 D_800D30B8;     /* the camera's distance */

#endif
