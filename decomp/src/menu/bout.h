#ifndef MENU_BOUT_H
#define MENU_BOUT_H

#include "common.h"
#include "actor.h"

/* The bout (menu3 80075060-80075748, 80079A8C-8007B210 but the camera's
 * functions; menu5 80083CE8): rounds, the referee, the replay of a round's
 * end and the result view. */

extern s32 D_800911D4;  /* debug: pad camera tuning, both sides' move names */
extern u8 D_80092884;   /* rubber band battle (an options page setting) */
extern s32 D_80092890;  /* who was knocked out: 0 the second actor, 1 the first, 2 both */
extern s32 D_800928AC;  /* replay frames left (0xFF: replay over) */
extern u8 D_800928C0;   /* frame of the recorded poses */
extern u8 D_800928D4;   /* the fight is on (from "FIGHT!!" to the knock-out) */
extern u8 D_800928F0;
extern u8 D_800928F4;   /* the actors face the other way */
extern s32 D_80092918;  /* draws */
extern s32 D_8009292C;  /* 0x100 at each round's start */
extern s32 D_80092944;  /* frames the bout has run */
extern s32 D_8009294C;  /* frames since the round started (the replay's length) */
extern s32 D_80092950;  /* round number */

void func_80079A8C(void);
void func_80079B0C(void);
s32 func_80079B44(void);
void func_80079DF0(Actor *first, Actor *second);
void func_8007A21C(s32 frames);
void func_8007A344(Actor *first, Actor *second);
void func_8007AC3C(void);
void func_8007AE10(Actor *first, Actor *second);
void func_80083CE8(void);

#endif
