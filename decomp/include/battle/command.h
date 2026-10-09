#ifndef BATTLE_COMMAND_H
#define BATTLE_COMMAND_H

#include "common.h"
#include "battle/scene.h"
#include "battle/work.h"

/* The battle command menu: a party member's turn in the menu, the attack
 * page and its combos, target selection, automatic turns and the gear, item
 * and escape commands (80079ED8's unit 8007FB70-80080C94, battle.c
 * 800826CC-8008B108, 8008B478's 8008C4A8, 8008CCCC's 8008CFB8-8009BAC4 and
 * 800B8098's turn cancel 800B8DA4). */

/* The menu's state. */
extern u8 D_800D366C;      /* menu effects enabled */
extern void *D_800D367C;   /* menu module block */
extern void *D_800C3DE8;   /* file 3 block */
extern u8 D_800C3E90[12];  /* default-target candidates */
extern u8 D_800D3274;      /* candidate count */
extern u16 D_800C3D64;
extern u8 D_800C3E2C;
extern u8 D_800CCC58;
extern s32 D_800D3288;
extern u8 D_800D39D4;
extern u8 D_800C34CC[];    /* combo step flags */
extern u16 D_800D2C32;     /* fuel gained by charging */

/* The timer reload by maximum and remaining AP: D_800C31EC (maximum 3-7) from
 * three rows before (battle.data.ld). */
extern u8 D_800C31D4[][8];
extern u8 D_800C4929;       /* healing ignores the gear */
extern u8 *D_800C3160[13]; /* combo input patterns (seven inputs each) */
/* The next combo step by step and AP paid: D_800C34B4 from one byte before
 * (battle.data.ld). */
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

/* A member's turn in the menu (80079ED8's unit). */
void func_8007FB70(u8 member);
void func_8007FCE8(void);    /* release the menu module block */
void func_8007FD38(u8 member); /* load the menu module block */
void func_8007FDEC(void);    /* release the file 3 block */
void func_8007FE3C(void);    /* load the file 3 block */
void func_80080160(u8 member); /* run a party member's command menu */
void func_80080BD0(void);    /* events done: refresh the actor's menu state */
void func_80080C94(u8 member); /* an automatic turn */

/* The attack page, targets and commands (battle.c). */
void func_800826CC(u8 member); /* the member boards its gear */
u8 func_80083FF4(u8 member, u8 slot); /* whether the member can attack slot */
u8 func_800841E0(u8 member);   /* order the member's attack candidates */
u8 func_80085084(u16 target, u8 member, s32 mode); /* select a target */
void func_80087A38(u8 member); /* enter the attack page */
u8 func_80087AF0(u8 member, u8 cost); /* execute the attack; the target reacted */
void func_8008AA74(u8 id);   /* play a menu sound */
void func_8008ADD0(u8 member); /* execute the chosen technique */

/* The combo, gear, item and escape commands (8008B478's and 8008CCCC's
 * units). */
u8 func_8008C4A8(u8 member); /* run the member's combo; 1 when cancelled */
u8 func_8008CFB8(u8 member); /* run the gear command menu; 1 when committed */
void func_80093B08(u8 member);
void func_8009413C(u8 member, u8 release);
s32 func_8009A9D0(void);     /* the escape succeeds */
void func_8009AA44(u8 member); /* the Defense command */
void func_8009AB00(u8 member); /* end the member's defending */
void func_8009BAC4(u8 slot, u8 *choice, s16 *busy); /* choose an automatic action */
void func_800B8DA4(void);    /* cancel the turn */

#endif
