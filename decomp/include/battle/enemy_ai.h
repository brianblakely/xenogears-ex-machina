#ifndef BATTLE_ENEMY_AI_H
#define BATTLE_ENEMY_AI_H

#include "common.h"

/* The enemies' AI: their script blocks and reaction state, the AI script
 * interpreter (800792f8's unit: turn, reaction and after-turn scripts;
 * 80079ed8's: the conditions and actions; docs/scripts/battle-ai.md), and the
 * enemies' names. */

/* Enemy AI block (0x40 bytes per enemy slot 3..10, from 800d3400). The
 * script pointers come from the enemy's table in the enemy data file
 * (ovl2615 801e4870; docs/scripts/battle-ai.md). */
typedef struct EnemyAi {
    u8 *script;        /* +0x00 table +0: the enemy's turn script (800799c8) */
    u8 *unk4;          /* table +2: only copied (80078e24), never run */
    u8 *reaction;      /* +0x08 table +4: reaction script, run by a party
                        * member's attack step on the enemy (80079ab0) */
    u8 *turnScript;    /* +0x0C table +6: run after a party member's turn for
                        * each enemy it targeted (80079c24) */
    s32 longs[4];      /* +0x10 (the debug overlay's state page: lFlag) */
    u16 vars[8];       /* +0x20 (hFlag) */
    u8 bytes[16];      /* +0x30 (bFlag) */
} EnemyAi;

extern EnemyAi D_800D3400[8];

/* Enemy reaction state (4 bytes per enemy from 800c3d18). */
typedef struct EnemyReaction {
    u8 armed;          /* the reaction script runs */
    u8 unk1[2];
    u8 unk3;
} EnemyReaction;

extern EnemyReaction D_800C3D18[8];

/* The enemies' names. */
extern void *D_800C3DDC;   /* enemy name table */
extern u8 D_800C3E40[8];   /* enemy name per enemy slot (3-10) */

/* The AI script interpreter (800792F8's unit 80079934-80079E7C, 80079ED8's
 * 8007EF6C-8007F8C0). */
void func_80079934(u8 **pc);                /* step past a four-byte instruction */
void func_800799C8(u8 slot, u16 attacking); /* run enemy slot's turn script */
s32 func_80079AB0(u8 slot); /* run the enemy reaction script; it ran action 0x62 */
void func_80079C24(void);                   /* the after-turn scripts */
u8 func_80079E7C(u16 mask);                 /* the first slot in mask; 11 when none */
u8 func_8007EF6C(u8 **pc, u8 enemy, u8 count); /* run an AI action */
u8 func_8007F8C0(u8 **pc, u8 enemy);        /* evaluate an AI condition */

#endif
