#ifndef MENU_SCRIPT_H
#define MENU_SCRIPT_H

#include "common.h"
#include "resident/window.h"
#include "actor.h"
#include "node.h"
#include "mode.h"

/* The arena's scenes (menu2 80070F80-80072D18): the scene script
 * interpreter (its scripts are assets, docs/scripts/arena-scene.md), the
 * opening and the scene list, the message window, the bout-end sequence
 * and the winner screen. */

/* A step in one of eight directions on the floor plane. */
typedef struct {
    s32 x;
    s32 z;
} FloorStep;

extern u8 D_80090F38[];           /* the opening's scene script */
extern u8 *D_8009105C[];          /* scene scripts */
extern FloorStep D_80091084[8];
extern u8 D_800910C4[];           /* the setup script */
extern LightRig *D_800910F0;      /* the scene's lights */
extern Actor *D_80092894;         /* actor the scene script drives */
extern s32 D_80092900;            /* bout-end sequence step */
extern u8 D_8009293C;             /* the one-time scene setup ran */
extern Window D_8009868C;         /* message window */

void func_80070F80(u8 *script);
s32 func_8007107C(void);
void func_80071724(u32 *ot);
void func_80071794(MenuImages *files);
void func_800718C0(void);
void func_8007191C(s32 scene);
void func_800719F0(void);
void func_80071AD0(void);
void func_80071DA4(Actor *actor);
void func_800720C4(void);
void func_800720D4(void);
void func_80072170(void);
void func_800725B0(Actor *scene);
void func_80072858(LightRig *rig);

#endif
