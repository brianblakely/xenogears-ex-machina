#ifndef BATTLE_COMMAND_H
#define BATTLE_COMMAND_H

/* Battle command menu: window confirms, target selection and turn flow
 * (80080160-80086f98). */

#include "battle_core.h"

u8 func_8008C4A8(u8 member); /* the member has a special available */
u8 func_8008CFB8(u8 member); /* the gear has a part list */
u8 func_80084548(u8 side, u8 any, u8 partyFirst);
u8 func_80084750(u8 member);
u16 func_80084DE4(u16 selection, u16 fallback, u8 member, u8 mode, u8 own);
void func_8009AB00(u8 member);
void func_8009BAC4(u8 slot, u8 *choice, s16 *busy);
u8 func_80087AF0(u8 member, u8 cost); /* execute the attack; the target reacted */
void func_800861D0(u8 code, u8 member);
u8 func_80085EB4(u8 mode, u8 member);
u8 func_80086F98(u8 step, u8 member);
s32 func_80086B88(s32 step, u8 member); /* the member can use combo step `step` now */
void func_8008AA40(u8 id); /* play a sound effect */
void func_800B8DA4(void);

/* The timer reload by maximum and remaining AP: D_800C31EC (maximum 3-7) from
 * three rows before (battle.data.ld). */
extern u8 D_800C31D4[][8];
s32 func_80086028(u8 member, s32 index, s32 column, u8 id, u32 **pixels, u8 offset);
s32 func_80086C88(u8 member, s32 index, s32 column, u8 step, u32 **pixels);
extern u8 D_800C4929;       /* healing ignores the gear */
extern u8 *D_800C3160[13]; /* combo input patterns (seven inputs each) */
/* The next combo step by step and AP paid, from one byte before D_800C34B4. */
extern u8 D_800C34B3[8][3];
extern u8 *D_800C31AC[];   /* per character: the deathblow of each combo */

/* The fuel cost of each combo step (1-based, indexed like the combo flags
 * 800c34cc) at the gear HUD of the battle work area. */
#define STEP_FUEL (&D_800CCCE8.gearHud.commands[-1])

/* A slot's ground position, read unsigned. */
#define SLOT_X(slot) ((u16)D_800C3EB4[slot].x)
#define SLOT_Z(slot) ((u16)D_800C3EB4[slot].z)
/* The square of a difference, taken of its magnitude. */
#define SQUARE(x) ((x) < 0 ? (-(x)) * (-(x)) : (x) * (x))

#endif
