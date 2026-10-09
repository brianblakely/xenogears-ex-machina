#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* The slots' battle sprites (resident sprite-engine objects) and their tasks,
 * the gear objects that stand in for them, and the battle camera (800B8098's
 * unit, 800B9F7C-800BCB54). */

extern u8 D_800C3664;      /* sprite updates paused */
extern s32 D_800C367C;
extern u16 D_800C3666;     /* the gear image places taken (D_800C3668) */
extern s32 D_800C35D8;     /* gear object loads running */
extern u8 D_800C37CC;      /* the gear objects are loaded */
extern u8 D_800C3688;      /* frame the sprites without their gear heights */

/* The battle camera (800d309c); its view matrix is also named D_800D30BC,
 * its eye and look-at points D_800D30A0, its angles D_800D30B0 and its range
 * D_800D30B8. */
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
extern SVECTOR D_800D30A0[2];  /* the camera's wanted eye and look-at points */
extern SVECTOR D_800D30B0;     /* the camera's angles */
extern s32 D_800D30B8;         /* the camera's distance */
extern MATRIX D_800D30BC;      /* the battle view matrix */
extern s32 D_800C3674;
extern s32 D_800C3678;
extern SpriteTask *D_800C3680; /* the eye sprite's task */
extern SpriteTask *D_800C3684; /* the look-at sprite's task */
extern SVECTOR D_800C3740;     /* the camera's framing angles */
extern u8 D_800C372C;          /* stage drawing off */
extern SVECTOR D_800C3730;     /* the camera's up vector */
extern Sprite *D_800D39EC;     /* the sprite the camera circles */
extern s32 D_800C3738;         /* its distance from it */
extern s16 D_800C373C;         /* its angle round it */

/* The slots' sprites. */
void func_800BA4E0(s32 value);       /* end a slot's turn presentation */
void func_800BA614(Sprite *sprite);  /* aim a sprite's jump at its target */
void func_800BA768(Sprite *sprite);  /* the same, keeping its rising speed */
void func_800BA8F4(Sprite *sprite);  /* put a sprite on the scene's ground */
void func_800BAEB8(s32 slot);        /* face a slot's sprite along its side */
void func_800BAF48(s32 slot);        /* send a party slot's sprite off for its gear */
void func_800BB350(u32 slot);        /* create a slot's sprite following its object */
void func_800BB760(s32 slot);        /* start loading a slot's gear object */

/* The camera. */
void func_800BB9D4(void);            /* set the battle view and draw the stage */
void func_800BBAB8(void);            /* step the battle camera */
void func_800BC2F0(s32 mode);        /* set the camera mode */
void func_800BC3F8(s32 value);
void func_800BC404(s32 mask);        /* start a camera move */
void func_800BCAA4(void);            /* camera mode 4 */
void func_800BCAD0(void);            /* camera mode 1 */

#endif
