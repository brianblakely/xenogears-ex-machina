#ifndef MENU_CAMERA_H
#define MENU_CAMERA_H

#include "common.h"
#include "psyq/libgte.h"
#include "actor.h"

/* The camera (menu2 800707A8-80070F80; menu3 800796B8, 8007A768,
 * 8007A958; menu5 800831C8-80083CE8 but 80083BB4): its eye and look-at
 * point, the view modes, the idle orbit and the camera/scene modes. */

extern u8 D_8009287C;      /* idle camera */
extern s32 D_80092904;     /* camera view */
extern s32 D_8009290C;     /* side of the actors' line the eye takes (+-0x400) */
extern VECTOR D_8009867C;  /* eye */
extern VECTOR D_8009871C;  /* look-at point */

void func_800707A8(void);
s32 func_800707D8(s32 target, s32 current, s32 steps);
void func_80070808(VECTOR *target, s32 steps);
void func_800708C4(VECTOR *target, s32 steps);
void func_8007099C(u32 mode);
void func_800796B8(Actor *first, Actor *second);
void func_8007A768(Actor *actor);
void func_800831C8(void);
void func_800832C0(s32 buttons);
void func_80083310(s32 smooth);
void func_8008369C(void);
void func_80083738(Actor *first, Actor *second);
void func_80083C0C(s32 mode);
s32 func_80083CD8(void);

#endif
