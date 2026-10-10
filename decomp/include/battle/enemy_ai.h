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

extern EnemyAi battle_enemy_ai_blocks[8];

/* Enemy reaction state (4 bytes per enemy from 800c3d18). The setup (ovl2615)
 * arms the scripts an enemy has and clears unk3. */
typedef struct EnemyReaction {
    u8 armed;          /* the reaction script runs */
    u8 unk1[2];        /* [0] the after-turn script is armed; [1] a party
                        * member's turn targeted the enemy (both run it) */
    u8 unk3;
} EnemyReaction;

extern EnemyReaction battle_enemy_reactions[8];

/* The enemies' names. */
extern void *battle_enemy_name_table;   /* enemy name table */
extern u8 battle_enemy_name_indices[8];   /* enemy name per enemy slot (3-10) */

/* The AI script interpreter (800792F8's unit 80079934-80079E7C, 80079ED8's
 * 8007EF6C-8007F8C0). */
void battle_ai_step_instruction(u8 **pc);                /* step past a four-byte instruction */
void battle_ai_run_turn_script(u8 slot, u16 attacking); /* run enemy slot's turn script */
s32 battle_ai_run_reaction_script(u8 slot); /* run the enemy reaction script; it ran action 0x62 */
void battle_ai_run_targeted_scripts(void);                   /* the after-turn scripts */
u8 battle_find_first_slot_in_mask(u16 mask);                 /* the first slot in mask; 11 when none */
u8 battle_ai_run_action(u8 **pc, u8 enemy, u8 count); /* run an AI action */
u8 battle_ai_evaluate_condition(u8 **pc, u8 enemy);        /* evaluate an AI condition */

#endif
